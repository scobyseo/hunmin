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
private class MapStore : KeyBindingRepository {
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
        // 음성 is the action that still has a default (Alt + KANA since the
        // 2026-10-31 ruling), so it is the one where "replaces" is a claim that
        // can be wrong.
        val store = MapStore()
        val altKana = KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON
        assertTrue("Alt+KANA is the default 음성 trigger", KeyBindings.matches(store, KeyAction.VOICE, KANA_KEYCODE, altKana))
        KeyBindings.set(store, KeyAction.VOICE, KeyCombo(KeyEvent.KEYCODE_F9, 0))
        assertFalse("after rebinding, the old chord must not fire", KeyBindings.matches(store, KeyAction.VOICE, KANA_KEYCODE, altKana))
        assertTrue(KeyBindings.matches(store, KeyAction.VOICE, KeyEvent.KEYCODE_F9, 0))
    }

    @Test
    fun `an action with no default gains one when set`() {
        val store = MapStore()
        assertTrue(KeyBindings.combos(store, KeyAction.HANGUL_ENG).isEmpty())
        KeyBindings.set(store, KeyAction.HANGUL_ENG, KeyCombo(KeyEvent.KEYCODE_F12, 0))
        assertEquals(listOf(KeyCombo(KeyEvent.KEYCODE_F12, 0)), KeyBindings.combos(store, KeyAction.HANGUL_ENG))
        // The fixed keys survive the binding; they are not defaults.
        assertTrue(KeyBindings.matches(store, KeyAction.HANGUL_ENG, KANA_KEYCODE, 0))
    }

    // --- defaults are HEAD's behaviour, unchanged ----------------------------

    @Test
    fun `with nothing stored the fixed keys answer and the old chord defaults do not`() {
        // 한/영 has no default (a physical key on a Korean keyboard) and 한자 has
        // EISU as its platform default since the 2026-10-31 ruling. Alt+Z and
        // Alt+X are NOT triggers any more -- that is a behaviour change and this
        // test is where it is pinned down rather than discovered.
        val store = MapStore()
        val altLeft = KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON
        val altRight = KeyEvent.META_ALT_ON or KeyEvent.META_ALT_RIGHT_ON

        assertTrue("KANA still toggles 한/영", KeyBindings.matches(store, KeyAction.HANGUL_ENG, KANA_KEYCODE, 0))
        assertTrue("bare Right Alt still toggles 한/영", KeyBindings.matches(store, KeyAction.HANGUL_ENG, KeyEvent.KEYCODE_ALT_RIGHT, 0))
        assertTrue("EISU still opens 한자", KeyBindings.matches(store, KeyAction.HANJA, EISU_KEYCODE, 0))
        assertTrue("Shift+Right Alt still opens 한자", KeyBindings.matches(store, KeyAction.HANJA, KeyEvent.KEYCODE_ALT_RIGHT, KeyEvent.META_SHIFT_ON))

        assertFalse("Alt+Z is no longer a 한/영 trigger", KeyBindings.matches(store, KeyAction.HANGUL_ENG, KeyEvent.KEYCODE_Z, altLeft))
        assertFalse("Alt+X is no longer a 한자 trigger", KeyBindings.matches(store, KeyAction.HANJA, KeyEvent.KEYCODE_X, altLeft))
        assertNull(KeyBindings.stored(store, KeyAction.HANGUL_ENG))
        assertTrue(KeyBindings.combos(store, KeyAction.HANGUL_ENG).isEmpty())
        // EISU is 한자's DEFAULT now, not one of its alternates: the window shows
        // it (the row is not "not set") and a rebind replaces it.
        assertEquals(listOf(KeyCombo(EISU_KEYCODE)), KeyBindings.combos(store, KeyAction.HANJA))
        // Unused locals kept readable: the assertions above are clearer with the
        // chords spelled out than with a bare zero.
        assertTrue(altLeft != altRight)
    }

    @Test
    fun `the hanja default is EISU and a rebind takes it away`() {
        // The ruling's "configurable" half. A platform alternate would keep
        // firing beside the stored chord, so the physical 한자 key could never be
        // reassigned and the settings window's row would describe nothing.
        val store = MapStore()
        assertEquals(listOf(KeyCombo(EISU_KEYCODE)), KeyBindings.combos(store, KeyAction.HANJA))
        assertTrue(KeyBindings.matches(store, KeyAction.HANJA, EISU_KEYCODE, 0))

        KeyBindings.set(store, KeyAction.HANJA, KeyCombo(KeyEvent.KEYCODE_F9))
        assertEquals(listOf(KeyCombo(KeyEvent.KEYCODE_F9)), KeyBindings.combos(store, KeyAction.HANJA))
        assertTrue(KeyBindings.matches(store, KeyAction.HANJA, KeyEvent.KEYCODE_F9, 0))
        assertFalse("EISU was replaced, not shadowed", KeyBindings.matches(store, KeyAction.HANJA, EISU_KEYCODE, 0))
        // The keyboard conventions are still there: they are alternates, not
        // defaults, and a rebind never removes them.
        assertTrue(
            "Shift+Right Alt still opens 한자",
            KeyBindings.matches(store, KeyAction.HANJA, KeyEvent.KEYCODE_ALT_RIGHT, KeyEvent.META_SHIFT_ON),
        )
    }

    @Test
    fun `voice defaults to Alt plus KANA`() {
        // Ruled 2026-10-31: VOICE = Alt (the engine's HUNMIN_MOD_ALT default),
        // replacing the 2026-10-11 Shift+KANA instruction. Shift stays available
        // as a special-combination modifier but is no longer this action's default.
        val store = MapStore()
        assertEquals(
            listOf(KeyCombo(KANA_KEYCODE, KeyBinding.META_ALT_LEFT)),
            KeyBindings.combos(store, KeyAction.VOICE),
        )
        assertTrue(KeyBindings.matches(store, KeyAction.VOICE, KANA_KEYCODE, KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON))
        assertFalse("bare KANA is 한/영, not 음성", KeyBindings.matches(store, KeyAction.VOICE, KANA_KEYCODE, 0))
        assertFalse("Shift+KANA is no longer the 음성 default", KeyBindings.matches(store, KeyAction.VOICE, KANA_KEYCODE, KeyEvent.META_SHIFT_ON))
    }

    @Test
    fun `the old Alt plus V defaults are gone`() {
        val store = MapStore()
        val altLeft = KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON
        assertFalse(KeyBindings.matches(store, KeyAction.VOICE, KeyEvent.KEYCODE_V, altLeft))
    }

    @Test
    fun `auto toggle has no default and a stored binding is inert`() {
        // The maintainer's ruling 2026-10-12 (issue #2): the feature is disabled
        // by default in 0.1. The old default chord Right Alt + Enter must not
        // fire with nothing stored -- that is the ruling's mechanism, "disable
        // the default key composition". A stored binding (from a build where the
        // row was live, or a stale preference) still matches here: the SERVICE
        // skips the action, so the chord is not consumed -- pinned in the
        // service's own dispatch, not here.
        val store = MapStore()
        val altRightEnter = KeyEvent.META_ALT_ON or KeyEvent.META_ALT_RIGHT_ON
        assertTrue(KeyBindings.combos(store, KeyAction.AUTO_TOGGLE).isEmpty())
        assertFalse("Right Alt + Enter is not the default any more", KeyBindings.matches(store, KeyAction.AUTO_TOGGLE, KeyEvent.KEYCODE_ENTER, altRightEnter))
        assertNull(KeyBindings.stored(store, KeyAction.AUTO_TOGGLE))

        KeyBindings.set(store, KeyAction.AUTO_TOGGLE, KeyCombo(KeyEvent.KEYCODE_ENTER, KeyBinding.META_ALT_RIGHT))
        assertTrue("a stored binding still matches in the binding layer", KeyBindings.matches(store, KeyAction.AUTO_TOGGLE, KeyEvent.KEYCODE_ENTER, altRightEnter))
    }

    @Test
    fun `keycodes are the ones android jar declares`() {
        // javap -constants on android.jar API 35: KANA = 218, EISU = 212. If the
        // SDK ever renumbers them, the fixed keys would silently stop answering.
        assertEquals(218, KANA_KEYCODE)
        assertEquals(212, EISU_KEYCODE)
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

    // --- conflicting bindings -------------------------------------------------

    @Test
    fun `no conflict when the chord is free`() {
        val store = MapStore()
        assertNull(KeyBindings.conflict(store, KeyAction.HANJA, KeyCombo(KeyEvent.KEYCODE_F9)))
    }

    @Test
    fun `an action never conflicts with itself`() {
        val store = MapStore()
        KeyBindings.set(store, KeyAction.HANJA, KeyCombo(KeyEvent.KEYCODE_F9))
        assertNull(KeyBindings.conflict(store, KeyAction.HANJA, KeyCombo(KeyEvent.KEYCODE_F9)))
    }

    @Test
    fun `a chord another action already owns is reported`() {
        // 한/영 owns F9 now, and 한자 is tested after it, so 한자 would never fire.
        val store = MapStore()
        KeyBindings.set(store, KeyAction.HANGUL_ENG, KeyCombo(KeyEvent.KEYCODE_F9))
        assertEquals(
            KeyAction.HANGUL_ENG,
            KeyBindings.conflict(store, KeyAction.HANJA, KeyCombo(KeyEvent.KEYCODE_F9)),
        )
    }

    @Test
    fun `a chord another action's FIXED key owns is reported`() {
        // EISU is 한자's fixed 한자 입력 key. 한/영 is tested before 한자, so
        // binding 한/영 to EISU would take the key away from 한자 entirely.
        val store = MapStore()
        assertEquals(
            KeyAction.HANJA,
            KeyBindings.conflict(store, KeyAction.HANGUL_ENG, KeyCombo(EISU_KEYCODE)),
        )
    }

    @Test
    fun `the conflict is reported whichever side binds second`() {
        // Earlier action bound second: the new binding is the one that is dead.
        val store = MapStore()
        KeyBindings.set(store, KeyAction.HANJA, KeyCombo(KeyEvent.KEYCODE_F9))
        assertEquals(
            KeyAction.HANJA,
            KeyBindings.conflict(store, KeyAction.HANGUL_ENG, KeyCombo(KeyEvent.KEYCODE_F9)),
        )
        assertTrue("한/영 is tested first, so it is the one that survives",
            KeyAction.HANGUL_ENG.precedence < KeyAction.HANJA.precedence)
    }

    @Test
    fun `a platform alternate counts as owned`() {
        // Bare Right Alt is 한/영's hardware convention; binding 한자 to it really
        // does lose, because 한/영 is tested first.
        val store = MapStore()
        assertEquals(
            KeyAction.HANGUL_ENG,
            KeyBindings.conflict(store, KeyAction.HANJA, KeyCombo(KeyEvent.KEYCODE_ALT_RIGHT)),
        )
    }

    @Test
    fun `the reported winner is the one the service tests first`() {
        // If the service's order and the warning's order disagree, the warning is
        // a lie. This is what ties them together.
        assertEquals(
            listOf(KeyAction.HANGUL_ENG, KeyAction.VOICE, KeyAction.AUTO_TOGGLE, KeyAction.HANJA),
            KeyAction.byPrecedence(),
        )
    }

    @Test
    fun `both bindings are stored even when they clash`() {
        // Rebinding is not a lockout: the user asked to be warned, not blocked.
        val store = MapStore()
        KeyBindings.set(store, KeyAction.HANGUL_ENG, KeyCombo(KeyEvent.KEYCODE_F9))
        assertEquals(
            KeyAction.HANGUL_ENG,
            KeyBindings.conflict(store, KeyAction.HANJA, KeyCombo(KeyEvent.KEYCODE_F9)),
        )
        KeyBindings.set(store, KeyAction.HANJA, KeyCombo(KeyEvent.KEYCODE_F9))
        assertEquals(KeyCombo(KeyEvent.KEYCODE_F9), KeyBindings.stored(store, KeyAction.HANJA))
        assertEquals(KeyCombo(KeyEvent.KEYCODE_F9), KeyBindings.stored(store, KeyAction.HANGUL_ENG))
        // ...and 한/영 keeps the chord, because it is tested first.
        assertTrue(KeyBindings.matches(store, KeyAction.HANGUL_ENG, KeyEvent.KEYCODE_F9, 0))
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