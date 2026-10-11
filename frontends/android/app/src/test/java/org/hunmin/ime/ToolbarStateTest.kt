/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * ToolbarState — the dock's state, which is the one part of the toolbar that
 * can be tested without a Context.
 *
 * What these pin down is not the arithmetic but the two rules that are easy to
 * lose in a rewrite: an unknown layout id must still SAY something, and the
 * name the user hears must never be the shortened one.
 */
package org.hunmin.ime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ToolbarStateTest {

    private val layouts = listOf(
        LayoutEntry("dubeolsik", "두벌식"),
        LayoutEntry("sun3-p2-basic", "신세벌식 P2 basic"),
        LayoutEntry("us-qwerty", "Qwerty", LayoutMode.LATIN),
    )

    @Test
    fun `a known layout shows its label`() {
        val state = ToolbarState.of(layouts, "dubeolsik", false, true, VoiceState.IDLE)
        assertEquals("두벌식", state.layoutLabel)
        assertEquals("두벌식", state.layoutLabelFull)
    }

    @Test
    fun `the spoken name is never the abbreviated one`() {
        val state = ToolbarState.of(layouts, "sun3-p2-basic", true, true, VoiceState.IDLE)
        assertEquals("SUN3", state.layoutLabel)
        assertEquals("신세벌식 P2 basic", state.layoutLabelFull)
    }

    @Test
    fun `an unknown layout id is shown rather than hidden`() {
        val state = ToolbarState.of(layouts, "not-installed", false, true, VoiceState.IDLE)
        assertEquals("not-installed", state.layoutLabel)
        assertEquals("not-installed", state.layoutLabelFull)
    }

    @Test
    fun `no layout yet reads as no layout, not as nothing`() {
        val state = ToolbarState.of(layouts, null, false, false, VoiceState.LISTENING)
        assertEquals(ToolbarState.NO_LAYOUT, state.layoutLabel)
        assertFalse(state.modeIsHangul)
        assertEquals(VoiceState.LISTENING, state.voice)
    }

    @Test
    fun `hanja conversion is off unless something says otherwise`() {
        val off = ToolbarState.of(layouts, "dubeolsik", false, true, VoiceState.IDLE)
        val on = ToolbarState.of(layouts, "dubeolsik", false, true, VoiceState.IDLE, hanjaConverting = true)
        assertFalse(off.hanjaConverting)
        assertTrue(on.hanjaConverting)
    }

    @Test
    fun `two states equal only when everything shown is equal`() {
        val a = ToolbarState.of(layouts, "dubeolsik", false, true, VoiceState.IDLE)
        val b = ToolbarState.of(layouts, "dubeolsik", false, true, VoiceState.IDLE)
        val c = a.copy(modeIsHangul = false)
        assertEquals(a, b)
        assertTrue(a != c)
    }
}