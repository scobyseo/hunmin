/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1, the app-read symbol data lane (issue #297, ruling KP11).
 *
 * The golden here is the commit vector of every symbol key in the shipped
 * asset: data file -> SymbolScreen.parse -> commitSymbol -> the exact
 * characters that reach the client through RecordingInputConnection. No JNI,
 * no engine — KP11 rules the engine out of this screen, so these tests never
 * skip on a missing shim (unlike the adapter lanes) and always run.
 *
 * The content contract (ruled arrangement, function vocabulary) is asserted
 * by tools/symbol-data-check.py in ctest `keypad-symbol-data`; duplicating it
 * here would give the arrangement a second home. What only this lane can
 * assert is the app path: the parse of the real asset, and that a tap on each
 * symbol delivers exactly that character with nothing composing and no
 * surrounding-text violation (I9).
 *
 * Issue #312 adds the variants lane: the app must parse and expose the
 * gesture-variant mapping the checker accepts (no silent ignore), variant
 * characters must not masquerade as Tier 1 commit keys, and corrupted
 * variants fail loudly. The assignment itself is provisional data, pinned
 * nowhere but the file — see docs/DESIGN-ANDROID-SYMBOL-VARIANTS.md.
 */
package org.hunmin.ime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class SymbolScreenTest {

    private val repoRoot: File by lazy {
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) } ?: File("../..").canonicalFile
    }
    private val asset: File by lazy {
        File(repoRoot, "frontends/android/app/src/main/assets/keypad/symbols.json")
    }

    private fun load(): SymbolScreen = SymbolScreen.parse(asset.readText())

    @Test
    fun theShippedAssetParsesAndCoversAllAsciiPunctuation() {
        val screen = load()
        // The 32 ASCII punctuation characters, derived not transcribed.
        val ascii = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
        assertEquals(32, screen.symbols.size)
        assertEquals(ascii.map { it.toString() }.toSet(), screen.symbols.toSet())
        // KP6's three symbol rows, in order; the exact arrangement inside
        // them is the Python checker's contract, not restated here.
        val symbolRows = screen.rows.filter { it.kind == SymbolScreen.Kind.SYMBOLS }
        assertEquals(listOf(10, 12, 10), symbolRows.map { it.keys.size })
    }

    @Test
    fun everySymbolKeyCommitsExactlyItsCharacter() {
        val screen = load()
        for (key in screen.symbols) {
            val ic = RecordingInputConnection()
            screen.commitSymbol(key, ic)
            assertEquals("symbol $key", listOf(key), ic.commits)
            assertTrue("symbol $key composed something", ic.composings.isEmpty())
            assertEquals(0, ic.finishedComposing)
            assertTrue("I9 violations for $key: ${ic.violations}", ic.violations.isEmpty())
        }
    }

    @Test
    fun aWholeRowTypedInOrderArrivesInOrder() {
        val screen = load()
        val ic = RecordingInputConnection()
        val row = screen.rows.first { it.kind == SymbolScreen.Kind.SYMBOLS }
        row.keys.forEach { screen.commitSymbol(it, ic) }
        assertEquals(row.keys, ic.commits)
        assertTrue(ic.violations.isEmpty())
    }

    @Test
    fun functionIdsAreNotCommittableAsSymbols() {
        val screen = load()
        val ic = RecordingInputConnection()
        val functionKeys = screen.rows
            .filter { it.kind == SymbolScreen.Kind.FUNCTION }
            .flatMap { it.keys }
        assertTrue(functionKeys.isNotEmpty())
        for (id in functionKeys + listOf("a", "가", "")) {
            try {
                screen.commitSymbol(id, ic)
                throw AssertionError("commitSymbol accepted non-symbol $id")
            } catch (expected: IllegalArgumentException) {
                // refused: a function id is wiring, not text (KP11 data notes)
            }
        }
        assertTrue(ic.commits.isEmpty())
    }

    @Test
    fun theShippedVariantsParseAndStayConsistentWithTheRows() {
        // Issue #312: the app must see the variant mapping the checker
        // accepts — not silently ignore it. Which variant sits on which host
        // is provisional data (the Python checker's scope contract), so this
        // lane asserts only the parse-level invariants, not the assignment.
        val screen = load()
        assertTrue("shipped asset carries no variants", screen.variants.isNotEmpty())
        for ((host, v) in screen.variants) {
            assertTrue("host $host is not a symbol key", host in screen.symbols)
            assertTrue("popup empty for $host", v.popup.isNotEmpty())
            assertEquals("up must be popup[0] for $host", v.popup[0], v.up)
            for (ch in v.popup) {
                assertEquals("variant must be one character", 1, ch.length)
                assertTrue("variant $ch must be non-ASCII (Tier 2)", ch[0].code > 0x7F)
            }
        }
        // One home per variant character.
        val all = screen.variants.values.flatMap { it.popup }
        assertEquals(all.size, all.toSet().size)
    }

    @Test
    fun variantCharactersAreNotTier1CommitKeys() {
        // commitSymbol is the Tier 1 tap seam; a variant character reaches
        // the client only through the gesture wiring (open), never by
        // masquerading as a symbol key of the screen.
        val screen = load()
        val ic = RecordingInputConnection()
        for (ch in screen.variants.values.flatMap { it.popup }) {
            try {
                screen.commitSymbol(ch, ic)
                throw AssertionError("commitSymbol accepted variant $ch")
            } catch (expected: IllegalArgumentException) {
                // refused: not a Tier 1 key
            }
        }
        assertTrue(ic.commits.isEmpty())
    }

    @Test
    fun corruptedVariantsFailLoudlyAtParse() {
        val rows = """"rows":[{"kind":"symbols","keys":[".","<"]}]"""
        val bad = listOf(
            // host not among the screen's symbol keys
            """{"schemaVersion":1,$rows,"variants":{"!":{"up":"·","popup":["·"]}}}""",
            // up is not the first popup member
            """{"schemaVersion":1,$rows,"variants":{".":{"up":"…","popup":["·","…"]}}}""",
            // empty popup
            """{"schemaVersion":1,$rows,"variants":{".":{"up":"·","popup":[]}}}""",
            // missing up
            """{"schemaVersion":1,$rows,"variants":{".":{"popup":["·"]}}}""",
            // multi-character variant
            """{"schemaVersion":1,$rows,"variants":{".":{"up":"··","popup":["··"]}}}""",
        )
        for (text in bad) {
            try {
                SymbolScreen.parse(text)
                throw AssertionError("parse accepted: $text")
            } catch (expected: IllegalArgumentException) {
                // loud failure, same as a corrupted row
            }
        }
    }

    @Test
    fun corruptedAssetsFailLoudlyAtParse() {
        val bad = listOf(
            "not json",
            """{"schemaVersion": 2, "rows": [{"kind":"symbols","keys":["!"]}]}""",
            """{"schemaVersion": 1}""",
            """{"schemaVersion": 1, "rows": []}""",
            """{"schemaVersion": 1, "rows": [{"kind":"emoji","keys":["!"]}]}""",
            """{"schemaVersion": 1, "rows": [{"kind":"symbols","keys":[]}]}""",
            """{"schemaVersion": 1, "rows": [{"kind":"symbols","keys":["ab"]}]}""",
            """{"schemaVersion": 1, "rows": [{"kind":"symbols","keys":[""]}] }""",
            """{"schemaVersion": 1, "rows": [
                {"kind":"symbols","keys":["!"]},
                {"kind":"symbols","keys":["!"]}]}""",
        )
        for (text in bad) {
            try {
                SymbolScreen.parse(text)
                throw AssertionError("parse accepted: $text")
            } catch (expected: IllegalArgumentException) {
                // loud failure, not a blank key
            }
        }
    }
}
