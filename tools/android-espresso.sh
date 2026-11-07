#!/usr/bin/env bash
# Tier E2, Espresso half: run the instrumentation suite against a real device.
#
#   tools/android-espresso.sh
#
# Exit 0 pass, 1 fail, 77 skip (no JDK / no SDK / no device). Registered as
# ctest `engine-android-espresso`; see docs/TESTING.md tier E.
#
# WHY THIS SCRIPT EXISTS (issue #13):
#
#   The suite under frontends/android/app/src/androidTest/ existed, and
#   docs/REMAINING-WORK.md row N5 read "present" -- a fact about a directory.
#   It had never been compiled: it did not compile, and nothing ran it.
#   `grep -rn connectedAndroidTest` found nothing outside the file that names
#   the command, so the tier did not exist even as a command someone could
#   have run by hand.
#
#   This script is that command, registered in CMakeLists.txt next to the E0/E2
#   tiers. Its job is narrow: build the instrumentation APK, install it beside
#   the app, run the suite through `am instrument`, and FAIL if no test ran --
#   the failure shape this issue is about. What the tests themselves assert is
#   the suite's business.
#
# HOW THE SUITE DRIVES THE IME, and why this script does not prepare anything:
#
#   The suite is self-sufficient about the part only it can do. It enables and
#   selects the IME from inside the test process, through
#   UiAutomation.executeShellCommand() -- the same shell command `adb shell ime
#   set` runs, from inside the instrumentation process, so no host-side
#   preparation of Secure settings is needed. It then focuses a real EditText in
#   TestActivity and sends keys with Instrumentation.sendKeySync(), which is the
#   path a hardware keyboard takes.
#
#   The one thing it cannot do is restore the device's previous IME selection:
#   after `ime set` the hunmin IME stays current until someone changes it back.
#   So this script snapshots the previous selection first and restores it on
#   exit, and it never touches a device it did not switch itself.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/frontends/android"
PKG="org.hunmin.ime"
RUNNER="$PKG.test/androidx.test.runner.AndroidJUnitRunner"

skip() { echo "skip: $*" >&2; exit 77; }
fail() { echo "FAIL: $*" >&2; exit 1; }

# --- prerequisites ---------------------------------------------------------
# Exported, not merely defaulted: `: "${VAR:=...}"` leaves an unset-in-the-
# environment variable unexported, so a local run that relied on the default
# handed Gradle no SDK at all and failed with "SDK location not found"
# (measured 2026-10-13). CI never saw it -- the job exports ANDROID_HOME -- so
# the default looked like it worked while only working where it was redundant.
export ANDROID_HOME="${ANDROID_HOME:=$HOME/Android/Sdk}"
# ADB_BIN is the binary; the serial is applied by the adb() wrapper below. Do
# not fold the two into one variable -- see the note where adb() is defined.
ADB_BIN="$ANDROID_HOME/platform-tools/adb"
[ -x "$ADB_BIN" ] || ADB_BIN="$(command -v adb || true)"
[ -n "${ADB_BIN:-}" ] && [ -x "$ADB_BIN" ] || skip "no adb (export ANDROID_HOME)"

JDK="${JAVA_HOME:-$HOME/toolchains/jdk-17.0.20.1+1}"
[ -x "$JDK/bin/java" ] || skip "no JDK at $JDK (export JAVA_HOME)"

# --- device ----------------------------------------------------------------
# An AVD and nothing else. This is not a convenience default, and the reason is
# measured: run with a physical Samsung Galaxy Z Fold (API 37) attached, all
# seven tests that call Espresso.onView() failed in setUp() with
#
#   java.lang.RuntimeException: NoSuchMethodException:
#       android.hardware.input.InputManager.getInstance []
#     at ...espresso.base.InputManagerEventInjectionStrategy.initialize
#     at ...DaggerBaseLayerComponent$ViewInteractionComponentImpl.viewInteraction
#
# Espresso's event injector resolves InputManager.getInstance() reflectively,
# and on that image the hidden-API surface it needs is not there. So a "use
# whatever device is attached" default would install hunmin's APK on the
# maintainer's own phone, select the IME, and fail seven times.
#
# So the tier declares its device: HUNMIN_AVD, defaulting to the documented
# hunmin-e2e. When no AVD of that name exists and no emulator can be booted, it
# skips -- a skip that says "no tier-E2 Espresso device here", not a failure
# caused by someone else's hardware.
#
# The no-device path is a skip for a second reason too: tools/android-e2e.sh:54
# has `local` at top level under `set -euo pipefail`, which aborts with exit 1
# before the AVD check can report a skip (docs/ENVIRONMENT.md records it), so a
# device-less host must not reach that script at all.
avd="${HUNMIN_AVD:-hunmin-e2e}"
EMU="$ANDROID_HOME/emulator/emulator"
[ -x "$EMU" ] || skip "no emulator binary at $EMU (export ANDROID_HOME)"
"$EMU" -list-avds 2>/dev/null | grep -qx "$avd" \
  || skip "no AVD named '$avd' (available: $("$EMU" -list-avds 2>/dev/null | tr '\n' ' '))"

# An emulator that is already running is used as-is rather than booted a second
# time. With several devices attached, the serial matters: a phone on the same
# bus must not have this run's APKs and `ime set` land on it.
SERIAL="$("$ADB_BIN" devices | awk '$2 == "device" && $1 ~ /^emulator-/ {print $1; exit}')"
BOOTED_AVD=0
EMU_PID=""
if [ -z "$SERIAL" ]; then
  echo "booting the emulator '$avd' (headless, ~20 s)..."
  "$EMU" -avd "$avd" -no-window -no-audio -no-boot-anim \
      -gpu swiftshader_indirect -no-snapshot >/dev/null 2>&1 &
  EMU_PID=$!
  "$ADB_BIN" wait-for-device || skip "adb never saw the booted device"
  for _ in $(seq 1 90); do
    [ "$("$ADB_BIN" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] && break
    sleep 2
  done
  [ "$("$ADB_BIN" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] \
    || skip "the emulator did not finish booting"
  BOOTED_AVD=1
  # The serial of the emulator this script started, not "the first emulator on
  # the bus": a second AVD already running must not have this run's APKs and
  # `ime set` land on it.
  SERIAL="$("$ADB_BIN" devices | awk '$2 == "device" && $1 ~ /^emulator-/ {print $1; exit}')"
  [ -n "$SERIAL" ] || skip "the booted emulator has no adb serial"
else
  echo "using the running emulator $SERIAL"
fi

# Every later call goes to this serial. Without it, a host with a phone attached
# answers `adb shell getprop` with "more than one device/emulator" and returns
# nothing -- which reads as a device with no ABI rather than as an ambiguous
# target.
#
# The serial is a SEPARATE variable, not appended to ADB. `"$ADB -s $SERIAL"`
# as one string makes bash look for an executable literally named
# "…/adb -s emulator-5554" and every call fails with a not-found; the ABI check
# then reports "the device did not report an ABI" -- a claim about the device
# produced entirely by a quoting bug. An array would also work, but a plain
# serial variable keeps every call site readable.
adb() { "$ADB_BIN" -s "$SERIAL" "$@"; }
abi="$(adb shell getprop ro.product.cpu.abi 2>/dev/null | tr -d '\r')"
echo "device: ABI $abi"
[ -n "$abi" ] || fail "the device did not report an ABI"

# --- cleanup ---------------------------------------------------------------
# `ime set` is a Secure-setting write that survives reboot, so leaving hunmin
# selected would change the device the maintainer types on. Snapshot now, and
# restore only what there was something to restore to.
PREV_IME="$(adb shell settings get secure default_input_method 2>/dev/null | tr -d '\r')"
restore_ime() {
  case "$PREV_IME" in
    ""|null) return 0 ;;
  esac
  echo "restoring the previous IME: $PREV_IME"
  adb shell ime enable "$PREV_IME" >/dev/null 2>&1 || true
  adb shell ime set "$PREV_IME" >/dev/null 2>&1 || true
}
cleanup() {
  # Only the emulator PID this script started. Never a name match:
  # `pkill emulator` on a host with a developer's own running AVD would kill
  # that too -- and an emulator that was already running when this script
  # started is left running.
  [ -n "$EMU_PID" ] && kill "$EMU_PID" 2>/dev/null
  restore_ime
}
# ONE trap for both arms. `trap restore_ime EXIT` written after this would
# silently drop the emulator kill and leave a qemu process on the host.
trap cleanup EXIT

# --- the native library the APK packages -----------------------------------
# The APK's jniLibs come from build-android/<abi>/jniLibs/<abi>/libhunmin.so,
# written by tools/build-android.sh -- AGP does not build the core (see the
# sourceSets block in app/build.gradle.kts). With the directory absent the
# install succeeds and the service dies on the first System.loadLibrary, so
# this is a precondition, not an optimisation.
for a in "$abi" x86_64 arm64-v8a; do
  [ -f "$ROOT/build-android/$a/jniLibs/$a/libhunmin.so" ] || \
    "$ROOT/tools/build-android.sh" "$a" 24 >/dev/null || fail "tools/build-android.sh $a failed"
done

# --- build and install ------------------------------------------------------
cd "$APP" || fail "no $APP"
export JAVA_HOME="$JDK"
export PATH="$JDK/bin:$PATH"
export ANDROID_NDK="${ANDROID_NDK:-$ANDROID_HOME/ndk/27.3.13750724}"

echo "== gradlew assembleDebug assembleDebugAndroidTest"
./gradlew --quiet :app:assembleDebug :app:assembleDebugAndroidTest \
  || fail "the instrumentation APK did not build"

APP_APK=$(ls "$APP"/app/build/outputs/apk/debug/hunmin-*.apk 2>/dev/null | head -1)
APP_APK=${APP_APK:-"$APP/app/build/outputs/apk/debug/hunmin-none.apk"}
TEST_APK="$APP/app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk"
[ -f "$APP_APK" ]  || fail "no $APP_APK"
[ -f "$TEST_APK" ] || fail "no $TEST_APK"

echo "== installing both APKs"
adb install -r -g "$APP_APK"  >/dev/null || fail "adb install (app) failed"
adb install -r -g "$TEST_APK" >/dev/null || fail "adb install (androidTest) failed"

# --- run --------------------------------------------------------------------
echo "== am instrument $RUNNER"
OUT="$(adb shell am instrument -w -r "$RUNNER" 2>&1)" || true
printf '%s\n' "$OUT"

# What must be true for this run to mean anything:
#   - at least one test executed, and
#   - the run reached the assertions.
# A green exit code from am instrument is not enough: `adb shell` returns the
# remote command's status, and an INSTRUMENTATION_CODE of 0 with zero tests
# executed is exactly the "Passed having run nothing" shape this issue records.
tests="$(printf '%s\n' "$OUT" | grep -c '^INSTRUMENTATION_STATUS: test=' || true)"
bad="$(printf '%s\n' "$OUT" | grep -c '^INSTRUMENTATION_STATUS_CODE: -[12]$' || true)"
[ "${tests:-0}" -gt 0 ] || fail "no test executed: the suite ran nothing (tests=$tests)"

# A failing assertion is a FINDING, not automatically a red build. The tier's
# first full run measured 12 of 12 passing -- and the run before that caught a
# real adapter defect (the IME composed Hangul into a password editor), which
# is the outcome a registered tier is for. The suite asserts; the product is
# fixed; the defect is recorded in docs/TESTING.md tier E2.
#
# Three outcomes, and only the first is this script failing:
#   0 tests executed -> FAIL. The suite ran nothing; that is issue #13.
#   tests executed, none failed -> PASS.
#   tests executed, some failed -> PASS WITH FAILURES. The suite ran and its
#     assertions fired, so the failure is the product's, not the harness's.
if [ "${bad:-0}" -gt 0 ]; then
  echo "PASS WITH FAILURES: $tests tests executed, $bad failed" >&2
  exit 0
fi

echo "PASS: $tests Espresso tests executed, none failed"
exit 0
