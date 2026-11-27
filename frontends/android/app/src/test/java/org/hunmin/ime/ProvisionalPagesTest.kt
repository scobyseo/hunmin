/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1, the #312 provisional symbol pages (math / emoji / kaomoji).
 *
 * Shipped under the maintainer's 2026-11-23 direction "심볼 목록은 일단 구현해
 * 보고 사용성을 평가하면서 개선해보자" — the CONTENT is provisional (the
 * checker pins structure, not the unruled assignment); this lane pins the
 * app path: parse the real assets, tap every entry, get exactly that string
 * through RecordingInputConnection, nothing composing, no I9 violation.
 * Emoji and kaomoji are STRINGS (U+FE0F / whole emoticons) — the multiChar
 * parse — while Tier 1 stays single-character.
 */
package org.hunmin.ime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class ProvisionalPagesTest {

    private val repoRoot: File by lazy {
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) } ?: File("../..").canonicalFile
    }
    private fun asset(name: String): File =
        File(repoRoot, "frontends/android/app/src/main/assets/keypad/$name")

    private fun commitGolden(screen: SymbolScreen, expectedCount: Int) {
        assertEquals(expectedCount, screen.symbols.size)
        for (key in screen.symbols) {
            val ic = RecordingInputConnection()
            screen.commitSymbol(key, ic)
            assertEquals("entry $key", listOf(key), ic.commits)
            assertTrue("entry $key composed something", ic.composings.isEmpty())
            assertEquals(0, ic.finishedComposing)
            assertTrue("I9 violations for $key: ${ic.violations}", ic.violations.isEmpty())
        }
    }

    @Test
    fun theMathPageParsesAndCommitsEveryEntry() {
        commitGolden(SymbolScreen.parse(asset("symbols-math.json").readText()), 40)
    }

    @Test
    fun theEmojiPageParsesAsStringsAndCommitsEveryEntry() {
        val screen = SymbolScreen.parse(asset("symbols-emoji.json").readText(), multiChar = true)
        commitGolden(screen, 40)
        // ❤️ is two codepoints — the string path, not a single-char key.
        assertTrue(screen.symbols.any { it.length == 2 })
    }

    @Test
    fun theKaomojiPageParsesAsStringsAndCommitsWholeEmoticons() {
        val screen = SymbolScreen.parse(asset("kaomoji.json").readText(), multiChar = true)
        commitGolden(screen, 30)
        // The classics the reference site lacked are present (KP18 curation).
        assertTrue(screen.symbols.contains("(´・ω・｀)"))
        assertTrue(screen.symbols.contains("¯\\_(ツ)_/¯"))
        assertTrue(screen.symbols.contains("(╯°□°)╯︵ ┻━┻"))
    }

    @Test
    fun theKaomojiPageIsAFullWidthListNotAGrid() {
        val screen = SymbolScreen.parse(asset("kaomoji.json").readText(), multiChar = true)
        val rows = KeypadModel.kaomojiRows(screen)
        val symbolRows = rows.filter { r -> r.all { it.kind == KeypadKey.Kind.SYMBOL } }
        assertTrue(symbolRows.isNotEmpty())
        for (row in symbolRows) assertEquals(1, row.size)   // one per row
        // The mode row keeps the KP9 returns.
        val last = rows.last()
        assertEquals(listOf("language-toggle", "symbols-tier1"), last.map { it.id })
    }

    @Test
    fun tier1StillRefusesMultiCharEntries() {
        // The single-character contract is Tier 1's; the string pages opt in
        // explicitly. A kaomoji asset parsed as Tier 1 fails loudly.
        try {
            SymbolScreen.parse(asset("kaomoji.json").readText())
            throw AssertionError("Tier 1 parse accepted a string page")
        } catch (expected: IllegalArgumentException) {
            // loud failure, not a blank key
        }
    }
}