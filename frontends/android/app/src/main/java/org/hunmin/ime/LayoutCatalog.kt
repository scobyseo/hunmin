/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * LayoutCatalog — the layout list the settings window shows, split by mode and
 * in the order it shows them.
 *
 * WHY THE SORT IS CODE-POINT ORDER AND NOT A COLLATOR
 * --------------------------------------------------
 * The maintainer's requirement is 가나다순. For Hangul, Unicode code-point
 * order IS that order: the Hangul Syllables block is laid out by initial,
 * then medial, then final — AC00 is 가, B098 is 나, B2E4 is 다 — so comparing
 * code points sorts 두벌식 before 세벌식 before 신세벌식 before 영어, which is
 * the dictionary order a Korean reader expects.
 *
 * A Collator would be "more correct" for mixed Latin/Hangul names, and is the
 * wrong tool here for two reasons: its results depend on the ICU version
 * (the JVM's and Android's are different builds), and the SAME list has to come
 * out in the same order from the fcitx5 front-end, which is C++ and has no
 * Collator to reach for. One rule both platforms compute identically beats a
 * better rule only one of them can compute. `compareTo` is UTF-16 code-unit
 * order, which equals code-point order for everything in the BMP — every Hangul
 * syllable and every Latin letter in these names.
 *
 * The id is the tie-break, so two layouts whose display names collide still
 * have one deterministic order instead of whatever the sort happened to leave.
 */
package org.hunmin.ime

/** Which half of the two-mode IME a layout belongs to. */
enum class LayoutMode {
    HANGUL,
    LATIN;

    companion object {
        /**
         * Read the engine's answer. Anything unrecognised is [HANGUL], which is
         * what the engine itself does with an unknown value
         * (`docs/SPEC-SCHEMA.md`), so the two never disagree about one file.
         */
        fun of(raw: String): LayoutMode =
            if (raw == "latin") LATIN else HANGUL
    }
}

object LayoutCatalog {

    /**
     * 가나다순. See the file header for why this is code-point order and not a
     * Collator; the id tie-break is what makes it a total order.
     */
    fun sorted(entries: List<LayoutEntry>): List<LayoutEntry> =
        entries.sortedWith(compareBy({ it.label }, { it.id }))

    /** The rows one section of the settings window shows. */
    fun byMode(entries: List<LayoutEntry>, mode: LayoutMode): List<LayoutEntry> =
        sorted(entries.filter { it.mode == mode })

    /**
     * The layout to show as "currently in use" for [mode].
     *
     * Falls back to [fallback] when nothing is stored or stored, and does not
     * include [activeId] in the list — the active layout is what the strip
     * already shows, so a settings list that repeats it says nothing.
     */
    fun choose(
        entries: List<LayoutEntry>,
        mode: LayoutMode,
        activeId: String?,
        fallback: String,
    ): String =
        byMode(entries, mode).firstOrNull { it.id == activeId }?.id
            ?: byMode(entries, mode).firstOrNull { it.id == fallback }?.id
            ?: activeId
            ?: byMode(entries, mode).firstOrNull()?.id
            ?: fallback
}