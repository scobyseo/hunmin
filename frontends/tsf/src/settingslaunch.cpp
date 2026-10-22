/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The ruled settings surface's launcher (issue #71): the WinUI 3 settings
 * exe, resolved first-hit-wins and waited on (the property-sheet contract:
 * the exe's exit is the store's commit point).
 */
#include "settingslaunch.h"

#include "log.h"

#include <cstdio>
#include <cwchar>
#include <windows.h>

/* ---- the WinUI 3 settings exe (maintainer ruling 2026-10-17: the
 * configuration surface is a SEPARATE Windows App SDK exe; the plain-Win32
 * dialog is interim and stays reachable only through the rundll32 export /
 * show-settings.bat until it is removed) ----
 * Exe path resolution, first hit wins:
 *   1. environment HUNMIN_TSF_SETTINGS_EXE (test and override seam);
 *   2. <dll dir>\settings-ui\publish\hunmin-tsf-settingsui.exe — the
 *      deployed shape (<settings-ui.bat> publishes exactly that;
 *      frontends/tsf/settings-ui/build-settings-ui.bat);
 *   3. <dll dir>\..\settings-ui\publish\hunmin-tsf-settingsui.exe — the
 *      dev-checkout shape (DLL in frontends/tsf/build-msvc, exe beside the
 *      csproj).
 * Show() must not return while the dialog is open (the property-sheet
 * contract, docs/WINDOWS-TSF-CONFIG.md): the process handle is waited on.
 * Nothing is re-applied until the exe exits — the exe writes the HKCU store
 * and the exit is the commit point. */
bool hunmin_tsf_settings_exe_run(HWND parent, wchar_t *exeResolved, size_t cap) {
    (void)parent; /* the exe owns its own window; TSF gives no meaningful one */
    wchar_t exe[MAX_PATH];

    char opt[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("HUNMIN_TSF_SETTINGS_EXE", opt,
                                      (DWORD)sizeof(opt));
    if (n > 0 && n < sizeof(opt))
        MultiByteToWideChar(CP_UTF8, 0, opt, -1, exe, (int)sizeof(exe));
    else {
        HMODULE hSelf = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)(const void *)&hunmin_tsf_settings_exe_run, &hSelf);
        wchar_t dir[MAX_PATH];
        if (hSelf == nullptr ||
            GetModuleFileNameW(hSelf, dir, (DWORD)sizeof(dir)) == 0) {
            hunmin_logger.log("settings-exe: module path FAILED err=%lu\n", GetLastError());
            return false;
        }
        *wcsrchr(dir, L'\\') = L'\0';
        /* First hit wins: the dev publish shape, the dev-checkout climb,
         * then the INSTALLED shape (the .iss stages the publish content
         * directly under {app}\settings-ui — measured in the installer
         * sandbox round: without the third candidate the installed
         * ITfFnConfigure::Show found no exe and logged a miss). */
        swprintf(exe, MAX_PATH, L"%ls\\settings-ui\\publish\\hunmin-tsf-settingsui.exe",
                 dir);
        if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES)
            swprintf(exe, MAX_PATH, L"%ls\\..\\settings-ui\\publish\\hunmin-tsf-settingsui.exe",
                     dir);
        if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES)
            swprintf(exe, MAX_PATH, L"%ls\\settings-ui\\hunmin-tsf-settingsui.exe",
                     dir);
        if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) {
            hunmin_logger.log("settings-exe: not found under %ls\n", dir);
            return false;
        }
    }
    if (exeResolved && cap) {
        wcsncpy(exeResolved, exe, cap - 1);
        exeResolved[cap - 1] = L'\0';
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
