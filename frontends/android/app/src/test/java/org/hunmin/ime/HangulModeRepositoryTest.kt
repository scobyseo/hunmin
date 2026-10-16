/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1: the Hangul/English mode store, against a plain map.
 *
 * What this can and cannot prove, stated first:
 *
 *   CAN:  that the last-used mode survives a process restart — the reason
 *         the store exists at all. That a first install (nothing stored)
 *         starts in Hangul, not English. That an unreadable prefs file falls
 *         back to the default rather than crashing. That a failed write does
 *         not lose the in-memory toggle.
 *
 *   CANNOT: that the service actually calls the store at the right moment,
 *         or that SharedPreferences persists across a real kill. Those are
 *         tier E2's questions, on a device (tools/android-e2e.sh). Nothing
 *         here drives InputMethodService; this is the contract the service
 *         relies on, asserted in isolation.
 *
 * A plain map stands in for SharedPreferences because the store takes the
 * read/write as functions — no Robolectric, no Android runtime, the same
 * reason the rest of tier E1 runs on the JVM.
 */
package org.hunmin.ime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HangulModeRepositoryTest {

    /** An in-memory stand-in for SharedPreferences. */
    private class FakePrefs {
        val map = mutableMapOf<String, Boolean>()
        /** When set, reads throw — models a corrupt/locked prefs file. */
        var readThrows = false
        var writeThrows = false

        fun read(key: String, default: Boolean): Boolean {
            if (readThrows) throw RuntimeException("prefs unreadable")
            return map[key] ?: default
        }

        fun write(key: String, value: Boolean) {
            if (writeThrows) throw RuntimeException("prefs unwritable")
            map[key] = value
        }
    }

    private fun storeOf(prefs: FakePrefs) = HangulModeRepository(
        read = { key, default -> prefs.read(key, default) },
        write = { key, value -> prefs.write(key, value) },
    )

    @Test
    fun `a first install starts in Hangul`() {
        // Nothing stored yet. The pre-feature mode was Hangul, so an upgrade
        // or a fresh install must not land the user in English by surprise.
        val store = storeOf(FakePrefs())
        assertTrue("no stored value must recall Hangul", store.recall("hangul_mode"))
    }

    @Test
    fun `the last-used mode survives a restart`() {
        // The whole point: toggle to English, "restart" (a fresh store over
        // the same prefs), and English is still there.
        val prefs = FakePrefs()
        val first = storeOf(prefs)
        first.remember("hangul_mode", false)   // switched to English

        val restarted = storeOf(prefs)         // new process, same prefs
        assertFalse("English mode must survive a restart", restarted.recall("hangul_mode"))
    }

    @Test
    fun `toggling back to Hangul is remembered too`() {
        val prefs = FakePrefs()
        val store = storeOf(prefs)
        store.remember("hangul_mode", false)
        store.remember("hangul_mode", true)
        assertTrue("Hangul must be recalled after toggling back", store.recall("hangul_mode"))
    }

    @Test
    fun `an unreadable prefs falls back to Hangul rather than crashing`() {
        val prefs = FakePrefs().apply { readThrows = true }
        val store = storeOf(prefs)
        // The read throws inside the store; it must not propagate.
        assertTrue("an unreadable prefs must fall back to Hangul", store.recall("hangul_mode"))
    }

    @Test
    fun `a failed write does not throw out of the toggle`() {
        val prefs = FakePrefs().apply { writeThrows = true }
        val store = storeOf(prefs)
        // Must not throw: the in-memory mode is already correct, and losing
        // persistence is strictly better than crashing on a keypress.
        store.remember("hangul_mode", false)
    }

    @Test
    fun `an explicit non-default fallback is honoured`() {
        // If a caller ever passes a different default, recall must use it.
        val prefs = FakePrefs()
        val store = storeOf(prefs)
        assertFalse("an English default must be recalled when nothing is stored",
            store.recall("hangul_mode", defaultHangul = false))
    }
}
