---
title: The shared edit-key catalogue was executed, then sat untracked
created: 2026-11-04
updated: 2026-11-04
type: session
status: active
tags: [session, edit-keys, frontends, parity, catalogue, backspace, candidate-window, android, fcitx5, tsf]
sources:
  - ../../../tests/e2e/cases-edit.tsv
  - ../../../docs/FRONTEND-EQUIVALENCE.md
  - ../../../docs/TESTING.md
confidence: high
---

# The shared edit-key catalogue was executed, then sat untracked

One line: a 2026-10-25 planning note at the repository root listed seven edit-key
sequences every frontend should assert and a matrix of which ones did; nothing
referenced it, no issue carried it, and by the time it was noticed the matrix had
almost no blanks left.

## What the note asked for

Three surfaces see the same engine contract through different client APIs —
fcitx5 through the GTK/XIM callbacks, TSF through `ITfContext` edits, Android
through `InputConnection`. The note therefore asked for one **shared sequence
catalogue**, a per-frontend note on reachability, and a **verification map**: one
row per sequence, one column per frontend, driven to "no blanks".

The sequences, relabelled **EK1–EK7** here because `E1`–`E7` already means the
automatic-switch experiments (`docs/MILESTONES.md`, issue #180's "E7"):

| | sequence | engine expectation |
|---|---|---|
| EK1 | BackSpace over a pending jamo | the pending jamo leaves the word; an emptied word does not commit empty |
| EK2 | BackSpace on an empty preedit | the buffered word commits and the key passes |
| EK3 | mode toggle | a boundary: the buffered word commits before the layout switches |
| EK4 | candidate open → pick → commit | the preedit is replaced in place, never appended after |
| EK5 | candidate open → Escape | no commit; the preedit is untouched and typing resumes |
| EK6 | Return after a word-final boundary | the word commits, then the newline is forwarded |
| EK7 | punctuation the layout does not map | the word commits, the punctuation arrives as a literal |

The label collision is worth naming: two documents using `E7` for different
things is exactly how a citation goes wrong. This record uses EK*; the switch
experiments keep E*.

## What the tree actually holds (measured 2026-11-04)

| sequence | fcitx5 | Android | TSF |
|---|---|---|---|
| EK1 | `tests/e2e/cases-edit.tsv` | `HunminAdapterTest.kt` `backspace edits the pending composition` | `tsf-candidate-test.cpp` `BackspaceEditsThePendingComposition` (11a–c) |
| EK2 | `cases-edit.tsv` (`dkssud{BackSpace}ud{BackSpace}`) | `backspace past empty preedit commits the buffered word` | same test, case 11d (`AssertCompositionEnded(kAnchorAn)`) |
| EK3 | `cases-edit.tsv` (`dkssud^`) | `mode toggle is a boundary that flushes the word` | `ModeToggleIsABoundaryWithTheWindowOpen` |
| EK4 | `testhunminui.cpp` (F9 window, in-place replacement) | `candidate window open + pick first candidate + commit` | `ReturnCommitsTheCursor` |
| EK5 | `cases-edit.tsv` (`kfc{F9}{Escape}`) | `candidate window open + escape closes without committing` | `EscClosesAndTypingContinues` |
| EK6 | `cases-edit.tsv` (`{Return}`) | `enter after a committed word passes through as newline` | `tsf-config-test.cpp` (Enter passes at test and down phases) |
| EK7 | `cases-edit.tsv` | `comma after a word commits the word and forwards the comma` | **not asserted** — see below |

So the note's own "not yet" cells (Android EK6 and EK7, TSF EK5) were filled in
the week after it was written, and the note was never updated because nothing
consumed it. That is the finding: **a plan with no owner page rots in place.**

## The one real gap: EK7 on TSF

`frontends/tsf/test/tsf-config-test.cpp` presses `VK_OEM_COMMA` on sin3-p2 and
asserts the punctuation **composes into the word**, then that Enter passes
through. That is the opposite case from EK7, where the layout does **not** map the
punctuation and the word must commit before a literal reaches the client.

No TSF test presses a punctuation key the active layout declines. The behaviour
is close to `OutOfRangeDigitDrainsAndPasses` and `UnboundF9IsAKeyOfPassage`, so it
is probably already right — but "probably" is the thing this repository does not
accept as coverage, and there is no measurement either way.

It is recorded as a board row rather than fixed here: adding a mock case is a
small change, but the sequence contract is the kind of thing the maintainer rules
on, and a test written to match whatever the adapter does today would ratify the
current behaviour instead of asserting the intended one
([[decisions-belong-to-the-maintainer]]).

## Where it lives now

- The catalogue's live form is `tests/e2e/cases-edit.tsv` (fcitx5's tier D3),
  asserted present by board row `EDITKEY-CATALOGUE-95`. Nothing else carries a
  second copy of these expectations — which is the point: this page is the only
  note, and it exists so the next reader does not have to find the gap twice.
- The EK7-TSF gap is `EDITKEY-EK7-TSF` in `docs/REMAINING-WORK.md`.
- The root file `2026-10-25-shared-edit-key-sequence-plan.md` was **deleted**
  2026-11-04: it was untracked, so deleting it removed no history — this page and
  the board rows are the history it did not have.

Same shape as [[2026-11-03-x11-corpus-replays-every-tier-a-case]]: a coverage
claim that looked complete because nothing was asking it to prove itself.