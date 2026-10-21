/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The tier-F1 candidate fixture (issue #67): the mock setup, the key walk
 * and the assertion helpers, shared by the candidate tests
 * (tsf-candidate-test.cpp) and the legacy interim tests (tsf-legacy-test.cpp).
 * Header-only; each fragment keeps its own TEST_F definitions.
 *
 * Composition expectations come from hunmin-sim, not from this file's
 * author: dubeolsik `dkssud` ends with the engine's commit buffer carrying
 * `안` (wordbuf_) and preedit `녕` ([s] row commits `안`, [d] row shows the
 * in-flight `녕`); `rkrkrk` ends with wordbuf_ `가가` and preedit `가`.
 *
 * Registry hygiene: the layout seam is the env override
 * (HUNMIN_TSF_HANGUL_LAYOUT, an empty value means "no override"), so this
 * fixture never touches the production HKCU\Software\Hunmin\TSF store.
 */
#ifndef HUNMIN_TSF_TEST_CANDIDATE_FIXTURE_H_
#define HUNMIN_TSF_TEST_CANDIDATE_FIXTURE_H_

#include <gtest/gtest.h>

#include <cctype>
#include <cstring>
#include <string>
#include <windows.h>

#include "mock-tsf.h"
#include "tsf-keystate-mock.h"
#include "tsf-testdir.h"
#include "../src/tsf.h"

/* g_hunmin_hinst lives in the target's main TU (trace-emit.cpp / tsf-test.cpp). */

struct CandidateTest : public ::testing::Test {
    tsfmock::MockThreadMgr *tm = nullptr;
    tsfmock::MockContext *ctx = nullptr;
    tsfmock::MockDocumentMgr *dm = nullptr;
    CTextService *svc = nullptr;

    void Begin(const char *hangulLayout = "dubeolsik") {
        _putenv_s("HUNMIN_DATA_DIR", HUNMIN_TSF_DATA_DIR);
        /* Layout seam: dubeolsik without touching the real store; a case
         * needing another layout names it (case 21 replays the sin3-p2-yet
         * caps pair). */
        _putenv_s("HUNMIN_TSF_HANGUL_LAYOUT", hangulLayout);
        _putenv_s("HUNMIN_TSF_LATIN_LAYOUT", "us-qwerty");
        /* Key seams: the DEFAULTS, so a store the user's live session
         * mutated (their first capture wrote HKCU HanjaKey=v1:229:0 -
         * measured) cannot flip these cases' expectations. Per-case
         * _putenv_s still overrides (case 15 sets F9 BEFORE Begin(),
         * so the default only fills an UNSET variable). */
        {
            const char *have = std::getenv("HUNMIN_TSF_HANJA_KEY");
            if (!have)
                _putenv_s("HUNMIN_TSF_HANJA_KEY", "v1:25:0");
            have = std::getenv("HUNMIN_TSF_TOGGLE_KEYS");
            if (!have)
                _putenv_s("HUNMIN_TSF_TOGGLE_KEYS", "v1:21:0");
        }
        g_mockKeyState[VK_SHIFT & 0xFF] = 0;
        tsf_test_register_keystate();

        tm = new tsfmock::MockThreadMgr(); /* ref 1 */
        ctx = new tsfmock::MockContext(); /* ref 1 */
        dm = new tsfmock::MockDocumentMgr(ctx); /* ref 1 */
        ctx->AddRef(); /* dm holds the top reference */
        svc = new CTextService(); /* ref 1 */
        svc->Activate(tm, 1);
        if (!tm->keySink) {
            fprintf(stderr, "skip: Activate did not advise a key sink\n");
            Teardown();
            std::exit(77);
        }
        HunminTsfSnapshot snap = svc->Snapshot();
        if (!snap.ctx || !snap.ic) {
            fprintf(stderr, "skip: engine context unavailable (check HUNMIN_TSF_DATA_DIR)\n");
            Teardown();
            std::exit(77);
        }
    }

    void Teardown() {
        if (!svc)
            return;
        svc->Release(); /* frees ic_/ctx_; the candidate window closes with it */
        ctx->Release();
        dm->Release();
        tm->Release();
        svc = nullptr;
        ctx = nullptr;
        dm = nullptr;
        tm = nullptr;
    }

    void TestKey(WPARAM vk, BOOL *pfEat) {
        BOOL ignore = FALSE;
        svc->OnTestKeyDown(ctx, vk, 0, pfEat ? pfEat : &ignore);
    }
    void Key(WPARAM vk, BOOL *pfEat = nullptr) {
        BOOL testEat = FALSE, eat = FALSE;
        svc->OnTestKeyDown(ctx, vk, 0, &testEat);
        svc->OnKeyDown(ctx, vk, 0, &eat);
        if (pfEat)
            *pfEat = eat;
        (void)testEat;
    }
    /* One synthetic key per engine char (dubeolsik rows): the trace is
     * VK-based, so 'd' arrives as VK_D (0x44) and the engine derives the
     * shifted/unshifted row from the Shift bit the adapter passes beside the
     * base character (issue #106 - the adapter itself no longer decides). */
    void Type(const char *keys) {
        for (const char *p = keys; *p; ++p)
            Key((WPARAM)toupper((unsigned char)*p));
    }
    static std::wstring utf8Of(const char *u) {
        int n = MultiByteToWideChar(CP_UTF8, 0, u, -1, nullptr, 0);
        std::wstring w((size_t)(n > 1 ? n - 1 : 0), L'\0');
        if (n > 1)
            MultiByteToWideChar(CP_UTF8, 0, u, -1, &w[0], n);
        return w;
    }
    static std::string utf8(const std::wstring &w) {
        if (w.empty())
            return {};
        int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0,
                                    nullptr, nullptr);
        std::string s(n > 0 ? (size_t)n : 0, '\0');
        if (n > 0)
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n,
                                nullptr, nullptr);
        return s;
    }
    /* The observation channel: one snapshot at read time. */
    HunminTsfSnapshot Snap() { return svc->Snapshot(); }

    void AssertWordbuf(const char *utf8Expected, const char *what) {
        HunminTsfSnapshot s = Snap();
        EXPECT_EQ(s.wordbuf,
                  utf8Expected ? utf8Of(utf8Expected) : std::wstring())
            << what << ": wordbuf got " << utf8(s.wordbuf) << ", want "
            << (utf8Expected ? utf8Expected : "(empty)");
    }
    void AssertPreedit(const char *utf8Expected, const char *what) {
        HunminTsfSnapshot s = Snap();
        EXPECT_EQ(std::string(s.ic ? hunmin_ic_preedit(s.ic) : ""),
                  std::string(utf8Expected ? utf8Expected : ""))
            << what << ": preedit";
    }
    void AssertNoInsert(const char *what) {
        EXPECT_EQ(ctx->insertions.size(), (size_t)0)
            << what << ": insertions happened, want none";
    }

    /* The composition channel (issue #98): a boundary that drains a
     * pending word now ENDS the composition whose range already holds
     * the whole word - the fusing text is the commit, not a plain
     * SetText. Assert on that channel; never re-derive the commit by
     * extending it. */
    void AssertCompositionEnded(const char *utf8Expected, const char *what) {
        EXPECT_GE(ctx->compositionEnds, 1) << what << ": no composition ended";
        EXPECT_FALSE(ctx->compositionActive) << what << ": composition left active";
        EXPECT_EQ(utf8(ctx->compositionText),
                  std::string(utf8Expected ? utf8Expected : ""))
            << what << ": fused text got " << utf8(ctx->compositionText)
            << ", want " << (utf8Expected ? utf8Expected : "(empty)");
    }
    void AssertCompositionText(const char *utf8Expected, const char *what) {
        EXPECT_EQ(utf8(ctx->compositionText),
                  std::string(utf8Expected ? utf8Expected : ""))
            << what << ": composition text got " << utf8(ctx->compositionText)
            << ", want " << (utf8Expected ? utf8Expected : "(empty)");
    }
};

/* Anchors (see the tsf-candidate-test.cpp header): */
static const char *kAnchorAn = "\xEC\x95\x88"; /* 안 */
static const char *kAnchorAnNyeng = "\xEC\x95\x88\xEB\x85\x95"; /* 안녕 */
static const char *kAnchorPreeditNyeng = "\xEB\x85\x95"; /* 녕 */
static const char *kAnchorPreeditNyee = "\xEB\x85\x80"; /* 녀 */
static const char *kAnchorPreeditNieun = "\xE3\x84\xB4"; /* ㄴ */
static const char *kAnchorGa = "\xEA\xB0\x80"; /* 가 */
static const char *kAnchorGaGa = "\xEA\xB0\x80\xEA\xB0\x80"; /* 가가 */
static const char *kAnchorGaGaGa = "\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80"; /* 가가가 */

/* Type `dkssud`: wordbuf_ reaches `안`, preedit reaches `녕` (engine-measured). */
static void TypeAnNyeng(CandidateTest &f) {
    f.Type("dkssud");
    f.AssertWordbuf(kAnchorAn, "dkssud");
    f.AssertPreedit(kAnchorPreeditNyeng, "dkssud");
    f.AssertCompositionText(kAnchorAnNyeng, "dkssud (composition)");
}

#endif /* HUNMIN_TSF_TEST_CANDIDATE_FIXTURE_H_ */
