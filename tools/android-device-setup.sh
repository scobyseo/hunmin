#!/usr/bin/env bash
# Set up an Android device for MANUAL testing of the hunmin IME (issue #299).
#
#   tools/android-device-setup.sh [serial]
#
# Installs/updates the debug APK and the test editor, enables and selects the
# IME, and launches the editor so there is a field to type into. Existing app
# data (layout choice, mode, bindings) is preserved: `install -r`.
#
# Device selection is EXPLICIT: the serial argument, or ANDROID_SERIAL. With
# neither, the script proceeds only when exactly one device is attached —
# this host routinely has a physical phone AND an AVD, and an implicit choice
# could install onto the wrong one. The previous default IME is printed with
# the command that restores it.
#
# This is a setup tool, not a test tier: it asserts nothing about composition.
# The measured tiers are tools/android-e2e.sh (AVD) and ctest engine-android-e1.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${ANDROID_HOME:=$HOME/Android/Sdk}"
ADB="$ANDROID_HOME/platform-tools/adb"
[ -x "$ADB" ] || { echo "no adb at $ADB" >&2; exit 1; }

PKG="org.hunmin.ime"
SVC="$PKG/.HunminInputMethodService"
EDITOR_PKG="org.hunmin.testeditor"
APK=$(ls "$ROOT"/frontends/android/app/build/outputs/apk/debug/hunmin-*.apk 2>/dev/null | head -1)
EDITOR_APK="$ROOT/frontends/android/testeditor/build/outputs/apk/debug/testeditor-debug.apk"
if [ -z "$APK" ] || [ ! -f "$EDITOR_APK" ]; then
  echo "APK missing. Build first:" >&2
  echo "  (cd frontends/android && ./gradlew :app:assembleDebug :testeditor:assembleDebug)" >&2
  echo "with JAVA_HOME, ANDROID_HOME and ANDROID_NDK exported." >&2
  exit 1
fi

SERIAL="${1:-${ANDROID_SERIAL:-}}"
if [ -z "$SERIAL" ]; then
  mapfile -t DEVS < <("$ADB" devices | awk 'NR>1 && $2=="device" {print $1}')
  if [ "${#DEVS[@]}" -eq 1 ]; then
    SERIAL="${DEVS[0]}"
  else
    echo "refusing to guess: ${#DEVS[@]} devices attached. Pass a serial or set ANDROID_SERIAL:" >&2
    "$ADB" devices -l >&2
    exit 1
  fi
fi
export ANDROID_SERIAL="$SERIAL"
"$ADB" get-state >/dev/null 2>&1 || { echo "device $SERIAL is not reachable" >&2; exit 1; }
MODEL=$("$ADB" shell getprop ro.product.model | tr -d '\r')
API=$("$ADB" shell getprop ro.build.version.sdk | tr -d '\r')
echo "device: $SERIAL ($MODEL, API $API)"

PREV=$("$ADB" shell settings get secure default_input_method | tr -d '\r')
echo "current default IME: ${PREV:-none}"

echo "installing $(basename "$APK") (data preserved)"
"$ADB" install -r -g "$APK"
echo "installing the test editor"
"$ADB" install -r -g "$EDITOR_APK"

"$ADB" shell ime enable "$SVC" >/dev/null
"$ADB" shell ime set "$SVC"
SELECTED=$("$ADB" shell settings get secure default_input_method | tr -d '\r')
[ "$SELECTED" = "$SVC" ] || { echo "IME was not selected (got '$SELECTED')" >&2; exit 1; }
echo "hunmin is the selected IME"

"$ADB" shell am start -n "$EDITOR_PKG/.EditorActivity" >/dev/null 2>&1 || true

cat <<GUIDE

테스트 안내 (test editor가 실행되었습니다):
  · 한글 입력 후 스트립의 [한/영] 또는 키패드 [한/영] 키로 영어 전환
  · [⇧] 한 번 탭 = 다음 한 글자만 쉬프트 (원샷), 다시 탭 = 취소
  · [기호] = 기호 페이지 (32개 ASCII 기호 + 탐색키), [한/영] = 원래 모드로 복귀
  · 기본 한글 배열은 sin3-p2 입니다. 두벌식으로 바꾸려면:
      스트립의 설정(기어) → 한글 배열 → 두벌식
  · 물리 키보드(블루투스 등)를 연결하면 키 그리드가 자동으로 숨습니다 (KP12).
    스트립은 남습니다 — 하드웨어 키 입력 경로입니다.
  · 음성 입력은 마이크 권한이 설치 시 부여되어 있습니다 (-g).

원래 입력기로 복원:
  adb -s $SERIAL shell ime set ${PREV:-<이전 IME>}
GUIDE