---
title: "TSF frontend components: the DIP+SAP stages run (issues #67, #103)"
created: 2026-10-22
updated: 2026-10-22
type: session
status: active
ingested: 2026-10-22
tags: [session, windows, tsf, refactor, issue-67, issue-103, legacy, interim]
sources:
  - ../../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../../tests/manual-checklists/tsf-langbar-item.md
confidence: high
---

## What was directed

The [[2026-10-20-legacy-interim-input-path]] session ended with the F9 Hanja
flow and the delivery seam as fold-in fragments inside `src/tsf.cpp`
(commit `8db2cf1`), and the staged component refactor proposed in
`docs/REFACTOR-TSF-COMPONENTS.md` ([[2026-10-19-test-framework-migration]]
carries the seam history). On 2026-10-22 the maintainer ruled the run and
shaped it mid-flight: *"remove all include cpp ... properly extract
functions from class ... tsf.h exposes interface for extracted feature, and
registration intf too for feature implementation"*.

## Shipped (stages 1-3, tier-F1 green after each build)

- **All the include-.cpp seams are gone in both directions.** The tier-F1
  harness LINKS `tsf.cpp`, `delivery.cpp`, `candidates.cpp`,
  `keybinding.cpp` like the DLL does; nothing includes a `.cpp`.
- **The contracts live in `tsf.h`**: `TsfCore`, `Delivery` + the
  `HunminDeliveryFactory`/`HunminDeliveryRegistry` pair, `CandBackend`,
  `Candidates`, and `HunminTsfSnapshot` (the observation channel that
  replaces the old `#define private public` white-box seam). The
  implementations (delivery.cpp's `NativeDelivery`/`InterimDelivery`,
  candidates.cpp's `HunminCandidates`) register into the shell — deliveries
  through `hunmin_tsf_deliveries_register()` switched with `UseDelivery()`,
  the candidate flow through `hunmin_tsf_hanja_candidates_new()`. The
  GetKeyState mock is a REGISTERED provider
  (`hunmin_tsf_keystate_register`, keybinding.h) instead of a macro
  clobbered per TU (the old one-TU pattern that forced the includes).
- **Board rows re-pointed**, because the rows grep the fold-in homes:
  TSF-LAYOUT-RESOLVE-66 (`resolveLayoutSlot` →
  `hunmin_tsf_resolve_layout_slot`), TSF-INTERIM-SEAM-119
  (`struct NativeDelivery` → delivery.cpp), TSF-INTERIM-HANJA-119, and
  TSF-LISSYLL-67 (→ candidates.cpp). GTEST-TSF-109's seam description
  rewritten. docs-lint 0 errors.

## Verified

- Build clean (0 errors), `ctest -C Release -L tier-F1` **4/4** on the
  MSVC tree (conformance × 3 + tsf-test with every fragment group);
- `tools/docs-lint.py --build-dir frontends/tsf/build-msvc` 0 errors.

## Closure of issue #103

Merged on the maintainer's explicit "commit, merge and close". The
deferred minors ride on into the follow-on tracking issue (the TSF
clean-code pass), not on this page's pending list: the caret-before-symbol
re-measure at the next live round, and the jamotong comparator revive —
both are maintainer-owned live rounds, recorded in the checksheet
(`tests/manual-checklists/tsf-langbar-item.md`).

## What this leaves, measured

`frontends/tsf/src/tsf.cpp` is still **2,320 lines** with `CTextService`
implementing **eight TSF interfaces plus TsfCore, CandBackend and
HunminDeliveryRegistry** (`tsf.h:206`), 28 STDMETHOD bodies, and the
longest single function 113 lines (`Deactivate`); `settings.cpp` is 978
lines with one 136-line window proc. That is the measured basis for the
SRP/clean-code follow-on issue — stages 1-3 moved the mode machinery out;
they did not shrink the shell.
