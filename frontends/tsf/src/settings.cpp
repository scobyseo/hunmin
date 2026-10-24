/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The rundll32 entry point (issue #71): the only C-linkage settings symbol
 * (the .def file and rundll32 resolve it by undecorated name). The dialog
 * implementation is settingsdialog.cpp.
 */
#include "settingsdialog.h"

#include "log.h"

#include <windows.h>

/* The standalone export is a rundll32 signature and must stay C-linkage
 * callable by name; the implementation lives in settingsdialog.cpp. */
void CALLBACK HunminSettingsShow(HWND hwnd, HINSTANCE hinst, LPSTR cmdLine, int nShow) {
    (void)hinst;
    (void)cmdLine;
    (void)nShow;
    hunmin_logger.log("HunminSettingsShow entry\n");
    hunmin_tsf_settings_dialog_run(hwnd);
}
