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
import android.widget.PopupWindow
import android.widget.ScrollView
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

    /** KP14/KP7: one WORD-unit delete tick (backspace long-press repeat). */
    var onWordDelete: (() -> Unit)? = null


    /* Gesture timing lives here (the view owns the pointer); every DECISION
     * is KeypadModel's pure table. One handler for all keycaps; rebuild()
     * drops pending callbacks so a removed keycap never fires. */
    private val handler = android.os.Handler(android.os.Looper.getMainLooper())

    private val state = KeypadState()
    private var hangulCaps: KeypadKeycaps? = null
    private var latinCaps: KeypadKeycaps? = null
    private var symbols: SymbolScreen? = null
    private var extended: SymbolScreen? = null
    private var math: SymbolScreen? = null
    private var emoji: SymbolScreen? = null
    private var kaomoji: SymbolScreen? = null
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

    /** KP10's symbol pages (#312): extended / math / emoji / kaomoji. */
    fun setExtended(screen: SymbolScreen?) { extended = screen; rebuild() }
    fun setMath(screen: SymbolScreen?) { math = screen; rebuild() }
    fun setEmoji(screen: SymbolScreen?) { emoji = screen; rebuild() }
    fun setKaomoji(screen: SymbolScreen?) { kaomoji = screen; rebuild() }

    /** The symbol sets that have data — the picker lists and cycle order. */
    private fun availableSets(): List<KeypadState.Page> = buildList {
        add(KeypadState.Page.SYMBOLS)
        if (extended != null) add(KeypadState.Page.EXTENDED)
        if (math != null) add(KeypadState.Page.MATH)
        if (emoji != null) add(KeypadState.Page.EMOJI)
        if (kaomoji != null) add(KeypadState.Page.KAOMOJI)
    }

    /** The service's mode changed (toggle, judgement, new editor). */
    fun setMode(hangul: Boolean) {
        hangulMode = hangul
        state.modeChanged(hangul)
        rebuild()
    }

    private fun rebuild() {
        handler.removeCallbacksAndMessages(null)
        removeAllViews()
        val rows: List<List<KeypadKey>> = when (state.page) {
            KeypadState.Page.TEXT -> {
                val caps = if (hangulMode) hangulCaps else latinCaps
                if (caps == null) emptyList() else KeypadModel.textRows(caps, state.shiftActive)
            }
            KeypadState.Page.SYMBOLS ->
                symbols?.let { KeypadModel.symbolRows(it) } ?: emptyList()
            KeypadState.Page.EXTENDED ->
                extended?.let { KeypadModel.symbolRows(it) } ?: emptyList()
            KeypadState.Page.MATH ->
                math?.let { KeypadModel.symbolRows(it) } ?: emptyList()
            KeypadState.Page.EMOJI ->
                emoji?.let { KeypadModel.symbolRows(it) } ?: emptyList()
            KeypadState.Page.KAOMOJI -> {
                // KP18: a scrollable full-width LIST (a kaomoji is wider than
                // a grid key); a 5-row viewport keeps the IME window bounded.
                val scr = kaomoji
                if (scr != null) {
                    val all = KeypadModel.kaomojiRows(scr)
                    val inner = LinearLayout(context)
                    inner.orientation = VERTICAL
                    // Only the kaomoji rows scroll; the MODE row stays fixed
                    // below (KP9: the mode keys keep their slots, always
                    // visible — measured: inside the scroll it took 30 rows
                    // of scrolling to reach 기호 1).
                    for (row in all.filter { r -> r.any { it.kind == KeypadKey.Kind.SYMBOL } }) {
                        inner.addView(rowView(row))
                    }
                    val scroll = ScrollView(context)
                    scroll.addView(inner, LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT))
                    addView(scroll, LayoutParams(LayoutParams.MATCH_PARENT, dp(255)))
                    for (row in all.filter { r -> r.none { it.kind == KeypadKey.Kind.SYMBOL } }) {
                        addView(rowView(row))
                    }
                }
                emptyList()   // the scroll view + fixed mode row carry the page
            }
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

        val armedShift = key.kind == KeypadKey.Kind.SHIFT && state.shiftActive
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
        // WRAP_CONTENT + center: the view's bounds are the text box, so the
        // no-overlap condition (maintainer: "글자가 겹치지만 않는다면") is
        // measurable from the platform hierarchy (tools/keypad-overlap-check.py).
        frame.addView(main, FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.CENTER))

        // Corner hint, top-right (the picture's .hint: 11px, 롱프레스/swipe-up).
        key.hint?.let { h ->
            val tv = TextView(context)
            tv.text = h
            tv.setTextColor(hintInk)
            // 10sp (mockup 11px): measured overlap with the 19sp main label
            // at 11sp — the no-overlap condition wins (keypad-overlap-check).
            tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10f)
            tv.paint.isFakeBoldText = true
            frame.addView(tv, FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.TOP or Gravity.END).apply {
                setMargins(0, dp(1), dp(3), 0)
            })
        }
        // Sublabel, bottom-left (the picture's .sub: 10px, shifted jamo).
        // Placement KEPT AS-IS by the maintainer (2026-11-22: "현행 유지"),
        // after a top-corner move was applied and reverted the same day.
        // Non-overlap with the main label is measured from the hierarchy
        // bounds (the maintainer's condition: "글자가 겹치지만 않는다면").
        key.sub?.let { s ->
            val tv = TextView(context)
            tv.text = s
            tv.setTextColor(hintInk)
            tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9f)
            frame.addView(tv, FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM or Gravity.START).apply {
                setMargins(dp(4), 0, 0, 0)
            })
        }
        frame.setOnClickListener { tap(key) }
        /* The gesture state machine (#312, KP5/KP7/KP14). One pointer, three
         * outcomes, separated by moved-vs-held-vs-released:
         *  - swipe-LEFT deletes on MOVE once long enough (KP14, immediate);
         *  - swipe-UP ARMS on MOVE past the gate, commits on UP, and a return
         *    to the origin cancels it back to a tap (KP5's layered defences);
         *  - a still hold past the system long-press timeout fires the SAME
         *    action as swipe-up (KP7's general equivalence) — except the
         *    recorded per-key exceptions (KeypadModel.gestureAction): the
         *    기호 key opens the picker, backspace repeats WORD deletion at
         *    ~0.2 s until release. Movement past jitter cancels the hold.
         * The tap fires on UP only when no gesture fired, so a key is never
         * typed AND gestured. Thresholds are provisional (KP14's shared
         * 60dp; jitter/return radii below), the decision shape is the
         * requirement. */
        var downX = 0f
        var downY = 0f
        var gestureFired = false
        var armed: GestureKind? = null   // SWIPE_UP / SWIPE_DOWN, commit on UP
        var wordRepeat: Runnable? = null
        val threshold = dp(60).toFloat()   // provisional (KP14); ~two key widths
        val jitter = dp(10).toFloat()      // provisional: hold-cancelling movement
        val returnRadius = dp(12).toFloat() // provisional: return-to-cancel radius
        val longPressTimeout = android.view.ViewConfiguration.getLongPressTimeout().toLong()
        fun stopWordRepeat() {
            wordRepeat?.let { handler.removeCallbacks(it) }
            wordRepeat = null
        }
        val longPress = Runnable {
            if (gestureFired || armed != null || !frame.isAttachedToWindow) return@Runnable
            if (KeypadModel.gestureAction(key.kind, key.id, GestureKind.LONG_PRESS)
                == GestureAction.WORD_DELETE_REPEAT) {
                gestureFired = true
                onWordDelete?.invoke()   // first tick at the timeout
                val tick = object : Runnable {
                    override fun run() {
                        if (!frame.isAttachedToWindow) return
                        onWordDelete?.invoke()
                        handler.postDelayed(this, KeypadModel.WORD_REPEAT_INTERVAL_MS)
                    }
                }
                wordRepeat = tick
                handler.postDelayed(tick, KeypadModel.WORD_REPEAT_INTERVAL_MS)
            } else if (KeypadModel.gestureAction(key.kind, key.id, GestureKind.LONG_PRESS)
                != GestureAction.NONE) {
                gestureFired = true
                fireGesture(key, GestureKind.LONG_PRESS, frame)
            }
        }
        frame.setOnTouchListener { v, ev ->
            when (ev.actionMasked) {
                android.view.MotionEvent.ACTION_DOWN -> {
                    downX = ev.x; downY = ev.y
                    gestureFired = false; armed = null
                    handler.postDelayed(longPress, longPressTimeout)
                    true
                }
                android.view.MotionEvent.ACTION_MOVE -> {
                    val dx = ev.x - downX; val dy = ev.y - downY
                    if (!gestureFired) {
                        if (KeypadModel.isSwipeLeftDelete(dx, dy, threshold)) {
                            gestureFired = true
                            handler.removeCallbacks(longPress)
                            onSwipeLeftDelete?.invoke()
                        } else if (KeypadModel.isSwipeUp(dx, dy, threshold)) {
                            if (armed == null) handler.removeCallbacks(longPress)
                            armed = GestureKind.SWIPE_UP   // commit on UP (KP5)
                        } else if (KeypadModel.isSwipeDown(dx, dy, threshold)) {
                            if (armed == null) handler.removeCallbacks(longPress)
                            armed = GestureKind.SWIPE_DOWN
                        } else if (armed != null && KeypadModel.returnedToOrigin(dx, dy, returnRadius)) {
                            armed = null      // aborted: back to a tap
                        }
                        if (kotlin.math.abs(dx) > jitter || kotlin.math.abs(dy) > jitter) {
                            handler.removeCallbacks(longPress)   // moved, not held
                        }
                    }
                    true
                }
                android.view.MotionEvent.ACTION_UP -> {
                    handler.removeCallbacks(longPress)
                    stopWordRepeat()
                    val a = armed
                    if (a != null && !gestureFired) {
                        fireGesture(key, a, frame)
                    } else if (!gestureFired) {
                        v.performClick()
                    }
                    true
                }
                else -> {   // CANCEL: neither type nor gesture
                    handler.removeCallbacks(longPress)
                    stopWordRepeat()
                    true
                }
            }
        }
        return frame
    }

    /**
     * The exception table's action for a recognised gesture (KP7/KP10).
     * WORD_DELETE_REPEAT is handled by the long-press timer, not here; NONE
     * cells are open questions, not verdicts.
     */
    private fun fireGesture(key: KeypadKey, gesture: GestureKind, anchor: View) {
        when (KeypadModel.gestureAction(key.kind, key.id, gesture)) {
            GestureAction.SHIFTED_KEY -> fireShifted(key)
            GestureAction.SYMBOL_PICKER -> showPicker(anchor)
            GestureAction.CAPSLOCK -> { state.capsLock(); rebuild() }
            GestureAction.NEXT_SYMBOL_SET -> {
                state.cycleSymbolSet(forward = true, available = availableSets())
                rebuild()
            }
            GestureAction.PREV_SYMBOL_SET -> {
                state.cycleSymbolSet(forward = false, available = availableSets())
                rebuild()
            }
            else -> Unit
        }
    }

    /**
     * KP7's gesture result: the shifted form, as a PER-EVENT shift bit — the
     * one-shot state is untouched (the gesture is shift+key, not the soft
     * arming). The frequent keys' alternate is their hint (? "), the same
     * KP16 rule a shifted tap follows, committed through the symbol path.
     */
    private fun fireShifted(key: KeypadKey) {
        val alternate = KeypadModel.shiftedFrequentAlternate(key, symbols)
            ?: KeypadModel.shiftedDigitAlternate(key)
        if (alternate != null) {
            onSymbol?.invoke(alternate)
        } else {
            onCharacterKey?.invoke(key.keyCode, KeypadModel.META_SHIFT_ON)
        }
    }

    private fun tap(key: KeypadKey) {
        when (key.kind) {
            KeypadKey.Kind.CHARACTER, KeypadKey.Kind.DIGIT -> {
                // The one-shot shift rides the metaState, exactly as a hardware
                // Shift+key does; consuming it re-renders the plain labels.
                val meta = state.shiftMeta()
                val alternate = if (meta != 0) {
                    KeypadModel.shiftedFrequentAlternate(key, symbols)
                        ?: KeypadModel.shiftedDigitAlternate(key)
                } else null
                if (alternate != null) {
                    // The frequent keys' shift alternate is their hint (? "),
                    // not the app keymap's physical shift (< >) — measured bug.
                    onSymbol?.invoke(alternate)
                } else {
                    onCharacterKey?.invoke(key.keyCode, meta)
                }
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
                if (state.page != KeypadState.Page.TEXT) {
                    // 한/영 on either symbol page: return to the mode the pages
                    // were entered from (KP9).
                    val hangul = state.returnToText()
                    onReturnToText?.invoke(hangul)
                    rebuild()
                } else {
                    onToggleLanguage?.invoke()
                }
            KeypadKey.Kind.SYMBOLS_PAGE -> when (state.page) {
                KeypadState.Page.TEXT -> {
                    if (symbols == null) return  // no asset: the page cannot be drawn
                    state.enterSymbols(hangulMode)
                    rebuild()
                }
                // 기호 on its own page: tap stays (the gesture opens the picker).
                KeypadState.Page.SYMBOLS -> Unit
                // 기호 1 on any other symbol page: back to Tier 1 (KP9's mode row).
                else -> {
                    state.returnToSymbols()
                    rebuild()
                }
            }
            KeypadKey.Kind.SYMBOL -> onSymbol?.invoke(key.id)
        }
    }

    /**
     * KP10's page picker (DECIDED 2026-11-23: "페이지 선택은 기호 키 위에
     * 팝업하자"): a popup ABOVE the 기호 key, one labelled item per page that
     * has data. Data-driven — the math/emoji/kaomoji items appear when their
     * inventories are ruled and shipped, not as disabled placeholders. Tap an
     * item to open its page; tap outside to dismiss (non-focusable popup,
     * outside-touch interceptor — KP5's abort path; a focusable popup would
     * steal the IME's input focus and reset the mode, measured).
     */
    private fun showPicker(anchor: View) {
        val pages = buildList {
            if (extended != null) add("확장 기호" to { state.enterSymbolPage(KeypadState.Page.EXTENDED, hangulMode); rebuild() })
            if (math != null) add("수식 기호" to { state.enterSymbolPage(KeypadState.Page.MATH, hangulMode); rebuild() })
            if (emoji != null) add("이모지" to { state.enterSymbolPage(KeypadState.Page.EMOJI, hangulMode); rebuild() })
            if (kaomoji != null) add("카모지" to { state.enterSymbolPage(KeypadState.Page.KAOMOJI, hangulMode); rebuild() })
        }
        if (pages.isEmpty()) return
        val list = LinearLayout(context)
        list.orientation = VERTICAL
        list.background = GradientDrawable().apply {
            cornerRadius = dp(8).toFloat()
            setColor(keyBg)
            setStroke(dp(1), keyBorder)
        }
        /* NON-focusable, the standard IME popup pattern: a focusable popup
         * steals the IME window's focus and can restart the input session.
         * (The mode bug measured in the picker round was the stale entry
         * memory in KeypadState.enterExtended, not this — recorded so the
         * two are not confused.) Outside touches dismiss via the interceptor
         * — KP5's abort path. */
        val popup = PopupWindow(list, kotlin.math.max(dp(160), anchor.width),
            LinearLayout.LayoutParams.WRAP_CONTENT, false)
        popup.isOutsideTouchable = true
        popup.setBackgroundDrawable(android.graphics.drawable.ColorDrawable(Color.TRANSPARENT))
        popup.elevation = dp(8).toFloat()
        popup.setTouchInterceptor { _, ev ->
            if (ev.actionMasked == android.view.MotionEvent.ACTION_OUTSIDE) {
                popup.dismiss(); true
            } else false
        }
        for ((label, open) in pages) {
            val item = TextView(context)
            item.text = label
            item.setTextColor(ink)
            item.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15f)
            item.paint.isFakeBoldText = true
            item.gravity = Gravity.CENTER
            item.setPadding(dp(16), dp(12), dp(16), dp(12))
            item.setOnClickListener { popup.dismiss(); open() }
            list.addView(item, LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT))
        }
        list.measure(
            View.MeasureSpec.makeMeasureSpec(popup.width, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
        // Above the anchor: the 기호 key sits on the bottom row, so the popup
        // opens upward over the row above (its known, recorded cost).
        popup.showAsDropDown(anchor, 0, -(anchor.height + list.measuredHeight))
    }

    private fun dp(v: Int): Int =
        (v * resources.displayMetrics.density + 0.5f).toInt()
}