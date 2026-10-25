/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * HunminAdapter — the Android adapter, and nothing else.
 *
 * Same shape as frontends/fcitx5/src/hunminadapter.cpp: map platform key events to the
 * engine's US-QWERTY base character plus the shift bit, feed
 * hunmin_ic_process_key, present the preedit and the commit. No composition
 * logic, no per-layout knowledge, no jamo handling — PLAN.md rule #4. The JNI
 * layer (hunmin-jni.c) is thinner still: handle conversion and UTF-8
 * marshalling only.
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

/** hunmin.h's HUNMIN_LANGUAGE_HANGUL; the layer reports its language as this value. */
private const val HUNMIN_LANGUAGE_HANGUL = 0 /* hunmin.h: HANGUL 0, LATIN 1 */


/**
 * One editing session's engine state.
 *
 * Not a singleton and not a service-wide field: [HunminInputMethodService]
 * creates one per `onStartInput`, which is what makes two visible editors
 * independent. The fcitx5 addon attaches the same thing to a per-InputContext
 * property for the same reason (FRONTEND-CONTRACT §2).
 *
 * @param native the engine surface, injectable so tier E1 can drive the voice
 *   path with no device; production is [JniOps].
 */
class HunminAdapter(
    private val ctx: Long,
    layoutId: String,
    /** The latin slot (FE7). When present the layer owns BOTH slots and the
     *  language action is `toggleLanguage()`; when absent the adapter is a
     *  one-language frontend and the caller performs the switch. */
    private val latinLayoutId: String? = null,
    /** The commit unit (issue #128): "word" buffers until the word boundary,
     *  "syllable" delivers each syllable as the engine finalises it. Both
     *  produce the same text; the choice is the user's (setting `commit_behavior`). */
    private val commitBehavior: String = "word",
    private val native: NativeOps = JniOps,
) : AutoCloseable {

    /** The native surface, narrowed to what this class calls. */
    interface NativeOps {
        /* The shared buffered/syllable layer (issue #128): one object answers
         * consumption, commit and preedit for every frontend, so this adapter
         * keeps no buffer of its own. */
        fun layerNew(ctx: Long, hangul: String, latin: String?, behavior: String): Long
        fun layerClassify(layer: Long, base: Int, shift: Int, mods: Int, id: Int, candidatesOpen: Boolean, languageKey: Boolean): Int
        fun layerFree(layer: Long)
        fun layerKey(layer: Long, base: Int, shift: Int): Boolean
        fun layerDelivered(layer: Long): String
        fun layerDisplay(layer: Long): String
        fun layerCommitWord(layer: Long): String
        fun layerExternalText(layer: Long, text: String): String
        fun layerReset(layer: Long)
        fun layerSetOption(layer: Long, name: String, value: Int): Boolean
        /** The layer's language action (FE7); true when it switched. */
        fun layerToggleLanguage(layer: Long): Boolean
        /** HUNMIN_LANGUAGE_* as the layer reports it. */
        fun layerLanguage(layer: Long): Int
        // Hanja candidates (P3)
        fun suggestCount(ctx: Long, hangul: String): Int
        fun suggestText(ctx: Long, i: Int): String
        fun suggestDetail(ctx: Long, i: Int): String
        fun suggestKind(ctx: Long, i: Int): String
    }

    private object JniOps : NativeOps {
        override fun layerNew(ctx: Long, hangul: String, latin: String?, behavior: String) =
            HunminNativeBridge.nativeLayerNew(ctx, hangul, latin, behavior)
        override fun layerFree(layer: Long) = HunminNativeBridge.nativeLayerFree(layer)
        override fun layerClassify(
            layer: Long, base: Int, shift: Int, mods: Int, id: Int,
            candidatesOpen: Boolean, languageKey: Boolean,
        ) = HunminNativeBridge.nativeLayerClassify(layer, base, shift, mods, id, candidatesOpen, languageKey)
        override fun layerKey(layer: Long, base: Int, shift: Int) =
            HunminNativeBridge.nativeLayerKey(layer, base, shift)
        override fun layerDelivered(layer: Long) = HunminNativeBridge.nativeLayerDelivered(layer)
        override fun layerDisplay(layer: Long) = HunminNativeBridge.nativeLayerDisplay(layer)
        override fun layerCommitWord(layer: Long) = HunminNativeBridge.nativeLayerCommitWord(layer)
        override fun layerExternalText(layer: Long, text: String) =
            HunminNativeBridge.nativeLayerExternalText(layer, text)
        override fun layerReset(layer: Long) = HunminNativeBridge.nativeLayerReset(layer)
        override fun layerSetOption(layer: Long, name: String, value: Int) =
            HunminNativeBridge.nativeLayerSetOption(layer, name, value)
        override fun layerToggleLanguage(layer: Long) =
            HunminNativeBridge.nativeLayerToggleLanguage(layer) != 0
        override fun layerLanguage(layer: Long) = HunminNativeBridge.nativeLayerLanguage(layer)
        // Hanja candidates (P3)
        override fun suggestCount(ctx: Long, hangul: String) = HunminNativeBridge.nativeSuggestCount(ctx, hangul)
        override fun suggestText(ctx: Long, i: Int) = HunminNativeBridge.nativeSuggestText(ctx, i)
        override fun suggestDetail(ctx: Long, i: Int) = HunminNativeBridge.nativeSuggestDetail(ctx, i)
        override fun suggestKind(ctx: Long, i: Int) = HunminNativeBridge.nativeSuggestKind(ctx, i)
    }

    /** 0 when the layout id is unknown or unsupported — the engine declines. */
    /* TWO slots, so the language action is the layer's (FE7): the layer holds
     * both ids and switches between them, which is what lets the three
     * frontends share one definition of a mode change. Until 2026-10-25 this
     * passed `null` for the latin slot and the service did the switch. */
    private var layer: Long = native.layerNew(ctx, layoutId, latinLayoutId, commitBehavior)

    /** The layer holds both slots, so it can perform the language action. */
    val ownsLanguageAction: Boolean get() = layer != 0L && latinLayoutId != null

    /** The language action (FE7): boundary, switch and other-slot attach, done
     *  by the layer. False means this adapter is one-language. */
    fun toggleLanguage(): Boolean {
        if (layer == 0L || latinLayoutId == null) return false
        return native.layerToggleLanguage(layer)
    }

    /** True when the layer is in Hangul; meaningful only with both slots. */
    fun isHangulLanguage(): Boolean =
        layer != 0L && native.layerLanguage(layer) == HUNMIN_LANGUAGE_HANGUL

    /** The layout this layer was built for; a mismatch forces a rebuild. */
    val layout: String = if (layer != 0L) layoutId else ""

    val isUsable: Boolean get() = layer != 0L

    // ---------------------------------------------------------------- keys

    /**
     * Feed one key event. Returns true when the engine consumed it.
     *
     * The structure is the fcitx5 adapter's, and for the same reasons:
     *
     *  1. Ctrl/Alt/Super/Meta chords flush and pass through. Shift is *not* in
     *     that set — it is a layout row, not a modifier (invariant 4).
     *  2. The key reaches the engine as its UNSHIFTED base plus the AOSP
     *     shift bit, verbatim (hunmin_ic_process_key), and CapsLock as the
     *     `caps` option: the CapsLock×Shift case endpoint is the engine's,
     *     not this adapter's (invariant 5 of §5; FRONTEND-CONTRACT §3 A3',
     *     SPEC-P2 "IC input entry points").
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
        if (layer == 0L) return false

        /* The key's MEANING is the layer's answer (issue #128, decision C): the
         * adapter maps the platform key into the shared vocabulary -- unshifted
         * base, shift bit, chord bits; this path has no candidate window of its
         * own -- and the precedence (chord, unmapped, feed) is asserted once in
         * tier A's engine-key-rule instead of here. */
        val base0 = HunminKeyRegistry.engineBase(keyCode)
        val action = native.layerClassify(
            layer,
            base0?.code ?: 0,
            if (HunminKeyRegistry.isShiftOn(metaState)) 1 else 0,
            if (HunminKeyRegistry.isModifierChord(metaState)) HUNMIN_MOD_CHORD else 0,
            HUNMIN_KEY_TEXT,
            false,
            false,
        )
        if (action == HUNMIN_KEY_BOUNDARY_PASS) {
            // A chord or an unmapped key cannot compose: it is a word boundary
            // AND a pass-through, and the key then reaches the client.
            commitWord(ic_)
            return false
        }

        val caps = (metaState and android.view.KeyEvent.META_CAPS_LOCK_ON) != 0
        // Issue #112 (ruling "wire and make it common to all platform"): the
        // option is fed 1/0 PER EVENT, the fcitx5 shape (hunminadapter.cpp:327).
        // The earlier set-1-only feed left ic->caps sticky-on for the ic's
        // lifetime -- after the first CapsLock use every later key kept
        // resolving the P=1 tier. 0 is also a feed, not an absence.
        native.layerSetOption(layer, "caps", if (caps) 1 else 0)

        val base = base0
        if (base == null) {
            // Unmapped key: word boundary first, then pass through -- space,
            // Enter, punctuation reach the client AFTER the word lands. The
            // classifier already answered the same way above; this keeps the
            // non-null type for the mapping below.
            commitWord(ic_)
            return false
        }

        val tracing = HunminTraceRecorder.isEnabled
        // What the ADAPTER decided this key means, for the tier E3 trace: the
        // unshifted base, the modifier bits travelling verbatim in [metaState].
        // The layer a device quirk actually shows up in is the adapter's
        // mapping, not the engine's response to it.
        val tracedChar = if (tracing) base else null

        // The case endpoint is the ENGINE's (issue #105): unshifted base +
        // shift bit + the caps option above. hunmin_ic_process_key resolves
        // the row from the loader's own US-QWERTY map, so CapsLock×Shift is
        // no longer re-derived here -- the site of the earlier wrong-case
        // defect (FRONTEND-CONTRACT §3 A3') is deleted, not rewritten. The
        // bit passes verbatim, so CapsLock+Shift now composes the shifted
        // row; that delta is recorded in A3' and reversing it would be a
        // one-line change here awaiting a ruling.
        val used = native.layerKey(
            layer, base.code, if (HunminKeyRegistry.isShiftOn(metaState)) 1 else 0)
        /* One delivery point: whatever the layer decided to hand the client
         * belongs in the client, in the unit the user chose (word at the
         * boundary, or per syllable). A declined key is a boundary the layer
         * has ALREADY delivered, so there is no second path here -- the shape
         * the three adapters used to spell out separately. */
        delivered = native.layerDelivered(layer)
        if (delivered.isNotEmpty()) ic_.commitText(delivered, 1)
        updatePreedit(ic_)
        if (tracing) {
            HunminTraceRecorder.event(keyCode, metaState, tracedChar, delivered,
                              preedit(), used)
        }
        return used
    }

    /** What the last onKey delivered to the client, for the trace. */
    private var delivered: String = ""

    /**
     * Commit the word-so-far to the client and clear the buffer.
     *
     * Named for what it now DOES rather than inherited from raw ABI naming:
     * hunmin_ic_flush only ends the engine's composition; this joins that
     * remainder with the word and hands the WHOLE word to the client in one
     * commitText. It is the word boundary primitive of issue #75 -- a call
     * site needs it when the word in progress is done because a key that is
     * not a layout key arrived, the user switched mode or layout, or the
     * client is about to act on its own (the service's Space handler).
     *
     * @return what reached the client ("" when there was nothing).
     */
    fun commitWord(ic_: InputConnection): String {
        if (layer == 0L) return ""
        val word = native.layerCommitWord(layer)
        if (word.isNotEmpty()) ic_.commitText(word, 1)
        // The commit replaces the composing region's text one-for-one -- the
        // client was shown exactly [word] as composing text -- but the region
        // itself still carries our composing flag, so a clear must follow the
        // commit or a stale syllable stays on screen over the committed word
        // (measured per the preedit test in HunminAdapterTest).
        updatePreedit(ic_)
        return word
    }

    /**
     * Whether the client is currently showing a composing region THIS engine put
     * there. Per-session, because the service builds one engine per editor and
     * closes it in onFinishInput, so it cannot leak across two visible editors.
     */
    private var composing = false

    /**
     * Replace the in-flight composition with a Hanja chosen by the user.
     *
     * NOT [commitExternalText]: that one is invariant 5's pass-through for text
     * from outside the keyboard (voice), and its contract is deliberately
     * "the typed text followed by the spoken text". A Hanja conversion is the
     * opposite -- the Hangul being composed IS the thing being converted, so it
     * must be discarded, not committed before the Hanja. Flushing first and then
     * committing produced "가恨" instead of "恨", which is what the
     * composition-replacement test in HunminAdapterTest asserts against; reset()
     * drops the syllable without committing it, so only the Hanja lands.
     */
    fun commitHanja(ic_: InputConnection, hanja: String) {
        if (layer == 0L) return
        // The buffered word is part of the composition being replaced: the
        // Hanja lands ONE FOR ONE for word + in-flight, never after it.
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
        if (layer == 0L) return
        // The layer delivers the typed word first, then the engine appends the
        // spoken text, so the committed order stays "typed, then spoken".
        val out = native.layerExternalText(layer, text)
        if (out.isNotEmpty()) ic_.commitText(out, 1)
        // The preedit the client was showing is now stale -- either committed
        // by the pass-through or dropped with it -- so it must be cleared
        // explicitly. updatePreedit reads a non-destructive preedit that the
        // commit_text call did not clear for us.
        updatePreedit(ic_)
    }

    /** Drop the composition silently, as on focus change. */
    fun reset(ic_: InputConnection) {
        if (layer == 0L) return
        // The word is the layer's now, so dropping it is one call: leaving it
        // would show a word the engine has already forgotten.
        native.layerReset(layer)
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
    fun preedit(): String = if (layer != 0L) native.layerDisplay(layer) else ""

    /**
     * Push the current preedit to the client.
     *
     * setComposingText with an empty string is what CLEARS the composing region:
     * there is no "clear preedit" call, and an IME that stops updating leaves
     * its last composition on screen forever. Called after every state change,
     * not only after a commit, because the preedit is the composition and the
     * composition changes on every keystroke.
     *
     * BUT it is only safe when there is a composing region of OURS to replace.
     * `setComposingText("", 1)` against a field that has text and no composition
     * is the editor being told "the composing text is now the empty string, one
     * position from here", and with nothing of ours there to replace, editors
     * read it as "delete the character before the cursor" -- the last character
     * of the text, disappearing the moment the field is focused. Reported by the
     * maintainer against R5KL808WBPN (2026-10-12); the cause was read off this
     * code path rather than measured, and the fix was verified on that device.
     * The guard is held by tier E1, not by a device test.
     */
    fun updatePreedit(ic_: InputConnection) {
        if (layer == 0L) return
        // What the user reads: the layer's display string.
        val shown = native.layerDisplay(layer)
        if (shown.isEmpty() && !composing) return
        ic_.setComposingText(shown, 1)
        composing = shown.isNotEmpty()
    }

    override fun close() {
        if (layer != 0L) {
            native.layerFree(layer)
            layer = 0L
        }
    }

    companion object {
        /* The shared key vocabulary (hunmin.h). Literals rather than a JNI call
         * per constant: they are contract values, and tier A's engine-key-rule
         * is what pins their meaning. */
        private const val HUNMIN_MOD_CHORD = 0x07 /* CTRL | ALT | GUI */
        private const val HUNMIN_KEY_TEXT = 0
        private const val HUNMIN_KEY_BOUNDARY_PASS = 1

        /** Editor info hint: we do our own composition, so say so. */
        const val IME_FLAG_NO_PERSONALIZED_LEARNING: Int =
            EditorInfo.IME_FLAG_NO_PERSONALIZED_LEARNING
    }
}
