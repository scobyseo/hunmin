# MUTATION-TIER-OPTIONS — what `frontend-mutations` costs, measured

**Status: measurements only. No option is recommended, and none is ruled out.**
Every section below reports a number, what it costs, and what it would lose.
The choice is the maintainer's.

Produced 2026-10-10 on `task/MUT-options`, from the base where the mutation
strategy landed, by instrumenting `tools/frontend-mutations.py` rather than by
editing it: the tier file is **unchanged** by this work, and
`git diff tools/frontend-mutations.py` is empty.

The tier's own docstring states why it exists, and that is the constraint every
option below is measured against:

> docs/FRONTEND-CONTRACT.md §5 claims those invariants are guarded. A guard
> nobody ever tries is an assumption.

---

## 0. The premise, checked first

The task this responds to says the tier is "the second-longest thing in the
suite" and that the goal is a whole-suite `ctest` under 30 s. Both halves were
measured before anything else, because AGENTS.md's rule is to check the stated
premise as carefully as one's own result.

**The suite's critical path is not this tier.** Full `ctest -j16` on this host,
55 tests registered:

| rank | test | time |
|---|---|---|
| 1 | `engine-d3-gtk3` | **51.63 s** |
| 2 | `engine-d3-gtk4` | **51.05 s** |
| 3 | `frontend-mutations` | 30.10 s |
| 4 | `docs-lint` | 6.11 s |
| 5 | `engine-d3-x11` | 5.50 s |

Suite total 51.77 s. Excluding the tier entirely (`ctest -E frontend-mutations
-j16`) gives **51.35 s** — a saving of **0.42 s**, because the two GTK D3 tests
run concurrently with everything else and the suite is bounded by them.

So on this host the tier costs nothing at the suite level as long as D3-GTK
runs in parallel with it, and **the 30 s goal is not reachable by any change to
this tier.** That is a finding about the premise, not a verdict on the tier.
Whether the 30 s target is worth restating, and whether D3-GTK's 51 s is itself
addressable, are separate questions this document does not answer.

*(Pre-existing on this base and unrelated to this work: `data-selfcheck`,
`docs-lint` and seven `engine-android-conformance-*` tests fail here. The
`docs-lint` error is a `REMAINING-WORK.md` row about pyright, not a
consequence of anything below.)*

---

## 1. Where the 27 s actually goes

`tools/mutbench.py --mode vanilla` wraps the tier's own `main()` and times
every subprocess. Three runs, sandbox strategy, unoptimised, this host:

| phase | n | run 1 | run 2 | run 3 |
|---|---|---|---|---|
| per-mutation `cmake --build` | 9 | 18.69 s | 18.41 s | 18.46 s |
| sandbox first build | 1 | 5.87 s | 5.38 s | 5.71 s |
| `ctest -R engine-d2-ui` | 8 | 1.28 s | 1.27 s | 1.30 s |
| `cmake -B build` (configure) | 1 | 1.24 s | 1.17 s | 1.21 s |
| `ctest -R engine-d1-fcitx5` | 8 | 0.76 s | 0.77 s | 0.77 s |
| `check-frontend-invariants.py` | 8 | 0.19 s | 0.19 s | 0.18 s |
| `copytree` | 1 | 0.05 s | 0.04 s | 0.05 s |
| python overhead | — | 0.02 s | 0.02 s | 0.02 s |
| **wall** | | **28.10 s** | **27.24 s** | **27.70 s** |

Unattributed time is under 20 ms in every run, so the breakdown is complete.

**Reading it: 67 % is compiling `hunminadapter.cpp` nine times.** Setup (copy +
configure + first build) is 26 %, and the three guards together are 8 %.

### 1.1 The per-mutation build is one translation unit

Instrumenting the compile and link separately, in a prepared build tree:

| step | time | share |
|---|---|---|
| compile `hunminadapter.cpp` | 1.544–1.602 s | **91 %** |
| link `libhunmin.so` (the addon) | 0.063–0.093 s | 5 % |
| make bookkeeping (no-op targets) | 0.335 s | 4 % |

Two structural facts, both verified by mtime rather than inferred:

- **`testhunmin` and `testhunminui` are never recompiled or relinked.** They
  `dlopen` the addon at runtime (`testhunminui.cpp:302` says so in a comment),
  so a changed `hunminadapter.cpp` leaves both binaries byte-identical and untouched.
  `MUT_TARGETS = ["hunmin", "testhunmin", "testhunminui"]` therefore names two
  targets the mutation cannot affect.
- **Preprocessing is free** (`-E -P`: 0.005 s against 1.544 s). The cost is
  entirely code generation for one 492-line file.
- `-fsyntax-only` is 1.293 s, i.e. 84 % of the full compile is semantic
  analysis and code generation, not parsing.

---

## 2. Which mutations need a compile, and what skipping would lose

The table's own columns say `expect_checker` / `expect_d1` / `expect_d2`, and
the file describes a `False` as "data, not failure" — a marked gap.

| mutation | checker | D1 | D2 | needs a build for its recorded verdict? |
|---|---|---|---|---|
| I1-commit-flush-both | ✔ | ✘ | ✘ | **no** — the checker is the only layer that rejects it; the two ✘ are the recorded gaps |
| I5-caps-dropped | ✔ | ✔ | ✘ | **yes** — D1 is recorded as rejecting it |
| I5-layout-override-ignored | ✔ | ✔ | ✘ | **yes** — same |
| I3-accept-unguarded | ✔ | ✔ | ✘ | **yes** — same |
| D2-preedit-never-set | ✘ | ✘ | ✔ | **yes** — D2 is the *only* guard |
| D2-client-preedit-dropped | ✘ | ✘ | ✔ | **yes** — same |
| D2-cursor-at-zero | ✘ | ✘ | ✔ | **yes** — same |
| D2-panel-never-cleared | ✘ | ✘ | ✔ | **yes** — same |
| I4-shift-chord | ✔ | ✘ | ✘ | **yes, uniquely** — see below |

### 2.1 `I4-shift-chord` is the one that only a compile can decide

Its comment reads `# does not compile; reported as a build failure below`. The
mutation ORs `KeyState::Shift_L` into a `KeyStates` mask. **The build failing
IS the recorded expectation** — and it is also the checker finding, since I4
checks that `Shift` is absent from the mask textually. So this row is the
strongest argument in the file for keeping a compile: a no-build variant must
still decide it, and the only two ways to do so are a compiler invocation or a
re-implementation of the type rule in the checker.

### 2.2 Measured saving from a checker-only variant

`--mode skip-checked` runs the checker and stops for the five mutations the
table records as checker-caught; the other four take the full path.

| configuration | wall | build time | builds run |
|---|---|---|---|
| baseline (all nine) | 27.24–28.10 s | 18.4–18.7 s | 9 |
| checker-only for the 5 | **17.35 s** | 8.81 s | 4 |

**Saving: ~9.9 s (36 %).**

**What is lost, precisely.** For four of those five rows the `d1` column stops
being measured and becomes "not measured":

- `I5-caps-dropped`, `I5-layout-override-ignored`, `I3-accept-unguarded` each
  record **D1 rejects** — the guards that per-item `Layout=` handling and the
  caps assertion closed known gaps in. A checker-only tier would no longer prove
  D1 still rejects them, so a future change that silently broke D1's coverage of
  caps/layout/acceptance would leave this file reporting a verdict it never
  observed. That is precisely the
  "guard nobody ever tries is an assumption" failure, one layer down.
- `I1-commit-flush-both` records D1 and D2 as **✘** — a *known gap*. Skipping
  the build stops observing the gap. The gap would not be closed by the
  checker-only variant; it would simply stop being written down, which reads
  the same as closed.
- `I4-shift-chord`'s build-rejection stops being observed (see 2.1).

Nothing is lost for the four D2-only rows, which take the full path anyway.

**Evidence that would change the answer.** If D1's rejection of those three
were considered settled and no longer worth re-measuring per run — a maintainer
call, not an agent's — the 9.9 s is real and the only cost is the gap row
going unwritten.

---

## 3. Narrower target sets

Measured with `--mode targets`, which changes only the per-mutation build's
`--target` list. Setup still builds the tier's full `MUT_TARGETS`, so both test
binaries exist before the first mutation.

| per-mutation targets | wall | build time | verdicts |
|---|---|---|---|
| `hunmin,testhunmin,testhunminui` (registered) | 27.24–28.10 s | 18.41–18.69 s | 9/9 correct |
| `hunmin_addon` | 25.76 s | 16.34 s | 9/9 correct |
| `testhunminui` | 25.34 s | 16.39 s | 9/9 correct |
| `hunmin,testhunmin` | 27.13 s | 17.79 s | 9/9 correct |

**Saving: 1.4–2.4 s (5–9 %)**, not the 9 s one might hope for, because §1.1
already shows the two test binaries are not rebuilt anyway. `hunmin_addon` is
the honest minimum: it is the target that *is* the mutation.

### 3.1 The trap, measured rather than asserted

Narrowing the target list looks free, and it is not — **unless setup still
builds everything**. Building only `hunmin_addon` from a clean tree gives:

```
first build (hunmin_addon only): rc=0 1.94s
  testhunmin  exists after narrow setup: False
  testhunminui exists after narrow setup: False
  ctest D1: rc=8   1/1 Test #53: engine-d1-fcitx5 ... ***Not Run   0.00 sec
  ctest D2: rc=8   1/1 Test #55: engine-d2-ui   ... ***Not Run   0.00 sec
```

**`ctest` returns 8 for "did not run" exactly as it returns 8 for "failed", and
`frontend-mutations.py:338` reads `d1 != 0` / `d2 != 0` as "rejected".** A tier
that stopped building a test binary would read every row as guarded, on the
strength of a test that never executed. `mutbench.py` therefore checks ctest's
output for `Not Run` and fails on it; all four target variants above pass that
check, which is why their verdicts are believable.

This is the same class of defect as `docs/D3-ROOT-CAUSE.md` — a tier green
because it measured nothing — reached from the opposite direction.

---

## 4. Making the per-mutation rebuild cheaper

The rebuild is 91 % compiling one translation unit, so these are the only
levers that exist.

### 4.1 ccache — measured as "cannot hit", without installing it

ccache is not installed on this host, and installing a package to watch it miss
is not a measurement worth making. Its hit condition is computable instead: a
hit requires **identical preprocessed source**, compiler and flags. Hashing the
preprocessed form of all nine mutants, compiled with the addon's own exact
flags from `flags.make`:

```
(pristine / first build)     pp-hash 8b3c4226d9f8c39b
I1-commit-flush-both         pp-hash 5a865b7b35c5907a  unique
I5-caps-dropped              pp-hash c990b2b9f4285d99  unique
I5-layout-override-ignored   pp-hash 4e955b6e881cc2ba  unique
I3-accept-unguarded          pp-hash d8f99e2a24b27a7a  unique
D2-preedit-never-set         pp-hash 1351a1e54ce21d6e  unique
D2-client-preedit-dropped    pp-hash 05baa5b2998bf3ea  unique
D2-cursor-at-zero            pp-hash 33e73f9cd7a1a4f1  unique
D2-panel-never-cleared       pp-hash f4673f6704b07376  unique
I4-shift-chord               pp-hash ab80b349e478b208  unique

distinct preprocessed inputs among the 9 mutants: 9
mutants identical to pristine: 0
ccache hits available on a warm cache: 0 of 9
```

**Nine distinct inputs, zero of them equal to the pristine build.** ccache
cannot hit on any of them: each mutation is by construction a source the cache
has never seen. The savings are **0 s**, and the conclusion does not depend on
ccache's configuration, size or version.

A ccache *would* help if the tier ever gained a mode that recompiles the same
source repeatedly — the nine-way loop above does not.

### 4.2 Precompiled objects — fast, and it only relocates the cost

Copying a known-good `hunminadapter.cpp.o` back over the mutated one and relinking:

| approach | time |
|---|---|
| baseline: compile + link | 1.692 s |
| restore a prebuilt `hunminadapter.cpp.o`, then link | 0.062 s |
| link only, no compile (theoretical floor) | 0.063 s |

Object reuse is as fast as the floor — **but** producing those nine objects
requires compiling the nine mutants, which is exactly the 14 s the tier already
spends. It moves the cost rather than removing it, and adds a cache whose
correctness (which object belongs to which mutation) becomes the tier's problem.

### 4.3 Precompiled headers — measured, and worse

A PCH over the addon's own headers builds to **126 MB**, and compiling
`hunminadapter.cpp` against it **did not finish within 120 s**, against 1.587 s
without. The fcitx5 headers are template-heavy; forcing them all into one
image is a large cost paid on every compile. **Negative result, measured.**

### 4.4 Splitting the mutant translation unit — not measured, and why that is honest

The obvious remaining idea is to move the mutated regions into their own small
TU so the big one is compiled once. It was not attempted, because the mutation
anchors are statements inside `HunminAdapter::process()` and `updateUI()` — they
depend on `this`, on `ic_`, on `engine_->config()`, and are read from the member
functions themselves. Any split that made them separately compilable would
restructure the adapter along the lines the I8 thin-adapter budget measures, to
save 1.5 s in a tier that §0 shows is not on the suite's critical path. Stated
as a finding, not a rejection.

---

## 5. Parallel workers — the setup cost, measured

`tools/mutparallel.py` runs the nine mutations across N independent sandboxes,
each with its own copytree, configure and first build. Every worker verifies the
full expectation table, so a fast run that stopped testing something shows up
as a mismatch rather than as a pass.

| workers | `-j` each | all ready at | mutation phase | **wall** |
|---|---|---|---|---|
| 1 (today) | 16 | 6.8 s | 18.5 s | **27.2–28.1 s** |
| 2 | 8 | 7.13 s | 11.25 s | **18.39 s** |
| 3 | 5 | 12.75 s | 9.83 s | **22.58 s** |
| 3 | 5 | 7.88 s | 7.50 s | **15.38 s** |
| 5 | 3 | 11.18 s | 6.20 s | **17.38 s** |
| 9 | 1 | 15.65 s | 4.31 s | **19.96 s** |

All five configurations produced **all nine verdicts matching their recorded
expectations**.

**Per-worker setup is 7–16 s, and it does not shrink** — it grows, because N
configures and N first builds contend for the same 16 cores. The best observed
wall clock is **15.38 s** (3 workers, `-j5`), a saving of ~12 s (43 %).

The spread between the two 3-worker runs (22.58 s vs 15.38 s, identical
arguments) is contention against whatever else the host was doing. **The
honest reading is that the best case is ~15 s and the typical case is worse**;
this host is shared, and CI runners are 2–4 cores, where the parallel variant
would be a loss rather than a saving.

**Evidence that would change the answer.** A dedicated runner with ≥12 cores
and no competing load, where N=3 setup stays near 7.9 s and the mutation phase
near 7.5 s. On a 4-core runner the arithmetic inverts: setup alone is ~7 s per
worker and there is no spare parallelism for the mutation phase.

### 5.1 Reusing a warm sandbox — 5 s, and only within one checkout

A sandbox kept between runs rather than rebuilt:

| | wall |
|---|---|
| cold sandbox (today) | 6.95 s setup + 18.15 s mutations = **25.1 s** |
| warm sandbox | 2.09 s restore + 18.99 s mutations = **21.1 s** |

**~4 s**, and only when the same checkout runs the tier twice — which `ctest`
does not do. A CI checkout runs it once, so this is 0 s there.

### 5.2 Seeding a sandbox from a prebuilt template — measured, does not survive relocation

Copying the working tree *including* `build/` and reconfiguring fails outright:

```
CMake Error: The current CMakeCache.txt directory /tmp/seed.../src/build/CMakeCache.txt
is different than the directory /home/scoby/work/hunmin-mut-options/build where
CMakeCache.txt was created.
```

Copying a prebuilt sandbox and rewriting absolute paths (118 files, 0.113 s)
also fails to help: the post-copy build recompiles **everything** — 7 C objects,
`hunminadapter.cpp`, `language_detect.cpp`, both test binaries — because the rewrite
invalidates every dependency file. Setup stays at 5.53 s.

---

## 6. Moving the tier out of the default run

**This is the shape of the option, not a recommendation.** Measured saving on
this host: **0.42 s** (§0), because the suite's critical path is `engine-d3-gtk3`
at 51.63 s.

### 6.1 What exists today

`.github/workflows/ci.yml` defines **seven** job blocks as of 2026-10-13 —
`linux`, `android`, `deb`, `android-release`, `mingw`, `windows` (disabled,
`if: false`, issue #51) and `nightly-fuzz` — on `push`, `pull_request` and a
daily `schedule`. `docs/ENVIRONMENT.md` §CI carries the table; an earlier
version of this file listed four jobs and claimed there was no nightly job,
which was already false before the fuzz job existed. `docs/REMAINING-WORK.md`
item **N2** proposes a nightly/fuzz tier and is listed as not started: the
`nightly-fuzz` job is a LeakSanitizer sweep, not the libFuzzer 8h tier N2 asks
for.

No ctest test in this project carries a `LABELS` property — `grep` over
`CMakeLists.txt` finds none, so a label-based exclusion would be the first.

### 6.2 The two shapes

**Behind a label.** `set_tests_properties(frontend-mutations PROPERTIES LABELS
slow)`, then `ctest -LE slow` in the default run and `ctest -L slow` in a
nightly job. Self-contained, no new CI trigger needed, and reversible by
dropping one `-L`. Cost: the label is a convention nothing enforces, and
`ctest` without `-L` still runs everything — so a developer running bare
`ctest` locally gets the slow tier anyway, which is where its feedback value
is highest.

**Into a nightly job.** A `schedule:`-triggered job running
`ctest -L mutations` or the tool directly. Removes the tier from every push and
PR run.

### 6.3 What each costs the guards

The guards `docs/FRONTEND-CONTRACT.md` §5 claims are checked *only* by this
tier. If it runs nightly instead of per-push:

- A commit that breaks an invariant is caught **up to 24 h later** rather than
  in the PR that introduced it. Nothing else in the suite can catch it:
  `frontend-invariants` (the checker alone, 0.19 s) passes on a mutation the
  checker does not model, and D1/D2 cannot see I1/I2/I5 at all — that is the
  entire reason the tier exists.
- The four D2-only mutations are caught **only** by `engine-d2-ui`, which
  stays in the default run — so those guards are unaffected by this option.
  It is the five checker-caught mutations whose build-layer verification moves.
- The tier is currently the only thing that verifies **the checker itself
  still rejects** what it claims. A nightly run preserves that; a removal does
  not.

**Evidence that would change the answer.** §0 is decisive for the *default run*
on this host: with D3-GTK at 51 s, the tier is free. If D3-GTK is addressed
separately and the suite drops near the tier's own 27 s, then the tier becomes
the critical path and this option acquires a saving it does not have today.

---

## 7. Summary — cost, what is lost, what would change the answer

| option | saving | what is lost or risked | evidence that would change the answer |
|---|---|---|---|
| Baseline (as registered) | — | — | — |
| §2 checker-only for 5 | **9.9 s (36 %)** | D1's rejection of 3 mutations stops being observed; the I1 known-gap row stops being written; I4's build-rejection stops being decided | a maintainer ruling that those D1 results are settled |
| §3 `hunmin_addon` only | **1.4–2.4 s (5–9 %)** | nothing, *provided setup still builds the test binaries* — §3.1 shows `ctest` rc=8 reads "Not Run" as "rejected" | a decision to also narrow setup, which would need the Not Run check first |
| §4 ccache | **0 s** | nothing | a mode that recompiles the same source twice |
| §4 precompiled objects | 0 s net | moves 14 s of compile into a cache the tier must keep correct | — |
| §4 PCH | **negative** (>120 s vs 1.6 s) | 126 MB per build dir | — |
| §5 parallel, best case | **~12 s (43 %)** | contention risk; ~7–16 s setup per worker that does not shrink; only wins on ≥12 idle cores | a dedicated ≥12-core runner; on 4 cores it is a loss |
| §5 warm sandbox | ~4 s | nothing | only if the checkout runs the tier twice; CI does not |
| §6 nightly / label | **0.42 s** here | up to 24 h to catch a broken §5 invariant; nothing else covers it | D3-GTK being addressed, which would make the tier the critical path |

---

## 8. Provenance

**Host.** `~/work/hunmin-mut-options`, branch `task/MUT-options` from the base
where the mutation strategy landed, Linux, 16 cores, gcc 15, cmake 4.2.3, Unix
Makefiles generator, `CMAKE_BUILD_TYPE=` (unoptimised) unless stated. Build
configured with `-DCMAKE_BUILD_TYPE=RelWithDebInfo`.

**What did not change.** `tools/frontend-mutations.py` is byte-identical to its
state on the branch base — `git diff tools/frontend-mutations.py` is empty. The
instrumentation imports the tier's module and wraps its own `_run` /
`Sandbox.__enter__`; the tier's `main()` executes unmodified and its exit status
is propagated verbatim.

**Equivalence, verified.** After adding both harnesses, both strategies were
run end to end and all nine verdicts matched their recorded expectations:

```
$ python3 tools/frontend-mutations.py --strategy sandbox   → rc=0, 9/9 correct
$ python3 tools/frontend-mutations.py --strategy in-tree   → rc=0, 9/9 correct
```

`git status` afterwards: `tools/frontend-mutations.py` unmodified,
`frontends/fcitx5/src/hunminadapter.cpp` unmodified, tree clean apart from the two new
harness files.

**The harnesses are experiments and are marked as such** in their module
docstrings. `tools/mutbench.py` (`--mode vanilla|targets|skip-checked|setup-only`)
and `tools/mutparallel.py` (`--workers N`) exist to reproduce every number
above; neither is registered in CMake, so neither runs in `ctest`.

**Caveats a later session should not have to rediscover.**

- Run-to-run spread on this shared host is real: the same 3-worker parallel
  configuration measured 22.58 s and 15.38 s on two runs. Treat single runs as
  indicative and the ranges above as the honest figure.
- The `d-addon` / `d-tui` / `d-d1` numbers in §3 keep the full setup build, so
  they measure "narrow the per-mutation build", not "never build the binaries".
  §3.1 is the separate experiment for the latter, and it fails.
- `docs/REMAINING-WORK.md`, `docs/TESTING.md` and `PLAN.md` were not edited, and
  no option above is marked adopted or rejected. `docs/REMAINING-WORK.md`'s item
  N2 describes a nightly/fuzz tier as *proposed*; §6 measures what adding this
  tier to such a job would cost and does not change N2's status.