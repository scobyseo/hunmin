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
   (`Enable = 0x1` under the profile key).
5. `ITfInputProcessorProfileMgr::ActivateProfile(…,
   TF_IPPMF_FORSYSTEMALL | TF_IPPMF_ENABLEPROFILE)`, then the only
   evidence-bearing step: `GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD)` inside the
   registering process, CLSID logged. The legacy `ActivateLanguageProfile` runs
   as fallback only.
6. `ITfCategoryMgr::RegisterCategory` for `GUID_TFCAT_TIP_KEYBOARD` and
   `GUID_TFCAT_CATEGORY_OF_TIP`.

`frontends/tsf/register-tsf.bat` wraps it: paths from `%~dp0`, notepad killed
first, both logs deleted first, `regsvr32` output and the reg log printed after.

## Resulting registry state (measured)

```
HKCR\CLSID\{F2C70001-...-0001}\InprocServer32
    (default) = <abs path>\frontends\tsf\build-msvc\Release\hunmin-tsf.dll
    ThreadingModel = Apartment
HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-...-0001}          (default) = "Hunmin"
HKLM\SOFTWARE\Microsoft\CTF\TIP\{...}\LanguageProfile\0x00000412\{F2C70002-...-0002}
    Enable = 0x1
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
`UnregisterCategory` for both categories →
`RegDeleteTreeW(HKLM\SOFTWARE\Microsoft\CTF\TIP\{CLSID})` (after the COM calls,
because `AddLanguageProfile` created the subkeys) → `RegDeleteTreeW(HKCR\CLSID\{CLSID})`.
The HKCU SortOrder/Assemblies store is untouched: registration never writes it
(measured above). The elevated cycle itself is **not yet measured** on this host
— the probes live in `tests/manual-checklists/tsf-unregister-cleanup.md`
(TSF17).

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
