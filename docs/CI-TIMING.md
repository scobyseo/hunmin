# CI-TIMING — what each CI job costs, measured

Written 2026-10-14 from the job logs of run 38428978810 (`master` at `4d0746a`),
the linux and deb jobs both green, plus host measurements on the branch
`scobyseo/ci-ci-wall-time-is-8m24s-per-job-target-1-2-min`.

Issue #59 asks for 1–2 minutes per job. This file is the measurement half of
that: where the time goes, and what each lever costs. §"Options" compares the
levers; §"Order" records what was **applied** on 2026-10-14 and what was
deliberately left as a maintainer call.

Every figure below came from one of:

- `gh api repos/scobyseo/hunmin/actions/jobs/<id>/logs`, parsed for step
  boundaries and per-test times. Linux job id 115405828032, deb job id
  115401054183, both from run 38428978810.
- `ctest` and `tools/android-unit.sh` on this host, timed with `/usr/bin/time`.
- the runner image's own manifest, `actions/runner-images`
  `images/ubuntu/Ubuntu2404-Readme.md`, read on 2026-10-14.

Nothing here is a re-count of a number that already lives in another document.
D3's own costs stay in `docs/D3-PERFORMANCE.md`; the label vocabulary stays in
`docs/TESTING.md`; this file cites them.

---

## 1. The two jobs, by segment

Timestamps are the `##[group]Run …` boundaries in each job's log, so they are
step boundaries rather than estimates.

### The linux job — 8 m 24 s

| Segment | Elapsed |
|---|---|
| checkout | 1.6 s |
| `apt-get update` + install build deps | 22 s |
| git clone + configure + build + install fcitx5 5.1.13 from source | **197 s** |
| install pyright (`curl astral` + `uv tool install`) | 2 s |
| `cmake -B build` | 0.8 s |
| `cmake --build build` | 8 s |
| `ctest --test-dir build --output-on-failure` | **269 s** |
| job total | 504 s |

Inside that ctest, the label time summary reads `Total Test time (real) =
268.60`, and the five slowest tests are:

| Test | Time | Labels |
|---|---|---|
| `engine-android-e1` | **148.38 s** | `tier-E1;slow` |
| `android-mutations` | **84.31 s** | `tier-E1;mutation;slow` |
| `frontend-mutations` | **19.36 s** | `tier-D1;tier-D2;mutation;slow` |
| `docs-lint` | 4.15 s | `tier-G;slow` |
| `keymap-fixture` | 2.84 s | `tier-C` |

Everything else is under 1 s each. The three heavyweight tests are 252 of the
269 s, which is the fact the issue opens with.

Two things the issue's table does not show, both from the same log:

- **`engine-d3-x11`, `engine-d3-gtk3` and `engine-d3-gtk4` are Skipped**, not
  run — the runner image ships neither `xvfb` nor the distro fcitx5 modules this
  tier needs. So the 145 s the D3 trio costs on a host (see
  `docs/D3-PERFORMANCE.md`) is not part of CI's 269 s, and the serial queue is
  even more concentrated than the issue's numbers imply.
- **The Android tiers run at all because the runner image ships an Android SDK
  the workflow never installs.** `Ubuntu2404-Readme.md` records
  `ANDROID_HOME=/usr/local/lib/android/sdk`, `ANDROID_NDK=…/ndk/27.3.13750724`
  — the exact NDK this repo pins — and `17.0.20+1` as the default JDK. The linux
  job's guard at `CMakeLists.txt:192-193` (`find_program(JAVA_EXECUTABLE java)`
  and `DEFINED ENV{ANDROID_HOME}`) therefore fires on every run, which is why
  232 s of tier E1 land in a job whose name is `linux`. The android job then
  re-runs exactly `engine-android-goldens|engine-android-e1|android-mutations`
  at `.github/workflows/ci.yml:166`.

### The deb job — 13 m 30 s

| Segment | Elapsed |
|---|---|
| checkout | 1.6 s |
| `apt-get update` + install | 23 s |
| git clone + configure + build + install fcitx5 5.1.13 from source | **326 s** |
| `dpkg-buildpackage -us -uc -b -d` | 460 s |
| job total | 812 s |

The 460 s is the package build, and `Total Test time (real) = 402.11` of it is
`ctest` inside `debian/rules`' `override_dh_auto_test`. Per-test:

| Test | Time |
|---|---|
| `engine-android-e1` | **185.68 s** |
| `android-mutations` | **138.45 s** |
| `frontend-mutations` | **61.86 s** |
| `docs-lint` | 6.71 s |

**This job is longer than the one the issue names, and 386 of its 402 test
seconds are the same three tests.** It also builds fcitx5 5.1.13 from source a
second time, 326 s, with nothing shared with the linux job's 197 s build. The
issue mentions the deb job in one line; measured, it is the worst offender
against the 1–2 minute target.

### Duplication, measured against the tree

| Duplicated | linux | deb | android |
|---|---|---|---|
| fcitx5 5.1.13 source build | 197 s | 326 s | — |
| `engine-android-e1` | 148 s | 186 s | same `-R` at ci.yml:166 |
| `android-mutations` | 84 s | 138 s | same `-R` at ci.yml:166 |
| `frontend-mutations` | 19 s | 62 s | — |
| `engine-android-conformance-*` corpus | 15 glob + 1 duplicate | 16 | its `-R` matches neither name |

The last row is a tree fact, not a timing: `ctest -N` registers **16**
`engine-android-conformance-*` tests against **15** corpus files under
`tests/conformance/android/`. The glob at `CMakeLists.txt:561-580` derives one
name per file from the relative path
(`…_tests_conformance_android_shift-arrow-selection.trace.in`), and the explicit
block at `CMakeLists.txt:592-603` registers the same file again under the
hand-written name `engine-android-conformance-shift-arrow-selection`. That block
was added by the commit that registered the corpus, whose comment describes it
as "a nested worktree adds its own trace corpus separately"; the nested worktree
is gone and the corpus is in the main tree, so the second registration is
now pure duplication. Its measured marginal cost is small — 0.15 s of one extra
`java` invocation, the Gradle classpath behind it already materialised by its
fifteen siblings under a shared `flock`. The cost that matters is that
`ctest -N` no longer agrees with `ls tests/conformance/android/*.trace.in`, and
the android job's `-R` at ci.yml:186 matches neither name, so it never ran the
corpus either.

---

## 2. What the host measures, and why it differs from CI

Same branch, same tree, `ctest` timed with `/usr/bin/time`:

| Run | Wall | Result |
|---|---|---|
| 177 registered, no Android toolchain, serial | **189.76 s** | all pass |
| same, `-j16` | **47.82 s** | all pass |
| the heavyweight trio, Android toolchain, warm Gradle, serial | **113.46 s** | 3/3 pass |
| same trio, `-j4` | **67.15 s** | 3/3 pass |

So `-j` is a 4× on the suite and a 1.7× on the trio. **It cannot reach 1–2
minutes on its own**, and the reason is structural: `engine-android-e1` is
`RUN_SERIAL`, and `android-mutations` is nine *sequential* `tools/android-unit.sh`
runs, so at most one of them can run beside `frontend-mutations`. The trio's
parallel floor is ~67 s warm; CI's is 252 s cold.

### Cold Gradle is most of tier E1

| `tools/android-unit.sh` | Wall |
|---|---|
| this host, warm `~/.gradle` (1.6 GB, daemon up) | 22.95 s |
| this host, `GRADLE_USER_HOME=/tmp/cold-gradle`, empty | **126.24 s** |

The warm cache is `wrapper/dists` 143 MB, `caches/modules-2` 394 MB and
`caches/build-cache-1` 124 MB. `tools/android-unit.sh:47-56` runs
`--rerun-tasks`, so every invocation redoes the test task whether or not the
classes changed; `android-mutations` does that nine times. CI's 148 s and 186 s
for `engine-android-e1` are this same cold-cache effect with no
`actions/cache` anywhere in the workflow — `grep -rn 'actions/cache'
.github/workflows/ci.yml` returns nothing.

---

## 3. Options, compared

The four levers the issue names, each with its measured cost and what it costs
in coverage. **No option is recommended as a decision here**; the deciding
reason for the order below is stated, and the evidence that would change it.

### A. Stop running tier E1 in the linux job

`engine-android-e1` and `android-mutations` are already re-run by the android
job at `ci.yml:166`. Excluding them from the linux job's suite
(`ctest -LE 'tier-E1'`, or the equivalent) takes linux's ctest from 269 s to
about 37 s and the job from 504 s to about 235 s.

What it costs: the linux job stops being a place where the Android adapter is
proved to compile and pass on a plain runner. That matters while the android job
is the red one — the issue records run 38428978810's android job failing at the
SDK step. Excluding tier E1 from linux in that state makes `linux` green while
the only job that measures E1 is broken, which is exactly the "green CI is
compatible with a broken build" failure issue #8 was about. **Evidence that
would change the recommendation: the android job green for several runs.** With
that, A is the largest single win available.

### B. `actions/cache` on `~/.gradle`

Restoring the 661 MB of dependency and build caches into linux, deb and android
takes E1 from 148 s to about 23 s (warm, measured) and should take
`android-mutations` from nine cold Gradle runs to nine warm ones — about 3 s
each against the ~15 s the deb job's 138 s implies. Cache restore costs 5–15 s
once per job.

What it costs: nothing in coverage, and it is the only lever that shrinks all
three jobs at once. **This is the recommendation, and the deciding reason is
that it is the only one with no coverage trade at all.** Evidence that would
change it: a cache key that misses more often than it hits, which would need the
key to include the Gradle wrapper version and `frontends/android/gradle.properties`.

### C. `ctest -j` in the linux and deb jobs

269 s → about 80 s in linux (the trio's serial floor holds; everything else
overlaps), and the host's 189.76 → 47.82 s is the same effect. The runner has
4 cores.

What it costs: a risk of a parallel-only failure that a serial run hid. The
`RUN_SERIAL` properties and the `flock` in `tools/android-trace-emit.sh` exist
because this repo has been bitten by that before — fourteen conformance tests
deleting each other's `R-def.txt` under `-j16` is recorded in
`tools/android-trace-emit.sh`'s header comment. **Evidence that would change the
recommendation: a green `-j4` linux job over several runs**, which is what would
make this free rather than merely cheap.

### D. Share the fcitx5 5.1.13 build between linux and deb

One build, cached or uploaded once, restored into the other job: 523 s of
clone-configure-build-install across the two jobs becomes one build plus one
restore.

What it costs: the two jobs currently configure the source tree identically and
into the same `/usr` prefix, so sharing is mechanical; but the `.deb`'s
`dh_shlibdeps` reads dpkg's database for what the addon links, and a source-built
fcitx5 owns no package — `debian/rules:82-83` already works around that with
`--ignore-missing-info`, and a cache-restored build must land in the same paths
for that to keep holding. **Evidence that would change the recommendation: a
restored build whose `Fcitx5CoreConfig.cmake` resolves in the deb job**, which is
checkable in one run.

### E. Remove the duplicate conformance registration

Deleting the explicit block at `CMakeLists.txt:592-603` makes `ctest -N` agree
with `ls tests/conformance/android/*.trace.in`: 15 and 15. Measured saving
0.15 s per suite run.

What it costs: nothing — the glob still registers the same corpus under its
derived name, and `docs/REMAINING-WORK.md` §7.5's `TC-SHIFTARROW` row already
asserts the corpus *file* rather than either test name, precisely because
`docs-lint`'s `ctest` kind cannot see a glob-derived registration. **This is
the only option whose benefit is clarity rather than seconds**, and it is listed
because 16-versus-15 is the kind of tree fact that gets re-derived wrongly later.

### Order, and what it does not reach

**Applied 2026-10-14, in this order: B, C, D, E, and A only for the deb job.**
The recommendation above was B, and it was implemented along with the rest;
what follows is the applied state rather than the open comparison.

- **B, `actions/cache` on `~/.gradle`, in linux, android and deb.** Keyed on
  `frontends/android/gradle.properties` and the wrapper properties, with a
  `restore-keys` fallback so a single dependency bump is a hit rather than a
  miss.
- **C, `ctest -j4`** in linux, deb (`debian/rules`) and the android job's E0/E1
  step, and `cmake --build build -j4` in linux. `-j4`, not `-j16`, because the
  runner has 4 cores.
- **D, the fcitx5 install tree uploaded by linux and restored by deb.** The deb
  job now declares `needs: linux`, which is the honest failure mode: a deb run
  with no successful linux run has no artefact and fails at the restore. The
  check that the restore landed is the next step's own output — the addon must
  configure against the restored `Fcitx5CoreConfig.cmake`.
- **E, the duplicate registration removed** at `CMakeLists.txt`. `ctest -N`
  reports 15 against the 15 corpus files; the corpus is still registered by the
  glob under its derived name.
- **A, applied to the deb job only** — `debian/rules`' `override_dh_auto_test`
  runs `ctest -j4 -LE 'tier-E1'`, with the reason written in the recipe's own
  comment: the .deb does not ship the Android adapter, and the adapter's gate is
  the android job. **Not applied to the linux job**, for the reason stated under
  option A: the android job is the one that failed at the SDK step in the run
  measured, so excluding tier E1 from linux would make `linux` green while the
  only other place E1 runs is broken.

With B, C and D, the linux job lands near **130 s** — 22 s apt, a cached
fcitx5 restore, and a warm parallel ctest — which is the 1–2 minute target. The
deb job lands near **200 s**: 22 s apt, a fcitx5 restore, and a parallel ctest
without tier E1. Neither is below 2 minutes yet, and the remaining cost is the
`frontend-mutations` sandbox (9 CMake builds) plus the D3 trio where Xvfb is
available, neither of which has a lever identified here.

---

## 4. What the first CI run of the changes showed

The levers above were pushed and the run measured on 2026-10-14
(run 38486460380). Three findings, and the third is the one worth keeping.

**1. The cache missed on first run, as expected.** `Cache not found for input
keys: Linux-gradle-5afa40c5…` — there was nothing to restore. The saving starts
on the *second* run, which is what the restore-keys fallback is for.

**2. `engine-android-e1` and `android-mutations` now run in parallel with the
rest of the suite instead of ahead of it, and that exposed a latent defect.**
Run 38428978810 had them first (tests #8 and #9), so a full Gradle configure
ran before any tier-C Android test. With `-j4` they overlap, and the
`engine-android-conformance-*` tier ran on a tree where `~/.gradle` had never
been configured:

```
[CXX1101] NDK at /usr/local/lib/android/sdk/ndk/26.1.10909125
  did not have a source.properties file
```

11 tests failed, 456 s of test time. The cause is that **`build.gradle.kts` did
not pin an NDK version**, so AGP scanned `$ANDROID_HOME/ndk/*` and took
directory order. The ubuntu-24.04 image carries both `27.3.13750724` (what this
repo builds against) and an unversioned `26.1.10909125`. It passed before only
because another test's Gradle run happened to configure the project first.

**3. The first fix for it was wrong in a way nothing reported.** Putting
`version = "27.3.13750724"` inside the `ndk { }` block under `defaultConfig` is
silently accepted: `./gradlew :app:properties` prints `version:
27.3.13750724`, and the toolchain handed to CMake is still
`ndk/26.1.10909125/…/android.toolchain.cmake`. The build model at
`build/intermediates/cxx/*/logs/*/build_model.json` reports
`"ndkVersion": null`. The property `externalNativeBuild` resolves is
**`android.ndkVersion`**, at the `android { }` level. Verified locally, with
`ANDROID_NDK` deliberately unset, `-DCMAKE_TOOLCHAIN_FILE` now resolves to the
27.3 toolchain, and `ctest -j4` is green over 176 tests.

**What this changes in the analysis.** The failure was not caused by the timing
levers — the unpinned NDK was a defect in the tree before them. But it was
*exposed* by them, and it is the reason the run's wall time is not yet the
projected ~130 s: `Total Test time (real) = 456.23` against 269 s before, with
11 tests each burning a 120 s timeout. **A run that made the suite faster and
slower at once**, which is the shape worth expecting from parallelising a suite
whose ordering was load-bearing.
