---
title: The interim delivery cannot support autoswitch, and ON stops the English passthrough, 2026-11-02
created: 2026-11-02
updated: 2026-11-02
type: decision
status: active
tags: [decision, modes, tsf, delivery]
sources:
  - ../../docs/FRONTEND-EQUIVALENCE.md
  - ../../docs/WINDOWS-TSF-BUFFERING.md
  - ../../frontends/tsf/src/tsf.cpp
confidence: high
---

# Two rulings from one message, 2026-11-02

Both came as the answer to the two findings TSF's implementation of the
automatic switch exposed (issue #175). Quoted verbatim, because the wording is
the requirement — a paraphrase is what gets built.

> interim model cannot support autoswitch.

> autoswitch ON stops English passthrough to support eng -> han support

## What each one authorises

**The first** turns the delivery asymmetry into a ruled difference instead of a
defect. It is structural, not a missing call: `InterimWrite` moves the layer's
finished word into the document on every key (`hunmin_buffered_preedit_take_word`
REMOVES it) and `InterimDrain` closes with `ComposeReset` rather than
`commit_word()`, so at the boundary the layer holds only the syllable in flight —
the frozen log line is `interim boundary: word len=0`. There is no whole word to
judge, and ReqMode10 forbids reassembling syllables that are already client text.

Implemented as one setter (`CTextService::ApplyAutoswitch`) answering
`AutoswitchOn()` = stored value **and** native delivery, re-applied wherever the
delivery changes, so the engine never holds a state this delivery cannot honour.
Pinned by `TsfConfigTest.TheInterimDeliveryDoesNotHoldTheSwitch`.

**The second** makes ReqMode8's E1 line reachable on TSF. Until it, `OnTestKeyDown`
answered `eat = FALSE` for everything once the mode was Latin and `OnKeyDown`
returned at the passthrough branch, so a word typed in English mode never reached
the engine — and the 0.3 gate sentence (`English는 좋은 Language입니다`, tier A
`WordSetTest.TheZeroThreeGateMixedSentenceWorksInOneStream`) could not pass on
this frontend at all. Now both key phases admit the keys with the switch on, the
candidate surfaces stay Hangul-mode, and switch-OFF is byte-identical to what
shipped.

## What neither one decides

- **What the settings row may say** about a host whose delivery ignores the
  setting — decided 2026-11-05 as issue #215: the dialog cannot know the running
  host's mode, so the row names the support condition instead of guessing. The
  wording and reasoning are on
  [[2026-11-05-the-settings-row-names-the-support-condition]].
- **Whether routing English keys through the engine is free on a live host** —
  caret, undo units and selection in a real control. Filed as issue #216; the mock
  has no host insertion policy to measure against.
- **Whether the syllable-commit setting must also gate the option.** That is the
  other dependency, ruled separately on 2026-11-01
  ([[2026-11-01-syllable-commit-turns-autoswitch-off]]) and still owned by that
  issue (#207); this ruling is about the *delivery* axis, not the *commit unit*
  axis, and the two must not be confused for one another.

The measurement that motivated both, and the two live defects reported from the
same test session, are in `docs/FRONTEND-EQUIVALENCE.md` §2 and
`docs/WINDOWS-TSF-BUFFERING.md`. Rulings like these stay the maintainer's
([[decisions-belong-to-the-maintainer]]) — this page records what was decided and
what was left open, and its own list is the record of the open half.