# ANDROID-MUTATION-SANDBOX — what `android-mutations` costs, and what isolation buys

**Status: measurements and one implemented change. No option is recommended here.**
Where a choice is left open it is listed as open, and the evidence that would
settle it is named.

Measured 2026-10-10 on `task/E1-mut-sandbox`, on the base that merged
`task/MUT-options`, Ubuntu, 16 cores, 30 GB RAM, **`/tmp` a 16 GB tmpfs**.
Java 17.0.20.1, Android SDK from
`docs/ENVIRONMENT.md`.

---

## 0. The premise, checked first

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

## 1. Where the time goes

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

## 2. The three sandbox designs, measured

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

### 2.1 The "copy the warm build" question

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

## 3. What isolation costs, separated from the time

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

### 3.1 Concurrency, measured

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

### 3.2 The whole suite, which is what the flag was costing

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

## 4. Two defects found while measuring, both of which made the tier green while measuring nothing

### 4.1 A skip was counted as a catch

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

### 4.2 A red baseline made every mutation look caught

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

### 4.3 The report is deleted before every run

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

## 5. A pattern file nothing references

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

## 6. What this document does not settle

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