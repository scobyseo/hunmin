---
title: B1 retired — the hangul judgement needs no source
created: 2026-11-01
updated: 2026-11-01
type: decision
status: active
contested: false
tags: [decision, modes]
sources:
  - ../raw/sessions/2026-10-11-b3-off-default-and-the-data-gate.md
  - 2026-10-13-r8-mode-guessing-is-adaptor-owned.md
confidence: high
---

# B1 retired — the hangul judgement needs no source

## The words

The maintainer, 2026-11-01, on issue #161:

> want to select A

"A" is option A of `docs/OPTIONS-MEASURED.md` §B1: **retire B1**, the status
board's row "the hangul judgement source — what R8's `hangul_valid` looks up".

## Why the row had no surviving content

Two rulings had already emptied it, and neither had reached the board:

- **`docs/SPEC-MODES.md` §R8-정정 (2026-10-25)** replaced the word test with a
  syllable-structure test. `hangul_valid` asks whether the buffer composes into
  well-formed syllables — it looks nothing up, so there is no source to choose.
- **§R8-정정-2 (2026-11-01)** named the English half — hunspell `en_US` plus a
  user dictionary — and that artefact is shipped and its issue (#160) closed.

The board and `docs/MILESTONES.md` carried zero mentions of either ruling, which
is how a decided question kept reading as the root of five blocked items. The
measurements behind the ruling, including what the structural test still
discriminates (it is not a rubber stamp on Latin input), are
`docs/OPTIONS-MEASURED.md` §B1-3; they are not restated here.

## What the ruling changes

- `docs/REMAINING-WORK.md`: B1's row carries the retirement and its verbatim
  words; §2 is retitled and restated so each of its five items names what it waits
  on now — the judgement's implementation (#166, #168), a harness (#181), or its
  own ruling (#163, #180) — and none names a word source; the voice-input stage 8
  row no longer reads "blocked on B1"; the assertion table's decision-blocked
  comment says the same.
- `docs/MILESTONES.md`: the "hostage to B1" lines in the 0.1 and 0.3 lanes.
- `PLAN.md`: P8's "judgement source" row moves from **OPEN** to **ANSWERED**.

## What it does not decide

- **Whether the corpus list, its 50,000 cap and the `MODES-DATA` §4 sections that
  describe them are deleted or kept as the record.** `§R8-정정` left that open by
  name and it still has no issue. This is the one item in B1's neighbourhood that
  is genuinely undecided, and it is about the *English* list, not the Hangul one.
- **Whether E1 (the symmetric-rule measurement) gets an owner.** It now needs a
  corpus of typed Korean plus the shipped `en-US.json`, and no Korean dictionary —
  the krdict `pos` route its text names left the 2026-10-05 boundary.
- **Whether `§R8-정정`'s trailing "left open … whether R9's threshold is re-based"
  sentence is repaired**, since R9 itself retired the threshold 74 minutes later;
  the surviving method conflict (a measured N versus a design-time choice) is
  issue #163's.

## See also

- [[the-outstanding-decisions]] — D1 and D2, whose status this ruling moves.
- [[decisions-belong-to-the-maintainer]] — why the agent priced the options and
  did not pick one.
- [[2026-10-13-r8-mode-guessing-is-adaptor-owned]] — the earlier ruling that
  settled *who owns* the judgement, and explicitly not what it judges from.