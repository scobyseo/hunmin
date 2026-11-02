/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * SettingsActivity — surface B of R13-정정, reachable from the SYSTEM IME
 * settings (res/xml/method.xml's settingsActivity).
 *
 * OverlayActivity is the in-keyboard settings window; this is the screen the
 * platform names, so a person who has enabled the IME but has no editor open
 * can still find it. Both read the same state: the engine's `autoswitch`
 * value, the chord the engine holds, and B3's data answer. Nothing here is a
 * second copy of any of the three.
 *
 * The running service is used when it can answer, so a change takes effect in
 * the live IME with no restart. When no layer is live (the common case when
 * the screen is opened from system settings) AutoSwitchSettings builds its own
 * layer from the same data and records the engine's read-back for the next
 * editor.
 */
package org.hunmin.ime

import android.os.Bundle
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SwitchCompat
import java.io.File

class SettingsActivity : AppCompatActivity() {

    private var offline: AutoSwitchSettings? = null
    private var dataDir: File? = null

    private var toggle: SwitchCompat? = null
    private var valueView: TextView? = null
    private var keyView: TextView? = null
    private var hintView: TextView? = null

    /**
     * Set while [render] moves the switch, so the listener does not mistake the
     * engine's answer being drawn for a person's press. The two must not feed
     * each other: a refused ON would otherwise re-request itself forever.
     */
    private var rendering = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val palette = HudTheme.of(this)
        setContentView(buildContent(palette))
        render()
    }

    override fun onResume() {
        super.onResume()
        // The service may have started (or an editor may have been focused)
        // while this screen sat behind the system settings list.
        render()
    }

    override fun onDestroy() {
        offline?.close()
        offline = null
        super.onDestroy()
    }

    private fun buildContent(palette: HudTheme.Colors): View {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(24), dp(20), dp(24), dp(24))
        }

        root.addView(TextView(this).apply {
            text = getString(R.string.settings_title)
            textSize = 22f
            setTypeface(Typeface.DEFAULT_BOLD)
            setTextColor(palette.textPrimary)
        })

        root.addView(View(this).apply {
            setBackgroundColor(palette.divider)
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                dp(1),
            ).apply { setMargins(0, dp(16), 0, dp(8)) }
        })

        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            minimumHeight = dp(48)
        }

        val label = TextView(this).apply {
            id = View.generateViewId()
            text = getString(R.string.autoswitch_title)
            textSize = 16f
            setTypeface(Typeface.DEFAULT_BOLD)
            setTextColor(palette.textPrimary)
            layoutParams = LinearLayout.LayoutParams(
                0,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                1f,
            )
        }
        row.addView(label)

        row.addView(SwitchCompat(this).apply {
            labelFor = label.id
            contentDescription = getString(R.string.autoswitch_description)
            setOnCheckedChangeListener { _, checked ->
                if (!rendering) write(checked)
            }
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
            )
        }.also { toggle = it })

        root.addView(row)

        root.addView(TextView(this).apply {
            textSize = 15f
            setTypeface(Typeface.DEFAULT_BOLD)
            setTextColor(palette.accent)
            setPadding(0, dp(2), 0, dp(2))
        }.also { valueView = it })

        root.addView(TextView(this).apply {
            textSize = 13f
            typeface = Typeface.MONOSPACE
            setTextColor(palette.textSecondary)
            setPadding(0, dp(2), 0, dp(8))
        }.also { keyView = it })

        root.addView(TextView(this).apply {
            textSize = 13f
            setTextColor(palette.textSecondary)
            setLineSpacing(dp(2).toFloat(), 1f)
        }.also { hintView = it })

        return ScrollView(this).apply {
            background = GradientDrawable().apply { setColor(palette.bgPanel) }
            addView(root)
        }
    }

    /**
     * Draw the engine's answer. Called on resume and after every write; never
     * remembers the value locally.
     */
    private fun render() {
        val state = state()
        rendering = true
        toggle?.isChecked = state.on == 1
        toggle?.isEnabled = state.usable
        rendering = false

        valueView?.text = when {
            state.on == null -> getString(R.string.autoswitch_unknown)
            !state.dataPresent ->
                getString(R.string.autoswitch_unavailable) +
                    " (" + getString(R.string.autoswitch_missing_data) + ")"
            state.on == 1 -> getString(R.string.autoswitch_on)
            else -> getString(R.string.autoswitch_off)
        }

        keyView?.text =
            if (state.key.isEmpty()) "" else getString(R.string.autoswitch_key, state.key)

        hintView?.text = when {
            state.on == null -> getString(R.string.autoswitch_no_engine)
            !state.dataPresent -> getString(R.string.autoswitch_missing_hint)
            else -> getString(R.string.autoswitch_key_hint, state.key)
        }
    }

    /**
     * Write through the live service when it can answer, and through a private
     * layer otherwise. Either way the value requested is not what is shown: the
     * engine's read-back is ([AutoSwitchSettings.write] and the service both
     * return that), so B3's refusal is visible on the screen immediately.
     */
    private fun write(on: Boolean) {
        if (liveState() != null) {
            val answer = ImeSettings.onAutoSwitchChosen(on)
            if (answer != null) {
                render()
                return
            }
        }
        offline()?.write(on)
        render()
    }

    /** The engine state read from the running service, or null when it cannot answer. */
    private fun liveState(): AutoSwitchSettings.State? {
        if (!ImeSettings.connected) return null
        val on = ImeSettings.autoSwitchValue() ?: return null
        val chord = ImeSettings.autoSwitchChord() ?: return null
        return AutoSwitchSettings.State(
            on = on,
            key = SpecialFunction.chordLabel(chord),
            dataPresent = ImeSettings.autoSwitchDataPresent(),
        )
    }

    private fun state(): AutoSwitchSettings.State =
        liveState() ?: offline()?.state() ?: AutoSwitchSettings.State(null, "", false)

    /** Build the offline layer only when the running service cannot answer. */
    private fun offline(): AutoSwitchSettings? {
        if (offline == null) {
            val dir = dataDir ?: HunminDataInstaller.install(this)
            dataDir = dir
            if (dir != null) {
                val prefs = getSharedPreferences(HunminPrefs.FILE, MODE_PRIVATE)
                offline = AutoSwitchSettings(
                    dataDir = dir,
                    readStored = { prefs.getBoolean(HunminPrefs.AUTOSWITCH, false) },
                    writeStored = { value ->
                        prefs.edit().putBoolean(HunminPrefs.AUTOSWITCH, value).apply()
                    },
                )
            }
        }
        return offline
    }

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()
}
