---
title: "Symbol screen: app-read data file, engine uninvolved (B2)"
created: 2026-11-22
updated: 2026-11-22
type: decision
status: active
tags: [android, layouts, decision]
sources:
  - ../../../docs/SPEC-ANDROID-KEYPAD.md
  - ../../../docs/DESIGN-ANDROID-SYMBOLS.md
  - https://github.com/scobyseo/hunmin/issues/298
confidence: high
---

# Symbol screen: app-read data file, engine uninvolved (B2)

The maintainer ruled the symbol-layer data home for #298 (2026-11-22, verbatim):

> "앱이 데이터 파일로 그리기. 엔진은 관여 안함."

**Decision:** the Android symbol screen is drawn by the **app from a data file**;
the composition **engine is not involved** (option B2 in the design analysis).
This settles #298's core question — **no engine or composition-schema change**.
The symbol list stays *data* (the app reads a data file, not hardcoded
constants), honouring "layout behaviour is data"; its format/validation is a
frontend data contract settled at implementation.

## Why this, and what it avoids

- Not A (a third per-key `symbol` row): that needed a schema bump, loader, key
  API, key-log/replay and doctor changes — measured, not engine-free.
- Not B3 (symbol page as an engine-loaded layout): that loads with no engine
  change but appears in the engine's layout/settings list, needing a visibility
  contract. B2 avoids that entirely — the engine never sees the symbol screen.
- The gesture-variant extension (swipe/long-press to pick alternate symbols,
  preferred over a Tier 2 button) is split to **#312** (KP10).

## What would reopen it

A future need to share the symbol screen across frontends (Windows/fcitx5) would
reopen the data home and move toward B3. Until then the symbol screen is
Android-only presentation fed by data.

Recorded as `docs/SPEC-ANDROID-KEYPAD.md` KP11; the comparable analysis is
`docs/DESIGN-ANDROID-SYMBOLS.md`. The decision is the maintainer's, per
[[decisions-belong-to-the-maintainer]]; it builds on the criteria
([[2026-11-22-android-keypad-criteria]]) and the mockup
([[2026-11-22-android-keypad-mockup]]). The symbol-screen build is the follow-up
implementation, not part of the #298 decision issue.
