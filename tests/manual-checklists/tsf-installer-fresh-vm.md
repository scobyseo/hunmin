# Windows installer — fresh-machine round (issue #99)

**Why this checklist is a manual tier.** The 0.2 installation package is the
release shape: a user double-clicks one exe on a machine that has none of the
dev host's state (no VS BuildTools, no VC redist, no repo checkout, no stale
registration). No ctest tier can claim that — the gates `tools/build-windows-
installer.sh` can measure (the static-CRT dumpbin gate, the staged tree) are
measured there; what only a clean machine measures is collected here.

## Run it

Fresh Windows VM or Windows Sandbox (the field-test rounds of 2026-10-18 used
Windows Sandbox). A `.wsb` skeleton, with the worktree path substituted before
use (the sandbox maps folders read-only unless `ReadOnly=off`):

```xml
<Configuration>
  <MappedFolders>
    <MappedFolder>
      <HostFolder>%WORKTREE%\build-msvc\installer</HostFolder>
      <ReadOnly>false</ReadOnly>
    </MappedFolder>
  </MappedFolders>
  <LogonCommand>
    <Command>cmd /c start cmd /k "C:\Users\WDAGUtilityAccount\Desktop\installer\hunmin-0.2.0-setup.exe"</Command>
  </LogonCommand>
</Configuration>
```

The mapped folder carries the built installer (`hunmin-<version>-setup.exe`),
the probe host `ime-probe.exe` (built from
`frontends/tsf/test/ime-probe.c`; the build command is in its file header —
MSVC, static CRT, system-DLL imports only, same gate as the TIP), and this
round's logs back out (`ReadOnly=false`).

## Steps

```text
1. run hunmin-<version>-setup.exe (double-click; accept the elevation prompt)
2. Win+Space                -> Hunmin appears, Sejong icon, NOT blank
3. start ime-probe.exe      -> from the mapped folder, so the TIP debug log
   (follows the host process's working directory) lands IN the mapped
   folder and survives the uninstall -- the readback round 2 lost. There is
   no Notepad on the sandbox image (see the finding below).
4. switch to the probe, type dks sud / dkssud + Hanja key
5. reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-...-0001}" /s
6. reg query "HKCR\CLSID\{F2C70001-...-0001}" /s
7. uninstall (Settings > Apps, or {app}\unins000.exe), then repeat 5-6
8. check the install directory is gone
```

## What to look for, and why each probe is there

| step | expected | what it isolates |
|---|---|---|
| 2 | the language list shows Hunmin with the Sejong icon | the profile icon points at the INSTALLED DLL (`IconFile=<install path>`, index pinned 0 by `DllRegisterServer` itself); a blank entry is the stale-index regression measured 2026-10-18 |
| 3-4 | Hangul composes; a multi-syllable word + the Hanja key yields candidates | the TIP loads on a machine with NO VC redist — the `/MT` build's whole point (the 2026-10-18 field test root-caused the `/MD` build to die on `MSVCP140.dll` here) |
| 4 | `<install>\data\krdict\krdict.json` exists; the TIP debug log (`hunmin-tsf-debug.log`, following the host app's working directory) shows `layout_count=15` | candidates come from the INSTALLED data tree, not a checkout climb — the loader (`hunmin_ctx_from_module`) resolves `<module>\data` before the repo fallback, and on a clean machine no checkout exists |
| 5 | profile paths point at `<install>`, never a build-tree path | the issue's own verification row |
| 7 | both `{CLSID}` trees absent after uninstall | the uninstall runs `regsvr32 /u` on the installed DLL BEFORE deleting files; `DllUnregisterServer` removes the full registered state (issue #84: CLSID tree, HKLM CTF TIP tree, categories, profile) |
| 7 | HKCU CTF SortOrder/Assemblies: record exactly what remains and why | Windows' own store, written when the user actually selects the IME; registration never writes it and the uninstall must not clean it (the #84 boundary) — a stale slot with the profile gone is the recorded expectation |
| 8 | `{app}` gone | Inno removes the files it installed; the data tree ships inside `{app}\data`, so nothing is left by design |

## The host app is ime-probe, not Notepad (finding, 2026-10-20)

The sandbox image has no Notepad to start: `System32\notepad.exe` on this
Windows 11 image is the Store-app alias and does not launch inside Windows
Sandbox (measured when round 2's helper script, the dev-period
`tsf-sandbox-install.bat`, ended with exactly that line). The round's host
app is `ime-probe.exe` instead: one RichEdit window (RICHEDIT50W from the
inbox `msftedit.dll`), TSF-aware like Notepad, so the #98 native composition
renders inline; it falls back to a plain EDIT control (system floating
preedit) if `msftedit.dll` is missing, and its title names which control is
in use. Static CRT, system-DLL imports only — it loads where the TIP loads.

Round 7 (2026-10-20 00:06, the scripted driver + Edge over `target.html`)
measured the interactive rows' first blocker: the profile activation did not
survive the round. The driver's Win+Space taps never landed on Hunmin, the
tray showed only the default `A` + Hangul pair, `dkssud` committed as raw
Latin, and the copied debug log held only the ungated `DllMain PROCESS_ATTACH`
line (`HUNMIN_TSF_DEBUG_LOG` was never set in the host process). Round 8's
changes, all in the gitignored round cmd: the ko-KR language list is added
BEFORE the install (the post-install `Set-WinUserLanguageList` call is the
prime suspect for resetting the active processor to the MS IME default),
`HUNMIN_TSF_DEBUG_LOG=1` is set machine-wide before the host launches, and
the probe itself activates the Hunmin profile
(`ITfInputProcessorProfiles::ActivateLanguageProfile` — the same call
`DllRegisterServer` makes, `tsf.cpp:2460`; its `hr` lands in the probe log).

## Known-sharp edges, recorded rather than hidden

- The uninstall ordering claim (Inno runs `[UninstallRun]` before deleting
  files) is what the uninstaller's own `regsvr32 /u` depends on — step 7 is
  the probe that would catch it being wrong.
- A failed registration mid-install must fail LOUDLY (`hunmin.iss`'s
  `CurStepChanged` re-runs regsvr32 visibly and aborts): record the dialog
  if it ever appears, plus regsvr32's exit code.
- The WinAppSDK self-contained settings exe needs no store/runtime install —
  if the Start-menu entry (`Hunmin IME 설정`) fails to launch, that is a
  finding, not a prerequisite to add.

Status: **measured 2026-10-19, two Windows Sandbox rounds (scripted half) —
the interactive rows below are the owed half.** What the rounds measured,
from the mapped-folder logs:

- the silent install (`/VERYSILENT`) exits 0 on the clean image — no VC
  redist, no missing-`MSVCP140.dll` failure (the `/MT` fix, on the artefact);
- `reg query` after install: the CLSID `InprocServer32`, the HKLM CTF TIP
  tree (`Enable=1`, the category trees) and the language profile
  (`0x00000412`, `{F2C70002-...-0002}`, `IconFile=C:\Program Files\Hunmin
  IME\hunmin-tsf.dll`, `IconIndex=0x0`) — all pointing at the INSTALL path;
- `data\krdict\krdict.json` and `settings-ui\hunmin-tsf-settingsui.exe`
  present in the install dir;
- the silent uninstall exits 0 and BOTH `{CLSID}` registry trees read
  "unable to find" afterwards; the tree is fully removed (the
  `[UninstallDelete]` fix — the TIP's own debug log used to pin the
  directory), leaving only `unins000.exe` itself: the uninstaller cannot
  delete its own binary; its self-delete pends a reboot (the sandbox's
  10-second shutdown cut it short).

Owed: the interactive rows — Win+Space shows Hunmin with the Sejong icon,
typing composes in the probe, a multi-syllable word yields candidates,
`layout_count=15` in the debug log. The readback problem is solved by the
probe's launch directory: started from the mapped folder, the TIP debug log
lands in the mapped folder and survives the uninstall, so the host copy is
made before the round ends rather than before the uninstaller runs.
