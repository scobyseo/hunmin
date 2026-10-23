---
title: "TSF langbar item as a tear-off (stage 4 of the clean-code pass)"
created: 2026-10-22
updated: 2026-10-22
type: session
status: active
ingested: 2026-10-22
tags: [session, windows, tsf, refactor, langbar, issue-127, issue-103]
sources:
  - ../../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../../docs/REMAINING-WORK.md
  - ../../../frontends/tsf/src/langbar.h
confidence: high
---

## What was directed

The clean-code follow-on to [[2026-10-22-tsf-component-refactor]] (issue
#127) continued with one stage per PR. The maintainer chose the stage order
S6 -> S5 -> S4 -> S10 and, for S6, option A: the language-bar item becomes
a TEAR-OFF COM object rather than a helper the shell forwards to or a
helper now with the tear-off deferred. The measurement was presented before
the choice, per [[decisions-belong-to-the-maintainer]].

## The measured starting point, corrected

The issue text said eight TSF interfaces and 28 STDMETHOD bodies. The tree
said otherwise: `CTextService` had **10 bases** (`ITfTextInputProcessor`,
`ITfThreadMgrEventSink`, `ITfKeyEventSink`, `ITfCompositionSink`,
`ITfLangBarItemButton`, `ITfSource`, `ITfDisplayAttributeProvider`, plus
`TsfCore`, `CandBackend`, `HunminDeliveryRegistry`) and **30** STDMETHOD
bodies. `tsf.cpp` was **2,320** physical lines; the langbar surface was 11
methods plus `HunminLbiInputModeGuid`, `itemSink_`, `itemSinkCookie_` and
the AddItem/RemoveItem blocks.

## Shipped

- `frontends/tsf/src/langbar.h` / `langbar.cpp`: `LangBarItem` implements
  `ITfLangBarItemButton` (hence `ITfLangBarItem`) and `ITfSource`, owns the
  hosting shape (`GUID_LBI_INPUTMODE`, `BTN_MENU|SHOWNINTRAY`; the maintainer
  removed the BTN_BUTTON bit on 2026-10-22),
  the badge (`GetIcon`/`GetText`), the click/menu handlers and the kernel
  sink. It reads the mode through `LangBarHost::HangulMode()` only.
- `CTextService` drops the two bar interfaces (7 TSF interfaces -> 5; 9
  bases total because `LangBarHost` joins the three seams; 30 -> 19
  STDMETHOD bodies).
  QI for the three bar IIDs returns the item; `Activate`/`Deactivate` pass
  it to `AddItem`/`RemoveItem`; `UpdateLangBarItem` forwards to
  `LangBarItem::Update`; `Snapshot().itemSink` reads the item's sink.
- Tier-F1: 4/4, 3.74 s. `docs-lint`: 0 errors. `tsf.cpp` is 2,146 lines;
  `langbar.cpp` 249 and `langbar.h` 73.

## What this does not establish

The hosted tray presence and the right-click menu were field-verified on
the pre-tear-off shape. The tear-off is a new COM identity for the same
three IIDs, so it is a host re-measure OWED (not ctest-observable). The
documented shape, not the host behaviour, is what this stage verified.

## Findings left for a ruling

- `OnMenuSelect` passes `nullptr` to `hunmin_tsf_launch_settings`, whose
  `GetModuleHandleExW(FROM_ADDRESS, nullptr, ...)` fails, so the BTN_MENU
  route launches nothing. This is pre-existing and was preserved by the
  move; it is a behaviour question for the maintainer, not a stage
  decision.
- The double `delivery_->CaptureDeadCopy(compText_)` at tsf.cpp:1420/1425
  is recorded as a measurement for S5.

## Next

In the maintainer's order: the shell's duplicated paths + table-driven QI
(S5), the registration block as its own TU (S4), then the `settings.cpp`
split (S10). The double `CaptureDeadCopy(compText_)` at tsf.cpp:1420/1425
was recorded as a measurement for S5, not decided here.
