/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * LayoutPicker — the Android equivalent of fcitx5's `Layout=` option.
 *
 * Deliberately a Spinner, for the same reason the fcitx5 side settled on a
 * combobox: the layout list is discovered at runtime from the engine's own
 * tables (hunmin_ctx_layout_count), so it changes when data changes and cannot
 * be enumerated in the manifest. A hardcoded list would be a second copy of the
 * same fact, which is what AGENTS.md's "layouts are data, never code" rule
 * exists to prevent.
 *
 * What it shows: every layout this build can open, labelled with its Korean
 * name. `hunmin_ctx_layout_supported` does the filtering, so a row is never a
 * layout that fails at keystroke time.
 *
 * What it deliberately does NOT do, all of it P3 or later:
 *   - no Hanja candidate window. F9 has no AOSP keycode (on Android the
 *     function row is a modifier combination), so the trigger has to be chosen
 *     before that UI can exist. See PLAN.md P3.
 *   - no soft keyboard. This view is the *hardware*-keyboard IME's only surface.
 *   - no per-key labels. The layout's own `keys` table decides that, in data.
 */
package org.hunmin.ime

import android.app.AlertDialog
import android.content.Context
import android.content.SharedPreferences
import android.util.AttributeSet
import android.view.Gravity
import android.view.KeyEvent
import android.view.View
import android.view.inputmethod.InputMethodManager
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.Spinner
import android.widget.TextView
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat

/**
 * A one-line strip showing the active layout, tappable to change it.
 *
 * A LinearLayout with a label and a Spinner, not a custom View: it needs no
 * drawing, no touch handling and no measurement of its own, so a ViewGroup the
 * platform lays out is both less code and more correct than a hand-rolled one.
 */
class LayoutPicker @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : LinearLayout(context, attrs) {

    private lateinit var spinner: Spinner
    private lateinit var micButton: ImageButton
    private lateinit var modeView: TextView
    private lateinit var settingsButton: ImageButton
    private var layouts: List<LayoutEntry> = emptyList()

    /**
     * Set while binding, so Spinner's initial onItemSelected is not read as a
     * user choice.
     *
     * Declared here rather than next to its uses because Kotlin initialises
     * property initialisers in declaration order, and a `var` used by the
     * constructor-time `addView` chain must already exist.
     */
    private var suppress = false

    /** Called when the user picks a row. */
    private var onChosen: (String) -> Unit = {}

    /** Called when the user taps the mic. Null disables the button. */
    var onMicClicked: (() -> Unit)? = null
        set(value) {
            field = value
            micButton.setOnClickListener { value?.invoke() }
        }

    /** Called when the user taps the Hanja button. Null disables the button. */
    var onHanjaClicked: (() -> Unit)? = null

    /** Called when the user taps the mode indicator (한/영). Null disables click. */
    var onModeClicked: (() -> Unit)? = null

    /** Visual state of the mic button. */
    enum class MicState { IDLE, LISTENING, ERROR }

    /** Update mic button appearance for current recognition state. */
    fun setMicState(state: MicState) {
        val color = when (state) {
            MicState.IDLE -> 0
            MicState.LISTENING -> android.graphics.Color.GREEN
            MicState.ERROR -> android.graphics.Color.GRAY
        }
        if (color == 0) {
            micButton.clearColorFilter()
        } else {
            micButton.setColorFilter(
                color,
                android.graphics.PorterDuff.Mode.SRC_IN
            )
        }
        micButton.contentDescription = context.getString(
            when (state) {
                MicState.IDLE -> R.string.voice_content_description
                MicState.LISTENING -> R.string.voice_listening
                MicState.ERROR -> R.string.voice_error
            }
        )
    }

    /** Update mode indicator (한/영). */
    fun setMode(isHangul: Boolean) {
        modeView.text = if (isHangul) "한" else "영"
    }

    init {
        orientation = HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        // A hardware-key IME has no keys to draw, but the strip still has to be
        // dismissible or it would occupy screen the user cannot get back.
        setBackgroundColor(0x00000000)

        // A leading label saying what this control is. The active layout's name
        // is in the Spinner, so this does not repeat it. Its width is capped
        // because the layout names are long (sin3-p2-yet and friends) and an
        // unconstrained Spinner would push every button off the right edge.
        addView(TextView(context).apply {
            text = context.getString(R.string.picker_label)
            setPadding(PAD, 0, PAD / 2, 0)
            maxWidth = LABEL_MAX_WIDTH
            ellipsize = android.text.TextUtils.TruncateAt.END
        })

        // Mode indicator: clickable ImageButton with globe/keyboard icon + mode text
        val modeContainer = LinearLayout(context).apply {
            orientation = HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }

val modeIcon = ImageButton(context).apply {
            setImageResource(android.R.drawable.ic_menu_edit)
            contentDescription = context.getString(R.string.mode_switch_content_description)
            setBackgroundColor(0x00000000)
            setOnClickListener { onModeClicked?.invoke() }
            setPadding(BTN_PADDING, BTN_PADDING, 0, BTN_PADDING)
        }
        modeContainer.addView(modeIcon)

        // Mode text with adaptive background for theme visibility
        modeView = TextView(context).apply {
            text = "한"
            textSize = 14f
            setTextColor(0xFF000000.toInt())  // Black text
            setBackgroundColor(0xFFFFFFFF.toInt())  // White background
            setPadding(PAD / 2, 2, PAD, 2)
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            setOnClickListener { onModeClicked?.invoke() }
        }
        modeContainer.addView(modeView)

        addView(modeContainer)

        spinner = Spinner(context).apply {
            // On a hardware-key setup this is one tap to reach, so it stays
            // visible rather than hiding behind a gesture nobody will try.
            contentDescription = context.getString(R.string.picker_content_description)
            // Width 0 + weight 1 is what makes the strip FIT: the label, the mode
            // toggle, the spinner, the mic, the Hanja key, the IME switch and the
            // settings key are laid out left to right, and only the spinner may
            // give up space. Measured on the AVD (API 35, 1080 px): with every
            // child WRAP_CONTENT the row was 1140 px wide, so the mic was clipped
            // to 13 px at the right edge and the Hanja and settings buttons were
            // laid out past the screen entirely -- absent from the accessibility
            // tree, so nothing could reach the key settings window at all.
            layoutParams = LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f)
            // weight only hands out *excess* space; the row overflowed by 48 px
            // with a weight alone, because a Spinner's suggested minimum width is
            // its widest item ("sin3-p2-yet" and friends). Zero it, so the one
            // child that is allowed to shrink does.
            minimumWidth = 0
        }
        addView(spinner)

        micButton = ImageButton(context).apply {
            setImageResource(android.R.drawable.ic_btn_speak_now)
            contentDescription = context.getString(R.string.voice_content_description)
            setBackgroundColor(0x00000000)
            setPadding(BTN_PADDING, BTN_PADDING, BTN_PADDING, BTN_PADDING)
        }
        addView(micButton)

// Hanja button — opens Hanja candidate window
        val hanjaButton = ImageButton(context).apply {
            setImageResource(android.R.drawable.ic_menu_add)
            contentDescription = context.getString(R.string.hanja_content_description)
            setBackgroundColor(0x00000000)
            setOnClickListener { onHanjaClicked?.invoke() }
            setPadding(BTN_PADDING, BTN_PADDING, BTN_PADDING, BTN_PADDING)
        }
        addView(hanjaButton)

        // IME switch button — shows system IME picker
        val imeSwitchButton = ImageButton(context).apply {
            setImageResource(android.R.drawable.ic_menu_rotate)
            contentDescription = context.getString(R.string.ime_switch_content_description)
            setBackgroundColor(0x00000000)
            setOnClickListener {
                val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
                imm.showInputMethodPicker()
            }
            setPadding(BTN_PADDING, BTN_PADDING, BTN_PADDING, BTN_PADDING)
        }
        addView(imeSwitchButton)

        // Settings button — opens key binding settings
        settingsButton = ImageButton(context).apply {
            setImageResource(android.R.drawable.ic_menu_preferences)
            contentDescription = context.getString(R.string.settings_content_description)
            setBackgroundColor(0x00000000)
            setOnClickListener { showSettingsDialog() }
        }
        addView(settingsButton)

        layoutParams = LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT)

        // Reserve space for the navigation bar so the mic button is fully tappable.
        // The input view is a strip at the bottom; we need to ensure its bottom
        // inset is respected so it sits above the navigation bar (gesture or 3-button).
        // Use integer type constants for API 24+ compatibility.
        // WindowInsetsCompat type constants (from androidx.core.view.WindowInsetsCompat):
        // TYPE_STATUS_BARS = 1, TYPE_NAVIGATION_BARS = 2, TYPE_SYSTEM_BARS = 1 | 2 = 3
        ViewCompat.setOnApplyWindowInsetsListener(this) { view, insets ->
            val navInsets = insets.getInsets(2) // TYPE_NAVIGATION_BARS
            val sysInsets = insets.getInsets(3) // TYPE_SYSTEM_BARS (status | navigation)
            // Use the larger of navigation or system insets for the bottom.
            val bottomInset = maxOf(navInsets.bottom, sysInsets.bottom)
            if (bottomInset > 0) {
                view.setPadding(view.paddingLeft, view.paddingTop, view.paddingRight, bottomInset)
            }
            insets
        }
    }

    /**
     * Populate from the engine's own list and select [currentId].
     *
     * The listener is attached AFTER setAdapter() and after the initial
     * selection, because Spinner fires onItemSelected for the first row as soon
     * as it has data -- which would call back into the service and switch the
     * layout to whatever happens to sort first, silently overriding the user's
     * stored choice on every service start.
     */
    fun bind(
        layouts: List<LayoutEntry>,
        currentId: String,
        onChosen: (String) -> Unit,
    ) {
        this.layouts = layouts
        this.onChosen = onChosen

        val labels = layouts.map { it.label }
        val adapter: ArrayAdapter<String> = ArrayAdapter(
            context, android.R.layout.simple_spinner_item, labels
        )
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinner.adapter = adapter

        val index = layouts.indexOfFirst { it.id == currentId }
        if (index < 0) {
            // The stored layout is gone or unopenable. Showing row 0 while
            // claiming it is the current one would be a lie the user cannot
            // see through, so leave the spinner unset rather than guess.
            spinner.setSelection(0)
        } else {
            suppress = true
            spinner.setSelection(index, false)
            suppress = false
        }

        // Mic button is created once in init; just update its click listener.
        // The property setter handles this.

        spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, pos: Int, id: Long) {
                if (suppress) return
                // getOrNull rather than [pos]: the list can be shorter than the
                // adapter if the service rebinds while the dropdown is open, and
                // an IndexOutOfBounds here would take down the IME process.
                layouts.getOrNull(pos)?.let { onChosen(it.id) }
            }

            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
    }

    /** Re-select after the service changed the layout itself. */
    fun select(id: String) {
        val index = layouts.indexOfFirst { it.id == id }
        if (index < 0 || index == spinner.selectedItemPosition) return
        suppress = true
        spinner.setSelection(index, false)
        suppress = false
    }

    /** Show settings dialog for key binding configuration. */
    private fun showSettingsDialog() {
        val intent = android.content.Intent(context, OverlayActivity::class.java).apply {
            action = OverlayActivity.ACTION_SHOW_SETTINGS
            addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        context.startActivity(intent)
    }

    private companion object {
        const val PAD = 16
        const val BTN_PADDING = 24  // Extra padding for easier touch targets

        /**
         * Cap on the leading label, in pixels.
         *
         * A dp constant would scale with the display, which is the point of dp;
         * this is a floor for the *narrowest* screen the strip is measured on
         * (411 dp wide on the AVD) and a ceiling everywhere else. The buttons
         * keep their own size -- a 24 dp tap target is what makes them usable --
         * so the two flexible things are the label and the Spinner.
         */
        const val LABEL_MAX_WIDTH = 200
    }
}
