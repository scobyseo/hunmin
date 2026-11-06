/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The ruled settings surface's launcher (issue #71): the WinUI 3 settings
 * exe, resolved by the shared resolver and waited on (the property-sheet
 * contract: the exe's exit is the store's commit point).
 */
#include "settingslaunch.h"

#include "log.h"
#include "settingspath.h"

#include <windows.h>

/* ---- the WinUI 3 settings exe (maintainer ruling 2026-10-17: the
 * configuration surface is a SEPARATE Windows App SDK exe; the plain-Win32
 * dialog is interim and stays reachable only through the rundll32 export /
 * show-settings.bat until it is removed) ----
 * Path resolution is the shared one (settingspath.cpp; order in its
 * header). Show() must not return while the dialog is open (the
 * property-sheet contract, docs/WINDOWS-TSF-CONFIG.md): the process handle
 * is waited on. Nothing is re-applied until the exe exits — the exe writes
 * the HKCU store and the exit is the commit point. */
bool hunmin_tsf_settings_exe_run(HWND parent, wchar_t *exeResolved, size_t cap) {
    (void)parent; /* the exe owns its own window; TSF gives no meaningful one */
    wchar_t exe[MAX_PATH];
    if (!hunmin_tsf_settings_exe_find((const void *)&hunmin_tsf_settings_exe_run, exe, MAX_PATH))
        return false;
    if (exeResolved && cap) {
        wcsncpy_s(exeResolved, cap, exe, cap - 1);
    }

    hunmin_logger.log("settings-exe: launching %ls\n", exe);
    SHELLEXECUTEINFOW si = {};
    si.cbSize = sizeof(si);
    si.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    si.lpVerb = L"open";
    si.lpFile = exe;
    si.nShow = SW_SHOW;
    if (!ShellExecuteExW(&si) || si.hProcess == nullptr) {
        hunmin_logger.log("settings-exe: ShellExecuteEx FAILED err=%lu\n", GetLastError());
        return false;
    }
    /* Modal per the documented contract: do not return until the dialog
     * closes (the exe process exiting). */
    WaitForSingleObject(si.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(si.hProcess, &exitCode);
    CloseHandle(si.hProcess);
    hunmin_logger.log("settings-exe: exited %lu\n", (unsigned long)exitCode);
    return true;
}
