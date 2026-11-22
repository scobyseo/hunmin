---
title: "Android keypad criteria KP1–KP3, and the 104-key symbols already in the data"
created: 2026-11-22
updated: 2026-11-22
type: measurement
status: active
tags: [android, layouts, measurement, question]
sources:
  - ../../../docs/SPEC-ANDROID-KEYPAD.md
  - ../../../docs/ANDROID-SYMBOL-INTERACTION.md
  - https://github.com/scobyseo/hunmin/issues/298
confidence: high
ingested: 2026-11-22
---

# Android keypad criteria KP1–KP3, and the 104-key symbols already in the data

The maintainer set keypad criteria across several messages on 2026-11-22. They
are requirements now, with their verbatim text in
`docs/SPEC-ANDROID-KEYPAD.md` (KP1 symbol keys, KP2 functional keys, KP3
frequency/emphasis directions including the whole-keypad visual hierarchy, KP4
mode separation with very frequent symbols default-visible in Hangul/English
mode, KP5 number-row symbols by long-press and swipe-up with accidental swipes
preventable). This page records the measurement that grounds them and the fork
they expose; the quotes and tables live in the SPEC and the analysis doc, not
here.

## The measurement

`us-qwerty.json` already holds the entire standard 104-key symbol set — all 32
ASCII punctuation characters, each at its physical key id (the symbol is the
shift row of its physical key), and the number row pairs every digit with its
shifted symbol (1→! … 0→)). So KP1's Tier 1 needs no new symbol data: a keypad
surface mirroring `us-qwerty`'s positions inherits it. Tier 2 (characters a
104-key cannot type) is the part allowed to cost more touches.

## The fork it exposes

The surveyed IMEs group the symbol page by *category*; KP1 asks for *physical
positions* for the 104-key set. A QWERTY-positioned page satisfies KP1; a
categorised grid satisfies search time but not the physical-position clause.
This is recorded as a genuine fork, not resolved — the criterion prioritises
touch-typist muscle memory over categorisation. KP3's visual emphasis decouples
discoverability from geometry (it highlights without moving keys), and its
whole-keypad hierarchy (characters 진하게, numbers/edit 연하게) is a theme rule
that needs no frequency source. Frequency *placement* still needs a
symbol-frequency table, unmeasured here.

## Frequency method, and the number-row pairing

`tools/symbol-frequency.py` was added to measure KP3's frequency source. A
finding from running it: the local "corpora" (`624-ko.txt`, `nikl-2003-freq`,
`119-clean`) are word-frequency lists with no symbols — symbol frequency needs
raw text. The first prose reading (kowiki dump, 3000 pages stripped) ranks
`. , ( ) - : " '` first, matching KP4's default-visible set; password symbols
(`! @ # $ %`) are low in prose, so KP1's number-row pairing rests on the use
case, not prose frequency. The ranking, caveats (register, markup inflation of
`~ | =`) and reproduce command are in the analysis doc.

For KP1's digit↔symbol link the maintainer proposed long-press; the analysis doc
compares long-press / shift / swipe / double-tap on the password case. The
number row is the one place with no competing long-press meaning (jamo keys
spend it on doubled jamo), which is why long-press fits there; the cost is hold
latency, and a measured password digit:symbol ratio is what would change the
choice. Recorded as analysis, not a verdict.

## Swipe-up defences and symbol display (KP5)

The maintainer asked to add swipe-up alongside long-press, and whether an
unintended swipe-up can be avoided. Unexpected Keyboard (read for semantics)
shows layered defences: a user-adjustable swipe distance threshold
(`swipe_dist_px`, Manhattan distance gates swipe start), return-to-center cancel
(Roundtrip), commit-on-release with preview during, and direction quantisation.
Composed with a velocity gate and movement-vs-time separation from long-press,
and the fact that the number row has no glide-typing conflict, the accidental
swipe is avoidable. Display: digit 진하게 + faint corner symbol hint toward the
swipe direction, popup preview on gesture, release commits — the hint also makes
the gesture discoverable. Details and the per-key display rules are in the
analysis doc. Recorded as analysis, not a verdict.

## Status

Mechanism open (#298); nothing implemented, no verdict recorded. The criteria
are the maintainer's, as recorded in [[decisions-belong-to-the-maintainer]].
This complements the interaction-cost analysis
([[2026-11-22-symbol-interaction-cost]]) and the prior-art survey
([[2026-11-22-android-korean-ime-survey]]).
