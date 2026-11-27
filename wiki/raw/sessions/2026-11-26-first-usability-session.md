---
title: "The first usability session: the trace found a backspace that commits, a space bar that deletes, and a shift that was intended"
created: 2026-11-26
updated: 2026-11-26
ingested: 2026-11-26
type: session
status: active
tags: [android, tests, measurement, design]
sources:
  - ../../../docs/DESIGN-ANDROID-SYMBOL-GESTURES.md
  - ../../../docs/SPEC-ANDROID-KEYPAD.md
  - ../../../docs/SPEC-FRONTEND.md
  - https://github.com/scobyseo/hunmin/issues/312
  - https://github.com/scobyseo/hunmin/issues/332
confidence: high
---

# The first usability session: the trace found a backspace that commits, a space bar that deletes, and a shift that was intended

The maintainer asked for logging because the shipped keypad did not behave as
expected in real use ("쓰다 보니 입력이 기대대로 안되는 경우가 많이 나타나서").
The engine trace could not answer that — it records engine keys, while the
mismatches happen above the engine — so a UI-layer sink was added
(`UiTraceRecorder`, commit `15c9021`, design doc §6.5) and one phone session was
captured (316 UI gestures, 272 engine keys).

Artefacts live outside the repository:
`~/work/hunmin-measurements/usability-eval-312-2026-11-26/` with `meta.json`
(it is one evaluator's session, not a controlled experiment).

## What the trace showed, and what the maintainer ruled

**1. Backspace committed the composing word.** 9 of 34 backspaces (26%) came
back `consumed=false` with a non-empty commit, so the word was flushed into the
text instead of edited; 8 of 39 typed characters were leftover jamo. The oracle
reproduced it deterministically on the layer path Android uses
(`hunmin-sim --layer word --keys $'rkrk\b\b\b'` → `commit '가'`), while the ic
path deletes normally. Ruling: engine fix, both surfaces alike, its own issue →
#332, requirement `docs/SPEC-FRONTEND.md#§FE18`. This is the finding that
[[the-rules-keep-breaking]] would call a behaviour nobody had asserted, not a
bug someone forgot: the SYLLABLE delete unit's "one DEL too many commits the
buffered word" was documented (`docs/DESIGN-ANDROID-KEYPAD.md`) as a property,
and the usability session is what turned it into a complaint.

**2. Swipe-left on the space bar deleted text.** Twice in the session. Ruling:
"커서 이동으로 수정하자" — implemented as the recorded exception in
`KeypadModel.swipeLeftAction` (cursor left on space, delete everywhere else),
E1-pinned and device-verified (`ab` + swipe-left + `x` → `axb`).

**3. Two upward-left swipes on `t` typed ㅆ.** The maintainer: "이건 시프트를
의도한 게 맞아" — the recogniser was right and the measurement was of intent,
not of a defect. Written down so a later reader does not re-open it.

## Why this round is worth keeping

- **The gesture layer was not the problem.** Every non-tap mapped to its ruled
  action, all four symbol pages cycled, and the three long-press events produced
  exactly the doubled jamo the session needed. The 200 ms default was fine here.
- **What failed was a delete.** The most ordinary key on the keyboard, and the
  defect only appeared through real editing (delete a typo mid-word), which is
  why no golden had caught it: the goldens type forward.
- **The logging earned its cost in one session.** Without the `dx`/`dy`/gate
  fields, "my swipe-up became a tap" and "my backspace committed my word" would
  both have been unanswerable anecdotes; see the superseded
  2026-11-23 `312-symbol-variants` record for the earlier session where an
  unverified premise cost three commits, and [[where-the-practices-came-from]]
  for the rule it broke.