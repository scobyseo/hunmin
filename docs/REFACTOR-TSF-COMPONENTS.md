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
`RebuildEngineIc`. After that pass `tsf.cpp` was 2,076 lines (was 2,146); `langbar.cpp` 276
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
argument or log line changed; `tsf.cpp` was 2,096 lines.

Stage S4 then moved that block and the DLL's COM surface out: `registration.cpp`
(451) carries the helpers, `DllRegisterServer`/`DllUnregisterServer`,
`CClassFactory` and `DllGetClassObject`; `registration.h` declares only the
three exports the DEF names, so the helpers stay private. The three shared
GUIDs (service CLSID, language profile, display attribute) moved to `ids.h`/
`ids.cpp` (21/19) because registration and the display-attribute provider both
name them, and the `g_hunmin_hinst` extern moved with the only code that read
it. `tsf.cpp` was 1,659 lines (2,320 at the start of stage 4).

Stage S7 then moved the preedit styling out: `displayattr.cpp` (196) carries
the two provider objects (`ITfDisplayAttributeInfo`, the one-entry enum) and
the `DisplayAttr` channel that registers the GUID atom and sets/clears the
attribute; `displayattr.h` (40) declares the factories the shell forwards to
and the channel the composition session calls. The provider's measured
attribute shape and its logged `RegisterGUID` failure moved with the code
unchanged. `tsf.cpp` was 1,492 lines.

Stage F then moved the data source out: `datadir.cpp` (122) carries
`hunmin_ctx_from_module` (the one-data-source-wins search) and
`hunmin_tsf_resolve_layout_slot` (the mode/slot check); `datadir.h` (31)
declares both, and `settings.h` / `tsf.h` include it instead of repeating
them. The settings dialog and the shell still call the same functions, so
they cannot disagree about where data came from. `tsf.cpp` was 1,384 lines.

Stage C then moved the composition machinery out: `composition.cpp` (457)
carries `OnCompositionTerminated`, the `CompositionSession`, `CompositionWrite`
/ `CompositionEnd` / `DetachComposition`, the `FlushSession`, the
boundary/engine drain (`FlushEngineIntoWordbuf`, `BoundaryFlush`), the
`RectSession` and `CaretRect`. The edit-session classes stay nested members of
`CTextService` (declared in `tsf.h`, defined in the new TU), so they keep the
enclosing class's access without a new seam; the delivery seam and
`DisplayAttr` are used exactly as before. `tsf.cpp` was 945 lines.

Stage K then moved the key path out: `keydispatch.cpp` (386) defines the
test/down/up handlers, `TrackKeyContext`, the passage and modifier
predicates, the mode toggle, `DrainAndPass`, `VkToBase`, `HarvestFinalised`
and `FeedEngineKey`. These are member definitions of `CTextService`
(declared in `tsf.h`), so no header or seam was needed. `tsf.cpp` is 575
lines.

## Where the code stands

`frontends/tsf/src/tsf.cpp` is 575 lines (was 2,320 before stage 4, 2,146
after it, 2,076 after the S5 extraction, 2,096 after the registration
helpers, 1,659 after S4, 1,492 after S7, 1,384 after F, 945 after C and 575
after K; the figures here are physical lines). `CTextService` now carries the
activation lifecycle and the document-manager callbacks; the key path lives
in `keydispatch.cpp` (386), the composition machinery
in `composition.cpp` (457), the DLL's exports and registration in
`registration.cpp` (451), the shared GUIDs in `ids.cpp` (19), the preedit
styling in `displayattr.cpp` (196), the data source in `datadir.cpp` (122),
and the language-bar surface in `langbar.cpp` (276
lines) behind `langbar.h` (82), alongside `delivery.cpp` (325) and
`candidates.cpp` (345). After stage 5 the settings surface is five TUs;
`settingsdialog.cpp` (731) is now the largest non-shell TU and
`settings.cpp` (25) is only the export; the shell is 575. It is green
(tier-F1) and behaves; the remaining complaint is the settings-exe resolver
duplication and the un-tabled QI.

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
4. **`tsf.cpp`** — the shell's implementations plus the free functions the
   settings TU shares (`hunmin_ctx_from_module`, `resolveLayoutSlot`). The
   `Dll*` chain moved out in stage S4.
5. **`registration.h`/`registration.cpp`** (stage S4) — the DLL's COM
   surface: the class factory, `DllGetClassObject`, `DllRegisterServer` /
   `DllUnregisterServer` and their file-static helpers. `registration.h`
   declares only the three names the DEF exports. The CLSID, profile and
   display-attribute GUIDs live in `ids.h`/`ids.cpp` because registration
   and the display-attribute provider both name them.
6. **`utf.h`** (extracted from the candidate path, stable, no deps) —
   `HunminUtf8ToWide` / `HunminWideToUtf8`, the one conversion pair.
7. **`langbar.h`/`langbar.cpp`** (stage 4, issues #67/#103): `LangBarItem`
   implements `ITfLangBarItemButton` + `ITfSource` as a tear-off; the shell
   returns it from QI and passes it to `AddItem`/`RemoveItem`. The item
   reads the mode through `LangBarHost` (`tsf.h`) and owns the kernel sink,
   the hosting shape (`GUID_LBI_INPUTMODE`, `BTN_MENU|SHOWNINTRAY`), the badge and the settings menu.
 8. **`displayattr.h`/`displayattr.cpp`** (S7) — the preedit styling: the
   two provider objects (`ITfDisplayAttributeInfo` and the one-entry
   `IEnumTfDisplayAttributeInfo`) and `DisplayAttr`, which owns the
   `TfGuidAtom`, registers the GUID once and sets or clears the attribute
   over a range. The shell's two provider methods are forwards; the
   composition session calls `DisplayAttr::Apply`.
 9. **`settingsrows.h`/`settingsdialog.h`/`settingslaunch.h`** (stage 5,
   issue #71) — the settings surface split by responsibility:
   `settingsrows` reads the engine list into the dialog's rows and knows no
   window; `settingsdialog` is the interim Win32 surface (template, proc,
   W11 look, capture); `settingslaunch` resolves and waits on the WinUI 3
   exe; `settingsprovider.cpp` carries the TSF plumbing. `settings.cpp` is
   the only C-linkage piece, the rundll32 export.
 10. **`datadir.h`/`datadir.cpp`** (F) — the one data source
   (`HUNMIN_DATA_DIR` > `<module>\data` > the repo climb) and the stored-layout
   slot check. The shell, the interim dialog and the tier-F1 tests share
   these, which is the point: no two surfaces resolve data differently.
 11. **`composition.cpp`** (C) — the native preedit machinery: the host
   termination handler, the `CompositionSession`/`FlushSession`/`RectSession`
   edit sessions, the write/end/detach half and the engine/boundary drain.
   It has no header: the session classes stay nested inside `CTextService`
   (declared in `tsf.h`) and are DEFINED here, which keeps their access to
   the shell's composition state without widening the class's public surface.
   Delivery and the styling are reached through their existing seams.
 12. **`keydispatch.cpp`** (K) — the key path: test/down/up, context
   tracking, the passage and modifier predicates, the mode/candidate
   toggles, `DrainAndPass`, `HarvestFinalised` and `FeedEngineKey`. No
   header: these are `CTextService` member definitions, declared in
   `tsf.h`; the shell keeps the lifecycle and document callbacks.

## Staged execution (each stage lands ctest-green, then commits)

| Stage | Content | Risk |
|---|---|---|
| 1 | `utf.h`; the fold-in fragments dissolve (the text back inline) | low |
| 2 | `delivery.h`/`delivery.cpp`: `TsfCore` + the delivery types + their states; the interim machinery out of the shell | medium — the interim write half touches the compose half; the CMake target lists change |
| 3 | `candidates.h`/`candidates.cpp`: the flow class + `CandBackend` on the shell; the tier-F1 candidate tests read a Snapshot accessor instead of the service's private fields | medium — the white-box seam's reach shrinks |
| 4 | `langbar.h`/`langbar.cpp`: the item as a tear-off (`ITfLangBarItemButton` + `ITfSource`); QI forwards, `AddItem`/`RemoveItem` take the item | medium — the shell's identity changes for three IIDs; the hosted-tray behaviour needs a host re-measure |
| 5 | `settingsrows`/`settingsdialog`/`settingslaunch`/`settingsprovider`: the 979-line settings TU split by responsibility, export left behind | low — pure relocation; only the two statics become cross-TU calls, max TU 731 |
| 6 (S4) | `registration.cpp`/`registration.h` + `ids.cpp`/`ids.h`: the register/unregister helpers, the class factory, `DllGetClassObject` and the two exports out of the shell; the shared GUIDs into `ids` | low — pure relocation; verified by link + build, the elevated regsvr32 cycle is a manual checklist |
| 7 (S7) | `displayattr.cpp`/`displayattr.h`: the provider objects and the `DisplayAttr` atom channel out of the shell | low — pure relocation; the painted underlines need a host look (not ctest-observable), the provider shape is unchanged |
| 8 (F) | `datadir.cpp`/`datadir.h`: the data-source search and the layout-slot check out of the shell; `settings.h`/`tsf.h` include the new header | low — pure relocation; the tier-F1 datadir and config tests already assert both functions |
| 9 (C) | `composition.cpp`: the termination handler, the three edit sessions, the write/end/detach half and the engine/boundary drain out of the shell; the session classes stay nested in `tsf.h` and are defined here | low — pure relocation; the three win32 traces and the candidate group already exercise the composition lifecycle |
| 10 (K) | `keydispatch.cpp`: the test/down/up handlers, the predicates, the toggles, `DrainAndPass`, `VkToBase`, `HarvestFinalised` and `FeedEngineKey` as member definitions | low — pure relocation; the three win32 traces drive the whole key path and the candidate group the toggle |

The `settings.cpp` split (S10) landed 2026-10-22 (stage 5), the registration
TU (S4), the display attribute (S7), the data source (F), the composition
machinery (C) and the key path (K) on the same day (stages 6-10). Still open
from S5: the shell's duplicated paths and the table-driven QI; next is the
settings-exe resolver unification (candidate routing needs a ruling), then Q.

The standalone TSF configure now passes `/utf-8` like the top-level engine
build, so MSVC reads the UTF-8 sources (Korean and em dashes in comments) as
UTF-8 and C4819 is gone from every TU in this project.

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
