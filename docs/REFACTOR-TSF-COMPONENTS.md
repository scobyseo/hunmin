# Refactor proposal: the TSF frontend as components (DIP + SAP)

Status: PROPOSAL — the staged refactor the maintainer rules on; nothing is
implemented yet (the working tree holds the tested fold-in shape at
`8db2cf1`, implementation re-verified live 2026-10-21).

## Where the code stands

`frontends/tsf/src/` compiles `ctextservice`-as-one-class: `tsf.cpp` is
2,554 lines holding `CTextService` entirely inline, with the F9 Hanja
candidate flow (`candidates.cpp`, 179 lines) and the delivery seam
(`delivery.cpp`, 321 lines) folded INTO the class body — the
`keybinding.cpp` one-TU pattern. It is green (tier-F1) and behaves; the
complaint is structure: the TU is too big and the seams are invisible.

## The rules this refactor obeys (the maintainer's phrase)

- **DIP**: a component depends on ABSTRACTIONS the shell owns, never on the
  concrete TIP class; the shell depends on components and wires them.
- **SAP**: everything depends toward the shared, stable half — `log.h` has
  no dependencies; the component headers depend only on `log.h` (and,
  where the engine ABI is the subject matter, `hunmin.h`); the shell
  composes the components. Including a `.cpp` is banned in both directions.

## Design: the seams, exactly

1. **`delivery.h`** (the mode-delivery seam, issue #103's rules):
   - `class TsfCore` — the abstract slot the shell's behaviour plugs into:
     `wordbuf_`/`compText_` accessors, `ic_` cursor, `client id`,
     `InsertBufferedWord`, `CompositionWrite`, the gated logger handle.
   - `struct Delivery` (unchanged shapes): `Consumed / Border / Anchor /
     Anchor / CandidateCommit / FlushPending`.
   - `struct NativeDelivery` / `struct InterimDelivery` — own their states
     (the interim's deferral machinery incl. the timer + writer static).
     Defined in `delivery.cpp`; both take `TsfCore&`.
2. **`candidates.h`** (the Hanja flow, issue #67):
   - `class CandBackend` — the seam the flow READS: `Anchor / CommitPicked /
     CaretRect / EngineContext`.
   - `class HunminCandidates ctor(ctor(CandBackend&)`: one page constant,
     `LastSyllableOf` + `HanjaListCount` shared by the test/down halves,
     open/close/motion handlers, the commit that replaces the anchor.
   - No `CTextService` type appears here.
3. **`tsf.h`** — the shell's declaration: `CTextService : ... ; TsfCore`
   (implements the two seams via one-line forwards; owns the callbacks,
   the composition/update machinery, the langbar item, the TRAY —
   note: the tray module is removed; the registered path's registered
   lane is the langbar item).
4. **`tsf.cpp`** — the frontend's own exports: `Dll*` + the class factory +
   the free functions (`hunmin_ctx_from_module`, `resolveLayoutSlot`, the
   settings resolver) — resolved to the components.
5. **`utf.h`** (extracted from the candidate path, STABLE, no deps) —
   `HunminUtf8ToWide` / `HunminWideToUtf8`: the one conversion pair.

## Staged execution (each stage lands `ctest`-green, then commits)

| Stage | Content | Risk |
|---|---|---|
| 1 | `utf.h` + dissolve `candidates.cpp` (the flow's text back inline) | low |
| 2 | `delivery.h/.cpp`: `TsfCore` + the delivery types + their states; the interim machinery out of the shell | medium — the interim write path touches the compose half; CMake target lists change |
| 3 | `candidates.h/.cpp`: the kernel-side item's class + the mode-side CANDIDATE config; the user-facing tests' injected `CTextService::` overrides degrade to the defaults | medium — the user-facing tests' injected/kern   `CTextService` `Valid` config seam moves |

## What stays untouched

- The engine ABI (no `hunmin.h` change, no new `di` id), the TIP
  registration, the input layouts, the config schema, the test units.
- The field-verified behaviors: the mode flip, the item's presence, the
  pop-up's ships.

## The decision the agent does not make

Whether to run this at all, and in which order — the repo rule: one CHANGE
per its OWN discussion, the maintainer decides.
