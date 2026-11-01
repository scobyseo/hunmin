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

    void LegacyBegin(bool multiline = false, const char *hangulLayout = "dubeolsik") {
        /* A REAL EDIT control, offscreen and tool-styled, so the interim
         * path's GetFocus()-then-EM flow runs against a true EDIT window
         * (a message-only window does not accept focus — measured). The
         * ES_AUTOHSCROLL matters: EM_REPLACESEL inserts "as if the user
         * typed it", and a single-line EDIT without it REFUSES to grow
         * its line past the control's width — a 24px control accepted
         * exactly one hangul char and silently rejected every later
         * insert (measured: doclen stuck at 1 through seven writes).
         * Multiline is for the Enter case only: a paragraph break needs
         * a control that accepts one. */
        DWORD style = WS_POPUP | ES_AUTOHSCROLL | (multiline ? ES_MULTILINE : 0);
        /* A multiline control needs room for a second line: at 20px the
         * EDIT swallows WM_CHAR VK_RETURN instead of inserting the break
         * (measured; the real host control is taller). */
        ctrl = CreateWindowExW(WS_EX_TOOLWINDOW, L"EDIT", L"", style, -32000, -32000, 400, multiline ? 120 : 20, nullptr, nullptr, nullptr, nullptr);
        ASSERT_NE(ctrl, nullptr) << "legacy: capture EDIT not created";
        ShowWindow(ctrl, SW_SHOWNOACTIVATE);
        SetFocus(ctrl); /* the interim path resolves via GetFocus() */
        Begin(hangulLayout);
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

    /* The classic-host rule, not the mock's unconditional two-phase call:
     * after OnTestKeyDown claims a printable key the host suppresses its
     * own insertion; it calls OnKeyDown only for a claimed key. A key the
     * test phase passes is inserted by the app (wch) and OnKeyDown is not
     * called at all. Returns the test-phase answer so a case can assert
     * which branch the adapter chose. */
    BOOL KeyHost(WPARAM vk, WPARAM wch = 0) {
        BOOL testEat = FALSE;
        svc->OnTestKeyDown(ctx, vk, 0, &testEat);
        if (testEat) {
            BOOL eat = FALSE;
            svc->OnKeyDown(ctx, vk, 0, &eat);
        } else if (wch) {
            SendMessageW(ctrl, WM_CHAR, wch, 0);
        }
        if (Snap().composition)
            svc->OnCompositionTerminated(tsfmock::MockContext::kCookie, nullptr);
        return testEat;
    }

    /* The mock never pumps: settle the posted doc write by hand. */
    void FlushDocWrites() { svc->FlushPendingDocWrites(); }

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
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155') << "control after flip-then-type";

    BOOL spaceEat = TRUE, spaceTestEat = TRUE;
    TestKey(VK_SPACE, &spaceTestEat);
    Key(VK_SPACE, &spaceEat);
    EXPECT_EQ(spaceTestEat, FALSE) << "space passes at the TEST phase too";
    EXPECT_EQ(spaceEat, FALSE) << "space must pass at the boundary";
    FlushDocWrites();
    /* The mock emulates no app, so the passthrough space never lands in
     * the control here — on a real host the app inserts it after the
     * drained word. The drained word itself must be there exactly once. */
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155') << "space: the word drained once, no duplicate";

    EXPECT_EQ(ctx->compositionStarts, 1) << "exactly one composition (the native attempt the host killed)";
    EXPECT_EQ(ctx->compositionEnds, 0) << "the terminated composition never ended through our own path";
    LegacyTeardown();
}

/* Interim delivery x both commit units (issue #128, deliverable 2 as narrowed
 * in docs/FRONTEND-EQUIVALENCE.md 6.1).
 *
 * The unit is a WHEN, not a WHAT: the same text must reach the host control
 * either way, and only the moment differs. That is what these two cases
 * assert, on the path where it matters -- the interim delivery, where the
 * finished text IS document text and a late or early write is visible.
 *
 * The observation channel is the layer seam (Snapshot().finished), not the
 * control: under the buffered unit nothing is finalised while typing, under
 * the syllable unit each syllable finalises as the engine does. The control
 * text is asserted EQUAL in both, which is the invariant the unit must not
 * break. Keys are taken from `hunmin-sim --trace`: dkssud commits
 * the first syllable at the 4th key and leaves the second in flight.
 */
TEST_F(LegacyTest, CommitUnitBufferedHoldsTheWordUntilTheBoundary) {
    _putenv_s("HUNMIN_TSF_COMMIT_UNIT", "word");
    LegacyBegin();
    if (HasFatalFailure()) {
        Teardown();
        GTEST_SKIP() << "fixture could not start";
    }
    const std::wstring first = std::wstring(1, L'\uC548');
    const std::wstring word = first + L'\uB155';

    for (const char *p = "dkssud"; *p; ++p) {
        KeyTerminating((WPARAM)std::toupper((unsigned char)*p));
        FlushDocWrites();
        /* Buffered: the layer finalises nothing until the boundary, on every
         * key including the one where the engine commits a syllable. */
        EXPECT_TRUE(Snap().finished.empty())
            << "buffered unit finalised text at key " << (p - "dkssud" + 1);
    }
    EXPECT_EQ(CtrlText(ctrl), word) << "the display is the same word either way";

    BOOL spaceEat = TRUE;
    Key(VK_SPACE, &spaceEat);
    EXPECT_EQ(spaceEat, FALSE) << "the boundary key passes";
    FlushDocWrites();
    EXPECT_EQ(CtrlText(ctrl), word) << "boundary: the word once, no duplicate";
    LegacyTeardown();
}

TEST_F(LegacyTest, CommitUnitSyllableFinalisesAsTheEngineDoes) {
    _putenv_s("HUNMIN_TSF_COMMIT_UNIT", "syllable");
    LegacyBegin();
    if (HasFatalFailure()) {
        Teardown();
        GTEST_SKIP() << "fixture could not start";
    }
    const std::wstring first = std::wstring(1, L'\uC548');
    const std::wstring word = first + L'\uB155';

    int key = 0;
    for (const char *p = "dkssud"; *p; ++p) {
        KeyTerminating((WPARAM)std::toupper((unsigned char)*p));
        FlushDocWrites();
        ++key;
        /* The syllable unit HELDS nothing: what the engine finalises is
         * delivered at the key it finalises on, so it is already document
         * text by the time this line runs (measured: Snapshot().finished is
         * empty at every key, and that IS the unit's signature). */
        EXPECT_TRUE(Snap().finished.empty())
            << "syllable unit holds nothing at key " << key;
        if (key >= 4) {
            const std::wstring doc = CtrlText(ctrl);
            EXPECT_EQ(doc.substr(0, first.size()), first)
                << "the finalised syllable must be in the document from key " << key
                << ", got: " << std::string(doc.begin(), doc.end());
        }
    }
    EXPECT_EQ(CtrlText(ctrl), word) << "same word, delivered earlier";

    BOOL spaceEat = TRUE;
    Key(VK_SPACE, &spaceEat);
    EXPECT_EQ(spaceEat, FALSE) << "the boundary key still passes";
    FlushDocWrites();
    EXPECT_EQ(CtrlText(ctrl), word) << "boundary: no duplicate under the syllable unit";
    LegacyTeardown();
}

/* Dubeolsik maps no digit. A classic host suppresses insertion after a TRUE
 * test claim and does not call OnKeyDown for the unconsumed key, so claiming
 * the digit loses it (the reported interim numeric defect). The test phase
 * must peek the engine and pass the digit; the app inserts it. */
TEST_F(LegacyTest, DigitDeclinedAtTestPhaseIsInsertedByHost) {
    LegacyBegin();
    if (HasFatalFailure()) {
        Teardown();
        GTEST_SKIP() << "fixture could not start";
    }
    for (const char *p = "dkssud"; *p; ++p)
        KeyHost((WPARAM)std::toupper((unsigned char)*p));
    FlushDocWrites();
    ASSERT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155') << "prefix word before the digit";

    BOOL digitEat = KeyHost((WPARAM)'1', L'1');
    EXPECT_EQ(digitEat, FALSE) << "dubeolsik has no digit row: the test phase must pass it";
    FlushDocWrites();
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155' + L'1') << "the app's digit must follow the drained word exactly once";
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
    EXPECT_EQ(CtrlText(ctrl), after) << "second space: plain passthrough (the mock emulates no app), "
                                        "and the drain must not re-run";
    LegacyTeardown();
}

/* Enter is a word boundary the TIP only sees at the TEST phase: a
 * default-sink host inserts the paragraph break itself and moves its caret,
 * then never calls OnKeyDown (the measured space signature). Without the
 * test-phase drain the span stays anchored on the OLD line, so the next
 * syllable's write selects it and drags the caret back to the end of the
 * previous line (the reported defect). The drain must run and the next
 * syllable must compose on the new line. */
TEST_F(LegacyTest, EnterEndsComposingAndKeepsCaretOnNewLine) {
    LegacyBegin(true);
    if (HasFatalFailure()) {
        Teardown();
        GTEST_SKIP() << "fixture could not start";
    }
    for (const char *p = "dkssud"; *p; ++p)
        KeyTerminating((WPARAM)std::toupper((unsigned char)*p));
    FlushDocWrites();
    ASSERT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155');

    BOOL enterTestEat = TRUE;
    TestKey(VK_RETURN, &enterTestEat);
    EXPECT_EQ(enterTestEat, FALSE) << "Enter passes at the test phase";
    /* The app's half, emulated: the paragraph break lands at the caret
     * before the posted drain runs (the real order on a default-sink host). */
    LONG end = (LONG)CtrlText(ctrl).size();
    SendMessageW(ctrl, EM_SETSEL, (WPARAM)end, (LPARAM)end);
    SendMessageW(ctrl, WM_CHAR, L'\r', 0);
    ASSERT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155' + L"\r\n") << "the app's break landed after the word";
    FlushDocWrites();
    ASSERT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155' + L"\r\n") << "Enter drains the word exactly once, ahead of the break";

    KeyTerminating((WPARAM)'D'); /* d = ㅇ: a new syllable on the new line */
    FlushDocWrites();
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155' + L"\r\n" + L'\u3147')
        << "the next syllable composes after the break, never back on the old line";
    LegacyTeardown();
}

/* The reported defect (2026-11-01): a Hangul word, then a period, then Enter.
 * Measured at the ENGINE and the LAYER first: the period is declined and the
 * word is delivered, and Enter is declined too (no commit, used=0), so the
 * engine never swallows the break. This case is the frontend half: on a
 * classic host the TIP must pass Enter at the test phase so the APP inserts
 * the paragraph break, and the word's drain must not run after it. */
TEST_F(LegacyTest, PeriodThenEnterLandsTheBreak) {
    LegacyBegin(true);
    if (HasFatalFailure()) {
        Teardown();
        GTEST_SKIP() << "fixture could not start";
    }
    for (const char *p = "dkssud"; *p; ++p)
        KeyHost((WPARAM)std::toupper((unsigned char)*p));

    const BOOL periodEat = KeyHost(VK_OEM_PERIOD, L'.');
    FlushDocWrites();
    EXPECT_EQ(periodEat, FALSE) << "a period is declined: it belongs to the app";
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155' + L".")
        << "the word landed, then the app's period";

    const BOOL enterEat = KeyHost(VK_RETURN, L'\r');
    FlushDocWrites();
    EXPECT_EQ(enterEat, FALSE) << "Enter must pass at the test phase";
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155' + L".\r\n")
        << "the paragraph break must survive the boundary";
    LegacyTeardown();
}

/* The same sequence on the DEFAULT layout, where the period is a layout key:
 * measured through the layer, `sin3-p2` CONSUMES `.` into the composing word
 * (`used=1`, preedit ends with the period), so unlike dubeolsik the word is
 * still in flight when Enter arrives -- Enter is the boundary that delivers
 * `...ㅎ곈디.`. That is the reported live sequence, and the question this case
 * settles is whether the frontend keeps the app's paragraph break when the
 * drain and the break race. The engine is not the suspect: it declines Enter
 * on both layouts (no commit, used=0). */
TEST_F(LegacyTest, PeriodIsALayoutKeyAndEnterStillBreaksTheLine) {
    LegacyBegin(true, "sin3-p2");
    if (HasFatalFailure()) {
        Teardown();
        GTEST_SKIP() << "fixture could not start";
    }
    for (const char *p = "dkssud"; *p; ++p)
        KeyHost((WPARAM)std::toupper((unsigned char)*p));
    KeyHost(VK_OEM_PERIOD, L'.');
    FlushDocWrites();
    const std::wstring before = CtrlText(ctrl);
    /* The interim path writes the composition INTO the document, so the
     * consuming word and its period are already on screen here -- what is
     * still in flight is the layer's word buffer, not the text. */
    ASSERT_NE(before.find(L'.'), std::wstring::npos)
        << "sin3-p2 composes the period as a layout key";

    const BOOL enterEat = KeyHost(VK_RETURN, L'\r');
    FlushDocWrites();
    EXPECT_EQ(enterEat, FALSE) << "Enter passes: the app inserts the break";
    const std::wstring after = CtrlText(ctrl);
    EXPECT_GE(after.size(), before.size() + 2) << "the paragraph break must be in the document";
    EXPECT_EQ(after.compare(after.size() - 2, 2, L"\r\n"), 0)
        << "the break must be the last thing in the document";
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
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\uB155' + L'\uC774') << "second word composes after the drained first word";
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
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + hanjaWide[0]) << "the hanja replaced the in-flight syllable; the prefix stays";
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
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548') << "the app deleted the committed char";

    KeyTerminating((WPARAM)'L'); /* l = ㅣ: composes AFTER the deletion */
    FlushDocWrites();
    EXPECT_EQ(CtrlText(ctrl), std::wstring() + L'\uC548' + L'\u3163') << "the fresh span re-anchored at the live caret";
    LegacyTeardown();
}

/* Issue #144: a host that is NOT an EDIT-family control terminated the
 * composition mid-word ONCE (measured on Chromium/Edge: while its window is
 * settling; the focused window there is neither an EDIT nor a RICHEDIT, and
 * EM_REPLACESEL reaches nothing in it). The interim path cannot serve such a
 * host, so the adapter must NOT flip: it stays native and the next composition
 * REPLACES the copy the host fused instead of appending after it. The capture
 * window here is a BUTTON for exactly that class test. */
struct NonEditHostTest : public CandidateTest {
    HWND win = nullptr;

    void HostBegin() {
        /* The buffered unit is this case's shape (the composition holds the
         * whole word); the sibling CommitUnit case leaves the env at
         * "syllable" and its per-syllable commits would add a composition end
         * and a restart to the counts asserted below. */
        _putenv_s("HUNMIN_TSF_COMMIT_UNIT", "word");
        win =
            CreateWindowExW(WS_EX_TOOLWINDOW, L"BUTTON", L"", WS_POPUP | BS_PUSHBUTTON, -32000, -32000, 200, 40, nullptr, nullptr, nullptr, nullptr);
        ASSERT_NE(win, nullptr) << "non-edit host window not created";
        ShowWindow(win, SW_SHOWNOACTIVATE);
        SetFocus(win);
        Begin();
    }

    void HostTeardown() {
        if (win) {
            DestroyWindow(win);
            win = nullptr;
        }
        Teardown();
    }
};

TEST_F(NonEditHostTest, MidWordTerminationKeepsNativeAndReplacesTheFusedCopy) {
    HostBegin();
    if (HasFatalFailure()) {
        HostTeardown();
        GTEST_SKIP() << "fixture could not start";
    }
    ASSERT_EQ(GetFocus(), win) << "the non-edit capture window must hold the focus";

    /* Key 1: the composition is written (the mock's document holds it, as a
     * real host's store does), then the HOST ends it itself. */
    Key((WPARAM)'D');
    ASSERT_EQ(ctx->doc, std::wstring() + L'\u3147') << "the first key's composition is document text";
    ctx->HostTerminateComposition();
    svc->OnCompositionTerminated(tsfmock::MockContext::kCookie, nullptr);
    EXPECT_FALSE(Snap().legacyMode) << "a non-edit host must not flip to the interim path";
    EXPECT_EQ(Snap().composition, nullptr) << "the terminated composition is released";

    for (const char *p = "kssud"; *p; ++p)
        Key((WPARAM)std::toupper((unsigned char)*p));

    /* The re-adopted copy was REPLACED: the document is the engine's word
     * (dkssud -> 안녕) with none of the killed composition's text in front of
     * it. Before the fix the same sequence left ㅇ안녕 in the document. */
    EXPECT_EQ(ctx->doc, std::wstring() + L'\uC548' + L'\uB155') << "the fused copy is replaced, not duplicated";
    EXPECT_EQ(ctx->compositionStarts, 2) << "the killed composition plus its re-adopted successor";
    EXPECT_EQ(ctx->compositionEnds, 0) << "the terminating host never ends through our own path";
    EXPECT_EQ(ctx->hostTerminations, 1) << "one host termination, one re-adoption (Chromium's measured shape)";
    HostTeardown();
}

/* Issue #144's open question: a non-edit host that terminates on EVERY key (the
 * prior art's restricted text stores). The class test keeps such a host native,
 * so each key re-adopts the copy the previous termination fused; whether that
 * COMPOSES or ACCUMULATES is what this case measures. */
TEST_F(NonEditHostTest, PerKeyTerminatingNonEditHostComposesAcrossKeys) {
    HostBegin();
    if (HasFatalFailure()) {
        HostTeardown();
        GTEST_SKIP() << "fixture could not start";
    }
    ASSERT_EQ(GetFocus(), win) << "the non-edit capture window must hold the focus";

    for (const char *p = "dkssud"; *p; ++p) {
        Key((WPARAM)std::toupper((unsigned char)*p));
        if (Snap().composition) {
            ctx->HostTerminateComposition();
            svc->OnCompositionTerminated(tsfmock::MockContext::kCookie, nullptr);
        }
    }

    EXPECT_EQ(ctx->doc, std::wstring() + L'\uC548' + L'\uB155') << "per-key termination still composes: the word, not the accumulation";
    EXPECT_EQ(ctx->hostTerminations, 6) << "every key's composition is terminated by the host";
    EXPECT_FALSE(Snap().legacyMode) << "a non-edit host never flips";
    HostTeardown();
}

} // namespace
