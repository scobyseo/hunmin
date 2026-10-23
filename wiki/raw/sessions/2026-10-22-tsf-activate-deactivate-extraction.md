---
title: "Activate/Deactivate blocks extracted; the langbar style ruled (S5 first cut)"
created: 2026-10-22
updated: 2026-10-22
type: session
status: active
ingested: 2026-10-22
tags: [session, windows, tsf, refactor, langbar, issue-127, issue-103]
sources:
  - ../../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../../docs/REMAINING-WORK.md
  - ../../../frontends/tsf/src/tsf.cpp
  - ../../../frontends/tsf/src/langbar.cpp
confidence: high
---

## What was directed

After [[2026-10-22-tsf-settings-split]], the maintainer reviewed the shell
and said an anonymous block in `Activate()` "or so" should be extracted as a
private function at least, or moved to the relevant class as an API. While
that was being landed the maintainer also ruled the langbar item's style:
`TF_LBI_STYLE_BTN_MENU | TF_LBI_STYLE_SHOWNINTRAY` only, with no
`BTN_BUTTON`.

## Shipped

`Activate()` had three naked blocks and `Deactivate()` two, plus a local
edit-session class defined inside an `if`:

- `AdviseSinks()` — the thread-manager event sink and key-event sink
  registration.
- `DetectLegacyMode()` — the `HUNMIN_TSF_INTERIM` read and the delivery
  choice.
- `LangBarItem::Attach(ITfThreadMgr*)` / `Detach(ITfThreadMgr*)` — the
  `AddItem`/`RemoveItem` pair, with `attached_` replacing the shell's
  `langbar_registered_`; the manager's reference lifetime now lives beside
  the item the manager sees.
- `UnadviseSinks()` — the key-event unadvise and thread-manager release.
- `DeactSession` deleted: `Deactivate()` now calls the existing
  `InsertBufferedWord(lastCtx_)`, so the duplicate selection-SetText class
  is gone (one added `flush: insert` log line, no other difference).
- `FlushEngineIntoWordbuf()` — the engine-flush-plus-append body shared by
  `BoundaryFlush`, `DetachComposition` and `Deactivate` (the 4x UTF-8 append
  recorded in the S5 findings).
- `ApplyConfigStore()` had the same shape: its two parse scopes became
  `LoadToggleKeys(cfg)` and `LoadHanjaKey(cfg)`, and its `ic_` free+new pair
  (duplicated in `ToggleModeCore`) became `RebuildEngineIc()`.

Measured: `tsf.cpp` 2,147 -> 2,076 physical lines (the helpers added
function boundaries; the point is the named seams, not the count);
`langbar.cpp` 249 -> 278,
`langbar.h` 73 -> 82. Tier-F1 4/4 (2.47 s); docs-lint 0 errors.

The style ruling flipped the test back to the ruled pair: `GetInfo` is
`BTN_MENU | SHOWNINTRAY`, so the case now asserts exactly that and the
manual checklists' current-shape text follows. The sibling jamotong TIP's
`BTN_BUTTON|BTN_MENU|SHOWNINTRAY` stays recorded as the measured comparison,
not as our shape.

## A defect exposed, not fixed

`UnadviseSinks()` nulls `threadMgr_` before `Deactivate()` reaches the
detach, so `LangBarItem::Detach(NULL)` returns immediately: the item keeps
its attached state and kernel sink across deactivation, although the
comments and the old block intended `RemoveItem` to run there. This was
already true of the inline block (the guard was
`langbar_registered_ && threadMgr_`). Fixing it means moving the detach
ahead of `UnadviseSinks()` so `RemoveItem` actually runs — a behaviour
change, so it is recorded as board row TSF-LANGBAR-DETACH-127 and left to
the maintainer, per [[decisions-belong-to-the-maintainer]].

## Next

The rest of S5: the remaining shell duplication (the UTF-8 append sites,
`ic_` rebuild, the QI logging tree) and the table-driven QI. The QI answer
log is still a long if/else chain; the detach finding is waiting on a
ruling. See [[2026-10-22-tsf-langbar-tearoff]] for the item's tear-off
shape.
