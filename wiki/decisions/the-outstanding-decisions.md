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

## Open measurements (findings, not decisions)

- Stemming on 119, completing the three-way comparison.
- Whether the paradigm needs the 받침-이동 rule at all. The 2003 survey ships both
  spellings for the inflections that matter, and the 받침-led spellings occur
  **zero** times as corpus text, so the two arms may be equivalent.
- The combination test's false-accept rate on 형용사·부사·관형사·수사.

See [[decisions-belong-to-the-maintainer]] for why these are listed and not
answered.