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
            ACTION_SHOW_SETTINGS -> openRequestedSection()
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
     * The settings window: three sections, each opening its own list.
     *
     * Sections rather than one long list because the three answer different
     * questions and their answers are of different shapes: a layout is chosen
     * from twelve names, a style from three pictures-of-things, a key from
     * whatever the user presses. Flattened into one list, every row would have
     * to describe itself — the thing this window was rewritten to stop doing.
     *
     * The layout lists come from the running IME through [ImeSettings]: the
     * engine's context lives in the service, so this Activity cannot enumerate
     * layouts on its own. When no service is up it says so, rather than showing
     * an empty list that reads as "you have no layouts".
     */
    private fun showSettingsDialog() {
        val entries = ImeSettings.layoutsProvider()
        val connected = ImeSettings.connected

        fun labelOf(mode: LayoutMode, id: String?): String =
            entries?.firstOrNull { it.id == id && it.mode == mode }?.label ?: "—"

        val sections = arrayOf(
            "한글 배열: ${labelOf(LayoutMode.HANGUL, ImeSettings.layoutChoice(LayoutMode.HANGUL))}",
            "영문 배열: ${labelOf(LayoutMode.LATIN, ImeSettings.layoutChoice(LayoutMode.LATIN))}",
            "입력기 창 스타일: ${ImeSettings.toolbarStyle().label}",
            "키 바인딩",
        )

        dialog?.dismiss()
        dialog = AlertDialog.Builder(this)
            .setTitle("hunmin 설정")
            .setItems(sections) { _, which ->
                when (which) {
                    0 -> showLayoutSection(LayoutMode.HANGUL)
                    1 -> showLayoutSection(LayoutMode.LATIN)
                    2 -> showStyleSection()
                    else -> showKeyBindingSection()
                }
            }
            .setNegativeButton("닫기") { _, _ -> finish() }
            .setOnCancelListener { finish() }
            .setOnDismissListener { if (!isChangingConfigurations) dialog = null }
            .show()

        if (!connected) {
            // Once, here, instead of every section looking empty in turn.
            Toast.makeText(
                this,
                "입력기가 실행 중이 아니라 배열 정보를 읽을 수 없습니다",
                Toast.LENGTH_LONG,
            ).show()
        }
    }

    /**
     * The layouts of one mode, in 가나다순, with the current one ticked.
     *
     * Sorted by [LayoutCatalog], never by whatever the engine's directory scan
     * produced — the maintainer's rule, and the scan order is opendir()'s.
     */
    /** Open whichever section the intent asked for, or the top level. */
    private fun openRequestedSection() {
        when (intent?.getStringExtra(EXTRA_SETTINGS_SECTION)) {
            SECTION_LAYOUT_HANGUL -> showLayoutSection(LayoutMode.HANGUL)
            SECTION_LAYOUT_LATIN -> showLayoutSection(LayoutMode.LATIN)
            SECTION_STYLE -> showStyleSection()
            SECTION_KEYS -> showKeyBindingSection()
            else -> showSettingsDialog()
        }
    }

    private fun showLayoutSection(mode: LayoutMode) {
        val entries = LayoutCatalog.byMode(ImeSettings.layoutsProvider().orEmpty(), mode)
        if (entries.isEmpty()) {
            Toast.makeText(this, "사용할 수 있는 배열이 없습니다", Toast.LENGTH_SHORT).show()
            return
        }
        val checked = entries.indexOfFirst { it.id == ImeSettings.layoutChoice(mode) }
        dialog?.dismiss()
        dialog = AlertDialog.Builder(this)
            .setTitle(if (mode == LayoutMode.LATIN) "영문 배열" else "한글 배열")
            .setSingleChoiceItems(
                entries.map { it.label }.toTypedArray(),
                checked,
            ) { d, which ->
                d.dismiss()
                ImeSettings.onLayoutChosen(mode, entries[which].id)
                showSettingsDialog()
            }
            .setNegativeButton("뒤로") { _, _ -> showSettingsDialog() }
            .setOnDismissListener { if (!isChangingConfigurations) dialog = null }
            .show()
    }

    /**
     * The three input-view shapes, each with what it costs: the choice is only
     * meaningful if the difference is visible before it is made, not after.
     */
    private fun showStyleSection() {
        val styles = ToolbarStyle.values()
        val checked = styles.indexOf(ImeSettings.toolbarStyle())
        val labels = styles.map { "${it.label}\n${it.blurb}" }.toTypedArray()
        dialog?.dismiss()
        dialog = AlertDialog.Builder(this)
            .setTitle("입력기 창 스타일")
            .setSingleChoiceItems(labels, checked) { d, which ->
                d.dismiss()
                ImeSettings.onToolbarStyleChosen(styles[which])
                showSettingsDialog()
            }
            .setNegativeButton("뒤로") { _, _ -> showSettingsDialog() }
            .setOnDismissListener { if (!isChangingConfigurations) dialog = null }
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
    private fun showKeyBindingSection() {
        val store = KeyBindings.from(this)
        val actions = KeyAction.values().toList()
        val labels = actions.map { action ->
            // No default and nothing stored is a real state now: 한/영 전환 and
            // 한자 입력 are bound to PHYSICAL keys on a Korean keyboard, so the
            // row starts empty and the fixed keys still work. Printing the first
            // default would either crash on an empty list or, worse, imply a
            // chord the IME does not answer to.
            val combos = KeyBindings.combos(store, action)
            val shown = combos.firstOrNull()?.let { describe(it) } ?: NOT_SET
            "${action.label}: $shown"
        }.toTypedArray()

        dialog?.dismiss()
        dialog = AlertDialog.Builder(this)
            .setTitle("키 바인딩")
            .setItems(labels) { _, which -> showKeyCaptureDialog(actions[which]) }
            .setNegativeButton("뒤로") { _, _ -> showSettingsDialog() }
            .setOnCancelListener { showSettingsDialog() }
            .setOnDismissListener { if (!isChangingConfigurations) dialog = null }
            .show()
    }
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
                showKeyBindingSection()
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
        // Back to the key list rather than finish(): one binding per visit made
        // the window useless for the second key, and the new value has to be
        // visible on the row it belongs to.
        dialog?.dismiss()
        showKeyBindingSection()
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

        // Section to open on, when the caller wants somewhere deeper than the
        // top level. String ids rather than ordinals: an ordinal would make an
        // old APK misread a new one.
        const val EXTRA_SETTINGS_SECTION = "section"
        const val SECTION_LAYOUT_HANGUL = "layout-hangul"
        const val SECTION_LAYOUT_LATIN = "layout-latin"
        const val SECTION_STYLE = "style"
        const val SECTION_KEYS = "keys"

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

        /** Open the settings window, optionally straight at one section. */
        fun startSettings(context: android.content.Context, section: String? = null) {
            val intent = Intent(context, OverlayActivity::class.java).apply {
                action = ACTION_SHOW_SETTINGS
                if (section != null) putExtra(EXTRA_SETTINGS_SECTION, section)
                // NEW_TASK because the caller is an InputMethodService: a
                // Service has no task of its own.
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            context.startActivity(intent)
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