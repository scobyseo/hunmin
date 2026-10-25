---
title: "CI lessons from the Android job"
created: 2026-10-08
updated: 2026-10-25
type: concept
status: active
tags: [concept, ci, android]
sources:
  - ../../docs/CI-TIMING.md
  - ../../.github/workflows/ci.yml
confidence: high
---

---
title: "Android CI Lessons Learned"
date: 2026-10-14
tags: [ci, android, github-actions, emulator]
status: closed
---

# Android CI on GitHub Actions: Lessons Learned

## Problem Summary
Issue #58: `sdkmanager: command not found` in the "Install the Android SDK" step.

## Root Cause
`GITHUB_PATH` only affects **subsequent steps**, not the current step. The workflow appended the cmdline-tools path to `GITHUB_PATH` then immediately called `sdkmanager` in the same step.

## Fix Applied
```yaml
# In the "Install the Android SDK" step:
export PATH="$sdk_root/cmdline-tools/latest/bin:$PATH"
# Also for next step's avdmanager:
echo "$sdk_root/cmdline-tools/latest/bin" >> "$GITHUB_PATH"
echo "$sdk_root/platform-tools" >> "$GITHUB_PATH"
```

## Additional Issues Discovered & Fixed

### 1. ANDROID_SDK_ROOT Conflict
Runner has pre-existing `ANDROID_SDK_ROOT=/usr/local/lib/android/sdk` conflicting with our `ANDROID_HOME`.
```yaml
echo "ANDROID_SDK_ROOT=$sdk_root" >> "$GITHUB_ENV"
```

### 2. AVD Location Mismatch
`avdmanager` creates AVDs in `$HOME/.config/.android/avd` but emulator looks in `$HOME/.android/avd`.
```yaml
echo "ANDROID_AVD_HOME=$HOME/.config/.android/avd" >> "$GITHUB_ENV"
export ANDROID_AVD_HOME="$HOME/.config/.android/avd"
```

### 3. Host Build for Tests
Cross-compile (`build-android.sh`) only builds Android libs. Engine tests need host `build/libhunmin.so`.
```yaml
- run: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHUNMIN_TRACE_BUILD=ON
- run: cmake --build build --target hunmin
```

### 4. Disk Space Constraints
GitHub runners have ~2-3GB free; emulator needs ~7GB for pixel_6 AVD.
```yaml
# Disk cleanup before emulator
sudo apt-get clean
sudo rm -rf /usr/share/dotnet /opt/ghc /opt/hostedtoolcache/CodeQL
docker system prune -af --volumes 2>/dev/null || true
# Use smaller device profile
avdmanager create avd ... --device pixel -c 512M --force
# Reduce emulator RAM
emulator ... -memory 1536 -no-quickboot
```

### 5. IME Registration Timing
Need delay after `adb install` before `ime enable`.
```bash
"$ADB" install -r -g "$APK"
sleep 3
"$ADB" shell pm list packages "$PKG" >/dev/null || fail "package not found"
```

### 6. uiautomator Dump Reliability
`uiautomator dump` frequently returns empty/corrupt output on GitHub runners.
```bash
dump_ui() {
  for i in 1 2 3 4 5; do
    "$ADB" shell uiautomator dump --windows "$1" >/dev/null 2>&1 || true
    dump="$("$ADB" shell cat "$1" 2>/dev/null || true)"
    [ -n "$dump" ] && [ "$(printf '%s' "$dump" | wc -l)" -gt 5 ] && { printf '%s' "$dump"; return 0; }
    sleep 2
  done
  return 1
}
```

### 7. parse_bounds Robustness
```bash
parse_bounds() {
  local nums
  nums="$(printf '%s' "$1" | grep -o '[0-9][0-9]*' | tr '\n' ' ' || true)"
  [ -n "$nums" ] && printf '%s' "$nums"
}
```

## Costly Tests to Avoid Repeating

| Test | Time | Cost |
|------|------|------|
| Android SDK install | ~2 min | Downloads 1GB+ |
| Emulator boot | 30-60s | Needs KVM |
| Gradle assembleDebug | 3-5 min | Downloads deps |
| Tier E2 (android-e2e) | 2-3 min | uiautomator flaky |

## Recommended CI Flow
1. **Linux job**: All host tests (tier A/B/C/D, conformance)
2. **Android job**: Only E0/E1 (no emulator) - pushes lib + runs JVM tests
3. **E2/E3**: Run locally or on self-hosted runner with sufficient disk

## Environment Notes
- GitHub Free/Pro/Team/Enterprise runners: ~14GB total, ~2-3GB free (fixed)
- For emulator tests: use self-hosted runner or GitHub Enterprise larger runners
- Billing issues can halt all CI runs unexpectedly

## Related

- [[test-scenario-guide]] — which tier runs where.
- [[2026-10-10-ctest-labels]] — the labels these jobs select.

**Tier-job mapping corrected 2026-10-25:** this page said the Android job runs
only `E0`/`E1` and that `E2`/`E3` run locally. Measured against the workflow and
`docs/TESTING.md`: the automated Android tiers are `E0`/`E1`/`E2`, there is no
`E3`, and the linux job excludes `tier-E1` because the android job already runs
those tests.
