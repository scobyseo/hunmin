# ANDROID-KOREAN-IME-SURVEY — how open-source Android keyboards type Hangul

Status: **prior-art survey for issue #298 / Track 3, not a ruling.** It records
what other open-source Android IMEs do, so the symbol-layer and layout-form
choices are made against measured practice rather than a guess. Requirement
pointers: `docs/SPEC-OVERVIEW.md#§A1`, `docs/SPEC-OVERVIEW.md#§A2`,
`docs/MILESTONES.md` Track 3. The symbol-layer analysis itself is
`docs/DESIGN-ANDROID-SYMBOLS.md`. The interaction-count cost of typing
numbers/symbols mid-Hangul, and how the surveyed IMEs reduce it, is
`docs/ANDROID-SYMBOL-INTERACTION.md`.

**Method.** GitHub repository/code search and reading the upstream sources for
semantics, plus the engine trees already on this machine (`libhangul-0.2.0`,
`kime`). **No upstream code is copied.** kime, HeliBoard, Unexpected Keyboard
and Trime are GPL-3; FlorisBoard is Apache-2.0; fcitx5-android is LGPL-2.1
(repository label); open-keyboard is MIT. Licences are recorded per app because
they bound what may be reused, and the answer here is "read for facts, copy
nothing" (`AGENTS.md` hard rule).

## The maintainer's clarification of the goal

The 0.4 Track 3 goal is 두벌식 + QWERTY + 기호. The maintainer clarified what the
두벌식 part meant (2026-11-22, quoted verbatim):

> "그 입력기들이 제공하는 레이아웃을 보면 천지인 등의 다양한 입력 방식이 있는데,
> 그 중에서 두벌식 형태를 채용하자는 거였어."

So the Korean keypad **form** is 두벌식, chosen among the various Korean input
methods (천지인, 세벌식, 모아키, …). This is the layout-form choice; the
symbol-layer home (`#298`) is a separate question. The survey below bears on
both.

## 1. Which open-source Android IMEs compose Hangul

| app | licence | Hangul composition | Korean layouts offered | symbol mechanism |
|---|---|---|---|---|
| **HeliBoard** | GPL-3.0 | native `HangulCombiner` (Kotlin) | 두벌식 standard, 세벌식 390, 세벌식 최종 | separate `symbols` / `more_symbols` pages |
| **FlorisBoard** | Apache-2.0 | `HangulUnicode` composer | `korean` (South Korean standard = 두벌식), `korean_phonetic` | separate `symbols` / `symbols2` pages + long-press |
| **fcitx5-android** | LGPL-2.1 | `fcitx5-hangul` + `libhangul` (static link) | the libhangul set: 두벌식, 세벌식 variants, 안마태, 로마자, 옛글 | fcitx5 soft-keyboard symbol / picker pages |
| **Unexpected Keyboard** | GPL-3.0 | native composition | `hang_dubeolsik_kr` (두벌식 only) | layers / Fn for symbols |
| **open-keyboard** (json-choi) | MIT | native | 두벌식, 천지인, 모아키 + QWERTY | number / symbol pages + long-press accents/currency + emoji panel |
| **AnySoftKeyboard** | Apache-2.0 | none found (no Korean language pack) | — | — |
| **Trime** | GPL-3.0 | none found (RIME, no Korean schema bundled) | — | — |

Reading the sources:

- **HeliBoard** `HangulEventDecoder.kt` registers exactly three Korean layouts
  (`korean`, `korean_sebeolsik_390`, `korean_sebeolsik_final`) and
  `HangulCombiner.kt` composes with a 두벌식 combination table. There is no
  천지인. On the 두벌식 standard layout the shift row is doubled jamo
  (ㄲ/ㄸ/ㅃ/ㅆ/ㅉ and compound vowels), **not symbols**; on the 세벌식 layouts the
  shift row carries symbols (`@#$%…`, `※`, `“”`). Symbols otherwise come from
  the separate symbol pages.
- **FlorisBoard** `HangulUnicode.kt` is a layout-agnostic composer over Unicode
  jamo (initial/medial/final tables plus compound-vowel and compound-final
  maps). The keyboard layout decides which jamo a key emits; the offered Korean
  character layouts are `korean` ("South Korean standard") and
  `korean_phonetic`. Symbol pages are separate layout assets
  (`symbols`, `symbols2`, `symbolsMod`, `symbols2Mod`, including a `cjk` set).
- **fcitx5-android** `plugin/hangul` statically links `libhangul.a` and builds
  `fcitx5-hangul`, shipping libhangul's data. Its Korean layout set is therefore
  libhangul's, and its soft keyboard has symbol/picker pages
  (`PickerLayout`, `PickerPolicy`, `KeyboardWindow`). This is the architecture
  closest to hunmin: a composition engine behind a thin Android frontend.
- **Unexpected Keyboard** ships one Korean layout, `hang_dubeolsik_kr.xml`;
  symbols are reached through its layer/Fn model rather than a Hangul shift row.
- **open-keyboard** is a small MIT Hangul-first keyboard offering 두벌식, 천지인
  and 모아키 gestures plus QWERTY, with number/symbol pages, long-press
  accents/currency and an emoji panel. It is the only surveyed app that offers
  천지인.

## 2. The engine trees on this machine (not Android, for layout semantics)

- **libhangul 0.2.0** keyboards: 두벌식(`2`), 두벌식옛(`2y`), 세벌식
  `32`/`39`/`3f`/`3s`/`3y`, 안마태(`ahn`), 로마자(`ro`). **No 천지인.**
- **kime** (GPL-3, read for semantics only): 두벌식, 세벌식 3-90/3-91,
  세벌식신 1995/P2. **No 천지인.**

천지인 is absent from both reference engines and from every surveyed Android IME
except open-keyboard; it is principally a proprietary (Samsung) method. 세벌식
is present in HeliBoard, fcitx5-android/libhangul and kime.

## 3. What the survey settles (facts, not rulings)

1. **두벌식 is the common denominator.** Every surveyed Android IME that composes
   Hangul offers 두벌식. 천지인 is rare in open source; 세벌식 is common in the
   libhangul/kime lineage but not universal (FlorisBoard and Unexpected Keyboard
   do not offer it). The maintainer's 두벌식 choice is consistent with prior
   art; it is the most widely supported Korean form.
2. **The mainstream symbol mechanism is a separate symbol page, supplemented by
   long-press popups** — not a third per-key layer in the Hangul layout.
   HeliBoard, FlorisBoard, fcitx5-android and open-keyboard all ship dedicated
   symbol pages. This is option **B** in `docs/DESIGN-ANDROID-SYMBOLS.md`.
3. **The Hangul shift row is not a universal symbol home.** On 두벌식 it is
   doubled jamo (HeliBoard standard, and hunmin's own `dubeolsik.json`); on
   세벌식 it can carry symbols (HeliBoard 390/final, and hunmin's
   `sebeolsik-final.json` already does). So "put symbols on shift" is a
   per-layout convention, not a cross-layout rule — which is why a 두벌식 keypad
   needs a symbol page or a third row, while a 세벌식 keypad may already have
   symbols on shift.
4. **The closest architectural prior art is fcitx5-android**: a composition
   engine (libhangul) behind a thin Android frontend with symbol/picker pages.
   hunmin's engine-plus-adapter shape matches it; the difference is hunmin's
   layout-as-data contract and its own composition engine.

## 4. What the survey does not settle

- The symbol-layer home for hunmin (`#298`) — the survey shows the mainstream
  pattern (separate page) but the choice is the maintainer's, and hunmin's
  layout-as-data rule may favour a data-owned page over a frontend-owned one.
- The symbol inventory, geometry, shift behaviour, long-press timing,
  accessibility, candidate window and mic — the other Track 3 decisions.
- Whether to offer 세벌식/천지인 later — the goal names 두벌식 only; adding more
  is a content decision the data contract already permits.

No verdict is recorded. The maintainer's 두벌식 clarification is quoted above as
the requirement; the symbol-layer decision remains open.

## 5. Backspace long-press repeat (checked 2026-11-23, KP14's premise)

KP14's request carries the premise "다른 ime는 그런 기능을 제공할 거야" (other
IMEs provide word-unit delete on backspace long-press). Checked against
upstream sources (read for semantics, no code copied):

- **HeliBoard** (`PointerTracker.java`): generic key repeat — the first repeat
  fires after `keyRepeatStartTimeout`, later repeats at a constant
  `keyRepeatInterval` (the two `MainKeyboardView` attrs); no acceleration ramp
  in that path. The repeat re-sends the pressed key code, so backspace repeats
  `KEYCODE_DEL` — **character unit**, not word.
- **FlorisBoard** (`keyboard3/ImeActions.kt`,
  `keyboard3/interaction/AndroidInteractionController.kt`): Backspace is in the
  `Repeatable` action list; repeat timing comes from the **system**
  `keyRepeatTimeout` / `keyRepeatDelay`. The repeated action is the Backspace
  action — **character unit**; no word-unit repeat path found.
- **Closed-source IMEs** (Gboard, Samsung Keyboard): word/block deletion on
  backspace long-press is widely reported, but not verifiable from code here —
  **unverified**, recorded as such.

Finding: the surveyed open-source IMEs repeat character deletion; hunmin's
ruled word-unit repeat (~0.2 s, KP14) is a differentiating choice, not a copy
of the surveyed open-source behavior. The premise is partially supported
(closed-source, unverified) and not supported by the open-source survey. This
is a finding, not a verdict — the ruling stands in the SPEC.
