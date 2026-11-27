/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1, the UI trace sink (#312 usability evaluation).
 *
 * The sink must record the layers the engine trace cannot see — gesture
 * decisions with their trajectory and gates, symbol commits, page moves,
 * shift state — as parseable JSONL, escape the entries that contain quotes
 * and backslashes (a kaomoji like ¯\_(ツ)_/¯ would otherwise break the file
 * exactly when it is interesting), and stay inert when off.
 */
package org.hunmin.ime

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class UiTraceRecorderTest {

    @Test
    fun eventsAreParseableJsonlWithAMetaFirst() {
        val f = File.createTempFile("ui-trace-test", ".jsonl")
        try {
            UiTraceRecorder.enable(f, "e1-test")
            UiTraceRecorder.gesture("TEXT", "1", "DIGIT", 0f, -180f, 140L,
                "swipe_up", "SHIFTED_KEY", 157f, 26f, 31f)
            UiTraceRecorder.symbol("keypad-symbols-extended", "·")
            UiTraceRecorder.page("TEXT", "SYMBOLS", "tap")
            UiTraceRecorder.shift(armed = false, locked = true)
            UiTraceRecorder.mark("기대: ! 였음")
            UiTraceRecorder.disable()
            val lines = f.readLines().filter { it.isNotBlank() }.map { JSONObject(it) }
            assertEquals(6, lines.size)
            assertEquals("android-ui", lines[0].getJSONObject("meta").getString("platform"))
            val g = lines[1]
            assertEquals("gesture", g.getString("ui"))
            assertEquals("swipe_up", g.getString("recognized"))
            assertEquals(-180.0, g.getDouble("dy"), 0.01)
            assertEquals(157.0, g.getJSONObject("gates").getDouble("threshold_px"), 0.01)
            assertEquals("·", lines[2].getString("entry"))
            assertEquals("SYMBOLS", lines[3].getString("to"))
            assertTrue(lines[4].getBoolean("locked"))
            assertEquals("기대: ! 였음", lines[5].getString("label"))
        } finally {
            UiTraceRecorder.disable()
            f.delete()
        }
    }

    @Test
    fun entriesWithQuotesAndBackslashesStayParseable() {
        val f = File.createTempFile("ui-trace-test", ".jsonl")
        try {
            UiTraceRecorder.enable(f, "escape")
            // The kaomoji that would break a naive writer, and a quote entry.
            UiTraceRecorder.symbol("keypad-kaomoji", "¯\\_(ツ)_/¯")
            UiTraceRecorder.symbol("keypad-symbols", "\"")
            UiTraceRecorder.disable()
            val lines = f.readLines().filter { it.isNotBlank() }.map { JSONObject(it) }
            assertEquals("¯\\_(ツ)_/¯", lines[1].getString("entry"))
            assertEquals("\"", lines[2].getString("entry"))
        } finally {
            UiTraceRecorder.disable()
            f.delete()
        }
    }

    @Test
    fun resumeAppendsInsteadOfTruncating() {
        // #312 evaluation (2026-11-27): the IME service is recreated often; a
        // truncating re-open threw away a day of the session. A resumed trace
        // keeps the earlier lines and marks the seam.
        val f = File.createTempFile("ui-trace-test", ".jsonl")
        try {
            UiTraceRecorder.enable(f, "eval")
            UiTraceRecorder.symbol("p1", "before")
            UiTraceRecorder.disable()
            UiTraceRecorder.enable(f, "eval", resume = true)
            UiTraceRecorder.symbol("p2", "after")
            UiTraceRecorder.disable()
            val lines = f.readLines().filter { it.isNotBlank() }.map { JSONObject(it) }
            val entries = lines.filter { it.optString("ui") == "symbol" }.map { it.getString("entry") }
            assertEquals(listOf("before", "after"), entries)
            assertTrue("the resumed meta must mark the seam",
                lines.any { it.optJSONObject("meta")?.optBoolean("resumed") == true })
        } finally {
            UiTraceRecorder.disable()
            f.delete()
        }
    }

    @Test
    fun offMeansNoWritesAndNoFile() {
        val f = File.createTempFile("ui-trace-test", ".jsonl")
        f.delete()   // the sink must not create it while off
        try {
            assertFalse(UiTraceRecorder.isEnabled)
            UiTraceRecorder.gesture("TEXT", "q", "CHARACTER", 0f, 0f, 0L,
                "tap", "TAP", 1f, 1f, 1f)
            UiTraceRecorder.symbol("x", "y")
            UiTraceRecorder.mark("z")
            assertFalse(f.exists())
        } finally {
            UiTraceRecorder.disable()
            f.delete()
        }
    }
}