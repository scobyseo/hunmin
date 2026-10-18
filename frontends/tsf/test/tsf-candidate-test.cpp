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
  * style): MSVC builds this TU under the ANSI codepage (949), where a raw
  * UTF-8 literal is fatal (C2001) and a \uXXXX escape encodes through the
  * codepage, not the engine's UTF-8 — both measured during this milestone's
  * first run (docs/ENVIRONMENT.md pitfalls).
  *
  * Exit 0 clean, 1 assertion failed, 77 skip (engine data unavailable).
  */
#include <cstdio>
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

static int failures = 0;
#define CHECK(cond, ...)                  \
    do {                                  \
        if (!(cond)) {                    \
            printf("FAIL: " __VA_ARGS__); \
            printf("\n");                 \
            ++failures;                   \
        }                                 \
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

struct Fixture {
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
         * _putenv_s still overrides (case 15 sets F9 BEFORE Fixture
         * construction, so the default only fills an UNSET variable). */
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
            printf("skip: Activate did not advise a key sink\n");
            Teardown();
            std::exit(77);
        }
        if (!svc->ctx_ || !svc->ic_) {
            printf("skip: engine context unavailable (check HUNMIN_TSF_DATA_DIR)\n");
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
     * VK-based, so 'd' arrives as VK_D (0x44) and the adapter derives the
     * shifted/unshifted row from the Shift bit itself. */
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
};

/* Type `dkssud`: wordbuf_ reaches `안`, preedit reaches `녕` (engine-measured). */
static void TypeAnNyeng(Fixture &f) {
    f.Type("dkssud");
    f.AssertWordbuf(kAnchorAn, "dkssud");
    f.AssertPreedit(kAnchorPreeditNyeng, "dkssud");
}

int main() {
    /* 1. Empty buffer: the candidate key belongs to the client, in test
     * and down alike (default: the bare Hanja key, 0x19). */
    {
        Fixture f;
        f.Begin();
        BOOL testEat = TRUE, downEat = TRUE;
        f.TestKey(VK_HANJA, &testEat);
        f.Key(VK_HANJA, &downEat);
        CHECK(testEat == FALSE, "case 1: key test ate with no buffer");
        CHECK(downEat == FALSE, "case 1: key down ate with no buffer");
        CHECK(!f.svc->candActive_, "case 1: candidate state active after empty buffer key");
        f.AssertNoInsert("case 1");
        f.Teardown();
    }

    /* 2. `dkssud` + the Hanja key: the window opens over the whole
     * buffered word. */
    {
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        BOOL testEat = FALSE;
        f.TestKey(VK_HANJA, &testEat);
        CHECK(testEat == TRUE, "case 2: Hanja-key test did not eat for `dkssud`");
        BOOL eat = TRUE;
        f.Key(VK_HANJA, &eat);
        CHECK(eat == TRUE, "case 2: Hanja-key down did not eat");
        CHECK(f.svc->candActive_, "case 2: window not active after the Hanja key");
        CHECK(f.svc->candCount_ == 1, "case 2: count %d, want 1 (krdict dedupe merged the sense rows, tools/krdict-dedupe.py)",
              f.svc->candCount_);
        CHECK(f.svc->candCursor_ == 0 && f.svc->candTop_ == 0, "case 2: cursor/top != 0/0");
        CHECK(f.svc->candSource_ == kAnchorAnNyeng, "case 2: anchored to %s, want the whole word",
              f.svc->candSource_.c_str());
        f.AssertNoInsert("case 2");
        /* 2b. While open, the window's keys are eaten in the test phase. */
        BOOL dEat = TRUE;
        f.TestKey('1', &dEat);
        CHECK(dEat == TRUE, "case 2b: digit '1' not eaten in test phase");
        f.Teardown();
    }

    /* 3. Return commits the cursor (c0): exactly one insert, the Hanja text,
     * window closed, buffer empty, engine reset. */
    {
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        f.Key(VK_HANJA);
        std::string expected(hunmin_suggest_text(f.svc->ctx_, 0));
        CHECK(!expected.empty(), "case 3: candidate 0 text empty");
        BOOL eat = FALSE;
        f.Key(VK_RETURN, &eat);
        CHECK(eat == TRUE, "case 3: Return not consumed");
        CHECK(f.ctx->insertions.size() == 1, "case 3: %zu insertions, want 1",
              f.ctx->insertions.size());
        if (f.ctx->insertions.size() == 1)
            CHECK(utf8(f.ctx->insertions[0]) == expected, "case 3: inserted %s, want %s",
                  utf8(f.ctx->insertions[0]).c_str(), expected.c_str());
        CHECK(!f.svc->candActive_, "case 3: window still open after commit");
        f.AssertWordbuf(nullptr, "case 3");
        f.AssertPreedit(nullptr, "case 3");
        f.Teardown();
    }

    /* 4. Down moves the cursor; the moved cursor commits with Return.
     * (krdict-dedupe merged the two 安寧 rows to one, so a one-row list
     * clamps Down at the last row: cursor stays 0 and Return still commits
     * that row.) */
    {
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        f.Key(VK_HANJA);
        f.Key(VK_DOWN);
        CHECK(f.svc->candCursor_ == 0, "case 4: cursor %d after Down on a 1-row list, want 0 (clamped)",
              f.svc->candCursor_);
        std::string expected(hunmin_suggest_text(f.svc->ctx_, 0));
        f.Key(VK_RETURN);
        CHECK(f.ctx->insertions.size() == 1, "case 4: %zu insertions, want 1",
              f.ctx->insertions.size());
        if (f.ctx->insertions.size() == 1)
            CHECK(utf8(f.ctx->insertions[0]) == expected, "case 4: inserted %s, want %s",
                  utf8(f.ctx->insertions[0]).c_str(), expected.c_str());
        f.Teardown();
    }

    /* 5. Up at the top stays put (clamped, no rotation, no insertion). */
    {
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        f.Key(VK_HANJA);
        f.Key(VK_UP);
        CHECK(f.svc->candCursor_ == 0, "case 5: Up moved cursor below 0");
        f.AssertNoInsert("case 5");
        CHECK(f.svc->candActive_, "case 5: window closed by Up");
        f.Teardown();
    }

    /* 6. Digit select, dedupe edition: the two 安寧 senses merged into ONE
     * row (tools/krdict-dedupe.py), so digit '2' is now PAST the list end.
     * The measured rule for an out-of-range digit (the keys-of-passage
     * answer, same class as the unbound F-key and the edit keys): the list
     * closes, the pending word drains through the boundary flush, and the
     * digit itself passes to the app uneaten. */
    {
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        f.Key(VK_HANJA);
        BOOL eat = TRUE;
        f.Key('2', &eat);
        CHECK(eat == FALSE, "case 6: digit '2' past the end was consumed");
        CHECK(f.ctx->insertions.size() == 1, "case 6: %zu insertions, want 1 (the drained word)",
              f.ctx->insertions.size());
        if (f.ctx->insertions.size() == 1)
            CHECK(utf8(f.ctx->insertions[0]) == kAnchorAnNyeng,
                  "case 6: inserted %s, want the drained word",
                  utf8(f.ctx->insertions[0]).c_str());
        CHECK(!f.svc->candActive_, "case 6: window left open after the drain");
        f.Teardown();
    }

    /* 6b. In-range digit select commits the page-relative index — one-row
     * list: digit '1' commits row 0 (the only one). */
    {
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        f.Key(VK_HANJA);
        std::string expected(hunmin_suggest_text(f.svc->ctx_, 0));
        BOOL eat = TRUE;
        f.Key('1', &eat);
        CHECK(eat == TRUE, "case 6b: in-range digit '1' not consumed");
        if (f.ctx->insertions.size() == 1)
            CHECK(utf8(f.ctx->insertions[0]) == expected, "case 6b: inserted %s, want %s",
                  utf8(f.ctx->insertions[0]).c_str(), expected.c_str());
        CHECK(!f.svc->candActive_, "case 6b: window left open after digit commit");
        f.Teardown();
    }

    /* 7. Esc closes without touching the composition; typing continues. */
    {
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        f.Key(VK_HANJA);
        BOOL eat = FALSE;
        f.Key(VK_ESCAPE, &eat);
        CHECK(eat == TRUE, "case 7: Esc not consumed");
        CHECK(!f.svc->candActive_, "case 7: window open after Esc");
        f.AssertNoInsert("case 7");
        f.AssertWordbuf(kAnchorAn, "case 7");
        f.AssertPreedit(kAnchorPreeditNyeng, "case 7");
        /* Typing continues where the user left off — the key is consumed and
         * the preedit grows (engine composes; the exact glyph is the
         * engine's business, asserted nonempty). */
        f.Key('d');
        std::string now(hunmin_ic_preedit(f.svc->ic_));
        CHECK(!now.empty(), "case 7: preedit empty after resuming typing");
        f.AssertNoInsert("case 7 (resumed typing)");
        f.Teardown();
    }

    /* 8. Zero whole-word candidates, multi-syllable word: `rkrkrk` ->
     * `가가가` (0 hits over the word; the LAST syllable `가` has 65).
     * Issue #100: the fallback list OPENS, anchored to the last syllable,
     * and the key is eaten in both phases (one decision). */
    {
        Fixture f;
        f.Begin();
        f.Type("rkrkrk");
        f.AssertWordbuf(kAnchorGaGa, "rkrkrk");
        f.AssertPreedit(kAnchorGa, "rkrkrk");
        BOOL testEat = FALSE, eat = FALSE;
        f.TestKey(VK_HANJA, &testEat);
        f.Key(VK_HANJA, &eat);
        CHECK(testEat == TRUE, "case 8: Hanja-key test did not eat over the fallback list");
        CHECK(eat == TRUE, "case 8: Hanja-key down did not eat over the fallback list");
        CHECK(f.svc->candActive_, "case 8: fallback list did not open");
        CHECK(f.svc->candFallback_, "case 8: list not marked as a syllable-fallback list");
        CHECK(f.svc->candSource_ == kAnchorGa, "case 8: anchored to %s, want the last syllable",
              f.svc->candSource_.c_str());
        CHECK(f.svc->candCount_ >= 11, "case 8: count %d, want the measured 65-row `가` list",
              f.svc->candCount_);
        f.AssertNoInsert("case 8");
        f.Teardown();
    }

    /* 9. A digit past the end of the count closes the window and falls
     * through: the key itself reaches the composition boundary, the whole
     * word inserts once, the digit is not swallowed. */
    {
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        f.Key(VK_HANJA);
        BOOL eat = TRUE;
        f.Key('9', &eat);
        CHECK(eat == FALSE, "case 9: '9' swallowed (eat false expected)");
        CHECK(!f.svc->candActive_, "case 9: window survived the dead digit");
        CHECK(f.ctx->insertions.size() == 1, "case 9: %zu insertions, want 1",
              f.ctx->insertions.size());
        if (f.ctx->insertions.size() == 1)
            CHECK(utf8(f.ctx->insertions[0]) == kAnchorAnNyeng,
                  "case 9: inserted %s, want the whole word", utf8(f.ctx->insertions[0]).c_str());
        f.Teardown();
    }

    /* 10. The mode toggle is a composition boundary with the window open:
     * the candidate list closes and the buffered word inserts once. */
    {
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        f.Key(VK_HANJA);
        CHECK(f.svc->candActive_, "case 10: window did not open");
        f.Key(HUNMIN_TSF_VIRTUAL_HANGEUL);
        CHECK(!f.svc->candActive_, "case 10: window open after toggle");
        CHECK(!f.svc->hangulMode_, "case 10: mode did not switch");
        CHECK(f.ctx->insertions.size() == 1, "case 10: %zu insertions, want 1",
              f.ctx->insertions.size());
        if (f.ctx->insertions.size() == 1)
            CHECK(utf8(f.ctx->insertions[0]) == kAnchorAnNyeng, "case 10: insert %s",
                  utf8(f.ctx->insertions[0]).c_str());
        f.Teardown();
    }

    /* 11. Backspace edits the pending composition (the buffered-edit
     * contract), engine-measured: `녕` -> `녀` -> `ㄴ` -> empty, the last one
     * still consumed by the engine (no insert, the key never reaches the
     * app while the engine deletes). One MORE backspace hits the empty
     * preedit: the engine declines, the boundary inserts the buffered word
     * once, and the key passes. */
    {
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        BOOL eat = FALSE;
        f.Key(VK_BACK, &eat);
        CHECK(eat == TRUE, "case 11a: BS not consumed");
        f.AssertPreedit(kAnchorPreeditNyee, "case 11a");
        f.AssertWordbuf(kAnchorAn, "case 11a");
        f.AssertNoInsert("case 11a");
        f.Key(VK_BACK);
        f.AssertPreedit(kAnchorPreeditNieun, "case 11b");
        f.Key(VK_BACK, &eat);
        CHECK(eat == TRUE, "case 11c: BS on emptied syllable not consumed");
        f.AssertPreedit(nullptr, "case 11c");
        f.AssertNoInsert("case 11c");
        f.Key(VK_BACK, &eat);
        CHECK(eat == FALSE, "case 11d: BS past the empty preedit eaten anyway");
        CHECK(f.ctx->insertions.size() == 1, "case 11d: %zu insertions, want 1",
              f.ctx->insertions.size());
        if (f.ctx->insertions.size() == 1)
            CHECK(utf8(f.ctx->insertions[0]) == kAnchorAn, "case 11d: insert %s",
                  utf8(f.ctx->insertions[0]).c_str());
        f.Teardown();
    }

    /* 12. Paging over a 65-candidate list (`가` through the hanja table):
     * PageDown moves top by the page and the cursor with it. */
    {
        Fixture f;
        f.Begin();
        f.Type("rk");
        f.AssertPreedit(kAnchorGa, "case 12 (rk)");
        f.Key(VK_HANJA);
        CHECK(f.svc->candActive_, "case 12: window not active on single syllable");
        CHECK(f.svc->candCount_ >= 11, "case 12: count %d, want >= 11 for paging", f.svc->candCount_);
        f.Key(VK_NEXT);
        CHECK(f.svc->candTop_ == 10, "case 12: top %d after PageDown, want 10", f.svc->candTop_);
        CHECK(f.svc->candCursor_ == 10, "case 12: cursor %d after PageDown, want 10",
              f.svc->candCursor_);
        std::string expected(hunmin_suggest_text(f.svc->ctx_, 0 + 10));
        f.Key(VK_RETURN);
        CHECK(f.ctx->insertions.size() == 1, "case 12: %zu insertions, want 1",
              f.ctx->insertions.size());
        if (f.ctx->insertions.size() == 1)
            CHECK(utf8(f.ctx->insertions[0]) == expected, "case 12: inserted %s, want %s",
                  utf8(f.ctx->insertions[0]).c_str(), expected.c_str());
        f.Teardown();
    }

    /* 13. PageUp from the first page stays at 0 (clamped). */
    {
        Fixture f;
        f.Begin();
        f.Type("rk");
        f.Key(VK_HANJA);
        f.Key(VK_PRIOR);
        CHECK(f.svc->candTop_ == 0, "case 13: top %d, want 0", f.svc->candTop_);
        CHECK(f.svc->candActive_, "case 13: window closed by PageUp");
        f.Teardown();
    }

    /* 14. F9 is deliberately unbound by default (maintainer instruction:
     * the Hanja key is the candidate toggle). MEASURED here: unbound F9
     * (VK 0x78) does not disappear into the printable fallback — it is a
     * key of passage like the edit keys (the #95 rule, extended to the
     * F1..F12 block this case forced): the pending word drains once, and
     * F9 reaches the application uneaten. */
    {
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        BOOL testEat = TRUE, eat = TRUE;
        f.TestKey(VK_F9, &testEat);
        f.Key(VK_F9, &eat);
        CHECK(testEat == FALSE, "case 14: unbound F9 eaten in test phase");
        CHECK(eat == FALSE, "case 14: unbound F9 eaten in down phase");
        CHECK(!f.svc->candActive_, "case 14: unbound F9 opened the window");
        CHECK(f.ctx->insertions.size() == 1, "case 14: %zu insertions, want 1 (the drained word)",
              f.ctx->insertions.size());
        if (f.ctx->insertions.size() == 1)
            CHECK(utf8(f.ctx->insertions[0]) == kAnchorAnNyeng, "case 14: insert %s",
                  utf8(f.ctx->insertions[0]).c_str());
        CHECK(f.svc->wordbuf_.empty(), "case 14: nothing held after the F-key boundary");
        f.Teardown();
    }

    /* 15. The configured key is the store's word: the env seam sets F9
     * (v1:120:0) as the candidate key and the adapter honours it — both
     * phases mirror the open decision. */
    {
        _putenv_s("HUNMIN_TSF_HANJA_KEY", "v1:120:0");
        Fixture f;
        f.Begin();
        TypeAnNyeng(f);
        BOOL testEat = FALSE, eat = TRUE;
        f.TestKey(VK_F9, &testEat);
        f.Key(VK_F9, &eat);
        CHECK(testEat == TRUE, "case 15: configured F9 not eaten in test phase");
        CHECK(eat == TRUE, "case 15: configured F9 not eaten in down phase");
        CHECK(f.svc->candActive_, "case 15: configured F9 did not open the window");
        CHECK(f.svc->candCount_ == 1, "case 15: count %d walking the env seam (deduped)",
              f.svc->candCount_);
        _putenv_s("HUNMIN_TSF_HANJA_KEY", "");
        f.Teardown();
    }

    /* 16. Issue #100: a fallback list's commit replaces ONLY the last
     * syllable. `rkrkrk` -> the `가` fallback list; committing row 0 inserts
     * `가가` + the chosen Hanja (the word prefix stays Hangul), exactly one
     * insert, the buffer drains once like the ordinary commit. */
    {
        Fixture f;
        f.Begin();
        f.Type("rkrkrk");
        f.Key(VK_HANJA); /* opens the last-syllable fallback list (case 8) */
        CHECK(f.svc->candFallback_, "case 16: fallback list did not open");
        std::string expected(hunmin_suggest_text(f.svc->ctx_, 0));
        CHECK(!expected.empty(), "case 16: candidate 0 text empty against the data");
        if (!expected.empty()) {
            BOOL eat = FALSE;
            f.Key(VK_RETURN, &eat);
            CHECK(eat == TRUE, "case 16: Return not consumed on a fallback list");
            CHECK(f.ctx->insertions.size() == 1, "case 16: %zu insertions, want 1",
                  f.ctx->insertions.size());
            if (f.ctx->insertions.size() == 1)
                CHECK(utf8(f.ctx->insertions[0]) == std::string(kAnchorGaGa) + expected,
                      "case 16: inserted %s, want %s",
                      utf8(f.ctx->insertions[0]).c_str(),
                      (std::string(kAnchorGaGa) + expected).c_str());
            CHECK(!f.svc->candActive_, "case 16: window still open after fallback commit");
            f.AssertWordbuf(nullptr, "case 16");
            f.AssertPreedit(nullptr, "case 16");
        }
        f.Teardown();
    }

    /* 17. Zero-and-zero (`rkel` -> `가디`: 0 whole-word, 0 on the last
     * syllable 디 — measured): the Hanja key passes through in both phases,
     * no window, no state, no insert. */
    {
        Fixture f;
        f.Begin();
        f.Type("rkel");
        CHECK(utf8(f.svc->wordbuf_) + std::string(hunmin_ic_preedit(f.svc->ic_)) ==
                  std::string(kAnchorGa) + "\xEB\x94\x94",
              "case 17: composed buffer is not ga-di");
        f.AssertPreedit("\xEB\x94\x94", "case 17");
        BOOL testEat = TRUE, eat = TRUE;
        f.TestKey(VK_HANJA, &testEat);
        f.Key(VK_HANJA, &eat);
        CHECK(testEat == FALSE, "case 17: Hanja-key test ate on a zero-and-zero buffer");
        CHECK(eat == FALSE, "case 17: Hanja-key down ate on a zero-and-zero buffer");
        CHECK(!f.svc->candActive_, "case 17: state active on a zero-and-zero buffer");
        f.AssertNoInsert("case 17");
        f.Teardown();
    }

    /* 18. One-syllable zero (`el` -> `디`: 0 hits, and the fallback lookup
     * would be the same single syllable — word == last): pass-through in
     * both phases (issue #100 keeps one-syllable words as they were). */
    {
        Fixture f;
        f.Begin();
        f.Type("el");
        BOOL testEat = TRUE, eat = TRUE;
        f.TestKey(VK_HANJA, &testEat);
        f.Key(VK_HANJA, &eat);
        CHECK(testEat == FALSE, "case 18: Hanja-key test ate on a one-syllable zero");
        CHECK(eat == FALSE, "case 18: Hanja-key down ate on a one-syllable zero");
        CHECK(!f.svc->candActive_, "case 18: state active on a one-syllable zero");
        f.AssertNoInsert("case 18");
        f.Teardown();
    }

    if (failures == 0)
        printf("tsf-candidate-test: all cases passed\n");
    else
        printf("tsf-candidate-test: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
