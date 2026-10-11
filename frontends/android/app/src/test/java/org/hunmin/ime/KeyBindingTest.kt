/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1: the configurable key bindings, on the JVM with no Android framework.
 *
 * Every KeyEvent constant KeyBinding uses is a compile-time `static final int`,
 * so kotlinc inlines it and these tests read the same numbers android.jar
 * declares — no Robolectric, no device, and no chance of the test passing
 * against a framework that is not the one that ships.
 */
package org.hunmin.ime

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** In-memory store, the shape RecordingInputConnection uses for the same reason. */
private class MapStore : KeyBindingStore {
    val values = mutableMapOf<String, String>()
    override fun read(key: String): String? = values[key]
    override fun write(key: String, value: String) {
        values[key] = value
    }
}

class KeyBindingTest {

    // --- the modifier constants are the framework's, not invented -------------

    @Test
    fun `modifier bits equal android jar's`() {
        // KeyEvent's constants are compile-time `static final int`, so this
        // compares android.jar's literal against mine rather than a tautology.
        // If it fails, the inlined constants drifted from the platform and every
        // stored binding would stop matching real events.
        assertEquals(KeyEvent.META_SHIFT_ON, KeyBinding.META_SHIFT)
        assertEquals(KeyEvent.META_ALT_LEFT_ON, KeyBinding.META_ALT_LEFT)
        assertEquals(KeyEvent.META_ALT_RIGHT_ON, KeyBinding.META_ALT_RIGHT)
    }

    // --- normalisation -------------------------------------------------------

    @Test
    fun `aggregate alt bit expands to both sides`() {
        // What adb and some IMEs inject: META_ALT_ON with no side.
        val m = KeyBinding.normalise(KeyEvent.META_ALT_ON)
        assertEquals(KeyBinding.META_ALT_LEFT or KeyBinding.META_ALT_RIGHT, m)
    }

    @Test
    fun `side bit survives and the aggregate is dropped`() {
        // What a real chord delivers: both bits set.
        val m = KeyBinding.normalise(KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON)
        assertEquals(KeyBinding.META_ALT_LEFT, m)
    }

    @Test
    fun `left and right alt are different bindings`() {
        val left = KeyBinding.normalise(KeyEvent.META_ALT_LEFT_ON)
        val right = KeyBinding.normalise(KeyEvent.META_ALT_RIGHT_ON)
        assertTrue("Left Alt and Right Alt must not normalise alike", left != right)
    }

    @Test
    fun `caps lock is not part of a chord`() {
        val m = KeyBinding.normalise(KeyEvent.META_ALT_LEFT_ON or KeyEvent.META_CAPS_LOCK_ON)
        assertEquals(KeyBinding.META_ALT_LEFT, m)
    }

    // --- encode / decode -----------------------------------------------------

    @Test
    fun `encode decode round trip`() {
        val stored = KeyBinding.encode(KeyEvent.KEYCODE_X, KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON)
        assertEquals(KeyCombo(KeyEvent.KEYCODE_X, KeyBinding.META_ALT_LEFT), KeyBinding.decode(stored))
    }

    @Test
    fun `decode rejects what it does not own`() {
        // The format is versioned precisely so an older or hand-edited value is
        // refused rather than half-understood.
        assertNull(KeyBinding.decode(null))
        assertNull(KeyBinding.decode(""))
        assertNull(KeyBinding.decode("ALT_LEFT+Z"))          // the old name form
        assertNull(KeyBinding.decode("v2:45:16"))             // a future version
        assertNull(KeyBinding.decode("v1:abc:16"))
        assertNull(KeyBinding.decode("v1:45"))
    }

    // --- matching ------------------------------------------------------------

    @Test
    fun `matches the chord it was captured from`() {
        val stored = KeyBinding.encode(KeyEvent.KEYCODE_X, KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON)
        assertTrue(KeyBinding.matches(stored, KeyEvent.KEYCODE_X, KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON))
    }

    @Test
    fun `does not match the same key with the other alt`() {
        val stored = KeyBinding.encode(KeyEvent.KEYCODE_Z, KeyEvent.META_ALT_LEFT_ON)
        assertFalse(KeyBinding.matches(stored, KeyEvent.KEYCODE_Z, KeyEvent.META_ALT_RIGHT_ON))
    }

    @Test
    fun `an aggregate-only event still matches a side-specific binding`() {
        val stored = KeyBinding.encode(KeyEvent.KEYCODE_Z, KeyEvent.META_ALT_LEFT_ON)
        assertTrue(KeyBinding.matches(stored, KeyEvent.KEYCODE_Z, KeyEvent.META_ALT_ON))
    }

    @Test
    fun `an absent binding never matches`() {
        // Otherwise a settings row with nothing stored would swallow a keystroke.
        assertFalse(KeyBinding.matches(null, KeyEvent.KEYCODE_Z, 0))
        assertFalse(KeyBinding.matches("garbage", KeyEvent.KEYCODE_Z, 0))
    }

    // --- the round trip the settings window used to get wrong -----------------

    @Test
    fun `a saved binding is readable by the action it was saved for`() {
        // Regression, issue #12: the window wrote `key_한자 입력` while it read
        // `key_hanja`, so nothing it stored was ever read again.
        val store = MapStore()
        val combo = KeyCombo(KeyEvent.KEYCODE_F9, 0)
        KeyBindings.set(store, KeyAction.HANJA, combo)

        assertEquals(combo, KeyBindings.stored(store, KeyAction.HANJA))
        assertTrue(KeyBindings.matches(store, KeyAction.HANJA, KeyEvent.KEYCODE_F9, 0))
        // ...and only that action changed.
        assertNull(KeyBindings.stored(store, KeyAction.VOICE))
    }

    @Test
    fun `each action keeps its own slot`() {
        val store = MapStore()
        KeyBindings.set(store, KeyAction.HANJA, KeyCombo(KeyEvent.KEYCODE_F9, 0))
        KeyBindings.set(store, KeyAction.VOICE, KeyCombo(KeyEvent.KEYCODE_F10, 0))
        assertEquals(2, store.values.size)
        assertEquals(KeyCombo(KeyEvent.KEYCODE_F9, 0), KeyBindings.stored(store, KeyAction.HANJA))
        assertEquals(KeyCombo(KeyEvent.KEYCODE_F10, 0), KeyBindings.stored(store, KeyAction.VOICE))
    }

    @Test
    fun `a stored binding replaces the default`() {
        val store = MapStore()
        assertTrue("default Left Alt + X opens Hanja", KeyBindings.matches(store, KeyAction.HANJA, KeyEvent.KEYCODE_X, KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON))
        KeyBindings.set(store, KeyAction.HANJA, KeyCombo(KeyEvent.KEYCODE_F9, 0))
        assertFalse("after rebinding, the old chord must not fire", KeyBindings.matches(store, KeyAction.HANJA, KeyEvent.KEYCODE_X, KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON))
        assertTrue(KeyBindings.matches(store, KeyAction.HANJA, KeyEvent.KEYCODE_F9, 0))
    }

    // --- defaults are HEAD's behaviour, unchanged ----------------------------

    @Test
    fun `defaults match what the service accepted before the window existed`() {
        val store = MapStore()
        val altLeft = KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON
        val altRight = KeyEvent.META_ALT_ON or KeyEvent.META_ALT_RIGHT_ON

        assertTrue(KeyBindings.matches(store, KeyAction.HANGUL_ENG, KeyEvent.KEYCODE_Z, altLeft))
        assertTrue(KeyBindings.matches(store, KeyAction.HANJA, KeyEvent.KEYCODE_X, altLeft))
        assertTrue(KeyBindings.matches(store, KeyAction.VOICE, KeyEvent.KEYCODE_V, altLeft))
        assertTrue(KeyBindings.matches(store, KeyAction.VOICE, KeyEvent.KEYCODE_V, altRight))
        assertTrue(KeyBindings.matches(store, KeyAction.AUTO_TOGGLE, KeyEvent.KEYCODE_ENTER, altRight))
    }

    @Test
    fun `platform alternates survive a rebinding`() {
        // A user who rebinds 한/영 to F9 must not end up with a keyboard whose
        // physical 한/영 key (bare Right Alt, LANGUAGE_SWITCH, Samsung 218) is dead.
        val store = MapStore()
        KeyBindings.set(store, KeyAction.HANGUL_ENG, KeyCombo(KeyEvent.KEYCODE_F9, 0))
        assertTrue(KeyBindings.matches(store, KeyAction.HANGUL_ENG, KeyEvent.KEYCODE_ALT_RIGHT, 0))
        assertTrue(KeyBindings.matches(store, KeyAction.HANGUL_ENG, KeyEvent.KEYCODE_LANGUAGE_SWITCH, 0))
        assertTrue(KeyBindings.matches(store, KeyAction.HANGUL_ENG, 218, 0))
        assertTrue(KeyBindings.matches(store, KeyAction.HANJA, KeyEvent.KEYCODE_ALT_RIGHT, KeyEvent.META_SHIFT_ON))
    }

    @Test
    fun `a bare modifier binding matches its own key press`() {
        // Pressing Right Alt arrives as KEYCODE_ALT_RIGHT with an empty
        // metaState; storing the key rather than the bit is what makes that match.
        assertTrue(KeyBinding.matches(KeyCombo(KeyEvent.KEYCODE_ALT_RIGHT).encode(), KeyEvent.KEYCODE_ALT_RIGHT, 0))
    }

    @Test
    fun `modifier keys are recognised so a chord is not captured as its modifier`() {
        // Holding Left Alt delivers KEYCODE_ALT_LEFT first; capturing that would
        // bind 한/영 to a bare Left Alt instead of Left Alt + X.
        assertTrue(KeyBinding.isModifierKey(KeyEvent.KEYCODE_ALT_LEFT))
        assertTrue(KeyBinding.isModifierKey(KeyEvent.KEYCODE_SHIFT_RIGHT))
        assertFalse(KeyBinding.isModifierKey(KeyEvent.KEYCODE_X))
        assertFalse(KeyBinding.isModifierKey(KeyEvent.KEYCODE_F9))
    }
}