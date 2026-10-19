/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1 (issue #67): the Hanja-key candidate window, driven over the mock
 * TSF layer exactly like tsf-trace-emit drives the buffering contract. The
 * adapter source is included as-is (private-public seam) so the compiled
 * logic is the linked DLL's logic, and the candidate state (candActive_/
 * candCount_/candCursor_/candTop_), the pop-up's hide path (no metrics in a
 * headless run) and the ONE insert the commit produces are all assertable.
 *
 * Composition expectations come from hunmin-sim, not from this file's
 * author: dubeolsik `dkssud` ends with the engine's commit buffer carrying
 * `안` (wordbuf_) and preedit `녕` ([s] row commits `안`, [d] row shows the
 * in-flight `녕`); `rkrkrk` ends with wordbuf_ `가가` and preedit `가`.
 * Those two lookups are the anchors: `안녕` has 2 candidates (both word),
 * `가가가` has 0 over the whole word but the LAST syllable `가` has 65
 * (the hanja-table fallback, issue #100), and `가디`/`디` are zero in BOTH
 * lookups — measured with hunmin_suggest_count against the shipped tables
 * on 2026-10-18.
 *
 * Registry hygiene: the layout seam is the env override
 * (HUNMIN_TSF_HANGUL_LAYOUT, an empty value means "no override"), so this
 * test never touches the production HKCU\Software\Hunmin\TSF store.
 *
 /* Hangul in expected strings is written as UTF-8 byte escapes (\xEC\x95\x88
 *  style): MSVC builds this TU under the ANSI codepage (949), where a raw
 *  UTF-8 literal is fatal (C2001) and a \uXXXX escape encodes through the
 *  codepage, not the engine's UTF-8 — both measured during this milestone's
 *  first run (docs/ENVIRONMENT.md pitfalls).
 *
 * Exit 0 clean, 1 assertion failed, 77 skip (engine data unavailable).
 *
 * GoogleTest form (issue #109, option 1b): one ctest entry is kept
 * (tsf-candidate-test); the original per-case Fixture blocks map 1:1 onto
 * TEST_F cases, with Begin()/Teardown() still explicit calls so the
 * case-15 env-before-Begin ordering and the mid-Begin rc-77 exit keep
 * their exact semantics. The CHECK macro is backed by ADD_FAILURE() so
 * every original message stays verbatim and non-fatal.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <windows.h>

#include "tsf-testdir.h"

extern "C" HINSTANCE g_hunmin_hinst = nullptr;

/* The GetKeyState seam, copied from trace-emit.cpp: applies over the whole
 * TU, so the adapter and the toggle matcher it includes see the mock. */
SHORT tsf_mock_GetKeyState(int vk);
SHORT g_mockKeyState[256] = {};
SHORT tsf_mock_GetKeyState(int vk) { return g_mockKeyState[vk & 0xFF]; }
#define GetKeyState tsf_mock_GetKeyState

#include "mock-tsf.h"
#define private public
#include "../src/tsf.cpp"
#undef private
#undef GetKeyState

/* printf-style format → a gtest failure line. Every original CHECK message
 * survives unchanged; only the reporting moves from a failures counter to
 * the framework's per-case result. */
static std::string tsf_fmt(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return std::string(buf);
}
#define CHECK(cond, ...)               \
    do {                               \
        if (!(cond)) {                 \
            ADD_FAILURE() << tsf_fmt(__VA_ARGS__); \
        }                              \
    } while (0)

static std::string utf8(const std::wstring &w) {
    if (w.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0,
                                nullptr, nullptr);
    std::string s(n > 0 ? (size_t)n : 0, '\0');
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

/* Anchors (see the header comment): */
static const char *kAnchorAn = "\xEC\x95\x88"; /* 안 */
static const char *kAnchorAnNyeng = "\xEC\x95\x88\xEB\x85\x95"; /* 안녕 */
static const char *kAnchorPreeditNyeng = "\xEB\x85\x95"; /* 녕 */
static const char *kAnchorPreeditNyee = "\xEB\x85\x80"; /* 녀 */
static const char *kAnchorPreeditNieun = "\xE3\x84\xB4"; /* ㄴ */
static const char *kAnchorGa = "\xEA\xB0\x80"; /* 가 */
static const char *kAnchorGaGa = "\xEA\xB0\x80\xEA\xB0\x80"; /* 가가 */
static const char *kAnchorGaGaGa = "\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80"; /* 가가가 */

struct CandidateTest : public ::testing::Test {
    tsfmock::MockThreadMgr *tm = nullptr;
    tsfmock::MockContext *ctx = nullptr;
    tsfmock::MockDocumentMgr *dm = nullptr;
    CTextService *svc = nullptr;

    void Begin() {
        _putenv_s("HUNMIN_DATA_DIR", HUNMIN_TSF_DATA_DIR);
        /* Layout seam: dubeolsik without touching the real store. */
        _putenv_s("HUNMIN_TSF_HANGUL_LAYOUT", "dubeolsik");
        _putenv_s("HUNMIN_TSF_LATIN_LAYOUT", "us-qwerty");
        /* Key seams: the DEFAULTS, so a store the user's live session
         * mutated (their first capture wrote HKCU HanjaKey=v1:229:0 —
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
        if (!svc->ctx_ || !svc->ic_) {
            fprintf(stderr, "skip: engine context unavailable (check HUNMIN_TSF_DATA_DIR)\n");
            Teardown();
            std::exit(77);
        }
    }

    void Teardown() {
        if (!svc)
            return;
        svc->Release(); /* frees ic_/ctx_; candWnd_ dtor runs with it */
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
     * base character (issue #106 — the adapter itself no longer decides). */
    void Type(const char *keys) {
        for (const char *p = keys; *p; ++p)
            Key((WPARAM)toupper((unsigned char)*p));
    }
    void AssertWordbuf(const char *utf8Expected, const char *what) {
        CHECK(svc->wordbuf_ == (utf8Expected ? utf8Of(utf8Expected) : std::wstring()),
              "%s: wordbuf got %s, want %s", what, utf8(svc->wordbuf_).c_str(),
              utf8Expected ? utf8Expected : "(empty)");
    }
    static std::wstring utf8Of(const char *u) {
        int n = MultiByteToWideChar(CP_UTF8, 0, u, -1, nullptr, 0);
        std::wstring w((size_t)(n > 1 ? n - 1 : 0), L'\0');
        if (n > 1)
            MultiByteToWideChar(CP_UTF8, 0, u, -1, &w[0], n);
        return w;
    }
    void AssertPreedit(const char *utf8Expected, const char *what) {
        std::string got(std::string(svc->ic_ ? hunmin_ic_preedit(svc->ic_) : ""));
        CHECK(got == (utf8Expected ? utf8Expected : ""), "%s: preedit got %s, want %s", what,
              got.c_str(), utf8Expected ? utf8Expected : "(empty)");
    }
    void AssertNoInsert(const char *what) {
        CHECK(ctx->insertions.empty(), "%s: %zu insertions happened, want none", what,
              ctx->insertions.size());
    }

    /* The composition channel (issue #98): a boundary that drains a
     * pending word now ENDS the composition whose range already holds
     * the whole word -- the fusing text is the commit, not a plain
     * SetText. Assert on that channel; never re-derive the commit by
     * extending it. */
    void AssertCompositionEnded(const char *utf8Expected, const char *what) {
        CHECK(ctx->compositionEnds >= 1, "%s: no composition ended", what);
        CHECK(!ctx->compositionActive, "%s: composition left active", what);
        CHECK(utf8(ctx->compositionText) == std::string(utf8Expected ? utf8Expected : ""),
              "%s: fused text got %s, want %s", what,
              utf8(ctx->compositionText).c_str(),
              utf8Expected ? utf8Expected : "(empty)");
    }
    void AssertCompositionText(const char *utf8Expected, const char *what) {
        CHECK(utf8(ctx->compositionText) == std::string(utf8Expected ? utf8Expected : ""),
              "%s: composition text got %s, want %s", what,
              utf8(ctx->compositionText).c_str(),
              utf8Expected ? utf8Expected : "(empty)");
    }
};

/* Type `dkssud`: wordbuf_ reaches `안`, preedit reaches `녕` (engine-measured). */
static void TypeAnNyeng(CandidateTest &f) {
    f.Type("dkssud");
    f.AssertWordbuf(kAnchorAn, "dkssud");
    f.AssertPreedit(kAnchorPreeditNyeng, "dkssud");
    f.AssertCompositionText(kAnchorAnNyeng, "dkssud (composition)");
}

/* 1. Empty buffer: the candidate key belongs to the client, in test
 * and down alike (default: the bare Hanja key, 0x19). */
TEST_F(CandidateTest, EmptyBufferKeyBelongsToTheClient) {
    Begin();
    BOOL testEat = TRUE, downEat = TRUE;
    TestKey(VK_HANJA, &testEat);
    Key(VK_HANJA, &downEat);
    CHECK(testEat == FALSE, "case 1: key test ate with no buffer");
    CHECK(downEat == FALSE, "case 1: key down ate with no buffer");
    CHECK(!svc->candActive_, "case 1: candidate state active after empty buffer key");
    AssertNoInsert("case 1");
    Teardown();
}

/* 2. `dkssud` + the Hanja key: the window opens over the whole
 * buffered word. */
TEST_F(CandidateTest, HanjaKeyOpensTheWindowOverTheWholeWord) {
    Begin();
    TypeAnNyeng(*this);
    BOOL testEat = FALSE;
    TestKey(VK_HANJA, &testEat);
    CHECK(testEat == TRUE, "case 2: Hanja-key test did not eat for `dkssud`");
    BOOL eat = TRUE;
    Key(VK_HANJA, &eat);
    CHECK(eat == TRUE, "case 2: Hanja-key down did not eat");
    CHECK(svc->candActive_, "case 2: window not active after the Hanja key");
    CHECK(svc->candCount_ == 1, "case 2: count %d, want 1 (krdict dedupe merged the sense rows, tools/krdict-dedupe.py)",
          svc->candCount_);
    CHECK(svc->candCursor_ == 0 && svc->candTop_ == 0, "case 2: cursor/top != 0/0");
    CHECK(svc->candSource_ == kAnchorAnNyeng, "case 2: anchored to %s, want the whole word",
          svc->candSource_.c_str());
    AssertNoInsert("case 2");
    /* 2b. While open, the window's keys are eaten in the test phase. */
    BOOL dEat = TRUE;
    TestKey('1', &dEat);
    CHECK(dEat == TRUE, "case 2b: digit '1' not eaten in test phase");
    Teardown();
}

/* 3. Return commits the cursor (c0): exactly one insert, the Hanja text,
 * window closed, buffer empty, engine reset. */
TEST_F(CandidateTest, ReturnCommitsTheCursor) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    std::string expected(hunmin_suggest_text(svc->ctx_, 0));
    CHECK(!expected.empty(), "case 3: candidate 0 text empty");
    BOOL eat = FALSE;
    Key(VK_RETURN, &eat);
    CHECK(eat == TRUE, "case 3: Return not consumed");
    AssertCompositionEnded(expected.c_str(), "case 3");
    CHECK(!svc->candActive_, "case 3: window still open after commit");
    AssertWordbuf(nullptr, "case 3");
    AssertPreedit(nullptr, "case 3");
    Teardown();
}

/* 4. Down moves the cursor; the moved cursor commits with Return.
 * (krdict-dedupe merged the two 安寧 rows to one, so a one-row list
 * clamps Down at the last row: cursor stays 0 and Return still commits
 * that row.) */
TEST_F(CandidateTest, DownClampsAndReturnStillCommits) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    Key(VK_DOWN);
    CHECK(svc->candCursor_ == 0, "case 4: cursor %d after Down on a 1-row list, want 0 (clamped)",
          svc->candCursor_);
    std::string expected(hunmin_suggest_text(svc->ctx_, 0));
    Key(VK_RETURN);
    AssertCompositionEnded(expected.c_str(), "case 4");
    Teardown();
}

/* 5. Up at the top stays put (clamped, no rotation, no insertion). */
TEST_F(CandidateTest, UpAtTheTopStaysPut) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    Key(VK_UP);
    CHECK(svc->candCursor_ == 0, "case 5: Up moved cursor below 0");
    AssertNoInsert("case 5");
    CHECK(svc->candActive_, "case 5: window closed by Up");
    Teardown();
}

/* 6. Digit select, dedupe edition: the two 安寧 senses merged into ONE
 * row (tools/krdict-dedupe.py), so digit '2' is now PAST the list end.
 * The measured rule for an out-of-range digit (the keys-of-passage
 * answer, same class as the unbound F-key and the edit keys): the list
 * closes, the pending word drains through the boundary flush, and the
 * digit itself passes to the app uneaten. */
TEST_F(CandidateTest, OutOfRangeDigitDrainsAndPasses) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    BOOL eat = TRUE;
    Key('2', &eat);
    CHECK(eat == FALSE, "case 6: digit '2' past the end was consumed");
    AssertCompositionEnded(kAnchorAnNyeng, "case 6");
    CHECK(!svc->candActive_, "case 6: window left open after the drain");
    Teardown();
}

/* 6b. In-range digit select commits the page-relative index — one-row
 * list: digit '1' commits row 0 (the only one). */
TEST_F(CandidateTest, InRangeDigitCommits) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    std::string expected(hunmin_suggest_text(svc->ctx_, 0));
    BOOL eat = TRUE;
    Key('1', &eat);
    CHECK(eat == TRUE, "case 6b: in-range digit '1' not consumed");
    AssertCompositionEnded(expected.c_str(), "case 6b");
    CHECK(!svc->candActive_, "case 6b: window left open after digit commit");
    Teardown();
}

/* 7. Esc closes without touching the composition; typing continues. */
TEST_F(CandidateTest, EscClosesAndTypingContinues) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    BOOL eat = FALSE;
    Key(VK_ESCAPE, &eat);
    CHECK(eat == TRUE, "case 7: Esc not consumed");
    CHECK(!svc->candActive_, "case 7: window open after Esc");
    AssertNoInsert("case 7");
    AssertWordbuf(kAnchorAn, "case 7");
    AssertPreedit(kAnchorPreeditNyeng, "case 7");
    /* Typing continues where the user left off — the key is consumed and
     * the preedit grows (engine composes; the exact glyph is the
     * engine's business, asserted nonempty). */
    Key('d');
    std::string now(hunmin_ic_preedit(svc->ic_));
    CHECK(!now.empty(), "case 7: preedit empty after resuming typing");
    AssertNoInsert("case 7 (resumed typing)");
    Teardown();
}

/* 8. Zero whole-word candidates, multi-syllable word: `rkrkrk` ->
 * `가가가` (0 hits over the word; the LAST syllable `가` has 65).
 * Issue #100: the fallback list OPENS, anchored to the last syllable,
 * and the key is eaten in both phases (one decision). */
TEST_F(CandidateTest, FallbackListOpensAnchoredToTheLastSyllable) {
    Begin();
    Type("rkrkrk");
    AssertWordbuf(kAnchorGaGa, "rkrkrk");
    AssertPreedit(kAnchorGa, "rkrkrk");
    BOOL testEat = FALSE, eat = FALSE;
    TestKey(VK_HANJA, &testEat);
    Key(VK_HANJA, &eat);
    CHECK(testEat == TRUE, "case 8: Hanja-key test did not eat over the fallback list");
    CHECK(eat == TRUE, "case 8: Hanja-key down did not eat over the fallback list");
    CHECK(svc->candActive_, "case 8: fallback list did not open");
    CHECK(svc->candFallback_, "case 8: list not marked as a syllable-fallback list");
    CHECK(svc->candSource_ == kAnchorGa, "case 8: anchored to %s, want the last syllable",
          svc->candSource_.c_str());
    CHECK(svc->candCount_ >= 11, "case 8: count %d, want the measured 65-row `가` list",
          svc->candCount_);
    AssertNoInsert("case 8");
    Teardown();
}

/* 9. A digit past the end of the count closes the window and falls
 * through: the key itself reaches the composition boundary, the whole
 * word inserts once, the digit is not swallowed. */
TEST_F(CandidateTest, DeadDigitClosesAndFallsThrough) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    BOOL eat = TRUE;
    Key('9', &eat);
    CHECK(eat == FALSE, "case 9: '9' swallowed (eat false expected)");
    CHECK(!svc->candActive_, "case 9: window survived the dead digit");
    AssertCompositionEnded(kAnchorAnNyeng, "case 9");
    Teardown();
}

/* 10. The mode toggle is a composition boundary with the window open:
 * the candidate list closes and the buffered word inserts once. */
TEST_F(CandidateTest, ModeToggleIsABoundaryWithTheWindowOpen) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    CHECK(svc->candActive_, "case 10: window did not open");
    Key(HUNMIN_TSF_VIRTUAL_HANGEUL);
    CHECK(!svc->candActive_, "case 10: window open after toggle");
    CHECK(!svc->hangulMode_, "case 10: mode did not switch");
    AssertCompositionEnded(kAnchorAnNyeng, "case 10");
    Teardown();
}

/* 11. Backspace edits the pending composition (the buffered-edit
 * contract), engine-measured: `녕` -> `녀` -> `ㄴ` -> empty, the last one
 * still consumed by the engine (no insert, the key never reaches the
 * app while the engine deletes). One MORE backspace hits the empty
 * preedit: the engine declines, the boundary inserts the buffered word
 * once, and the key passes. */
TEST_F(CandidateTest, BackspaceEditsThePendingComposition) {
    Begin();
    TypeAnNyeng(*this);
    BOOL eat = FALSE;
    Key(VK_BACK, &eat);
    CHECK(eat == TRUE, "case 11a: BS not consumed");
    AssertPreedit(kAnchorPreeditNyee, "case 11a");
    AssertWordbuf(kAnchorAn, "case 11a");
    AssertNoInsert("case 11a");
    Key(VK_BACK);
    AssertPreedit(kAnchorPreeditNieun, "case 11b");
    Key(VK_BACK, &eat);
    CHECK(eat == TRUE, "case 11c: BS on emptied syllable not consumed");
    AssertPreedit(nullptr, "case 11c");
    AssertNoInsert("case 11c");
    Key(VK_BACK, &eat);
    CHECK(eat == FALSE, "case 11d: BS past the empty preedit eaten anyway");
    AssertCompositionEnded(kAnchorAn, "case 11d");
    Teardown();
}

/* 12. Paging over a 65-candidate list (`가` through the hanja table):
 * PageDown moves top by the page and the cursor with it. */
TEST_F(CandidateTest, PageDownMovesTopAndCursor) {
    Begin();
    Type("rk");
    AssertPreedit(kAnchorGa, "case 12 (rk)");
    Key(VK_HANJA);
    CHECK(svc->candActive_, "case 12: window not active on single syllable");
    CHECK(svc->candCount_ >= 11, "case 12: count %d, want >= 11 for paging", svc->candCount_);
    Key(VK_NEXT);
    CHECK(svc->candTop_ == 10, "case 12: top %d after PageDown, want 10", svc->candTop_);
    CHECK(svc->candCursor_ == 10, "case 12: cursor %d after PageDown, want 10",
          svc->candCursor_);
    std::string expected(hunmin_suggest_text(svc->ctx_, 0 + 10));
    Key(VK_RETURN);
    AssertCompositionEnded(expected.c_str(), "case 12");
    Teardown();
}

/* 13. PageUp from the first page stays at 0 (clamped). */
TEST_F(CandidateTest, PageUpFromTheFirstPageStaysAtZero) {
    Begin();
    Type("rk");
    Key(VK_HANJA);
    Key(VK_PRIOR);
    CHECK(svc->candTop_ == 0, "case 13: top %d, want 0", svc->candTop_);
    CHECK(svc->candActive_, "case 13: window closed by PageUp");
    Teardown();
}

/* 14. F9 is deliberately unbound by default (maintainer instruction:
 * the Hanja key is the candidate toggle). MEASURED here: unbound F9
 * (VK 0x78) does not disappear into the printable fallback — it is a
 * key of passage like the edit keys (the #95 rule, extended to the
 * F1..F12 block this case forced): the pending word drains once, and
 * F9 reaches the application uneaten. */
TEST_F(CandidateTest, UnboundF9IsAKeyOfPassage) {
    Begin();
    TypeAnNyeng(*this);
    BOOL testEat = TRUE, eat = TRUE;
    TestKey(VK_F9, &testEat);
    Key(VK_F9, &eat);
    CHECK(testEat == FALSE, "case 14: unbound F9 eaten in test phase");
    CHECK(eat == FALSE, "case 14: unbound F9 eaten in down phase");
    CHECK(!svc->candActive_, "case 14: unbound F9 opened the window");
    AssertCompositionEnded(kAnchorAnNyeng, "case 14");
    CHECK(svc->wordbuf_.empty(), "case 14: nothing held after the F-key boundary");
    Teardown();
}

/* 15. The configured key is the store's word: the env seam sets F9
 * (v1:120:0) as the candidate key and the adapter honours it — both
 * phases mirror the open decision. */
TEST_F(CandidateTest, ConfiguredKeyIsTheStoresWord) {
    _putenv_s("HUNMIN_TSF_HANJA_KEY", "v1:120:0");
    Begin();
    TypeAnNyeng(*this);
    BOOL testEat = FALSE, eat = TRUE;
    TestKey(VK_F9, &testEat);
    Key(VK_F9, &eat);
    CHECK(testEat == TRUE, "case 15: configured F9 not eaten in test phase");
    CHECK(eat == TRUE, "case 15: configured F9 not eaten in down phase");
    CHECK(svc->candActive_, "case 15: configured F9 did not open the window");
    CHECK(svc->candCount_ == 1, "case 15: count %d walking the env seam (deduped)",
          svc->candCount_);
    _putenv_s("HUNMIN_TSF_HANJA_KEY", "");
    Teardown();
}

/* 16. Issue #100: a fallback list's commit replaces ONLY the last
 * syllable. `rkrkrk` -> the `가` fallback list; committing row 0 inserts
 * `가가` + the chosen Hanja (the word prefix stays Hangul), exactly one
 * insert, the buffer drains once like the ordinary commit. */
TEST_F(CandidateTest, FallbackCommitReplacesOnlyTheLastSyllable) {
    Begin();
    Type("rkrkrk");
    Key(VK_HANJA); /* opens the last-syllable fallback list (case 8) */
    CHECK(svc->candFallback_, "case 16: fallback list did not open");
    std::string expected(hunmin_suggest_text(svc->ctx_, 0));
    CHECK(!expected.empty(), "case 16: candidate 0 text empty against the data");
    if (!expected.empty()) {
        BOOL eat = FALSE;
        Key(VK_RETURN, &eat);
        CHECK(eat == TRUE, "case 16: Return not consumed on a fallback list");
        /* Issue #98: the commit is the composition's END (the candidate
         * replaces the composition the list anchored to), so the fused
         * text is observed on the composition channel, not the plain
         * edit-session insert channel (mock-tsf.h: insertions = one
         * entry per NON-composition SetText). Exactly one fused content
         * and zero plain inserts is the one-insert contract here. */
        CHECK(!svc->candActive_, "case 16: window still open after fallback commit");
        AssertCompositionEnded((std::string(kAnchorGaGa) + expected).c_str(), "case 16");
        AssertNoInsert("case 16");
        AssertWordbuf(nullptr, "case 16");
        AssertPreedit(nullptr, "case 16");
    }
    Teardown();
}

/* 17. Zero-and-zero (`rkel` -> `가디`: 0 whole-word, 0 on the last
 * syllable 디 — measured): the Hanja key passes through in both phases,
 * no window, no state, no insert. */
TEST_F(CandidateTest, ZeroAndZeroPassesThrough) {
    Begin();
    Type("rkel");
    CHECK(utf8(svc->wordbuf_) + std::string(hunmin_ic_preedit(svc->ic_)) ==
              std::string(kAnchorGa) + "\xEB\x94\x94",
          "case 17: composed buffer is not ga-di");
    AssertPreedit("\xEB\x94\x94", "case 17");
    BOOL testEat = TRUE, eat = TRUE;
    TestKey(VK_HANJA, &testEat);
    Key(VK_HANJA, &eat);
    CHECK(testEat == FALSE, "case 17: Hanja-key test ate on a zero-and-zero buffer");
    CHECK(eat == FALSE, "case 17: Hanja-key down ate on a zero-and-zero buffer");
    CHECK(!svc->candActive_, "case 17: state active on a zero-and-zero buffer");
    AssertNoInsert("case 17");
    Teardown();
}

/* 18. One-syllable zero (`el` -> `디`: 0 hits, and the fallback lookup
 * would be the same single syllable — word == last): pass-through in
 * both phases (issue #100 keeps one-syllable words as they were). */
TEST_F(CandidateTest, OneSyllableZeroPassesThrough) {
    Begin();
    Type("el");
    BOOL testEat = TRUE, eat = TRUE;
    TestKey(VK_HANJA, &testEat);
    Key(VK_HANJA, &eat);
    CHECK(testEat == FALSE, "case 18: Hanja-key test ate on a one-syllable zero");
    CHECK(eat == FALSE, "case 18: Hanja-key down ate on a one-syllable zero");
    CHECK(!svc->candActive_, "case 18: state active on a one-syllable zero");
    AssertNoInsert("case 18");
    Teardown();
}

/* 19. Issue #106: the shifted row is the ENGINE's decision now. The
 * adapter hands the unshifted base + the Shift bit to
 * hunmin_ic_process_key; the row comes from the loader's own US-QWERTY
 * map. Injected shift + VK_R ('r'): dubeolsik plain ㄱ, shift row ㄲ
 * (dubeolsik.json) — the adapter's deleted shift column used to decide
 * exactly this, so a regression here is the migration undone. */
TEST_F(CandidateTest, ShiftedRowIsTheEnginesDecision) {
    Begin();
    Type("r");
    AssertPreedit("\xE3\x84\xB1", "case 19: plain r is not gigyeong");
    Teardown();
}
TEST_F(CandidateTest, InjectedShiftSelectsTheShiftRow) {
    Begin();
    g_mockKeyState[VK_SHIFT & 0xFF] = (SHORT)0x8000;
    Key('R'); /* the VK the OS sends for the key; case is the engine's */
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    AssertPreedit("\xE3\x84\xB2", "case 19: shifted r is not ssang-gigyeong");
    AssertNoInsert("case 19");
    Teardown();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
