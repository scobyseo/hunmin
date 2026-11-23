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
            for (g in GestureKind.entries) {
                assertEquals(
                    "$kind/$g", GestureAction.SHIFTED_KEY,
                    KeypadModel.gestureAction(kind, "a", g),
                )
            }
        }
    }

    @Test
    fun theRecordedExceptionsArePerKeyFacts() {
        // 기호 mode key: both gestures open the picker (KP10).
        for (g in GestureKind.entries) {
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
        // shift→capslock: the shift state machine is open (KP7).
        for (g in GestureKind.entries) {
            assertEquals(GestureAction.NONE, KeypadModel.gestureAction(KeypadKey.Kind.SHIFT, "shift", g))
            // Symbol-screen character keys: no shift meaning (KP6), open.
            assertEquals(GestureAction.NONE, KeypadModel.gestureAction(KeypadKey.Kind.SYMBOL, ".", g))
            // 한/영 and the other function keys: no stated gesture meaning.
            assertEquals(GestureAction.NONE, KeypadModel.gestureAction(KeypadKey.Kind.MODE, "language-toggle", g))
            assertEquals(GestureAction.NONE, KeypadModel.gestureAction(KeypadKey.Kind.FUNCTION, "space", g))
            assertEquals(GestureAction.NONE, KeypadModel.gestureAction(KeypadKey.Kind.FUNCTION, "enter", g))
        }
    }

    @Test
    fun wordRepeatIntervalIsTheMaintainersEstimate() {
        // SPEC KP14 (2026-11-23): "0.2초 정도 간격" — provisional, pinned so a
        // tuning change is a deliberate edit with the SPEC moved too.
        assertEquals(200L, KeypadModel.WORD_REPEAT_INTERVAL_MS)
    }
}