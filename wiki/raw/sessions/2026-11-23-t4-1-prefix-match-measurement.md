---
title: "T4-1: prefix-match measurement on krdict — the fallback was already drifted"
created: 2026-11-23
updated: 2026-11-23
type: session
status: active
tags: [hanja, measurement, provenance, tooling, data, fcitx5, tsf, android]
sources:
  - ../../../docs/OPTIONS-MEASURED.md
  - ../../../docs/MILESTONES.md
  - ../../../docs/SPEC-TSF.md
  - ../../../tools/measure-hanja-prefix.py
  - ../../../libhunmin/src/candidate_tables.c
  - ../../../frontends/fcitx5/src/hunminadapter.cpp
  - ../../../frontends/tsf/src/candidates.cpp
  - ../../../frontends/android/app/src/main/java/org/hunmin/ime/HunminInputMethodService.kt
confidence: high
---

# T4-1: prefix-match measurement on krdict

Issue #300 (0.4 Track 4, parent #286): measure before any code — how often a
typed Hangul buffer has a dictionary prefix, how long the prefix is, how many
selection rounds an out-of-dictionary eojeol needs, priced in keystrokes
against the single-syllable fallback. The figures, the model assumptions, the
corpus pins and the priced options live once in `docs/OPTIONS-MEASURED.md`
§HPX; this page records how the measurement was made and what it changed
about the question. No numbers are restated here.

## What was built

`tools/measure-hanja-prefix.py`: reads the same JSON tables
`candidate_tables.c` loads (membership + multiplicity, not a reimplementation
of composition), segments each eojeol form by greedy longest dictionary
prefix, and computes the minimum-rounds segmentation by dynamic program so the
greedy rule's gap to the best rule is measured rather than assumed. A
frequency-ordered sample is cross-checked against the engine oracle
(`hunmin_suggest_count` via ctypes) and the probe is timed — the per-probe
cost a prefix search would repeat. Guard: ctest `hanja-prefix-selfcheck`
(tier-G), fixture aggregates computed by hand, including a negative-savings
case and a negative control that must fail.

Three sha256-pinned frequency lists, three registers — the same discipline as
[[the-three-corpora-are-three-registers]]: kowiki (shippable, encyclopedic
head), AIHub 119 (administrative, byte-identical reproducible), AIHub 624 (web
prose, closest to typed text, extraction not reproducible). No corpus is "the"
population; choosing the pass-line population is a ruling, and
[[decisions-belong-to-the-maintainer]] applies to every finding below.

## Premises checked, and what did not hold

AGENTS.md: check the maintainer's claims as carefully as your own. Three
stated premises were checked against the tree before measuring:

1. **"Single char as fallback" is not uniform.** TSF retries the last syllable
   on a whole-word miss (TSF10, `LastSyllableOf`). fcitx5's `openCandidates`
   and Android's Hanja path pass the whole preedit to an exact-match lookup:
   on an OOV multi-syllable buffer F9 offers nothing. The rounds model is the
   same either way, but the drift option 2 fears is already real — the
   fallback itself differs per frontend today.
2. **"The candidate window already re-opens per selection" (#286) does not
   hold on fcitx5.** `commitCandidate` resets the preedit and closes the
   window; re-opening takes a fresh composition and F9. The cheap-round line
   of the pricing is therefore new behaviour to build, not reuse. Recorded as
   a discrepancy in §HPX-0, not resolved: the older wording may encode an
   intent the code has not caught up with. TSF/Android post-selection paths
   were not audited.
3. **The krdict entry count in `LICENSE.md` is stale** (pre-dedupe, issue
   #272). The measurement counts from the files and records the drift; the
   licence file was not edited in this task.

## What this session got wrong

- The first selfcheck assertion aggregated the savings histogram wrongly —
  its keys are savings *values*, not sums; the fixture caught it on first run,
  which is the fixture doing its job.
- The first probe timing included the lazy table load (6 MB JSON on the first
  `suggest` call), pricing the load instead of the probe. Fixed with a warm-up
  call before the timed loop; the difference was an order of magnitude.
- A hypothesis worth naming because the measurement refuted it: that prefix
  matching would also *expand coverage* — convert words whose syllables have
  no hanja reading at all. It almost never does (§HPX-3's coverage finding):
  the mechanism is a keystroke saving, not a coverage fix. The zero-candidate
  F9 population stays zero-candidate.

## What the measurement changed about the question

- The match-rule decision (longest vs frequency-ranked) has almost no
  round-count headroom: greedy is within a fraction of a percent of the DP
  optimum on tokens. A frequency rule would have to justify itself on
  candidate quality, which this instrument does not measure.
- The re-open behaviour is worth roughly twice the keystroke gain — it is a
  presentation decision with a measurable price, not a detail.
- The fallback's hidden cost is paging exposure (homophone counts far past
  the 10-label page on single-syllable rounds; dictionary rounds never page).
  Which candidate the user wants is rank data no frequency list carries, so
  it stays exposure, unpriced — a pass line would need it.

All figures: `docs/OPTIONS-MEASURED.md` §HPX. Artefacts and tool-written
provenance: `~/work/hunmin-measurements/t4-1-prefix/` (outside the repository,
per the bulk-corpora rule). Open: everything in #286's decision list; this
session records measurements and closes nothing.

## Verification, and a baseline finding

- `ctest -N` diff against the pre-change baseline: exactly one new test
  (`hanja-prefix-selfcheck`), green; `docs-lint` and `wiki-lint` report zero
  errors; `req-map`, `abi`, `schema-contract` green.
- **`engine-android-e2e` is red on the clean baseline too.** With this task's
  changes stashed, the test still fails — and on a *different* assertion than
  the run before it (the key-binding storage check, then the KP12 auto-show
  check). That is the documented intermittent tier (`docs/REMAINING-WORK.md`,
  the five refuted hypotheses), not this diff: nothing here touches Android
  code. The emulator had been running since before this session and was left
  alone (host rule: kills scope to PIDs you spawned).
- `tools/ci-time-budget.sh build` was run with the toolchain env exported
  (log preserved beside the artefacts as `ci-time-budget.log`): inner loop
  78.6 s vs 30 s, slow tier 144.7 s vs 60 s — both over. Attribution measured,
  not assumed: excluding tier-E2 the inner loop is 23.3 s (within budget),
  excluding tier-E1 the slow tier is 41.3 s (within budget). The overspend is
  the pre-existing Android emulator tier on this host (emulator up since
  before the session, e2e flaky-red on the clean baseline), not the added
  guard, which measures 0.08 s. A later inner-loop run without the guard
  measured 410 s — the same tier's variance, which is the documented
  intermittency, not a regression. The script's local/CI delta caveat applies;
  trimming coverage to meet the budget is not this task's call (AGENTS.md:
  a budget is not a reason to drop coverage).
