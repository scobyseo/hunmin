/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1, the TOUCH golden (issue #299, T3-3): the Latin commit path through
 * the engine survives touch.
 *
 * The issue's verification, stated as what this lane measures: a soft-key tap
 * is a (keycap id, one-shot shift) decision that becomes the SAME
 * (keyCode, metaState) a hardware key delivers. This lane replays the tier A
 * corpus tests/golden/cultural-pangram-us-qwerty.jsonl through that decision
 * and the REAL engine (host JNI shim), asserting per-key commit and preedit —
 * the same expectations hunmin-sim --check asserts on the host. A divergence
 * between the touch mapping and the hardware path is a failure here, not a
 * discovery on a device.
 *
 * What only a device can prove — that the platform delivers the tap to the
 * view at all — is tier E2's touch round (tools/android-e2e.sh). Nothing here
 * can fail if the view is unreachable, which is why both exist.
 *
 * The second case pins the symbol-page seam (design §1): a pending dubeolsik
 * word is flushed BEFORE the direct symbol commit (KP11), so the symbol never
 * lands ahead of the composition, with no I9 violation.
 */
package org.hunmin.ime

import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import java.io.File

class KeypadTouchGoldenTest {

    private var ctx: Long = 0

    private val repoRoot: File by lazy {
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) } ?: File("../..").canonicalFile
    }
    private val dataDir: File by lazy { File(repoRoot, "libhunmin/data").canonicalFile }
    private val corpus: File by lazy {
        File(repoRoot, "tests/golden/cultural-pangram-us-qwerty.jsonl")
    }
    private val symbolAsset: File by lazy {
        File(repoRoot, "frontends/android/app/src/main/assets/keypad/symbols.json")
    }

    @Before
    fun setUp() {
        val shim = System.getenv("HUNMIN_JNI_LIB")
        assumeTrue(
            "HUNMIN_JNI_LIB is unset; run tools/android-unit.sh, which builds and " +
                "points at the host JNI shim (got '$shim')",
            !shim.isNullOrEmpty() && File(shim).isFile,
        )
        assumeTrue("libhunmin data tables missing at $dataDir", dataDir.isDirectory)
        assumeTrue("the us-qwerty golden corpus is missing at $corpus", corpus.isFile)
        ctx = HunminNativeBridge.nativeCtxNew(dataDir.absolutePath)
        assertTrue("hunmin_ctx_new found no layout", ctx != 0L)
    }

    @After
    fun tearDown() {
        if (ctx != 0L) HunminNativeBridge.nativeCtxFree(ctx)
        ctx = 0
    }

    /**
     * The keycap a user taps for [ch], and whether the tap needs the one-shot
     * shift: the registry's unshifted column when the character sits there,
     * the shifted column otherwise (uppercase letters, the digits' symbols).
     * This is the VIEW's decision, derived from the same pinned table — not a
     * second character map (the recorded failure mode of hand-derived maps).
     */
    private fun touchKeyFor(ch: Char): Pair<String, Boolean> {
        val all = HunminKeyRegistry.all()
        all.firstOrNull { it.unshifted == ch }?.let { return ch.toString() to false }
        val shifted = all.firstOrNull { it.shifted == ch }
            ?: error("no keycap for '$ch' — the registry is missing a layout key")
        return shifted.unshifted.toString() to true
    }

    @Test
    fun `touch replays the us-qwerty golden through the engine`() {
        // Under word buffering (issue #75) the layer delivers at the boundary,
        // not per keystroke — the corpus records ENGINE-level per-key commits
        // (hunmin-sim --trace), so the comparison is the TOTAL text the client
        // eventually saw, the same pattern HunminAdapterTest's tier-A replay
        // uses: golden commits joined + the last event's preedit + the spaces
        // the client inserts at boundaries. Each case gets a FRESH engine, as
        // hunmin-sim does; a trailing preedit never carries into the next case.
        val caps = KeypadKeycaps.parse(File(dataDir, "layouts/us-qwerty.json").readText())
        val gridIds = KeypadModel.textRows(caps, shift = false).flatten()
            .filter { it.kind == KeypadKey.Kind.CHARACTER }.map { it.id }.toSet()

        data class Event(val ch: Char, val shift: Boolean, val commit: String, val preedit: String)
        data class Case(val name: String, val text: String, val events: MutableList<Event> = mutableListOf())

        val cases = mutableListOf<Case>()
        for (line in corpus.readLines()) {
            if (line.isBlank()) continue
            val obj = JSONObject(line)
            if (obj.has("meta")) {
                val meta = obj.getJSONObject("meta")
                if (meta.has("layout")) {
                    assertEquals("the corpus is the us-qwerty lane", "us-qwerty",
                        meta.getString("layout"))
                }
                if (meta.has("case")) {
                    cases.add(Case(meta.getString("case"), meta.getString("text")))
                }
                continue
            }
            assertTrue("event before any case meta — the parse is wrong", cases.isNotEmpty())
            val inObj = obj.getJSONObject("in")
            cases.last().events.add(
                Event(
                    inObj.getString("ch").single(),
                    // The corpus encodes shift two ways: uppercase letters as
                    // the shifted character ("T"), digits/punctuation as the
                    // base plus a shift flag ("1", shift:true). Both are taps.
                    inObj.optBoolean("shift", false),
                    obj.getString("commit"),
                    obj.getString("preedit"),
                )
            )
        }
        assertTrue("the corpus replayed no cases", cases.isNotEmpty() && cases.all { it.events.isNotEmpty() })

        for (case in cases) {
            val adapter = HunminAdapter(ctx, "us-qwerty", null, "word")
            assertTrue("us-qwerty is not usable", adapter.isUsable)
            val ic = RecordingInputConnection()
            val state = KeypadState()
            for (ev in case.events) {
                if (ev.ch == ' ') {
                    // The service's space semantics (onKeyDown's space branch):
                    // the word is delivered at the boundary, then the space
                    // itself — the space does not go through the engine.
                    adapter.commitWord(ic)
                    ic.commitText(" ", 1)
                    continue
                }
                val (id, needShift) =
                    if (ev.shift) ev.ch.toString() to true else touchKeyFor(ev.ch)
                assertTrue("keycap '$id' is not on the grid — unreachable by touch", id in gridIds)
                // The tap as the view dispatches it: arm the one-shot when the
                // character sits on the shift row, consume it on the key tap.
                if (needShift) state.tapShift()
                adapter.onKey(KeypadModel.keyCodeFor(id), state.shiftMeta(), ic)
            }
            adapter.commitWord(ic)
            adapter.close()

            val expectedTotal = buildString {
                for (ev in case.events) {
                    if (ev.ch == ' ') append(' ') else append(ev.commit)
                }
                append(case.events.last().preedit)
            }
            assertEquals(
                "case '${case.name}' touch total", expectedTotal, ic.commits.joinToString(""),
            )
            // The case meta's text is the human-readable ground truth the
            // generator re-typed whole; the touch path must reproduce it.
            assertEquals("case '${case.name}' against the corpus text", case.text, ic.commits.joinToString(""))
            assertTrue("I9 violations in '${case.name}': ${ic.violations}", ic.violations.isEmpty())
        }
    }

    @Test
    fun `a symbol tap flushes the pending word before the symbol`() {
        // The buffered-layer fact (HunminAdapterTest's invariant-2 case, issue
        // #75): under word buffering the client is shown "안녕" as ONE composing
        // region, commits NOTHING until the boundary, and the boundary delivers
        // "안녕" once. (The raw engine trace commits 안 mid-word — the layer
        // holds it; the sim trace is not the client's view.)
        val caps = KeypadKeycaps.parse(File(dataDir, "layouts/dubeolsik.json").readText())
        val adapter = HunminAdapter(ctx, "dubeolsik", null, "word")
        assertTrue("dubeolsik is not usable", adapter.isUsable)
        val ic = RecordingInputConnection()
        val state = KeypadState()
        for (id in listOf("d", "k", "s", "s", "u", "d")) {
            adapter.onKey(KeypadModel.keyCodeFor(id), state.shiftMeta(), ic)
        }
        assertEquals("nothing commits while the word is composing", emptyList<String>(), ic.commits)
        assertEquals("안녕", ic.composing())

        // The symbol-page tap, in the service's order (commitSymbolFromTouch):
        // flush FIRST, then the direct commit — KP11, engine uninvolved in the
        // symbol itself, but the buffered word may never land after it.
        val screen = SymbolScreen.parse(symbolAsset.readText())
        adapter.commitWord(ic)
        screen.commitSymbol("!", ic)

        assertEquals(listOf("안녕", "!"), ic.commits)
        assertEquals("", ic.composing())
        assertTrue("I9 violations: ${ic.violations}", ic.violations.isEmpty())
        adapter.close()
    }
}