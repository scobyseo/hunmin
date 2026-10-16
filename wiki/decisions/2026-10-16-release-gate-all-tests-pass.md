---
title: The release gate is every ctest test passing, with named exceptions
created: 2026-10-16
updated: 2026-10-16
type: decision
status: active
tags: [decision, testing, release-plan]
sources:
  - ../../../AGENTS.md
  - ../../../docs/TESTING.md
  - ../../../docs/REMAINING-WORK.md
  - ../../../tools/release-gate.sh
confidence: high
---

# The release gate is every ctest test passing, with named exceptions

**Ruling (2026-10-16), the maintainer's words:**

> on release, all TCs in this repo should pass, except data-selfcheck which
> cannot pass

`TC` is this repo's ctest test. The ruling is recorded here as it was given; the
enforcement that follows is a reading of it, and the parts of the reading that
were *not* the maintainer's words are marked as such below.

## The ruling

- A release requires the whole registered suite passing. A green subset is not
  a release gate: `tools/ctest-select.sh` exists precisely because a filtered
  run cannot prove what it skipped.
- `data-selfcheck` is an accepted exception. It cannot pass without the 368 MB
  gitignored 국립국어원 dump (`docs/REMAINING-WORK.md` §0, issue #46), and its
  self-contained guard is the separate ctest test `krdict-fixture`.

## What the measurement showed, and the reading this page adds

The ruling named one exception; the tree has **three** on this host, and the
difference is a fact about the environment rather than about the ruling:

| Skipped test | Why | Reading |
|---|---|---|
| `data-selfcheck` | gitignored krdict dump absent | the ruling's own exception |
| `modes-corpus-selfcheck` | needs a corpus at `tests/modes-corpus`, which lives outside the repo and has never been placed there | **same class as `data-selfcheck`** — a corpus this repository deliberately does not carry |
| `engine-android-e2e` | **not environmental.** It PASSED twice end-to-end on 2026-10-16 (108 s each, `dkssud` → `ㅎ곈디`, both permission branches, 7 trace lines, settings window) and then began failing non-deterministically. Not an exception: a red test | see [[2026-10-16-android-e2e-keys-bypass-the-ime]] |
| `engine-d3-gtk4` | skips only when the D3 trio contends at high `-j`; **passes standalone** (49.2 s) | **not an exception at all** — contention is not a reason to skip, so `tools/release-gate.sh` re-runs it alone |

So the ruling's exception count is **one class**, not one test: the corpus tests,
`data-selfcheck` and `modes-corpus-selfcheck` together. Everything else is
expected to pass — and at the time of writing, `engine-android-e2e` does not, so
the gate is **red**, which is the gate doing its job rather than a reason to add
an exception for it. What would change the reading: a decision that
`modes-corpus-selfcheck` should get a committed fixture the way
`tests/fixtures/krdict/` was, which would leave `data-selfcheck` as the single
named exception.

## How the ruling is enforced

`tools/release-gate.sh`, because AGENTS.md requires procedures to be files. It
runs the full suite and fails on:

1. any failed test;
2. any skipped test not in its exception table, with the reason recorded;
3. any declared exception that **stopped** skipping — a stale exception is a
   stale claim, and a gate that keeps honouring it has stopped measuring.

Rule 3 is not in the maintainer's words. It is added because an exception list
that can only grow is how a gate decays into a comment: without it, the day
`data-selfcheck` starts passing, the gate would still be reporting an exception
that no longer applies.

`tools/release-gate.sh --selftest` runs the verdict logic against planted
verdicts. That self-test exists because the first version of the skip parser
matched only spaces where ctest indents with a **tab**, left the skip list
empty, and would have passed an arbitrary skip list while looking like a gate
that worked — the same "green while measuring nothing" shape as
[[2026-10-10-d3-case-coverage]] and `docs/D3-ROOT-CAUSE.md`.

## Measured

2026-10-16, `64589e6`, this host, JDK 17 / Android SDK 35 / NDK 27.3.13750724
exported at configure time, `-DHUNMIN_TRACE_BUILD=ON`, AVD `hunmin-e2e` booted,
`-j4`: **184 registered, 2 skipped** (`data-selfcheck`,
`modes-corpus-selfcheck`) and `engine-android-e2e` alternating between red and
skipped across identical consecutive runs. `engine-android-goldens` passes once
the AVD is attached; before that it skips with "no Android device attached".

**Addendum 2026-10-17 (maintainer instruction, does not rewrite the ruling):**
`data-selfcheck` was unregistered from ctest — the maintainer's instruction is
that it is not touched before its own dev stage, and on the Windows host it
was red on every pass (the converter-vs-committed-data divergence; the #46
skip only solved the absent dump). The gate's exception list therefore holds
`modes-corpus-selfcheck` alone, and the self-contained `krdict-fixture` guard
remains. The ruling quote above stays as it was given.

## What this ruling does NOT do

- It does not say 0.1 is ready to ship. That is `docs/REMAINING-WORK.md` §1 and
  the maintainer's.
- It does not check the artefacts. `.deb`, release APK, tag and GitHub Release
  are `tests/manual-checklists/release-0.1.md` and `tools/release-0.1.sh`.
- It does not restore CI, which is not running at all: no job has executed
  since 2026-10-13, and the cause is the account's Actions minutes, not this
  tree. `docs/REMAINING-WORK.md` §3 and `docs/CI-TIMING.md` §6 are the
  authoritative record; the gate exists precisely because CI cannot be.