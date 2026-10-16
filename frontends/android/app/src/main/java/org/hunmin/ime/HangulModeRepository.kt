/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * HangulModeRepository — where the Hangul/English mode choice is remembered.
 *
 * Android's InputMethodService is killed and recreated often (memory pressure,
 * a configuration change, an IME switch), and `isHangulMode` is ordinary
 * process state. Without persistence a user who switched to English to type a
 * URL finds the keyboard back in Hangul on the next field. The choice has to
 * survive the process.
 *
 * Why this is not just two `getSharedPreferences` calls inline: the read has a
 * default (first install, a wiped prefs file) and the write has to be
 * crash-tolerant, and both are behaviour worth asserting on their own. The
 * store takes plain read/write functions instead of a `SharedPreferences` so
 * tier E1 can drive it from a plain map on the JVM — no Robolectric, no
 * Android runtime, same as the rest of that tier.
 *
 * The functions are injected rather than reached for, which is what keeps this
 * a testable contract and not a re-implementation of the service.
 */
package org.hunmin.ime

/**
 * Remembers which Hangul/English mode the IME was last in.
 *
 * The prefs file name and the key are owned by the caller (the service) so the
 * store stays independent of how they are spelled; only the shape of the
 * remember/recall contract lives here.
 */
class HangulModeRepository(
    private val read: (key: String, default: Boolean) -> Boolean,
    private val write: (key: String, value: Boolean) -> Unit,
) {
    /**
     * The mode to start a session in.
     *
     * Falls back to [defaultHangul] when nothing is stored — a first install,
     * or a prefs file that could not be read. Hangul is the answer because
     * that is the mode the engine was in before this feature existed, so an
     * upgrade must not land the user in English by surprise.
     */
    fun recall(key: String, defaultHangul: Boolean = true): Boolean =
        runCatching { read(key, defaultHangul) }.getOrDefault(defaultHangul)

    /**
     * Persist [hangul] so the next session starts there.
     *
     * Swallows failures: a prefs write that throws (a full disk, a locked
     * store) must not take the toggle down with it. The in-memory mode is
     * already correct; losing the persistence only means the next session
     * falls back to the default, which is the pre-feature behaviour and is
     * strictly better than crashing on a keypress.
     */
    fun remember(key: String, hangul: Boolean) {
        runCatching { write(key, hangul) }
    }
}
