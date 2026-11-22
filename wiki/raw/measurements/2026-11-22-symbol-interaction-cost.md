---
title: "Symbol/number interaction cost mid-Hangul, and the ISO 25010 premise"
created: 2026-11-22
updated: 2026-11-22
type: measurement
status: active
tags: [android, layouts, measurement, question]
sources:
  - ../../../docs/ANDROID-SYMBOL-INTERACTION.md
  - ../../../docs/ANDROID-KOREAN-IME-SURVEY.md
  - https://github.com/scobyseo/hunmin/issues/298
confidence: high
ingested: 2026-11-22
---

# Symbol/number interaction cost mid-Hangul, and the ISO 25010 premise

The maintainer reported the recurring pain of pressing the symbol key to type
numbers/symbols during Hangul input, and asked how other IMEs reduce it, citing
ISO 25010 ("interaction 횟수가 적은 UI가 좋은 UI"). The interaction-count table,
the per-app mechanisms and the verbatim quote live in
`docs/ANDROID-SYMBOL-INTERACTION.md`; this page records the method and the
premise check.

## How it was made

Reading the upstream layout assets and settings for semantics: HeliBoard
`korean.json` / `KeyboardId.kt` / `layouts.md`, FlorisBoard `korean.json` /
`AppPrefs.kt` / `LongPress.kt`, fcitx5-android `NumberRow.kt` / `PickerPolicy.kt`.
The finding that matters: the 두벌식 keypad body in HeliBoard and FlorisBoard is
jamo only — shift and long-press give doubled jamo, not symbols — so digits come
from an optional number row and symbols from a separate page. The counts in the
doc are derived from those mechanisms, not device-timed.

## The premise check

ISO 25010's relevant characteristic is usability (operability); "fewer
interactions" is a proxy for efficiency (ISO 9241-11), not a literal ISO 25010
clause, and ISO 25010 also carries performance efficiency (time) separately. So
the doc reports interaction count alongside the trade-off each mechanism pays
(hold latency, screen height, burst cost). The goal is sound; "count alone
decides it" is the part that needed the other axes. This is the standing rule to
check the maintainer's premises as carefully as one's own, applied without
arguing the feature unnecessary.

## What it bears on

Evidence for the Track 3 keypad decisions, not a verdict: the number row is the
mainstream answer to the digit half, the sticky symbol page is already
count-optimal for bursts, and long-press/one-shot/dedicated keys reduce the
single-symbol cost. The choice stays the maintainer's
([[decisions-belong-to-the-maintainer]]). It complements the layout/symbol-home
survey ([[2026-11-22-android-korean-ime-survey]]) and the symbol-layer measurement
([[2026-11-22-symbol-layer-options]]).

## Refinement: search time and mis-taps

The maintainer sharpened the goal: the core cost is *finding* where a symbol is
(visual search), and a narrow screen with crowded keys causes mis-taps. The doc
now carries two more axes beyond interaction count — discoverability and target
size — with the prior art for each (HeliBoard's dedicated comma/period keys and
grouped symbol page, FlorisBoard's adjustable key spacing, long-press variant
clusters) and a geometry-derived target-width table against the 48 dp touch
guideline. The reading is a trade-off triangle: visible symbols crowd the narrow
screen and shrink targets; hidden symbols keep targets large but cost search and
switches. The mainstream bundle addresses all three. The refined quote and the
tables live in the doc, not here.
