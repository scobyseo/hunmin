---
title: "Milestone D — candidate & edit-mode UX (issue #67)"
created: 2026-10-18
updated: 2026-10-18
type: session
status: active
ingested: 2026-10-18
tags: [session, windows, tsf, candidates, langbar, key-capture, krdict, issue-67, issue-71]
sources:
  - ../../../docs/WINDOWS-TSF-CANDIDATES.md
  - ../../../docs/WINDOWS-TSF-CONFIG.md
  - ../../../docs/WINDOWS-TSF-BUFFERING.md
  - ../../../docs/WINDOWS-TSF-API-NOTES.md
confidence: high
---

## What the issue asked

[[2026-10-17-milestone-c-layout-wiring-data-loading]] closed milestone C, and
issue #67 is milestone D — "candidate & edit-mode UX" — the last 0.2 feature
lane before [[2026-10-17-issue-72-buffering-docs-closure]]'s packaging lane.
The maintainer's field-test directives through the session, in order: the
candidate window itself; the Hanja key (not F9) as its default and
configurable in the dialog; the dialog usable ("compact", readable, on
screen); key capture in BOTH surfaces; the settings reachable without the
repo (Start menu, language bar); the duplicate hanja rows collapsed the
ship-minimal way; CTRL/ALT chords as application input.

## Shipped (PR #97, verified by tier-F1 ctest + docs-lint)

- **Candidate window** anchored to the whole buffered word (engine tables are
  word-keyed, measured); Return commits the cursor, digits select in-range,
  arrows/page motion, Esc closes-and-types, zero-candidate pass-through.
- **The candidate key**: store value `HanjaKey` (default `v1:25:0`, the bare
  physical Hanja key), env seam `HUNMIN_TSF_HANJA_KEY`, unparseable → back to
  default (the window can never become unreachable). Unbound F-keys joined
  issue #95's keys-of-passage.
- **Key capture**: the interim dialog's two 키 입력 buttons arm a thread
  `WH_KEYBOARD` hook that sees the true physical VKs pre-IME (the
  `WM_KEYDOWN` route inside a modal dialog was measured dead twice — the
  finding, not an accident); the WinUI 3 exe uses its Root_KeyDown with the
  VK 229 guard (no IMM32 there; the measured first capture stored a dead
  binding). Store grammar unchanged.
- **Screen presence defects (both roots measured)**: Mica'ed dialog rendered
  ghost-white → Mica removed from the interim dialog; the ownerless rundll32
  modal is NOT displayed by `WS_VISIBLE` alone on this host (the dlgbox docs
  claim does not hold here, second measurement) → explicit ShowWindow +
  work-area center + topmost + foreground; initdialog logs visible=1.
- **The system surfaces**: Start-menu entry written by registration
  (settings-ui exe first, rundll32 fallback, deleted on unregister);
  the conversion compartment (`GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION`)
  written by the TIP at Activate/Apply/toggle — the documented compartment the
  bar's 中/英 badge reads (Microsoft Q&A 5865217); the system langbar menu
  extension (`ITfSystemLangBarItemSink` advised via the item's `ITfSource`),
  the docs' own "Extending System Language Bar Items" mechanism — inactive
  here, see below.
- **krdict (ko,hanja) dedupe** (ship-minimal data choice ruled by the
  maintainer among engine/frontend/data designs): 34,142 → 33,184 rows, 958
  merged; meanings joined unique; cross-word pairs left (the 251-case tail);
  suggest-test green — the hanja-candidate indexes did not shift; the idiom
  overlap remains (가가호호 2, not 3); `안녕` 2→1 moved the tier-F1 anchor.

## Overturned verdicts (recorded so they do not recur)

- **The langbar surface is CLOSED on this build (10.0.26100 host):** a
  reference item (BTN_BUTTON only, fresh GUID, plain thread) fails `AddItem`
  with the same `0x80004005` the real item gets — environmental refusal, not
  our registration. The `TF_LBI_STYLE_SHOWNINTRAY`/`SHOWNINTRAYONLY` flags are
  documented "not currently supported". The item code stays dormant-and-
  correct; nothing to fix locally.
- **The sink direction**: docs say the KERNEL advises an `ITfLangBarItemSink`
  onto the ITEM's `ITfSource`; the first cut had the service creating and
  pushing a sink through `AdviseItemSink` — wrong, deleted. The correction's
  provenance is the win32 docs source (cloned C:\Workspace\win32) — consult
  the repo markdown, not memory, for this family.
- **Adding the Sejong icon exposed a stale profile icon index**: the live
  profile carried `IconFile=<dll>, IconIndex=1` (an empty slot) so the
  language entry rendered blank; registration now rewrites `IconIndex=0`
  (and the asset's licence terms require the 출처: 어반브러시 credit —
  appliances/README + debian/copyright).

## Pending at closure (explicitly not shipped)

- **Hanja last-syllable fallback** (user note, recorded in
  docs/WINDOWS-TSF-BUFFERING.md): zero-whole-word lookups should produce
  candidates from the LAST syllable and the commit replaces only it — board
  row TSF-LISSYLL-67 asserts its ABSENCE so the agreement cannot silently
  look done.
- **The chord rule's mode-persist alternative** (the note's stronger
  reading): the transient form ships; the persist form is recorded,
  awaiting an explicit ruling (row TSF-MODEPERSIST-67).
- **Live re-registration** of the badge/menu-extension build (elevation
  pending on the user's side) and the tray pick-up measurement of the
  compartment write.
- **CI**: the Actions billing failure still vetoes every job (the release's
  automated-verification gate); [[2026-10-17-milestone-c-layout-wiring-data-loading]]'s
  plan keeps that outside this milestone.
