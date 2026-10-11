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
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.util.TypedValue
import android.util.Log
import android.view.KeyEvent
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
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

    /**
     * High-contrast, compact color palette for HUD-style inspector UI.
     * Deep dark slate base with crisp cyan accents for hardware-keyboard environments.
     */
    private object CompactTheme {
        val BG_PANEL = Color.parseColor("#1E1E24")         // Deep dark slate
        val TEXT_PRIMARY = Color.parseColor("#F2F2F7")     // High contrast white
        val TEXT_SECONDARY = Color.parseColor("#8E8E93")   // Neutral grey
        val KEY_BG = Color.parseColor("#2C2C2E")           // Keycap fill
        val KEY_BORDER = Color.parseColor("#48484A")       // Crisp keycap stroke
        val ACCENT = Color.parseColor("#64D2FF")           // Crisp electric cyan/blue
        val ACCENT_BG = Color.parseColor("#152D3D")
        val DIVIDER = Color.parseColor("#2C2C2E")
    }

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
     * Compact, high-density settings list (takes ~40% less vertical height)
     * High-contrast, hardware-keyboard-oriented inspector style.
     */
    private fun showSettingsDialog() {
        val entries = ImeSettings.layoutsProvider()
        val connected = ImeSettings.connected

        fun labelOf(mode: LayoutMode, id: String?): String =
            entries?.firstOrNull { it.id == id && it.mode == mode }?.label ?: "—"

        data class SettingItem(val title: String, val badge: String, val onClick: () -> Unit)

        val rows = listOf(
            SettingItem("한글 배열", labelOf(LayoutMode.HANGUL, ImeSettings.layoutChoice(LayoutMode.HANGUL))) {
                showLayoutSection(LayoutMode.HANGUL)
            },
            SettingItem("영문 배열", labelOf(LayoutMode.LATIN, ImeSettings.layoutChoice(LayoutMode.LATIN))) {
                showLayoutSection(LayoutMode.LATIN)
            },
            SettingItem("창 스타일", ImeSettings.toolbarStyle().label) {
                showStyleSection()
            },
            SettingItem("키 바인딩", "단축키 관리 ➔") {
                showKeyBindingSection()
            }
        )

        val container = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            // setView() adopts the view's own LayoutParams, defaulting to
            // WRAP_CONTENT; then the alert's custom-panel chain wraps to that
            // and the window resize lands on the *window* only, leaving the
            // rows in a narrow centred column. Match-parent propagates the
            // widened window into the rows.
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
            )
            setPadding(dp(14), dp(10), dp(14), dp(10))
            background = GradientDrawable().apply {
                setColor(CompactTheme.BG_PANEL)
                cornerRadius = dp(10).toFloat()
            }
        }

        // Compact Header (single line, minimal height)
        container.addView(TextView(this).apply {
            text = "hunmin 설정"
            textSize = 14f
            setTypeface(Typeface.DEFAULT_BOLD)
            setTextColor(CompactTheme.TEXT_PRIMARY)
            setPadding(dp(6), dp(4), dp(6), dp(8))
        })

        rows.forEachIndexed { index, item ->
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                minimumHeight = dp(38) // Reduced from 52dp to 38dp
                setPadding(dp(8), dp(4), dp(8), dp(4))
                isClickable = true
                val outValue = TypedValue()
                theme.resolveAttribute(android.R.attr.selectableItemBackground, outValue, true)
                setBackgroundResource(outValue.resourceId)

                setOnClickListener {
                    dialog?.dismiss()
                    item.onClick()
                }
            }

            val tvTitle = TextView(this).apply {
                text = item.title
                textSize = 15f
                setTypeface(Typeface.DEFAULT_BOLD)
                setTextColor(CompactTheme.TEXT_PRIMARY)
                maxLines = 1
                // The value is the variable-width half (a layout name can be long,
                // a title never is), so the title takes its exact width and the
                // badge absorbs the remainder and ellipsizes. Weighting the title
                // instead let a long badge starve it to one glyph per line.
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                ).apply { marginEnd = dp(8) }
            }

            val tvBadge = TextView(this).apply {
                text = item.badge
                textSize = 13f
                setTypeface(Typeface.MONOSPACE, Typeface.BOLD)
                setTextColor(CompactTheme.ACCENT)
                maxLines = 1
                ellipsize = android.text.TextUtils.TruncateAt.END
                gravity = Gravity.END
                // Weight 1 with wrap width: takes the leftover space, and shrinks
                // (ellipsizing) rather than pushing the title off when a layout
                // name is longer than the row.
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    1f,
                )
                setPadding(dp(8), dp(4), dp(8), dp(4))
                background = GradientDrawable().apply {
                    setColor(CompactTheme.ACCENT_BG)
                    setStroke(dp(1), CompactTheme.ACCENT)
                    cornerRadius = dp(6).toFloat()
                }
            }

            row.addView(tvTitle)
            row.addView(tvBadge)
            container.addView(row)

            if (index < rows.lastIndex) {
                container.addView(View(this).apply {
                    setBackgroundColor(CompactTheme.DIVIDER)
                    layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(1)).apply {
                        setMargins(dp(6), 0, dp(6), 0)
                    }
                })
            }
        }

        dialog?.dismiss()
        dialog = AlertDialog.Builder(this)
            .setView(container)
            .setNegativeButton("닫기") { _, _ -> finish() }
            .setOnCancelListener { finish() }
            .setOnDismissListener { if (!isChangingConfigurations) dialog = null }
            .show()
        // The wrap_content card came out ~1/3 of the screen width on the AVD, which
        // ellipsized every badge to two glyphs. A settings list wants the width it
        // can have; the rows distribute it themselves.
        dialog?.window?.setLayout(
            (resources.displayMetrics.widthPixels * 0.92).toInt(),
            ViewGroup.LayoutParams.WRAP_CONTENT,
        )

        if (!connected) {
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
     * Compact Keybinding List with crisp physical keycap aesthetics
     */
    private fun showKeyBindingSection() {
        val store = KeyBindings.from(this)
        val actions = KeyAction.values().toList()

        val container = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            // Same as the settings container: setView adopts these params, and
            // without match-parent the widened window never reaches the rows.
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
            )
            setPadding(dp(14), dp(10), dp(14), dp(10))
            background = GradientDrawable().apply {
                setColor(CompactTheme.BG_PANEL)
                cornerRadius = dp(10).toFloat()
            }
        }

        container.addView(TextView(this).apply {
            text = "키 바인딩"
            textSize = 14f
            setTypeface(Typeface.DEFAULT_BOLD)
            setTextColor(CompactTheme.TEXT_PRIMARY)
            setPadding(dp(6), dp(4), dp(6), dp(8))
        })

        actions.forEachIndexed { index, action ->
            val combos = KeyBindings.combos(store, action)
            val chordText = combos.firstOrNull()?.let { describe(it) } ?: NOT_SET
            val isCustomized = chordText != NOT_SET

            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                minimumHeight = dp(38) // Reduced height
                setPadding(dp(8), dp(4), dp(8), dp(4))
                isClickable = true
                val outValue = TypedValue()
                theme.resolveAttribute(android.R.attr.selectableItemBackground, outValue, true)
                setBackgroundResource(outValue.resourceId)

                setOnClickListener {
                    dialog?.dismiss()
                    showKeyCaptureDialog(action)
                }
            }

            val tvTitle = TextView(this).apply {
                text = action.label
                textSize = 13f
                setTextColor(CompactTheme.TEXT_PRIMARY)
                maxLines = 1
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                ).apply { marginEnd = dp(8) }
            }

            // Keycap style box
            val tvBadge = TextView(this).apply {
                text = chordText
                textSize = 12f
                setTypeface(Typeface.MONOSPACE, Typeface.BOLD)
                setTextColor(if (isCustomized) CompactTheme.TEXT_PRIMARY else CompactTheme.TEXT_SECONDARY)
                maxLines = 1
                ellipsize = android.text.TextUtils.TruncateAt.END
                gravity = Gravity.END
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    1f,
                )
                setPadding(dp(8), dp(4), dp(8), dp(4))
                background = GradientDrawable().apply {
                    setColor(if (isCustomized) CompactTheme.ACCENT_BG else CompactTheme.KEY_BG)
                    setStroke(dp(1), if (isCustomized) CompactTheme.ACCENT else CompactTheme.KEY_BORDER)
                    cornerRadius = dp(6).toFloat()
                }
            }

            row.addView(tvTitle)
            row.addView(tvBadge)
            container.addView(row)

            if (index < actions.lastIndex) {
                container.addView(View(this).apply {
                    setBackgroundColor(CompactTheme.DIVIDER)
                    layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(1)).apply {
                        setMargins(dp(6), 0, dp(6), 0)
                    }
                })
            }
        }

        dialog?.dismiss()
        // No setTitle: the container already carries its own header, and a title
        // above it would print "키 바인딩" twice.
        dialog = AlertDialog.Builder(this)
            .setView(container)
            .setNegativeButton("뒤로") { _, _ -> showSettingsDialog() }
            .setOnCancelListener { showSettingsDialog() }
            .setOnDismissListener { if (!isChangingConfigurations) dialog = null }
            .show()
        dialog?.window?.setLayout(
            (resources.displayMetrics.widthPixels * 0.92).toInt(),
            ViewGroup.LayoutParams.WRAP_CONTENT,
        )
    }
    /**
     * The capture dialog, now with a live chord preview: modifiers pressed so
     * far render in the preview before the final key commits the binding.
     */
    private fun showKeyCaptureDialog(action: KeyAction) {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(dp(24), dp(28), dp(24), dp(28))
            isFocusable = true
            isFocusableInTouchMode = true
        }

        val tvSub = TextView(this).apply {
            text = "지정할 물리 키를 누르세요"
            textSize = 13f
            setTextColor(Color.parseColor("#79747E"))
        }

        val tvPreview = TextView(this).apply {
            text = "키 입력 대기 중..."
            textSize = 18f
            typeface = Typeface.MONOSPACE
            setTextColor(Color.parseColor("#6750A4"))
            gravity = Gravity.CENTER
            setPadding(0, dp(14), 0, dp(6))
        }

        val tvHint = TextView(this).apply {
            text = "수정자 키(Alt, Ctrl 등)와 조합할 수 있습니다\n(취소: Esc 또는 뒤로 가기)"
            textSize = 11f
            setTextColor(Color.parseColor("#9E9E9E"))
            gravity = Gravity.CENTER
        }

        root.addView(tvSub)
        root.addView(tvPreview)
        root.addView(tvHint)

        dialog?.dismiss()
        capturingAction = action
        capturingDone = false

        val captureDialog = AlertDialog.Builder(this)
            .setTitle("${action.label} 키 설정")
            .setView(root)
            .setCancelable(true)
            .setNegativeButton("취소") { _, _ ->
                capturingAction = null
                capturingDone = false
                KeyCaptureSession.clear()
                captureWindow?.dismiss()
                captureWindow = null
                dialog?.dismiss()
                showKeyBindingSection()
            }
            .show()

        // 수정자 키 단독 누름 시 실시간 피드백 갱신 콜백 연결
        KeyCaptureSession.install { act, keyCode, metaState ->
            updatePreviewUI(tvPreview, act, keyCode, metaState)
            handleCaptureKey(act, keyCode, metaState)
        }

        dialog = captureDialog
        captureWindow = captureDialog

        val decor = captureDialog.window?.decorView
        decor?.isFocusableInTouchMode = true
        decor?.setOnKeyListener { _, _, event ->
            updatePreviewUI(tvPreview, event.action, event.keyCode, event.metaState)
            handleCaptureKey(event)
        }
        decor?.requestFocus()
        root.requestFocus()

        captureDialog.setOnDismissListener {
            if (!isChangingConfigurations) dialog = null
        }

        root.setOnKeyListener { _, _, event ->
            updatePreviewUI(tvPreview, event.action, event.keyCode, event.metaState)
            handleCaptureKey(event)
        }
    }

    /** 키를 누르고 있는 도중 시각적으로 조합 상태를 보여주는 헬퍼 */
    private fun updatePreviewUI(tv: TextView, action: Int, keyCode: Int, metaState: Int) {
        if (action == KeyEvent.ACTION_DOWN) {
            val parts = mutableListOf<String>()
            if ((metaState and KeyEvent.META_CTRL_ON) != 0) parts.add("CTRL")
            if ((metaState and KeyEvent.META_ALT_LEFT_ON) != 0) parts.add("ALT_LEFT")
            if ((metaState and KeyEvent.META_ALT_RIGHT_ON) != 0) parts.add("ALT_RIGHT")
            if ((metaState and KeyEvent.META_SHIFT_ON) != 0) parts.add("SHIFT")

            if (KeyBinding.isModifierKey(keyCode)) {
                val modName = KeyEvent.keyCodeToString(keyCode).removePrefix("KEYCODE_")
                if (!parts.contains(modName)) parts.add(modName)
                tv.text = parts.joinToString("+") + " + ..."
                tv.setTextColor(Color.parseColor("#6750A4"))
            } else {
                val keyName = KeyEvent.keyCodeToString(keyCode).removePrefix("KEYCODE_")
                tv.text = (parts + listOf(keyName)).joinToString("+")
                tv.setTextColor(Color.parseColor("#2E7D32")) // 녹색 확정 강조
            }
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

    /** dp to px helper */
    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()

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