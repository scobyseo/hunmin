/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The TSF frontend's logging facility (extracted 2026-10-21 at the
 * maintainer's direction — tsf.cpp got too long and the logging thread ran
 * through it). ONE singleton owns the three decisions every log site shares:
 *
 *   - the GATE: HUNMIN_TSF_DEBUG_LOG (1 / on / yes / true), re-read per
 *     open so a running session can be flipped without a restart (the
 *     maintainer instruction, 2026-10-17: debug output is an option, not
 *     an ambient side effect — no file is created while the gate is off);
 *   - the DIRECTORY: HUNMIN_TSF_LOG_DIR (when it names a real directory)
 *     else the serving DLL's own directory — the registered InprocServer32
 *     path is absolute, so the DLL's directory is the one clone-independent
 *     location every host can resolve (never a drive root);
 *   - the FILE: one open per write site (append), closed by the site —
 *     the existing `if (dbg)` discipline stays callable through plain
 *     FILE*.
 *
 * Header-only and included by tsf.cpp (the keybinding.cpp pattern): the
 * tier-F1 mock harness includes the adapter TU directly, so the facility
 * must ride the same translation unit — a .cpp target source would leave
 * the harness linking a second copy.
 */
#ifndef HUNMIN_TSF_LOG_H
#define HUNMIN_TSF_LOG_H

#include <windows.h>
#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

class TsfLog {
  public:
    /* The process-wide instance: one instance, everywhere, because the
     * directory decision is shared state (a per-site decision would let
     * two hosts in one process drift apart). No other instance construction
     * pattern is offered on purpose — easier management is the point. */
    static TsfLog &Instance() {
        static TsfLog log;
        return log;
    }

    /* The gate: re-read per call, so flipping the env var takes a new host
     * process's NEXT open, not a restart of the DLL. */
    bool Enabled() const {
        char v[16];
        DWORD n = GetEnvironmentVariableA("HUNMIN_TSF_DEBUG_LOG", v, (DWORD)sizeof(v));
        if (n == 0 || n >= sizeof(v))
            return false;
        for (DWORD i = 0; i < n && v[i]; ++i)
            v[i] = (char)tolower((unsigned char)v[i]);
        return strcmp(v, "1") == 0 || strcmp(v, "on") == 0 ||
               strcmp(v, "yes") == 0 || strcmp(v, "true") == 0;
    }

    /* The directory for a log file name; false and no write when neither
     * the override nor the module anchor resolves. STATIC so its address
     * stays a plain function pointer usable as the FROM_ADDRESS module
     * anchor (a member pointer is not). */
    static bool PathFor(const char *fname, char *out, size_t cap) {
        wchar_t dirW[MAX_PATH];
        wchar_t envW[MAX_PATH];
        DWORD envAttr;
        if (GetEnvironmentVariableW(L"HUNMIN_TSF_LOG_DIR", envW, MAX_PATH) > 0 &&
            (envAttr = GetFileAttributesW(envW)) != INVALID_FILE_ATTRIBUTES &&
            (envAttr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
            lstrcpynW(dirW, envW, MAX_PATH);
        } else {
            /* The FROM_ADDRESS anchor is this very method: the address is
             * inside the serving DLL no matter which module alias it is
             * loaded under, and under DllMain's loader lock (the dllmain
             * attach line) it stays legal. */
            HMODULE hMod = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                    (LPCWSTR)(const void *)&TsfLog::PathFor,
                                    &hMod) ||
                !hMod)
                return false;
            if (GetModuleFileNameW(hMod, dirW, MAX_PATH) == 0)
                return false;
            if (!PathRemoveFileSpecW(dirW))
                return false;
        }
        char dirA[MAX_PATH * 3];
        if (WideCharToMultiByte(CP_ACP, 0, dirW, -1, dirA, sizeof(dirA), nullptr,
                                nullptr) == 0)
            return false;
        int n = snprintf(out, cap, "%s\\%s", dirA, fname);
        return n > 0 && (size_t)n < cap;
    }

    /* The site-facing open: null = the gate is off (or the path could not
     * resolve), and the site's `if (dbg)` skips. */
    FILE *Open(const char *fname, const char *mode = "a") {
        if (!Enabled())
            return nullptr;
        char path[MAX_PATH];
        if (!PathFor(fname, path, sizeof(path)))
            return nullptr;
        return fopen(path, mode);
    }
};

#endif /* HUNMIN_TSF_LOG_H */
