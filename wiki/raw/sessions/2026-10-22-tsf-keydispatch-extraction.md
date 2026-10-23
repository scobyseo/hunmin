---
title: "Key path extracted (K)"
created: 2026-10-22
updated: 2026-10-22
type: session
status: active
ingested: 2026-10-22
tags: [session, windows, tsf, refactor, keydispatch, issue-127]
sources:
  - ../../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../../frontends/tsf/src/keydispatch.cpp
  - ../../../frontends/tsf/src/tsf.h
confidence: high
---

## What was directed

The approved component plan's key stage: everything between the editor
key handlers and the engine feed is one responsibility, and it had become
the largest block left in the shell after the composition cut
([[2026-10-22-tsf-composition-extraction]]).

## Shipped

`keydispatch.cpp` (386) now defines, unchanged:

- `OnTestKeyDown` / `OnKeyDown` / `OnTestKeyUp` / `OnKeyUp` /
  `OnPreservedKey` and `TrackKeyContext`.
- `IsModeToggle` / `ApplyModeToggle`, `IsCandidateToggle`, `IsKeyOfPassage`,
  `DrainAndPass` (with its "edit key"/"declined" tags), `VkToBase`,
  `IsModifierVk`.
- `HarvestFinalised` (the destructive `hunmin_ic_commit` drain) and
  `FeedEngineKey` (the jamo feed and composition write trigger).

These are ordinary `CTextService` member definitions — the class stays
declared in `tsf.h`, so there is no new header and no `KeyHost` seam; the
mock harness links the TU like the DLL. `tsf.cpp` 945 -> 575.

## Verification

Clean build, no warnings; tier-F1 4/4 — the three win32 traces exercise
test/down, passage, toggle and feed, and the candidate group the toggle's
window branch. Board rows `TSF-BUFFERING-72`, `TSF-EDITKEYS-95` and
`TSF-CHORD-67` were re-pointed at `keydispatch.cpp`.

## Next

Only the table-driven QI (stage Q) and the settings-exe resolver
unification (stage S) remain from the plan. S is gated on a maintainer
ruling: the three resolvers disagree about candidate order and depth.
