---
title: "The TSF extraction round, 2026-10-22: eleven stages, one subject"
created: 2026-10-25
updated: 2026-10-25
type: concept
status: active
tags: [concept, windows, tsf, refactor, issue-127]
sources:
  - ../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../docs/WINDOWS-TSF-REGISTRATION.md
confidence: high
---

# The TSF extraction round, 2026-10-22

`frontends/tsf/src/tsf.cpp` began the day at 2,320 lines with one class
implementing eight TSF interfaces plus three seams. The round moved it to 528
by extracting one translation unit per responsibility, each stage verified on
tier F1 before the next. The plan is `docs/REFACTOR-TSF-COMPONENTS.md`; this page
is the round's outcome, and the eleven session records it replaces stay as
`superseded` pages carrying their own measurements.

## The stages

| stage | TU | what moved |
|---|---|---|
| component refactor (1–3) | — | the shell's shape: `TsfCore`, `Delivery`, the registry |
| stage 4 | `langbar.cpp` | the tear-off language-bar item |
| stage 5 | `settingsrows.cpp`, `settingsprovider.cpp`, `settingsdialog.cpp` | the settings surface, split by responsibility |
| S4 | `registration.cpp` | `DllRegisterServer`/`DllUnregisterServer` and the class factory |
| S5 first cut | `tsf.cpp` | `Activate`/`Deactivate` blocks |
| S7 | `displayattr.cpp` | the display-attribute provider |
| C | `composition.cpp` | the native composition machinery |
| K | `keydispatch.cpp` | the key path (test/down, passage, feed, toggle) |
| F | `datadir.cpp` | the data source and the layout-slot check |
| S | `settingspath.cpp` | the settings-exe resolver, unified on the deployed-first ruling |
| Q | `tsf.h` | the QI answers, tabled instead of an if/else chain |

## What the round established

- **A component shell was the shape that made the extraction possible**:
  interfaces in `tsf.h`, one implementation TU per responsibility, registered
  into the shell — including the .cpp is banned in both directions. The
  measured effect: an adapter-touch rebuild went 12.7 s → 8.8 s.
- **The two seams the later work depended on** arrived here: `Snapshot()` as the
  test observation channel (replacing `#define private public`) and the
  key-state provider as a registered seam.
- **One defect was found and deliberately left**: `UnadviseSinks()` nulls
  `threadMgr_` before `Deactivate()` reaches the langbar detach, so the item
  keeps its kernel sink past deactivation. Fixing it changes behaviour
  (`RemoveItem` would run), so it is recorded for a ruling, not made.

## What it does not establish

Nothing here is evidence about live hosts: the round ran against the mock
harness (tier F1), and the checks that need a registered TIP stayed manual. The
host-bound re-measures live in `docs/REMAINING-WORK.md`.

## Related

- [[where-the-practices-came-from]] — the working rules the round ran under.
- [[the-rules-keep-breaking]] — the failure class that made one-measurement-per-
  stage worth the ceremony here.

The eleven stage records are named `2026-10-22-tsf-*` under `raw/sessions/`; they
are `superseded` stubs now, and this page is what a reader should cite. A stub
is still the evidence that the stage ran on that date.
