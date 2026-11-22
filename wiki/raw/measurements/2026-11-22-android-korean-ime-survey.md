---
title: "Android Korean IME survey: 두벌식 is the common denominator"
created: 2026-11-22
updated: 2026-11-22
type: measurement
status: active
tags: [android, layouts, measurement, licence, question]
sources:
  - ../../../docs/ANDROID-KOREAN-IME-SURVEY.md
  - ../../../docs/DESIGN-ANDROID-SYMBOLS.md
  - https://github.com/scobyseo/hunmin/issues/298
confidence: high
ingested: 2026-11-22
---

# Android Korean IME survey: 두벌식 is the common denominator

The maintainer clarified that the 0.4 Korean keypad goal meant adopting 두벌식
among the various Korean input methods (천지인 etc.), and asked for a survey of
how open-source Android keyboards support Hangul. The survey table, the
per-app licences, the layout sets and the maintainer's verbatim quote live in
`docs/ANDROID-KOREAN-IME-SURVEY.md`; this page records how it was made and the
licence constraint.

## How it was made

GitHub repository and code search (`gh search repos` / `gh search code`) plus
reading upstream sources for semantics: HeliBoard's `HangulCombiner.kt` /
`HangulEventDecoder.kt`, FlorisBoard's `HangulUnicode.kt` and layout assets,
fcitx5-android's `plugin/hangul` build, Unexpected Keyboard's layout list, and
open-keyboard's README. The engine trees already on this machine
(`libhangul-0.2.0`, `kime`) were mined for their layout sets rather than
importing anything new.

## The licence constraint

kime, HeliBoard, Unexpected Keyboard and Trime are GPL-3; FlorisBoard is
Apache-2.0; fcitx5-android is LGPL-2.1 (repository label); open-keyboard is MIT.
The survey reads for facts and copies no code, per the standing hard rule. The
per-app licence is recorded in the survey doc because it bounds reuse.

## What it bears on

The survey is evidence for two open Track 3 questions, not a verdict on either:
the Korean layout form (두벌식 is universally offered; 천지인 is rare in open
source) and the symbol-layer home (the mainstream pattern is a separate symbol
page, which is option B in the symbol-layer analysis). The choice stays the
maintainer's, as recorded in [[decisions-belong-to-the-maintainer]]. The
symbol-layer measurement this complements is
[[2026-11-22-symbol-layer-options]].
