# Plan — 0.3 feature check under #183 (handoff for another agent)

Audience: the next agent picking up milestone **0.3 — automatic
Hangul/English switch** in this worktree/branch
(`scobyseo/modes-milestone-0.3-automatic-hangul-english-swi`).
Requirement home: `docs/SPEC-MODES.md` (R1–R15, E1–E8).
Tracker: issue #183 (stays OPEN — maintainer closes).
Release close-out: issue #182 (stays OPEN).

**Not in scope:** no tag `0.3.0`, no `gh release`, no version bump
(`CMakeLists.txt`, `debian/changelog`, Android `versionName`,
installer `AppVersion` — all still `0.2.0`), no B2/E4 pass-line ruling,
no fix for #239 (0.4), no close of #102/#83.

## Current state (measured 2026-11-07)

- Phase issues #159–#181 on milestone 1 are CLOSED; only #182/#183 remain
  OPEN. Open issues elsewhere: #265 (reopened by fix 1), #239 (0.4),
  #102 (billing), #83 (mock audit). All are list-only per maintainer
  choice.
- Draft release notes: `docs/release-notes/0.3.0.md` (measured R/E claims,
  each with its ctest id; includes README staleness findings and owed
  debts). Every behaviour claim must cite a ctest/command (issue #182).
- `README.md` fixed: TSF `(planned)` removed; status now
  `P0–P6 implemented; 0.3 in check`; install points at 0.2 / 0.3-draft;
  `SPEC-MODES.md` linked. `docs-lint: 0 errors`, `doctor: 0 errors`.
  Known remaining debt: keymaps APK still `0.1.0-p5.5`.
- Fixes already on this branch (uncommitted):
  1. Portable `getenv` guard: `libhunmin/tests/{wordset,buffered-preedit,hangul-valid,key-rule}-test.cpp`
     gate `_dupenv_s` behind `defined(_WIN32) && defined(_MSC_VER)` —
     Linux/WSL + llvm-mingw broke after #265. Issue #265 **reopened**.
  2. Android symlink fixture fallback to copy:
     `AutoSwitchGateTest.kt`, `AutoSwitchSettingsTest.kt`
     (`copyTree` + split teardown). JVM symlinks need Developer Mode;
     3 tests skipped without it.
  3. `tools/jamo-keytable.py`: `SIM_OVERRIDE` so `modes-metrics.py`
     `--sim` is honoured by the jamo probe (was hardcoded `build/hunmin-sim`,
     broke `modes-metrics-selfcheck` on non-`build` dirs).

## Check results (second pass — all four host lanes re-run)

| Host | Configure | Result |
|---|---|---|
| Windows MinGW `build/` (Ninja) | ✅ | 99/99 pass, 4 skipped (`keymap-fixture`, `builds-fcitx5-addon`, `modes-corpus-selfcheck`, `align-16k`); includes `engine-wordset`, `engine-golden-judgement`, `engine-off-identity`, `docs-lint`, `req-map` |
| MSYS Git Bash `build-mingw/` (llvm-mingw) | ✅ | 99/99 pass, same 4 skips; **with `$ANDROID_HOME/platform-tools` on PATH before configure**, 105 tests register and the Android lanes join it — `engine-android-e2e` passes in the harness shell, and `android-mutations` (290 s), `engine-android-goldens` (11 s), `engine-android-espresso` (43 s) pass once MSYS argument conversion is restored (`env -u MSYS_NO_PATHCONV -u MSYS2_ARG_CONV_EXCL`) |
| MSYS Git Bash Android JVM (tier E1) | ✅ | `HUNMIN_BUILD_DIR=…/build-mingw bash tools/android-unit.sh` → **PASS 157/157** |
| WSL Ubuntu 26.04 `build-wsl/` (fcitx5 5.1.19) | ✅ | 113/113; includes `host-guards`, `frontend-mutations`, `parity-sweep`, tier D1/D2, `docs-lint`, `req-map`, `modes-metrics-selfcheck`; tier D3 (`engine-d3-x11/gtk3/gtk4/edit/candidate`) is registered but **Skipped**, and the 12 android-conformance cases skip too |
| MSVC `build-msvc/` (VS 17 generator, 14.44 toolset) | ✅ | `ctest -C Release` → 124/124; tier-F1 25 (incl. `tsf-test`), tier-C 19 |

`frontend-mutations` inside the WSL lane reports its recorded set
unchanged: ten mutations `guarded`, one `GAP` (`I12-autoswitch-not-reapplied`
— the recorded gap, not a regression), and `I4-shift-chord` as
`build rejects` (`KeyState::Shift_L` does not exist, so the mutant cannot
compile and proves nothing either way).

Coverage NOT earned, each for a named reason:

- **fcitx5 live round (X11/GTK)** — WSLg is present (`DISPLAY=:0`,
  `WAYLAND_DISPLAY=wayland-0`) but the guest has no fcitx5 installed and
  `sudo` requires interactive authentication, so the `dpkg -i` +
  `fcitx5-diagnose` step of `tests/manual-checklists/release-0.1.md` §3.1
  cannot run unattended.
- **Live TSF round (Notepad)** — needs a human at the keyboard; the
  registered TIP is the maintainer's 0.2.0 install.
- **Corpus metrics** — `tools/modes-metrics.py` needs a corpus passed as
  an argument; no corpus lives in the repository by licence.
- **Android E2/E3** — the earlier "no hypervisor here" premise did not
  hold; measured in the next section.

## Android E2 (tier `engine-android-e2e`)

The premise behind the earlier "emulator — no hypervisor here" line was
re-measured and does not hold on this host:

```
$ emulator -accel-check
accel: 0
WHPX(10.0.26300) is installed and usable.
$ emulator -list-avds
hunmin-e2e
```

So `tools/android-e2e.sh` (boots the AVD headless, installs the debug
APKs, types through the platform IME and reads the committed text back
with `uiautomator dump` — the assertion is on-screen text, never a log)
is measurable here. **Result: PASS.** The field read back `ㅎ곈디` —
byte-identical to the tier A golden — the settings window opened with its
four sections, and a chord captured in the settings window (F9) was the
key the running IME fired.

Three script defects had to be fixed on the branch before the lane could
run, all of them hidden by the tier never being registered here:

1. **`adb` was not on PATH at configure time**, so
   `find_program(ADB_EXECUTABLE adb HINTS $ENV{ANDROID_HOME}/platform-tools)`
   failed and `ctest -N` listed no `engine-android-e2e` at all. The tier
   existed, the script existed, and nothing could reach it — the shape
   issue #13 records. Recipe: put `$ANDROID_HOME/platform-tools` on PATH
   **before** configuring.
2. **A stale assertion** demanded the auto-toggle row read `미정 (0.3)`.
   Issue #174 made the action live and `OverlayActivity.NOT_SET`
   (`설정 안 됨 (고정 키 사용)`) replaced that wording; the B4 ruling the
   assertion encodes (no chord until the user sets one) is unchanged. The
   assertion now reads the value node that follows the row title instead
   of the wording, so a future label change cannot fail it.
3. **Host-path normalisation.** `adb` is a native program; a caller whose
   shell disables MSYS argument conversion (`MSYS_NO_PATHCONV=1`,
   `MSYS2_ARG_CONV_EXCL=*`) hands it `/c/…` verbatim and gets
   `failed to stat /c/…/app-debug.apk: No such file or directory`. The
   script now normalises both APK paths with `cygpath -m`, the way
   `tools/build-android.sh` already does for cmake/ninja, so the tier does
   not depend on the caller's shell.

Proof of (3) as measured, against a booted `hunmin-e2e`:

```
$ adb install -r -g /c/…/app-debug.apk                  # conversion off
adb.exe: failed to stat /c/…/app-debug.apk: No such file or directory
$ env -u MSYS_NO_PATHCONV -u MSYS2_ARG_CONV_EXCL adb install -r -g /c/…/app-debug.apk
Success
```

The debug APKs come from
`frontends/android/gradlew :app:assembleDebug :testeditor:assembleDebug`.

## Host findings measured this pass (each cost a failed run first)

0. **Three Android tiers were registered nowhere on this host, and three
   scripts break when the caller's shell disables MSYS argument
   conversion.** With `adb` absent at configure time
   (`find_program(ADB_EXECUTABLE adb HINTS $ENV{ANDROID_HOME}/platform-tools)`),
   `ctest -N` listed no `engine-android-e2e`, `engine-android-goldens` or
   `engine-android-espresso` — the scripts existed, the board named them,
   and no `ctest -R` could reach them. With `platform-tools` on PATH they
   register (105 tests in `build-mingw`) and all three of
   `android-mutations` (`python3.exe: can't open file 'C:\c\Workspace\…'`),
   `engine-android-goldens` (`adb: error: cannot stat '/c/…'`) and
   `engine-android-espresso` (`adb.exe: failed to stat /c/…/app-debug.apk`)
   fail on the same cause: this agent's shell exports `MSYS_NO_PATHCONV=1`
   and `MSYS2_ARG_CONV_EXCL=*`, so native programs get `/c/…` verbatim.
   Run with `env -u MSYS_NO_PATHCONV -u MSYS2_ARG_CONV_EXCL` they pass
   (290 s / 11 s / 43 s). `tools/android-e2e.sh` no longer depends on the
   shell because of the `cygpath -m` fix above; the other three still do —
   hardening them the same way is a judgement call, recorded, not made.

1. **`source ~/.hunmin-env` does not export.** The file is plain
   `KEY=value`, so children (`tools/build-android.sh`, `dpkg-buildpackage`)
   saw an empty `ANDROID_NDK` and died at
   `: "${ANDROID_NDK:?export ANDROID_NDK=…}"`. Recipe:
   `set -a; source ~/.hunmin-env; set +a`.
2. **`JAVA_HOME` is written in `C:/` form**, which MSYS cannot resolve as
   a PATH entry — this is the old `keytool: command not found` blocker.
   Keep the `C:/` form for native JVM consumers, but prepend
   `/c/Users/pc/toolchains/jdk-17.0.20.1+1/bin` for bash lookup.
3. **`build-msvc` carried a poisoned cache entry**
   `HUNMIN_STATIC_LIB:FILEPATH=<repo-root>/ON` (a stale
   `-DHUNMIN_STATIC_LIB=ON` configure, converted to an absolute path),
   which made the TIP link fail with `LNK1181: … 'ON.obj'`. Fixed with
   `cmake -B build-msvc -UHUNMIN_STATIC_LIB`; the standalone
   `frontends/tsf/build-msvc` tree had the correct value all along.
4. **A linked Windows worktree is invisible to WSL git.** The `.git` file
   held `gitdir: C:/Workspace/hunmin/.git/worktrees/<id>`; every git call
   under `/mnt/c` failed with `not a git repository: /mnt/c/…/C:/Workspace/…`,
   which failed `host-guards` on `git check-attr`. Rewriting the file as a
   **relative** gitdir (`gitdir: ../.git/worktrees/<id>`) works for both
   Windows git and WSL git; the WSL suite went green after it and
   `git worktree list` is unaffected. The original line is backed up under
   the session scratch dir.
5. **The deb release dir cannot satisfy its own Recommends.**
   `tools/build-deb-wsl.sh` stages `hunmin_<v>_amd64.deb` only;
   `hunmin-en-us_<v>_all.deb` (the autoswitch data half) is built and left
   in `$HOME`. With the data package absent the feature is refused by
   design (B3), so the staged directory is an incomplete pair — a #182
   decision, recorded here, not patched.

Host commands used (MSYS/Git Bash):
`& "C:\Program Files\Git\bin\bash.exe" -lc 'set -a; source ~/.hunmin-env; set +a; …'`.
WSL via `wsl bash -c 'cd /mnt/c/Workspace/hunmin/<worktree>; …'`.

## Install artifacts (built this pass, versions still 0.2.0)

| Artifact | Path | Bytes | sha256 |
|---|---|---|---|
| Windows installer | `build-msvc/installer/hunmin-0.2.0-setup.exe` | 48,249,640 | `542c302aceec25e3b2be95ee97e6f92dc2c5da64e3d446c4180cff580eb61f7e` |
| · staged TIP | `build-msvc/installer/stage/hunmin-tsf.dll` | 0.5 MB | `08981a4f4d6322ff57d4d1d9b580d5af4a74f4f2060f91bd383bd711fd2d371f` |
| Debian | `build/release-0.2.0/hunmin_0.2.0_amd64.deb` | 1,290,686 | `802430073a3353d6e70bcf26d06b0ff9e3014e09f753be640d661b07f28c42dc` |
| · data half (not staged) | `~/hunmin-en-us_0.2.0_all.deb` (WSL guest) | 269,512 | `49b88eaadc5d949f18aa6635efd132760558020204b2c733f0fefe1c44f9daff` |
| Android APK | `frontends/android/app/build/outputs/apk/release/app-release.apk` | 1,923,686 | `ea2d600df1b5588db8204e956d30bdbad2eed9544304f256330e9faa142fe2fd` |

Read-backs performed:

- installer: `dumpbin -DEPENDENTS` on the staged DLL lists system DLLs only
  (`ole32, OLEAUT32, USER32, KERNEL32, GDI32, SHELL32, ADVAPI32, SHLWAPI,
  dwmapi, UxTheme`) — no `VCRUNTIME14`/`MSVCP14`, so `/MT` holds. Size
  budget in `build-msvc/installer/SIZE.txt` (settings-ui 168.6 MB, data
  9.0 MB, installer 46.0 MB).
- deb: `dpkg-deb --info/--contents` — `Version: 0.2.0`, `Depends: libc6
  (>= 2.38), libfcitx5* (>= 5.1.19), fcitx5`, `Recommends: hunmin-en-us`;
  ships `usr/lib/x86_64-linux-gnu/fcitx5/libhunmin.so`,
  `libhunmin.so.0.2.0`, `usr/share/fcitx5/{addon,inputmethod}` and
  `usr/share/hunmin/*`.
- APK: both ABIs present (`lib/arm64-v8a`, `lib/x86_64`), each built fresh
  from this tree; `apksigner verify --print-certs` → `CN=hunmin-ci,
  O=hunmin, L=CI, C=KR` (the throwaway keystore from
  `tools/build-release.sh`, not a release-grade signature).

## Next steps (todo order)

1. **Manual rounds that remain blocked** — fcitx5 (needs an interactive
   `sudo` in the WSL guest) and the live TSF round (needs a human at the
   keyboard, and the registered TIP belongs to the maintainer's 0.2.0
   install). Record the blocker; do not fake the round.
2. **Hand-off:** one comment on #183 with the per-host gate output, the
   artifact table above, the manual-round notes and the owed debts
   (E4/B2 ruling, #239, #102, #83, keymaps `0.1.0-p5.5`, the deb
   `en-us` staging gap). Update `docs/release-notes/0.3.0.md` only after
   the manual test. No `git tag`, no close.

## Rules for this work

- One branch/worktree (this one); agent never closes issues — maintainer does.
- Requirement text lives in `docs/SPEC-MODES.md` only; cite ids elsewhere.
- Analysis presents designs as comparable; no agent verdicts.
- Prose in English; Korean only in data files.
- Commits keep `ctest` green; data changes pass `tools/doctor.py`.
- Time budgets: `tools/ci-time-budget.sh build` before anything adds a test.
- Golden/metrics rules: use `tools/jamo-keytable.py` /
  `tools/sin3p2-ksgen.py` / `tools/modes-metrics.py`; never hand-derive keys.
- Bare test counts never belong in prose (`docs-lint` enforces).
