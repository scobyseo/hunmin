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
    val platformAlternates: List<KeyCombo> = emptyList(),
) {
    HANGUL_ENG(
        "hangul_eng",
        "한/영 전환",
        listOf(KeyCombo(KeyEvent.KEYCODE_Z, KeyBinding.META_ALT_LEFT)),
        // HunminInputMethodService.kt: bare Right Alt, LANGUAGE_SWITCH, Samsung 218.
        listOf(
            KeyCombo(KeyEvent.KEYCODE_ALT_RIGHT),
            KeyCombo(KeyEvent.KEYCODE_LANGUAGE_SWITCH),
            KeyCombo(218),
        ),
    ),
    HANJA(
        "hanja",
        "한자 입력",
        listOf(KeyCombo(KeyEvent.KEYCODE_X, KeyBinding.META_ALT_LEFT)),
        // HunminInputMethodService.kt: Shift+Right Alt, Shift+LANGUAGE_SWITCH.
        listOf(
            KeyCombo(KeyEvent.KEYCODE_ALT_RIGHT, KeyBinding.META_SHIFT),
            KeyCombo(KeyEvent.KEYCODE_LANGUAGE_SWITCH, KeyBinding.META_SHIFT),
        ),
    ),
    VOICE(
        "voice",
        "음성 입력",
        listOf(
            KeyCombo(KeyEvent.KEYCODE_V, KeyBinding.META_ALT_LEFT),
            KeyCombo(KeyEvent.KEYCODE_V, KeyBinding.META_ALT_RIGHT),
        ),
    ),
    AUTO_TOGGLE(
        "auto_toggle",
        "자동 한/영 토글",
        listOf(KeyCombo(KeyEvent.KEYCODE_ENTER, KeyBinding.META_ALT_RIGHT)),
    ),
}

/**
 * Where a binding is stored.
 *
 * An interface, so tier E1 can test the read/write/replace round trip on the JVM
 * with a map instead of the platform's SharedPreferences — the same shape
 * RecordingInputConnection uses for the InputConnection.
 */
interface KeyBindingStore {
    fun read(key: String): String?
    fun write(key: String, value: String)
}

class SharedPrefsStore(private val prefs: android.content.SharedPreferences) : KeyBindingStore {
    override fun read(key: String): String? = prefs.getString(key, null)
    override fun write(key: String, value: String) {
        prefs.edit().putString(key, value).apply()
    }
}

object KeyBindings {

    const val PREFS_NAME = "hunmin_keys"

    fun from(context: android.content.Context): KeyBindingStore =
        SharedPrefsStore(context.getSharedPreferences(PREFS_NAME, android.content.Context.MODE_PRIVATE))

    /** The stored chord for [action], or null when the default is in force. */
    fun stored(store: KeyBindingStore, action: KeyAction): KeyCombo? =
        KeyBinding.decode(store.read(action.prefKey))

    /** Every chord that fires [action]: the stored one, else the defaults. */
    fun combos(store: KeyBindingStore, action: KeyAction): List<KeyCombo> =
        listOfNotNull(stored(store, action)) + action.defaults.takeIf {
            store.read(action.prefKey) == null
        }.orEmpty()

    fun set(store: KeyBindingStore, action: KeyAction, combo: KeyCombo) {
        store.write(action.prefKey, combo.encode())
    }

    /**
     * Does this key event fire [action]?
     *
     * True for a configured binding, for a default while nothing is stored, or for
     * a platform alternate — in that order, and the store is read once per call so
     * an event never costs more than one lookup.
     */
    fun matches(store: KeyBindingStore, action: KeyAction, keyCode: Int, metaState: Int): Boolean {
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
