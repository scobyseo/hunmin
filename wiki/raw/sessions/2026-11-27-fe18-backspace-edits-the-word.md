---
title: FE18 — backspace edits the composing word, and the checklist's walk was a guess
created: 2026-11-27
updated: 2026-11-27
type: session
status: active
tags: [session, engine, backspace, fe18, buffered-preedit, autoswitch, android, tsf]
sources:
  - ../../../docs/SPEC-FRONTEND.md
  - ../../../libhunmin/src/buffered_preedit.c
  - ../../../libhunmin/tests/buffered-preedit-test.cpp
confidence: high
---

# FE18 — backspace edits the composing word

One line: the buffered layer now answers a BackSpace the engine declined by
deleting the last code point of its own word, so the word stays composing and
identical keystrokes give identical text on the physical and the soft path —
and the walk the issue's verification checklist predicted was off by one, which
the measurement, not a ruling, corrected.

## What was wrong

`docs/SPEC-FRONTEND.md` §FE18 (`#332`) records the maintainer's words
(2026-11-26): backspace deletes while keeping the word, the same on physical and
soft keyboards, as an engine fix. The measurement behind it:
`hunmin-sim --layer word --keys $'rkrk\b\b\b'` committed `가` instead of
deleting — 9 of 34 backspaces in one phone session left jamo in the client.

The mechanism: `hunmin_ic` keeps one syllable and declines a DEL once its jamo
stack is empty; the finished syllables of the same word sit in
`hunmin_buffered_preedit`'s `word`. "A declined key IS the boundary" then
committed that word. The engine's decline was correct — it knows nothing of the
layer's syllables — so the fix belongs to the layer.

## The fix

`backspace_deletes_word()` in `libhunmin/src/buffered_preedit.c`: when a
BackSpace (`base == 0x08`) is declined in `HUNMIN_COMMIT_AT_BOUNDARY` mode and
the word is not empty, `word_pop_char()` removes its last UTF-8 code point and
the key is consumed — no commit, nothing passed to the client. In
`HUNMIN_COMMIT_PER_SYLLABLE` mode nothing changes: those syllables are already
in the client, whose own delete the declined key reaches.

Measured after the fix, `dubeolsik --layer word --keys $'rkrk\b\b\b\b'`:

```
ㄱ → 가 → 각 → 가가 → 가ㄱ → 가 → (empty) → (declined)
```

No key commits; the fourth BackSpace is the declined one. This is exactly the
ic path's walk on the same keys (the in-flight syllable first, then the
finished one), which is what FE1/FE2 ask for.

## What the checklist got wrong

`#332`'s verification checklist said the preedit "walks 가가 → 가 → ㄱ →
(empty)". Measurement gives `가가 → 가ㄱ → 가 → (empty)`: the DEL acts on the
engine's in-flight syllable before the layer's finished one, so its jamo come
off first. The endpoints the checklist cares about — no commit, the fourth
BackSpace reaches the client — hold; the two middle states were a guess. The
corrected walk is in FE18's implementation note, and the issue body is not
edited (it is the request record, not the result).

## The conflict ReqMode4 now carries

`docs/SPEC-MODES.md` §ReqMode4 constrains "the consumed/declined decision for
each key", and FE18 makes the boundary layer consume a BackSpace the engine
declines. Those two clauses answer differently for that one key.
`hunmin-sim --off-check` was changed to apply the client's delete on the side
that declined a BackSpace and compare the text a person ends with; the text
comparison still fails if a layer swallows a BackSpace it had nothing to
delete. The measurement is recorded in §ReqMode4 and in FE18 with the ruling
left open — a test edit is not where that gets settled.

## The abstain, and what it costs

A delete breaks the correspondence between the key log and the word: the log
records (base, shift) slots that produced text, and it cannot un-press them.
So `keys_unreliable` is set for the rest of the word and the `ReqMode8`
readings are not taken — the automatic switch abstains rather than infer a mode
from text that is no longer on screen. The alternative (recompute the log from
the word) needs a per-character key mapping the commit text does not carry.
The abstain is fail-closed and is recorded in FE18 rather than being a ruling.

## Evidence

- `engine-buffered-preedit` — `BackspaceEditsTheComposingWordAndNeverCommitsIt`.
- `engine-golden-case-dubeolsik` — the ic half of the walk (`backspace-edits-the-word`).
- `engine-android-e1` (219/219) — `HunminAdapterTest.backspace deletes the
  buffered word instead of committing it`; the old test asserted the defect.
- TSF cases 11d–11e updated to the same contract, **not run** (Windows host).
- The Android espresso E2E round and the phone session are environment-bound
  and were not re-run here; the E2E warm-up path is unaffected.

## Related

- [[2026-11-26-first-usability-session]] — the session that found the defect.
- [[2026-11-22-ibus-coverage-parity]] — the edit-key lanes the new tier-A case
  and the frontend tests sit beside.