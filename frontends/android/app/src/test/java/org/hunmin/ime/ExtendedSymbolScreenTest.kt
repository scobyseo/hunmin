/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1, the extended-symbols page lane (#312, KP10/KP11).
 *
 * The designed Tier 2 panel ships as keypad/symbols-extended.json; this lane
 * pins the app path exactly as SymbolScreenTest pins Tier 1: parse the real
 * asset, tap every symbol, get exactly that character through
 * RecordingInputConnection, nothing composing, no I9 violation. The content
 * contract (the ruled 4x10 panel, the mode-row vocabulary) is the Python
 * checker's — not restated here. The page-navigation state (TEXT -> SYMBOLS
 * -> EXTENDED and the KP9 returns) is KeypadState's pure half, pinned here;
 * the picker popup itself is a device round (E2E).
 */
package org.hunmin.ime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class ExtendedSymbolScreenTest {

    private val repoRoot: File by lazy {
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) } ?: File("../..").canonicalFile
    }
    private val asset: File by lazy {
        File(repoRoot, "frontends/android/app/src/main/assets/keypad/symbols-extended.json")
    }

    private fun load(): SymbolScreen = SymbolScreen.parse(asset.readText())

    @Test
    fun everyExtendedSymbolCommitsExactlyItsCharacter() {
        val screen = load()
        // The designed panel is 4x10 (KP9); the arrangement itself is the
        // Python checker's contract, not restated here.
        val symbolRows = screen.rows.filter { it.kind == SymbolScreen.Kind.SYMBOLS }
        assertEquals(listOf(10, 10, 10, 10), symbolRows.map { it.keys.size })
        assertEquals(40, screen.symbols.size)
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
    fun theModeRowIdsAreNotCommittableAsSymbols() {
        val screen = load()
        val ic = RecordingInputConnection()
        val functionKeys = screen.rows
            .filter { it.kind == SymbolScreen.Kind.FUNCTION }
            .flatMap { it.keys }
        assertEquals(listOf("language-toggle", "symbols-tier1"), functionKeys)
        for (id in functionKeys) {
            try {
                screen.commitSymbol(id, ic)
                throw AssertionError("commitSymbol accepted function id $id")
            } catch (expected: IllegalArgumentException) {
                // refused: a mode action is wiring, not text (KP11)
            }
        }
        assertTrue(ic.commits.isEmpty())
    }

    @Test
    fun theModeKeysMapToPageActionsNotKeyEvents() {
        val rows = KeypadModel.symbolRows(load())
        val pageKeys = rows.flatten().filter { it.kind == KeypadKey.Kind.SYMBOLS_PAGE }
        assertEquals(listOf("symbols-tier1"), pageKeys.map { it.id })
        assertEquals("기호 1", pageKeys.single().label)
        val mode = rows.flatten().filter { it.kind == KeypadKey.Kind.MODE }
        assertEquals(listOf("language-toggle"), mode.map { it.id })
        // KP9: no editing keys on the extended page.
        assertTrue(rows.flatten().none {
            it.kind == KeypadKey.Kind.FUNCTION &&
                it.id in listOf("backspace", "space", "enter")
        })
    }

    // ------------------------------------------------- page navigation (KP9)

    @Test
    fun extendedIsEnteredFromSymbolsAndReturnsThroughThem() {
        val s = KeypadState()
        s.enterSymbols(hangulMode = true)
        s.enterExtended(hangulMode = true)
        assertEquals(KeypadState.Page.EXTENDED, s.page)
        s.returnToSymbols()
        assertEquals(KeypadState.Page.SYMBOLS, s.page)
        // 한/영 still returns to the mode the pages were entered from (KP9).
        assertTrue(s.returnToText())
        assertEquals(KeypadState.Page.TEXT, s.page)
    }

    @Test
    fun aModeDriftUnderTheExtendedPageReturnsToText() {
        val s = KeypadState()
        s.enterSymbols(hangulMode = false)
        s.enterExtended(hangulMode = false)
        s.modeChanged(hangulMode = false)   // the entered-from mode holds
        assertEquals(KeypadState.Page.EXTENDED, s.page)
        s.modeChanged(hangulMode = true)    // drifted: the return memory is stale
        assertEquals(KeypadState.Page.TEXT, s.page)
    }

    @Test
    fun enteringTheExtendedPageClearsTheOneShot() {
        val s = KeypadState()
        s.enterSymbols(hangulMode = true)
        s.tapShift()
        assertTrue(s.shiftArmed)
        s.enterExtended(hangulMode = true)
        assertTrue(!s.shiftArmed)
    }

    @Test
    fun thePickerFromTextRecordsTheEntryMode() {
        // MEASURED 2026-11-23 (E2E picker round): TEXT -> EXTENDED via the
        // picker bypassed enterSymbols, the return memory kept its stale
        // default, and 한/영 toggled to 한 instead of restoring 영.
        val s = KeypadState()
        s.enterExtended(hangulMode = false)   // straight from TEXT, English
        assertEquals(KeypadState.Page.EXTENDED, s.page)
        assertTrue(!s.returnToText())         // restores English, not the default
        val s2 = KeypadState()
        s2.enterExtended(hangulMode = true)
        assertTrue(s2.returnToText())
        // From SYMBOLS the memory is kept, not overwritten by the picker.
        val s3 = KeypadState()
        s3.enterSymbols(hangulMode = false)
        s3.enterExtended(hangulMode = true)   // the mode drifted; memory holds
        assertTrue(!s3.returnToText())
    }
}