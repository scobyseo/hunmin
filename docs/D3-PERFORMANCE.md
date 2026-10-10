# Tier D3 — where the time actually goes

Measured 2026-10-10 on the host in `docs/ENVIRONMENT.md`, from a `PS4`-stamped
copy of `tests/e2e/run-d3.sh` (a temporary instrumented copy, not committed;
the numbers below are the deltas it printed at each step of `run_case`).

## Why this page exists

The D3 speedup — reuse the XIM harness per layout group, the GTK harness per
case — was written on the premise that **starting a client harness was the
majority of the D3 tier** and that reuse would take `engine-d3-gtk3` and
`engine-d3-gtk4` from ~50s to ~15s. Reuse was implemented for the GTK client
and it is sound. The premise was wrong: harness startup is 8% of the GTK run.
This page records the breakdown, because the next person to be handed "make the
GTK D3 tests fast" will otherwise re-derive it.

**The corpus.** `tests/e2e/cases.tsv` has 8 cases. They are *not* two
contiguous layout groups: the file interleaves them, so `run-d3.sh` sees four
layout changes and starts fcitx5 **four** times, not two. A layout group in
`run_case` means a contiguous run of one layout — four of them, two cases each
for the middle two and one case each at the ends.

## The breakdown, GTK 3.0, ~49.7s for all 8 cases

| component | measured | share |
|---|---|---|
| `xtest-type.py` key injection | 24.4s | 49% |
| fcitx5 start/stop, 4 restarts @ ~2.55s | 10.2s | 21% |
| fixed sleeps, 8 × (0.4 + 0.5 + 0.3) | 9.8s | 20% |
| **GTK harness start → READY, 8 × 0.56s** | **4.5s** | **9%** |
| per-case teardown (`SIGTERM` + `wait`) | 0.2s | <1% |

The injection figure is entirely the four long-text cases: `dkssuds`, `jk1`
and `kfcQ` inject in 0.4–0.8s, while the four sentence cases take 5.0–6.0s
each. `xtest-type.py` sleeps `delay_ms` (default 40) after **every** key event,
so a press+release pair costs 80ms and an uppercase letter 160ms; the longest
case is 72 characters.

Two figures from that change do not survive measurement:

* *"Starting a harness costs ~5s (X connect + XIM handshake + IC creation)"* —
  for the **XIM** harness, `run_case-begin → READY` measures **0.14s**. The
  5s figure is not what this host does, and per-case start therefore costs
  ~1.1s over a whole eight-case tier, not the "80% of the tier" claimed.
* *"starting a GTK harness costs several seconds"* — it reaches `READY` in
  0.56s. Separately, a GTK 4 harness started with **no fcitx5 on the display**
  takes 5.6s, which is probably where the impression came from; with the stack
  up, both versions are ~0.5s.

## What reuse is worth, measured

| | before | after |
|---|---|---|
| `engine-d3-gtk3` | 50.41s, 8 cases | 48.9s, 8 cases |
| `engine-d3-gtk4` | 49.42s, 8 cases | 48.8s, 8 cases |
| `engine-d3-x11` | 5.01s, **1 case** | 46.3s, **8 cases** |

The GTK saving is ~1.5s and that is all reuse can return: 8 starts become 4,
at 0.56s each. The X11 row is not a regression — see
`wiki/raw/measurements/2026-10-10-d3-case-coverage.md` for why 5.01s was one
case.

## Where ~15s would have to come from

Not proposed here; these are the three levers, with what each would cost, for
the maintainer to rule on.

1. **`xtest-type.py`'s 40ms per event — 24.4s.** The obvious one, and also the
   one that changes what the tier measures: that delay is the margin between
   injection and the stack processing each key. Halving it would take the tier
   to ~37s; a fifth of it, ~32s. Whether the margin is still needed is a
   question about fcitx5 and Xvfb that nothing here answers.
2. **The four fcitx5 restarts — 10.2s, ~2.55s each.** Reordering `cases.tsv`
   so each layout is contiguous would halve this to ~5s with no change to any
   assertion. It is a one-line change to the case file and it was *not* made
   here because the file's ordering is itself a record of how the corpus grew,
   and reordering it rewrites the tier's history. The tier is also correct as
   it stands: four layout changes are four genuinely different stack
   configurations.
3. **The fixed sleeps — 9.8s.** Two of the three are already load-bearing and
   documented in `docs/TESTING.md` (the post-injection settle, and the
   pre-injection settle whose removal produced a real `TEXT:dㅏㄴ녕ㄴ`). Only
   `sleep 0.5` / `sleep 0.3` after injection are candidates, and replacing
   them with a "buffer stopped changing" debounce in the harness is a
   behavioural change to the settle criterion, not a speedup.

1 + 2 together reach ~22s. Nothing here reaches 15s without also changing the
injector's pacing.