---
title: No modes measurement until the maintainer says so; for 0.3 the English dictionary is enough
created: 2026-11-08
updated: 2026-11-08
type: decision
status: active
tags: [decision, modes, measurement, data]
sources:
  - ../../../docs/MODES-MEASUREMENT.md
  - ../../../docs/REMAINING-WORK.md
confidence: high
---

# No modes measurement until the maintainer says so; for 0.3 the English dictionary is enough

**Ruling (2026-11-08, issue #272).** Asked which corpus a fresh ② / ②b / ③-bis
run should use, the maintainer declined the choice and stated the rule instead:

> "no measurement until i mention. current rule - using english dict only for
> auto-switch is enough for me."

and, asked what "enough" was scoped to:

> "i mean enough for 0.3"

## What it settles

- **No run is planned, wanted, or permitted on the maintainer's behalf.** Not on
  the ko.wikipedia dump, not on AIHub 624, not on any corpus found later. The
  candidate corpora named in `docs/MODES-MEASUREMENT.md` §4 are what a run would
  have to be licensed like if one is ever asked for; they are not a work queue.
- **For the 0.3 lane, `english_valid` as an `en_US` dictionary lookup is the
  whole of the judgement's data.** In that lane the retired corpus word list, its
  50,000-entry cap, the curated supplement that was once deferred, and a search
  for a typed-text corpus are all unwanted — not deferred to a later phase.
- **The 0.3 scope is the scope, and it is not a claim about later lanes.**
  Whether 0.4 or 1.0 needs more English than a dictionary was not asked and is
  not answered here; it would be its own ruling, and nothing in the tree assumes
  either answer.
- **Board row B2 is unblocked-by-nobody.** Its pass lines stay unset; the row
  waits on the maintainer raising the measurement, not on a corpus, a harness or
  a budget.
- **B2 is no longer 0.3's exit gate** (ruled the same day, against
  `docs/MILESTONES.md` §0.3 as it was written). 0.3 exits on the mixed-sentence
  gate — `English는 좋은 Language입니다` typed as one stream — and nothing else;
  the pass lines and every reading behind them move past 0.3.

## What it does not settle

- The pass lines themselves, if a measurement is ever asked for. The shapes and
  the algebra a run would report are in `docs/MODES-MEASUREMENT.md` §2, so the
  question can be answered without rebuilding the deleted papers.
- ReqMode9's threshold, now that `hangul_valid` is a structure test rather than a
  word test. That residue was never part of the corpus question.
- What the judgement's English data should be past 0.3 (see above).

## Why the record matters later

A later session that finds `MODES-MEASUREMENT.md` with definitions and no figures
has three wrong moves available: re-derive the retired measurement from git, build
a new corpus because a file says "reproducible", or treat an unset pass line as a
gap to close. This page is the answer to all three. See
[[2026-11-08-the-retired-corpus-measurement-is-deleted]] for the deletion this
extends, and [[2026-11-01-the-english-source-is-hunspell-en-us-plus-a-user-dictionary]]
for the data ruling it declares sufficient.