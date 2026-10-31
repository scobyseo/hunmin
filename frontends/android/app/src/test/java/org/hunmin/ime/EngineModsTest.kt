/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1: the modifier bits the adapter hands the engine.
 *
 * The engine's chord vocabulary is `HUNMIN_MOD_CTRL | ALT | GUI` (hunmin.h), and
 * the special-function table keys on those bits one by one: VOICE is Alt, so a
 * lumped "some chord" value could never resolve it. These cases pin the mapping
 * per family, and pin that Shift stays out of it — Shift is a layout row, and a
 * Shift+한/영 press must not become a special function
 * (docs/SPEC-MODES.md §E5, ruled 2026-10-31).
 */
package org.hunmin.ime

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Test

class EngineModsTest {
    @Test
    fun `each modifier family maps to its own engine bit`() {
        assertEquals("Ctrl", 0x1, HunminKeyRegistry.engineMods(KeyEvent.META_CTRL_ON))
        assertEquals("Alt", 0x2, HunminKeyRegistry.engineMods(KeyEvent.META_ALT_ON))
        assertEquals("Meta/Super", 0x4, HunminKeyRegistry.engineMods(KeyEvent.META_META_ON))
        assertEquals("Ctrl+Alt", 0x3, HunminKeyRegistry.engineMods(KeyEvent.META_CTRL_ON or KeyEvent.META_ALT_ON))
    }

    @Test
    fun `the side bits do not change the family`() {
        // A real Alt press arrives with the family bit AND a side bit; the engine
        // wants the family only, because the physical key is the frontend's.
        val altLeft = KeyEvent.META_ALT_ON or KeyEvent.META_ALT_LEFT_ON
        val altRight = KeyEvent.META_ALT_ON or KeyEvent.META_ALT_RIGHT_ON
        assertEquals(0x2, HunminKeyRegistry.engineMods(altLeft))
        assertEquals(0x2, HunminKeyRegistry.engineMods(altRight))
    }

    @Test
    fun `shift alone is not a chord bit`() {
        // Shift selects a layout row (invariant 4). A Shift+한/영 press therefore
        // hands the engine no chord bit, which is what keeps it out of the
        // special-function table.
        assertEquals(0, HunminKeyRegistry.engineMods(KeyEvent.META_SHIFT_ON))
        assertEquals(0, HunminKeyRegistry.engineMods(0))
        // CapsLock is the `caps` option, not a chord either.
        assertEquals(0, HunminKeyRegistry.engineMods(KeyEvent.META_CAPS_LOCK_ON))
        // Alt with Shift is still Alt for the engine's purposes.
        assertEquals(0x2, HunminKeyRegistry.engineMods(KeyEvent.META_ALT_ON or KeyEvent.META_SHIFT_ON))
    }

    @Test
    fun `the voice default is Alt plus the Han-Eng key`() {
        // The ruled Table A (2026-10-31): VOICE = Alt, AUTOSWITCH = Ctrl, and the
        // engine ships both. Android's shipped Shift+KANA default is the value
        // that moved.
        assertEquals(
            listOf(KeyCombo(KANA_KEYCODE, KeyBinding.META_ALT_LEFT)),
            KeyBindings.combos(MapStore(), KeyAction.VOICE),
        )
    }
}

/** In-memory store, the shape KeyBindingTest uses for the same reason. */
private class MapStore : KeyBindingRepository {
    private val values = mutableMapOf<String, String>()
    override fun read(key: String): String? = values[key]
    override fun write(key: String, value: String) {
        values[key] = value
    }
}
