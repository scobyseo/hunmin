/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * ToolbarState — everything the IME's dock shows, in one immutable value.
 *
 * WHY A STATE HOLDER AND NOT MORE SETTERS
 * --------------------------------------
 * The dock used to be told about the world in pieces: setLayouts(), then
 * setActiveLayout(), then setMode(), then setMicState(). Each call reached into
 * a view that might not exist yet, so every setter had to remember what it had
 * last been told and replay it after a rebuild. That is the arrangement that
 * produced the "style B crashes, style A does not" report: the state lived in
 * the view tree and in the service at the same time.
 *
 * One value, built by whoever knows the truth, bound by whoever draws: the
 * rebuild cannot lose a state it never had, and a new style is the only thing
 * that has to rebuild at all.
 *
 * It is a plain data class on purpose — the whole point is that it can be
 * compared, logged and asserted in a JVM unit test without a Context.
 */
package org.hunmin.ime

/**
 * What the dock renders.
 *
 * @property layoutLabel what the layout chip shows; may be abbreviated or cut.
 * @property layoutLabelFull the name as spoken and long-pressed. A truncated
 *   label is a display decision, not a loss of information.
 * @property modeIsHangul which half of the 한 / A segment is filled.
 * @property voice the mic's state.
 * @property hanjaConverting whether a Hanja conversion is in progress.
 */
data class ToolbarState(
    val layoutLabel: String,
    val layoutLabelFull: String,
    val modeIsHangul: Boolean,
    val voice: VoiceState,
    val hanjaConverting: Boolean = false,
) {
    companion object {
        /**
         * The state for a layout id, resolving the label from [layouts].
         *
         * An id that is not in the list shows its own text rather than nothing:
         * a blank indicator reads as "no layout", and the id is enough to go
         * looking with.
         */
        fun of(
            layouts: List<LayoutEntry>,
            layoutId: String?,
            abbreviated: Boolean = false,
            modeIsHangul: Boolean,
            voice: VoiceState,
            hanjaConverting: Boolean = false,
        ): ToolbarState {
            val entry = layouts.firstOrNull { it.id == layoutId }
            val full = entry?.label ?: layoutId ?: NO_LAYOUT
            return ToolbarState(
                layoutLabel = if (entry != null && abbreviated) entry.shortLabel() else full,
                layoutLabelFull = full,
                modeIsHangul = modeIsHangul,
                voice = voice,
                hanjaConverting = hanjaConverting,
            )
        }

        /** Shown when no layout has been resolved yet. */
        const val NO_LAYOUT = "—"
    }
}

/** The mic's states, in the order they matter to the user. */
enum class VoiceState { IDLE, LISTENING, ERROR }