/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1: the Android adapter, against a mocked InputConnection.
 *
 * What this can and cannot prove, stated first because getting it wrong is how
 * a green suite means nothing:
 *
 *   CAN:  that a keycode reaches the right engine character; that the character
 *         reaches the engine; that exactly one commit or flush reaches the
 *         client per keystroke; that a declined key reaches the client; that
 *         CapsLock is handed over as an option; that spoken text bypasses the
 *         jamo pipeline (invariant 5); that the adapter never touches
 *         surrounding text (I9).
 *
 *   CANNOT:  whether the platform routes hardware keys to an IME at all. That
 *         is the service's question, not the engine's, and it is answered by
 *         tier E2 on a device (tools/android-e2e.sh). Nothing here can fail if
 *         the answer is no, which is why E2 exists and is not optional.
 *
 * The engine is REAL here — hunmin_ic_* via JNI — not a stub. A test with a
 * mocked engine would assert that the adapter forwards to a mock, which is the
 * definition of a test that cannot fail. libhunmin.so is a host .so here (the
 * JVM test loads the desktop build), so this tier needs no emulator and no
 * Android runtime; it is fast enough to run on every commit.
 *
 * The golden expectations are read from the repo's tests/golden corpus rather than typed
 * in: the same corpus tier A replays, so a divergence between the Android
 * adapter and the host adapter shows up as a test failure instead of as two
 * subtly different engines.
 */
package org.hunmin.ime

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import java.io.File

class HunminEngineTest {

    private var ctx: Long = 0

    private val repoRoot: File by lazy {
        // HUNMIN_REPO_ROOT is set by build.gradle.kts because a Gradle test
        // JVM's working directory is the module dir, and "../../.." from there
        // is not the repo root -- it depends on which module is being built.
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) }
            ?: File("../..").canonicalFile
    }

    private val dataDir: File by lazy {
        // The repo's own tables, the same ones tier A and tier E0 replay.
        File(repoRoot, "libhunmin/data").canonicalFile
    }

    @Before
    fun setUp() {
        // Skips (rc 77 in ctest) rather than fails when the host shim was not
        // built: tools/build-host-jni.sh needs a JDK, and CI's linux job may not
        // have one. A missing build step must not read as a passing suite, so
        // the skip is reported as a skip -- see tools/android-unit.sh.
        // Each precondition reports WHICH one failed. An earlier revision had
        // two bare assumeTrue calls and the whole suite reported "skipped" with
        // an empty message, which is how a green Gradle run came to mean
        // nothing at all -- see tools/android-unit.sh.
        val shim = System.getenv("HUNMIN_JNI_LIB")
        assumeTrue(
            "HUNMIN_JNI_LIB is unset or empty; run tools/android-unit.sh, which " +
                "sets it to the host build of the JNI shim (got '$shim')",
            !shim.isNullOrEmpty(),
        )
        assumeTrue(
            "the host JNI shim does not exist at $shim; run tools/build-host-jni.sh",
            File(shim!!).isFile,
        )
        assumeTrue("libhunmin data tables missing at $dataDir", dataDir.isDirectory)
        ctx = HunminNative.nativeCtxNew(dataDir.absolutePath)
        assertTrue("hunmin_ctx_new found no layout in ${dataDir.absolutePath}", ctx != 0L)
    }

    /** Type [keys] as keycodes through the adapter and return what the client saw. */
    private fun type(vararg keys: Char, layout: String = "sin3-p2"): RecordingInputConnection {
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, layout)
        assertTrue("layout $layout is not usable", engine.isUsable)
        for (k in keys) engine.typeChar(k, ic)
        engine.close()
        return ic
    }

    /** The AOSP keycode for a character, via the adapter's own table. */
    private fun keyCodeOf(ch: Char): Int = press(ch).first

    /** Feed [ch] as a keypress would deliver it, shift bit included. */
    private fun HunminEngine.typeChar(ch: Char, target: RecordingInputConnection): Boolean {
        val (code, meta) = press(ch)
        return onKey(code, meta, target)
    }

    // AOSP metaState bits, read out of android.jar with javap -constants
    // (2026-10-03, API 35). Hard-coded because android.view.KeyEvent cannot be
    // touched from a plain JVM. An earlier revision of THIS file guessed them and
    // got two of four wrong -- 0x2000 is META_CTRL_RIGHT_ON, not ALT, and 0x10000
    // is META_META_ON, not CapsLock -- which showed up as "Alt is not a chord"
    // and "CapsLock must not be a modifier chord" failing. Cross-check these
    // against android.jar if a modifier test starts failing.
    private val shiftOn get() = 0x00000001     // META_SHIFT_ON          = 1
    private val altOn get() = 0x00000002       // META_ALT_ON            = 2
    private val ctrlOn get() = 0x00001000      // META_CTRL_ON           = 4096
    private val metaOn get() = 0x00010000      // META_META_ON           = 65536
    private val capsOn get() = 0x00100000      // META_CAPS_LOCK_ON      = 1048576

    /**
     * A keypress for [ch], including the shift bit when [ch] is an uppercase
     * letter.
     *
     * Needed because AOSP has no KEYCODE_Q: the same physical key produces 'q'
     * and 'Q' depending on metaState, exactly as on X11. Typing Q therefore means
     * KEYCODE_Q with META_SHIFT_ON, and a test that passed metaState 0 for an
     * uppercase key was asking the adapter for the wrong character.
     */
    private fun press(ch: Char): Pair<Int, Int> {
        val key = HunminKeys.all().firstOrNull { it.unshifted == ch }
            ?: HunminKeys.all().firstOrNull { it.shifted == ch }
            ?: error("no AOSP keycode for '$ch' -- the table is missing a layout key")
        val shift = if (key.shifted == ch && ch.isUpperCase()) shiftOn else 0
        return key.keyCode to shift
    }

    // ------------------------------------------------------------ keycodes

    @Test
    fun `every layout key has a keycode`() {
        // The 47 keys libhunmin/data/layouts/*.json reference. If one has no
        // AOSP keycode the adapter would pass it through and the layout would be
        // silently unusable on Android -- a defect no host tier can see.
        val have = HunminKeys.all().flatMap { listOfNotNull(it.unshifted, it.shifted) }.toSet()
        // Scan the layouts' own "keys" arrays rather than every quoted string in
        // the file: a regex over the whole document picks up JSON syntax and
        // Korean text in compose tables, so the "keys" it reports have nothing
        // to do with the keys a layout uses.
        val need = dataDir.resolve("layouts").listFiles { f -> f.name.endsWith(".json") }
            .orEmpty()
            .flatMap { f -> layoutKeysIn(f.readText()) }
            .toSet()
        // A layout file also quotes Hangul names and layout ids; only the ASCII
        // printables and space are keys, so the scan is narrowed to what
        // tools/keymap-fixture.py treats as the key set.
        val missing = (need - have).filter { it.isLetterOrDigit() || it in PUNCT || it == ' ' }
        assertTrue("layout keys with no AOSP keycode: $missing", missing.isEmpty())
        // 47 keys across every layout in libhunmin/data -- the number
        // tools/keymap-fixture.py reports, so a drift here is a drift there.
        // It is 47 distinct CHARACTERS; keycodes number 48 because KEYCODE_DEL
        // carries backspace, which no layout file names.
        assertEquals("the scan found an unexpected number of layout keys", 47, need.size)
    }

    @Test
    fun `keycodes are not ASCII`() {
        // The mistake this file exists to prevent, made once already: AOSP
        // numbers KEYCODE_0 at 7 and KEYCODE_A at 29, so keyCode.toChar() is
        // wrong for every single key. Assert the disagreement explicitly so a
        // future "simplification" back to ASCII fails here.
        val a = HunminKeys.all().first { it.unshifted == 'a' }
        val zero = HunminKeys.all().first { it.unshifted == '0' }
        assertEquals("KEYCODE_A", 29, a.keyCode)
        assertEquals("KEYCODE_0", 7, zero.keyCode)
        assertTrue(a.keyCode != 'a'.code)
        assertTrue(zero.keyCode != '0'.code)
    }

    @Test
    fun `shift is data not a modifier`() {
        val shifted = HunminKeys.engineChar(keyCodeOf('k'), shiftOn)
        assertEquals('K', shifted)
        // Shift must not be classified as a chord -- that would drop every
        // shifted key to the client and silently break sin3-p2's symbol tiers.
        assertFalse(HunminKeys.isModifierChord(shiftOn))
    }

    @Test
    fun `ctrl chords are refused`() {
        assertTrue(HunminKeys.isModifierChord(ctrlOn))
        assertTrue(HunminKeys.isModifierChord(ctrlOn or shiftOn))
        assertTrue(HunminKeys.isModifierChord(altOn))
    }

    @Test
    fun `capslock inverts the shift and is not itself a chord`() {
        assertFalse("CapsLock must not be a modifier chord", HunminKeys.isModifierChord(capsOn))
        // The engine must receive the UNSHIFTED letter plus the caps option,
        // never the uppercase form: sin3-p2-yet reads caps as a separate option
        // (SPEC-P2.md) and an uppercase K would be read as a symbol-tier row.
        assertEquals('k', HunminKeys.engineChar(keyCodeOf('k'), capsOn))
    }

    // ------------------------------------------------------------ composition

    @Test
    fun `a four-key sequence composes through the adapter`() {
        // Measured, not assumed. `kfcQ` on sin3-p2 is the case tier D3 already
        // pins (tests/e2e/cases.tsv), so this is the same expectation reached
        // through the Android key path instead of X11's. It was tempting to use
        // `dkssud` here because it is the famous "한" sequence -- but under
        // sin3-p2 it composes to 곈 with a 디 preedit, and it only becomes 한
        // under dubeolsik. Written down here so the next reader does not have to
        // rediscover it:
        //   $ hunmin-sim --layout sin3-p2 --keys dkssud   -> ㅎ곈, preedit 디
        //   $ hunmin-sim --layout dubeolsik --keys dkssud -> 한
        // hunmin-sim --layout sin3-p2 --keys kfcQ commits 각 and leaves ㅒ in the
        // preedit: Q is the shifted form of q, and uppercase Q in sin3-p2 is a
        // symbol-tier key, not a second consonant.
        val ic = type('k', 'f', 'c', 'Q')
        // hunmin-sim --layout sin3-p2 --keys kfcQ commits 각 (preedit ㅒ).
        assertEquals("각", ic.text())
        // And nothing reached around the engine (I9):
        assertEquals(
            "no surrounding-text access (I9): ${ic.violations}",
            emptyList<String>(), ic.violations,
        )
    }

    @Test
    fun `the layout id decides the output, not the key sequence`() {
        // The same keys, two layouts, two answers -- which is what makes "layouts
        // are data" a claim rather than a slogan. sin3-p2 turns dkssud into 곈;
        // dubeolsik turns it into 한.
        assertEquals("ㅎ곈", type('d', 'k', 's', 's', 'u', 'd', layout = "sin3-p2").text())
        assertEquals("안", type('d', 'k', 's', 's', 'u', 'd', layout = "dubeolsik").text())
    }

    @Test
    fun `preedit is presented while composing and cleared after`() {
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        engine.typeChar('k', ic)
        assertEquals("ㄱ", ic.composings.last())
        engine.typeChar('f', ic)
        assertEquals("가", ic.composings.last())
        // Space is what finishes a syllable in a real layout: the engine declines
        // it, which is how the composition is committed and the preedit cleared.
        // That path (engine declines -> flush -> updatePreedit) is what clears
        // the composing region; flush() on its own only commits.
        engine.onKey(62 /* KEYCODE_SPACE */, 0, ic)
        assertEquals("가", ic.text())
        assertEquals("", ic.composings.last())
        engine.close()
    }

    @Test
    fun `space is declined by the engine and reaches the client`() {
        // Invariant 3: consumption follows the ENGINE's decision, not the
        // keycode table's. Space composes nothing, so it must pass through --
        // and an IME that ate space would be unusable in every text field.
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        engine.typeChar('k', ic)
        val consumed = engine.onKey(62 /* KEYCODE_SPACE */, 0, ic)
        assertFalse("space must not be consumed", consumed)
        engine.close()
    }

    /*
     * --- The 2026-10-11 Hangul/English and Hanja fixes, as invariants ---
     *
     * Three bugs, one shape. Every one of them lost characters the user had
     * already typed, and every one of them was invisible on the build machine:
     *
     *   1. The Hangul/English toggle (한/영 key) did not flush, so the syllable
     *      composing at the moment of the switch vanished into the engine's
     *      buffer and reappeared only after switching back. Fixed by flushing
     *      both engines in toggleHangulEnglish().
     *   2. Alt alone -- delivered as its own ACTION_DOWN before Alt+X for Hanja
     *      -- reached HunminEngine.onKey, whose isModifierChord path flushes.
     *      The Hanja lookup then found an empty preedit. Fixed by not routing a
     *      bare modifier to the engine at all.
     *   3. The picked Hanja was never committed: OverlayActivity called
     *      setResult(), but an IME is a Service and receives no activity result.
     *      Fixed with an explicit-package broadcast the service commits.
     *
     * The shared property is what tier E1 can state, because flush() and
     * commitExternalText() are adapter-level and the service wiring is tier E2:
     *
     *      whatever interrupts a composition, the composing characters must
     *      reach the client -- either committed, or still composing, but never
     *      neither.
     *
     * Each test below breaks that property in a specific way, so a regression
     * in the flush contract fails here rather than on a device.
     */

    @Test
    fun `a flush during composition commits it instead of dropping it`() {
        // Bug 1's core. The service flushes on a mode switch; if flush did not
        // commit what was composing, the typed syllable would be lost exactly
        // as the user reported. flush() must also clear the preedit, or the
        // stale composition stays on screen over the committed text.
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        engine.typeChar('k', ic)
        engine.typeChar('f', ic)
        assertEquals("가", ic.composings.last())

        engine.flush(ic)
        assertEquals("the composing syllable must reach the client", "가", ic.text())
        assertEquals("", ic.composings.last())
        engine.close()
    }

    @Test
    fun `a Hanja commit replaces the composition, never appends after it`() {
        // Bug 3's core: the Hanja broadcast path commits through commitHanja.
        // If it did not take the in-flight composition first, the Hangul would
        // stay in the preedit and the Hanja would be committed after it --
        // "한恨" instead of "恨". That is what commitExternalText would do,
        // and it is why the Hanja path does not use it (see commitHanja's doc).
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        engine.typeChar('k', ic)
        engine.typeChar('f', ic)

        engine.commitHanja(ic, "恨")
        assertEquals("the Hangul must not survive alongside the Hanja", "恨", ic.text())
        assertEquals("", ic.composings.last())
        engine.close()
    }

    @Test
    fun `a modifier chord does not silently swallow the composition`() {
        // Bug 2's core, at the layer E1 owns. HunminEngine.onKey flushes on a
        // modifier chord -- correct for the engine, which must not compose
        // Ctrl/Alt -- but the flush must COMMIT, not discard. The service guard
        // (do not route a bare modifier here at all) is tier E2; this asserts
        // the engine half, which is what would have lost the syllable if the
        // guard were the only change and flush were broken.
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        engine.typeChar('k', ic)
        engine.typeChar('f', ic)

        engine.onKey(45 /* KEYCODE_X */, altOn, ic)
        assertEquals("a modifier chord must commit, not drop", "가", ic.text())
        engine.close()
    }

    @Test
    fun `a four-key sequence commits and leaves the rest composing`() {
        // Invariant 2, the one tier A cannot see. Two commits here would mean
        // every syllable appears twice in every application. FRONTEND-CONTRACT
        // §4 records that dropping the commit/flush split passes the fcitx5 D1
        // suite, so this assertion is here for a reason that is not paranoia.
        // Measured: `hunmin-sim --layout dubeolsik --keys dkssud` commits 안 and
        // leaves 녕 in the preedit -- the final ㄴ has no 받침 partner yet, so it
        // cannot become a syllable until another consonant arrives. Asserting 한
        // here (as an earlier revision did) was an assumption, not a measurement.
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "dubeolsik")
        for (c in "dkssud") engine.typeChar(c, ic)
        engine.close()
        assertEquals(listOf("안"), ic.commits)
        assertEquals("녕", ic.composings.last())
    }

    @Test
    fun `an unmapped key flushes and is not consumed`() {
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        engine.typeChar('k', ic)
        val consumed = engine.onKey(9999 /* not a keycode */, 0, ic)
        assertFalse(consumed)
        // The pending composition must have been committed, not dropped.
        assertTrue("pending composition lost: ${ic.commits}", ic.commits.any { it.isNotEmpty() })
        engine.close()
    }

    // ------------------------------------------------------------ invariant 5

    @Test
    fun `external text bypasses the jamo pipeline`() {
        // PLAN.md invariant 5. The whole point: spoken Hangul must NOT be fed to
        // hunmin_ic_process, or it is composed a second time. A spoken "안녕"
        // fed as keystrokes would compose into something else entirely.
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        engine.commitExternalText(ic, "안녕하세요")
        assertEquals("안녕하세요", ic.text())
        engine.close()
    }

    @Test
    fun `external text commits in-flight composition first`() {
        // Ordering, not just content: the user typed "가" and then spoke. The
        // result must be "가" then the speech, because that is the order they
        // produced them in.
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        engine.typeChar('k', ic)
        engine.typeChar('f', ic)
        engine.commitExternalText(ic, "안녕")
        assertEquals("가안녕", ic.text())
        engine.close()
    }

    @Test
    fun `external text does not disturb later typing`() {
        // The tier state must survive: speaking is not a reset. If it were, the
        // 전처리수식 flags would drop and a real user's next keystroke would
        // compose under the wrong rules.
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        engine.commitExternalText(ic, "abc")
        engine.typeChar('k', ic)
        engine.typeChar('f', ic)
        assertEquals("가", ic.composings.last())
        engine.close()
    }

    @Test
    fun `malformed external text is refused rather than committed`() {
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        engine.commitExternalText(ic, "\uD800")   // lone surrogate
        assertEquals("malformed text must not reach the client", "", ic.text())
        engine.close()
    }

    // ------------------------------------------------------------ goldens

    @Test
    fun `adapter replays a tier A golden byte-for-byte`() {
        // The cross-platform check. If the Android adapter and the host adapter
        // ever disagree, this is where it shows -- and the expectation is the
        // SAME corpus tier A uses, not a second transcription of it.
        val golden = File(repoRoot, "tests/golden/manual-sin3-p2.jsonl")
        assumeTrue("golden corpus missing", golden.isFile)

        // Parse the tier A format properly rather than with a regex over the
        // whole line. The .jsonl lines are one JSON object each, and a regex
        // that also matched the "meta" lines would silently pick up the case
        // labels as if they were keystrokes -- a test that passes on a
        // misparse is worse than no test. Keys come from the "in"."ch" field
        // and expectations from "commit", which is exactly hunmin-sim's
        // --trace output (docs/trace-spec.md).
        // Split the corpus into CASES, not into one long keystroke stream.
        // Each {"meta":{"case":...}} line opens a case that starts from a fresh
        // engine -- hunmin-sim creates a new hunmin_ic per case, and a trailing
        // preedit never carries into the next one. Concatenating every case's
        // keys into a single ic interleaves them and compares nonsense:
        // case "k" followed by case "kf" would type "kkf" and compose 까, not 가.
        // That was measured, not theorised: an earlier revision of this test
        // reported `expected:<ㅅㄱ> but was:<까과>` for exactly this reason.
        data class Case(val name: String, val keys: MutableList<Char>, val commits: MutableList<String>)

        val cases = mutableListOf<Case>()
        for (line in golden.readLines()) {
            if (line.isBlank()) continue
            val ch = FIELD_CH.find(line)?.groupValues?.get(1)
            if (ch == null || ch.length != 1) {
                // A "meta" line: a case boundary when it names a case.
                val name = FIELD_CASE.find(line)?.groupValues?.get(1)
                if (name != null) cases.add(Case(name, mutableListOf(), mutableListOf()))
                continue
            }
            if (cases.isEmpty()) cases.add(Case("<first>", mutableListOf(), mutableListOf()))
            cases.last().keys.add(ch[0])
            cases.last().commits.add(FIELD_COMMIT.find(line)?.groupValues?.get(1) ?: "")
        }
        assertTrue(
            "the golden parse produced no cases -- the field regexes do not " +
                "match this corpus at all, and a zero-case comparison passes " +
                "vacuously",
            cases.isNotEmpty() && cases.any { it.keys.isNotEmpty() },
        )

        // Replay each case into its OWN engine, exactly as hunmin-sim does.
        for (c in cases) {
            if (c.keys.isEmpty()) continue
            val ic = RecordingInputConnection()
            val engine = HunminEngine(ctx, "sin3-p2")
            for (k in c.keys) engine.typeChar(k, ic)
            // NO flush here, and that is deliberate. The golden corpus records
            // what each keystroke COMMITTED, not the end-of-case state: case "k"
            // types one consonant, leaves ㄱ in the preedit, and expects an empty
            // commit. hunmin-sim does not flush either. Flushing at case end
            // turned every trailing-preedit case into a mismatch
            // (`expected:<> but was:<ㄱ>`) that says nothing about the adapter.
            engine.close()

            assertEquals(
                "case '${c.name}' (keys=${c.keys.joinToString("")}) committed a " +
                    "different string than the tier A golden",
                c.commits.filter { it.isNotEmpty() }.joinToString(""),
                ic.commits.filter { it.isNotEmpty() }.joinToString(""),
            )
            assertEquals(
                "case '${c.name}' touched surrounding text (I9): ${ic.violations}",
                emptyList<String>(), ic.violations,
            )
        }
    }

    // ------------------------------------------------------------ misc

    @Test
    fun `one keystroke yields at most one commit call`() {
        // FRONTEND-CONTRACT section 5 rule 1: exactly one of commit and flush
        // per keypress.
        //
        // MEASURED LIMITATION, recorded because it is not obvious and it is the
        // same finding as FRONTEND-CONTRACT section 4's "still open" note for
        // the desktop adapter. Replacing flush() with commit() on the declined
        // path is NOT observable, and tools/android-mutations.sh confirms it:
        // the planted mutation leaves this suite green. The reason is in
        // libhunmin/src/ic.c -- every decline goes through ic_push(), which
        // clears the buffer and calls save_commit() BEFORE returning false
        // (ic.c:149-153). So by the time the adapter chooses a branch, the
        // in-flight syllable is already in the commit buffer and the preedit is
        // already empty. hunmin_ic_commit and hunmin_ic_flush therefore return
        // the same text and leave the same preedit on that path.
        //
        // The `else` is kept because it is the correct call (flush is what a
        // decline MEANS) and because the equivalence is a property of the
        // current engine, not a guarantee: a layout that declined without saving
        // would lose the syllable. Asserting commit COUNT is still worth doing,
        // because it is the property that would break first.
        //
        // The sequence is the one that forces both readers to have something:
        // "dkss" leaves 안 committed and ㄴ in the preedit, then a space makes the
        // engine decline, which is the flush path. Under the mutation the space
        // commits twice.
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "dubeolsik")
        engine.typeChar('d', ic)
        engine.typeChar('k', ic)
        engine.typeChar('s', ic)
        assertEquals("nothing commits while the syllable is still forming", 0, ic.commits.size)

        engine.typeChar('s', ic)
        // The fourth key completes 안 and commits it: one call.
        assertEquals("the committing keystroke must commit exactly once",
            1, ic.commits.size)
        assertEquals(listOf("안"), ic.commits)

        // Now the key the engine DECLINES. Space is the one: it finishes the
        // syllable, which means hunmin_ic_process returns false, which is the
        // only way to reach the else-branch where flush() lives. An earlier
        // revision of this test used space while the preedit was EMPTY, where
        // flush has nothing to emit and the mutation is invisible -- the suite
        // stayed green with the split deleted. The composition has to be live.
        // THE KEY INSIGHT, and the reason this mutation is hard to catch. When
        // the engine declines a key it has ALREADY put the in-flight syllable
        // into the commit buffer -- ic_push() calls save_commit() before
        // returning false (libhunmin/src/ic.c:149-153). So on this path
        // hunmin_ic_commit() and hunmin_ic_flush() return THE SAME TEXT, and the
        // concatenated output is identical either way.
        //
        // What differs is the PREEDIT: hunmin_ic_flush() sets ic->preedit[0]=0
        // before saving (ic.c:646), and hunmin_ic_commit() does not touch the
        // preedit at all. So the only observable consequence of taking the wrong
        // branch is that the composing region is left on screen holding a
        // syllable that has just been committed -- text the user sees twice.
        //
        // This is the same defect FRONTEND-CONTRACT section 4 records as passing
        // the desktop D1 suite. Asserting the commit COUNT does not catch it;
        // asserting the PREEDIT does.
        val beforeSpace = ic.commits.size
        assertFalse(
            "space must be declined by the engine",
            engine.onKey(62 /* KEYCODE_SPACE */, 0, ic),
        )
        assertEquals(
            "the flush path must add exactly one commitText call; commits are " +
                "${ic.commits}",
            beforeSpace + 1, ic.commits.size,
        )
        assertEquals("안ㄴ", ic.text())
        assertEquals(
            "after a flush the composing region must be EMPTY, not still showing " +
                "a syllable that was just committed",
            "", ic.composings.last(),
        )
        engine.close()
    }

    @Test
    fun `capslock reaches the engine as an option`() {
        // FRONTEND-CONTRACT section 5 rule 5. Only sin3-p2-yet reads `caps`
        // (SPEC-P2.md), so a test on sin3-p2 asserts nothing and the set_option
        // call can be deleted with the suite still green -- which is exactly
        // what the mutation test found. This one pins the layout to sin3-p2-yet
        // and replays its caps pair from tests/golden/manual-sin3-p2-yet.jsonl.
        val capsOn = capsOn
        val open = RecordingInputConnection()
        val blocked = HunminEngine(ctx, "sin3-p2-yet")
        blocked.typeChar('k', open)
        assertEquals("ㄱ", open.composings.last())
        blocked.close()

        // The caps pair, straight out of tests/golden/manual-sin3-p2-yet.jsonl
        // case "k/" (caps=1): k composes ㄱ, then / commits ㄱ and composes ㅋ.
        // Taken from the corpus rather than invented, because an earlier
        // revision of this test asserted that caps makes `k` itself into ㅋ --
        // measured, that is false: without the caps option the golden's own
        // case produces ㄱ, so a test written that way would have been testing
        // a behaviour the engine does not have.
        val capped = RecordingInputConnection()
        val e2 = HunminEngine(ctx, "sin3-p2-yet")
        val (kCode, _) = press('k')
        e2.onKey(kCode, capsOn, capped)      // k with caps held
        assertEquals("ㄱ", capped.composings.last())
        val (slashCode, _) = press('/')
        e2.onKey(slashCode, capsOn, capped)  // / with caps held -> ㅋ
        assertEquals("ㅋ", capped.composings.last())
        e2.close()
    }

    // ------------------------------------------------------------ picker

    @Test
    fun `every offered layout can actually be instantiated`() {
        // The list the picker shows comes from hunmin_ctx_layout_supported, and
        // the invariant worth holding is that filtering on it is not
        // decorative: every id offered must produce a usable engine. A row that
        // failed at keystroke time would be worse than not offering it.
        val n = HunminNative.nativeLayoutCount(ctx)
        var offered = 0
        for (i in 0 until n) {
            if (!HunminNative.nativeLayoutSupported(ctx, i)) continue
            val id = HunminNative.nativeLayoutId(ctx, i)
            val engine = HunminEngine(ctx, id)
            assertTrue("the picker offers '$id' but it is not usable", engine.isUsable)
            engine.close()
            offered++
        }
        assertTrue("no layouts offered at all -- the picker would be empty", offered > 0)
    }

    @Test
    fun `every offered layout has a Korean label`() {
        // The picker labels rows with hunmin_ctx_layout_name(.., "ko"). An empty
        // label renders as a blank row, so this is a UI defect the engine
        // cannot report on its own -- it returns "" happily.
        val n = HunminNative.nativeLayoutCount(ctx)
        for (i in 0 until n) {
            if (!HunminNative.nativeLayoutSupported(ctx, i)) continue
            val id = HunminNative.nativeLayoutId(ctx, i)
            val ko = HunminNative.nativeLayoutName(ctx, i, "ko")
            assertTrue("layout '$id' has no Korean name, so its picker row is blank",
                ko.isNotEmpty())
        }
    }

    @Test
    fun `switching layout flushes rather than drops the composition`() {
        // The user's typed text must reach the app even when they switch layout
        // mid-composition. Dropping it would be a data-loss bug and is the one
        // behaviour here that tier E1 can see without a picker UI at all.
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        engine.typeChar('k', ic)
        engine.typeChar('f', ic)
        assertEquals("가", ic.composings.last())
        // What HunminInputMethodService.setLayout does, in order.
        engine.flush(ic)
        engine.close()
        assertTrue("the composition was lost on the layout switch: ${ic.commits}",
            ic.commits.any { it.contains("가") })
    }

    @Test
    fun `an unknown layout yields an unusable engine rather than a crash`() {
        val engine = HunminEngine(ctx, "no-such-layout")
        assertFalse(engine.isUsable)
        val ic = RecordingInputConnection()
        // Every key must pass through, not crash on a null handle.
        assertFalse(engine.typeChar('k', ic))
        engine.updatePreedit(ic)
        engine.flush(ic)
        engine.commitExternalText(ic, "x")
        engine.close()
    }

@Test
    fun `an arrow key emits no text of its own and only flushes what was composing`() {
        // Issue #39, half 1. Deliberately asserts only what holds under EITHER
        // eventual ruling, because whether the IME should consume Shift+Arrow or
        // pass it through for the editor to extend the selection is not this
        // agent's call. What is not a decision is the text: a cursor key is not
        // a jamo and not a layout row, so it must contribute no commit of its
        // own. It MAY flush an in-flight syllable -- that is the documented
        // unmapped-key path ("an unmapped key flushes and passes through") and
        // is how space reaches the client -- so what is pinned is exactly that:
        // commits grow by precisely the composing text, and the composing
        // region is left empty.
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        assumeTrue(engine.isUsable)

        for (keyCode in listOf(21, 22, 19, 20)) { // LEFT RIGHT UP DOWN
            for (meta in listOf(0, 1)) {           // META_SHIFT_ON
                // Start each step with a syllable in flight, so "emits nothing"
                // is a claim about state rather than about an empty buffer.
                engine.typeChar('k', ic)
                val composingBefore = ic.composing()
                assertEquals("test setup: no composition to flush", "ㄱ", composingBefore)
                val committedBefore = ic.text()

                engine.onKey(keyCode, meta, ic)

                assertEquals("arrow $keyCode meta $meta did not flush exactly what was composing",
                    committedBefore + composingBefore, ic.text())
                assertEquals("arrow $keyCode meta $meta left a composition behind",
                    "", ic.composing())
            }
        }
        engine.close()
    }

    @Test
    fun `an arrow keycode is not a layout key`() {
        // Why the test above holds at the mapping layer rather than by accident:
        // an arrow code has no engineChar at all, so the adapter cannot be
        // silently reaching the jamo pipeline with a character that happens to
        // compose to nothing.
        assertEquals(null, HunminKeys.engineChar(21, 0))  // LEFT,  no shift
        assertEquals(null, HunminKeys.engineChar(22, 1))  // RIGHT, META_SHIFT_ON
    }

    @Test
    fun `issue 39 shift+arrow over a standing selection publishes no composing region`() {
        // Issue #39's reproduction, stated as the maintainer's: the field holds
        // text, a selection is standing, and Shift+Arrow is used to extend it --
        // and "the selected text tends to be deleted".
        //
        // The mechanism turns out to be the one the issue guessed at, one layer
        // along. Not "the composition rule emits", but updatePreedit's
        // unconditional setComposingText("", 1): an arrow keycode resolves to no
        // engineChar, so the adapter takes the unmapped-key path, which flushes,
        // and the flush ends in updatePreedit. With no composition of ours on
        // screen, setComposingText("", 1) is the editor being told "replace the
        // composing region with nothing" -- and where a selection is standing,
        // that selection is what gets replaced. Hence "with shift", which is how
        // a selection is normally extended.
        //
        // Asserted as the ABSENCE of an emit, which is the property that matters
        // and which holds under either eventual ruling on `consumed`: the adapter
        // must not publish a composing region it does not own, because that is
        // what a selection cannot survive.
        val ic = RecordingInputConnection()
        val engine = HunminEngine(ctx, "sin3-p2")
        assumeTrue(engine.isUsable)

        val composingsBefore = ic.composings.size
        val commitsBefore = ic.commits.size

        for (keyCode in listOf(21, 22)) {   // LEFT, RIGHT
            engine.onKey(keyCode, 1, ic)     // META_SHIFT_ON
        }

        assertEquals("the adapter published a composing region it does not own",
            composingsBefore, ic.composings.size)
        assertEquals("the adapter committed text on a navigation key",
            commitsBefore, ic.commits.size)
        assertTrue("the adapter must not delete surrounding text (I9): ${ic.violations}",
            ic.violations.none { it.startsWith("deleteSurroundingText") })
        engine.close()
    }

    @Test
    fun `a fresh engine does not tell the client to compose nothing`() {
        // The bug this pins: onStartInput builds an engine and calls
        // updatePreedit immediately. With no composition of our own on screen,
        // setComposingText("", 1) is not a no-op -- it is the editor being told
        // "the composing text is now empty", and with nothing to replace, the
        // character before the cursor goes. Measured on R5KL808WBPN: focusing a
        // field that already had text ate its last character before the user
        // typed anything.
        val engine = HunminEngine(ctx, "sin3-p2")
        assumeTrue(engine.isUsable)
        val ic = RecordingInputConnection()
        engine.updatePreedit(ic)
        assertEquals(
            "a session with no composition must not emit setComposingText at all",
            emptyList<String>(), ic.composings)
        engine.close()
    }

    @Test
    fun `a composition IS cleared afterwards, so the client is not left stale`() {
        // The other half: the guard above must not stop the engine from clearing
        // a composition it really did put there, or the last syllable stays on
        // screen forever.
        val engine = HunminEngine(ctx, "sin3-p2")
        assumeTrue(engine.isUsable)
        val ic = RecordingInputConnection()
        assertTrue(engine.typeChar('k', ic))
        assertEquals("ㄱ", ic.composings.last())
        engine.flush(ic)
        assertEquals("the composing region outlived the flush", "", ic.composings.last())
        engine.close()
    }

    private companion object {
        // Anchored to the "in" object so the "meta" lines -- which also carry
        // text -- cannot be mistaken for events.
        // Regex, not Pattern: Kotlin's Regex already exposes matcher/group.
        // NOTE the `\s*` after every colon. The .jsonl goldens are written by
        // json.dumps with default separators -- `{"in": {"ch": "k"}, ...}` --
        // so a regex written as `"ch":"` matches nothing and the test silently
        // compares zero events. It reported 17 skips rather than a failure for
        // exactly that reason.
        val FIELD_CH = Regex(""""in"\s*:\s*\{\s*"ch"\s*:\s*"(.)"""")
        val FIELD_COMMIT = Regex(""""commit"\s*:\s*"(.*?)"""")
        val FIELD_CASE = Regex(""""case"\s*:\s*"([^"]*)"""")
        // The "keys" array of a layout file, and only that array. A regex over
        // the whole document finds 48 characters because it also matches the
        // JSON's own quotes and braces; keymap-fixture.py reads the same array
        // through json.loads and reports 47, and the two must agree.
        // Triple-quoted so the backslashes are regex escapes, not Kotlin string
        // escapes. Written as a normal string it became \s (a literal backslash
        // plus 's'), matched nothing, and the key scan reported 0 keys.
        // "keys" is a JSON OBJECT -- {"'": {"shift": ...}, ",": ...} -- not an
        // array, and each value nests three more levels. Two wrong assumptions
        // lived here first: that it was an array (found 0 keys), and then that
        // every quoted single character in it was a key (found 68, because
        // "case", "if" and "then" are strings at depth).
        //
        // So this tracks BRACE DEPTH and only accepts a key at depth 1. That is
        // the smallest thing that is correct without pulling a JSON parser into a
        // unit test, and it is cross-checked against the count
        // tools/keymap-fixture.py derives with json.loads (47 for sin3-p2).
        fun layoutKeysIn(json: String): List<Char> {
            val start = json.indexOf("\"keys\"")
            require(start >= 0) { "no \"keys\" in this layout file" }
            val open = json.indexOf('{', start)
            require(open >= 0) { "\"keys\" has no object body" }

            val keys = mutableListOf<Char>()
            var depth = 0
            var i = open
            while (i < json.length) {
                when (json[i]) {
                    '{' -> depth++
                    '}' -> {
                        depth--
                        if (depth == 0) break
                    }
                    '"' -> {
                        // Read the string, and note whether it is a key: a string
                        // at depth 1 followed by ':' is one.
                        val sb = StringBuilder()
                        var j = i + 1
                        while (j < json.length && json[j] != '"') {
                            if (json[j] == '\\' && j + 1 < json.length) j++
                            sb.append(json[j]); j++
                        }
                        var k = j + 1
                        while (k < json.length && json[k].isWhitespace()) k++
                        if (depth == 1 && k < json.length && json[k] == ':' &&
                            sb.length == 1
                        ) {
                            keys.add(sb[0])
                        }
                        i = j
                    }
                }
                i++
            }
            return keys
        }

        // The punctuation a layout key may be. Used only to narrow the scan:
        // a layout file also quotes JSON syntax and Korean names, and those are
        // not keys.
        const val PUNCT = ",-./;='[]\\\\`"
    }
}
