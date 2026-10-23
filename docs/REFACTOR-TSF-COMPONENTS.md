# Refactor proposal: the TSF frontend as components (DIP + SAP)

Status: RUN — stages 1-3 executed 2026-10-22, stage 4 (the language-bar
tear-off) and stage 5 (the settings split) executed the same day on the
maintainer's direction
("remove all include cpp ... properly extract functions from class ...
tsf.h exposes interface for extracted feature, and registration intf too
for feature implementation"), in the
windows-mode-indicator-right-click-to-settings-f-2 worktree; tier-F1 green
after each build. The landed shape follows the maintainer's steer, which
refines the design below: the CONTRACTS (TsfCore, Delivery +
HunminDeliveryFactory/HunminDeliveryRegistry, CandBackend, Candidates,
HunminTsfSnapshot) all live in `tsf.h`; the implementations
(delivery.cpp's NativeDelivery/InterimDelivery, candidates.cpp's
HunminCandidates) are registered into the shell — deliveries as factories
through `hunmin_tsf_deliveries_register()`, switched with `UseDelivery()`;
the candidate flow arrives through `hunmin_tsf_hanja_candidates_new()`.
The include-.cpp seam is gone in both directions: keybinding.cpp,
delivery.cpp and candidates.cpp are compiled TUs, and the tier-F1 harness
LINKS them instead of including tsf.cpp — the white-box reads go through
`CTextService::Snapshot()` (the observation channel) and the GetKeyState
mock is a REGISTERED provider (`hunmin_tsf_keystate_register`,
keybinding.h). The board rows that grepped the fold-in homes were
re-pointed (TSF-LAYOUT-RESOLVE-66, TSF-INTERIM-SEAM-119,
TSF-INTERIM-HANJA-119, TSF-LISSYLL-67, TSF-LANGBAR-67; GTEST-TSF-109's
seam description rewritten); docs-lint 0 errors.

Stage 4 (2026-10-22) moved the language-bar surface to a TEAR-OFF COM object
(`langbar.h`/`langbar.cpp`): `CTextService` no longer implements
`ITfLangBarItemButton` or `ITfSource` (7 TSF interfaces -> 5; 9 bases total,
since `LangBarHost` joins the three existing seams; 30 STDMETHOD bodies ->
19). QI for `ITfLangBarItem`/`ITfLangBarItemButton`/`ITfSource` returns the
item, `Activate`/`Deactivate` pass it to `AddItem`/`RemoveItem`, and
`UpdateLangBarItem` forwards to `LangBarItem::Update`. The item reads only
`LangBarHost::HangulMode()` from the shell. Tier-F1 green (4/4, 3.7 s);
docs-lint 0 errors.

Stage 5 (2026-10-22) split `settings.cpp` (979 lines) by responsibility on
the maintainer's choice of option A: `settingsrows.cpp` (42) holds the
layout-row collector and `settingsrows.h` (34) its C struct;
`settingsdialog.cpp` (731) is the interim plain-Win32 dialog — in-memory
template, modal proc, the Windows 11 look, the key-capture hook;
`settingslaunch.cpp` (97) resolves and waits on the WinUI 3 exe;
`settingsprovider.cpp` (145) is the `ITfFunctionProvider` handed out by QI
and the `ITfFnConfigure` whose Show launches the exe then re-applies the
store. `settings.cpp` (25) keeps only the rundll32 export. The move is pure
relocation: the two former statics became cross-TU calls
(`hunmin_tsf_settings_dialog_run`, `hunmin_tsf_settings_exe_run`) and each
anchors its own function address in place of `&HunminSettingsShow` (same
module, same resolution). Board rows re-pointed (TSF-SETTINGS-SHOW-71,
TSF-CAPTURE-HOOK-67, TSF-SCREEN-67); tier-F1 green (4/4, 2.7 s); docs-lint
0 errors.

Stage S5 began the same day on the maintainer's directive: an anonymous
block in `Activate()` (or thereabouts) should be a private function, or the
relevant class's API. `Activate()`'s three naked blocks became
`AdviseSinks()`, `DetectLegacyMode()` and `LangBarItem::Attach()`;
`Deactivate()`'s advice block became `UnadviseSinks()`, its local
`DeactSession` class was replaced by the existing `InsertBufferedWord()`
(the SetText duplication recorded in the S5 findings), and the
engine-flush duplication was folded into `FlushEngineIntoWordbuf()`, shared
by `BoundaryFlush`, `DetachComposition` and `Deactivate`. The same pass
took `ApplyConfigStore`'s two parse scopes to `LoadToggleKeys` /
`LoadHanjaKey` and its ic free+new pair, shared with `ToggleModeCore`, to
`RebuildEngineIc`. After that pass `tsf.cpp` was 2,076 lines (was 2,146); `langbar.cpp` 278
and `langbar.h` 82, carrying
`Attach`/`Detach` and the `attached_` flag that used to be the shell's
`langbar_registered_`. Tier-F1 4/4; docs-lint 0 errors. One defect was
exposed and NOT changed: `UnadviseSinks()` nulls `threadMgr_` before
`Deactivate()` reaches the detach, so `Detach` (and `OnRemoved`) is a no-op
today; recorded as TSF-LANGBAR-DETACH-127 for a ruling.

The maintainer then pointed the same rule at `DllRegisterServer()`. Its seven
scopes became file-static helpers, one per registration artifact:
`RegisterComServer()` (CLSID/InprocServer32), `RegisterTipKey()` (the HKLM TIP
key), `RegisterInputProcessorProfile()` (`Register` + `AddLanguageProfile` +
`EnableLanguageProfile`, anchored on the helper's own address),
`PinProfileIconIndex()` (the two-pass IconIndex pin), `ActivateInputProcessorProfile()`
(ProfileMgr then the legacy fallback, with its measurement comment),
`RegisterTipCategories()` and `CreateStartMenuShortcut()`. `DllRegisterServer()`
is now the ordered list of those calls. `DllUnregisterServer()` mirrors it with
`UnregisterInputProcessorProfile()` and `UnregisterTipCategories()`, the tree
deletions staying inline in the documented reverse order. No call order,
argument or log line changed; `tsf.cpp` is 2,096 lines. The S4 half still owed
is the move itself: these helpers and the two exports belong in their own TU.

## Where the code stands

`frontends/tsf/src/tsf.cpp` is 2,096 lines (was 2,320 before stage 4, 2,146
after it and 2,076 after the S5 extraction; the figures here are physical
lines). `CTextService` still carries the
composition write machinery, the key dispatch and the registration
exports; the language-bar surface now lives in `langbar.cpp` (249 lines)
behind `langbar.h` (73), alongside `delivery.cpp` (307) and
`candidates.cpp` (323). After stage 5 the settings surface is five TUs ?
`settingsdialog.cpp` (731) is now the largest non-shell TU and
`settings.cpp` (25) is only the export; the shell is 2,096. It is green
(tier-F1) and behaves; the remaining complaint is the duplication inside
the shell and the un-tabled QI.

## The rules this refactor obeys

- **DIP**: a component depends on ABSTRACTIONS the shell owns, never on the
  concrete TIP class; the shell depends on components and wires them.
- **SAP**: everything depends toward the shared, stable half — `log.h` has
  no dependencies; a component header depends on the shell-owned CONTRACT
  headers it implements or reads (`tsf.h`, `log.h`) and, where the engine
  ABI is the subject matter, `hunmin.h` — never on `CTextService` or a
  `.cpp`. The shell composes the components. Including a `.cpp` is banned
  in both directions.

## Design: the seams, exactly

1. **`delivery.h`** (the mode-delivery seam, the #119 machinery):
   - `class TsfCore` — the abstract slot the shell's behaviour plugs into:
     the word-buffer/comp-text accessor pair, the engine `ic` handle, the
     client id, `InsertBufferedWord`, `CompositionWrite`, the logger handle.
   - `struct Delivery` (unchanged method set): `Consumed`, `Boundary`,
     `CandidateAnchor`, `CandidateCommit`, `FlushPending`.
   - `struct NativeDelivery` / `struct InterimDelivery` — own their states
     (the interim deferral machinery incl. the timer and the writer
     static). Defined in `delivery.cpp`; both take `TsfCore&`.
2. **`candidates.h`** (the Hanja flow, issue #67):
   - `class CandBackend` — the seam the flow READS: `Anchor`,
     `CommitPicked`, `CaretRect`, `EngineContext`.
   - `class HunminCandidates` ctor `(CandBackend&)`: one page constant,
     `LastSyllableOf` + `HanjaListCount` shared by the test/down halves,
     the open/close/motion handlers, the commit that replaces the anchor.
   - No `CTextService` type appears here.
3. **`tsf.h`** — the shell's declaration: `CTextService` implements the two
   seams with one-line forwards; it owns the TSF callbacks, the
   composition write machinery, one reference to the tear-off langbar item,
   and the settings bridge.
4. **`tsf.cpp`** — the frontend's exports: the `Dll*` chain, the class
   factory, and the free functions (`hunmin_ctx_from_module`,
   `resolveLayoutSlot`).
5. **`utf.h`** (extracted from the candidate path, stable, no deps) —
   `HunminUtf8ToWide` / `HunminWideToUtf8`, the one conversion pair.
6. **`langbar.h`/`langbar.cpp`** (stage 4, issues #67/#103): `LangBarItem`
   implements `ITfLangBarItemButton` + `ITfSource` as a tear-off; the shell
   returns it from QI and passes it to `AddItem`/`RemoveItem`. The item
   reads the mode through `LangBarHost` (`tsf.h`) and owns the kernel sink,
   the hosting shape (`GUID_LBI_INPUTMODE`, `BTN_MENU|SHOWNINTRAY`), the badge and the settings menu.
7. **`settingsrows.h`/`settingsdialog.h`/`settingslaunch.h`** (stage 5,
   issue #71) — the settings surface split by responsibility:
   `settingsrows` reads the engine list into the dialog's rows and knows no
   window; `settingsdialog` is the interim Win32 surface (template, proc,
   W11 look, capture); `settingslaunch` resolves and waits on the WinUI 3
   exe; `settingsprovider.cpp` carries the TSF plumbing. `settings.cpp` is
   the only C-linkage piece, the rundll32 export.

## Staged execution (each stage lands ctest-green, then commits)

| Stage | Content | Risk |
|---|---|---|
| 1 | `utf.h`; the fold-in fragments dissolve (the text back inline) | low |
| 2 | `delivery.h`/`delivery.cpp`: `TsfCore` + the delivery types + their states; the interim machinery out of the shell | medium — the interim write half touches the compose half; the CMake target lists change |
| 3 | `candidates.h`/`candidates.cpp`: the flow class + `CandBackend` on the shell; the tier-F1 candidate tests read a Snapshot accessor instead of the service's private fields | medium — the white-box seam's reach shrinks |
| 4 | `langbar.h`/`langbar.cpp`: the item as a tear-off (`ITfLangBarItemButton` + `ITfSource`); QI forwards, `AddItem`/`RemoveItem` take the item | medium — the shell's identity changes for three IIDs; the hosted-tray behaviour needs a host re-measure |
| 5 | `settingsrows`/`settingsdialog`/`settingslaunch`/`settingsprovider`: the 979-line settings TU split by responsibility, export left behind | low — pure relocation; only the two statics become cross-TU calls, max TU 731 |

Next in the maintainer's chosen order (2026-10-22): the shell's duplicated
paths + the table-driven QI (S5), then the registration block as its own TU
(S4). The `settings.cpp` split (S10) landed 2026-10-22 (stage 5).

## What stays untouched

- The engine ABI, the TIP registration, the layouts, the config schema.
- The field-verified behaviours: the mode flip, the pop-up and the interim
  fallback. The item's hosted presence (`AddItem hr=0x0`) was field-verified
  on the pre-tear-off shape; the tear-off's tray presence and right-click
  menu are a host re-measure OWED (not ctest-observable) — recorded here so
  the stage is not mistaken for host-verified.

## The decision the agent does not make

Whether to run this at all, and in which order — the maintainer decides;
the proposal only carries the design and the stage costs.
