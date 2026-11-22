# ANDROID-SYMBOL-INTERACTION — the cost of typing numbers/symbols mid-Hangul

Status: **usability analysis for issue #298 / Track 3, not a ruling.** It
measures, in interaction count, how open-source Android IMEs let a person insert
a digit or symbol while typing Hangul and return to Hangul. Requirement
pointers: `docs/SPEC-OVERVIEW.md#§A1`, `docs/SPEC-OVERVIEW.md#§A5`,
`docs/MILESTONES.md` Track 3. Companion documents:
`docs/ANDROID-KOREAN-IME-SURVEY.md` (which apps compose Hangul and where symbols
live) and `docs/DESIGN-ANDROID-SYMBOLS.md` (the symbol-layer home).

## The problem, in the maintainer's words

Quoted verbatim (2026-11-22):

> "내가 다른 한글 입력기를 사용할 때 항상 불편해 하던 건, 한글 입력 중에 숫자나
> 기호 입력이 많은데, 그걸 위해 기호 키를 눌러야 하는 경우가 많다는 거야. 이
> 문제를 다른 입력기들은 어떻게 접근하고 있는지 알 수 있을까? ISO 25010을
> 고려하면, interaction 횟수가 적은 UI가 좋은 UI 거든?"

The pain is the **mode-switch tax**: every digit/symbol burst during Hangul
input costs a trip to the symbol page and back. The question is how other IMEs
reduce that tax, and whether interaction count is the right measure.

### The refinement: finding the symbol, and not mis-tapping it

Quoted verbatim (2026-11-22, follow-up):

> "뭐든 기호 입력이 생각보다 많으니 기호가 어디 있는지 찾느라 시간을 쏟는 걸
> 줄이고 싶다는 게 핵심이야. 안드로이드 장치는 화면이 좁고, 너무 키들이 붙어
> 있으면 잘못된 조작이 많이 발생할 수 있으니, 그걸 고려하더라도 기호 입력이
> 쉬웠으면 좋겠다."

This sharpens the goal beyond interaction count. The core cost is **visual
search time** — locating *where* a symbol is — and the constraint is a **narrow
screen where crowded keys cause mis-taps**. So the measure is three axes, not
one: interactions to reach the symbol, time to find it, and the error rate of
the tap. A design that minimises only the first can worsen the other two.

## The ISO 25010 premise, checked

ISO/IEC 25010 is a product-quality model. The relevant characteristic is
**usability**, whose sub-characteristics include **operability** (ease of
operation and control), learnability, user error protection, user interface
aesthetics and accessibility. "Fewer interactions is a better UI" is a reasonable
**proxy for efficiency**, but it is not a literal ISO 25010 clause: efficiency as
"resources expended relative to accuracy and completeness" is defined in
**ISO 9241-11**, and ISO 25010 also carries **performance efficiency** (time
behaviour, resource utilisation) as a separate characteristic.

So interaction count is a useful, measurable axis — but not the only one. A
long-press is one interaction yet adds hold latency; a number row removes
interactions yet spends screen height; a one-shot symbol page removes the return
tap for a single symbol yet doubles the cost of a burst. The comparison below
therefore reports interaction count **and** the trade-off each mechanism pays.
This checks the premise rather than refusing it: the goal (less switching) is
sound; "count alone decides it" is the part that needs the other axes. The
refinement maps onto ISO 25010 usability directly: **appropriateness
recognizability** and **learnability** (can the person find/recognise the
symbol), **user error protection** (mis-taps on a crowded narrow screen), and
**operability**; the search and movement time itself is **performance
efficiency** (time behaviour).

## What the surveyed IMEs actually do

Measured from the upstream sources (read for semantics, no code copied):

- **The Hangul keypad body carries no digits or symbols.** HeliBoard
  `korean.json` and FlorisBoard `korean.json` are three rows of jamo only. On
  both, the shift row / long-press popup of a 두벌식 key is the **doubled jamo**
  (ㅂ→ㅃ, ㄱ→ㄲ, ㅏ→ㅒ…), not a symbol or digit.
- **Digits: an optional number row.** HeliBoard `mShowsNumberRow` (and
  `mShowsNumberRowInSymbols`); FlorisBoard `keyboard__number_row` (default off);
  fcitx5-android a collapsible `NumberRow` bar. With it on, a digit is one tap
  with no page switch.
- **Symbols: a separate symbol page/panel.** HeliBoard `symbols` / `more_symbols`
  layouts with an `_alpha_` / `_symbol_alpha_` return key; FlorisBoard `symbols` /
  `symbols2` layouts; fcitx5-android a symbol `Picker`. The page is a distinct
  keyboard view; the common model is sticky (stay on symbols until the return
  key), with auto-return after one symbol a possible variant.
- **Long-press popups for punctuation/currency exist, but not on the 두벌식 jamo
  keys.** HeliBoard's layout grammar has comma/period popup groups (`groupId`
  1/2) and a currency key (`$$$`) with long-press currencies; FlorisBoard has a
  general `LongPress` extended-key popup. These sit on dedicated keys, not on
  the Hangul letters.
- **Context-aware layouts.** HeliBoard `KeyboardMode` (TEXT/URL/EMAIL/NUMBER/
  DATE/TIME/DATETIME) and `variation_selector`; Android `inputType` drives a
  numpad/phone/date layout automatically. A pure-number field needs no manual
  switch at all.
- **세벌식 puts symbols on shift.** HeliBoard `korean_sebeolsik_390/final` shift
  rows carry `@#$%…`, `※`, `“”`. This is a per-layout convention, unavailable to
  두벌식 whose shift is doubled jamo.

## Interaction-count comparison

Metric: touches/gestures to insert **one** digit or symbol mid-Hangul and be
back in Hangul, and the cost of a burst of **N** symbols. Counts are derived
from the documented mechanism, not device-timed; hold latency and error rate
are noted separately because count alone is incomplete (§ISO above).

| mechanism | one symbol mid-Hangul | burst of N | trade-off paid |
|---|---|---|---|
| symbol page, sticky (mainstream) | 3 (enter + symbol + return) | 2 + N | mode-switch tax per burst; cheap for long bursts |
| symbol page, one-shot / auto-return | 2 (enter + symbol) | 2N | no return tap, but worse for bursts |
| number row always on (digits only) | 1 | N | screen height; symbols still need a page |
| long-press popup on a key | 1–2 (hold + pick, or hold-slide) | N × (hold + pick) | hold latency, discoverability; needs popup data |
| symbols on shift (세벌식 style) | 2 (shift + key) | shift-lock + N | conflicts with 두벌식 doubled-jamo shift |
| dedicated punctuation keys (. , ? !) | 1 | N | layout space; only the chosen characters |
| context auto-switch (number/date field) | 0 manual | 0 manual | only pure-number/date fields, not mixed text |
| swipe / flick on a key | 1 gesture | N gestures | learnability, gesture conflict with glide |

Reading the table:

- For **digits**, the number row is the clear interaction-count winner (1 tap,
  no return) and is what HeliBoard/FlorisBoard/fcitx5-android offer as an option.
  It does nothing for symbols.
- For **a single symbol** mid-Hangul, long-press, one-shot page, shift-symbols
  and dedicated keys all beat the sticky page (1–2 vs 3).
- For **a burst of symbols**, the sticky page is cheapest (2 + N); one-shot and
  long-press scale worse (2N, N×hold). The maintainer's pain ("기호 입력이 많은데")
  is therefore partly a burst problem, where the sticky page is already optimal
  on count — the tax is the enter/return pair, amortised over the burst.
- **Context auto-switch** removes the tax entirely but only where the field is
  purely numeric; mixed Hangul+digit text gets no help from it.

## Finding the symbol, and not mis-tapping it (the refined axes)

### What the surveyed IMEs do for search time and target size

Measured from the upstream layout assets and settings:

- **Common punctuation as dedicated, always-visible keys.** HeliBoard's
  `functional_keys.json` puts **comma and period on the main bottom row**, full
  height, no page switch. The comma key is context-aware (`variation_selector`:
  default `,`, email field `@`, URI field `/`) and carries a long-press popup
  group of further punctuation. This is the direct answer to "finding the
  symbol": the most-used ones are never hidden.
- **The symbol page is grouped, not a flat grid.** HeliBoard `symbols.txt`
  clusters brackets/math (`[ ] < > { } = \ |`), then currency/operators
  (`@ # $$$ _ & - + ( ) /`), then punctuation (`* " ' : ; ! ?`). FlorisBoard
  `symbols/western.json` is two rows (10 then 7 keys) with a currency slot, and
  it ships a `western_samsung.json` variant. Grouping cuts the effective choice
  set a person scans (Hick's law: decision time grows with the number of
  alternatives), so a categorised page is faster to search than one flat grid of
  the same symbols.
- **Long-press clusters related variants.** HeliBoard symbol keys carry variants
  on long-press (`-` → `– — ·`, `(` → `⁽ < { [`, `*` → `† ‡ ★`), so a family of
  related symbols lives under one visible key instead of occupying many.
- **User-adjustable spacing and height.** FlorisBoard exposes
  `keyboard__key_spacing_horizontal` / `_vertical` and keyboard height;
  HeliBoard `mKeyboardHeightScale` and horizontal/vertical gap; open-keyboard a
  key-height setting. This is the explicit lever against mis-taps on a narrow
  screen: the person widens gaps or raises keys rather than the design guessing.
- **Fewer keys per row on the symbol page than on the alphabet.** FlorisBoard's
  symbol rows are 10 and 7; the 두벌식 alphabet rows are 10/9/7. A symbol page
  does not cram more keys than the alphabet, so its targets are no smaller.

### Target size on a narrow screen (geometry-derived)

Android's accessibility guidance recommends a **48 dp minimum touch target**;
Fitts's law says movement time and error rate rise as target width shrinks
(`MT = a + b·log2(1 + D/W)`). On a common 360 dp-wide phone, ignoring gaps, the
per-key width is just screen width divided by keys-per-row:

| keys per row | width/key on 360 dp | vs 48 dp guideline |
|---:|---:|---|
| 7 (두벌식 bottom row, symbol punctuation row) | ~51 dp | meets |
| 9 (두벌식 middle row) | ~40 dp | below |
| 10 (두벌식 top row, number row, symbol top row) | ~36 dp | below |
| 11 (HeliBoard symbol bracket row) | ~33 dp | below |

These are geometry-derived, not device-measured error rates; gaps and key height
move them, which is exactly why the spacing/height settings exist. The reading:
a 10-across row on a narrow phone is already below the touch-target guideline,
so **adding more visible symbol keys to the main layout shrinks targets further
and raises mis-taps** — the constraint the maintainer named. A separate symbol
page keeps the alphabet keys large; putting everything on one screen does not.

### The trade-off triangle

The three axes pull against each other on a narrow screen:

- **Discoverability** (symbol visible, no switch) wants many keys on the main
  layout → smaller targets → more mis-taps.
- **Target size / low error** wants few, large, well-spaced keys → symbols hidden
  on a page or under long-press → more search and interactions.
- **Interaction count** wants no mode switch → visible symbols or long-press →
  crowding or hold latency.

No mechanism maximises all three. The mainstream resolution is a **bundle**:
a few high-frequency symbols always visible as large keys (comma, period,
context `@`/`/`), digits via an optional number row, the rest on a **grouped**
symbol page whose targets are no smaller than the alphabet, long-press variants
clustering related symbols, and user-adjustable spacing/height. Frequency
ordering (common symbols first) is a further search-time lever the data could
carry; it is unmeasured here.

## What this means for a hunmin 두벌식 keypad

The maintainer's goal — less time *finding* symbols and fewer mis-taps on a
narrow screen, not just fewer switches — is supported by the evidence; the
mechanism is the open choice. Comparable levers, none ruled:

1. **Optional number row** — digits at 1 tap, the mainstream answer to the digit
   half of the pain. A geometry/presentation decision (Track 3), no engine change.
2. **Long-press symbol popups on jamo keys** — would cut single-symbol cost to
   1–2, but HeliBoard/FlorisBoard use that popup for doubled jamo on 두벌식, so
   hunmin would be choosing a different popup content. Needs per-key popup data
   (a data home, like the symbol-layer question) and pays hold latency.
3. **One-shot symbol page** — cuts the return tap for single symbols; worse for
   bursts. A frontend interaction rule.
4. **Dedicated punctuation keys** — 1 tap for a chosen few (. , ? !); spends
   layout space.
5. **Context auto-switch** — already partly available through Android
   `inputType`; hunmin's secure-editor handling is the existing precedent for
   field-aware behaviour.
6. **Symbols on shift** — available to 세벌식 data already, but 두벌식 shift is
   doubled jamo; adopting it for 두벌식 would change the layout's shift semantics
   (a layout-data decision, not an engine one).
7. **Grouped symbol page** — categorise symbols (brackets/math, currency,
   punctuation) rather than a flat grid, cutting visual-search time; the
   mainstream answer to "finding the symbol". A data-organisation decision.
8. **User-adjustable key spacing / height** — the explicit anti-mis-tap lever
   FlorisBoard/HeliBoard expose; lets the person fit the narrow screen rather
   than the design guessing one geometry. A presentation/settings decision.
9. **Frequency ordering** — common symbols first, so the frequent ones are found
   fastest; needs usage data and is unmeasured here. A data decision.

These compose: dedicated common punctuation + a number row + a **grouped**
symbol page + context auto-switch + adjustable spacing is the mainstream bundle
that addresses all three axes; long-press popups, one-shot and frequency
ordering are optional reductions on top. The interaction-count table and the
target-size table are the evidence for choosing among them, with the trade-off
triangle as the counterweight to "count alone decides it".

## What this analysis does not settle

- Device-measured latency, visual-search time and error rate per mechanism (the
  counts and target widths here are mechanism- and geometry-derived; hold time,
  search time and mis-taps are unmeasured).
- The symbol-layer data home (`#298`) — long-press popups and a symbol page both
  need a data home, which is the separate open question.
- Geometry, number-row default, shift behaviour, accessibility — Track 3
  decisions.
- The maintainer's choice. No verdict is recorded; the pain and the ISO framing
  are quoted as the requirement, and the mechanisms are presented as comparable.
