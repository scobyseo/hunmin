/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1, the #312 gesture decision lane (KP7/KP14, KP5's gates).
 *
 * The pure half of the gesture layer: the swipe-up gate, the return-to-cancel
 * radius, the per-key exception table and the word-repeat interval. The view
 * owns the pointer timing; the device round (E2E) asserts the wired behavior.
 * The NONE cells of the table are OPEN questions pinned as open — a change
 * that silently gives them a meaning fails here until the SPEC says so.
 */
package org.hunmin.ime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class KeypadGestureTest {

    private val t = 60f   // the provisional threshold, as the view passes it

    @Test
    fun swipeUpGateIsDistanceAndDirection() {
        // Straight up past the threshold fires.
        assertTrue(KeypadModel.isSwipeUp(0f, -t, t))
        assertTrue(KeypadModel.isSwipeUp(0f, -t * 2, t))
        // Below the threshold: still a tap (KP5).
        assertFalse(KeypadModel.isSwipeUp(0f, -(t - 1), t))
        // Up-dominant diagonal fires; sideways-dominant does not.
        assertTrue(KeypadModel.isSwipeUp(-30f, -t, t))
        assertFalse(KeypadModel.isSwipeUp(-31f, -t, t))
        assertFalse(KeypadModel.isSwipeUp(t, -t, t))
        // Down and pure sideways are not swipe-up.
        assertFalse(KeypadModel.isSwipeUp(0f, t, t))
        assertFalse(KeypadModel.isSwipeUp(-t, 0f, t))
        // The mirror of KP14's left gate: same shape, rotated.
        assertTrue(KeypadModel.isSwipeLeftDelete(-t, 0f, t))
        assertFalse(KeypadModel.isSwipeUp(-t, 0f, t))
    }

    @Test
    fun returnToOriginCancelsWithinTheRadius() {
        val r = 12f
        assertTrue(KeypadModel.returnedToOrigin(0f, 0f, r))
        assertTrue(KeypadModel.returnedToOrigin(r, -r, r))
        assertFalse(KeypadModel.returnedToOrigin(r + 1, 0f, r))
        assertFalse(KeypadModel.returnedToOrigin(0f, -(r + 1), r))
    }

    @Test
    fun theGeneralRuleIsEquivalenceOnCharacterKeys() {
        for (kind in listOf(KeypadKey.Kind.CHARACTER, KeypadKey.Kind.DIGIT)) {
            for (g in listOf(GestureKind.SWIPE_UP, GestureKind.LONG_PRESS)) {
                assertEquals(
                    "$kind/$g", GestureAction.SHIFTED_KEY,
                    KeypadModel.gestureAction(kind, "a", g),
                )
            }
            // Swipe-down on text keys is UNRULED: a NONE cell, not a copy of up.
            assertEquals(
                GestureAction.NONE,
                KeypadModel.gestureAction(kind, "a", GestureKind.SWIPE_DOWN),
            )
        }
    }

    @Test
    fun shiftGesturesLockCapslockAndSymbolGesturesCycleSets() {
        // KP7 (decided 2026-11-23): shift's gestures -> capslock.
        for (g in listOf(GestureKind.SWIPE_UP, GestureKind.LONG_PRESS)) {
            assertEquals(GestureAction.CAPSLOCK,
                KeypadModel.gestureAction(KeypadKey.Kind.SHIFT, "shift", g))
        }
        assertEquals(GestureAction.NONE,
            KeypadModel.gestureAction(KeypadKey.Kind.SHIFT, "shift", GestureKind.SWIPE_DOWN))
        // KP10 (decided 2026-11-23): symbol-screen up/down cycle the sets;
        // long-press follows the equivalence (next set).
        assertEquals(GestureAction.NEXT_SYMBOL_SET,
            KeypadModel.gestureAction(KeypadKey.Kind.SYMBOL, ".", GestureKind.SWIPE_UP))
        assertEquals(GestureAction.PREV_SYMBOL_SET,
            KeypadModel.gestureAction(KeypadKey.Kind.SYMBOL, ".", GestureKind.SWIPE_DOWN))
        assertEquals(GestureAction.NEXT_SYMBOL_SET,
            KeypadModel.gestureAction(KeypadKey.Kind.SYMBOL, ".", GestureKind.LONG_PRESS))
        // The 기호 mode key's swipe-down is unruled.
        assertEquals(GestureAction.NONE,
            KeypadModel.gestureAction(KeypadKey.Kind.SYMBOLS_PAGE, "symbols-page", GestureKind.SWIPE_DOWN))
    }

    @Test
    fun swipeDownGateIsTheMirrorOfSwipeUp() {
        assertTrue(KeypadModel.isSwipeDown(0f, t, t))
        assertFalse(KeypadModel.isSwipeDown(0f, t - 1, t))
        assertTrue(KeypadModel.isSwipeDown(30f, t, t))
        assertFalse(KeypadModel.isSwipeDown(31f, t, t))
        assertFalse(KeypadModel.isSwipeDown(0f, -t, t))
    }

    @Test
    fun capslockHoldsTheBitUntilATapReleasesIt() {
        val s = KeypadState()
        s.capsLock()
        assertTrue(s.capsLocked)
        // The bit holds across keys (not consumed like the one-shot).
        assertEquals(KeypadModel.META_SHIFT_ON, s.shiftMeta())
        assertEquals(KeypadModel.META_SHIFT_ON, s.shiftMeta())
        assertTrue(s.shiftActive)
        // "다시 shift를 누르면 해제": a tap while locked releases.
        s.tapShift()
        assertTrue(!s.capsLocked)
        assertEquals(0, s.shiftMeta())
        // A tap while unlocked is still the one-shot (KP2).
        s.tapShift()
        assertTrue(s.shiftArmed)
        assertEquals(KeypadModel.META_SHIFT_ON, s.shiftMeta())
        assertEquals(0, s.shiftMeta())
        // Page entry and mode changes clear the lock with the one-shot.
        s.capsLock()
        s.enterSymbols(hangulMode = true)
        assertTrue(!s.capsLocked)
    }

    @Test
    fun symbolSetCyclingWrapsOverThePagesThatHaveData() {
        val two = listOf(KeypadState.Page.SYMBOLS, KeypadState.Page.EXTENDED)
        val s = KeypadState()
        s.enterSymbols(hangulMode = true)
        s.cycleSymbolSet(forward = true, available = two)
        assertEquals(KeypadState.Page.EXTENDED, s.page)
        s.cycleSymbolSet(forward = true, available = two)   // wraps
        assertEquals(KeypadState.Page.SYMBOLS, s.page)
        s.cycleSymbolSet(forward = false, available = two)  // back wraps
        assertEquals(KeypadState.Page.EXTENDED, s.page)
        // One set only: a no-op, not a jump to text.
        s.cycleSymbolSet(forward = true, available = listOf(KeypadState.Page.SYMBOLS))
        assertEquals(KeypadState.Page.EXTENDED, s.page)
        // From TEXT the cycle does nothing (the gesture lives on symbol keys).
        s.returnToText()
        s.cycleSymbolSet(forward = true, available = two)
        assertEquals(KeypadState.Page.TEXT, s.page)
        // All five sets cycle in picker order (#312 provisional pages).
        val all = listOf(KeypadState.Page.SYMBOLS, KeypadState.Page.EXTENDED,
            KeypadState.Page.MATH, KeypadState.Page.EMOJI, KeypadState.Page.KAOMOJI)
        s.enterSymbols(hangulMode = true)
        for (expected in listOf(KeypadState.Page.EXTENDED, KeypadState.Page.MATH,
            KeypadState.Page.EMOJI, KeypadState.Page.KAOMOJI, KeypadState.Page.SYMBOLS)) {
            s.cycleSymbolSet(forward = true, available = all)
            assertEquals(expected, s.page)
        }
    }

    @Test
    fun theRecordedExceptionsArePerKeyFacts() {
        // 기호 mode key: swipe-up/long-press open the picker (KP10).
        for (g in listOf(GestureKind.SWIPE_UP, GestureKind.LONG_PRESS)) {
            assertEquals(
                GestureAction.SYMBOL_PICKER,
                KeypadModel.gestureAction(KeypadKey.Kind.SYMBOLS_PAGE, "symbols-page", g),
            )
        }
        // Backspace: long-press repeats WORD deletion (KP14); swipe-up is an
        // OPEN cell — the ruling names long-press, not the pair.
        assertEquals(
            GestureAction.WORD_DELETE_REPEAT,
            KeypadModel.gestureAction(KeypadKey.Kind.FUNCTION, "backspace", GestureKind.LONG_PRESS),
        )
        assertEquals(
            GestureAction.NONE,
            KeypadModel.gestureAction(KeypadKey.Kind.FUNCTION, "backspace", GestureKind.SWIPE_UP),
        )
    }

    @Test
    fun openCellsStayOpenUntilTheSpecSaysOtherwise() {
        for (g in GestureKind.entries) {
            // 한/영 and the other function keys: no stated gesture meaning.
            assertEquals(GestureAction.NONE, KeypadModel.gestureAction(KeypadKey.Kind.MODE, "language-toggle", g))
            assertEquals(GestureAction.NONE, KeypadModel.gestureAction(KeypadKey.Kind.FUNCTION, "space", g))
            assertEquals(GestureAction.NONE, KeypadModel.gestureAction(KeypadKey.Kind.FUNCTION, "enter", g))
        }
    }

    @Test
    fun theDigitAlternateIsTheDrawnHintInBothModes() {
        // MEASURED 2026-11-23: in Hangul mode the soft 두벌식 pair has no digit
        // keys, so the engine path dropped the shift bit and shift+1 typed '1'
        // — the corner hint promises '!'. The alternate is the hint (KP16's
        // rule), derived from the registry, not transcribed.
        val caps = KeypadModel.textRows(
            KeypadKeycaps.parse(java.io.File(
                System.getenv("HUNMIN_REPO_ROOT")?.let { java.io.File(it) } ?: java.io.File("../..").canonicalFile,
                "libhunmin/data/layouts/us-qwerty.json").readText()), shift = false)
        val digit = caps.flatten().first { it.kind == KeypadKey.Kind.DIGIT && it.id == "1" }
        assertEquals("!", KeypadModel.shiftedDigitAlternate(digit))
        val letter = caps.flatten().first { it.kind == KeypadKey.Kind.CHARACTER }
        assertEquals(null, KeypadModel.shiftedDigitAlternate(letter))
    }

    @Test
    fun wordRepeatIntervalIsTheMaintainersEstimate() {
        // SPEC KP14 (2026-11-23): "0.2초 정도 간격" — provisional, pinned so a
        // tuning change is a deliberate edit with the SPEC moved too.
        assertEquals(200L, KeypadModel.WORD_REPEAT_INTERVAL_MS)
    }

    @Test
    fun longPressTimeoutIsTheProvisionalTuningValue() {
        // Maintainer 2026-11-26: "long-press가 너무 긴데. 200ms 이하로 테스트해보고
        // 싶어" — the system value (500 ms phone / 400 ms AVD) was too slow.
        // Pinned so a tuning change is a deliberate edit with the record moved.
        assertEquals(200L, KeypadModel.LONG_PRESS_TIMEOUT_MS)
        // Configurable (maintainer 2026-11-26): the default is among the
        // settings window's choices, and the "200ms 이하" experiment is covered.
        assertTrue(KeypadModel.LONG_PRESS_TIMEOUT_MS in KeypadModel.LONG_PRESS_CHOICES_MS)
        assertTrue(KeypadModel.LONG_PRESS_CHOICES_MS.any { it < 200L })
    }
}