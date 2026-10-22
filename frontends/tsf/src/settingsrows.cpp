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

#include <cstring>

/* ---- the pure half: what Apply writes, what the rows contain ---- */

int hunmin_tsf_rows_for_mode(hunmin_ctx *ctx, bool hangul, HunminTsfLayoutRow *rows,
                             int cap) {
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
            strncpy(rows[n].id, id, HUNMIN_TSF_SETTINGS_TEXT_CAP - 1);
            rows[n].id[HUNMIN_TSF_SETTINGS_TEXT_CAP - 1] = '\0';
            const char *name = hunmin_ctx_layout_name(ctx, i, "ko");
            strncpy(rows[n].name, (name && name[0]) ? name : id,
                    HUNMIN_TSF_SETTINGS_TEXT_CAP - 1);
            rows[n].name[HUNMIN_TSF_SETTINGS_TEXT_CAP - 1] = '\0';
            rows[n].supported = hunmin_ctx_layout_supported(ctx, i) ? 1 : 0;
            ++n;
        }
    }
    return n;
}
