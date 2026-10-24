/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The settings dialog's READ half (issue #71, split out in the clean-code
 * pass): the layout rows the two combo boxes list, derived from the engine
 * list by mode. No window, no store, no COM - the tier-F1 settings test
 * drives this directly.
 */
#ifndef HUNMIN_TSF_SETTINGSROWS_H_
#define HUNMIN_TSF_SETTINGSROWS_H_

#include "hunmin.h"

/* One layout row as the dialog lists it: engine list, split by
 * hunmin_ctx_layout_mode, display name from hunmin_ctx_layout_name(...,
 * 'ko'). C-shaped so the tier-F1 test can drive the collector directly. */
#define HUNMIN_TSF_SETTINGS_TEXT_CAP 64
#define HUNMIN_TSF_SETTINGS_MAX_ROWS 64
typedef struct HunminTsfLayoutRow {
    char id[HUNMIN_TSF_SETTINGS_TEXT_CAP];
    char name[HUNMIN_TSF_SETTINGS_TEXT_CAP]; /* UTF-8 */
    int index; /* engine list position */
    int supported; /* 1 when the build can run it */
} HunminTsfLayoutRow;

/* Fill rows[0..cap) with the layouts whose mode matches: the exact split
 * the dialog lists each slot with (same predicate resolveLayoutSlot
 * accepts stored ids by). Returns the row count. */
int hunmin_tsf_rows_for_mode(hunmin_ctx *ctx, bool hangul, HunminTsfLayoutRow *rows, int cap);

#endif /* HUNMIN_TSF_SETTINGSROWS_H_ */
