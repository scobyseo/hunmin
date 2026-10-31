/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * KeyBinding — a configurable IME trigger, stored as numbers.
 *
 * WHY NUMBERS AND NOT NAMES
 * -------------------------
 * The window this replaces stored "ALT_LEFT+Z" and built the string it stored with
 * `KeyEvent.keyCodeToString`, which yields "KEYCODE_Z" — so the two could never be
 * compared, and `keyCodeToString` is a *method*, so it is unavailable in the JVM
 * unit tests of tier E1 (it returns null there). A name-based binding therefore
 * needs either a keycode<->name table in Kotlin (a second copy of AOSP's, free to
 * drift from android.jar) or an untestable branch. `v1:<keyCode>:<modifiers>`
 * needs neither: the numbers are the framework's own, and comparing them is the
 * only thing this file has to get right.
 *
 * WHY THE PURE LAYER HAS NO FRAMEWORK CALLS
 * -----------------------------------------
 * Every KeyEvent constant below is a compile-time `static final int`, so kotlinc
 * inlines the literal into this class and a JVM unit test reads the same value
 * android.jar declares. `keyCodeToString` is not a constant, which is exactly why
 * display formatting lives in OverlayActivity and not here.
 *
 * WHY SIDE-SPECIFIC MODIFIER BITS
 * ------------------------------
 * Android sets both the aggregate (`META_ALT_ON`) and the side bit
 * (`META_ALT_LEFT_ON`) on a real chord, so the aggregate carries no information the
 * side bit does not, and four of the defaults differ *only* by side: Left Alt + Z
 * toggles 한/영 while Right Alt + Enter toggles the auto-switch. Normalising to the
 * side bits therefore makes one stored binding match exactly one chord. When an
 * event says only "Alt" with no side — a synthetic key event, some IMEs on injected
 * input — the aggregate is expanded to both sides rather than dropping the chord,
 * because refusing a chord the user did press is the worse error.
 *
 * CapsLock and Shift are deliberately *not* normalised away: Shift is a layout row
 * (invariant 4 in docs/FRONTEND-CONTRACT.md) and CapsLock is the engine's `caps`
 * option, so a binding may legitimately ask for either. Their live bits
 * (META_SHIFT_ON) are kept; the stuck-state bits are not part of a chord.
 */
package org.hunmin.ime

import android.view.KeyEvent

/** One trigger: a keycode plus the modifiers that must be held with it. */
data class KeyCombo(val keyCode: Int, val modifiers: Int = 0) {
    fun encode(): String = "${KeyBinding.VERSION}:$keyCode:$modifiers"
}

/**
 * Keycodes with no `KeyEvent` constant of their own on the SDKs this builds
 * against, measured with `javap -constants` on android.jar API 35.
 *
 * KANA (218) is the 한/영 key on Samsung keyboards; EISU (212) is AOSP's かな/한글
 * key, and the maintainer named it the fixed 한자 입력 key on 2026-10-11. Both are
 * named constants rather than bare literals because the service used to carry
 * `218` inline, and a bare number in a precedence list is unreadable.
 */
const val KANA_KEYCODE = 218
const val EISU_KEYCODE = 212

object KeyBinding {

    /** Stored-format tag. Bumped only if the shape below ever changes. */
    const val VERSION = "v1"

    // Mirrors android.view.KeyEvent. These are the real values, measured with
    // `javap -constants` on android.jar API 35 (docs/ENVIRONMENT.md); a
    // compile-time constant is inlined, so this stays true on the JVM.
    const val META_SHIFT = 0x00000001        // META_SHIFT_ON
    private const val META_ALT = 0x00000002          // META_ALT_ON
    private const val META_CTRL = 0x00001000         // META_CTRL_ON
    private const val META_META = 0x00010000         // META_META_ON
    const val META_ALT_LEFT = 0x00000010      // META_ALT_LEFT_ON
    const val META_ALT_RIGHT = 0x00000020     // META_ALT_RIGHT_ON
    private const val META_CTRL_LEFT = 0x00002000       // META_CTRL_LEFT_ON
    private const val META_CTRL_RIGHT = 0x00004000      // META_CTRL_RIGHT_ON
    private const val META_META_LEFT = 0x00020000       // META_META_LEFT_ON
    private const val META_META_RIGHT = 0x00040000      // META_META_RIGHT_ON
    private const val META_CAPS_LOCK = 0x00100000       // META_CAPS_LOCK_ON

    /** Modifier bits a binding may carry, and the only ones normalised. */
    private const val SIDE_MASK =
        META_SHIFT or META_ALT_LEFT or META_ALT_RIGHT or
            META_CTRL_LEFT or META_CTRL_RIGHT or
            META_META_LEFT or META_META_RIGHT

    /** Modifier keycodes, so a bare modifier press can be told from a chord. */
    private val MODIFIER_KEYCODES = setOf(
        KeyEvent.KEYCODE_SHIFT_LEFT, KeyEvent.KEYCODE_SHIFT_RIGHT,
        KeyEvent.KEYCODE_ALT_LEFT, KeyEvent.KEYCODE_ALT_RIGHT,
        KeyEvent.KEYCODE_CTRL_LEFT, KeyEvent.KEYCODE_CTRL_RIGHT,
        KeyEvent.KEYCODE_META_LEFT, KeyEvent.KEYCODE_META_RIGHT,
        KeyEvent.KEYCODE_CAPS_LOCK, KeyEvent.KEYCODE_NUM_LOCK, KeyEvent.KEYCODE_SCROLL_LOCK,
    )

    /**
     * The modifier bits of [metaState] that a binding can be built from.
     *
     * Drops the aggregates, CapsLock and the lock-state bits; expands an aggregate
     * with no side bit to both sides rather than dropping the chord.
     */
    fun normalise(metaState: Int): Int {
        var m = metaState and SIDE_MASK
        if (metaState and META_ALT != 0 && (m and (META_ALT_LEFT or META_ALT_RIGHT)) == 0) {
            m = m or META_ALT_LEFT or META_ALT_RIGHT
        }
        if (metaState and META_CTRL != 0 && (m and (META_CTRL_LEFT or META_CTRL_RIGHT)) == 0) {
            m = m or META_CTRL_LEFT or META_CTRL_RIGHT
        }
        if (metaState and META_META != 0 && (m and (META_META_LEFT or META_META_RIGHT)) == 0) {
            m = m or META_META_LEFT or META_META_RIGHT
        }
        return m
    }

    private const val ALT_MASK = META_ALT_LEFT or META_ALT_RIGHT
    private const val CTRL_MASK = META_CTRL_LEFT or META_CTRL_RIGHT
    private const val META_MASK = META_META_LEFT or META_META_RIGHT

    /**
     * Do the modifiers of [event] satisfy the ones [stored] asks for?
     *
     * Compared per family rather than as one mask, because only the
     * left/right families are ambiguous: an event that says "Alt" without a side
     * — a synthetic key event, an IME injecting its own — must satisfy a binding
     * that named either side, because refusing a chord the user did press is the
     * worse error. A binding that asked for *both* sides still requires both.
     *
     * Shift is a single bit with no sides, so it is plain equality.
     */
    fun modifiersMatch(stored: Int, event: Int): Boolean {
        if ((stored and META_SHIFT) != (event and META_SHIFT)) return false
        if (!sideMatches(stored and ALT_MASK, event and ALT_MASK)) return false
        if (!sideMatches(stored and CTRL_MASK, event and CTRL_MASK)) return false
        if (!sideMatches(stored and META_MASK, event and META_MASK)) return false
        return true
    }

    private fun sideMatches(stored: Int, event: Int): Boolean {
        if (stored == 0) return event == 0
        if (event == 0) return false
        return (event and stored) == stored
    }

    /** Is [keyCode] a modifier on its own, i.e. not the key half of a chord? */
    fun isModifierKey(keyCode: Int): Boolean = keyCode in MODIFIER_KEYCODES

    /**
     * The bits [keyCode] implies when it is the *only* key pressed.
     *
     * A binding on a bare modifier stores the key, not the bit: pressing Right Alt
     * arrives as `KEYCODE_ALT_RIGHT` with an empty metaState, and comparing it
     * against `META_ALT_RIGHT` would mean the binding could never match. The
     * platform's own 한/영 alternates are written this way for the same reason.
     */
    fun bareModifierBits(keyCode: Int): Int = when (keyCode) {
        KeyEvent.KEYCODE_ALT_LEFT -> META_ALT_LEFT
        KeyEvent.KEYCODE_ALT_RIGHT -> META_ALT_RIGHT
        KeyEvent.KEYCODE_SHIFT_LEFT, KeyEvent.KEYCODE_SHIFT_RIGHT -> META_SHIFT
        KeyEvent.KEYCODE_CTRL_LEFT -> META_CTRL_LEFT
        KeyEvent.KEYCODE_CTRL_RIGHT -> META_CTRL_RIGHT
        KeyEvent.KEYCODE_META_LEFT -> META_META_LEFT
        KeyEvent.KEYCODE_META_RIGHT -> META_META_RIGHT
        else -> 0
    }

    fun encode(keyCode: Int, metaState: Int): String =
        "$VERSION:$keyCode:${normalise(metaState)}"

    /** The stored chord, or null when nothing valid is stored (use the default). */
    fun decode(stored: String?): KeyCombo? {
        val parts = stored?.split(':') ?: return null
        if (parts.size != 3 || parts[0] != VERSION) return null
        val keyCode = parts[1].toIntOrNull() ?: return null
        val modifiers = parts[2].toIntOrNull() ?: return null
        return KeyCombo(keyCode, modifiers)
    }

    /**
     * Does this key event fire [stored]?
     *
     * An unparseable or absent value never matches: a binding nobody set must not
     * be able to swallow a keystroke.
     */
    fun matches(stored: String?, keyCode: Int, metaState: Int): Boolean {
        val combo = decode(stored) ?: return false
        if (combo.keyCode != keyCode) return false
        return modifiersMatch(combo.modifiers, normalise(metaState))
    }

    /** Is [combo] one of the chords stored for [stored]'s action? */
    fun matchesAny(combos: List<KeyCombo>, keyCode: Int, metaState: Int): Boolean {
        val m = normalise(metaState)
        return combos.any { it.keyCode == keyCode && modifiersMatch(it.modifiers, m) }
    }
}

/**
 * The four triggers the settings window exposes.
 *
 * [defaults] is what HEAD accepted with no preference set, and it is a list
 * because two of them accepted more than one chord: voice fired on both Alt+V
 * sides while the old settings dialog displayed only "ALT_LEFT+V", and the
 * documented trigger (`docs/REMAINING-WORK.md` voice stage 1) says *Right* Alt + V.
 * Narrowing that to one chord to fit a one-slot UI would be a behaviour change
 * smuggled in as a refactor, so both stay and the disagreement is recorded on the
 * issue rather than resolved here.
 *
 * [platformAlternates] are different in kind: they are Android and hardware
 * conventions (bare Right Alt, `KEYCODE_LANGUAGE_SWITCH`, the Samsung keyCode 218,
 * Shift+Alt). They apply whatever the binding says, because a user rebinding 한/영
 * must not end up with a keyboard whose physical 한/영 key does nothing.
 *
 * A stored binding *replaces* [defaults] — that is what rebinding means — and never
 * [platformAlternates].
 */
enum class KeyAction(
    val prefKey: String,
    val label: String,
    val defaults: List<KeyCombo>,
    /**
     * Which trigger the service tests first.
     *
     * This lives here rather than as the order of four `if`s in the service,
     * because two things need it and neither may guess: the service, which must
     * test in this order, and the conflict warning, which must say honestly which
     * of two actions sharing a chord will actually fire. If the two orders ever
     * disagreed, the warning would be a lie told by code that looks like it knows.
     */
    val precedence: Int,
    /**
     * Android and hardware conventions that apply whatever the binding says —
     * bare Right Alt, `KEYCODE_LANGUAGE_SWITCH`, the Samsung 한/영 keycode 218.
     * A user rebinding 한/영 must not end up with a keyboard whose physical 한/영
     * key does nothing.
     */
    val platformAlternates: List<KeyCombo> = emptyList(),
) {
    HANGUL_ENG(
        prefKey = "hangul_eng",
        label = "한/영 전환",
        precedence = 0,
        // No default, by the maintainer's instruction 2026-10-11: 한/영 is a
        // PHYSICAL key on a Korean keyboard (KANA, LANGUAGE_SWITCH, bare Right
        // Alt), and a chord default would be a second way to do something the
        // hardware already does — and a way that does not exist at all on the
        // keyboards that have no Alt row. What still works with nothing stored is
        // [platformAlternates]; the window shows "not set" for this row, and a
        // binding set here replaces nothing that is not already there.
        defaults = emptyList(),
        platformAlternates = listOf(
            KeyCombo(KeyEvent.KEYCODE_ALT_RIGHT),
            KeyCombo(KeyEvent.KEYCODE_LANGUAGE_SWITCH),
            KeyCombo(KANA_KEYCODE),
        ),
    ),
    VOICE(
        prefKey = "voice",
        label = "음성 입력",
        precedence = 1,
        // Alt + the 한/영 key. Two rulings stack here: the 2026-10-31 assignment
        // (modifier + 한/영 is the special-function namespace, on all three
        // frontends, and the engine owns the default chord) replaces the
        // 2026-10-11 Shift + KANA instruction. Shift is not a chord bit in the
        // engine -- it selects a layout row -- so the default had to move to the
        // engine's chord vocabulary to be one rule instead of three; the engine's
        // own default is HUNMIN_MOD_ALT for VOICE (docs/SPEC-MODES.md §E5,
        // wiki/decisions/2026-10-31-special-functions-engine-resolved-frontend-implemented.md).
        defaults = listOf(KeyCombo(KANA_KEYCODE, KeyBinding.META_ALT_LEFT)),
    ),
    AUTO_TOGGLE(
        prefKey = "auto_toggle",
        label = "자동 한/영 토글",
        precedence = 2,
        // No default, by the maintainer's ruling 2026-10-12 (issue #2): the
        // feature is disabled by default in 0.1. Right Alt + Enter was the
        // stage-2 stub's default chord; with no default and no stored binding,
        // that chord is NOT consumed here -- Enter reaches the editor as
        // Enter. The action stays in the enum because a stored binding must
        // still win over the placeholder row (the window shows 미정 until
        // something is set, exactly like 한/영 and 한자's empty defaults), and
        // the precedence order / conflict warning keep working unchanged.
        defaults = emptyList(),
    ),
    HANJA(
        prefKey = "hanja",
        label = "한자 입력",
        precedence = 3,
        // The maintainer's instruction 2026-10-31: "set hanja key default for
        // hanja input - it is also configurable through setting dialog", and the
        // key is platform specific. On Android that key is EISU (212), AOSP's
        // かな/한글 toggle key.
        //
        // A DEFAULT, not a [platformAlternates] entry, and the difference is the
        // "configurable" half: a stored binding replaces [defaults], so the user
        // can take EISU away, while an alternate keeps firing beside whatever the
        // row says -- which would make the key impossible to reassign and make
        // the window's "not set" a lie about a key that does fire.
        defaults = listOf(KeyCombo(EISU_KEYCODE)),
        platformAlternates = listOf(
            // These two stay alternates: they are keyboard conventions that hold
            // whatever the row says, they were not what the ruling named, and a
            // keyboard without EISU would otherwise have no 한자 key at all
            // unless the user configured one.
            KeyCombo(KeyEvent.KEYCODE_ALT_RIGHT, KeyBinding.META_SHIFT),
            KeyCombo(KeyEvent.KEYCODE_LANGUAGE_SWITCH, KeyBinding.META_SHIFT),
        ),
    ),
    ;

    companion object {
        /**
         * The order `onKeyDown` tests the actions in; the first match wins.
         */
        fun byPrecedence(): List<KeyAction> = values().sortedBy { it.precedence }
    }
}

/**
 * Where a binding is stored.
 *
 * An interface, so tier E1 can test the read/write/replace round trip on the JVM
 * with a map instead of the platform's SharedPreferences — the same shape
 * RecordingInputConnection uses for the InputConnection.
 */
interface KeyBindingRepository {
    fun read(key: String): String?
    fun write(key: String, value: String)
}

class SharedPrefsKeyBindingRepository(private val prefs: android.content.SharedPreferences) : KeyBindingRepository {
    override fun read(key: String): String? = prefs.getString(key, null)
    override fun write(key: String, value: String) {
        prefs.edit().putString(key, value).apply()
    }
}

object KeyBindings {

    const val PREFS_NAME = "hunmin_keys"

    fun from(context: android.content.Context): KeyBindingRepository =
        SharedPrefsKeyBindingRepository(context.getSharedPreferences(PREFS_NAME, android.content.Context.MODE_PRIVATE))

    /** The stored chord for [action], or null when the default is in force. */
    fun stored(store: KeyBindingRepository, action: KeyAction): KeyCombo? =
        KeyBinding.decode(store.read(action.prefKey))

    /** Every chord that fires [action]: the stored one, else the defaults. */
    fun combos(store: KeyBindingRepository, action: KeyAction): List<KeyCombo> =
        listOfNotNull(stored(store, action)) + action.defaults.takeIf {
            store.read(action.prefKey) == null
        }.orEmpty()

    /**
     * Is [keyCode] the 한/영 key, whatever modifiers are held?
     *
     * The compound press needs this and cannot use [matches]: "Alt + 한/영" is not
     * a chord any row lists -- the modifier is the NAMESPACE for special
     * functions (ruled 2026-10-31), not part of the assignment -- so matching with
     * the modifiers in the way would miss every compound press it must resolve.
     * The key set is the same one the toggle uses: the stored binding, the
     * defaults in force, and the platform alternates.
     */
    fun isLanguageKey(store: KeyBindingRepository, keyCode: Int): Boolean {
        val all = combos(store, KeyAction.HANGUL_ENG) + KeyAction.HANGUL_ENG.platformAlternates
        return all.any { it.keyCode == keyCode }
    }

    fun set(store: KeyBindingRepository, action: KeyAction, combo: KeyCombo) {
        store.write(action.prefKey, combo.encode())
    }

    /**
     * Another action that already claims [combo], whichever of the two wins.
     *
     * Two actions can legitimately end up with the same chord — the window
     * allows it on purpose, rebinding is not a lockout — but the service tests in
     * precedence order, so exactly one of them keeps working and the other goes
     * quiet with no message. Naming the other action is what lets the window say
     * which one it was.
     *
     * The direction matters and is not symmetric. Binding a *later* action to a
     * chord an *earlier* one owns makes the new binding dead; binding an *earlier*
     * action to a chord a later one owns kills the existing one instead. Both are
     * conflicts and the caller has to know which side is which, so the winner is
     * `min(precedence)` of the pair and the caller compares.
     *
     * Platform alternates count: binding 한자 to a bare Right Alt really does
     * conflict, because 한/영 owns that chord as a hardware convention.
     */
    fun conflict(store: KeyBindingRepository, action: KeyAction, combo: KeyCombo): KeyAction? =
        KeyAction.byPrecedence().firstOrNull { other ->
            other != action &&
                (combos(store, other) + other.platformAlternates).any { it == combo }
        }

    /**
     * Does this key event fire [action]?
     *
     * True for a configured binding, for a default while nothing is stored, or for
     * a platform alternate — in that order, and the store is read once per call so
     * an event never costs more than one lookup.
     */
    fun matches(store: KeyBindingRepository, action: KeyAction, keyCode: Int, metaState: Int): Boolean {
        val raw = store.read(action.prefKey)
        val m = KeyBinding.normalise(metaState)
        if (raw != null) {
            val combo = KeyBinding.decode(raw)
            if (combo != null && combo.keyCode == keyCode &&
                KeyBinding.modifiersMatch(combo.modifiers, m)
            ) {
                return true
            }
        } else if (KeyBinding.matchesAny(action.defaults, keyCode, metaState)) {
            return true
        }
        return KeyBinding.matchesAny(action.platformAlternates, keyCode, metaState)
    }
}
/**
 * The key the settings window is currently waiting for.
 *
 * WHY THE ACTIVITY CANNOT LISTEN FOR ITSELF. Measured on the AVD (API 35): with
 * hunmin the current input method and its window showing, a hardware keyevent is
 * dispatched to the *IME window*, not to the activity on top of it. The capture
 * dialog was open, the key was injected, and neither the TextView's listener, the
 * dialog decor view's listener, nor `Activity.dispatchKeyEvent` saw a single event
 * -- the IME service saw it, which is the only place it goes. So the window asks
 * the service to feed it, and the service checks this before its own bindings:
 * a chord being captured must not also toggle 한/영 or type a character.
 *
 * A process-wide singleton because the service and the activity are the same
 * process and there is exactly one of each; a ContentProvider or a bound service
 * would add a failure mode without adding reachability.
 */
object KeyCaptureSession {

    @Volatile
    private var handler: ((action: Int, keyCode: Int, metaState: Int) -> Boolean)? = null

    fun install(h: (action: Int, keyCode: Int, metaState: Int) -> Boolean) {
        handler = h
    }

    fun clear() {
        handler = null
    }

    /**
     * Offer this event to a waiting capture. True when the capture consumed it,
     * which is the caller's signal to swallow the keystroke.
     */
    fun feed(action: Int, keyCode: Int, metaState: Int): Boolean {
        val h = handler ?: return false
        return h(action, keyCode, metaState)
    }
}
