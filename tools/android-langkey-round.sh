#!/usr/bin/env bash
# The Han/Eng key round on an Android DEVICE (issue #159, docs/OPTIONS-MEASURED.md §HEK-2).
#
#   tools/android-langkey-round.sh -s <serial>            # a device or the emulator
#   tools/android-langkey-round.sh -s emulator-5554
#
# What it measures, and why it is a script and not prose: which keycodes the
# Han/Eng key can be, and which of them ever reaches the IME's onKeyDown. The
# emulator and a device disagree on this, so both are worth a run.
#
# The HOLD (the 한/영 key down while a second key arrives) cannot be injected:
# `input` emits a down and an up as one unit. That part is measured by hand on
# hardware -- the script ends by printing the two steps and what to read.
set -euo pipefail
export MSYS2_ARG_CONV_EXCL='/sdcard;/data'

SERIAL=""
while [ $# -gt 0 ]; do
  case "$1" in
    -s) SERIAL="$2"; shift 2 ;;
    *) echo "usage: $0 -s <serial>" >&2; exit 2 ;;
  esac
done
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# cmake/adb.exe are native Windows programs and MSYS path translation is off on
# this host: a /c/... argument reaches adb as a path it cannot resolve (measured:
# "failed to stat /c/.../app-debug.apk"). cygpath -m gives the C:/... form.
command -v cygpath >/dev/null 2>&1 && ROOT="$(cygpath -m "$ROOT")"
PKG="org.hunmin.ime"
SVC="$PKG/.HunminInputMethodService"
APK=$(ls "$ROOT"/frontends/android/app/build/outputs/apk/debug/hunmin-*.apk 2>/dev/null | head -1)
APK=${APK:-"$ROOT/frontends/android/app/build/outputs/apk/debug/hunmin-none.apk"}
EDITOR_PKG="org.hunmin.testeditor"
EDITOR_APK="$ROOT/frontends/android/testeditor/build/outputs/apk/debug/testeditor-debug.apk"

: "${ANDROID_HOME:=$HOME/Android/Sdk}"
ADB="$ANDROID_HOME/platform-tools/adb"
[ -x "$ADB" ] || ADB="$ANDROID_HOME/platform-tools/adb.exe"
[ -x "$ADB" ] || { echo "skip: no adb under $ANDROID_HOME" >&2; exit 77; }
[ -n "$SERIAL" ] || { echo "usage: $0 -s <serial>" >&2; exit 2; }

state="$("$ADB" -s "$SERIAL" get-state 2>&1 | tr -d '\r' | head -1)"

if [ "$state" != "device" ]; then
  cat >&2 <<MSG
skip: $SERIAL is '$state', not 'device'.
      'unauthorized' means the phone is showing (or will show) "Allow USB
      debugging?" -- unlock it, tap Allow, tick "always allow from this
      computer", then re-run. Nothing on the host can answer that dialog.
MSG
  exit 77
fi

"$ADB" -s "$SERIAL" shell getprop ro.product.model
"$ADB" -s "$SERIAL" shell getprop ro.build.version.sdk
"$ADB" -s "$SERIAL" shell getprop ro.product.cpu.abi

[ -f "$APK" ] || { echo "skip: no APK at $APK (run tools/build-android.sh <abi> && gradlew assembleDebug)" >&2; exit 77; }
[ -f "$EDITOR_APK" ] || { echo "skip: no test editor APK (gradlew :testeditor:assembleDebug)" >&2; exit 77; }

"$ADB" -s "$SERIAL" install -r -g "$APK" >/dev/null
"$ADB" -s "$SERIAL" install -r -g "$EDITOR_APK" >/dev/null
"$ADB" -s "$SERIAL" shell ime enable "$SVC" >/dev/null
"$ADB" -s "$SERIAL" shell ime set "$SVC" >/dev/null
"$ADB" -s "$SERIAL" shell am start -n "$EDITOR_PKG/.EditorActivity" >/dev/null
sleep 3

# Tap the editor and confirm the service is the one running, or every keycode
# row below measures the previous IME.
"$ADB" -s "$SERIAL" shell uiautomator dump /sdcard/ui.xml >/dev/null 2>&1
BOUNDS="$("$ADB" -s "$SERIAL" shell cat /sdcard/ui.xml | tr '>' '\n' | grep "$EDITOR_PKG:id/editor" | grep -o 'bounds="[^"]*"' | head -1)"
echo "editor bounds: ${BOUNDS:-unknown (tap it yourself)}"
if [ -n "$BOUNDS" ]; then
  set -- $(echo "$BOUNDS" | tr -cd '0-9,][ \n' | tr -c '0-9\n' ' ')
  X0=$1; Y0=$2; X1=$3; Y1=$4
  "$ADB" -s "$SERIAL" shell input tap $(( (X0 + X1) / 2 )) $(( (Y0 + Y1) / 2 ))
  sleep 1
fi
"$ADB" -s "$SERIAL" shell dumpsys input_method | grep -m1 mCurMethodId

"$ADB" -s "$SERIAL" logcat -c
# The candidate keycodes, 204 LAST: on this image 204 switches the IME, so a
# 204-first sweep measures the NEXT IME for every key after it (measured).
# 218 KANA (Samsung 한/영),
# 212 EISU, and the three modifier families for the positive control.
for k in 218 212 58 59 113 204; do
  "$ADB" -s "$SERIAL" shell input keyevent "$k"
  sleep 0.4
done
sleep 1

echo "--- IME onKeyDown rows (the measured answer) ---"
"$ADB" -s "$SERIAL" logcat -d | grep "onKeyDown: keyCode" || echo "(none: no key reached the IME)"
echo "--- IME after the 204 press (did the framework take it?) ---"
"$ADB" -s "$SERIAL" shell dumpsys input_method | grep -m1 mCurMethodId

cat <<'MSG'

--- the HOLD, by hand (injection cannot do it) ---
1. With the IME selected, hold the convention's 한/영 key (a Korean keyboard's
   한/영 key, or a BT/OTG keyboard's equivalent) and press a letter; release.
2. adb logcat -d | grep -i "hunmin" — the Han/Eng branch logs
   "Hangul/English toggle triggered by keyCode=" only on the single press; a
   chord must NOT log it, and the letter must compose.
Nothing in the current service tracks a held key, so today this step measures
the PLATFORM (does a second key arrive while 한/영 is down) rather than the
adapter; the adapter half is the Windows shape in docs/OPTIONS-MEASURED.md §HEK-3.
MSG
