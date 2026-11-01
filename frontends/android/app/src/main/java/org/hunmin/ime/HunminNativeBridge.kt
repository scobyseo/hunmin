/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * HunminNativeBridge — the external declarations for hunmin-jni.c.
 *
 * Split from HunminAdapter on purpose: the engine holds logic worth unit-testing,
 * this class holds none. Everything below is a one-line pass-through to a C
 * function, so a mocked InputConnection test can construct the engine around it
 * without a device, a .so, or a JNI environment.
 *
 * Two ABI details worth keeping, because both fail late and confusingly:
 *
 *  - @JvmStatic is load-bearing. Without it these compile to *instance* methods
 *    on HunminNativeBridge.INSTANCE, and JNI then hands the C side a jobject where the
 *    signature says jclass. Both are pointers to _jobject so nothing crashes,
 *    and every function here ignores the parameter -- which is exactly why the
 *    mistake would survive a full emulator run and only surface as a linker
 *    warning about an undefined symbol the day someone reorders a parameter.
 *
 *  - System.loadLibrary searches the APK's lib/<abi>/, which is where
 *    tools/build-android.sh's CMake output lands once AGP packages
 *    build-android/<abi>/jniLibs as a jniLibs source dir. A missing .so throws
 *    here, at first touch, which is the only point where the message can still
 *    name the library; catching it later would hide an ABI mismatch behind "the
 *    IME silently does nothing".
 */
package org.hunmin.ime

object HunminNativeBridge {
    init {
        // HUNMIN_JNI_LIB lets tier E1 load a HOST build of the same shim from an
        // absolute path. A JVM unit test cannot dlopen an Android .so -- the ELF
        // machine type is wrong and there is no bionic -- so without this the
        // only way to run the engine under test on a developer machine would be
        // an emulator, and an emulator per unit test is a tier E2 cost paid for
        // E1's assertions. The shim is the same C file either way; only the
        // linker differs.
        val explicit = System.getenv("HUNMIN_JNI_LIB")
        if (explicit.isNullOrEmpty()) {
            // "hunjni", NOT "hunmin". The engine is libhunmin.so and this shim is
            // libhunjni.so; when both were called libhunmin.so the shim's build
            // output collided with its own link input and the APK shipped the
            // bare engine with no adapter. See app/src/main/cpp/CMakeLists.txt.
            System.loadLibrary("hunjni")
        } else {
            System.load(explicit)
        }
    }

    /** ctx = hunmin_ctx_new([dataDir]) — 0 on failure (no layout found). */
    @JvmStatic external fun nativeCtxNew(dataDir: String): Long

    @JvmStatic external fun nativeCtxFree(ctx: Long)

    @JvmStatic external fun nativeLayoutCount(ctx: Long): Int

    /** "" when [i] is out of range, never null. */
    @JvmStatic external fun nativeLayoutId(ctx: Long, i: Int): String

    @JvmStatic external fun nativeLayoutName(ctx: Long, i: Int, lang: String?): String

    @JvmStatic external fun nativeLayoutSupported(ctx: Long, i: Int): Boolean

    /** "hangul" | "latin" — the layout JSON's `mode` (docs/SPEC-SCHEMA.md). */
    @JvmStatic external fun nativeLayoutMode(ctx: Long, i: Int): String

    /** ic = hunmin_ic_new(ctx, layout) — 0 for an unknown or unsupported id. */
    /** The shared buffered/syllable layer (issue #128). Two languages: a null
     *  latin layout makes the layer's Han/Eng action report "not ours".
     *  behavior is "word" (buffered) or "syllable". */
    @JvmStatic external fun nativeLayerNew(ctx: Long, hangul: String, latin: String?, behavior: String): Long

    /** The shared key rule (issue #128): base/shift/mods/key id in, action out.
     *  See hunmin_key_action in hunmin.h; the precedence lives in the layer. */
    @JvmStatic external fun nativeLayerClassify(
        layer: Long,
        base: Int,
        shift: Int,
        mods: Int,
        id: Int,
        candidatesOpen: Boolean,
        languageKey: Boolean,
    ): Int

    /**
     * The special-function host (hunmin.h hunmin_special_host): the object the
     * engine calls back into for supports/perform. Its two methods are looked up
     * by name in hunmin-jni.c, so [HunminSpecialHost] owns them and nothing here
     * may rename them. null unregisters.
     */
    @JvmStatic external fun nativeLayerSpecialHost(layer: Long, host: HunminSpecialHost?)

    /** >0 the function performed, -1 bound but this frontend refuses, 0 not a
     *  chord (the single press, which the classifier answers as a toggle). */
    @JvmStatic external fun nativeLayerSpecial(layer: Long, base: Int, shift: Int, mods: Int): Int

    /** The same three answers without calling the host -- the test phase. */
    @JvmStatic external fun nativeLayerSpecialPeek(layer: Long, base: Int, shift: Int, mods: Int): Int

    /** Seed or override one function's chord bits (the dialog's write path). */
    @JvmStatic external fun nativeLayerSpecialBind(layer: Long, fn: Int, mods: Int): Boolean

    /** The chord a function currently holds: 0 when unbound. */
    @JvmStatic external fun nativeLayerSpecialChord(layer: Long, fn: Int): Int

    @JvmStatic external fun nativeLayerFree(layer: Long)

    /** Feed one key: unshifted base + shift bit. Answers consumed. */
    @JvmStatic external fun nativeLayerKey(layer: Long, base: Int, shift: Int): Boolean

    /** What the last call delivered to the client ("" when nothing). */
    @JvmStatic external fun nativeLayerDelivered(layer: Long): String

    /** What the user should see composing (word so far + syllable in flight). */
    @JvmStatic external fun nativeLayerDisplay(layer: Long): String

    /** Commit the word now (focus loss, a chord, an unmapped key). */
    @JvmStatic external fun nativeLayerCommitWord(layer: Long): String

    /** External text such as voice input: word first, then the text. */
    @JvmStatic external fun nativeLayerExternalText(layer: Long, text: String): String

    @JvmStatic external fun nativeLayerReset(layer: Long)

    @JvmStatic external fun nativeLayerSetOption(layer: Long, name: String, value: Int): Boolean

    /** The layer's language action (FE7): 1 when it switched, 0 when one-language. */
    @JvmStatic external fun nativeLayerToggleLanguage(layer: Long): Int

    /** HUNMIN_LANGUAGE_HANGUL / _LATIN. */
    @JvmStatic external fun nativeLayerLanguage(layer: Long): Int

    /** R8's judgement, taken once per key (ruled 2026-11-01, #164): the mode to
     *  adopt now, or -1 when the engine judged nothing. */
    @JvmStatic external fun nativeLayerTakeJudgement(layer: Long): Int

    @JvmStatic external fun nativeIcNew(ctx: Long, layout: String): Long

    @JvmStatic external fun nativeIcFree(ic: Long)

    /**
     * hunmin_ic_process — the legacy case-encoding call. The Android ADAPTER
     * no longer uses it (it passes base + shift via [nativeProcessKey]); this
     * declaration stays because the tier C trace runner, TraceEmit, replays
     * the android corpus's engine-character events straight into the engine.
     */
    @JvmStatic external fun nativeProcess(ic: Long, ch: Int): Boolean

    /**
     * hunmin_ic_process_key — the explicit-modifier entry (issue #90; the
     * adapter migrated in #105). [base] is the UNSHIFTED US-QWERTY character
     * and [shift] is the AOSP META_SHIFT_ON bit, verbatim; the engine owns
     * the CapsLock×Shift case endpoint (docs/SPEC-P2 "IC input entry
     * points"). A nonzero [shift] selects the shifted row when the base has
     * one and is idempotent when it does not (the already-resolved rule).
     */
    @JvmStatic external fun nativeProcessKey(ic: Long, base: Int, shift: Int): Boolean

    /**
     * hunmin_ic_commit — a DESTRUCTIVE read: the text is delivered exactly once
     * and the buffer is cleared. Calling it twice for one keypress drops the
     * second result; calling it and then hunmin_ic_flush double-emits. See
     * docs/FRONTEND-CONTRACT.md §5 rules 1 and 2.
     */
    @JvmStatic external fun nativeCommit(ic: Long): String

    /** hunmin_ic_preedit — non-destructive, safe to re-read every keystroke. */
    @JvmStatic external fun nativePreedit(ic: Long): String

    /** hunmin_ic_flush — commits the in-flight syllable AND the commit buffer. */
    @JvmStatic external fun nativeFlush(ic: Long): String

    @JvmStatic external fun nativeReset(ic: Long)

    @JvmStatic external fun nativeSetOption(ic: Long, name: String, value: Int): Boolean

    /**
     * hunmin_ic_commit_text — invariant 5's external-text pass-through.
     *
     * Voice input goes here and nowhere else. The text is committed verbatim and
     * never fed through the jamo pipeline; any in-flight composition is
     * committed first so the two texts come out in the order the user produced
     * them. Returns the whole string (typed composition + spoken text), or ""
     * for malformed UTF-8.
     */
    @JvmStatic external fun nativeCommitText(ic: Long, text: String): String

    @JvmStatic external fun nativeSuggestCount(ctx: Long, hangul: String): Int

    @JvmStatic external fun nativeSuggestText(ctx: Long, i: Int): String

    @JvmStatic external fun nativeSuggestDetail(ctx: Long, i: Int): String

    @JvmStatic external fun nativeSuggestKind(ctx: Long, i: Int): String
}
