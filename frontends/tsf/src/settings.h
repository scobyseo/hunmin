/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef HUNMIN_TSF_SETTINGS_H_
#define HUNMIN_TSF_SETTINGS_H_

/* The settings dialog (issue #71): a Win32 window, plain API only, that edits
 * the configuration store (frontends/tsf/src/config.h) and shows the feature
 * rows. Deliberately a separate TU from tsf.cpp: the adapter is already far
 * past the ~700-line thin-frontend smell, and the dialog is presentation,
 * not input mapping.
 *
 * Two reachability paths, the same dialog:
 *   1. TSF-native: the text service object is QueryInterface-able for
 *      ITfFunctionProvider; GetFunction(IID_ITfFnConfigure) returns the
 *      configure function whose Show() runs this dialog modally and then
 *      makes the LIVE adapter re-read the store (Apply without a restart).
 *      Which Windows surfaces actually reach it is a measurement, not a
 *      claim - docs/WINDOWS-TSF-CONFIG.md carries the results.
 *   2. Clone-independent direct launcher: HunminSettingsShow is a rundll32
 *      export (frontends/tsf/show-settings.bat resolves the DLL from %~dp0
 *      first, the registered InprocServer32 path as fallback). No live
 *      service exists on this path, so Apply writes the store only and a
 *      running TIP sees it at its next Activate (config.h read order).
 */

#include <windows.h>
#include "hunmin.h"

/* The mode-toggle key bindings, Android KeyBinding.kt ported
 * (frontends/tsf/src/keybinding.h) - see above. */
#include "keybinding.h"

/* Language linkage: ONLY the rundll32 export is extern "C" (rundll32 and
 * the .def file resolve it by undecorated name); everything else is C++
 * internal, shared by tsf.cpp / settings.cpp / the tier-F1 harness — none
 * of it is part of the DLL's ABI beyond that one export and .def's four. */
extern "C" {
/* The rundll32 entry point signature: void CALLBACK(HWND, HINSTANCE, LPSTR, int). */
void CALLBACK HunminSettingsShow(HWND hwnd, HINSTANCE hinst, LPSTR cmdLine, int nShow);
}

/* Engine context for the dialog's layout lists. Same one-data-source-wins
 * rule as the adapter (env -> <module>\data -> repo climb); defined in
 * tsf.cpp so the dialog and the adapter cannot disagree about where data
 * comes from. */
hunmin_ctx *hunmin_ctx_from_module(void);

/* TSF-native bridge: the text service QIs for ITfFunctionProvider and gets
 * the provider object created here; svc stays a strongly referenced COM
 * pointer to the LIVE CTextService (tsf.cpp) so Apply can push the new
 * store into the running session through hunmin_tsf_apply_config_store()
 * (tsf.cpp, cast and re-apply). Returns an object with ref count 1. */
IUnknown *hunmin_tsf_settings_new_provider(void *svc);

/* tsf.cpp helper the ITfFnConfigure::Show path calls after the store was
 * saved: cast *svc back to CTextService and re-read config + rebuild
 * (Factory-style void* to keep the two TUs' headers from entangling). */
void hunmin_tsf_apply_config_store(void *svc);

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
int hunmin_tsf_rows_for_mode(hunmin_ctx *ctx, bool hangul, HunminTsfLayoutRow *rows,
                             int cap);
#endif /* HUNMIN_TSF_SETTINGS_H_ */
