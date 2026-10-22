/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef HUNMIN_TSF_SETTINGS_H_
#define HUNMIN_TSF_SETTINGS_H_

/* The settings surface (issue #71). Maintainer ruling 2026-10-17: the
 * configuration UI is a SEPARATE WinUI 3 exe (frontends/tsf/settings-ui,
 * Windows App SDK) — this TU keeps the DLL-side plumbing (the
 * ITfFunctionProvider / ITfFnConfigure objects, the Win32 row collector and
 * the interim plain-Win32 dialog, reachable only through the rundll32
 * export until it is removed).
 *
 * Reachability:
 *   1. TSF-native: the text service object is QueryInterface-able for
 *      ITfFunctionProvider; GetFunction(IID_ITfFnConfigure) returns the
 *      configure function whose Show() LAUNCHES THE EXE and waits on the
 *      process (the property-sheet contract: not returning while the dialog
 *      is open), then makes the LIVE adapter re-read the store (Apply
 *      without a restart). Exe resolution: HUNMIN_TSF_SETTINGS_EXE >
 *      <dll dir>\settings-ui\publish\ > <dll dir>\..\settings-ui\publish\;
 *      the interim Win32 dialog is NOT reachable from Show.
 *   2. Clone-independent direct launcher: frontends/tsf/show-settings.bat
 *      launches the same exe (resolved beside the DLL; InprocServer32 as
 *      fallback), falling back to the rundll32 HunminSettingsShow interim
 *      dialog while it exists.
 */

#include <windows.h>
#include "hunmin.h"
#include "settingsrows.h"

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

/* Launch the ruled settings surface (the WinUI 3 exe beside the DLL, the
 * rundll32 HunminSettingsShow fallback). Defined in tsf.cpp; the langbar
 * item's click/menu handlers call it with their own module address as the
 * resolution anchor. */
HRESULT hunmin_tsf_launch_settings(void *anchor);

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

/* The layout rows and hunmin_tsf_rows_for_mode live in settingsrows.h
 * (included above): the dialog's read half is its own TU now. */
#endif /* HUNMIN_TSF_SETTINGS_H_ */
