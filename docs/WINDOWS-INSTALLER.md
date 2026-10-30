# The Windows 0.2 installation package (issue #99)

The 0.2 release requires an installation package a normal user reaches by
double-clicking (maintainer instruction 2026-10-18); the dev-period staging
— `regsvr32` by hand over a build tree — is not the release shape. This
document is the package's contract: what ships, what the installer runs,
what is measured, and what only a clean machine can still measure.

## What ships

| Installed path | What it is | Why it is there |
|---|---|---|
| `<install>\hunmin-tsf.dll` | the TIP | registered by the installer; the engine is static-linked inside it (the static archive is an intermediate, not an installed artifact — top-level `CMakeLists.txt`) |
| `<install>\data\...` | `libhunmin/data` shipped whole | the loader resolves `<module>\data` first (`hunmin_ctx_from_module`, `frontends/tsf/src/tsf.cpp`), so the installed IME serves word/idiom Hanja candidates from the installed tables; `krdict\` stays its own directory tree (CC-BY-SA ShareAlike isolation) |
| `<install>\settings-ui\...` | the self-contained WinUI 3 settings exe publish (issue #71's ruled settings surface) | the Start-menu entry points here, and that entry is the installer's alone: `DllRegisterServer` writes no `.lnk` (ruled 2026-10-30, issue #146). The interim dialog stays reachable as the rundll32 fallback |
| `<install>\LICENCE.txt`, `LICENSE.txt` | the licence page text, the LGPL-2.1-or-later text | the attribution terms in the package itself, beside `data\*\LICENSE.md` |

Default install dir: `C:\Program Files\Hunmin IME` (per-machine —
registration writes HKLM, so the installer runs elevated).

## What the installer runs

`frontends/tsf/installer/hunmin.iss` (Inno Setup 6; chosen because it runs
the issue's literal `regsvr32` step, and its licence page, recursive file
staging, Start-menu entry and Add/Remove-Programs uninstall map one to one
onto the issue's checklist):

1. copies the tree above (64-bit install mode, so `{sys}` is the real
   System32 and the 64-bit `regsvr32` runs — the 32-bit one refuses the DLL);
2. at post-install, `regsvr32 /s <install>\hunmin-tsf.dll` — the DLL's own
   `DllRegisterServer` writes the HKCR CLSID tree, the HKLM CTF TIP tree and
   the Korean language profile; the profile icon then points at the
   installed DLL's own path with icon index 0 (the pin the DLL rewrites
   itself, measured 2026-10-18: a stale index left the language entry
   blank). A nonzero exit re-runs regsvr32 visibly and fails the install
   loudly — a half-installed IME whose profile points at a registration
   that never happened is the failure mode this avoids;
3. Start menu: `Hunmin IME 설정` → `settings-ui\hunmin-tsf-settingsui.exe`.
   This is the ONLY Start-menu writer, and Inno's uninstall log is what removes
   it (ruled 2026-10-30, issue #146: the DLL stopped writing a per-user `.lnk`
   of the same name, which used to appear beside this one);
4. uninstall: `regsvr32 /u /s` BEFORE file deletion, and `DllUnregisterServer`
   removes the full registered state (issue #84: the CLSID tree, the HKLM
   CTF TIP tree, the four categories, the language profile). Windows' own
   HKCU CTF SortOrder/Assemblies store is deliberately not touched —
   registration never writes it (the #84 boundary, re-measured per round by
   `tests/manual-checklists/tsf-installer-fresh-vm.md`).

Installing over a dev-staged machine repoints the shared CLSID's
`InprocServer32` from the build-tree DLL to the installed one — last
registration wins; re-register the dev tree (`frontends/tsf/register-tsf.bat`)
to switch back.

## The CRT choice, and the clean-VM finding it answers

The 0.2 field test round 2 (clean VM, 2026-10-18) root-caused the fresh-host
load failure: the `/MD` TIP imports `MSVCP140.dll` / `VCRUNTIME140.dll` /
`VCRUNTIME140_1.dll`, which a clean Windows image does not carry. The issue
records both fixes as legitimate; the package takes the static runtime:
`HUNMIN_MSVC_STATIC_RUNTIME=ON` (default OFF — dev builds keep the `/MD`
default) sets `CMAKE_MSVC_RUNTIME_LIBRARY` for every MSVC target in the
configure, because the static engine lib and the TIP must agree (LNK4098
otherwise). The packaging build's dumpbin gate enforces it:

```text
dumpbin -DEPENDENTS hunmin-tsf.dll
  ole32.dll OLEAUT32.dll USER32.dll KERNEL32.dll GDI32.dll
  SHELL32.dll ADVAPI32.dll SHLWAPI.dll dwmapi.dll UxTheme.dll
```

— system DLLs only; no VC redist ships, no `vc_redist.x64.exe` (~26 MB) runs
at install time, nothing to forget on the next toolchain bump.

## Size budget, measured at packaging time

```sh
bash tools/build-windows-installer.sh   # prints it; writes build-msvc/installer/SIZE.txt
```

Measured 2026-10-19, this host (VS2022 BuildTools 14.44, dotnet 10.0.402,
Inno Setup 6.7.3):

| Part | Measured | Note |
|---|---|---|
| `hunmin-tsf.dll` | 0.5 MB | `/MT` Release; engine static-linked inside |
| `data\` | 7.5 MB | of which `krdict\krdict.json` is 6,046,975 bytes (`ls -l libhunmin/data/krdict/krdict.json`) |
| `settings-ui\` | 168.6 MB | the self-contained WinAppSDK publish — the #71 shape, whose gitignore note already measured the publish at 169 MB |
| `hunmin-0.2.0-setup.exe` | 45.8 MB | 47,985,039 bytes, lzma2 solid |

The issue's premise that `krdict.json` alone is ~13 MB did not hold: the
shipped extract is 5.8 MB. The package carries the committed extract, not
the 368 MB dump the extract derives from (`libhunmin/data/krdict/LICENSE.md`).

## Build it

```sh
bash tools/build-windows-installer.sh
```

The script: builds the core and the TIP under MSVC Release with the static
CRT (through a generated scratch `.bat` — vcvars is batch-native, and
cmd.exe mangles both LF-only batch files and forward-slash paths, both
measured), runs the dumpbin gate, publishes the settings exe, stages the
tree, compiles the installer with ISCC, and prints the size table. Host
requirements: VS2022 BuildTools, a dotnet SDK, Inno Setup 6 (`ISCC=`
overrides the detected path). Everything lands under `build-msvc/installer/`
(gitignored); no machine path enters the repository.

## Verified here vs owed to a clean machine

Verified on this host, from the built artefacts (2026-10-19):

- the installer compiles and stages the exact tree above (`SIZE.txt`,
  the ISCC log, `find build-msvc/installer/stage/data -type f | wc -l` → 29);
- the packaged DLL imports system DLLs only (the dumpbin gate);
- the tier-F1 mock harness is green on the static-runtime build
  (`ctest --test-dir frontends/tsf/build-msvc -C Release` — 7 tests, tier F1);
- docs gates: `python3 tools/docs-lint.py` errors 0.

Owed to the issue's own verification rows: the INTERACTIVE half of the
clean-machine round — Win+Space shows Hunmin with the Sejong icon → typing
composes → a multi-syllable word yields candidates from
`<install>\data\krdict\krdict.json` with `layout_count=15` in the debug log.
The SCRIPTED half is measured (two Windows Sandbox rounds, 2026-10-19, the
logs in the checklist): silent install exit 0 with no VC redist, install-path
registration (`IconFile=<install path>`, `IconIndex=0x0`), krdict + settings
exe present, silent uninstall exit 0, both `{CLSID}` trees gone, the tree
removed down to the uninstaller's own pending self-delete.

## Provenance

Produced 2026-10-19 on the issue worktree
`windows-0.2-release-requires-the-windows-install`, branch
`scobyseo/windows-0.2-release-requires-the-windows-install`, base 5d4206d.
Corrected premise: the issue's krdict size (measured above). The installer
technology (Inno Setup 6) and the static CRT are the two choices the issue
left as "either is legitimate"; the deciding reasons are recorded above.
`docs/REMAINING-WORK.md` and `AGENTS.md` deliberately untouched — shared
mutable files, parallel sessions may patch the same lines; board rows for
this issue go in after the merge.
