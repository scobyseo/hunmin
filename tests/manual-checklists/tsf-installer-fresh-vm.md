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

**Resolution (2026-10-20, issue #119): the ruling was "file issue to
implement legacy support"; the interim-input route (c) landed as the
runtime-detected legacy path** — a host-driven mid-word
`OnCompositionTerminated` flips the service to an interim delivery (no
composition; the word is doc text rewritten per key), composing across
keys on this machine's RICHEDIT50W and plain-EDIT probes
(`docs/TSF-INTERIM-INPUT-SPIKE.md` §3-D: the space now lands, fast
typing loses nothing, the one-char Hanja rule). The sandbox rounds of
this checklist are adjusted for the shipped shape when next run.

## The settings round trip (issue #120)

Sandbox round 10 observed a save to `dubeolsik` not reflected in typing
(`applied hangul=sin3-p2` at activation) nor on reopening the settings exe.
Re-measured on the dev host against the same sources (2026-10-20, UIA-driven
user path — visible-row click, 적용, relaunch, plus 3 scripted Win+Space
cycles):

- the save LANDS: `reg query "HKCU\Software\Hunmin\TSF" /v HangulLayout` reads
  the saved value after 적용, and the reopened exe restores it — the
  "save never lands" shape is disproven for a real row click;
- the TIP reads the store ONLY in `Activate()` (`tsf.cpp:613`); the settings
  exe is a separate process, so a save is not pushed to already-running
  hosts — the "no live apply" shape is the ruled state (the contract comment
  in `frontends/tsf/src/config.h`);
- a Win+Space round-trip (Hunmin → other IME → back) re-runs Activate() in the
  SAME host process and picks the save up — measured; so round 10's "typing
  still sin3-p2" is the expected no-round-trip state, not a lost save.

Still owed in the sandbox: round 10's own reg-query readback inside the VM
after a save (the issue's checkbox), which this host cannot substitute for
the INSTALLER-shaped exes.

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

## The modern-host row of issue #119, measured (issue #126, 2026-10-26)

Issue #126 asks for the one row of #119's verification checklist that no ctest
covers: *"modern hosts unchanged: Notepad (dev host) and Edge (sandbox round)
compose with no double delivery"*. Measured on the dev host (Windows 11
10.0.26300) from branch `scobyseo/windows-119s-modern-host-no-double-delivery-row`
at `e73cf57`.

The machine's install was stale before the round:
`C:\Program Files\Hunmin IME\hunmin-tsf.dll` was the 2026-10-24 09:32 package,
which predates the #128 shared-layer commits (`81cb3f8`, `66a446c`, `1430ab7`,
`7c007a6`, `8b8a2f9`, all 2026-10-25). The rounds ran this branch's image: the
dev-host rounds on a hash-verified swap into the installed path
(`sha256 9227b54d...`), the sandbox rounds on `hunmin-0.2.0-setup.exe` built by
`tools/build-windows-installer.sh` from the same tree.

The oracle for each row is the engine driven directly, never a hand-written
string: `./build-mingw/hunmin-sim.exe --data libhunmin/data --layout dubeolsik
--keys 'dkssud '` commits `안` then `녕` and passes the space through, so the
document text must be exactly `안녕 `.

| host | vehicle | text read back | delivery trace |
|---|---|---|---|
| Notepad | dev host, installed image | `안녕 ` | `applied hangul=dubeolsik`, six `consumed` rows, one `composition start`, one `composition end` at the space, no `interim:` row |
| Edge | dev host, installed image | `안녕 ` | same shape |
| Edge | Windows Sandbox, installer | `안녕 ` (dubeolsik) / `ㅎ곈디 ` (sin3-p2, the store default) | 10 rounds: 8 native, 2 flipped (below), 9 of 10 texts correct |

Reproduce the rows (the dev-host driver sets the store layout for the round and
restores it afterwards; the sandbox driver installs the mapped package, the ruled
vehicle for a sandbox round):

```sh
python3 tools/tsf-devhost-round.py --host notepad --layout dubeolsik --keys 'dkssud ' --expect '안녕 '
python3 tools/tsf-devhost-round.py --host edge    --layout dubeolsik --keys 'dkssud ' --expect '안녕 '
powershell -File tools/tsf-edge-round.ps1 -Keys 'dkssud ' -Expect '안녕 ' -Layout dubeolsik
```

The run dir each driver prints carries `result.json` plus the TIP log, so the
delivery path is readable from the same run as the text.

### The finding: a mid-word flip on Chromium loses the word

On the dev host's Edge, typing within a second of the window taking the
foreground terminates the composition mid-word (`OnCompositionTerminated ...
self=0`), the runtime detector takes the interim path, and on Chromium the
interim writes report `doclen=0 focused=0` — the control it resolved has neither
text nor focus — so the chained syllables never reach the document:

| settle after the window is foreground | rounds | texts read back |
|---|---|---|
| 0 s | 4 | `아 `, `안 `, `안녀 `, `안 ` — all short of the engine's `안녕 ` |
| 1 s | 2 | `안녕 ` |
| 2 s | 2 | `안녕 ` |
| 3 s | 1 | `안녕 ` |
| 5 s | 1 | `안녕 ` |

The trigger is therefore host timing, and the threshold sits between 0 s and 1 s
(it is not measured inside that interval). The trace of a lost round
(`build-tsf-devhost/edge-20261026-180953`):

```text
 consumed, display len=1 preedit=아
OnCompositionTerminated comp=000001F724523380 display=1 self=0
legacy detection: mid-word host termination (compText=1 display=1) -> interim path
interim: control hwnd=00000000001C0546
interim: span now 1 chars at 0 (caret=0 doclen=0 focused=1)
interim: span now 1 chars at 1 (caret=0 doclen=0 focused=0)
```

Two things are separable here: the flip is #119's designed fallback and is
measured healthy on the probe hosts (RICHEDIT50W, plain EDIT — the word composes
across keys after it), while on Chromium its writes land nowhere. A flip at the
boundary key is harmless by comparison: `install-20261026-161908` flips at the
space and the final text is still correct.

**In the sandbox the same signature occurred once in ten rounds and did not
recur.** `install-20261026-161448` terminated after key 1 and kept `ㅎ ` where the
layout produces `ㅎ곈디 `; nine repeats of that configuration (same installer,
same keys, same early typing) then took the native path with the correct text.
So the reproducible half is the dev-host timing above; the sandbox instance is a
single measurement, consistent with the same mechanism, that repetition did not
reach.

Tracked as issue #144 (measurement only; no fix attempted here).

**Fixed 2026-10-28 (issue #144), measured on this dev host:** the flip now
needs an EDIT-family focused window, and a host that is not one keeps the native
path with the fused copy re-adopted by the next composition. The same command
exits 0: `--settle 0` gives `안녕 ` in 4 of 4 rounds (`native recovery:
re-adopted 1-2 fused chars` in the trace), `--settle 3` in 2 of 2, and the
Notepad row (`--host notepad --settle 2`, 2 rounds) is the #126 modern-host shape
unchanged. The shape and the experiment rows behind it are in
`docs/TSF-INTERIM-INPUT-SPIKE.md` §3-F. Both sandbox rows are green from this
tree's build: the probe (RICHEDIT50W) still flips and composes `안녕`, and the
Edge round installed from `hunmin-0.2.0-setup.exe` reads `안녕 ` with one
composition start/end and no flip. The Chromium startup race itself does not
recur in the sandbox (no account popup), so the sandbox row cannot exercise the
recovery path — only the dev host can.

A Notepad round needs a FRESH process: Win11 Notepad is a packaged app, and a
leftover instance keeps the environment it was started with, so the TIP writes no
log in the run dir and the round fails as `profile not activated; applied rows:
[]`. The driver now closes stale Notepad windows first (`close_stale_notepad`).

Reproduce the failure:
`python3 tools/tsf-devhost-round.py --host edge --layout dubeolsik --keys 'dkssud '
--expect '안녕 ' --settle 0`.

Consecutive sandbox rounds need a pause: a second `WindowsSandbox.exe` launch
inside the previous instance's teardown fails as a duplicated instance and yields
an empty out folder with no guest at all (measured: `install-20261026-180452`
after `...180417` completed with no pause between them). `tools/tsf-edge-round.ps1` waits for the sandbox workers to exit and
then ~10 s before it launches.

Not measured by these rounds: any modern host other than Notepad and Edge; the
candidate-window row on a modern host (the Hanja key is still exercised only on
classic hosts); the installer's own rows, which the 2026-10-20 rounds above
carry. `docs/REMAINING-WORK.md` carries the assertion rows for this measurement
(item `TSF-MODERNHOST-126`) and for the flip below (`TSF-CHROMIUM-FLIP-144`);
the board is a shared file, so those rows were added at the close, after the
measurements were done, rather than while rounds were still running.

## Round 2026-10-31 — the 0.2.0 package rebuilt, one Start-menu writer (issue #146)

Windows Sandbox, clean image `10.0.26100.9711`, the package built from branch
`scobyseo/tsf-is-the-unregister-mirror-complete-the-start`, driven through the
`wsb` CLI (`start` → `share` → `exec --run-as System`) with the guest log in the
mapped folder. Guest steps in one batch: silent install → registry and tree
snapshots → silent uninstall → leftover snapshots. `HUNMIN_TSF_DEBUG_LOG=1` set
before the install so the installer's own `regsvr32` wrote
`hunmin-reg-debug.log` into the mapped folder.

| # | assertion | measured |
|---|---|---|
| 1 | silent install exits 0 on the clean image, no VC redist | `install rc=0` |
| 2 | `HKCR\…\InprocServer32` → the INSTALL path | `C:\Program Files\Hunmin IME\hunmin-tsf.dll`, `ThreadingModel=Apartment` |
| 3 | HKLM CTF TIP: profile + categories | `Description=Hunmin`, `IconFile=<install path>`, `IconIndex=0x0`; **five** `Category\Category` keys — `{046B8C80-…}` display-attribute, `{13A016DF-…}` **IMMERSIVESUPPORT**, `{25504FB4-…}` systray, `{34745C63-…}` TIP, `{534C48C1-…}` category-of-TIP |
| 4 | HKCU Enablement after install | `HKCU\…\TIP\{CLSID}\LanguageProfile\0x0412\{profile}\Enable = 0x1` |
| 5 | staged tree | `data\krdict\krdict.json`, `data\layouts\sin3-p2.json`, `settings-ui\hunmin-tsf-settingsui.exe`, `hunmin-tsf.dll` 534528 B |
| 6 | **exactly ONE Start-menu entry, the installer's** | all-users: `Hunmin IME 설정.lnk`; per-user: "cannot find the path specified" (no file) — the #146 ruling holds on a fresh account |
| 7 | the installer's own regsvr32 log | `Register hr=0`, `AddLanguageProfile hr=0`, `EnableLanguageProfile hr=0`, `RegisterCategory keyboard/category/dispattr/systray/immersive = 0` — all five, immersive included |
| 8 | silent uninstall exits 0 | `uninstall rc=0` |
| 9 | both `{CLSID}` trees gone | HKCR CLSID and HKLM CTF TIP both "unable to find" |
| 9b | per-user tree after uninstall | **before the ruling of 2026-10-31:** `HKCU\…\TIP\{CLSID}\LanguageProfile\0x0412\{profile}\Enable = 0x1` survived install + uninstall on this clean image. **After it (option A, round re-run from a fresh environment against the rebuilt package):** `reg query …\LanguageProfile` exits **1 — gone**; `HKCU\…\CTF\TIP\{CLSID}` remains as an EMPTY container (msctf's, not this registration's); `SortOrder` and `Assemblies` absent/untouched on a fresh profile |
| 10 | install dir residue | "File Not Found" — the tree is gone |

Row 6 is the one this round adds to the 2026-10-20 set: before the ruling the
DLL also wrote a per-user shortcut, so a fresh account saw two entries. Row 9b
is the Q2 residue, reproduced where no prior session could have written it —
so it is the TIP's own `EnableLanguageProfile`, not a dev-host artefact.

That residue is what the maintainer ruled on; the round was then re-run from a fresh
environment against the package carrying the fix, which is the `1 — gone` cell above.
Both numbers are kept — the residue is the evidence the ruling was made on, and
deleting it from the record would delete the reason it exists.

Not covered here: the interactive half (Win+Space icon, typed composition,
candidate window) — unchanged from the 2026-10-20 rounds, and the WT packaged-host
row is measured on the dev host in `docs/WINDOWS-TSF-REGISTRATION.md`
§"Packaged hosts".

Reproduce: `build/round146`-style guest batch; the round's own log was
`vm-round.log` in the mapped installer folder (a `wsb` share, not committed).
