---
title: No frontend core move — the seam is a contract
created: 2026-11-08
updated: 2026-11-08
type: decision
status: active
tags: [decision, modes, engine, android, windows, fcitx5]
sources:
  - ../../../docs/DESIGN-FRONTEND.md
  - ../../../docs/DESIGN-PHASE5-CORE-MOVE.md
confidence: high
---

# No frontend core move — the seam is a contract

**Ruling (2026-11-08, issue #272, selection D1).** Asked which slice of the
frontend core-move to build, the maintainer closed it: no core move, and the seam
written down as a contract instead.

The contract is `docs/DESIGN-FRONTEND.md` §11; the evidence behind it is
`docs/DESIGN-PHASE5-CORE-MOVE.md` Steps 1–3.

## What the inventory showed

Issue #272 measured ~570–620 code lines of judgement/autoswitch/caps/candidate/
`v1:`-binding logic as triplicated across fcitx5, Android and TSF. Separating
host actions from engine-facing workflow shrinks that number substantially:

- **The judgement's host action is the differing part.** All three frontends
  read `take_judgement` and branch on `HUNMIN_LANGUAGE_*`; what happens next is a
  fcitx5 input-method trigger, a TSF mode plus delivery/passthrough change, or an
  Android layer plus `InputConnection` change. A core helper would have to model
  host topology, which the equivalence invariant and FE13 forbid.
- **The autoswitch value, its data gate and its read-back are already core's.**
  Only the stores differ (fcitx5 config watcher and push, Android prefs over two
  engines, TSF registry plus `ApplyAutoswitch`), and consolidating them needs a
  cross-frontend state contract verified on all three tiers.
- **Caps differences are host event models, not duplication. Candidate policy is
  already in the core; only presentation is a frontend's. `v1:` is a stored
  format, not a shared parser** — TSF stores a VK, Android a keyCode.

## What it settles, and what would reopen it

Settled: the core owns judgement, readings/validity, the short-word guard, the
`autoswitch` value and gate, special-chord resolution, key classification and the
buffer cap; each frontend owns the host action, key translation, delivery,
candidate presentation, settings storage and the stored binding format. A
duplicated *policy* gets a `DESIGN-*.md` heading before it gets code.

Reopening it is a new ruling, not a continuation: FE11's shell scope for Android
and TSF written down and showing duplicated policy rather than wiring, or a
second frontend needing an identical state contract. See
[[2026-11-01-the-engine-judges-the-frontend-switches]] for the ownership ruling
this rests on, and
[[2026-11-08-no-modes-measurement-until-the-maintainer-says-so]] for the other
2026-11-08 ruling that closed a modes question rather than deferring it.