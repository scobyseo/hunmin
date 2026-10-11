/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * LayoutEntry — one row in the settings window's layout list.
 */
package org.hunmin.ime

/**
 * One layout as the UI knows it.
 *
 * [mode] is the layout JSON's own `mode` field (docs/SPEC-SCHEMA.md), read
 * from the engine through `hunmin_ctx_layout_mode`. It is defaulted rather than
 * required so a row built by hand in a test is a Hangul row unless it says
 * otherwise — the same default the engine applies.
 */
data class LayoutEntry(
    val id: String,
    val label: String,
    val mode: LayoutMode = LayoutMode.HANGUL,
)
