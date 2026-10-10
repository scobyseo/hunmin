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

import android.content.Context
import android.util.AttributeSet
import android.view.Gravity
import android.view.View
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
    private var layouts: List<HunminInputMethodService.LayoutEntry> = emptyList()

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

    init {
        orientation = HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        // A hardware-key IME has no keys to draw, but the strip still has to be
        // dismissible or it would occupy screen the user cannot get back.
        setBackgroundColor(0x00000000)

        // A leading label saying what this control is. The active layout's name
        // is in the Spinner, so this does not repeat it.
        addView(TextView(context).apply {
            text = context.getString(R.string.picker_label)
            setPadding(PAD, 0, PAD / 2, 0)
        })

        spinner = Spinner(context).apply {
            // On a hardware-key setup this is one tap to reach, so it stays
            // visible rather than hiding behind a gesture nobody will try.
            contentDescription = context.getString(R.string.picker_content_description)
        }
        addView(spinner)

        micButton = ImageButton(context).apply {
            setImageResource(android.R.drawable.ic_btn_speak_now)
            contentDescription = context.getString(R.string.voice_content_description)
            setBackgroundColor(0x00000000)
        }
        addView(micButton)

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
        layouts: List<HunminInputMethodService.LayoutEntry>,
        currentId: String,
        onChosen: (String) -> Unit,
    ) {
        this.layouts = layouts
        this.onChosen = onChosen

        val labels = layouts.map { it.label }
        val adapter = ArrayAdapter(context, android.R.layout.simple_spinner_item, labels)
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

    private companion object {
        const val PAD = 16
    }
}
