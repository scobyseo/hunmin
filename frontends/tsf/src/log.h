/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The TSF frontend's logging facility (extracted 2026-10-21 at the
 * maintainer's direction): ONE BaseLog TEMPLATE owns the three decisions
 * every log site shares, parameterized per log type by its tag:
 *
 *   - the GATE: the tag's env var (1 / on / yes / true), re-read per open
 *     so a running session can be flipped without a restart; no file is
 *     created while the gate is off (debug output is an OPTION, not an
 *     ambient side effect — maintainer instruction, 2026-10-17);
 *   - the DIRECTORY: HUNMIN_TSF_LOG_DIR when it names a real directory,
 *     else the serving DLL's own directory (the registered InprocServer32
 *     path is absolute, so the DLL's directory is the one clone-independent
 *     location every host resolves — never a drive root);
 *   - the WRITE: one gated call - open (append), write (narrow or wide per
 *     overload), flush, close. No FILE* discipline leaks out of the class.
 *
 * The log TYPES are the specializations: TsfLog (the adapter's debug log,
 * narrow) and RegLog (the registration chain's log, wide, with the reset()
 * the register head uses for the truncate the plain fopen("w") did).
 * The handles are hunmin_logger / hunmin_reg_logger; every site writes
 * through one of them.
 *
 * Header-only and included by tsf.cpp (the keybinding.cpp pattern): the
 * tier-F1 mock harness includes the adapter TU directly, so the facility
 * must ride the same translation unit - a .cpp target source would leave
 * the harness linking a second copy.
 */
#ifndef HUNMIN_TSF_LOG_H
#define HUNMIN_TSF_LOG_H

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>

#include <shlwapi.h>
#include <windows.h>

#pragma comment(lib, "Shlwapi.lib")

class BaseLogDirectory {
public:
    /* The directory for a log file name (never a drive root). STATIC so its
     * address stays a plain function pointer usable as the FROM_ADDRESS
     * module anchor (a member pointer is not). */
    static bool PathFor(const char *fname, char *out, size_t cap) {
        wchar_t dirW[MAX_PATH];
        wchar_t envW[MAX_PATH];
        DWORD envAttr;
        if (GetEnvironmentVariableW(L"HUNMIN_TSF_LOG_DIR", envW, MAX_PATH) > 0 && (envAttr = GetFileAttributesW(envW)) != INVALID_FILE_ATTRIBUTES &&
            (envAttr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
            lstrcpynW(dirW, envW, MAX_PATH);
        } else {
            HMODULE hMod = nullptr;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                    (LPCWSTR)(const void *)&PathFor, &hMod) ||
                !hMod)
                return false;
            if (GetModuleFileNameW(hMod, dirW, MAX_PATH) == 0)
                return false;
            if (!PathRemoveFileSpecW(dirW))
                return false;
        }
        char dirA[MAX_PATH * 3];
        if (WideCharToMultiByte(CP_ACP, 0, dirW, -1, dirA, sizeof(dirA), nullptr, nullptr) == 0)
            return false;
        int n = snprintf(out, cap, "%s\\%s", dirA, fname);
        return n > 0 && (size_t)n < cap;
    }
};

/* Tag contract: kFileName = the log file name; kEnvName = the gate env var. */
template <typename T> class BaseLog : public BaseLogDirectory {
public:
    static BaseLog &Instance() {
        static BaseLog log;
        return log;
    }

    /* The gate: re-read per call, so flipping the env var takes the next
     * open, not a restart of the DLL. */
    bool Enabled() const {
        char v[16];
        DWORD n = GetEnvironmentVariableA(T::kEnvName, v, (DWORD)sizeof(v));
        if (n == 0 || n >= sizeof(v))
            return false;
        for (DWORD i = 0; i < n && v[i]; ++i)
            v[i] = (char)tolower((unsigned char)v[i]);
        return strcmp(v, "1") == 0 || strcmp(v, "on") == 0 || strcmp(v, "yes") == 0 || strcmp(v, "true") == 0;
    }

    /* One gated line, narrow. */
    void log(const char *fmt, ...) {
        FILE *f = openFor_("a");
        if (!f)
            return;
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fflush(f);
        fclose(f);
    }

    /* One gated line, wide. */
    void log(const wchar_t *fmt, ...) {
        FILE *f = openFor_("a");
        if (!f)
            return;
        va_list ap;
        va_start(ap, fmt);
        vfwprintf(f, fmt, ap);
        va_end(ap);
        fflush(f);
        fclose(f);
    }

    /* Truncate the file; the head line's writer calls this first (the old
     * fopen "w"-mode open's only job). Gated like every write. */
    void reset(void) {
        if (!Enabled())
            return;
        FILE *f = openFor_("w");
        if (f)
            fclose(f);
    }

private:
    FILE *openFor_(const char *mode) {
        if (!Enabled())
            return nullptr;
        char path[MAX_PATH];
        if (!PathFor(T::kFileName, path, sizeof(path)))
            return nullptr;
        FILE *f = nullptr;
        return fopen_s(&f, path, mode) == 0 ? f : nullptr;
    }
};

struct TsfLogTag {
    static constexpr const char *kFileName = "hunmin-tsf-debug.log";
    static constexpr const char *kEnvName = "HUNMIN_TSF_DEBUG_LOG";
};
struct RegLogTag {
    static constexpr const char *kFileName = "hunmin-reg-debug.log";
    static constexpr const char *kEnvName = "HUNMIN_TSF_DEBUG_LOG";
};

using TsfLog = BaseLog<TsfLogTag>;
using RegLog = BaseLog<RegLogTag>;

/* The logging handles - the ONE logging syntax:
 * hunmin_logger.log("...") / hunmin_reg_logger.log(L"...") / .reset(). */
inline TsfLog &hunmin_logger = TsfLog::Instance();
inline RegLog &hunmin_reg_logger = RegLog::Instance();

#endif /* HUNMIN_TSF_LOG_H */
