#!/usr/bin/env bash
# Tier E3: control the shipped IME's trace sink over adb, and pull the result.
#
#   tools/e3-trace.sh start [case-label] [source]   begin a trace
#   tools/e3-trace.sh stop                         close it
#   tools/e3-trace.sh pull <name>                  fetch to tests/conformance/<name>.trace
#   tools/e3-trace.sh status                       is one open?
#
# Exits 0, 1 on failure, 77 when there is no device.
#
# WHY adb AND NOT A DEBUG-ONLY INTENT. An IME's service cannot be started from
# `am start`; it is bound by the platform when the user selects it. And it
# cannot receive `onActivityResult` or a permission result at all
# (docs/FRONTEND-CONTRACT.md B5), so there is no callback path to turn tracing
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
  "$ADB" shell am broadcast -a "$ACTION" \
      --es op enable --es case "$case_label" --es source "$source" \
      "$PKG" >/dev/null
  echo "trace enabled (case=$case_label)"
  echo "  -> pull it with: tools/e3-trace.sh pull <name>"
  ;;
stop)
  require_debuggable
  "$ADB" shell am broadcast -a "$ACTION" --es op disable "$PKG" >/dev/null
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
pull)
  name="${2:?usage: e3-trace.sh pull <name>}"
  require_debuggable
  out_dir="$ROOT/tests/conformance"
  mkdir -p "$out_dir"
  # The path is fixed by HunminInputMethodService, so a pull is unambiguous.
  remote="/data/data/$PKG/files/e3.trace"
  if ! "$ADB" shell run-as "$PKG" test -f "$remote"; then
    echo "FAIL: no trace at $remote -- did the session run 'start' first?" >&2
    exit 1
  fi
  dest="$out_dir/${name}.trace"
  "$ADB" shell run-as "$PKG" cat "$remote" > "$dest"

  # A trace with a meta line and NO events is not a result, it is an empty
  # session, and archiving it would be worse than archiving nothing: the next
  # person to read tests/conformance/ would take "no quirks found" from a
  # session in which the sink was never actually bound to the field. Measured:
  # enabling the sink writes meta immediately, so this file exists and looks
  # plausible even when zero keys were recorded.
  events=$(grep -c '"in"' "$dest" || true)
  if [ "$events" -lt 1 ]; then
    rm -f "$dest"
    echo "FAIL: the trace holds a meta line and no events -- the sink was" >&2
    echo "      enabled but never received a key, which is what happens when" >&2
    echo "      it is opened before the IME owns the field. Nothing archived." >&2
    exit 1
  fi

  lines=$(grep -c . "$dest" || true)
  echo "pulled $lines line(s), $events event(s) -> ${dest#"$ROOT"/}"
  # A trace whose meta is missing is rejected by every runner, so say so here
  # rather than leaving it to fail later in a conformance test.
  head -1 "$dest" | grep -q '^{"meta":' \
    || { echo "FAIL: the first line is not a meta object" >&2; exit 1; }
  echo "  meta: $(head -1 "$dest")"
  ;;
*)
  sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
  exit 2
  ;;
esac
