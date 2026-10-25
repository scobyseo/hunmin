---
title: "Test scenario guide: which tier runs what"
created: 2026-10-08
updated: 2026-10-25
type: concept
status: active
tags: [concept, testing, ci]
sources:
  - ../../docs/TESTING.md
confidence: high
---

---
title: "Test Scenario Development Guide"
date: 2026-10-14
tags: [testing, ci, android, development]
status: active
---

# Test Scenario Development Guide

This guide documents the test architecture, CI lessons, and patterns for adding new test scenarios to the hunmin project.

## Test Tier Architecture

The tier vocabulary is `docs/TESTING.md` §The vocabulary — cited here, not
restated, because this page's original table contradicted it (tier-B was written
as "Engine on Android" when tier-B is the ABI and data contracts, tier-D as the
Android mutation tier when tier-D1/D2/D3 are the three fcitx5 tiers, and an `E3`
tier was listed that the tree does not have). What this page keeps is the shape
that is not in `TESTING.md`: which tier a change touches, and the CI limits a
tier runs under.


## CI Environment Constraints (GitHub Actions)

| Resource | Limit | Workaround |
|----------|-------|------------|
| Disk free | ~2-3GB | `apt-get clean`, remove dotnet/ghc/CodeQL, `docker prune` |
| RAM | 7GB | Emulator `-memory 1536`, no quickboot |
| CPU | 2 cores | Emulator warns about 4 cores |
| Time | 45 min/job | Split tiers across jobs |
| Billing | Can halt CI | Monitor account health |

## Adding a New Test Scenario

### 1. Engine-Level Test (Tier A)
```bash
# 1. Add JSONL to tests/golden/
# 2. Run locally:
./build/hunmin-sim --data libhunmin/data --layout sin3-p2 --check tests/golden/new-test.jsonl

# 3. CI runs it automatically via ctest label "tier-A"
```

### 2. Conformance Test (Tier C)
```bash
# 1. Generate trace from golden:
python3 tools/gen-conformance.py --golden tests/golden/x.jsonl \
    --platform x11 --layout sin3-p2 --case new-case \
    --out tests/conformance/x11/new-case.trace.in

# 2. Verify:
python3 tools/trace-check.py tests/conformance/x11/new-case.trace.in \
    --runner build/frontends/fcitx5/test/trace-emit --emit
```

### 3. Android Test (Tier E0/E1)
```bash
# 1. Build engine for device:
export ANDROID_NDK=~/Android/Sdk/ndk/27.3.13750724
./tools/build-android.sh x86_64 24

# 2. Run E0 (goldens on device):
./tools/android-golden-check.sh

# 3. Run E1 (JVM tests):
./tools/android-unit.sh
```

### 4. Full E2E Test (Tier E2)
```bash
# 1. Build host engine (for test harness):
cmake -B build && cmake --build build --target hunmin

# 2. Cross-compile & build APK:
./tools/build-android.sh x86_64 24
cd frontends/android && ./gradlew assembleDebug

# 3. Run E2 (requires emulator):
./tools/android-e2e.sh x86_64
```

## CI Job Patterns

### Fast Feedback Job (Linux)
```yaml
# Runs on every push
# - Tier A/B/C/D (host only)
# - No Android SDK, no emulator
# - ~3 minutes
```

### Android Validation Job
```yaml
# Runs on Android-relevant changes
# - Install SDK, create AVD
# - Cross-compile, build APK
# - Run E0 + E1 (no emulator boot)
# - ~5 minutes
```

### Full E2E Job (Expensive)
```yaml
# Runs on schedule / manual
# - Includes emulator boot
# - Runs E2 + Espresso
# - ~15 minutes
# - Use self-hosted runner if possible
```

## Debugging CI Failures

### "sdkmanager: command not found"
```yaml
# Wrong: GITHUB_PATH doesn't affect current step
echo "$PATH" >> "$GITHUB_PATH"
sdkmanager --install ...

# Correct: export in same step
export PATH="$sdk_root/cmdline-tools/latest/bin:$PATH"
sdkmanager --install ...
```

### "avdmanager: command not found"
```yaml
# Add to GITHUB_PATH for NEXT step
echo "$sdk_root/cmdline-tools/latest/bin" >> "$GITHUB_PATH"
```

### "ANDROID_SDK_ROOT conflict"
```yaml
echo "ANDROID_SDK_ROOT=$sdk_root" >> "$GITHUB_ENV"
```

### "Unknown AVD name"
```yaml
echo "ANDROID_AVD_HOME=$HOME/.config/.android/avd" >> "$GITHUB_ENV"
export ANDROID_AVD_HOME="$HOME/.config/.android/avd"
```

### "No space left on device"
```yaml
# Pre-emulator cleanup
sudo apt-get clean
sudo rm -rf /usr/share/dotnet /opt/ghc /opt/hostedtoolcache/CodeQL
docker system prune -af --volumes 2>/dev/null || true
```

### "tr: write error: Broken pipe" (uiautomator)
```bash
# Use dump_ui with retry
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

## Local Development Workflow

```bash
# 1. Make changes
# 2. Build & test host:
cmake -B build && cmake --build build
ctest --test-dir build -LE slow

# 3. Test Android cross-compile:
./tools/build-android.sh x86_64 24

# 4. If E2 needed, use local emulator:
# (requires Android Studio / SDK installed locally)
cd frontends/android && ./gradlew assembleDebug
./tools/android-e2e.sh x86_64

# 5. Before push, run tier C trace check:
python3 tools/trace-check.py tests/conformance/... --runner build/... --emit
```

## Key Principles

1. **Host tests first** - Fast, deterministic, no emulator
2. **Cross-compile separately** - `build-android.sh` per ABI
3. **Emulator is last resort** - Expensive, flaky, resource-constrained
4. **E0/E1 without emulator** - Engine tests don't need IME UI
5. **Document flakiness** - uiautomator, emulator boot, disk space
6. **Monitor billing** - CI stops silently when account has issues

## Related

- [[the-rules-keep-breaking]] — a tier that cannot fail is the failure this
  guide's labelling exists to prevent.
- [[2026-10-10-ctest-labels]] — the label design and the two silent defects.
