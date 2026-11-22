---
title: "Candidate gloss on IBus: verified paths, and the correction of 'IBus cannot'"
created: 2026-11-22
updated: 2026-11-22
type: measurement
status: active
tags: [ibus, fcitx5, candidates, presentation, measurement]
sources:
  - ../../../docs/SPEC-FRONTEND.md
  - ../../../frontends/ibus/src/hunmin_state.h
  - ../../../frontends/ibus/src/hunmin_engine.c
  - https://github.com/scobyseo/hunmin/issues/316
confidence: high
ingested: 2026-11-22
---

# Candidate gloss on IBus: verified paths

The maintainer set the requirement (2026-11-22): "ibus로 전환하기 위해서는 그
기능을 지원할 방법을 찾아야 해" — gloss display must be supported for the
IBus switch. Recorded as FE16 in `docs/SPEC-FRONTEND.md`. This page is the
verified analysis; the presentation choice is the maintainer's.

## The correction, first

Earlier in this session I said IBus "cannot" show the gloss because the
panel has no comment slot. **That was wrong as stated.** The engine's own
header says the quiet part: `hunmin_state_candidate_detail()` is exposed
"so the choice is a frontend's and is measured rather than assumed" — the
current engine DROPS the gloss by choice; the host does have a display path.
The claim conflated "no per-candidate comment slot" (true) with "no way to
show the gloss" (false). This is the [[the-rules-keep-breaking]] shape: a
true fact stretched into a false impossibility.

## What is verified, from the 1.5.26 source

- `ibus_engine_update_auxiliary_text` exists at the 1.5.26 floor
  (`src/ibusengine.h`).
- The GTK3 panel ROUTES it: `panelbinding.c` forwards
  `UpdateAuxiliaryText` to the candidate panel.
- The candidate panel DRAWS it: `candidatepanel.c` keeps a dedicated
  `GtkLabel *m_aux_label` and sets its text in
  `candidate_panel_set_auxiliary_text`.
- The data path already exists in this tree:
  `hunmin_state_candidate_detail(state, i)` returns the gloss, and the
  sink's candidates callback fires on every cursor change
  (`candidates_notify`), so the highlighted candidate's gloss can be
  republished as the cursor moves.

What is NOT possible: a per-candidate second text slot. `IBusText`
attributes carry colour/underline only, and the lookup table's labels are
the 1..9 selection numbering. "Comment beside every candidate at once" has
no native IBus slot.

## The comparable designs

| | A: auxiliary text | B: gloss embedded in candidate text |
|---|---|---|
| what shows | gloss of the HIGHLIGHTED candidate, above/below the table, tracks the cursor | gloss beside EVERY candidate, always visible |
| candidate text | stays bare Hanja — shared expectations unchanged | becomes "刊 (간할 간)" — D2 assertions and any candidate-text golden change |
| closest to fcitx5 today | no (fcitx5 shows all comments at once) | yes |
| host slot | native, verified drawn at 1.5.26 | native (candidate text is always drawn) |
| engine cost | ~20 lines: publish detail on open/cursor change | ~5 lines: append detail per candidate |
| panel width | unchanged | grows with the gloss |

Recommendation: **A**, because it keeps the candidate text the shared
corpus already asserts and uses a host-native slot verified at the floor.
What would change it: the maintainer wanting all glosses visible at once
(the fcitx5 look) — then B, accepting the candidate-text change.

## The fcitx5 5.1.7 half (floor-lowering path)

Verified in the 5.1.7 source: classicui draws `auxDown`
(`src/ui/classic/inputwindow.cpp`) and `candidate.customLabel()`. So where
`setComment` (5.1.9+) is absent, the same highlighted-gloss UX is available
via `auxDown`; `customLabel` works too but replaces the selection numbering
unless the number is carried inside it. Version-conditional `setComment`
plus an `auxDown` fallback gives one UX across versions — and matches
IBus option A. See [[2026-11-22-fcitx5-retirement-ci-cost]] for the CI
context and [[2026-11-22-ibus-coverage-parity]] for the coverage table
whose gloss row this supersedes.
