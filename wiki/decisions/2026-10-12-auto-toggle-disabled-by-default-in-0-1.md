---
title: B4 0.1 half — the auto Hangul/English toggle ships inert, default key composition disabled
created: 2026-10-12
updated: 2026-11-02
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

## Superseded 2026-11-02 (issue #174) — the Android half is live

Implemented rather than kept inert, because the state this page ruled about has
changed: the engine half landed ([[2026-11-01-the-engine-judges-the-frontend-switches]]
— the timing; issues #164/#165/#169 — the judgement, the `autoswitch` option and
B3's data gate), and issue #174 asks for the Android frontend to implement it.
Measured 2026-11-02, tier E1 (`AutoSwitchTest`, `AutoSwitchGateTest`):

- the service's dispatch **fires** `AUTO_TOGGLE` again, and the press writes the
  engine's `autoswitch` value — the read-back, never the request, so the row
  cannot show a state the judgement does not hold;
- the settings list reports the engine's value and the engine's chord
  (`Ctrl+한/영`), and says "사용할 수 없음" rather than "꺼짐" when B3's gate
  answers absent;
- **the default chord stays empty**, which is where this page's own last
  paragraph expected a default to be re-added. Issue #174 rules on the
  *mechanism* instead: the assignment is the engine's chord (the 2026-10-31
  namespace ruling), and the configurable binding is a second surface onto the
  same state rather than a second assignment. The stale-binding guard is
  therefore gone while the empty default remains — the sentence in the section
  above is left as it was written, and this paragraph is the correction.

What is **not** settled here: the 0.1 release lane's own choice is untouched (a
later release could still decide the feature is OFF in 0.1); whether the
configurable `AUTO_TOGGLE` binding should be retired now that the engine owns
the chord; and the full settings screen (issue #177), of which this is a row.

See also [[2026-10-31-special-functions-engine-resolved-frontend-implemented]].
