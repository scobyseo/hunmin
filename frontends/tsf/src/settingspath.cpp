/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * settingspath.cpp - the shared settings-exe resolver (stage S), replacing
 * the three divergent copies. Candidate order is the maintainer's
 * deployed-first ruling: the installed shape outranks checkout climbs, so
 * every launcher (the langbar, `ITfFnConfigure`) finds the same installed
 * WinUI exe instead of falling back to rundll32. The Start-menu entry is the
 * installer's absolute-path [Icons] entry and never passes through here
 * (ruling of 2026-10-30, issue #146).
 */
#include "settingspath.h"

#include "log.h"

#include <cwchar>
#include <windows.h>

namespace {

bool Exists(const wchar_t *path) { return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES; }

/* First hit wins, deployed shapes before climbs. Every candidate is
 * normalised with GetFullPathNameW so a ".." segment in the log and in
 * the shell link reads as the real path. */
bool FindInDir(const wchar_t *dllDir, wchar_t *exe, size_t cap) {
    const wchar_t *const kCandidates[] = {
        L"%s\\settings-ui\\publish\\hunmin-tsf-settingsui.exe",
        L"%s\\settings-ui\\hunmin-tsf-settingsui.exe",
        L"%s\\..\\settings-ui\\publish\\hunmin-tsf-settingsui.exe",
        L"%s\\..\\..\\settings-ui\\publish\\hunmin-tsf-settingsui.exe",
    };
    for (const wchar_t *fmt : kCandidates) {
        wchar_t cand[2 * MAX_PATH];
        _snwprintf(cand, _countof(cand), fmt, dllDir);
        if (!Exists(cand))
            continue;
        wchar_t full[MAX_PATH];
        if (GetFullPathNameW(cand, MAX_PATH, full, nullptr)) {
            wcsncpy(exe, full, cap - 1);
            exe[cap - 1] = L'\0';
        } else {
            wcsncpy(exe, cand, cap - 1);
            exe[cap - 1] = L'\0';
        }
        return true;
    }
    exe[0] = L'\0';
    hunmin_logger.log("settings-exe: not found under %ls\n", dllDir);
    return false;
}

} // namespace

bool hunmin_tsf_settings_exe_find(const void *anchor, wchar_t *exe, size_t cap) {
    if (!exe || cap == 0)
        return false;
    exe[0] = L'\0';

    /* The override seam is UTF-8 (the convention every other setting uses)
     * and wins over every layout, including the installed one. */
    char opt[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("HUNMIN_TSF_SETTINGS_EXE", opt, (DWORD)sizeof(opt));
    if (n > 0 && n < sizeof(opt)) {
        MultiByteToWideChar(CP_UTF8, 0, opt, -1, exe, (int)cap);
        return true;
    }

    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)anchor, &self) || !self) {
        hunmin_logger.log("settings-exe: module lookup FAILED err=%lu\n", GetLastError());
        return false;
    }
    wchar_t dll[MAX_PATH];
    if (GetModuleFileNameW(self, dll, MAX_PATH) == 0) {
        hunmin_logger.log("settings-exe: module path FAILED err=%lu\n", GetLastError());
        return false;
    }
    wchar_t dir[MAX_PATH];
    wcsncpy(dir, dll, MAX_PATH - 1);
    dir[MAX_PATH - 1] = L'\0';
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (!slash)
        return false;
    *slash = L'\0';
    return FindInDir(dir, exe, cap);
}
