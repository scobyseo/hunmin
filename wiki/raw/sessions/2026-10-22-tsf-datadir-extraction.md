---
title: "Data source and layout-slot check extracted (F)"
created: 2026-10-22
updated: 2026-10-22
type: session
status: active
ingested: 2026-10-22
tags: [session, windows, tsf, refactor, datadir, issue-127, issue-66]
sources:
  - ../../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../../frontends/tsf/src/datadir.cpp
  - ../../../frontends/tsf/src/settings.h
confidence: high
---

## What was directed

Next in the approved component plan after the display attribute
([[2026-10-22-tsf-displayattr-extraction]]): the data-source resolution and
the stored-layout slot check are not shell behaviour, and both are already
shared with the settings dialog and the tier-F1 tests.

## Shipped

- `datadir.cpp` (122) carries `hunmin_ctx_from_module` (the one data source:
  `HUNMIN_DATA_DIR` > `<module>\data` > the repo checkout climb, with the
  duplicate-tree measurement comment intact) and
  `hunmin_tsf_resolve_layout_slot` (accept a stored id only when the engine
  can instantiate it and its mode matches the slot).
- `datadir.h` (31) declares both. `tsf.h` and `settings.h` include it and no
  longer repeat the declarations, so every caller still reaches the same
  functions.
- `tsf.cpp` 1,492 -> 1,384; `datadir.cpp` joined the DLL, the tier-F1
  harness and the trace runner source lists.

## Verification

Clean build, no warnings; tier-F1 4/4 — the datadir and config tests already
drive both functions, so the move is asserted by the existing tests rather
than by new ones. docs-lint 0 errors after the board row
`TSF-DATA-SINGLE-SOURCE-66` was re-pointed at `datadir.cpp`.

## Next

The plan's remaining stages are the composition TU and the key-dispatch TU
(both need a seam on the shell), the settings-exe resolver unification
(needs a ruling on candidate order) and the table-driven QI. See
[[2026-10-22-tsf-settings-split]] for the settings split schema.
