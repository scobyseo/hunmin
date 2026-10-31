# Windows TSF registration and event propagation for Hunmin

How `hunmin-tsf.dll` is registered, what registry state that produces, and how a
key crosses from the host application to the engine and back. What must hold is
`docs/SPEC-TSF.md` — TSF16 (the langbar item) and TSF17 (unregistration mirrors
registration) — and the key path's rules are TSF1/TSF7/TSF8 with
`docs/SPEC-FRONTEND.md` FE2/FE4.

Provenance: measured on Windows 11 (26200), Windows SDK 10.0.26100, VS BuildTools
14.44, 2026-10-16/17. GUID literals not visible in the measured registry state
are named only.

## Constants

| Constant | Value |
|---|---|
| `CLSID_HunminTSF` | `{F2C70001-0000-0000-0000-000000000001}` |
| language-profile GUID | `{F2C70002-0000-0000-0000-000000000002}` |
| language id | `0x0412` (Korean) |
| `GUID_TFCAT_TIP_KEYBOARD` | `{34745C63-B2F0-4784-8B67-5E12C8701A31}` |
| `GUID_TFCAT_CATEGORY_OF_TIP` | declared in lib headers; referenced by name only |

## The regsvr32 flow

`regsvr32.exe <abs path>\hunmin-tsf.dll` (elevation required: HKLM/HKCR writes)
resolves the module by the absolute path and calls the exports in order. Every
step logs to `<module dir>\hunmin-reg-debug.log`.

1. `DllRegisterServer` resolves its own path
   (`GetModuleHandleExW(FROM_ADDRESS, &DllRegisterServer)` + `GetModuleFileNameW`).
2. `HKCR\CLSID\{CLSID}\InprocServer32` — default = the absolute module path,
   `ThreadingModel = Apartment`.
3. `HKLM\SOFTWARE\Microsoft\CTF\TIP\{CLSID}` — default = `Hunmin`, written with
   raw `RegCreateKeyExW` before any CTF COM call.
4. `CoInitialize` + `CoCreateInstance(CLSID_TF_InputProcessorProfiles)` →
   `Register({CLSID})` → `AddLanguageProfile({CLSID}, 0x0412, {profile},
   L"Hunmin", icon = <module path>)` → `EnableLanguageProfile(..., TRUE)`
   (per-user `Enable = 0x1` — see the correction under the registry table).
5. `ITfInputProcessorProfileMgr::ActivateProfile(…,
   TF_IPPMF_FORSYSTEMALL | TF_IPPMF_ENABLEPROFILE)`, then the only
   evidence-bearing step: `GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD)` inside the
   registering process, CLSID logged. The legacy `ActivateLanguageProfile` runs
   as fallback only.
6. `ITfCategoryMgr::RegisterCategory` for `GUID_TFCAT_TIP_KEYBOARD`,
   `GUID_TFCAT_CATEGORY_OF_TIP`, `GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER`
   (issue #98), `GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT`
   `{25504FB4-7BAB-4BC1-9C69-CF81890F0EF5}` (issue #68, measured 2026-10-29:
   without this category the Win11 input indicator gives the TIP no entry of
   its own — see `tests/manual-checklists/tsf-langbar-item.md`) and
   `GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT` `{13A016DF-560B-46CD-947A-4C3AF1E0E35D}`
   (measured 2026-10-31: a PACKAGED host offers a TIP only when it declares
   this — Windows Terminal, MSIX, cycled MS IME and 날개셋 with Win+Space and
   never reached Hunmin, and `Win+Space` set the active profile to Hunmin while
   the DLL was still never loaded into `WindowsTerminal.exe` — see §"Packaged
   hosts"). All five are unregistered in the mirror.

**The flow writes no file.** The Start-menu entry `Hunmin IME 설정` is the
installer's `[Icons]` entry and nothing else — ruled 2026-10-30 (issue #146,
option C of `docs/OPTIONS-MEASURED.md` §URM-3). `DllRegisterServer` used to
write a second one into the per-user Programs folder; measured on 2026-10-30,
those were two different files in two different Start-menu hierarchies, so an
installed machine showed the name twice. Both the write and the `rundll32
<dll>,HunminSettingsShow` fallback left the registration flow with the ruling;
`HunminSettingsShow` itself stays exported (the interim dialog is still
reachable, and `ITfFnConfigure` launches the settings exe through the shared
resolver).

`frontends/tsf/register-tsf.bat` wraps it: paths from `%~dp0`, notepad killed
first, both logs deleted first, `regsvr32` output and the reg log printed after.

## Resulting registry state (measured)

```
HKCR\CLSID\{F2C70001-...-0001}\InprocServer32
    (default) = <abs path>\frontends\tsf\build-msvc\Release\hunmin-tsf.dll
    ThreadingModel = Apartment
HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-...-0001}          (default) = "Hunmin"
HKLM\SOFTWARE\Microsoft\CTF\TIP\{...}\LanguageProfile\0x00000412\{F2C70002-...-0002}
    Description = Hunmin   IconFile = <the DLL's absolute path>   IconIndex = 0x0
HKCU\SOFTWARE\Microsoft\CTF\TIP\{...}\LanguageProfile\0x00000412\{F2C70002-...-0002}
    Enable = 0x1                                             <- per-user (see the
                                                                correction below)
HKCU\Software\Microsoft\CTF\Assemblies\0x00000412\{34745C63-...-1A31}
    Default = {A028AE76-01B1-46C2-99C4-ACD9858AE02F}      <- Microsoft IME
    Profile = {B5FE1F02-D5F2-4445-9C03-C568F23C99A1}
    KeyboardLayout = 0x04120412
HKCU\Software\Microsoft\CTF\SortOrder\AssemblyItem\0x00000412\{34745C63-...}\00000000
    CLSID = {A028AE76-...}   Profile = {B5FE1F02-...}      <- slot 0 = Microsoft IME
HKCU\...\SortOrder\AssemblyItem\0x00000412\{34745C63-...}\00000001
    CLSID = {F2C70001-...}   Profile = {F2C70002-...}      <- slot 1 = Hunmin, listed
HKCU\...\SortOrder\Language\00000000 = 00000412
```

**Correction, measured 2026-10-30 (issue #146):** the `Enable` value is per-user,
under HKCU — this list had it under the HKLM profile key, where the HKLM side in
fact carries `Description`/`IconFile`/`IconIndex` and no `Enable` (the HKLM *root*
key carries a separate `Enable = 1` as a string, written by msctf — this file
contains no such write, only the `EnableLanguageProfile` call). The documentation
agrees with the measurement:
`EnableLanguageProfile` "enables or disables a language profile for the current
user". The consequence for the mirror is in the section below.

## Activation model (measured)

Registering makes the profile *enabled and listed*, not *active*.

- `ActivateLanguageProfile` returns `S_OK` while activating nothing beyond the
  calling process; `ActivateProfile` with `FORSESSION` or `FORSYSTEMALL` (each
  with `ENABLEPROFILE`) also returns `S_OK`, and a fresh process still reported
  the Microsoft IME active. No flag combination tested persisted the default.
- The persisted default is Windows Settings' own HKCU CTF state; writing it from
  the TIP is out of scope, and hand-writing `Assemblies\Default` was measured to
  have no effect while CTF holds it.
- The shipping model: the profile is registered enabled, the user selects Hunmin
  (Win+Space or the language indicator), and TSF loads the TIP into the focused
  process. Verified end to end in Notepad: selection loads, keys compose, text
  commits.

## Unregister mirror (issue #84)

`DllUnregisterServer` reverses the flow in the same `CoInitialize` scope:
`UnregisterProfile({CLSID}, 0x0412, {profile})` → `Unregister({CLSID})` →
`UnregisterCategory` for the four categories →
`RegDeleteTreeW(HKLM\SOFTWARE\Microsoft\CTF\TIP\{CLSID})` (after the COM calls,
because `AddLanguageProfile` created the subkeys) → `RegDeleteTreeW(HKCR\CLSID\{CLSID})`.
The HKCU SortOrder/Assemblies store is untouched: registration never writes it
(measured above). The HKCU `Enable` value **is** registration's own write
(`EnableLanguageProfile`, per-user) and the HKLM tree deletion cannot reach it;
whether the COM unregister calls remove it is measured in the section below.
The elevated cycle itself is **measured** — see §"Unregister state".

## Unregister state (measured 2026-10-30, elevated)

The cycle `tests/manual-checklists/tsf-unregister-cleanup.md` owed from #84 was
run elevated on this host, against the SHIPPED DLL (the stage build the installer
carries, `build-msvc\installer\stage\hunmin-tsf.dll`), with the log gate on
(`HUNMIN_TSF_DEBUG_LOG=1`, `HUNMIN_TSF_LOG_DIR` set) and with ONE msctf unregister
call at a time through a throwaway probe, so each removal is attributed rather
than inferred from a single before/after pair.

| step | HKCR CLSID | HKLM CTF TIP tree | HKCU `…\{profile}\Enable` | what the reg log said |
|---|---|---|---|---|
| register | present, `InprocServer32` = that DLL | present: default `Hunmin`, `Enable=1`, four category pairs, profile `Description`/`IconFile`/`IconIndex` | `0x1` | `Register`, `AddLanguageProfile`, `EnableLanguageProfile`, `ActivateProfile`, `GetActiveProfile active.clsid={F2C70001}`, four `RegisterCategory` — all `0x0` |
| `UnregisterProfile` alone | untouched | still present; the profile's VALUES gone (`Description`/`IconFile` removed) | `0x1`, untouched | `UnregisterProfile hr=0x0` |
| `Unregister` alone | untouched | **GONE** — the whole tree, categories and `LanguageProfile` included | `0x1`, untouched | `Unregister hr=0x0` |
| `regsvr32 /u` on top | removed by our `RegDeleteTree` (`st=0x0`) | already gone (`st=0x2`, ERROR_FILE_NOT_FOUND) | `0x1`, untouched | four `UnregisterCategory` all `0x80004005` |

Three facts that change what the mirror's code means:

1. **`Unregister` alone deletes the HKLM TIP tree and its categories.** In a
   healthy cycle the four `UnregisterCategory` calls and the HKLM `RegDeleteTree`
   return `E_FAIL` / `0x2`: they are redundancy that costs nothing, and they earn
   their place only against a partially-removed state (a crashed register, a
   half-deleted tree). That is what the "run `/u` twice" edge exercises:
   `UnregisterProfile` on an absent profile returns `E_FAIL`, `Unregister` returns
   `S_OK`, and `regsvr32 /u` still exits 0.
2. **The per-user `Enable` value survives the whole flow.**
   `HKCU\SOFTWARE\Microsoft\CTF\TIP\{CLSID}\LanguageProfile\0x0412\{profile}\Enable = 0x1`
   is still there after `UnregisterProfile` + `Unregister` + `regsvr32 /u`. Nothing
   in msctf removes it and the mirror never touches HKCU, yet `EnableLanguageProfile`
   is registration's own write — so a literal mirror would have to delete it, and no
   API in the flow does. **Reproduced 2026-10-31 on a clean Windows Sandbox image**
   (`10.0.26100.9711`, install then uninstall, no prior session to blame):
   `tests/manual-checklists/tsf-installer-fresh-vm.md` row 9b. So the residue is
   the TIP's own write, not a dev-host artefact, and `HKCU\…\SortOrder` is
   untouched on a fresh profile.
3. **`RemoveLanguageProfile` is callable and adds nothing.** Against a present
   profile it returns `S_OK`, and the HKLM profile values are already gone
   (`UnregisterProfile` removed them); against an absent profile it returns
   `E_FAIL`. In neither case did it remove the per-user `Enable`. The never-called
   inverse is therefore not a gap in the HKLM mirror; the residual artefact is the
   HKCU key, and no msctf call in the flow owns it.

Also measured in the same round: the SortOrder slot for our profile stays in the
user's language list after unregistering (stale, as the checklist predicted), and
the shipped DLL writes NO Start-menu entry — the pre-ruling per-user `.lnk` was
deleted and did not come back across a register → unregister → register cycle
(issue #146, option C).

## Packaged hosts (measured 2026-10-31)

A PACKAGED host — Windows Terminal ships as MSIX — only offers a TIP that
declares `GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT`
`{13A016DF-560B-46CD-947A-4C3AF1E0E35D}`. Hunmin did not, and the symptom was
exact: with a WT pane focused, `Win+Space` cycled MS IME and 날개셋 and never
reached Hunmin, `GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD)` nevertheless
reported Hunmin, and `hunmin-tsf.dll` was never loaded into
`WindowsTerminal.exe` or `OpenConsole.exe` — while the same build was mapped in
`explorer.exe`, `Notepad.exe`, `msedgewebview2.exe` and an Electron terminal and
composed correctly in Notepad.

The diff that found it: compare the `Category\Category` sets of a working IME
and ours.

```
날개셋  {5CC2EDB1-…}  Category\Category\{13a016df-560b-46cd-947a-4c3af1e0e35d}   IMMERSIVESUPPORT
                      …also {364215d9-…} COMLESS, {49d2f9ce-…} SECUREMODE,
                      {49d2f9cf-…} UIELEMENTENABLED, {ccf05dd7-…} INPUTMODECOMPARTMENT
Hunmin  {F2C70001-…}  none of those
```

Registering IMMERSIVESUPPORT alone made the TIP appear in WT's `Win+Space`
cycle and compose there (maintainer-confirmed, 2026-10-31). The other four are
NOT declared: only the capability whose absence matched the symptom is claimed,
so a regression in the others cannot hide behind this one.

## The seam tier F1 drives (issue #107)

Every outside-world call in the flow above goes through `HunminRegSeam`
(`frontends/tsf/src/registrationseam.h`): COM activation, the HKCR/HKLM key
writes, opens and tree deletions. The
default table IS the platform call, so production runs the same calls in the
same order with one indirection; `hunmin_tsf_regseam_install(NULL)` restores
it. The seam covers the registry and not only `CoCreateInstance`, because
registration writes HKCR/HKLM (elevation only) and unregistration DELETES
both trees: an unseamed harness is either access-denied or, elevated,
destructive to the machine's real TIP registration.

Tier F1 (`frontends/tsf/test/tsf-registration-test.cpp`) runs that code over
the doubles in `mock-tsf-registration.h` and asserts, per artifact, what the
adapter asked for and in what order. It does NOT assert what the real msctf
servers write to the registry; the elevated-cycle measurement above stays
device-bound and is not closed by the mock tier.

Two corrections to the flow lists above, measured 2026-10-26 against the
shipped `registration.cpp`:

- Step 6 registers THREE categories, not two: `GUID_TFCAT_TIP_KEYBOARD`,
  `GUID_TFCAT_CATEGORY_OF_TIP` and (issue #98)
  `GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER`. The unregister mirror carries all
  three. **Superseded 2026-10-31: step 6 registers five — add
  `GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT` (issue #68) and
  `GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT` (see §"Packaged hosts").**
- The flow also pins the profile's stored `IconIndex` to 0 (two HKLM key
  opens after `AddLanguageProfile`). That step is not in the list above.

  **The Start-menu asymmetry is closed, not documented (measured and ruled
  2026-10-30, issue #146).** There were TWO Start-menu entries, in two folders,
  written by two programs: the DLL's, in the *per-user* Programs folder
  (`FOLDERID_Programs`), and the installer's `[Icons]` entry in the *all-users*
  one (`{group}`), which is the only one in Inno's uninstall log
  (`unins000.dat`). The ruling is option C of `docs/OPTIONS-MEASURED.md`
  §URM-3: the installer owns the entry, the DLL writes none, and the
  registration seam therefore carries no Programs-folder or shell-link call any
  more. `TsfRegistration.RegistrationWritesNoFile` pins it — a shell link
  re-added to the flow surfaces as an `CoCreateUnknown` activation the doubles
  do not recognise.

## Module view

The DLL is the single registered artefact and is self-contained: the engine is
linked statically (`hunmin_static`), because a TIP is loaded by its registry path
alone and Windows does not search the DLL's own directory for dependencies —
linking `hunmin.dll` measured `ERROR_MOD_NOT_FOUND (126)` in every host process
regardless of PATH or placement.

```
      +--------------------------------------------------+
      |  hunmin-tsf.dll        (one registered path)      |
      |    DllMain / DllRegisterServer / ClassFactory     |
      |    CTextService                                   |
      |      key map (VK -> layout char)                  |
      |      hunmin_buffered_preedit  (the word, #128)    |
      |      delivery: ITfComposition | interim EM span   |
      |      langbar item, candidate popup                |
      |    engine (libhunmin, linked statically)          |
      +--------------------------------------------------+
        deps (dumpbin -DEPENDENTS, measured):
        ole32 user32 shlwapi gdi32 advapi32 kernel32
        + ucrt (api-ms-win-crt-*) + vcruntime140(+_1)
        no hunmin.dll
```

Engine context resolution at `Activate()` (`hunmin_ctx_from_module`):
`HUNMIN_DATA_DIR` → `<module dir>\data` → the repository's `libhunmin/data`. A
NULL context leaves every later key unhandled.

## Key-down propagation

`ITfKeystrokeMgr` → `OnTestKeyDown` (the eat preview) → `OnKeyDown`: a
modifier-only event changes nothing; otherwise the VK becomes a layout character
(0 = unmapped) and the layer decides. Consumed: the engine's commit goes through
the active delivery (`ITfComposition` on modern hosts, the posted EM span on
classic ones) and the key is eaten. Declined — space, Enter, punctuation,
backspace, an unmapped key, an edit key, a toggle, a chord: the boundary delivers
the buffered word once and the key passes, so the application inserts it. What
the user sees composing is the layer's display; focus loss and `Deactivate()`
flush the remaining word through the same path before the context goes away.
(TSF1, TSF4, TSF7, TSF8; FE4.)

## Logging

Both logs are module-relative (next to the DLL; `HUNMIN_TSF_LOG_DIR` overrides the
directory) and gated: with `HUNMIN_TSF_DEBUG_LOG` unset, no file is created and
the chains behave identically. The runtime log appends `DllMain ATTACH`,
`DllGetClassObject`, `Activate`/`Advise`, `OnTestKeyDown`/`OnKeyDown`
(`consumed, word len=` / `boundary, word len=`), `flush: insert hr=` and focus
events — one file per host process.

## Verification checklist

- Registration: read the tree above with `reg query`; `regsvr32` exit 0 and the
  reg log showing `pProfileMgr->GetActiveProfile ... active.clsid={F2C70001}`.
- Self-contained: `dumpbin -DEPENDENTS` lists only system DLLs and the
  UCRT/VC runtime; a clean-environment `LoadLibraryW` probe
  (`PATH=C:\Windows\System32`) loads with all three exports resolvable. (Probe
  caveat: a `DllImport` of `LoadLibraryW` needs `CharSet=Unicode`, else it
  reports a false `err=126`.)
- Buffering: select Hunmin, type a multi-syllable word, watch the composition
  hold the word and the document stay empty until the first boundary key; the
  runtime log shows `consumed, word len=` growing and one `boundary, word len=`
  per word.
