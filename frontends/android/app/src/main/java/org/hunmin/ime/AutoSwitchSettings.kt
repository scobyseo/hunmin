/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * AutoSwitchSettings — the engine's automatic-switch state, for a settings
 * screen that may be open while no IME engine is live.
 *
 * The Android IME keeps its engine per editor and throws it away between
 * fields, so the system settings screen can be reached with no layer in the
 * process. It still has to show the value the engine holds, the key the engine
 * names, and B3's data answer (docs/SPEC-MODES.md §ReqMode13). This class builds
 * the same kind of layer the IME builds, hands it the stored read-back, and
 * reports what the ENGINE settled on -- a refused ON reads as 0, never as the
 * request.
 *
 * The stored boolean is a cache of the engine's read-back, not a second
 * setting: it is written from [write]'s answer, and the value shown always
 * comes back through `hunmin_buffered_preedit_get_option` after the layer has
 * had its say.
 *
 * It takes plain files and lambdas rather than an Android Context so tier E1
 * can drive it on a JVM with the host JNI shim, the same way AutoSwitchTest
 * drives the adapter.
 */
package org.hunmin.ime

import java.io.File

internal class AutoSwitchSettings(
    dataDir: File,
    private val readStored: () -> Boolean,
    private val writeStored: (Boolean) -> Unit,
) : AutoCloseable {

    private var ctx: Long = 0
    private var layer: Long = 0

    init {
        ctx = HunminNativeBridge.nativeCtxNew(dataDir.absolutePath)
        if (ctx != 0L) {
            layer = HunminNativeBridge.nativeLayerNew(
                ctx,
                HunminInputMethodService.DEFAULT_LAYOUT,
                HunminInputMethodService.DEFAULT_LATIN_LAYOUT,
                "word",
            )
        }
        if (layer != 0L) {
            /* Seed the fresh layer with the last answer and record what the
             * engine made of it. A stored ON on an install that lost its modes
             * data is refused here, exactly as the service's restore path
             * refuses it, so the row cannot promise a state the judgement will
             * not use. */
            HunminNativeBridge.nativeLayerSetOption(
                layer,
                OPTION,
                if (readStored()) 1 else 0,
            )
            writeStored(option() == 1)
        }
    }

    /** The three facts ReqMode13's surface B has to display. */
    data class State(
        val on: Int?,
        val key: String,
        val dataPresent: Boolean,
    ) {
        val usable: Boolean get() = on != null && dataPresent
    }

    /** Read-only: the layer already carries the stored value since [init]. */
    fun state(): State {
        if (layer == 0L) return State(null, "", false)
        return State(
            on = option(),
            key = SpecialFunction.chordLabel(
                HunminNativeBridge.nativeLayerSpecialChord(
                    layer,
                    SpecialFunction.AUTOSWITCH,
                ),
            ),
            dataPresent = HunminNativeBridge.nativeCtxAutoswitchDataPresent(ctx),
        )
    }

    /**
     * Ask for [on] and return the engine's answer. The request is never
     * echoed: B3's gate can refuse ON, and the value stored is the read-back.
     */
    fun write(on: Boolean): State {
        if (layer == 0L) return state()
        HunminNativeBridge.nativeLayerSetOption(layer, OPTION, if (on) 1 else 0)
        writeStored(option() == 1)
        return state()
    }

    private fun option(): Int? =
        if (layer == 0L) null
        else HunminNativeBridge.nativeLayerGetOption(layer, OPTION)?.firstOrNull()

    override fun close() {
        if (layer != 0L) HunminNativeBridge.nativeLayerFree(layer)
        if (ctx != 0L) HunminNativeBridge.nativeCtxFree(ctx)
        layer = 0
        ctx = 0
    }

    private companion object {
        const val OPTION = "autoswitch"
    }
}
