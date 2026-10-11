/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * DockStyle — the floating pill: style A's new shape.
 *
 * THE SHAPE THE MAINTAINER ASKED FOR
 * ----------------------------------
 *   ┌──────────────────────────────────────────────────────────┐
 *   │ [ 두벌식 ▾ ] │ [ 한 | A ]        漢   🎙   ⚙             │  48 dp, floating
 *   └──────────────────────────────────────────────────────────┘
 * status and switching on the left, actions on the right, space between. The
 * layout name is the one elastic part; everything else keeps its 48 dp target.
 *
 * WHY THIS IS CODE AND NOT AN XML LAYOUT
 * --------------------------------------
 * The dock's parts are created in a builder because the OTHER two styles build
 * theirs the same way, and a layout XML would have to be inflated and then
 * taken apart for style B and C anyway. What the XML would have bought —
 * the shapes — is six small drawables, each named for what it is.
 *
 * NOTHING HERE TAKES FOCUS
 * ------------------------
 * An IME that takes focus takes the editor's cursor with it. Every child here is
 * focusable = false, in touch mode as well, and the IME's input view is not
 * focusable to begin with.
 *
 * The heavier instrument — descendantFocusability = FOCUS_BLOCK_DESCENDANTS — is
 * deliberately NOT used, and that is a measurement, not a preference: with it set,
 * the whole dock disappears from the accessibility tree, `uiautomator dump` shows the
 * window and none of its children, and tier E2 fails on "no mic button in the IME view"
 * with nothing visibly wrong. A toolbar that no screen reader and no test can reach is
 * worse than one that could in principle take a focus it is never offered.
 */
package org.hunmin.ime

import android.content.Context
import android.animation.ObjectAnimator
import android.graphics.Typeface
import android.text.TextUtils
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.Space
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat

/**
 * Builds and binds the dock.
 *
 * The builder returns the root; [bind] is what the rest of the IME talks to, and
 * it never rebuilds anything — a style change builds a new dock, a state change
 * repaints this one. That split is the whole of the crash fix in
 * [LayoutPicker.rebuild]: a view that is not rebuilt cannot be reparented.
 */
internal class DockToolbar(
    private val context: Context,
    private val state: ToolbarState,
    private val actions: Actions,
) {
    /** What the dock can ask for. Every one of them may be absent. */
    data class Actions(
        val onLayoutClicked: (() -> Unit)? = null,
        val onModeClicked: (() -> Unit)? = null,
        val onHanjaClicked: (() -> Unit)? = null,
        val onMicClicked: (() -> Unit)? = null,
        val onSettingsClicked: (() -> Unit)? = null,
    )

    private val onSurface = color(R.color.hunmin_on_surface)
    private val onPrimary = color(R.color.hunmin_on_primary)
    private val micGreen = color(R.color.hunmin_mic_active)
    private val onMicGreen = color(R.color.hunmin_on_mic_active)
    private val primary = color(R.color.hunmin_primary)

    private var pulseAnimator: ObjectAnimator? = null

    private val chipLabel = TextView(context)
    private val hangulHalf = TextView(context)
    private val latinHalf = TextView(context)
    private val hanjaKey = TextView(context)
    private var micKey: ImageView? = null
    private var gearKey: ImageView? = null

    /** The pill. Marginally transparent background so a shadow has something to sit on. */
    fun build(): View {
        val dock = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            minimumHeight = dp(48)
            setPadding(dp(10), dp(4), dp(10), dp(4))
            background = ContextCompat.getDrawable(context, R.drawable.toolbar_dock)
            ViewCompat.setElevation(this, dp(4).toFloat())
            isFocusable = false
            isFocusableInTouchMode = false
            isFocusableInTouchMode = false
        }
        dock.layoutParams = FrameLayout.LayoutParams(MATCH, WRAP).apply {
            setMargins(dp(10), dp(4), dp(10), dp(4))
        }
        dock.addView(layoutChip(), lp(0, weight = 1f, padEnd = dp(8)))
        // Sized HERE, once. A child that sets its own layoutParams and is then
        // added with addView(child, params) gets those params overwritten — and
        // the row measured 1080x2209 on the emulator with the divider filling it,
        // because a WRAP-sized View with no intrinsic size is not "wrap", it is
        // whatever the parent feels like. Measured: the keys were laid out at
        // x = 1253, 1389, 1525 on a 1080 px window, i.e. off-screen, and every one
        // of them was visible: false in the accessibility tree.
        dock.addView(divider(), LinearLayout.LayoutParams(dp(1), dp(14)).apply { marginEnd = dp(8) })
        dock.addView(modeSegment(), lp(WRAP))
        dock.addView(Space(context), lp(0, weight = 1f))
        dock.addView(hanjaKey(), lp(WRAP, padEnd = dp(4)))
        dock.addView(micKey(), lp(WRAP, padEnd = dp(4)))
        dock.addView(gearKey(), lp(WRAP))
        bind(state)
        return FrameLayout(context).apply {
            // The window itself stays full width and transparent: the dock floats
            // in it. A wrap-content window would shrink the IME's touch target
            // area and, on some hosts, hide the window entirely.
            addView(dock, FrameLayout.LayoutParams(MATCH, WRAP))
            isFocusable = false
            isFocusableInTouchMode = false
            isFocusableInTouchMode = false
        }
    }

    /** Repaint from [state]. Nothing here allocates a view. */
    fun bind(state: ToolbarState) {
        chipLabel.text = state.layoutLabel
        chipLabel.contentDescription =
            context.getString(R.string.picker_content_description) + ": " + state.layoutLabelFull

        val hangulOn = state.modeIsHangul
        segmentHalf(hangulHalf, "한", hangulOn, R.string.mode_hangul)
        segmentHalf(latinHalf, "A", !hangulOn, R.string.mode_latin)

        hanjaKey.setBackgroundResource(
            if (state.hanjaConverting) R.drawable.toolbar_hanja_active else 0,
        )
        hanjaKey.setTextColor(if (state.hanjaConverting) primary else onSurface)

        val mic = micKey
        when (state.voice) {
            VoiceState.IDLE -> {
                mic?.setBackgroundResource(0)
                mic?.clearColorFilter()
            }
            // Three signals, because one was not enough and the maintainer said
            // so: the key turns into a green capsule, the glyph goes white on it,
            // and the key breathes while the recogniser is running. The capsule
            // says "this key is live"; the pulse says "and it is live NOW", which
            // is the difference nobody can miss from across a desk.
            VoiceState.LISTENING -> {
                mic?.setBackgroundResource(R.drawable.toolbar_mic_active)
                mic?.setColorFilter(onMicGreen)
                pulse(mic)
            }
            VoiceState.ERROR -> {
                stopPulse(mic)
                mic?.setBackgroundResource(0)
                mic?.setColorFilter(micGreen)
            }
        }
        mic?.contentDescription = context.getString(
            when (state.voice) {
                VoiceState.IDLE -> R.string.voice_content_description
                VoiceState.LISTENING -> R.string.voice_listening
                VoiceState.ERROR -> R.string.voice_error
            },
        )
    }

    // ------------------------------------------------------------------ parts

    /**
     * The layout name as an outlined chip with a caret.
     *
     * The caret is what makes it read as "opens something" — a bare label with
     * a dotted underline would not, and this strip has no room for both. Tapping
     * it opens the settings at the layout section; the maintainer's design named
     * either a cheat sheet or a quick picker there, and a cheat sheet is not
     * built. The label is the elastic child and the caret is not: a caret with
     * its triangle cut off is just a dash.
     */
    private fun layoutChip(): View = LinearLayout(context).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        minimumHeight = dp(40)
        setPadding(dp(12), 0, dp(10), 0)
        background = ContextCompat.getDrawable(context, R.drawable.toolbar_chip)
        isFocusable = false
            isFocusableInTouchMode = false
        isFocusableInTouchMode = false
        clickable(actions.onLayoutClicked != null)
        setOnClickListener { actions.onLayoutClicked?.invoke() }
        chipLabel.apply {
            textSize = 14f
            maxLines = 1
            ellipsize = TextUtils.TruncateAt.END
            setTextColor(onSurface)
            importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
            layoutParams = lp(0, weight = 1f)
        }
        addView(chipLabel)
        addView(ImageView(context).apply {
            setImageResource(R.drawable.ph_caret_down)
            setColorFilter(onSurface)
            importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
            layoutParams = lp(WRAP)
        })
    }

    /** The 1 dp rule between the layout chip and the mode segment. Sized at addView. */
    private fun divider(): View = View(context).apply {
        setBackgroundColor(color(R.color.hunmin_outline))
    }

    /**
     * 한 | A, as a segment rather than a single letter.
     *
     * Both halves are always visible so the control says what it toggles; the
     * active one is filled. It reads at a glance in a way a lone letter does
     * not, and it costs one extra word.
     */
    private fun modeSegment(): View = LinearLayout(context).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        isFocusable = false
            isFocusableInTouchMode = false
        isFocusableInTouchMode = false
        clickable(actions.onModeClicked != null)
        setOnClickListener { actions.onModeClicked?.invoke() }
        contentDescription = context.getString(R.string.mode_switch_content_description)
        addView(hangulHalf, lp(WRAP))
        addView(latinHalf, lp(WRAP))
    }

    private fun segmentHalf(view: TextView, label: String, active: Boolean, descRes: Int) {
        view.apply {
            text = label
            contentDescription = context.getString(descRes)
            // Selection is the state, and it is what a screen reader announces
            // and what the end-to-end suite asserts. The label is a display
            // choice — the dock says "A" for Latin, the maintainer's design, and
            // styles B and C say "영" — so "which half is filled" has to be
            // readable without reading the colour.
            isSelected = active
            importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
            textSize = 14f
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
            minimumWidth = dp(34)
            minimumHeight = dp(32)
            setPadding(dp(10), 0, dp(10), 0)
            setBackgroundResource(if (active) R.drawable.toolbar_segment_active else R.drawable.toolbar_segment_track)
            setTextColor(if (active) onPrimary else onSurface)
        }
    }

    /**
     * The Hanja key carries the character, not a picture of it.
     *
     * It was `ic_menu_add` — a plus — which says "add something". The
     * maintainer's words: "한자로 바꾼다는 게 눈에 띄었으면 좋겠는데". A
     * 48 dp key in an IME strip has room for one character and the character
     * is what Korean keyboards have always used for this key.
     */
    private fun hanjaKey(): TextView = TextView(context).apply {
        text = "漢"
        textSize = 18f
        typeface = Typeface.DEFAULT_BOLD
        gravity = Gravity.CENTER
        setTextColor(onSurface)
        contentDescription = context.getString(R.string.hanja_content_description)
        minimumWidth = dp(48)
        minimumHeight = dp(48)
        importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
        clickable(actions.onHanjaClicked != null)
        setOnClickListener { actions.onHanjaClicked?.invoke() }
    }

    /**
     * `also { … }` is load-bearing, not decoration.
     *
     * These builders used to RETURN a fresh view while the same-named FIELD was
     * a separate ImageView created at construction: bind() then repainted an
     * object that was never in the view tree, so every state change to the mic
     * was invisible while the log said LISTENING and the field was non-null.
     * Measured on the phone: the content-desc stayed "음성 입력" through a whole
     * session and the pixels did not move. The chip and the segment did not have
     * this shape — their fields ARE the views that get added — which is exactly
     * why 한/영 reacted and the mic did not.
     */
    private fun micKey(): ImageView = icon(
        R.drawable.ph_microphone,
        R.string.voice_content_description,
        actions.onMicClicked,
    ).also { micKey = it }

    private fun gearKey(): ImageView = icon(
        R.drawable.ph_gear,
        R.string.settings_content_description,
        actions.onSettingsClicked,
    ).also { gearKey = it }

    private fun icon(drawable: Int, descRes: Int, onClick: (() -> Unit)?): ImageView =
        ImageView(context).apply {
            setImageResource(drawable)
            contentDescription = context.getString(descRes)
            setPadding(dp(12), dp(12), dp(12), dp(12))
            minimumWidth = dp(48)
            minimumHeight = dp(48)
            scaleType = ImageView.ScaleType.FIT_CENTER
            clickable(onClick != null)
            setOnClickListener { onClick?.invoke() }
        }

    // ---------------------------------------------------------------- helpers

    /**
     * The listening pulse: 1.0 to 1.08 and back, 700 ms, forever, until stopped.
     *
     * Deliberately slow and deliberately small. A keyboard that pulses hard is
     * a keyboard that draws the eye away from the text being typed into, and the
     * whole point of this key is that it is at the edge of attention.
     */
    private fun pulse(mic: ImageView?) {
        val view = mic ?: return
        if (pulseAnimator?.target === view && pulseAnimator?.isRunning == true) return
        pulseAnimator?.cancel()
        pulseAnimator = ObjectAnimator.ofFloat(view, View.SCALE_X, 1f, 1.08f).apply {
            duration = 700
            repeatMode = ObjectAnimator.REVERSE
            repeatCount = ObjectAnimator.INFINITE
            addUpdateListener { view.scaleY = it.animatedValue as Float }
            start()
        }
    }

    private fun stopPulse(mic: ImageView?) {
        pulseAnimator?.cancel()
        pulseAnimator = null
        mic?.scaleX = 1f
        mic?.scaleY = 1f
    }

    /** A tappable key that still refuses focus — the editor keeps the caret. */
    private fun View.clickable(canClick: Boolean) {
        isFocusable = false
        isFocusableInTouchMode = false
        isClickable = canClick
    }

    private fun lp(width: Int, weight: Float = 0f, padEnd: Int = 0): LinearLayout.LayoutParams =
        LinearLayout.LayoutParams(width, WRAP, weight).apply {
            if (padEnd > 0) marginEnd = padEnd
        }

    private fun color(id: Int): Int = ContextCompat.getColor(context, id)

    private fun dp(value: Int): Int = TypedValue.applyDimension(
        TypedValue.COMPLEX_UNIT_DIP, value.toFloat(), context.resources.displayMetrics,
    ).toInt()

    private companion object {
        const val WRAP = LinearLayout.LayoutParams.WRAP_CONTENT
        const val MATCH = FrameLayout.LayoutParams.MATCH_PARENT
    }
}