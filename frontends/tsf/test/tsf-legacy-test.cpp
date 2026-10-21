/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1 (issue #119): the legacy interim path for classic (default-sink)
 * TSF hosts. A classic host terminates the composition after every key and
 * FUSES the composed text into its document — measured live on the dev host
 * (docs/TSF-INTERIM-INPUT-SPIKE.md rounds) and reproduced here with the mock
 * host driving the adapter's own sink right after each key returns, the way
 * a real per-key-terminating host does.
 *
 * Under that host signature the adapter must flip to the interim path
 * (detection: the FIRST mid-word OnCompositionTerminated that is not its own
 * EndComposition) and the word must compose across keys — on the interim
 * path the finalised syllables and the in-flight preedit land in the HOST
 * CONTROL through the EDIT-family messages (EM_REPLACESEL), not through the
 * mock's document string, so the capture window is a REAL (invisible,
 * message-child) EDIT control inside the test process and the assertion
 * channel is WM_GETTEXT of that control.
 *
 * The interim doc writes are POSTED (never inline in a TSF callback — the
 * measured focus-storm defect), so a test settles them with
 * FlushDocWrites() before reading the control; the mock never pumps, so
 * the thread timer never fires.
 *
 * A key the adapter passes (the boundary space) is inserted by the APP on a
 * real host. The mock drives the adapter directly and emulates no app, so
 * the passthrough space never appears in the capture control — the tests
 * assert the eat flag at BOTH key phases instead (the live defect was the
 * test phase eating what the down phase passed), plus the control text
 * without the space.
 *
 * Standalone TU (the include-.cpp seam is retired); the fixture is the
 * candidate fragment's (tsf-candidate-fixture.h). Runs inside the one
 * registered tsf-test entry (no additional add_test; docs-lint check 6
 * counts literals).
 *
 * Hangul in expected text is written as wide \uXXXX literals (MSVC rule:
 * narrow \uXXXX encodes through the ANSI codepage, measured).
 */
#include <gtest/gtest.h>

#include <string>

#include "tsf-candidate-fixture.h"

namespace {

using utf8Wide = std::wstring; /* readability: assertions carry wide text */

struct LegacyTest : public CandidateTest {
    HWND ctrl = nullptr;

    void LegacyBegin() {
        /* A REAL EDIT control, offscreen and tool-styled, so the interim
         * path's GetFocus()-then-EM flow runs against a true EDIT window
         * (a message-only window does not accept focus — measured). The
         * ES_AUTOHSCROLL matters: EM_REPLACESEL inserts "as if the user
         * typed it", and a single-line EDIT without it REFUSES to grow
         * its line past the control's width — a 24px control accepted
         * exactly one hangul char and silently rejected every later
         * insert (measured: doclen stuck at 1 through seven writes). */
        ctrl = CreateWindowExW(WS_EX_TOOLWINDOW, L"EDIT", L"",
                               WS_POPUP | ES_AUTOHSCROLL, -32000, -32000,
                               400, 20, nullptr, nullptr, nullptr, nullptr);
        ASSERT_NE(ctrl, nullptr) << "legacy: capture EDIT not created";
        ShowWindow(ctrl, SW_SHOWNOACTIVATE);
        SetFocus(ctrl); /* the interim path resolves via GetFocus() */
        Begin();
    }

    void LegacyTeardown() {
        if (ctrl) {
            DestroyWindow(ctrl);
            ctrl = nullptr;
        }
        Teardown();
    }

    /* One key, terminated exactly as a classic host does: the adapter's
     * key handling runs, and the host ends the composition it produced
     * right after the handler returns (the measured per-key signature). */
    void KeyTerminating(WPARAM vk) {
        BOOL eat = FALSE;
        svc->OnTestKeyDown(ctx, vk, 0, &eat);
        svc->OnKeyDown(ctx, vk, 0, &eat);
        if (Snap().composition)
            svc->OnCompositionTerminated(tsfmock::MockContext::kCookie, nullptr);
    }

    /* The mock never pumps: settle the posted doc write by hand. */
    void FlushDocWrites() {
        svc->FlushPendingDocWrites();
    }

    static std::wstring CtrlText(HWND hwnd) {
        int n = GetWindowTextLengthW(hwnd) + 1;
        std::wstring w((size_t)n, L'\0');
        GetWindowTextW(hwnd, &w[0], n);
        w.resize(wcslen(w.c_str()));
        return w;
    }
};

/* Type `dkssud` then space against a per-key-terminating host. */
TEST_F(LegacyTest, FlipOnFirstMidWordTerminationAndCompose) {
    LegacyBegin();
    if (HasFatalFailure()) {
        Teardown();
        GTEST_SKIP() << "fixture could not start";
    }
    for (const char *p = "dkssud"; *p; ++p)
        KeyTerminating((WPARAM)std::toupper((unsigned char)*p));
    EXPECT_TRUE(Snap().legacyMode) << "first mid-word termination must flip the service";

    FlushDocWrites();
    /* The interim write rebuilt the word: the finalised part sits in the
     * control ahead of the live preedit. */
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155')
        << "control after flip-then-type";

    BOOL spaceEat = TRUE, spaceTestEat = TRUE;
    TestKey(VK_SPACE, &spaceTestEat);
    Key(VK_SPACE, &spaceEat);
    EXPECT_EQ(spaceTestEat, FALSE) << "space passes at the TEST phase too";
    EXPECT_EQ(spaceEat, FALSE) << "space must pass at the boundary";
    FlushDocWrites();
    /* The mock emulates no app, so the passthrough space never lands in
     * the control here — on a real host the app inserts it after the
     * drained word. The drained word itself must be there exactly once. */
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155')
        << "space: the word drained once, no duplicate";

    EXPECT_EQ(ctx->compositionStarts, 1)
        << "exactly one composition (the native attempt the host killed)";
    EXPECT_EQ(ctx->compositionEnds, 0)
        << "the terminated composition never ended through our own path";
    LegacyTeardown();
}

/* The interim word follows a backspace the engine declines at the start:
 * the boundary drain rewrites the span, never duplicates it. */
TEST_F(LegacyTest, BoundaryDrainNeverDuplicates) {
    LegacyBegin();
    if (HasFatalFailure()) {
        Teardown();
        GTEST_SKIP() << "fixture could not start";
    }
    for (const char *p = "droa"; *p; ++p)
        KeyTerminating((WPARAM)std::toupper((unsigned char)*p));
    EXPECT_TRUE(Snap().legacyMode);
    FlushDocWrites();
    /* whatever the buffers hold, the SECOND boundary delivery must not
     * re-run the append: the boundary is consumed once. */
    BOOL eat = TRUE, testEat = TRUE;
    TestKey(VK_SPACE, &testEat);
    Key(VK_SPACE, &eat);
    EXPECT_EQ(testEat, FALSE) << "boundary space passes at the test phase";
    EXPECT_EQ(eat, FALSE) << "boundary space passes";
    FlushDocWrites();
    std::wstring after = CtrlText(ctrl);
    TestKey(VK_SPACE, &testEat);
    Key(VK_SPACE, &eat);
    EXPECT_EQ(testEat, FALSE) << "second space also passes at the test phase";
    EXPECT_EQ(eat, FALSE) << "second space also passes";
    FlushDocWrites();
    EXPECT_EQ(CtrlText(ctrl), after)
        << "second space: plain passthrough (the mock emulates no app), "
           "and the drain must not re-run";
    LegacyTeardown();
}

/* The word after a drained boundary: the fresh span re-anchors at the live
 * caret, so the SECOND word composes after the first instead of rewriting
 * the document start (the drain's stale preStart_ was the defect). */
TEST_F(LegacyTest, SecondWordComposesAfterTheFirst) {
    LegacyBegin();
    if (HasFatalFailure()) {
        Teardown();
        GTEST_SKIP() << "fixture could not start";
    }
    for (const char *p = "dkssud"; *p; ++p)
        KeyTerminating((WPARAM)std::toupper((unsigned char)*p));
    FlushDocWrites();
    Key(VK_SPACE); /* boundary: drains the word; the app would insert ' ' */
    for (const char *p = "dl"; *p; ++p)
        KeyTerminating((WPARAM)std::toupper((unsigned char)*p));
    FlushDocWrites();
    /* d=ㅇ l=ㅣ -> 이: the second word composes AFTER 안녕, not over it. */
    EXPECT_EQ(CtrlText(ctrl),
              std::wstring() + L'\uC548' + L'\uB155' + L'\uC774')
        << "second word composes after the drained first word";
    LegacyTeardown();
}

/* The one-char ruling: the Hanja candidate replaces the in-flight syllable
 * only; the already-finalised prefix stays Hangul and nothing duplicates. */
TEST_F(LegacyTest, HanjaCommitReplacesOnlyTheInFlightSyllable) {
    LegacyBegin();
    if (HasFatalFailure()) {
        Teardown();
        GTEST_SKIP() << "fixture could not start";
    }
    for (const char *p = "dkssud"; *p; ++p)
        KeyTerminating((WPARAM)std::toupper((unsigned char)*p));
    FlushDocWrites();
    ASSERT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155');

    BOOL openEat = TRUE;
    Key(VK_HANJA, &openEat);
    EXPECT_TRUE(Snap().candActive) << "the per-syllable anchor must open candidates";
    /* Capture the candidate the commit will take, before the commit's own
     * suggest calls invalidate the pointer (the candidate-test pattern). */
    std::string expected(hunmin_suggest_text(Snap().ctx, 0));
    ASSERT_FALSE(expected.empty()) << "candidate 0 text empty";

    Key((WPARAM)'1'); /* the digit submits candidate 0 */
    FlushDocWrites();
    std::wstring hanjaWide = utf8Of(expected.c_str());
    ASSERT_EQ(hanjaWide.size(), (size_t)1) << "the one-char ruling: candidate 0 is one char";
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + hanjaWide[0])
        << "the hanja replaced the in-flight syllable; the prefix stays";
    LegacyTeardown();
}

/* The backspace at a CLOSED word: the test phase passes it (the app
 * deletes the committed char through its own edit), and the next key's
 * fresh-span anchor re-reads the live caret — the word after the deletion
 * composes at the right position, never over the stale bookkeeping. */
TEST_F(LegacyTest, BackspaceAtClosedWordPassesAndSelfHeals) {
    LegacyBegin();
    if (HasFatalFailure()) {
        Teardown();
        GTEST_SKIP() << "fixture could not start";
    }
    for (const char *p = "dkssud"; *p; ++p)
        KeyTerminating((WPARAM)std::toupper((unsigned char)*p));
    FlushDocWrites();
    Key(VK_SPACE); /* the word closes; the app would insert ' ' */
    FlushDocWrites();
    ASSERT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155');

    BOOL testEat = TRUE;
    TestKey(VK_BACK, &testEat);
    EXPECT_EQ(testEat, FALSE) << "backspace at an empty in-flight passes";
    /* The app-side deletion, emulated: the EDIT deletes the backspace
     * through WM_CHAR (the translated char), not the raw key down. */
    SendMessageW(ctrl, WM_CHAR, 0x08, 0);
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548')
        << "the app deleted the committed char";

    KeyTerminating((WPARAM)'L'); /* l = ㅣ: composes AFTER the deletion */
    FlushDocWrites();
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\u3163')
        << "the fresh span re-anchored at the live caret";
    LegacyTeardown();
}

} // namespace
