/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * UiTraceRecorder — the gesture/symbol-layer trace sink (#312 usability
 * evaluation).
 *
 * WHY A SECOND SINK. The engine trace (HunminTraceRecorder, docs/trace-spec.md)
 * records key events and composition — but the failures the maintainer wants
 * to study ("입력이 기대대로 안되는 경우") happen ABOVE the engine: a swipe
 * recognised as a tap, a symbol committed from the wrong page, a mode that
 * drifted. Those layers never reach the engine trace (symbol commits are
 * KP11 direct commits, engine uninvolved). This sink records the UI decisions
 * with their inputs — the pointer trajectory and the gate values — so an
 * expectation mismatch can be reconstructed: what the finger did, what the
 * recognizer decided, what the app delivered.
 *
 * SEPARATE FILE, SEPARATE FORMAT. The engine trace format is normative
 * (trace-spec.md, tier C conformance); UI events go to ui.trace in their own
 * JSONL dialect and never mix into it. Same lifecycle: OFF unless enabled by
 * the same debuggable-guarded broadcast (tools/e3-trace.sh), one Boolean
 * check per event when off, every failure ends with tracing off.
 */
package org.hunmin.ime

import android.util.Log
import java.io.File

object UiTraceRecorder {

    private const val TAG = "UiTraceRecorder"

    @Volatile private var sink: File? = null
    @Volatile private var open = false

    fun enable(file: File?, caseLabel: String, resume: Boolean = false) {
        disable()
        if (file == null) return
        try {
            file.parentFile?.mkdirs()
            /* resume=true continues a session after an IME-service restart
             * (#312 evaluation) instead of truncating it — see the engine
             * sink's enable() for the measurement. */
            if (!resume) file.writeText("")
            sink = file
            open = true
            val meta = StringBuilder(160)
            meta.append("{\"meta\": {\"platform\": \"android-ui\", \"case\": \"")
            meta.append(HunminTraceRecorder.escape(caseLabel)).append('"')
            if (resume) meta.append(", \"resumed\": true")
            meta.append("}}")
            write(meta.toString())
        } catch (e: Exception) {
            Log.w(TAG, "cannot open a ui trace at $file; continuing without it", e)
            disable()
        }
    }

    fun disable() {
        open = false
        sink = null
    }

    val isEnabled: Boolean get() = open

    /**
     * One pointer outcome on a keycap: the trajectory ([dx]/[dy] from DOWN,
     * [durMs]), what the recognizer decided ([recognized]: tap / swipe_up /
     * swipe_down / swipe_left / long_press / cancel), the action it fired
     * ([action]), and the gate values it was judged against — enough to tell
     * "my swipe-up was under the threshold" from "the gesture fired but the
     * action was wrong".
     */
    fun gesture(
        page: String, key: String, kind: String,
        dx: Float, dy: Float, durMs: Long,
        recognized: String, action: String,
        thresholdPx: Float, jitterPx: Float, returnPx: Float,
    ) {
        if (!open) return
        write(
            """{"ui": "gesture", "page": "${HunminTraceRecorder.escape(page)}", """ +
                """"key": "${HunminTraceRecorder.escape(key)}", "kind": "$kind", """ +
                """"dx": $dx, "dy": $dy, "dur_ms": $durMs, """ +
                """"recognized": "$recognized", "action": "${HunminTraceRecorder.escape(action)}", """ +
                """"gates": {"threshold_px": $thresholdPx, "jitter_px": $jitterPx, "return_px": $returnPx}}"""
        )
    }

    /** A direct symbol commit (KP11): the page it came from, the whole entry. */
    fun symbol(page: String, entry: String) {
        if (!open) return
        write(
            """{"ui": "symbol", "page": "${HunminTraceRecorder.escape(page)}", """ +
                """"entry": "${HunminTraceRecorder.escape(entry)}"}"""
        )
    }

    /** A page transition and what triggered it (tap / picker / cycle / return). */
    fun page(from: String, to: String, trigger: String) {
        if (!open) return
        write("""{"ui": "page", "from": "$from", "to": "$to", "trigger": "$trigger"}""")
    }

    /** The shift state after a change: one-shot armed, capslock locked. */
    fun shift(armed: Boolean, locked: Boolean) {
        if (!open) return
        write("""{"ui": "shift", "armed": $armed, "locked": $locked}""")
    }

    /** A free-form marker, so a session can be annotated at the moment of interest. */
    fun mark(label: String) {
        if (!open) return
        write("""{"ui": "mark", "label": "${HunminTraceRecorder.escape(label)}"}""")
    }

    private fun write(line: String) {
        val f = sink ?: return
        try {
            f.appendText(line + "\n")
        } catch (e: Exception) {
            Log.w(TAG, "ui trace write failed; disabling", e)
            disable()
        }
    }
}