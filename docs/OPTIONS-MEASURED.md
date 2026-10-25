# OPTIONS-MEASURED — four cost questions, measured

Four documents that were one genre: a question nobody had priced, the options
priced against each other, and the maintainer left to choose. They are one file
now — the four shared preambles and provenance blocks were the duplication, and
each section keeps its own status, because two of the four have been decided
since they were written.

Section keys are prefixed by subject (`MUT-`, `AND-`, `TST-`, `IC-`) so a
cross-document citation stays unambiguous: `docs/OPTIONS-MEASURED.md §AND-4.2`
is the android sandbox's §4.2.

| section | question | status |
|---|---|---|
| MUT | what the mutation self-tests cost, and what each way of cutting that loses | open |
| AND | what `android-mutations` costs, and what isolation buys | open |
| TST | what a test-framework migration would cost | **decided: option 1b**, shipped (#109) |
| IC | the case-encoding `ch` input | **decided: option B**, recorded (#87 close) |

Why they were written this way: `AGENTS.md` — "the purpose of an analysis is to
find the optimum design, not to judge one candidate", and a decision is the
maintainer's. Each section states what a choice costs and what it would lose, and
records the ruling once one exists.



## MUT — Mutation tier: what `frontend-mutations` costs

### MUT-0 The premise, checked first

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

### MUT-1 Where the 27 s actually goes

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

#### MUT-1.1 The per-mutation build is one translation unit

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

### MUT-2 Which mutations need a compile, and what skipping would lose

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

#### MUT-2.1 `I4-shift-chord` is the one that only a compile can decide

Its comment reads `# does not compile; reported as a build failure below`. The
mutation ORs `KeyState::Shift_L` into a `KeyStates` mask. **The build failing
IS the recorded expectation** — and it is also the checker finding, since I4
checks that `Shift` is absent from the mask textually. So this row is the
strongest argument in the file for keeping a compile: a no-build variant must
still decide it, and the only two ways to do so are a compiler invocation or a
re-implementation of the type rule in the checker.

#### MUT-2.2 Measured saving from a checker-only variant

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

### MUT-3 Narrower target sets

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

#### MUT-3.1 The trap, measured rather than asserted

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

### MUT-4 Making the per-mutation rebuild cheaper

The rebuild is 91 % compiling one translation unit, so these are the only
levers that exist.

#### MUT-4.1 ccache — measured as "cannot hit", without installing it

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

#### MUT-4.2 Precompiled objects — fast, and it only relocates the cost

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

#### MUT-4.3 Precompiled headers — measured, and worse

A PCH over the addon's own headers builds to **126 MB**, and compiling
`hunminadapter.cpp` against it **did not finish within 120 s**, against 1.587 s
without. The fcitx5 headers are template-heavy; forcing them all into one
image is a large cost paid on every compile. **Negative result, measured.**

#### MUT-4.4 Splitting the mutant translation unit — not measured, and why that is honest

The obvious remaining idea is to move the mutated regions into their own small
TU so the big one is compiled once. It was not attempted, because the mutation
anchors are statements inside `HunminAdapter::process()` and `updateUI()` — they
depend on `this`, on `ic_`, on `engine_->config()`, and are read from the member
functions themselves. Any split that made them separately compilable would
restructure the adapter along the lines the I8 thin-adapter budget measures, to
save 1.5 s in a tier that §0 shows is not on the suite's critical path. Stated
as a finding, not a rejection.

---

### MUT-5 Parallel workers — the setup cost, measured

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

#### MUT-5.1 Reusing a warm sandbox — 5 s, and only within one checkout

A sandbox kept between runs rather than rebuilt:

| | wall |
|---|---|
| cold sandbox (today) | 6.95 s setup + 18.15 s mutations = **25.1 s** |
| warm sandbox | 2.09 s restore + 18.99 s mutations = **21.1 s** |

**~4 s**, and only when the same checkout runs the tier twice — which `ctest`
does not do. A CI checkout runs it once, so this is 0 s there.

#### MUT-5.2 Seeding a sandbox from a prebuilt template — measured, does not survive relocation

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

### MUT-6 Moving the tier out of the default run

**This is the shape of the option, not a recommendation.** Measured saving on
this host: **0.42 s** (§0), because the suite's critical path is `engine-d3-gtk3`
at 51.63 s.

#### MUT-6.1 What exists today

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

#### MUT-6.2 The two shapes

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

#### MUT-6.3 What each costs the guards

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

### MUT-7 Summary — cost, what is lost, what would change the answer

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

### MUT-8 Provenance

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


## AND — Android mutation sandbox: what isolation buys

### AND-0 The premise, checked first

The task this responds to says the tier takes **43.64 s**. That number does not
reproduce in this worktree, and the difference is worth recording before any
comparison:

| `tools/android-mutations.sh`, all nine mutations | run |
|---|---|
| first run after a fresh `cmake` configure | 34.06 s |
| subsequent runs, warm Gradle daemon | 24.69 s, 25.55 s |

The gap is Gradle daemon and page-cache warmth, not the tier. Every figure
below is warm, measured in this worktree, and the tier was run **alternately**
by strategy so drift hits both arms equally.

---

### AND-1 Where the time goes

Nine planted mutations, each a `apply-mutation.py` call plus one
`tools/android-unit.sh` run. Per-mutation, warm:

| step | in-tree | sandbox |
|---|---|---|
| `android-unit.sh` (one Gradle run) | 2.9 – 3.1 s | 2.9 – 3.2 s |
| apply + restore | < 0.05 s | < 0.05 s |

`android-unit.sh` passes `--rerun-tasks`, so every run recompiles the three
mutated Kotlin files and re-executes all 44 tests. That is the whole cost, and
it is the same cost in both strategies — Gradle does not get faster because the
tree it builds in is on tmpfs.

---

### AND-2 The three sandbox designs, measured

The engine (`build/libhunmin.so`) is not in the sandbox by default:
`tools/build-host-jni.sh` links with `-L$ROOT/build -lhunmin` and bakes that
directory into the shim's RUNPATH, so the sandbox must supply an engine one way
or another.

| design | setup | 9 mutations | total | peak `/tmp` |
|---|---|---|---|---|
| in-tree (old behaviour) | — | — | **24.7 – 25.6 s** | 0 |
| sandbox, **self-built** engine (`cmake -B build` in the sandbox) | 0.05 copy + 1.19 configure + 7.96 build + 0.11 shim + 3.29 first run = **12.6 s** | 26.4 s | **~39 s** | 46 MB |
| sandbox, engine **copied** from the main build, shim rebuilt inside | 0.05 copy + 3.29 first run = **3.3 s** | 26.1 s | **~29 s** | 23 MB |
| sandbox, engine **and** shim copied from the main build | 3.3 s | 26.1 s | ~29 s | 23 MB |

**The implemented strategy is the third row.** The self-built engine is 10 s
more expensive than the tier it would protect, and buys an engine that no
mutation in this set can change.

The difference between rows three and four is one `cc` invocation (0.11 s, run
by `android-unit.sh` itself because the sandbox has no shim) and it is the
difference between a sandbox and a directory that only *looks* like one: the
copied shim carries `RUNPATH=<main checkout>/build`, so every test would load
the main checkout's engine. `tools/android-mutations.py` asserts this with
`ldd` on the first run and fails the tier if the resolved path is outside the
sandbox.

#### AND-2.1 The "copy the warm build" question

`frontends/android/app/build` is 38 MB in a checkout that has run
`assembleDebug` (9.8 MB after `testDebugUnitTest` alone). Copying it plus
`.gradle` into the sandbox, against copying neither:

| sandbox contents | copy | first run | 9 mutations | total |
|---|---|---|---|---|
| cold | 0.06 s / 32 MB | 3.22 s | 27.89 s | 31.17 s |
| warm (`app/build` + `.gradle`) | 0.06 s + 0.05 s / 33 MB | 3.44 s | 26.37 s | 29.86 s |

**Not worth it.** Copying the warm build costs 0.05 s and changes nothing a
run can measure; the 1.3 s difference is inside the run-to-run spread (27.9 s
and 26.1 s for the same configuration elsewhere in this document). The reason
is `frontends/android/gradle.properties:4`, `org.gradle.caching=true`, plus a
30 MB `~/.gradle/caches/build-cache-1`: a first run in a directory that has
never been built costs **3.22 s**, against 4.41 s for a warm one in the same
tree. The build cache already does the job a warm copy would.

That also retires the assumption behind `--rerun-tasks`: it forces the whole
graph to re-run, so the cold/warm distinction is nearly worthless here.

---

### AND-3 What isolation costs, separated from the time

Measured, same conditions, alternating, warm:

| strategy | runs | mean |
|---|---|---|
| in-tree | 26.84, 28.07, 28.35, 29.43, 29.73 | **28.5 s** |
| sandbox | 27.66, 28.24, 28.72, 29.16, 29.96 | **28.5 s** |

**Isolation is free.** The sandbox and the working tree cost the same to the
noise floor. The 3–4 s by which both exceed the old shell version (24.7 –
25.6 s) is not the sandbox — it is the **baseline run** added in §4.1, which
both strategies pay.

So the answer to "does the sandbox bring the tier below its current time while
isolating it" is: **no, it does not go faster, and it does not go slower.**
What it changes is that `android-mutations` is no longer `RUN_SERIAL`.

#### AND-3.1 Concurrency, measured

| configuration | wall | outcome |
|---|---|---|
| `android-mutations --strategy sandbox` alone | 37.77 s | pass |
| sandbox tier ‖ `engine-android-e1` (in-tree, same Gradle project) | 31.04 s | both pass |
| two sandbox tiers at once | 39.33 s | both pass |
| `ctest -j2 -R '^(android-mutations\|frontend-mutations)$'` | 33.15 s (sum 66.16 s) | both pass |

A Gradle daemon serves two projects at once, and two concurrent sandboxes cost
about 1.5 s over one — the second tier is nearly free. `RUN_SERIAL` is dropped
in `CMakeLists.txt`; the change is confined to the `android-mutations` test
properties and its comment.

Note that `engine-android-e1` keeps `RUN_SERIAL`: it builds in the shared
checkout and has no strategy. With ctest, a `RUN_SERIAL` test runs *alone*, so
the tier still cannot overlap it — `ctest -R '^(android-mutations|engine-
android-e1)$' -j2` took 31.65 s, exactly the sum. What `RUN_SERIAL` bought was
protection against *other worktrees and sessions*, not against this suite.

#### AND-3.2 The whole suite, which is what the flag was costing

Full `ctest -j16`, 57 tests, same worktree, both configurations:

| | suite total | the tier's own time |
|---|---|---|
| base, `RUN_SERIAL TRUE` | **78.84 s**, 79.69 s | 25.75 s, 26.37 s |
| sandbox, no `RUN_SERIAL` | **53.99 s**, 54.07 s, 54.68 s | 35.58 s, 45.52 s, 45.73 s |

The tier itself gets **slower** — 26 s to 45 s — because it now shares a
saturated 16-core box with `frontend-mutations` and the D3 GTK tests instead of
running alone. The suite gets **25 s faster**, about a third, because it is no
longer waiting for a test that could have been overlapping all along.

This is the whole argument for the change, and it is not visible in the tier's
own runtime. Anyone reading "the tier went from 26 s to 45 s" and stopping
there has measured the wrong thing.

(`data-selfcheck` fails on the base as well and is unrelated;
`modes-corpus-selfcheck` skips for want of a corpus.)

---

### AND-4 Two defects found while measuring, both of which made the tier green while measuring nothing

#### AND-4.1 A skip was counted as a catch

The old script ran `if android-unit.sh; then SURVIVED; else caught; fi`.
`android-unit.sh` exits **77** when there is no JDK, so on such a host:

```
$ JAVA_HOME=/nonexistent ./tools/android-mutations.sh
  caught: the caps option is never fed to the engine
  ... (nine times)
PASS: 9 caught by tier E1, 1 recorded as known gaps, all as expected
EXIT=0     elapsed 0.31 s
```

(The tenth line is `one tap starts two recognisers`, planted as
`expect_caught=False` on 2026-10-10 — a re-plant of the real defect as it shipped,
which tier E1 cannot see because `start()` needs a Context and the
suite is a plain JVM with no mocking library. See `tools/android-mutations.py`.

`CMakeLists.txt` registers `SKIP_RETURN_CODE 77`, which could never fire
because the script never returned 77. This is `docs/D3-ROOT-CAUSE.md`'s shape
one level above the one `tools/android-unit.sh` was written to close: the
skip-loud check in the unit script is undone by its caller. Both strategies
now propagate 77 and the tier skips.

#### AND-4.2 A red baseline made every mutation look caught

The first version of the rewrite had the same hole in a subtler place. Given a
`gradlew` that builds nothing and exits 0, with no report on disk:

```
android-unit.sh rc=1  "FAIL: no test results at ..."
```

which the old script reads as *caught*, nine times, and reports `PASS`. That
checkout passes a neutered build.

The fix is an **unmutated run first**, and it is load-bearing, not
ceremony. On the same doctored checkout the tier now stops before planting
anything:

```
FAIL: tier E1 is already red with no mutation planted -- every 'caught' below
would be meaningless
```

Cost: one Gradle run, 2.9 s. Without it, a red checkout is indistinguishable
from nine working mutations.

#### AND-4.3 The report is deleted before every run

`android-unit.sh` audits
`frontends/android/app/build/test-results/testDebugUnitTest` and fails when
zero tests ran or any were skipped. That audit is only worth something if the
directory holds *this* run's report. Measured, with a stale all-green XML on
disk and a `gradlew` that builds nothing:

| | result |
|---|---|
| `android-unit.sh` alone, stale report present | `PASS: 44/44` |
| `_run_unit` (wipes first), stale report present | `FAIL: no test results`, and the stale XML is gone |
| `_run_unit`, real build | `PASS: 44/44` |

So `android-unit.sh`'s skip-loud check **is** defeatable by a stale report on
its own — `--rerun-tasks` is what saves it, and only when Gradle actually runs.
Deleting the directory first makes "green because it read a stale report"
impossible: a run that produces nothing fails loudly instead.

Note the direction of the first row. With a stale report present the tier
reports *survived* and goes red, so the pre-existing tier was not exploitable
that way — the exposure was the reverse (§4.1, §4.2). The wipe closes it
anyway, because it costs a `rm -rf`.

---

### AND-5 A pattern file nothing references

`tools/android-mutations/voice-double-start` is on disk and referenced by no
tool, test, or document. The tier plants the other nine and not this one.

Measured, in a sandbox, with the mutation applied:

```
applied: True
tier E1 rc=0 -> SURVIVED    PASS: 44/44 tier E1 tests ran and passed
```

So it is a **second** mutation that no test catches — the same category as the
flush/commit swap, which `tools/android-mutations.py` names in a comment
precisely so it is not quietly omitted.

**This is recorded, not decided.** The harness's own stated principle is that
naming an uncaught mutation is better than omitting it silently, and that
principle currently has an exception nobody wrote down. Options that follow —
plant it as `EXPECTED-TO-SURVIVE`, delete the file, or document it in
`docs/FRONTEND-CONTRACT.md` next to §5.1's flush/commit entry — are the
maintainer's to pick. The measurement above is the same shape as the one
`FRONTEND-CONTRACT.md` §5.1 already records for flush/commit, so the
documentation route is the closest fit; that is a recommendation, and the
ruling is not this document's to make.

---

### AND-6 What this document does not settle

- **The 43.64 s figure.** Not reproducible here (24.7 s warm, 34.1 s cold).
  Whichever host produced it, the *relative* comparison in §2–3 was measured
  with both arms on this one.
- **The staleness of the copied engine.** The sandbox tests the main build's
  `libhunmin.so`, the same binary the in-tree tier tests, copied rather than
  referenced. No mutation in this set can change C code, so it cannot matter
  today. A future mutation that touches `hunmin-jni.c` or the core would test
  a stale engine unless the tier gains a cmake build (9.1 s). The trade is
  named, not resolved.
- **`engine-android-e1` is still `RUN_SERIAL`**, for the same structural
  reason this tier was, and nothing here changes it.


## TST — Test framework migration (decided: option 1b)

### TST-0 The premise, checked first

Each premise in the issue body was re-verified against this tree before being
relied on.

1. **"No external test-framework dependency anywhere" — verified.**
   `grep -rni -E 'gtest|gmock|googletest' CMakeLists.txt frontends/tsf/
   libhunmin/tests/ frontends/fcitx5/` → 0 hits (2026-10-19, base 5d4206d).
2. **"Engine: hand-rolled `check()`/`expect_str()`" — verified, with a line
   correction.** The pair sits at `libhunmin/tests/commit-text-test.c:23-38`
   (16 lines including the `static int failures` counter), not 22-33. Three C
   executables carry 63 assertion sites total: 28 in commit-text-test.c,
   11 in process-key-test.c, 24 in candidate-tables-test.cpp.
3. **"mock-tsf.h, ~643 lines" — corrected: 658 lines** at base 5d4206d. The
   tier-F1 C++ surface is also wider than the issue's framing (mock + trace
   runner): five files totalling about 2,390 lines, of which the mock is 658
   and the runner is 480. Three files each re-define the same `CHECK` macro
   (tsf-candidate-test.cpp:58, tsf-config-test.cpp:58, tsf-settings-test.cpp:42)
   with 139 `CHECK(` call sites between them (70 + 44 + 25);
   tsf-datadir-test.cpp asserts by exit code with no shared macro, and
   trace-emit.cpp emits traces instead of asserting.
4. **"fcitx5: same trace-emit.cpp pattern" — verified,** plus three more
   runner executables (testhunmin.cpp, testhunminui.cpp, testlanguagedetect.cpp)
   with 5 / 69 / 35 assertion sites, registered as `engine-d1-fcitx5`,
   `engine-d1-language-detect`, `engine-d2-ui`.
5. **"Android: JUnit, outside C++ scope" — verified** (9 `.kt` files under
   `frontends/android/app/src/test`). Nothing below touches it.
6. **"gmock brings no COM QI support; the QI/vtable plumbing stays
   hand-written either way" — verified by inspection.** The mock's
   `QueryInterface` implementations are hand-written per interface
   (mock-tsf.h:76, 152, 189, 252, ...); gmock's model mocks ordinary C++
   classes and has no COM `IUnknown` machinery, so under any framework option
   that plumbing remains manual.
7. **A premise the issue did not state, which the tree forces:** the engine
   public header already carries `extern "C"` guards (libhunmin/include/hunmin.h:10),
   so compiling the C engine tests under a C++ framework requires no header
   retrofit. The "C/C++ split" cost in option 1 is a build-mode decision, not
   an API change.
8. **Another unstated premise:** the tree already vendors a third-party
   dependency — `third_party/cJSON` (MIT, recorded at debian/copyright:65-68,
   "distributed unmodified"), compiled into four targets. The mechanics option
   1 needs (vendored source, per-directory licence recording) are an exercised
   practice here, not a novel pattern.

### TST-1 What "the suite" is here

`ctest -N` on this host (configured without `ANDROID_HOME`) registers 52
entries; the workspace-documented 55 includes tier E1, which registers only
when the Android SDK is found at configure time. Of those 52, most are
data- or script-driven gates (31 data-driven golden entries
(`ls tests/golden/*.jsonl | wc -l`), the schema/lint/docs gates, the android
tiers). **A C++ test framework migrates assertion code, not those entries.**
The migration surface is the hand-written assertion code:

| surface | code | assertion sites | ctest entries (this host) |
|---|---|---|---|
| engine C executables | 3 files, C | 63 | 3 |
| tsf tier-F1 runners + COM mock | 5 files, ~2,390 lines | native gtest EXPECTs + exit-code + trace emission | 4 (3 per-trace + 1 executable; the four groups merged 2026-10-19) |
| fcitx5 runners | 4 files | ~111 | 3 |

Three assertion shapes coexist: the C `check()`/`expect_str()` pair, the TSF
per-file `CHECK` macro (defined three times), and exit-code-only files. A
framework would replace the first two shapes; the third (tsf-datadir-test)
could adopt either the shared macro or a framework with no behaviour change.

The property that makes tier F1 correct is **not an assertion at all**: it is
the differential comparison `tools/trace-check.py` performs between the
runner's emitted trace and the tier-A expectations carried in each
`.trace.in` file. No framework produces this by itself; every option below
keeps `trace-check.py` and the `.trace.in` contract intact.

### TST-2 The options, measured

Figures measured on this host this session; the spike lived entirely in the
scratch directory (`$TMPDIR/googletest-1.17.0`), not in the repo.

#### Option 1 — GoogleTest + gmock, vendored

- **Dependency weight, measured:** the v1.17.0 release tarball is 885,595
  bytes; the unpacked tree — what a vendored commit actually carries — is
  4,095,045 bytes (~3.9 MiB). The issue's "order-of-magnitude
  ~1 MB" describes the tarball; the tree is ~4 MB.
- **Both toolchains compile it, measured:** llvm-mingw (Ninja) builds
  gtest + gmock + both `_main` libraries in 4.3 s, exit 0. VS 2022 x64
  (Release) configures in 7.4 s and builds in 19.9 s, exit 0, with one benign
  MSB8029 warning caused by placing the build under the system temp directory
  (the spike's scratch location), not by gtest. These are per-toolchain
  figures, not additive — a build tree pays only its own. For scale: the
  repo's full build under llvm-mingw/Ninja (engine, sim, tools, tests — 35
  steps) is 1.9 s, so the Ninja-side gtest cost is about three times the tree
  it joins; the MSVC Release figure is a dev-host shape, and the linux CI
  job's compiler is not measured here (the Ninja figure is the closest proxy).
- **Per-case ctest spawn overhead, measured:** a scratch binary of 20 trivial
  cases, registered via `gtest_discover_tests`, runs 0.595 s under `ctest`
  (20 process spawns) and 0.052 s as a single process — **~27 ms per spawned
  case** on this host. Against the 30 s inner-loop budget, spawn overhead is
  irrelevant at today's granularity (52 entries) and becomes the binding cost
  only if registration granularity explodes to about a thousand cases (for
  scale: the largest golden file alone carries 401 lines of expectations, so
  per-case registration of goldens — which nobody proposes — would spend the
  budget on its own).
  Registration shape, not framework choice, is the budget lever.
- **A host friction, measured:** `gtest_discover_tests` executes the test
  binary at build time; an llvm-mingw-linked binary without `-static` cannot
  launch outside the toolchain's PATH (its runtime DLLs do not resolve) and
  discovery fails with "Error running test executable". With `-static` (or
  the toolchain bin on PATH) discovery works. Linux CI is unaffected; on this
  host, per-case discovery needs one of the two.
- **gmock vs the COM mock:** the QI/vtable plumbing stays hand-written
  (premise 6). gmock's matchers would buy assertion introspection over the
  mock's interaction records that the current printf-style CHECKs do not
  have — the "where a mock pays" part of the request. What it would replace
  first is not the COM plumbing but the three duplicated CHECK macros and
  their 139 sites.
- **C engine tests:** compilable as C++ (premise 7). Sixty-three sites across
  three files map to TEST/EXPECT macros 1:1, or stay hand-rolled if the
  ruling scopes the framework out of C (option 2).

#### Option 1b — the same, fetched at configure time (FetchContent)

The issue disqualifies this with "FetchContent is disqualified: CI cannot
fetch dependencies while the billing failure stands, issue #102". Checked:
the billing failure is real (every CI job dies in ~3 s at billing, so a
configure that fetches cannot be verified on CI today) — but it is a
transient account state, not a property of the mechanism. Measured on this
host, the mechanism itself has two shapes:

- **URL form — fails out of the box here, measured.** cmake's bundled
  downloader rejects GitHub's certificate chain ("SSL peer certificate or
  SSH remote key was not OK ... certificate signer not trusted") where
  git-bash curl succeeds; configure dies at 2.1 s. Content integrity would
  still be pinable (`URL_HASH SHA256=65fab701...` of the v1.17.0 tarball),
  but the default path needs `CMAKE_TLS_CAINFO` pointed at a working CA
  bundle or verification disabled.
- **GIT_REPOSITORY form — works, measured.** A fresh configure including the
  clone of v1.17.0 takes 7.2 s (git's own TLS, unaffected), builds green,
  and its 20-case spike passes in 0.35 s. The same `-static` discovery
  friction as option 1 applies; the acquisition method does not change it.

What stays different from vendoring once billing clears, beyond the fetch
step: a configure without network cannot build the tree (a vendored checkout
is complete by definition); every fresh CI runner repeats the 7.2 s fetch;
and the tree's one existing third-party dependency is vendored (cJSON), so
FetchContent would introduce a second acquisition pattern whose licence
recording (fetched copy under `_deps/`) does not fit debian/copyright's
`Files:` globs — a packaging note, since gtest is test-only and ships in no
binary.

#### Option 2 — GoogleTest for the C++ frontend tiers only

Touches 10 of the migration-surface entries (7 tsf + 3 fcitx5); the 3 C
executables keep the 16-line `check()`/`expect_str()` pair. Same dependency
weight and toolchain measurements as option 1. The C tier's remaining
hand-rolled shape stays a shape (three shapes become two).

#### Option 3 — Catch2 / doctest

Same C++-only reach as option 2 with a lighter dependency (amalgamated
header). **Not measured on this host** — only gtest was compiled this
session; the figures above must not be extrapolated to them. Neither has a
CTest module of gtest's maturity (doctest ships `doctest_discover_tests`,
Catch2 has its own CMake integration), and neither offers something this
tree needs that gtest lacks; gmock's matchers have no equivalent in doctest.

#### Option 4 — keep hand-rolled everywhere

Zero new dependency, zero migration cost; the cheapest shape measured (the
single-process runner at 0.052 s for 20 cases). What it does not buy: one
assertion idiom across the three shapes, the duplicated-CHECK collapse, and
matcher-level introspection on the COM mock's interaction records.

#### What would change the answer

- **If the ruling wants one assertion idiom and the CHECK duplication gone**,
  the framework cost is ~3.9 MiB vendored and two toolchain builds that are
  already measured cheap; the deciding residual is whether the C tier
  migrates too (option 1 vs 2).
- **If the 30 s inner-loop budget is the binding constraint**, it binds on
  registration granularity, not on the framework: keep one ctest entry per
  binary under any option and the measured overhead is ~ms, not seconds.
- **If tier-F1 correctness ever needs richer interaction introspection**, that
  is gmock's matcher layer — and it layers on top of the hand-written COM
  plumbing either way; it is not a reason to switch framework candidates.
- **If billing clears and offline configure is never a scenario**, the
  ~3.9 MiB vendored tree stops buying much: the residual differences are the
  measured 7.2 s cold fetch per fresh configure and hermeticity of the
  checkout, not build cost (the framework compiles either way).
- **Nothing above is verifiable on CI today**: the GitHub Actions jobs fail
  at billing level (issue #102), so toolchain and budget verification is
  host-measured until that clears.

### TST-3 Summary

| option | new vendored tree | C engine tier | tsf tier-F1 | fcitx5 | ctest entries at risk | measured toolchain cost |
|---|---|---|---|---|---|---|
| 1 gtest+gmock vendored | ~3.9 MiB | migrates (63 sites) | migrates CHECK sites; QI stays manual | migrates | 10 | 4.3 s (Ninja) or 19.9 s (MSVC) per fresh build tree, not additive |
| 1b gtest+gmock via FetchContent | none in-tree; ~3.9 MiB under `_deps/` per configure | migrates | same as 1 | migrates | 10 | same builds + 7.2 s cold fetch per fresh configure (URL form fails TLS here) |
| 2 gtest, C++ tiers only | ~3.9 MiB | stays hand-rolled | same as 1 | migrates | 7 | same as 1 |
| 3 Catch2/doctest | lighter, not measured | C++-only reach | same reach as 2 | migrates | 7 | not measured here |
| 4 keep hand-rolled | none | unchanged | unchanged | unchanged | 0 | 0 |

The chosen row is **1b**. Under every option: `trace-check.py` and the `.trace.in` differential
contract stay; the 31 data-driven golden entries and the lint/schema gates
are untouched; the registered-entry count cannot shrink below the 3 + 4 + 3
code-driven entries listed in §1 (each hand-rolled binary maps to at least
one framework case). Amended 2026-10-19, post-ruling: the four TSF runner
entries merged into one executable at the maintainer's direction, so the
option-1 surface 13 became 10 and the MSVC-tree registered count 59 became
56; the never-shrinks gate holds against the merged shape.

### TST-4 Provenance

- Tree state: base 5d4206d, worktree
  `C:/Workspace/hunmin/ci-test-framework-migration-google-test-gmock-wh`,
  clean at start; this branch changes `docs/` only.
- `grep -rni -E 'gtest|gmock|googletest' CMakeLists.txt frontends/tsf/
  libhunmin/tests/ frontends/fcitx5/` → 0 hits.
- `wc -l` on the five tier-F1 files (2,390 total; mock-tsf.h 658).
- `ctest --test-dir build-mingw -N` → 52 entries (llvm-mingw configure,
  no `ANDROID_HOME`; 55 with tier E1 per the workspace record).
- `ls tests/golden/*.jsonl | wc -l` → 31; `wc -l tests/golden/ref-dubeolsik.jsonl`
  → 401 lines.
- Googletest v1.17.0 tarball downloaded to scratch: 885,595 bytes;
  unpacked 4,095,045 bytes. Compiled with llvm-mingw (Ninja, 4.3 s, exit 0)
  and VS 2022 x64 Release (19.9 s, exit 0) — both from the scratch tree,
  never added to this repo.
- Spawn-overhead spike: 20-case scratch binary, `gtest_discover_tests`
  registration, `time ctest` 0.595 s vs `time` direct run 0.052 s; discovery
  without `-static` reproduced the launcher failure, with `-static` it passed.
- FetchContent variants (scratch): URL form failed TLS at 2.1 s (exact error
  quoted in §2); GIT_REPOSITORY form configured fresh in 7.2 s incl. clone,
  built green with `-static`, 20 cases in 0.35 s. Tarball sha256
  65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c recorded
  for a future URL_HASH pin.
- Proportion anchor: `time cmake --build build-mingw` (fresh configure, same
  worktree, 35 build steps) → 1.9 s, exit 0.
- Read-but-not-executed (named so they are not mistaken for measurements):
  Catch2 and doctest builds on this host; gmock matcher behaviour against
  the COM mock's interaction records; anything CI-side (jobs fail at billing,
  issue #102). If a framework ever lands, building it as a repo target and
  running it in the cheap tier would turn these host-side figures into
  in-tree ones.


## IC — IC input: the case-encoding `ch` (decided: option B)

### IC-0 The maintainer's stated premise, checked first

The wording (issue #87 quotes it verbatim from the 2026-10-17 milestone C
session):

> libhunmin engine is case sensitive in current implementation. it would be
> better to receive ascii code and to transmit strings., but some env does
> not support it in my knowledge - check by yourself.

The issue body already carried a check. It was re-measured before being
relied on, and one part is wrong in a way that matters for the ruling.

1. **The engine's only key input is the case-encoded char — verified.**
   `hunmin_ic_process(hunmin_ic*, uint32_t ch)` (`libhunmin/include/hunmin.h:65`,
   implemented at `libhunmin/src/ic.c:594`) is the sole entry point. The ABI
   symbol list under `libhunmin/tests/abi/symbols.txt` carries **22** defined
   symbols, and it is the only `hunmin_ic_process*` among them. There is no
   call arriving as a virtual key with an explicit modifier state, and no
   per-call strings-in for composition (the one text-in exists —
   `hunmin_ic_commit_text` — and is a deliberate pass-through that does not
   touch the jamo pipeline, per the invariant 5 and
   `libhunmin/tests/commit-text-test.c`).

2. **"the tier-A goldens and the conformance corpus encode that convention
   event-by-event" — corrected, and the correction shrinks the regen story.**
   Only the tier-A goldens record `{"in": {"ch": "5"}}`-shaped events
   (`ls tests/golden/*.jsonl | wc -l` → 31 files; `grep -h '"in"'
   tests/golden/*.jsonl | wc -l` → 4243 event lines). The tier-C conformance
   corpora are deliberately at the **native-event level with an explicit
   modifier field**, which is the whole reason tier C exists
   (`docs/trace-spec.md:10-19`): `{"keysym": k, "mods": m}` for x11 (118
   files, 3863 events), `{"vk", "scan", "mods"}` for win32 (2 files, 52
   events), `{"keycode", "mods"}` for android (15 files, 35 events).
   The translation into the case-encoded `ch` happens *below* tier C, in the
   per-platform runners — for win32 the TSF mock injects the `mods` bit into
   `GetKeyState` (`frontends/tsf/test/trace-emit.cpp:49-57, 313-314`) so the
   shifted row is tested through the adapter's own synthesis path. So an
   appended entry point does **not** invalidate the corpora; the double regen
   implied by "changing the input shape is a golden + corpus regen" is a
   measure of the *replacement* option (§2-D) only.

3. **The fragility claim — verified, and sharpened into a duplication
   measurement.** The TSF mock harness has no `ToUnicode`-style layout
   synthesis, so `frontends/tsf/src/tsf.cpp` hand-rolls the VK → character
   mapping including the shift half (`tsf.cpp:483-530`: VK_A…VK_Z with a
   `GetKeyState(VK_SHIFT)` bit, VK_0…VK_9 against a hardcoded
   `"0123456789"` / `")!@#$%^&*("` pair, and 11 `VK_OEM_*` pairs).

4. **The premise the issue did not state: the engine already owns a copy of
   the same US-QWERTY shift map.** At *data-load* time,
   `libhunmin/src/schema_loader.c:30-35` (`shifted_of()`, built from the committed
   `BASE[]` / `SHIFTED[]` tables at `schema_loader.c:18-28`) maps each key's
   `{plain, shift}` halves onto the flat `keys[128]` table, placing the shift
   row at the shifted character's index
   (`schema_loader.c:487-493`: string "shift" → `code = shifted_of(plain_cp)`).
   So at measurement time at least three homes carried the same US-QWERTY
   knowledge:
   `schema_loader.c` (load-time), `tsf.cpp` (hand-rolled, runtime), and
   `frontends/android/…/HunminKeyRegistry.kt` (generated by
   `tools/keymap-fixture.py`, which is ctest `keymap-fixture`); the fcitx5
   adapter buys its mapping from the host XKB resolve instead
   (`docs/FRONTEND-CONTRACT.md` A1) and then *undoes* one case at the
   adapter for CapsLock (`hunminadapter.cpp:326-330`,
   `frontends/fcitx5/src/hunminadapter.cpp`).
   **Correction 2026-10-19 (issue #106):** the `tsf.cpp` runtime home is
   gone — the adapter passes the unshifted base + the Shift bit to
   `hunmin_ic_process_key` (`hunmin.h:84`) and the loader's map selects the
   row. Two homes remain (`schema_loader.c`, the generated `HunminKeyRegistry.kt`),
   both fenced. The same correction fixes this section's own stale citation:
   the mapping this document measured as "`tsf.cpp:483-530`" lived at
   `tsf.cpp:1228-1281` on master `198ea15` — the line range quoted here had
   drifted from the tree before the migration deleted it.

### IC-1 What "case sensitive" actually carries (measured)

The layout JSON is authored **per physical key, with both rows explicit**
— no uppercase-row trick in the data:

```
libhunmin/data/layouts/sin3-p2.json
  "q": {"plain": …, "shift": …}     -- and there is no "Q" entry at all:
  "1": {"plain": …, "shift": …}     -- the keys object names base characters only
  (dubeolsik.json is the same shape: "q" → {"shift": "ㅃ", "plain": "ㅂ"})
```

`python3 -c …` over the shipped layouts: sin3-p2 has 47 key entries and
dubeolsik 26, zero of them uppercase keys. **Corrected 2026-10-18 (issue
#105 migration): an earlier revision of this snippet showed `"Q" => null`
placeholders and a dubeolsik "q" row of ㄲ/ㄷ, and neither matches the
shipped files** — no layout's `keys` object names an uppercase or
shifted-punctuation character at all (dubeolsik's q is ㅂ plain / ㅃ
shifted; the uppercase `"H"`/`"W"` strings in the two sin3-p2 files are
`flags` overrides, the 전처리수식 set_option keys, not keys entries). The
uppercase indexes of the runtime `keys[128]` table are purely the loader's
materialisation — the loader puts the shift row at the shifted character's
index — so `docs/FRONTEND-CONTRACT.md` A2's "layouts encode shift as
uppercase rows" describes that materialisation, not the authoring
convention. Saying "uppercase rows" as if the data had them is how a reader
ends up concluding a data-model change is required for an explicit-modifier
input; it is not (§2-A/B).

At runtime, the engine only reaches the layout through
`ic.c:624-627`:

```c
if (ch < 128) {
  hm_state st = {.D = ic->b.cho, …, .P = ic->caps, …};
  rv = hm_resolvev(&ic->L->keys[ch], st);
}
```

and the state the resolver sees carries **no case information whatsoever**:
`P` is the CapsLock *option* (`hunmin_ic_set_option(ic, "caps", …)`), `J` is
`supports_case && jung && jvar`. The case of `ch` is consumed entirely by
*table addressing* — the loader pre-split `{plain, shift}` onto
`keys[base]` / `keys[shifted_of(base)]`, so the case bit of the arriving
character is a **transport selector between two rows the data file already
names explicitly**. *(Verified by reading: `hunmin_internal.h:42-46` and
`ic.c:624-627`; `shifted_of` is injective over a–z and the 21 punctuation
base keys by inspection of `schema_loader.c:18-35` — its input space is exactly the
BASE keyspace of the layouts, and each shifted form is distinct there.)*

That is what makes the two conventions informationally equivalent, and it is
measured in the sense that the equivalence is a property of the shipped
tables rather than of any argument about keyboard layouts.

### IC-2 The options

Each states what it costs, what it gains, and what would change the answer.
Outward-facing costs are measured; opportunity costs are stated as such.

#### A. Keep the case-encoding contract (status quo)

**Cost: nothing.** What it leaves as measured residue:

- **The US-QWERTY knowledge in at least three homes.** `schema_loader.c` at load
  time; `tsf.cpp:483-530` at runtime (line range as measured then; see the
  2026-10-19 correction in §0-4 — the range was stale and #106 has since
  removed this home);
  `HunminKeyRegistry.kt` (generated, so
  its divergence risk is the generator drift the registry itself documents);
  the fcitx5 adapter partially from the host. They are fenced —
  `tools/keymap-fixture.py` asserts every key a layout can use has a native
  encoding on each platform, and win32 tier F1 replays the real adapter —
  but being fenced is what makes each new copy *additionally* correct, not
  *single-sourced*.
- **The case decision lives in the adapters.** Three of them each decide
  CapsLock-and-shift interaction on their own, and the Android one is where
  a CapsLock×Shift interaction already shipped wrong in an earlier revision
  (`FRONTEND-CONTRACT.md` A3': the revision ignored Shift under CapsLock).
  A second frontend of that shape would inherit the same decision to make
  rather than the made decision.
- **The engine-level composition has only one home today** — tier A
  exercises it, but only after the adapter's synthesis; the adapters'
  correctness beyond that is checked per platform by the corpora, which the
  tree already has.

**What would change the answer:** a second layout-less host adapter
appearing (e.g. another harness like the TSF mock), or a third copy of the
table needing maintenance without a generator.

#### B. Append an explicit-modifier entry point (dual convention, append-only)

The shape whose cost is measured here: `hunmin_ic_process_key(ic,
uint32_t base, uint32_t shift)` (exact signature is a spec detail; the
measured surface is the shape).

**Engine cost, measured against the current code:**

- The implementation is a small public wrapper: `ch = shift ?
  shifted_of(base) : base`, then the existing `hunmin_ic_process` body. The
  wrapper reuses the table the loader already uses — **no new US table, no
  data change** (§1), and no change to any expression/state machine.
- The reversed-contract edge case has to be specified, not invented: a
  `shift=1` on a base char with no shifted form, and a `base` character that
  *is already the shifted form* (e.g. a frontend whose host resolves the
  shifted character before the adapter sees the key — the fcitx5 case). The
  cheap faithful rule is "the char is treated as already
  resolved whenever `shifted_of(char)` is zero and the char is not a
  lowercase/base form", but the wording belongs in the ruling, in
  `docs/SPEC-P2.md` (engine interface) and `docs/FRONTEND-CONTRACT.md`.
- ABI: +1 symbol onto the 22 (append-only act, `nm` command in AGENTS.md;
  the `abi` test prints the command when it fails).
- Tier A: an equality test — for every shipped layout and every key row, the
  new call must commit/preedit *identically* to the legacy call — same
  commit, same preedit — which also pins the entry edge above. The
  precedent exists as `libhunmin/tests/commit-text-test.c`
  (ctest `engine-commit-text`); this shape is comparable in size.
- Tools: `tools/sim.c` needs a modifier-carrying key syntax so tier A can
  reach the new entry, and `tools/gen-conformance.py` keeps emitting the
  legacy shape unless a dialect wants both.

**Frontend cost per platform, if each migrates (both calls stay valid in the
dual period, so none of this is forced):**

| frontend | today | under B |
|---|---|---|
| fcitx5 (`hunminadapter.cpp`) | host-resolved keysym + adapter CapsLock undo | unchanged; keysym arrives pre-resolved, its case *is* the row selector. B offers it nothing to delete. |
| Android (`HunminKeyRegistry.kt` + adapter) | generated `(unshifted, shifted)` pairs; adapter computes the case under CapsLock×Shift (`engineChar`, the site of the earlier wrong case). | table keeps generating; the adapter passes `unshifted` + a shift bit and the `caps` option, deleting the CapsLock×Shift case matrix. The table stays either way — it is generated from the fixture. |
| Windows TSF (`tsf.cpp`) | hand-rolled VK→char incl. shifted forms | the switch keeps only the VK→base half; the shift column and the `GetKeyState(VK_SHIFT)` branching leave the adapter. **Shipped 2026-10-19 (issue #106).** |

**What B actually buys, stated exactly:** not "the frontend stops
synthesizing" (the VK→base table stays; only the case half and its
CapsLock×shift interactions migrate), but "the case endpoint (the thing
three adapters get to re-decide, one of which was wrong once) moves into the
engine — a single home, engine-tested without any host layout."

**Corpora effect, measured:** none. Tier C is at `(key, mods)` level already
(§0-2); both runners reach the engine through the legacy call unless a
runner *chooses* to migrate, which changes no recorded expectation.

#### C. Strings in ("receive ascii code and to transmit strings" — the strings half)

Measured rather than assumed, because the tree already has all the pieces:

- **Out is strings already**: `hunmin_ic_commit` /
  `hunmin_ic_preedit` / `hunmin_ic_flush` return UTF-8; the external-text
  pass-through `hunmin_ic_commit_text` accepts strings in, deliberately
  outside the jamo pipeline.
- **In-side strings exist only as tooling**: `tools/sim.c --keys/--batch`
  feeds a whole character string through `hunmin_ic_process` per char
  (`sim.c:320`) — and `tools/sin3p2-ksgen.py` used exactly that (BFS, with
  `hunmin-sim --batch` as oracle) to generate the win32/word corpus keys.
- **An ABI strings-in for composition cannot replace the event stream**
  without losing information: a string carries no modifiers and can express
  backspace only by re-embedding the `0x08` code — i.e. it would re-encode
  the same convention *inside* a string rather than removing it. Measured at
  the code level: `hunmin_ic_process` distinguishes exactly those two things
  today (`ic.c:599`, `ic.c:624`).

So the strings half of the maintainer's sentence is already true for output
and true-but-tooling-only for input; the input half of the ask reduces to
the case-encoding question, which is §2-A/B.

#### D. Full replacement of the convention (no dual period)

Everything B does, plus the tree's own history: the append-only rule exists
*for this*, and a two-call period is a pattern the tree already used once
(`hunmin_ic_commit_text` was appended beside `hunmin_ic_process`, never
replacing it). The replacement's extra cost is the regen the issue's check
named:

- tier-A goldens: 31 files, 4243 event lines under the `{"in": {"ch": …}}`
  shape; regen tooling is `tools/regen-goldens.sh`
- tier C: 118 + 2 + 15 trace files per platform, expectations recomputed by
  `hunmin-sim --trace` (`tools/gen-conformance.py`,
  `tools/trace-check.py --emit`).
- The mechanical cost is not the deciding one: `docs/trace-spec.md`'s
  gate is "Review diffs; never blind-accept" — the review burden is the
  cost an ABI append avoids entirely.

**What would change the answer:** an ABI-size pressure that makes the
22-symbol surface a problem — nothing measured today suggests it.

### IC-3 Summary

| option | ABI | data | tier-A goldens | tier-C corpora | engine LOC | adapters |
|---|---|---|---|---|---|---|
| A status quo | 0 | 0 | 0 | 0 | 0 | keep the case decision in three homes; one of them shipped wrong before |
| B append entry | +1 symbol | 0 | 0 | 0 | small wrapper over `shifted_of`; one equality test; one sim syntax | case half migrates where the adapter's speaker can be freed (Android, TSF); fcitx5 unchanged |
| C strings-in | n/a — output already strings; input already exists as pass-through + tooling | | | | | |
| D replacement | 0 net (same +1) | 0 | full regen, reviewed by hand | full regen, reviewed by hand | same as B | same as B |

C is listed for completeness of the maintainer's sentence, not as a live
candidate: as measured in §2-C the strings-in it would add already exists in
the two forms that can exist for it (pass-through text, and tooling
keystreams).

### IC-4 Provenance

- Host: Windows 11, reading pass over the tree at base `0b9e0e7`
  (branch `scobyseo/engine-question-replace-the-case-encoding-ch-inp`,
  worktree `C:\Workspace\hunmin\engine-question-replace-...`). The
  measurements are file reads, Python over the shipped JSON, and `grep`,
  not executables; nothing depended on a build.
- Changed files: `docs/IC-INPUT-OPTIONS.md` only — that was this section's own
  file until 2026-10-25, when the four options papers became this document. The
  parallel
  work on issue #86 (portable mingw suite, worktree
  `ci-portable-mingw-suite-on-the-windows-host-andr`) is a separate session;
  this branch deliberately leaves `docs/REMAINING-WORK.md`, `AGENTS.md` and
  every engine/frontend file untouched so the two sessions cannot collide,
  and no option above has been implemented here.
- Read-but-not-executed items, labelled: the injectivity of `shifted_of`
  and the case-is-transport-only-claim in §1 are verified from the code
  paths named there (`schema_loader.c`, `hunmin_internal.h`, `ic.c`), not by a
  binary run; if either turns out false the equivalence in §2-B's wrapper
  changes, which the proposed tier-A equality test would then measure as
  red before any frontend moved.
- Issue #87 stays open and decision-gated: closing it records a ruling
  only the maintainer can make.
