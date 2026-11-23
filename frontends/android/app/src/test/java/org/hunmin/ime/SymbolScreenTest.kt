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
