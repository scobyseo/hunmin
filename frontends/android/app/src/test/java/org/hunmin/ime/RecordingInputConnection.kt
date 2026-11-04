/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * RecordingInputConnection — an InputConnection that records instead of typing.
 *
 * Tier E1's whole instrument. The voice recogniser itself is an external service
 * and stays untested (TESTING.md E1), but everything between "a key arrived" and
 * "these exact characters reached the client" is ours, and it is assertable
 * against a recording fake.
 *
 * The interface is large and most of it is irrelevant here, so this fake
 * implements every abstract member and records only what an IME actually uses:
 *
 *  - commitText is the payload. Every character the user would see lands here.
 *  - setComposingText is what an IME calls "the preedit". Also the payload: a
 *    preedit that never reaches the client is a composition the user cannot see,
 *    and a stale one is a composition that will not go away.
 *  - finishComposingText is recorded too, because "the composing region ended"
 *    is the event a client needs in order to style the text as final.
 *
 * EVERYTHING SURROUNDING IS A TRAP, not a stub. getTextBeforeCursor,
 * getTextAfterCursor, getSelectedText, getSurroundingText, getExtractedText,
 * deleteSurroundingText, replaceText and commitContent all land in [violations]
 * instead of returning a harmless default. Invariant I9 forbids the adapter from
 * reading or rewriting surrounding text -- many clients implement it badly or
 * not at all -- so a fake that quietly answered those calls would let a real
 * violation pass. A non-empty [violations] is a test failure.
 *
 * Note there is deliberately NO `setComposingRegion` recording: the adapter sets
 * the composing region implicitly by calling setComposingText, and a fake that
 * silently accepted an explicit region call would hide an adapter that took a
 * code path nothing else exercises.
 */
package org.hunmin.ime

import android.os.Bundle
import android.os.Handler
import android.view.KeyEvent
import android.view.inputmethod.CompletionInfo
import android.view.inputmethod.CorrectionInfo
import android.view.inputmethod.ExtractedText
import android.view.inputmethod.ExtractedTextRequest
import android.view.inputmethod.InputContentInfo
import android.view.inputmethod.InputConnection

class RecordingInputConnection : InputConnection {

    /** Every commitText() payload, in order. What the user would have seen. */
    val commits = mutableListOf<String>()

    /** Every setComposingText() payload, in order, including "" (clear). */
    val composings = mutableListOf<String>()

    /** finishComposingText() count — the "this text is final now" signal. */
    var finishedComposing: Int = 0
        private set

    /**
     * Every sendKeyEvent() payload, in order.
     *
     * The adapters do NOT use this: an unconsumed key is reported by returning
     * false from onKey, and the service hands it to the platform through
     * super.onKeyDown(). The recorder exists so a future adapter that tried to
     * re-send the key itself (a double delivery) would be visible instead of
     * silently swallowed by a stub `= true`.
     */
    val sentEvents = mutableListOf<KeyEvent>()

    /**
     * Calls the adapter must never make. Invariant I9: no surrounding-text
     * reads, no surrounding-text rewrites.
     */
    val violations = mutableListOf<String>()

    /** Everything the user would see, concatenated — what most tests assert. */
    fun text(): String = commits.joinToString("")

    /** The composing region as the client last saw it. */
    fun composing(): String = composings.lastOrNull() ?: ""

    private fun forbid(what: String) {
        violations.add(what)
    }

    /** For a forbidden call that must still return the "I did it" boolean. */
    private fun forbidBool(what: String): Boolean {
        forbid(what)
        return true
    }

    // -------------------------------------------------------------- payload

    override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
        commits.add(text?.toString() ?: "")
        return true
    }

    override fun setComposingText(text: CharSequence?, cursor: Int): Boolean {
        composings.add(text?.toString() ?: "")
        return true
    }

    override fun finishComposingText(): Boolean {
        finishedComposing++
        composings.add("")
        return true
    }

    override fun setSelection(start: Int, end: Int): Boolean = true

    // ------------------------------------------------------- I9: forbidden

    override fun getTextBeforeCursor(length: Int, flags: Int): CharSequence? {
        forbid("getTextBeforeCursor($length)")
        return null
    }

    override fun getTextAfterCursor(length: Int, flags: Int): CharSequence? {
        forbid("getTextAfterCursor($length)")
        return null
    }

    override fun getSelectedText(flags: Int): CharSequence? {
        forbid("getSelectedText")
        return null
    }

    override fun getSurroundingText(beforeLength: Int, afterLength: Int, flags: Int):
            android.view.inputmethod.SurroundingText? {
        forbid("getSurroundingText")
        return null
    }

    override fun getExtractedText(request: ExtractedTextRequest?, flags: Int): ExtractedText {
        forbid("getExtractedText")
        return ExtractedText()
    }

    override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
        forbid("deleteSurroundingText($beforeLength,$afterLength)")
        return true
    }

    override fun deleteSurroundingTextInCodePoints(before: Int, after: Int): Boolean {
        forbid("deleteSurroundingTextInCodePoints")
        return true
    }

    // Signatures verified with javap against android.jar on 2026-10-03 rather
    // than from memory: replaceText takes four arguments (no TextAttribute),
    // reportFullscreenMode takes a boolean, getHandler returns a platform
    // android.os.Handler, and commitContent takes a Bundle third. Guessing these
    // produced three "overrides nothing" errors in a single compile.
    // replaceText is deliberately NOT overridden. It exists in android.jar as a
    // five-argument default method (API 33), but the mockable android.jar that
    // unit tests compile against is generated for minSdk 24 and has it stripped,
    // so overriding it here is a compile error ("overrides nothing") rather than
    // a safety net. The adapter does not call it: see updatePreedit, which uses
    // setComposingText only. If a future change does call replaceText, this fake
    // must gain the override AND minSdk must be reconsidered -- check the
    // compile error, it names the exact method.

    // Non-null InputContentInfo, per javap against android.jar.
    override fun commitContent(content: InputContentInfo, flags: Int,
                                opts: Bundle?): Boolean = forbidBool("commitContent")

    // -------------------------------------------------------------- ignored

    override fun setComposingRegion(start: Int, end: Int): Boolean = true

    override fun commitCompletion(text: CompletionInfo?): Boolean = true

    override fun commitCorrection(text: CorrectionInfo?): Boolean = true

    override fun performEditorAction(actionCode: Int): Boolean = true

    override fun performContextMenuAction(id: Int): Boolean = true

    override fun beginBatchEdit(): Boolean = true

    override fun endBatchEdit(): Boolean = true

    override fun sendKeyEvent(event: KeyEvent?): Boolean {
        if (event != null) sentEvents.add(event)
        return true
    }

    override fun clearMetaKeyStates(flags: Int): Boolean = true

    override fun reportFullscreenMode(isFullscreen: Boolean): Boolean = true

    override fun performPrivateCommand(action: String?, data: Bundle?): Boolean = true

    override fun requestCursorUpdates(cursorUpdateMode: Int): Boolean = true

    override fun getCursorCapsMode(reqModes: Int): Int = 0

    override fun getHandler(): Handler? = null

    override fun closeConnection() {}
}
