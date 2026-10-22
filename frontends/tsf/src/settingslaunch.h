/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The ruled settings surface's launcher (issue #71): resolve the WinUI 3
 * settings exe (HUNMIN_TSF_SETTINGS_EXE, the deploy shapes, the installed
 * shape), launch it and wait for its exit - the property-sheet contract,
 * because the exe's exit is the store's commit point.
 */
#ifndef HUNMIN_TSF_SETTINGSLAUNCH_H_
#define HUNMIN_TSF_SETTINGSLAUNCH_H_

#include <windows.h>

/* TRUE when the exe ran (its exit code is logged); false when it could not
 * be resolved or launched. exeResolved may be null/0. */
bool hunmin_tsf_settings_exe_run(HWND parent, wchar_t *exeResolved, size_t cap);

#endif /* HUNMIN_TSF_SETTINGSLAUNCH_H_ */
