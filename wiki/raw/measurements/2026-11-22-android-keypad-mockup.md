---
title: "Provisional Android keypad mockup, and the decisions it surfaces"
created: 2026-11-22
updated: 2026-11-22
type: measurement
status: active
tags: [android, layouts, measurement, question]
sources:
  - ../../../docs/ANDROID-KEYPAD-MOCKUP.md
  - ../../../docs/android-keypad-mockup.html
  - https://github.com/scobyseo/hunmin/issues/298
confidence: high
ingested: 2026-11-22
---

# Provisional Android keypad mockup, and the decisions it surfaces

The maintainer asked whether the criteria had reached the level of drawing the
main screens, and what is left to decide in #298. The answer: yes, provisionally
— `docs/ANDROID-KEYPAD-MOCKUP.md` draws Hangul/English/symbol screens from KP1–KP5
(HTML source + rendered PNG beside it), and the picture makes the open decisions
concrete. The wireframes, the decided/open inventory and the regenerate command
live in that doc, not here.

## What it shows

The criteria fix the Korean form (두벌식), mode separation, number-row
digit↔symbol pairing, functional-key rules and visual hierarchy — enough for a
coherent screen. They do not fix the exact geometry, the precise frequent-symbol
set, or the data home, so the mockup is a candidate to react to, not a spec.

Updated for KP6: the symbol page now has **no shift** and carries navigation keys
(arrows/Tab/Home/End/PgUp/PgDn) instead; a fourth panel shows the Korean Tier 2
special symbols grouped (문장부호/낫표/도형/화살표·수학/통화). The `~`/`` ` ``
question was a text-number-row mockup gap (digits 1–0 only); both are in
us-qwerty and on the symbol page. The Korean Tier 2 ranking is measured from
kowiki prose (· 《》 〈〉 「」 『』 lead); `※ ₩ ♡ ★` are curated, flagged unmeasured
in chat. Details in `docs/ANDROID-SYMBOL-INTERACTION.md`.

**Decided (maintainer ruling, 2026-11-22):** the symbol page shows all 32 symbols
as separate buttons (no shift), in the **grouped order `[]{} `**, applied
consistently to `<>` (`,. <>`); `` ` `` leads row 2 before `-`. Layout:
`!@#$%^&*()` / `` `~-_=+[]{}\| `` / `;:'",.<>/?`. The partner-adjacent alternative
(`[{ ]}`, `,< .>`) was considered and not chosen. Recorded in
`docs/SPEC-ANDROID-KEYPAD.md` KP6; data home / nav-key set / Tier 2 still open.

## The core decision still open in #298

The symbol-layer **data home**: a third per-key row (A, schema+engine cost),
a separate symbol page/table (B), a symbol-page-as-layout (B3, no engine change
but a visibility contract), or number-row popup data. The mockup embodies
B3 + popup data + dedicated frequent keys as one coherent candidate; it is not
the ruled answer. The adjacent Track 3 decisions (frequent set, number-row
default, geometry, shift state, gesture defaults, emphasis, frequency source,
Tier 2 grouping, candidate/mic) are listed in the doc.

No verdict recorded; the choice is the maintainer's
([[decisions-belong-to-the-maintainer]]). It builds on the criteria measurement
([[2026-11-22-android-keypad-criteria]]).
