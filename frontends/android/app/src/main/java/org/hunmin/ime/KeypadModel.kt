/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * KeypadModel — the soft keypad as data and state, no pixels (issue #299).
 *
 * docs/DESIGN-ANDROID-KEYPAD.md §1: three seams keep the frontend thin (FE11).
 * This file is the two that are pure Kotlin — keycap labels/rows derived from
 * the layout's own JSON, and the one-shot shift / page state — so tier E1 can
 * drive every decision here on a plain JVM. KeypadView renders what this file
 * answers; the service wires taps into the SAME onKeyDown path hardware uses.
 *
 * What is deliberately absent:
 *  - No per-layout branch (FE12). Which cells the grid draws is answered by the
 *    layout's own key set; the geometry is the physical US positions, one for
 *    every layout. Adding a layout adds a JSON file, not code (AGENTS.md's one
 *    idea, on the presentation side).
 *  - No composition logic. A character keycap is a (keyCode, metaState) pair;
 *    what it composes is the engine's answer, exactly as for a hardware key.
 *  - No gesture variants (KP5/KP7/KP10): long-press/swipe symbol selection is
 *    #312, on the same app-read data home (KP11).
 */
package org.hunmin.ime

import org.json.JSONObject
import java.text.Normalizer

/**
 * Keycap labels for one layout, parsed from the engine's own layout JSON.
 *
 * The app reads the SAME file the engine loads (installed by
 * HunminDataInstaller) — presentation only, no second copy of the data.
 * A literal value `=x` displays as `x`; a conjoining jamo displays as its
 * compatibility form, because U+1100-block jamo render as invisible or
 * positional forms in most fonts. That display map is DERIVED, not
 * transcribed, from the Unicode data the JDK carries:
 *
 *  - choseong/jungseong: every compatibility jamo U+3131..U+318E decomposes
 *    under NFKD to its conjoining source. (NFD was the first draft's mistake,
 *    measured before shipping: the decomposition is `<compat>`, which only
 *    NFKD applies — an NFD-built table is empty.)
 *  - jongseong: NO decomposition exists — the compat letter ㄱ decomposes to
 *    the CHOSEONG ᄀ, not the jongseong ᆨ. The correspondence is the letter
 *    identity, so it is matched by Unicode character name: HANGUL JONGSEONG
 *    GIEOK ↔ HANGUL LETTER GIEOK. The same name match covers any extended
 *    jamo the NFKD pass misses.
 *
 * It is Unicode, not layout behaviour — the same for every layout. A jamo
 * with no compatibility form at all (rare old-Hangul letters) displays raw;
 * the Track 3 layouts are measured fully covered (KeypadModelTest).
 *
 * A `case` value (SPEC-SCHEMA's condition-resolved primitive, sin3-p2's tier
 * keys) displays its DEFAULT branch (`else`, falling back to `then`): what the
 * key types with no tier state active is what a keycap at rest should show.
 * The engine still resolves the real value per keystroke; this is presentation.
 */
class KeypadKeycaps private constructor(
    private val plain: Map<String, String>,
    private val shifted: Map<String, String>,
    /** The layout's `mode` (SPEC-SCHEMA): hangul or latin. Drives the picture's
     *  shifted-jamo sublabels, which the Hangul screen draws and the English
     *  screen does not. */
    val mode: String,
) {
    /** Every key id the layout defines. */
    val ids: Set<String> get() = plain.keys

    /** The keycap label for [id] on the given row; the id itself when unknown. */
    fun label(id: String, shift: Boolean): String {
        val raw = (if (shift) shifted[id] else plain[id]) ?: return id
        if (raw.isEmpty()) return id
        return display(raw)
    }

    companion object {
        /** Parse the `keys` object of a layout file. Throws on a malformed one. */
        fun parse(json: String): KeypadKeycaps {
            val root = JSONObject(json)
            val keys = root.optJSONObject("keys")
                ?: throw IllegalArgumentException("layout file has no keys object")
            val plain = HashMap<String, String>()
            val shifted = HashMap<String, String>()
            for (id in keys.keys()) {
                val key = keys.getJSONObject(id)
                plain[id] = defaultValue(key.opt("plain")) ?: ""
                shifted[id] = defaultValue(key.opt("shift")) ?: plain[id]!!
            }
            require(plain.isNotEmpty()) { "layout file defines no keys" }
            return KeypadKeycaps(plain, shifted, root.optString("mode", "hangul"))
        }

        /**
         * The display value of a key primitive: a string as-is, a `case` tree
         * by its default branch, anything else (noop) as null — the keycap
         * then falls back to the key id.
         */
        private fun defaultValue(v: Any?): String? = when (v) {
            is String -> v
            is JSONObject -> {
                val case = v.optJSONObject("case")
                if (case == null) null
                else defaultValue(case.opt("else")) ?: defaultValue(case.opt("then"))
            }
            else -> null
        }

        private val JAMO_DISPLAY: Map<Char, Char> = buildMap {
            // Pass 1: NFKD of the compatibility block gives choseong/jungseong;
            // the letter NAMES give the jongseong correspondence pass 2 needs.
            val letters = HashMap<String, Char>()
            for (cp in 0x3131..0x318E) {
                val compat = cp.toChar()
                val nfkd = Normalizer.normalize(compat.toString(), Normalizer.Form.NFKD)
                if (nfkd.length == 1 && nfkd[0].code in 0x1100..0x11FF) put(nfkd[0], compat)
                val name = Character.getName(cp) ?: continue
                if (name.startsWith("HANGUL LETTER ")) {
                    letters[name.removePrefix("HANGUL LETTER ")] = compat
                }
            }
            // Pass 2: jongseong has no decomposition; match the letter identity
            // by Unicode name. Never overrides a pass-1 mapping.
            for (cp in 0x1100..0x11FF) {
                val ch = cp.toChar()
                if (ch in this) continue
                val name = Character.getName(cp) ?: continue
                val suffix = when {
                    name.startsWith("HANGUL JONGSEONG ") -> name.removePrefix("HANGUL JONGSEONG ")
                    name.startsWith("HANGUL CHOSEONG ") -> name.removePrefix("HANGUL CHOSEONG ")
                    name.startsWith("HANGUL JUNGSEONG ") -> name.removePrefix("HANGUL JUNGSEONG ")
                    else -> continue
                }
                letters[suffix]?.let { put(ch, it) }
            }
        }

        /** A raw layout value as a keycap label: literal marker off, jamo displayed. */
        internal fun display(raw: String): String {
            if (raw.startsWith("=")) return raw.substring(1)
            return raw.map { JAMO_DISPLAY[it] ?: it }.joinToString("")
        }
    }
}

/**
 * One keycap. [keyCode] is the AOSP keycode a tap delivers through the shared
 * key path, or -1 for the kinds that are not key events (mode toggle, page
 * switch, direct symbol commit — KP11).
 */
data class KeypadKey(
    val id: String,
    val label: String,
    val kind: Kind,
    val keyCode: Int,
    /** Corner hint, top-right (the mockup's 롱프레스/swipe-up 기호 힌트). Drawn
     *  as the picture draws it; the gestures themselves are #312 (KP13). */
    val hint: String? = null,
    /** Sublabel, bottom-left (the mockup's shifted-jamo small print). */
    val sub: String? = null,
) {
    enum class Kind {
        /** A layout key (or a frequent symbol): engine path, 진하게. */
        CHARACTER,
        /** A number-row digit: engine path, 연하게 (the picture's 숫자 연하게). */
        DIGIT,
        /** A named key the engine declines (space, enter, backspace, navigation). */
        FUNCTION,
        /** The one shift (KP2): arms the one-shot, never a key event itself. */
        SHIFT,
        /** 한/영: toggle on the text page, return-to-original-mode on the symbol page. */
        MODE,
        /** 기호: enter the symbol page. */
        SYMBOLS_PAGE,
        /** A symbol-page character: direct commit, engine uninvolved (KP11). */
        SYMBOL,
    }

    /**
     * The uiautomator content-desc the device tier taps by. Key IDs, not labels:
     * a label is a display choice (jamo today, something else tomorrow), the id
     * is the key's identity across modes and layouts.
     */
    val contentDesc: String
        get() = when (kind) {
            Kind.CHARACTER, Kind.DIGIT -> "key:$id"
            Kind.FUNCTION -> "fn:$id"
            Kind.SHIFT -> "fn:shift"
            Kind.MODE -> "fn:language-toggle"
            Kind.SYMBOLS_PAGE -> "fn:$id"   // symbols-page (기호) / symbols-tier1 (기호 1)
            Kind.SYMBOL -> "symbol:$id"
        }
}

/**
 * The grid and its state, pure: rows of [KeypadKey] for a text mode or the
 * symbol page, and the one-shot shift / page memory a tap consults.
 *
 * Geometry is the DECIDED mockup picture (KP13, maintainer 2026-11-22:
 * "docs/android-keypad-mockup.html 로 정했잖아?") — NOT a generic physical-position
 * grid (the first implementation's deviation, overturned). The picture draws:
 * a 10-key number row (digits only, shifted-symbol corner hints), 10/9/7 letter
 * rows, one ⇧ (⇪ hint) and ⌫ on the third, and the bottom row
 * `[한/영] [기호] ,<?> [space] .<"> [⏎]` (KP15). Labels come from the layout data;
 * the Hangul screen is 두벌식 and the English screen QWERTY (Track 3's goal
 * layouts). A layout key outside these cells (sin3-p2's punctuation) is not
 * touch-reachable through this geometry — a recorded consequence of the
 * decided picture, not a bug to silently "fix" by adding cells back.
 *
 * Digits are always drawn (an unmapped digit is the classifier's
 * boundary-and-pass, so it inserts itself exactly as the hardware key does).
 * `,` and `.` live in the bottom row in EVERY text mode (KP4/KP9): when the
 * layout defines them, the bottom-row key IS that layout key.
 */
object KeypadModel {

    /**
     * AOSP META_SHIFT_ON, the bit a shifted tap carries. javap -constants
     * against android.jar (API 35), the same verification HunminKeyRegistry
     * documents; KeypadModelTest cross-checks it against the registry's own
     * isShiftOn so the two cannot drift.
     */
    const val META_SHIFT_ON = 0x00000001

    /* The picture's rows (docs/android-keypad-mockup.html, KP13). Digits 1–0
     * in the picture's order; letters at their QWERTY positions, 10/9/7. No
     * extension cells: the picture's number row has no ` - =, its letter rows
     * no [ ] \ ; ' /. */
    private val NUMBER_ROW = listOf("1", "2", "3", "4", "5", "6", "7", "8", "9", "0")
    private val TOP_ROW = listOf("q", "w", "e", "r", "t", "y", "u", "i", "o", "p")
    private val HOME_ROW = listOf("a", "s", "d", "f", "g", "h", "j", "k", "l")
    private val BOTTOM_ROW = listOf("z", "x", "c", "v", "b", "n", "m")

    /** The frequent keys' hinted alternates (KP16): one source for the corner
     *  hint AND the shift result — the armed keycap folds the hint into its
     *  label, so the decision must not read the keycap's current hint. */
    private val FREQUENT_HINTS = mapOf("," to "?", "." to "\"")

    /** The digit's shifted-symbol corner hint, from the registry's own shift
     *  column (the picture's 1<!> … 0<)>); derived, not transcribed. */
    private val DIGIT_HINTS: Map<String, String> = buildMap {
        for (key in HunminKeyRegistry.all()) {
            if (key.unshifted.isDigit() && key.shifted != null) {
                put(key.unshifted.toString(), key.shifted.toString())
            }
        }
    }

    /**
     * AOSP keycodes for the named keys, javap -constants against android.jar
     * (API 35, verified 2026-11-23). The navigation keys are deliberately NOT
     * in HunminKeyRegistry: no layout keys them and the engine has no meaning
     * to give them, so the registry refuses them — the keypad sends them
     * through the same onKeyDown path, where the classifier's unmapped rule
     * delivers boundary-then-pass to the app, exactly as the hardware keys.
     */
    private val FUNCTION_KEYCODES = mapOf(
        "space" to 62,        // KEYCODE_SPACE
        "enter" to 66,        // KEYCODE_ENTER
        "backspace" to 67,    // KEYCODE_DEL
        "tab" to 61,          // KEYCODE_TAB
        "home" to 122,        // KEYCODE_MOVE_HOME
        "end" to 123,         // KEYCODE_MOVE_END
        "page-up" to 92,      // KEYCODE_PAGE_UP
        "page-down" to 93,    // KEYCODE_PAGE_DOWN
        "up" to 19,           // KEYCODE_DPAD_UP
        "down" to 20,         // KEYCODE_DPAD_DOWN
        "left" to 21,         // KEYCODE_DPAD_LEFT
        "right" to 22,        // KEYCODE_DPAD_RIGHT
    )

    /** The AOSP keycode for a layout key id (its unshifted US-QWERTY character). */
    fun keyCodeFor(id: String): Int {
        require(id.length == 1) { "a layout key id is one character: $id" }
        val ch = id[0]
        return HunminKeyRegistry.all().firstOrNull { it.unshifted == ch }?.keyCode
            ?: throw IllegalArgumentException("no AOSP keycode for layout key '$id'")
    }

    /** The keycode a symbol-page function id delivers, or null for 한/영 (a mode action). */
    fun functionKeyCode(id: String): Int? = FUNCTION_KEYCODES[id]

    /**
     * The shifted alternate of a FREQUENT punctuation key (`, .`): its drawn
     * hint (? and "), when the symbol page can commit it. Bug fix measured
     * 2026-11-22 (maintainer: "시프트 입력을 해보려고 했는데, <>가 각각
     * 입력되네?"): an unmapped shifted key passed to the app is resolved by
     * the app's KeyCharacterMap — US physical Shift+, is <, not the ? the
     * hint advertises. The frequent keys' touch-shift alternate is therefore
     * the hint (KP7's direction: the alternate is the same character whether
     * reached by shift or swipe), committed through the symbol path (flush
     * first). Digits keep the engine/app path: their physical shift already
     * matches the hint (! @ # …). Hardware keys are untouched.
     */
    fun shiftedFrequentAlternate(key: KeypadKey, symbols: SymbolScreen?): String? {
        val hint = FREQUENT_HINTS[key.id] ?: return null
        return if (symbols != null && hint in symbols.symbols) hint else null
    }

    /**
     * The shifted alternate of a DIGIT key: its drawn corner hint (! @ # …),
     * committed through the symbol path like the frequent keys' alternate.
     * MEASURED 2026-11-23 (#312 gesture round): in Hangul mode the soft 두벌식
     * pair has no digit keys, so the classifier's boundary-and-pass delivers
     * the bare digit and DROPS the shift bit — shift+1 typed '1', not the '!'
     * the corner hint promises (KP16's hint-is-a-promise rule). The English
     * pair's shift row already matches the hint; routing digits through the
     * hint makes both modes keep the promise. Hardware keys are untouched.
     */
    fun shiftedDigitAlternate(key: KeypadKey): String? =
        if (key.kind == KeypadKey.Kind.DIGIT) DIGIT_HINTS[key.id] else null

    /**
     * KP17 (maintainer 2026-11-22: "소프트 키보드는 두벌식만 쓸건데?"): the
     * soft keypad's Hangul mode is 두벌식 and its English mode QWERTY,
     * regardless of the layout setting — the setting governs the hardware
     * session. The soft keypad has its OWN engine pair built from these
     * constants; the two implementations are independent (maintainer:
     * "소프트 키보드와 물리 키보드 구현은 전혀 관계가 없어야 하는데...").
     */
    const val SOFT_HANGUL_LAYOUT = "dubeolsik"
    const val SOFT_LATIN_LAYOUT = "us-qwerty"

    /**
     * KP14's gesture decision, pure: a swipe-left deletes when it is LONG
     * (leftward distance at least [thresholdPx]) and horizontally dominant
     * (at least twice the vertical drift) — a tap that drifts up or down a
     * little is still a tap (KP5's accidental-gesture protection). The
     * threshold is provisional; the decision's shape is the requirement.
     */
    fun isSwipeLeftDelete(dx: Float, dy: Float, thresholdPx: Float): Boolean =
        dx <= -thresholdPx && -dx >= 2f * kotlin.math.abs(dy)

    /**
     * KP7's swipe-up gate, pure — the mirror of [isSwipeLeftDelete]: upward
     * distance at least [thresholdPx] and vertically dominant (at least twice
     * the horizontal drift). A tap that drifts sideways a little is still a
     * tap (KP5). Recognition only; the commit is on release, and a return to
     * the origin cancels ([returnedToOrigin]) — the layered defences recorded
     * in docs/ANDROID-SYMBOL-INTERACTION.md §Avoiding the accidental swipe.
     * The threshold is provisional, shared with KP14's.
     */
    fun isSwipeUp(dx: Float, dy: Float, thresholdPx: Float): Boolean =
        dy <= -thresholdPx && -dy >= 2f * kotlin.math.abs(dx)

    /** KP10's symbol-screen swipe-down gate: the mirror of [isSwipeUp]. */
    fun isSwipeDown(dx: Float, dy: Float, thresholdPx: Float): Boolean =
        dy >= thresholdPx && dy >= 2f * kotlin.math.abs(dx)

    /**
     * KP5's return-to-cancel: the pointer is back within [radiusPx] of the
     * origin key, so a begun swipe-up is aborted. Provisional radius.
     */
    fun returnedToOrigin(dx: Float, dy: Float, radiusPx: Float): Boolean =
        kotlin.math.abs(dx) <= radiusPx && kotlin.math.abs(dy) <= radiusPx

    /**
     * KP14's backspace long-press repeat interval: the maintainer's estimate
     * (2026-11-23, verbatim in SPEC KP14: "0.2초 정도 간격으로 단어 삭제를
     * 반복하면 될 것 같아") — a provisional tuning constant, not a measured
     * optimum.
     */
    const val WORD_REPEAT_INTERVAL_MS = 200L

    /**
     * The long-press hold timeout. PROVISIONAL tuning (maintainer 2026-11-26:
     * "long-press가 너무 긴데. 200ms 이하로 테스트해보고 싶어") — the system
     * value (ViewConfiguration.getLongPressTimeout(), 500 ms on the test
     * phone, 400 ms on the AVD) was too slow in use. 200 ms is the upper bound
     * the maintainer named, the starting point of the experiment.
     *
     * The trade-off to watch in the evaluation: a SLOW swipe that has not yet
     * passed the distance gate can fire long-press first (the movement-vs-time
     * race); the jitter cancel (movement > 10 dp kills the hold) is the
     * defence. If the evaluation shows slow swipes becoming shifts, the gate
     * or this timeout moves — recorded, not decided.
     */
    const val LONG_PRESS_TIMEOUT_MS = 200L

    /**
     * The settings window's choices for the hold timeout (maintainer
     * 2026-11-26: "설정할 수 있게 하는 게 제일 좋아"). Covers the "200ms 이하"
     * experiment and slower values; the default [LONG_PRESS_TIMEOUT_MS] is
     * among them.
     */
    val LONG_PRESS_CHOICES_MS = listOf(100L, 150L, 200L, 300L, 400L, 500L)

    /**
     * The per-key gesture exception table (KP7, maintainer 2026-11-23:
     * "long press와 swipe up은 일반적으로 동일하지만, 기호 키나, backspace 등
     * 일부 키에서 동작이 달라지는 거야"). The GENERAL rule is equivalence —
     * both gestures reach [GestureAction.SHIFTED_KEY] on character keys — and
     * the exceptions are recorded per-key facts: the 기호 mode key opens the
     * page picker (KP10), backspace long-press repeats word deletion (KP14).
     *
     * [GestureAction.NONE] cells are OPEN, not verdicts: shift→capslock (the
     * shift state machine is open, KP7), swipe-up on backspace (the ruling
     * names long-press), symbol-screen keys (no shift meaning, KP6), and the
     * other function keys. KP14's swipe-left delete is independent of this
     * table — it starts from any keycap.
     */
    fun gestureAction(kind: KeypadKey.Kind, id: String, gesture: GestureKind): GestureAction = when {
        // Swipe-DOWN on text keys and the 기호 key is UNRULED — a NONE cell,
        // not a silent copy of swipe-up (KP8: a direction earns its meaning).
        (kind == KeypadKey.Kind.CHARACTER || kind == KeypadKey.Kind.DIGIT) &&
            gesture != GestureKind.SWIPE_DOWN -> GestureAction.SHIFTED_KEY
        kind == KeypadKey.Kind.SYMBOLS_PAGE && gesture != GestureKind.SWIPE_DOWN ->
            GestureAction.SYMBOL_PICKER
        // KP7 (decided 2026-11-23): shift's gestures lock capslock; a tap
        // while locked releases it (KeypadState.tapShift).
        kind == KeypadKey.Kind.SHIFT && gesture != GestureKind.SWIPE_DOWN ->
            GestureAction.CAPSLOCK
        // KP10 (decided 2026-11-23): on the symbol screen, swipe-up/down move
        // to the next/previous symbol set; long-press follows the equivalence.
        kind == KeypadKey.Kind.SYMBOL -> when (gesture) {
            GestureKind.SWIPE_DOWN -> GestureAction.PREV_SYMBOL_SET
            else -> GestureAction.NEXT_SYMBOL_SET
        }
        kind == KeypadKey.Kind.FUNCTION && id == "backspace" &&
            gesture == GestureKind.LONG_PRESS -> GestureAction.WORD_DELETE_REPEAT
        else -> GestureAction.NONE
    }

    /**
     * The text-mode grid for [caps], as the decided picture draws it (KP13):
     * number row with corner hints, 10/9/7 letter rows (the Hangul screen's
     * shifted-jamo sublabels when the layout's mode is hangul), ⇧/⌫ on the
     * third, and the bottom row `[한/영] [기호] ,<?> [space] .<"> [⏎]` (KP15).
     */
    fun textRows(caps: KeypadKeycaps, shift: Boolean): List<List<KeypadKey>> {
        fun letter(id: String): KeypadKey {
            val plainLabel = caps.label(id, shift = false)
            val shiftLabel = caps.label(id, shift = true)
            // The picture draws the shifted jamo small print on the Hangul
            // screen only; while shift is armed the shifted form IS the label.
            val sub = if (!shift && caps.mode == "hangul" && shiftLabel != plainLabel) shiftLabel else null
            return KeypadKey(id, caps.label(id, shift), KeypadKey.Kind.CHARACTER, keyCodeFor(id), sub = sub)
        }

        val rows = mutableListOf<List<KeypadKey>>()
        rows.add(NUMBER_ROW.map { id ->
            // Armed shift shows what the tap will type (KP16 display rule):
            // the digit's symbol, with the corner hint folded into the label.
            val hint = DIGIT_HINTS[id]
            KeypadKey(id, if (shift) hint ?: id else id, KeypadKey.Kind.DIGIT,
                keyCodeFor(id), hint = if (shift) null else hint)
        })
        rows.add(TOP_ROW.mapNotNull { id -> if (id in caps.ids) letter(id) else null })
        rows.add(HOME_ROW.mapNotNull { id -> if (id in caps.ids) letter(id) else null })
        rows.add(
            listOf(KeypadKey("shift", "⇧", KeypadKey.Kind.SHIFT, -1, hint = "⇪")) +
                BOTTOM_ROW.mapNotNull { id -> if (id in caps.ids) letter(id) else null } +
                listOf(KeypadKey("backspace", "⌫", KeypadKey.Kind.FUNCTION, FUNCTION_KEYCODES.getValue("backspace")))
        )
        // , . : the layout's own key when it defines one (sin3-p2, us-qwerty),
        // else the classifier's boundary-and-pass inserts the character — the
        // same key event either way. The ? " hints are the picture's KP7
        // swipe affordances; the gestures are #312, the hints are drawn (KP13).
        // KP16 display rule: armed shift shows the alternate the tap will
        // type (? and "), the hint folded into the label — same principle as
        // the jamo keycaps showing ㅃ while armed.
        fun punct(id: String): KeypadKey {
            val hint = FREQUENT_HINTS.getValue(id)
            return KeypadKey(id, if (shift) hint else id, KeypadKey.Kind.CHARACTER,
                keyCodeFor(id), hint = if (shift) null else hint)
        }
        rows.add(
            listOf(
                KeypadKey("language-toggle", "한/영", KeypadKey.Kind.MODE, -1),
                KeypadKey("symbols-page", "기호", KeypadKey.Kind.SYMBOLS_PAGE, -1),
                // KP15: comma LEFT of space — the space sits further right.
                punct(","),
                KeypadKey("space", "space", KeypadKey.Kind.FUNCTION, FUNCTION_KEYCODES.getValue("space")),
                punct("."),
                KeypadKey("enter", "⏎", KeypadKey.Kind.FUNCTION, FUNCTION_KEYCODES.getValue("enter")),
            )
        )
        return rows
    }

    /**
     * The symbol page: the shipped app-read screen (#297), rows verbatim.
     * Symbol keys commit directly (KP11); function ids become key events,
     * except 한/영 which is the return-to-original-mode action (KP9).
     */
    fun symbolRows(screen: SymbolScreen): List<List<KeypadKey>> =
        screen.rows.map { row ->
            row.keys.map { id ->
                when (row.kind) {
                    SymbolScreen.Kind.SYMBOLS ->
                        KeypadKey(id, id, KeypadKey.Kind.SYMBOL, -1)
                    SymbolScreen.Kind.FUNCTION ->
                        when (id) {
                            "language-toggle" -> KeypadKey(id, "한/영", KeypadKey.Kind.MODE, -1)
                            // KP9's mode-key slots (#312): 기호 on the Tier 1 screen
                            // (tap stays, gesture opens the picker), 기호 1 on the
                            // extended page (tap returns to Tier 1). Page actions,
                            // not key events — the view's tap() reads the page.
                            "symbols-page" -> KeypadKey(id, "기호", KeypadKey.Kind.SYMBOLS_PAGE, -1)
                            "symbols-tier1" -> KeypadKey(id, "기호 1", KeypadKey.Kind.SYMBOLS_PAGE, -1)
                            else -> KeypadKey(
                                id, functionLabel(id), KeypadKey.Kind.FUNCTION,
                                functionKeyCode(id)
                                    ?: throw IllegalArgumentException("unknown function id: $id"),
                            )
                        }
                }
            }
        }

    /**
     * KP18's kaomoji page: a full-width LIST, one string per row — a kaomoji
     * is wider than a grid key. Same Kind.SYMBOL commit path (the whole
     * string commits through the flush-then-commit seam, KP11).
     */
    fun kaomojiRows(screen: SymbolScreen): List<List<KeypadKey>> =
        screen.rows.map { row ->
            row.keys.map { id ->
                when (row.kind) {
                    SymbolScreen.Kind.SYMBOLS ->
                        KeypadKey(id, id, KeypadKey.Kind.SYMBOL, -1)
                    SymbolScreen.Kind.FUNCTION ->
                        if (id == "language-toggle") KeypadKey(id, "한/영", KeypadKey.Kind.MODE, -1)
                        else KeypadKey(id, "기호 1", KeypadKey.Kind.SYMBOLS_PAGE, -1)
                }
            }
        }

    private fun functionLabel(id: String): String = when (id) {
        "backspace" -> "⌫"
        "tab" -> "⇥"
        "home" -> "Home"
        "end" -> "End"
        "page-up" -> "PgUp"
        "page-down" -> "PgDn"
        "left" -> "←"
        "up" -> "↑"
        "down" -> "↓"
        "right" -> "→"
        else -> id
    }
}

/**
 * KP12's decision, pure: the soft grid auto-shows only without a physical
 * keyboard (maintainer, 2026-11-22: "이 UI는 물리 키보드가 없을 때만 자동으로 뜨는 거야.").
 *
 * Constants javap -constants against android.jar (API 35, 2026-11-22). Note
 * HARDKEYBOARDHIDDEN_NO is 1, not 0 — the same guess-and-measure failure the
 * modifier bits in HunminKeyRegistry record. [showImeWithHardKeyboard] is the
 * platform's standard user opt-in (Settings.Secure.show_ime_with_hard_keyboard):
 * an EXPLICIT request, not an automatic appearance, so it shows the grid even
 * with a keyboard exposed. The strip is never hidden by this decision — it is
 * the IME window hardware keys arrive through (B1); only the grid's visibility
 * is KP12's subject.
 */
object KeypadVisibility {
    private const val KEYBOARD_NOKEYS = 1          // Configuration.KEYBOARD_NOKEYS
    private const val HARDKEYBOARDHIDDEN_NO = 1    // Configuration.HARDKEYBOARDHIDDEN_NO

    fun autoShown(keyboard: Int, hardKeyboardHidden: Int, showImeWithHardKeyboard: Boolean): Boolean {
        val physicalExposed = keyboard != KEYBOARD_NOKEYS &&
            hardKeyboardHidden == HARDKEYBOARDHIDDEN_NO
        return showImeWithHardKeyboard || !physicalExposed
    }
}

/** The gestures of #312's general-equivalence rule (KP7) and KP10's sets. */
enum class GestureKind { SWIPE_UP, SWIPE_DOWN, LONG_PRESS }

/** What a gesture does on a key — see [KeypadModel.gestureAction]. */
enum class GestureAction {
    SHIFTED_KEY, SYMBOL_PICKER, WORD_DELETE_REPEAT, CAPSLOCK,
    NEXT_SYMBOL_SET, PREV_SYMBOL_SET, NONE
}

/**
 * KP14's delete unit for the long swipe-left gesture (maintainer 2026-11-22:
 * "key단위, 음절단위, 단어단위, 전체 삭제로 설정할 수 있어도 좋겠다").
 * [pref] is the stored value, [label] the settings window's Korean row.
 */
enum class DeleteUnit(val pref: String, val label: String) {
    KEY("key", "키 단위"),
    SYLLABLE("syllable", "음절 단위"),
    WORD("word", "단어 단위"),
    ALL("all", "전체 삭제");

    companion object {
        /** The stored value, defaulting to KEY — the least destructive unit.
         *  Provisional default; the maintainer may choose another (KP14). */
        fun of(pref: String?): DeleteUnit = entries.firstOrNull { it.pref == pref } ?: KEY
    }
}

/**
 * The keypad's interaction state: one-shot shift and the symbol-page memory.
 * Pure so tier E1 asserts the semantics without a view; KeypadView holds one.
 *
 * Shift is ONE-SHOT (#285's minimum, KP2's single instance): tap arms, the next
 * CHARACTER tap consumes, a second tap cancels, function keys do not consume.
 * No capslock — the state machine beyond one-shot is open (KP7).
 */
class KeypadState {
    enum class Page { TEXT, SYMBOLS, EXTENDED, MATH, EMOJI, KAOMOJI }

    var page: Page = Page.TEXT
        private set
    var shiftArmed: Boolean = false
        private set

    /** KP7 (decided 2026-11-23): capslock, locked by shift's gestures. */
    var capsLocked: Boolean = false
        private set

    /** What the keycaps render: one-shot armed OR capslock locked. */
    val shiftActive: Boolean get() = shiftArmed || capsLocked

    /** The mode the symbol page was entered from (KP9's return-to-original-mode). */
    var symbolsEnteredHangul: Boolean = true
        private set

    /**
     * KP7's decided state machine: a tap while LOCKED releases capslock; a
     * tap while unlocked arms/cancels the one-shot (KP2). The gestures lock
     * ([capsLock]); "다시 shift를 누르면 해제" is the tap branch.
     */
    fun tapShift() {
        if (capsLocked) {
            capsLocked = false
        } else {
            shiftArmed = !shiftArmed
        }
    }

    /** Shift's swipe-up/long-press: lock capslock (KP7, decided 2026-11-23). */
    fun capsLock() {
        capsLocked = true
        shiftArmed = false
    }

    /**
     * The metaState a CHARACTER tap carries: capslock holds the bit across
     * keys; the one-shot is consumed by the first character tap.
     */
    fun shiftMeta(): Int {
        if (capsLocked) return KeypadModel.META_SHIFT_ON
        val meta = if (shiftArmed) KeypadModel.META_SHIFT_ON else 0
        shiftArmed = false
        return meta
    }

    /**
     * KP10's symbol-screen set navigation (decided 2026-11-23: "기호 화면에서
     * swipe-up / down으로 다음 기호 조합으로 이동"). Cycles the symbol PAGES
     * that have data, wrapping; a no-op when only one set exists. The reading
     * (pages, not row-groups) is recorded in SPEC KP10 for correction.
     */
    fun cycleSymbolSet(forward: Boolean, available: List<Page>) {
        if (page == Page.TEXT || page !in available) return
        val sets = available.filter { it != Page.TEXT }
        if (sets.size < 2) return
        val i = sets.indexOf(page)
        page = sets[(i + (if (forward) 1 else sets.size - 1)) % sets.size]
    }

    fun enterSymbols(hangulMode: Boolean) {
        page = Page.SYMBOLS
        symbolsEnteredHangul = hangulMode
        shiftArmed = false
        capsLocked = false
    }

    /**
     * KP10's page picker picked the extended page (#312). From the SYMBOL
     * screen the 한/영 return memory (KP9) is kept; from TEXT (the picker on
     * the text keypad's 기호 key) this IS the entry, so the memory is set —
     * MEASURED 2026-11-23: skipping that left the stale default and the
     * return key toggled to 한 instead of restoring 영.
     */
    fun enterExtended(hangulMode: Boolean) = enterSymbolPage(Page.EXTENDED, hangulMode)

    /**
     * KP10's picker opened a symbol page (#312). From TEXT this IS the entry,
     * so the KP9 return memory is set; from a symbol page it is kept.
     */
    fun enterSymbolPage(target: Page, hangulMode: Boolean) {
        require(target != Page.TEXT) { "a symbol page, not TEXT" }
        if (page == Page.TEXT) symbolsEnteredHangul = hangulMode
        page = target
        shiftArmed = false
        capsLocked = false
    }

    /** 기호 1 on the extended page: back to the Tier 1 symbol screen. */
    fun returnToSymbols() {
        page = Page.SYMBOLS
    }

    /** Leave the symbol pages; returns the mode to restore (KP9). */
    fun returnToText(): Boolean {
        page = Page.TEXT
        return symbolsEnteredHangul
    }

    /**
     * The service's mode changed (hardware 한/영, judgement adoption, a new
     * editor). The text grid follows the mode; a symbol page stays only while
     * the mode it was entered from still holds — a mode change under it returns
     * to text, because its return-key memory would be stale.
     */
    fun modeChanged(hangulMode: Boolean) {
        shiftArmed = false
        capsLocked = false
        if (page != Page.TEXT && hangulMode != symbolsEnteredHangul) page = Page.TEXT
    }
}