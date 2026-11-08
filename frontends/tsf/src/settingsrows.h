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

/* ReqMode13 surface B (issue #175): the automatic-switch row, DERIVED from the
 * engine so the dialog spells neither the value, the key, nor the reason it can
 * be disabled:
 *   - `value` is what hunmin_buffered_preedit_get_option(layer,"autoswitch")
 *     answers after the stored preference was handed to a layer built for this
 *     pair, i.e. the engine's veto (data absent, B3/#169) is already applied;
 *   - `key` is the chord the engine holds for HUNMIN_SPECIAL_AUTOSWITCH plus the
 *     platform's own name for the physical Han/Eng key -- the chord half is the
 *     engine's by ruling, the key's NAME is the one thing only the platform
 *     knows (docs/FRONTEND-EQUIVALENCE.md §3.1);
 *   - `available` is the data gate, and `label` carries the reason when it is 0.
 *     Every label also names the support condition ("지원 앱에서만"): the dialog
 *     is a separate process and cannot know a host's delivery mode, so it states
 *     the condition rather than guessing the host's state (issue #215).
 * Returns 1 when the layer could be built (ctx and both ids), 0 otherwise -- in
 * which case the row still answers with the data gate and a reason, because a
 * settings surface that silently disables itself is the defect ReqMode13 bans. */
#define HUNMIN_TSF_KEYNAME_CAP 48
#define HUNMIN_TSF_FEATURE_LABEL_CAP 160
typedef struct HunminTsfAutoswitchRow {
    int available; /* 1 = the judgement's dictionary is installed */
    int value; /* ON/OFF as the ENGINE answers it */
    char key[HUNMIN_TSF_KEYNAME_CAP]; /* UTF-8, e.g. "Ctrl+한/영" */
    char label[HUNMIN_TSF_FEATURE_LABEL_CAP]; /* UTF-8 checkbox text, incl. the reason */
} HunminTsfAutoswitchRow;

void hunmin_tsf_autoswitch_row(hunmin_ctx *ctx, const char *hangulId, const char *latinId, int stored, HunminTsfAutoswitchRow *out);

#endif /* HUNMIN_TSF_SETTINGSROWS_H_ */
