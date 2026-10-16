# Release 0.1 checklist

How the two 0.1 artefacts are gated, built and verified. Every step is a command
or a pointer to a document that already carries the detail.

**Scope.** This file describes a procedure. It does not say whether 0.1 is ready
to ship — that claim belongs to `docs/REMAINING-WORK.md` and to the maintainer.

**The order of authority: CI first, local second.** A green master run of
`.github/workflows/ci.yml` is the machine-checked gate; §2 is the same gate run
by hand, for when CI is unavailable or for a change CI does not cover. Both were
run on 2026-10-16 against `e953ae8`; the results are in §6.

---

## 1. The gate: what CI checks

Job definitions: `.github/workflows/ci.yml`. Per-job measurements:
`docs/CI-TIMING.md`.

| Job | Proves | Notes |
|---|---|---|
| `linux` | the full `ctest` suite + `hunmin-doctor`, configured `-DHUNMIN_TRACE_BUILD=ON` | 3-minute ceiling; the fcitx5 5.1.13 install tree is cached, and on a cache miss the job cannot fit |
| `android` | engine cross-build both ABIs, `assembleDebug`, tiers E0/E1/E2 on an AVD | job-level `if:` path filter |
| `deb` | `dpkg-buildpackage -us -uc -b -d`, with the suite run **inside** the package build; uploads the `.deb` | |
| `android-release` | `assembleRelease` both ABIs, throwaway keystore; uploads the APK | **gated** on a push to `master` and `vars.HUNMIN_BUILD_RELEASE == 'true'`, so it is skipped by default |
| `mingw` | the core cross-builds to Windows | `-DBUILD_TESTING=OFF` |
| `windows` | nothing — `if: false` per issue #51's ruling | kept in the file so "was Windows built, and why did it stop" stays answerable |
| `nightly-fuzz` | LeakSanitizer + random-key sweep | `schedule` only |

`android-release` being gated means **a green master run does not imply a release
APK was built**. Check its job list, or build the APK locally (§3.2).

### Measured 2026-10-16: no CI job executes, and the cause is not the tree

Run `38824471508` on `e953ae8` reports **0 steps for every job**: `mingw`,
`android` and `linux` conclude `failure`; `deb`, `android-release`, `windows` and
`nightly-fuzz` conclude `skipped`. All within three seconds of each other. The job
log blob returns 404, and the repo-level Actions settings are
`enabled: true, allowed_actions: all`. The local copy of `ci.yml` parses as YAML
and both `if: failure()` occurrences are inside comments.

So the CI half of this gate currently measures nothing. **The cause is not this
repository**: the maintainer, 2026-10-16 — *"current failures are on payment
issue. i'll maintain free account policy, and wait for the limitation unlocked.
until then, we should rely on local tests."* Nothing in `ci.yml` needs fixing,
and the numbers in `docs/CI-TIMING.md` are the last ones measured against runs
that executed. The measured evidence, and what the API could and could not
distinguish, is `docs/REMAINING-WORK.md` §3.

**Until that changes, §2 is not a fallback — it is the gate.** §1 stays here
because it is what the gate becomes once CI runs again, and because the job list
is the record of what each tier is trusted for.

---

## 2. The local gate — the operative one

One command, which is the ruling of 2026-10-16 encoded:

**Precondition: boot the AVD first.** With no device attached,
`engine-android-goldens` and `engine-android-e2e` skip with "no Android device
attached", and since neither is a declared exception **the gate fails**. That is
intended — a release gate should not pass with the Android tiers silently absent
— but it is only a precondition because CI is not currently booting the AVD for
us:

```sh
$ANDROID_HOME/emulator/emulator -avd hunmin-e2e \
    -no-window -no-audio -no-boot-anim -gpu swiftshader_indirect -no-snapshot &
"$ANDROID_HOME/platform-tools/adb" wait-for-device
```

```sh
tools/release-gate.sh --list          # the gate and its declared exceptions
tools/release-gate.sh -j4             # run it
```

It runs the full suite and fails on any failed test, on any skipped test that is
not in its declared exception table, **and on a declared exception that stopped
skipping** — because an exception that no longer applies is a stale claim.
`tools/release-gate.sh --selftest` runs the verdict logic against planted
verdicts, which is how the parser that reads ctest's skip list was caught
matching nothing (ctest indents that block with a tab). The ruling and what the
measurement added to it: `wiki/decisions/2026-10-16-release-gate-all-tests-pass.md`.

The same sequence by hand, for when you want to watch it. Run from the
repository root. Export the Android toolchain **at configure time** —
the Android tiers register conditionally on it, so a build configured without
them reports green having run fewer tests (`docs/ENVIRONMENT.md` Batch 2, and
`docs/CI-TIMING.md` §4 for why counts must always name their configuration).

```sh
export JAVA_HOME=$HOME/toolchains/jdk-17.0.20.1+1
export ANDROID_HOME=$HOME/Android/Sdk
export ANDROID_NDK=$ANDROID_HOME/ndk/27.3.13750724

cmake -B build -DHUNMIN_TRACE_BUILD=ON        # add -G Ninja to match CI's linux job
cmake --build build -j16

ctest --test-dir build -N                     # the count for THIS configuration
ctest --test-dir build -j16 --output-on-failure

python3 tools/docs-lint.py --build-dir build  # document claims vs the tree
python3 tools/wiki-lint.py                    # wiki frontmatter, page count
```

Inner loop and tier selection: `tools/ctest-select.sh -L tier-D` prints what it
will run and what it will not. Time budgets: `tools/ci-time-budget.sh build`
measures against the budgets in `AGENTS.md`; run it before pushing anything that
adds a test.

Expected non-passes on a fresh clone, by design: `data-selfcheck`,
`modes-corpus-selfcheck` and `engine-android-goldens` report **Skipped** without
the gitignored corpora (`docs/REMAINING-WORK.md` §0), `engine-android-e2e` needs
a booted AVD, `engine-d3-gtk4` needs GTK 4.

---

## 3. The two artefacts

### 3.1 The `.deb`

```sh
tools/build-release.sh deb                  # dpkg-buildpackage -us -uc -b
```

The suite runs **inside** the package build (`debian/rules`
`override_dh_auto_test`), so a red test fails the packaging — by design, not as a
side effect. The `.deb` lands one level **above** the source tree.

What must be in it (paths from `frontends/fcitx5/src/CMakeLists.txt`'s
`install()` rules, and from the install tree `DESTDIR=/tmp/d cmake --install
build` produces):

```sh
dpkg -c ../hunmin_0.1.0_*.deb | grep -E 'libhunmin\.so|hunmin.*\.conf'
```

- `usr/lib/<triplet>/fcitx5/libhunmin.so` — the addon fcitx5 dlopens
- `usr/lib/<triplet>/libhunmin.so.0.1.0` (+ the `.so` / `.so.0` symlinks) — the
  core the addon has as `DT_NEEDED`
- `usr/share/fcitx5/addon/hunmin.conf`
- `usr/share/fcitx5/inputmethod/hunmin.conf`, `hunmin-romaja.conf`
- `usr/share/hunmin/{layouts,compose,hanja,idioms,krdict}/…`

`debian/control` requires `fcitx5-modules-dev (>= 5.1.13)` and the addon needs
fcitx5 >= 5.1.13 for real (`FCITX_ADDON_FACTORY_V2`,
`frontends/fcitx5/src/engine.cpp`); Ubuntu 24.04's 5.1.7 is below that floor.

Install and check it loads:

```sh
sudo dpkg -i ../hunmin_0.1.0_*.deb           # postinst reloads fcitx5 over D-Bus
fcitx5-diagnose | grep -A2 hunmin
```

### 3.2 The release APK

```sh
tools/build-release.sh apk x86_64             # arm64-v8a is the other ABI
```

Keystore resolution, in order (`frontends/android/app/build.gradle.kts`,
`frontends/android/keymaps/build.gradle.kts` — both modules, identically):
`HUNMIN_RELEASE_STORE` → `frontends/android/local.properties` → a throwaway key
generated into `frontends/android/build/release-ci.keystore`. The generated key is
reused if it already exists; it is a build-proof, not a release signature.

Verify it, rather than trusting the build's exit code:

```sh
export PATH="$JAVA_HOME/bin:$PATH"            # apksigner execs a bare `java`
BT="$ANDROID_HOME/build-tools/35.0.0"
APK=frontends/android/app/build/outputs/apk/release/app-release.apk

"$BT/apksigner" verify --print-certs "$APK"   # exits 0; prints the signer DN
"$BT/aapt" dump badging "$APK" | grep '^package:'
unzip -l "$APK" | grep -oE 'lib/[a-z0-9_-]+/' | sort -u   # both ABIs
```

Version convention (ruled 2026-10-12, issue #9): the release is `0.1.0` in
`CMakeLists.txt` and `debian/changelog`; the Android `versionName` keeps the
build-phase suffix `0.1.0-p5`. Recorded in `docs/MILESTONES.md` §0.1.

Install and compose on a device or AVD — the full procedure, including the
`pm clear` and warm-up artefacts that look like R8 damage and are not, is
`tests/manual-checklists/release-r8-shrinker.md`. Do not skip it: no CI job
installs the release APK, and `tools/android-e2e.sh` drives the **debug** APK by
design.

### 3.3 Tag, publish, and the release notes

The distribution channel is a **GitHub Release on tag `0.1.0`** with
`hunmin_0.1.0_amd64.deb` attached (ruled 2026-10-16, issue #7 —
`wiki/decisions/2026-10-16-0-1-distribution-channel-github-release.md`). The
procedure is a script, not prose:

```sh
tools/release-0.1.sh              # dry run: prints every command, changes nothing
tools/release-0.1.sh --yes        # build .deb, tag, push, create release, upload
```

It delegates the build to `tools/build-release.sh deb` (§3.1) so the build path
lives in one place, and uploads `docs/release-notes/0.1.0.md` as the body. Do not
re-type its steps here; that is how two copies of a release procedure drift.

`docs/HISTORY.md` takes **one entry per thing that shipped**, written at the tag,
and states its own rules for what does not go in (no commit hashes, no
every-commit listing, nothing that reads as a boast). The release-notes file is
the *user-facing* document; `HISTORY.md` is the record of what we learned, and
neither is a copy of the other. `versionCode` is still `1` and no tag exists —
`docs/REMAINING-WORK.md` §7.5 records that as not closed by the version
convention.

---

## 4. Environment-bound: no CI substitute

| What | Procedure | State |
|---|---|---|
| Physical BT-keyboard sign-off | `tests/manual-checklists/tier-e3-bt-keyboard.md` | closed as an issue (#5); the checklist's device rows are still unticked |
| Voice recognition quality | `tests/manual-checklists/tier-e4-voice.md` | measured once on hardware; a person-in-loop tier, no unattended run |
| Release APK install + compose | `tests/manual-checklists/release-r8-shrinker.md` | measured 2026-10-11; re-run after any change to `proguard-rules.pro`, the release `signingConfig`, or the JNI shim |

---

## 5. Release-gating issues

Cited, not restated — each carries its own content and its own state.

| Issue | Gate | State |
|---|---|---|
| #1 — B8, the keystore path | packaging the signed APK | **Closed 2026-10-13.** Ruling: `wiki/decisions/2026-10-13-b8-keystore-path-resolved.md` |
| #2 — B4, the E5 한/영 key | 0.1's Android behaviour | **Closed 2026-10-11.** Ruling: `wiki/decisions/2026-10-12-auto-toggle-disabled-by-default-in-0-1.md` |
| #3 — B9, the R8/R3 contradiction | shipping one architecture description | **Closed 2026-10-14.** Ruling: `wiki/decisions/2026-10-13-r8-mode-guessing-is-adaptor-owned.md`; see also `docs/architecture.md` |
| #4 — the R8 shrinker path end-to-end | release-build confidence | **Closed**; measured 2026-10-11, procedure in §3.2's pointer |
| #5 — tier E3 device sign-off | the P5 gate | **Closed 2026-10-11**; the device rows remain unticked (§4) |
| #7 — the 0.1 distribution channel | how a user gets 0.1 | **Closed.** GitHub Release on tag `0.1.0`, scripted in `tools/release-0.1.sh` |
| #8 — CI builds neither 0.1 artefact | artefact reproducibility | **Closed 2026-10-13**: jobs `deb` and `android-release`. The ruling it asked for — must a release use the CI artefacts — is still open; see §1 on whether CI is running at all |

Issue #10's body named #1, #2, #3 and #5 as "still open". All four have since
been closed; the table above is the current state, read from GitHub.

---

## 6. Verification — measured 2026-10-16, this host, at `e953ae8`

Configuration: Ubuntu, gcc, fcitx5 5.1.19 from the distro, JDK 17 / Android SDK
35 / NDK 27.3.13750724 exported at configure time, `-DHUNMIN_TRACE_BUILD=ON`,
Makefile generator, `-j16`.

| Step | Command | Result |
|---|---|---|
| configure + build | `cmake -B build -DHUNMIN_TRACE_BUILD=ON`; `cmake --build build -j16` | rc 0 |
| suite | `ctest --test-dir build -j16` | **1 failed of 184**, 5 skipped, 178 passed |
| the red one | `ctest --test-dir build -R '^frontend-mutations$' --output-on-failure` | `I5-layout-override-ignored: anchor not found (source moved)` |
| docs | `python3 tools/docs-lint.py --build-dir build` | 0 errors, 8 warnings |
| wiki | `python3 tools/wiki-lint.py` | 0 errors, 32 pages, 2 advisory |
| release APK | `tools/build-release.sh apk x86_64` | rc 0; 1,532,493 bytes; both ABIs; `apksigner verify` ok, signer `CN=hunmin-ci`; `versionName='0.1.0-p5'`, `versionCode='1'` |
| `.deb` | `tools/build-release.sh deb` | **rc 2** — no artefact; `frontend-mutations` fails inside the package build (64 registered there: 63 pass, 1 fail, 5 skipped) |
| install tree | `DESTDIR=/tmp/d cmake --install build` | rc 0; paths as listed in §3.1 |
| CI | run `38824471508` | no job executed (§1) |

The 184 here is this configuration's number, not a property of the tree; read
`docs/CI-TIMING.md` §4 point 4 for why, and re-measure with `ctest -N` rather
than quoting a figure.

### Findings — measurements, not verdicts

1. **`frontend-mutations` is red on master, and it blocks the `.deb`.** The
   mutation `I5-layout-override-ignored` anchors on
   `frontends/fcitx5/src/engine.cpp` reading
   `*engine_->config().layout`; the source now reads
   `*engine_->config().hangulLayout` (`engine.cpp:404`), renamed by `1521fad`
   ("Add DVORAK and Colemak layouts with separate Hangul/Latin pickers"). The
   anchor is `tools/frontend-mutations.py:56-57`. The self-test fails closed, as
   designed — which is why `dpkg-buildpackage` aborts. Note that
   `ctest -LE slow` does **not** cover it (the test carries the `slow` label), so
   a green `-LE slow` run says nothing about this.
2. **No CI job executes** (§1). Cause not determined here.
3. **`README.md` still contradicts the tree**, in the two places §5's issues
   closed: line 36-38 says the R8 shrinker path "has never actually removed
   anything" and that `assembleRelease` "runs in no CI job" (measured green
   2026-10-11, and the `android-release` job exists); line 108-109 says the
   keystore path "is still undecided (issue #1)" (decided 2026-10-13, and the APK
   built in §6 reads that config).
4. **`tools/build-release.sh` comments are stale the same way**: line 9 "CI builds
   neither 0.1 artefact", lines 76-79 "Issue #1 (B8) is unresolved:
   `build.gradle.kts:77` hardcodes …".
5. **`docs/REMAINING-WORK.md` §1's B9 row and `docs/MILESTONES.md` §0.1 still
   carry B9 as undecided**, after issue #3 was closed 2026-10-14 by `a5c46dc`.
6. **`tools/build-release.sh`'s deb-failure hint is stale**: it names
   `data-selfcheck` as the expected red, which now *skips* (§0). The red that
   actually stops the build is finding 1.