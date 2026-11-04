---
title: Tier E1's eight red cases were seven wrong tests and one real defect
created: 2026-11-04
updated: 2026-11-04
type: session
status: active
tags: [session, android, tier-E1, backspace, mode-toggle, candidate-window, pass-through, mutation]
sources:
  - ../../../frontends/android/app/src/test/java/org/hunmin/ime/HunminAdapterTest.kt
  - ../../../frontends/android/app/src/main/java/org/hunmin/ime/HunminAdapter.kt
  - ../../../tools/android-mutations.py
  - ../../../docs/FRONTEND-CONTRACT.md
  - ../../../docs/FRONTEND-EQUIVALENCE.md
confidence: high
---

# Tier E1's eight red cases were seven wrong tests and one real defect

One line: issue #246 measured eight `HunminAdapterTest` failures at HEAD without
claiming a cause. They were not one bug. Seven were assertions about a contract
this frontend does not have (or about a key sequence that composes something
else), and **one hid a shipped data-loss defect** that no other tier could see.

## What each failure actually was

| case | diagnosis | where the error lived |
|---|---|---|
| `backspace edits the pending composition` | typed `dkss` and expected `안녕`; the layer shows `안ㄴ`, and backspace pops a **jamo**, not a syllable | the test |
| `mode toggle is a boundary that flushes the word` | built a **one-slot** adapter, which refuses the action by design, so the code under test never ran | the test — and the delivery it would have checked was missing |
| `backspace past empty preedit commits the buffered word` | reached the boundary with one BackSpace instead of four; the engine still had `녕` to delete | the test |
| `backspace on an empty buffer is a no-op` | `composings.last()` on an empty list: a no-op emits no `setComposingText` at all | the test |
| `candidate window … pick` / `… escape` | F9 has no AOSP keycode and this adapter has no candidate window; both keycodes are unmapped boundaries | the tests (merged into one) |
| `enter … passes through as newline` / `comma … forwards the comma` | counted `sendKeyEvent` payloads on the fake, which the adapter never sends — pass-through is the **return value** | the test |

The backspace expectations came from TSF case 11a-11d, which was measured
against the same engine: `녕` → `녀` → `ㄴ` → empty, then the fourth press
declines, the boundary inserts the buffered word once and the key passes. The
Android cases had collapsed that into "one press removes one syllable", which is
not what any frontend does.

## The one real defect

The layer's language action is a **boundary**: `toggle_language()` commits the
pending word and hands it back in `out.commit`. TSF's `ToggleModeCore` and
fcitx5's `applyJudgement` both insert it. Android's `toggleLanguage()` discarded
it and the service only re-rendered the preedit, so the word being composed at
the moment of a 한/영 press was lost — the exact shape of the bug the file's own
header says the flush contract exists to prevent.

It survived because the test that was supposed to guard it could not reach the
code: a one-slot adapter answers `false` and the service's hand-rolled swap takes
over. Fixed by taking the `InputConnection` and delivering the layer's commit;
`tools/android-mutations.py` now plants `no-toggle-delivery` and the suite
catches it.

## Numbers

- before: `tools/android-unit.sh` → 157 tests, 8 failed
- after: 156 tests, 0 failed (`PASS: 156/156`), `ctest -R engine-android-e1` 9 s
- `android-mutations`: 10 caught, 1 recorded known gap — as expected, 114.4 s
  locally (the tenth mutation costs one tier run, ~10 s; `docs/CI-TIMING.md`
  records 84.31 s with nine on the CI image). The `slow` tier is over its 60 s
  budget either way; that line is a maintainer's to move.
- one test fewer than before, not more: the two candidate-window cases became a
  single case asserting what is true here.

## What this says about the tier

A tier that reads red for a week under a board row that says `registered` is not
merely late — the red is the only evidence the suite exercises anything. Seven of
the eight reds were the suite measuring its own instrument (a fake's unused list,
a list's `.last()`), which is the "green report that measured nothing" failure
one layer along: an assertion that cannot pass is as uninformative as one that
cannot fail, and it hides the eighth case that could have.

The edit-key sequences themselves are the shared catalogue in
[[2026-11-04-shared-edit-key-catalogue-was-executed]]; what this session adds is
the Android column's real reachability — EK4 and EK5 do not map to this
frontend, and saying so is the difference between five assertions and seven
unfalsifiable ones. The `slow`-tier budget is a line only the maintainer may
move, per [[decisions-belong-to-the-maintainer]].