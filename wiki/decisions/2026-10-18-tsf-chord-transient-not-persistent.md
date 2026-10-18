---
title: "A CTRL/ALT chord in hangul mode is transient processing — the mode does not switch"
created: 2026-10-18
updated: 2026-10-18
type: decision
status: active
tags: [decision, windows, tsf]
sources:
  - ../../../docs/WINDOWS-TSF-BUFFERING.md
  - ../raw/sessions/2026-10-18-milestone-d-candidate-edit-mode-ux.md
confidence: high
---

# A CTRL/ALT chord in hangul mode is transient processing — the mode does not switch

Closes the ruling request on issue #101 ("does a CTRL/ALT chord in hangul mode
switch the mode persistently?"). The question was raised against the shipped
transient chord rule from milestone D
([[2026-10-18-milestone-d-candidate-edit-mode-ux]]): the rule commits the pending
preedit through the boundary flush, closes the candidate window, and passes the
chord to the app uneaten, while the mode word stays on the hangul side.

The ruling was given as a comment on the issue itself — the same shape as the
issue #9 ruling, where the issue is the surviving source because the words were
typed in the working session, not in a separate record.

**Maintainer, 2026-10-18** (verbatim, issue #101 comment):

> will not switch mode; transient processing is enough.

## What was ruled

The transient reading IS the rule: a CTRL/ALT chord does not compose and does
not leave english mode behind. Repeated Ctrl+V then a letter keeps typing
hangul — the mode never moved. The stronger reading in the user note (the chord
ALSO switches the mode to english persistently) is closed; re-raising it would
need new evidence, per [[decisions-belong-to-the-maintainer]] — a settled
question stays settled unless a measurement moves it.

No code change was needed: the tree already ran the transient rule. Verified
against the shipped sources the day the ruling landed:

- `frontends/tsf/src/tsf.cpp:1214-1224` — the chord block: `CloseCandidateWindow()`,
  `BoundaryFlush()`, `*pfEat = FALSE`, and no write to `hangulMode_`;
- `frontends/tsf/src/tsf.cpp:959-965` — `OnTestKeyDown` answers the same
  uneaten, so the key arrives at the app in its chord sense (Ctrl+A selects
  all rather than composing a jamo of its own);
- `frontends/tsf/src/tsf.cpp:1644-1654` — `BoundaryFlush`: the in-flight
  syllable is flushed into the buffered word and the whole word inserts once —
  that is the "may need buffer flush" half of the user note;
- the absence grep `frontends/tsf/src/tsf.cpp:hangulMode_ = false` (board row
  TSF-MODEPERSIST-67 in `docs/REMAINING-WORK.md`), which guarded the
  alternative as not-shipped, now guards the ruled-in behaviour as shipped.

The Android side already shared the semantics: the adapter commits at a
modifier chord and does not switch mode (board row WB-75-ADAPTER), so the two
frontends never diverged on the question the ruling answered.

## What the ruling does not close

- **The behavioural pin.** Nobody replays a chord at tier F1: `trace-emit.cpp`
  injects only the Shift bit, and `docs/trace-spec.md` defines the win32 `mods`
  bitfield as "`0x01` = Shift" and nothing else. A ctrl/alt chord case needs
  those bits spec'd and a corpus case written; until then the chord rule's
  guard is the two source-grep rows (TSF-CHORD-67, TSF-MODEPERSIST-67) plus
  the milestone-D device notes, not a replay assertion.
- **Chord-chain order** (read off the code, not measured): the toggle
  matcher, the Hanja key and the candidate-window block all precede the chord
  block, and the candidate block does not look at modifiers — with the window
  open, Ctrl+Return submits the cursor row and an in-range digit picks, where a
  strict "chord is application input" sentence would hand the key to the app.
  The ruling did not name this interaction; recording it here keeps it from
  reading as decided.
- **The chord's own keydown.** The chord block fires on any key while CTRL or
  ALT is down, so the flush can land on the Ctrl keydown itself, before the
  letter arrives — committed text is identical either way, but the log line
  (`ctrl/alt chord -> commit + passthrough`) rides on the earlier event.
  Device note, not a defect claim.
