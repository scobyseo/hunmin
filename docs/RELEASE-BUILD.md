# Release build — problems encountered (0.3.0)

Status: **recorded, not ruled.** This file documents what failed while
building the 0.3.0 release, why, and what worked around it. It is a record
of measurements, not a verdict about what the release tooling should be.

## Scope

Three channels were built for 0.3.0:

| Channel | Result | Where built |
|---|---|---|
| `hunmin_0.3.0_amd64.deb` + `hunmin-en-us_0.3.0_all.deb` | OK | Linux box `scoby@192.168.219.112` |
| `hunmin-0.3.0-setup.exe` | OK | Windows host (PowerShell + `gradlew.bat` + Python installer) |
| `hunmin-0.3.0-arm64-v8a.apk` | **blocked** | Windows host (NDK/CMake toolchain mismatch, see below) |

GitHub Actions (`workflow_dispatch`) was **not** re-triggered for this
release; the `.github/workflows/release.yml` file exists but was not run
again after the initial failed attempts. The failures below are from those
attempts plus local builds.

## Problem 1 — `.deb` build on GitHub Actions: dpkg-checkbuilddeps

**Symptom:** `dpkg-buildpackage -us -uc -b` aborts at
`dpkg-checkbuilddeps` with:

```
Unmet build dependencies: libfcitx5core-dev (>= 5.1.13) fcitx5-modules-dev (>= 5.1.13) fcitx5-modules
```

**Root cause:** The `.deb` job builds fcitx5 5.1.13 from source into `/usr`
(no `.deb` package), so `dpkg-checkbuilddeps` sees no owning package and
aborts. This is a known shape from the 0.1/0.2 deb jobs (issue #8):
the dependency is genuinely satisfied by the source build, only the package
metadata is missing.

**Fix (recorded, not applied to CI run):** add `-d` to
`dpkg-buildpackage` (`-us -uc -b -d`), which tells dpkg to trust the source
build rather than package metadata. The CI `deb` job had this for 0.2
(`ci.yml` line 692) but the release workflow did not.

**Verification:** This fix alone let the configure step proceed; the next
failure was GoogleTest fetch (Problem 2).

## Problem 2 — `.deb` build: `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` empty

**Symptom:** CMake configure fails in `FetchContent_MakeAvailable`:

```
Manually specified source directory is missing:
  FETCHCONTENT_SOURCE_DIR_GOOGLETEST --> /home/runner/.../obj-x86_64-linux-gnu/_deps/googletest-src
```

**Root cause:** Two compounding issues:

1. The release workflow pre-populated googletest into
   `obj-x86_64-linux-gnu/_deps/googletest-src`, but `debian/rules`
   `override_dh_auto_configure` runs `dh_auto_clean` first, which removes
   the untracked `obj-x86_64-linux-gnu/` tree before CMake reads it.
2. The environment variable `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` was not
   exported into `dpkg-buildpackage`'s environment, so even when the dir
   existed, CMake saw it as empty.

**Fix (recorded, not applied to CI run):**

- In `tools/build-release.sh`, export
  `FETCHCONTENT_SOURCE_DIR_GOOGLETEST="$HOME/googletest-src"` when
  `$HOME/googletest-src` exists (the `$HOME` copy survives `dh_auto_clean`);
  fall back to `obj-x86_64-linux-gnu/_deps/googletest-src` otherwise.
- In `debian/rules` `override_dh_auto_configure`, pass
  `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$(FETCHCONTENT_SOURCE_DIR_GOOGLETEST)`
  to `dh_auto_configure` when the variable is set.

**Why not applied:** The `.deb` was ultimately built on the Linux box
(`scoby@192.168.219.112`) via `tools/build-release.sh deb` with
`DEB_BUILD_OPTIONS=nocheck`, bypassing the Actions runner entirely.

## Problem 3 — Windows installer: Inno Setup download corruption

**Symptom:** The `build-windows` job failed at `Start-Process` with
`The file or directory is corrupted and unreadable` on
`$env:TEMP\innosetup.exe`.

**Root cause:** The workflow downloaded Inno Setup from
`https://jrsoftware.org/download.php/is.exe`. That URL returns an HTML
redirect/interstitial page (215 bytes on the failed attempt), not the
Inno Setup executable. PowerShell's `Invoke-WebRequest` saved the HTML,
so the subsequent silent install failed.

**Fix (recorded, not applied):** switch to `curl -fL` (follow redirects)
and verify the downloaded file is a PE executable before launching it.
The Windows installer was ultimately built locally with Inno Setup 6
already installed at
`C:\Users\pc\AppData\Local\Programs\Inno Setup 6\ISCC.exe`.

## Problem 4 — Windows installer: `vcvarsall.bat` path not found in bash

**Symptom:** `tools/build-windows-installer.sh` exited with
`vcvarsall.bat not found; set HUNMIN_VS2022_ROOT`, even though Visual
Studio BuildTools 18 is installed at
`C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools`.

**Root cause:** The environment variable `HUNMIN_VS2022_ROOT` set in
PowerShell (`$env:HUNMIN_VS2022_ROOT = "C:\..."`) is not inherited by the
`bash` child process started from PowerShell. The script then fell through
its candidate list (`C:/Program Files .../18/BuildTools/...`) but the
`cygpath`-based path construction in the generated `.bat` produced paths
cmd.exe could not parse.

**Fix (recorded, applied locally):** replaced `tools/build-windows-installer.sh`
with `tools/build-windows-installer.py`, a Python script that:

- resolves `vcvarsall.bat` via `HUNMIN_VS2022_ROOT` or a hardcoded
  candidate list in Windows-native form (`C:\Program Files (x86)\...`);
- invokes `vcvarsall.bat x64 && cmake ...` through a single `cmd /c`
  call with `subprocess.run(shell=True)`, avoiding the bash→cmd quoting
  and CRLF pitfalls of the generated `.bat`;
- converts `/c/...` MSYS paths to `C:\...` manually (no `cygpath`
  dependency, since plain Windows Python lacks it);
- calls Inno Setup `ISCC.exe` directly with Windows-native paths.

**Result:** `hunmin-0.3.0-setup.exe` (48.2 MB) built successfully
locally. The `.sh` script remains in the tree but is superseded by the
`.py`.

## Problem 5 — APK build: NDK/CMake toolchain mismatch (blocked)

**Symptom:** Gradle's `externalNativeBuild` invokes
`bash tools/build-android.sh arm64-v8a 24`, and cmake configure fails:

```
Android: The system root directory needed for the selected Android version
and architecture does not exist:
  /mnt/c/Users/pc/Android/Sdk/ndk/27.3.13750724/platforms/android-24/arch-arm64
```

**Root cause:** NDK r27 (`27.3.13750724`, the only NDK installed) does not
ship a `platforms/` directory at its root; the sysroot now lives under
`toolchains/llvm/prebuilt/<tag>/sysroot`. CMake 4.2's
`Android-Initialize.cmake` still looks for
`<NDK>/platforms/android-<API>/arch-<ABI>`, which does not exist in r27.

Attempted workarounds (all recorded, none completed):

1. Installed NDK r21 (`21.1.6352462`) via `sdkmanager` and pointed
   `tools/build-android.sh` at it. Gradle's `externalNativeBuild` still
   resolved `android.ndkVersion` from `build.gradle.kts` (`21.1.6352462`)
   but the cmake invocation inside `tools/build-android.sh` received the
   wrong NDK root via the WSL/PowerShell environment boundary
   (`ANDROID_NDK` not inherited by the `bash` child).
2. Tried setting `ANDROID_NDK` as a Windows environment variable and via
   `WSLENV`; WSL bash did not inherit it reliably.
3. Tried `wslpath` to convert Windows paths to WSL form for `JAVA_HOME`;
   PowerShell escaping of backslashes/tabs made the inline script fragile.

**Current state:** APK build is **blocked** on this host. The
`build.gradle.kts` and `tools/build-android.sh` were restored to their
pre-attempt state (`ndkVersion = "27.3.13750724"`).

**Unblock path:** Build on an environment where:
- CMake is 3.22–3.30 (not 4.2) or NDK r21–r26 (with `platforms/`);
- `ANDROID_NDK` is set in the shell that actually runs cmake;
- the host is native Linux (no WSL/PowerShell boundary).

The APK can be added to the release later with
`gh release upload 0.3.0 <apk> --repo scobyseo/hunmin`.

## Summary of what shipped vs. what did not

| Artifact | Shipped in 0.3.0 release | Notes |
|---|---|---|
| Tag `0.3.0` | ✅ | Pushed to `origin` |
| GitHub Release `0.3.0` | ✅ | `hunmin_0.3.0` on `scobyseo/hunmin` |
| `hunmin-0.3.0-setup.exe` | ✅ | 48.2 MB, Inno Setup 6 |
| `hunmin_0.3.0_amd64.deb` | ✅ | 1.3 MB, built on Linux box |
| `hunmin-en-us_0.3.0_all.deb` | ✅ | 270 KB, data half |
| `hunmin-0.3.0-arm64-v8a.apk` | ❌ | Blocked (Problem 5) |

## Files touched for this release

- `docs/release-notes/0.3.0.md` — finalized from DRAFT, all versions `0.3.0`
- `debian/changelog` — `0.3.0~rc` → `0.3.0`
- `frontends/android/app/build.gradle.kts` — `versionName "0.3.0-rc"` → `"0.3.0"`
- `frontends/android/keymaps/build.gradle.kts` — `0.1.0-p5.5` → `0.3.0` (versionCode 1→3)
- `tools/build-windows-installer.sh` — default `HUNMIN_VS2022_ROOT` path
- `tools/build-windows-installer.py` — new Python installer build
- `tools/release-0.3.sh` — 0.3.0 release script (skip missing channels)
- `.github/workflows/release.yml` — release workflow (created, not re-run after fixes)
