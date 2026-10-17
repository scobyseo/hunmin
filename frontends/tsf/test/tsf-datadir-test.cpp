/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1 (issue #66, data-loading half): hunmin_ctx_from_module() must
 * resolve the engine data from the module's own directory — the
 * "<module>\data" branch — with no environment help. The build stages
 * libhunmin/data next to this executable (frontends/tsf/test/CMakeLists.txt
 * POST_BUILD); this test clears HUNMIN_DATA_DIR (an empty value means unset
 * for the loader, the same rule the env branch itself applies) and asserts
 * the load, that every staged layout file shows up in the enumeration, and
 * that the fcitx5-default hangul layout instantiates.
 *
 * Exit 0 clean, 1 assertion failed, 77 skip (staging absent).
 */
#include <cstdio>
#include <cstring>
#include <windows.h>

#include "tsf-testdir.h"

/* Same white-box seam as trace-emit.cpp: the search chain under test is
 * static inside the adapter TU and deliberately not part of the ABI. */
extern "C" HINSTANCE g_hunmin_hinst = nullptr;
#define private public
#include "../src/tsf.cpp"
#undef private

static int countLayoutJson(const char *layoutsDirA) {
    char pattern[MAX_PATH * 2];
    snprintf(pattern, sizeof(pattern), "%s\\*.json", layoutsDirA);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    int n = 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            ++n;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return n;
}

int main() {
    /* Unset, not merely empty-of-meaning: GetEnvironmentVariableW returns 0
     * for an empty value, which is exactly what the loader's env branch
     * treats as "no override". */
    _putenv_s("HUNMIN_DATA_DIR", "");

    char exePath[MAX_PATH * 2];
    if (!GetModuleFileNameA(nullptr, exePath, (DWORD)sizeof(exePath))) {
        printf("tsf-datadir-test: cannot resolve own path\n");
        return 1;
    }
    char *slash = strrchr(exePath, '\\');
    if (!slash)
        slash = strrchr(exePath, '/');
    if (slash)
        *slash = '\0';
    const char *exeDir = exePath;

    /* The staged copy must exist: the POST_BUILD step is what makes the
     * shipped shape real, and its absence means the test would silently
     * verify the repo-climb fallback instead. */
    char staged[MAX_PATH * 2];
    snprintf(staged, sizeof(staged), "%s\\data\\layouts", exeDir);
    DWORD attr = GetFileAttributesA(staged);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        printf("skip: staged data missing at %s (POST_BUILD staging did not run)\n", staged);
        return 77;
    }

    hunmin_ctx *ctx = hunmin_ctx_from_module();
    if (!ctx) {
        printf("FAIL: hunmin_ctx_from_module() returned NULL with HUNMIN_DATA_DIR cleared\n");
        return 1;
    }

    const int files = countLayoutJson(staged);
    const int enumerated = hunmin_ctx_layout_count(ctx);
    if (files <= 0) {
        printf("FAIL: staged layouts dir holds %d json files\n", files);
        return 1;
    }
    if (files != enumerated) {
        printf("FAIL: staged layout files=%d but hunmin_ctx_layout_count=%d "
               "(a staged layout did not parse)\n",
               files, enumerated);
        hunmin_ctx_free(ctx);
        return 1;
    }

    hunmin_ic *ic = hunmin_ic_new(ctx, "sin3-p2");
    if (!ic) {
        printf("FAIL: hunmin_ic_new(sin3-p2) returned NULL against staged data\n");
        hunmin_ctx_free(ctx);
        return 1;
    }
    hunmin_ic_free(ic);

    /* The repo-climb fallback must not be what satisfied the load: the env
     * is cleared and the module dir branch is the only working path here.
     * Pin that the ctx's data actually came from beside us by keying one
     * layout that exists and asserting a known pair composes. */
    hunmin_ic *d = hunmin_ic_new(ctx, "dubeolsik");
    if (!d) {
        printf("FAIL: dubeolsik did not instantiate against staged data\n");
        hunmin_ctx_free(ctx);
        return 1;
    }
    /* dubeolsik maps 'r' to  ㄱ: the first key must be consumed and show in
     * preedit. If the ctx somehow resolved a different data tree, this pair
     * still holds for any dubeolsik-bearing tree, so it is a leap check for
     * pathological staging, not a layout assertion. */
    bool consumed = hunmin_ic_process(d, 'r');
    const char *pre = hunmin_ic_preedit(d);
    if (!consumed || !pre || !pre[0]) {
        printf("FAIL: dubeolsik 'r' inconsumed, preedit=\"%s\"\n", pre ? pre : "");
        hunmin_ic_free(d);
        hunmin_ctx_free(ctx);
        return 1;
    }
    hunmin_ic_free(d);
    hunmin_ctx_free(ctx);

    printf("ok: module-dir data load, %d layouts enumerated, dubeolsik composes\n", files);
    return 0;
}
