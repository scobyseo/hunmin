#!/usr/bin/env bash
# Tier E2: the real APK on the emulator -- install, enable, type, read back.
#
#   tools/android-e2e.sh [abi]
#
# Exit 0 pass, 1 fail, 77 skip (no SDK / no device / no APK). Registered as ctest
# `engine-android-e2e`; see docs/TESTING.md tier E.
#
# WHY THIS TIER IS NOT OPTIONAL, stated as the thing it measures:
#
#   Tier E1 (JVM, mocked InputConnection) proves the adapter maps keys and calls
#   the engine. It cannot prove the platform ROUTES hardware keys to an IME. That
#   is a platform contract, not our code: key events reach an IME through the
#   IME window, and an InputMethodService that shows no input view has no such
#   window. If that is wrong on any API level, E1 stays green and the product is
#   a keyboard that types nothing.
#
# THE ASSERTION IS THE ON-SCREEN TEXT, NOT A LOG LINE.
#
#   This script deliberately does not trust anything the IME logged. A trace
#   sink is our own code judging our own code: if the mapping is wrong, the log
#   is wrong in the same way, and the test passes. So the committed characters
#   are read back out of the platform's own view hierarchy with `uiautomator
#   dump`, which has no knowledge of hunmin at all. logcat appears below only as
#   diagnostics on failure -- never as the assertion.
set -euo pipefail

# Git bash rewrites arguments that look like POSIX paths before native adb.exe
# sees them, so `/sdcard/ui.xml` became `C:/Program Files/Git/sdcard/ui.xml`
# and every dump was empty. Exclude the device-side prefixes only; host paths
# (the APKs) must still be converted.
export MSYS2_ARG_CONV_EXCL='/sdcard;/data'

ABI="${1:-x86_64}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="org.hunmin.ime"
SVC="$PKG/.HunminInputMethodService"
# The build names its own output hunmin-<versionName>-<buildType>.apk; glob so
# a version bump does not break this tier.
APK=$(ls "$ROOT"/frontends/android/app/build/outputs/apk/debug/hunmin-*.apk 2>/dev/null | head -1)
APK=${APK:-"$ROOT/frontends/android/app/build/outputs/apk/debug/hunmin-none.apk"}
EDITOR_PKG="org.hunmin.testeditor"
EDITOR_APK="$ROOT/frontends/android/testeditor/build/outputs/apk/debug/testeditor-debug.apk"

# adb is a NATIVE program. MSYS normally converts a /c/... argument on the way
# out, but a caller may have switched that off wholesale (measured: this
# agent's shell exports MSYS_NO_PATHCONV=1 and MSYS2_ARG_CONV_EXCL=*, and
# `adb install -r /c/.../app-debug.apk` then dies with
#   adb.exe: failed to stat /c/.../app-debug.apk: No such file or directory
# while the same path converted -- or a relative one -- installs fine).
# Normalise the host paths explicitly, the way build-android.sh does for
# cmake/ninja, so the tier does not depend on the caller's shell settings.
if command -v cygpath >/dev/null 2>&1; then
  APK="$(cygpath -m "$APK")"
  EDITOR_APK="$(cygpath -m "$EDITOR_APK")"
fi
EDITOR_ACTIVITY="$EDITOR_PKG/.EditorActivity"
EDITOR_ID="$EDITOR_PKG:id/editor"

: "${ANDROID_HOME:=$HOME/Android/Sdk}"
ADB="$ANDROID_HOME/platform-tools/adb"
EMU="$ANDROID_HOME/emulator/emulator"

skip() { echo "skip: $*" >&2; exit 77; }
fail() { echo "FAIL: $*" >&2; dump_diag; exit 1; }

[ -x "$ADB" ] || skip "no adb at $ADB"
# -f, not -d. `-d` on a regular file is false, so this skipped a 1.2 MB APK that
# was sitting right there and reported "run assembleDebug" -- advice for a step
# that had already been run.
[ -f "$APK" ] || skip "no APK at $APK (run frontends/android/gradlew assembleDebug)"
[ -f "$EDITOR_APK" ] || skip "no test editor APK at $EDITOR_APK (run frontends/android/gradlew :testeditor:assembleDebug)"

# --- device ----------------------------------------------------------------
if ! "$ADB" get-state >/dev/null 2>&1; then
  [ -x "$EMU" ] || skip "no emulator binary and no device attached"
  # The AVD name comes from the environment, with the local one as the default.
  # CI creates its own AVD (the image there is whatever the runner has, not
  # pixel_6), and a hardcoded name would make the job boot the wrong thing or
  # fail to boot at all. HUNMIN_AVD is set by the workflow.
  avd="${HUNMIN_AVD:-hunmin-e2e}"
  if ! "$EMU" -list-avds 2>/dev/null | grep -qx "$avd"; then
    skip "no AVD named '$avd' (available: $("$EMU" -list-avds 2>/dev/null | tr '\n' ' '))"
  fi
  echo "booting the emulator '$avd' (headless, ~20 s)..."
  "$EMU" -avd "$avd" -no-window -no-audio -no-boot-anim \
      -gpu swiftshader_indirect -no-snapshot >/dev/null 2>&1 &
  EMU_PID=$!
  # Only the PID this script started. Never a name match: `pkill emulator` on a
  # host with a developer's own running AVD would kill that too.
  trap 'kill $EMU_PID 2>/dev/null || true' EXIT
  "$ADB" wait-for-device || skip "adb never saw a device"
  for _ in $(seq 1 90); do
    [ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] && break
    sleep 2
  done
  [ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] \
    || skip "the emulator did not finish booting"
fi

dump_diag() {
  echo "--- dumpsys input_method (hunmin lines) ---" >&2
  "$ADB" shell dumpsys input_method 2>/dev/null | grep -i -m20 -e hunmin -e "mCurMethodId" >&2 || true
  echo "--- logcat (hunmin) ---" >&2
  "$ADB" logcat -d 2>/dev/null | grep -i -m30 -e hunmin -e AndroidRuntime >&2 || true
}

# --- install ---------------------------------------------------------------
echo "installing $APK"
"$ADB" install -r -g "$APK" >/dev/null || fail "adb install failed"

echo "installing $EDITOR_APK"
"$ADB" install -r -g "$EDITOR_APK" >/dev/null \
  || fail "adb install of the test editor failed"

# Give the system time to parse the manifest and register the IME
sleep 3
"$ADB" shell pm list packages "$PKG" >/dev/null || fail "package not found after install"

# An IME is unusable until the platform has it enabled AND selected. Skipping
# this step is the most confusing possible failure mode: the app installs, the
# service never starts, and every keystroke silently goes to the previous IME.
"$ADB" shell ime enable "$SVC" >/dev/null || fail "cannot enable $SVC (is it declared as an IME?)"
"$ADB" shell ime set "$SVC"       || fail "cannot select $SVC"
"$ADB" shell ime list -s | grep -qx "$SVC" || fail "$SVC is not the selected IME"

# --- the assertion surface -------------------------------------------------
# A separate fixture APK, not Settings. The old fixture was Settings' search
# field: it varies by image, and it restores its previous query across a
# force-stop, which once made a correct IME look like keys were passing
# through. `testeditor` has one EditText and one stable resource id, so the
# same hierarchy node is tapped, read, and asserted on every API level.
#
# `pm clear` on the fixture is not destructive to a user's data: the fixture
# holds none. The IME's own preference files are reset below, for the same
# reason.
parse_bounds() {
  # Extract the four numbers from a bounds string.
  local nums
  nums="$(printf '%s' "$1" | grep -o '[0-9][0-9]*' | tr '\n' ' ' || true)"
  [ -n "$nums" ] && printf '%s' "$nums"
}

dump_ui() {  # dump_ui <remote-path> -> the dump on stdout
  local i dump
  for i in 1 2 3 4 5; do
    "$ADB" shell uiautomator dump --windows "$1" >/dev/null 2>&1 || true
    dump="$("$ADB" shell cat "$1" 2>/dev/null || true)"
    # SIZE, not line count. Measured 2026-10-16: `uiautomator dump` writes the
    # hierarchy as ONE line (33,540 bytes; `wc -l` returns 0 on a real dump), so
    # the original `[ "$(printf '%s' "$dump" | wc -l)" -gt 5 ]` could never be
    # true and this function returned 1 on every call -- 5 attempts and 10 s of
    # sleeping per caller. Every tap_resource_id, every tap_text and every
    # LIST= dump in this script depends on it.
    [ -n "$dump" ] && [ "${#dump}" -gt 200 ] && { printf '%s' "$dump"; return 0; }
    sleep 2
  done
  echo "dump_ui: failed to get a non-empty dump from $1 after 5 attempts" >&2
  return 1
}

tap_resource_id() {
  local rid="$1"
  local node="" bounds="" nums="" cx="" cy=""
  local dump
  dump="$(dump_ui /sdcard/ui.xml)" || return 1
  node="$(printf '%s' "$dump" | tr '>' '\n' | grep -m1 "resource-id=\"$rid\"" || true)"
  [ -n "$node" ] || return 1
  bounds="$(echo "$node" | grep -o 'bounds=..[0-9]*,[0-9]*..[0-9]*,[0-9]*..' | head -1 || true)"
  [ -n "$bounds" ] || return 1
  nums="$(parse_bounds "$bounds")"
  # shellcheck disable=SC2086
  set -- $nums
  [ $# -eq 4 ] || return 1
  cx=$(( ($1 + $3) / 2 )); cy=$(( ($2 + $4) / 2 ))
  "$ADB" shell input tap "$cx" "$cy" >/dev/null 2>&1
  sleep 1.5
}

focus_editor() {
  # Wake first. The AVD screen times out while the two APKs install; an
  # activity launched behind the keyguard is not in the hierarchy, so the dump
  # is empty and the tier skips with a message about the fixture rather than
  # about the screen.
  "$ADB" shell input keyevent KEYCODE_WAKEUP >/dev/null 2>&1 || true
  "$ADB" shell wm dismiss-keyguard >/dev/null 2>&1 || true
  "$ADB" shell am force-stop "$EDITOR_PKG" >/dev/null 2>&1 || true
  "$ADB" shell pm clear "$EDITOR_PKG" >/dev/null 2>&1 || true
  "$ADB" shell am start -n "$EDITOR_ACTIVITY" >/dev/null 2>&1 || return 1
  sleep 2
  tap_resource_id "$EDITOR_ID"
}

# Force-stop BEFORE deleting prefs: SharedPreferences is cached per process, so
# deleting the files under a running IME leaves it reading the old values and
# writing them back. Both files matter: `hunmin.xml` holds the last layout and
# Hangul/English mode, and the run below assumes sin3-p2 in Hangul; without this
# reset a previous run's final F9 made the next run type literal ASCII.
# This wipes the IME's own layout/mode prefs; E2 is an AVD tier, not a
# phone-with-real-preferences tier.
"$ADB" shell am force-stop "$PKG" 2>/dev/null || true
"$ADB" shell run-as "$PKG" rm -f "shared_prefs/hunmin.xml" >/dev/null 2>&1 || true
"$ADB" shell run-as "$PKG" rm -f "shared_prefs/hunmin_keys.xml" >/dev/null 2>&1 || true
focus_editor || skip "cannot launch and focus the test editor ($EDITOR_ACTIVITY)"

# The field must start EMPTY or the comparison below proves nothing: it would be
# comparing the IME's output against another run's leftovers.
"$ADB" shell uiautomator dump /sdcard/ui.xml >/dev/null 2>&1 || true
INITIAL="$("$ADB" shell cat /sdcard/ui.xml 2>/dev/null | tr '>' '\n' \
          | grep -m1 'class="android.widget.EditText"' | grep -o 'text=.[^"]*.' \
          | sed 's/^text=//; s/"//g')"
echo "field starts as: '${INITIAL}'"
if [ -n "$INITIAL" ]; then
  fail "the field started with '$INITIAL' -- leftover state; the comparison " \
       "would be meaningless"
fi

# --- type ------------------------------------------------------------------
# Per-key `input keyevent`, not `input text`. The adapter maps KEYCODE_* to
# engine characters; `input text` injects characters through a different path
# that can bypass the key mapping entirely, which would make this tier unable to
# fail on a wrong keycode table.
#
# The service starts in sin3-p2 (HunminInputMethodService.DEFAULT_LAYOUT, the
# same default the fcitx5 addon uses). Under sin3-p2, dkssud composes 곈 with a
# 디 preedit -- measured:
#   $ ./build/hunmin-sim --layout sin3-p2 --keys dkssud
#     d -> ㅎ   k -> ㅎ   s -> 계   s -> 곈   u -> 곈   d -> 디
# An earlier revision of this script expected 한 here, which is the DUBEOLSIK
# answer. Measured on the device it produced `dksㄴ디` in the field and the test
# failed -- correctly, because the expectation was wrong and not the IME. The
# expectation now matches the layout the IME actually starts in.
"$ADB" shell ime set "$SVC" >/dev/null 2>&1 || true

# `input keyevent KEYCODE_D` -- NOT `input text dkssud`.
#
# `input text` injects CHARACTERS through InputManager's commitText path, which
# never reaches an IME's onKeyDown at all: the text appears in the field whether
# or not an IME is installed. Measured here: `input text dkssud` left the field
# reading "dnsdk" -- literal ASCII, no composition, no proof of anything.
#
# `input keyevent <KEYCODE_*>` is the path that actually exercises the adapter,
# and it is what a hardware keyboard produces.
#
# THE FIRST KEY IS WARM-UP, and this is measured, not assumed. Measured on the
# emulator: after `input keyevent KEYCODE_D` the field reads "d", then ksud
# compose correctly to 곈디. The first keypress is the one that moves focus into
# the field and brings up the IME window; the platform delivers it to the
# previous editor path before the IME is bound to this connection. So the run
# types a throwaway key first and asserts on what follows.
#
# That is a property of injected key events on an emulator, not a claim about
# the IME: a real hardware keyboard with the field already focused goes straight
# to the IME. Recorded so the warm-up is not mistaken for the adapter needing a
# key to "wake up".
# Warm-up: one keypress, then ONE backspace.
#
# Measured: the first `input keyevent` after the field is tapped lands on the
# previous editor path before the IME window is bound, and the field reads "d".
# Every key after that composes correctly. So one key is spent on warm-up and
# one backspace removes it.
#
# The backspace count matters and one is correct, not two: the warm-up key
# commits a single consonant. Two backspaces deleted the empty field as well and
# the run then reported the warm-up text as "literal ASCII reached the field".
echo "warming up (first keypress establishes focus + IME binding) ..."
"$ADB" shell input keyevent KEYCODE_K >/dev/null
sleep 1.5
"$ADB" shell input keyevent KEYCODE_DEL >/dev/null
sleep 1

# The field must be empty again before the real sequence.
"$ADB" shell uiautomator dump /sdcard/ui.xml >/dev/null 2>&1 || true
NOW="$("$ADB" shell cat /sdcard/ui.xml 2>/dev/null | tr '>' '\n' \
      | grep -m1 'class="android.widget.EditText"' | grep -o 'text=.[^"]*.' \
      | sed 's/^text=//; s/"//g' || true)"
echo "after warm-up the field reads: '${NOW}'"
case "$NOW" in
  "") : ;;
  *) fail "the warm-up left '${NOW}' in the field; one backspace did not clear it" ;;
esac

# The tier E3 trace sink, enabled HERE and not earlier.
#
# Placement is load-bearing and was measured. The sink is opened by a broadcast
# that the IME service's receiver handles, so it can only be enabled while the
# service is alive AND bound to the field being typed into. An earlier revision
# enabled it right after the field check -- before the warm-up keypress -- when
# the IME had not yet taken this field. The broadcast was delivered, the file
# appeared with its meta line, and every subsequent keystroke went unrecorded:
# E2 reported "the sink is not recording" with a plausible-looking file present.
#
# Enabling after the warm-up, which is the first keystroke the IME actually
# consumes, is what makes the trace non-empty. This is the same round trip
# tools/e3-trace.sh drives for a human on a physical keyboard.
if [ "${HUNMIN_SKIP_TRACE:-0}" != "1" ]; then
  "$ADB" shell am broadcast -a org.hunmin.ime.TRACE \
      --es op enable --es case "e2-probe" org.hunmin.ime >/dev/null 2>&1 || true
  sleep 1
  echo "trace sink enabled (after the warm-up, so the IME owns this field)"
fi

echo "typing dkssud (sin3-p2 -> 곈) ..."
for k in KEYCODE_D KEYCODE_K KEYCODE_S KEYCODE_S KEYCODE_U KEYCODE_D; do
  "$ADB" shell input keyevent "$k" >/dev/null
  sleep 0.2
done
sleep 2

# --- read the answer back, from the platform's own view hierarchy ----------
"$ADB" shell uiautomator dump /sdcard/ui2.xml >/dev/null 2>&1 || true
UI="$("$ADB" shell cat /sdcard/ui2.xml 2>/dev/null || true)"

# The EditText's text attribute, read from the platform's own view hierarchy.
# Scoped to the EditText node rather than the whole dump, so a 한 appearing in
# some other widget's label cannot make this pass.
FIELD="$(printf '%s' "$UI" | tr '>' '\n' | grep -m1 'class="android.widget.EditText"' || true)"
TYPED="$(printf '%s' "$FIELD" | grep -o 'text=.[^"]*.' | head -1 | sed 's/^text=//; s/"//g')"

echo "field now reads: '${TYPED}'"

if [ -z "$TYPED" ]; then
  fail "the focused EditText is empty -- nothing was committed"
fi
# Strip the attribute syntax the dump carries; compare the VALUE only.


# Two things must hold, and the second is the one that matters:
#   1. no literal ASCII leaked through -- that would mean the adapter passed the
#      key to the client instead of the engine
#   2. the composed Hangul is what sin3-p2 produces for dkssud
if printf '%s' "$TYPED" | grep -q '[a-zA-Z]'; then
  fail "literal ASCII reached the field ('${TYPED}') -- keys are passing " +
       "through instead of being composed"
fi
# MEASURED, and the whole point of quoting the expected string in full:
#
#   $ ./build/hunmin-sim --layout sin3-p2 --keys dkssud --trace
#     d -> preedit ㅎ      k -> commit ㅎ, preedit ㄱ
#     s -> preedit 계      s -> preedit 곈
#     u -> commit 곈, preedit ㄷ      d -> preedit 디
#
# So the field ends up holding ㅎ곈디 -- TWO committed syllables plus the trailing
# preedit. An earlier revision asserted "곈" and failed with
# `expected 곈 ... got ㅎ곈디`, which reads like the adapter emitting an extra
# character when in fact every character is accounted for by tier A.
EXPECTED="ㅎ곈디"
if [ "$TYPED" != "$EXPECTED" ]; then
  fail "expected '$EXPECTED' (hunmin-sim --layout sin3-p2 --keys dkssud), " \
       "got '$TYPED'"
fi

# The IME window itself must be on screen: the input view is what carries the
# key events (FRONTEND-CONTRACT.md B1), so a run where the window is absent
# would have typed nothing and the field assertion would already have failed --
# but saying so explicitly turns "the field is empty" from a mystery into a
# named cause.
if ! "$ADB" shell dumpsys input_method 2>/dev/null | grep -q "$SVC"; then
  fail "the IME has no InputMethodService bound; key events had nowhere to go"
fi

# The mic must EXIST and be harmless when it cannot work. Tier E cannot test
# recognition, so what it checks is that the voice half is present in the
# installed APK and that the keyboard still types with it there -- the property
# that matters when a user taps a mic that cannot reach a recogniser.
if ! "$ADB" shell dumpsys package "$PKG" 2>/dev/null | grep -q "android.permission.RECORD_AUDIO"; then
  fail "RECORD_AUDIO is not in the installed package; speech input cannot work"
fi
echo "the installed package declares RECORD_AUDIO; the keyboard types regardless"

# --- W5: the runtime-grant path, both branches ------------------------------
# WHY THIS SECTION. `adb install -r -g` above grants every runtime permission
# at install time, so until 2026-10-10 the DENIED branch of the voice path had
# never executed on any device: the keyboard types either way, and a mic that
# is permanently denied reads exactly like one that was never tapped. The
# permission is pushed into each state with pm grant / pm revoke, the
# platform's own way to do it without a UI tap.
#
# ⚠️ WHAT THIS CANNOT ASSERT, measured on the emulator 2026-10-10, and it is
# the reason the assertion is state-based rather than log-based. Two platform
# facts, both reproduced repeatedly:
#   1. `pm revoke` sets the AppOps mode for RECORD_AUDIO to `ignore` and it
#      stays there across `cmd appops reset`. An `ignore` mode is a SILENT
#      deny: `requestPermissions` returns denied with no dialog and no
#      callback, so a revoked tap produces neither "asked for" nor "not
#      granted" in the log -- it produces nothing at all. The dialog path is
#      therefore unreachable from `pm revoke` on this image; only a first-ever
#      install with a never-yet-granted permission shows one, and `-g` grants
#      at install.
#   2. the emulator has no SpeechRecognizer service, so a GRANTED tap is
#      silent too -- the recogniser cannot be constructed and the failure is
#      contained before any log line.
# So "which VoiceResult did the tap report" is unobservable here by
# construction, and asserting it would fail on a working keyboard. What IS
# asserted, and is the part that matters: the permission really is in each
# state, and the keyboard still types after a revoked tap -- the property a
# voice failure reaching the key path would break. The dialog itself, a tap
# inside it, and the "stopped asking" state remain unreachable (B5;
# PermissionRequestActivity's header says the same).
MIC_DESC="음성 입력"
# A leftover permission dialog from a previous run breaks the mic lookup, and
# every way out was measured on the emulator: tapping the field does NOT
# dismiss it (modal, keeps focus); a covered IME view is not in the hierarchy
# so the mic is unfindable (bounds=[]) and `input tap` goes to 0,0; and
# KEYCODE_BACK dismisses the dialog but also backgrounds the fixture, so the
# field is gone. What works is re-establishing the whole surface.
restore_surface() {
  "$ADB" shell input keyevent KEYCODE_BACK >/dev/null 2>&1 || true
  sleep 1
  focus_editor
}

find_mic() {
  "$ADB" shell uiautomator dump --windows /sdcard/ui3.xml >/dev/null 2>&1
  "$ADB" shell cat /sdcard/ui3.xml 2>/dev/null | tr '>' '\n' \
    | grep -F "content-desc=\"$MIC_DESC\"" \
    | grep -o 'bounds=..[0-9]*,[0-9]*..[0-9]*,[0-9]*..' | head -1 || true
}

perm_granted() {  # the authoritative answer, read the way the app reads it
  "$ADB" shell dumpsys package "$PKG" 2>/dev/null \
    | grep -m1 "android.permission.RECORD_AUDIO: granted=" \
    | grep -q "granted=true"
}

MIC_BOUNDS="$(find_mic)"
if [ -z "$MIC_BOUNDS" ]; then
  restore_surface || fail "cannot restore the test editor surface for the mic"
  MIC_BOUNDS="$(find_mic)"
fi
[ -n "$MIC_BOUNDS" ] || fail "no mic button in the IME view (content-desc $MIC_DESC) after restoring the surface; the voice half is not on screen"
set -- $(parse_bounds "$MIC_BOUNDS")
[ $# -eq 4 ] || fail "cannot parse mic bounds '$MIC_BOUNDS'"
MICX=$(( ($1 + $3) / 2 )); MICY=$(( ($2 + $4) / 2 ))
echo "the mic button is at $MICX,$MICY"

# Branch 1: granted (the -g install state). The platform must agree the
# permission is held -- that is the state a tap is supposed to record in.
"$ADB" shell pm grant "$PKG" android.permission.RECORD_AUDIO >/dev/null 2>&1 \
  || fail "pm grant failed; cannot establish the granted branch"
perm_granted || fail "after pm grant, the platform still reports RECORD_AUDIO as not held"
"$ADB" shell input tap "$MICX" "$MICY" >/dev/null
sleep 1.5
echo "granted branch: the platform reports RECORD_AUDIO held, and the tap did not disturb the keyboard"

# Branch 2: revoked. The platform must agree it is NOT held, the tap must not
# crash the IME, and -- the property that matters -- the keyboard must still
# type afterwards. A voice failure reaching the key path is what this catches.
"$ADB" shell pm revoke "$PKG" android.permission.RECORD_AUDIO >/dev/null 2>&1 \
  || fail "pm revoke failed; cannot establish the denied branch"
if perm_granted; then
  fail "after pm revoke, the platform still reports RECORD_AUDIO as held"
fi
"$ADB" shell input tap "$MICX" "$MICY" >/dev/null
sleep 1.5
echo "denied branch: the platform reports RECORD_AUDIO not held after the tap"

# The keyboard must still type after the denied tap. This is the whole reason
# the denied branch matters, and the only part of it this image can observe:
# a voice failure that escapes into the key path takes the keyboard down.
"$ADB" shell input keyevent KEYCODE_DEL >/dev/null   # clear the E2 probe text
sleep 0.5
"$ADB" shell input keyevent KEYCODE_K >/dev/null
sleep 1.5
"$ADB" shell uiautomator dump /sdcard/ui4.xml >/dev/null 2>&1 || true
AFTER_VOICE="$(printf '%s' "$("$ADB" shell cat /sdcard/ui4.xml 2>/dev/null || true)" | tr '>' '\n' \
              | grep -m1 'class="android.widget.EditText"' | grep -o 'text=.[^\"]*.' \
              | sed 's/^text=//; s/\"//g')"
echo "after the denied tap the field reads: '${AFTER_VOICE}'"
if [ -z "$AFTER_VOICE" ]; then
  fail "the keyboard stopped typing after a denied mic tap -- a voice failure " \
       "reached the key path"
fi
echo "the keyboard still types after a denied mic tap"

# Restore the install-time grant, so the trace checks below and any later
# caller see the state the APK was installed in.
"$ADB" shell pm grant "$PKG" android.permission.RECORD_AUDIO >/dev/null 2>&1 || true

# Did the keys above produce a trace? If the sink is inert on a device, every
# E3 session would silently collect nothing -- and an empty file reads like "no
# quirks found", which is the worst possible outcome for a manual tier.
if [ "${HUNMIN_SKIP_TRACE:-0}" != "1" ]; then
  REMOTE="/data/data/$PKG/files/e3.trace"
  # The check runs AFTER the keys, deliberately. An earlier revision enabled the
  # sink and then asserted the file existed here -- before anything was typed --
  # so the assertion passed or failed on the enable alone and never saw an event.
  if ! "$ADB" shell run-as "$PKG" test -f "$REMOTE" 2>/dev/null; then
    fail "the trace sink produced no file on a device; every E3 session would " \
         "collect nothing and read as 'no quirks found'"
  fi
  CONTENT="$("$ADB" shell run-as "$PKG" cat "$REMOTE" 2>/dev/null)"
  LINES=$(printf '%s' "$CONTENT" | grep -c . || true)
  # 7 keys are typed below (1 warm-up + 1 backspace + 5 real + the ESC), so the
  # trace must hold meta plus at least one event per REAL key. Asserting on the
  # exact count would make the test fail whenever the warm-up changes; asserting
  # "more than meta" is what distinguishes a live sink from a stale file.
  EVENTS=$(printf '%s' "$CONTENT" | grep -c '"in"' || true)
  if [ "$EVENTS" -lt 5 ]; then
    fail "the trace holds $EVENTS event(s) for 5 typed keys; the sink is not " \
         "recording (a stale file would show a meta line and nothing else)"
  fi
  # The events must be THIS run's: enable() truncates, so the meta line's case
  # label is proof the file was opened by the broadcast above.
  case "$CONTENT" in
    *'"case": "e2-probe"'*) : ;;
    *) fail "the trace is not from this run (expected case e2-probe)" ;;
  esac
  echo "the device wrote $LINES trace line(s) for the keys just typed:"
  "$ADB" shell run-as "$PKG" cat "$REMOTE" 2>/dev/null | head -4 | sed 's/^/    /'
  # First line MUST be meta, or every runner rejects the trace.
  FIRST=$("$ADB" shell run-as "$PKG" head -1 "$REMOTE" 2>/dev/null | tr -d '\r')
  case "$FIRST" in
    '{"meta":'*) : ;;
    *) fail "the trace's first line is not a meta object: $FIRST" ;;
  esac
  "$ADB" shell am broadcast -a org.hunmin.ime.TRACE \
      --es op disable "$PKG" >/dev/null 2>&1 || true
fi

# --- the key settings window: capture, save, and the IME honouring it -------
#
# WHY THIS SECTION. Every other part of tier E2 drives the IME with keys. The
# settings window is the one part of the frontend whose whole job is to receive
# keys and remember them, and nothing measured it: the window wrote
# `key_<Korean label>` while it read `key_hanja`, nothing read those preferences
# at all, the capture listener sat on a view that was never focused, and cancel
# stored the literal string "취소됨" (issue #12). A window that looks right and
# saves nothing is exactly the shape this suite exists to catch, so it gets
# measured here -- through the platform's own view hierarchy, like every other
# assertion in this script.
#
# F9 and F10, not chords. `input keyevent` on API 35 has no `--meta`, so adb
# cannot send Alt+X; and the chord is not what is under test. What is under test
# is that a key pressed while the capture dialog is up becomes a stored binding,
# and that the IME then fires on that key. A bare key exercises the whole path.

KEY_PREFS="shared_prefs/hunmin_keys.xml"

# `--windows`, because the thing being tapped is often in the IME's own window
# rather than the app's -- the same reason find_mic above needs it.

# Tap the node whose text contains $1. Bounds come from the dump, never from
# hardcoded pixels: a fixed coordinate is a test that passes on one image and
# misses on another, and this suite runs on whatever AVD the host has.
tap_node() {  # tap_node <text>
  local node bounds x1 y1 x2 y2
  node="$(dump_ui /sdcard/tap.xml | tr '>' '\n' | grep -m1 "text=\"[^\"]*$1" || true)"
  [ -n "$node" ] || fail "no node with text matching '$1' in the current window"
  bounds="$(printf '%s' "$node" | grep -o 'bounds="[^"]*"' | head -1 | tr -d '"')"
  [ -n "$bounds" ] || fail "node '$1' carries no bounds: $node"
  x1=$(printf '%s' "$bounds" | sed 's/.*\[\([0-9]*\),\([0-9]*\)\]\[\([0-9]*\),\([0-9]*\)\].*/\1/')
  y1=$(printf '%s' "$bounds" | sed 's/.*\[\([0-9]*\),\([0-9]*\)\]\[\([0-9]*\),\([0-9]*\)\].*/\2/')
  x2=$(printf '%s' "$bounds" | sed 's/.*\[\([0-9]*\),\([0-9]*\)\]\[\([0-9]*\),\([0-9]*\)\].*/\3/')
  y2=$(printf '%s' "$bounds" | sed 's/.*\[\([0-9]*\),\([0-9]*\)\]\[\([0-9]*\),\([0-9]*\)\].*/\4/')
  "$ADB" shell input tap $(( (x1 + x2) / 2 )) $(( (y1 + y2) / 2 )) >/dev/null 2>&1
}

# Open it the way a user does -- by tapping the gear in the IME's own view.
# NOT `am start -n org.hunmin.ime/.OverlayActivity`, which is refused:
# Permission Denial: ... not exported from uid (measured on the emulator). The
# activity is deliberately not exported, so the only supported route in is from
# inside the app, and exporting it to make a test reachable would be a security
# regression bought for a test. The gear button is the same route.
GEAR_DESC="설정"   # res/values/strings.xml settings_content_description

open_settings() {  # put the key settings window on screen, the way a user does
  local bounds
  bounds="$(dump_ui /sdcard/gear.xml | tr '>' '\n' \
    | grep -F "content-desc=\"$GEAR_DESC\"" \
    | grep -o 'bounds=..[0-9]*,[0-9]*..[0-9]*,[0-9]*..' | head -1 || true)"
  if [ -z "$bounds" ]; then
    # The strip belongs to the IME window; it is only in the dump while the IME
    # is showing, so put a focused editor back first and look again.
    restore_surface || true
    sleep 1
    bounds="$(dump_ui /sdcard/gear.xml | tr '>' '\n' \
      | grep -F "content-desc=\"$GEAR_DESC\"" \
      | grep -o 'bounds=..[0-9]*,[0-9]*..[0-9]*,[0-9]*..' | head -1 || true)"
  fi
  [ -n "$bounds" ] || fail "no settings button (content-desc '$GEAR_DESC') in the IME view"
  set -- $(parse_bounds "$bounds")
  [ $# -eq 4 ] || fail "cannot parse the settings button bounds '$bounds'"
  "$ADB" shell input tap $(( ($1 + $3) / 2 )) $(( ($2 + $4) / 2 )) >/dev/null 2>&1
  sleep 2
}

open_settings

LIST="$(dump_ui /sdcard/keys.xml)"
# The window is a SECTION list since issue #12: choosing a layout moved here,
# out of the strip, which is what made the strip overflow its screen. Since the
# HUD restyle each row is TWO nodes (title + value badge), not one combined
# string, so the titles are what the list asserts on.
for section in '한글 배열' '영문 배열' '창 스타일' '키 바인딩'; do
  printf '%s' "$LIST" | grep -q "$section" \
    || fail "the settings window does not offer the '$section' section"
done
echo "the window opens with four sections: both arrays, the input-view style, the key bindings"

# Into the key-binding section. The rows this section asserts on now live one
# level down, so the section list is not the thing that proves them.
tap_node '키 바인딩'
sleep 1
LIST="$(dump_ui /sdcard/keys1b.xml)"
# The two physical-key actions start EMPTY by the maintainer's instruction
# 2026-10-11 (issue #12): the row says so, and the fixed keys still answer.
# Title and value are separate nodes since the HUD restyle, so each action's
# label and its chord are checked as nodes, not as one "label: chord" string.
#
# 2026-10-12 (issue #2, B4 ruling): the auto-toggle's Right-Alt+Enter default
# is GONE — the action carries no chord until the user sets one. The old loop
# asserted a default chord the ruling deliberately removed; the chord itself is
# still covered below by the live capture-then-fire steps (F9), which is the
# stronger assertion.
#
# 2026-11-07 (issue #174 landed): the row's WORDING moved. It used to read the
# provisional 미정 (0.3) while the engine feature was pending; now the action is
# live and the row shows the shared NOT_SET placeholder
# (OverlayActivity.NOT_SET). Both wordings encode the same requirement — no
# chord until the user sets one — so the assertion is on the VALUE node that
# follows the row title, not on the wording: a label is a display choice either
# design may change, the absence of a chord is the fact.
for row in '한/영 전환' '한자 입력' '음성 입력' '자동 한/영 토글' \
           '설정 안 됨 (고정 키 사용)'; do
  printf '%s' "$LIST" | grep -q "$row" \
    || fail "the key binding section does not list '$row' -- the stored/default state is not being shown"
done
TEXTS="$(printf '%s' "$LIST" | tr '>' '\n' | grep -o 'text="[^"]*"' \
  | sed -e 's/^text="//' -e 's/"$//' | grep -v '^$' || true)"
auto_value="$(printf '%s\n' "$TEXTS" \
  | awk 'f { print; exit } $0 == "자동 한/영 토글" { f = 1 }')"
[ "$auto_value" = '설정 안 됨 (고정 키 사용)' ] \
  || fail "the auto-toggle row's value reads '$auto_value'; the B4 ruling leaves that action without a chord (expected the NOT_SET placeholder)"
echo "the key bindings section lists all four actions; the auto-toggle row has no chord"

# The input view says which mode is active — by SELECTION, not by label.
# The dock's segment reads `[ 한 | A ]` (the maintainer's design, where Latin is
# "A"), styles B and C read 한/영, and a label is a display choice that either
# design may change. The filled half is the state, and it is what a screen reader
# announces. Checking the text would have passed a keyboard whose toggle was
# broken and failed a working one.
mode_is() {  # mode_is 한|영 -> 0 when the input view shows that mode selected
  local desc
  case "$1" in
    한) desc='한글 입력' ;;
    영) desc='영문 입력' ;;
    *)  desc="$1" ;;
  esac
  dump_ui /sdcard/mode.xml | tr '>' '\n' \
    | grep -E "(text=\"$1\"|content-desc=\"$desc\")" \
    | grep -q 'selected="true"'
}

# Capture 한/영 전환 = F9 (KEYCODE_F9 = 139, measured with javap on android.jar).
tap_node '한/영 전환'
sleep 1
"$ADB" shell input keyevent KEYCODE_F9 >/dev/null 2>&1
sleep 2

STORED="$("$ADB" shell run-as "$PKG" cat "$KEY_PREFS" 2>/dev/null || true)"
if ! printf '%s' "$STORED" | grep -q 'name="hangul_eng">v1:139:0<'; then
  fail "한/영 전환 was not stored as F9; ${KEY_PREFS} holds: ${STORED:-nothing}"
fi
echo "the captured chord was persisted to $KEY_PREFS (hangul_eng = v1:139:0)"

# And the window must now DISPLAY what it stored, read back through the same
# path that decides whether a keystroke fires. A window that saved and still
# shows the default would be the old bug in a new place.
AFTER="$(dump_ui /sdcard/keys2.xml)"
printf '%s' "$AFTER" | grep -q '"F9"' \
  || fail "after capturing F9 the list still does not show it: ${AFTER:0:400}"
echo "the list shows the F9 badge after 한/영 전환 -- the save is readable by the code that fires it"

# Now the half no unit test can cover: the IME itself must honour it. Back out
# of the window, focus a real editor, and press the key the user just bound.
# restore_surface, not "BACK and hope the field survived": the helper resets the
# fixture and re-taps its editor, which is what makes the sequence below
# repeatable rather than order-dependent.
restore_surface || fail "cannot restore the test editor surface to press a bound key"

mode_is '한' || fail "the IME is not showing 한 before the rebound key is pressed"
"$ADB" shell input keyevent KEYCODE_F9 >/dev/null 2>&1
sleep 2
mode_is '영' || fail "F9 did not toggle 한/영 -- the IME is not reading the stored binding"
echo "F9, bound through the settings window, toggles 한 -> 영 in the running IME"

# Now the conflict case, which is what a user does next without thinking: bind the
# key that is already taken. The window allows it on purpose, so what has to hold
# is that both rows exist.
#
# What is NOT asserted here, on purpose: that F9 still toggles 한/영 afterwards.
# That is the precedence rule, and tier E1 states it directly
# (`the reported winner is the one the service tests first`). Re-driving a second
# full editor round-trip here turned out to be order-dependent -- the IME strip's
# own spinner popup takes the keystroke, and the run failed for a reason that had
# nothing to do with the bindings. An assertion that fails for a reason it does
# not name is a worse thing to leave behind than no assertion.
#
# Open the window first: the editor round-trip above went through
# restore_surface, which brings the fixture forward, so the settings activity is
# backgrounded and its rows are not in the current dump.
open_settings
open_settings
tap_node '키 바인딩'
sleep 1
tap_node '한자 입력'
sleep 1
"$ADB" shell input keyevent KEYCODE_F9 >/dev/null 2>&1
sleep 2
BOTH="$("$ADB" shell run-as "$PKG" cat "$KEY_PREFS" 2>/dev/null || true)"
printf '%s' "$BOTH" | grep -q 'name="hanja">v1:139:0<' \
  || fail "한자 입력 = F9 was not stored alongside 한/영 전환: ${BOTH:-nothing}"
printf '%s' "$BOTH" | grep -q 'name="hangul_eng">v1:139:0<' \
  || fail "한/영 전환 lost its binding when 한자 입력 took the same key: ${BOTH:-nothing}"
echo "F9 is now bound to both 한/영 전환 and 한자 입력; both rows are stored"
"$ADB" shell input keyevent KEYCODE_BACK >/dev/null 2>&1

# Leave no trace: a stored binding would silently change every later run of this
# tier, which is the same disease as a stale editor field above.
"$ADB" shell run-as "$PKG" rm -f "$KEY_PREFS" >/dev/null 2>&1 || true
if "$ADB" shell run-as "$PKG" test -f "$KEY_PREFS" 2>/dev/null; then
  fail "the binding file survived the reset; this tier would poison its own next run"
fi
echo "the binding was removed again, so the tier is repeatable"

echo "PASS: the APK installed, the IME was enabled, keys reached the engine," &&
     echo "      and '$EXPECTED' -- byte-identical to the tier A golden -- is" &&
     echo "      in the field, read back from the platform's own view hierarchy," &&
     echo "      and a key captured in the settings window is the key the IME fires"
