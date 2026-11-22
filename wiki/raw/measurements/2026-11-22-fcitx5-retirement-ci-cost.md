---
title: "What retiring fcitx5 would do to CI build time — measured A/B, not a verdict"
created: 2026-11-22
updated: 2026-11-22
type: measurement
status: active
tags: [ci, fcitx5, ibus, measurement, timing]
sources:
  - ../../../docs/CI-TIMING.md
  - ../../../.github/workflows/ci.yml
  - https://github.com/scobyseo/hunmin/issues/316
confidence: high
ingested: 2026-11-22
---

# What retiring fcitx5 would do to CI build time

The maintainer plans to retire fcitx5 once IBus evaluation is sufficient
(their words, 2026-11-22: "이건 내가 ibus 평가를 충분히 한 후에 fcitx5를
퇴역시킬거라서 그래"), and asked what replacing the fcitx5 PR test cycle
with IBus would do to build time. This page records the measurement; the
retirement decision and its timing are the maintainer's.

The key structural fact: **the IBus lanes already run in the PR job**
(issue #316), so "replace" is almost pure subtraction — nothing new needs
to be built for the comparison.

## Local A/B, runner-equivalent conditions

Same tree, two fresh build dirs, no Android SDK (the runner has none),
`-DHUNMIN_TRACE_BUILD=ON`, `-j4` (the runner's core count), suite
`-LE 'tier-E1|tier-D3'` (both skip on the runner):

| | fcitx5 ON | fcitx5 OFF | delta |
|---|---|---|---|
| configure + build | 36.6 s | 20.6 s | **−16.0 s (−44%)** |
| ctest suite | 33.5 s | 15.6 s | **−17.9 s (−53%)** |
| tests ran | 160 | 131 | −29 |

Both 100% green. The 29 removed: 24 fcitx5 tier-C trace lanes,
engine-d1-fcitx5, engine-d1-judgement, engine-d2-ui, frontend-mutations,
trace-optout, builds-fcitx5-addon. The IBus equivalents of the D1/D2/C
lanes stay (they are already in the OFF column).

## The PR job, projected from measured steps

Current linux job, measured (runs 45002302838 / 45008187622): ~164–173 s
warm. fcitx5-attributable segments:

| segment | now | after retirement | basis |
|---|---|---|---|
| apt (fcitx5's ~25-package dev list) | 36 s | ~12–18 s | jammy's IBus-only list measured 16–21 s |
| fcitx5 cache restore/install/upload | ~6 s | 0 | step timings |
| **fcitx5 source build, cache MISS** | **197 s** | **0** | docs/CI-TIMING.md §5 |
| cmake build -j4 | 23 s | ~13 s | A/B −44% |
| ctest -j4 | 72 s | ~34–40 s | A/B −53% |

Projected: **~90–100 s warm** (vs 164–173 s), and — the structural part —
**the cold path fits the 3-minute ceiling too** (~110–130 s): the 197 s
fcitx5 build that today makes a cache miss a guaranteed timeout simply
ceases to exist, and the cache machinery can go with it. The ceiling
would no longer depend on a cache hit.

Local development: the D3 slow tier drops from ~190 s serial (fcitx5
x11/gtk3/gtk4/edit/candidate) to ~115 s (IBus three lanes, measured
2026-11-22).

## What exists only on fcitx5 today (inputs to "sufficient evaluation")

Facts, not a verdict — the retirement criterion is the maintainer's:

- **frontend-mutations** (25.5 s): the mutation tier targets the fcitx5
  adapter's seams. The IBus equivalent is a documented roadmap gap
  (docs/TESTING.md coverage table). Retiring without rebuilding it loses
  mutation coverage.
- **D3 GTK4 client**: IBus D3 runs GTK3 only (documented gap).
- **D3 XIM client path**: fcitx5's XIM server is clean; IBus's was measured
  to send compound text, commit a stale preedit snapshot, and route a
  teardown flush into the next case ([[2026-11-22-ibus-coverage-parity]]).
  Non-GTK X11 clients reach IBus through that host path.
- In the other direction: the IBus judgement lane already runs the FULL
  corpus including the latin-start cases fcitx5's lane skips — IBus is a
  superset there.
- Packaging: the nobilus profile is built and measured nightly (the
  ibus-jammy-package job); the noble .deb currently ships the fcitx5
  addon.
- Cost shape that stays: IBus D2 is ~8.5 s vs fcitx5's ~0.4 s (GTestDBus
  per test) — already in the job, not a new cost.

See also [[the-rules-keep-breaking]] for why the coverage items above are
listed as facts to weigh, not smoothed into the time saving.
