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
            return KeypadKeycaps(plain, shifted)
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
) {
    enum class Kind {
        /** A layout key (or an always-drawn digit / frequent symbol): engine path. */
        CHARACTER,
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
            Kind.CHARACTER -> "key:$id"
            Kind.FUNCTION -> "fn:$id"
            Kind.SHIFT -> "fn:shift"
            Kind.MODE -> "fn:language-toggle"
            Kind.SYMBOLS_PAGE -> "fn:symbols-page"
            Kind.SYMBOL -> "symbol:$id"
        }
}

/**
 * The grid and its state, pure: rows of [KeypadKey] for a text mode or the
 * symbol page, and the one-shot shift / page memory a tap consults.
 *
 * Geometry is PROVISIONAL (docs/ANDROID-KEYPAD-MOCKUP.md status) and generic:
 * the physical US positions, a cell drawn when the active layout defines that
 * key. Digits are always drawn (an unmapped digit is the classifier's
 * boundary-and-pass, so it inserts itself exactly as the hardware key does).
 * `,` and `.` live in the bottom row in EVERY text mode (KP4) and are never
 * duplicated at their physical row-3 position: when the layout defines them,
 * the bottom-row key IS that layout key (KP9: one key, one position, every mode).
 */
object KeypadModel {

    /**
     * AOSP META_SHIFT_ON, the bit a shifted tap carries. javap -constants
     * against android.jar (API 35), the same verification HunminKeyRegistry
     * documents; KeypadModelTest cross-checks it against the registry's own
     * isShiftOn so the two cannot drift.
     */
    const val META_SHIFT_ON = 0x00000001

    /* Physical US positions. ` and - = lead/trail the number row; [ ] \ the
     * top row; ; ' the home row; / the bottom row. , . are NOT here — they are
     * the bottom-row frequent keys (KP4), in every mode. */
    private val NUMBER_ROW = listOf("`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=")
    private val TOP_ROW = listOf("q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\")
    private val HOME_ROW = listOf("a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'")
    private val BOTTOM_ROW = listOf("z", "x", "c", "v", "b", "n", "m", "/")

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
     * The text-mode grid for [caps]: number row, three letter rows (shift and
     * backspace on the third, KP2), and the bottom row
     * `[한/영] [기호] [space] , . [⏎]` (the mockup's order).
     */
    fun textRows(caps: KeypadKeycaps, shift: Boolean): List<List<KeypadKey>> {
        fun cell(id: String): KeypadKey =
            KeypadKey(id, caps.label(id, shift), KeypadKey.Kind.CHARACTER, keyCodeFor(id))

        val rows = mutableListOf<List<KeypadKey>>()
        // Digits always; the rest of the number row only when the layout keys it.
        rows.add(NUMBER_ROW.mapNotNull { id ->
            if (id in caps.ids || id[0].isDigit()) cell(id) else null
        })
        rows.add(TOP_ROW.mapNotNull { id -> if (id in caps.ids) cell(id) else null })
        rows.add(HOME_ROW.mapNotNull { id -> if (id in caps.ids) cell(id) else null })
        rows.add(
            listOf(KeypadKey("shift", "⇧", KeypadKey.Kind.SHIFT, -1)) +
                BOTTOM_ROW.mapNotNull { id -> if (id in caps.ids) cell(id) else null } +
                listOf(KeypadKey("backspace", "⌫", KeypadKey.Kind.FUNCTION, FUNCTION_KEYCODES.getValue("backspace")))
        )
        rows.add(
            listOf(
                KeypadKey("language-toggle", "한/영", KeypadKey.Kind.MODE, -1),
                KeypadKey("symbols-page", "기호", KeypadKey.Kind.SYMBOLS_PAGE, -1),
                KeypadKey("space", "␣", KeypadKey.Kind.FUNCTION, FUNCTION_KEYCODES.getValue("space")),
                cell(","),
                cell("."),
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
                        if (id == "language-toggle") {
                            KeypadKey(id, "한/영", KeypadKey.Kind.MODE, -1)
                        } else {
                            KeypadKey(
                                id, functionLabel(id), KeypadKey.Kind.FUNCTION,
                                functionKeyCode(id)
                                    ?: throw IllegalArgumentException("unknown function id: $id"),
                            )
                        }
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

/**
 * The keypad's interaction state: one-shot shift and the symbol-page memory.
 * Pure so tier E1 asserts the semantics without a view; KeypadView holds one.
 *
 * Shift is ONE-SHOT (#285's minimum, KP2's single instance): tap arms, the next
 * CHARACTER tap consumes, a second tap cancels, function keys do not consume.
 * No capslock — the state machine beyond one-shot is open (KP7).
 */
class KeypadState {
    enum class Page { TEXT, SYMBOLS }

    var page: Page = Page.TEXT
        private set
    var shiftArmed: Boolean = false
        private set

    /** The mode the symbol page was entered from (KP9's return-to-original-mode). */
    var symbolsEnteredHangul: Boolean = true
        private set

    fun tapShift() {
        shiftArmed = !shiftArmed
    }

    /** The metaState a CHARACTER tap carries; consumes the one-shot. */
    fun shiftMeta(): Int {
        val meta = if (shiftArmed) KeypadModel.META_SHIFT_ON else 0
        shiftArmed = false
        return meta
    }

    fun enterSymbols(hangulMode: Boolean) {
        page = Page.SYMBOLS
        symbolsEnteredHangul = hangulMode
        shiftArmed = false
    }

    /** Leave the symbol page; returns the mode to restore (KP9). */
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
        if (page == Page.SYMBOLS && hangulMode != symbolsEnteredHangul) page = Page.TEXT
    }
}