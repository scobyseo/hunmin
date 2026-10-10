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
import android.view.Gravity
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AlertDialog

class OverlayActivity : Activity() {

    companion object {
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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Transparent, no title, no history
        setTheme(android.R.style.Theme_Translucent_NoTitleBar)
        // Finish immediately after showing UI
        isFinishing
    }

    override fun onResume() {
        super.onResume()
        val action = intent?.action ?: return finish()

        when (action) {
            ACTION_SHOW_HANJA -> showHanjaDialog()
            ACTION_SHOW_SETTINGS -> showSettingsDialog()
            ACTION_SHOW_TOAST -> showToast()
            else -> finish()
        }
    }

    private fun showHanjaDialog() {
        val preedit = intent?.getStringExtra(EXTRA_HANJA_PREEDIT) ?: return finish()
        val candidates = intent?.getStringArrayExtra(EXTRA_HANJA_CANDIDATES) ?: return finish()
        val details = intent?.getStringArrayExtra(EXTRA_HANJA_DETAILS) ?: return finish()

        val items = candidates.mapIndexed { idx, text ->
            "$text  (${details[idx]})"
        }.toTypedArray()

        AlertDialog.Builder(this)
            .setTitle("한자 선택: $preedit")
            .setItems(items) { _, which ->
                val selected = candidates[which]
                setResult(RESULT_HANJA_SELECTED, Intent().putExtra(EXTRA_SELECTED_HANJA, selected))
                finish()
            }
            .setNegativeButton("취소") { _, _ -> finish() }
            .setOnCancelListener { finish() }
            .show()
    }

    private fun showSettingsDialog() {
        // For now, just show a toast and finish
        // Full settings UI would need more elaborate UI
        Toast.makeText(this, "설정: 한/영 전환=Alt_L+Z, 한자=Alt_L+X, 음성=Alt+V", Toast.LENGTH_LONG).show()
        finish()
    }

    private fun showToast() {
        val text = intent?.getStringExtra(EXTRA_TOAST_TEXT) ?: return finish()
        Toast.makeText(this, text, Toast.LENGTH_SHORT).show()
        finish()
    }
}