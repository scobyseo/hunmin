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

## 3. Geometry — provisional, and generic by construction

The physical US positions are the geometry (KP1's physical-position criterion);
a cell is drawn when the active layout defines that key. Consequences:

- **두벌식** draws exactly the mockup's Hangul screen: 10/9/7 letter keys, no
  punctuation cells (the layout defines none). **QWERTY** draws its extra
  literal keys (`[ ] \ ; ' - = ` `` ` ``) at their physical positions, so every
  one of its 47 keys is touch-reachable. **sin3-p2**, the default, is also fully
  reachable for the same reason — the scope boundary excludes a 세벌식
  *ergonomics block*, not the ability to type the default layout by touch.
- **Digits are always drawn** (number row), even for dubeolsik which defines no
  digit keys: an unmapped key is the classifier's boundary-and-pass, so a digit
  tap inserts the digit exactly as the hardware key does. KP1's digit↔symbol
  *pairing* (long-press/swipe-up, KP5) is #312; the digits themselves are not.
- **`,` and `.` live in the bottom row in every text mode** (KP4's
  default-visible frequent symbols; the mockup's bottom row). They are never
  duplicated at their physical row-3 position: when the layout defines them
  (sin3-p2, us-qwerty) the bottom-row key IS that layout key, fed through the
  engine path like any character key. One key, one position, every mode — KP9's
  positional consistency. The exact frequent-symbol set beyond `, .` remains
  open (symbols.json `x-notes.frequent-keys`); `?` and `"` are on the symbol
  page meanwhile.
- **Functional keys** follow KP2: one shift (row 3 left), backspace (row 3
  right), enter (bottom row right), space (bottom row middle) — no two of
  space/enter/backspace adjacent, none at physical size or position. The bottom
  row reads `[한/영] [기호] [space] , . [⏎]`, the mockup's order.
- **Visual hierarchy (KP3, provisional):** character keycaps use the primary
  text colour, functional keycaps the secondary one — the endorsed
  문자는 진하게, 편집키/숫자는 연하게 direction, using the existing HudTheme tokens.
  Frequency data and emphasis beyond this are not spent here.

Row density for layouts with punctuation cells (13 keys on the top row at
360 dp ≈ 27 dp/key) is the recorded trade-off of generic reachability; the
in-scope 두벌식 grid is 10/9/7. Geometry sizes are provisional (mockup status).

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
  through the TOUCH mapping (keycap id + one-shot shift → keycode + meta →
  `HunminAdapter.onKey`), asserting per-key commit and preedit. This is the
  issue's golden: the Latin commit path through the engine survives touch.
  A second case pins the flush seam: a pending dubeolsik word is delivered
  BEFORE a symbol-page commit, in one order, with no I9 violation.
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
- Geometry sizes, landscape, haptics, accessibility polish (#285 sub-issue).
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

Measurement boundary: the test AVD `hunmin-e2e` has `hw.keyboard=no`, so E2E
asserts the AUTO-SHOW path and the tap round. The hide path with a keyboard
exposed is pinned by the E1 decision table only; asserting it on a device needs
an AVD/device with `hw.keyboard=yes` — environment-bound, named here rather
than smoothed over.
