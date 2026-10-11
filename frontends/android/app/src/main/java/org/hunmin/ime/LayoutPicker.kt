/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * LayoutPicker — the IME's input view. Three shapes of one toolbar.
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
 * with two shapes, because the maintainer's instruction was "설정에 A, B, C
 * 중 선택할 수 있게 만들어도 좋아".
 *
 *   ONE_LINE (한줄 모드) the floating dock — DockStyle.kt
 *   TWO_LINE (두줄 모드) status above, actions below
 *
 * THE FIVE THINGS, IN EVERY STYLE
 * ------------------------------
 *   the layout in use  ·  한/영 mode  ·  Hanja  ·  voice  ·  settings
 *
 * Only the LAYOUT NAME may be truncated, per the same instruction. That is the
 * reason it is the one elastic child: everything else keeps its 48 dp touch
 * target on any screen, and the one elastic thing is the text the user already
 * knows they can go and read in full in the settings window.
 *
 * STATE LIVES HERE ONLY AS A VALUE
 * --------------------------------
 * Everything the strip shows arrives as a [ToolbarState] built in renderState()
 * and pushed into whichever views the current style has. That is the whole of
 * the crash fix: a rebuild that cannot lose the state it was holding, because
 * the rebuild never held it.
 */
package org.hunmin.ime

import android.content.Context
import android.util.AttributeSet
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat

class LayoutPicker @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : FrameLayout(context, attrs) {

    private var style: ToolbarStyle = ToolbarStyle.ONE_LINE

    /** Every layout this build can run, for the indicator's label lookup. */
    private var layouts: List<LayoutEntry> = emptyList()
    private var activeLayoutId: String? = null

    /** Style B shows an abbreviation; A and C show the full name, cut off. */
    

    private var modeIsHangul = true
    private var voiceState = VoiceState.IDLE
    private var hanjaConverting = false

    // The dock (style A) binds in place; the flat row and the column (B and C)
    // keep their own references, because they were written before the dock was
    // and reworking them is a separate piece of work.
    private var dock: DockToolbar? = null
    private var layoutIndicator: TextView? = null
    private var modeView: TextView? = null
    private var micButton: ImageButton? = null
    private var hanjaButton: TextView? = null
    private var settingsButton: ImageButton? = null

    /**
     * Every callback below re-renders on assignment, and that is a fix rather
     * than a style choice: the style's views capture the callbacks when they are
     * built, and the service builds the picker first and wires the callbacks
     * afterwards — so a dock built at construction time had null everywhere and
     * every key was inert. Measured: the gear's bounds were in the accessibility
     * tree and the tap did nothing at all.
     */

    /** Called when the user taps the mic. Null disables the button. */
    var onMicClicked: (() -> Unit)? = null
        set(value) {
            field = value
            rebuild()
        }

    /** Called when the user taps the Hanja button. Null disables the button. */
    var onHanjaClicked: (() -> Unit)? = null
        set(value) {
            field = value
            rebuild()
        }

    /** Called when the user taps the mode indicator (한/영). Null disables click. */
    var onModeClicked: (() -> Unit)? = null
        set(value) {
            field = value
            rebuild()
        }

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
            rebuild()
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
        set(value) {
            field = value
            rebuild()
        }

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
        renderState()
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
        renderState()
    }

    /** Update mode indicator (한/영). */
    fun setMode(isHangul: Boolean) {
        modeIsHangul = isHangul
        renderState()
    }

    /** Update mic button appearance for current recognition state. */
    fun setMicState(state: VoiceState) {
        android.util.Log.i("HunminIme", "mic state -> $state")
        voiceState = state
        renderState()
    }

    /**
     * Whether a Hanja conversion is running.
     *
     * Nothing reports this yet — the Hanja window is an activity the IME starts
     * and does not watch — so the dock's highlight stays off rather than being
     * driven by a flag that means "somebody called the setter". It is wired to
     * the same [ToolbarState.hanjaConverting] the dock renders, so turning the
     * highlight on is one call away when there is a true source for it.
     */
    fun setHanjaConverting(converting: Boolean) {
        hanjaConverting = converting
        renderState()
    }

    // ------------------------------------------------------------- composition

    /** The one place the strip's state is assembled, whichever style is up. */
    private fun renderState() {
        val state = stateForCurrentStyle()
        dock?.bind(state)
        layoutIndicator?.let { applyFlatState(it, state) }
        modeView?.let { it.text = if (state.modeIsHangul) "한" else "영" }
        applyFlatVoiceState()
    }

    private fun stateForCurrentStyle(): ToolbarState = ToolbarState.of(
        layouts = layouts,
        layoutId = activeLayoutId,
        modeIsHangul = modeIsHangul,
        voice = voiceState,
        hanjaConverting = hanjaConverting,
    )

    private fun rebuild() {
        removeAllViews()
        // Fresh views every rebuild, NOT the cached ones. Measured crash on the
        // phone (SM-F971N, Android 17): choosing style B or C threw
        //   IllegalStateException: The specified child already has a parent.
        // from ViewGroup.addViewInner, because removeAllViews() detaches the
        // row but leaves each button parented to that detached row, and
        // addView() refuses a view that already has one. Style A never showed
        // it, because A is the default and applyStyle() short-circuits when the
        // style has not changed.
        dock = null
        layoutIndicator = null
        modeView = null
        micButton = null
        hanjaButton = null
        settingsButton = null
        // MATCH_PARENT on the width, explicitly. The root is a FrameLayout, whose
        // generateDefaultLayoutParams() is WRAP_CONTENT — and a wrap-content
        // scroll view measures to nothing useful and clips its own row away, so
        // the strip came up with the layout indicator and nothing else. Measured:
        // the buttons were absent from the accessibility tree entirely, which
        // is the same failure as the gear button being off-screen.
        val fill = FrameLayout.LayoutParams(MATCH_PARENT, WRAP)
        when (style) {
            ToolbarStyle.ONE_LINE -> addView(oneLine(), fill)
            ToolbarStyle.TWO_LINE -> addView(twoRows(), fill)
        }
        renderState()
    }

    /** Style A: the floating pill. */
    private fun buildDock(): View {
        val toolbar = DockToolbar(
            context = context,
            state = stateForCurrentStyle(),
            actions = DockToolbar.Actions(
                onLayoutClicked = onLayoutClicked,
                onModeClicked = onModeClicked,
                onHanjaClicked = onHanjaClicked,
                onMicClicked = onMicClicked,
                onSettingsClicked = onSettingsClicked,
            ),
        )
        dock = toolbar
        return toolbar.build()
    }

    /** One row, wrapped in a scroll view that normally never scrolls.
 *
 * The wrapper is unconditional on purpose: it is the answer to the width
 * where the gaps are already at their smallest and the buttons still do not
 * fit, and a view that does not need to scroll is invisible to the user.
 */
private fun oneLine(): View {
    val scroll = HorizontalScrollView(context).apply {
        isHorizontalScrollBarEnabled = false
        isFillViewport = true
        isFocusable = false
        isFocusableInTouchMode = false
    }
    val row = LinearLayout(context).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        isFocusable = false
        isFocusableInTouchMode = false
    }
    val gap = gapDp()
    // Only the indicator is elastic: width 0 + weight 1 means "take the
    // leftover". Every other child is WRAP_CONTENT -- width 0 with weight 0
    // is zero pixels wide, which is how the three buttons vanished from the
    // strip on the first run of this.
    row.addView(layoutIndicator(), lp(0, weight = 1f, padEnd = gap))
    row.addView(modePill(), lp(WRAP, padEnd = gap))
    row.addView(hanjaButton(), lp(WRAP, padEnd = gap))
    row.addView(micButton(), lp(WRAP, padEnd = gap))
    row.addView(settingsButton(), lp(WRAP))
    scroll.addView(row)
    return scroll
}

/** Two rows: status above, evenly spread buttons below. */
    private fun twoRows(): View {
        val column = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            isFocusable = false
            isFocusableInTouchMode = false
        }
        val status = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            isFocusable = false
            isFocusableInTouchMode = false
        }
        val gap = gapDp()
        status.addView(layoutIndicator(), lp(0, weight = 1f, padEnd = gap))
        status.addView(modePill(), lp(WRAP))
        // MATCH width on the rows: lp(0) in a vertical LinearLayout would be
        // zero pixels wide. Use MATCH_PARENT so the rows fill the width.
        column.addView(status, lp(MATCH_PARENT))

        // weight = 1 on each button, so they spread evenly and stay 48 dp even
        // on a narrow screen: two rows means the horizontal budget is twice as
        // generous.
        val buttons = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            isFocusable = false
            isFocusableInTouchMode = false
        }
        buttons.addView(hanjaButton(), lp(0, weight = 1f, padEnd = gap))
        buttons.addView(micButton(), lp(0, weight = 1f, padEnd = gap))
        buttons.addView(settingsButton(), lp(0, weight = 1f))
        column.addView(buttons, lp(MATCH_PARENT))
        return column
    }

    private fun layoutIndicator(): TextView =
        TextView(context).also { layoutIndicator = it }.apply {
            textSize = 14f
            gravity = Gravity.CENTER_VERTICAL
            // One line, ellipsized: the maintainer's rule — the name may be cut,
            // but which layout is in use must stay legible.
            maxLines = 1
            ellipsize = android.text.TextUtils.TruncateAt.END
            layoutParams = lp(0, weight = 1f)
            setOnClickListener { onLayoutClicked?.invoke() }
            isClickable = onLayoutClicked != null
            isFocusable = false
            contentDescription = context.getString(R.string.picker_content_description)
        }

    /** The 한/영 toggle: a pill with the mode letter, tappable. */
    private fun modePill(): View {
        val label = TextView(context).also { modeView = it }.apply {
            textSize = 14f
            gravity = Gravity.CENTER
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            setBackgroundColor(0xFFFFFFFF.toInt())
            setTextColor(0xFF000000.toInt())
            setPadding(PAD_DP, PILL_PAD_DP, PAD_DP, PILL_PAD_DP)
            contentDescription = context.getString(R.string.mode_switch_content_description)
            setOnClickListener { onModeClicked?.invoke() }
            isFocusable = false
        }
        label.text = if (modeIsHangul) "한" else "영"
        // Selected as well as labelled: which mode is active has to be readable
        // without reading the colour, which is what the dock's segment does.
        label.isSelected = modeIsHangul
        return FrameLayout(context).apply {
            minimumWidth = TOUCH_DP
            minimumHeight = TOUCH_DP
            addView(label, FrameLayout.LayoutParams(WRAP, WRAP, Gravity.CENTER))
        }
    }

    /**
     * The Hanja key: 漢, a label rather than an icon.
     *
     * It was `ic_menu_add` — a plus sign — which says "add something", not "make
     * this Hangul into Hanja". The maintainer's words: "한자로 바꾼다는 게 눈에
     * 띄었으면 좋겠는데". The dock's own design (DockStyle.kt) asks for [ 漢 ] for
     * the same reason, so B and C say it in the same character rather than
     * leaving two spellings of one key.
     */
    private fun hanjaButton(): TextView =
        TextView(context).apply {
            text = "漢"
            textSize = 16f
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
            setTextColor(0xFF000000.toInt())
            setBackgroundColor(0xFFFFFFFF.toInt())
            contentDescription = context.getString(R.string.hanja_content_description)
            setOnClickListener { onHanjaClicked?.invoke() }
            setPadding(HANJA_PAD_DP, HANJA_PAD_DP, HANJA_PAD_DP, HANJA_PAD_DP)
            minimumWidth = TOUCH_DP
            minimumHeight = TOUCH_DP
            isFocusable = false
            layoutParams = lp(WRAP)
        }

    private fun micButton(): ImageButton =
        ImageButton(context).also { micButton = it }.apply {
            setImageResource(R.drawable.ph_microphone)
            contentDescription = context.getString(R.string.voice_content_description)
            setBackgroundColor(0x00000000)
            setOnClickListener { onMicClicked?.invoke() }
            setPadding(BTN_PADDING_DP, BTN_PADDING_DP, BTN_PADDING_DP, BTN_PADDING_DP)
            minimumWidth = TOUCH_DP
            minimumHeight = TOUCH_DP
            isFocusable = false
            layoutParams = lp(WRAP)
        }

    private fun settingsButton(): ImageButton =
        ImageButton(context).also { settingsButton = it }.apply {
            setImageResource(R.drawable.ph_gear)
            contentDescription = context.getString(R.string.settings_content_description)
            setBackgroundColor(0x00000000)
            setOnClickListener { onSettingsClicked?.invoke() }
            setPadding(BTN_PADDING_DP, BTN_PADDING_DP, BTN_PADDING_DP, BTN_PADDING_DP)
            minimumWidth = TOUCH_DP
            minimumHeight = TOUCH_DP
            isFocusable = false
            layoutParams = lp(WRAP)
        }

    // ----------------------------------------------------------------- helpers

    /** B and C's label view: the same text and the same spoken name as the dock. */
    private fun applyFlatState(view: TextView, state: ToolbarState) {
        view.text = state.layoutLabel
        view.contentDescription = state.layoutLabelFull
            .let { context.getString(R.string.picker_content_description) + ": " + it }
    }

    /**
     * B and C's mic colouring.
     *
     * The green capsule is the dock's business; a flat row has no pill to fill,
     * so the icon carries the state there, as it did before.
     */
    private fun applyFlatVoiceState() {
        val button = micButton ?: return
        val color = when (voiceState) {
            VoiceState.IDLE -> 0
            VoiceState.LISTENING -> 0xFF43A047.toInt()
            VoiceState.ERROR -> android.graphics.Color.GRAY
        }
        if (color == 0) button.clearColorFilter() else button.setColorFilter(color)
        button.contentDescription = context.getString(
            when (voiceState) {
                VoiceState.IDLE -> R.string.voice_content_description
                VoiceState.LISTENING -> R.string.voice_listening
                VoiceState.ERROR -> R.string.voice_error
            },
        )
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
        const val HANJA_PAD_DP = 10
        const val TOUCH_DP = 48
    }
}

/** Style B's abbreviation: the id's first token, uppercased, cut to 6 chars. */
internal fun LayoutEntry.shortLabel(): String {
    val head = id.substringBefore('-').uppercase()
    return if (head.length <= 6) head else head.take(6)
}