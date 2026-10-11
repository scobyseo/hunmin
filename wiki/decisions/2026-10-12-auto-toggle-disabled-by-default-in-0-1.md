---
title: B4 0.1 half — the auto Hangul/English toggle ships inert, default key composition disabled
created: 2026-10-12
updated: 2026-10-12
type: decision
status: active
tags: [decision, modes, android, release-plan]
sources:
  - raw/sessions/2026-10-12-b4-auto-toggle-inert-in-0-1.md
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/MILESTONES.md
confidence: high
---

# B4 0.1 half — the auto Hangul/English toggle ships inert, default key composition disabled

Closes the **0.1 shipping half** of **B4** (issue #2: "E5 한/영 key assignment —
ship inert or ship working in 0.1") from `docs/REMAINING-WORK.md` §1 and the
0.1 lane of `docs/MILESTONES.md`.

**Maintainer, 2026-10-12** (verbatim, from the issue-#2 session; see
[[2026-10-12-b4-auto-toggle-inert-in-0-1]]):

> make the feature disabled by default in 0.1

followed, when asked *how*, by the mechanism:

> to do so, disable the default key composition

## What was ruled

1. **The shipping state.** The auto Hangul/English toggle (voice-input stage 2)
   ships **inert in 0.1** — "disabled by default".
2. **The mechanism, in the maintainer's words:** disable the default key
   composition. In the tree this is three things, and all three were done:
   - `KeyAction.AUTO_TOGGLE`'s default chord (Right Alt + Enter) is removed —
     `defaults = emptyList()` in `KeyBinding.kt`, the same convention as 한/영
     and 한자's empty defaults. With nothing stored, **the chord is not
     consumed: Enter reaches the editor as Enter.**
   - The service's dispatch skips the `AUTO_TOGGLE` action (`onKeyDown`), so a
     **stale stored binding** — from a build where the row was live — cannot
     swallow the chord either. The handler itself stays as a log-only stub for
     the 0.3 lane.
   - The settings window's "자동 한/영 토글" row is rendered **disabled**, badge
     **미정 (0.3)**, and does not open the capture dialog. This is the
     R13-정정 constraint satisfied: the screen no longer advertises a chord
     whose action logs "not yet implemented" (the divergence
     `docs/SPEC-MODES.md` E5 forbids).

## What this does NOT close

- **The E5 assignment itself** — which key is the 한/영 key, and whether single
  vs compound input mean manual switch vs auto-toggle on/off. Still undecided
  (`docs/SPEC-MODES.md` E5), still a 0.3-lane item (`docs/MILESTONES.md`).
  The F9 constraint (occupied by 전처리수식, no `KEYCODE_F9` on Android) still
  applies to that future assignment.
- **B1 / D1 — the hangul judgement source.** Stage 8 (engine integration)
  remains blocked on it, exactly as before.
- **B2 — pass lines.** Unchanged.
- **B3's data gate** (the 0.3 mechanism,
  [[2026-10-11-auto-switch-off-by-default]]) — unchanged. The two rulings are
  consistent: 0.1 ships no switch at all, 0.3 ships one that is OFF by default
  and enable-able only where the separate data is present.

## Why the record matters later

The ruling is a *shipping state*, not a design: nothing here decides which key
the feature will use, so the 0.3 lane inherits the assignment question
untouched. The mechanism sentence — "disable the default key composition" — is
quoted rather than paraphrased because it fixed the implementation shape: an
empty-defaults convention that already existed for 한/영 and 한자, rather than a
new per-feature switch. If 0.3 lands the assignment, the row is re-enabled by
giving it a default again, and the stale-binding guard in the dispatch can go
at the same time.

See [[decisions-belong-to-the-maintainer]] — a ruling recorded because the
maintainer made it.
