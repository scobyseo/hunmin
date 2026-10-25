# Windows TSF registration and event propagation for Hunmin

## Purpose

Describe, for `hunmin-tsf.dll`, how the TIP is registered (`regsvr32` + registry),
what state that produces, and how a key stroke propagates from the host
application to the engine and back to the document. This is the companion to
`docs/WINDOWS-TSF-BUFFERING.md`, which specifies the buffering contract that the
key-down chain implements.

Provenance: every behaviour and registry value below was measured on this
host — Windows 11 (26200), Windows SDK 10.0.26100, VS BuildTools 14.44 — during
the 2026-10-16/17 TSF loading and buffering work. GUID literals that are not
visible in the measured registry state are named only, not quoted.

## Debug logging is an option (maintainer instruction, issue #66)

Every runtime log this file mentions (`hunmin-tsf-debug.log`,
`hunmin-reg-debug.log`) is written only when `HUNMIN_TSF_DEBUG_LOG` is set to
`1` / `on` / `yes` / `true`; the gate is `tsf_log_open` (tsf.cpp), the one
function all sites open through. With the switch off, no file is created and
the registration/key-down chains behave identically apart from logging; the
variable is re-read per open, so a session can be flipped on or off without a
restart. The file still lands next to the DLL (or `HUNMIN_TSF_LOG_DIR`) exactly
as described below.


## Constants (code: `frontends/tsf/src/tsf.cpp`)

| Constant | Value |
|---|---|
| `CLSID_HunminTSF` | `{F2C70001-0000-0000-0000-000000000001}` |
| language-profile GUID | `{F2C70002-0000-0000-0000-000000000002}` |
| language id | `0x0412` (Korean) |
| `GUID_TFCAT_TIP_KEYBOARD` | `{34745C63-B2F0-4784-8B67-5E12C8701A31}` — measured as the CTF `SortOrder\AssemblyItem\0x00000412\` container key name |
| `GUID_TFCAT_CATEGORY_OF_TIP` | value not readable from this host's headers (declared, lib-defined); referenced by name only |

## The regsvr32 flow

`regsvr32.exe <abs path>\hunmin-tsf.dll` (elevation required: HKLM and HKCR
writes) resolves the DLL by the absolute path given on the command line and
calls the standard exports in order:

1. `DllMain(DLL_PROCESS_ATTACH)` — records `g_hunmin_hinst`, writes the first
   line of the runtime log.
2. `DllRegisterServer()` — every step below is logged to
   `<module dir>\hunmin-reg-debug.log` (opened `"w"`, module-relative; see
   "Logging" below). The steps, from the log of a measured run:
   1. Resolve the module's own absolute path
      (`GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, &DllRegisterServer)`
      + `GetModuleFileNameW`).
   2. `HKCR\CLSID\{CLSID}\InprocServer32` — default value = the absolute
      module path, `ThreadingModel = Apartment`.
   3. `HKLM\SOFTWARE\Microsoft\CTF\TIP\{CLSID}` — default value = `Hunmin`
      (raw `RegCreateKeyExW`, before any CTF COM call).
   4. `CoInitialize` + `CoCreateInstance(CLSID_TF_InputProcessorProfiles)` →
      `ITfInputProcessorProfiles`.
   5. `Register({CLSID})`.
   6. `AddLanguageProfile({CLSID}, 0x0412, {profile}, L"Hunmin", icon = <module
      path>)` — the CTF service writes
      `HKLM\...\TIP\{CLSID}\LanguageProfile\0x00000412\{profile}`.
   7. `EnableLanguageProfile(..., TRUE)` — sets `Enable = 0x1` under the same
      profile key.
   8. `QueryInterface(ITfInputProcessorProfileMgr)` then
      `ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, 0x0412, {CLSID}, {profile},
      hkl = nullptr, TF_IPPMF_FORSYSTEMALL | TF_IPPMF_ENABLEPROFILE)`.
   9. Verification, the only evidence-bearing step:
      `GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD)` inside the registering
      process, whose CLSID is logged. The legacy
      `ActivateLanguageProfile` runs only as fallback (no ProfileMgr, or the
      verify failed).
   10. `ITfCategoryMgr` `RegisterCategory` for `GUID_TFCAT_TIP_KEYBOARD` and
       `GUID_TFCAT_CATEGORY_OF_TIP`.
3. `DllCanUnloadNow` — not exported by `src/hunmin-tsf.def` today.

`frontends/tsf/register-tsf.bat` wraps this: paths derived from `%~dp0`,
notepad killed first, both debug logs deleted first, `regsvr32` output and the
registration log printed after.

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
HKCU\Software\Microsoft\CTF\SortOrder\AssemblyItem\0x00000412\{34745C63-...}\00000001
    CLSID = {F2C70001-...}   Profile = {F2C70002-...}      <- slot 1 = Hunmin, listed
HKCU\Software\Microsoft\CTF\SortOrder\Language\00000000 = 00000412
```

`{A028AE76-...}`/`{B5FE1F02-...}` are the Microsoft Korean IME's TIP CLSID and
profile; the registry values were read from this host.

## Activation model (measured)

Registering makes the profile *enabled and listed*, not *active*. Measured on
this host:

- `ITfInputProcessorProfiles::ActivateLanguageProfile` returns `S_OK` while
  activating nothing beyond the calling process.
- `ITfInputProcessorProfileMgr::ActivateProfile` with
  `TF_IPPMF_FORSESSION` and with `TF_IPPMF_FORSYSTEMALL` (each measured with
  `TF_IPPMF_ENABLEPROFILE`) also returns `S_OK`, and a *fresh* process still
  reported the Microsoft IME active
  (`GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD)` probe). No flag combination
  tested persisted the default.
- The persisted default lives in the CTF per-user state above
  (`Assemblies\...\Default` and `SortOrder\AssemblyItem` slot 0) and is owned
  by Windows Settings. Writing it from the TIP is deliberately out of scope;
  hand-writing `Assemblies\Default` was measured to have no effect on what
  fresh processes get while CTF holds the state.
- The shipping model: the profile is registered enabled; the user selects
  Hunmin (Win+Space or the language indicator) and TSF then loads the TIP into
  the focused process (`DllMain` attach, `Activate()`, key-sink advise). The
  full chain was verified end to end in Notepad: selection loads, keys compose,
  text commits.

Unregister mirror (issue #84): `DllUnregisterServer` now reverses the
registration flow in the same `CoInitialize` process scope:

1. `ITfInputProcessorProfileMgr::UnregisterProfile({CLSID}, 0x0412, {profile})`
   — mirror of `EnableLanguageProfile`/`ProfileMgr` activation, then
2. `ITfInputProcessorProfiles::Unregister({CLSID})` — mirror of `Register()`,
3. `ITfCategoryMgr::UnregisterCategory` for `GUID_TFCAT_TIP_KEYBOARD` and
   `GUID_TFCAT_CATEGORY_OF_TIP`,
4. `RegDeleteTreeW(HKLM\SOFTWARE\Microsoft\CTF\TIP\{CLSID})` — after the COM
   calls, because `AddLanguageProfile` created the `LanguageProfile\0x00000412`
   subkeys under it,
5. `RegDeleteTreeW(HKCR\CLSID\{CLSID})` — the only deletion the previous
   implementation performed.

Windows Settings' own HKCU CTF store (`SortOrder`/`Assemblies`) is deliberately
NOT touched: registration never writes it (measured, above). Every step logs to
the same module-relative `hunmin-reg-debug.log` the register flow uses, opened
in append mode so register+unregister share one log. The elevated
register/unregister cycle itself is **not yet measured on this host** — it
needs an elevated shell; the probes and rows to record live in
`tests/manual-checklists/tsf-unregister-cleanup.md`.

## Module view

The TIP DLL is the single registered artefact and is self-contained: the
engine is linked in statically (`hunmin_static` from the top-level
`CMakeLists.txt`, preferred by `frontends/tsf/CMakeLists.txt` over the shared
`hunmin.lib`). This is load-bearing: a TIP is loaded by its registry path
alone, Windows does not search a DLL's own directory for its dependencies, and
linking `hunmin.dll` measured as `ERROR_MOD_NOT_FOUND (126)` in every host
process regardless of PATH or file placement.

```
                +------------------------------------------------+
                |  hunmin-tsf.dll        (one registered path)   |
                |                                                |
                |  adapter (frontends/tsf/src)                   |
                |    DllMain / DllRegisterServer / ClassFactory  |
                |    CTextService                                |
                |      key map (VK -> layout char)               |
                |      wordbuf_  - buffered word, issue #73      |
                |      ITfEditSession inserts (selection SetText)|
                |      overlay window (STATIC, caret-anchored)   |
                |                                                |
                |  engine (libhunmin, linked statically)         |
                |    version.c tables.c schema_loader.c expr.c            |
                |    ic.c candidate_tables.c + third_party/cJSON          |
                +------------------------------------------------+
                  deps (dumpbin -DEPENDENTS, measured):
                  ole32 user32 shlwapi gdi32 advapi32 kernel32
                  + ucrt (api-ms-win-crt-*) + vcruntime140(+_1)
                  no hunmin.dll
```

Engine context resolution at `Activate()` (`hunmin_ctx_from_module`):
`HUNMIN_DATA_DIR` env var, `<module dir>\data`, then the repository's
`libhunmin/data` — in that order. A NULL context would leave every later key
unhandled.

## C&C: registration

```
user / CI                     regsvr32.exe                       hunmin-tsf.dll
   |  regsvr32 <dll>  (elevated)  |                                   |
   |----------------------------->| LoadLibrary -> DllMain(ATTACH)     |
   |                              |----------------------------------> |
   |                              | call DllRegisterServer             |
   |                              |----------------------------------> |
   |                              |   HKCR  CLSID\...\InprocServer32   |
   |                              |   HKLM  CTF\TIP\{clsid} = "Hunmin" |
   |                              |   CoCreate TF_InputProcessorProfiles
   |                              |   Register / AddLanguageProfile    |
   |                              |     (LanguageProfile\0x0412 Enable=1)
   |                              |   ProfileMgr::ActivateProfile      |
   |                              |     FORSYSTEMALL|ENABLEPROFILE     |
   |                              |   GetActiveProfile -> log verify   |
   |                              |   CategoryMgr::RegisterCategory    |
   |<--- exit 0, reg log printed -|                                    |
   v
state: COM class registered, profile enabled+listed (SortOrder slot 1),
desktop default UNCHANGED (slot 0 = Microsoft IME)
```

## C&C: key-down propagation

```
host app (Notepad/Word)        msctf / ctfmon                hunmin-tsf.dll (in-proc)
   |  key-down (WM_KEYDOWN)       |                                |
   |----------------------------->| ITfKeystrokeMgr -> sink        |
   |                              |   OnTestKeyDown (pfEat preview)|
   |                              |------------------------------->|
   |                              |   OnKeyDown(pic, wParam, ...)  |
   |                              |------------------------------->|
   |                              |    modifier-only -> S_OK, no state change
   |                              |    ch = VK -> layout char (0 = unmapped)
   |                              |    hunmin_ic_process(ic, ch)
   |                              |      |
   |                              |      +-- consumed (mapped Hangul):
   |                              |      |    commit = ic_commit()  (destructive)
   |                              |      |    wordbuf_ += commit    (no NUL)
   |                              |      |    overlay <- wordbuf_ + ic_preedit()
   |                              |      |    *pfEat = TRUE
   |                              |      |
   |                              |      +-- declined (space/Enter/punct/
   |                              |           backspace/unmapped) = BOUNDARY:
   |                              |           flush = ic_flush()   (finalises in-flight)
   |                              |           wordbuf_ += flush
   |                              |           edit session (TF_ES_SYNC|READWRITE):
   |                              |             GetSelection -> SetText(wordbuf_)
   |                              |             -> collapse end -> SetSelection
   |                              |           wordbuf_.clear()
   |                              |           *pfEat = FALSE
   |<---- original key delivered -|<-------------------------------|
   v  (app inserts the boundary key itself: space, Enter, ...)
```

Presentation: the pending word never enters the document before a boundary
(commit-only model, `docs/WINDOWS-TSF-BUFFERING.md`); the overlay window
shows `wordbuf_` plus the in-flight preedit, positioned from
`ITfContextView::GetTextExt` when available and from the caret rect otherwise.
Focus loss and `Deactivate()` flush the remaining word through the same
edit-session insert before the context goes away.

## Logging

Both logs are module-relative (`hunmin_tsf_log_path` in tsf.cpp): they land
next to the DLL (`frontends/tsf/build-msvc/Release/`), never at a drive root;
`HUNMIN_TSF_LOG_DIR` overrides the directory. The runtime log accumulates
`DllMain PROCESS_ATTACH`, `DllGetClassObject`, `Activate`/`Advise`,
`OnTestKeyDown`/`OnKeyDown` (`consumed, word len=` / `boundary, word len=`),
`flush: insert hr=`, and focus events — one file per host process, appended.

## Verification checklist

- Registration: read the tree in "Resulting registry state" with
  `reg query`; `regsvr32` exit code 0 plus the reg log showing
  `pProfileMgr->GetActiveProfile ... active.clsid={F2C70001}`.
- Self-contained: `dumpbin -DEPENDENTS` on the DLL lists only system DLLs and
  the UCRT/VC runtime; a clean-environment `LoadLibraryW` probe
  (`PATH=C:\Windows\System32`) loads with all three registry exports
  resolvable. (Probe caveat: a `DllImport` of `LoadLibraryW` needs
  `CharSet=Unicode`, else it reports false `err=126`.)
- Buffering: manual run per `docs/WINDOWS-TSF-BUFFERING.md` — select Hunmin,
  type a multi-syllable word, observe the overlay accumulate and the document
  stay empty until the first boundary key; runtime log shows
  `consumed, word len=` growing and one `boundary, word len=` line per word.
