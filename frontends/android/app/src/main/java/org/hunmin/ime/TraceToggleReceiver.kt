/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * TraceToggleReceiver — the tier-E3 / #312-evaluation broadcast receiver.
 *
 * WHY THIS IS A MANIFEST RECEIVER AND NOT THE SERVICE'S DYNAMIC ONE. The IME
 * service is bound by the platform and exists only while an editor has focus,
 * so a broadcast sent while the phone is locked (or simply has no field
 * focused) reached nothing: the evaluation's toggle reported "enabled" and the
 * sinks stayed shut. Measured 2026-11-27 — the trace had stopped at 20:40 the
 * previous evening, because the service had been recreated and no later
 * broadcast could start it again.
 *
 * A RELEASED IME STILL WRITES NOTHING. The receiver exists in every build, but
 * its first act is the same guard the dynamic one used: if this APK is not
 * debuggable it returns before touching a preference or a file. `am broadcast`
 * cannot turn file writing on in a build a person installed from a release.
 *
 * WHAT IT DOES. "enable" stores the case label (so a service created later
 * resumes the trace, appending), opens the sinks when a service is already up
 * in this process, and "disable" clears the stored label. "mark" annotates the
 * open UI trace.
 */
package org.hunmin.ime

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.ApplicationInfo
import android.util.Log
import java.io.File

class TraceToggleReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context?, intent: Intent?) {
        if (context == null || intent?.action != ACTION_TRACE) return
        // A released IME must not accept a broadcast that turns on file writing.
        if ((context.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) == 0) return

        val prefs = context.getSharedPreferences(HunminInputMethodService.PREFS, Context.MODE_PRIVATE)
        when (intent.getStringExtra("op")) {
            "enable" -> {
                val case = intent.getStringExtra("case") ?: "manual"
                val source = intent.getStringExtra("source") ?: ""
                prefs.edit()
                    .putString(HunminInputMethodService.TRACE_PREF_CASE, case)
                    .putString(HunminInputMethodService.TRACE_PREF_SOURCE, source)
                    .apply()
                // Open now too, when this process already hosts the sinks (the
                // service may be alive); a later service start resumes from the
                // stored label either way.
                HunminTraceRecorder.caseLabel = case
                HunminTraceRecorder.source = source
                HunminTraceRecorder.enable(
                    File(context.filesDir, HunminInputMethodService.TRACE_FILE),
                    HunminInputMethodService.DEFAULT_LAYOUT,
                )
                UiTraceRecorder.enable(
                    File(context.filesDir, HunminInputMethodService.UI_TRACE_FILE), case,
                )
                Log.i(TAG, "hunmin: trace enabled by broadcast (case=$case)")
            }
            "disable" -> {
                prefs.edit()
                    .remove(HunminInputMethodService.TRACE_PREF_CASE)
                    .remove(HunminInputMethodService.TRACE_PREF_SOURCE)
                    .apply()
                HunminTraceRecorder.close()
                UiTraceRecorder.disable()
                Log.i(TAG, "hunmin: trace closed by broadcast")
            }
            "mark" -> UiTraceRecorder.mark(intent.getStringExtra("label") ?: "")
        }
    }

    private companion object {
        const val TAG = "HunminTraceToggle"
        const val ACTION_TRACE = "org.hunmin.ime.TRACE"
    }
}