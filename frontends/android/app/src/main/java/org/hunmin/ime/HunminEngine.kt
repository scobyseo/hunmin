/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * HunminEngine — the Android adapter, and nothing else.
 *
 * Same shape as frontends/fcitx5/src/engine.cpp: map platform key events to the
 * engine's US-QWERTY character convention, feed hunmin_ic_process, present the
 * preedit and the commit. No composition logic, no per-layout knowledge, no jamo
 * handling — PLAN.md rule #4. The JNI layer (hunmin-jni.c) is thinner still:
 * handle conversion and UTF-8 marshalling only.
 *
 * What is deliberately absent, and why each is not an oversight:
 *
 *  - No keyboard view. P5 is a hardware-keyboard IME; a soft keyboard is
 *    later work (docs/FRONTEND-CONTRACT.md). See HunminInputMethodService for
 *    why that is a real design constraint rather than a simplification.
 *  - No F9 / Hanja window. F9 has no AOSP keycode at all — on Android the
 *    function row is a modifier combination — so the trigger has to be chosen
 *    per-device or deferred to P3's own decision. tools/keymap-fixture.py
 *    declares F9 in ANDROID_UNREACHABLE for exactly this reason.
 *  - No surrounding text. Invariant I9: many Android clients implement
 *    getTextBeforeCursor badly or not at all, and the Fcitx5 addon is forbidden
 *    from reading it for the same reason.
 *
 * This class holds the hunmin_ic, one per InputConnection, and nothing else
 * does. Two visible editors must not share a composition buffer.
 */
package org.hunmin.ime

import android.view.inputmethod.InputConnection
import android.view.inputmethod.EditorInfo

/**
 * One editing session's engine state.
 *
 * Not a singleton and not a service-wide field: [HunminInputMethodService]
 * creates one per `onStartInput`, which is what makes two visible editors
 * independent. The fcitx5 addon attaches the same thing to a per-InputContext
 * property for the same reason (FRONTEND-CONTRACT §2).
 *
 * @param commitText injected so tier E1 can drive the voice path with no device
 *   and no JNI; production passes [HunminNative]::nativeCommitText.
 */
class HunminEngine(
    private val ctx: Long,
    layoutId: String,
    private val native: NativeOps = JniOps,
) : AutoCloseable {

    /** The native surface, narrowed to what this class calls. */
    interface NativeOps {
        fun icNew(ctx: Long, layout: String): Long
        fun icFree(ic: Long)
        fun process(ic: Long, ch: Int): Boolean
        fun commit(ic: Long): String
        fun preedit(ic: Long): String
        fun flush(ic: Long): String
        fun reset(ic: Long)
        fun setOption(ic: Long, name: String, value: Int): Boolean
        fun commitText(ic: Long, text: String): String
        // Hanja candidates (P3)
        fun suggestCount(ctx: Long, hangul: String): Int
        fun suggestText(ctx: Long, i: Int): String
        fun suggestDetail(ctx: Long, i: Int): String
        fun suggestKind(ctx: Long, i: Int): String
    }

    private object JniOps : NativeOps {
        override fun icNew(ctx: Long, layout: String) = HunminNative.nativeIcNew(ctx, layout)
        override fun icFree(ic: Long) = HunminNative.nativeIcFree(ic)
        override fun process(ic: Long, ch: Int) = HunminNative.nativeProcess(ic, ch)
        override fun commit(ic: Long) = HunminNative.nativeCommit(ic)
        override fun preedit(ic: Long) = HunminNative.nativePreedit(ic)
        override fun flush(ic: Long) = HunminNative.nativeFlush(ic)
        override fun reset(ic: Long) = HunminNative.nativeReset(ic)
        override fun setOption(ic: Long, name: String, value: Int) =
            HunminNative.nativeSetOption(ic, name, value)
        override fun commitText(ic: Long, text: String) = HunminNative.nativeCommitText(ic, text)
        // Hanja candidates (P3)
        override fun suggestCount(ctx: Long, hangul: String) = HunminNative.nativeSuggestCount(ctx, hangul)
        override fun suggestText(ctx: Long, i: Int) = HunminNative.nativeSuggestText(ctx, i)
        override fun suggestDetail(ctx: Long, i: Int) = HunminNative.nativeSuggestDetail(ctx, i)
        override fun suggestKind(ctx: Long, i: Int) = HunminNative.nativeSuggestKind(ctx, i)
    }

    /** 0 when the layout id is unknown or unsupported — the engine declines. */
    private var ic: Long = native.icNew(ctx, layoutId)

    /** The layout this ic was built for; a mismatch forces a rebuild. */
    val layout: String = if (ic != 0L) layoutId else ""

    val isUsable: Boolean get() = ic != 0L

    // ---------------------------------------------------------------- keys

    /**
     * Feed one key event. Returns true when the engine consumed it.
     *
     * The structure is the fcitx5 adapter's, and for the same reasons:
     *
     *  1. Ctrl/Alt/Super/Meta chords flush and pass through. Shift is *not* in
     *     that set — it is a layout row, not a modifier (invariant 4).
     *  2. CapsLock is handed to the engine as an option, with the letter
     *     un-shifted, because AOSP delivers the shifted form under CapsLock
     *     (invariant 5 of §5; FRONTEND-CONTRACT §3 A3).
     *  3. An unmapped key flushes and passes through. That is how space and
     *     friends reach the client.
     *  4. hunmin_ic_commit is read **or** hunmin_ic_flush is called, never both.
     *     Both are destructive reads of overlapping state and doing both emits
     *     the same text twice.
     *
     * Release events must be filtered by the caller, not here: see
     * [HunminInputMethodService.onKeyDown], which is the only place that knows
     * whether the platform has already delivered a matching `up`.
     */
    fun onKey(keyCode: Int, metaState: Int, ic_: InputConnection): Boolean {
        if (ic == 0L) return false

        if (HunminKeys.isModifierChord(metaState)) {
            flush(ic_)
            return false
        }

        val caps = (metaState and android.view.KeyEvent.META_CAPS_LOCK_ON) != 0
        if (caps) native.setOption(ic, "caps", 1)

        val ch = HunminKeys.engineChar(keyCode, metaState)
        if (ch == null) {
            flush(ic_)
            return false
        }

        val tracing = HunminTrace.isEnabled
        // What the ADAPTER decided this key means, for the tier E3 trace. The
        // layer a device quirk actually shows up in is the adapter's mapping, not
        // the engine's response to it.
        val tracedChar = if (tracing) ch else null

        val used = native.process(ic, ch.code)
        if (used) {
            // Read the destructive buffer exactly once. An empty string means
            // the key contributed no complete text yet, which is the normal case
            // while a syllable is composing.
            //
            // `delivered` holds it before the read is handed on, because
            // hunmin_ic_commit is destructive: a trace written after this point
            // would see "" for every commit and look like nothing was ever
            // committed while the user watched syllables appear.
            val commit = native.commit(ic)
            delivered = commit
            if (commit.isNotEmpty()) ic_.commitText(commit, 1)
        } else {
            // Engine declined. flush() emits the pending commit buffer AND the
            // in-flight syllable in one string, because hunmin_ic_flush appends
            // via save_commit and reads back through hunmin_ic_commit.
            delivered = if (tracing) captureFlush(ic_) else ""
            flush(ic_)
        }
        updatePreedit(ic_)
        if (tracing) {
            HunminTrace.event(keyCode, metaState, tracedChar, delivered,
                              native.preedit(ic), used)
        }
        return used
    }

    /** What the last onKey delivered to the client, for the trace. */
    private var delivered: String = ""

    /**
     * Flush and return the text, so the trace sees what the client got.
     *
     * Only called when tracing is on, because calling it twice would emit the
     * text twice -- hunmin_ic_flush is destructive. Hence the `tracing` guard at
     * the call site rather than here.
     */
    private fun captureFlush(target: InputConnection): String {
        val s = native.flush(ic)
        if (s.isNotEmpty()) target.commitText(s, 1)
        return s
    }

    /** Commit whatever is composing and hand it to the client. */
    fun flush(ic_: InputConnection) {
        if (ic == 0L) return
        val s = native.flush(ic)
        if (s.isNotEmpty()) ic_.commitText(s, 1)
        // flush() commits but does not clear the composing region on its own
        // (see the preedit test in HunminEngineTest): a caller that stops here
        // leaves the stale syllable on screen over the text it just committed.
        updatePreedit(ic_)
    }

    /**
     * Replace the in-flight composition with a Hanja chosen by the user.
     *
     * NOT [commitExternalText]: that one is invariant 5's pass-through for text
     * from outside the keyboard (voice), and its contract is deliberately
     * "the typed text followed by the spoken text". A Hanja conversion is the
     * opposite -- the Hangul being composed IS the thing being converted, so it
     * must be discarded, not committed before the Hanja. Flushing first and then
     * committing produced "가恨" instead of "恨", which is what the
     * composition-replacement test in HunminEngineTest asserts against; reset()
     * drops the syllable without committing it, so only the Hanja lands.
     */
    fun commitHanja(ic_: InputConnection, hanja: String) {
        if (ic == 0L) return
        reset(ic_)
        ic_.commitText(hanja, 1)
    }

    /**
     * Invariant 5: commit text that came from outside the keyboard.
     *
     * Voice input lands here and nowhere else. The whole point is that this
     * does NOT go through [onKey]: feeding spoken syllables to hunmin_ic_process
     * would compose them a second time. hunmin_ic_commit_text commits any
     * in-flight composition first, so the result is the typed text followed by
     * the spoken text, in the order the user produced them.
     */
    fun commitExternalText(ic_: InputConnection, text: String) {
        if (ic == 0L) return
        val out = native.commitText(ic, text)
        if (out.isNotEmpty()) ic_.commitText(out, 1)
        // The preedit the client was showing is now stale -- either committed
        // by the pass-through or dropped with it -- so it must be cleared
        // explicitly. updatePreedit reads a non-destructive preedit that the
        // commit_text call did not clear for us.
        updatePreedit(ic_)
    }

    /** Drop the composition silently, as on focus change. */
    fun reset(ic_: InputConnection) {
        if (ic == 0L) return
        native.reset(ic)
        updatePreedit(ic_)
    }

    // ------------------------------------------------------------ Hanja (P3)

    /** Get the number of Hanja candidates for the given Hangul text. */
    fun suggestCount(hangul: String): Int = native.suggestCount(ctx, hangul)

    /** Get the Hanja candidate text at index [i]. */
    fun suggestText(i: Int): String = native.suggestText(ctx, i)

    /** Get the detail (meaning/pronunciation) for candidate [i]. */
    fun suggestDetail(i: Int): String = native.suggestDetail(ctx, i)

    /** Get the kind ("hanja" | "idiom" | "word") for candidate [i]. */
    fun suggestKind(i: Int): String = native.suggestKind(ctx, i)

    /** Get the current preedit text for this engine's input context. */
    fun preedit(): String = if (ic != 0L) native.preedit(ic) else ""

    /**
     * Push the current preedit to the client.
     *
     * setComposingText with an empty string is what CLEARS the composing region:
     * there is no "clear preedit" call, and an IME that stops updating leaves
     * its last composition on screen forever. Called after every state change,
     * not only after a commit, because the preedit is the composition and the
     * composition changes on every keystroke.
     */
    fun updatePreedit(ic_: InputConnection) {
        if (ic == 0L) return
        val preedit = native.preedit(ic)
        ic_.setComposingText(preedit, 1)
    }

    override fun close() {
        if (ic != 0L) {
            native.icFree(ic)
            ic = 0L
        }
    }

    companion object {
        /** Editor info hint: we do our own composition, so say so. */
        const val IME_FLAG_NO_PERSONALIZED_LEARNING: Int =
            EditorInfo.IME_FLAG_NO_PERSONALIZED_LEARNING
    }
}
