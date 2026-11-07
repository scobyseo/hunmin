# Release routine — unify materials, isolate builds, add a Linux box

Audience: the next agent working on hunmin release packaging.
Requirements home: `docs/SPEC-*.md` per requirement; release procedure:
`docs/MILESTONES.md`, `docs/REMAINING-WORK.md` §3, issue #182/#183.
Status: **plan, 2026-11-07** — nothing below is implemented yet. Findings
from the 0.3 feature check of the same date are folded into "Why it fails
frequently" and "Verification"; the measurements behind them (commands,
output, artifact hashes) live in `PLAN-0.3-CHECK.md` and the hand-off
comment on issue #183, not here.

**Implemented 2026-11-07 (the Linux-box half only, see §It is wired now).**
The unified `tools/build-materials.sh` does not exist; the `.deb` route does
and dispatches to the box when `HUNMIN_LINUX_HOST` is set.

## It is wired now

Three new files and one changed script carry the box route. Unsetting
`HUNMIN_LINUX_HOST` restores the previous behaviour exactly, which is what
makes it safe to land ahead of the rest of this plan.

| File | Runs where | Job |
|---|---|---|
| `tools/host-env.sh` | sourced | env file with export semantics (`set -a`), `native_path()`/`wsl_path()`, the JDK in the form the shell can use, and the ssh helpers |
| `tools/build-deb-linux.sh` | Windows/Linux driver | ships the tree, runs the remote half, fetches **every** package produced, stages it with bytes + sha256 |
| `tools/deb-build-remote.sh` | the box | LF/deps/mode normalisation, googletest pre-populate when debhelper ≥ 13.31, `dpkg-buildpackage -us -uc -b -nc` |
| `tools/build-deb.sh` | either | route selection: box → WSL guest → native, printing which it took |

`tools/build-release.sh deb` and `tools/build-deb-wsl.sh` were changed to
match: the former delegates on Windows or when the host is set (CI, native
Linux, host unset — unchanged), and both now stage the `hunmin-en-us` half
that the WSL route used to leave behind.

Route selection, measured on the Windows host:

```
$ tools/build-deb.sh                 # HUNMIN_LINUX_HOST unset, MSYS + wsl.exe
=== route: WSL guest (/mnt/c/…/tools/build-deb-wsl.sh)
$ HUNMIN_LINUX_HOST=scoby@192.168.219.112 tools/build-deb.sh
=== route: linux box (scoby@192.168.219.112)
$ tools/build-deb.sh --route=linux   # without the variable
build-deb: --route=linux needs HUNMIN_LINUX_HOST     (exit 2)
```

### Verified end to end (2026-11-07, box = Ubuntu 26.04.1, 12 cores)

`tools/build-deb.sh` with the box configured, from the Windows checkout:

```
=== route: linux box (scoby@192.168.219.112)
=== 0.2.0: …/modes-milestone-0.3-… -> scoby@…:/home/scoby/hunmin-build/0.2.0-20261107-190409/src
=== tree copied
100% tests passed, 0 tests failed out of 113       (58.5 s, inside dpkg-buildpackage)
=== staged in build/release-0.2.0/
     1291208 bytes  9ce3aa75…  hunmin_0.2.0_amd64.deb
      269512 bytes  49b88eaa…  hunmin-en-us_0.2.0_all.deb
=== ok
```

Wall clock from `route:` to `ok`: about two minutes — the WSL route on the same
checkout took roughly nine, almost all of it drvfs. The `hunmin-en-us` package
hashed **identically** to the WSL build of the same commit (`49b88eaa…`), so
the data half is reproducible across builders; the main package differs in
bytes (embedded build paths/timestamps), which is expected and not claimed.

Three host prerequisites surfaced only by failing, now named by the remote
half before it compiles anything:

| Missing | Symptom it produced | Where it came from |
|---|---|---|
| `cmake` (the package) | `dpkg-checkbuilddeps: unmet build dependencies: cmake (>= 3.22)` | a pip `~/.local/bin/cmake` satisfied the command but not dpkg, and later died with `ModuleNotFoundError: No module named 'cmake'` inside `dh_auto_configure` |
| `libxkbcommon-dev` | `xim-harness.c:41: fatal error: xkbcommon/xkbcommon.h: No such file` | undeclared in `debian/control`, installed by the CI workflow instead (§Why it fails, last bullet) |
| `pyright` | `docs-lint: 1 errors … M5-CLOSED claims pyright 'tools/' is clean, but the tree says absent` | the suite asserts on it; `~/.local/bin` must stay on PATH, which is why the PATH order is system-first-then-operator, not system-only |

Prerequisites on the box, checked by the remote half and named when missing
(it exits 3 with the apt line rather than failing three steps into a build):
`build-essential cmake pkg-config git dpkg-dev debhelper fakeroot`, plus
`libfcitx5core-dev` or `FCITX_PREFIX`.

Access is one key, installed once by the operator; every ssh call is
`BatchMode=yes`, so a missing key is a diagnosis that prints the public half
and the command to run, rather than a hung build:

```
cat ~/.ssh/id_ed25519.pub            # the line to authorise
mkdir -p ~/.ssh && chmod 700 ~/.ssh
cat >> ~/.ssh/authorized_keys        # paste that line, then Ctrl-D
chmod 600 ~/.ssh/authorized_keys
```

## How the three materials are made today

| Material | Route | Outputs |
|---|---|---|
| `.deb` | `tools/build-release.sh deb` (Linux) **or** `tools/build-deb-wsl.sh` (Windows→WSL) | `../hunmin_*.deb` vs `build/release-<ver>/`; build dir `obj-x86_64-linux-gnu/` inside the copy |
| APK | `tools/build-release.sh apk` → `tools/build-android.sh <abi>` → `gradlew assembleRelease` | `build-android/<abi>/`, keystore `build/release-ci.keystore`, APK under `frontends/android/app/build/` |
| Installer | `tools/build-windows-installer.sh` (7 steps) | `build-msvc/` core + `frontends/tsf/build-msvc/` TIP + `build-msvc/installer/stage/` + `hunmin-<ver>-setup.exe` |
| Publish | `tools/release-0.2.sh` (stage→tag→`gh release create`→verify) | `build/release-<ver>/` channel-named assets |

## Why it takes long

`docs/CI-TIMING.md`: Linux job 504 s (fcitx5 source build 197 s +
`ctest` 269 s, of which E1 148 s + `android-mutations` 84 s +
`frontend-mutations` 19 s). Deb job 812 s (`ctest` 402 s, mutations 386 s).
Cold vs warm: E1 126 s → 23 s. Same fcitx5/E1/mutation work re-runs in
three jobs; `build-deb-wsl.sh` recreates the tree from scratch per run.
Each of `build/`, `build-wsl/`, `build-mingw/`, `build-msvc/`,
`frontends/tsf/build-msvc/`, `build-android/<abi>/` fetches googletest
separately.

Local measurement on this host (2026-11-07) puts the Android device tiers
at the top of that list — the 105-test `build-mingw` suite spends most of
its wall clock in `android-mutations` and `engine-android-e2e`, not in the
engine tiers. Commands and per-test times are in `PLAN-0.3-CHECK.md`; the
point for the split below is that the cost is measurable per tier rather
than assumed.

## Why it fails frequently

- **Command drift:** `-b` vs `-b -nc` vs CI `-b -d`; plain Route A fails on
  Ubuntu 26.04 (debhelper 13.31 forces disconnected FetchContent; the fix
  lives only in the WSL script).
- **Host facts per run:** CRLF `source/format` exit 25, `/mnt/c` 0777
  modes exit 2, `keytool` not on PATH, keystore in four places,
  `cygpath -m` present in nine scripts and missing from three more that
  need it, vcvars quoting, VS-generator pin on VS18 hosts,
  fcitx5 ≥5.1.13 floor.
- **Output-path mismatch:** `../hunmin_*.deb` vs `build/release-*/` vs
  `artifacts/` (0.1 stale-byte incident).
- **Silent green:** mutation sandbox hardcodes Linux lib names;
  `modes-metrics-selfcheck` hardcoded `build/hunmin-sim` — that one is
  fixed on branch `scobyseo/modes-milestone-0.3-…` (`tools/jamo-keytable.py`
  takes `SIM_OVERRIDE`, threaded from `modes-metrics.py --sim`), so the
  bullet is history rather than an open item.

### Measured on 2026-11-07, which sharpens six of the above

- **The environment file does not export.** `~/.hunmin-env` is plain
  `KEY=value`, so every child process — `tools/build-android.sh`,
  `dpkg-buildpackage`, `gradlew` — sees an *empty* `ANDROID_NDK` and dies
  at `: "${ANDROID_NDK:?export ANDROID_NDK=…}"`. Consuming it needs
  `set -a; source ~/.hunmin-env; set +a`. The host-env helper must do
  that, or export explicitly; a routine that only `source`s it looks
  configured and is not.
- **`keytool`/`java` "not on PATH" has a root cause:** `JAVA_HOME` is
  stored in `C:/` form, which MSYS cannot use as a PATH entry. The same
  value must be kept for native JVM consumers and mapped to `/c/…` for
  bash lookup; document both forms rather than adding a fourth
  hand-written `PATH=` line.
- **`cygpath -m` is in fewer scripts than the bullet claims, and the gap
  is load-bearing.** Measured: `tools/android-golden-check.sh`,
  `tools/android-espresso.sh` and `tools/android-mutations.py` contain no
  `cygpath` at all, and all three fail when the calling shell disables
  MSYS argument conversion (`MSYS_NO_PATHCONV=1`, `MSYS2_ARG_CONV_EXCL=*`)
  — `adb: error: cannot stat '/c/…'`, `adb.exe: failed to stat /c/…`,
  `python3.exe: can't open file 'C:\c\Workspace\…'`. They pass with the
  conversion restored. `tools/android-e2e.sh` now normalises its APK
  paths and no longer cares; the other three still do.
- **A tier can exist, be named on the board, and register nowhere.**
  `find_program(ADB_EXECUTABLE adb HINTS $ENV{ANDROID_HOME}/platform-tools)`
  fails when `adb` is not on PATH at configure time, so
  `engine-android-e2e`, `engine-android-goldens` and
  `engine-android-espresso` were absent from `ctest -N` on this host and a
  green suite said nothing about them. With `platform-tools` on PATH they
  register and pass (issue #13's shape, found again).
- **A stale `-D` in a build dir outlives its meaning.** `build-msvc`
  carried `HUNMIN_STATIC_LIB:FILEPATH=<repo-root>/ON` from an earlier
  configure and the TIP link died with `LNK1181: … 'ON.obj'`, while the
  standalone TSF tree had the right value. Per-toolchain × mode dirs make
  this rarer; one route means one place to `-U` lookup variables and one
  place to fail loudly instead of caching a boolean as a path.
- **A linked worktree confuses git under WSL.** An absolute Windows
  gitdir in `.git` (`gitdir: C:/Workspace/…/worktrees/<id>`) makes every
  git call under `/mnt/c` fail with
  `not a git repository: /mnt/c/…/C:/Workspace/…`: `host-guards` goes red
  on `git check-attr`, and `dpkg-buildpackage`'s git steps stop being
  meaningful. A **relative** gitdir (`gitdir: ../.git/worktrees/<id>`)
  works for both Windows git and WSL git. The "same LF/chmod/preclone
  steps" of the remote path inherits this if the copy is made from a
  worktree.
- **`Build-Depends` is not enough to build this package.** Measured on the
  Linux box, with every declared dependency satisfied: the suite's
  `tests/e2e/xim-harness.c` includes `<xkbcommon/xkbcommon.h>`
  unconditionally, `CMakeLists.txt` treats xkbcommon as **optional**
  (it warns and skips the library when `pkg_check_modules` misses), and
  `debian/control` does not name `libxkbcommon-dev`. CI installs it in the
  workflow's apt line instead, so the gap is invisible there and hard-fails
  on any host that trusts the packaging. Either the include gets a guard
  matching the CMake branch, or the package joins `Build-Depends`; that is
  a ruling, not a build-script change. `tools/deb-build-remote.sh` probes
  the header first so the failure arrives before the compile.

## Unified design

**One routine, both hosts, full automation, tier-gated suite.**
`tools/build-materials.sh` (new; run from Git Bash or `wsl bash`).

- Per-toolchain × mode dirs: `build/<toolchain>-<mode>/`, e.g.
  `build/wsl-debug/`, `build/wsl-release/`, `build/msvc-release/`,
  `build/android-<abi>/`. Debug and release never share a dir; the plain
  `cmake -B build && cmake --build build` stays as the fast debug path.
- Single fetches: one googletest source
  (`FETCHCONTENT_SOURCE_DIR_GOOGLETEST`), one shared keytable cache,
  one `VERSION` read, `INSTALL_GTEST=OFF`.
- `--fast` skips mutations (artifact labelled *fast*, not green); full
  mode stays the release gate (`docs-lint`, `doctor`, `ci-time-budget`).
- All channels staged under `build/release-<ver>/` with read-back
  verification (`dpkg-deb --info`, `apksigner verify`,
  `dumpbin -DEPENDENTS`, `SIZE.txt`), and the verification covers **every
  binary package a channel produces, not just the one the script happens
  to copy**: the deb build here emits `hunmin_<ver>_amd64.deb` and
  `hunmin-en-us_<ver>_all.deb`, and `tools/build-deb-wsl.sh` stages only
  the first, leaving the autoswitch data half in `$HOME` — a release
  directory that cannot satisfy its own `Recommends`.
- Full automation: bump versions in one commit, scaffold
  `docs/release-notes/<ver>.md`, stage channels, dry-run publish.
  Tag/publish stay `--yes`-gated; #182/#183 remain maintainer-closed.
- `cygpath` normalization becomes one sourced helper
  (`tools/host-env.sh`); `GT_TAG` read from `CMakeLists.txt`. The helper
  owns four things, each measured as a failure above: export the env file
  (`set -a; source …; set +a`) rather than only sourcing it; expose one
  `native_path()` (`cygpath -m`, identity elsewhere) for every path handed
  to a native program — `adb`, `python3`, `cmake`, `ninja`, `keytool`;
  resolve the JDK in both forms (`JAVA_HOME` as `C:/…` for the JVM,
  `/c/…/bin` for the shell); and assert, before any suite run, that
  `ctest -N` lists the device tiers the run claims to have measured.

## Dedicated Linux box (SSH, becomes CI runner)

When available, a permanently reachable Ubuntu 26.04+ box owns the
Linux side; the Windows host keeps TSF mock + installer + Android E1
while the box runs tier-A/D and `.deb` builds in parallel.

1. `HUNMIN_LINUX_HOST` (SSH target of the box) set → Linux branch
   available in `tools/build-materials.sh`. Unset → current
   Windows+WSL path, byte-identical behavior.
2. Flexible split: the script measures `deb build+test` vs
   `android build+test` cost and picks Linux only when it wins. If
   Windows+Android ≤ deb, only the `.deb` moves; else fcitx5/D1 + E1 +
   `.deb` all move. The measured choice is recorded, not hardcoded.
3. Remote build reuses the same script over SSH (`rsync` + same
   LF/chmod/preclone steps, minus the drvfs workarounds) and stages the
   `.deb` back to `build/release-<ver>/`.
4. CI maturation: the box becomes a self-hosted GitHub Actions runner
   running the identical `tools/build-materials.sh` entry point.

## Verification

1. `HUNMIN_LINUX_HOST` unset: routine behaves exactly as today.
2. Set + `--fast`: `.deb` from the box, APK/installer local, read-back green.
3. Full mode: suite green inside the remote `.deb`; timing vs WSL recorded.
4. Publish dry-run prints tag/upload commands, changes nothing.
5. **Registration before green.** `ctest -N` in each configured dir lists
   every device tier the run claims — `engine-android-e2e`,
   `engine-android-goldens`, `engine-android-espresso`, `android-mutations`
   — and the run prints the count it ran. A tier missing from `ctest -N`
   is the failure mode measured today: three of them were absent on this
   host for want of `adb` on PATH while the suite reported 97% passed.
6. **Channels verified as sets, not singles.** The deb pair
   (`hunmin_<ver>_amd64.deb` **and** `hunmin-en-us_<ver>_all.deb`),
   the APK with both ABIs present, the installer's staged DLL importing
   system DLLs only; each with bytes and a sha256 in the record, the shape
   used for the 0.3 check's hand-off.
7. **Staging is repeatable.** A fresh build dir and a previously used one
   both produce the same set — the second run is where a cached
   `-DFOO=ON` sitting in a `FILEPATH` slot shows up, as it did today in
   `build-msvc` (`LNK1181: … 'ON.obj'`).
