/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * datadir.h - where the TSF frontend finds its engine data, and how a
 * stored layout id is resolved against it (issues #66/#71). Shared by
 * the shell, the settings dialog and the tier-F1 tests, so no two
 * surfaces can disagree about the data source.
 */
#ifndef HUNMIN_TSF_DATADIR_H_
#define HUNMIN_TSF_DATADIR_H_

#include <string>

#include "hunmin.h"

/* The one data source: HUNMIN_DATA_DIR, else <module>\data, else the
 * repo checkout climb. Returns NULL when none exists or the engine
 * cannot be created. */
hunmin_ctx *hunmin_ctx_from_module(void);

/* Accept a stored layout id only when the engine can instantiate it AND
 * its mode matches the slot. A stale registry value naming a latin
 * layout in the hangul slot degrades to the default. */
void hunmin_tsf_resolve_layout_slot(const hunmin_ctx *ctx, const std::string &stored, const char *def, bool wantHangul, std::string *outId);

#endif /* HUNMIN_TSF_DATADIR_H_ */
