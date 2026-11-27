---
title: The outstanding decisions
created: 2026-10-04
updated: 2026-11-01
type: decision
status: active
contested: true
tags: [decision, modes, question]
sources:
  - ../raw/sessions/2026-10-04-stemming-investigation.md
  - ../concepts/the-three-corpora-are-three-registers.md
confidence: medium
---

# The outstanding decisions

Everything here is **open and belongs to the maintainer.** Recorded so that the
state is visible rather than reconstructed from commit messages.

## D1. The hangul judgement method

ReqMode8's `hangul_valid` must answer "is this a hangul word?". Three candidate
definitions, and the evidence is genuinely mixed:

| | what it looks up | status |
|---|---|---|
| (a) corpus top-N | a frequency list | **rejected by measurement** — no saturation; 200,000 forms reach 2.90% of hangul forms, and ReqMode8's "ambiguous → keep current mode" then silently disables the feature |
| (b) dictionary lookup | a dictionary | **weakened** — the 45.42% figure came from a hanja-filtered subset (31,253 of 56,555), so the real coverage is unknown and probably higher; still not measured |
| (c) combination test | "is this any hangul vowel/final combination" | **the fallback, and the one the maintainer has provisionally adopted** |

(c) is the only one with no cap problem. Its known weakness is unmeasured: it
accepts every combinable form, so it will admit 형용사·부사·관형사·수사 that no
dictionary contains. See [[the-outstanding-decisions]]'s open measurement below.

**Status 2026-11-01 — this decision has been made, elsewhere.** `docs/SPEC-MODES.md`
§ReqMode8 (2026-10-25) ruled option (c) and replaced the word test with a
syllable-structure test, so `hangul_valid` no longer looks anything up; the English
half was ruled separately (§ReqMode8, 2026-11-01, issue #160). The table above is
the record of the evidence, not the current definition. Whether board row B1 is
therefore retired — and what happens to the corpus list this page's siblings still
describe — is issue #161, priced in `docs/OPTIONS-MEASURED.md` §B1.

## D2. E1 — whether the symmetry rule holds

ReqMode8's second line (english mode → hangul) is implemented-or-not independent of
the first. The corpus now makes the measurement possible: krdict carries `pos`,
so 조사/어미 separation is countable. **Not yet measured.**

**Status 2026-11-01:** the route named here left the 2026-10-05 scope boundary, and
§ReqMode8 removed the need for it — the measurement now needs a corpus of typed
Korean and the shipped `libhunmin/data/modes/en-US.json`, no Korean dictionary and
no `pos` field. Still not measured, and still unfiled (`docs/OPTIONS-MEASURED.md`
§B1-2).

## D3. Whether 119 belongs in the shipped lists

119 is official administrative prose with 238,721,031 tokens and a head of document
connectives. It is not distributable (AIHub), so it can only ever be a
measurement instrument. The open question is whether a *frequency list derived
from it* is a reshaping of the dataset — the licence answer is probably no, but
it has not been settled and should not be assumed either way.

## D4. Whether this wiki stays

Adopted 2026-10-04 on the reasoning that rules keep breaking and parallel work
was not proceeding. To be re-evaluated later, explicitly. The things that would
justify dropping it:

- `wiki-lint.py` finds nothing across two weeks of work
- the pages are written but not read before decisions
- it becomes a second home for figures that live in `docs/`

The thing that would justify keeping it: a decision made this month that cites
a `raw/` record and would otherwise have been re-derived.

## D5–D9. Track 2: the automatic switch's design (opened 2026-11-27)

Track 2's purpose is recorded in `docs/MILESTONES.md` §0.4 Track 2 (the track has
to *find the design*, not grade the shipped one), the three proposals are quoted in
[[2026-11-27-t2-direction-and-the-three-proposals]], the prior art is
`docs/MODES-PRIOR-ART.md`, and the maintainer's own typing is measured in
[[2026-11-27-personal-rewrites]]. What is open, in dependency order:

- **D5. What "more data" means, and when it is enough.** Decided 2026-11-27 that
  more has to be collected; not decided *which* sources, *how much*, or what makes
  a dataset sufficient (a fixed count, or a coverage curve that stops moving). The
  candidates on this machine are the maintainer's own typing at scale (session
  store, prompt history), a typed-web frequency list (AIHub 624 — licence: use
  only, never redistributable), or a live key log from a real session
  (`docs/trace-spec.md` §"Personal key log", now carrying `judged`).
- **D6. Which design is prototyped first** — the hint (show the alternative as a
  candidate), the explicit trigger (convert only when a key asks), or the mixed
  token (English stem + Korean particle). Each has a unit
  `docs/MODES-MEASUREMENT.md` does not yet define for it.
- **D7. Whether the user dictionary is a Track 2 lever.** Every rewrite on the
  recorded typing was a word the shipped list does not know, and
  `hunmin_buffered_preedit_add_user_word` fixes the verified case — but who
  supplies the words (a frontend row, a shipped list, a personal file) is a data
  and UX decision, not an engine one.
- **D8. What #295 is from now on** — a gate on whichever display ships, a
  regression line only, or retired in favour of D6's comparison — and what happens
  to `judge_hold` (keep at its 0 default, make 1 the default — an E6 ruling — or
  delete it with the display it steadies).
- **D9. Whether a live session may be logged** — a trace build behind the
  maintainer's real typing means replacing `/usr/bin/hunmin-ibus` (or a runtime
  option); the private nested session `tools/ibus-real-use.sh` needs nothing.

Nothing here is answered; each is a ruling, and **D5 blocks the rest** — the
experiments D6/D7 need a corpus whose sufficiency D5 defines.

## Open measurements (findings, not decisions)

- Stemming on 119, completing the three-way comparison.
- Whether the paradigm needs the 받침-이동 rule at all. The 2003 survey ships both
  spellings for the inflections that matter, and the 받침-led spellings occur
  **zero** times as corpus text, so the two arms may be equivalent.
- The combination test's false-accept rate on 형용사·부사·관형사·수사.

See [[decisions-belong-to-the-maintainer]] for why these are listed and not
answered.