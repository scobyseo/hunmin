---
title: "T3-3: the soft keyboard UI, and the mistakes the measurements caught"
created: 2026-11-22
updated: 2026-11-22
ingested: 2026-11-22
type: session
status: active
tags: [android, keypad, tests, kp12]
sources:
  - ../../../docs/SPEC-ANDROID-KEYPAD.md
  - ../../../docs/DESIGN-ANDROID-KEYPAD.md
  - ../../../docs/REQ-MAP.md
  - https://github.com/scobyseo/hunmin/issues/299
  - https://github.com/scobyseo/hunmin/issues/285
confidence: high
---

# T3-3: the soft keyboard UI, and the mistakes the measurements caught

Issue #299: draw the soft keypad, wire the symbol page, and prove the Latin
commit path survives touch. The governing rulings were already in place —
KP11 (the symbol screen is app-read data, engine uninvolved,
[[2026-11-22-t3-2-symbol-data-and-issue-alignment]]) and #285's scope (두벌식 +
QWERTY grids, gestures are #312). Mid-task the maintainer added one more
(2026-11-22, verbatim):

> "이 UI는 물리 키보드가 없을 때만 자동으로 뜨는 거야."

Recorded as KP12 in `docs/SPEC-ANDROID-KEYPAD.md`: the grid auto-shows only
without a physical keyboard; the strip never hides, because it is the IME
window hardware keys arrive through (the B1 fact tier E2 measures). The
platform's `show_ime_with_hard_keyboard` opt-in is honoured as an explicit
request, not an automatic appearance.

## What was built

The design (`docs/DESIGN-ANDROID-KEYPAD.md`) splits the keypad into three
seams: `KeypadModel.kt` (pure Kotlin — labels from the layout's own JSON, one
generic physical-position grid, one-shot shift, page memory, KP12's decision
table), `KeypadView.kt` (pixels and tap dispatch only), and four service
callbacks into paths the hardware side already uses. **Touch is hardware**: a
keycap tap synthesizes the same `(keyCode, metaState)` and enters the same
`onKeyDown`, so the space boundary, judgement adoption, secure-editor guard
and key capture apply unchanged. The symbol page is the KP11 exception: flush
the pending word, then `SymbolScreen.commitSymbol`.

No engine change, no ABI symbol, no per-layout code: sin3-p2 (the default) is
fully touch-reachable through the same generic grid as dubeolsik.

## What the measurements caught (the point of this page)

Every one of these was a confident first draft that a measurement refuted:

1. **NFD does not decompose compatibility jamo.** The display map was first
   built by NFD-decomposing U+3131–U+3163; the decomposition is `<compat>`,
   which only NFKD applies, so the table would have been EMPTY and every
   Hangul keycap blank. Caught by a python probe before shipping.
2. **Jongseong has no decomposition at all.** ㄱ decomposes to the *choseong*
   ᄀ, not the jongseong ᆨ. The fix matches Unicode letter names
   (`HANGUL JONGSEONG GIEOK` ↔ `HANGUL LETTER GIEOK`) — still derived, not
   transcribed.
3. **sin3-p2's keys are not all strings.** 207 dict values across the tree
   (the `case` primitive); a `getString` parse threw, which would have left
   the DEFAULT layout's grid empty. Keycaps now show the default (`else`)
   branch — what the key types at rest.
4. **Word buffering means the client sees nothing mid-word.** The flush-seam
   test first expected the raw engine trace's per-key commits (`안`); the
   buffered layer holds them and delivers `안녕` once at the boundary — the
   established invariant-2 fact, re-learned by failing.
5. **The golden corpus encodes shift two ways**: uppercase letters as the
   shifted character (`"T"`), digits/punctuation as base + `shift:true`.
   Reading only `ch` typed `1234567890` for `!@#$%^&*()`.
6. **The keypad's bottom row sat under the platform's IME navigation bar.**
   On the AVD, a tap on `기호` landed on the strip's Back button
   (`input_method_navigation_bar_view`, measured bounds): KEYCODE_BACK hid the
   IME instead of opening the symbol page. Fixed by padding the input view by
   the system-bar insets; the bottom row now ends exactly at the strip's top.
   This is the class of defect only a real tap on a real window can find —
   [[ci-android-lessons]] is the same lesson one tier up.
7. **The E2E sequence itself was wrong once**: it expected `hello W ` without
   tapping the space between the words. The IME was right; the test was not.

## Verification (measured 2026-11-22)

- `engine-android-e1`: 186 tests, 0 failed, 0 skipped — including
  `KeypadModelTest` (20: labels, reachability of all 47 keys of each goal
  layout, keycodes, state, KP12's table) and `KeypadTouchGoldenTest` (2: the
  tier A us-qwerty corpus replayed through the touch decision into the real
  engine; the flush seam).
- `engine-android-e2e` on the `hunmin-e2e` AVD (`hw.keyboard=no`), scoped to
  the emulator with `ANDROID_SERIAL` because a physical phone was attached:
  hardware round unchanged (`ㅎ곈디`), then real taps — KP12 auto-show,
  `hello W ` (one-shot shift + boundary), `hello W !` (symbol page), KP9
  return to the original mode, and `ㅎ곈디` again by touch on sin3-p2.
- The hardware-present HIDE path is pinned by E1's decision table only;
  asserting it on a screen needs an `hw.keyboard=yes` device —
  environment-bound, named in the design doc and the map, not smoothed over.

## What stays open

The candidate window shape (#285's decision — this task changed nothing:
preedit + the existing Hanja dialog), the frequent-symbol set beyond `, .`,
gesture variants and Tier 2 (#312), capslock beyond one-shot (KP7), geometry
sizes and landscape. None of these was decided here; the design doc lists
them with the evidence that would move them.

## The mockup was the decided UI (KP13) — the deviation and its overturning

The maintainer tested the first build and rejected it: "T3-1에서 정한 UI가 아닌데?",
"docs/android-keypad-mockup.html 로 정했잖아?", "왜 그 mockup 대로 구현하지 않았지?"
The mockup picture IS the decided UI (recorded as KP13). The first implementation
had read the mockup document's "provisional, not a ruling" header as permission
and built a generic physical-position grid with extension cells and the HUD
palette instead. The cause is the lesson: when a document's status and the
maintainer's intent disagree, ASK — do not pick. The grid, palette, hints and
sublabels were rebuilt to the picture; the deviation is recorded in
[[2026-11-22-t3-3-soft-keyboard-ui]]'s design doc (§3) and SPEC KP13.

## The device test found three functional bugs — and requested KP14

The maintainer's phone test (2026-11-22): 한/영 changed the engine but not the
grid; backspace did nothing in most scenarios; enter did nothing. All three
reproduced on the AVD with real taps:

1. the toggle refreshed the strip only — `refreshKeypad()` added to both paths;
2. the platform's default onKeyDown forwarding DROPS synthesized key events
   (hardware events carry a keyboard source; ours did not) — declined touch
   keys are now forwarded explicitly via InputConnection.sendKeyEvent, with
   the super fallback suppressed during touch dispatch so nothing lands twice;
3. same drop, plus Enter needs `sendDefaultEditorAction` first — and the test
   editor was single-line, now textMultiLine so the newline is assertable.

Each fix carries an E2E regression assertion. The same session requested KP14
(verbatim in SPEC): a long swipe-left from any key is backspace, configurable
키/음절/단어/전체 — implemented with a pure gesture decision, four delete units
through the tap machinery (no I9 read), and a 키패드 settings section. The
maintainer also measured that long-press and swipe-up do nothing — true, they
are #312, and KP13 draws their hints; the gap is recorded, not hidden.
