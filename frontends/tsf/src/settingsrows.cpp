/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The settings dialog's READ half (issue #71): the layout rows the two
 * combo boxes list, derived from the engine list by mode. No window, no
 * store, no COM.
 */
#include "settingsrows.h"

#include <cstdio>
#include <cstring>

/* ---- the pure half: what Apply writes, what the rows contain ---- */

int hunmin_tsf_rows_for_mode(hunmin_ctx *ctx, bool hangul, HunminTsfLayoutRow *rows, int cap) {
    int n = 0;
    if (ctx) {
        const int count = hunmin_ctx_layout_count(ctx);
        for (int i = 0; i < count && n < cap; ++i) {
            const char *mode = hunmin_ctx_layout_mode(ctx, i);
            if (!mode)
                continue;
            if (hangul ? strcmp(mode, "hangul") != 0 : strcmp(mode, "latin") != 0)
                continue;
            const char *id = hunmin_ctx_layout_id(ctx, i);
            if (!id || !id[0])
                continue;
            rows[n].index = i;
            strncpy_s(rows[n].id, HUNMIN_TSF_SETTINGS_TEXT_CAP, id, _TRUNCATE);
            const char *name = hunmin_ctx_layout_name(ctx, i, "ko");
            strncpy_s(rows[n].name, HUNMIN_TSF_SETTINGS_TEXT_CAP, (name && name[0]) ? name : id, _TRUNCATE);
            rows[n].supported = hunmin_ctx_layout_supported(ctx, i) ? 1 : 0;
            ++n;
        }
    }
    return n;
}

/* ---- the automatic-switch row (issue #175, R13-정정 surface B) ---- */

/* The chord's modifier bits, in the order Windows spells them. The bits are
 * the ENGINE's (hunmin_buffered_preedit_special_chord), so a frontend that
 * rebinds nothing still advertises the chord the engine honours. */
static const char *modsText(unsigned mods, char *buf, size_t cap) {
    buf[0] = '\0';
    if (mods & HUNMIN_MOD_CTRL)
        strncat_s(buf, cap, "Ctrl+", _TRUNCATE);
    if (mods & HUNMIN_MOD_ALT)
        strncat_s(buf, cap, "Alt+", _TRUNCATE);
    if (mods & HUNMIN_MOD_GUI)
        strncat_s(buf, cap, "Win+", _TRUNCATE);
    if (mods & HUNMIN_MOD_SHIFT)
        strncat_s(buf, cap, "Shift+", _TRUNCATE);
    return buf;
}

void hunmin_tsf_autoswitch_row(hunmin_ctx *ctx, const char *hangulId, const char *latinId, int stored, HunminTsfAutoswitchRow *out) {
    if (!out)
        return;
    memset(out, 0, sizeof *out);
    /* B3's gate first: it answers even when no layer can be built, which is the
     * state a machine without the separate data package is in. */
    out->available = (ctx && hunmin_ctx_autoswitch_data_present(ctx)) ? 1 : 0;
    out->value = stored ? 1 : 0;
    /* The chord and the value are both read from a LAYER, never spelled here:
     * the engine owns the assignment (R13-정정) and it owns the data veto. */
    hunmin_buffered_preedit *layer = (ctx && hangulId && latinId) ? hunmin_buffered_preedit_new(ctx, hangulId, latinId, HUNMIN_COMMIT_AT_BOUNDARY) : nullptr;
    if (layer) {
        char mods[24];
        const unsigned chord = hunmin_buffered_preedit_special_chord(layer, HUNMIN_SPECIAL_AUTOSWITCH);
        /* The key's NAME is the platform's (the physical Han/Eng key); only the
         * chord half is the engine's. A frontend on a host with a different
         * physical key passes its own name here -- no frontend spells Ctrl. */
        snprintf(out->key, sizeof out->key, "%s\uD55C/\uC601", modsText(chord, mods, sizeof mods));
        /* Hand the stored preference to the layer the way the adapter does, then
         * read back what the ENGINE settled on: a 1 the engine refused (data
         * absent) reads 0 here without the dialog guessing. */
        (void)hunmin_buffered_preedit_set_option(layer, "autoswitch", stored ? 1 : 0);
        int value = 0;
        if (hunmin_buffered_preedit_get_option(layer, "autoswitch", &value)) out->value = value;
        hunmin_buffered_preedit_free(layer);
    } else {
        /* No engine, no state: the value cannot be ON when there is no layer to
         * hold it, so the row reports OFF rather than a stored 1 the engine would
         * refuse. */
        out->value = 0;
        snprintf(out->key, sizeof out->key, "Ctrl+\uD55C/\uC601");
    }
    if (out->available) {
        snprintf(out->label, sizeof out->label, "\uC790\uB3D9 \uD55C/\uC601 \uC804\uD658 (%s)", out->key);
    } else {
        /* R13-정정's discoverability rule applies to the disabled state: say
         * WHICH data is missing, not merely that the control is off. */
        snprintf(out->label, sizeof out->label, "\uC790\uB3D9 \uD55C/\uC601 \uC804\uD658 (%s) \u2014 \uC0AC\uC804 \uC5C6\uC74C: \uC0AC\uC6A9\uD560 \uC218 \uC5C6\uC74C", out->key);
    }
}
