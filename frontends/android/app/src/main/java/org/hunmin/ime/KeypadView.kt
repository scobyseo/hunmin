/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * KeypadView — the soft keypad's pixels (issue #299, T3-3).
 *
 * THE UI IS THE DECIDED MOCKUP PICTURE (KP13, maintainer 2026-11-22:
 * "docs/android-keypad-mockup.html 로 정했잖아?"). Palette, proportions, corner
 * hints and sublabels below are transcribed from that artefact — it is the
 * requirement's picture, so its values are the spec, not a theme choice. The
 * first implementation used the HUD palette and a generic grid; that deviation
 * is overturned and recorded in docs/DESIGN-ANDROID-KEYPAD.md.
 *
 * Rendering and tap dispatch ONLY: every decision (labels, rows, keycodes,
 * shift/page state, visibility) is KeypadModel's / KeypadVisibility's, and every
 * tap leaves through one of four callbacks into the service's existing paths.
 * Touch is hardware (design §1): a character tap is the (keyCode, metaState) a
 * physical key delivers, entering the SAME onKeyDown; a symbol tap is KP11's
 * direct commit, flushed first by the service.
 */
package org.hunmin.ime

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
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

    /** KP14: a long swipe-left from any keycap — delete, not type. */
    var onSwipeLeftDelete: (() -> Unit)? = null

    private val state = KeypadState()
    private var hangulCaps: KeypadKeycaps? = null
    private var latinCaps: KeypadKeycaps? = null
    private var symbols: SymbolScreen? = null
    private var hangulMode = true

    /* The mockup's palette (docs/android-keypad-mockup.html :root). The picture
     * draws a light theme; night-mode adaptation is not drawn and stays open
     * (KP13 scope notes). */
    private val bg = Color.parseColor("#f4f5f7")
    private val keyBg = Color.parseColor("#ffffff")
    private val keyBorder = Color.parseColor("#d7dae0")
    private val ink = Color.parseColor("#1a1c20")        // 진하게: primary characters
    private val faint = Color.parseColor("#9aa0ab")      // 연하게: numbers
    private val hintInk = Color.parseColor("#b6bbc4")    // corner hints / sublabels
    private val fnBg = Color.parseColor("#eef0f3")
    private val fnInk = Color.parseColor("#6b7280")
    private val accent = Color.parseColor("#2f6df6")     // 한/영 · ⏎
    private val accentInk = Color.parseColor("#ffffff")
    private val spaceBg = Color.parseColor("#e9edf5")

    init {
        orientation = VERTICAL
        setBackgroundColor(bg)
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
        // The picture: 46px keys, 5px gaps, at a 360px phone — dp for dp.
        val rowHeight = dp(46)
        val half = dp(5) / 2f
        for (key in row) {
            val cap = keycap(key)
            val weight = when {
                key.kind == KeypadKey.Kind.FUNCTION && key.id == "space" -> 4f
                key.id == "backspace" || key.kind == KeypadKey.Kind.SHIFT -> 1.4f
                else -> 1f
            }
            val lp = LayoutParams(0, rowHeight, weight)
            lp.setMargins(half.toInt(), 0, (dp(5) - half.toInt()), dp(5))
            line.addView(cap, lp)
        }
        return line
    }

    private fun keycap(key: KeypadKey): View {
        val frame = FrameLayout(context)
        frame.contentDescription = key.contentDesc
        frame.isClickable = true
        frame.isFocusable = true

        val armedShift = key.kind == KeypadKey.Kind.SHIFT && state.shiftArmed
        val background = when {
            armedShift -> accent
            key.kind == KeypadKey.Kind.MODE -> accent
            key.kind == KeypadKey.Kind.FUNCTION && key.id == "enter" -> accent
            key.kind == KeypadKey.Kind.FUNCTION && key.id == "space" -> spaceBg
            key.kind == KeypadKey.Kind.FUNCTION || key.kind == KeypadKey.Kind.SHIFT ||
                key.kind == KeypadKey.Kind.SYMBOLS_PAGE -> fnBg
            else -> keyBg
        }
        val foreground = when {
            armedShift || key.kind == KeypadKey.Kind.MODE ||
                (key.kind == KeypadKey.Kind.FUNCTION && key.id == "enter") -> accentInk
            key.kind == KeypadKey.Kind.DIGIT -> faint
            key.kind == KeypadKey.Kind.FUNCTION || key.kind == KeypadKey.Kind.SHIFT ||
                key.kind == KeypadKey.Kind.SYMBOLS_PAGE -> fnInk
            else -> ink
        }
        val textSize = when (key.kind) {
            KeypadKey.Kind.CHARACTER, KeypadKey.Kind.DIGIT, KeypadKey.Kind.SYMBOL -> 19f
            KeypadKey.Kind.MODE, KeypadKey.Kind.SYMBOLS_PAGE -> 13f
            KeypadKey.Kind.FUNCTION -> if (key.id == "space") 13f else 14f
            KeypadKey.Kind.SHIFT -> 14f
        }
        frame.background = GradientDrawable().apply {
            cornerRadius = dp(8).toFloat()
            setColor(background)
            setStroke(dp(1), keyBorder)
        }

        val main = TextView(context)
        main.text = key.label
        main.gravity = Gravity.CENTER
        main.setTextColor(foreground)
        main.setTextSize(TypedValue.COMPLEX_UNIT_SP, textSize)
        main.paint.isFakeBoldText = true   // the picture: font-weight 700
        frame.addView(main, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))

        // Corner hint, top-right (the picture's .hint: 11px, 롱프레스/swipe-up).
        key.hint?.let { h ->
            val tv = TextView(context)
            tv.text = h
            tv.setTextColor(hintInk)
            tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
            tv.paint.isFakeBoldText = true
            frame.addView(tv, FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.TOP or Gravity.END).apply {
                setMargins(0, dp(2), dp(5), 0)
            })
        }
        // Sublabel, bottom-left (the picture's .sub: 10px, shifted jamo).
        key.sub?.let { s ->
            val tv = TextView(context)
            tv.text = s
            tv.setTextColor(hintInk)
            tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10f)
            frame.addView(tv, FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM or Gravity.START).apply {
                setMargins(dp(5), 0, 0, dp(1))
            })
        }
        frame.setOnClickListener { tap(key) }
        /* KP14: a long swipe-left deletes instead of typing. The gesture is
         * recognised on MOVE once it is long enough (immediate feedback); the
         * tap fires on UP only when no gesture fired, so the key is never
         * typed AND deleted. A short drift stays a tap (KP5 protection). */
        var downX = 0f
        var downY = 0f
        var swiped = false
        val threshold = dp(60).toFloat()   // provisional (KP14); ~two key widths
        frame.setOnTouchListener { v, ev ->
            when (ev.actionMasked) {
                android.view.MotionEvent.ACTION_DOWN -> {
                    downX = ev.x; downY = ev.y; swiped = false; true
                }
                android.view.MotionEvent.ACTION_MOVE -> {
                    if (!swiped && KeypadModel.isSwipeLeftDelete(ev.x - downX, ev.y - downY, threshold)) {
                        swiped = true
                        onSwipeLeftDelete?.invoke()
                    }
                    true
                }
                android.view.MotionEvent.ACTION_UP -> {
                    if (!swiped) v.performClick()
                    true
                }
                else -> true   // CANCEL: neither type nor delete
            }
        }
        return frame
    }

    private fun tap(key: KeypadKey) {
        when (key.kind) {
            KeypadKey.Kind.CHARACTER, KeypadKey.Kind.DIGIT -> {
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