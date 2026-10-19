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

## The composition finding: native-TSF hosts only (rounds 8-14, 2026-10-20)

With activation and the trace fixed (rounds 10+), the sandbox rounds measured
the interactive rows' real blocker, and it is not the installer:

- **Modern TSF hosts keep the composition and typing composes.** Win11
  Notepad on the dev host (#98's own verification) and Edge in the sandbox
  (round 14: Hunmin in the switcher with the right icon, `layout_count=15`
  from the installed tree, and the maintainer's live judgement "it works
  quite well").
- **Default-sink hosts — classic `EDIT`, msftedit `RICHEDIT50W` (rounds
  11, 13) — terminate the composition after EVERY keystroke**, and the
  termination handler (tsf.cpp `OnCompositionTerminated`) drains the buffers
  and resets the engine (#98's never-deliver-twice rule). Result: every key
  evaluates its fresh/bare-jamo branch — measured on both hosts and both
  layouts (sin3-p2 `jfshta` → `ㅇㅍㄴㄴㅋㅇ`; dubeolsik `dkssud` → loose
  `ㅇㅏㄴㄴㅕㅇ`), `word len=0` on every key, Hanja source always empty
  (`count=0`), space/backspace passing through with nothing to act on.
- The tier-F1 mock never terminates compositions, which is why the whole
  corpus stays green while every classic host fails.
- The bisect ruled out the adapter's own per-key writes as the trigger:
  with the caret `SetSelection` (round 11) and the per-key display-attribute
  write (round 12) removed, the termination persisted on both hosts; both
  edits were reverted to the #98-verified shape. What remains in the write
  session is the minimal preedit set (GetSelection → StartComposition →
  SetText) — the same pattern every TIP uses.

So the TIP supports the native TSF composition path only. The legacy shape —
the pre-#98 overlay-window preedit with direct document commits, which is
what classic hosts need — was replaced by #98 and is gone from the tree.
Fix directions awaiting the maintainer's ruling: (a) ship 0.2 native-only
with this limitation documented; (b) restore the overlay/direct-insert
fallback for hosts observed terminating compositions (the pre-#98 code is
in git history; #98's native path stays for hosts that keep the
composition); (c) the TSF interim-input route as new work.

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

Status: **measured 2026-10-20, two Windows Sandbox rounds (scripted half) —
the interactive half measured 2026-10-20 rounds 8-14 (see the composition
finding above).** What the scripted rounds measured, from the mapped-folder
logs:

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

Interactive half, as measured in the sandbox (round 14, Edge as the host):
Win+Space shows Hunmin with the Sejong icon; typing composes (the
maintainer's live judgement: "it works quite well"); `layout_count=15`
lands in the mapped-folder log through the probe's redirection. Still owed:
the candidate-window row on a modern host (the Hanja key was only exercised
on classic hosts, where the source is always empty per the finding above) —
and, pending the maintainer's ruling on that finding, the classic-host
(rows 2-8 on `EDIT`/`RICHEDIT50W`) story entirely: they are where the
native-TSF-only shape is measured broken.
