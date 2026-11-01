/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1: the automatic Hangul/English switch, through the REAL JNI shim and
 * the real engine (issue #174).
 *
 * What this pins, and each line is a way the feature can look shipped and not be:
 *
 *  - the ENGINE decides, and the adapter's only job is to adopt the answer.
 *    An English-valid buffer is judged Latin and delivers its English reading;
 *    a Hangul-valid one is not switched. Both directions, because a case that
 *    only drives the English one passes on an adapter that switches on every
 *    boundary.
 *  - the CONTROL. The same keys with `autoswitch` off answer -1, so the two
 *    cases above cannot pass because the judgement fired for another reason
 *    (a dictionary that happened to decide, a boundary that always answers).
 *  - the value is R13-정정's ONE value: written through the adapter, read back
 *    from the engine, and never cached by the caller. The gate is asserted in
 *    both states, because "off because the person chose off" and "off because
 *    the data is missing" are the two sentences a settings row has to tell
 *    apart.
 *
 * NOT asserted here: whether a hardware key reaches the IME at all. That is the
 * service's question and tier E2's, exactly as in HunminAdapterTest.
 */
package org.hunmin.ime

import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import java.io.File

class AutoSwitchTest {

    /* hunmin.h HUNMIN_LANGUAGE_*: HANGUL 0, LATIN 1; -1 is the engine's "nothing
     * judged". AOSP metaState bits as in HunminAdapterTest (read out of
     * android.jar, API 35). */
    private val latin = 1
    private val nothingJudged = -1
    private val shiftOn = 0x00000001 // META_SHIFT_ON

    private var ctx: Long = 0

    private val repoRoot: File by lazy {
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) } ?: File("../..").canonicalFile
    }
    private val dataDir: File by lazy { File(repoRoot, "libhunmin/data").canonicalFile }

    @Before
    fun setUp() {
        val shim = System.getenv("HUNMIN_JNI_LIB")
        assumeTrue(
            "HUNMIN_JNI_LIB is unset; run tools/android-unit.sh, which builds and " +
                "points at the host JNI shim (got '$shim')",
            !shim.isNullOrEmpty() && File(shim).isFile,
        )
        assumeTrue("libhunmin data tables missing at $dataDir", dataDir.isDirectory)
        ctx = HunminNativeBridge.nativeCtxNew(dataDir.absolutePath)
        assertTrue("hunmin_ctx_new found no layout", ctx != 0L)
    }

    @After
    fun tearDown() {
        if (ctx != 0L) HunminNativeBridge.nativeCtxFree(ctx)
    }

    /**
     * The two-slot layer the service actually builds (FE7): Hangul on
     * sin3-p2, English on us-qwerty, word-at-boundary delivery. The judgement
     * needs the pair -- one language has no second reading.
     */
    private fun pair(hangul: String = "sin3-p2"): HunminAdapter =
        HunminAdapter(ctx, hangul, "us-qwerty", "word")

    /** A keypress for [ch], with the shift bit when it is an uppercase letter. */
    private fun press(ch: Char): Pair<Int, Int> {
        val key = HunminKeyRegistry.all().firstOrNull { it.unshifted == ch }
            ?: HunminKeyRegistry.all().firstOrNull { it.shifted == ch }
            ?: error("no AOSP keycode for '$ch'")
        val shift = if (key.shifted == ch && ch.isUpperCase()) shiftOn else 0
        return key.keyCode to shift
    }

    /** Type [keys] through the adapter, reading the judgement after every key
     *  exactly as the service does. Returns the last non-null judgement. */
    private fun type(adapter: HunminAdapter, ic: RecordingInputConnection, keys: String): Int {
        var judged = nothingJudged
        for (ch in keys) {
            val (code, shift) = press(ch)
            adapter.onKey(code, shift, ic)
            val j = adapter.takeJudgement()
            if (j != nothingJudged) judged = j
        }
        return judged
    }

    // ------------------------------------------------------------ the decision

    @Test
    fun `an English-valid word is judged Latin and delivered in English`() {
        val adapter = pair()
        val ic = RecordingInputConnection()
        assertEquals("the data gate accepts ON with the shipped list", 1, adapter.setAutoswitch(true))
        assertTrue("premise: this install carries the modes data", adapter.autoswitchDataPresent())

        /* `computer` on the Android default pair: English-valid, and its sin3-p2
         * reading is not well-formed Hangul, so R8's two readings disagree and
         * the word is delivered in the language it was judged. The space is the
         * boundary the judgement fires on. Measured 2026-11-01 through this shim:
         * the same keys with the switch off deliver the sin3-p2 reading instead
         * (see the control below). */
        assertEquals(latin, type(adapter, ic, "computer "))
        assertEquals("the judged word is delivered in the language it was judged", "computer", ic.text())
        adapter.close()
    }

    @Test
    fun `a Hangul-valid word is not switched`() {
        /* dubeolsik because that is where these keys ARE syllables: `gksrmf` is
         * 한글, whose Latin reading is not a dictionary word, so R8 says keep the
         * mode. sin3-p2's own 안녕 sequence is `dkssud`, which that layout renders
         * as a non-word (measured), so it would leave this case with no valid
         * Hangul buffer to be about. */
        val adapter = pair("dubeolsik")
        val ic = RecordingInputConnection()
        assertEquals(1, adapter.setAutoswitch(true))

        assertEquals(
            "a Hangul-valid buffer must leave the mode alone",
            nothingJudged,
            type(adapter, ic, "gksrmf "),
        )
        assertEquals("한글", ic.text())
        adapter.close()
    }

    @Test
    fun `with the switch off the same keys are not judged`() {
        /* The control for the case above (issue #174's checklist): the SAME keys
         * on the SAME pair, with the engine's default OFF (B3). If this answered
         * `latin` too, "it switched" would be a property of the fixture rather
         * than of the switch. */
        val adapter = pair()
        val ic = RecordingInputConnection()
        assertEquals("OFF with no option call at all", 0, adapter.autoswitch())

        assertEquals(nothingJudged, type(adapter, ic, "computer "))
        /* The control's text is the HANGUL reading the layout produces with no
         * judgement to rewrite it -- the text the switched case replaces with
         * `computer`. If both runs delivered the same string, "the judgement
         * fired" would be indistinguishable from "it did not". */
        val asTyped = ic.text()
        assertTrue("premise: sin3-p2 renders these keys as something", asTyped.isNotEmpty())
        assertFalse("no judgement, so the Latin reading must not appear", asTyped == "computer")
        adapter.close()
    }

    @Test
    fun `the switch does not survive being asked twice`() {
        /* take_judgement is CONSUMING (hunmin.h): the service reads it once per
         * key, and a second read must not switch the mode again. Pinned here
         * because the service's per-key loop is the only other reader. */
        val adapter = pair()
        val ic = RecordingInputConnection()
        assertEquals(1, adapter.setAutoswitch(true))
        for (ch in "computer") {
            val (code, shift) = press(ch)
            adapter.onKey(code, shift, ic)
            adapter.takeJudgement() // the service's per-key read
        }
        val (spaceCode, spaceShift) = press(' ')
        adapter.onKey(spaceCode, spaceShift, ic)
        assertEquals("the boundary answers once", latin, adapter.takeJudgement())
        assertEquals("a consumed judgement reads -1", nothingJudged, adapter.takeJudgement())
        assertEquals("computer", ic.text())
        adapter.close()
    }

    // -------------------------------------------------- R13-정정's one value

    @Test
    fun `the value the settings row shows is the engine's read-back`() {
        val adapter = pair()
        assertEquals("OFF on a fresh layer (B3, ruled 2026-10-11)", 0, adapter.autoswitch())
        assertEquals(1, adapter.setAutoswitch(true))
        assertEquals("the settings row reads the engine, not its own copy", 1, adapter.autoswitch())
        assertEquals(0, adapter.setAutoswitch(false))
        assertEquals(0, adapter.autoswitch())
        adapter.close()
    }

    @Test
    fun `the data gate is answered by the engine, not by the frontend`() {
        /* The row's three sentences: present+off is "off, chosen"; absent is
         * "cannot be enabled here". The engine answers the presence question
         * from the same directory search every other data table uses (issue
         * #169), so a frontend that assembled the path itself would be a second
         * answer waiting to disagree. */
        val adapter = pair()
        assertTrue(
            "libhunmin/data carries modes/en-US.json -- the shipped install must be able to enable it",
            adapter.autoswitchDataPresent(),
        )
        adapter.close()
    }

    @Test
    fun `the key the row names is the engine's chord`() {
        /* R13-정정's "explainable" bullet: the dialog asks for the chord and
         * renders it; it never spells one. Ctrl is HUNMIN_MOD_CTRL and the ruled
         * AUTOSWITCH default (docs/SPEC-MODES.md §E5). */
        val adapter = pair()
        val chord = adapter.specialChord(SpecialFunction.AUTOSWITCH)
        assertEquals("the engine's default chord for the automatic switch", 0x1, chord)
        assertEquals("Ctrl+한/영", SpecialFunction.chordLabel(chord))
        adapter.close()
    }

    @Test
    fun `an unbound function has no chord to name`() {
        assertEquals("no chord means no key to advertise", "", SpecialFunction.chordLabel(0))
        assertEquals("Alt+한/영", SpecialFunction.chordLabel(0x2))
        assertEquals("Ctrl+Alt+한/영", SpecialFunction.chordLabel(0x3))
    }

    @Test
    fun `the adapter reports no value when it has no layer`() {
        /* null, not 0: the settings row must be able to say "cannot answer"
         * instead of showing an OFF it did not read. */
        val adapter = pair()
        adapter.close()
        assertNull(adapter.autoswitch())
        assertNull(adapter.setAutoswitch(true))
        assertFalse(adapter.isUsable)
    }

    @Test
    fun `the layer carries the value across a language toggle`() {
        /* The engine's own case (tier A) pins this too; here it is asserted
         * through the adapter the service uses, because the service's 한/영 path
         * switches the SAME layer and a value lost there is invisible until the
         * judgement stops firing. */
        val adapter = pair()
        assertEquals(1, adapter.setAutoswitch(true))
        assertTrue("the pair switches language", adapter.toggleLanguage())
        assertEquals(1, adapter.autoswitch())
        assertNotNull(adapter.autoswitch())
        adapter.close()
    }
}
