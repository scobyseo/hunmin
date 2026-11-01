/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * hangul-valid-test.cpp - tier A for the structural half of the modes judgement
 * (docs/SPEC-MODES.md §R8-정정, issue #166).
 *
 * The requirement is one sentence: `hangul_valid` holds when the buffer
 * "composes as Korean syllables of the shape (choseong + jungseong) or
 * (choseong + jungseong + jongseong) only". So the cases below are that
 * sentence, case by case, plus the rejections it implies and does not spell out
 * (an in-flight jamo, a digit run, old Hangul).
 *
 * Two kinds of assertion, deliberately separated:
 *
 *   1. LITERAL cases - the string IS the contract. `안녕` is well-formed by the
 *      shape rule, not because the engine happens to produce it, so writing it
 *      down costs nothing and a change to the layout cannot silently move it.
 *   2. ENGINE-DERIVED cases - the expectation is read off the engine, so a
 *      hand-typed guess cannot encode a defect. `dkssud` renders as whatever
 *      두벌식 renders it as; the predicate must say 1 for the finished word and
 *      0 once an unfinished consonant joins it.
 *
 * The "no data file is consulted" half of #166's checklist holds by
 * construction: hunmin_hangul_valid takes a string and no context, so there is
 * no directory it could read. `ATakesNoLayer` pins that shape.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "hunmin.h"
}

namespace {

const char *kDataPath = nullptr;

hunmin_ctx *make_ctx() {
    const char *dirs[1] = {kDataPath};
    return hunmin_ctx_new(dirs, 1);
}

/* Everything the client would have received for one word: the per-key commits
 * accumulated, plus the syllable still composing. */
std::string typed(const std::string &keys) {
    hunmin_ctx *ctx = make_ctx();
    hunmin_ic *ic = ctx ? hunmin_ic_new(ctx, "dubeolsik") : nullptr;
    std::string out;

    if (!ic) return std::string("<no engine>");
    for (char c : keys) {
        hunmin_ic_process(ic, (uint32_t)(unsigned char)c);
        const char *commit = hunmin_ic_commit(ic);
        if (commit) out += commit;
    }
    if (hunmin_ic_preedit(ic)) out += hunmin_ic_preedit(ic);
    hunmin_ic_free(ic);
    hunmin_ctx_free(ctx);
    return out;
}

int valid(const std::string &s) { return hunmin_hangul_valid(s.c_str()); }

/* The shape itself: one syllable of each legal shape is valid. */
TEST(HangulValidTest, AcceptsChoJungAndChoJungJong) {
    EXPECT_EQ(1, valid("가"));   /* cho + jung */
    EXPECT_EQ(1, valid("각"));   /* cho + jung + jong */
    EXPECT_EQ(1, valid("안녕")); /* a run of them */
}

/* A well-formed run that is not a word is valid. This is the whole point of
 * R8-정정 replacing the word test: the predicate knows shape, not vocabulary,
 * so novel syllables pass and no dictionary can change the answer. */
TEST(HangulValidTest, AcceptsWellFormedNonWords) {
    EXPECT_EQ(1, valid("뷁똠"));
    EXPECT_EQ(1, valid("쀍쀍쀍"));
}

/* R8's own example: `안녕ㅎ` is neither a Hangul word nor an English word, and it
 * is not structurally Hangul either — the trailing consonant never composed. */
TEST(HangulValidTest, RejectsAnUncomposedJamo) {
    EXPECT_EQ(0, valid("안녕ㅎ"));
    EXPECT_EQ(0, valid("ㄱ"));       /* a lone choseong, the buffer's first key */
    EXPECT_EQ(0, valid("가ㄴ"));     /* a finished syllable plus an in-flight one */
    EXPECT_EQ(0, valid("뷁ㅏ"));     /* a jamo in the middle is just as invalid */
}

/* Conjoining jamo are what the engine holds before composition; the
 * compatibility forms above are what it shows. Neither is a syllable. */
TEST(HangulValidTest, RejectsConjoiningJamo) {
    EXPECT_EQ(0, valid("\xE1\x84\x80"));             /* U+1100 choseong kiyeok */
    EXPECT_EQ(0, valid("\xE1\x85\xA1"));             /* U+1161 jungseong a */
    EXPECT_EQ(0, valid("\xE1\x87\xAE"));             /* U+11EE jongseong yesieung */
}

/* Old Hangul: `hm_jamo_to_syllable` composes only the modern 19/21/28, so an
 * archaic jongseong cannot become a precomposed syllable and the composer
 * renders it as jamo. Four shipped layouts (dubeolsik-yet, sebeolsik-yet,
 * ahnmatmae, sin3-p2-yet) therefore answer 0 here — the literal reading of
 * "well-formed MODERN syllable", recorded in libhunmin/src/hangul_valid.c. */
TEST(HangulValidTest, RejectsOldHangulBecauseItCannotPrecompose) {
    EXPECT_EQ(0, valid("안\xE1\x87\xAE"));  /* 안 + U+11EE, an archaic jongseong */
}

/* A buffer completes at a delimiter, so a sentence is not a buffer: whatever
 * the caller passed, a space, punctuation or a latin letter is not a syllable. */
TEST(HangulValidTest, RejectsNonSyllableCharacters) {
    EXPECT_EQ(0, valid("hello"));
    EXPECT_EQ(0, valid("1234"));         /* a digit run is a buffer under R6, and
                                          * not a Hangul word under R8 */
    EXPECT_EQ(0, valid("안녕 하세요"));
    EXPECT_EQ(0, valid("안녕."));
    EXPECT_EQ(0, valid("Ａ"));           /* fullwidth A, not a syllable */
}

/* Empty and unreadable input is "not valid", never a crash: a caller asks before
 * it knows whether it has a buffer at all. */
TEST(HangulValidTest, EmptyNullAndMalformedAreZero) {
    EXPECT_EQ(0, hunmin_hangul_valid(nullptr));
    EXPECT_EQ(0, valid(""));
    EXPECT_EQ(0, valid("\xC3"));                     /* truncated 2-byte sequence */
    EXPECT_EQ(0, valid("\xED\xA0\x80"));             /* UTF-16 surrogate half */
    EXPECT_EQ(0, valid("\xC0\xAF"));                 /* overlong "/" */
}

/* Engine-derived: the finished word 두벌식 produces is structurally valid, and
 * the same word with one more unfinished consonant is not. The expectations are
 * read from the engine, so this cannot pass by the test's own arithmetic. */
TEST(HangulValidTest, AgreesWithWhatTheEngineRenders) {
    std::string word = typed("dkssud");
    ASSERT_EQ(std::string("안녕"), word) << "두벌식 dkssud no longer renders 안녕";
    EXPECT_EQ(1, valid(word));

    std::string unfinished = typed("dkssudz"); /* the same word, then ㅋ in flight */
    EXPECT_EQ(0, valid(unfinished)) << unfinished;
    EXPECT_EQ(word, unfinished.substr(0, word.size()));

    EXPECT_EQ(0, valid(word + "ㅎ")) << "R8's own example (안녕ㅎ) must be invalid";
}

/* The predicate takes a string and no context, so there is no data directory it
 * could consult — #166's "no data file is consulted by the test" holds because
 * the shape makes it impossible. This pins the shape against a later signature
 * that quietly grows a hunmin_ctx. */
TEST(HangulValidTest, TakesNoLayer) {
    EXPECT_EQ(1, valid("가"));
    EXPECT_EQ(0, hunmin_hangul_valid(nullptr));
}

} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    kDataPath = std::getenv("HUNMIN_DATA_PATH");
    if (!kDataPath) {
        std::fprintf(stderr, "HUNMIN_DATA_PATH is not set; this test drives the "
                             "shipped tables, not a copy\n");
        return 1;
    }
    return RUN_ALL_TESTS();
}