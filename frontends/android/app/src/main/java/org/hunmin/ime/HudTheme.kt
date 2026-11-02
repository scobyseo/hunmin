/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * HudTheme — the Compact HUD palette, shared by every settings surface.
 *
 * OverlayActivity's dialogs and the system IME settings screen draw the same
 * rows and must read the same tokens. Keeping the lookup in one object is what
 * stops the second screen from growing a second copy of the palette (review F1:
 * colours live in values/colors.xml and values-night/colors.xml, never inline).
 */
package org.hunmin.ime

import android.content.Context
import androidx.core.content.ContextCompat

internal object HudTheme {
    fun of(context: Context) = Colors(
        bgPanel = color(context, R.color.hunmin_hud_panel),
        textPrimary = color(context, R.color.hunmin_hud_on_panel),
        textSecondary = color(context, R.color.hunmin_hud_secondary),
        keyBg = color(context, R.color.hunmin_hud_keycap),
        keyBorder = color(context, R.color.hunmin_hud_keycap_outline),
        accent = color(context, R.color.hunmin_hud_accent),
        accentBg = color(context, R.color.hunmin_hud_accent_bg),
        divider = color(context, R.color.hunmin_hud_keycap),
        confirmed = color(context, R.color.hunmin_hud_confirmed),
    )

    private fun color(context: Context, id: Int): Int =
        ContextCompat.getColor(context, id)

    /** The cached token values for one build of a screen. */
    internal data class Colors(
        val bgPanel: Int,
        val textPrimary: Int,
        val textSecondary: Int,
        val keyBg: Int,
        val keyBorder: Int,
        val accent: Int,
        val accentBg: Int,
        val divider: Int,
        /** The "chord confirmed" green — 7.01:1 on the panel. */
        val confirmed: Int,
    )
}
