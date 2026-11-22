# DESIGN-ANDROID-KEYPAD — the soft keyboard UI (issue #299, T3-3)

Status: **implementation design for issue #299**, the soft-keyboard UI sub-issue
of Track 3 (#285). The rulings that govern it are KP11 (symbol screen data home,
`docs/SPEC-ANDROID-KEYPAD.md#§KP11`) and the Track 3 scope boundary (두벌식 +
QWERTY grids, no 세벌식 ergonomics block, gestures are #312). Requirement text
is cited by id, never restated. The provisional geometry it draws from is
`docs/ANDROID-KEYPAD-MOCKUP.md`; the symbol data contract is
`docs/DESIGN-ANDROID-SYMBOLS.md` (Implementation section, issue #297).

## 1. The shape: three seams, one key path

The frontend stays thin (FE11) by splitting the keypad into what is data, what
is state, and what is pixels:

| seam | file | what it owns |
|---|---|---|
| `KeypadKeycaps` / `KeypadModel` / `KeypadState` | `KeypadModel.kt` | pure Kotlin: layout JSON → keycap labels and rows; key id → AOSP keycode; one-shot shift and page state. No Android view classes, so tier E1 drives it on a plain JVM. |
| `KeypadView` | `KeypadView.kt` | rendering and tap dispatch only: rows of keycaps, HudTheme colours, content-desc for the device tier. |
| service wiring | `HunminInputMethodService.onCreateInputView` | four callbacks into methods the hardware path already uses. |

**Touch is hardware.** A character or function keycap tap synthesizes the same
`(keyCode, metaState)` a physical key delivers and enters through the service's
`onKeyDown` — the ONE key path. Everything the hardware path owns therefore
applies to touch unchanged: the space boundary and judgement adoption
(ReqMode8), the secure-editor pass-through (issue #13), key capture, the
classifier's chord/unmapped/feed precedence. A second key path for touch would
be a second home for every one of those policies.

The symbol page is the deliberate exception KP11 rules: its symbol keys commit
directly (`SymbolScreen.commitSymbol`), engine uninvolved. The wiring flushes
the pending word FIRST (`HunminAdapter.commitWord`) so the symbol never lands
ahead of the buffered composition (`docs/DESIGN-ANDROID-SYMBOLS.md` §4,
FE1/FE2), and takes the judgement at that boundary exactly as the space branch
does. **Known consequence, measured not assumed:** a direct symbol commit does
not enter the autoswitch key log (ReqMode4 records `(base, shift)` of key
events); this is the trade-off §1.4 of the symbols design recorded when KP11
chose the direct path over the mode-toggle path.

## 2. Keycap labels are the layout's own data

The grid draws the ACTIVE layout's keys from the same JSON the engine loads
(`libhunmin/data/layouts/<id>.json`, installed to `filesDir/hunmin-data/` by
`HunminDataInstaller`). The app reads it for presentation only; the engine's
composition is untouched and no ABI symbol is added. This is the touch-key data
source question of #285 answered with **runtime JSON, no codegen, no new JNI** —
the file is already packaged and extracted, so a second copy would be drift.

- A literal value `=x` displays as `x`.
- A conjoining jamo (U+1100 block) displays as its compatibility form (ㅂ, ㅗ,
  ㄱ…) because conjoining jamo render as invisible or positional forms in most
  fonts. The mapping is **derived, not transcribed**, from the Unicode data the
  JDK carries, in two passes: compatibility jamo U+3131–U+318E decompose under
  **NFKD** to their choseong/jungseong source; jongseong has **no decomposition**
  (ㄱ decomposes to the choseong ᄀ, not the jongseong ᆨ), so it is matched by
  Unicode letter name (`HANGUL JONGSEONG GIEOK` ↔ `HANGUL LETTER GIEOK`).
  **Recorded mistake:** the first draft used NFD — the decomposition is
  `<compat>`, which only NFKD applies, so an NFD-built table is empty; measured
  before shipping, and the jongseong gap was measured the same way. A jamo with
  no compatibility form at all (rare old-Hangul letters in the yet layouts)
  displays raw; the Track 3 layouts are measured fully covered
  (`KeypadModelTest`). It is Unicode, not layout behaviour — same for every
  layout.
- A `case` value (SPEC-SCHEMA's condition-resolved primitive — sin3-p2's tier
  keys) displays its **default branch** (`else`, falling back to `then`): what
  the key types with no tier state active is what a keycap at rest should show.
  The engine still resolves the real value per keystroke; this is presentation,
  and it is generic over the schema, not a sin3-p2 branch.
- No per-layout branch anywhere (FE12): the grid is one generic geometry, and
  which cells appear is answered by the layout's own key set.

## 3. Geometry — the decided picture (KP13), and the overturned deviation

**Decided (KP13, maintainer 2026-11-22, verbatim: "docs/android-keypad-mockup.html
로 정했잖아?"):** the UI is `docs/android-keypad-mockup.html` AS DRAWN. The grid
is the picture's: a 10-key number row (digits only, shifted-symbol corner
hints), 10/9/7 letter rows, one ⇧ (⇪ hint) and ⌫ on the third row, and the
bottom row `[한/영] [기호] ,<?> [space] .<"> [⏎]` (KP15: comma left of space). The Hangul screen is 두벌식,
the English screen QWERTY — Track 3's goal layouts. Labels still come from the
layout's own JSON (§2); the GEOMETRY is the picture's, and the view transcribes
the picture's palette and proportions (white keys, #f4f5f7 panel, accent
한/영/⏎, 연하게 digits and functional keys, 46 dp keys, 8 dp corners, 11 sp
corner hints, 10 sp shifted-jamo sublabels). The picture draws a light theme;
night-mode adaptation is not drawn and stays open.

**Overturned — the first implementation's generic grid.** Before KP13 was
recorded, this doc proposed a generic physical-position grid: a cell drawn
wherever the active layout defines a key, extension cells ([ ] \ ; ' / ` - =)
included, so every layout (sin3-p2 among them) was fully touch-reachable. The
maintainer rejected it — "T3-1에서 정한 UI가 아닌데?", "왜 그 mockup 대로
구현하지 않았지?" The deviation's cause is recorded so it is not repeated: the
mockup document's header said "provisional, not a ruling", and the work read
that as permission to design a different geometry instead of asking. When a
document's status and the maintainer's intent disagree, ASK; do not pick.

Consequences of the decided picture, recorded rather than smoothed over:

- **sin3-p2's punctuation keys are not touch-reachable** through the picture's
  cells (its ; ' [ ] \ ` - = keys fall outside the 두벌식/QWERTY form). The
  letter keys draw at their positions; , . keep their bottom-row slots (they
  ARE layout keys there). Whether the keypad should offer other layouts, or
  force 두벌식 regardless of the setting, is OPEN (§7) — the picture fixes the
  form, not the layout-setting interaction.
- **The corner hints are drawn; the gestures are #312.** The digit hints and
  shifted-jamo sublabels are reachable today through the one-shot shift; the
  ,→? / .→" / ⇧→⇪ affordances advertise KP5/KP7 gestures that do not exist yet.
  The picture draws them, so they are drawn (KP13); the gap is recorded here.
- Digits are always drawn (an unmapped digit is the classifier's
  boundary-and-pass, inserting itself exactly as the hardware key does).
- `,` and `.` live in the bottom row in EVERY text mode (KP4/KP9): when the
  layout defines them, the bottom-row key IS that layout key — one key, one
  position, every mode.

## 4. Shift, pages, and the symbol page's return key

- **Shift is one-shot** (#285's minimum): tap arms it, the next CHARACTER key
  consumes it (the tap carries `META_SHIFT_ON`), function keys do not consume
  it, a second tap cancels. No capslock, no double-tap lock — the shift state
  machine beyond one-shot is open (KP7). While armed, keycaps show the layout's
  shift row (ㅃ, Q, sin3-p2's symbol tier): the row is data, the display follows.
- **기호** enters the symbol page from either text mode; the page is the shipped
  `assets/keypad/symbols.json` (KP6's three ruled rows, navigation row, KP9
  bottom row), parsed by `SymbolScreen` (#297). No shift on the page (KP6);
  every shifted variant is its own key, as the data already holds.
- **The symbol page's 한/영 key returns to the original mode** — KP9's words:
  "남은 공간에 원래 모드로 (이걸 한/영이라고 표시해도 헷갈리지 않을 것 같고)". The
  mode at entry is remembered; the return key restores it (a language toggle
  only if the mode drifted while the page was up, e.g. by a hardware 한/영
  press). It is a return, not a toggle; the text page's 한/영 key toggles as
  before.
- Navigation ids (`tab home page-up page-down end left up down right backspace`)
  are wired to their AOSP keycodes through the same `onKeyDown` path; the
  engine declines them (no layout keys them), so they reach the app as
  boundary-then-pass — identical to the hardware keys. The Tier 2 slot is
  absent by KP10/#312, as the data file records.

## 5. Candidate window — unchanged, and the open alternative

The issue names two shapes: `onCreateCandidatesView()` or preedit-only + the
existing Hanja dialog. **This implementation changes nothing here**: the
preedit is presented as before and the Hanja dialog (strip button,
`OverlayActivity`) is reused. That is the status quo, not a ruling — the
candidate-window decision is listed among #285's open decisions and stays the
maintainer's. The measured cost of the alternative, for the ruling:
`onCreateCandidatesView` adds a second platform surface (a candidates view the
service must populate, size and dismiss) and a second presentation of the same
Hanja list the dialog already renders; the dialog path reuses a surface tier E2
already exercises. No engine or ABI change is needed by either.

## 6. Verification

- **Tier E1, `KeypadModelTest`** (no JNI): labels derived from the real layout
  files; every key of dubeolsik / us-qwerty / sin3-p2 reachable exactly once in
  the grid; symbol rows equal the shipped asset; function keycodes equal the
  javap-verified AOSP constants; one-shot shift and page state semantics.
- **Tier E1, `KeypadTouchGoldenTest`** (real engine via the host JNI shim):
  replays `tests/golden/cultural-pangram-us-qwerty.jsonl` — the tier A corpus —
  through BOTH touch surfaces: letters/digits via the text grid (keycap id +
  one-shot shift → keycode + meta → `HunminAdapter.onKey`), punctuation via the
  symbol page's KP11 direct commit (flush first), since KP13's picture has no
  punctuation cells. Per-case totals must equal the golden text. This is the
  issue's golden: the Latin commit path survives touch. A second case pins the
  flush seam: a pending dubeolsik word is delivered BEFORE a symbol-page
  commit, in one order, with no I9 violation.
- **Tier E2, `tools/android-e2e.sh` touch round**: taps the real keycaps on the
  emulator (by content-desc, bounds from the platform's own hierarchy) — an
  English word with a shifted letter and a space, a symbol from the symbol
  page, the return key, and a Hangul word on the default layout — and reads the
  committed text back out of the EditText, the same assertion surface the
  hardware round uses.

## 7. What stays open (not decided here)

- Candidate window shape (§5) — #285 decision, maintainer's.
- The frequent-symbol set beyond `, .` and the number-row default (KP4, mockup
  OPEN items); gesture variants and the Tier 2 mapping (#312, KP10).
- Capslock / shift state machine beyond one-shot (KP7), swipe defences (KP5).
- The keypad ↔ layout-setting interaction: the picture's Hangul screen is
  두벌식, but the service default is sin3-p2 (hardware parity with fcitx5).
  Today the keypad draws the ACTIVE layout in the picture's geometry, so the
  두벌식 screen appears when 두벌식 is chosen (settings → 한글 배열). Whether
  the keypad should force 두벌식, or the default should change, is the
  maintainer's — not decided here.
- Night-mode palette (the picture draws light only), landscape, haptics,
  accessibility polish (#285 sub-issue).
- Long-press backspace = repeated WORD-unit delete at a reasonable interval
  (maintainer request 2026-11-22, verbatim in #312's comment): added to #312's
  scope; the WORD unit of KP14 is the reusable seam. The prior-art premise
  ("다른 ime는 그런 기능을 제공할 거야") is unverified in the survey — a
  design-analysis item there.
- KP3 frequency-based placement/emphasis beyond the provisional colour
  hierarchy — needs the frequency source the symbols design names.

## 8. Visibility — auto-show only without a physical keyboard (KP12)

The maintainer's requirement (2026-11-22, verbatim): "이 UI는 물리 키보드가 없을 때만
자동으로 뜨는 거야." Recorded as `docs/SPEC-ANDROID-KEYPAD.md#§KP12`.

The decision is a pure function, `KeypadVisibility.autoShown(keyboard,
hardKeyboardHidden, showImeWithHardKeyboard)`, so tier E1 pins the whole table
without a device:

- a physical keyboard is PRESENT AND EXPOSED when `keyboard != KEYBOARD_NOKEYS`
  and `hardKeyboardHidden == HARDKEYBOARDHIDDEN_NO`;
- the grid auto-shows exactly when that is false;
- `show_ime_with_hard_keyboard` (the platform's standard user opt-in, read from
  `Settings.Secure`) is an explicit request, not an automatic appearance, and
  shows the grid even with a keyboard exposed.

Constants are javap-verified against android.jar (API 35, 2026-11-22):
`KEYBOARD_NOKEYS = 1`, `HARDKEYBOARDHIDDEN_NO = 1` — NOT 0; guessing these is
the recorded failure mode of the modifier bits in HunminKeyRegistry.

The service applies it in `onCreateInputView` and re-applies on
`onConfigurationChanged` (a keyboard attached or a lid opened mid-session is a
configuration change). **Only the grid's visibility changes** — the strip, and
therefore the IME window hardware keys arrive through (B1), is never hidden:
the requirement protects the hardware path, and hiding the whole input view
would break it. With the grid gone the input view is byte-for-byte the
pre-keypad surface, so the hardware round of tier E2 is unaffected.

Measurement (updated 2026-11-23): the RUNNING `hunmin-e2e` AVD reports
`keyboard=2` (QWERTY) exposed with `show_ime_with_hard_keyboard=1` — so E2E
measures BOTH KP12 paths on it: opt-in 0 hides the grid (strip stays, B1),
opt-in 1 shows it. The earlier "environment-bound, needs hw.keyboard=yes"
note was wrong about this instance — measured, not assumed. A genuine
NOKEYS device (a phone) exercises the auto-show arm; the maintainer's device
confirmed it.

## 9. Measured defects found by the device round (recorded so they are not repeated)

- **The keypad's bottom row sat under the platform's IME navigation bar.**
  The platform draws `input_method_navigation_bar_view` (with a Back button)
  over the bottom of the IME window; measured bounds on the AVD: strip
  `[0,2274][1080,2400]`, Back `[71,2274][254,2400]`, and the `기호` keycap
  `[166,2274][323,2400]` overlapped it. A tap on `기호` was delivered as
  KEYCODE_BACK and HID the IME (`HIDE_SOFT_INPUT_BY_BACK_KEY` in ImeTracker)
  instead of opening the symbol page. No JVM tier can see this: it is the
  platform's window composition, not our logic. Fix: the input view pads itself
  by the system-bar insets (`ViewCompat.setOnApplyWindowInsetsListener` in
  `onCreateInputView`); re-measured, the bottom row ends exactly at the strip's
  top (2274) and the tap opens the page. The E2E touch round is what caught it.
- **The E2E sequence expected a space it never tapped** (`hello W ` after
  typing `hello`, shift, `w`, space). The IME was right; the test was wrong.
  Recorded because the failure read like a shift defect for one run.


## 10. The reported bugs and their fixes (2026-11-22, maintainer device test)

The maintainer tested on the physical device and reported three functional
bugs. Each was reproduced on the AVD with real taps, fixed, and pinned by an
E2E assertion:

1. **한/영 toggled the engine but not the grid.** `toggleHangulEnglish()`
   refreshed the strip only; the keypad never heard the mode change, so the
   labels stayed Hangul while English composed. Fix: `refreshKeypad()` in both
   toggle paths. E2E asserts the q keycap's label reads `q` in English mode.
2. **Backspace did nothing in most scenarios.** MEASURED: the engine correctly
   declined the synthesized DEL (`consumed=false` in logcat) and the platform's
   default `onKeyDown` forwarding DROPPED the synthesized event — hardware
   events carry a keyboard source and flags, ours did not. Fix: a declined
   touch key is forwarded explicitly via `InputConnection.sendKeyEvent`
   (down+up); `passThrough()` suppresses the super fallback during touch
   dispatch so the key lands exactly once. This also fixes every other
   pass-through key (navigation, punctuation the layout declines).
3. **Enter did nothing.** Same forwarding drop, plus the fixture was
   single-line. Fix: the explicit forwarding, and Enter tries
   `sendDefaultEditorAction(true)` first (Done/Go/Send/Next), falling back to
   the key event (a newline in a multi-line editor). The test editor is now
   `textMultiLine` so the newline is assertable.

## 11. KP14 — long swipe-left delete, configurable unit

The maintainer's request (2026-11-22, verbatim in
`docs/SPEC-ANDROID-KEYPAD.md#§KP14`): a long swipe-left from any keycap is
backspace, configurable 키/음절/단어/전체.

- **Gesture**: `KeypadModel.isSwipeLeftDelete(dx, dy, threshold)` — leftward
  distance ≥ threshold (provisional 60 dp ≈ two key widths) AND horizontally
  dominant (≥ 2× the vertical drift). Recognised on MOVE once long enough; the
  tap fires on UP only if no gesture fired, so a key is never typed AND
  deleted. A short drift stays a tap (KP5 protection). Long-press and swipe-up
  remain #312 — the maintainer measured them absent ("long-press도 swipe-up도
  동작하지 않아"), recorded in KP14's entry; the hints KP13 draws advertise
  gestures that are not wired yet.
- **Units** (service `deleteBySwipeUnit`, all through the tap machinery or
  platform actions, no surrounding-text read — I9):
  - KEY: the backspace key's own path.
  - SYLLABLE: jamo-by-jamo DEL against the composing display, STOPPING when
    the last syllable is gone — one DEL too many commits the buffered word and
    deletes into it (measured: ㅎ곈디 → ㅎ곈ㄷ → ㅎ곈, a third DEL eats 곈). The
    jamo cap is NFD of the last syllable; the stop condition is the display
    losing that whole character.
  - WORD: `reset()` drops the buffered word when composing; else Ctrl+DEL to
    the app (its own word-delete).
  - ALL: `reset()` + Ctrl+A + DEL as key events through the same queue.
    MEASURED: `performContextMenuAction(selectAll)` did not select before the
    delete arrived (only the trailing space was deleted); the key-event pair
    orders itself. No length is read from the client (I9).
- **Setting**: settings window → 키패드 section, four choices; stored in
  `hunmin.xml` (`swipe_left_delete`), read by the gesture and shown by the
  window through the same `ImeSettings` bus as the other rows. Default KEY
  (least destructive) — provisional.
- **Verification**: E1 pins the gesture decision table and the unit vocabulary;
  E2E swipes a real keycap per unit (KEY: ㅎ곈디 → ㅎ곈ㄷ; WORD: the composing
  word dropped; ALL: a committed field cleared) and asserts the settings row
  and its four choices.

## 12. KP16 — the hint is a promise about the shift result

The maintainer tried shift+, and shift+. following the drawn hints and got
`<` `>` ("comma 위에 ?, period 위에 "를 적어서 시프트 입력을 해보려고 했는데,
<>가 각각 입력되네?"). Measured cause: an unmapped shifted key passes to the
app, whose US KeyCharacterMap resolves Shift+, as `<`. The ruling: "시프트를
사용해도 동일한 결과가 나와야지." — shift on the frequent keys yields the
hinted alternate (? "), committed through the symbol path (flush first), in
every mode and layout; digits and the hardware path unchanged.

Display rule (endorsed the same day): while shift is armed the keycaps SHOW
the alternate they will type — ? " on the frequent keys, ! @ # … on digits,
ㅃ on jamo (already the engine's shift row). The corner hint folds into the
label while armed. One bug found by the E2E assertion: the armed keycap's
`hint` is null (folded), so the alternate decision must read a constant map
(`FREQUENT_HINTS`), not the keycap's current hint — fixed and pinned.

Placement: a top-corner move of the jamo sublabel was applied and REVERTED the
same day — "현행 유지": the sublabel stays bottom-left as the picture draws.

The no-overlap condition ("글자가 겹치지만 않는다면") is MEASURED, not assumed:
the main label is WRAP_CONTENT-centered so its hierarchy bounds are the text
box, and `tools/keypad-overlap-check.py` asserts pairwise disjoint text bounds
per keycap, run by E2E in both plain and armed states. It caught a real overlap
(digit `2` vs hint `@` at the mockup's 11px hint size); the corner texts are
10sp/9sp with tightened margins — a recorded deviation from the picture's
11px/10px, forced by the condition. 44 keycaps, 13 text pairs plain / 1 armed,
no overlap.

Harness hardening from measured flakes: `mode_is` retries until the IME window
is in the dump (a surface restore can lag it); `field_text` handles
uiautomator's single-quoted text attribute (it switches delimiter when the
value contains `"` — measured with `?"`).

## 13. KP17 — two independent surfaces

The maintainer's rulings (2026-11-22/23, verbatim in
`docs/SPEC-ANDROID-KEYPAD.md#§KP17`): the soft keyboard uses 두벌식/qwerty only
("소프트 키보드는 두벌식만 쓸건데?", "일단은"), the two implementations must
have no relation ("소프트 키보드와 물리 키보드 구현은 전혀 관계가 없어야
하는데..."), and the reason: their UX characteristics are OPPOSITE — hardware
has no gestures but easy repeat, soft the reverse — so each is optimized for
its own ("이런 서로 다른 특성을 최대한 맞추어서 최적화해야 해").

Architecture:

- **Soft surface**: its own `HunminAdapter` pair (`touchEngine`), built per
  editor from the fixed constants `SOFT_HANGUL_LAYOUT=dubeolsik` /
  `SOFT_LATIN_LAYOUT=us-qwerty`. `dispatchTouchKey` is its own path: secure
  editor pass-through, its own space boundary + judgement adoption, explicit
  forwarding of declined keys (the platform drops synthesized events —
  measured). It never enters `onKeyDown` and never reads `layoutId`.
- **Hardware surface**: unchanged — `onKeyDown`, the configured layouts, the
  settings window's choice. The E2E hardware round still measures sin3-p2
  (ㅎ곈디) on the same AVD where the touch round measures 두벌식 (안녕).
- **Shared, and only this**: the editor (InputConnection), the user's single
  mode indication (한/영 toggles both pairs' language), and editor hygiene at
  a surface switch — the other pair's word in flight is delivered before the
  active surface composes, because the editor has ONE composing region. That
  is text order in the shared editor, not a coupling of layouts or state.
- KP12 visibility is presentation only; no engine rebuild on surface change.

Overturned drafts, recorded: (1) "touch is hardware" — one shared onKeyDown
path; (2) the visibility-tied layout switch with engine rebuilds. Both coupled
the surfaces; the rulings rejected them. The first draft of THIS section's
mechanism also tied layout to visibility — superseded before shipping.

The 두벌식/qwerty-only scope is provisional ("일단은"); expansion is a data
and feature change on the soft surface, not a rewrite of the hardware path.
