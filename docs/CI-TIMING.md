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
  (the `android` job's tier E0/E1 ctest step in `.github/workflows/ci.yml`).

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
| `engine-android-e1` | 148 s | 186 s | the same `-R` in the android job |
| `android-mutations` | 84 s | 138 s | the same `-R` in the android job |
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
the android job's `-R` matches neither name, so it never ran the corpus either.

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
`ndk/26.1.10909125/…/android.toolchain.cmake`. The property
`externalNativeBuild` resolves is **`android.ndkVersion`**, at the `android { }`
level. Verified locally, with `ANDROID_NDK` deliberately unset,
`-DCMAKE_TOOLCHAIN_FILE` now resolves to the 27.3 toolchain, and `ctest -j4` is
green over the suite.

Two details worth recording, because both were got wrong on the way here. AGP
parses *every* installed NDK's `package.xml` during discovery, so the string
`ndk/26.1.10909125` appears in a `--info` log whether or not it is in use —
checking for its presence proves nothing; what matters is
`-DCMAKE_TOOLCHAIN_FILE`. And AGP 8.6's `build_model.json` carries **no
`ndkVersion` key at all**, so reading it reports `None` regardless of the pin;
the file's `configurationArguments` holds the real CMake invocation and is what
should be asserted.

**4. The counts below are configuration-dependent, and `docs-lint` check 1
enforces it.** This host measures 176 registered with no Android toolchain in
the environment; CI's runner image ships one, so the same tree there registers
181. The first version of this section wrote "176 tests" next to a CI figure
and check 1 failed the run for it — which is the check doing its job, since a
bare count next to another job's numbers is the drift the rule exists to stop.
Every count below now names its configuration.

**What this changes in the analysis.** The failure was not caused by the timing
levers — the unpinned NDK was a defect in the tree before them. But it was
*exposed* by them, and it is the reason the run's wall time is not yet the
projected ~130 s: `Total Test time (real) = 456.23` against 269 s before, with
11 tests each burning a 120 s timeout. **A run that made the suite faster and
slower at once**, which is the shape worth expecting from parallelising a suite
whose ordering was load-bearing.

---

## 5. The 3-minute ceiling, and the two levers that meet it (2026-10-15)

Written after the maintainer set a 3-minute limit on CI jobs (5 minutes first,
then 3 the same day). This section records what was measured to meet it, what
was deliberately not changed, and the two tools that now exist so the limit is
checked before a push rather than discovered by a red run.

### What the ceiling forced, and what it cost in coverage

The `linux` job measured **8 m 24 s** (§1), with **269 s of it a single ctest
step**. Of those 269 s, three tests were 252 s: `engine-android-e1` 148 s,
`android-mutations` 84 s, `frontend-mutations` 19 s. The first two are
**duplicates** — the `android` job runs exactly
`engine-android-goldens|engine-android-e1|android-mutations` at its own
"Tier E0 + E1" step (§"Duplication, measured against the tree").

So the lever is option A of §3, applied: `ctest -LE 'tier-E1'` in the linux
job. Measured there as taking linux's ctest from **269 s to about 37 s** and
the job from **504 s to about 235 s** — inside the 300 s ceiling with 22%
headroom, the headroom being the two segments a runner-image change can move
(fcitx5 source build 197 s, apt install 22 s).

**The coverage cost is stated, not hidden.** The linux job stops being a
second place the Android adapter is proved to pass. §3 already weighed this
and left it as a maintainer call because it matters *while the android job is
the red one*; the 5-minute ceiling is what makes the trade necessary rather
than optional, and the android job's own run is what keeps the coverage.

### What was NOT given a 5-minute ceiling, and why

A limit that kills a job is not a limit, it is a broken build. Four jobs keep
a larger budget, each with the measurement in `.github/workflows/ci.yml` next
to the number:

| job | budget | measured floor |
|---|---|---|
| `deb` | 45 min | 13 m 30 s — `dpkg-buildpackage` alone is 460 s |
| `android` | 45 min | SDK download + AVD boot + two instrumentation suites |
| `android-release` | 45 min | SDK/NDK download + two-ABI Gradle release build |
| `nightly-fuzz` | 120 min | 10,000 iterations, by construction; `schedule`-only |

`linux` and `mingw` got 5 minutes; `mingw` builds one target with no tests, so
its ceiling is a runaway guard rather than a fit.

### The local/CI measurement gap, now instrumented

The reason a local green run is not evidence about the CI budget is structural
and is now measured rather than asserted:

- **tier E1 does not register locally** unless `ANDROID_HOME` is set
  (`CMakeLists.txt:192`). A host without the Android SDK sees **0** tier-E1
  tests, not skipped ones — the two look identical in a pass/fail summary.
  Measured on this host: `tier-E1: 0`.
- **tier D3 skips on the runner** (§1: no `xvfb`, no distro fcitx5 modules),
  so CI never pays for the four D3 runs local does. Measured here:
  `tier-D3: 4`.

Two tools close the gap, both runnable locally and both cheap:

- **`tools/ci-time-budget.sh [build-dir]`** — measures the inner loop
  (`-LE slow`, budget **30 s**), the slow tier (`-L slow`, budget **60 s**) and
  the whole suite, then prints the local/CI delta explicitly instead of
  pretending the two agree. Exits non-zero when a budget is over.
- **`tools/ci-env-snapshot.sh [build-dir]`** — captures the runner facts that
  decide which tiers register at all (toolchain, env vars, fcitx5 version,
  per-label test counts, data inputs). In CI it runs **before ctest and
  unconditionally**, so a hard abort that skips later steps still leaves the
  snapshot, and on failure it is uploaded as `linux-failure-evidence` together
  with `LastTest.log`.

The budgets themselves live in `AGENTS.md`, in one table, next to the rule
that a budget is not a reason to drop coverage: when a new test pushes a
budget over, the first move is to make the test cheaper, not to leave the
behaviour unasserted.

### What this section does not claim

The 37 s / 235 s figures are §3's measurement of the same change, not a fresh
run on the ceiling as applied — no CI run has executed since. The first run
after this lands is the confirmation, and if `linux` times out the segment
table in §1 is where to look before raising the number.

### 5b. From 5 minutes to 3, and the bug the second lever exposed

The 5-minute ceiling was met by one lever (tier E1 exclusion, above). Three
minutes needed a second, because the remaining budget was 235 s against 180 s
and the largest segment in it was **197 s of fcitx5 5.1.13 built from source on
every commit with nothing cached** — the job uploaded its install tree for the
deb job to restore, but never cached its own.

So the install tree is now cached (`actions/cache@v4`, keyed on the pinned
`5.1.13` tag, `restore-keys` falling back to the newest entry). The build step
is gated on `cache-hit != 'true'` and installs to `/tmp/fcitx5-install` rather
than `/usr` directly, so the cached path and the freshly-built path are the
same tree; a separate step copies it into `/usr` either way, behind a guard
that names the file it expects.

**The resulting budget, segment by segment:** apt install 22 s (measured, §1) +
cache restore ~10 s + `cmake` configure/build 9 s (§1) + ctest ~37 s (§3) +
`ci-env-snapshot.sh` **0.14 s** (measured on a 16-core host, not estimated) =
**~78 s against 180 s** — 56% headroom, so the ceiling is a budget with room
rather than a number the job barely fits.

**On a cache miss the job cannot fit.** The fcitx5 build is 197 s on its own.
That is the cold path, paid once per tag bump, and it presents as a timeout
rather than a silent slow run. The recorded response is to let the cold build
finish and cache it, not to raise the ceiling.

### A latent bug in the artefact layout, found while adding the cache

Worth recording because it was not the change being made and it would have
read as something else. The upload step listed absolute `/usr/lib/...` and
`/usr/include/...` paths. `upload-artifact@v4` strips the **least common
ancestor** of the listed paths, which for those two is `/usr` — so the archive
held `lib/` and `include/` at its root. The deb job restores into
`/tmp/fcitx5-install` and then copies `/tmp/fcitx5-install/usr/.` into `/usr/`,
i.e. it looks for a `usr/` level the archive never contained. Its own guard
(`test -d /tmp/fcitx5-install/usr/lib/x86_64-linux-gnu/cmake/Fcitx5Core`) would
therefore fail with "restore is missing Fcitx5Core cmake configs" — a message
about the restore, for a cause in the upload.

Naming `root: /tmp/fcitx5-install/usr` makes the layout explicit instead of
implied by which paths happen to be listed. **Not re-measured on CI**: no run
has executed since, so the first deb run after this lands is the confirmation.
The commit that introduced the mismatch is `6cd5d20` ("shared fcitx5"), which
is also the commit that made the deb job depend on the artefact at all — the
two halves of that change did not agree with each other.

---

## 6. None of this can currently be re-measured (2026-10-16)

Every figure above came from runs that executed. **No CI job has run since
2026-10-13**: every run reports zero steps, and the maintainer's answer to why
is an account one, not a tree one — *"current failures are on payment issue.
i'll maintain free account policy, and wait for the limitation unlocked."*
Measured evidence and the resolution: `docs/REMAINING-WORK.md` §3.

Three consequences, stated here because each one invalidates a thing this
document would otherwise be used for:

1. **The budgets in §5 are the last measured ones, not current ones.** The
   3-minute `linux` ceiling and the ~78 s projection were measured against runs
   that did execute. Nothing here says they still hold; nothing here can say so
   until a run does.
2. **The PR-versus-nightly split is deferred, not rejected.** It was raised on
   2026-10-16 and the objections are recorded (per-job trigger scoping, because
   `android-release` is gated on `push`; tier E1 is the tier that proves the
   adapter's `onKeyDown` path, so moving it to nightly is a coverage decision
   rather than a saving). It cannot be costed against runs that never execute,
   so the decision waits for the limitation to lift and is re-opened then.
3. **The local gate is the gate meanwhile.** `tools/release-gate.sh` — 184
   registered tests on this host at `-j4`, 2 declared skips, and a verdict on
   both. It is the local stand-in for §5's budget, and it is what a release is
   checked against until CI runs again. Its own numbers are local, and CI-TIMING
   §2 is the standing warning that local and CI do not measure the same suite —
   which is now the whole comparison, since only the local side is available.

## 6. A red run's evidence — audited 2026-11-04

Asked as three questions: where the time goes when CI runs, how it actually
runs, and whether a failure leaves enough behind to debug without re-running.
The third one had holes, and they are fixed here.

### How it runs

Enabled jobs only, from `.github/workflows/ci.yml` as it stands: **`linux`**, on
pull requests, ceiling 3 minutes; **`mingw`**, on every trigger, ceiling 3
minutes, build-only (no ctest). `android`, `deb`, `android-release`, `windows`
and `nightly-fuzz` all carry `if: false`. And nothing runs at all today: issue
#102 is the account-level billing failure, and the repository reports
`actions/permissions: enabled=false` (checked 2026-11-04), so every job starts
and dies in seconds.

### Where the time goes

The `linux` job's own budget (§5): apt 22 s + cache restore ~10 s + cmake/build
9 s + ctest ~37 s + snapshot 0.14 s ≈ **78 s of 180 s** when the fcitx5 install
tree is cached. The only segment that can exceed the ceiling is the cold fcitx5
build (**197 s**) — a cache MISS is over the ceiling before ctest starts, which
is why the file says a timeout there is the cold path and not a regression.

Inside ctest on the runner's 4 cores, the mutation self-test was the local pole
after #249/#255 removed the D3 tiers' dead clock. Measured on a 4-core slice
(`taskset -c 0-3`, the runner's parallelism) with the new slot pool:

| cores | slots | wall |
|---|---|---|
| 16 | 1 | 38.2 s |
| 16 | 4 | 23.3 / 23.7 s |
| 4 | 1 | 36.4 / 37.3 s |
| 4 | 2 | **27.5 / 29.4 s** |

Verdicts identical in all of them. The registered tier now measures **27.0 s**
in `ctest` on this host (43–56 s before), and the slow tier's wall is **50.7 s**
on this branch alone — the D3 tiers' own dead clock (issue #255, PR #256) is
what takes it to 40.6 s once that change is on top. The 4-core numbers are why
the pool defaults to half the cores rather than being a local-only trick.

### What a failure left behind, and what was missing

The `linux` job's evidence step uploaded exactly two paths:
`ci-env-snapshot.txt` and `build/Testing/Temporary/LastTest.log`. Four holes:

1. **`if: failure()` does not fire on a timeout.** GitHub cancels the job when
   `timeout-minutes` expires, and a cancelled run does not satisfy `failure()` —
   so the one failure the file says to expect (the 197 s cold build) produced
   **no artefact at all**. Now `failure() || cancelled()`.
2. **No machine-readable result.** `LastTest.log` is prose. Added
   `ctest --output-junit build/junit.xml`, and the artifact carries it.
3. **The failed/skipped lists were left behind.** `LastTestsFailed.log` names
   the failures in two lines; `LastTestsSkipped.log` says which tests skipped
   and is the only place a "green" run that measured nothing shows up. Both are
   in the artifact now.
4. **The mutation tier's own output was discarded.** `_run()` captured the
   compiler and ctest output into a variable and dropped it, so a GAP printed
   two booleans and a build failure printed `build rejects` with the compiler's
   error lost. Both are printed now, bounded: the rejecting guards' output
   (6 lines each) when a row is not a plain `guarded`, and the first compiler
   error (4 lines) when a build fails. Measured on the standing GAP
   (`I12-autoswitch-not-reapplied`): the row now carries
   `engine-d1-fcitx5 (Subprocess aborted)` instead of nothing.

### What a run prints, so the judgement does not need a person

Three outputs, in the order a run produces them. The question each answers is
stated, because that is what makes an output worth printing.

**1. `ci-env-snapshot.txt`, before ctest** (`tools/ci-env-snapshot.sh`):

| section | the question it settles |
|---|---|
| runner, os, nproc, date | which image this was, and how much parallelism |
| toolchain variables, each printed even when `(unset)` | why a tier is absent rather than skipped |
| tools, each `path` or `MISSING` | which tier *could* have run |
| fcitx5 version | whether the addon's 5.1.13 floor is met |
| **build configuration from the CMake cache** — generator, build type, `BUILD_TESTING`, `HUNMIN_TRACE_BUILD`, both compilers, `CMAKE_PREFIX_PATH`, `Fcitx5Core_DIR` | what the suite was built *to do*, as opposed to what the command line asked for |
| **cache outcomes** — `HUNMIN_CI_CACHE_FCITX5`, `HUNMIN_CI_CACHE_GRADLE` | the one fact that explains a job 197 s longer than the last |
| registered tests per label | how many of each tier this configuration actually has |
| data inputs, `present`/`ABSENT` | a missing table reads as a skip, not a pass |

**2. `ctest` timing, always, on green runs too** (`tools/ci-test-timing.py`,
reading `--output-junit`):

```
tests: N  passed: N  failed: 0  skipped: 0
sum of per-test times: … s (wall clock is ctest's own total, smaller under -j)
slowest 10:
   27.0 s  pass  frontend-mutations
   …
skipped (a skip is not a pass):
   …
```

plus the same table as markdown on the run's **summary page**, so "where is it
slow" is answered without downloading anything. This is the output whose absence
made a 47 s tier look like weather: a green run that is 3× the previous one is
the finding, and it only shows if it is printed on every run.

**3. On failure or cancellation**, the artifact: the snapshot, `LastTest.log`,
`LastTestsFailed.log`, `LastTestsSkipped.log`, `build/junit.xml`.

**Still not printed, and why.** Per-step wall times are only in the log
timestamps and the UI's step list — GitHub provides them, so a duplicate in the
artifact would drift. The budget comparison (78 s against 180 s) is a local
figure from `tools/ci-time-budget.sh`; a CI run cannot compare itself to it
without knowing its own expected cost, which is the number under measurement.

### What is still not collected, named rather than fixed

- The mutation sandboxes are deleted on exit; a post-mortem on a failed build
  needs the log the tool now prints into the job output, not the tree.
- D3's stage directories (`/tmp/hunmin-d3.*`, with `fcitx5.log`,
  `harness.log` and the gate dump) are deleted unless `HUNMIN_D3_KEEP=1`. D3
  skips on the runner, so this is a local-debug gap, not a CI one.
- The `android` job's emulator log-on-failure step lives inside a disabled job.

**Evidence status.** None of the above has run in CI: the workflow is disabled
at the account level, so the two workflow edits are unexecuted and are recorded
as such. The tool changes are measured locally (25.5 s, verdicts unchanged, the
GAP row carrying its evidence).
