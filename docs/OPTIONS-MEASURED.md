# OPTIONS-MEASURED — twelve cost questions, measured

Four documents that were one genre: a question nobody had priced, the options
priced against each other, and the maintainer left to choose. They are one file
now — the four shared preambles and provenance blocks were the duplication, and
each section keeps its own status, because two of the four have been decided
since they were written. §URM arrived later, as a section rather than a fifth
document, because an issue that asks a question and leaves it open belongs in
the file that already holds the open ones; §HEK, §E5 and §EDW followed the same way.

Section keys are prefixed by subject (`MUT-`, `AND-`, `TST-`, `IC-`, `URM-`, `HEK-`,
`E5-`, `EDW-`, `B1-`, `PAS-`, `MRO-`, `BND-`) so a cross-document citation stays unambiguous: `docs/OPTIONS-MEASURED.md §AND-4.2`
is the android sandbox's §4.2. `HEK` and `E5` are the two halves of issue #159 and are
deliberately separate, because they answer different questions: **§HEK** measures whether
each environment can deliver the gesture at all (key shapes), and **§E5** prices what the
gesture *means* (the assignment). §HEK-4's shapes and §E5-3's assignments are not
alternatives to each other. §EDW (issue #160) is a third subject and cites itself.
§B1 (issue #161) asks a different kind of question again: not which option to pick but
whether the block the other items sit behind still exists, so its options are answers to
§PAS (issue #162) is arithmetic rather than a probe: the
two numbers it prices are gradeable only once #181's harness picks a corpus, so it
prices the units instead. §MRO (issue #164) is the first section whose subject
already exists twice in the tree — it asks which home the rule should keep, not
whether one is missing, and its ruling is the one that also fixed R9's threshold.
§BND (issue #163) prices three parameters of one function — the buffer cap, the
short-word threshold and the flicker — and finds that two of the three premises the
issue states are not what the tree does. §BND-7 (issue #171) measures a fourth
thing on the same object: whether R6's digit run is actually held in the layer's
word.

| section | question | status |
|---|---|---|
| MUT | what the mutation self-tests cost, and what each way of cutting that loses | open |
| AND | what `android-mutations` costs, and what isolation buys | open |
| TST | what a test-framework migration would cost | **decided: option 1b**, shipped (#109) |
| IC | the case-encoding `ch` input | **decided: option B**, recorded (#87 close) |
| URM | whether `DllUnregisterServer` mirrors the Start-menu shortcut and `RemoveLanguageProfile` | **Q1 decided: option C**, shipped (#146); Q2 measured 2026-10-30 — the residue is the per-user `Enable` key, one ruling away |
| HEK | whether the Han/Eng key can act as a held modifier in each frontend environment, and what the compound press would cost | measured per environment (Windows live, Android emulator, fcitx5 read-only); the **compound's action is decided** 2026-10-31 — `docs/SPEC-MODES.md` §E5, §E5 status below | 
| E5 | the 한/영 key: what each single-press / compound-press assignment costs, and its premises | **assignment decided 2026-10-31**: single = manual switch, modifier + 한/영 = special-function namespace, default, remappable in the config dialog, all three frontends; per-function modifier still open (#159) |
| EDW | which English dictionary R8-정정's `english_valid` looks up, under what licence, and whether it ships separately | **decided 2026-11-01: hunspell `en_US` + a user dictionary, shipped in-tree for now** (#160); four candidate dictionaries and the corpus-list baseline were priced on one probe. The curated supplement is deferred; the separate-distribution split is #190 |
| B1 | whether the board's B1 ("the hangul judgement source") is still a blocker at all now that `hangul_valid` is a structural test | **decided 2026-11-01: option A — B1 retired** (#161); the Hangul half is answered by R8-정정, the English half is decided and shipped by R8-정정-2 |
| PAS | the pass lines for the two scored metrics (keystroke saving, manual toggles) — in which unit each can be graded at all | open (#162); the arithmetic is measured, no number is chosen. Both validity halves and the autoswitch state shipped 2026-11-01, but the judgement the numbers grade is still #168 and the harness that would run them is still #181 — so the units are priced and the corpus that fixes them is named as the missing input, not the number |
| MRO | who owns the modes judgement — the shared layer, which already holds the buffer, both layouts and the mode, or each adapter | **decided 2026-11-01: option C** (#164) — the engine judges, the frontend performs the switch; the same ruling sets R9's threshold at three **key inputs** (unit clarified 2026-11-01, §R9). The material is measured in three places and the formula in none; the fcitx5 act still needs an input method to switch to |
| BND | the three parameters of the judgement boundary: where the buffer must end (E2), what counts as a short word (E3), and whether judgement ever moves inside the word (E6) | **E2 decided 2026-11-01 (issue #163): option B at 256 B, configurable** — implemented, the preedit bounded and lossless at every length; **E3's value came from §MRO's ruling** — three key inputs (2026-11-01, unit clarified the same day) — and this section measures what that N partitions; **E6 decided 2026-11-01: both timings, per-key by default** — `judge_timing` is an option, 64.05% of dictionary forms flick back inside the word under the default (94.18% without R9's guard), and the delivered text is identical in both timings. **E1 was ruled the same day** while this section's mixed-sentence measurement was on the table: the symmetric line ships, and that sentence is the 0.3 gate |


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


## URM — Unregister mirror: the Start-menu shortcut and `RemoveLanguageProfile`

**Decided: option C** (the installer owns the Start-menu entry), ruled
2026-10-30; the implementation and its verification are in §URM-7, and the
elevated round that closes question 2 is recorded in §URM-4 (choosing E survives;
the residue is a per-user key nobody in the flow removes). The measurements that
produced both are unchanged below.
Issue #146 asks two questions about whether `DllUnregisterServer` is a complete
mirror of `DllRegisterServer`, and it named, for the first question, the single
fact that would change the answer: "whether the Inno Setup uninstaller deletes
the Start-menu entry". That fact is measured in §URM-1, and it did not read the
way the issue presumed — the uninstaller does delete a Start-menu entry, but a
different one, in a different folder, written by a different program.

### URM-0 The premise, checked first

The issue's own sentences, each re-measured against the tree and the host.

1. **"Registration writes `<Programs>\Hunmin IME 설정.lnk`"** — *verified as a
   path expression, corrected as a location.* The code asks for
   `FOLDERID_Programs` (`frontends/tsf/src/registration.cpp`, `SeamProgramsFolder`).
   `FOLDERID_Programs` is the **per-user** Start-menu folder
   (`%APPDATA%\Microsoft\Windows\Start Menu\Programs`); the all-users folder is
   `FOLDERID_CommonPrograms` (`%ProgramData%\…\Programs`). The installer writes
   to the all-users one. Both files exist on this host — see §URM-1.
2. **"pointing at the settings exe when it is deployed beside the DLL, else
   `rundll32 <dll>,HunminSettingsShow`"** — *verified, and measured live*: the
   installed shortcut's target is
   `C:\Program Files\Hunmin IME\settings-ui\hunmin-tsf-settingsui.exe`.
3. **"`DllUnregisterServer` deletes only two things"** — *verified*. The
   unregister tail is eight steps, no file operation among them:
   `UnregisterProfile`, `Unregister`, four `UnregisterCategory`, then
   `RegDeleteTreeW` on `HKLM\SOFTWARE\Microsoft\CTF\TIP\{CLSID}` and on
   `HKCR\CLSID\{CLSID}` — asserted verbatim by
   `TsfRegistration.UnregisterMirrorsEveryRegisteredArtifact`.
4. **"an unregister-then-move-the-checkout leaves a dead Start-menu entry that
   resolves to nothing"** — *corrected, narrower.* That is the checkout case
   only: with no `settings-ui` beside the DLL, the link is the `rundll32` form
   and its argument is the DLL's absolute path, so moving the checkout kills it.
   On an installed product the link carries the settings-exe path and survives a
   checkout move untouched; it dies at **uninstall** instead, when `{app}` goes.
   Both end dead; the trigger differs, and the second one is the shipped case.
5. **"the installer's uninstall path may already handle it (not measured
   here)"** — *measured, and the answer is "a different artefact"*. Inno's own
   uninstall log (`C:\Program Files\Hunmin IME\unins000.dat`, read as the list
   the uninstaller replays) records the all-users group directory
   `C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Hunmin IME` and the
   `.lnk` inside it. The per-user path is **not** in that log: decoding the log
   and searching for `AppData` returns nothing. So the uninstaller removes its
   own entry and leaves the DLL's.
6. **"nothing calls `RemoveLanguageProfile`"** — *verified*: the issue's own
   command, `grep -c "RemoveLanguageProfile" frontends/tsf/src/registration.cpp`,
   returns 0.
7. **"Unregistering relies on `UnregisterProfile` + `Unregister` + the HKLM tree
   deletion"** — *verified, with one omission*: the HKCR CLSID tree is deleted
   too, and so are the four categories. The issue's list is the profile half.
8. **Premise the issue does not state, which binds both questions:**
   `docs/WINDOWS-TSF-REGISTRATION.md`'s "the HKCU SortOrder/Assemblies store is
   untouched: registration never writes it" is true for `SortOrder` and
   `Assemblies`, and **not** true of HKCU as a whole. `EnableLanguageProfile` is
   documented as enabling a profile "for the current user", and the `Enable`
   value is under **HKCU**, not HKLM — measured in §URM-2, and corrected in that
   document in this branch. So registration already writes one per-user key, and
   the mirror's HKLM tree deletion cannot reach it; whether `UnregisterProfile`
   removes it is the unmeasured half of question 2.

### URM-1 The two Start-menu writers, and the folder mismatch

| writer | code | folder | resolved path on this host | who deletes it |
|---|---|---|---|---|
| `DllRegisterServer` | `CreateStartMenuShortcut` (`registration.cpp`) | `FOLDERID_Programs` — **per-user** | `C:\Users\pc\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Hunmin IME 설정.lnk` (2198 bytes) | nothing, today — and since the 2026-10-30 ruling the DLL writes no `.lnk` at all (§URM-7) |
| the installer | `[Icons]` in `frontends/tsf/installer/hunmin.iss` | `{group}` = `{commonprograms}\{DefaultGroupName}` — **all users** | `C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Hunmin IME\Hunmin IME 설정.lnk` (1109 bytes) | Inno's uninstaller — that path is in `unins000.dat`; the per-user one is not |

Both files were written in the same minute by the same install
(2026-10-29 22:11) and both target the same executable, so the one visible
effect on a machine with the product installed is **two Start-menu entries with
the same name** — one at the top level of Programs, one inside a `Hunmin IME`
folder. Measured by listing both folders; this is the state the maintainer's
own desktop is in now.

Three consequences — the first two measured, the third read from the code:

- **The uninstaller and the DLL are not two writers of one artefact** — they are
  two writers of two artefacts in two hierarchies. That is what settles the
  issue's deciding question: deleting the DLL's `.lnk` in `DllUnregisterServer`
  would not fight the uninstaller, because the uninstaller never touches that
  path.
- **Per-machine registration, per-user artifact.** Registration is elevated and
  writes HKLM/HKCR, but the shortcut lands in the profile of whoever ran
  `regsvr32`. A machine-wide install therefore gives the Start-menu entry to the
  installing account only; another account on the same machine gets none.
- **The DLL's shortcut is the only artefact in the flow whose read-side is a
  filesystem path a user may rewrite.** Renaming it is not detectable — the code
  recomputes the name and would write a fresh one, leaving the renamed one.

### URM-2 The registry surface question 2 sits on

Measured on this host with the 0.2.0 package registered
(`HKCR\CLSID\{…}\InprocServer32` → `C:\Program Files\Hunmin IME\hunmin-tsf.dll`).
The HKLM TIP tree holds 20 keys: the default `Hunmin`, `Enable = 1`, the four
category pairs under `Category\Category` and `Category\Item`, and

```
HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-…-0001}\LanguageProfile\0x00000412
    IconIndex = 0x0
    …\{F2C70002-…-0002}   Description = Hunmin
                          IconFile    = C:\Program Files\Hunmin IME\hunmin-tsf.dll
                          IconIndex   = 0x0
```

The per-user halves, which the unregister flow does not read at all:

```
HKCU\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-…-0001}\LanguageProfile\0x00000412\{F2C70002-…-0002}
    Enable = 0x1
HKCU\Software\Microsoft\CTF\SortOrder\AssemblyItem\0x00000412\{34745C63-…}\00000001
    CLSID = {F2C70001-…-0001}   Profile = {F2C70002-…-0002}   KeyboardLayout = 0
```

The calls involved, by their own documentation, one line each:

- `ITfInputProcessorProfiles::AddLanguageProfile` — "Creates a language profile
  that consists of a specific text service and a specific language identifier."
- `ITfInputProcessorProfiles::RemoveLanguageProfile` — "Removes a language
  profile." Its counter-call is `AddLanguageProfile`.
- `ITfInputProcessorProfiles::Unregister` — "Removes a text service from TSF."
  Its counter-call is `Register`.
- `ITfInputProcessorProfileMgr::UnregisterProfile` — "unregisters the text
  service and the profile", and its counter-call is the profile registration
  path this flow uses.

What the tree already settles, without an elevated round: **the HKLM language
profile cannot outlive the unregister flow**, because
`RegDeleteTreeW(HKLM\…\TIP\{CLSID})` deletes the whole subtree including
`LanguageProfile`. `RemoveLanguageProfile` therefore cannot add anything to the
HKLM side. What it could add — and what `Unregister`/`UnregisterProfile` may or
may not remove — is the **per-user** `Enable` value and the SortOrder slot.
Attribution needs the per-call evidence, not another mock: the log line each
call already writes (`hunmin-reg-debug.log`) plus a before/after snapshot of the
HKCU keys, in the elevated round
`tests/manual-checklists/tsf-unregister-cleanup.md` describes and #84 still owes.
That checklist was extended in this branch to snapshot the two keys above and
both Start-menu paths, because without them the round cannot answer either
question.

### URM-3 Question 1 — the surviving shortcut

**A — delete it in unregister (mirror the write).**
Cost: the seam gains a file operation. `HunminRegSeam` has seven entries and no
file delete; `IPersistFile` has no delete either, so the call is `DeleteFileW`
and tier F1 needs a double for it, plus a delete counter beside
`trace.shortcutSaves`. The path must be recomputed exactly as the write does
(`programs_folder` + the same name) so a hand-renamed link is left alone — a
rename is not detectable and would then be preserved, which is the honest
outcome. Gain: the one artefact registration creates without a mirror gets one;
`regsvr32 /u` on a checkout stops leaving a dead entry behind. Loss: an
unregister that runs on a never-registered machine now attempts a delete (it
must tolerate the absent file). What would change the answer: a decision that
the DLL should not write a shortcut at all (option C), which makes A moot.

**B — leave it, and let the documentation carry the asymmetry.**
Cost: zero code; the sentence already exists
(`docs/WINDOWS-TSF-REGISTRATION.md` §"The seam tier F1 drives"). Gain: nothing
but the record. Loss: the dead entry survives uninstall in its checkout form, and
the duplicate name survives while installed. What would change the answer:
nothing measured — B is the status quo restated, so it is the option the other
three are measured against.

**C — stop writing it from the DLL; let the installer own the Start-menu
entry.**
Cost: `CreateStartMenuShortcut` and the `rundll32` fallback leave
`DllRegisterServer`; the F1 tests that assert the save
(`TsfRegistration.TheStartMenuShortcutIsNeverRemoved`,
`…ARefusedShortcutSaveStillRegisters`) lose their subject and would be deleted;
`docs/WINDOWS-TSF-REGISTRATION.md`'s flow loses a step. Gain: one writer, one
artefact, in the hierarchy that matches per-machine registration. Loss: a
checkout-registered DLL (no installer) stops being discoverable, which is
exactly what the `rundll32` fallback was added for. What would change the
answer: evidence that the checkout workflow does not need the entry — this
branch did not measure a developer actually using it.

**D — keep writing it, but to `FOLDERID_CommonPrograms`, and delete in
unregister.**
Cost: A's seam work plus one constant, and the `{group}` name collision with
the installer's folder becomes visible in one place (either coincide into one
entry — impossible, the paths differ by a directory level — or stay two entries
until the installer's `[Icons]` entry is dropped). Gain: the entry matches the
per-machine registration and every account gets it; the duplicate at the top
level disappears if the installer's entry is dropped in the same change. Loss:
an all-users write needs elevation, which `DllRegisterServer` already has, but a
non-elevated `regsvr32` (the per-user dev registration path) would then fail to
write the shortcut at all instead of writing it to a folder it can reach. What
would change the answer: whether a non-elevated registration is a supported
path — measured today only in `frontends/tsf/register-tsf.bat`, which does not
elevate itself.

**Ruling, 2026-10-30 — "C"** (the maintainer's answer to the four options as
presented above; quoted verbatim, and the reason A was measured first survives
in §URM-1: the uninstaller never touches the DLL's path, so A would not have
been a second writer — C was chosen for the single-writer shape, not because A
was impossible). The implementation is §URM-7. A, B and D are not taken; D
remains the shape that would matter if a non-elevated registration ever needed a
Start-menu entry the installer cannot see.

### URM-4 Question 2 — `RemoveLanguageProfile`

**RULED 2026-10-31: option A.** The maintainer chose to delete the per-user
tree in `DllUnregisterServer` — implemented, tier F1 updated (three tree
deletions, HKCU last, and no deletion may name `SortOrder`), and measured
elevated: `LanguageProfile` exits 1 after `regsvr32 /u`, `SortOrder` and
`Assemblies` unchanged, the absent-state re-run still exits 0. The empty
`HKCU\…\CTF\TIP\{CLSID}` container stays: msctf creates it, not this
registration. The measurement text below is kept as it was written.

**E — status quo (not taken).** `UnregisterProfile` + `Unregister` + five category
unregisters + the two tree deletions. The HKLM side is provably covered by the
tree deletion (§URM-2). The per-user `Enable` and SortOrder slot are covered
only if msctf's own unregister calls do it — measured 2026-10-30: they do not
(§"Unregister state" in `docs/WINDOWS-TSF-REGISTRATION.md`), and the residue
reproduced on a clean Windows Sandbox image 2026-10-31
(`tests/manual-checklists/tsf-installer-fresh-vm.md` row 9b). Cost: 0. What
would change the answer: an msctf call that turns out to remove the per-user
value — the elevated round found none, and F does not remove it either.

**F — add `RemoveLanguageProfile` before `UnregisterProfile`.** Cost: one COM
call and one mock expectation; the unregister tail's asserted order changes from
eight steps to nine. Gain: the register side's `AddLanguageProfile` gets its
documented inverse, which is the *shape* the issue is asking about. Loss: the
checklist's own rule — "the mirror is exact, not more: do not add calls that
remove state registration never wrote" — and the risk of a call that removes a
profile `UnregisterProfile` already removed, which must then tolerate a failure.
What would change the answer: the round showing the `Enable` value or a profile
entry surviving E.

**G — decide only after the elevated round.** Cost: it is environment-bound and
owed from #84, so the lane is idle until an elevated session runs
`tests/manual-checklists/tsf-unregister-cleanup.md`. Gain: the answer becomes a
measurement of msctf rather than a reading of its documentation. What would
change the answer: nothing — the round is the discriminator, and the checklist
now names the per-user keys that make it decisive.

**Measured 2026-10-30, elevated — question 2 is answered, and not by
`RemoveLanguageProfile`.** The full rundown is
`docs/WINDOWS-TSF-REGISTRATION.md` §"Unregister state"; one line per finding:

- **E is sufficient on the HKLM side.** `Unregister` alone deletes the whole
  `CTF\TIP\{CLSID}` tree including `LanguageProfile`, and `UnregisterProfile`
  removes the profile's values. In a healthy cycle the four `UnregisterCategory`
  calls and the HKLM `RegDeleteTree` return `E_FAIL` / `0x2` — harmless
  redundancy kept for the partially-removed state, which the two-run edge
  exercises (`UnregisterProfile` → `E_FAIL` on an absent profile, `Unregister` →
  `S_OK`, `regsvr32 /u` exits 0).
- **F buys a shape, not a removal.** `RemoveLanguageProfile` returns `S_OK`
  against a present profile and removes nothing `UnregisterProfile` left behind;
  it returns `E_FAIL` when the profile is absent. It never touched the one
  artefact that does survive.
- **The residue is per-user.** `HKCU\SOFTWARE\Microsoft\CTF\TIP\{CLSID}\LanguageProfile\0x0412\{profile}\Enable = 0x1`
  is still present after `UnregisterProfile` + `Unregister` + `regsvr32 /u`.
  Registration wrote it (`EnableLanguageProfile`), so a literal mirror would
  delete it — nothing in msctf does, and no option here did. Deleting it means a
  `RegDeleteTree` on that HKCU subtree in `DllUnregisterServer`, which crosses
  the line the #84 boundary drew in the other direction ("registration never
  writes HKCU") — that premise is corrected in the registration document, and
  the choice is the maintainer's.

### URM-5 Summary

| option | `registration.cpp` | seam + tier F1 | `hunmin.iss` | docs | user-visible |
|---|---|---|---|---|---|
| A delete the DLL's `.lnk` | + a delete step in the unregister tail | +1 seam entry (file delete), +1 trace counter, 1 new assertion | — | asymmetry paragraph becomes "mirrored" | dead entry gone after `regsvr32 /u`; duplicate name while installed stays |
| B leave it | 0 | 0 | — | already recorded | dead entry after uninstall (installed case); duplicate name |
| **C installer owns it — DECIDED 2026-10-30** | − the shortcut step and the rundll32 fallback | − the Programs-folder seam entry, − the shell-link double, − two tests, + `RegistrationWritesNoFile` | — (the entry is unchanged; it is now the only one) | flow loses a step, the asymmetry becomes the ruling | one entry, one writer; checkout registration undiscoverable |
| D write to all-users + delete | A's delete + one constant | A's seam work | `[Icons]` entry becomes redundant if the names are unified | both documents change | every account gets the entry; one entry if the installer's goes |
| E status quo (Q2) | 0 | 0 | — | — | residue iff msctf leaves any |
| F add `RemoveLanguageProfile` | +1 call | unregister tail 8 → 9 steps | — | mirror list grows | none observable unless E leaves residue |
| G wait for the round | 0 until measured | 0 | — | checklist extended here | — |

### URM-6 Provenance

- Host: Windows 11, worktree
  `C:\Workspace\hunmin\tsf-is-the-unregister-mirror-complete-the-start`, branch
  `scobyseo/tsf-is-the-unregister-mirror-complete-the-start`, base `950490e`.
  The host carries the 0.2.0 package installed at `C:\Program Files\Hunmin IME`
  and registered, which is what makes the two-shortcut and registry
  measurements possible without an elevated round of our own.
- Commands as measurements: `ls` of both Start-menu folders; a Python read of
  both `.lnk` files' UTF-16 string tables for their targets; `reg query … /s` of
  the HKLM TIP tree, the HKCR CLSID tree and the HKCU CTF tree; a decode of
  `unins000.dat` searched for `AppData` (absent) and for `Start Menu` (the
  all-users pair); `grep -c "RemoveLanguageProfile"` in `registration.cpp`.
- Changed files: `docs/OPTIONS-MEASURED.md` (this section and the header table),
  `docs/WINDOWS-TSF-REGISTRATION.md` (the `Enable` hive corrected, the
  Start-menu asymmetry restated with the folder measured),
  `tests/manual-checklists/tsf-unregister-cleanup.md` (the per-user snapshot and
  the two shortcut paths).
- **Read but not executed, labelled:** the uninstaller's deletion is inferred
  from its own recorded uninstall log, not from running an uninstall; Inno's
  `{group}` resolution is the documented rule corroborated by the path in that
  log. The checkout case of premise 4 and the non-elevated registration path in
  option D are read from code and from `register-tsf.bat`, not driven.
- Deliberately untouched: `docs/REMAINING-WORK.md` and `AGENTS.md` (shared
  mutable files, and the board rows for this issue are the maintainer's to flip
  at close), `frontends/tsf/src/registration.cpp` and the tier-F1 tests — no
  option here has been implemented.
- Issue #146 stays open and decision-gated: closing it records a ruling only the
  maintainer can make, and question 2 additionally needs the elevated round.

### URM-7 The ruling as implemented (option C)

What left the registration flow, all of it found by following the ruling rather
than by choosing extra work:

- `CreateStartMenuShortcut` and its call in `DllRegisterServer`
  (`frontends/tsf/src/registration.cpp`), with the `rundll32
  <dll>,HunminSettingsShow` fallback it carried. `HunminSettingsShow` stays
  exported — the interim dialog is still reachable, and `ITfFnConfigure` runs the
  settings exe through `settingspath.cpp`.
- The `programs_folder` seam entry (`frontends/tsf/src/registrationseam.h`): the
  seam reaches outside the process three ways now, not four, because the flow
  writes no file. Its platform double, and the mock's
  `IShellLinkW`/`IPersistFile` class, went with it.
- `hunmin_tsf_settings_exe_in_dir` (`settingspath.h/.cpp`): its only caller was
  the shortcut builder, so the header no longer exports it. The resolver
  `hunmin_tsf_settings_exe_find` is unchanged and still serves the langbar and
  `ITfFnConfigure`.
- The two tests that asserted the save
  (`TsfRegistration.TheStartMenuShortcutIsNeverRemoved`,
  `…ARefusedShortcutSaveStillRegisters`), replaced by one that pins the ruled
  shape positively-when-absent: `TsfRegistration.RegistrationWritesNoFile`
  asserts zero `CoCreateUnknown` activations around a register+unregister cycle,
  and the doubles record that event for any CLSID they do not model — so a shell
  link re-added to the flow fails a test instead of silently rebuilding a
  shortcut.

Verification, on this host: MSVC (`Visual Studio 18 2026`, x64) —
`hunmin_static` built, the tree reconfigured, `tsf-test` and `hunmin-tsf` built,
`ctest --test-dir build-msvc -C Release -L tier-F1` runs the tier, and
`tsf-test.exe --gtest_filter='TsfRegistration.*'` is green (5/5). The `tsf-test`
ctest entry is red on this build tree at BASELINE too: 15 `CandidateTest` cases
fail identically with the change stashed (measured by rebuilding from the
stash), so that red is the tree's, not this ruling's.

What the ruling does NOT settle: a checkout-registered DLL (no installer) now
writes no Start-menu entry at all, which was option C's named loss and is
recorded, not hidden. And question 2 (§URM-4) is untouched — nothing here calls
`RemoveLanguageProfile`.

---

## HEK — the Han/Eng key as a modifier: can each environment deliver it?

**Measurements only. No option is recommended here, and the compound press's
action is not decided here** (that is SPEC-MODES E5, issue #159). This section
prices the *key shape* the E5 question needs — the Han/Eng key held while a
second key arrives — in each frontend environment, and records what the Windows
frontend now does with it.

**Answered later the same day (2026-10-31), so read §HEK-4 with this in hand:** the
maintainer ruled for **modifier + 한/영** — shape **B** below — as the default, on all
three frontends, with the composition remappable in the config dialog
(`docs/SPEC-MODES.md` §E5; `wiki/decisions/2026-10-31-han-eng-modifier-compound-special-functions.md`;
the assignment itself is priced in §E5-3). The TSF change shipped by this section's branch
implements shape **A** — the Han/Eng key itself held, single press settled on release.
Under B no frontend needs that arming to express the assignment, and it carries the
behaviour deltas §HEK-3 lists (toggle on the release, auto-repeat re-arms, a typed key no
longer toggles first), so whether to keep A as well is a decision and not a remainder.
It is recorded here, not reverted.

Produced 2026-10-31 on branch
`scobyseo/modes-ruling-the-han-eng-key-assignment-e5-singl` off `cc65c62`, on the
Windows 11 dev host. `docs/REMAINING-WORK.md` and `AGENTS.md` are untouched
(parallel sessions own them).

### HEK-0 The premise, checked first

The maintainer's words, verbatim:

> han/eng key as modifier. is it possible for all env?

preceded by:

> check all front-ends can use han/eng for composition; android might not
> support it. but in supported env, let's use it as composable key.

Two premises carried by the issue body were checked, one of them against a
running Android image:

1. **"Android has no AOSP keycode for it" — partly corrected.** AOSP defines
   both `KEYCODE_LANGUAGE_SWITCH` (204) and `KEYCODE_KANA` (218); what Android
   lacks is a keycode that *reaches the IME*. Measured (HEK-2): 204 is consumed
   by the framework before any IME sees it, 218 and 212 are delivered.
2. **"the state is one value the engine owns" (R13-정정) — verified for the
   mode, absent for the compound assignment.** The mode lives in the shared
   layer; the assignment has no home there yet, because the engine's key
   vocabulary is press-only and `hunmin_key_event.language_key` is one boolean
   (`hunmin.h`, "The key rule").

The premise the issue does **not** state, and which every option inherits: the
Han/Eng key fires on key-**down** today in all three frontends — TSF's
`ApplyModeToggle` (`keydispatch.cpp`), Android's `onKeyDown` branch
(`HunminInputMethodService.kt`), and fcitx5's own trigger. A held prefix cannot
exist without moving that decision to the release, which is a behaviour delta
in every environment, not only in the new code.

### HEK-1 What "as a modifier" requires of the shared key vocabulary

The engine's key event carries `base`, `shift`, `mods` (`HUNMIN_MOD_CTRL|ALT|
GUI|SHIFT`), a named-key id, `candidates_open` and the one boolean
`language_key`; `hunmin_buffered_preedit_classify` answers
`LANGUAGE_TOGGLE` **before** the chord branch. So the vocabulary today can say
"this key is the Han/Eng key" and cannot say "this key arrived while the
Han/Eng key was held", nor "the Han/Eng key was released".

Three consequences, all of them work that any option for HEK pays:

| need | today | cost |
|---|---|---|
| distinguish single press from chord | impossible: the toggle fires on the press | the decision moves to key-up, or a timeout appears |
| carry the held state | no field, no bit | a `HUNMIN_MOD_*` bit or a new event field (append-only ABI) |
| hold the assignment (R13-정정: the engine owns it) | per frontend | a new engine-level value + readers per frontend |

### HEK-2 Per-environment measurement

| environment | can the Han/Eng key be a held prefix? | evidence |
|---|---|---|
| Windows TSF | **yes, measured live** | `langkey down vk=21 held=1` → `langchord prefix-vk=21 chord-vk=68 held=1` → `langkey up vk=21 -> single press, toggled on release`, in one round |
| Android IME | **yes for hardware keyboards; unmeasured on real hardware** | `onKeyDown: keyCode=218`, `=212`, `=58/57/59/60/111/113` logged on the emulator; `keyCode=204` never logged |
| fcitx5 addon | **possible, two changes first; unmeasured here** | read-only: the addon returns early on releases and reports `language_key = 0` |

**Windows — measured live** with the worktree image loaded through a per-user
`HKCU\Software\Classes\CLSID` override (no admin, no installer round), Notepad
as the host, `tools/tsf-devhost-round.py --host notepad --layout dubeolsik --keys
'{L+d}k{L}'`. The trace lines above are the TIP's own log; the round's readback
was **아**, i.e. the chord's second key composed normally and the trailing single
press toggled to English and committed the syllable. Two facts this settles that
the mock tier cannot: Windows still reports `VK_HANGEUL` as **down** while it is
held (`held=1`), and the second key arrives at the TIP while it is held.

**Android — measured on the emulator** (`sdk_gphone64_x86_64`, Android 15 /
API 35, `hunmin-e2e`), by injecting each key into the real APK against the
test-editor activity:

| keycode | arrives at `onKeyDown`? | note |
|---|---|---|
| 204 `KEYCODE_LANGUAGE_SWITCH` | **no** | the platform switches the IME instead: `mCurMethodId` flipped from `org.hunmin.ime` to Gboard in the same round |
| 218 `KEYCODE_KANA` (Samsung 한/영) | yes | fired the Han/Eng action |
| 212 `KEYCODE_EISU` | yes | reserved for 한자 |
| 57/58 Alt, 59/60 Shift, 111/113 Ctrl | yes | modifiers arrive with their own keydowns |

So the `KEYCODE_LANGUAGE_SWITCH` entry in `KeyAction.HANGUL_ENG.platformAlternates`
(`KeyBinding.kt`) names a key that, on this image, never reaches the service and
takes the user *off* hunmin. That is a finding about the tree, separate from
E5; it is recorded here and not changed in this branch.

The hold itself is **not measurable by injection**: `adb shell input` emits a
down and an up as one unit, so a "held key while a second key arrives" ordering
cannot be produced without real hardware. The service has both `onKeyDown` and
`onKeyUp`, so the shape is implementable; whether a physical Korean keyboard
delivers that ordering is environment-bound (a device is attached to this host
but not authorised for debugging).

**fcitx5 — read, not executed.** `HunminAdapter::keyEvent` returns immediately
on a release (`hunminadapter.cpp`), so no hold state can exist; the adapter
hard-codes `language_key = 0` because the language switch is fcitx5's own
trigger, and the shared layer is built one-slot (`hunmin_buffered_preedit_new(...,
nullptr, ...)`). All three are ours to change; whether the `Hangul` keysym
reaches the addon at all depends on the host keymap and was **not measured** in
this session (no Linux host was used).

### HEK-3 What the Windows frontend does now

`keydispatch.cpp`/`tsf.h`: a Han/Eng binding whose key is **not itself a
modifier** is *armed* on the key-down instead of applied — the down is eaten, no
mode change. The key-up settles it: released with nothing in between, the press
toggles (the single press); a different key-down in between resolves it as the
**compound press**. Modifier keys used as 한/영 (Right Alt, Android's platform
alternate) keep the immediate toggle, so `Alt+A` stays the application's chord.

The compound press performs **nothing**: its action is undecided (issue #159),
so the second key takes its ordinary path and the event is logged
(`langchord ... action undecided`). The behaviour deltas this creates are:

| behaviour | before | after |
|---|---|---|
| single press | toggle on the key-down | toggle on the key-up |
| auto-repeat of the press | swallowed, no toggle | swallowed, re-arms the same press |
| a configured non-modifier chord (Shift+Space) | toggle on the down | toggle on the release |
| a key arriving while 한/영 is held | toggle happened first, key typed in the new mode | no toggle; the key is typed as before and the chord is logged |
| Right Alt as 한/영 | immediate toggle | unchanged (modifier keys do not arm) |

Pinned by `TsfConfigTest.HanEngHoldIsAPrefixNotAToggle` (three outcomes) plus the
updated drive case and candidate/session fixtures, which now send the release a
real host sends. `ctest --test-dir build-msvc -C Release -L tier-F1` → 12/12.

**Correction, later the same day: shape A was reverted when the ruling picked
shape B.** The arm-and-settle-on-release mechanism exists only to tell a single
press from a *held* 한/영, and the ruled compound is a modifier + 한/영 chord,
which needs no press/release distinction at all. Carrying both would have moved
the single press to the key release for no gain, so the Windows frontend went
back to the key-down toggle and the case above was replaced by
`TsfConfigTest.TheHanEngKeyCarriesTheSpecialFunctionOnAChord`. What survives
from the measurement is §HEK-2's platform rows -- that Windows reports the held
`VK_HANGEUL` as down, and that a second key arrives while it is held -- which is
what made the chord believable before it was built. See
`docs/MODES-ENGINE-RULES.md` §3 for what replaced it.

### HEK-4 The options

**A — Han/Eng held as a modifier (the shape measured above).** Gain: the E5
assignment becomes expressible at all, and the press/release distinction lives
in one place per frontend. Cost: the toggle moves to the release everywhere; an
engine-side home for the assignment is still owed (R13-정정); fcitx5 needs
release tracking, a keysym mapping and a two-slot layer; Android needs its own
held-state map because the key is not a modifier bit in `metaState`.
What would change the answer: a measurement showing the release timing is
observable as a user-visible delay on a real keyboard (none was seen on Windows
in one round).

**B — an existing modifier plus 한/영 (Shift+한/영).** Gain: expressible in all
three environments with data that already exists — Android and TSF store and
match `(keycode, modifiers)` pairs today (`KeyBinding.kt` `Shift+LANGUAGE_SWITCH`,
`keybinding.h` `"v1:21:1"`), fcitx5 only needs the binding — and the single press
keeps its key-down contract. Cost: the compound press cannot be pressed with
one hand on the Han/Eng key alone, and it is a different gesture from the
maintainer's phrasing ("han/eng key as modifier"). What would change the answer:
a ruling that the compound gesture must be the Han/Eng key itself.

**C — leave E5 unshipped.** Gain: no behaviour change in any frontend. Cost:
E5's assignment stays unexpressible, and the E5 candidate ("compound = automatic
switch on/off") cannot be built on any frontend.

| surface | A | B | C |
|---|---|---|---|
| engine ABI | new mod bit or event field + an assignment value | none | none |
| engine data | none | none | none |
| TSF adapter | shipped in this branch (HEK-3) | binding only | revert HEK-3 |
| Android adapter | held-state map + a new field through JNI | binding only (218 works, 204 does not) | none |
| fcitx5 adapter | release tracking + keysym mapping + two-slot layer | binding only | none |
| tier A | none | none | none |
| tier F1 | the new case (shipped) | the new case, re-pointed | the new case reverted |
| tier-C corpora | none (no text change) | none | none |
| user-visible delta | toggle lands on release | a second gesture to learn | none |

### HEK-5 Provenance

- Host: Windows 11, `build-msvc` (Visual Studio 17 2022, x64) for the TSF tree;
  Android SDK emulator `hunmin-e2e` (`sdk_gphone64_x86_64`, API 35) for the
  Android rows; `ANDROID_HOME` from `hunmin-sdk-env`.
- Windows round: `python tools/tsf-devhost-round.py --host notepad --layout
  dubeolsik --keys '{L+d}k{L}'`, run dir `build-tsf-devhost/notepad-20261031-194737`
  (gitignored), TIP log `hunmin-tsf-debug.log`. The image under test was this
  worktree's `build-msvc/frontends/tsf/Release/hunmin-tsf.dll`, loaded via a
  per-user `HKCU\Software\Classes\CLSID\{F2C70001-…-0001}\InprocServer32`
  override; the installed machine-wide registration was left untouched.
- Android rows: `tools/android-langkey-round.sh -s emulator-5554` (the committed
  sweep: it installs, selects the IME, injects the candidate keycodes and prints
  the onKeyDown rows) against the
  debug APK plus `testeditor`, read back from `logcat -s HunminIme` and
  `dumpsys input_method`.
- **Read but not executed**: every fcitx5 claim (no Linux host in this session),
  and the physical-keyboard hold on Android.
- Changed by this section's branch: `frontends/tsf/src/{keydispatch.cpp,tsf.cpp,
  keybinding.{h,cpp},tsf.h}`, `frontends/tsf/test/{tsf-config-test.cpp,
  tsf-session-test.cpp,tsf-candidate-fixture.h}`, `tools/tsf-devhost-round.py`
  (`{L}` / `{L+…}` tokens), and this file/`README.md` index rows.

---

## E5 — the 한/영 key: what each single-press / compound-press assignment costs

Status: **open** (issue #159). The requirement is `docs/SPEC-MODES.md` §E5 and the
hook's placement is `docs/MODES-ENGINE-RULES.md` §3; neither is restated here. What is
priced here is the assignment those two leave open, and the two *mechanisms* the words
"단일 입력 / 복합 입력" can mean.

The maintainer's question, verbatim (E5):

> 지금은 한영키를 단순히 한/영 전환을 위해서만 쓰는데, 단일 입력이면 전환, 복합 입력이면
> 특수 키로 만들어도 괜찮지 않을까? 이 기능을 끄고 켜는 데 쓸 수 있을 것 같은데.

**복합 입력 has two readings and they are different mechanisms, not two spellings of
one:** a **chord** (a second key held down with 한/영) and a **sequence** (the same key
pressed twice, which needs a clock). §E5-3 prices both, because a chord fits the key
layers that exist and a sequence does not.

### E5-0 The premises, checked first

Four premises are carried by the issue, `docs/SPEC-MODES.md` §E5 and
`docs/MODES-ENGINE-RULES.md` §3. `AGENTS.md` asks for stated premises to be checked as
carefully as one's own result, so each was run against the tree before anything was
priced.

| # | premise | verdict |
|---|---|---|
| 1 | F9 is occupied by 전처리수식 (the preprocessing expression) | **does not hold as stated.** The 전처리수식 keys are the `flags` overrides on `z`, `h`, `H` (and `w`, `W` in the yet variant) in `libhunmin/data/layouts/sin3-p2.json` — pat.im §2-6, recorded in `docs/SPEC-P2.md` §"ist file anatomy". F9 is a key of **no** layout: 15 layouts × 47 key entries, **0 named (non-printable) keys** — `tools/keymap-fixture.py` prints that count on this tree. F9's only claim in the tree is the fcitx5 adapter's Hanja window (`frontends/fcitx5/src/hunminadapter.cpp` `keyEvent`, `FcitxKey_F9` → the P3 flow): a frontend's window key, not a layout key. **And the maintainer ruled the same day that F9's use as the Hanja key was a test scaffold**, with the Hanja key as the shipped default — `wiki/decisions/2026-10-31-hanja-key-default-f9-was-a-test-scaffold.md`. |
| 2 | Android has no AOSP keycode for F9 | **does not hold as stated.** `android.view.KeyEvent.KEYCODE_F9` exists and is **139**; see below. What is true is narrower and is *not* about the keycode. |
| 3 | the 0.1 half shipped the feature inert | **holds.** `KeyAction.AUTO_TOGGLE` has `defaults = emptyList()`, the service's dispatch skips the action (`if (action == KeyAction.AUTO_TOGGLE) continue`), and the settings row renders disabled at 미정 (0.3). Ruled 2026-10-12, `wiki/decisions/2026-10-12-auto-toggle-disabled-by-default-in-0-1.md`. |
| 4 | the assignment must be a value the engine holds (R13-정정) | **holds as a requirement; the tree cannot express it.** `grep -rn autoswitch libhunmin/ frontends/ tests/` → **0**. `hunmin_ic_set_option()` accepts one name (`caps`); `hunmin_buffered_preedit_set_option()` takes the same shape; no exported symbol returns a key binding. So the value named on screen has no home today, and nothing can diverge from it yet only because nothing shows it. |

**Correction (2026-11-01, issue #165): row 4's implementation half no longer holds.**
The value has a home now — `"autoswitch"` on `hunmin_ic_set_option()`, read back through
`hunmin_ic_get_option()` / `hunmin_buffered_preedit_get_option()`, OFF on a fresh ic and
carried across a language toggle — the binding this row says no symbol returns is
`hunmin_buffered_preedit_special_chord()`, and the transition is the engine's
(`hunmin_buffered_preedit_special()` flips its own value on a serviced press, ruled
2026-11-01). What survives from the row is its *requirement* half only. Both halves are
recorded in `docs/MODES-ENGINE-RULES.md` §3 ("The value the `AUTOSWITCH` function flips").
The row above is left as measured on its own date rather than rewritten.

**Premise 2, measured.** `javap` is not installed on this host, so
`android/view/KeyEvent.class` was read out of `~/Android/Sdk/platforms/android-30/android.jar`
directly (constant pool + `ConstantValue` attributes) — the same method the fixture's own
comment records for API 35 with `javap -constants`:

```
KEYCODE_F9 139   KEYCODE_KANA 218   KEYCODE_EISU 212
KEYCODE_LANGUAGE_SWITCH 204   KEYCODE_ALT_RIGHT 58       (289 KEYCODE_* fields in total)
```

The binding layer already accepts it: `frontends/android/app/src/test/java/org/hunmin/ime/KeyBindingTest.kt`
binds `KeyEvent.KEYCODE_F9` in three cases (free chord, conflict with 한자, 한/영 rebind).
**The fixture's own check was circular, and that is what kept the wrong sentence alive:**
`tools/keymap-fixture.py` set `ANDROID_UNREACHABLE = {"F9"}` with the comment "KEYCODE_F9
does not exist", and the check meant to confirm it asked `android_keycode("F9")` — which
returned `None` **because the fixture's own tables** carry no F9 entry. That is the failure
mode `docs/TESTING.md` says the fixture avoids ("checked against the platform's own header
rather than against a table that would merely restate the code under test"), true for the
X11 keysyms and not here.

**Fixed 2026-10-31**, in the tool rather than in prose: `ANDROID_UNREACHABLE` is now empty
(no layout key lacks an AOSP keycode), and the declaration is verified against
`android.jar` — planting `{"F9": "F9"}` now fails with
`('F9', 'F9', 'KEYCODE_F9 = 139') -- the platform HAS that keycode`, which is the check
doing its job instead of restating itself. The same pass fixed a second defect found while
running it: the header parser accepted `XKB_KEY_*`/`XF86XK_*` but not X11's `XK_*`, so on a
host whose only keysym header is `/usr/include/X11/keysymdef.h` it parsed **0 defines** and
failed; it now reads 2,111.

**What survives is the constraint that matters for E5, and it is not a keycode absence:**
no measured *input source* on a phone produces keyCode 139 (the F row is a modifier
combination in hardware), so a phone's on-screen keyboard cannot send F9 while a USB or
Bluetooth keyboard can, and no measurement of that exists in the tree. F9 is therefore
available to every *binding layer* in this repo and unavailable to the *phone keyboard* —
the distinction E5 needs, and neither the sentence in `docs/TESTING.md` nor the one in
`docs/REMAINING-WORK.md` B4 (which also cites `docs/TESTING.md:280-281`, a line that has
moved) says it.

### E5-1 What each frontend does with the 한/영 key today

| | Android | TSF | fcitx5 |
|---|---|---|---|
| the Han/Eng action | `hunmin_buffered_preedit_toggle_language()` over JNI (`frontends/android/app/src/main/cpp/hunmin-jni.c`) | the same symbol, from `ToggleModeCore` (`frontends/tsf/src/tsf.cpp`) | **not in the addon.** fcitx's own trigger owns the key (FE13; `docs/FRONTEND-EQUIVALENCE.md` §4, §8.3(1)) |
| the physical key | `platformAlternates` of `KeyAction.HANGUL_ENG`: bare `ALT_RIGHT`, `LANGUAGE_SWITCH`, KANA `218` — no default chord. **Measured (§HEK-2): `LANGUAGE_SWITCH` (204) never reaches the service on the API 35 image — the framework switches the IME to Gboard instead**; 218 and 212 arrive | `{VK_HANGEUL (0x15), mods 0}` appended by `LoadToggleKeys()` whatever the store says | none by default: `Hangul` is one of fcitx's default `triggerKeys`, consumed before the addon (§E5-2 addendum) |
| stored form | `"v1:<keycode>:<normalised metaState>"`, one per row | `"v1:<vk>:<mods>;…"`, a list per action (`toggleKeys`, `hanjaKey`) | — |
| remap path | yes: settings window, per action | yes: registry/env list + settings dialog | — |
| does a compound arrive as a distinct event? | **yes, with no code change**: `modifiersMatch()` compares Shift by equality and Alt/Ctrl by family, so a bare alternate does not fire under a modifier | **yes**: `hunmin_tsf_keybindings_match()` compares the three family bits exactly | — |
| precedence / ownership order | `HANGUL_ENG` 0 < `VOICE` 1 < `AUTO_TOGGLE` 2 < `HANJA` 3 (`KeyAction.byPrecedence()`) | toggle first (`IsModeToggle`), then the candidate window, then the chord rule | — |

So the tree holds **three** assignments of one abstract action, plus one absence. R13-정정
asks for one value with three readers; §E5-3 counts the edits that takes.

### E5-2 The collision map — measured, per platform

| chord | Android | TSF | fcitx5 |
|---|---|---|---|
| bare 한/영 | `HANGUL_ENG` (three keycodes) | `HANGUL_ENG` (VK_HANGEUL) | fcitx trigger, outside the addon |
| **Shift + 한/영** | `VOICE` when the keyboard reports 218 (`defaults = Shift+KANA`); free on bare `ALT_RIGHT`. `LANGUAGE_SWITCH` is not a working 한/영 key on the measured image (§HEK-2), so the Shift variant of it is moot | free: exact-bit match means the bare toggle does not fire, and Shift is not part of the chord rule | the keysym is a legal `Key` with a Shift state, but the press is eaten by fcitx's trigger first (§E5-2 addendum) |
| **Alt / Ctrl + 한/영** | free in the binding layer, but bare `ALT_RIGHT` *is* a 한/영 keycode on some keyboards, so the compound is only expressible against KANA / `LANGUAGE_SWITCH` | **intercepted before any handler.** `keydispatch.cpp` `OnTestKeyDown` answers uneaten for a Ctrl/Alt chord in hangul mode, and the measured note there records that a host never calls `OnKeyDown` for a key its test phase passes — the ruled transient-chord rule (2026-10-18, issue #101). The chord would not arrive unless that rule is carved out. | same as Shift: legal as `Key(FcitxKey_Hangul, Alt)`, eaten by the trigger first |
| Shift + 한자 key | `HANJA` alternate is EISU `212`, bare | `VK_HANJA (0x19)`, bare | — |
| Shift + Space | consumed as a boundary before the action loop (`onKeyDown` commits the word, then `" "`) | same shape (`VK_SPACE` boundary branch) | free keysym the addon does see |
| F9 | keycode 139 declared; no phone input source measured | `VK_F9` unclaimed | the Hanja window (P3) |

Two of these are the real prices: **Shift+한/영 already means 음성 입력 on the Samsung
keycode 218**, and **Alt/Ctrl + 한/영 cannot reach a TSF handler at all** without editing a
rule the maintainer ruled on 2026-10-18.

#### E5-2 addendum — fcitx5: not as a modifier; as an event, in two ways (measured 2026-10-31)

The question "can the 한/영 key be a modifier in fcitx5" splits into two, and they have
different answers. Read from fcitx5 `master` on 2026-10-31 (LGPL-2.1; read for semantics,
nothing copied) — `fcitx-utils/keysym.h`, `fcitx-utils/key.cpp`, `fcitx/instance.h`,
`fcitx/instance.cpp`, `fcitx/globalconfig.cpp`. **Read, not run:** no fcitx5 host was
driven here.

**As a modifier: no, and it is structural.** Modifiers in fcitx5 are XKB mod bits, not
keys. `enum class KeyState` is a closed set — `Shift`, `CapsLock`, `Ctrl`(`Mod1`),
`Alt`, `NumLock`(`Mod2`), `Hyper`(`Mod3`), `Super`(`Mod4`), `Mod5`, `Meta`,
`MousePressed`, `Repeat` — and there is no bit a Hangul key could occupy. `Key::isModifier()`
is a hard-coded keysym membership test (`Control_L/R`, `Meta_L/R`, `Alt_L/R`,
`Super_L/R`, `Hyper_L/R`, `Shift_L/R`); `FcitxKey_Hangul` is not in it. And `Key::check()`
with `Key::normalize()` mask every incoming key's states down to
`{Ctrl, Alt, Shift, Super, Mod3}` before comparing — a state bit that is not one of those
five is dropped before any binding is consulted. So "Hangul + X" is not a `Key` fcitx5 can
represent, and no addon can extend the enum.

The only way to make the *physical* key a modifier is the XKB keymap (`modifier_map`
assigning it to `Mod3`/`Mod4`, or mapping it to `Hyper_L`), which is a system-wide
keymap edit outside the IME. It also changes what the key *is*: it stops reporting
`FcitxKey_Hangul`, so fcitx's own `Hangul` trigger and any `FcitxKey_Hangul` binding stop
matching, and only `Mod3`/`Super` survive the mask above (`Mod5` is dropped).

**As an event: yes, but not by the default path.** `HotkeyConfig::triggerKeys` defaults to
`{Control+space, Zenkaku_Hankaku, Hangul}` (`globalconfig.cpp`), and the trigger watcher is
registered at `EventWatcherPhase::InputMethod` in `instance.cpp` and calls
`keyEvent.filterAndAccept()` on a match — so under the shipped defaults the press is
consumed before the addon's `keyEvent` runs at all. That is `docs/FRONTEND-EQUIVALENCE.md`
§4/§8.3(1)'s "fcitx owns the trigger", and it is **a default configuration, not an API
limit**. Two routes exist:

1. **The user removes `Hangul` from TriggerKeys.** The keysym then reaches the IM phase
   and the addon can match `keyEvent.key().check(FcitxKey_Hangul)` and consume it. A chord
   is legal on the addon side too: `Key(FcitxKey_Hangul, KeyState::Alt)`.
2. **The addon registers at `EventWatcherPhase::PreInputMethod`**, which runs before the
   trigger watcher. `Instance::watchEvent()` is public, and `fcitx/instance.h` documents
   that phase for exactly this: "Useful for addons that want to implement an independent
   mode … Check a hotkey in PostInputMethod phase to trigger the mode; handle all the key
   event in PreInputMethod phase just like regular input method."

Route 2 is the one that matters for E5, and it is not free: it **shadows the user's global
trigger** — 한/영 would stop switching input methods for anyone using the addon, which is a
behaviour change to a setting the user owns, made by an addon rather than by the config
they can see. Route 1 has the mirror-image cost: the assignment only works after a global
setting is changed, so "identical in all environments" becomes "identical once configured".

**Two adapter-side gaps standing between that and a working hook, measured by §HEK-2
(read, not executed there either):** `HunminAdapter::keyEvent` returns immediately on a
release, so no hold state can exist, and the adapter hard-codes `language_key = 0` while
the layer is built one-slot (`hunmin_buffered_preedit_new(..., nullptr, ...)`). Neither is
fatal for a *chord* (Alt+한/영 needs no hold state — the modifier is in the state mask),
but both block the *held-prefix* shape, and both are ours to change.

One more measured negative: the shipped Korean IME, `fcitx5-hangul`, does not attempt any
of this. Its `keyEvent` flushes and returns on any `Ctrl`/`Alt`/`Shift`/`Super`/`Hyper`
state (`src/engine.cpp`), and it never binds `FcitxKey_Hangul` — it relies on the global
trigger. So there is no prior art in fcitx5 for the 한/영 key meaning anything beyond the
trigger.

#### E5-2 addendum 2 — the chord, executed on this host (measured 2026-11-01)

The addendum above was **read from source, not run** ("no fcitx5 host was driven here").
There is one now, so the two premises it left open were executed: a 25-line throwaway
`PreInputMethod` watcher inside `HunminAdapter` (removed afterwards; it is a measurement,
not a change), driven through `engine-d1-fcitx5`'s headless instance with fcitx5 **5.1.19**
from the prefix, whose shipped trigger list the run itself prints:
`Trigger Key: Control+space Zenkaku_Hankaku Hangul`.

| # | question | result |
|---|---|---|
| 1 | does the addon see `Hangul` at all under the shipped defaults? | **The IM phase does not** (`HunminAdapter::keyEvent` is never called for it), but a `PreInputMethod` watcher **does**: `PROBE phase key=Hangul accepted=0` arrives, and passing it on lets the trigger take it — the harness's `sendKeyEvent` returned `accepted=1` and `instance->inputMethod(ic)` flipped `hunmin` → `keyboard-us` |
| 2 | can the chord be claimed while `Hangul` stays a trigger? | **Yes.** `Key(FcitxKey_Hangul, KeyState::Ctrl)` reaches the watcher as `Control+Hangul`, the watcher claims it (`filterAndAccept()`), the harness reports `accepted=1`, the active IM is **unchanged** (`hunmin`), and the engine's `autoswitch` value flipped 0 → 1 through the same `pushAutoswitch()` the settings row uses |
| 3 | does claiming the chord shadow the plain press? | **No, and this corrects the addendum above.** It said route 2 "shadows the user's global trigger — 한/영 would stop switching input methods for anyone using the addon". Measured: a watcher that claims only `Ctrl+Hangul` leaves the bare press to the trigger (row 1). Shadowing is a property of *what the watcher claims*, not of the phase it registers at |

So route 2's price is smaller than the addendum priced it: the syntax-level obstacle
(`EventWatcherPhase::PreInputMethod`, `Instance::watchEvent`, both public in 5.1.13+) was
already known, and the behavioural cost the addendum attached to it — losing the user's
trigger — does not apply to the selective form. What remains is real and unchanged:
a watcher at that phase sees **every** key event instance-wide, so it has to answer "is
this my IM" (`instance->inputMethod(ic) == "hunmin"`) before touching anything, and it sits
in front of every other IM's input path.

**What implementing it costs, counted.** The engine seam already exists and is unchanged
by this: `hunmin_buffered_preedit_special()` / `..._special_peek()` resolve the event to a
function and return `>0` / `0` / `-1`, the chord comes from `..._special_chord()`, and a
frontend supplies `hunmin_special_host` (`supports()` may answer 0 — the recorder a VOICE
press needs does not exist here). The addon side is: one watcher registered in the
constructor, the key→(`base`, `mods`, `language_key`) mapping it already does in
`keyEvent`, a `special_peek()` in the test phase and a `special()` in the down, and a
pass-through on `-1`. The pieces that do **not** exist yet on this addon: a
`hunmin_special_host` (so `special()` answers `-1` for everything today — a refusal, which
passes the key, which is the ruled behaviour) and the release handling for a held-prefix
gesture (not needed for a chord).

**What a ruling has to include** (none of it is decided here):

- whether an addon may claim a keysym the desktop also uses as a trigger, even when the
  claim is narrowed to chords — the measured row 2 says it works, not that it should;
- whether the watcher answers only while hunmin is the active IM (the cheap guard) or
  also in another IM's context (which would make hunmin a global hotkey owner);
- whether the same watcher carries `VOICE`, whose `supports()` must answer 0 on this
  platform, and whether a `-1` refusal passing the key through is the desired outcome for
  a press the user aimed at a function;
- and the still-open trigger question above: route 1 stays available for users who prefer
  it, and a watcher does not take it away.

**Not measured here:** a real desktop session (this is the headless D1 instance: no
Wayland/X11 seat, no compositor, no other IM competing), a keymap where 한/영 arrives as
something other than `FcitxKey_Hangul`, and whether any application wants
`Ctrl+Hangul` for itself. Those are environment-bound and they narrow the claim rather
than support it.

### E5-3 The options

Each is stated as mechanism, what it costs in this tree, and what it loses. Nothing here
is a verdict: the costs are comparable, and one of them buys a smaller diff for a larger
loss, which is the kind of trade only the maintainer can weigh.

**1 — single = manual toggle, compound = Shift + 한/영 (the proposed reading).**
*Android:* no change to the binding layer, but `AUTO_TOGGLE` needs a **second alternate
list** of its own, because 한/영 arrives as any of three keycodes and one stored chord
covers only one; and it must be moved **above `VOICE`** in `byPrecedence()`, or Shift+KANA
stays 음성 입력 — which reorders the conflict warning too (three tests pin the order).
*TSF:* free chord, but a new action and a third stored list (`toggleKeys`, `hanjaKey`, and
this), plus the settings row.
*fcitx5:* expressible in two ways, neither of them as a modifier (§E5-2 addendum): the
addon registers a `PreInputMethod` watcher and matches `Key(FcitxKey_Hangul, Shift)`,
shadowing the user's global trigger; or the user removes `Hangul` from TriggerKeys and the
addon matches it in its own `keyEvent`.
*Loses:* the one-key-two-meanings ambiguity is now on the key whose second meaning is
"voice input" on the most common Android keyboard.

**2 — single = manual toggle, compound = Alt/Ctrl + 한/영.**
*Android:* free, with the caveat that bare Right Alt is itself 한/영 on some hardware.
*TSF:* blocked by the transient chord rule (§E5-2) — carving it out touches the 2026-10-18
ruling and its test-phase mirror, which is the largest single edit in this table.
*fcitx5:* as option 1, with the chord written `Key(FcitxKey_Hangul, Alt)` and the same
shadowing or configuration cost.
*Loses:* the autoswitch switch would be the exception to "a Ctrl/Alt chord belongs to the
app", on every Windows host.

**3 — reverse: single = autoswitch on/off, compound = manual toggle.**
*Cost:* the largest prerequisite of any option, because the autoswitch state does not exist
in the engine (§E5-0 premise 4), so this is the only one that cannot ship without new
engine surface *and* three re-mappings; and it takes the single press away from the
interaction the physical key exists for on all three platforms.
*Loses:* R13 said on/off is "a key combination"; a single press is not a combination, so
this reading needs R13 re-read, not just E5 answered.

**4 — compound = the 한/영 key pressed twice.**
*Cost:* a clock. Every layer decides one event at a time and statelessly
(`KeyBindings.matches`, `hunmin_tsf_keybindings_match`, `fcitx5 keyEvent`), and the
engine's key path takes `(base, shift, mods)` with no time. Putting the window in each
frontend is three copies of one rule (R3); putting it in the engine needs a per-event clock
seam that does not exist.
*Loses:* a mistimed second press — the natural reaction when a switch misfires — silently
toggles a feature the user did not aim at. Nothing in the tree measures double-press timing.

**5 — no compound: the single press stays the toggle, and on/off is reachable in the UI
only.**
*Cost:* the smallest in the tree — Android already has the disabled row, TSF and fcitx5
need one settings item each, and the engine still needs the state (§E5-0 premise 4).
*Loses:* R13's stated preference ("the most preferred way is a key combination, not UI"),
and the 0.3 row would show 미정 forever.

**6 — a key combination that is not built on 한/영 (e.g. Shift + Space).**
*Cost:* the only option that needs no change to fcitx's trigger behaviour at all, because
space is a keysym the addon already maps and no trigger list contains it. It needs a
carve-out above the space boundary branch on Android and TSF (`onKeyDown`'s
`KEYCODE_SPACE` branch, `VK_SPACE` in `keydispatch.cpp`), and one engine symbol for the
state.
*Loses:* discoverability — Shift+Space is the 한/영 convention of other IMEs, so it reads
as a language switch rather than as a feature switch; and it does not use the key the
question is about, so it answers R13 without answering E5.

### E5-4 What a ruling has to include

The issue's own verification line ("the assignment appears once in the engine, each
frontend's row reads it, one tier-A case drives the action") is satisfiable only when the
ruling says all five of these — otherwise the implementer decides the rest by default:

1. **The shape:** which press is which action, and by which mechanism — a chord, or a
   sequence with a window.
2. **Fixed or remappable.** If remappable, which store owns it; today three do
   (Android preferences, TSF registry/env, and fcitx5 nothing).
3. **What counts as "the 한/영 key" on each platform** — 218 / `LANGUAGE_SWITCH` / bare
   Right Alt on Android, VK_HANGEUL on Windows, and on fcitx5 *nothing the addon sees*.
4. **What the compound does when it meets a shipped chord**: Shift+KANA = 음성 입력,
   TSF's transient Ctrl/Alt rule, Android's and TSF's space boundary.
5. **Whether the addon may take the 한/영 press away from fcitx's global trigger** — by a
   `PreInputMethod` watcher (works with no configuration, shadows the user's own TriggerKeys
   setting) or by requiring that setting to change (no shadowing, but the assignment only
   works once configured). Either answer is implementable; they differ in who owns the key
   (§E5-2 addendum). This one sentence decides options 1, 2, 3 and 6.

The engine-side prerequisite is the same for every option and is the one piece that can be
built before the ruling: one append-only symbol that exposes the assignment (and R13-정정's
state), so the three frontends read one value instead of holding three.

### E5-5 Summary

| option | mechanism | Android | TSF | fcitx5 | loses |
|---|---|---|---|---|---|
| 1 Shift+한/영 | chord | alternate list + precedence reorder | new action + store entry | `PreInputMethod` watcher (shadows the trigger), or a TriggerKeys edit | Shift+KANA means 음성 입력 |
| 2 Alt/Ctrl+한/영 | chord | free (Alt caveat) | blocked by the transient chord rule | same two routes as 1 | exception to "chords belong to the app" |
| 3 reversed | chord-less | 3 re-mappings | 3 re-mappings | 3 re-mappings | the key's own purpose; contradicts R13's wording |
| 4 double press | sequence | clock | clock | clock | silent toggles on a misfire; no measurement |
| 5 UI only | — | smallest | smallest | smallest | R13's stated preference |
| 6 Shift+Space | chord | boundary carve-out | boundary carve-out | **works** | discoverability; does not answer E5 |

**If a starting point is wanted rather than a menu:** option 1 keeps the key the user
already reaches for and needs no new mechanism on two of three frontends; the deciding cost
is fcitx5, where the key is currently fcitx's own trigger — and since the addon *can* take
it (§E5-2 addendum), the question that would change this is not "is it possible" but
"may an addon shadow the user's TriggerKeys setting", which is a separate ruling, not a
measurement.

### E5-6 The shared rule now answers the compound — and what each frontend owes

**Engine (changed 2026-10-31, verified here).** `HUNMIN_KEY_LANGUAGE_CHORD = 4` is
appended to `hunmin_key_action` (`libhunmin/include/hunmin.h`) and
`hunmin_buffered_preedit_classify()` now answers it when `language_key` is set **and**
`mods & HUNMIN_MOD_CHORD` (Ctrl / Alt / GUI). SHIFT is deliberately not a chord bit — it
is a layout row, so a shift-only press stays the plain toggle. `engine-key-rule`'s
`TheLanguageKeyWinsOverEverythingElse` was pinned to the old answer (chord → toggle) and
is updated with the ruling as the reason.

Verified on this host without cmake or gtest — a standalone probe compiled against
`libhunmin/src/*.c` and run over `libhunmin/data` (13 assertions: bare = 2, ctrl/alt/gui/
alt+ctrl = 4 with and without a list open, shift-only = 2, Ctrl+A = 1, plain letter = 0).
All passed. The gtest case carries the same matrix; **it was not compiled here** (no
cmake, no gtest), so `ctest -R engine-key-rule` on a real build is still owed.

**The rule is inert in all three frontends today, measured:** no caller passes
`language_key = 1`. Android's `HunminAdapter.onKey` hard-codes `false` at the
`layerClassify` call (the JNI parameter exists, `hunmin-jni.c`), fcitx5 documents
"this adapter always reports language_key = 0" (`hunminadapter.cpp`), and TSF builds no
`language_key` at all. So the branch is reachable only from tier A until the frontends
route the Han/Eng press through it. That routing *is* the per-frontend work:

| frontend | what it owes | the test that would settle it |
|---|---|---|
| Android | seed the layer's table from its default and its store (`..._special_bind`), register a host whose `perform` starts the recorder, and stop letting the action loop answer before the engine: the `HANGUL_ENG` match must build the event with `languageKey = true` and the metaState-derived `mods`, then call `..._special()` — `>0` serviced, `0` not a compound, `-1` refused (pass the key through) | a `HunminAdapterTest` case: `layerClassify` with `languageKey=1, mods=ALT` → `4`, plus a service-level case that a bound function is performed and the language did **not** toggle. There is no substitute for the JVM run: `./gradlew :app:testDebugUnitTest` needs JDK + SDK 35, and this host has neither (only `platforms/android-30`) |
| fcitx5 | three things before any test can pass: report `language_key = 1` for the `Hangul` keysym, stop returning early on releases (a hold state needs the release), and get the keysym past fcitx's trigger (`TriggerKeys` or a `PreInputMethod` watcher — §E5-2 addendum). Then the same three calls as Android, with a host that refuses `VOICE` (`supports()` → 0), because there is no recorder on the platform | a tier-D1 case in `testhunmin`: feed `FcitxKey_Hangul` with the Alt state and assert `..._special()` returns the bound function and the refusal path returns `-1`, plus a release-order case. Not writable here: no fcitx5 tree on this host |

**The dispatch has a home too, ruled the same day** (the maintainer's paragraph quoted in
`docs/MODES-ENGINE-RULES.md` §3): the frontend supplies the key codes and the function
implementations, the engine owns each `function → chord` binding, resolves one event to one
function and refuses what nobody can service. Five append-only symbols carry it —
`hunmin_buffered_preedit_special_bind/_special_chord/_special_host/_special` and
`hunmin_special_name` — and `libhunmin/tests/abi/symbols.txt` moved from 41 to 46 lines by
the `nm` command the abi test prints, with the gcc-built `.so` as the source (the diff is
exactly those five, nothing dropped). Two `engine-key-rule` cases pin the semantics: a bound
chord services the function once, an unbound one is not ours, a host that refuses or a
missing host answers `-1`, an ordinary character never reaches a SHIFT binding, and an
unbound Shift+한/영 stays the toggle rather than becoming a dead key.

**The defaults are ruled too (2026-10-31, confirmed from the listed options):** `VOICE =
Alt`, `AUTOSWITCH = Ctrl`, shipped by the engine so a frontend that seeds nothing still
agrees; a store override replaces one entry. Android's own default moves accordingly —
`KeyAction.VOICE` was `Shift + KANA` and is now `Alt + KANA` (`KeyBinding.kt`), with
`KeyBindingTest` asserting both the new default and that Shift+KANA no longer fires it.
**Shift is admitted as a special-combination modifier for the language key only** ("it is
used with normal characters, work as capitalize. but with special combination such as shift
+ han/eng, it should be able to do special work"): the event already carries Shift in its own
field, `special_want()` reads it there, and no character event can reach a SHIFT binding —
capitalize keeps working. A chord nobody bound still answers the toggle, so an unassigned
Shift+한/영 is not a dead key.

The Shift question that used to sit here is therefore closed: the shipped table is Alt/Ctrl,
and Shift is a free slot a frontend or a store may bind, not the default for either function.

### E5-7 Provenance

- Host: Linux, worktree
  `~/orca/workspaces/hunmin/modes-ruling-the-han-eng-key-assignment-e5-singl-2`, branch
  `scobyseo/modes-ruling-the-han-eng-key-assignment-e5-singl-2`, base `cc65c62`.
- Commands as measurements: `grep -rn autoswitch libhunmin/ frontends/ tests/` (0);
  `grep -rn "hunmin_buffered_preedit_toggle_language" …` (definition + two call sites);
  `python3 tools/keymap-fixture.py` — its own summary line is the layout census
  (`47 keys used across 15 layouts (47 printable, 0 named)`), and it skips both
  platform-file checks on this host; a JSON read of every `libhunmin/data/layouts/*.json`
  for `keys` and `flags`; a constant-pool parse of `android.jar` (API 30) for the
  `KEYCODE_*` values quoted in §E5-0.
- Read, not executed, labelled as such: the TSF behaviour is read from
  `keydispatch.cpp` / `keybinding.cpp` / `tsf.cpp` on Linux — no Windows host was used
  here, so the transient-chord interception is the code's shape, corroborated by the
  2026-10-18 record, not a fresh run. Android's on-device behaviour is likewise unrun: no
  device or emulator.
- The fcitx5 half (§E5-2 addendum) is read from upstream source, fetched 2026-10-31 from
  `gitlab.com/fcitx/fcitx5` (`master`): `src/lib/fcitx-utils/keysym.h`, `key.cpp`,
  `src/lib/fcitx/instance.h`, `instance.cpp`, `globalconfig.cpp`; and
  `gitlab.com/fcitx/fcitx5-hangul` `src/engine.cpp` for the prior-art check. LGPL-2.1 — read
  for semantics, nothing copied. No fcitx5 host was driven here, and no fcitx5 tree exists on
  this box (no cmake, no headers), so every claim in that addendum is code-shape, not
  observed keystrokes. The version the tree requires (>= 5.1.13) is what CI builds.
- Changed files: `docs/OPTIONS-MEASURED.md` (this section and the header table),
  `docs/SPEC-MODES.md` §E5 and `docs/MODES-ENGINE-RULES.md` §3 (a one-line pointer each —
  no requirement text moves).
- Changed by §E5-6 (engine): `libhunmin/include/hunmin.h` (the appended enum value),
  `libhunmin/src/buffered_preedit.c` (`classify`), `libhunmin/tests/key-rule-test.cpp`
  (the pin, updated). Verification: a standalone C probe compiled against
  `libhunmin/src/*.c` + `third_party/cJSON/cJSON.c` with
  `gcc -std=c11 -Ilibhunmin/include -Ilibhunmin/src -Ithird_party/cJSON`, run as
  `HUNMIN_DATA_PATH=libhunmin/data /tmp/keyrule_probe` — 13 assertions, all ok. That
  probe is scratch, not committed: the repo's copy of the same matrix is the gtest case,
  and it was not compiled here.
- **Fixed after it, as its own change with its own verification** (it neither depended on
  the E5 ruling nor was blocked by it): `tools/keymap-fixture.py`'s circular
  `ANDROID_UNREACHABLE` claim, its missing `XK_*` header support, and the false sentence
  the pair kept alive in `docs/TESTING.md`. Planted-failure checked, not just green.
- Issue #159 stays open and decision-gated: nothing here chooses, and a ruling is the
  maintainer's (`wiki/decisions/decisions-belong-to-the-maintainer.md`).
## EDW — which English dictionary `english_valid` looks up (issue #160)

**Measurements only. No option is recommended here and nothing is decided; the
choice is the maintainer's.** Issue #160 asks for three things that are one
question — the source, its licence, and whether the artefact ships separately —
so all three are priced below on one probe, and can be compared.

**Ruled 2026-11-01, after this section was written:**

> "hunspell en_US 를 사용하고, 사용자 사전을 추가할 수 있게 하자."

So `libhunmin/data/modes/en-US.json` ships (generated by `tools/en-us2data.py`,
SCOWL permission notice + BSD `.aff`), and a person's own words are merged into
the same lookup — `hunmin_buffered_preedit_load_english`,
`..._load_user_words`, `..._add_user_word`, `..._english_valid`,
`..._english_ready`. Requirement text: `docs/SPEC-MODES.md` R8-정정-2; file
schema: `docs/SPEC-SCHEMA.md` "English word list". **Not** shipped by that
ruling: the curated supplement of EDW-3 (deferred), and the separate
distribution of EDW-4 (follow-up issue). The measurements below are unchanged —
they are what the ruling was made on.

Produced 2026-11-01 on branch `scobyseo/modes-ruling-which-english-dictionary-its-licenc`
(base `master` at `0d72312`), Windows 11, `python3` 3.13. Deliberately **not**
touched by this branch: `docs/REMAINING-WORK.md` and `AGENTS.md`, which parallel
sessions patch.

### EDW-0 The premise, checked first

The issue's claims in its own words, each measured:

1. "**The tree has no such data**: `libhunmin/data/` holds `compose, hanja,
   idioms, krdict, layouts` only" — **verified** (`ls -d libhunmin/data/*/`).
2. "`docs/SPEC-MODES.md` R8-정정 defines `english_valid` as 'a lookup in the
   ENGLISH DICTIONARY'" — **verified verbatim**, and one thing the issue does
   not say is worth stating: that definition has **no consumer in the tree yet**.
   `grep -rn english_valid libhunmin/ frontends/` returns 0; all 23 hits are in
   `docs/` and `tools/`. So this ruling gates an unimplemented path, and nothing
   in the engine breaks while the source stays unnamed.
3. "a dictionary filter drops reproduction 86.35% → 43.87% on the 624-word
   evaluation" — **not re-derivable on this host**, and the reason is not a
   defect: both inputs live outside the repository by licence and size
   (`~/work/wikicorpus/full/latin.txt`,
   `~/work/aihub-records-corpus/624-en-mixed.txt`), and the AIHub terms are
   use-and-training only. What was measured instead:
   - **§4-7d's dictionary has a name now.** `/usr/share/dict/words` on Debian is
     the `wamerican` package (2020.12.07, 104,334 lines), and the `README_en_US.txt`
     shipped with the hunspell dictionary states "the normal (non-large)
     dictionaries correspond to SCOWL size 60". The 43.87% figure was measured
     against one member of a **family** whose members share a vocabulary size
     and a licence — which is what makes the other members' cost predictable
     instead of guessed.
   - **A second instrument**: `tools/modes-dict-probe.py` walks ko.wikipedia
     through the MediaWiki API using the stripper and token rule of
     `tools/build-modes-corpus.py` (imported from it, never re-typed — that file
     exists because both of those steps were once wrong). Two disjoint walks:
     **A** = 3,000 pages from `가` (4,769 forms / 16,815 tokens), which builds
     the corpus-list baseline; **B** = 3,000 pages from `히` (2,987 forms /
     12,018 tokens), the holdout every candidate is scored on.
4. The premise the issue does **not** state, which every option inherits:
   R8-정정 defers the fate of the 50,000 cap and of the corpus list, so any
   dictionary choice re-opens "what happens to `docs/MODES-DATA.md` §4". And
   §4-7a records that a dictionary calls markup valid — `px km cm align center
   style` are all in `/usr/share/dict/words` — a precision cost a harvested list
   does not have. It is re-measured per candidate in EDW-2.

### EDW-1 What `english_valid` asks of the data

One membership test on the Latin reading of the keys, once per finished buffer
(space or symbol); the three-line judgement formula in `docs/SPEC-MODES.md` R8
decides from that one bit plus `hangul_valid`, which is structural and needs no
data at all. So the artefact is
a **word set**, not a lexicon: no senses, no part of speech, no definitions, and
no morphology *output* — but morphology *coverage* matters, because a buffer that
completes as `dogs` or `played` has to be found. Measured (EDW-2): one level of
affix expansion is worth +4.39pp on the holdout, i.e. it is a real property of
the data, not an implementation detail.

| cost | value | how measured |
|---|---|---|
| in-memory, 8 B per slot at 50% load | 0.82 MB for 102,485 forms | `len(words) * 8`, the model MODES-DATA §4-7a uses |
| plain text list, one form per line | 985 KB (`wamerican`'s 102,485) | `wc -c` |
| JSON array of the same forms | 1.30 MB raw / 275 KB gzipped | `json.dumps` + `gzip -9` |
| hunspell `.dic` + `.aff` | 555 KB raw / 192 KB gzipped | `ls -l`, `gzip -9` |

Case has to be folded by the loader: the dictionaries ship lowercase plus some
capitalised and `'s` forms, the buffer is whatever the layout produced. Every
number below is lowercased on both sides, and the tool prints that it did.

### EDW-2 The candidates, measured on the holdout

Token-weighted coverage of B, `python3 tools/modes-dict-probe.py measure …`:

| candidate | licence | entries | ships | embed | cover (raw) | cover (≥2 chars) |
|---|---|---|---|---|---|---|
| SCOWL-60 `wamerican` 2020.12.07 | permissive, notice | 102,485 | 0.99 MB | 0.82 MB | 69.90% | 62.59% |
| hunspell `en_US` stems | same (SCOWL) | 48,262 | 0.55 MB | 0.39 MB | 68.41% | 60.75% |
| hunspell `en_US` + affixes | same (SCOWL) + BSD `.aff` | 118,173 | 0.55 MB | 0.95 MB | 71.94% | 65.14% |
| `english-words` (dwyl) | Unlicense | 370,105 | 4.23 MB | 2.96 MB | 70.75% | 63.66% |
| `google-10000-english` | **none stated** | 10,000 | 0.08 MB | 0.08 MB | 66.58% | 58.48% |
| corpus list (all of A) | CC BY-SA 4.0 (Wikipedia) | 4,769 | — | 0.04 MB | 66.16% | 57.95% |

Five things this table says, and one it cannot:

- **Among general English dictionaries the choice is worth ~3pp** (60.75–65.14%
  at ≥2 chars), while licence, size and provenance differ by everything. The
  source that decides the licence question barely moves the reproduction
  question.
- **Affix expansion is a data property**: the same SCOWL stems score 60.75% raw
  stems, 65.14% expanded — the gap is not in the dictionary choice.
- **A frequency-capped 10,000-entry list costs 2–4pp** against a 48,000–102,000
  entry one (58.48% vs 60.75% and 62.59%), the same direction as MODES-DATA
  §4-7a's cap table measured on its own instrument.
- **Every dictionary counts markup as valid**: `px km cm align center style` are
  found in all three general dictionaries (`px` is absent from the two
  harvested lists). §4-7a's finding is a property of the class, not of one list.
- **What it cannot say**: this is a 3,000-page sample, so the corpus-list row is
  not comparable with §4-7's 86.35%. There the list had 50,000 entries harvested
  from a whole dump and was scored on typed text; here it has 4,769 entries and
  only 806 of B's 2,987 forms (780 at ≥2 chars) are among them. The rows compare
  **candidates on one probe**, which is what the ruling needs; they do not
  reproduce the absolute figures, which need the corpora that are not in the
  repo.

The 10 named tokens — the mechanism §4-7d says halves reproduction — replicate
exactly, at every size:

| candidate | has | missing |
|---|---|---|
| SCOWL-60 / `wamerican` | 0 / 10 | `kbs mbc sbs fc fifa uefa sk kt nhk cj` |
| hunspell `en_US` | 2 / 10 | `kbs mbc sbs fc fifa uefa nhk cj` (`sk kt` present) |
| `english-words` | 3 / 10 | `kbs mbc sbs fifa uefa nhk cj` (`fc sk kt` present) |
| `google-10k` | 3 / 10 | `kbs mbc sbs fifa uefa kt nhk` (`fc sk cj` present) |

### EDW-3 What a supplement buys, and what it costs in licence

The dictionary is not where the loss is, so the obvious repair is dictionary +
a list of the forms it lacks. Measured on the same probe (SCOWL-60 plus the
top-k word-shaped forms of A that SCOWL lacks, scored on B, ≥2 chars):

| added forms | cover on B | gain | added embed |
|---|---|---|---|
| 0 | 62.59% | — | — |
| 100 | 71.16% | +8.57pp | 0.001 MB |
| 250 | 72.19% | +9.59pp | 0.002 MB |
| 500 | 72.77% | +10.17pp | 0.004 MB |
| 1,000 | 73.99% | +11.39pp | 0.008 MB |
| 2,000 | 75.53% | +12.93pp | 0.016 MB |

2,425 of A's word-shaped forms are missing from SCOWL-60 — 35.78% of A's
word-shaped tokens.
The curve flattens at ~2,000 because that is where A's vocabulary runs out; it is
a ceiling of the sample, not of the design. The forms at the top are `fc` `png`
`fifa` `isbn` `uefa` `nhk` `mbc` `kbs` plus markup (`colspan`, `text-align`), so
the supplement is organisations, file formats and residue — the thing §4-7d
identified — and a curated list of those is small in bytes and large in gain.

**The licence consequence is the point of this subsection.** A supplement
harvested from Wikipedia prose is CC BY-SA and pulls the ShareAlike question back
in, in a directory of its own at best (`libhunmin/data/krdict/` is the precedent);
a supplement written by hand has no upstream to attribute, but then its content
is decided by a person, not measured. Neither is priced further here: it is a
ruling, not a measurement.

### EDW-4 Where the artefact ships — B3's mechanics, priced per surface

B3's wording is the constraint, quoted in the decision page
(`wiki/decisions/2026-10-11-auto-switch-off-by-default.md`):

> 자동변환에 필요한 사전이든 뭐든 추가 데이터는 별도 배포를 가정하고 그 데이터가
> 있을때만 기능을 켤 수 있게 하면 transition이 용이할 거 같아.

That same page rules the *mechanism* out of scope — "The mechanism (data format,
presence detection, package name) is design work, not ruled" — so what follows
prices four mechanisms, not four decisions.

| surface | what a separate artefact costs today |
|---|---|
| Debian | `debian/control` has one `Package: hunmin`; a second package is a new stanza plus splitting `install(DIRECTORY libhunmin/data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/hunmin)` in the top-level `CMakeLists.txt`, plus `debian/copyright` and `changelog` entries |
| Windows installer | `frontends/tsf/installer/hunmin.iss` ships `data\` whole (krdict already sits in its own tree for ShareAlike isolation); an optional component is a `[Components]` entry with `Flags: unchecked` |
| Android | assets are staged by a `Sync` from `libhunmin/data` that already excludes `krdict/**` (`frontends/android/app/build.gradle.kts`); data that must not ship needs a download step or an asset flavour — the only surface with a runtime path |
| engine | the presence gate has a precedent: `libhunmin/src/candidate_tables.c` keeps `loaded` as 0 untried / 1 loaded / **-1 absent**; the same three-state answer at the judgement boundary needs no new ABI symbol |

Four mechanisms, each with what it buys and what it costs:

- **In `libhunmin/data/modes/`, shipped whole.** Cheapest: one directory, its
  `LICENSE.md` (the `hanja`/`idioms`/`krdict` convention), a `docs/SPEC-SCHEMA.md`
  section, one provenance `meta.json`. It requires a **permissive** source —
  SCOWL and `english-words` qualify, a Wikipedia-derived list does not without
  the krdict-style isolation. And it makes B3's gate a build-time option rather
  than a distribution fact, because the data can no longer be absent.
- **As a separate package** (`hunmin`-adjacent deb binary, installer component,
  Android download): the B3 shape as written, at the cost of the three surfaces
  above; the gate stays real because absence is a real state.
- **Downloaded on first use**: no data in any package and the licence stays at
  the host, but it adds a network at a moment the person is typing, an integrity
  and hosting answer, and a failure mode on the two frontends whose users may be
  offline.
- **No data at all**: `english_valid` would have no source, which is R8-정정
  reverted — an option only in the sense that the requirement could change.

#### EDW-4 status, 2026-11-01 — the second mechanism, as far as it is buildable here

Issue #172 (and #190, which prices the same change) shipped the **separate
package** row as a *seam*, not as a default: the maintainer's 2026-11-01 ruling
is one dictionary by default, additional ones later. What each surface now
says, measured on the host that made the change:

| surface | state after #172 | evidence, and what was not executed |
|---|---|---|
| Debian | **shipped** | `data` and `modes-data` install components (`CMakeLists.txt`), binary package `hunmin-en-us` (`debian/control`, `debian/hunmin-en-us.install`), and `Recommends: hunmin-en-us` on the base package (ruled 2026-11-01, "Recommends.") — one dictionary on a plain install, and still a package of its own. The partition was exercised with debhelper 13.31's `dh_install` against a staged tree: three files to `hunmin-en-us`, 35 to `hunmin`, no file in both. The `.deb` itself was **not built** — debhelper is not installed on that host and the CI `.deb` job is where dpkg-buildpackage runs. |
| Windows installer | unchanged | the `data\\` copy in `hunmin.iss` still ships `modes\`. The edit is a `[Components]` entry plus `Components:` on that one `[Files]` line; ISCC is not on that host, so it is written-not-executed and issue #172 keeps it. |
| Android | unchanged, and now deliberately so | the asset `Sync` ships `modes/`, so the APK carries one dictionary — which is what the 2026-11-01 ruling asks for ("have one dict by default because we don't have additional distribution mech."). The runtime path (an asset flavour without `modes/`, or a download step) is what a *second* dictionary would need, and it is not built. |
| engine | **complete** | `hunmin_buffered_preedit_english_ready` answers the presence question, the base-only install tree delivers byte-identical text to the full one (`ctest data-split`), and the half that acts on the answer shipped as #169 (PR #201): `hunmin_ic_set_option(ic,"autoswitch",1)` is refused and the stored value vetoed to 0 where the words are absent. Measured on a base-only component install — `data_present=0, set(1)=0, stored=0` — against the full tree's `1, 1, 1`. |

Two facts from that pass worth keeping:

- `hunmin_ctx_layout_autoswitch_capable()` is **not** the gate. Its body answers
  "does this layout's keys emit conjoining jamo" (`schema_loader.c`
  `compute_capability`), a static property of the *layout*, and a machine with
  twelve Hangul layouts and no word list answers 1 for all twelve. Anyone
  wiring a "feature available" row to it would be wiring it to the wrong
  question.
- **The default is one dictionary, and that is a ruling, not a packaging
  accident** (2026-11-01, issue #172 — the words and the record are in
  `wiki/decisions/2026-11-01-one-dict-by-default-the-release-page-later.md`):

  > "Recommends."
  >
  > "have one dict by default because we don't have additional distribution
  > mech. may use github release page for additional package distribution
  > mech. we need to test the feature even before we have distribution mech."

  So the words package is `Recommends` (installed by default, removable), the
  APK keeps its dictionary, and the mechanism for a *second* dictionary is
  deferred to the GitHub release page. What the ruling does not settle is the
  release page's shape (an asset name, a checksum, whether the app or the
  person fetches it), and the absent state stays real — `--no-install-recommends`,
  a distro that drops the words package, an uninstall — which is why the gate
  (#169) is still owed rather than retired.

### EDW-5 Summary

| option | ABI | data | tier A goldens | tier C corpora | adapters | where it decides |
|---|---|---|---|---|---|---|
| SCOWL-60 in-tree | none | +0.99 MB, licence + notice | none (judgement tables, not text) | none | none | licence/attribution only |
| `en_US` + affixes in-tree | none | +0.55 MB, SCOWL + BSD | none | none | loader gains affix expansion or ships the expanded set | same, plus a build step |
| `english-words` in-tree | none | +4.23 MB, Unlicense | none | none | none | size, and no provenance per form |
| dictionary + curated supplement | none | +dictionary, +≈16 KB at 2,000 forms | a new engine test (no tier carries the judgement: it is unimplemented) | none | none | who curates, and under what licence |
| separate artefact (any source) | none | none in the tree | the presence-gated case in that same new test | none | deb splits, installer component, Android download | B3's gate, made real |
| corpus list only | none | +6.53 MB at 815,822 forms (MODES-DATA §4-7a) | the pre-정정 definition's cases | none | none | ShareAlike, and §4's fate |

Nothing here is a verdict. The measurements that would change a reader's choice
are named: a typed-text probe (the 624 set, or another AIHub vertical) would move
the coverage ranking; a 50,000-form harvest from a full dump would make the
corpus-list row comparable with §4-7; and the supplement's plateau at ~2,000
forms is a property of this sample's vocabulary, so a larger harvest would move
that knee.

### EDW-6 Provenance

- Host: Windows 11, `python3` 3.13.14 (`mwparserfromhell` 0.7.2 installed into it
  for the walk). No Linux host in this session, so nothing here is a statement
  about the fcitx5 frontend.
- Instrument: `tools/modes-dict-probe.py`, new on this branch.
  - `fetch --pages 3000 --start 가 --out <A>` and `--start 히 --out <B>`: the two
    walks. `meta.json` beside each records source, timestamp, page count, the
    token rule and stripper it borrowed, the licence, and what the sample does
    **not** represent (typed text, the 624 set, sentence replay, a
    frequency-ordered corpus of its own).
  - `measure` on B with `--corpus-list <A>`; its output is the EDW-2 table.
- Candidate files, fetched 2026-10-31 into a gitignored `build/edl/`, none
  committed: `wamerican` 2020.12.07 (`apt-get download` + `dpkg-deb -x`, no root
  needed), hunspell `en_US` from `LibreOffice/dictionaries` (its README names
  SCOWL 2020.12.07 as the base; the `.aff` carries a 2024-01-29 changelog entry,
  so it is the same family one revision later), `dwyl/english-words`
  `words_alpha.txt`, `first20hours/google-10000-english`.
- Licences read at the source, not summarised from memory: `debian/copyright`
  inside the `wamerican` package (SCOWL's permission notice, verbatim),
  `README_en_US.txt` (SCOWL lineage, "SCOWL size 60"), the `.aff`'s BSD note in
  the same file, `dwyl/english-words`'s `LICENSE.md` (Unlicense),
  `first20hours/google-10000-english`'s repo licence (`NOASSERTION` via the
  GitHub API), and `rspeer/wordfreq`'s README — whose licence section refuses
  the separable-list form this ruling would need: "No. The CSV format does not
  have any space for attribution or license information, and therefore does not
  follow the CC-By-SA license."
- **Read but not executed**: nothing in EDW-4 is measured — the deb/installer/
  Android rows are read off the packaging files named there. The one test that
  would make them measured is a presence-gated judgement case in tier F1, which
  cannot exist before the judgement does.
- Changed by this branch: `docs/OPTIONS-MEASURED.md` (this section and the
  header table), `README.md` (its index row), `tools/modes-dict-probe.py` (new).


## B1 — is B1 still a blocker now that `hangul_valid` is structural (issue #161)

**Decided 2026-11-01: option A — B1 is retired.** The maintainer, verbatim:
*"want to select A"*. What that changes is the cost list under A in B1-4; it was
propagated in the same commit to `docs/REMAINING-WORK.md` (the B1 row, §2 restated
per B1-2, the voice stage 8 row, and the assertion-table comment),
`docs/MILESTONES.md`, `PLAN.md`'s P8 table and D1/D2 of
`wiki/decisions/the-outstanding-decisions.md`. **Not decided by it:** whether the
corpus list, its cap and the `MODES-DATA` sections are deleted or kept (R8-정정
left that open and it still has no issue), and whether E1 gets an owner.

As written, this section was measurements only — no option chosen, the choice the
maintainer's — because issue #161 asks for a ruling and asks that the options stay
comparable. It asks three things: whether B1 is retired or narrowed, whether §2's
five items stay frozen, and whether R9's threshold is re-based. This section prices
each answer against the tree and records what the tree already decides.

Produced 2026-11-01 on `scobyseo/modes-ruling-is-b1-still-a-blocker-now-that-hang`
at base `7f20be8` (`origin/master`), Windows host, engine built with the
llvm-mingw toolchain into this worktree's `build/`. It deliberately does **not**
edit `docs/REMAINING-WORK.md` or `AGENTS.md`: both are shared mutable files that
parallel sessions patch, and the board edits a ruling on B1 triggers belong after
the ruling, not in its support paper — issue #161's own verification step says the
same.

### B1-0 The premise, checked first

| # | what issue #161 says | measured in the tree | verdict |
|---|---|---|---|
| 1 | the board records B1 as blocking everything in §2 | row B1 ends "Everything in §2 inherits this"; §2's heading is "Blocked behind B1 — measurable only once the source is chosen" and its preamble says the items "are simply downstream of the judgement source" | **verified** |
| 2 | the voice-input stage 8 row still reads "🔒 Blocked on B1" | stage 8 of the voice table reads exactly that, and its note repeats "gated on B1 as before" | **verified** |
| 3 | R8-정정 (2026-10-25) changed what the question was about | the ruling's formula block reads "`hangul_valid` = the buffer composes as Korean syllables of the shape (choseong + jungseong) or (choseong + jungseong + jongseong) only" | **verified** |
| 4 | so the Hangul side needs no word source at all | no data path exists or is planned for it: there is no `hangul_valid` symbol in the ABI, no file it could consult, and the ruling says "`hangul_valid` becomes a structural test, not a word test" | **verified** — and nothing implements the test yet (#166) |
| 5 | "the source problem that remains is the English one (separate issue)" | **it no longer remains.** R8-정정-2 (2026-11-01) named hunspell `en_US` plus a user dictionary; issue #160 is closed and its work is shipped and merged: `libhunmin/data/modes/en-US.json` (118,150 forms, `meta.json` provenance beside it), `libhunmin/src/wordset.c`, the `hunmin_buffered_preedit_english*` ABI, and `libhunmin/tests/wordset-test.cpp` | **corrected** |

Two premises the issue does not state, and every option inherits them:

**(a) §2's five items are not only board rows any more — four of them carry their
own issue.** Enumerated from the open set: #181 is the harness ③-bis needs,
#163 holds E2/E3/E6, #180 holds E7, and the judgement itself is #166 (the
structural test), #168 (the call site) and #178 (its golden corpus). **E1 (M2) is
the one item with no filed owner.** A retirement decision therefore reclassifies
issues rather than unfreezing prose.

**(b) R9's threshold — the issue's third question — was answered 74 minutes
later in the same document.** `git log` dates the two edits: R8-정정 landed as
`72c668e` at 15:45:07 on 2026-10-25 and still ends "Not decided by this entry,
and left open: … whether R9's threshold (E3) is re-based"; R9's own text landed
as `343ba6a` at 16:59:41 the same day and opens "**The numeric threshold is not a
requirement (ruled 2026-10-25).** §2.E3 is retired". So the question has an
answer in the requirement's home, and the earlier sentence was never updated — a
stale cross-reference inside one file, not a disagreement about the ruling.

There *is* a real conflict one level down, and it is already filed: §2.E3 still
specifies a **measured** N ("set N to 2, 3, 4 and measure …, find the N at which
(b) is minimal"), while R9 now makes it a **design-time** choice "made where the
judgement is built and recorded there". That is a question of method, not of
wording; #163 holds it, so this paper records it and leaves it there.

### B1-1 What the words mean now, and what of it is built

- **`hangul_valid` has no source to choose.** It is a predicate over the finished
  buffer, and the only input it needs is the composition rule the engine already
  carries (`libhunmin/src/tables.c`). Its decision surface is the 11,172-syllable
  modern block (19 choseong × 21 jungseong × 28 jongseong): every precomposed
  syllable in U+AC00–U+D7A3 is well-formed by construction, so the test is
  "the rendered text is syllables and nothing else". "No source" is not the same
  as "no cost" — the definition admits any well-formed run, word or not, which is
  the trade the ruling accepted in its own text.
- **`english_valid` has a source, and it is settled and shipped** — see B1-0
  row 5. Two states are normal rather than errors: an absent shipped list answers
  "not english" and `..._english_ready` answers 0 (B3's data gate); the user's
  words are a plain-text file merged into the same lookup at load time.
- **The substrate the judgement needs already ships**, which
  `docs/MODES-ENGINE-RULES.md` does not yet say. That document's header reads
  "design direction settled, no core implementation" and its "Where the mode
  lives" section plans the mode onto `hunmin_ctx`; the tree instead has
  `hunmin_buffered_preedit` constructed with **both** layout ids
  (`hunmin_buffered_preedit_new(ctx, hangul_layout_id, latin_layout_id, …)`),
  owning the composition buffer, the language state, the boundary
  (`HUNMIN_KEY_BOUNDARY_PASS`) and the toggle. #168's body states the same against
  the header. **What remains of the modes work is one predicate (#166) and one
  call site (#168), not an architecture** — which is the only reason retiring B1
  changes anything. MODES-ENGINE-RULES §1's "future work" paragraphs are obsolete
  as written; they are recorded here and left for #168's PR rather than edited
  into this one.
- **The formula needs both renderings of one key sequence** — the composed Hangul
  (for structure) and the literal Latin (for the dictionary). That is why neither
  validity is a lookup alone, and it is the work #168 evaluates.

### B1-2 What each §2 item actually needs

| §2 item | what its own text says it needs | measured state | needs a Hangul source? | owner now |
|---|---|---|---|---|
| **M1** ③-bis wrong-direction metric | "needs the same source R8 does" | `docs/MODES-METRICS.md` §1.4 is `NOT MEASURED`; no runnable judgement and no harness exist | **no** — the pair is a structural predicate plus a shipped dictionary | #181 (harness) + #168 |
| **M2** E1 symmetry | "Was measurable via krdict's `pos`; that route left scope, so it must be re-derived inside the boundary" | not measured, and unfiled | **no** — under the structural definition the measurement needs a corpus of typed Korean (outside the repo, path as an argument) and the shipped list; the josa/eomi/pos requirement disappeared with the word test | nobody |
| **M3** E2/E3/E6/E7 thresholds | "all `[E]`" | E3 retired by R9; E2 and E6 are #163; E7 is #180 | **no** | #163, #180 |
| **M4** third-party correctness | "evaluated against both 두벌식 and 세벌식, at minimum 신세벌식 P2" (`PLAN.md`, P8 table) | not started; P2's oracles already exist (pat.im worked examples, kime facts) | **no** — this is composition correctness, independent of any judgement | nobody (P2 lane) |
| **M5** word-list schema | "`docs/SPEC-SCHEMA.md` has no word-list section" | **stale**: that document now carries "English word list (`data/modes/`)", the JSON shape, `meta.json`'s contents and the user dictionary's file format; `debian/copyright` carries the SCOWL entry | **no** | done for the shipped list; its pos/frequency/level and josa/eomi sub-questions are moot on the Korean side because there is no Korean list any more |

**Counted: none of the five waits on a Hangul word source.** Three wait on
implementation or a harness (#166/#168/#178/#181), one on nothing but its own
measurement (E1), one is already answered (M5), and M4 was never a judgement
question at all.

### B1-3 The structural test's decision surface, measured

The question a reader should not have to take on faith: if `hangul_valid` consults
no data, does it still discriminate, or does it say "yes" to everything? Measured
by rendering every word of the shipped dictionary through the 두벌식 layout with
the engine's own simulator (`build/hunmin-sim.exe --batch`, one fresh `ic` per
line) and classifying the rendered text against R8-정정's definition — every
character a syllable in U+AC00–U+D7A3, and nothing else.

| | count |
|---|---|
| shipped forms in `libhunmin/data/modes/en-US.json` | 118,150 |
| fed to the engine (letters only) | 85,718 |
| skipped (internal `'` or `-`, not layout keys) | 32,432 |
| **both valid** — rendering is all well-formed syllables | **1,722 (2.01%)** |
| **english only** — rendering leaves a jamo or a Latin char | **83,996 (97.99%)** |

Examples of the both-valid class, where R8's formula keeps the current mode:
`aback → 뮴차`, `abeam → 뮫므`, `abraham → 뮥모므`. Examples of the english-only
class, where the formula switches: `aa → ㅁㅁ`, `aachen → ㅁㅁ초두`,
`aardvark → ㅁㅁㄱㅇㅍㅁ가`.

Read plainly: an English word typed in Hangul mode usually leaves a word-final
consonant or a doubled initial as a lone jamo, which is not a syllable — that is
what makes it `hangul_valid = false` and lets the dictionary side decide alone.
The structural test is not a rubber stamp on Latin input. The 2.01% are the cases
where R8 specifies **no switch**; that is the measured size of the class the
ruling's definition makes inert, and it is a different quantity from the
86.35% → 43.87% reproduction loss R8-정정 records (that one is about the
dictionary, not about structure).

**What this probe is not.** The classifier is re-implemented in Python and stands
in for the predicate #166 will add to the engine; the composition it classifies
comes from the engine, so only the "is it syllables" step is duplicated. It says
nothing about the Korean side of the pair: the only Korean word list in the tree
is `krdict/`, which the maintainer's 2026-10-05 boundary excludes from this
research, so Korean input is reasoned about from the definition in B1-1 and
labelled as such. E1's measurement is the one that would test it, and it needs a
corpus path rather than a dictionary.

### B1-4 The options

**A — Retire B1. ← selected 2026-11-01.** Its Hangul half is answered by R8-정정 and its English half is
decided and shipped by R8-정정-2, so the row has no surviving content; §2's rows
are then restated per B1-2 and the "everything inherits this" claim comes out.
*Cost:* prose only — the B1 row, §2's heading and preamble and the voice stage 8
row in `docs/REMAINING-WORK.md` (**6 of its 10 mentions carry the claim; the other
four are the B1–B9 legend and table comments**), the "hostage to B1" bullets in
`docs/MILESTONES.md` (**5 of its 8 mentions are claims; three are the id-range
legend**), P8's "judgement source … **OPEN**" row in `PLAN.md`, and the dated
status line on `wiki/decisions/the-outstanding-decisions.md` that this branch
already added under D1 (it records that the ruling exists and points at #161 for
the board row; it does not retire anything). No code, no ABI, no data, no goldens.
*Gain:* the block on §2 becomes what it is — implementation issues #166/#168/#178
and #181 — so the next session reads the issue list instead of re-deriving a
source question that no longer exists.
*What would change it:* evidence that a Korean word source is needed for
something **outside** R8's validity (a future feature asking "is this a word?",
not "is this well-formed"). Nothing in the tree asks that today.

**B — Retire only the Hangul half, keep B1 as "the English source".** *Cost:* a
board row whose answer is R8-정정-2 — a second home for a decided fact, which the
repository forbids. *Gain:* none measured; #160 is closed, and the only live part
of its subject is #190 (the separate distribution), which is packaging, not a
source. *What would change it:* a decision to re-open the dictionary choice —
which would re-open §EDW, not B1.

**C — Retire B1 and carry its one genuinely open remnant as its own row.** R8-정정
left "whether the corpus list, the cap and the `MODES-DATA` sections that describe
them are deleted or kept as the record of the measurement" open, and R8-정정-2 did
not answer it. That remnant is about the **English** list, so it is not B1 either;
but it is the only undecided item in B1's neighbourhood and it has no issue.
*Cost:* one row plus one issue. *Gain:* the delete-or-keep decision stops
travelling inside a blocker that is being retired. *What would change it:* evidence
that `MODES-DATA` §4 is already cited as live by something — measured: SPEC-MODES
§5's file table still calls it "the `valid` criterion of R8", which R8-정정 has
since demoted to a record, so that pointer needs the same correction whichever
option wins.

**D — Leave B1 open as written.** *Cost:* the two homes keep disagreeing (the SPEC
has not mentioned B1 since R8-정정; the board has never mentioned R8-정정 or
R8-정정-2), and this page is re-derived. *Gain:* none measured — the disagreement
has not produced a wrong decision yet, only re-reads.

### B1-5 Summary

| option | board / milestones prose | SPEC text | issues reclassified | code, ABI, data, goldens | docs-lint |
|---|---|---|---|---|---|
| A | 6 of REMAINING-WORK's 10 mentions, 5 of MILESTONES' 8, 1 PLAN row, 1 wiki line | none | none closed by this paper | none | 0 errors |
| B | 1 row reworded, same footprint elsewhere | none | none | none | 0 errors |
| C | A's footprint plus 1 row | none | 1 new issue proposed | none | 0 errors |
| D | none | none | none | none | 0 errors |

### B1-6 Provenance

- **Measured with:** `git log -1 --format='%ad'` on `72c668e` and `343ba6a` (the
  74-minute gap); `git merge-base --is-ancestor` (confirms the order);
  `grep -rn "\bB1\b" --include=*.md` over `docs PLAN.md README.md wiki AGENTS.md`
  (the 10/8/2 split, with `docs/FRONTEND-CONTRACT.md`'s two hits being a
  different B1 — an A/B finding label — and `AGENTS.md`'s one being the B1–B9
  legend); `gh issue list --state open` for the owner map; the engine probe in
  B1-3 (`cmake --build build --target hunmin-sim` then `python3
  build/probe_b1_structure.py`, 1.2 s, one process over all lines).
- **Read, not executed:** nothing claimed as measured above is unmeasured. The
  Korean side of the validity pair is stated from R8-정정's definition and
  labelled as such; its measurement is E1's, and E1's test does not exist.
- **Not touched deliberately:** `docs/REMAINING-WORK.md` and `AGENTS.md`
  (parallel sessions); `docs/MODES-ENGINE-RULES.md` §1 (edited here as a finding,
  belongs to #168's PR); `docs/SPEC-MODES.md` (no requirement text changed — the
  R9/E3 method conflict is #163's, and R8-정정's stale "left open" sentence is
  recorded in B1-0 rather than rewritten, because the two texts disagree about
  method and only the maintainer settles that).
- **Propagated, not decided:** the header table row, the section key and the
  `README.md` count line, plus the `README.md` count correction described in the
  commit (it said "seven priced questions" over eight sections).

## PAS — pass lines for the two scored metrics (issue #162)

**Measurements and arithmetic only. Neither number is chosen here; both are the
maintainer's** (`AGENTS.md`, "never decide on your own"). Issue #162 asks for a
value for the keystroke metric (`docs/SPEC-MODES.md` R12, E4) and for the manual
on/off count (R7-정정, `docs/MODES-METRICS.md` §2). What is priced below is what
each candidate number *costs* — the corpus property it requires, the misfire
budget it implies, and what it fails to grade when the corpus is not what the
person who chose it assumed.

Produced 2026-11-01 on branch
`scobyseo/modes-ruling-pass-lines-for-the-metrics-b2-e4-ke` (base `master` at
`7f20be8`), Windows 11. Deliberately **not** touched by this branch:
`docs/REMAINING-WORK.md` and `AGENTS.md`, which parallel sessions patch.

### PAS-0 The premises, checked first

The issue's own claims, each measured against the tree:

1. "`docs/MODES-METRICS.md` §2 states it plainly: *Pass Line — none [E]*" —
   **verified**, and the file's three grading rows (`②`, `②b`, `③-bis`) each
   read `undetermined`.
2. "the numbers must come with the harness that produces them (separate
   issue)" — the harness is **#181, open and unbuilt**: `ls tools/ | grep -i
   'metric\|replay'` returns nothing, and the six harnesses
   `docs/MODES-METRICS.md` §3 lists as unfit were scratch scripts that are not in
   the tree. So the harness is a precondition of a *run*, not of a *number*.
3. **The feature the numbers grade is half on `master`.** Re-measured 2026-11-01
   after rebasing onto `master` `1029377`, per half:
   - `english_valid` — **shipped**: `hunmin_buffered_preedit_english_valid` over
     `libhunmin/data/modes/en-US.json` (1,492,075 bytes), and the Latin reading it
     is asked about is replayed by the layer itself
     (`hunmin_buffered_preedit_latin_text`, #167).
   - `hangul_valid` — **shipped**: `hunmin_hangul_valid` in
     `libhunmin/src/hangul_valid.c` (#166), a predicate over the composition rule
     with no data behind it.
   - the `autoswitch` state — **shipped**: `hunmin_ctx_layout_autoswitch_capable`,
     `ic->autoswitch`, and the AUTOSWITCH chord flipping it (#165). The state the
     feature's ON/OFF is read from exists.
   - the judgement that applies R8's formula at the boundary — **not shipped**.
     #168 is open: `boundary()` (behind `hunmin_buffered_preedit_commit_word` and
     the declined key) commits the finished word in the current mode and asks
     neither predicate, and `libhunmin/include/hunmin.h` still says "Nothing here
     decides to switch modes".
   So a replay today can measure both validity halves and nothing that switches. A
   number ruled now is a number the harness can only report **after #168**, and it
   has to be worded so that it is gradeable then rather than measured now.
4. **No sentence corpus is on this host.** `ls ~/work` returns `edw` only; the
   three measurement inputs `docs/MODES-DATA.md` §4-3 names
   (`~/work/wikicorpus/full/*.txt`, `~/work/aihub-records-corpus/...`) are
   absent. Nothing below is derived from replay data, and because
   `docs/MODES-METRICS.md` §3 voids every number produced from a corpus
   constructed in-session, **no such corpus was constructed here.** The arithmetic
   below needs no corpus: it is the shape the harness will have to report.
5. **The board and the SPEC disagree on how many numbers there are, and the reason
   the SPEC gives for there being two has just expired.** Board row B2
   (`docs/REMAINING-WORK.md` §1) says "pass lines for **all three** metrics"; E4's
   text says "the **two** numbers needed now are two", on the ground that `③-bis`
   has no judgement source. **B1 was retired 2026-11-01 (#161, `§B1`):** the
   Hangul half is the structural rule `hangul_valid` now ships and the English half
   is R8-정정-2's shipped list, so `③-bis` no longer lacks a source — it lacks only
   the judgement (#168) and a row in the harness (#181). Whether the ruling covers
   two numbers or three is therefore a live choice, not a settled one. Recorded,
   not edited: both files are outside this branch's scope.

### PAS-1 What the two numbers are quantities *of*

A pass line is a number in some unit, and the unit decides whether the number can
be met at all. Fix the units first.

Notation for one text: `n` characters, `t` script transitions — a transition is a
point where the script changes, and `t` is exactly the number of 한/영 presses the
same text needs by hand. Let `d = t/n`, the corpus's **transition density**.

Per transition the feature has three outcomes:

| outcome | share | keys the person spends |
|---|---|---|
| switches correctly | `a` | 0 |
| misfires — wrong word committed | `r` | `2k+1` (backspace `k`, 한/영 once, retype `k`) |
| does not switch | `s = 1-a-r` | 1 (한/영) |

`k` is the mean length of the word a misfire costs. The feature **off** is
`s = 1`: `n + t` keys. The feature **on** is `n + t·(s + r(2k+1))`.

So, as a share of the switching the same text needs by hand — the pool the feature
can act on — and then over all keystrokes:

```
q  = pool share captured  = a - (2k+1)r = 1 - u - (2k+1)r
u  = 1 - a                = manual-toggle share of the pool, the ②b metric
overall saving            = q · d/(1+d)
```

Two consequences that decide the rest of this section:

- **A ruled pair `(q, u)` is a precision requirement on the judgement.** Inverting
  the line gives the misfire budget the ruling implies, `r ≤ (1-u-q)/(2k+1)` —
  whether or not that was intended, the two numbers do not leave `r` free.
- **`②` has a hard ceiling that is a property of the text, not of the feature.**
  Even with `r = 0` and `u = 0`, the most any implementation can save is `t` keys
  against `n + t`, i.e. `d/(1+d)`.

| `d` (transitions per character) | 0.01 | 0.02 | 0.05 | 0.10 | 0.20 | 0.50 |
|---|---|---|---|---|---|---|
| ceiling on `②`, `r=u=0` | 1.0% | 2.0% | 4.8% | 9.1% | 16.7% | 33.3% |

and, inverted, an overall-percentage pass line requires this much switching:

| overall pass line, `r=u=0` | 2% | 5% | 10% | 25% |
|---|---|---|---|---|
| `d` required (one transition per this many characters) | 0.020 (49) | 0.053 (19) | 0.111 (9) | 0.333 (3) |

`d` is not a free parameter the ruling may ignore, because **the corpus is
unwritten**. `docs/MODES-METRICS.md` §1.1's census (marked measured, finalized)
counts 259,590 English tokens appearing near Hangul against 1,016,338,031 Hangul
tokens in the 624 corpus — one English token per ~3,915 — so `d ≈ 2.6·10⁻⁴` and
the overall ceiling is **0.03%**. On a broad corpus this metric measures nothing
at all. The opposite extreme is a corpus of messenger sentences, where a person
switches between Korean and English a few times per sentence. **The pass line's
meaning is therefore a property of the corpus #181 has not chosen yet** — which is
why a number stated as a percentage of all keystrokes cannot be ruled before that
choice, while a number stated as a share of the pool can.

*(§1.1's figures are read from that section, not re-derived: the 624 corpus is not
on this host. They are used here only for the order of magnitude.)*

### PAS-2 `②` — the options for the keystroke pass line

| | shape | reads | what it costs, and where it fails |
|---|---|---|---|
| K1 | **percentage of all keystrokes**, e.g. "≥5% fewer" | `q·d/(1+d)` | Requires `d ≥ 0.053` at `q=1` — one transition per 19 characters. On the broad corpus of §1.1 it is unsatisfiable by any implementation; on a dense corpus it is passed by a mediocre one. The number silently moves by an order of magnitude when the corpus changes |
| K2 | **share of the pool `q`**, e.g. "≥50% of the switching removed" | `a - (2k+1)r` | Corpus-invariant: the same number grades a dense corpus and a sparse one. Loses the "how big is the win overall" reading, which is `q·d/(1+d)` and can be printed beside it |
| K3 | **absolute**, e.g. "≥25 keystrokes per 1,000 characters" | `1000·d·q` | Comparable only across runs on the *same* corpus; means nothing as a quality statement until `d` is fixed |
| K4 | **no net loss** (`q ≥ 0`, i.e. `r ≤ (1-u)/(2k+1)`) | sign | Satisfiable by an implementation that is merely careful; grades the absence of harm, not the presence of the feature. Its value is as a *guard* beside a K2 line, not as the line |

The risk every one of these carries is the one R11 states plainly — a person is
allowed to fix a misfire with backspace — so **the repair cost has to be inside
the metric**, not excluded from it. K1 and K3 do not put it anywhere near the
number unless `d` is fixed; K2 does, because `q` is defined net of repair.

### PAS-3 `②b` — the options for the manual-toggle cap

R7-정정's words are about "turning the feature off and on"
(`docs/SPEC-MODES.md` §R7-정정), so the strict reading counts **mode presses
only** — backspace repairs are `②`'s business. That is a choice, and it has a
price: measured solely as mode presses, the metric reads well for an
implementation that lets the person repair constantly rather than toggle.

| | shape | reads | what it costs, and where it fails |
|---|---|---|---|
| T1 | **zero toggles per sentence** | `u = 0` | Binary and it forbids the behaviour R7-정정 explicitly permits ("사용자가 의도적으로 어색한 단어를 입력할 수 있으니"). A single deliberate word for one sentence fails it |
| T2 | **rate cap**, e.g. "≤1 per 10 sentences" | `u·t/sentences` | Attainable and readable, but it hides `t`: one toggle per 10 sentences is excellent on dense text and impossible to beat on sparse text if `t` itself is small |
| T3 | **ratio to the manual baseline**, `u ≤ X%` | `u` | The same quantity as T2 divided by the corpus's own `t`; corpus-invariant for the same reason K2 is. Must be stated with the `k` it assumes, or `②` and `②b` can be ruled into a negative `r` |

T2 and T3 are the same quantity scaled by the corpus's own `t`, so they agree
only when `t` is fixed. The ruling has to say which of them is *binding*: the
harness can report either, and a rate cap reads differently on a corpus with four
transitions per sentence than on one with none.

**The misfire budget a pair implies** — `r ≤ (1-u-q)/(2k+1)`:

| `q` \ `u` | `k=2` | `k=3` | `k=4` | `k=5` |
|---|---|---|---|---|
| `q=0`, `u=25%` | 15.0% | 10.7% | 8.3% | 6.8% |
| `q=25%`, `u=25%` | 10.0% | 7.1% | 5.6% | 4.5% |
| `q=40%`, `u=25%` | 7.0% | 5.0% | 3.9% | 3.2% |
| `q=50%`, `u=25%` | 5.0% | 3.6% | 2.8% | 2.3% |
| `q=50%`, `u=10%` | 8.0% | 5.7% | 4.4% | 3.6% |
| `q=75%`, `u=5%` | 4.0% | 2.9% | 2.2% | 1.8% |

Read across a row: the same saving at a longer repaired word needs more precision.
Read down a column: 25 percentage points of extra saving costs a few points of
misfire budget. Every cell is a requirement on the judgement (#168) and on the
short-word threshold (#163's E3), not on the harness.

### PAS-4 What a ruling has to include

Each of these is a sub-decision a bare number leaves open, and each one changes
the measured value:

1. **The corpus**, and whether it is the target population (typed messages where
   people mix scripts) or a broad one. `d` differs by three orders of magnitude
   between them (§PAS-1).
2. **Aggregation**: per sentence (mean of ratios) or corpus-wide (ratio of sums).
   They are different numbers, and the per-sentence mean is dominated by the many
   sentences with `t=0` or `t=1`.
3. **`k`**, the mean length of a repaired word — the tables above are per `k`.
4. **Whether a repair counts as a manual intervention** (`②b`) or only a mode
   press does.
5. **Which metric set the numbers cover.** B2 says three, E4 says two. `③-bis` has
   no source (B1), so two is what can be ruled; the third needs its own line, or
   the board row narrowed.
6. **What happens on a corpus where the feature cannot fire** (no `en-US.json`
   present — B3's data gate). A metric that can be met by not switching needs the
   OFF-by-default state stated as a precondition, not silently averaged in.

### PAS-5 Summary and the recommendation

Nothing above decides. If a recommendation is wanted:
**`②` as a share of the pool, `q ≥ 50%`, with `②b` as `u ≤ 10%`** — the deciding
reason is that `q` and `u` are the only two forms in these tables that do not
change meaning when #181's corpus changes, and that pair implies a misfire budget
of `r ≤ 5.7%` at `k=3`, which is a number the judgement can be built to instead
of a number discovered after.

What would change it: a measured `d` and `k` from the harness's chosen corpus (if
`k=2` the same `q` permits `r ≤ 8.0%`, and if the corpus is dense enough that
`d ≥ 0.11`, an overall-percentage form becomes meaningful and K1 becomes the
readable choice); and a ruling that `③-bis` joins the scored set, which would add
a third line with its own `k`-independent definition — a live choice since B1 was
retired, not a blocked one.

### PAS-6 Provenance

- Host: Windows 11, `python3` 3.13. Nothing here needed a build: no C was
  compiled and no binary was run, because the feature these numbers grade is
  unimplemented on this base (§PAS-0, item 3).
- The tables in §PAS-1/§PAS-3 are arithmetic from
  `q = 1 - u - (2k+1)r` and `ceiling = d/(1+d)`; they are re-derivable in one line
  each, e.g.
  `python3 -c "q=0.5;u=0.1;print([round((1-u-q)/(2*k+1)*100,1) for k in (2,3,4,5)])"`
  → `[8.0, 5.7, 4.4, 3.6]`, the `q=50%,u=10%` row above. No script is
  committed for them, deliberately: a tool here would be a second home for the
  harness #181 owns, and there is no input data for it to read.
- Read, not re-derived: `docs/MODES-METRICS.md` §1.1's 624 census figures (the
  corpus is not on this host) and §3's harness table (the scripts are not in the
  tree).
- Checked against the tree by `gh issue view` (161/163/165/166/168/181 state),
  `git worktree list` + the sibling branches' `git log` (which half of the
  judgement existed where), and `ls ~/work` (the absent corpora). Re-measured
  after rebasing onto `master` `1029377`, where #165/#166/#167 had merged and
  `§B1` had retired B1 — the two premises that changed are §PAS-0 items 3 and 5.
- Changed by this branch: `docs/OPTIONS-MEASURED.md` (this section and the header
  table), `README.md` (its index row).

## MRO — who owns the modes judgement: the shared layer, or each adapter (issue #164)

**Decided 2026-11-01: option C — the engine judges, the frontend performs the
switch.** The maintainer's words, verbatim:

> "fcitx5의 제약이 있으면, engine은 판정만 하고, 전환은 frontend가 할 수 밖에
> 없지 않니? 3자 이상이면 판단하자."

The same message rules R9's threshold — **three key inputs or more are judged**
(the ruling's wording was `3자`; **corrected 2026-11-01**, when the maintainer named
the unit: *"3. 키입력 세개로 명확히 하자."*), which is §MRO-4 item 2 — and asks the
fcitx5 ON/OFF surface to be built if this decision leaves no problem, which is
§MRO-4 item 1.

**Not settled by this, and named rather than dropped:** ~~the threshold's unit
(characters typed, syllables produced, or the Latin reading's length give three
different answers for `안녕`)~~ — **settled 2026-11-01: key inputs**, so `안녕` (6
keys) is judged and the guard is `JUDGE_MIN_KEYS 3`; whether fcitx5 must ship an
English input-method entry before its act can work at all (§MRO-4 item 1). ~~and the
symmetric direction of R8's formula, which §R8 already leaves open~~ — **settled
2026-11-01 (E1): both lines of the formula ship**, pinned by the 0.3 gate
(`docs/SPEC-MODES.md` §E1).

*As written, this section was measurements only with no option recommended. The
pricing below is left exactly as it was — the decision is quoted here rather than
folded back into the options, so what was weighed stays visible.*

**What shipped with the decision (same branch, 2026-11-01):** the formula and R9's
guard in `libhunmin/src/buffered_preedit.c#judge_boundary`, the answer in
`hunmin_buffered_preedit_take_judgement`, tier-A cases in `engine-wordset`, the
three frontends acting on it (TSF, Android, fcitx5 — the last of which needed the
English slot it had been passing as NULL), and `language_detect.{h,cpp}` retired
with its `engine-d1-language-detect` case. The fcitx5 settings row (§MRO-4 item 1's
surface) is #176, and the ON/OFF state is still not persisted by any frontend.

Produced 2026-11-01 on branch `scobyseo/modes-ruling-does-the-shared-layer-own-the-judge`,
base `a099f4f`, host Windows 11. This branch touches documentation only: nothing
under `libhunmin/`, `frontends/`, `tools/` or `tests/` is modified, so it cannot
collide with the sessions working this repo in parallel (`git worktree list` shows
the primary tree, this one, and `modes-ruling-e2-e3-e6-buffer-cap-short-word-thre`).

### MRO-0 The premise, checked first

The issue's own words, once: *"does the judgement go into the shared layer (where
the buffer, both layouts and the mode already live), with the fcitx5 detector
retired, or does the adapter stay the owner and each frontend repeats it?"* Its
stated premises were measured against the tree, in the order it states them.

| # | the issue's statement | measured | verdict |
|---|---|---|---|
| 1 | **B9 (2026-10-14) ruled R8 adaptor-owned**; "the owner is a design decision, not a requirement" | `docs/REMAINING-WORK.md` row B9 (`DECIDED 2026-10-14`), `wiki/decisions/2026-10-13-r8-mode-guessing-is-adaptor-owned.md`, and `docs/SPEC-MODES.md` §R3 in the same words | **verified** |
| 2 | **#128 put the state, both layout ids, the word buffer and the boundary into the shared layer**, with `HUNMIN_LANGUAGE_HANGUL/LATIN`, `..._toggle_language` and `HUNMIN_KEY_LANGUAGE_TOGGLE` | `libhunmin/include/hunmin.h` (the layer's construct takes `hangul_layout_id` + `latin_layout_id`; the language enum, the toggle and the key action are exported); `libhunmin/src/buffered_preedit.c`'s struct holds `hangul_mode`, both ids, `word`, `keys` | **verified** |
| 3 | "the fcitx5 detector replays the word on a shadow Latin context and returns text with **no validity test at all** (only a `minLen` guard)" | `frontends/fcitx5/src/language_detect.cpp#LanguageDetector::judge` is `minLen` + replay, nothing else; `grep -rn "english_valid\|hangul_valid" frontends/` returns no line | **verified** |
| 4 | "its single call site discards the result" | `frontends/fcitx5/src/hunminadapter.cpp#HunminState::keyEvent` calls `switcher_.judge(*engine_->config().latinLayout);` and uses nothing it returns | **verified** |
| 5 | "`hunmin_buffered_preedit_toggle_language` has **no caller in any frontend** (issue #136)" | `frontends/tsf/src/tsf.cpp#CTextService::ToggleModeCore` and `frontends/android/app/src/main/cpp/hunmin-jni.c` (`nativeLayerToggleLanguage`, called from `HunminAdapter.toggleLanguage`, used by `HunminInputMethodService`'s HANGUL_ENG action) both call it. fcitx5 has zero call sites, **by design and already recorded** (`hunminadapter.cpp`'s language comment, `docs/SPEC-FRONTEND.md` §FE13, `docs/REQ-MAP.md` FE7: "implemented on the two frontends that own a key (TSF, Android); fcitx5 is FE13's case") | **corrected** — two of the three frontends call it, so #136's zero-call-site measurement is stale as written (the board row it cites has since been re-pointed) |
| 6 | "nothing user-visible depends on it (issue #132)" | the adapter's *answer* is discarded, so that half is verified; but see N1 and N3 — in fcitx5 the feature cannot even be turned ON | **verified for the answer, corrected for the feature** |

**Premises the issue does not state, and the tree forces on every option:**

**N1 — fcitx5 builds its layer with one language.** `hunminadapter.cpp` passes
`nullptr` as the layer's Latin id, with a comment saying so ("the layer's toggle
reports 'not ours' (NULL latin layout)"). In fcitx5 the layer's `english_valid`,
its Latin reading and its toggle therefore all answer "nothing"; the only live
judgement there is the adapter's discarded one. TSF and Android both pass a
Latin id (`tsf.cpp`, `hunmin-jni.c`).

**N2 — the replay mechanism already lives in the core.** `latin_reading()` in
`libhunmin/src/buffered_preedit.c` (27 lines) rebuilds the finished word's Latin
reading by replaying the logged `(base, shift)` pairs through the pair's Latin
layout, at the boundary, and `hunmin_buffered_preedit_latin_text` hands it out.
`libhunmin/tests/wordset-test.cpp` pins it (`TheLatinReadingIsTheSameKeysOnTheLatinLayout`,
`ShiftSurvivesTheReplay`), and its own comment says the mechanism "lived in the
fcitx5 adapter alone until this moved into the shared layer (issue #164 asks who
owns the JUDGEMENT; the reading is what it reads)". So the question is no longer
"does the layer have the material" — it has it, tested; the adapter now carries a
second copy of the same replay (64 lines in `language_detect.cpp`).

**N3 — the ON/OFF state is unreachable in fcitx5.** `grep -rn "special" frontends/fcitx5/src/`
returns no line, and `config.h` has three options, none of them the autoswitch
state; the state exists in the engine (`hunmin_ic_set_option("autoswitch")`,
`HUNMIN_SPECIAL_AUTOSWITCH`) and reachable on TSF (`keydispatch.cpp`) and Android
(`HunminAdapter.special`). Whichever component owns the judgement, fcitx5 gets no
behaviour from it until a settings row or a chord exists there.

**N4 — one quoted document is stale about the state, not about the ruling.**
`docs/SPEC-MODES.md` §R3 says "The current implementation is an *adaptor*: …
keeps a shadow `hunmin_ctx` on the Latin layout and replays the buffered word".
That described the tree before #167 moved the same mechanism into the core; the
sentence is a design note, so a dated state correction is recorded under §R3
rather than a rewrite (see "Changed by this branch" in §MRO-5). Its requirement
half — one rule, one owner, identical on every frontend — is untouched and is
what the options below are measured against.

### MRO-1 The unpacked subject — "the judgement" is six pieces, in different homes

Reading the issue's phrase as one thing is how the comparison goes wrong: R8's
formula is three lines, and the pieces around it are where the cost is.

| piece | where it lives at this base | symbol, and what pins it |
|---|---|---|
| the **reading** (the keys replayed on the Latin layout) | **twice**: core `latin_reading()` (from the layer's `keys` log, at the boundary) and adapter `LanguageDetector::judge` (from its own `record()` buffer) | `hunmin_buffered_preedit_latin_text` / `LanguageDetector::judge`; `engine-wordset`, `engine-d1-language-detect` |
| **english_valid** | core | `hunmin_buffered_preedit_english_valid` + `..._load_english_shipped` + the user dictionary; `engine-wordset` |
| **hangul_valid** | core | `hunmin_hangul_valid`, `hunmin_hangul_valid_old`, and the two layout answers `hunmin_ctx_layout_autoswitch_capable` / `..._old_hangul`; `engine-hangul-valid` |
| the **formula** (R8's three lines) | **nowhere** | `#168`; `boundary()` still delivers in the current mode unconditionally (`buffered_preedit.c`, 14 lines) |
| the **act** (boundary + switch + the other layout's ic, with the word delivered in the judged language) | TSF and Android: the layer. fcitx5: the host | `hunmin_buffered_preedit_toggle_language`; fcitx5's slot is the profile's per-item `Layout=` read through `layoutFor()` at `Activate` (§FE13) |
| the **ON/OFF state** | core (the ic option, carried across `attach_ic`) | `hunmin_ic_set_option("autoswitch")`; no fcitx5 surface (N3) |

So "the owner" is not one decision about one function. It is four: who produces
the reading (one copy or two), who holds the formula, who can *act* on an answer
(the layer on two frontends, the host on fcitx5), and who owns the state. The
options differ on the four in different ways, which is why they are priced
separately below rather than ranked.

| frontend | layer's languages | today's judgement | today's switch owner | mode display | autoswitch reachable |
|---|---|---|---|---|---|
| fcitx5 | **one** (`nullptr` Latin id) | the adapter's, answer discarded | the host (per-IM `Layout=` in the profile) | none | **no** |
| TSF | two | **none** | the layer (`ToggleModeCore`) | langbar + conversion compartment, mirrored from `..._language` after a toggle | yes |
| Android | two | **none** | the layer (`toggleLanguage()` → `HANGUL_ENG`) | picker + toast, mirrored from `..._language` after a toggle | yes |

Two consequences a reader can check in the table: the feature is unimplemented
everywhere (no frontend delivers a mode from a judgement), and the split runs
opposite to intuition at this base — the two frontends that own a Han/Eng key are
exactly the two that hand the switch to the layer, while the one that owns no key
(fcitx5) is the one carrying its own judgement code. The
indicator column matters for all three options: TSF and Android update their
display only on a toggle key today, so an automatic switch at a boundary would
move the layer's mode without moving the indicator — two small reads per
frontend, not zero.

### MRO-2 The options

#### A — the shared layer owns the formula and performs the switch

The shape #168 already specifies: at the boundary, when `autoswitch` is ON,
compute the validity pair over the reading and the buffer, apply R8's three
lines, and on a switch take the existing switch path — the word is delivered in
the judged language and the mode change applies from the next key (R10).

- **What changes**: `latin_reading()`'s neighbours in `boundary()`; fcitx5 passes
  `latinLayout.c_str()` at its `hunmin_buffered_preedit_new` call and deletes
  `language_detect.{h,cpp}`; TSF and Android re-read `..._language` after a key so
  the indicator follows.
- **ABI**: none. The formula runs inside `process_key`, `out->commit` already
  carries the delivered word, and `..._language` already reports the mode. No new
  symbol, so `libhunmin/tests/abi/symbols.txt` does not move.
- **Cost**: the core grows one formula function (~40 lines) plus tier-A cases per
  formula line including both-valid and both-invalid; fcitx5 sheds
  `language_detect.cpp` (64 lines), `language_detect.h` (94) and a 206-line test
  with its `engine-d1-language-detect` registration; TSF and Android a few lines
  each. Documentation: six `docs/REQ-MAP.md` rows name the retired code or the
  retired test (R2, R3, R7, R8, R9 and the parent A9, whose architecture and
  implementation cells both name the class), and `tools/req-map-check.py`
  fails on a `verified by` id that is not registered — so retiring the test and
  re-pointing the rows is one atomic change, not two.
- **Gain**: one implementation of R3's "two frontends must never answer
  differently"; the material the formula reads is already in the layer and already
  tested; TSF and Android get the feature without new per-frontend judgement code;
  and fcitx5 needs **no** host-level switch action, because the layer's switch is
  an `attach_ic` inside it.
- **What would change the answer**: evidence that fcitx5's mode must stay the
  host's (§FE13's reading). Under A, fcitx5's layer can be in Latin mode while the
  host's input method still says Hangul — the host's IM entry carries no state the
  layer writes. Whether that divergence is acceptable is a ruling, not a
  measurement; the seam that would remove it is priced in §MRO-4 item 1.

#### B — the adapter stays the owner, each frontend repeats it

B9 taken literally: one judgement rule, one owner per frontend.

- **What changes**: the fcitx5 detector gains the two validities; TSF and Android
  each grow a detector (neither has one, and neither has a test for one).
- **ABI**: none.
- **Cost**: three formulas. The reading is already one copy in the core and the
  layer hands it out — so the honest form of B is "three formulas over one
  reading", and a frontend that keeps its own reading (as fcitx5 does today) pays
  a second replay per finished word (the adapter's own header measures one
  judgement at 3.48 µs). The three adapters measure 683, 646 and 562 lines
  (`hunminadapter.cpp`, `tsf.cpp`, `hunmin-jni.c`), so the two that would grow a
  detector are the two nearest the ~700-line guidance for a frontend, and one of
  them is the JNI file that also carries the candidate and settings bridges.
- **Gain**: B9 stands untouched, no `REQ-MAP` row moves, and fcitx5 keeps choosing
  the Latin layout it judges on through its own config rather than the layer's
  constructor argument.
- **What would change the answer**: a measurement that the three frontends judge
  the same word differently in practice. R3 forbids it as a requirement, so B
  would need a *shared* test for the three copies to prove equality — a shared
  home for the cases, which is most of what the ownership question was about.
- **Already visible under B**: the two copies that exist today do not agree, and
  neither is the R8-정정 formula. The adapter enforces `minLen` (default 2, a
  value R9's threshold issue has not fixed); the core applies no length guard at
  all. Shift survives both, by different means (the adapter replays a
  case-encoded `A`–`Z` character through `hunmin_ic_process`; the core logs
  `(base, shift)` and replays through `hunmin_ic_process_key`). Neither consults
  `english_valid` or `hangul_valid`. That is the shape of the divergence R3 exists
  to prevent, measured before either copy has been adopted.

#### C — the layer owns the formula, the frontend performs the act ← **selected 2026-11-01**

The split the 2026-10-31 special-function ruling already uses: the engine resolves,
the frontend performs what only it can perform.

- **What changes**: one append-only query answering "the finished word is judged
  to be <language> / keep the current mode"; TSF and Android act with
  `hunmin_buffered_preedit_toggle_language` (already implemented and called);
  fcitx5 acts through its host.
- **ABI**: one new symbol. Not a wider `hunmin_key_result`: callers allocate that
  struct on their own stack (`hunmin_key_result out{}` in all three frontends), and
  the ABI guard compares symbol names only (`tools/abi-check.sh` against
  `symbols.txt`), so a grown struct would break the contract without failing the
  guard.
- **Cost**: the query plus tier-A cases; three act paths instead of one; and on
  fcitx5 the act needs a mode in the *host* to switch to. Measured: `Instance`
  exposes `setCurrentInputMethod(const std::string &imName)` and a per-input-context
  overload (`/usr/include/Fcitx5/Core/fcitx/instance.h:381-397`, read on the WSL
  side — no fcitx5 build exists on this host, so this is **read, not executed**),
  and what it sets is an **input method**, not a layout: the addon registers two
  IM entries (한 / 로) and neither carries `Layout=`; the layout arrives from the
  profile's per-item override. So C's fcitx5 act reaches an English mode only if
  an IM entry with a Latin layout exists in the user's group — today that is a user
  configuration, not a shipped one.
- **Gain**: §FE13 stays honest on fcitx5 (the host's language remains the host's),
  and the act stays where the platform knows how to perform it — the same rule the
  special-function ruling fixed.
- **What would change the answer**: a ruling that §FE13 already answers "the host"
  for fcitx5 — then C is the only shape consistent with it, and A needs the seam
  in §MRO-4 item 1.

### MRO-3 Summary — each option against each surface

Counts are file- and row-counts from the tree at the base named above, not
estimates. Behaviour is net-new in every column: no frontend delivers a mode from
a judgement today (§MRO-1).

| surface | A — layer owns both | B — adapter owns | C — layer judges, frontend acts |
|---|---|---|---|
| ABI symbols | none | none | one, and not a struct field |
| data files | none (`en-US.json` shipped) | none | none |
| core code | one formula function, `~40` lines | none | the formula + a query |
| tier-A | a case per formula line, incl. both-valid / both-invalid | none in the core | the formula's cases + the query's |
| tier-D1 | `engine-d1-language-detect` retires | two suites added (TSF, Android) | the D1 suite retires |
| fcitx5 | −`language_detect.cpp`/`.h`, +1 constructor argument | +the two validities in its detector | +an act path through the host |
| TSF / Android | a few lines each (indicator) | ~one detector each, plus tests | none |
| `docs/REQ-MAP.md` rows to re-point | 6 (R2, R3, R7, R8, R9, A9) | 0 | 6 |
| where `language_detect` is named in prose | rewritten, 19 lines across 10 files | unchanged | rewritten, 19 lines across 10 files |
| fcitx5 can show the mode | no (the layer has no display) | no | n/a — it is the host's mode |

### MRO-4 Facts the question does not state, and every option pays

1. **The fcitx5 mode seam.** Under A and C, fcitx5's mode either lives in the
   layer and diverges from the host's IM entry, or has to be pushed to the host.
   The push mechanism exists (`setCurrentInputMethod`, read above) and costs an
   English IM entry that ships with a Latin `Layout=`, which the addon does not
   have today. B is the option that never faces this, because it never switches
   anything in fcitx5 — which is also why B leaves fcitx5's own answer discarded
   unless the wiring is added.
2. **R9's threshold is still open** (`#163`). The adapter hardcodes `minLen = 2`;
   whichever component owns the formula needs the value, and the value is a
   ruling, not a measurement this paper can supply.
3. **The reading needs a pair.** `hunmin_buffered_preedit_latin_text` answers
   empty for a one-language layer (fail-closed, B3). A and C therefore require
   fcitx5 to pass a Latin id; B does not, because it keeps its own shadow context.
4. **Documentation is atomic with the code.** `language_detect` is named on 19
   lines across 10 files, and `engine-d1-language-detect` is the `verified by`
   cell of five `REQ-MAP` rows, and `A9` names the class in both its architecture
   and its implementation cells while its `verified by` cell already says `none`.
   `req-map-check`
   fails on an unregistered id in that cell, so a retirement that leaves the rows
   behind is a red run, not a stale sentence.
5. **The OFF path must be byte-identical** (#168's third checklist item). A
   changes the boundary function, which every key every frontend processes; the
   other two options do not touch it. That is A's one regression surface and the
   reason its verification is a whole-tier run plus an identity assertion, not the
   formula's own cases alone.

### MRO-5 Provenance

- Host: Windows 11, base `a099f4f`, branch as named in the header. `git worktree
  list` used to establish the parallel-session scope; `gh issue view` for
  132/136/164/166/168 to check which half of the judgement had shipped where.
- Every count here is re-derivable: `wc -l frontends/fcitx5/src/language_detect.{h,cpp}`
  and `frontends/fcitx5/test/testlanguagedetect.cpp`; `grep -rn "language_detect\|LanguageDetector"
  --include=*.md docs README.md PLAN.md` → 19 lines over 10 files;
  `grep -n "language_detect\|engine-d1-language-detect" docs/REQ-MAP.md` → the six
  rows named in §MRO-3; `grep -rn "toggle_language" frontends/` → two call sites
  (TSF's `ToggleModeCore`, the Android JNI entry point), with the engine's own
  cases in `buffered-preedit-test.cpp` and `key-rule-test.cpp`.
- **Read, not executed**: `Instance::setCurrentInputMethod` (fcitx5 headers on the
  WSL side; there is no fcitx5 build on this Windows host, so nothing in this
  section was compiled or run against fcitx5). The 3.48 µs per-judgement figure is
  quoted from `language_detect.h`'s own measurement, not re-timed here.
- Nothing in this section needed a build: no C was compiled and no binary was run.
  The facts are file contents, symbol names, and `grep`/`wc` counts, each named
  above.
- Changed by this branch: `docs/OPTIONS-MEASURED.md` (this section, its header row,
  the section-key sentence, the H1's count), `README.md` (its index row), and a
  dated state note under `docs/SPEC-MODES.md` §R3 (N4) — a factual correction, not
  a ruling.

## BND — the judgement boundary's three parameters: buffer cap (E2), short-word threshold (E3), flicker (E6) (issue #163)

**As written, this section was measurements only: no option recommended, no
value chosen** (`AGENTS.md`, "never decide on your own"). **E2 was ruled on
2026-11-01 and is implemented on this branch** — the decision is at the end of
this preamble and §BND-2 carries the post-ruling measurement beside the
pre-ruling one. **E3 and E6 remain open**: nothing below chooses an N or moves a
judgement point.

Issue #163 asks for one ruling covering three evaluable parameters of
`docs/SPEC-MODES.md` §2 — how far the composition buffer may grow (E2), what
counts as a short word that is not switched (E3), and whether judgement ever
moves inside the word (E6). Each is priced below against what the tree actually
does today, because two of the three premises the issue states do not survive
being measured (§BND-0).

Produced 2026-11-01 on branch
`scobyseo/modes-ruling-e2-e3-e6-buffer-cap-short-word-thre` (base `master` at
`a099f4f`), Windows 11. The engine was built in this worktree's `build-mingw/`
with the llvm-mingw toolchain and driven through `tools/modes-boundary-probe.py`,
which this branch commits so that every number below is re-derivable (§BND-6).
Deliberately **not** touched: `docs/REMAINING-WORK.md` and `AGENTS.md`, which
parallel sessions patch.

The maintainer's steer, verbatim, given mid-session on 2026-11-01 while this
paper was being written:

> "buffer cap should be reasonable; at least longest english word should be
> covered by it. i think 4KB buffer can be managable; if it is to be overflowed,
> we can flush first half of the buffer."

That is the shape §BND-2 measures E2 against. It says nothing about E3 or E6, and
it left three things open that §BND-2's last table named rather than settled.

**E2 — decided 2026-11-01 (option B), and the value is not the one first
proposed.** Asked whether the cap should shrink to 256 B, the maintainer's
answer was *"make cap 256B by default, and make it configurable."* So: the
layer's word cap is **256 bytes of UTF-8 by default**, an overflow commits the
word's first half, and the value is a layer option (`word_cap`, bytes, `0` = no
cap) rather than a constant. The three things that stays silent about — bytes or
characters (bytes, because an allocation is what is bounded), whether a flushed
half is judged (not decided; #168's call site, unreachable in practice), and how
the flush treats the Latin key log (dropped, fail-closed) — are recorded in
§BND-2 and in the code's own comments, not inferred here.

### BND-0 The premises, checked first

| # | what issue #163 states | measured in the tree | verdict |
|---|---|---|---|
| 1 | "R4 extends the buffer to the delimiter; the engine's `STR_CAP` is 64 ucschar (`libhunmin/src/ic.c`) and overflowing it truncates silently" | `STR_CAP` is not in `ic.c`: it is `hunmin_internal.h` (`#define STR_CAP 64 /* ucschar buffer capacity (libhangul parity) */`), and `libhunmin/src/ic.c` has no `STR_CAP` at all. It sizes `ucschar commit[STR_CAP], preedit[STR_CAP]` on a `hunmin_ic` | **corrected** — right constant, wrong file |
| 2 | it is "the effective ceiling on buffer extension" | it is not reached: the commit is cleared at the entry of every `hunmin_ic_process*` call, and a finished syllable is at most three characters, so the longest single read measured over a 30-syllable run is **1 character**. The buffer that actually grows is the shared layer's `word`, a heap string with no ceiling (§BND-2) | **corrected** — the ceiling is elsewhere, and it is a different buffer |
| 3 | "the engine's boundary code" is where a ruled cap would be cited | the shipped path's buffer is `hunmin_buffered_preedit`'s `word` (`libhunmin/src/buffered_preedit.c`, `word_append`), and all three frontends go through that object: `frontends/fcitx5/src/hunminadapter.cpp`, `frontends/tsf/src/keydispatch.cpp`, and Android's `HunminAdapter` through its `layerKey` call. A cap on `hunmin_ic`'s arrays would bound nothing a frontend sees | **corrected** — the issue's verification step names the wrong layer |
| 4 | "E3's numeric threshold was retired 2026-10-25 … R9 now says only that a threshold exists" | verified: R9 reads "**The numeric threshold is not a requirement (ruled 2026-10-25).** §2.E3 is retired … a design choice, made where the judgement is built and recorded there" | **verified**, and the method conflict is real: §2.E3 still prescribes "set N to 2, 3, 4 and measure … find the N at which (b) is minimal" |
| 5 | "judging once at the delimiter (R4) means the display never flips mid-word" | verified structurally: `boundary()` is the only place a word is completed, and `build_preedit()` renders `word` plus the in-flight syllable in the layer's current language only. Nothing in the object can show one language and then another while a word is open | **verified**, and now shipped: the judgement is `judge_boundary()` inside `boundary()` (#164, `f9f5992`, merged 2026-11-01), called once per finished word |

Two premises the issue does not state, and every option inherits them:

- **The threshold has no home in the tree, in any of the three senses "where"
  can mean.** No constant, no data key, no option carries it:
  `hunmin_ic_set_option`/`get_option` answer only `caps` and `autoswitch`, and
  `docs/SPEC-SCHEMA.md` has no length key. So "record it where the judgement is
  built" is literally #168's call site, and every other home — an option value, a
  schema key, a per-layout field — is new surface, not an existing one.
- **The two directions of R8 do not share a length semantics.** With
  `hangul_valid` structural, a Hangul buffer of any length that composes to whole
  syllables is Hangul-valid, so in the Hangul to English direction the threshold
  can only suppress a switch that the dictionary side alone decided. In the
  English to Hangul direction (E1) the same N forbids switching a Latin run whose
  Hangul reading is well-formed — a different population, counted in §BND-3.

### BND-1 What "length" is a length of

Three quantities travel under the word "buffer", and a cap or threshold that
names one of them bounds the other two only through a conversion:

| quantity | where it lives | how it grows |
|---|---|---|
| keys typed | the frontend, and the layer's `keys` log | one per event |
| characters in the buffer | `word` (finished syllables) plus the ic's in-flight syllable | 1 syllable per 2–3 keys on 두벌식 (`아` is 2, `안` is 3) |
| bytes | the same string as UTF-8 | 3 per modern syllable, 1 per ASCII digit (R6's digit runs) |

So **a threshold expressed in syllables is 2–3× a threshold expressed in
keystrokes**, and a cap in bytes is a third of the same cap in characters for
Hangul while equal to it for a digit run. A cap of N characters is therefore met
at 2N–3N keystrokes on 두벌식, and the exhaustive sweep of §BND-3 lands in the same
range from the other side: over 1–4 letter strings the rendering averages
0.75–1.0 characters per key. Those are letter strings, not Korean, which is why
the layout fact above is the one to quote.

The judgement boundary itself is one function with four callers, and that number
is what E6 turns on: `boundary()` — a declined key in
`hunmin_buffered_preedit_process_key`, plus `..._commit_word`, `..._external_text`
and the language toggle. Every one of them is a word ending.

### BND-2 E2 — where the buffer must end, measured

`tools/modes-boundary-probe.py buffer` drives the shared layer key by key with
`안` repeated (three keys per syllable). Measured **before** the ruling, when no cap
existed — the record of what the tree did, and why the question was open:

| syllables typed | bytes typed | longest preedit seen | commit at the boundary | lossless | over a 4 KiB cap |
|---|---|---|---|---|---|
| 1 | 3 | 3 | 3 | yes | no |
| 21 | 63 | 63 | 63 | yes | no |
| 22 | 66 | 66 | 66 | yes | no |
| 64 | 192 | 192 | 192 | yes | no |
| 200 | 600 | 600 | 600 | yes | no |
| 1,365 | 4,095 | 4,095 | 4,095 | yes | no |
| 1,366 | 4,098 | 4,098 | 4,098 | yes | **yes** |
| 2,000 | 6,000 | 6,000 | 6,000 | yes | **yes** |

**As implemented 2026-11-01 (option B: 256 B cap, flush the first half,
configurable).** The same probe against the same build, with the cap read from the
layer's own `word_cap` option:

| syllables typed | bytes typed | longest preedit seen | delivered bytes | mid-word commits | lossless | over the 256 B cap |
|---|---|---|---|---|---|---|
| 64 | 192 | 192 | 192 | 0 | yes | no |
| 85 | 255 | 255 | 255 | 0 | yes | no |
| 200 | 600 | **255** | 600 | 3 | yes | no |
| 1,365 | 4,095 | **255** | 4,095 | 31 | yes | no |
| 2,000 | 6,000 | **255** | 6,000 | 46 | yes | no |

Three facts that table establishes, each pinned by a test in
`libhunmin/tests/buffered-preedit-test.cpp`
(`TheCapCommitsTheFirstHalfAndLosesNothing`, `TheCapIsConfigurableAndZeroMeansNoCap`,
`EachFlushedHalfIsTheNextPrefixAndTheRemainderFlushesCleanly`): **the preedit never
exceeds the cap** (255 B is the largest multiple of three below 256, because the
in-flight syllable counts against the cap); **nothing is lost, duplicated or
reordered** (delivered equals typed at every length); **the cut is on a syllable
boundary** and no single commit exceeds the cap. The third test evaluates the split
piece by piece instead of in aggregate — each mid-word commit must be the exact next
prefix of what the engine produces, the held text must be the exact continuation,
and the explicit flush (`..._commit_word`, what a frontend calls on focus loss) must
deliver exactly the remainder and leave the layer empty and usable.

**Non-vacuity, and a defect it exposed.** Every cap test fails with `WORD_CAP` 0 and
passes at 256. The piece-by-piece test was also run against a deliberately broken
flush (`word_flush_half` skipping one syllable in the tail) — and **passed**, because
200 repetitions of `안` are one string of one repeated three-byte syllable, in which
a one-syllable shift is invisible. With the sequence varied (`dks rk dl tj gks` →
`안 가 이 서 핫`, 2 and 3 keys per syllable) the same mutation fails at the first
post-flush key. That is why the test is non-uniform, and it is the reason an
aggregate "the totals match" assertion is not enough for this code: it cannot see a
shift at all.

**What the cap is *not*, measured.** The cap bounds the layer's own buffer. It does
not carry the option that was proposed first (4 KiB, "at least the longest English
word") any further than 256 B already does: the longest form in the oracle is 24
characters / 24 bytes (`electroencephalography's`), the mean is 8.64, and **0 of
118,150 forms exceed 256 bytes** — a 10.7× margin, where 4 KiB was a 170.7× one.
What shrinks with the value is the length at which the overflow path becomes
reachable: 256 B is 85 syllables, so ~256 consecutive letter keys with no space and
no symbol, against ~4,096 for the first proposal. Nothing a person types reaches
either; a stuck key or a machine-generated line reaches 256.

**The 4 KiB pricing, kept because the arithmetic is what the 256 B value inherits:**

**The pre-ruling state, in words.** **There was no cap, and nothing truncated.** The 21- and 22-syllable rows are
there because 64 characters (the `STR_CAP` number) falls between them for a
one-character syllable: the boundary sails past it, which is the point. The word
buffer doubles its allocation (`reserve()`), the preedit a frontend is handed is
`word` plus the in-flight syllable, and the commit at the boundary is the whole
word.

**What the first steer (4 KiB) bought, measured against its own premise.** The longest form
in the shipped oracle is 24 characters / 24 bytes
(`electroencephalography's`); the longest that is layout keys only is 23
(`electroencephalographic`); the mean form is 8.64 characters and no form at all
exceeds the cap (0 of 118,150). 4,096 bytes is **1,365 syllables** and **170.7×**
the longest word the dictionary knows, so "at least the longest English word is
covered" holds with two orders of magnitude to spare. Two things the margin does
not cover, named rather than measured: a **user word**
(`..._add_user_word` takes any line) longer than 1,365 syllables, and a buffer
that never sees a delimiter — reaching 4 KiB needs **4,095 consecutive letter keys
with no space and no symbol**, which no delivered text does but a stuck key or a
replayed paste could.

**The cap and the shipped judgement interact in exactly one place, measured.** The
judgement is `boundary()`'s (#164), so a flush commutes a half **unjudged** — and
because the flush empties the reading's key log, the word that later reaches the
boundary is judged against the **remainder**: a 400-key word (`가` × 200) flushed
three times arrives at the boundary with a **61-key** reading, not 400. So the
remainder is treated as its own word, which is the only reading the layer still
holds; the alternative (halving the key log) would describe a word the buffer no
longer contains.

**What a flush at half does, and what it must also do.** Flushing the first half
at overflow (as implemented: 128 bytes of the 256, about 42 syllables) commits text
at a boundary that is not a delimiter, which R4 did not authorise — R4's sentence
was "the moment the next delimiter (space or symbol) appears", and its own
amendment clause ("in groups of at most N characters") is exactly that grant, which
is now `docs/SPEC-MODES.md` §R4-정정. Two shapes existed and they were not the same
ruling:

- **flush silently** — the first half is committed in the current language with no
  judgement and the surviving half continues, so a word longer than the cap is
  never judged as a whole and can never switch. The layer's Latin key log (`keys`)
  must then be shortened by the same half, or `latin_text` describes a word the
  buffer no longer holds — a divergence nothing would crash on, and every misfire
  after it would read like a judgement bug.
- **flush as a boundary** — the half is judged before it is committed, i.e. a
  judgement point inside a word. That is legal only as an explicit E6 exception, and
  it is where E2 and E6 stop being independent: nothing judges yet (#168), and the
  implementation chose the silent flush (§R4-정정), so E6 stays inert at 256 B and
  would go live only under option C or a small display cap (option D below).

**E2's own stated worry is not answered by any cap on memory.** §E2 quotes "if
the preedit becomes too long, most apps misbehave": that is a threshold of tens of
syllables, and 256 B is 85. The cap is a bound on the layer's buffer and on the
longest preedit a frontend can be handed; it is not a measurement of what an
application tolerates, and it does not bite at any length a person can actually
reach (no word is 85 syllables). Nothing on this host measures what an application
tolerates —
Fcitx5, TSF and Android are three different preedit carriers and none was driven
here — so the number that would settle that question is unmeasured, not small.

Options, as costs:

| option | cost | gain | what would change it |
|---|---|---|---|
| **A — no cap (the pre-ruling state, still reachable as `word_cap` 0)** | a pathological run grows the word unboundedly; no reachable failure measured | nothing to change | evidence that a stuck-key or replay run can reach megabytes |
| **B — cap, flush the first half ← selected 2026-11-01, at 256 B, as an option** | one R4 amendment's worth of prose (now §R4-정정); the flush drops the Latin key log rather than splitting it; a word over the cap is never judged whole (0 forms in the oracle at either value) | a bound on what a frontend must render, and a hard allocation bound | a word longer than 85 syllables that must be judged as a whole — a user-dictionary word nothing bounds; the implementation records the choice |
| **C — cap, judge at the cap** | as B plus a mid-word judgement, which R4 forbade and E6 was written to avoid | a flush that never commits unjudged text | a decision that an overflow is a boundary; it is only ever reached at ~42 syllables now, so this is the option that would make E6 live |
| **D — cap the displayed preedit at a small N (about 10 syllables)** | every word longer than N is committed and judged in pieces: R4's "extend to the delimiter" becomes false for ordinary long words, and E6's flicker becomes visible at N rather than theoretical | the only option that bounds what the application is asked to render, which is what §E2 names | a measurement of how long a preedit each of the three carriers tolerates — not available on this host |

### BND-3 E3 — what a threshold of N would be a threshold on

R9 requires that short words are not switched, and the number is now a design
choice. What a value chooses between is measured exhaustively: every `a`–`z`
string of length 1–4 is a thing a person can type, `english_valid` is the shipped
`en-US.json` membership test, and `hangul_valid` is the engine's own predicate over
that string's rendering through the 두벌식 layout.

| length (keys) | strings | both valid (R8 keeps the mode) | english only | hangul only (E1) | neither (keeps the mode) | **switches to English, after R9's guard** | **switches to Hangul, after R9's guard** | rendered chars per key |
|---|---|---|---|---|---|---|---|---|
| 1 | 26 | 0 | 26 | 0 | 0 | **0** | **0** | 1.000 |
| 2 | 676 | 80 (11.83%) | 268 (39.64%) | 88 (13.02%) | 240 (35.50%) | **0** | **0** | 0.862 |
| 3 | 17,576 | 212 (1.21%) | 1,066 (6.07%) | 2,238 (12.73%) | 14,060 (80.00%) | **1,066 (6.07%)** | **2,238 (12.73%)** | 0.775 |
| 4 | 456,976 | 230 (0.05%) | 3,239 (0.71%) | 31,214 (6.83%) | 422,293 (92.41%) | **3,239 (0.71%)** | **31,214 (6.83%)** | 0.745 |

**The value is no longer chosen here: R9 records the maintainer's "3자 이상이면
판단하자" (§MRO, ruled 2026-11-01) and its unit, named the same day — "키입력 세개로
명확히 하자" — so `libhunmin/src/buffered_preedit.c` carries it as `JUDGE_MIN_KEYS 3`,
three **key inputs**.** The table below is what that N partitions, in the same unit,
so the `1` row is what N=2 removes and the `3` row is what ships at N=3:

Read plainly — these are the facts a threshold N is a choice between:

- **At N = 1 and N = 2 nothing switches at all — because of R9's guard, not because
  of the validities.** All 26 one-letter buffers are English words and none renders
  as a syllable, and 268 of the 676 two-letter strings are English-only: without the
  guard those would switch. The shipped `JUDGE_MIN_KEYS 3` is what removes both rows,
  so the first switchable length is 3 and the guard — not the validity pair — is the
  reason the two shortest rows are empty. (An earlier draft of this section read the
  unguarded columns as the behaviour; the two right-hand columns are the correction.)
- **The two directions have different sizes at every length.** A 3-letter buffer
  that switches to English is 6.07% of that space; one that switches to Hangul is
  12.73%. A single N applies to both, so its cost is not symmetric — and the
  hangul-only column is conditional on E1, the symmetric rule, which is still
  unmeasured.
- **The unit is key inputs, clarified 2026-11-01, and that is what the table counts.**
  The ruling's wording was `3자` (three characters); asked which unit the requirement
  should name, the maintainer settled it — *"3. 키입력 세개로 명확히 하자."* — which is
  what `JUDGE_MIN_KEYS 3` already implemented. The gap the question was about is
  §BND-1's 2–3× conversion: `안녕` is 6 keys and 2 characters, so a key rule judges
  it on the sixth key and a character rule would never judge it. Every row below is
  in keys typed, so the `3` row is the class the shipped guard admits.
- **The threshold's price on the English side, in the oracle's own terms:** forms
  at or below N are 26 (N=1), 374 (N=2), 1,655 (N=3), 5,268 (N=4) — 0.02%, 0.32%,
  1.40%, 4.46% of 118,150. Those forms could never switch automatically while the
  threshold holds. That is a count of vocabulary, not of typing: nothing in the
  shipped data says how often any form is typed.

Where it could live, and what each home costs:

| home | cost today |
|---|---|
| a constant in the judgement's own rule (`boundary()`) — **what shipped**: `JUDGE_MIN_KEYS 3` above `judge_boundary()` | none — the constant arrived with the judgement's first implementation (#164), so R9's "recorded where the judgement is built" is met as written |
| an `hunmin_ic_set_option` value | the option namespace has exactly `caps` and `autoswitch`; a third name is an ABI-compatible addition, but it makes a requirement a runtime preference, which nothing has asked for |
| a `docs/SPEC-SCHEMA.md` key | the schema has no length key at all; a new section plus `tools/doctor.py` validation, for a value that is a design parameter, not data |
| a per-layout field | the one thing `AGENTS.md` forbids by name ("no per-layout anything in the core") |

What a measured N would need, and why no N is offered here: §2.E3's method — "set
N to 2, 3, 4, count misfires and manual toggles, take the N that minimises the
toggles" — needs a corpus of typed Korean with real frequencies. No such corpus
is on this host (`ls ~/work` returns `edw` only) and `docs/MODES-METRICS.md` §3
voids numbers from a corpus built in session, so no corpus was built. The sweep
above is the space a threshold partitions; the weight of each cell in it is the
missing input.

### BND-4 E6 — what moving judgement inside the word would cost

**Ruled 2026-11-01: both timings ship, per-key is the default** — *"옵션처리하자.
설정할 수 있게. default는 B로."* So this is no longer a choice between two futures:
`judge_timing` selects the display (1 = per key, the default; 0 = boundary only) and
`docs/SPEC-MODES.md` §E6 carries the ruling. What follows is the price of the default,
and it is the 도깨비 the item describes.

The boundary timing's cost is zero by construction: `judge_boundary()` is called
once, in `boundary()`, over the finished word (#164), and the display is `word` +
the in-flight syllable in one language (§BND-0 row 5). The price of the alternative
is countable over the same oracle: judge at every prefix, and count how often the
judged language **changes more than once** inside one form — a flip away from the
current mode and back.

**R9's three-key guard is part of the count.** A buffer under
`JUDGE_MIN_KEYS` key inputs is judged by nothing, so the first two keys of a word can
never flip, and the pre-guard figures (measured before the guard existed in this
sweep) are larger than what ships:

| | without the guard | **with the guard (shipped)** |
|---|---|---|
| forms that flip at least once | 82,408 (96.1%) | **83,746 (97.7%)** |
| forms that flip **back** inside the word | 80,730 (94.18%) | **54,903 (64.05%)** |
| mean changes per form | 2.95 | **2.59** |
| deepest | `alphabetically`, 12 | **`alphabetically`, 11** |

| | forms | share of 85,718 |
|---|---|---|
| never changes (the whole word stays Hangul) | 3,310 | 3.86% |
| one flip (the switch R8 intends) | 1,678 | 1.96% |
| **flips back at least once (flicker)** | **80,730** | **94.18%** |
| mean flips per form, all forms | 2.95 | — |
| deepest | `alphabetically`, 12 | — |

Distribution of flips, with the guard: 0 → 1,972 forms, 1 → 28,843, 2 → 253,
3 → 41,526, 5 → 11,804, 7 → 1,244, 9 → 72, 11 → 4. The odd values dominate because
the first flip is the intended switch and every further pair is a flip back: a prefix
that is a dictionary word arrives, then a prefix that is not (`algebraicall` before
`algebraically`), then it recovers.

Two limits on that number, stated rather than hidden: it is over dictionary forms
typed in Hangul mode, not over what people type, and it counts the Hangul to
English direction only — the English to Hangul direction's display is not
implemented (E1). What it establishes is the order of magnitude: per-key judgement
does not flicker on an edge case, it flickers on almost every English word.

**What the maintainer's own mixed sentence would need — measured, and it is not
E6.** `English는 좋은 Language입니다` typed in one stream through the shipped build
(`autoswitch` on, shipped dictionary, `judge_timing` at its default):

| word | keys | preedit while typing | delivered at the space | judgement | mode after |
|---|---|---|---|---|---|
| `English` | `english` (+Shift) | `ㄸ 뚜 뚷 뚜히 뚜히ㅑ 뚜히ㅑㄴ` then **`English`** | `English` | switch to Latin | **Latin** |
| `는` | `sms` | `s sm sms` | `sms` | none | Latin |
| `좋은` | `whdms` | `whdms` | `whdms` | none | Latin |
| `Language` | `language` (+Shift) | Latin | `Language` | none | Latin |
| `입니다` | `dlqkTek` | Latin | `dlqkTek` | none | Latin |

At the time of that measurement the first word worked, including its
capitalisation, and everything after it came out as raw Latin: `judge_boundary()`
only fired in Hangul mode, so the return direction (English → Hangul) did not exist.
**That was E1, not E6, and it was ruled the same day** — *"E1도 당연히 지원해야지.
내 예제가 동작하는 것까지를 0.3 release gate로 보자."* The symmetric line now ships
(`hangul_reading()` and the Latin branch of `judge_boundary()`), so the table above
describes the tree before that ruling; the same sentence now delivers
`English 는 좋은 Language 입니다` and is the 0.3 gate,
`WordSetTest.TheZeroThreeGateMixedSentenceWorksInOneStream`. E6 still decides only
what the person sees *while* one word is typed.

**The coupling, stated once.** E2 and E6 are not independent choices, and which E2
option is picked decides whether E6 is live at all: with A, B or C the flicker
cannot occur at any reachable length, because the boundary is the only judgement
point and the one exception needs 682 syllables; with D it occurs at the cap, for
words longer than the cap, and the 94.18% above becomes the size of the problem
rather than a hypothetical. §E6's own sentence — "the two options are mutually
exclusive" — is true of D and false of A, B and C, which is a sharper statement
than the item currently makes.

### BND-5 The three items against each surface

| item | options priced | ABI | data / schema | tier-A goldens | engine code | gradeable today against |
|---|---|---|---|---|---|---|
| E2 | A, B, C, D (§BND-2) — **B selected, shipped** | none — the cap is a name-value option (`word_cap`), not a symbol | none | `engine-buffered-preedit`'s two cap cases | `word_flush_half` / `word_cap` in `buffered_preedit.c` | `tools/modes-boundary-probe.py buffer` — the pre-ruling and post-ruling rows, and both tests fail with `WORD_CAP` 0 |
| E3 | a constant (shipped), an option, a schema key, a per-layout field (§BND-3) | an option name would be additive | schema work only for the schema option | a golden per direction per N | `JUDGE_MIN_KEYS` and `judge_boundary()` in `buffered_preedit.c` | the sweep's four columns; the shipped guard's own cases in `engine-wordset`; **no** corpus for the weights |
| E6 | both, as an option — **per-key default, shipped** | none — `judge_timing` is a name-value option | none | `TheJudgementTimingChangesTheDisplayAndNeverTheText`, `PerKeyTimingFlickersOnAnEnglishWordAndBoundaryTimingDoesNot` | `judged_display` / `build_preedit` in `buffered_preedit.c` | §BND-4's flip counts, guarded and unguarded, and the tier-A equality between the two timings; felt experience: unmeasured |

No option changes the ABI, and none needs new data. That is the summary of the
whole section: **all three items are parameters of one function's behaviour, and
the expensive option in each case re-opens a requirement — R4's "extend to the
delimiter" for D in E2, R9's existence of a threshold for "no threshold" in E3,
R4's single judgement point for per-key in E6 — rather than a mechanism.**

### BND-7 R6 — where a digit run actually ends (issue #171)

Added after the three items above, on the same object: R6 (`docs/SPEC-MODES.md`
§R6) says the ON buffer holds **digit runs** as well as eojeols, which is a
statement about this layer's `word`. Measured 2026-11-01 on Linux with the built
tree (`python3 tools/modes-boundary-probe.py digits`); "engine" is the raw
`hunmin_ic` driven key by key, "layer" is `hunmin_buffered_preedit` with
`HUNMIN_COMMIT_AT_BOUNDARY` and one declined key (space) as the boundary.

| layout | keys | what the raw engine does | what the layer holds after the last key | early deliveries | boundary commit | lossless |
|---|---|---|---|---|---|---|
| `sin3-p2` | `1234567890` | commits a digit **per key** (10 commits, preedit always empty) | `1234567890` | 0 | `1234567890` | yes |
| `sin3-p2` | `dks123` | `ㅎ계` then `1`,`2`,`3` as separate commits | `ㅎ계123` | 0 | `ㅎ계123` | yes |
| `sin3-p2` | `jk1` | tier 101 entered by `k`, then `1` → `µ` | `ㅇµ` | 0 | `ㅇµ` | yes |
| `sin3-p2` | `jl1` | tier 102, `1` → `①` | `ㅇ①` | 0 | `ㅇ①` | yes |
| `sin3-p2` | `j;1` | tier 103, `1` → `⑪` | `ㅇ⑪` | 0 | `ㅇ⑪` | yes |
| `sebeolsik-final` | `1234567890` | the digit keys are jamo (`1`=ᇂ …, `0`=ᄏ): a syllable per commit | `ㅎㅆㅂㅛㅠㅑㅖㅢㅜㅋ` | 0 | same | yes |
| `sebeolsik-final` | `dks123` | `ㅣ`, `ᄀᅠᆭ`, `ㅆ` | `ㅣᄀᅠᆭㅆㅂ` | 0 | same | yes |

What the table settles, and the four observations it added to R6's grounds:

- **R6 holds in buffered mode, on the layout it names.** `sin3-p2`'s digit keys
  are the case the requirement is about — they resolve to literals and the
  ENGINE commits each one immediately, so without the layer's word the second
  digit is typed into a buffer that has already handed the first to the client.
  With the layer the run accumulates in the preedit and leaves once, whole, at
  the boundary. Zero early deliveries in every row.
- **`sebeolsik-final` (3f) is not a literal-digit layout.** Its digit keys are
  jamo, so "a digit run" there is a run of jamo and the engine's per-syllable
  commit applies exactly as it does to letters. R6's Korean text
  (*"세벌식에서는 숫자열까지 사용하기 때문에"*) is about the number ROW being part
  of composition, and 3f spends that row on jamo; the layout where a run of
  literal digits exists is `sin3-p2`. **The issue's verification command names
  `sebeolsik-3-final`, which is not a layout id** — the shipped ids are
  `sebeolsik-final` (alias `3f`), `sebeolsik-3-2` and `sebeolsik-3-90`.
- **The symbol tier resolves only the first digit of a run.** `T` is single-shot
  (`ic.c`, `ic->T = 0; /* tier is single-shot */`), so `jk123` is `µ`,`2`,`3`:
  the tier applies to `1` and the run continues as plain digits.
- **The spec's tier examples are the shift row.** §R6 says `1` becomes `Ⅰ` at 101,
  `₁` at 102, `¹` at 103; measured, those three are `shift+1`, and plain `1` at
  those tiers is `µ`, `①`, `⑪`. §R6's wording was then replaced on the
  maintainer's instruction the same day (*"R6문구는 적절히 변경하자"*), and the new
  sentence states exactly this: the shift row is the shift row, and the tier is
  single-shot.
- **The only early delivery is R4-정정's cap, and it is counted in bytes.** With
  `word_cap` 4 on `sin3-p2`, digits `1`…`4` are held and the fifth key flushes
  `12`; a digit costs 1 byte where a syllable costs 3, so a digit run reaches the
  default 256 B at 256 keys rather than 85. It is the same rule, not a second one,
  and `engine-buffered-preedit` now pins it.

**Not a property of the layout, a property of the policy.** Under
`HUNMIN_COMMIT_PER_SYLLABLE` the same `sin3-p2` digit run is delivered one digit
per key (measured: commits `1`, `2`, `3`) — R6's guarantee is what the BUFFERED
policy buys, and all three frontends default to it
(`frontends/fcitx5/src/hunminadapter.cpp`, `frontends/tsf/src/tsf.cpp`,
`frontends/android/app/src/main/cpp/hunmin-jni.c`), with `syllable` a per-user
option. Whether "ON" must therefore pin the policy is R13's state and is not
decided here.

### BND-6 Provenance

- **The probe is committed**, because every number above is load-bearing:
  `tools/modes-boundary-probe.py`, added by this branch. It loads the built
  library through ctypes and uses the engine as the oracle — rendering,
  composition and `hangul_valid` are the engine's own functions; only dictionary
  membership is Python's, over the same `libhunmin/data/modes/en-US.json` the layer
  loads. Reproduce with:
  ```sh
  cmake -S . -B build-mingw -G Ninja -DCMAKE_C_COMPILER=C:/llvm-mingw-20261006-msvcrt-x86_64/bin/gcc.exe
  cmake --build build-mingw
  python3 tools/modes-boundary-probe.py all --max-len 4
  ```
  Timings on this host: `space` 0.4 s at `--max-len 3` and 3.7 s at 4; `prefix`
  2.6 s; `buffer` 0.2 s. Length 5 was not swept: it is 11.9 million strings and
  adds nothing the 1–4 rows do not already say.
- **Measured, not read:** every count and every buffer row. **Read, not
  executed:** the four `boundary()` call sites (a grep, not a run), the option
  namespace (read from `hunmin_ic_set_option`), and §E2's and §E3's own wording.
- **Absent, and named as missing inputs rather than assumed away:** a corpus of
  typed Korean with frequencies (E3's weights; `ls ~/work` returns `edw` only), and
  any measurement of how long a preedit Fcitx5, TSF or an Android editor tolerates
  (E2's option D). Neither is estimated here.
- **Corrections recorded, not rewritten:** §BND-0 rows 1–3 are left in
  `docs/SPEC-MODES.md` for the ruling to act on, because that sentence is an
  evaluable item's own text. The one factual fix made in this commit is the file
  citation inside it, which named `ic.c` for a constant that lives in
  `hunmin_internal.h`; the reasoning is §BND-0 row 1.
- **Re-measured after rebasing onto master `d1e90fb`, where #164 had merged the
  judgement itself**: `judge_boundary()` now runs in `boundary()` and R9's guard is
  `JUDGE_MIN_KEYS 3`, so §BND-0 row 5, §BND-2, §BND-3 and §BND-4 were corrected from
  "not implemented" to what ships, and the unit gap between the ruling's `3자` and
  the guard's three keys was recorded — then **settled the same day**, when the
  maintainer named key inputs; §BND-3 and `§R9` state the unit rather than question
  it. The cap and the judgement were built independently and combined without a
  conflict; the flush/reading measurement above is the one place they meet.
- **The ruling landed on the same branch, so the probe now measures the shipped
  behaviour as well as the pre-ruling one.** §BND-2 carries both tables; the
  post-ruling rows come from the same command, and the two cap tests fail when
  `WORD_CAP` is 0 (they were confirmed that way, then restored).
- **Parallel-session scope:** this branch touches `docs/OPTIONS-MEASURED.md`
  (this section and the header table), `README.md` (its index row and count),
  `docs/SPEC-MODES.md` (§R4-정정, §R9's unit, §E2/§E3/§E6's markers, the §E6 ruling,
  the §E2 file citation), `libhunmin/src/buffered_preedit.c` and
  `libhunmin/src/hunmin_internal.h` (the cap, the judgement timing and their
  options), `libhunmin/include/hunmin.h` (both options' contract),
  `libhunmin/tests/buffered-preedit-test.cpp` (the cap cases),
  `libhunmin/tests/wordset-test.cpp` (the timing cases) and
  `tools/modes-boundary-probe.py` (the guard-aware sweep and the reading). It does
  not touch `AGENTS.md`, `PLAN.md` or any frontend (no frontend exposes either
  option yet). It does touch `docs/REMAINING-WORK.md` §2's M2 and M3 rows and
  `docs/MILESTONES.md`'s 0.3 lane, once the maintainer ruled E1 and named the mixed
  sentence as the release gate: a board that says "E1 has no owner" beside a shipped
  E1 is the drift the board exists to catch.
