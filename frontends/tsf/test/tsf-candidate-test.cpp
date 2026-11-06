/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1 (issue #67): the Hanja-key candidate window, driven over the mock
 * TSF layer exactly like tsf-trace-emit drives the buffering contract. The
 * adapter logic under test is the compiled DLL's logic (linked TUs; the
 * white-box seam reads the shell's Snapshot, tsf-candidate-fixture.h), and
 * the candidate state (candActive/candCount/candCursor/candTop), the
 * pop-up's hide path (no metrics in a headless run) and the ONE insert the
 * commit produces are all assertable.
 *
 * Standalone TU (the include-.cpp seam is retired): the ctest entry
 * `tsf-candidate-test` selects this group with --gtest_filter=CandidateTest.*;
 * the rc-77 skip probe lives in tools/tsf-run-if-data.py.
 *
 * Hangul in expected strings is written as UTF-8 byte escapes (\xEC\x95\x88
 * style): MSVC builds this TU under the ANSI codepage (949), where a raw
 * UTF-8 literal is fatal (C2001) and a \uXXXX escape encodes through the
 * codepage, not the engine's UTF-8 - both measured (docs/ENVIRONMENT.md
 * pitfalls).
 */
#include "tsf-candidate-fixture.h"

/* 1. Empty buffer: the candidate key belongs to the client, in test
 * and down alike (default: the bare Hanja key, 0x19). */
TEST_F(CandidateTest, EmptyBufferKeyBelongsToTheClient) {
    Begin();
    BOOL testEat = TRUE, downEat = TRUE;
    TestKey(VK_HANJA, &testEat);
    Key(VK_HANJA, &downEat);
    EXPECT_EQ(testEat, FALSE) << "case 1: key test ate with no buffer";
    EXPECT_EQ(downEat, FALSE) << "case 1: key down ate with no buffer";
    EXPECT_FALSE(Snap().candActive) << "case 1: candidate state active after empty buffer key";
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
    EXPECT_EQ(testEat, TRUE) << "case 2: Hanja-key test did not eat for `dkssud`";
    BOOL eat = TRUE;
    Key(VK_HANJA, &eat);
    EXPECT_EQ(eat, TRUE) << "case 2: Hanja-key down did not eat";
    EXPECT_TRUE(Snap().candActive) << "case 2: window not active after the Hanja key";
    /* krdict dedupe merged the sense rows, tools/krdict-dedupe.py. */
    EXPECT_EQ(Snap().candCount, 1) << "case 2: candidate count";
    EXPECT_EQ(Snap().candCursor, 0) << "case 2: cursor";
    EXPECT_EQ(Snap().candTop, 0) << "case 2: top";
    EXPECT_EQ(Snap().candSource, kAnchorAnNyeng) << "case 2: anchored to the whole word";
    AssertNoInsert("case 2");
    /* 2b. While open, the window's keys are eaten in the test phase. */
    BOOL dEat = TRUE;
    TestKey('1', &dEat);
    EXPECT_EQ(dEat, TRUE) << "case 2b: digit '1' not eaten in test phase";
    Teardown();
}

/* 3. Return commits the cursor (c0): exactly one insert, the Hanja text,
 * window closed, buffer empty, engine reset. */
TEST_F(CandidateTest, ReturnCommitsTheCursor) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    std::string expected(hunmin_suggest_text(Snap().ctx, 0));
    EXPECT_FALSE(expected.empty()) << "case 3: candidate 0 text empty";
    BOOL eat = FALSE;
    Key(VK_RETURN, &eat);
    EXPECT_EQ(eat, TRUE) << "case 3: Return not consumed";
    AssertCompositionEnded(expected.c_str(), "case 3");
    EXPECT_FALSE(Snap().candActive) << "case 3: window still open after commit";
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
    EXPECT_EQ(Snap().candCursor, 0) << "case 4: cursor after Down on a 1-row list, want 0 (clamped)";
    std::string expected(hunmin_suggest_text(Snap().ctx, 0));
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
    EXPECT_EQ(Snap().candCursor, 0) << "case 5: Up moved cursor below 0";
    AssertNoInsert("case 5");
    EXPECT_TRUE(Snap().candActive) << "case 5: window closed by Up";
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
    EXPECT_EQ(eat, FALSE) << "case 6: digit '2' past the end was consumed";
    AssertCompositionEnded(kAnchorAnNyeng, "case 6");
    EXPECT_FALSE(Snap().candActive) << "case 6: window left open after the drain";
    Teardown();
}

/* 6b. In-range digit select commits the page-relative index — one-row
 * list: digit '1' commits row 0 (the only one). */
TEST_F(CandidateTest, InRangeDigitCommits) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    std::string expected(hunmin_suggest_text(Snap().ctx, 0));
    BOOL eat = TRUE;
    Key('1', &eat);
    EXPECT_EQ(eat, TRUE) << "case 6b: in-range digit '1' not consumed";
    AssertCompositionEnded(expected.c_str(), "case 6b");
    EXPECT_FALSE(Snap().candActive) << "case 6b: window left open after digit commit";
    Teardown();
}

/* 7. Esc closes without touching the composition; typing continues. */
TEST_F(CandidateTest, EscClosesAndTypingContinues) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    BOOL eat = FALSE;
    Key(VK_ESCAPE, &eat);
    EXPECT_EQ(eat, TRUE) << "case 7: Esc not consumed";
    EXPECT_FALSE(Snap().candActive) << "case 7: window open after Esc";
    AssertNoInsert("case 7");
    AssertWordbuf(kAnchorAn, "case 7");
    AssertPreedit(kAnchorPreeditNyeng, "case 7");
    /* Typing continues where the user left off — the key is consumed and
     * the preedit grows (engine composes; the exact glyph is the
     * engine's business, asserted nonempty). */
    Key('d');
    std::string now(hunmin_buffered_preedit_composing(Snap().layer));
    EXPECT_FALSE(now.empty()) << "case 7: preedit empty after resuming typing";
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
    EXPECT_EQ(testEat, TRUE) << "case 8: Hanja-key test did not eat over the fallback list";
    EXPECT_EQ(eat, TRUE) << "case 8: Hanja-key down did not eat over the fallback list";
    EXPECT_TRUE(Snap().candActive) << "case 8: fallback list did not open";
    EXPECT_TRUE(Snap().candFallback) << "case 8: list not marked as a syllable-fallback list";
    EXPECT_EQ(Snap().candSource, kAnchorGa) << "case 8: anchored to the last syllable";
    /* The measured 65-row `가` list. */
    EXPECT_GE(Snap().candCount, 11) << "case 8: fallback list count";
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
    EXPECT_EQ(eat, FALSE) << "case 9: '9' swallowed (eat false expected)";
    EXPECT_FALSE(Snap().candActive) << "case 9: window survived the dead digit";
    AssertCompositionEnded(kAnchorAnNyeng, "case 9");
    Teardown();
}

/* 10. The mode toggle is a composition boundary with the window open:
 * the candidate list closes and the buffered word inserts once. */
TEST_F(CandidateTest, ModeToggleIsABoundaryWithTheWindowOpen) {
    Begin();
    TypeAnNyeng(*this);
    Key(VK_HANJA);
    EXPECT_TRUE(Snap().candActive) << "case 10: window did not open";
    Key(HUNMIN_TSF_VIRTUAL_HANGEUL);
    EXPECT_FALSE(Snap().candActive) << "case 10: window open after toggle";
    EXPECT_FALSE(Snap().hangulMode) << "case 10: mode did not switch";
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
    EXPECT_EQ(eat, TRUE) << "case 11a: BS not consumed";
    AssertPreedit(kAnchorPreeditNyee, "case 11a");
    AssertWordbuf(kAnchorAn, "case 11a");
    AssertNoInsert("case 11a");
    Key(VK_BACK);
    AssertPreedit(kAnchorPreeditNieun, "case 11b");
    Key(VK_BACK, &eat);
    EXPECT_EQ(eat, TRUE) << "case 11c: BS on emptied syllable not consumed";
    AssertPreedit(nullptr, "case 11c");
    AssertNoInsert("case 11c");
    Key(VK_BACK, &eat);
    EXPECT_EQ(eat, FALSE) << "case 11d: BS past the empty preedit eaten anyway";
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
    EXPECT_TRUE(Snap().candActive) << "case 12: window not active on single syllable";
    EXPECT_GE(Snap().candCount, 11) << "case 12: want >= 11 for paging";
    Key(VK_NEXT);
    EXPECT_EQ(Snap().candTop, 10) << "case 12: top after PageDown";
    EXPECT_EQ(Snap().candCursor, 10) << "case 12: cursor after PageDown";
    std::string expected(hunmin_suggest_text(Snap().ctx, 0 + 10));
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
    EXPECT_EQ(Snap().candTop, 0) << "case 13: top";
    EXPECT_TRUE(Snap().candActive) << "case 13: window closed by PageUp";
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
    EXPECT_EQ(testEat, FALSE) << "case 14: unbound F9 eaten in test phase";
    EXPECT_EQ(eat, FALSE) << "case 14: unbound F9 eaten in down phase";
    EXPECT_FALSE(Snap().candActive) << "case 14: unbound F9 opened the window";
    AssertCompositionEnded(kAnchorAnNyeng, "case 14");
    EXPECT_TRUE(Snap().finished.empty()) << "case 14: nothing held after the F-key boundary";
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
    EXPECT_EQ(testEat, TRUE) << "case 15: configured F9 not eaten in test phase";
    EXPECT_EQ(eat, TRUE) << "case 15: configured F9 not eaten in down phase";
    EXPECT_TRUE(Snap().candActive) << "case 15: configured F9 did not open the window";
    EXPECT_EQ(Snap().candCount, 1) << "case 15: walking the env seam (deduped)";
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
    EXPECT_TRUE(Snap().candFallback) << "case 16: fallback list did not open";
    std::string expected(hunmin_suggest_text(Snap().ctx, 0));
    EXPECT_FALSE(expected.empty()) << "case 16: candidate 0 text empty against the data";
    if (!expected.empty()) {
        BOOL eat = FALSE;
        Key(VK_RETURN, &eat);
        EXPECT_EQ(eat, TRUE) << "case 16: Return not consumed on a fallback list";
        /* Issue #98: the commit is the composition's END (the candidate
         * replaces the composition the list anchored to), so the fused
         * text is observed on the composition channel, not the plain
         * edit-session insert channel (mock-tsf.h: insertions = one
         * entry per NON-composition SetText). Exactly one fused content
         * and zero plain inserts is the one-insert contract here. */
        EXPECT_FALSE(Snap().candActive) << "case 16: window still open after fallback commit";
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
    EXPECT_EQ(utf8(Snap().finished) + std::string(hunmin_buffered_preedit_composing(Snap().layer)), std::string(kAnchorGa) + "\xEB\x94\x94")
        << "case 17: composed buffer is not ga-di";
    AssertPreedit("\xEB\x94\x94", "case 17");
    BOOL testEat = TRUE, eat = TRUE;
    TestKey(VK_HANJA, &testEat);
    Key(VK_HANJA, &eat);
    EXPECT_EQ(testEat, FALSE) << "case 17: Hanja-key test ate on a zero-and-zero buffer";
    EXPECT_EQ(eat, FALSE) << "case 17: Hanja-key down ate on a zero-and-zero buffer";
    EXPECT_FALSE(Snap().candActive) << "case 17: state active on a zero-and-zero buffer";
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
    EXPECT_EQ(testEat, FALSE) << "case 18: Hanja-key test ate on a one-syllable zero";
    EXPECT_EQ(eat, FALSE) << "case 18: Hanja-key down ate on a one-syllable zero";
    EXPECT_FALSE(Snap().candActive) << "case 18: state active on a one-syllable zero";
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
    g_mockKeyState[VK_SHIFT & 0xFF] = (SHORT)-32768;
    Key('R'); /* the VK the OS sends for the key; case is the engine's */
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    AssertPreedit("\xE3\x84\xB2", "case 19: shifted r is not ssang-gigyeong");
    AssertNoInsert("case 19");
    Teardown();
}

/* 20. Issue #112 (ruling "wire and make it common to all platform"): the
 * adapter feeds the engine `caps` option per key event, and CapsLock still
 * selects NO row — the Shift bit is the only row transport (issue #106's
 * engine case endpoint). The issue's sketch expected dubeolsik r -> ㄲ under
 * CapsLock; measured against the tree that premise is wrong: the option
 * feeds only the yethangul P variable (hunmin.h:93), and both other
 * frontends leave the row plain under caps alone (fcitx5 even UNDOES the
 * host's uppercase sym, hunminadapter.cpp:324-326). The common behaviour is the
 * plain row, pinned here through the real engine. */
TEST_F(CandidateTest, CapsLockFeedsTheOptionButSelectsNoRow) {
    Begin();
    g_mockKeyState[VK_CAPITAL & 0xFF] = (SHORT)0x0001; /* the toggle bit */
    Key('R');
    g_mockKeyState[VK_CAPITAL & 0xFF] = 0;
    AssertPreedit("\xE3\x84\xB1", "caps 20: r under CapsLock is the plain row");
    AssertNoInsert("caps 20");
    Teardown();
}

/* 21. What the wiring is FOR, end to end: sin3-p2-yet resolves a different
 * tier under the P variable, the exact pair tier A pins in
 * tests/golden/p2-2-4-3-caps.jsonl — j then k with caps=1 resolves the
 * yet-form preedit (U+1141 U+1160), with caps=0 the plain ㅇ (U+3147).
 * Both rounds run on this adapter with the mock's VK_CAPITAL toggle bit
 * injected, so the option's per-event shape (1 when on, 0 when off) is
 * what the engine saw — the TSF frontend honours the yethangul P variable
 * exactly like fcitx5 and Android now do. */
TEST_F(CandidateTest, CapsLockDrivesTheYethangulPVariable) {
    Begin("sin3-p2-yet");
    g_mockKeyState[VK_CAPITAL & 0xFF] = (SHORT)0x0001;
    Type("jk");
    AssertPreedit("\xE1\x85\x81\xE1\x85\xA0", "caps 21: P=1 tier with CapsLock toggled on");
    g_mockKeyState[VK_CAPITAL & 0xFF] = 0;
    Teardown();

    Begin("sin3-p2-yet");
    Type("jk");
    AssertPreedit("\xE3\x85\x87", "caps 21: plain tier with CapsLock off");
    Teardown();
}
