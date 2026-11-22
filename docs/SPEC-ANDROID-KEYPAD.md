# SPEC-ANDROID-KEYPAD — Android soft-keypad requirements

Requirements for the Android virtual keypad (0.4 Track 3, issue #285 / #298).
These bind the Android frontend's keypad; the common frontend requirements are
in `docs/SPEC-FRONTEND.md` and are cited, not restated. The composition engine
and layout-data contract are unchanged by this SPEC — it is about how the keypad
presents keys, not how the engine composes.

**Status:** the criteria below were stated by the maintainer on 2026-11-22 and
are recorded verbatim as requirements. The **mechanism is open** (issue #298):
no layout, geometry or engine change is ruled here, and none is implemented yet.
The design analysis that prices the options is
`docs/ANDROID-SYMBOL-INTERACTION.md`; the prior-art survey is
`docs/ANDROID-KOREAN-IME-SURVEY.md`.

# Requirements

### KP1. 104-key symbols at minimal touch and physical positions; number-row symbols paired with digits

The maintainer's words (2026-11-22, verbatim):

> "중요한 기준을 세워보자면, 표준 104키 키보드로 입력할 수 있는 기호들은 최소한의
> 터치로 입력할 수 있고, 물리 키보드의 위치와 유사하게 제공한다. 그 외의 자주
> 사용되는 특수 문자들은 조금 더 입력이 힘들어도 상관 없다. 특히 숫자열에 있는
> 기호들은 숫자 키와 연결해서 제공하는 게 좋다. 이건 암호 입력 시에 숫자와
> 기호를 연결해서 사용하는 경우가 많아서 그래."

Observable outcome, in three parts:

- **Tier 1 — the symbols a standard 104-key keyboard can type** are reachable in
  the minimum number of touches, at positions similar to the physical keyboard.
- **Tier 2 — other frequently-used special characters** (those a 104-key cannot
  type) may cost more touches; that is acceptable.
- **The number-row symbols** (the shifted digits) are provided **linked to the
  number keys**, because password entry commonly interleaves digits and their
  symbols.

The mechanism that satisfies this — a QWERTY-positioned symbol page, a third
per-key row, long-press pairing on the number row, or another arrangement — is
the open choice of issue #298. *Implementation:* none. *Refuted by:* none yet.

### KP2. Functional keys separated, compact and non-physical; one shift

The maintainer's words (2026-11-22, verbatim):

> "스페이스, 엔터, 백스페이스는 실수로 입력하는 것을 피하기 위해 서로 가까이
> 붙어있어서는 안되긴 하나, 물리 키보드처럼 큰 면적을 차지할 필요도 없고, 물리
> 키보드의 위치에 있을 필요도 없음. 시프트도 마찬가지로 2벌 있을 필요 없음."

Observable outcome:

- **Space, Enter and Backspace are not placed close together**, so a mis-tap
  does not turn a space into a deletion or a premature commit (user error
  protection).
- They **need not occupy the large area** a physical keyboard gives them, and
  **need not sit at physical-keyboard positions** — the physical-position rule
  of KP1 applies to character/symbol keys, not to these functional keys.
- **Shift needs only one instance**, not the two a physical keyboard carries.

The placement that satisfies this is a geometry/presentation decision (Track 3),
not an engine change: the engine already treats space/enter/backspace as named
keys and shift as a per-event modifier (`docs/SPEC-FRONTEND.md#§FE6`).
*Implementation:* partial (#299) — one one-shot shift, separated compact
functional keys: `docs/DESIGN-ANDROID-KEYPAD.md` §3–§4. *Refuted by:* none yet.

### KP3. Frequency-based placement and visual emphasis — symbols and the main keypad (endorsed directions)

The maintainer's words (2026-11-22, verbatim):

> "기호들의 사용 빈도를 기반으로 배치하는 건 좋은 방향이라고 생각해. 다른
> 방법으로는 사용 빈도가 높은 기호를 강조해서 보여주는 - 색상이나, bold 등의
> 방법으로 - 것도 좋은 방향일 것이라고 생각해."

Two endorsed directions for cutting the visual-search time of KP1, stated as
directions rather than a fixed mechanism:

- **Frequency-based placement** — arrange symbols by how often they are used, so
  the frequent ones are reached and found first.
- **Visual emphasis** — highlight the frequent symbols (colour, bold, …) so they
  are found faster *without* moving them.

The maintainer extended the emphasis direction to the whole keypad (2026-11-22,
verbatim):

> "다른 면으로는 한글/영어 입력 시에도 숫자, 편집키 등은 연하게, 문자는 진하게
> 표시하는 것도 같은 이유에서 하는 일이라고 생각하면 돼."

So the principle is a **visual hierarchy across the keypad**, not only symbols:
in Hangul/English text mode the character keys (jamo/letters) are the primary
targets and are shown prominently (진하게), while numbers and editing keys are
secondary and shown faintly (연하게). The goal is the same — guide the eye to the
current mode's primary targets and cut visual-search time. It is a
presentation/theme decision, with no engine change.

These are complementary, and they interact with KP1: frequency *repositioning*
can pull away from the physical-position familiarity KP1 asks for, whereas
*emphasis* keeps the positions and adds salience, so it improves discoverability
without spending screen space or shrinking targets. Both need a symbol-frequency
source; a first prose measurement and its method are in
`docs/ANDROID-SYMBOL-INTERACTION.md` (`tools/symbol-frequency.py`), and the bulk
corpora live outside the repository. Emphasis
must not rely on colour alone (accessibility, `docs/SPEC-FRONTEND.md` usability;
ISO 25010 user error protection / appropriateness recognizability). The choice and
combination are open. *Implementation:* none. *Refuted by:* none yet.

### KP4. Mode separation; very frequent symbols default-visible in Hangul/English mode (stated direction)

The maintainer's words (2026-11-22, verbatim):

> "그래서 화면 입력기에서는 한글, 영어, 기호 입력이 분리되고, 정말 자주 사용되는
> ,.?"등은 한글/영어 모드에 기본 표시되게 하는 방향까지는 정리된거지?"

The stated direction: the soft keypad **separates Hangul, English and symbol
input**, and the **very frequent symbols** (`, . ? "` and the like) are
**default-visible in the Hangul/English mode**, not hidden behind the symbol
page. This is the synthesis of KP1 (minimal touch for the frequent set) and KP3
(emphasis/search-time), and it matches the prior art (HeliBoard's dedicated
comma/period on the main bottom row). It is consistent with the recorded
criteria; the **exact symbol set, its geometry, and how the separation is
realised** (separate page vs layer) remain the open mechanism of issue #298.
*Implementation:* partial (#299) — mode pages separated; `, .` default-visible
in every text mode: `docs/DESIGN-ANDROID-KEYPAD.md` §3. The set beyond `, .`
is open. *Refuted by:* none yet.

### KP5. Number-row symbols by long-press AND swipe-up; accidental swipes preventable (stated direction)

The maintainer's words (2026-11-22, verbatim):

> "swipe up도 함께 제공하면 좋겠네. 문제는 swipe up을 의도하지 않았는데 그렇게
> 입력되는 것. 회피할 수 있니? 기호를 화면에 어떻게 표시하면 좋을까?"

Stated direction and requirement:

- The number-row digit↔symbol link (KP1) is offered through **both long-press
  and swipe-up**, not long-press alone.
- **An unintended swipe-up must be preventable** — a tap that drifts upward must
  not silently insert a symbol (ISO 25010 user error protection). This is a
  requirement on the gesture, not a suggestion.
- How symbols are shown on screen is an open question; the analysis and prior
  art are in `docs/ANDROID-SYMBOL-INTERACTION.md` (gesture-disambiguation
  defences and the corner-hint / popup-preview display). The mechanism is open.

*Implementation:* none. *Refuted by:* none yet.

### KP6. Symbol page: no shift, navigation keys instead; Korean special symbols inputtable (stated direction)

The maintainer's words (2026-11-22, verbatim):

> "왜 ~`는 누락되어 있니? 기호 창에서의 shift 사용은 필요없을 것 같고, 대신에
> 방향키, tab이 home/end/pgup/pgdn이 있으면 유용할거야. 그 외에도 한글 사용시에
> 많이 쓰는 특수 기호들을 다시 찾아보고 입력할 수 있게 해볼까?"

Stated directions:

- **The symbol page needs no shift key** — its symbols are shown directly, so
  shift is redundant there. In its place, **navigation/editing keys** are useful:
  arrows (← ↑ → ↓), Tab, Home, End, PgUp, PgDn.
- **Korean-frequent special symbols** (the ones a 104-key cannot type — Tier 2)
  are to be found and made inputtable. A first frequency reading and a grouped
  candidate set are in `docs/ANDROID-SYMBOL-INTERACTION.md`.
- The `~` / `` ` `` omission noted here was a **mockup gap in the text-mode number
  row** (digits 1–0 only); both characters are in `us-qwerty` and on the symbol
  page. Whether the text number row also carries `` ` ~ - _ = + `` (a 13-key row,
  narrower targets) is an open geometry choice.

Clarification (2026-11-22, verbatim):

> "기호 모드에서 시프트를 안 쓴다는 건, {나 \"도 버튼을 분리한다는 거야. 그리고
> `는 첫 줄에 표시하기 힘들다면 두번째 줄 - 앞에 표시하는 게 나을 것 같아."

So "no shift" means the **shifted variants are separate buttons too**: `{` and
`"` (and every shifted form) get their own key, not a corner hint — the symbol
page shows all 32 ASCII punctuation as individual keys, each unshifted symbol
adjacent to its shifted partner. The maintainer gave the exact arrangement
(2026-11-22, verbatim): row `` ` ~ - _ = + [ { ] } \ | `` (`` ` `` leading, before
`-`) over row `! @ # $ % ^ & * ( )`. The remaining pairs follow the same rule:
`; : ' " , < . > / ?`. So:

```
! @ # $ % ^ & * ( )          (10)
` ~ - _ = + [ { ] } \ |      (12, ` before -)
; : ' " , < . > / ?          (10)
```

(11–12-key rows are ~30–33 dp on a 360 dp screen — denser than the text modes,
acceptable for an occasional-use page; the target-size trade-off is recorded,
not ruled.)

Arrangement — deliberated, then **decided** (2026-11-22, verbatim):

> "살짝 고민 중인 건 []{}로 하는 게 나을까? 정도야."
> "그건 <> 에 대해서도 동일하게 적용해야 해."
> "[]{} 순서로 결정하자."

**Ruling: the grouped order `[]{} `** — the unshifted pair, then the shifted pair
— and the consistency rule applies it to every paired symbol, `<>` included. The
symbol page layout is therefore:

```
! @ # $ % ^ & * ( )          (10)
` ~ - _ = + [ ] { } \ |      (12, ` before -, brackets grouped)
; : ' " , . < > / ?          (10, comma/period grouped the same way)
```

The alternative was partner-adjacent (`[{ ]}`, `,< .>`), which mirrors the
physical key (KP1: `{` is Shift+`[`, `>` is Shift+`.`); the maintainer chose the
grouped order (a symbol family clustered for visual search, KP3) over the
physical pairing. This sub-decision is settled; the data home, navigation-key set
and Tier 2 inventory remain open (#298).

The navigation-key set, the Tier 2 inventory/grouping, and the data home that
makes them inputtable are open mechanism (#298). *Implementation:* partial
(#297/#299) — the Tier 1 page and nav row are drawn from the app-read asset
and tap-verified: `docs/DESIGN-ANDROID-KEYPAD.md` §4; Tier 2 is #312 (KP10).
*Refuted by:* none yet.

Navigation-key order — decided (2026-11-22, verbatim): "home pgup pgdn end 순서로
변경." So the editing cluster reads **Home · PgUp · PgDn · End** (Tab leading,
⌫ trailing), not Home/End/PgUp/PgDn. The exact key set is still open; this fixes
the order of those four.

### KP7. Swipe-up as a universal substitute for long-press / shift (stated direction)

The maintainer's words (2026-11-22, verbatim):

> ",? 키에 swipe-up을 써서 ?를 입력하게 할 수 있을까? 그리고 shift도 swipe-up으로
> capslock 처럼 동작하게 할 수 있니?"
> "즉 swipe-up을 long-press 혹은 shift 대체제로 쓸 수 있게 하는 것."

The general principle: **swipe-up on a key reaches that key's alternate**, as a
substitute for long-press or shift. Instances:

- punctuation `,` swipe-up → `?` (and `.` swipe-up → `"`);
- number key swipe-up → its symbol (KP1/KP5);
- **shift swipe-up → capslock** (tap = one-shot shift);
- by extension a letter/jamo key swipe-up → its shifted form (`q`→`Q`,
  `ㅂ`→`ㅃ`), substituting for shift+key on a single character.

This is the press/swipe key-value model of Unexpected Keyboard ("what a key does
when pressed or swiped"), and the shift→capslock gesture is an alternative to
FlorisBoard's `CAPSLOCK_BY_DOUBLE_TAP` / `CAPSLOCK_BY_CYCLE`. The KP5
accidental-swipe defences apply to every such swipe. The glide-typing conflict
is **resolved by ruling**: the maintainer will not add glide typing (2026-11-22,
verbatim: "글라이드 기능은 불편하기만 해서 추가할 생각이 없어."), so swipe-up on
letter/jamo keys has no competing gesture. The shift state machine (how capslock
unlocks) and whether swipe-up-on-letters is enabled are open mechanism.
*Implementation:* none. *Refuted by:* none yet.

### KP8. Per-key swipe directions may vary, bounded by learnability (stated direction + constraint)

The maintainer's words (2026-11-22, verbatim):

> "한 키에 대해 swipe 방향에 따라 기능을 다양하게 할 생각은 있지만, 이것도 너무
> 복잡하면 사람이 기억하고 쓸 수 없더라."

Stated direction with a guardrail: a key **may** carry different functions per
swipe direction (up/down/left/right), but the scheme must stay **simple enough to
remember and use**. Complexity that defeats memory is a failure, not a feature
(ISO 25010 learnability / appropriateness recognizability). The extreme to avoid
is Unexpected Keyboard's 16-direction + circle/roundtrip gesture set — capable
but a power-user memorisation burden. The way to have several directions without
the burden is **recognition over recall**: visible per-direction hints (KP3), a
consistent mapping across keys (e.g. up = alternate everywhere), and a small cap
on directions per key. How many directions, which functions, and the hint design
are open mechanism; the constraint is that a person can use it without
memorising a table. *Implementation:* none. *Refuted by:* none yet.

### KP9. Positional consistency across modes; symbol-page functional keys (stated direction)

The maintainer's words (2026-11-22, verbatim):

> "Tier2 버튼의 위치가 어색하고, 기호 입력 창에 스페이스가 있어야 하는 지 의문임.
> 백스페이스는 필요할 것 같은데, 엔터, 스페이스는 삭제하고, 남은 공간에 원래 모드로
> (이걸 한/영이라고 표시해도 헷갈리지 않을 것 같고), Tier2 버튼이 다른 모드와
> 동일하게 들어가고, 스페이스바 위치에 방향키가 있는 게 더 자연스러울 거 같아.
> 모드와 상관없이 같은 키는 같은 위치에 있는 게 실수가 적어."
> "Tier2도 10개 키를 한 줄에 배치해서 줄 수를 맞추는 게 좋겠다."

Two things:

- **General principle — positional consistency:** regardless of mode, the *same*
  key sits in the *same* position, because that reduces mis-taps. Shared
  functional keys (backspace, the mode-switch / return key) keep their places
  across Hangul / English / symbol modes; a mode-specific slot may be repurposed
  (the space-bar slot becomes arrows in symbol mode).
- **Symbol-page functional layout:** drop **space and enter** (a symbol page
  needs neither), keep **backspace**; a **return-to-original-mode** key labelled
  **한/영** (not confusing) and the **Tier 2** button take the mode-key positions
  used in the other modes; **arrows go in the space-bar slot**. The Tier 2 button
  leaves its awkward inline spot on the symbol row. **Tier 2 uses 10 keys per
  row** to match the symbol page's row structure — done by **keeping the original
  8-key category groups and moving 2 symbols from the bottom row up into each**
  (2026-11-22, verbatim: "기존에 8개로 묶은 이유가 있을텐데, 맨 아래줄에 있는 기호
  2개씩을 각 줄로 옮기는 방식이 더 깔끔해."), not by reflowing the categories.

Tier 2 carries **no editing keys** (2026-11-22, verbatim: "tier2에는 편집키가
필요없어."): it is a symbol picker, so the navigation cluster, arrows and
backspace are absent — correction happens in the text mode. Its bottom row is
just the mode-return keys (`한/영`, `기호 1`), in the same slots as Tier 1.

A layout that follows (bottom row mirrors the text modes' `[한/영][기호]…`):
`[한/영] [Tier 2] [←] [↑] [↓] [→]`, backspace on the navigation row. The exact
geometry is provisional; the binding rules are positional consistency and the
space/enter removal. *Implementation:* partial (#299) — no space/enter on the
page, arrows in the space slot, 한/영 returns to the original mode (tap-verified
on the AVD): `docs/DESIGN-ANDROID-KEYPAD.md` §4; the Tier 2 slot is #312.
*Refuted by:* none yet.

### KP10. Symbol selection by swipe/long-press preferred over a Tier 2 button; extension split out (stated direction)

The maintainer's words (2026-11-22, verbatim):

> "심볼 키에 대한 swipe 동작이나 longpress로 어떤 기호를 입력할지 선택하는 게 tier2
> 버튼을 두는 것보다 나아 보인다. 일단 이렇게만 정리해 두고 이 기능을 확장하는 건
> 별도 이슈로 만들자."

Stated direction: reaching alternate symbols by **swipe / long-press on a symbol
key** is preferred over a separate **Tier 2 button/page**. The Tier 2 Korean
special symbols become gesture variants of the Tier 1 symbol keys (KP7/KP8),
rather than a page of their own. This deprioritises the separate Tier 2 page of
KP6/KP9; those records stand as the history of the considered alternative.

**Scope ruling:** #298 records this direction only; **extending the feature is a
separate issue, #312** (`[android] extend symbol selection via swipe/long-press on
symbol keys`). The variant mapping and the per-key gesture set (bounded by KP8)
are #312's work, not #298's; the variant mapping's data home is already settled
by KP11 — the app-read symbol data file, with the engine uninvolved. *Implementation:*
none. *Refuted by:* none yet.

### KP11. Symbol screen data home — app-read data file, engine uninvolved (DECIDED)

The maintainer's ruling (2026-11-22, verbatim):

> "앱이 데이터 파일로 그리기. 엔진은 관여 안함."

**Decided:** the symbol screen is drawn by the **Android app from a data file**;
the composition **engine is not involved**. This is option **B2** of
`docs/DESIGN-ANDROID-SYMBOLS.md` — a separate symbol inventory, read by the
frontend, not a third per-key row (A) and not an engine-loaded layout (B3). It
settles #298's core question: **no engine or composition-schema change** for the
symbol screen. The symbol list is still *data* (honouring "layout behaviour is
data"), just data the app reads rather than the engine.

Consequences: the engine's Hangul/Latin composition is untouched; the symbol
screen does not appear in the engine's layout/settings list (no visibility
contract needed, unlike B3); the data file's format and validation are a
frontend data contract, settled at implementation. The gesture-variant mapping
(KP10) is #312. *Implementation:* #297 (data + contract) and #299 (UI wiring:
the app draws the page, flushes the pending word, then commits the symbol
directly — `docs/DESIGN-ANDROID-KEYPAD.md` §1). *Refuted by:* a future need to
share the symbol screen across frontends would reopen the data home (move
toward B3).

### KP12. The soft keypad auto-shows only without a physical keyboard (DECIDED)

The maintainer's words (2026-11-22, verbatim):

> "이 UI는 물리 키보드가 없을 때만 자동으로 뜨는 거야."

Observable outcome: the soft key grid appears **automatically only when no
physical keyboard is attached**. With a physical keyboard present and exposed,
the input view stays the strip alone — the hardware path is the product there,
and a key grid over it is noise. The strip itself must remain in every case:
it is the IME window hardware key events arrive through (the B1 fact tier E2
measures, `docs/DESIGN-FRONTEND.md` Part B §3); hiding the whole input view
would break the hardware path this requirement protects.

Mechanism (implementation, not part of the requirement): the platform's own
`Configuration.keyboard` / `hardKeyboardHidden` answer presence, and the
platform's standard user opt-in `show_ime_with_hard_keyboard` is an EXPLICIT
request for the soft keyboard, not an automatic appearance — it is honoured as
the exception it is. *Implementation:* `KeypadVisibility` (decision table),
service wiring in `onCreateInputView` / `onConfigurationChanged`
(`docs/DESIGN-ANDROID-KEYPAD.md` §8). *Verified by:* tier E1
`KeypadModelTest` (decision table), tier E2 (BOTH paths measured on the AVD,
which reports a QWERTY keyboard exposed: opt-in 0 hides the grid with the
strip staying, opt-in 1 shows it; the maintainer's phone confirmed auto-show
on a NOKEYS device).

### KP13. The decided keypad UI is the mockup picture (DECIDED)

The maintainer's words (2026-11-22, verbatim):

> "T3-1에서 정한 UI가 아닌데?"
> "/home/scoby/work/hunmin/docs/android-keypad-mockup.html 로 정했잖아?"
> "왜 그 mockup 대로 구현하지 않았지?"

**Decided:** the soft keypad's UI is `docs/android-keypad-mockup.html` AS
DRAWN — the 두벌식 and QWERTY text screens (10-key number row with the shifted-
symbol corner hints, 10/9/7 letter rows, the shifted-jamo sublabels, one ⇧ with
the ⇪ hint, ⌫, and the bottom row `[한/영] [기호] [space] ,<?> .<"> [⏎]`, amended by KP15 to comma-left-of-space), the
symbol page, and the picture's palette and proportions. The document's own
"provisional / not a ruling" header predates this ruling and is superseded by
it; the header is kept as the record of what was believed when the first
implementation deviated (a generic data-driven grid with extension cells —
overturned, recorded in `docs/DESIGN-ANDROID-KEYPAD.md`).

Scope notes, so the picture and the later rulings read together:

- The Hangul screen is 두벌식 and the English screen is QWERTY — the Track 3
  goal layouts (#285: "두벌식 형태를 채용"). Keycap LABELS still come from the
  layout data; the GEOMETRY is the picture's, not a generic physical-position
  grid. A layout whose keys fall outside the picture's cells (sin3-p2's
  punctuation keys) is not fully touch-reachable through this geometry — a
  recorded consequence, and whether the keypad offers other layouts is open.
- The corner hints are drawn as the picture draws them. The GESTURES they
  advertise (long-press / swipe-up, KP5/KP7) remain #312; the shifted-jamo
  sublabels and digit hints are reachable today through the one-shot shift.
- The symbol page's Tier 2 button in the picture is superseded by KP10/#312
  (gesture variants instead of a Tier 2 page); the shipped `symbols.json`
  omits that slot, and the bottom row reads `[한/영] [←] [↑] [↓] [→]`.
- The picture draws a light palette; night-mode adaptation is not drawn and
  stays open.

*Implementation:* `KeypadModel.textRows` (the picture's geometry),
`KeypadView` (the picture's palette/proportions/hints) —
`docs/DESIGN-ANDROID-KEYPAD.md`. *Verified by:* engine-android-e1
(`KeypadModelTest`, `KeypadTouchGoldenTest`), engine-android-e2e (touch round).

### KP14. Long swipe-left anywhere is backspace, with a configurable delete unit (REQUESTED)

The maintainer's words (2026-11-22, verbatim):

> "기능 하나를 추가했으면 하는 게, 어느 위치에서든 swipe-left를 (길게) 하면
> backspace로 동작하면 좋겠다. 이 기능은 설정가능하게 해서, key단위, 음절단위,
> 단어단위, 전체 삭제로 설정할 수 있어도 좋겠다."

Observable outcome:

- A **long swipe-left starting at any keycap** deletes text instead of typing
  that key. The underlying tap must not also fire (the gesture replaces it).
- The delete unit is **configurable**: 키(key) / 음절(syllable) / 단어(word) /
  전체(all). A short leftward drift is still a tap — KP5's accidental-gesture
  protection applies to this gesture too.

Mechanism (implementation, provisional where noted): the gesture decision is a
pure function (`KeypadModel.isSwipeLeftDelete` — leftward distance over a
threshold, horizontally dominant); the threshold is provisional. Delete units:
KEY is the backspace key's own path; SYLLABLE removes the last syllable of the
composing display (jamo by jamo, stopping when the syllable is gone — measured,
not guessed); WORD drops the buffered word when composing, else sends Ctrl+DEL
to the app; ALL clears the composition and sends Ctrl+A then DEL as key events
(no surrounding-text read, I9). The default unit is KEY (the
least destructive) — provisional, the maintainer may choose another. The
setting lives in the settings window's 키패드 section.

Also recorded (maintainer, 2026-11-22, verbatim): "long-press도 swipe-up도
동작하지 않아." — measured fact: those gestures are #312's scope and are not
wired; KP13 draws their hints. This entry does not pull #312 forward; it records
the observation so the gap between the drawn hints and the wired gestures is
visible.

*Implementation:* `KeypadModel` (gesture decision, `DeleteUnit`), `KeypadView`
(touch handling), `HunminInputMethodService` (delete units, setting),
`OverlayActivity` (키패드 section) — `docs/DESIGN-ANDROID-KEYPAD.md`.
*Verified by:* engine-android-e1 (decision table, unit vocabulary),
engine-android-e2e (swipe on the device, per-unit deletion).

### KP15. Bottom row amended: comma left of space (DECIDED)

The maintainer's words (2026-11-22, verbatim):

> "스페이스 위치가 살짝 애매한데, ,를 스페이스 왼쪽으로 옮겨서, 스페이스가
> 좀더 오른손쪽으로 오는 게 좋겠어."

**Decided:** the text-mode bottom row reads
`[한/영] [기호] [,] [space] [.] [⏎]` — the comma moves to the space's LEFT, so
the space bar sits further toward the right hand. This amends the KP13 picture
(the mockup artefacts are updated to match; the picture and the ruling must not
drift). The period stays to the space's right; KP9's positional consistency
holds — the row is the same in Hangul and English modes.

*Implementation:* `KeypadModel.textRows` bottom row;
`docs/android-keypad-mockup.html` / `.md` amended. *Verified by:*
engine-android-e1 (`KeypadModelTest` row order), engine-android-e2e (taps).

### KP16. Shift on the frequent , . keys yields the hinted alternate — same result as the gesture (DECIDED)

The maintainer's bug report (2026-11-22, verbatim):

> "comma 위에 ?, period 위에 "를 적어서 시프트 입력을 해보려고 했는데,
> <>가 각각 입력되네?"

The ruling (2026-11-22, verbatim):

> "시프트를 사용해도 동일한 결과가 나와야지."

**Decided:** on the touch keypad, shift+`,` yields `?` and shift+`.` yields `"`
— the characters their drawn hints advertise, the SAME result the swipe-up
gesture will produce (KP7: swipe-up is a substitute for shift, so the alternate
is one character regardless of mechanism). The hint is a promise about the
key's alternate, not about the physical keyboard's shift row.

Measured cause of the defect: in a layout that does not key `,`/`.` (dubeolsik),
the shifted tap passed through to the app, whose US KeyCharacterMap resolves
Shift+, as `<` and Shift+. as `>`. The fix commits the hint character through
the symbol path (pending word flushed first), for the two frequent keys only,
in every mode and layout. Digits keep the engine/app path — their physical
shift already matches the hints (! @ # …). The hardware key path is untouched.

*Implementation:* `KeypadModel.shiftedFrequentAlternate`, `KeypadView` tap
dispatch — `docs/DESIGN-ANDROID-KEYPAD.md`. *Verified by:* engine-android-e1
(`KeypadModelTest`), engine-android-e2e (shift taps on the device).

KP16 display rule (maintainer, 2026-11-22, verbatim):

> "시프트가 입력되면 그 키가 ?"로 출력되게 하는 것도 좋은 방법이고."

While shift is armed, the frequent keycaps DISPLAY the alternate they will
type (`?` and `"`), the corner hint folded into the label — the same principle
as the jamo keycaps showing `ㅃ` while armed (endorsed the same day: "ㅂ위에
ㅃ이 출력되는 것도 좋은 방법인데 말이지"). Digits likewise show their symbol
(`! @ # …`) while armed. The display and the result are one promise.

Placement ruling (maintainer, 2026-11-22, verbatim): "현행 유지." — the
shifted-jamo sublabel stays at the mockup's bottom-left; a top-corner move
applied earlier the same day was reverted. The condition attached to the
display proposals ("글자가 겹치지만 않는다면 내가 제안한 방식을 적용해 봐")
is verified by measuring the keycap TextViews' bounds for overlap.

### KP17. The soft keyboard uses 두벌식 only (DECIDED)

The maintainer's words (2026-11-22, verbatim):

> "그게 왜 필요한데? 소프트 키보드는 두벌식만 쓸건데?"

**Decided:** the soft keypad's Hangul mode is 두벌식 — always, regardless of
the 한글 배열 setting. The setting governs the HARDWARE-keyboard session; the
keypad session (no physical keyboard, KP12) composes with dubeolsik, and the
English keypad with us-qwerty (the picture's QWERTY). This closes the
keypad↔layout-setting question: sin3-p2's punctuation keys being outside the
picture's cells is moot for touch, because touch is 두벌식.

The architectural ruling (maintainer, 2026-11-22, verbatim):

> "소프트 키보드와 물리 키보드 구현은 전혀 관계가 없어야 하는데..."

Mechanism: the soft keypad has its OWN engine pair (dubeolsik + us-qwerty,
fixed constants), built per editor alongside the hardware pair. Touch keys
dispatch to the touch pair; hardware keys keep the existing path and the
configured layouts. The two implementations share only the editor
(InputConnection) and the user's single mode indication (한/영 toggles both
pairs' language); neither reads the other's layout or state. KP12 visibility
is presentation only — no engine rebuild on surface change. The first draft
of this mechanism tied the layout to visibility and rebuilt the engine on
transition; that coupling is what the ruling rejected. The settings window
stores the hardware choice, inert for the soft surface.

*Implementation:* `KeypadModel.SOFT_HANGUL_LAYOUT` / `SOFT_LATIN_LAYOUT`,
the service's touch engine pair and touch dispatch —
`docs/DESIGN-ANDROID-KEYPAD.md`. *Verified by:* engine-android-e1 (constants,
touch golden), engine-android-e2e (the touch round composes 두벌식; the
hardware round keeps sin3-p2 — the surfaces are independent).

Rationale for the independence (maintainer, 2026-11-22, verbatim):

> "이 결정을 하는 이유는, 물리 키보드와 소프트 키보드의 UX가 전혀 다르기 때문이야.
> 물리 키보드는 swipe-up이 불가능하지만 반복 입력이 쉽고, 소프트 키보드는 그 반대.
> 이런 서로 다른 특성을 최대한 맞추어서 최적화해야 해."

The two surfaces have OPPOSITE UX characteristics — hardware: no gestures,
easy key repeat; soft: gestures available, repeat tapping hard. Each must be
optimized for its own characteristics, which is why one shared implementation
(the first draft's "touch is hardware" key path, and the visibility-tied layout
switch) is the wrong shape: it compromises both. Consequences already in train:
the soft surface gets gesture alternates (KP7/#312) and long-press word-delete
(#312, compensating hard repeat); the hardware surface keeps OS key repeat and
the configured layouts. Independence is the means; per-surface optimization is
the end.
