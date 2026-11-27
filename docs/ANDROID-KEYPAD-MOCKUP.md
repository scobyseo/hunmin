# ANDROID-KEYPAD-MOCKUP — the decided keypad UI (KP13)

Status: **DECIDED — the maintainer ruled this picture is the UI (2026-11-22,
verbatim: "/home/scoby/work/hunmin/docs/android-keypad-mockup.html 로 정했잖아?").
Recorded as `docs/SPEC-ANDROID-KEYPAD.md#§KP13`.** The HTML artefact
(`docs/android-keypad-mockup.html`) is the authoritative picture; the wireframes
below mirror it. The original header called this "a provisional proposal, not a
ruling" — that status predates the ruling and is kept here only as the record
of why the first implementation deviated (it read "provisional" as permission
and built a generic grid; overturned). The OPEN markers below are historical:
where a later ruling settles one (KP10/#312 for Tier 2, KP11 for the data home,
KP12 for auto-show), the later ruling wins; the rest are settled by the picture
itself being decided.

Artefacts: `docs/android-keypad-mockup.html` (source) and
`docs/android-keypad-mockup.png` (rendered, 2400×1720). Regenerate the PNG:

```sh
google-chrome --headless=new --disable-gpu --no-sandbox --hide-scrollbars \
  --force-device-scale-factor=2 --window-size=1200,860 \
  --screenshot=docs/android-keypad-mockup.png \
  "file://$PWD/docs/android-keypad-mockup.html"
```

## Are we at mockup level? Yes — provisionally

The criteria KP1–KP5 fix enough to draw a coherent screen: the Korean form
(두벌식), the mode separation, the number-row digit↔symbol pairing, the functional
-key rules, and the visual hierarchy. What they do **not** fix is the exact
geometry, the precise frequent-symbol set, and the data home — so the mockup is
a *proposal that surfaces those choices*, not a final spec. Drawing it is useful
precisely because a picture makes the open decisions visible.

## The three screens (text wireframe)

Legend: `KEY` = character/symbol (진하게); `(fn)` = functional (연하게);
`<hint>` = long-press / swipe-up corner hint; number row digits are 연하게.

### Hangul mode (두벌식)

```
 1<!> 2<@> 3<#> 4<$> 5<%> 6<^> 7<&> 8<*> 9<(> 0<)>     ← number row (OPEN: default on/off)
 ㅂ<ㅃ> ㅈ<ㅉ> ㄷ<ㄸ> ㄱ<ㄲ> ㅅ<ㅆ> ㅛ ㅕ ㅑ ㅐ<ㅒ> ㅔ<ㅖ>
 ㅁ ㄴ ㅇ ㄹ ㅎ ㅗ ㅓ ㅏ ㅣ
 (⇧) ㅋ ㅌ ㅊ ㅍ ㅠ ㅜ ㅡ (⌫)                          ← one shift; ⌫ separated
 [한/영] [기호] ,<?> [   space   ] .<"> [⏎]              ← KP4 frequent keys; ⏎ separated
```

### English mode (QWERTY)

```
 1<!> 2<@> 3<#> 4<$> 5<%> 6<^> 7<&> 8<*> 9<(> 0<)>
 q w e r t y u i o p
 a s d f g h j k l
 (⇧) z x c v b n m (⌫)
 [한/영] [기호] ,<?> [   space   ] .<"> [⏎]
```

### Symbol mode (Tier 1 — all 32 symbols as separate buttons, no shift)

```
 ! @ # $ % ^ & * ( )                          (10)
 ` ~ - _ = + [ ] { } \ |                      (12, ` before -; brackets GROUPED []{})
 ; : ' " , . < > / ?                          (10, comma/period grouped ,. <> too)
 (Tab) (Home) (PgUp) (PgDn) (End) (⌫)         ← nav + backspace; NO shift (KP6)
 [한/영] [Tier 2] (←) (↑) (↓) (→)             ← KP9: no space/enter; return-mode + Tier 2
                                                in the mode-key slots, arrows in the space slot
```

**Decided (maintainer, 2026-11-22):** the grouped order `[]{} `, applied
consistently to `<>` (`,. <>`). Every shifted variant is its own button (no shift,
no corner hints). `~` / `` ` `` and `- _ = +` live here, not on the text number
row.

### Symbol mode Tier 2 (Korean special symbols, grouped)

```
 · … – — ‘ ’ “ ” ₩ €                      (10, 문장부호 + ₩€)
 「」 『』 《》 〈〉 £ ¥                      (10, 낫표/화살괄호 + £¥)
 ○ ● □ ■ △ ▲ ※ ★ © ®                      (10, 도형/표시 + ©®)
 → ← ↑ ↓ ↔ ° ± × ™ ♡                      (10, 화살표/수학 + ™♡)
 [한/영] [기호 1]                            ← 편집키 없음 (Tier 2는 기호 선택 전용)
```

Tier 2 reaches 10 keys/row by **keeping the original 8-key category groups and
moving 2 symbols from the bottom row up into each** (KP9), not by reflowing the
categories.

**Tier 2 remaining space — open question.** With the editing keys removed, the
bottom ~2 rows are free. Comparable ways to fill them (none ruled):

- **A fifth symbol row** — more special symbols. The corpus has 194 non-ASCII
  symbols, but past the first 40 the prose tail is thin (fullwidth variants,
  box drawings, rare marks); the high-value ones are mostly already placed. A
  chat corpus would rank more usefully (circled numbers `①②③`, `☆ † ÷ ∞ ―`).
- **A frequent/recent symbols row** — dynamic, KP3-aligned, personalised fast
  access; needs usage state and the layout shifts (muscle-memory cost).
- **Larger keys / taller rows** — spend the space on touch-target size (KP2
  error protection) rather than content; best if 40 symbols suffice.
- **Category tabs / page switcher** — if Tier 2 paginates by group; a navigation
  aid, but adds complexity (KP8 caution).
- **An emoji access key** — bridge to emoji, a frequent class of special character.

The deciding question is whether 40 symbols are enough for the target population:
if yes, larger targets (ergonomics) or a frequency row serve better than rare
symbols; if a chat corpus surfaces more high-value symbols, a fifth row earns its
place. The prose tail measured here is thin, so filling with rare symbols has
diminishing returns.

**Update (KP10, corrected 2026-11-23):** swipe-up on the **기호 mode key** opens
a page picker — emoji / extended symbols / math symbols — and picking one opens
that symbol page (maintainer's clarifications, verbatim in
`docs/SPEC-ANDROID-KEYPAD.md#§KP10`). The Tier 2 panel above is **content
reached this way**, not a deprioritised alternative; the gesture replaces the
Tier 2 *button*. The 2026-11-22 version of this paragraph ("symbols become
gesture variants of the Tier 1 keys") was agent drift, corrected; the
remaining-space question above stays open for the extended-symbols page.
#298 records the direction only; the gesture wiring, picker and pages are
**#312**'s work (`docs/DESIGN-ANDROID-SYMBOL-GESTURES.md`).

The number row's corner hints are the KP1/KP5 pairing (digit 진하게, symbol hint
연하게, reached by long-press or swipe-up). The symbol page mirrors `us-qwerty`'s
physical positions, which the data already holds (all 32 ASCII punctuation,
including `` ` `` and `~`). The symbol page has **no shift** (KP6) — symbols are
shown directly — and carries navigation keys instead. Tier 2 groups the
Korean-frequent special symbols; the measured ranking and the curated additions
are in `docs/ANDROID-SYMBOL-INTERACTION.md`.

## What is decided (criteria) vs what is left (mechanism)

**Recorded criteria** (`docs/SPEC-ANDROID-KEYPAD.md`): KP1 104-key symbols at
minimal touch + physical positions, number-row pairing; KP2 functional keys
separated/compact/non-physical, one shift; KP3 frequency placement + visual
emphasis + whole-keypad hierarchy; KP4 mode separation + frequent symbols
default-visible; KP5 long-press **and** swipe-up, accidental swipes preventable;
KP6 symbol page no shift + navigation keys (arrows/Tab/Home/PgUp/PgDn/End) +
Korean special symbols (Tier 2) inputtable; KP7 swipe-up as a universal
substitute for long-press/shift (number→symbol, `,`→`?`, shift→capslock ⇪,
letter/jamo→shifted form).

**The core decision — DECIDED (maintainer, 2026-11-22): B2.** "앱이 데이터 파일로
그리기. 엔진은 관여 안함." The symbol screen is drawn by the Android app from a
data file; the engine is not involved (KP11). No engine/composition-schema change.
The options below are the considered record, not open choices:

| option | what it means | schema/engine cost |
|---|---|---|
| A — third per-key `symbol` row | symbol value in each layout key | schema bump + loader + key API + key-log/replay + doctor (measured: not engine-free) |
| B — separate symbol page/table | a symbol inventory outside the per-key rows | new data file + doctor; engine unchanged if direct commit |
| B3 — symbol page as a layout | reuse `us-qwerty`-style literal layout | loads with no engine change; needs a visibility/mode contract |
| popup data for the number row | per-key long-press/swipe targets | a data home for popup hints (A-like or a side table) |

The mockup was drawn as a **B3 candidate** (symbol page as a layout) to make the
choices concrete; the **ruled answer is B2** (app-read data file, engine
uninvolved), which avoids B3's settings-list visibility contract. The symbol
screen build is the follow-up implementation; the gesture-variant mapping is #312.

**Adjacent Track 3 decisions the picture surfaces** (siblings of #298, not all
in its scope):

1. Exact frequent-symbol set on the main mode (`, . ? "` named; frequency reading
   also suggests `( ) - :`).
2. Number row default on/off.
3. Geometry: key widths, spacing, landscape, the functional-key positions KP2
   constrains but does not place.
4. Shift behaviour: one-shot vs lock (KP2 fixes one shift *key*, not its state
   semantics).
5. Gesture threshold defaults (swipe distance, long-press timeout) — KP5 fixes
   the defences, not the numbers.
6. Emphasis targets and theme tokens (KP3 direction; accessibility: not colour
   alone).
7. Frequency data source: prose measured (`tools/symbol-frequency.py`); chat and
   password populations unmeasured.
8. Symbol-page Tier 2 grouping and the return-to-text behaviour; the navigation
   -key set (KP6); the Korean special-symbol inventory (measured prose top ranks
   plus curated additions such as `※ ₩ ♡ ★`, unmeasured in chat).
9. Candidate window, mic button, accessibility/haptics — other Track 3 rulings.
10. Whether the text number row carries `` ` ~ - _ = + `` too (13-key row, narrower
    targets) or keeps digits `1–0` only.

## What would move this from provisional to ruled

The **data home is decided** (KP11 / #298, B2: the app draws the symbol screen
from a data file; the engine is uninvolved). The **frequent-symbol set** and
**number-row default** would fix the main screens; the geometry and gesture
defaults can then be tuned against the narrow-screen target-size analysis
(`docs/ANDROID-SYMBOL-INTERACTION.md`). Those remaining choices are open; the
mockup is the concrete proposal they act on.
