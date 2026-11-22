/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * KeypadView — the soft keypad's pixels (issue #299, T3-3).
 *
 * Rendering and tap dispatch ONLY: every decision (labels, rows, keycodes,
 * shift/page state, visibility) is KeypadModel's / KeypadVisibility's, and every
 * tap leaves through one of four callbacks into the service's existing paths.
 * docs/DESIGN-ANDROID-KEYPAD.md §1: touch is hardware — a character tap is the
 * (keyCode, metaState) a physical key delivers, entering the SAME onKeyDown;
 * a symbol tap is KP11's direct commit, flushed first by the service.
 *
 * The view holds no policy the model does not already answer. Rebuilds are
 * cheap (a few dozen TextViews) and happen on state changes only: shift taps,
 * page switches, mode/layout refreshes from the service.
 */
package org.hunmin.ime

import android.content.Context
import android.graphics.drawable.GradientDrawable
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView

class KeypadView(context: Context) : LinearLayout(context) {

    /** A character/function keycap tap: the shared key path's two arguments. */
    var onCharacterKey: ((keyCode: Int, metaState: Int) -> Unit)? = null

    /** A symbol-page character tap: KP11's direct commit (service flushes first). */
    var onSymbol: ((String) -> Unit)? = null

    /** 한/영 on the TEXT page: the language toggle, as the strip's mode key. */
    var onToggleLanguage: (() -> Unit)? = null

    /** 한/영 on the SYMBOL page: return to the mode the page was entered from (KP9). */
    var onReturnToText: ((hangul: Boolean) -> Unit)? = null

    private val state = KeypadState()
    private val theme = HudTheme.of(context)
    private var hangulCaps: KeypadKeycaps? = null
    private var latinCaps: KeypadKeycaps? = null
    private var symbols: SymbolScreen? = null
    private var hangulMode = true

    init {
        orientation = VERTICAL
        // The platform's key-click sound is the hardware keyboard's feedback;
        // a soft keypad that plays it double-clicks against the app's own.
        isSoundEffectsEnabled = false
    }

    /** The active layouts' keycaps, read by the service from the engine's data. */
    fun setKeycaps(hangul: KeypadKeycaps?, latin: KeypadKeycaps?) {
        hangulCaps = hangul
        latinCaps = latin
        rebuild()
    }

    fun setSymbols(screen: SymbolScreen?) {
        symbols = screen
        rebuild()
    }

    /** The service's mode changed (toggle, judgement, new editor). */
    fun setMode(hangul: Boolean) {
        hangulMode = hangul
        state.modeChanged(hangul)
        rebuild()
    }

    private fun rebuild() {
        removeAllViews()
        val rows: List<List<KeypadKey>> = when (state.page) {
            KeypadState.Page.TEXT -> {
                val caps = if (hangulMode) hangulCaps else latinCaps
                if (caps == null) emptyList() else KeypadModel.textRows(caps, state.shiftArmed)
            }
            KeypadState.Page.SYMBOLS ->
                symbols?.let { KeypadModel.symbolRows(it) } ?: emptyList()
        }
        for (row in rows) addView(rowView(row))
    }

    private fun rowView(row: List<KeypadKey>): View {
        val line = LinearLayout(context)
        line.orientation = HORIZONTAL
        val rowHeight = dp(48)
        for (key in row) {
            val cap = keycap(key)
            val weight = when {
                key.id == "space" -> 3f
                key.kind == KeypadKey.Kind.MODE || key.kind == KeypadKey.Kind.SYMBOLS_PAGE -> 1.4f
                key.id == "enter" || key.id == "backspace" || key.kind == KeypadKey.Kind.SHIFT -> 1.5f
                else -> 1f
            }
            line.addView(
                cap,
                LayoutParams(0, rowHeight, weight).apply {
                    setMargins(dp(1), dp(1), dp(1), dp(1))
                },
            )
        }
        return line
    }

    private fun keycap(key: KeypadKey): TextView {
        val tv = TextView(context)
        tv.text = key.label
        tv.gravity = Gravity.CENTER
        tv.contentDescription = key.contentDesc
        tv.isClickable = true
        tv.isFocusable = true
        // KP3's provisional hierarchy: characters 진하게, functional 연하게.
        val primary = key.kind == KeypadKey.Kind.CHARACTER || key.kind == KeypadKey.Kind.SYMBOL
        tv.setTextColor(if (primary) theme.textPrimary else theme.textSecondary)
        tv.setTextSize(
            TypedValue.COMPLEX_UNIT_SP,
            if (primary) 18f else 14f,
        )
        val armedShift = key.kind == KeypadKey.Kind.SHIFT && state.shiftArmed
        tv.background = GradientDrawable().apply {
            cornerRadius = dp(4).toFloat()
            setColor(if (armedShift) theme.accentBg else theme.keyBg)
            setStroke(dp(1), theme.keyBorder)
        }
        tv.setOnClickListener { tap(key) }
        return tv
    }

    private fun tap(key: KeypadKey) {
        when (key.kind) {
            KeypadKey.Kind.CHARACTER -> {
                // The one-shot shift rides the metaState, exactly as a hardware
                // Shift+key does; consuming it re-renders the plain labels.
                val meta = state.shiftMeta()
                onCharacterKey?.invoke(key.keyCode, meta)
                if (meta != 0) rebuild()
            }
            KeypadKey.Kind.FUNCTION ->
                // Function keys do not consume the one-shot (design §4).
                onCharacterKey?.invoke(key.keyCode, 0)
            KeypadKey.Kind.SHIFT -> {
                state.tapShift()
                rebuild()
            }
            KeypadKey.Kind.MODE ->
                if (state.page == KeypadState.Page.SYMBOLS) {
                    val hangul = state.returnToText()
                    onReturnToText?.invoke(hangul)
                    rebuild()
                } else {
                    onToggleLanguage?.invoke()
                }
            KeypadKey.Kind.SYMBOLS_PAGE -> {
                if (symbols == null) return  // no asset: the page cannot be drawn
                state.enterSymbols(hangulMode)
                rebuild()
            }
            KeypadKey.Kind.SYMBOL -> onSymbol?.invoke(key.id)
        }
    }

    private fun dp(v: Int): Int =
        (v * resources.displayMetrics.density + 0.5f).toInt()
}