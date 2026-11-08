/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The special-function namespace (ruled 2026-10-31): modifier + 한/영 asks
 * the engine which function the chord means, and the frontend implements it or
 * refuses.
 *
 * The split, in one place because three frontends share it:
 *
 *   ENGINE   owns the binding table (function -> chord bits) and the resolution
 *            of one key event to one function. Nothing here duplicates that --
 *            the defaults (VOICE = Alt, AUTOSWITCH = Ctrl) live in
 *            libhunmin/src/buffered_preedit.c so fcitx5, TSF and Android cannot
 *            disagree about a table nobody seeded.
 *   FRONTEND owns the platform key code that counts as 한/영, and the feature:
 *            recording audio, flipping the auto-switch state. It may say no.
 *
 * [SpecialFunction] mirrors hunmin.h's hunmin_special_function. The values are
 * ABI; append, never renumber.
 */
package org.hunmin.ime

object SpecialFunction {
    const val NONE = 0

    /** Start voice input. Engine-blind: whoever can record implements it. */
    const val VOICE = 1

    /** Flip the automatic Hangul/English switch. */
    const val AUTOSWITCH = 2

    /**
     * The name of a chord, from the ENGINE's own modifier bits (hunmin.h
     * `HUNMIN_MOD_*`) — the frontend names the modifiers it prints, the engine
     * names the function and holds the assignment (docs/SPEC-MODES.md §ReqMode13:
     * "the UI has to know which key it is ... this information is provided by
     * the engine").
     *
     * The 한/영 half is not a bit: it is the platform's own language key, which
     * is why it is spelled here and the modifier is not. An empty answer means
     * the engine holds no chord for the function, and the row must say so rather
     * than invent one.
     */
    fun chordLabel(mods: Int): String {
        if (mods == 0) return ""
        val parts = mutableListOf<String>()
        if (mods and MOD_CTRL != 0) parts += "Ctrl"
        if (mods and MOD_ALT != 0) parts += "Alt"
        if (mods and MOD_SHIFT != 0) parts += "Shift"
        if (mods and MOD_GUI != 0) parts += "Win"
        if (parts.isEmpty()) return ""
        return parts.joinToString("+") + "+한/영"
    }

    /* hunmin.h HUNMIN_MOD_* — contract values, pinned by EngineModsTest. */
    private const val MOD_CTRL = 0x1
    private const val MOD_ALT = 0x2
    private const val MOD_GUI = 0x4
    private const val MOD_SHIFT = 0x8
}

/**
 * What this frontend can do with a function, and how it does it.
 *
 * [supports] is asked BEFORE anything is performed, and false is a legal answer:
 * the engine answers -1, the chord is not consumed, and the key reaches the
 * application like any unhandled shortcut. A frontend that pretends (a phone
 * without RECORD_AUDIO claiming VOICE) would swallow the chord and do nothing,
 * which is the failure this callback exists to make impossible.
 */
interface SpecialServices {
    fun supports(fn: Int): Boolean
    fun perform(fn: Int)
}

/**
 * The object libhunmin calls back into.
 *
 * hunmin-jni.c finds these two methods BY NAME -- `specialSupports` with
 * signature `(I)Z` and `specialPerform` with `(I)V`. Renaming either, or making
 * them non-public, makes the shim refuse to register the host (it clears the
 * pending NoSuchMethodError and installs no host at all), and every chord then
 * reports "not ours" instead of performing. That is deliberate: a host that
 * answers "refused" for everything while looking registered would read as a
 * platform limitation. SpecialHostTest pins the names through the real shim.
 */
class HunminSpecialHost(private val services: SpecialServices) {
    fun specialSupports(fn: Int): Boolean = services.supports(fn)

    fun specialPerform(fn: Int) {
        services.perform(fn)
    }
}
