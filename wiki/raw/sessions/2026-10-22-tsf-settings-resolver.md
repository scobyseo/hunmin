---
title: "Settings-exe resolver unified on the deployed-first ruling (S)"
created: 2026-10-22
updated: 2026-10-22
type: session
status: active
ingested: 2026-10-22
tags: [session, windows, tsf, refactor, settings, issue-127, issue-71]
sources:
  - ../../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../../frontends/tsf/src/settingspath.cpp
  - ../../../docs/REMAINING-WORK.md
confidence: high
---

## What was directed

The plan's last stage had one open question: the three settings-exe
resolvers disagreed about candidates and depth. Measured before the
question:

| Caller | Candidates (before) |
|---|---|
| `settingslaunch.cpp` (FnConfigure) | env > `<dll>\settings-ui\publish` > `<dll>\..\settings-ui\publish` > `<dll>\settings-ui`, waits |
| `tsf.cpp:hunmin_tsf_launch_settings` (langbar) | `<dll>\settings-ui\publish` > `<dll>\..\..\settings-ui\publish` > `<dll>\settings-ui`, no env, no wait |
| `registration.cpp:CreateStartMenuShortcut` | `<dll>\settings-ui\publish` > `<dll>\..\..\settings-ui\publish`, no installed candidate |

The consequence was concrete: in an installed tree the .iss stages the
publish content under `{app}\settings-ui`, so FnConfigure found the exe while
the Start-menu shortcut fell back to rundll32.

The maintainer ruled **deployed-first**: env > `<dll>\settings-ui\publish` >
`<dll>\settings-ui` > `<dll>\..\settings-ui\publish` >
`<dll>\..\..\settings-ui\publish`.

## Shipped

- `settingspath.cpp` (100) / `settingspath.h` (32): `hunmin_tsf_settings_exe_find`
  (anchor -> DLL dir, env override) and `hunmin_tsf_settings_exe_in_dir` (the
  shortcut has the dir already). The header names the order once.
- `settingslaunch.cpp` (56) keeps only the wait + ShellExecuteEx;
  `hunmin_tsf_launch_settings` keeps only the rundll32 fallback;
  `CreateStartMenuShortcut` uses the shared list. `registration.cpp` 451 -> 442,
  `tsf.cpp` 597 -> 571.
- Consequence, per the ruling: an installed Start-menu shortcut now targets
  the WinUI exe instead of rundll32. A host look at the shortcut target after
  an elevated install is the check; it is not ctest-observable.

## Verification

Clean build, no warnings; tier-F1 4/4; docs-lint 0 errors. The three callers
now share one candidate list, and `TSF-SETTINGS-SHOW-71` records the new
order.

## Next

The approved plan is complete. Remaining items on the board are feature work
and the host-side re-measures recorded in `docs/REMAINING-WORK.md`, not
refactor stages. See [[2026-10-22-tsf-qi-table]] for the preceding stage.
