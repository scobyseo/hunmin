---
title: "DllRegisterServer/DllUnregisterServer blocks extracted (S5, registration)"
created: 2026-10-22
updated: 2026-10-22
type: session
status: active
ingested: 2026-10-22
tags: [session, windows, tsf, refactor, registration, issue-127, issue-84]
sources:
  - ../../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../../docs/REMAINING-WORK.md
  - ../../../frontends/tsf/src/tsf.cpp
confidence: high
---

## What was directed

After [[2026-10-22-tsf-activate-deactivate-extraction]], the maintainer
pointed at `DllRegisterServer()` with the same rule that had taken
`Activate()` apart: an anonymous block becomes a named function, or moves to
the relevant class as an API. Registration has no class, so the helpers are
file-static and the exports stay the ordered list of calls.

## Shipped

`DllRegisterServer()` had seven naked scopes; each is now one artifact:

- `RegisterComServer(guidStr, dllPath)` - the CLSID key and its
  `InprocServer32` value + `ThreadingModel`, returning the two-step failure
  the old block spelled inline.
- `RegisterTipKey(guidStr)` - the HKLM TIP key and its display name.
- `RegisterInputProcessorProfile(guidStr)` - `CoCreateInstance`, `Register`,
  `AddLanguageProfile`, `EnableLanguageProfile`; the module anchor is the
  helper's own address (same module, same resolution as the old
  `&DllRegisterServer`).
- `PinProfileIconIndex(guidStr)` - the two-pass IconIndex pin, with the
  2026-10-18 measurement comment that explains why the index is rewritten.
- `ActivateInputProcessorProfile(pProfile, profileGuid)` - the ProfileMgr
  call, the `GetActiveProfile` check and the legacy fallback, carrying the
  long measurement comment on why `ActivateLanguageProfile` alone is not
  enough.
- `RegisterTipCategories()` - the three `RegisterCategory` calls.
- `CreateStartMenuShortcut(dllPath)` - candidate A/B resolution, the shell
  link, and the "Hunmin IME 설정" shortcut.

`DllUnregisterServer()` mirrors the register flow: the profile body became
`UnregisterInputProcessorProfile()` and the category body
`UnregisterTipCategories()`, while the two `RegDeleteTreeW` calls stay inline
in the documented reverse order (CTF COM state -> HKLM TIP tree -> HKCR CLSID
tree). No call order, argument, return value or log line changed; the
`CoInitialize`/`CoUninitialize` pairing and the "do not touch HKCU" note are
where they were.

## Measured

`tsf.cpp` 2,076 -> 2,096 physical lines (the named seams cost a few lines;
the point is the list). Tier-F1 4/4 (8.30 s wall, Release); docs-lint 0
errors; wiki-lint 0 errors. Nothing here is host-observable: the register and
unregister cycles remain the elevated manual checklist of issue #84
(`tests/manual-checklists/tsf-unregister-cleanup.md`), unchanged by this
pass.

## Then the S4 move (same day)

The plan's S4 half followed immediately: `registration.cpp` (451 lines) now
carries the helpers, both exports and the DLL's COM surface (the class factory
and `DllGetClassObject`), with `registration.h` declaring only the three names
the DEF exports. The three shared GUIDs moved to `ids.cpp` (19) behind
`ids.h` (21) because registration and the display-attribute provider both
name them; `g_hunmin_hinst` moved with the only code that read it. `tsf.cpp`
2,096 -> 1,659. CMake lists the two new TUs for the DLL, the mock harness and
the trace runner. The standalone TSF configure now passes `/utf-8` (the
engine build already did), which removes the C4819 noise from every TU.

## Next

The rest of S5 remains the QI logging tree and the duplicated `CaptureDeadCopy`
site; the component plan then continues with the display attribute,
composition and key-dispatch TUs. See [[2026-10-22-tsf-settings-split]] for
the stage-5 split that preceded this session and
[[2026-10-22-tsf-langbar-tearoff]] for the stage-4 tear-off.
