/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier D1: ModeSwitchAdapter unit tests.
 *
 * Tests the R8 shadow-layout adapter in isolation (no UI stack).
 * Oracle: hunmin-sim --trace expectations (tier A is authority).
 */
#include "modeswitch.h"
#include "testdir.h"
#include <cstdlib>
#include <cstring>
#include <fcitx-utils/log.h>
#include <fcitx-utils/testing.h>
#include <hunmin.h>
#include <string>
#include <vector>

using namespace fcitx;

static hunmin_ctx *g_ctx = nullptr;

static void setup() {
    const char *dataDir = getenv("HUNMIN_DATA_PATH");
    if (!dataDir) {
        dataDir = TESTING_SOURCE_DIR "/libhunmin/data";
    }
    const char *dirs[] = {dataDir};
    g_ctx = hunmin_ctx_new(dirs, 1);
    FCITX_ASSERT(g_ctx);
}

static void teardown() {
    if (g_ctx) {
        hunmin_ctx_free(g_ctx);
        g_ctx = nullptr;
    }
}

/* Feed keys to the adapter, then judge on us-qwerty. Returns the judged text. */
static std::string judgeWord(ModeSwitchAdapter &adapter, const std::vector<uint32_t> &keys, const std::string &latinLayout = "us-qwerty",
                             size_t minLen = 2) {
    for (uint32_t ch : keys) {
        adapter.record(ch);
    }
    return adapter.judge(latinLayout, minLen);
}

int main() {
    setupTestingEnvironmentPath(TESTING_BINARY_DIR, {}, {TEST_DATA_DIR});
    fcitx::Log::setLogRule("default=5,hunmin=5");
    setup();

    /* 0. The layout metadata this adapter's judgement rests on.
     *
     * judgeWord() judges on a hardcoded "us-qwerty", and the layouts themselves
     * now declare which half of the IME they belong to (`mode` in the layout
     * JSON, docs/SPEC-SCHEMA.md). Asserting the two agree is what stops that
     * string from quietly becoming a Hangul table: if a layout's mode moved, or
     * a second latin layout appeared, this fails instead of the feature judging
     * Korean text as if it were Latin.
     */
    {
        const int count = hunmin_ctx_layout_count(g_ctx);
        std::string latinId;
        int latinCount = 0, hangulCount = 0;
        for (int i = 0; i < count; i++) {
            const char *mode = hunmin_ctx_layout_mode(g_ctx, i);
            FCITX_ASSERT(mode != nullptr) << "layout " << i << " has no mode";
            const char *id = hunmin_ctx_layout_id(g_ctx, i);
            FCITX_ASSERT(id != nullptr);
            if (!strcmp(mode, "latin")) {
                latinId = id;
                latinCount++;
            } else {
                FCITX_ASSERT(!strcmp(mode, "hangul")) << "layout " << id << " has mode '" << mode << "', which is neither";
                hangulCount++;
            }
        }
        FCITX_ASSERT(latinCount >= 1) << "expected at least one latin layout, found " << latinCount;
        FCITX_ASSERT(latinId == "us-qwerty") << "us-qwerty must exist among latin layouts for this adapter to judge on it, but found '" << latinId
                                             << "'";
        // No assertion on the ORDER the scanner yields: it walks the directory
        // (ahnmatmae first), and the frontends sort by display name anyway. An
        // assertion here would pin an implementation detail of opendir() to a
        // test, which is how a harmless rescandir becomes a red build.
        FCITX_ASSERT(hangulCount > 1) << "the hangul half is suspiciously small";
    }

    /* 1. record() + judge() round-trip: feed "kbs" on sin3-p2, get "kbs" on us-qwerty */
    {
        ModeSwitchAdapter adapter(g_ctx);
        std::string result = judgeWord(adapter, {'k', 'b', 's'});
        FCITX_ASSERT(result == "kbs") << "Expected 'kbs', got '" << result << "'";
    }

    /* 2. discard() clears buffer */
    {
        ModeSwitchAdapter adapter(g_ctx);
        adapter.record('k');
        adapter.record('b');
        FCITX_ASSERT(adapter.size() == 2);
        adapter.discard();
        FCITX_ASSERT(adapter.empty());
        FCITX_ASSERT(adapter.size() == 0);
        FCITX_ASSERT(adapter.word().empty());
    }

    /* 3. empty() / size() / word() getters */
    {
        ModeSwitchAdapter adapter(g_ctx);
        FCITX_ASSERT(adapter.empty());
        FCITX_ASSERT(adapter.size() == 0);
        FCITX_ASSERT(adapter.word().empty());

        adapter.record('k');
        FCITX_ASSERT(!adapter.empty());
        FCITX_ASSERT(adapter.size() == 1);
        FCITX_ASSERT(adapter.word().size() == 1);
        FCITX_ASSERT(adapter.word()[0] == 'k');

        adapter.record('b');
        FCITX_ASSERT(adapter.size() == 2);
        FCITX_ASSERT(adapter.word().size() == 2);
        FCITX_ASSERT(adapter.word()[0] == 'k');
        FCITX_ASSERT(adapter.word()[1] == 'b');
    }

    /* 4. minLen short-word guard (R9): word shorter than minLen is never switched */
    {
        ModeSwitchAdapter adapter(g_ctx);
        /* Single key 'k' with minLen=2 (default) should return empty */
        std::string result = judgeWord(adapter, {'k'}, "us-qwerty", 2);
        FCITX_ASSERT(result.empty()) << "Single key with minLen=2 should return empty, got '" << result << "'";

        /* Same word with minLen=1 should return 'k' */
        ModeSwitchAdapter adapter2(g_ctx);
        result = judgeWord(adapter2, {'k'}, "us-qwerty", 1);
        FCITX_ASSERT(result == "k") << "Single key with minLen=1 should return 'k', got '" << result << "'";

        /* Two keys with minLen=2 should work */
        ModeSwitchAdapter adapter3(g_ctx);
        result = judgeWord(adapter3, {'k', 'b'}, "us-qwerty", 2);
        FCITX_ASSERT(result == "kb") << "Two keys with minLen=2 should return 'kb', got '" << result << "'";
    }

    /* 5. Non-existent latinLayout handling: error log, empty return */
    {
        ModeSwitchAdapter adapter(g_ctx);
        adapter.record('k');
        adapter.record('b');
        adapter.record('s');
        std::string result = adapter.judge("non-existent-layout", 2);
        FCITX_ASSERT(result.empty()) << "Non-existent layout should return empty, got '" << result << "'";
        /* Buffer should still be cleared even on error */
        FCITX_ASSERT(adapter.empty());
    }

    /* 6. Destructive-read fix: hunmin_ic_commit() must be read per-key during replay.
     *
     * The bug (fixed in cfe43f9): reading commit only once at the end loses earlier
     * commits. Replaying "qbs" on us-qwerty returned "s" instead of "qbs".
     *
     * This test ensures the per-key commit read is preserved. */
    {
        ModeSwitchAdapter adapter(g_ctx);
        /* "qbs" on us-qwerty: q -> 'q', b -> 'b', s -> 's' */
        std::string result = judgeWord(adapter, {'q', 'b', 's'});
        FCITX_ASSERT(result == "qbs") << "Destructive-read fix: expected 'qbs', got '" << result << "'";

        /* Also test with keys that would have produced multi-char commits on some layouts */
        ModeSwitchAdapter adapter2(g_ctx);
        result = judgeWord(adapter2, {'a', 'b', 'c'});
        FCITX_ASSERT(result == "abc") << "Expected 'abc', got '" << result << "'";
    }

    /* 7. Judge clears the buffer after use (whether successful or not) */
    {
        ModeSwitchAdapter adapter(g_ctx);
        adapter.record('k');
        adapter.record('b');
        adapter.record('s');
        std::string result = adapter.judge("us-qwerty", 2);
        FCITX_ASSERT(result == "kbs");
        FCITX_ASSERT(adapter.empty()) << "Buffer should be cleared after judge()";

        /* Calling judge again on empty buffer returns empty */
        result = adapter.judge("us-qwerty", 2);
        FCITX_ASSERT(result.empty());
    }

    /* 8. Mixed: record, judge, then record again (new word) */
    {
        ModeSwitchAdapter adapter(g_ctx);
        std::string result = judgeWord(adapter, {'h', 'e', 'l', 'l', 'o'});
        FCITX_ASSERT(result == "hello");

        /* New word after judge */
        result = judgeWord(adapter, {'w', 'o', 'r', 'l', 'd'});
        FCITX_ASSERT(result == "world");
    }

    teardown();
    return 0;
}