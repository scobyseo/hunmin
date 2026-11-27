#!/usr/bin/env bash
# Tier E3: control the shipped IME's trace sink over adb, and pull the result.
#
#   tools/e3-trace.sh start [case-label] [source]   begin a trace
#   tools/e3-trace.sh stop                         close it
#   tools/e3-trace.sh pull <name>                  fetch to tests/conformance/<name>.trace
#                                                  (+ <name>.ui.trace, #312 evaluation)
#   tools/e3-trace.sh mark <label>                 annotate the open UI trace
#   tools/e3-trace.sh status                       is one open?
#
# TWO SINKS, ONE TOGGLE (#312 usability evaluation): the engine key trace
# (e3.trace, docs/trace-spec.md) and the UI-layer trace (ui.trace — gesture
# decisions with trajectory+gates, symbol commits, page transitions, shift
# state). The UI sink exists because expectation mismatches happen ABOVE the
# engine: a swipe recognised as a tap never becomes an engine event.
#
# Exits 0, 1 on failure, 77 when there is no device.
#
# WHY adb AND NOT A DEBUG-ONLY INTENT. An IME's service cannot be started from
# `am start`; it is bound by the platform when the user selects it. And it
# cannot receive `onActivityResult` or a permission result at all
# (docs/DESIGN-FRONTEND.md Part B B5), so there is no callback path to turn tracing
# on from outside. What an IME *can* be driven with is a broadcast, so the sink
# is toggled by one -- which also means the toggle is testable on the emulator,
# not just claimed.
#
# The trace file is written into the app's own filesDir, because an IME writing
# anywhere else is a permissions question, and `adb shell run-as` is how it comes
# back out.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="org.hunmin.ime"
: "${ANDROID_HOME:=$HOME/Android/Sdk}"
ADB="$ANDROID_HOME/platform-tools/adb"
ACTION="org.hunmin.ime.TRACE"

skip() { echo "skip: $*" >&2; exit 77; }
[ -x "$ADB" ] || skip "no adb at $ADB"
"$ADB" get-state >/dev/null 2>&1 || skip "no device attached"

# Debug builds are `debuggable`, which is what makes run-as available. A
# release APK cannot be poked this way, and that is correct: a shipped IME does
# not accept a broadcast that turns on file writing.
require_debuggable() {
  if ! "$ADB" shell run-as "$PKG" true >/dev/null 2>&1; then
    echo "FAIL: $PKG is not debuggable, so its trace cannot be read." >&2
    echo "      A release APK must not be traceable by a broadcast." >&2
    exit 1
  fi
}

case_label="${2:-manual}"
source="${3:-}"

case "${1:-}" in
start)
  require_debuggable
  # An EMPTY `--es source ""` makes the device's sh hand `am` a bare word that
  # swallows the trailing package argument, so the broadcast goes out with no
  # target and no receiver sees it (measured 2026-11-27: the script said
  # "enabled" while the app never received it). Omit the extra when empty.
  if [ -n "$source" ]; then
    "$ADB" shell am broadcast -a "$ACTION" -f 0x20 \
        --es op enable --es case "$case_label" --es source "$source" \
        "$PKG" >/dev/null
  else
    "$ADB" shell am broadcast -a "$ACTION" -f 0x20 \
        --es op enable --es case "$case_label" \
        "$PKG" >/dev/null
  fi
  echo "trace enabled (case=$case_label)"
  echo "  -> pull it with: tools/e3-trace.sh pull <name>"
  ;;
stop)
  require_debuggable
  "$ADB" shell am broadcast -a "$ACTION" -f 0x20 --es op disable "$PKG" >/dev/null
  echo "trace closed"
  ;;
status)
  if "$ADB" shell dumpsys activity services "$PKG" 2>/dev/null \
       | grep -q "HunminTraceRecorder"; then
    echo "a trace sink appears active"
  else
    echo "no trace sink visible in dumpsys (it is off unless enabled)"
  fi
  ;;
mark)
  label="${2:?usage: e3-trace.sh mark <label>}"
  require_debuggable
  "$ADB" shell am broadcast -a "$ACTION" -f 0x20 --es op mark --es label "$label" "$PKG" >/dev/null
  echo "marked: $label"
  ;;
pull)
  name="${2:?usage: e3-trace.sh pull <name>}"
  require_debuggable
  out_dir="$ROOT/tests/conformance"
  mkdir -p "$out_dir"
  # The paths are fixed by HunminInputMethodService, so a pull is unambiguous.
  remote="/data/data/$PKG/files/e3.trace"
  ui_remote="/data/data/$PKG/files/ui.trace"
  dest="$out_dir/${name}.trace"
  ui_dest="$out_dir/${name}.ui.trace"
  have_engine=0; have_ui=0
  "$ADB" shell run-as "$PKG" test -f "$remote" && have_engine=1
  "$ADB" shell run-as "$PKG" test -f "$ui_remote" && have_ui=1
  if [ "$have_engine" = 0 ] && [ "$have_ui" = 0 ]; then
    echo "FAIL: no trace at $remote -- did the session run 'start' first?" >&2
    exit 1
  fi
  events=0; ui_events=0
  if [ "$have_engine" = 1 ]; then
    "$ADB" shell run-as "$PKG" cat "$remote" > "$dest"
    events=$(grep -c '"in"' "$dest" || true)
  fi
  if [ "$have_ui" = 1 ]; then
    "$ADB" shell run-as "$PKG" cat "$ui_remote" > "$ui_dest"
    ui_events=$(grep -c '"ui"' "$ui_dest" || true)
  fi

  # A trace with a meta line and NO events is not a result, it is an empty
  # session, and archiving it would be worse than archiving nothing: the next
  # person to read tests/conformance/ would take "no quirks found" from a
  # session in which the sink was never actually bound to the field. Measured:
  # enabling the sink writes meta immediately, so this file exists and looks
  # plausible even when zero keys were recorded. A UI-only session (symbol
  # commits never reach the engine) counts via its own events.
  if [ "$((events + ui_events))" -lt 1 ]; then
    rm -f "$dest" "$ui_dest"
    echo "FAIL: the traces hold meta lines and no events -- the sinks were" >&2
    echo "      enabled but never received input, which is what happens when" >&2
    echo "      opened before the IME owns the field. Nothing archived." >&2
    exit 1
  fi

  if [ "$have_engine" = 1 ]; then
    lines=$(grep -c . "$dest" || true)
    echo "pulled $lines line(s), $events engine event(s) -> ${dest#"$ROOT"/}"
    head -1 "$dest" | grep -q '^{"meta":' \
      || { echo "FAIL: the first line is not a meta object" >&2; exit 1; }
    echo "  meta: $(head -1 "$dest")"
  fi
  if [ "$have_ui" = 1 ]; then
    ui_lines=$(grep -c . "$ui_dest" || true)
    echo "pulled $ui_lines line(s), $ui_events UI event(s) -> ${ui_dest#"$ROOT"/}"
  fi
  ;;
*)
  sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
  exit 2
  ;;
esac
