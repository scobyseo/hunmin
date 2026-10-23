---
title: "QI answers tabled (Q)"
created: 2026-10-22
updated: 2026-10-22
type: session
status: active
ingested: 2026-10-22
tags: [session, windows, tsf, refactor, query-interface, issue-127]
sources:
  - ../../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../../frontends/tsf/src/tsf.cpp
confidence: high
---

## What was directed

The last structural item of the approved plan that needed no ruling: the QI
trace and the QI dispatch were two parallel if/else chains, so an interface
could be answered without being logged and the two lists could drift.

## Shipped

`tsf.cpp` now has one `kQiRoutes` table: `{IID, trace tag, kind, cast}`.
`FindQiRoute` looks the asked IID up once; the same result writes the trace
tag and dispatches:

- `kQiThis` — the `ITfTextInputProcessor` / thread-mgr-sink / key-sink /
  composition-sink / display-attribute-provider subobject, then `AddRef()`.
- `kQiSettingsProvider` — the settings provider factory (issue #71).
- `kQiLangBar` — forwarded to the tear-off item, `E_NOINTERFACE` when it is
  absent (S6).

The trace line is the same bytes as before (`QI riid=... answered=<tag>
pid=...`), the answer order is unchanged, and `IID_IUnknown` still answers
with the `ITfTextInputProcessor` subobject. `tsf.cpp` 575 -> 597.

## Verification

Clean build, no warnings; tier-F1 4/4. The QI trace is asserted indirectly by
the traces (the langbar item's routing goes through it); a host look at the
log line is the direct check and the mocks do not read it.

## Next

Only stage S remains: the three settings-exe resolvers disagree about
candidate order and depth, and unifying them changes which executable an
installed Start-menu shortcut launches — a maintainer ruling, not an agent
decision.
