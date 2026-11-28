---
title: "T2-3: the mid-word flicker, counted, and a stability knob"
created: 2026-11-24
updated: 2026-11-24
ingested: 2026-11-24
type: session
status: active
tags: [modes, e6, expectability, gates, measurement, issue-295]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/MODES-MEASUREMENT.md
  - ../../../docs/MODES-GATES.md
  - ../../../docs/DESIGN-CORE.md
  - ../../../libhunmin/src/buffered_preedit.c
  - ../../../tools/modes-metrics.py
  - ../../../tools/autoswitch-gates.py
  - ../../../tests/golden/judgement-r8.jsonl
  - https://github.com/scobyseo/hunmin/issues/295
  - https://github.com/scobyseo/hunmin/issues/284
confidence: medium
---

# T2-3: the mid-word flicker, counted, and a stability knob

Issue #295, G3 of the Track 2 gates ([[2026-11-21-modes-failures-and-the-domain-mode-idea]]
is the finding that shaped Track 2): "mid-word display flips reduced at equal pool
share". The gate is a shape, not a number, so the first job was to make the unit
exist at all — nothing in the tree counted a display flip on a corpus.

## 1. What the gate's two halves force

"Reduced" and "at equal pool share" cannot both be true of a judgement change: the
pool share `q` is a function of what the boundary delivers, so anything that moves
`q` is not the comparison G3 asks for. The only quantity left is the screen, and
`judged_display` is the only place the screen is decided. That settled the design
before any code: count the display, change the display, and let the equality of `q`
be checked from the same pair of runs rather than assumed.

## 2. The unit, and why it had to be spelled out

`docs/MODES-MEASUREMENT.md` §2.5 now defines a flip as a change, between two
consecutive keys of one eojeol, of the language a preedit RENDERS. Two details are
load-bearing and were got wrong in the first draft:

- **Keys that render no language** (the empty preedit at a delimiter, ReqMode6's
  digit runs) neither count nor clear the previous answer. A counter that reset on
  them read a held digit as a flip.
- **The arm matters.** The first count used the repaired arm of `modes-metrics`
  and came out 37% higher than the raw ON arm (3,500 vs 2,546 on kldp-772): the
  repaired arm moves the mode to the script a person would have pressed, so it is a
  keystroke model, not a screen. The report counts the raw ON arm.

The detector's own guard is `flips_below_the_guard`, which must be 0: ReqMode9
refuses to judge under three key inputs, so a flip in a shorter eojeol would be a
display that judged without the guard. `modes-metrics-selfcheck` now also fails if
it cannot see a known flicker (`alphabetically`) or if the stability setting stops
reducing it — a counter that cannot fail loudly is the §3 failure class this
document keeps recording.

## 3. The knob, and the line it does not cross

E6's ruling (2026-11-01) made the per-key judgement an option with per-key as the
default: *"옵션처리하자. 설정할 수 있게. default는 B로."* A worker may not move that
default, so the mechanism ships as a second option, `judge_hold`:

- `0` — the shipped default — is the ruled rendering: the display adopts a changed
  per-key judgement at once.
- `N>0` — the display waits until the judgement has agreed for `N+1` consecutive
  keys. One key of display latency buys the removal of the one-key flash.

It is not `judge_timing` and it is not a judgement change: `judge_boundary` never
reads it, `take_judgement` still answers once per word, and the delivered text is
byte-identical at every value. That is what makes "equal pool share" structural
rather than an empirical claim — and the pair of runs confirms it to the digit.

State is three fields (`judge_verdict`, `judge_run`, `judge_shown`), advanced by
exactly one call per consumed key (`build_preedit_adv(l, true)` from
`process_key`); `display()` and the toggle's re-render pass `false`, so a re-render
cannot count as a second agreeing key. The run is the word's: `boundary()`,
`reset()` and any option write clear it.

## 4. What it measured

| corpus | words | flips at 0 | flips at 1 | flip-backs at 0 → 1 | `q` both runs |
|---|---|---|---|---|---|
| kldp-772 (6,690 words) | 6,690 | 2,546 | 477 | 591 → 37 | −4.379396 |
| wiki-2500 (8,927 words) | 8,927 | 1,687 | 304 | 432 → 35 | −6.320790 |

Hangul-typed eojeol (E6's own complaint): 1,513 → 273 and 1,031 → 198.
Latin-typed (the E1 mirror): 1,033 → 204 and 656 → 106. The figures, strata and
provenance are in `docs/SPEC-MODES.md` §E6; only the pointers live here.

The dictionary population `tools/modes-boundary-probe.py` counts (64.05% of forms
flipping at least once) is measured at the shipped default and does not move.

## 5. What is open

The **default** is the maintainer's: `judge_hold` ships at 0, so the ruled rendering
is untouched, and whether 1 becomes the default is an E6 ruling made from the table
above — the ruling the per-key default itself came from is
[[2026-11-01-the-judgement-timing-is-per-key-by-default]]. What it would decide
between: flashes nearly gone versus a genuine mid-word flip arriving one key later.
G1 still fails on both corpora (`docs/MODES-GATES.md` keeps that finding); the G3
gate grades the pair and says nothing about the feature's overall worth.

Also not done: no frontend exposes the new row (E6 already states that no frontend
exposes `judge_timing` either), and the felt experience remains unmeasurable here.
## 6. What it cost, measured before pushing

The rule is `tools/ci-time-budget.sh build` before anything that adds a test. Run
on this host it reports both local budgets over — **pre-existing**, the state
`docs/CI-TIMING.md` §6 records for master, and this host's attached emulator ran
tier E2 for ~400 s in one of the runs and skipped in another, which is the
documented local/CI delta rather than anything the change did.

So the number that belongs to this work is the delta on the tests it touched,
measured against a detached worktree at `master` (`731633c`) on the same host,
with `ANDROID_HOME`/`JAVA_HOME`/`ANDROID_NDK` exported and the keytable cache
removed first so both sides rebuilt it:

| test | master | this branch | delta |
|---|---|---|---|
| `engine-buffered-preedit` | 0.33 s | 0.29 s | noise |
| `engine-golden-judgement` | 0.65 s | 0.72 s | +0.07 s (4 more cases) |
| `modes-metrics-selfcheck` | 7.70 s | 7.68 s | noise |
| `modes-gates-selfcheck` | 0.07 s | 0.07 s | — |
| `docs-lint` | 8.05 s | 8.67 s | noise |
| `req-map` | 1.29 s | 1.40 s | noise |

No budget line moves. What is added is 4 golden cases, 4 tier-A cases and two
selfcheck assertions; what is *not* added is a test that replays a corpus, because
the corpus lives outside the repository and a ctest that needed one would be red
in a fresh worktree.
