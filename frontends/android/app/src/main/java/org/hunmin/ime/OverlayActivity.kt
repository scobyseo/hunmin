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
import android.widget.Toast
import androidx.appcompat.app.AlertDialog

class OverlayActivity : Activity() {

    private var dialog: AlertDialog? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Transparent, no title, no history
        setTheme(android.R.style.Theme_Translucent_NoTitleBar)
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
            "$text  (${details[idx]})"
        }.toTypedArray()

        dialog = AlertDialog.Builder(this)
            .setTitle("한자 선택: $preedit")
            .setItems(items) { _, which ->
                val selected = candidates[which]
                setResult(RESULT_HANJA_SELECTED, Intent().putExtra(EXTRA_SELECTED_HANJA, selected))
                finish()
            }
            .setNegativeButton("취소") { _, _ -> finish() }
            .setOnCancelListener { finish() }
            .setOnDismissListener { finish() }
            .show()
    }

private fun showSettingsDialog() {
        val prefs = getSharedPreferences("hunmin_keys", MODE_PRIVATE)
        
        // Build settings list
        val currentBindings = mapOf(
            "한/영 전환" to prefs.getString("key_hangul_eng", "ALT_LEFT+Z"),
            "한자 입력" to prefs.getString("key_hanja", "ALT_LEFT+X"),
            "음성 입력" to prefs.getString("key_voice", "ALT_LEFT+V"),
            "자동 한/영 토글" to prefs.getString("key_auto_toggle", "ALT_RIGHT+ENTER"),
        )
        
        val items = currentBindings.map { "${it.key}: ${it.value}" }.toTypedArray()
        
        // Use default AlertDialog - no custom theme to avoid Resource ID #0x0 crash
        dialog = AlertDialog.Builder(this)
            .setTitle("키 바인딩 설정")
            .setItems(items) { _, which ->
                val action = currentBindings.keys.elementAt(which)
                showKeyCaptureDialog(action) { keyCombo ->
                    val prefs = getSharedPreferences("hunmin_keys", MODE_PRIVATE)
                    prefs.edit().putString("key_$action", keyCombo).apply()
                    Toast.makeText(this, "$action: $keyCombo 로 설정됨", Toast.LENGTH_SHORT).show()
                }
            }
            .setNegativeButton("닫기") { _, _ -> finish() }
            .setOnCancelListener { finish() }
            .show()
    }

    private fun showKeyCaptureDialog(actionName: String, onComplete: (String) -> Unit) {
        val dialogView = android.widget.TextView(this).apply {
            text = "$actionName: 키를 누르세요..."
            textSize = 18f
            gravity = android.view.Gravity.CENTER
            setPadding(48, 48, 48, 48)
        }

        val dialog = AlertDialog.Builder(this)
            .setTitle("$actionName 키 설정")
            .setView(dialogView)
            .setCancelable(true)
            .setOnCancelListener { onComplete("취소됨"); finish() }
            .show()

        dialogView.setOnKeyListener { _, keyCode, event ->
            if (event.action == KeyEvent.ACTION_DOWN) {
                val mods = event.metaState
                val parts = mutableListOf<String>()
                if ((mods and KeyEvent.META_SHIFT_ON) != 0) parts.add("SHIFT")
                if ((mods and KeyEvent.META_ALT_LEFT_ON) != 0) parts.add("ALT_LEFT")
                if ((mods and KeyEvent.META_ALT_RIGHT_ON) != 0) parts.add("ALT_RIGHT")
                if ((mods and KeyEvent.META_CTRL_ON) != 0) parts.add("CTRL")
                if ((mods and KeyEvent.META_META_ON) != 0) parts.add("META")

                val keyName = KeyEvent.keyCodeToString(keyCode)
                val combo = (parts + listOf(keyName)).joinToString("+")
                dialogView.text = "설정됨: $combo"
                
                // Dismiss after a short delay
                android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                    dialog.dismiss()
                    onComplete(combo)
                    finish()
                }, 500)
                true
            } else {
                false
            }
        }
    }

    private fun showToast() {
        val text = intent?.getStringExtra(EXTRA_TOAST_TEXT) ?: return finish()
        Toast.makeText(this, text, Toast.LENGTH_SHORT).show()
        finish()
    }

    companion object {
        private const val TAG = "OverlayActivity"
        const val ACTION_SHOW_HANJA = "org.hunmin.ime.action.SHOW_HANJA"
        const val ACTION_SHOW_SETTINGS = "org.hunmin.ime.action.SHOW_SETTINGS"
        const val ACTION_SHOW_TOAST = "org.hunmin.ime.action.SHOW_TOAST"

        const val EXTRA_HANJA_PREEDIT = "preedit"
        const val EXTRA_HANJA_CANDIDATES = "candidates"
        const val EXTRA_HANJA_DETAILS = "details"
        const val EXTRA_TOAST_TEXT = "text"
        const val EXTRA_SETTINGS_ACTION = "action"
        const val EXTRA_SETTING_KEY = "key"
        const val EXTRA_SETTING_VALUE = "value"

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