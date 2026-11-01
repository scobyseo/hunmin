/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * LayoutRegistry — the layout list the settings window shows, split by mode and
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

object LayoutRegistry {

    /**
     * The layout [mode] should actually run, out of what this build offers.
     *
     * Three steps, in order: the stored choice if it is here AND belongs to this
     * mode, then the mode's documented default, then anything in the same mode.
     *
     * The mode check is the part that is easy to lose. A stored 한글 preference
     * naming a Latin layout "validates" perfectly well if you only ask "is this
     * id openable", and lands in the English engine — the failure is invisible
     * until someone notices their Korean input became qwerty.
     *
     * Pure, so the fallback chain is testable without an engine: the service
     * used to carry this as a loop over the native context and nothing could
     * check it.
     */
    fun resolve(
        entries: List<LayoutEntry>,
        mode: LayoutMode,
        stored: String?,
        defaultId: String,
    ): String {
        val offered = byMode(entries, mode)
        return offered.firstOrNull { it.id == stored }?.id
            ?: offered.firstOrNull { it.id == defaultId }?.id
            ?: offered.firstOrNull()?.id
            // Nothing in this mode is openable. Keep the stored id when there was
            // one so the log names what was asked for; the engine path reports
            // the real failure when it cannot instantiate it.
            ?: stored
            ?: defaultId
    }

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
/**
 * The three shapes the input view can take.
 *
 * A setting rather than a decision, by the maintainer's instruction
 * 2026-10-11: "설정에 A, B, C 중 선택할 수 있게 만들어도 좋아". The blurb is
 * what the settings window shows under each name — an enum whose members a
 * user cannot picture is a setting nobody changes, it is a constant with extra
 * steps.
 *
 * All three show the same five things: the layout in use, the 한/영 mode, the
 * Hanja key, the voice key, and settings. Only the arrangement differs, and the
 * only one that may truncate is the layout NAME (the maintainer's rule, and the
 * reason A is listed first: with nothing to truncate, nothing is lost).
 */
enum class ToolbarStyle(
    val key: String,
    val label: String,
    val blurb: String,
) {
    /** One row: layout name left, mode and buttons right. */
    ONE_LINE("one_line", "한줄 모드", "배열 이름 왼쪽, 모드·버튼 오른쪽. 이름이 길면 잘립니다"),

    /** Two rows: status above, evenly spaced buttons below. */
    TWO_LINE("two_line", "두줄 모드", "상태 한 줄 + 버튼 한 줄. 높이를 가장 많이 씁니다");

    companion object {
        fun of(raw: String?): ToolbarStyle =
            values().firstOrNull { it.key == raw } ?: ONE_LINE
    }
}

/**
 * The settings window's way into the running IME.
 *
 * The window is an Activity and the state it edits lives in an
 * InputMethodService; there is no bound reference between them, and the
 * key-binding capture already had this exact problem (see KeyCaptureSession:
 * while an IME is the current input method, keys reach the service and nothing
 * else). So the service installs these callbacks and the window uses them —
 * one writer for every setting, rather than the Activity writing preferences
 * behind the service's back and hoping it re-reads them.
 *
 * Every accessor is nullable because the window can be opened when no IME is
 * running at all. The window then says so instead of showing an empty list that
 * looks like "you have no layouts".
 */
object ImeSettings {

    /** Every layout this build can run, with its mode and display name. */
    var layoutsProvider: () -> List<LayoutEntry>? = { null }

    /** The layout currently chosen for a mode, null when the IME is not up. */
    var layoutChoice: (LayoutMode) -> String? = { null }

    /** Called when the window sets a mode's layout. */
    var onLayoutChosen: (LayoutMode, String) -> Unit = { _, _ -> }

    /** The strip style in force. */
    var toolbarStyle: () -> ToolbarStyle = { ToolbarStyle.ONE_LINE }

    /** Called when the window sets the strip style. */
    var onToolbarStyleChosen: (ToolbarStyle) -> Unit = {}

    /**
     * The engine's automatic-switch value (1 on, 0 off, null when no engine can
     * answer) — docs/SPEC-MODES.md §R13-정정. The window SHOWS this and never
     * keeps its own copy; the key chord writes the same field.
     */
    var autoSwitchValue: () -> Int? = { null }

    /** The chord the engine holds for the automatic switch (HUNMIN_MOD_*), so
     *  the window names the key the engine honours instead of spelling one. */
    var autoSwitchChord: () -> Int? = { null }

    /** Whether this install carries the modes data at all (B3's gate, #169).
     *  False is why a row is disabled, and it is a different sentence from
     *  "off, chosen". */
    var autoSwitchDataPresent: () -> Boolean = { false }

    /** Called when the window asks for a new value. Returns the engine's answer
     *  (which the gate may have refused), not the request. */
    var onAutoSwitchChosen: (Boolean) -> Int? = { null }

    /** Whether a service is there to talk to at all. */
    val connected: Boolean get() = layoutsProvider() != null
}
