/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1 (issue #66, data-loading half): hunmin_ctx_from_module() must
 * resolve the engine data from the module's own directory — the
 * "<module>\data" branch — with no environment help. The build stages
 * libhunmin/data next to the test executable (frontends/tsf/test/
 * CMakeLists.txt POST_BUILD); this case clears HUNMIN_DATA_DIR (an empty
 * value means unset for the loader, the same rule the env branch itself
 * applies) and asserts the load, that every staged layout file shows up in
 * the enumeration, and that the fcitx5-default hangul layout instantiates.
 *
 * The staged-copy PRESENCE check is the wrapper's (tools/tsf-run-if-data.py,
 * --staged): its absence is the rc-77 skip, not a failure. Its parsing is
 * this case's business.
 *
 * Standalone TU (the include-.cpp seam is retired): the ctest entry
 * `tsf-datadir-test` selects this group with --gtest_filter=Datadir.*.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <windows.h>

#include "../src/settings.h"
#include "../src/tsf.h"

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

TEST(Datadir, ModuleDirDataLoadsWithNoEnvironmentHelp) {
    /* Unset, not merely empty-of-meaning: GetEnvironmentVariableW returns 0
     * for an empty value, which is exactly what the loader's env branch
     * treats as "no override". */
    _putenv_s("HUNMIN_DATA_DIR", "");

    char exePath[MAX_PATH * 2];
    ASSERT_TRUE(GetModuleFileNameA(nullptr, exePath, (DWORD)sizeof(exePath))) << "cannot resolve own path";
    char *slash = strrchr(exePath, '\\');
    if (!slash)
        slash = strrchr(exePath, '/');
    if (slash)
        *slash = '\0';
    const char *exeDir = exePath;

    hunmin_ctx *ctx = hunmin_ctx_from_module();
    ASSERT_NE(ctx, nullptr) << "hunmin_ctx_from_module() returned NULL with HUNMIN_DATA_DIR cleared";

    char staged[MAX_PATH * 2];
    snprintf(staged, sizeof(staged), "%s\\data\\layouts", exeDir);
    const int files = countLayoutJson(staged);
    const int enumerated = hunmin_ctx_layout_count(ctx);
    ASSERT_GT(files, 0) << "staged layouts dir holds " << files << " json files";
    ASSERT_EQ(files, enumerated) << "staged layout files=" << files << " but hunmin_ctx_layout_count=" << enumerated
                                 << " (a staged layout did not parse)";

    hunmin_ic *ic = hunmin_ic_new(ctx, "sin3-p2");
    ASSERT_NE(ic, nullptr) << "hunmin_ic_new(sin3-p2) returned NULL against staged data";
    hunmin_ic_free(ic);

    /* The repo-climb fallback must not be what satisfied the load: the env
     * is cleared and the module dir branch is the only working path here.
     * Pin that the ctx's data actually came from beside us by keying one
     * layout that exists and asserting a known pair composes. */
    hunmin_ic *d = hunmin_ic_new(ctx, "dubeolsik");
    ASSERT_NE(d, nullptr) << "dubeolsik did not instantiate against staged data";
    /* dubeolsik maps 'r' to  ㄱ: the first key must be consumed and show in
     * preedit. If the ctx somehow resolved a different data tree, this pair
     * still holds for any dubeolsik-bearing tree, so it is a leap check for
     * pathological staging, not a layout assertion. */
    bool consumed = hunmin_ic_process(d, 'r');
    const char *pre = hunmin_ic_preedit(d);
    EXPECT_TRUE(consumed && pre && pre[0]) << "dubeolsik 'r' inconsumed, preedit=\"" << (pre ? pre : "") << "\"";
    hunmin_ic_free(d);
    hunmin_ctx_free(ctx);
}
