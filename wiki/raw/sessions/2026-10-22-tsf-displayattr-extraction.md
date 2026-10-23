---
title: "Display-attribute provider and styling channel extracted (S7)"
created: 2026-10-22
updated: 2026-10-22
type: session
status: active
ingested: 2026-10-22
tags: [session, windows, tsf, refactor, display-attribute, issue-127]
sources:
  - ../../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../../frontends/tsf/src/displayattr.cpp
  - ../../../frontends/tsf/src/tsf.cpp
confidence: high
---

## What was directed

The approved component plan's next stage after the registration TU
([[2026-10-22-tsf-registration-extraction]]) moved the preedit styling out of
the shell: the display-attribute provider objects and the GUID-atom channel
are one responsibility, and the shell only forwards to them.

## Shipped

- `displayattr.cpp` (196) now carries `HunminDisplayAttrInfo` (the one
  attribute: transparent background, underline, theme text/line colors — the
  measured MS-IME shape) and `HunminEnumDisplayAttrInfo` (one entry, clone
  copies the position), plus the factories
  `HunminDisplayAttrEnumNew()` and `HunminDisplayAttrNewForGUID()`.
- `DisplayAttr` owns the `TfGuidAtom` the shell used to hold as
  `displayAttrAtom_`: it registers `HunminDisplayAttrGuid` with the category
  manager once (logging a failed `RegisterGUID`), then sets or clears
  `GUID_PROP_ATTRIBUTE` over the composition range. Without a category
  manager the text renders unstyled, exactly as before.
- The shell keeps its two provider methods as forwards; the composition
  session calls `svc_->displayAttr_->Apply(...)`. The header (40) declares
  the class and the two factories only.
- `tsf.cpp` 1,659 -> 1,492; `displayattr.cpp` joined the DLL, the tier-F1
  harness and the trace runner source lists.

## Verification

Clean build with no warnings (C4819 stays gone after the `/utf-8` change
recorded in [[2026-10-22-tsf-registration-extraction]]); tier-F1 4/4 in
2.3 s. The painted underline is not ctest-observable: the provider's
attribute shape is unchanged by the move, so the existing Notepad
measurement stands, but a host look after the move is still the honest check.

## Next

The plan's remaining stages are the composition TU, the key-dispatch TU, the
data-dir resolution, the settings-exe resolver unification (needs a
maintainer ruling on candidate order) and the table-driven QI. See
[[2026-10-22-tsf-settings-split]] for the settings split schema.
