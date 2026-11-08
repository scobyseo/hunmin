# DESIGN-PHASE5-CORE-MOVE — Frontend core-move plan (issue #272, D1)

> **Status:** design/plan (D1 ruling: frontend core-move included in #272).
> This is a plan, not an implementation. Requirement text lives only in
> `docs/SPEC-*.md`; this document cites REQ ids and records mechanism/options.
> FE11 scope for Android/TSF shell code is noted where unwritten.

## Goal

Reduce triplicated frontend logic by moving host-independent behavior behind
core/library seams, while preserving frontend-specific host integration
(event mapping, presentation, focus/lifecycle, and platform key routing).
The move is a **design change**, not a line-count target (issue #272).

## Triplication map (measured starting points)

Issue #272 identified ~570–620 code lines of triplicated
judgement/autoswitch/caps/candidate/`v1:`-binding logic. Verify against
current code before each move.

| Area | fcitx5 | Android | TSF | Core today | Candidate move |
|---|---|---|---|---|---|
| Apply ReqMode8 judgement after boundary key | `hunminadapter.cpp` `applyJudgement()` reads `take_judgement`, toggles layer language, returns boundary commit | `HunminAdapter`/service path reads judgement and applies mode change | TSF key/composition path reads judgement and performs host-allowed switch | `hunmin_buffered_preedit_take_judgement`, `toggle_language`, judged delivery | Shared "apply judgement to layer" helper/API or a thin core step returning structured action; frontends still perform host presentation/IM-state effects. |
| Autoswitch option + data gate + read-back | `applyAutoswitch`, `pushAutoswitch`, config annotation/chord/data-present | `applyAutoswitch`, `AutoSwitchSettings`, overlay/settings UI | settings rows/read-back (`hunmin_tsf_autoswitch_row`) + config wiring | `set/get_option("autoswitch")`, `hunmin_ctx_autoswitch_data_present`, special chord | Core owns value/gate/read-back (already mostly true). Unify frontend glue patterns where possible; keep UI presentation frontend-owned. FE11 Android/TSF shell scope still unwritten. |
| Caps handling (per key event) | `hunminadapter.cpp` reads raw key CapsLock, passes `caps` option | service/adapter path passes caps/meta state per event | key dispatch translates modifiers | `set_option(...,"caps",...)` on layer/ic | Core API for "normalize caps for key event"? Only if it removes real duplication without hiding host key semantics. Many differences are host event models. |
| Candidate handling | fcitx5 candidate list/UI integration | Android candidate UI (`OverlayActivity`, adapter) | TSF candidate/display integration | candidate pages/list/cursor via layer | Presentation is host-owned; shared policy/state should stay in core. Move only duplicated *policy* not UI. |
| `v1:` binding grammar | — | Android key-binding storage/format | TSF `v1:<vk>:<mods>` parse/store/UI/config (`keybinding.*`, `config.*`, `settingsdialog.cpp`, settings UI) | no shared frontend binding parser | **Do not move UI/storage format blindly.** Evaluate a small shared parser/validator library or common test vectors only if fcitx5/Android need the same grammar; today TSF/Android share format concepts but hosts differ (VK vs Android keyCode). |

## Constraints

- Preserve SPEC/REQ behavior (especially ReqMode3/ReqMode8/ReqMode9/ReqMode10/ReqMode13/ReqMode13/B3, FE*).
- Keep frontends thin (AGENTS guideline) but do not force host-specific code
  into core (fcitx5 trigger ownership/FE13, TSF text-store delivery modes,
  Android IME lifecycle/UI).
- ABI append-only; symbol renames break docs-lint/REQ-MAP/abi. Prefer adding
  focused helpers if needed.
- No per-layout/frontend conditionals in core behavior.
- Each migration step must keep `ctest --test-dir build -j16 -LE slow` green
  and preserve the Phase 0 `ctest -N`/properties baseline unless a deliberate
  test registration change is documented (Phase 0 baseline then updated with
  justification).

## Proposed sequence (sequential, D2)

1. **Design lock per area**: for each row above, record exact current behavior,
   host constraints, and the minimal core seam. Reject moves where the common
   part is smaller than the adapter complexity.
2. **Autoswitch/judgement glue** (lowest host UI coupling): introduce/verify a
   core helper for "post-key judgement application result" if it removes real
   duplication across all three frontends; otherwise document why frontends keep
   thin adapters.
3. **Caps normalization**: only after measuring host key models; likely keep
   per-frontend extraction, centralize only option application semantics if
   duplicated.
4. **Candidate policy**: keep UI host-owned; move shared state transitions only
   if currently duplicated outside `buffered_preedit`.
5. **Binding grammar (`v1:`)**: create shared test vectors / parser module only
   if Android+TSF need identical parsing semantics; otherwise document the
   format as a cross-frontend contract in `DESIGN-FRONTEND.md` with
   host-specific field meanings.
6. **FE11 shell scope**: write Android/TSF FE11 boundaries (what remains shell)
   before/with code moves; update `DESIGN-FRONTEND.md`.

## Verification

For each implementation PR/commit series:
- [ ] Phase 0 baseline `ctest -N`/properties unchanged, or baseline updated with
      explicit justification.
- [ ] `cmake -B build && cmake --build build`
- [ ] `ctest --test-dir build -j16 -LE slow`
- [ ] `docs-lint`, `req-map`, `abi`, `schema-contract` green (or pre-existing
      failures explicitly accounted for, as at Phase 0).
- [ ] Frontend-specific tests covering touched paths: tier-D1/D2/D3 as
      applicable, Android/TSF tests available in the environment.
- [ ] No second-home figures/requirements; new multi-place rationale linked
      from `DESIGN-CORE.md`/`DESIGN-FRONTEND.md`.

## Open implementation questions (for maintainer when reached)

- Should the core expose a single "process key and return host actions"
  structure (judgement switch, commits, UI update hints), or should frontends
  continue composing existing calls? A larger struct is an ABI/design change.
- How much of TSF/Android settings UI (`v1:` capture/editing) counts as FE11
  shell versus shared logic?
- fcitx5 has one-IM/two-modes behavior (FE13 context noted in code comments);
  any shared judgement helper must not assume host mode topology.

## Step 1 inventory (2026-11-08, #272 P5/D1)

Purpose: make the D1 move testable and incremental by recording where the same
engine-facing workflow is consumed today. Facts only; no design decision.
Requirement/contract homes remain `docs/SPEC-MODES.md`, `docs/SPEC-FRONTEND.md`,
`docs/FRONTEND-EQUIVALENCE.md` (stub → `DESIGN-FRONTEND.md`), and REQ-MAP.

### Engine seams frontends consume (stable API surface)

- Context/layout/data: `hunmin_ctx_*`, layout mode/capability, `hunmin_ctx_autoswitch_data_present`.
- IC/layer: `hunmin_ic_*`, `hunmin_buffered_preedit_*` (`new`, `process_key`/option path,
  `commit_word`/flush, `take_judgement`, `latin_text`, `hangul_text`/readings as exposed,
  `classify`/key action, `special_{peek,set_mods,set_host,functions,clear,chord}`,
  options `caps`/`autoswitch`/`word_cap`/`judge_timing`/commit policy/layout ids).
- Candidate/hanja/idiom: `hunmin_ic_hanja_*` and related UI-facing candidate APIs.
- Frontends must not re-implement judgement, readings validity, autoswitch data gate,
  or special-chord resolution semantics (SPEC-MODES ReqMode3/8/13; DESIGN-FRONTEND).

### Triplication map (engine-facing workflow)

| Concern | fcitx5 | Android | TSF | Host-specific remainder |
|---|---|---|---|---|
| Adapter entry | `frontends/fcitx5/src/hunminadapter.{h,cpp}` | `HunminAdapter.kt`, `HunminNativeBridge.kt`, `hunmin-jni.c` | `keydispatch.cpp`, `composition.cpp`, `delivery.cpp`, `tsf.cpp` | fcitx5 InputContext/preedit/candidate UI; Android InputConnection/IME lifecycle/Compose UI; TSF ITf* composition/display-attribute/candidate window/threadmgr |
| Key/feed path | adapter handles key event → layer/classify/process | `onKey`/key handling → JNI/native layer | `OnKeyDown/Up` + keydispatch/composition | key code translation, modifier models (A2/A3/A2'/A3'), candidate navigation keys |
| Boundary/delivery | commit string + preedit update | commit/preedit via InputConnection | native/interim delivery modes | host delivery contracts, undo/selection behavior |
| Judgement action | reads `take_judgement`, performs mode change per host rules | reads/apply judgement, mode/autoswitch state updates | `ApplyAutoswitch`/judgement handling; interim/native differences | FE13 trigger ownership, interim cannot hold switch the same way (DESIGN-FRONTEND §2.x) |
| Autoswitch UI/state | config/UI wiring (partial per REQ-MAP) | `AutoSwitchSettings`, `OverlayActivity`, `SettingsActivity`, service wiring | `settingsrows.cpp`, `config.cpp`, dialog rows | settings storage/presentation remains frontend-owned; state meaning is engine-owned |
| Special chords | config/keybinding mapping to layer | `SpecialFunction.kt`, `KeyBinding.kt`, host handling | `keybinding.*`, settings/config | physical key names, default bindings, host-consumable actions |
| Candidates | fcitx5 candidate list | Android candidate UI | TSF candidate UI/window | presentation only; core owns candidate state/policy where exposed |

### Test coverage to preserve during D1 moves

- Tier A/G core: `engine-*` (buffered-preedit, key-rule, wordset, hangul-valid,
  golden-judgement, process-key, etc.), `abi`, `schema-contract`, `req-map`.
- fcitx5: `engine-d1-fcitx5`, `engine-d1-judgement`, `frontend-mutations`, conformance/corpus tests as registered.
- Android: tier E1 unit/instrumented tests registered in `cmake/HunminAndroidTests.cmake` where SDK present (`AutoSwitch*`, `JudgementCorpusTest`, etc.); `android-mutations`.
- TSF: tests under `frontends/tsf/test` (Windows build/test environment; not registered in this Linux ctest run).
- Guards: `docs-lint`, `wiki-lint`, `req-map-check`, frontend invariants/FE11 scope per DESIGN-FRONTEND.

### D1 constraints (restate)

- No frontend-local reimplementation of engine-owned semantics.
- Host-specific APIs stay in frontends; move only shared, test-equivalent logic.
- One step at a time, green targeted tests/baseline properties preserved.
- Any moved contract must keep REQ-MAP/SPEC/DESIGN pointers accurate (no `file:line` anchors).

## Step 1b — P5 micro-slice proposal (investigation, 2026-11-08)

No code move made. Facts gathered to identify a host-agnostic, testable D1 slice.

### Observed frontend patterns

- **fcitx5** (`hunminadapter.cpp`): copies `out.commit` before `applyJudgement()` (ordering fix #178); `take_judgement` → if `HUNMIN_LANGUAGE_*`, deliver switched text and perform host mode change; autoswitch apply/read-back/push with config and `autoswitchDataPresent_`.
- **TSF** (`keydispatch.cpp`, `composition.cpp`, `tsf.cpp`/settings): consumes `take_judgement`/autoswitch in key/composition flows; delivery differs (native/interim/legacy), and switch-ON disables English passthrough per ruling; host composition/edit sessions own delivery.
- **Android** (`HunminInputMethodService.kt`, `HunminAdapter.kt`, JNI): consumes `takeJudgement()` and performs `isHangulMode`/InputConnection/layer changes; autoswitch applied to both engines where present; explicit logging for rejected/null cases.

### Candidate slices (not selected)

| Candidate | Why it looks movable | Block / risk |
|---|---|---|
| "take judgement and emit host switch" | all read `take_judgement` and branch on `HUNMIN_LANGUAGE_*` | host action is host-specific (fcitx5 input method trigger/config, TSF mode/delivery/passthrough, Android IME mode/layers/InputConnection). Core already owns judgement; moving the branch target is not host-agnostic. |
| autoswitch desired→effective apply | B3/data gate exists in core; frontends gate/push/read-back | each frontend has different config/store/state propagation (fcitx5 config watcher/push, Android prefs + two engines, TSF registry/config + ApplyAutoswitch). Consolidation needs a cross-frontend state contract; verify on fcitx5+Android+TSF tiers. |
| key classify→action dispatch | `classify` already in core | dispatch performs host delivery/candidate UI/focus actions. |
| English shipped load/setup | `load_english_shipped`/data presence in core | when to load, error reporting, and per-frontend data-dir/lifecycle differ. |

### Recommended next investigation (before any D1 code)

1. Choose **one** seam with existing multi-frontend tests (fcitx5 D1/mutations, Android E1 AutoSwitch/Judgement where available, TSF tier-F1/judgement on Windows).
2. Define acceptance as **no behavior change**: same commits/preedit/judgement outcomes and host-observable state transitions for covered cases.
3. Prefer an **additive** core/helper API or documented adapter pattern first; refactor frontends one at a time only when that tier is runnable.
4. Record the micro-slice in an issue (`[core/frontend] #272 P5/D1: ...`) with verification commands; do not close issues.

**Status:** proposal only, for maintainer/next-session selection. No implementation in this step.

## Step 2 — P5/D1 characterization results (2026-11-08)

Selected approach: Option 1 — no production refactor, pin the invariants at the
move boundary before extracting anything.

Added/confirmed characterization (tests only):

- `engine-buffered-preedit` — `LayerTest.TakeJudgementIsConsumingAfterBoundaryDelivery`:
  a boundary key's delivered text is in that key's commit; `take_judgement()` is
  answered after boundary processing and is consuming (second read `-1`).
- Existing coverage already pins adjacent contracts (not duplicated here):
  - `engine-key-rule` B3 data-gate cases (`hunmin_ctx_autoswitch_data_present`).
  - `engine-wordset` reading/`latin_text` and second-read/switch cases.
  - fcitx5 `engine-d1-fcitx5`/`engine-d1-judgement` for adapter ordering/judgement.

Not characterized here (require their environments / no deterministic host-agnostic
assertion yet):

- Android E1 `takeJudgement`/autoswitch host-apply ordering (needs E1 runner).
- TSF native/interim/legacy judgement delivery ordering (needs Windows tier per
  `docs/WINDOWS-TSF-DEVENV-CHECKS.md`).
- A full “apply judgement then re-read state” adapter contract: the host action is
  host-specific, so an assertion would pin an adapter, not the core seam.

**Status:** Option 1 characterization pass complete for the core-observable seam.
Production micro-slice (Option 2 additive ABI or Option 3 fcitx5-only) still needs a
target choice; Option 2 additionally needs explicit ABI approval.

## Step 3 — P5/D1 Option 3: fcitx5 adapter comment dedupe (2026-11-08)

Selected Option 3 (fcitx5-only, behaviour-preserving) as the first code-adjacent
slice; no production logic changed, no ABI change, no cross-frontend claims.

- `frontends/fcitx5/src/hunminadapter.cpp`: condensed the rationale that belongs
  to a design home, keeping only adapter-local correctness facts:
  - shared-layer ownership of consumption/commit/preedit → pointer to
    `DESIGN-FRONTEND.md` §5; the CommitBehavior wording stays a short local note.
  - borrowed-string ordering (`out.commit` read after `applyJudgement()` loses
    the word) kept as a local measured fact, tightened.
  - `applyJudgement` keeps "one IM ⇒ mode is the layer slot" (ReqMode2) and the
    once-per-key read; the engine's list of `-1` conditions is no longer
    restated here.
  - `applyAutoswitch` keeps push-and-read-back; the B3 refusal stays the
    engine's and is no longer spelled out in the adapter.
- Old stub filenames in fcitx5 comments retargeted to
  `DESIGN-FRONTEND.md` Part A/Part B (`config.h`, adapter, and the fcitx5 tests
  that cite the equivalence/contract text). Part B is named explicitly where the
  old `§5` meant the invariants rather than the shared layer.

Not done in this slice: Android and TSF comment cleanup (their tiers are not
runnable here), and any logic move (needs a named seam plus all three fronts).

**Verification:** `cmake --build build -j8`; `engine-d1-fcitx5`,
`engine-d1-judgement`, `frontend-invariants`, `frontend-mutations` all pass;
`tools/check-frontend-invariants.py` holds (adapter 656 code LOC, limit 700);
`tools/req-map-check.py` passes; ctest registration/properties still match the
Phase 0 baseline.

## Step 4 — D1 ruled 2026-11-08: no core move

**The maintainer's selection: close D1 as no core move, and record the seam as a
contract.** The contract is `docs/DESIGN-FRONTEND.md` §11 — what the core owns
(judgement, readings, guard, autoswitch value/gate/read-back, special-chord
resolution, key classification, buffer/cap) and what stays a frontend's (the
host action, key-code translation and modifier models, delivery, candidate
presentation, settings storage, the stored binding format).

Why the measured inventory supports it rather than merely permitting it: the
common part of every candidate slice turned out to be a read of something the
core already returns (`take_judgement`, the `autoswitch` value, `classify`),
while the part that differs is the host action each frontend performs. Dedupe
of that branch requires the core to model host topology — which §1 and FE13
forbid — so the "~570–620 movable lines" measured when the issue was written
does not survive the separation.

**This document is therefore closed as a plan.** Steps 1–3 stand as the
evidence: the inventory, the micro-slice proposal, the characterization test and
the fcitx5 comment dedupe. No production code moves.

**What would reopen it**, recorded so the decision can be revisited rather than
re-argued: FE11's shell scope for Android and TSF written down and showing
duplicated *policy* (not wiring); or a second frontend needing an identical
state contract — today only the autoswitch desired→effective path looks like
that, and its three stores (fcitx5 config watcher, Android prefs over two
engines, TSF registry) still differ. Either is a new ruling.
