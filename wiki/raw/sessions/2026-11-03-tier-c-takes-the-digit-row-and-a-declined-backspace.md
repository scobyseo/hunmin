---
title: 2026-11-03 tier C's x11 corpus takes the digit row and the declined BackSpace
created: 2026-11-03
updated: 2026-11-03
type: session
status: active
tags: [session, conformance, tier-c, corpus, issue-229, fcitx5, generator]
sources:
  - ../../../docs/TESTING.md
  - ../../../docs/trace-spec.md
  - ../../../PLAN.md
confidence: high
---

# 2026-11-03 tier C's x11 corpus takes the digit row and the declined BackSpace

One line: issue #229 asked for a declined key and the number row on x11; the
first half had already merged while the issue sat, the second half needed a
designed case per layout, and generating it exposed two generator defects that
had never been reachable before.

## What the issue claimed, and what the tree said

The issue measured 263 x11 events with `passed = 0` and reported that the digit
row was half pressed. Between the measurement and this session, the 두벌식 and
P2 designed families merged ([[2026-11-03-x11-corpus-replays-every-tier-a-case]]),
so the first premise no longer held: `tools/tier-c-wiring-coverage.py` read 598
events with 162 declined. What survived was narrower and worth checking line by
line rather than restating: which *kinds* of decline existed, and which digits
were pressed.

Measured before any change: every layout declined spaces (and 두벌식 declined its
unmapped digits and `.`), so the "the adapter never declines" half was already
closed; **no layout pressed `4`-`9`**, and **no layout declined a BackSpace**,
because the only BackSpace in any x11 trace found a non-empty buffer. So the work
was one case, not two, plus the two things generating it broke.

## The two generator defects

Both were unreachable until a golden carried an empty-buffer BackSpace, and both
would have produced a wrong expectation rather than a crash — the shape
`docs/trace-spec.md` invariant 4 exists to forbid.

1. **`consumed` for a named key was read off the previous preedit for EVERY
   named key** (`tools/gen-conformance.py`). A named key is not composition input:
   `libhunmin/src/buffered_preedit.c` classifies every non-text key as
   `HUNMIN_KEY_BOUNDARY_PASS`, so the adapter declines it. Only BackSpace carries a
   character (0x08) into the engine, and only that one is consumed when the buffer
   is non-empty. The old rule labelled a space or an arrow as consumed whenever a
   syllable was in flight. No committed trace carried one, so nothing was red —
   `trace-check.py`'s `.in`/`.gold` check would have caught it, which is exactly
   the detection a generator should not rely on.
2. **A golden spells BackSpace as `{"ch": "\u0008"}`, and the generator knew only
   `{"key": "backspace"}` and the two-character `\b` escape.** A golden written by
   `hunmin-sim --trace` therefore died with `no X11 keysym known for '\x08'` —
   and, less visibly, the old code read `spec["ch"]` unconditionally, so a golden
   in the *named* spelling raised `KeyError`. Measured: the pre-change generator
   cannot regenerate the committed `win32/dubeolsik-seosi.trace.in` at all.

## What landed

One designed case, `digit-row-and-empty-backspace` (`0123456789 \b`), for every
layout the x11 corpus replays: two cases added to `tools/gen-case-golden.py`'s
existing tables and two new tables (`sin3-p2-basic`, `sin3-p2-yet`) created for
it, because tier C is per LAYOUT and those two appeared in no other x11 trace.
Expectations stay the engine's: the goldens were regenerated from `hunmin-sim
--trace` and merged by `tools/merge-golden-cases.py`, and every `.gold` was
re-emitted and read before being accepted.

Measured after: 661 events / 185 declined on x11 (16 traces), `0`-`9` pressed by
every x11 layout, and each layout declines one BackSpace. 두벌식 declines all ten
digits, P2's three layouts deliver them as text with `consumed: true`, 세벌식 최종
turns them into jongseong jamo the closing space delivers.

## What is still open, and what it would take

- **A key the platform maps to NO base character** (arrow, Return, Tab) is still
  unobserved on x11, and cannot be generated: tier A feeds the engine a character
  (`tools/sim.c` `check_file` accepts `{"ch": ...}` and `{"key": "backspace"}`
  only), so no golden can carry an arrow, and hand-writing the expectation is
  what invariant 5 forbids. The base-0 branch would need an engine-level encoding
  at tier A first. Recorded as `TIER-C-WIRING-ARROW` in
  `docs/REMAINING-WORK.md`, with the reason it cannot be generated, rather than
  left as a phrase in an issue. Closing it is a rule for the maintainer
  ([[decisions-belong-to-the-maintainer]]).
- The issue's separate fidelity point — the android dialect writes printable keys
  as ASCII rather than `android.view.KeyEvent.KEYCODE_*` — was not touched here.
- Most punctuation (`, - = [ \ ]`, the backtick) is still pressed by no x11 event.

## Baseline note

`engine-d1-judgement` is red on this checkout **without** these changes too
(verified by stashing them): `commitString: ㅏ됴` against the pushed expectation.
Nothing in this session touches the judgement path.