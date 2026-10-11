---
title: "0.1: the Android versionName keeps the phase suffix — 0.1.0-p5, not 0.1.0"
created: 2026-10-12
updated: 2026-10-12
type: decision
status: active
tags: [decision, android, release-plan, question]
sources:
  - https://github.com/scobyseo/hunmin/issues/9
  - ../../../docs/MILESTONES.md
  - ../../../docs/ENVIRONMENT.md
confidence: high
---

# 0.1: the Android versionName keeps the phase suffix — `0.1.0-p5`, not `0.1.0`

Closes the naming half of issue #9: three version strings carried
`0.1.0`-ish values that did not agree, and the question was whether the
Android `versionName` should drop its `-p5` phase marker to match the .deb.

**Ruling (maintainer, 2026-10-12, answering issue #9's question in the
worktree session that implemented it):** keep the phase marker. The release is
`0.1.0`; the `-pN` suffix on an Android `versionName` is the build-phase
label. The release notes and the README say which string is the release and
which is the phase — the README and `docs/MILESTONES.md` §0.1 now do.

## What each string now means

| where | string | meaning |
|---|---|---|
| `CMakeLists.txt:2` | `0.1.0` | the release, bare |
| `debian/changelog:1` | `0.1.0` | the release, bare |
| `frontends/android/app/build.gradle.kts:53` | `0.1.0-p5` | release `0.1.0`, built in Android phase P5 (the IME) |
| `frontends/android/keymaps/build.gradle.kts:20` | `0.1.0-p5.5` | release `0.1.0`, built in phase P5.5 (the KCM APK) |

The phase labels come from `docs/ENVIRONMENT.md` batch 2 — P5 is the Android
IME, P5.5 the keymaps APK — so the suffix encodes *when the artifact was
built*, and the `0.1.0` part is what a user compares against the .deb.

## What this does NOT close

- **`versionCode = 1` has never been incremented.** The issue measured that,
  and the ruling did not touch it: whatever `versionName` says, an installed
  APK reports versionCode 1. A version-code scheme for actual releases is
  still unnamed (issue #9 listed it; `PLAN.md` P5's packaging set also names
  it). Raising it is a release-time act, not a documentation one.
- **No tag and no GitHub release exist yet**, so nothing else has committed to
  a naming convention; the first tag will be the next place this convention
  meets a real artifact.

## Why the record matters later

If 0.2 reuses `-pN` suffixes, the phase numbers there refer to *that* lane's
phases, not Android's P5/P5.5 — the suffix is per-frontend phase labelling,
not a global build counter. The convention recorded in
`docs/MILESTONES.md` §0.1 is the one to re-derive from, per the no-second-home
rule (see [[the-rules-keep-breaking]] for how a figure written twice drifts).

See [[decisions-belong-to-the-maintainer]] — this page records a ruling the
maintainer made, which is the only kind of ruling that belongs here. The
request itself is preserved in issue #9; see also
[[2026-10-11-auto-switch-off-by-default]] for the same pattern one day
earlier: a question the issue could state but not settle.
