/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * HunminTraceRecorder — the shipped IME's key trace sink, for tier E3.
 *
 * WHY THIS IS IN THE SHIPPED APP AND NOT ONLY IN A TEST BUILD. docs/TESTING.md
 * tier E3 says every manual session on a physical keyboard records a tier-C
 * trace "so device quirks become regression data". That is only true if the IME
 * a person is holding can produce one. A sink that existed only in a test build
 * would capture a different adapter than the one that shipped, which is the
 * mistake docs/FRONTEND-CONTRACT.md §3 already records once for fcitx5.
 *
 * OFF UNLESS TURNED ON. Inert unless [enable] is called, which tier E3 does
 * explicitly. A released IME does not write files, and the cost when off is one
 * Boolean check per keystroke.
 *
 * The format is docs/trace-spec.md, the Android dialect: one
 * `{"in": {"keycode": N, "mods": M}, ...}` object per event, `meta` first. `mods`
 * is the platform bitfield verbatim -- including CapsLock as
 * META_CAPS_LOCK_ON -- because "an adapter that cannot see CapsLock cannot be
 * tested for it, and that is the point of the tier".
 */
package org.hunmin.ime

import android.util.Log
import java.io.File

object HunminTraceRecorder {

    private const val TAG = "HunminTraceRecorder"

    @Volatile private var sink: File? = null

    /** True while writing. Read on every key, so it must stay a field read. */
    @Volatile private var open = false

    /** Free-form label tying this session to a checklist step. */
    @Volatile var caseLabel: String = "manual"

    /** Optional provenance: the tier A case this replays. */
    @Volatile var source: String = ""

    /**
     * Start writing to [file]; a no-op when null or unwritable.
     *
     * Tracing must never be the reason an IME misbehaves, so every failure here
     * ends with tracing off rather than an exception.
     */
    fun enable(file: File?, layout: String, platform: String = "android") {
        disable()
        if (file == null) return
        try {
            file.parentFile?.mkdirs()
            file.writeText("")
            sink = file
            open = true
            val src = if (source.isEmpty()) "" else """, "source": "$source""""
            write(
                """{"meta": {"platform": "$platform", "layout": "$layout", """ +
                    """"case": "$caseLabel"$src}}"""
            )
        } catch (e: Exception) {
            Log.w(TAG, "cannot open a trace at $file; continuing without it", e)
            disable()
        }
    }

    fun disable() {
        open = false
        sink = null
    }

    val isEnabled: Boolean get() = open

    /**
     * One event: the native key, what the adapter made of it, and what the
     * client received.
     *
     * [consumed] is the adapter's decision, not the engine's: a key the engine
     * declined and the frontend passed through is the interesting case on a
     * device whose keyboard delivers keys X11 never would.
     *
     * Every field is escaped. The trace is JSON, and a key that produced a quote
     * or a backslash -- a symbol tier, or a literal like `"=a"` -- would
     * otherwise produce a file no runner can parse, i.e. the trace would be
     * silently lost exactly when a device quirk produced one.
     */
    fun event(
        keyCode: Int,
        mods: Int,
        engineChar: Char?,
        commit: String,
        preedit: String,
        consumed: Boolean,
    ) {
        if (!open) return
        val ch = if (engineChar == null) "null" else "\"${escape(engineChar.toString())}\""
        write(
            """{"in": {"keycode": $keyCode, "mods": $mods}, "char": $ch, """ +
                """"commit": "${escape(commit)}", "preedit": "${escape(preedit)}", """ +
                """"consumed": $consumed}"""
        )
    }

    fun close() = disable()

    private fun write(line: String) {
        val f = sink ?: return
        try {
            f.appendText(line + "\n")
        } catch (e: Exception) {
            // A failing trace must not take the IME down; stop trying instead.
            Log.w(TAG, "trace write failed; disabling", e)
            disable()
        }
    }

    /** JSON string escaping, for the text fields. */
    private fun escape(s: String): String {
        val sb = StringBuilder(s.length + 8)
        for (c in s) {
            when (c) {
                '"' -> sb.append("\\\"")
                '\\' -> sb.append("\\\\")
                '\n' -> sb.append("\\n")
                '\r' -> sb.append("\\r")
                '\t' -> sb.append("\\t")
                else ->
                    if (c < ' ' || c == ' ' || c == ' ') {
                        sb.append(String.format("\\u%04x", c.code))
                    } else {
                        sb.append(c)
                    }
            }
        }
        return sb.toString()
    }
}
