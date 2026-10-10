---
title: Two mutation tiers that were green while measuring nothing, 2026-10-10
created: 2026-10-10
updated: 2026-10-10
type: session
status: active
ingested: 2026-10-10
tags: [mutation, android, testing, sandbox, tmpfs]
sources:
  - ../../../docs/ANDROID-MUTATION-SANDBOX.md (the Android measurements, authoritative)
  - ../../../docs/MUTATION-TIER-OPTIONS.md (the fcitx5 tier's own measurements)
  - ../../../docs/D3-ROOT-CAUSE.md (the shape both of these had)
confidence: high
---

# Two mutation tiers that were green while measuring nothing

> **Compressed.** Every figure and its reasoning now lives in the document that
> owns it: `docs/ANDROID-MUTATION-SANDBOX.md` (all four defects, the three
> sandbox designs measured, the warm-copy question, and §5's unreferenced
> mutation pattern file — including its *"recorded, not decided"*) and
> `docs/MUTATION-TIER-OPTIONS.md` (the fcitx5 tier). `docs/HISTORY.md` carries
> the timeline. What is kept here is the findings table and the two parts that
> generalise, because `docs/` records what was found and not what it is an
> example of.

## What was found, and where it went

| finding | now in |
|---|---|
| `tools/android-mutations.sh` read `android-unit.sh`'s skip exit (77) as "caught": *"PASS: all 9 planted mutations were caught"*, exit 0, 0.31 s, nothing built, and `SKIP_RETURN_CODE 77` could never fire | `docs/ANDROID-MUTATION-SANDBOX.md` §4.1 |
| a checkout whose suite was already red read the same way — nine working mutations or a broken tree, indistinguishable | §4.2 |
| a stale all-green XML plus a neutered `gradlew` reports `PASS: 44/44`; deleting the report directory before each run closes it | §4.3 |
| `shutil.ignore_patterns` matches **names**, not paths, so `"data-orig/krdict"` excluded nothing and 369 MB of gitignored XML rode into every sandbox on tmpfs — **RAM**, held for the life of the tier | the comment at `tools/frontend-mutations.py:164`; the "a check that cannot fail" table in [[the-rules-keep-breaking]] |
| copying the warm Gradle build is not a speedup — `org.gradle.caching` already does the work; the sandbox is not faster either (28.5 s either way), isolation is free | `docs/ANDROID-MUTATION-SANDBOX.md` §2.1 and §3 |
| `RUN_SERIAL` was costing the whole suite 25 s: 78.8 s → 54.0 s, while the tier itself went 26 s → 45 s because it stopped running alone | §3.2 |
| `tools/android-mutations/voice-double-start` is on disk, referenced by nothing; tier E1 stays green 44/44 with it applied | §5 — **not decided**, and left that way |

## The two parts that generalise

**A harness that fails loudly is only as good as its callers' handling of its
exit codes.** *"We return 77 on skip"* is not a property of a script; it is a
property of the contract every caller must honour. The Android unit script's
skip-loud check existed and its caller undid it.

**A brief's number is not a measurement.** The 43.64 s in the brief did not
reproduce (34.06 s cold, 24.69 s warm), and the sandbox cost was guessed in
both directions before it was measured. A comparison taken against a figure
from another host's warm state is a comparison against nothing.

The shape both of these had — a green tier that was measuring something else —
is collected with the other four instances in [[the-rules-keep-breaking]], and
the first of them is in [[2026-10-10-d3-case-coverage]].