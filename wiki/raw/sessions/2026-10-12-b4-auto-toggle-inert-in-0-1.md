---
title: The B4 0.1 ruling — auto toggle inert by disabling the default key composition
created: 2026-10-12
updated: 2026-10-12
type: session
status: active
ingested: 2026-10-12
tags: [session, decision, android, keybinding, b4]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/MILESTONES.md
confidence: high
---

# The B4 0.1 ruling — auto toggle inert by disabling the default key composition

The issue-#2 session: what was measured for the ruling, the maintainer's two
sentences, and how "disable the default key composition" became a tree change.
The ruling itself is on
[[2026-10-12-auto-toggle-disabled-by-default-in-0-1]]; this page is the how.

## The measurement that framed the question

Posted as the issue comment before any ruling was requested, it established:

- The stage-2 handler existed fully wired at the binding layer — default chord
  Right Alt + Enter, dispatch in `onKeyDown`, a settings row advertising it —
  but the handler body only logs "not yet implemented", and stage 8 is blocked
  on B4 → B1.
- "Ship working" collides with the B3 ruling (OFF until 0.3, data-gated): a
  working toggle in 0.1 is an ON switch for a feature with no judgement source
  and no data-package mechanism.
- "Ship inert" is not zero work: ReqMode13 forbids the UI advertising a chord
  whose action does nothing — the row had to stop advertising or the screen
  lies.
- The issue body's citation of `docs/TESTING.md:112-114` for the
  no-`KEYCODE_F9` fact had drifted; the fact lives at lines 280-281.

## The two sentences

1. `make the feature disabled by default in 0.1` — the shipping state.
2. `to do so, disable the default key composition` — the mechanism, given in
   answer to "how".

The second sentence is what fixed the shape: an empty `defaults` list, the
convention `KeyAction.HANGUL_ENG` and `KeyAction.HANJA` already use, rather
than a new per-feature flag. A paraphrase ("turn the binding off") would have
left the stale-binding hole and the row's honest-state requirement
undiscovered.

## The three tree facts that make "inert" true

1. **No default chord** (`KeyBinding.kt`): with nothing stored, Right Alt +
   Enter is unbound — Enter reaches the editor, not the stub.
2. **The dispatch skips the action** (`HunminInputMethodService.onKeyDown`):
   a stored binding from a build where the row was live cannot swallow the
   chord either. The skip is per-action, not per-chord, so precedence order
   and the conflict warning are untouched.
3. **The row is disabled and honest** (`OverlayActivity`): badge 미정 (0.3),
   not clickable, does not open the capture dialog — the ReqMode13 divergence
   removed rather than papered over.

A test pins the binding-layer half (`auto toggle has no default and a stored
binding is inert` in `KeyBindingTest`); the dispatch skip is the service's
own, pinned by the settings-window tier E2 path it shares.

## What would have gone wrong without the mechanism sentence

Only hiding the row would have left the default chord live: Right Alt + Enter
still consumed, still logging, the user's Enter gone. The chord is the
binding, not the row — which is the whole lesson of
[[2026-10-11-b3-off-default-and-the-data-gate]]'s "date vs condition" half:
disable the thing, not the sign for the thing.
