/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * OverlayActivity — transparent Activity for showing dialogs/Toasts from IME Service.
 *
 * IME Service context cannot show dialogs/Toasts directly (no window token).
 * This transparent Activity is launched with FLAG_ACTIVITY_NEW_TASK and provides
 * a valid window context for dialogs, Toasts, and other UI.
 */
package org.hunmin.ime

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.util.Log
import android.view.KeyEvent
import android.view.Gravity
import android.view.View
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity

class OverlayActivity : AppCompatActivity() {

    private var dialog: AlertDialog? = null

    /**
     * Non-null exactly while a capture dialog is waiting for a chord.
     *
     * The flag [capturingDone] makes the two capture paths (the focused view's
     * listener and the Activity's [dispatchKeyEvent]) mutually exclusive: the
     * first to answer stores the chord, and the other returns without storing.
     * Both exist because which one the platform routes a hardware key through is
     * not something this code controls, and a settings window that silently
     * captures nothing is worse than one that captures twice.
     */
    private var capturingAction: KeyAction? = null
    private var capturingDone = false

    /**
     * The capture dialog, held separately from [dialog].
     *
     * It is replaced by the list dialog the moment a chord is stored, and
     * dismissing a window that is already being torn down leaves Android logging
     * `WindowLeaked: ... DecorView[OverlayActivity]` on destroy. Holding the
     * reference lets onDestroy close both without depending on which one the
     * shared field points at.
     */
    private var captureWindow: AlertDialog? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Theme comes from AndroidManifest (Theme.HunminOverlay, AppCompat-derived).
        // Do NOT setTheme() to a platform theme here: the AppCompat AlertDialog
        // path needs the AppCompat-derived theme to be in effect.
    }

    override fun onResume() {
        super.onResume()
        val action = intent?.action ?: return finish()

        Log.d(TAG, "onResume action=$action")

        when (action) {
            ACTION_SHOW_HANJA -> showHanjaDialog()
            ACTION_SHOW_SETTINGS -> showSettingsDialog()
            ACTION_SHOW_TOAST -> showToast()
            else -> finish()
        }
    }

    override fun onPause() {
        super.onPause()
        // Don't finish immediately if dialog is showing
        if (dialog == null || !dialog!!.isShowing) {
            finish()
        }
    }

    override fun onDestroy() {
        // A capture left installed would swallow the next keystroke the user
        // types into an editor, with no window on screen to explain why.
        KeyCaptureSession.clear()
        capturingAction = null
        captureWindow?.dismiss()
        captureWindow = null
        dialog?.dismiss()
        dialog = null
        super.onDestroy()
    }

    private fun showHanjaDialog() {
        val preedit = intent?.getStringExtra(EXTRA_HANJA_PREEDIT) ?: return finish()
        val candidates = intent?.getStringArrayExtra(EXTRA_HANJA_CANDIDATES) ?: return finish()
        val details = intent?.getStringArrayExtra(EXTRA_HANJA_DETAILS) ?: return finish()

        Log.d(TAG, "showHanjaDialog preedit=$preedit, candidates=${candidates.size}")

        val items = candidates.mapIndexed { idx, text ->
            // detail is the engine's "ko | en [tag]" gloss; drop the parens
            // entirely when it is empty (a rare Hanja with neither gloss), so
            // the candidate is not shown as a bare "字 ()".
            val detail = details.getOrNull(idx)?.trim().orEmpty()
            if (detail.isEmpty()) text else "$text  ($detail)"
        }.toTypedArray()

        dialog = AlertDialog.Builder(this)
            .setTitle("한자 선택: $preedit")
            .setItems(items) { _, which ->
                val selected = candidates[which]
                // An IME Service cannot receive an activity result, so the
                // selection goes back over a broadcast the service listens for
                // (see ACTION_HANJA_SELECTED and HanjaReceiver in
                // HunminInputMethodService). setResult() alone is dead here.
                val result = Intent(ACTION_HANJA_SELECTED).apply {
                    setPackage(packageName)
                    putExtra(EXTRA_SELECTED_HANJA, selected)
                }
                sendBroadcast(result)
                finish()
            }
            .setNegativeButton("취소") { _, _ -> finish() }
            .setOnCancelListener { finish() }
            .setOnDismissListener { finish() }
            .show()
    }

/**
     * The four configurable triggers, each showing the chord that is in force.
     *
     * Reads through [KeyBindings] rather than the raw preference, so a stored
     * binding and the default it replaced are displayed by the same code path
     * that decides whether a keystroke fires. That is the whole point of the
     * rewrite: the window used to write `key_<Korean label>` and read
     * `key_hanja`, so what it showed and what it saved had nothing to do with
     * each other.
     */
    private fun showSettingsDialog() {
        val store = KeyBindings.from(this)
        val actions = KeyAction.values().toList()
        val labels = actions.map { action ->
            // No default and nothing stored is a real state now: 한/영 전환 and
            // 한자 입력 are bound to PHYSICAL keys on a Korean keyboard, so the
            // row starts empty and the fixed keys still work. Printing the first
            // default would either crash on an empty list or, worse, imply a
            // chord the IME does not actually answer to.
            val combos = KeyBindings.combos(store, action)
            val shown = combos.firstOrNull()?.let { describe(it) } ?: NOT_SET
            "${action.label}: $shown"
        }.toTypedArray()

        dialog?.dismiss()
        dialog = AlertDialog.Builder(this)
            .setTitle("키 바인딩 설정")
            .setItems(labels) { _, which -> showKeyCaptureDialog(actions[which]) }
            .setNegativeButton("닫기") { _, _ -> finish() }
            .setOnCancelListener { finish() }
            .setOnDismissListener { if (!isChangingConfigurations) dialog = null }
            .show()
    }

    /**
     * Wait for one key chord and store it.
     *
     * Two platform facts shape this, both measured on the emulator (issue #12):
     *
     *  - A bare `TextView` with a key listener never sees a keystroke. It is not
     *    focusable and nothing focuses it; the dialog window takes the focus and
     *    the listener is dead code. So the view is made focusable *and* focused,
     *    and the Activity watches `dispatchKeyEvent` as a second path — whichever
     *    the platform routes, the first one to answer wins and [captured] stops
     *    the other from storing a second chord.
     *  - Holding Alt delivers `KEYCODE_ALT_LEFT` before `KEYCODE_X`. Capturing
     *    that first event would bind the action to a bare Left Alt, which is not
     *    what anybody pressing Shift+한자 meant. Modifiers are skipped while
     *    down, and a modifier becomes the binding only on its *release*, when
     *    nothing else followed it.
     */
    private fun showKeyCaptureDialog(action: KeyAction) {
        val view = TextView(this).apply {
            text = "${action.label}: 키를 누르세요\n(취소하려면 뒤로 가기)"
            textSize = 18f
            gravity = Gravity.CENTER
            setPadding(48, 48, 48, 48)
            isFocusable = true
            isFocusableInTouchMode = true
        }

        dialog?.dismiss()
        capturingAction = action
        capturingDone = false

        val captureDialog = AlertDialog.Builder(this)
            .setTitle("${action.label} 키 설정")
            .setView(view)
            .setCancelable(true)
            .setOnCancelListener {
                // Cancel stores nothing: the old one wrote the literal string
                // "취소됨" into the preference and finished the window.
                capturingAction = null
                capturingDone = false
                KeyCaptureSession.clear()
                captureWindow?.dismiss()
                captureWindow = null
                dialog?.dismiss()
                showSettingsDialog()
            }
            .show()
        // The service feeds the capture -- see KeyCaptureSession for why the
        // activity cannot listen for itself. Installed before the dialog shows so
        // a key pressed the moment it appears is not lost.
        KeyCaptureSession.install { keyAction, keyCode, metaState ->
            handleCaptureKey(keyAction, keyCode, metaState)
        }

        dialog = captureDialog
        captureWindow = captureDialog
        // Attached immediately, NOT in onShow: measured on the AVD, a listener
        // installed from onShow is too late — the dialog's decor view had already
        // taken input focus and the key went to the activity window instead of
        // the dialog, where nothing was listening.
        val decor = captureDialog.window?.decorView
        decor?.isFocusableInTouchMode = true
        decor?.setOnKeyListener { _, _, event -> handleCaptureKey(event) }
        decor?.requestFocus()
        view.requestFocus()
        captureDialog.setOnDismissListener {
            if (!isChangingConfigurations) dialog = null
        }

        view.setOnKeyListener { _, keyCode, event ->
            if (handleCaptureKey(event)) true else false
        }
    }

    /**
     * The one place a chord becomes a stored binding.
     *
     * Modifiers on their own are ignored while held: pressing Left Alt before X
     * delivers `KEYCODE_ALT_LEFT` first, and storing that would bind 한/영 to a
     * bare Left Alt. A modifier becomes the binding only on release, which is how
     * "I want that key itself" is expressed.
     */
    private fun handleCaptureKey(keyAction: Int, keyCode: Int, metaState: Int): Boolean {
        val action = capturingAction ?: return false
        if (capturingDone) return true

        val modifier = KeyBinding.isModifierKey(keyCode)
        when (keyAction) {
            KeyEvent.ACTION_DOWN -> if (!modifier) storeChord(action, keyCode, metaState)
            KeyEvent.ACTION_UP -> if (modifier) storeChord(action, keyCode, 0)
            else -> return false
        }
        return true
    }

    /** The Activity's own listener, for the case where no IME holds the keys. */
    private fun handleCaptureKey(event: KeyEvent): Boolean =
        handleCaptureKey(event.action, event.keyCode, event.metaState)

    /** Store [combo] for [action] and return to the list, exactly once. */
    private fun storeChord(action: KeyAction, keyCode: Int, metaState: Int) {
        if (capturingDone) return
        capturingDone = true
        capturingAction = null
        KeyCaptureSession.clear()
        val combo = KeyCombo(keyCode, KeyBinding.normalise(metaState))
        Log.d(TAG, "captured ${action.prefKey} = ${combo.keyCode}/${combo.modifiers}")
        captureWindow?.dismiss()
        captureWindow = null
        val store = KeyBindings.from(this)
        // Warn BEFORE writing, against the state that is in force right now —
        // otherwise a chord already claimed by another action reads as "no
        // conflict" because this action's own new row is what is being compared.
        val clash = KeyBindings.conflict(store, action, combo)
        KeyBindings.set(store, action, combo)
        if (clash != null) {
            // The binding is stored anyway — a user who wants the same key on two
            // actions gets both — but which of the two actually fires is decided
            // by precedence, and saying so is the whole point of the warning. The
            // loser is named, because "이미 사용 중" without that would be a
            // warning the reader cannot act on.
            val text = if (clash.precedence < action.precedence) {
                "${clash.label} 가 먼저 처리됩니다 — 방금 설정한 ${action.label} 는 동작하지 않습니다"
            } else {
                "${clash.label} 는 동작하지 않게 됩니다 — 방금 설정한 ${action.label} 가 먼저 처리됩니다"
            }
            Toast.makeText(this, "$text\n($combo)", Toast.LENGTH_LONG).show()
        } else {
            Toast.makeText(this, "${action.label}: ${describe(combo)}", Toast.LENGTH_SHORT).show()
        }
        // Back to the list rather than finish(): one binding per visit made the
        // window useless for the second key, and the new value has to be visible.
        dialog?.dismiss()
        showSettingsDialog()
    }

    /**
     * Second capture path, for when the dialog's own view never gets focus.
     *
     * Only active while a capture dialog is up ([capturingAction] non-null), and
     * only for a chord the user could plausibly mean: a modifier on its own is
     * ignored here so a half-typed chord does not bind only its modifier.
     */
    /**
     * Second capture path, for when the dialog's own view never gets focus.
     *
     * Returns true while a capture is pending so the chord never leaks to
     * whatever is behind the dialog; before this, a captured key could fall
     * through to the IME and type a character into the field the user was editing.
     */
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (capturingAction == null) return super.dispatchKeyEvent(event)
        handleCaptureKey(event)
        return true
    }

    /**
     * A chord as the user reads it: `SHIFT+EISU`, `ALT_RIGHT` on its own.
     *
     * The only place in the feature that needs the framework's name lookup —
     * `KeyEvent.keyCodeToString` is a method, so it is unavailable to tier E1's
     * JVM tests, which is part of why storage is numeric (KeyBinding's header).
     */
    private fun describe(combo: KeyCombo): String {
        val parts = mutableListOf<String>()
        if ((combo.modifiers and KeyBinding.META_ALT_LEFT) != 0) parts.add("ALT_LEFT")
        if ((combo.modifiers and KeyBinding.META_ALT_RIGHT) != 0) parts.add("ALT_RIGHT")
        if ((combo.modifiers and KeyBinding.META_SHIFT) != 0) parts.add("SHIFT")
        parts.add(KeyEvent.keyCodeToString(combo.keyCode).removePrefix("KEYCODE_"))
        return parts.joinToString("+")
    }

    private fun showToast() {
        val text = intent?.getStringExtra(EXTRA_TOAST_TEXT) ?: return finish()
        Toast.makeText(this, text, Toast.LENGTH_SHORT).show()
        finish()
    }

    companion object {
        private const val TAG = "OverlayActivity"

        /** Shown for an action with no binding; the fixed keys still answer. */
        const val NOT_SET = "설정 안 됨 (고정 키 사용)"
        const val ACTION_SHOW_HANJA = "org.hunmin.ime.action.SHOW_HANJA"
        const val ACTION_SHOW_SETTINGS = "org.hunmin.ime.action.SHOW_SETTINGS"
        const val ACTION_SHOW_TOAST = "org.hunmin.ime.action.SHOW_TOAST"
        /** Sent back to the IME when the user picks a Hanja candidate. */
        const val ACTION_HANJA_SELECTED = "org.hunmin.ime.action.HANJA_SELECTED"

        const val EXTRA_HANJA_PREEDIT = "preedit"
        const val EXTRA_HANJA_CANDIDATES = "candidates"
        const val EXTRA_HANJA_DETAILS = "details"
        const val EXTRA_TOAST_TEXT = "text"

        const val RESULT_HANJA_SELECTED = 1
        const val EXTRA_SELECTED_HANJA = "selected_hanja"

        fun startHanja(activity: Activity, preedit: String, candidates: Array<String>, details: Array<String>) {
            val intent = Intent(activity, OverlayActivity::class.java).apply {
                action = ACTION_SHOW_HANJA
                putExtra(EXTRA_HANJA_PREEDIT, preedit)
                putExtra(EXTRA_HANJA_CANDIDATES, candidates)
                putExtra(EXTRA_HANJA_DETAILS, details)
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            activity.startActivity(intent)
        }

        fun startSettings(activity: Activity) {
            val intent = Intent(activity, OverlayActivity::class.java).apply {
                action = ACTION_SHOW_SETTINGS
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            activity.startActivity(intent)
        }

        fun showToast(activity: Activity, text: String) {
            val intent = Intent(activity, OverlayActivity::class.java).apply {
                action = ACTION_SHOW_TOAST
                putExtra(EXTRA_TOAST_TEXT, text)
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            activity.startActivity(intent)
        }
    }
}