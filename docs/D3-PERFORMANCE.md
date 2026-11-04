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

## 2026-11-04 — two levers measured: one taken, one refused by measurement

The edit tier (`engine-d3-edit`) is the one that was on the critical path
(94.0 s of the slow tier's 94.8 s), so this round measured its two biggest
components instead of the GTK tier's.

### Lever taken: the tier was running another registered test's cases

`engine-d3-edit` ran `cases-edit.tsv` and then `cases.tsv` — and
`engine-d3-x11` runs that same `cases.tsv` through the same XIM stack and
client. The duplication was 8 of the tier's 18 cases and **43.6 s of its
93.3 s**, with no assertion the x11 tier does not already make. The CMake
comment for the tier already said the edit cases are separate "so a failure is
diagnosed by its scenario, not mixed into the ordinary D3 cases"; the extra
run contradicted it. `run-d3.sh` now stops after the edit loop.

| | before | after |
|---|---|---|
| `engine-d3-edit` | 94.0 s | **49.7 s** |
| slow tier (`-j16 -L slow`) | 94.8 s | **50.0 s** (3 runs: 50.0 / 50.2 / 50.3) |
| D3 failures in those runs | 0 | **0** |

Both are inside the 60 s slow-tier budget, with the injector untouched — no
margin moved.

### Lever refused: lowering the injection delay

The 40 ms per event is half of the edit tier and the obvious target. It is also
the margin between injection and the stack processing each key, so the first
sweep looked decisive — six `ctest -j16 -L slow` runs, three D3 tiers starting
three X stacks at once:

| delay | runs | runs with a D3 failure |
|---|---|---|
| 40 ms | 3 | 0 |
| 20 ms | 2 | 1 |
| 15 ms | 2 | 0 |
| 10 ms | 2 | 2 |
| 5 ms | 1 | 1 |

**A 5-pair A/B then undercut that reading, and the A/B is the experiment that
counts.** Alternating the two values on the same tree, same selection:

| delay | runs | runs with a D3 failure |
|---|---|---|
| 40 ms | 5 | **2** (`engine-d3-edit` twice) |
| 5 ms | 5 | **3** (`engine-d3-edit` 3×, `engine-d3-x11` 3×) |

2/5 against 3/5 at n=5 does not separate the two values: **the delay is not
established as the cause, and lowering it is not established as safe.** What the
A/B does establish is worse than either: this host currently flakes a D3 tier in
roughly half of `-j16` slow-tier runs *at the shipped 40 ms too*, with two
failure modes that are both stack-level rather than injector-level —

- `FAIL […]: key injection failed` — `xtest-type.py` exited nonzero, i.e. it
  could not inject into the display at all; and
- a garbled **first** word followed by correct output (`ㄷ왕ㄶ내ㅕㅁㅇ눌과` where the
  engine says `동해물과`), i.e. keys arriving before the case's focus settled.

Neither is a lost key in the middle of a sentence, which is what a too-short
margin would produce. The standalone tier passes; only the parallel selection
shows it.

**And it is not this change.** Master's own `run-d3.sh`, injector untouched at
40 ms, three `-j16` slow-tier runs: **2 had a D3 failure** (`engine-d3-edit`),
one ran clean at 93.4 s. Same failure messages, same tier. So the flake predates
the dedupe above and has its own issue (#250); what this page records is that no
measurement of a D3 tier's *wall time* is trustworthy while it is in the mix — a
run that loses a tier halfway finishes faster than a green one (70 s against
93 s on the same runner).

**1 ms is not a lever at all.** With the duplication removed, injection is ~3 s
of the 27.5 s the tier would take at 5 ms, and measured: 5 ms 27.5 s, 1 ms
25.1–27.6 s. It buys ~2 s and, per the A/B, does not buy reliability.

**Serialising the D3 tiers is the other way to remove the failure mode, and it
costs the budget.** Measured at 1 ms, `-j1` over the three tiers: **63.9 s** and
**64.3 s**, both fully green (`engine-d3-x11` 24.0 / 23.9, `engine-d3-edit` 24.8
/ 25.3, `engine-d3-candidate` 15.1 / 15.2). That is the three D3 tiers alone,
before the rest of the slow tier — `frontend-mutations` is 37 s in the same
selection — and the budget is 60 s *wall*. Parallel, the same selection costs
50 s and flakes about half the time. So `RUN_SERIAL` on the D3 tiers would move
the slow tier from "flaky under budget" to "reliable and ~2× over budget"; that
trade is a decision, and it is not taken here.

Note what the flake rate means for this document: a single green `-j16` run is
not evidence that a D3 change is safe, and neither is a single red one. Rates
need repeats at the same value — the A/B exists because the first sweep, which
looked decisive at n≤3, did not survive one.

### What is left, in measured size

For the edit tier at 40 ms with the duplication gone (49.7 s): injection ~25 s,
the fixed sleeps ~17 s (per case 0.4 + 0.5 + 0.3, per layout restart 1.0, ~0.1
polling), stack startup ~4 s, parsing/teardown ~2 s. The injector can only be
reduced by giving the stack something better to synchronise on than a clock:
a per-key or per-run completion signal from the harness would let the injector
run at full speed on an idle box and slow down only when the stack lags. That is
a harness change, not a constant, and it is not measured here.

A probe of the *other* structural option was taken incidentally: the `-j16`
selection starts three X stacks that between them consume all six display
reservations (`/tmp/hunmin-d3-disp.94` … `.99`), so a fourth concurrent D3 tier
would find none and skip. Not a defect at three tiers; it is the ceiling.

## 2026-11-04, second round — the dead time the gate already proves

Two waits, both on the XIM path, both measured before they were removed.

**1. The post-gate settle.** `start_fcitx` waits for two facts: the root-window
`@server=fcitx` property and `Loaded addon hunmin` in the log. Instrumented over
51 starts — 24 with three candidate tiers at once, 24 with the real
x11+edit+candidate mix, 3 under 16 CPU hogs on a 16-core box — the gate is true
at **1.20–1.35 s** (1.44–1.76 s starved), and `@server=fcitx` and `Loaded addon
xim` become true in the *same poll*. After it the script slept `0.3` inside
`start_fcitx` and another `1.0` in the case loop, three times a tier. Both are
now `0.1`: a settling allowance, not a wait for a signal. GTK keeps its second
(a reused harness wires the IMContext per case, and gtk3/gtk4 skip here); the
candidate path keeps its own.

**2. The post-typing settle.** `sleep 0.5` then `sleep 0.3`, with nothing between
them — one 0.8 s wait written twice, and the comment on the second sleep made it
read as two stages. On the XIM path that clock is unnecessary: a fresh client
process per case writes each line as it arrives, so a log that stops growing is
the completion signal. Measured over 27 cases: quiet after **0.119–0.213 s**
(median 0.122, p90 0.126). The debounce is 3 consecutive 30 ms quiet polls
(~90 ms) with the old 0.81 s as its cap, so a slow client behaves exactly as
before; hitting the cap prints a note and is not an assertion — the comparison
that follows is. GTK keeps `sleep 0.8` (the same two sleeps, collapsed, because
its settle is load-bearing: at zero settle a case came back `TEXT:dㅏㄴ녕ㄴ`).

| tier | before | after (3 runs) |
|---|---|---|
| `engine-d3-edit` | 49.4 s | **40.5 / 40.3 / 41.7 s** |
| `engine-d3-x11` | 46.8 s | **37.6 / 37.4 / 39.4 s** |
| `engine-d3-candidate` | 16.8 s | 16.9 / 16.7 / 17.3 s (untouched) |
| slow tier wall | 49.3 s | 43.7 / 41.7 / 56.6 s |

**The wall clock is no longer D3's.** `frontend-mutations` measured 43.7, 41.6
and 56.6 s in those same three runs — it is now the slow tier's long pole, and
the D3 seconds bought here do not show in wall time until that tier is bounded.
The 60 s budget therefore survives by 3 s on a bad run (56.6 s), not by 16 s.

## 2026-11-05 — what the gate that never fired was actually looking at (issue #253)

Issue #253 caught one start in eight parallel slow-tier runs where the hunmin
addon loaded at +1.09 s and the root-window `@server=fcitx` property never
appeared inside the 20 s gate, and left two readings open: the xim addon can
lose a race and register late, or the gate asks a question the display can
answer wrongly. Neither is what the daemon was doing. This round instrumented
the shipped gate, caught two occurrences, and the evidence is below.

### The gate now records a timeline, on every start

`start_fcitx` times each of its three facts (`Loaded addon xim`,
`Loaded addon hunmin`, the `@server=fcitx` property) and appends one line per
start to `$STAGE/gate.log`, kept with `HUNMIN_D3_KEEP=1`. On a failure it names
which fact was missing and dumps the root-window properties, the daemon's
process state and environment, the X sockets, the auth entries **by display name
only** (a cookie is a credential and does not belong in a test log), the Xephyr
and Xvfb logs, and the complete `fcitx5.log` rather than its tail. It also counts
how many of its own `xprop` polls could reach the display, because that is the
fact that separates "nothing published" from "unreachable from here too", and it
used to be discarded with stderr.

The failure path only runs when a real run flakes, so nothing else would notice
it breaking: `tools/d3-gate-failure-test.sh` extracts `start_fcitx` from the
script and drives it against a stub daemon that loads both addons and never opens
an X connection — the shape both caught occurrences had — asserting the message,
every dump section and that no xauth cookie value reaches the log. It is not
registered in ctest: it costs the gate's own 20 s and guards no behaviour the
tier passing does not already guard.

Measured over 201 timed starts in two batches (3 XIM tiers at once, `-j16`):

| fact | min | median | p90 | max |
|---|---|---|---|---|
| `@server=fcitx` property | 112 ms | 122 ms | 139 ms | 270 ms |
| `Loaded addon xim` | 112 ms | 122 ms | 141 ms | 270 ms |
| `Loaded addon hunmin` | 1104 ms | 1340 ms | 1464 ms | 1616 ms |

The property and the xim addon appear in the *same* poll every time, and the
gate closes on the hunmin addon 1.0–1.4 s later — 1.2 s against a 20 s budget.
The timeline costs three `grep`s and one `xprop` per 100 ms poll; the three-tier
selection measured 39.5–41.0 s per round both before and after the change, so
nothing moved against the 60 s slow-tier budget.

### What the two caught failures were

Two of 52 parallel rounds failed inside the gate (`engine-d3-edit` once,
`engine-d3-x11` once); four more failed somewhere else in a batch whose
per-failure output was not captured, and none of those was the gate. Both gate
failures share one signature, and it is not a race in the registration:

- `Loaded addon xim` at 116 ms and `Loaded addon hunmin` at 1.2 s — both addons
  loaded;
- `@server=fcitx` never, in 200 polls over 21 s;
- the full `xprop -root` for the display carries **only** `_XKB_RULES_NAMES`:
  no XIM property of any kind, so nothing had published and nothing had taken
  the name;
- fcitx5 alive in `ep_poll`, no children, dbus name taken — a healthy daemon;
- `DISPLAY` was the right display both times (`:95`, `:96`), and its socket
  existed at dump time;
- the log carries no `Connecting to X11 display, display name::NN.` line, which
  every passing start has (it sits between `Loaded addon wayland` and
  `Loaded addon xcb`).

That last fact is the decisive one. Probed directly: with `DISPLAY` pointing at
a display with nothing on it, and with `DISPLAY` unset, fcitx5 logs
`Loaded addon xcb` and **no** connection line and no error — a failed X
connection is silent. So the daemon in those two starts never had an X
connection, and its xim addon had no display on which to register an XIM server.

### What this rules out, and what it does not

Ruled out: "the gate is too short" (starvation moved it by 0.4 s in the earlier
round, against a 20 s budget), and "the xim addon registered late or lost the
selection" (nothing was registered; the property never existed). The gate's
question was sound — the daemon was the thing that was wrong, and "did not come
up" was, accidentally, close to right.

Not explained: **why the connection failed**, and why it fails on a *restart* —
both occurrences were later starts in a stage that had already served one or
three successful starts on that display. The next occurrence carries the
reachability count, the daemon's environment and the server logs, so this is a
question with an instrument pointed at it rather than a re-instrumented copy.

Three ways to act on it, none taken here (they differ in what they cost when the
daemon is genuinely broken): leave the tier red when it happens and read the
dump; retry `start_fcitx` once when the daemon logs no X connection, which turns
the flake into 20 s of lost time but would also hide a daemon that was never
going to connect; or gate on a readiness marker the addon writes after the XIM
server is registered, which changes what the tier waits for rather than what it
reports.