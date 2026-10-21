# Refactor proposal: the TSF frontend as components (DIP + SAP)

Status: PROPOSAL — the staged refactor the maintainer rules on; nothing is
implemented yet (the worktree holds the tested fold-in shape from
`8db2cf1`, re-verified live 2026-10-21).

## Where the code stands

`frontends/tsf/src/tsf.cpp` is 2,554 lines: `CTextService` compiles entirely
inline, with the F9 Hanja candidate flow (`candidates.cpp`) and the delivery
seam (`delivery.cpp`) folded INTO the class body — the `keybinding.cpp`
one-TU pattern. It is green (tier-F1) and behaves; the complaint is
structure: the TU is too big and the seams are invisible.

## The rules this refactor obeys

- **DIP**: a component depends on ABSTRACTIONS the shell owns, never on the
  concrete TIP class; the shell depends on components and wires them.
- **SAP**: everything depends toward the shared, stable half — `log.h` has
  no dependencies; the component headers depend only on `log.h` (and, where
  the engine ABI is the subject matter, `hunmin.h`); the shell composes the
  components. Including a `.cpp` is banned in both directions.

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
   composition write machinery, the langbar item, and the settings bridge.
4. **`tsf.cpp`** — the frontend's exports: the `Dll*` chain, the class
   factory, and the free functions (`hunmin_ctx_from_module`,
   `resolveLayoutSlot`, the settings-exe resolver).
5. **`utf.h`** (extracted from the candidate path, stable, no deps) —
   `HunminUtf8ToWide` / `HunminWideToUtf8`, the one conversion pair.

## Staged execution (each stage lands ctest-green, then commits)

| Stage | Content | Risk |
|---|---|---|
| 1 | `utf.h`; the fold-in fragments dissolve (the text back inline) | low |
| 2 | `delivery.h`/`delivery.cpp`: `TsfCore` + the delivery types + their states; the interim machinery out of the shell | medium — the interim write half touches the compose half; the CMake target lists change |
| 3 | `candidates.h`/`candidates.cpp`: the flow class + `CandBackend` on the shell; the tier-F1 candidate tests read a Snapshot accessor instead of the service's private fields | medium — the white-box seam's reach shrinks |

## What stays untouched

- The engine ABI, the TIP registration, the layouts, the config schema.
- The field-verified behaviours: the mode flip, the item's presence
  (`AddItem hr=0x0`), the pop-up, the interim fallback.

## The decision the agent does not make

Whether to run this at all, and in which order — the maintainer decides;
the proposal only carries the design and the stage costs.
