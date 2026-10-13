---
title: B8 2026-10-13 — keystore path contradiction resolved
created: 2026-10-13
updated: 2026-10-13
type: decision
status: active
tags: [decision, android, release-plan]
sources:
  - ../../docs/REMAINING-WORK.md
  - ../../frontends/android/app/build.gradle.kts
  - ../../frontends/android/keymaps/build.gradle.kts
confidence: high
---

# B8 2026-10-13 — keystore path contradiction resolved

**Maintainer, 2026-10-13** — issue #1 (B8) is ruled.

## What was ruled

The keystore config lives in gitignored `local.properties`
(`frontends/android/local.properties`); nothing in committed build config
hardcodes a host path. `storeFile`, `storePassword`, `keyPassword`, and
`keyAlias` are all read from that single gitignored file by both Android
modules, and no host path (`/home/scoby/...`) appears anywhere in the
committed build files.

In the tree this is two things, both done:

1. `frontends/android/app/build.gradle.kts` (signingConfigs `release`) and
   `frontends/android/keymaps/build.gradle.kts` read the keystore values
   from `local.properties` via `rootProject.file("local.properties")`.
2. Both modules honor the `HUNMIN_RELEASE_STORE` environment override
   (the CI `android-release` job and `tools/build-release.sh` hand a
   generated keystore to Gradle through that env var), falling back to
   generating a throwaway release key at build time when no keystore is
   present. The old hardcoded
   `storeFile = file("/home/scoby/.android/debug.keystore")` lines are
   removed.

See [[decisions-belong-to-the-maintainer]] for why a ruling is recorded
rather than presumed.

## What this does NOT close

- The maintainer's choice of the permanent canonical keystore location is
  still theirs to make: keep `local.properties` as the documented place
  (matching what the issue text stated), or move it. Either way the tree
  now contains no contradiction.
- The throwaway-key fallback in the `else` branch is a build-only
  convenience, not a releasable signature. Whether to keep it, shrink it,
  or wire a real key at release time is the maintainer's call.
- A real key must never be committed; `local.properties` stays gitignored.

## Verification

- `grep -rn "/home/scoby"` across `frontends/android/*.kts` → `NONE`.
- Both modules parse under Gradle (`gradle help` rc=0) with `ANDROID_HOME`
  set.
- Ad-hoc check script `/tmp/hermes-verify-b8.py` (now cleaned up):
  11/11 PASS.

See `docs/REMAINING-WORK.md` §3 W3 for the tracked row.
