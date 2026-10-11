---
title: The key settings window, and the two things above it that were broken
created: 2026-10-11
updated: 2026-10-11
type: measurement
status: active
ingested: 2026-10-11
tags: [android, ui, tier-e2, measurements]
sources:
  - ../../../docs/REMAINING-WORK.md
  - ../../../tools/android-e2e.sh
  - ../../../frontends/android/app/src/main/java/org/hunmin/ime/KeyBinding.kt
confidence: high
---

# The key settings window, and the two things above it that were broken

The maintainer's report was *"키 입력을 받아 저장하는 기능이 동작하지 않아"* — the key
settings window does not take a key and does not save it. Both halves were true, and
neither was the only defect. Three layers were broken, each hiding the next:

1. **The button that opens the window was not on the screen.** The input view is a
   horizontal `LinearLayout`: label, mode toggle, layout Spinner, mic, Hanja, IME
   switch, settings. Every child was `WRAP_CONTENT`, and the row measured 1140 px on a
   1080 px AVD — so the mic was clipped to 13 px at the right edge, the Hanja button and
   the settings button were laid out past the edge entirely, and neither appeared in the
   accessibility tree at all. Measured from `uiautomator dump --windows`: the IME view
   listed `음성 입력`, `한/영 전환`, `한글 입력 레이아웃 선택` and `Switch input method`,
   and nothing else. Fixing it is one line that was missing — `LayoutParams(0, WRAP_CONTENT, 1f)`
   on the Spinner, plus zeroing its suggested minimum width, because weight only hands
   out *excess* space and the row was not short of excess.
2. **The window could never receive a key, even once it was open.** This is the part
   worth remembering. While hunmin is the current input method and its window is
   showing, a hardware key event is dispatched to the **IME window**, not to the
   activity drawn on top of it. The capture dialog was open, `input keyevent KEYCODE_F9`
   was injected, and the event went to nobody in the dialog: the `TextView`'s
   `setOnKeyListener` never fired (it is not focusable and nothing focused it), the
   dialog's decor view's listener never fired, and `Activity.dispatchKeyEvent` never ran
   either — for a dialog, which is its own window, the Activity is not in the key path.
   The only code that saw the key was `HunminInputMethodService.onKeyDown`. The fix
   follows from the measurement rather than from a focus tweak: the window registers a
   capture session, the service feeds it before its own bindings, and the session is
   cleared on capture, on cancel and on destroy. Belt and braces are still attached to
   the decor view for the case where no IME holds the keys, guarded so they cannot
   capture twice.
3. **The save was a write-only sink**, which is the defect the maintainer named. The
   window wrote `key_한자 입력` (the Korean label) while it read `key_hanja`; nothing in
   the app read `hunmin_keys` at all — the four triggers were hardcoded in
   `onKeyDown`; and cancel stored the literal string `취소됨` — the full list, with
   line citations, is on GitHub issue #12.

## What is stored now, and why it is numbers

`v1:<keyCode>:<modifiers>`. The previous format was `"ALT_LEFT+Z"`, built with
`KeyEvent.keyCodeToString` — which returns `KEYCODE_Z`, so the two forms could never be
compared, and which is a *method*, so it is unavailable in the JVM unit tests that tier
E1 runs. A name-based binding needs a keycode↔name table in Kotlin that nothing keeps in
step with android.jar; a numeric one needs neither. The modifier mask is normalised to
the side-specific bits (`META_ALT_LEFT_ON` is 16, `META_ALT_RIGHT_ON` is 32 — measured
with `javap -constants` on android.jar API 35) because four of the defaults differ only
by side. An event that says "Alt" without a side satisfies a binding that named either
one: refusing a chord the user did press is the worse error.

## The measurement, and what it cost to get one

`tools/android-e2e.sh` gained a section that drives the real window: tap the gear,
tap the 한/영 전환 row, inject F9, read `shared_prefs/hunmin_keys.xml` with `run-as`,
re-read the list, dismiss the window, focus a real editor, press F9, and assert the
input view flipped 한 → 영. It cleans up after itself so the tier stays repeatable.

Two things that shaped it, both measured rather than assumed:

- **`input keyevent` cannot send a chord on API 35.** There is no `--meta`; the flags
  are `--longpress`, `--duration`, `--doubletap`, `--async`, `--delay`. That is why the
  section binds a bare F9 rather than an Alt+X: the bare key exercises the whole path,
  and the chord is not what is under test.
- **`OverlayActivity` is not exported**, so `am start -n org.hunmin.ime/.OverlayActivity`
  is refused with `Permission Denial: ... not exported from uid`. That is correct
  security posture and the test goes around it the way a user does — by tapping the gear
  — rather than by exporting the activity to make itself reachable.

## The adjacent finding: N5's Espresso suite never compiled

Trying to add an instrumentation test first, because a dialog's focus behaviour is not
something a JVM test can reach. `frontends/android/app/src/androidTest/` does not compile
at HEAD: six type errors in `HunminEspressoTest.kt` (`KeyEvent(action, code, meta)` does
not exist — the three-int form kotlinc bound was `KeyEvent(long downTime, long eventTime,
int action)`), two unresolved imports (`Espresso.pressKey`, `ViewMatchers.not`), and
`Activity.waitForIdle()`, which is not a method. Beyond compiling, its `setUp` calls
`startService` on an `InputMethodService`, which cannot work — it is a service bound
with `BIND_INPUT_METHOD`.

This is [[the-rules-keep-breaking]]'s shape again, and the board's own guard missed it:
`docs/REMAINING-WORK.md` §7.5 asserts `N5-CLOSED | path-present |
frontends/android/app/src/androidTest | present` — the *directory*. A directory exists
whether or not anything in it compiles, and no tier builds androidTest at all, so nine
tests that have never run were closed by an assertion that could not fail. Recording
that is not the same as fixing it; the scope is the maintainer's.

The narrower lesson for the next tier: a test that cannot be run by any harness is not a
test, and the assertion that "guards" it must be one the tree can contradict. The same
argument applies to [[2026-10-10-d3-case-coverage]] and to the label work in
[[2026-10-10-ctest-labels]], where the guard is a label rather than a path.