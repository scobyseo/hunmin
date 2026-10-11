# Release build — the R8 shrinker path end-to-end

This checklist records the measurement issue #4 asked for: does R8 actually run in
`assembleRelease`, and is the resulting APK usable on the AVD. Until 2026-10-11 the
answer was "the rules exist, the shrinker has never run" — see the measured row in
`docs/REMAINING-WORK.md` §3 ("The R8 shrinker path end-to-end").

**Status: MEASURED 2026-10-11 on this host (AVD `hunmin-e2e`, API 35, x86_64). All
four checks below passed.** This is a manual/environment tier like E3: nothing in CI
runs `assembleRelease` (`tools/ctest-select.sh` cannot reach a device), so re-run it
by hand after any change to `proguard-rules.pro`, the release `signingConfig`, or the
JNI shim's CMakeLists.

## Why this is a checklist and not a script

`tools/android-e2e.sh` asserts against the **debug** APK by design — E2's job is the
platform routing contract, and a debug build is enough for it. The property here is
different: that the *shrinker* did not take anything the running IME needs. That is
only visible in a release build, and the acceptance is "the IME installs, appears,
and composes" — which needs an emulator, so it is environment-bound.

## Procedure (exactly what was run 2026-10-11)

```sh
export JAVA_HOME=$HOME/toolchains/jdk-17.0.20.1+1
export ANDROID_HOME=$HOME/Android/Sdk
export ANDROID_NDK=$ANDROID_HOME/ndk/27.3.13750724

# 0. Host preconditions. The engine .so per ABI must exist first — the JNI shim's
#    CMake prints exactly this instruction as a FATAL_ERROR when it does not:
./tools/build-android.sh x86_64 24
./tools/build-android.sh arm64-v8a 24

#    ANDROID_NDK must point at r27 (docs/ENVIRONMENT.md); note AGP's own toolchain
#    still compiles the shim with the SDK cmake/3.22.1 + default NDK 26.1.10909125.

# 1. The release build. Exit code must be 0.
./frontends/android/gradlew -p frontends/android assembleRelease
#    -> frontends/android/app/build/outputs/apk/release/app-release.apk

# 2. The shrinker actually ran: the mapping outputs must exist.
ls -l frontends/android/app/build/outputs/mapping/release/
#    -> mapping.txt configuration.txt seeds.txt usage.txt
#    -> every class named by a -keep in app/proguard-rules.pro appears in seeds.txt
#       and maps to ITSELF in mapping.txt (JNI names survive).

# 3. Install the release APK and register it with the platform.
$ANDROID_HOME/emulator/emulator -avd hunmin-e2e \
    -no-window -no-audio -no-boot-anim -gpu swiftshader_indirect -no-snapshot &
#    wait for: adb shell getprop sys.boot_completed  -> 1
adb install -r frontends/android/app/build/outputs/apk/release/app-release.apk
adb shell ime list -a | grep -i hunmin
#    -> org.hunmin.ime/.HunminInputMethodService

# 4. Composition on the shrunk build. `input keyevent`, NOT `input text` —
#    for the reason measured in tools/android-e2e.sh:259-266. The Settings search
#    field must first be tapped into a real EditText (android-e2e.sh:112-130), and
#    the first keypress after selecting the IME is warm-up (android-e2e.sh:275-295).
adb shell ime enable org.hunmin.ime/.HunminInputMethodService
adb shell ime set    org.hunmin.ime/.HunminInputMethodService
#    open Settings, tap the search bar, warm up (KEYCODE_K, KEYCODE_DEL), then:
for k in D K S S U D; do adb shell input keyevent KEYCODE_$k; sleep 0.2; done
adb shell uiautomator dump /sdcard/ui.xml >/dev/null
adb shell cat /sdcard/ui.xml | tr '>' '\n' | grep -m1 'class="android.widget.EditText"'
#    -> text="ㅎ곈디"  (the tier A sin3-p2 dkssud record, android-e2e.sh:355-371)
```

## What the 2026-10-11 run observed, beyond the checks

- APK 1,505,137 bytes; both ABIs inside (`lib/arm64-v8a` + `lib/x86_64`,
  `libhunmin.so` and `libhunjni.so` each) — R8 does not touch the native libs, and
  they were verified present in the release APK.
- The signingConfig hardcoded to the debug keystore installed without complaint
  (issue #1 records the hardcoding; it is not a build blocker).
- One warm-up artifact worth knowing before you "fix" it: a `pm clear` of the IME's
  own package resets the platform's selected-IME setting back to Latin, and the
  first injected keyevent after re-selecting goes to the previous editor path. Both
  are properties of the injected-event path, already documented in
  `tools/android-e2e.sh`; they are NOT R8 damage.

## Do NOT

- Do not edit build config to make a red run green; a failure here is the
  measurement (issue #4's constraint).
- Do not kill an emulator you did not start (docs/ENVIRONMENT.md host rules).
