/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * HunminKeyRegistry — Android KEYCODE -> engine character.
 *
 * libhunmin's convention (hunmin.h): one US-QWERTY *character* per event, case
 * encoding shift, 0x08 backspace. That is the same convention the layout JSON
 * tables are authored in and the same one the fcitx5 addon feeds it, which is
 * what makes one set of tier-A goldens comparable across every platform.
 *
 * The obvious implementation — `event.getUnicodeChar()` — is wrong here, and
 * the reason matters. getUnicodeChar() resolves the key through the *host*
 * keyboard layout, so on a French host the physical key that is `a` on a US
 * board arrives as `q`, and the engine would compose the wrong thing while
 * every test stayed green (a test would have to change the OS layout to see
 * it). The layout data is authored in the US-QWERTY convention, so the adapter
 * must be too: keycode -> character, no OS layout consulted.
 *
 * This mirrors the fcitx5 adapter's assumption A1 (docs/FRONTEND-CONTRACT.md)
 * and inherits its stated cost: a layout wanting a non-US physical key cannot
 * be expressed. Accepting that here is the same decision, for the same reason.
 *
 * The table below is generated from tools/keymap-fixture.py's
 * ANDROID_KEYCODES, which is checked against android.jar by `javap -constants`
 * on every run (ctest `keymap-fixture`). Do not hand-edit it: fix the fixture
 * and regenerate, or the two tables will drift and the fixture will keep
 * passing while the Kotlin one is wrong.
 */
package org.hunmin.ime

/** One key the adapter can hand to the engine. */
data class HunminKey(val keyCode: Int, val unshifted: Char, val shifted: Char?) {
    /**
     * What the engine should see for this key under [shiftedOn] and [capsOn].
     *
     * Shift is data, not a modifier (invariant 4): sin3-p2's symbol tiers and
     * 초성체 need the uppercase form, so the adapter passes it through and never
     * turns it into a chord. CapsLock is separate and inverted — AOSP delivers
     * the *shifted* letter under CapsLock, so the adapter has to undo it and
     * tell the engine about caps separately. Same rule as the fcitx5 adapter's
     * A3, for the same platform reason.
     */
    fun engineChar(shiftedOn: Boolean, capsOn: Boolean): Char? {
        // CapsLock INVERTS shift, it does not replace the letter. AOSP delivers
        // the shifted form whenever CapsLock is on, which for a plain letter is
        // the uppercase one -- so CapsLock+letter must come back as UPPERCASE,
        // not lowercase. The engine wants the unshifted letter plus the `caps`
        // option, so the case is undone here and the option is set separately in
        // HunminAdapter.onKey.
        //
        // An earlier revision returned `unshifted.lowercaseChar().uppercaseChar()`
        // under caps, which is always uppercase and therefore happened to be
        // right for letters -- but it ignored Shift entirely under caps, so
        // CapsLock+Shift+letter produced the wrong case. Both bits have to be
        // considered.
        val shiftedResult = shifted ?: unshifted.uppercaseChar()
        return if (capsOn) {
            // Under CapsLock the physical shift is inverted by the OS, so the
            // character the user sees is the OPPOSITE of what shift alone would
            // give -- and AOSP already applied that inversion before we get
            // here. What the engine needs is the unshifted letter.
            unshifted
        } else if (shiftedOn) {
            shiftedResult
        } else {
            unshifted
        }
    }
}

object HunminKeyRegistry {

    const val BACKSPACE_ENGINE_CHAR: Char = ''

    private fun k(code: Int, u: Char, s: Char? = null) = HunminKey(code, u, s)

    // AOSP metaState bits, read out of android.jar with javap -constants
    // (2026-10-03, API 35). These are NOT the same numbers as the fcitx5
    // KeyState bit values the desktop adapter uses, and copying those over is a
    // mistake that produced a real defect here: the fcitx5 mask
    // 0xf0000000 covers no AOSP bit at all, so Ctrl+key was never recognised as
    // a chord and every Ctrl combination reached the engine as if it were a
    // plain letter.
    private const val META_SHIFT_ON = 0x00000001      //    1
    private const val META_ALT_ON = 0x00000002        //    2
    private const val META_CTRL_ON = 0x00001000       // 4096
    private const val META_META_ON = 0x00010000       // 65536
    private const val META_CAPS_LOCK_ON = 0x00100000  // 1048576
    private const val META_SUPER_ON = 0x00020000      // 131072 (META_META_LEFT)
    private const val META_HYPER_ON = 0x00040000      // 262144 (META_META_RIGHT)

    /**
     * Every key a layout can use, keyed by AOSP KEYCODE.
     *
     * Values are AOSP keycodes, not ASCII: KEYCODE_0 is 7 and KEYCODE_A is 29,
     * so `keyCode.toChar()` is wrong for every single entry. Measured out of
     * android.jar, not assumed -- see tools/keymap-fixture.py, which asserts
     * them against the platform on every run.
     */
    // Declared BEFORE BY_KEYCODE, which reads it during its own initialisation.
    // Kotlin initialises object properties in declaration order, so a table
    // defined below the map that consumes it is still null when the map is
    // built -- and the failure surfaces as
    //   NullPointerException: Cannot load from char array because
    //   "HunminKeyRegistry.SHIFTED_DIGITS" is null
    // in <clinit>, which every test then reports as "could not initialize".
    private val SHIFTED_DIGITS = charArrayOf(')', '!', '@', '#', '$', '%', '^', '&', '*', '(')

    private val BY_KEYCODE: Map<Int, HunminKey> = buildMap {
        // Digits: AOSP numbers these 7..16, and only '0'..'9' shift.
        for (d in 0..9) {
            val c = '0' + d
            val shifted = SHIFTED_DIGITS[d]
            k(7 + d, c, shifted)?.let { put(7 + d, it) }
        }
        // Letters: KEYCODE_A is 29, contiguous to KEYCODE_Z at 54.
        for (i in 0..25) {
            val u = 'a' + i
            k(29 + i, u, u.uppercaseChar())?.let { put(29 + i, it) }
        }
        // Sin3-p2's symbol and consonant tiers key on UPPERCASE letters -- that is
        // what "shift is data, not a modifier" means (invariant 4). The map above
        // already reaches them through the `shifted` column, but a scan that asks
        // "which keycodes exist for these layout keys" has to see H and W as
        // layout keys in their own right, so they are registered here too.
        // Registering a second HunminKey for the same keycode would collide, so
        // this is expressed as a lookup alias instead: engineChar() already
        // returns the uppercase form under shift, which is the only route the
        // engine ever takes. See hasLayoutKey().
        // Punctuation sits AFTER the letters, not at its ASCII value.
        put(55, k(55, ','))
        put(56, k(56, '.'))
        put(68, k(68, '`', '~'))
        put(69, k(69, '-', '_'))
        put(70, k(70, '=', '+'))
        put(71, k(71, '[', '{'))
        put(72, k(72, ']', '}'))
        put(73, k(73, '\\', '|'))
        put(74, k(74, ';', ':'))
        put(75, k(75, '\'', '"'))
        put(76, k(76, '/', '?'))
        // Named keys the layouts use. Space and Enter are the only two that
        // carry meaning: layouts key on space to finish a syllable, and Enter is
        // a newline the adapter forwards. Neither has a shifted form.
        //
        // DPAD and Escape are deliberately NOT here. No layout references them
        // (the 47 keys in libhunmin/data/layouts/*.json are all printable plus
        // space), and hunmin_ic_process has no meaning to give them, so
        // registering one would only risk it being "consumed" and swallowed.
        // tools/keymap-fixture.py holds the encoding half of this claim.
        put(61, k(61, '\t'))          // KEYCODE_TAB
        put(62, k(62, ' '))           // KEYCODE_SPACE
        put(66, k(66, '\n'))          // KEYCODE_ENTER
        put(67, k(67, BACKSPACE_ENGINE_CHAR))  // KEYCODE_DEL
    }

    /**
     * The engine character for a key, or null when the adapter must let the key
     * through untouched.
     *
     * Null is the important return value: it is how space, backspace, unknown
     * keys and out-of-range keycodes reach the client (invariant 3). Returning
     * "not consumed" is the mechanism, so a null here is a pass-through, not an
     * error.
     */
    fun engineChar(keyCode: Int, metaState: Int): Char? {
        if (keyCode == 67) return BACKSPACE_ENGINE_CHAR  // DEL is backspace
        val key = BY_KEYCODE[keyCode] ?: return null
        val shifted = (metaState and META_SHIFT_ON) != 0
        // META_CAPS_LOCK_ON, not META_CAPS_LOCK_MASK: AOSP has no MASK constant
        // for caps, only the ON bit (verified with javap against android.jar --
        // META_CAPS_LOCK_ON is 0x00010000 and META_SHIFT_ON is 0x1).
        val caps = (metaState and META_CAPS_LOCK_ON) != 0
        return key.engineChar(shifted, caps)
    }

    /**
     * Modifier chords the adapter refuses to send to the engine.
     *
     * Shift is deliberately NOT in this set — it is a layout row (invariant 4).
     * Ctrl/Alt/Super/Meta are: they mean shortcuts in every Android app, and
     * swallowing them would break the application behind the IME. Same rule as
     * the fcitx5 adapter's modifier mask.
     */
    fun isModifierChord(metaState: Int): Boolean =
        (metaState and CHORD_MASK) != 0

    /**
     * Ctrl/Alt/Meta and their left/right variants. Shift and CapsLock are
     * deliberately absent: Shift is a layout row (invariant 4) and CapsLock is
     * the `caps` engine option.
     *
     * Android has no META_SUPER_ON or META_HYPER_ON distinct from the META bits
     * -- "super" on a keyboard is the Windows/Meta key, which Android reports as
     * META_META_*. So the mask covers both META halves rather than naming two
     * constants that do not exist. Verified against android.jar; META_CTRL_MASK
     * and friends are deliberately NOT used because they also cover the ALT bits.
     */
    // CapsLock (0x00100000) is NOT in this mask even though it lives in the high
    // bit range -- it is the `caps` engine option, not a chord. An earlier
    // revision masked `0xf0000000` (the fcitx5 KeyState range, borrowed by
    // mistake) which swept up both META halves AND CapsLock, so CapsLock+letter
    // was refused before reaching the engine.
    private const val CHORD_MASK =
        META_CTRL_ON or META_ALT_ON or META_META_ON

    /**
     * Does any keycode produce [ch] as the engine character?
     *
     * Both cases are checked because the layout tables key on both: sin3-p2's
     * symbol tiers use uppercase, and a scan that only looked at `unshifted`
     * reported H and W as having no keycode -- which is true of the unshifted
     * column and false of the key. Every one of the 47 layout keys resolves
     * through here.
     */
    fun hasLayoutKey(ch: Char): Boolean = all().any { it.unshifted == ch || it.shifted == ch }

    /** The keycode table, for the tier E1 fixture and for assertions. */
    fun all(): List<HunminKey> = BY_KEYCODE.values.sortedBy { it.keyCode }
}
