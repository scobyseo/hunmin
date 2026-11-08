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
