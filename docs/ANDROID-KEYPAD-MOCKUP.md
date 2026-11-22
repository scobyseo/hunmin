# ANDROID-KEYPAD-MOCKUP — provisional screens, and what is left to decide

Status: **a provisional proposal for issue #298 / Track 3, not a ruling.** It
draws the main keypad screens from the recorded criteria
(`docs/SPEC-ANDROID-KEYPAD.md`, KP1–KP5) so the remaining decisions become
concrete. The picture is a candidate to react to; every geometry choice in it is
open until the maintainer rules. Requirements are cited by id, not restated.

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
 [한/영] [기호] [   space   ] ,<?> .<"> [⏎]              ← KP4 frequent keys; ⏎ separated
```

### English mode (QWERTY)

```
 1<!> 2<@> 3<#> 4<$> 5<%> 6<^> 7<&> 8<*> 9<(> 0<)>
 q w e r t y u i o p
 a s d f g h j k l
 (⇧) z x c v b n m (⌫)
 [한/영] [기호] [   space   ] ,<?> .<"> [⏎]
```

### Symbol mode (Tier 1 — all 32 symbols as separate buttons, no shift)

```
 ! @ # $ % ^ & * ( )                          (10)
 ` ~ - _ = + [ ] { } \ |                      (12, ` before -; brackets GROUPED []{})
 ; : ' " , . < > / ?  [Tier 2]                (10, comma/period grouped ,. <> too)
 (Tab) (Home) (PgUp) (PgDn) (End) (⌫)         ← NO shift (KP6); nav order Home·PgUp·PgDn·End
 (←) (↑) (↓) (→) [   space   ] [⏎]
 [한/영] [ABC/한글]
```

**Decided (maintainer, 2026-11-22):** the grouped order `[]{} `, applied
consistently to `<>` (`,. <>`). Every shifted variant is its own button (no shift,
no corner hints). `~` / `` ` `` and `- _ = +` live here, not on the text number
row.

### Symbol mode Tier 2 (Korean special symbols, grouped)

```
 · … – — ‘ ’ “ ”                          ← 문장부호 (measured top ranks)
 「」 『』 《》 〈〉                         ← 낫표/화살괄호
 ○ ● □ ■ △ ▲ ※ ★                          ← 도형/표시 (※★ curated)
 → ← ↑ ↓ ↔ ° ± ×                          ← 화살표/수학·단위
 ₩ € £ ¥ © ® ™ ♡                          ← 통화/기타 (curated)
 [한/영] [기호 1] [ space ] [⏎]
```

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

**The core decision still open in #298** — the symbol-layer **data home**, which
the criteria narrow but do not settle:

| option | what it means | schema/engine cost |
|---|---|---|
| A — third per-key `symbol` row | symbol value in each layout key | schema bump + loader + key API + key-log/replay + doctor (measured: not engine-free) |
| B — separate symbol page/table | a symbol inventory outside the per-key rows | new data file + doctor; engine unchanged if direct commit |
| B3 — symbol page as a layout | reuse `us-qwerty`-style literal layout | loads with no engine change; needs a visibility/mode contract |
| popup data for the number row | per-key long-press/swipe targets | a data home for popup hints (A-like or a side table) |

The mockup embodies **B3 + number-row popup data + dedicated frequent keys**; it
is one coherent candidate, not the ruled answer. If A is chosen, the schema and
engine work in `docs/DESIGN-ANDROID-SYMBOLS.md` §3.A is the path; if B/B3, the
data home and the Android visibility contract are the path.

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

A maintainer choice of the **data home** (the #298 core) plus the **frequent-symbol
set** and **number-row default** would fix the main screens; the geometry and
gesture defaults can then be tuned against the narrow-screen target-size analysis
(`docs/ANDROID-SYMBOL-INTERACTION.md`). No verdict is recorded here; the mockup is
the concrete proposal those decisions act on.
