/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1, the soft keypad's decisions (issue #299, T3-3).
 *
 * No JNI, no engine, no Android runtime: KeypadModel/KeypadState/KeypadVisibility
 * are pure Kotlin, so every decision the view renders is asserted here on a
 * plain JVM. What this lane proves:
 *
 *  - keycap labels are DERIVED from the real layout files (the same JSON the
 *    engine loads), jamo displayed as compatibility forms, literals unmarked;
 *  - every key of every shipped layout is touch-reachable exactly once in the
 *    generic grid — including sin3-p2, with no per-layout code (FE12);
 *  - the symbol page is the shipped asset verbatim, function ids wired to the
 *    javap-verified AOSP keycodes;
 *  - one-shot shift and the symbol-page return memory (KP9) behave as ruled;
 *  - KP12's visibility table: auto-show only without a physical keyboard.
 *
 * The view itself (pixels, tap dispatch) is tier E2's subject —
 * tools/android-e2e.sh's touch round taps these keycaps on a device.
 */
package org.hunmin.ime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class KeypadModelTest {

    private val repoRoot: File by lazy {
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) } ?: File("../..").canonicalFile
    }
    private val layouts: File by lazy { File(repoRoot, "libhunmin/data/layouts") }
    private val symbolAsset: File by lazy {
        File(repoRoot, "frontends/android/app/src/main/assets/keypad/symbols.json")
    }

    private fun caps(id: String): KeypadKeycaps =
        KeypadKeycaps.parse(File(layouts, "$id.json").readText())

    /** The engine-path key ids of a grid (CHARACTER + DIGIT), in order. */
    private fun characterIds(rows: List<List<KeypadKey>>): List<String> =
        rows.flatten()
            .filter { it.kind == KeypadKey.Kind.CHARACTER || it.kind == KeypadKey.Kind.DIGIT }
            .map { it.id }

    // ------------------------------------------------------------ labels

    @Test
    fun `dubeolsik keycaps are the layout's jamo in display form`() {
        val c = caps("dubeolsik")
        // The layout's raw values are conjoining jamo (q = U+1102); the keycap
        // shows the compatibility form. Derived via NFD, not transcribed.
        assertEquals("ㅂ", c.label("q", shift = false))
        assertEquals("ㅃ", c.label("q", shift = true))
        assertEquals("ㄷ", c.label("e", shift = false))
        assertEquals("ㄸ", c.label("e", shift = true))
        assertEquals("ㅗ", c.label("h", shift = false))
    }

    @Test
    fun `qwerty keycaps are the literal characters, shift is the upper row`() {
        val c = caps("us-qwerty")
        assertEquals("a", c.label("a", shift = false))
        assertEquals("A", c.label("a", shift = true))
        assertEquals("1", c.label("1", shift = false))
        assertEquals("!", c.label("1", shift = true))
        assertEquals("[", c.label("[", shift = false))
        assertEquals("{", c.label("[", shift = true))
    }

    @Test
    fun `display covers every jamo of the Track 3 layouts`() {
        // The NFKD + name-match table must cover dubeolsik, us-qwerty and
        // sin3-p2 fully — a missing entry would render a blank/positional
        // keycap on a device no JVM test can see. The yet/old-Hangul layouts
        // carry rare letters with no compatibility form; those display raw
        // (the fallback), so only "does not throw" is asserted for them.
        fun values(f: File): List<String> {
            val keys = org.json.JSONObject(f.readText()).getJSONObject("keys")
            fun default(v: Any?): String? = when (v) {
                is String -> v
                is org.json.JSONObject -> {
                    val case = v.optJSONObject("case")
                    if (case == null) null else default(case.opt("else")) ?: default(case.opt("then"))
                }
                else -> null
            }
            return keys.keys().asSequence().flatMap { id ->
                val k = keys.getJSONObject(id)
                listOfNotNull(default(k.opt("plain")), default(k.opt("shift")))
            }.toList()
        }
        for (id in listOf("dubeolsik", "us-qwerty", "sin3-p2")) {
            for (value in values(File(layouts, "$id.json"))) {
                if (value.startsWith("=")) continue
                for (ch in value) {
                    if (ch.code in 0x1100..0x11FF) {
                        val shown = KeypadKeycaps.display(ch.toString())
                        assertTrue(
                            "$id: jamo U+${ch.code.toString(16)} has no display form",
                            shown.length == 1 && shown[0].code in 0x3131..0x318E,
                        )
                    }
                }
            }
        }
        // Jongseong specifically: it has NO decomposition (the compat letter
        // decomposes to the choseong), so this pins the name-match pass.
        assertEquals("ㄱ", KeypadKeycaps.display("\u11A8"))
        assertEquals("ㄹ", KeypadKeycaps.display("\u11AF"))
        assertEquals("ㅎ", KeypadKeycaps.display("\u11C2"))
        for (f in layouts.listFiles { f -> f.name.endsWith(".json") && f.name != "meta.json" }.orEmpty()) {
            for (value in values(f)) {
                if (!value.startsWith("=")) KeypadKeycaps.display(value)
            }
        }
    }

    @Test
    fun `a case-primitive key shows its default branch`() {
        // sin3-p2's tier keys are condition-resolved values (SPEC-SCHEMA).
        // The keycap at rest shows what the key types with no tier state:
        // the else branch. Measured from sin3-p2.json: ' plain else-chain ᄐ,
        // ' shift else-chain =/, , plain else-chain =,.
        val c = caps("sin3-p2")
        assertEquals("ㅌ", c.label("'", shift = false))
        assertEquals("/", c.label("'", shift = true))
        assertEquals(",", c.label(",", shift = false))
    }

    // ------------------------------------------------------------ reachability

    @Test
    fun `the dubeolsik grid is the decided picture, key for key`() {
        // KP13: the mockup HTML is the decided UI. The 두벌식 screen is the
        // picture's number row + 10/9/7 letter rows + ⇧/⌫ + the bottom row —
        // every dubeolsik key reachable exactly once, nothing extra.
        val c = caps("dubeolsik")
        val rows = KeypadModel.textRows(c, shift = false)
        val ids = characterIds(rows)
        for (id in c.ids) assertEquals("key '$id' reachable exactly once", 1, ids.count { it == id })
        for (d in '0'..'9') assertTrue("digit $d", d.toString() in ids)
        assertTrue("," in ids && "." in ids)
        assertEquals("five rows, as the picture draws", 5, rows.size)
        assertEquals("number row: 10 digits", 10, rows[0].size)
        assertTrue(rows[0].all { it.kind == KeypadKey.Kind.DIGIT })
        assertEquals(listOf(10, 9, 7), rows.drop(1).take(3).map { r -> r.count { it.kind == KeypadKey.Kind.CHARACTER } })
        // ⇧ leads the third row, ⌫ trails it (KP2, the picture).
        assertEquals(KeypadKey.Kind.SHIFT, rows[3].first().kind)
        assertEquals("backspace", rows[3].last().id)
        // The bottom row, in the picture's order.
        assertEquals(
            listOf("language-toggle", "symbols-page", ",", "space", ".", "enter"),
            rows[4].map { it.id },
        )
        // The picture's hints and sublabels.
        assertEquals("!", rows[0].first { it.id == "1" }.hint)
        assertEquals(")", rows[0].first { it.id == "0" }.hint)
        assertEquals("⇪", rows[3].first().hint)
        assertEquals("?", rows[4].first { it.id == "," }.hint)
        assertEquals("\"", rows[4].first { it.id == "." }.hint)
        assertEquals("ㅃ", rows[1].first { it.id == "q" }.sub)
        assertEquals("ㅖ", rows[1].first { it.id == "p" }.sub)
        assertNull("ㅛ has no shifted form, no sublabel", rows[1].first { it.id == "y" }.sub)
    }

    @Test
    fun `the qwerty grid is the picture and its punctuation lives on the symbol page`() {
        // KP13: the English screen is the picture's 10/9/7 letter rows — no
        // extension cells. us-qwerty's 32 punctuation keys are KP1 Tier 1,
        // reachable on the symbol page (the picture's 기호 mode), not the grid.
        val c = caps("us-qwerty")
        val rows = KeypadModel.textRows(c, shift = false)
        val ids = characterIds(rows)
        val letters = c.ids.filter { it[0].isLetter() }
        assertEquals(26, letters.size)
        for (id in letters) assertEquals("letter '$id' reachable exactly once", 1, ids.count { it == id })
        assertEquals(1, ids.count { it == "," })
        assertEquals(1, ids.count { it == "." })
        // No extension cells: the picture's rows carry letters and digits only.
        assertTrue("no [ ] \\ ; ' / ` - = cells in the picture's grid",
            ids.none { it in listOf("[", "]", "\\", ";", "'", "/", "`", "-", "=") })
        // Every punctuation key the grid omits is on the symbol page.
        val screen = SymbolScreen.parse(symbolAsset.readText())
        val omitted = c.ids - ids.toSet()
        assertTrue("omitted keys are exactly the symbol page's inventory: $omitted",
            omitted.all { it in screen.symbols })
        // The English screen draws no shifted-letter sublabels (the picture).
        assertNull(rows[2].first { it.id == "a" }.sub)
    }

    @Test
    fun `sin3-p2 draws the picture geometry and its extra keys are a recorded consequence`() {
        // KP13 fixes the geometry to the picture (두벌식/QWERTY form). sin3-p2's
        // letter keys draw at their positions; its punctuation keys fall outside
        // the picture's cells and are NOT touch-reachable through this geometry.
        // That is the decided picture's consequence, recorded in SPEC KP13 —
        // not a licence to add extension cells back (the overturned deviation).
        val c = caps("sin3-p2")
        val rows = KeypadModel.textRows(c, shift = false)
        val ids = characterIds(rows)
        val letters = c.ids.filter { it[0].isLetter() }
        for (id in letters) assertEquals("letter '$id' reachable exactly once", 1, ids.count { it == id })
        assertEquals(listOf(10, 9, 7), rows.drop(1).take(3).map { r -> r.count { it.kind == KeypadKey.Kind.CHARACTER } })
        assertTrue("sin3-p2's punctuation is outside the picture's cells",
            (c.ids - ids.toSet()).containsAll(listOf(";", "'", "[", "]")))
        // , . still drawn in the bottom row (KP4): they ARE layout keys here.
        assertTrue("," in ids && "." in ids)
    }

    @Test
    fun `shift changes labels, not positions`() {
        val c = caps("dubeolsik")
        val plain = KeypadModel.textRows(c, shift = false)
        val shifted = KeypadModel.textRows(c, shift = true)
        assertEquals(plain.map { r -> r.map { it.id } }, shifted.map { r -> r.map { it.id } })
        assertEquals("ㅃ", shifted.flatten().first { it.id == "q" }.label)
    }

    // ------------------------------------------------------------ keycodes

    @Test
    fun `character keycodes are the registry's, not a second table`() {
        for (id in listOf("a", "z", "0", "9", ",", ".", ";", "'", "[", "]", "\\", "`", "-", "=", "/")) {
            val want = HunminKeyRegistry.all().first { it.unshifted == id[0] }.keyCode
            assertEquals("keycode for '$id'", want, KeypadModel.keyCodeFor(id))
        }
    }

    @Test
    fun `function keycodes are the javap-verified AOSP constants`() {
        // javap -constants against android.jar (API 35, 2026-11-22).
        assertEquals(61, KeypadModel.functionKeyCode("tab"))
        assertEquals(122, KeypadModel.functionKeyCode("home"))
        assertEquals(92, KeypadModel.functionKeyCode("page-up"))
        assertEquals(93, KeypadModel.functionKeyCode("page-down"))
        assertEquals(123, KeypadModel.functionKeyCode("end"))
        assertEquals(67, KeypadModel.functionKeyCode("backspace"))
        assertEquals(21, KeypadModel.functionKeyCode("left"))
        assertEquals(19, KeypadModel.functionKeyCode("up"))
        assertEquals(20, KeypadModel.functionKeyCode("down"))
        assertEquals(22, KeypadModel.functionKeyCode("right"))
        assertEquals(62, KeypadModel.functionKeyCode("space"))
        assertEquals(66, KeypadModel.functionKeyCode("enter"))
        // 한/영 is a mode action, not a key event.
        assertNull(KeypadModel.functionKeyCode("language-toggle"))
    }

    @Test
    fun `the shift bit agrees with the registry's own reader`() {
        // One vocabulary: the bit the keypad arms is the bit the adapter reads.
        assertTrue(HunminKeyRegistry.isShiftOn(KeypadModel.META_SHIFT_ON))
        assertFalse(HunminKeyRegistry.isShiftOn(0))
    }

    @Test
    fun `key kinds carry keycodes exactly when they are key events`() {
        val rows = KeypadModel.textRows(caps("us-qwerty"), shift = false) +
            KeypadModel.symbolRows(SymbolScreen.parse(symbolAsset.readText()))
        for (key in rows.flatten()) {
            when (key.kind) {
                KeypadKey.Kind.CHARACTER, KeypadKey.Kind.DIGIT, KeypadKey.Kind.FUNCTION ->
                    assertTrue("${key.id} is a key event", key.keyCode >= 0)
                KeypadKey.Kind.SHIFT, KeypadKey.Kind.MODE,
                KeypadKey.Kind.SYMBOLS_PAGE, KeypadKey.Kind.SYMBOL ->
                    assertEquals("${key.id} is not a key event", -1, key.keyCode)
            }
        }
    }

    // ------------------------------------------------------------ symbol page

    @Test
    fun `the symbol page is the shipped asset verbatim`() {
        val screen = SymbolScreen.parse(symbolAsset.readText())
        val rows = KeypadModel.symbolRows(screen)
        assertEquals(screen.rows.map { it.keys }, rows.map { r -> r.map { it.id } })
        // Symbol keys are direct commits (KP11); the nav row is key events;
        // 한/영 is the return action (KP9).
        val symbols = rows.flatten().filter { it.kind == KeypadKey.Kind.SYMBOL }
        assertEquals(32, symbols.size)
        val mode = rows.flatten().filter { it.kind == KeypadKey.Kind.MODE }
        assertEquals(listOf("language-toggle"), mode.map { it.id })
        // KP6: no shift on the symbol page.
        assertTrue(rows.flatten().none { it.kind == KeypadKey.Kind.SHIFT })
    }

    @Test
    fun `content descs name identities, not labels`() {
        val rows = KeypadModel.textRows(caps("dubeolsik"), shift = false)
        val q = rows.flatten().first { it.id == "q" }
        assertEquals("key:q", q.contentDesc)
        val symbol = KeypadModel.symbolRows(SymbolScreen.parse(symbolAsset.readText()))
            .flatten().first { it.kind == KeypadKey.Kind.SYMBOL }
        assertEquals("symbol:${symbol.id}", symbol.contentDesc)
    }

    // ------------------------------------------------------------ state

    @Test
    fun `shift is one-shot`() {
        val s = KeypadState()
        assertEquals(0, s.shiftMeta())
        s.tapShift()
        assertTrue(s.shiftArmed)
        assertEquals(KeypadModel.META_SHIFT_ON, s.shiftMeta())
        assertFalse("consumed by the character tap", s.shiftArmed)
        assertEquals(0, s.shiftMeta())
    }

    @Test
    fun `a second shift tap cancels`() {
        val s = KeypadState()
        s.tapShift()
        s.tapShift()
        assertFalse(s.shiftArmed)
        assertEquals(0, s.shiftMeta())
    }

    @Test
    fun `the symbol page remembers the mode it was entered from`() {
        val s = KeypadState()
        s.enterSymbols(hangulMode = false)
        assertEquals(KeypadState.Page.SYMBOLS, s.page)
        // KP9: return to the ORIGINAL mode, not a toggle.
        assertFalse(s.returnToText())
        assertEquals(KeypadState.Page.TEXT, s.page)
        s.enterSymbols(hangulMode = true)
        assertTrue(s.returnToText())
    }

    @Test
    fun `entering the symbol page disarms shift`() {
        val s = KeypadState()
        s.tapShift()
        s.enterSymbols(hangulMode = true)
        assertFalse(s.shiftArmed)
    }

    @Test
    fun `a mode change under the symbol page returns to text`() {
        val s = KeypadState()
        s.enterSymbols(hangulMode = true)
        s.modeChanged(hangulMode = true)
        assertEquals("same mode: the page stays", KeypadState.Page.SYMBOLS, s.page)
        s.modeChanged(hangulMode = false)
        assertEquals("drifted mode: the return memory would be stale", KeypadState.Page.TEXT, s.page)
    }

    // ------------------------------------------------------------ KP16 shift alternate

    @Test
    fun `the frequent keys shift alternate is their hint, not the physical shift`() {
        val screen = SymbolScreen.parse(symbolAsset.readText())
        val rows = KeypadModel.textRows(caps("dubeolsik"), shift = false)
        val comma = rows.flatten().first { it.id == "," }
        val period = rows.flatten().first { it.id == "." }
        assertEquals("?", KeypadModel.shiftedFrequentAlternate(comma, screen))
        assertEquals("\"", KeypadModel.shiftedFrequentAlternate(period, screen))
        val q = rows.flatten().first { it.id == "q" }
        assertNull("letters keep the engine's shift row", KeypadModel.shiftedFrequentAlternate(q, screen))
        assertNull("no symbol page: fall back to the key path",
            KeypadModel.shiftedFrequentAlternate(comma, null))
        // The armed keycap folds the hint into its label (hint=null) — the
        // decision must still answer ? (measured bug: it read key.hint).
        val armedComma = KeypadModel.textRows(caps("dubeolsik"), shift = true)
            .flatten().first { it.id == "," }
        assertNull(armedComma.hint)
        assertEquals("?", KeypadModel.shiftedFrequentAlternate(armedComma, screen))
        // KP16 display rule: armed shift shows the alternate on the keycap.
        val armed = KeypadModel.textRows(caps("dubeolsik"), shift = true).flatten()
        assertEquals("?", armed.first { it.id == "," }.label)
        assertEquals("\"", armed.first { it.id == "." }.label)
        assertNull("hint folded into the label while armed", armed.first { it.id == "," }.hint)
        assertEquals("!", armed.first { it.id == "1" }.label)
        assertEquals("ㅃ", armed.first { it.id == "q" }.label)
        val plain = KeypadModel.textRows(caps("dubeolsik"), shift = false).flatten()
        assertEquals(",", plain.first { it.id == "," }.label)
        assertEquals("?", plain.first { it.id == "," }.hint)
    }

    // ------------------------------------------------------------ KP14 gesture

    @Test
    fun `swipe-left deletes only when long and horizontally dominant`() {
        val t = 100f
        assertTrue(KeypadModel.isSwipeLeftDelete(-150f, 0f, t))
        assertTrue("some vertical drift is still a swipe", KeypadModel.isSwipeLeftDelete(-150f, 40f, t))
        assertFalse("too diagonal: a drifting tap, not a swipe (KP5)", KeypadModel.isSwipeLeftDelete(-150f, 90f, t))
        assertFalse("short drift stays a tap", KeypadModel.isSwipeLeftDelete(-99f, 0f, t))
        assertFalse("rightward is not delete", KeypadModel.isSwipeLeftDelete(150f, 0f, t))
        assertFalse("upward is not delete", KeypadModel.isSwipeLeftDelete(0f, -150f, t))
        assertFalse("no movement is a tap", KeypadModel.isSwipeLeftDelete(0f, 0f, t))
    }

    @Test
    fun `delete units round-trip through their stored values`() {
        assertEquals(4, DeleteUnit.entries.size)
        for (u in DeleteUnit.entries) assertEquals(u, DeleteUnit.of(u.pref))
        assertEquals("the provisional default is the least destructive", DeleteUnit.KEY, DeleteUnit.of(null))
        assertEquals(DeleteUnit.KEY, DeleteUnit.of("bogus"))
    }

    // ------------------------------------------------------------ KP12 visibility

    @Test
    fun `the grid auto-shows only without a physical keyboard`() {
        // Configuration constants, javap -constants android.jar API 35
        // (2026-11-22): KEYBOARD_NOKEYS=1, KEYBOARD_QWERTY=2, KEYBOARD_12KEY=3,
        // HARDKEYBOARDHIDDEN_UNDEFINED=0, NO=1, YES=2.
        val nokeys = 1; val qwerty = 2; val twelve = 3
        val undefined = 0; val exposed = 1; val hidden = 2
        // No physical keyboard: auto-shown, whatever the hidden field says.
        assertTrue(KeypadVisibility.autoShown(nokeys, undefined, false))
        assertTrue(KeypadVisibility.autoShown(nokeys, exposed, false))
        // Physical keyboard exposed: hidden. THIS is KP12's requirement.
        assertFalse(KeypadVisibility.autoShown(qwerty, exposed, false))
        assertFalse(KeypadVisibility.autoShown(twelve, exposed, false))
        // Keyboard present but hidden (lid closed / slider): shown again.
        assertTrue(KeypadVisibility.autoShown(qwerty, hidden, false))
        assertTrue(KeypadVisibility.autoShown(qwerty, undefined, false))
        // The platform's explicit user opt-in is not an automatic appearance,
        // and it shows the grid even with a keyboard exposed.
        assertTrue(KeypadVisibility.autoShown(qwerty, exposed, true))
        assertTrue(KeypadVisibility.autoShown(nokeys, exposed, true))
    }
}