/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * LayoutPicker — the IME's input view: an indicator and four buttons.
 *
 * WHY IT IS NOT A LAYOUT PICKER ANY MORE
 * -------------------------------------
 * The class kept its name; the job changed. Choosing a layout moved to the
 * settings window (issue #12), because the Spinner that did it was the reason
 * the strip overflowed: seven WRAP_CONTENT children measured 1140 px on a
 * 1080 px screen, so the Hanja and settings buttons were laid out past the edge
 * and were absent from the accessibility tree entirely — the settings window
 * could not be reached at all.
 *
 * What is left is a toolbar, and it says so: [ToolbarStyle] is a user setting
 * with three shapes, because the maintainer's instruction was "설정에 A, B, C
 * 중 선택할 수 있게 만들어도 좋아".
 *
 * THE FIVE THINGS, IN EVERY STYLE
 * ------------------------------
 *   the layout in use  ·  한/영 mode  ·  Hanja  ·  voice  ·  settings
 *
 * Only the LAYOUT NAME may be truncated, per the same instruction. That is the
 * reason it is the one child with weight: everything else keeps its 48 dp touch
 * target on any screen, and the one elastic thing is the text the user already
 * knows they can go and read in full in the settings window.
 *
 * HOW IT FITS ANY SCREEN
 * ----------------------
 * Two mechanisms, because one is not enough. The gaps between buttons step
 * down (20 / 12 / 6 dp) as the screen narrows, which handles every ordinary
 * width. A HorizontalScrollView wraps the row anyway, which handles the width
 * where even the smallest gaps overflow: a scroll view that does not need to
 * scroll costs nothing, and a button the user cannot reach costs the feature.
 */
package org.hunmin.ime

import android.content.Context
import android.util.AttributeSet
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.view.ViewCompat

class LayoutPicker @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : FrameLayout(context, attrs) {

    private var style: ToolbarStyle = ToolbarStyle.COMPACT

    /** Every layout this build can run, for the indicator's label lookup. */
    private var layouts: List<LayoutEntry> = emptyList()
    private var activeLayoutId: String? = null

    private var layoutIndicator: TextView? = null

    /** Style B shows an abbreviation; A and C show the full name, cut off. */
    private var layoutAbbreviated = false
    private var modeView: TextView? = null
    private var micButton: ImageButton? = null
    private var hanjaButton: ImageButton? = null
    private var settingsButton: ImageButton? = null

    /** Called when the user taps the mic. Null disables the button. */
    var onMicClicked: (() -> Unit)? = null

    /** Called when the user taps the Hanja button. Null disables the button. */
    var onHanjaClicked: (() -> Unit)? = null

    /** Called when the user taps the mode indicator (한/영). Null disables click. */
    var onModeClicked: (() -> Unit)? = null

    /**
     * Called when the settings button is tapped. Null disables the button.
     *
     * Separate from [onLayoutClicked] because they open different sections, and
     * one of them being wired without the other is exactly how this button came
     * to do nothing at all on the first run of the new strip.
     */
    var onSettingsClicked: (() -> Unit)? = null
        set(value) {
            field = value
            settingsButton?.setOnClickListener { value?.invoke() }
        }

    /**
     * Called when the layout indicator is tapped.
     *
     * It opens the settings window at the layout section — it does not choose a
     * layout. Keeping the choice in one place is the whole point of the move:
     * a picker that both shows and sets is a second source of truth for which
     * layout is active.
     */
    var onLayoutClicked: (() -> Unit)? = null

    /** Visual state of the mic button. */
    enum class MicState { IDLE, LISTENING, ERROR }

    private var micState: MicState = MicState.IDLE

    init {
        installInsetsListener()
        rebuild()
    }

    // ------------------------------------------------------------- public API

    /** Re-render in [style]. Same style is a no-op, so a save on every start is cheap. */
    fun applyStyle(style: ToolbarStyle) {
        if (style == this.style) return
        this.style = style
        rebuild()
    }

    /** The engine's layout list; the indicator resolves its label from this. */
    fun setLayouts(layouts: List<LayoutEntry>) {
        this.layouts = layouts
        refreshLayoutIndicator()
    }

    /**
     * Which layout is in use.
     *
     * A layout that is not in [setLayouts]' list shows its id rather than
     * nothing: a blank indicator reads as "no layout", and the id is enough for
     * the user to go looking with.
     */
    fun setActiveLayout(id: String) {
        activeLayoutId = id
        refreshLayoutIndicator()
    }

    /** Update mode indicator (한/영). */
    fun setMode(isHangul: Boolean) {
        modeIsHangul = isHangul
        modeView?.text = if (isHangul) "한" else "영"
    }

    /** Update mic button appearance for current recognition state. */
    fun setMicState(state: MicState) {
        micState = state
        val button = micButton ?: return
        val color = when (state) {
            MicState.IDLE -> 0
            MicState.LISTENING -> android.graphics.Color.GREEN
            MicState.ERROR -> android.graphics.Color.GRAY
        }
        if (color == 0) {
            button.clearColorFilter()
        } else {
            button.setColorFilter(color, android.graphics.PorterDuff.Mode.SRC_IN)
        }
        button.contentDescription = context.getString(
            when (state) {
                MicState.IDLE -> R.string.voice_content_description
                MicState.LISTENING -> R.string.voice_listening
                MicState.ERROR -> R.string.voice_error
            }
        )
    }

    // ------------------------------------------------------------- composition

    private fun rebuild() {
        removeAllViews()
        // MATCH_PARENT on the width, explicitly. The root is a FrameLayout, whose
        // generateDefaultLayoutParams() is WRAP_CONTENT — and a wrap-content
        // scroll view measures to nothing useful and clips its own row away, so
        // the strip came up with the layout indicator and nothing else. Measured:
        // the buttons were absent from the accessibility tree entirely, which
        // is the same failure as the gear button being off-screen.
        val fill = FrameLayout.LayoutParams(MATCH_PARENT, WRAP)
        when (style) {
            ToolbarStyle.COMPACT -> addView(scrollingRow(iconOnlyLayout = false), fill)
            ToolbarStyle.ICON_ROW -> addView(scrollingRow(iconOnlyLayout = true), fill)
            ToolbarStyle.TWO_ROW -> addView(twoRows(), fill)
        }
        // Re-apply the state the rebuild's fresh views do not know about.
        setMicState(micState)
        refreshLayoutIndicator()
    }

    /**
     * One row, wrapped in a scroll view that normally never scrolls.
     *
     * The wrapper is unconditional on purpose: it is the answer to the width
     * where the gaps are already at their smallest and the buttons still do not
     * fit, and a view that does not need to scroll is invisible to the user.
     */
    private fun scrollingRow(iconOnlyLayout: Boolean): View {
        val scroll = HorizontalScrollView(context).apply {
            isHorizontalScrollBarEnabled = false
            isFillViewport = true
        }
        val row = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        val gap = gapDp()
        // Only the indicator is elastic: width 0 + weight 1 means "take the
        // leftover". Every other child is WRAP_CONTENT -- width 0 with weight 0
        // is zero pixels wide, which is how the three buttons vanished from the
        // strip on the first run of this.
        row.addView(layoutIndicator(iconOnlyLayout), lp(0, weight = 1f, padEnd = gap))
        row.addView(modePill(), lp(WRAP, padEnd = gap))
        row.addView(hanjaButton(), lp(WRAP, padEnd = gap))
        row.addView(micButton(), lp(WRAP, padEnd = gap))
        row.addView(settingsButton(), lp(WRAP))
        scroll.addView(row)
        return scroll
    }

    /** Status above, evenly spread buttons below. Costs about twice the height. */
    private fun twoRows(): View {
        val column = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL }
        val status = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        val gap = gapDp()
        status.addView(layoutIndicator(iconOnlyLayout = false), lp(0, weight = 1f, padEnd = gap))
        status.addView(modePill(), lp(0))
        column.addView(status, lp(0))

        // weight = 1 on each button, so they spread evenly and stay 48 dp even
        // on a narrow screen: two rows means the horizontal budget is twice as
        // generous, which is the point of style C.
        val buttons = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        buttons.addView(hanjaButton(), lp(0, weight = 1f, padEnd = gap))
        buttons.addView(micButton(), lp(0, weight = 1f, padEnd = gap))
        buttons.addView(settingsButton(), lp(0, weight = 1f))
        column.addView(buttons, lp(0))
        return column
    }

    private fun layoutIndicator(iconOnlyLayout: Boolean): TextView =
        (layoutIndicator ?: TextView(context).also { layoutIndicator = it }).apply {
            textSize = 14f
            gravity = Gravity.CENTER_VERTICAL
            // One line, ellipsized: the maintainer's rule — the name may be cut,
            // but which layout is in use must stay legible.
            maxLines = 1
            ellipsize = android.text.TextUtils.TruncateAt.END
            layoutParams = lp(0, weight = 1f)
            setOnClickListener { onLayoutClicked?.invoke() }
            isClickable = onLayoutClicked != null
            contentDescription = context.getString(R.string.picker_content_description)
            layoutAbbreviated = iconOnlyLayout
        }

    /** The 한/영 toggle: a pill with the mode letter, tappable. */
    private fun modePill(): View {
        val label = TextView(context).apply {
            textSize = 14f
            gravity = Gravity.CENTER
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            setBackgroundColor(0xFFFFFFFF.toInt())
            setTextColor(0xFF000000.toInt())
            setPadding(PAD_DP, PILL_PAD_DP, PAD_DP, PILL_PAD_DP)
            contentDescription = context.getString(R.string.mode_switch_content_description)
            setOnClickListener { onModeClicked?.invoke() }
        }
        modeView = label
        setMode(modeIsHangul)
        return FrameLayout(context).apply {
            minimumWidth = TOUCH_DP
            minimumHeight = TOUCH_DP
            addView(label, FrameLayout.LayoutParams(WRAP, WRAP, Gravity.CENTER))
        }
    }

    /** Survives a style rebuild, which recreates the pill. */
    private var modeIsHangul = true

    private fun hanjaButton(): ImageButton =
        (hanjaButton ?: ImageButton(context).also { hanjaButton = it }).apply {
            setImageResource(android.R.drawable.ic_menu_add)
            contentDescription = context.getString(R.string.hanja_content_description)
            setBackgroundColor(0x00000000)
            setOnClickListener { onHanjaClicked?.invoke() }
            setPadding(BTN_PADDING_DP, BTN_PADDING_DP, BTN_PADDING_DP, BTN_PADDING_DP)
            minimumWidth = TOUCH_DP
            minimumHeight = TOUCH_DP
            layoutParams = lp(WRAP)
        }

    private fun micButton(): ImageButton =
        (micButton ?: ImageButton(context).also { micButton = it }).apply {
            setImageResource(android.R.drawable.ic_btn_speak_now)
            contentDescription = context.getString(R.string.voice_content_description)
            setBackgroundColor(0x00000000)
            setOnClickListener { onMicClicked?.invoke() }
            setPadding(BTN_PADDING_DP, BTN_PADDING_DP, BTN_PADDING_DP, BTN_PADDING_DP)
            minimumWidth = TOUCH_DP
            minimumHeight = TOUCH_DP
            layoutParams = lp(WRAP)
        }

    private fun settingsButton(): ImageButton =
        (settingsButton ?: ImageButton(context).also { settingsButton = it }).apply {
            setImageResource(android.R.drawable.ic_menu_preferences)
            contentDescription = context.getString(R.string.settings_content_description)
            setBackgroundColor(0x00000000)
            setOnClickListener { onSettingsClicked?.invoke() }
            setPadding(BTN_PADDING_DP, BTN_PADDING_DP, BTN_PADDING_DP, BTN_PADDING_DP)
            minimumWidth = TOUCH_DP
            minimumHeight = TOUCH_DP
            layoutParams = lp(WRAP)
        }

    // ----------------------------------------------------------------- helpers

    private fun refreshLayoutIndicator() {
        val view = layoutIndicator ?: return
        val entry = layouts.firstOrNull { it.id == activeLayoutId }
        view.text = when {
            entry != null -> if (layoutAbbreviated) entry.shortLabel() else entry.label
            activeLayoutId != null -> activeLayoutId
            else -> "—"
        }
        // Spoken and long-press text is the FULL name even when the visible one
        // is shortened or cut off: a truncated label is a display decision, not a
        // loss of information.
        view.contentDescription = entry?.let { context.getString(R.string.picker_content_description) + ": " + it.label }
            ?: context.getString(R.string.picker_content_description)
    }

    /** 20 dp on a tablet-width window, 12 on a phone, 6 when it is tight. */
    private fun gapDp(): Int {
        val widthDp = resources.displayMetrics.widthPixels / resources.displayMetrics.density
        return when {
            widthDp >= 600 -> 20
            widthDp >= 400 -> 12
            else -> 6
        }
    }

    private fun lp(width: Int, weight: Float = 0f, padEnd: Int = 0): LinearLayout.LayoutParams =
        LinearLayout.LayoutParams(width, LinearLayout.LayoutParams.WRAP_CONTENT, weight).apply {
            if (padEnd > 0) {
                marginEnd = dp(padEnd)
                // Pre-API-26 name, still honoured on old devices.
                @Suppress("DEPRECATION")
                (this as? android.view.ViewGroup.MarginLayoutParams)?.rightMargin = dp(padEnd)
            }
        }

    private fun dp(value: Int): Int = TypedValue.applyDimension(
        TypedValue.COMPLEX_UNIT_DIP, value.toFloat(), resources.displayMetrics,
    ).toInt()

    private fun installInsetsListener() {
        // A hardware-key IME has no keys to draw, but the strip still has to be
        // dismissible or it would occupy screen the user cannot get back. The
        // bottom inset keeps it above the navigation bar (gesture or 3-button).
        ViewCompat.setOnApplyWindowInsetsListener(this) { view, insets ->
            val navInsets = insets.getInsets(2)   // TYPE_NAVIGATION_BARS
            val sysInsets = insets.getInsets(3)   // TYPE_SYSTEM_BARS
            val bottom = maxOf(navInsets.bottom, sysInsets.bottom)
            if (bottom > 0) {
                view.setPadding(view.paddingLeft, view.paddingTop, view.paddingRight, bottom)
            }
            insets
        }
    }

    private companion object {
        const val WRAP = LinearLayout.LayoutParams.WRAP_CONTENT
        const val MATCH_PARENT = FrameLayout.LayoutParams.MATCH_PARENT
        const val PAD_DP = 8
        const val PILL_PAD_DP = 6
        const val BTN_PADDING_DP = 12
        const val TOUCH_DP = 48
    }
}

/** Style B's abbreviation: the id's first token, uppercased, cut to 6 chars. */
private fun LayoutEntry.shortLabel(): String {
    val head = id.substringBefore('-').uppercase()
    return if (head.length <= 6) head else head.take(6)
}

/** Set by [LayoutPicker.layoutIndicator]; read by [LayoutPicker.refreshLayoutIndicator]. */
private var TextView.layoutIndicatorText: Boolean
    get() = tag == STYLE_B_TAG
    set(value) {
        tag = if (value) STYLE_B_TAG else null
    }

private const val STYLE_B_TAG = "hunmin:toolbar-style-b"