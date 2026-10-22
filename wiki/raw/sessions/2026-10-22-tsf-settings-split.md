---
title: "The settings TU split by responsibility (stage 5 of the clean-code pass)"
created: 2026-10-22
updated: 2026-10-22
type: session
status: active
ingested: 2026-10-22
tags: [session, windows, tsf, refactor, settings, issue-127, issue-71]
sources:
  - ../../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../../docs/REMAINING-WORK.md
  - ../../../frontends/tsf/src/settingsdialog.cpp
  - ../../../frontends/tsf/src/settingsrows.h
confidence: high
---

## What was directed

The clean-code pass from [[2026-10-22-tsf-langbar-tearoff]] (issue #127)
continued one stage per PR. The maintainer's stated order was S6 -> S5 ->
S4 -> S10, then the maintainer directed the settings split (S10) ahead of
S5 and chose split **option A**: four TUs by responsibility plus the export.
Options B (dialog+rows together) and C (one TU per function group, six
files) were measured and presented before the choice, per
[[decisions-belong-to-the-maintainer]].

## The measured starting point

`frontends/tsf/src/settings.cpp` was **979** physical lines holding five
unrelated jobs: the layout-row collector, an in-memory dialog template with
its modal proc, the Windows 11 look, a `WH_KEYBOARD` capture hook, the
WinUI 3 exe resolver/launcher, and the `ITfFunctionProvider` /
`ITfFnConfigure` plumbing. `settings.h` was 78 lines and mixed the row
struct with the COM/bridge declarations.

## Shipped

- `settingsrows.h` (34) / `settingsrows.cpp` (42): `HunminTsfLayoutRow`,
  the caps and `hunmin_tsf_rows_for_mode`. `settings.h` now includes it, so
  the tier-F1 settings test compiles unchanged.
- `settingsdialog.h` / `settingsdialog.cpp` (731): the interim plain-Win32
  dialog (template, modal proc, W11 look, capture hook) as
  `hunmin_tsf_settings_dialog_run(HWND)`.
- `settingslaunch.h` / `settingslaunch.cpp` (97): the WinUI 3 exe
  resolution/launch/wait as `hunmin_tsf_settings_exe_run(HWND, wchar_t*,
  size_t)`.
- `settingsprovider.cpp` (145): `kProviderTypeGuid`, `HunminTsfConfigFn`,
  `HunminTsfFunctionProvider` and `hunmin_tsf_settings_new_provider`.
- `settings.cpp` (25): only `HunminSettingsShow`, the rundll32 export.
- CMake: the four new TUs added to `hunmin-tsf`, `tsf-trace-emit` and
  `tsf-test`.
- Board rows re-pointed after the symbol move: TSF-SETTINGS-SHOW-71 ->
  `settingslaunch.cpp:hunmin_tsf_settings_exe_run`, TSF-CAPTURE-HOOK-67 and
  TSF-SCREEN-67 -> `settingsdialog.cpp`. Tier-F1 4/4 (2.68 s); docs-lint 0
  errors.

## What changed behaviourally: nothing intended

The move is relocation. Two file-static functions became cross-TU calls
with prefixed names, and each now anchors `GetModuleHandleExW(FROM_ADDRESS,
...)` on its own function address instead of `&HunminSettingsShow`; both
live in the same DLL module, so the resolved path is identical. The unused
`hunmin_tsf_log_path` declaration was dropped. No store key, dialog id,
hook lifetime or wait semantics changed.

## What this does not establish

The dialog was not driven by hand in this stage, and neither was the exe
launch: tier-F1 exercises the rows/provider surfaces, not the modal dialog
or the shell handoff. The field-verified behaviours recorded in
[[2026-10-22-tsf-langbar-tearoff]] remain the authority, and the hosted
tray / settings menu re-measure is still owed.

## Next

The maintainer's remaining order: the shell's duplicated paths plus the
table-driven QI (S5, with the double `CaptureDeadCopy(compText_)` at
tsf.cpp:1420/1425 still to rule on), then the registration block as its own
TU (S4).
