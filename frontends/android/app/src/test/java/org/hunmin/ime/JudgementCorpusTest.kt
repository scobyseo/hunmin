/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier E1, the JUDGEMENT lane (issue #211).
 *
 * docs/FRONTEND-EQUIVALENCE.md §6.2.1 records the maintainer's ruling: every
 * frontend is evaluated against the same corpus, and a frontend's own
 * characteristics may differ only where §3 lists them. This is the Android half
 * of that. It replays tests/golden/judgement-r8.jsonl -- the same file
 * `engine-golden-judgement` asserts and `engine-d1-judgement` already replays
 * through fcitx5 -- through the real HunminAdapter and the real engine.
 *
 * What this lane adds over the shared-layer runner is what only the adapter can
 * get wrong: the delivered text per KEY (the corpus's step_commits, so R6's
 * held run is distinguished from a per-key commit), the judgements the adapter
 * reads and acts on, and the mode the layer is left in, asked through the same
 * probe the corpus carries for every case that ends at a boundary.
 *
 * The two Android-specific differences, named rather than skipped:
 *
 *   - The starting mode IS settable here. The fcitx5 addon has no key that sets
 *     the layer's initial language, so its lane skips every `mode: latin` case.
 *     Android's pair adapter owns both slots (FE7), so the lane asks it for the
 *     latin start with toggleLanguage() and runs those cases instead of
 *     dropping two of them.
 *   - The corpus's one case that ends MID-WORD (r4-nothing-is-judged-in-flight)
 *     carries no probe: a probe typed after it would append to the word still
 *     in the buffer rather than stand alone. There the lane asserts the
 *     adapter's own language read-back instead, because no probe can be taken.
 *
 * The commit unit is the buffered one, `word`, which is what Android ships
 * (§6.1); the judgement cannot run under PER_SYLLABLE at all, because earlier
 * syllables are already in the client (R10).
 *
 * The corpus is read, never transcribed: the fields this file asserts
 * (step_commits, judged, mode_after, probe) all come out of the JSONL.
 */
package org.hunmin.ime

import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import java.io.File

class JudgementCorpusTest {

    /* HunminAdapter's language values: HANGUL 0, LATIN 1. */
    private val hangul = 0
    private val latin = 1

    /* AOSP metaState META_SHIFT_ON, as in HunminAdapterTest. */
    private val shiftOn = 0x00000001

    private var ctx: Long = 0

    private val repoRoot: File by lazy {
        System.getenv("HUNMIN_REPO_ROOT")?.let { File(it) } ?: File("../..").canonicalFile
    }
    private val dataDir: File by lazy { File(repoRoot, "libhunmin/data").canonicalFile }
    private val corpus: File by lazy { File(repoRoot, "tests/golden/judgement-r8.jsonl") }

    @Before
    fun setUp() {
        val shim = System.getenv("HUNMIN_JNI_LIB")
        assumeTrue(
            "HUNMIN_JNI_LIB is unset; run tools/android-unit.sh, which builds and " +
                "points at the host JNI shim (got '$shim')",
            !shim.isNullOrEmpty() && File(shim).isFile,
        )
        assumeTrue("libhunmin data tables missing at $dataDir", dataDir.isDirectory)
        assumeTrue(
            "the judgement corpus is missing at $corpus -- this lane compares against " +
                "the shared file, so a missing corpus must not read as a pass",
            corpus.isFile,
        )
        ctx = HunminNativeBridge.nativeCtxNew(dataDir.absolutePath)
        assertTrue("hunmin_ctx_new found no layout", ctx != 0L)
    }

    @After
    fun tearDown() {
        if (ctx != 0L) HunminNativeBridge.nativeCtxFree(ctx)
    }

    // ------------------------------------------------------------ the corpus

    private data class Probe(val keys: String, val hangul: String, val latin: String)

    private data class Case(
        val name: String,
        val layout: String,
        val mode: String,
        val keys: String,
        val autoswitch: Int,
        val judged: List<String>,
        val modeAfter: String,
        /** Absent when a language toggle delivered text of its own, so the
         *  per-key commits would not add up to the case's delivered text. */
        val stepCommits: List<String>?,
        val probe: Probe?,
    )

    private fun loadCorpus(file: File): List<Case> =
        file.readLines().filter { it.isNotBlank() }.mapNotNull { line ->
            val row = JsonParser(line).parseOne() as? Map<*, *>
                ?: error("corpus line is not a JSON object: $line")
            // Every line but the first is a case; the first is provenance.
            if (row.containsKey("meta")) return@mapNotNull null
            val probeRow = row["probe"] as? Map<*, *>
            Case(
                name = row.string("case"),
                layout = row.string("layout"),
                mode = row.string("mode"),
                keys = row.string("keys"),
                autoswitch = (row["autoswitch"] as? Number)?.toInt() ?: 1,
                judged = (row["judged"] as? List<*>)?.map { it as? String ?: "" } ?: emptyList(),
                modeAfter = row.string("mode_after"),
                stepCommits = (row["step_commits"] as? List<*>)
                    ?.map { it as? String ?: "" },
                probe = probeRow?.let {
                    Probe(it.string("keys"), it.string("hangul"), it.string("latin"))
                },
            )
        }

    private fun Map<*, *>.string(key: String): String =
        this[key] as? String ?: error("corpus field '$key' is missing or not a string")

    // --------------------------------------------------------------- replay

    private data class Stroke(val keyCode: Int, val metaState: Int)

    private fun stroke(ch: Char, shifted: Boolean): Stroke {
        val key = HunminKeyRegistry.all().firstOrNull { it.unshifted == ch }
            ?: HunminKeyRegistry.all().firstOrNull { it.shifted == ch }
            ?: error("no AOSP keycode for '$ch' -- the corpus uses a key this adapter cannot send")
        val useShift = shifted || (key.shifted == ch && ch.isUpperCase())
        return Stroke(key.keyCode, if (useShift) shiftOn else 0)
    }

    /**
     * hunmin-sim's key language, the one every golden in tests/golden/ uses:
     * a plain byte, "\s<char>" (that character with Shift held), "\b"
     * (BackSpace), "\\" (a literal backslash).
     */
    private fun parseKeys(keys: String): List<Stroke> {
        val strokes = mutableListOf<Stroke>()
        var i = 0
        while (i < keys.length) {
            val c = keys[i]
            if (c == '\\' && i + 1 < keys.length) {
                when (keys[i + 1]) {
                    's' -> {
                        require(i + 2 < keys.length) { "trailing \\s in keys '$keys'" }
                        strokes.add(stroke(keys[i + 2], shifted = true))
                        i += 3
                    }
                    'b' -> {
                        strokes.add(Stroke(67 /* KEYCODE_DEL */, 0))
                        i += 2
                    }
                    '\\' -> {
                        strokes.add(stroke('\\', shifted = false))
                        i += 2
                    }
                    else -> error("unknown key escape \\${keys[i + 1]} in '$keys'")
                }
            } else {
                strokes.add(stroke(c, shifted = false))
                i++
            }
        }
        return strokes
    }

    private fun langId(name: String): Int = when (name) {
        "hangul" -> hangul
        "latin" -> latin
        else -> error("corpus names an unknown language '$name'")
    }

    /**
     * Feed [keys] through the adapter, performing the frontend's half of R8
     * after every key: read the judgement, and switch when the engine asked.
     *
     * This is the same sequence `hunmin-sim --judge` uses and the same one
     * fcitx5's lane uses -- the engine delivers the word in the judged language
     * and the frontend adopts the MODE. Reading it here, rather than inferring
     * it from the delivered text, is what makes `judged` assertable at all.
     *
     * @param expected the corpus's step_commits, or null when the case carries
     *   none (a toggle's own delivery would make the per-key sum differ).
     */
    private fun feed(
        adapter: HunminAdapter,
        ic: RecordingInputConnection,
        keys: String,
        expected: List<String>? = null,
        judged: MutableList<Int>? = null,
    ) {
        val strokes = parseKeys(keys)
        strokes.forEachIndexed { step, key ->
            val before = ic.commits.size
            adapter.onKey(key.keyCode, key.metaState, ic)
            val mode = adapter.takeJudgement()
            if (mode >= 0) {
                judged?.add(mode)
                assertTrue(
                    "case step $step judged ${if (mode == hangul) "hangul" else "latin"} " +
                        "but the pair adapter could not perform the language action",
                    adapter.toggleLanguage(),
                )
            }
            if (expected != null) {
                val delivered = ic.commits.drop(before).joinToString("")
                assertEquals(
                    "step $step of keys '$keys' delivered a different text than the corpus",
                    expected.getOrElse(step) { "<no step_commits entry>" },
                    delivered,
                )
            }
        }
        if (expected != null) {
            assertEquals(
                "keys '$keys' are not the number of keys the corpus recorded a step for",
                expected.size,
                strokes.size,
            )
        }
    }

    private fun runCase(c: Case) {
        val ic = RecordingInputConnection()
        // A fresh pair per case: the layer's mode and buffer are state, and the
        // corpus states a starting mode per case. The service builds this same
        // two-slot adapter (FE7); the one-slot latinEngine is not the judgement
        // surface.
        val adapter = HunminAdapter(ctx, c.layout, "us-qwerty", "word")
        assertTrue("layout '${c.layout}' is not usable", adapter.isUsable)
        try {
            assertEquals(
                "the corpus's autoswitch is a VALUE, so it is written as one",
                c.autoswitch,
                adapter.setAutoswitch(c.autoswitch != 0),
            )

            // Android's pair starts in Hangul. `mode: latin` is settable here,
            // unlike fcitx5's lane, so no case is skipped for its start state.
            if (c.mode == "latin") {
                assertTrue(
                    "premise: a fresh pair starts in Hangul before the latin start is asked for",
                    adapter.isHangulLanguage(),
                )
                assertTrue("the pair could not enter latin mode", adapter.toggleLanguage())
                assertFalse("premise: the latin start left the layer in Hangul", adapter.isHangulLanguage())
            } else {
                assertTrue(
                    "corpus mode '${c.mode}' is not the Hangul start this lane expects",
                    c.mode == "hangul" && adapter.isHangulLanguage(),
                )
            }

            val judged = mutableListOf<Int>()
            feed(adapter, ic, c.keys, c.stepCommits, judged)
            assertEquals(
                "case '${c.name}' took a different sequence of judgements than the corpus",
                c.judged.map { langId(it) },
                judged,
            )

            if (c.probe != null) {
                // The mode, asserted through its consequence: the corpus carries
                // what the SAME probe keys deliver in each mode, computed by the
                // engine, so a frontend that judged but did not switch fails here.
                val want = if (c.modeAfter == "latin") c.probe.latin else c.probe.hangul
                val before = ic.commits.size
                feed(adapter, ic, c.probe.keys)
                val got = ic.commits.drop(before).joinToString("")
                assertEquals(
                    "case '${c.name}' left the layer in a different mode: probe '${c.probe.keys}' " +
                        "delivered '$got', expected the ${c.modeAfter} reading '$want'",
                    want,
                    got,
                )
            } else {
                // r4 ends mid-word, so the corpus has no probe: no probe typed now
                // would stand alone. The adapter's own read-back is the only
                // available witness, and it is named here rather than skipped.
                assertEquals(
                    "case '${c.name}' has no probe; the adapter's language read-back " +
                        "must agree with mode_after '${c.modeAfter}'",
                    c.modeAfter == "hangul",
                    adapter.isHangulLanguage(),
                )
            }

            assertEquals(
                "case '${c.name}' touched surrounding text (I9): ${ic.violations}",
                emptyList<String>(),
                ic.violations,
            )
        } finally {
            adapter.close()
        }
    }

    @Test
    fun `the judgement corpus replays through the Android pair adapter`() {
        val cases = loadCorpus(corpus)
        assertTrue(
            "the corpus parse produced no cases -- a zero-case replay passes vacuously",
            cases.isNotEmpty(),
        )
        // The two mode:latin cases are the difference this lane takes, so state
        // the premise that the corpus really carries them: if a future edit drops
        // them, "no skips" would be true for the wrong reason.
        assertTrue(
            "the corpus no longer carries a mode:latin case, so the start-mode " +
                "difference this lane takes is untested",
            cases.any { it.mode == "latin" },
        )

        var ran = 0
        for (c in cases) {
            try {
                runCase(c)
            } catch (e: AssertionError) {
                throw AssertionError("case '${c.name}': ${e.message}", e)
            }
            ran++
        }
        assertEquals("the lane did not run every corpus case", cases.size, ran)
    }

    // --------------------------------------------------------------- parser

    /**
     * A minimal JSON reader for the corpus's one-object-per-line format. A test
     * that transcribed the expectations with regexes has already drifted twice
     * in this suite (HunminAdapterTest and TraceEmit both record a misparse that
     * reported a pass), so this reads the actual values -- arrays, nested
     * objects and escapes included -- rather than matching field text.
     */
    private class JsonParser(private val text: String) {
        private var i = 0

        fun parseOne(): Any? {
            val value = parseValue()
            skipSpace()
            require(i == text.length) { "trailing characters at offset $i" }
            return value
        }

        private fun skipSpace() {
            while (i < text.length && text[i].isWhitespace()) i++
        }

        private fun parseValue(): Any? {
            skipSpace()
            require(i < text.length) { "unexpected end of JSON" }
            return when (text[i]) {
                '{' -> parseObject()
                '[' -> parseArray()
                '"' -> parseString()
                't' -> { expect("true"); true }
                'f' -> { expect("false"); false }
                'n' -> { expect("null"); null }
                else -> parseNumber()
            }
        }

        private fun parseObject(): Map<String, Any?> {
            expect("{")
            val map = LinkedHashMap<String, Any?>()
            skipSpace()
            if (peek() == '}') { i++; return map }
            while (true) {
                skipSpace()
                val key = parseString()
                skipSpace()
                expect(":")
                map[key] = parseValue()
                skipSpace()
                when (peek()) {
                    ',' -> i++
                    '}' -> { i++; return map }
                    else -> error("expected ',' or '}' at offset $i")
                }
            }
        }

        private fun parseArray(): List<Any?> {
            expect("[")
            val list = mutableListOf<Any?>()
            skipSpace()
            if (peek() == ']') { i++; return list }
            while (true) {
                list.add(parseValue())
                skipSpace()
                when (peek()) {
                    ',' -> i++
                    ']' -> { i++; return list }
                    else -> error("expected ',' or ']' at offset $i")
                }
            }
        }

        private fun parseString(): String {
            expect("\"")
            val sb = StringBuilder()
            while (i < text.length) {
                val c = text[i++]
                when (c) {
                    '"' -> return sb.toString()
                    '\\' -> {
                        require(i < text.length) { "unterminated escape" }
                        when (val e = text[i++]) {
                            '"' -> sb.append('"')
                            '\\' -> sb.append('\\')
                            '/' -> sb.append('/')
                            'b' -> sb.append('\b')
                            'f' -> sb.append('\u000C')
                            'n' -> sb.append('\n')
                            'r' -> sb.append('\r')
                            't' -> sb.append('\t')
                            'u' -> {
                                require(i + 4 <= text.length) { "short \\u escape" }
                                sb.append(text.substring(i, i + 4).toInt(16).toChar())
                                i += 4
                            }
                            else -> error("unknown escape \\$e")
                        }
                    }
                    else -> sb.append(c)
                }
            }
            error("unterminated string")
        }

        private fun parseNumber(): Long {
            val start = i
            while (i < text.length && text[i] !in ",]} \t\r\n") i++
            return text.substring(start, i).toLong()
        }

        private fun peek(): Char {
            require(i < text.length) { "unexpected end of JSON" }
            return text[i]
        }

        private fun expect(token: String) {
            require(text.startsWith(token, i)) {
                "expected '$token' at offset $i, got '${text.substring(i, minOf(i + token.length, text.length))}'"
            }
            i += token.length
        }
    }
}
