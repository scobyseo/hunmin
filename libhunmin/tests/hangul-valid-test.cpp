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
 * (choseong + jungseong + jongseong) only". So the test asserts that PROPERTY
 * over its whole domain rather than sampling it:
 *
 *   1. every (cho, jung, jong) triple the requirement allows is accepted, and
 *      the accepted string is the syllable the standard forms from that triple;
 *   2. nothing else in the BMP is accepted on its own — the set is closed, not
 *      merely non-empty;
 *   3. no jamo, conjoining or compatibility, can appear anywhere in a valid
 *      string (they are the half-composed form, not a syllable);
 *   4. concatenation keeps validity and one foreign character anywhere breaks
 *      it, which is what "composed of … syllables" means for a run.
 *
 * The oracle is independent of the implementation on purpose: the engine
 * derives its sets from libhunmin/src/tables.c (libhangul's own predicates),
 * while this file derives the same 11,172 syllables from the Unicode arithmetic
 * — U+AC00 + ((cho*21 + jung)*28 + jong) — and the standard block boundaries.
 * The two agreeing over the whole domain is the assertion; a drift in either
 * reads red instead of passing through both sides.
 *
 * Engine-derived cases come last: they read the rendering off the engine
 * (`dkssud` → 안녕) instead of typing the expected string, so a hand-written
 * expectation cannot encode a defect.
 *
 * The "no data file is consulted" half of #166's checklist holds by
 * construction: hunmin_hangul_valid takes a string and no context, so there is
 * no directory it could read. `TakesNoLayer` pins that shape.
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

/* The modern sets the requirement names, counted rather than guessed: 19
 * choseong, 21 jungseong, 28 jongseong slots (27 + "no jongseong") = 11,172
 * syllables, which is exactly the U+AC00..U+D7A3 block. */
const uint32_t kChoCount = 19;
const uint32_t kJungCount = 21;
const uint32_t kJongCount = 27;
const uint32_t kSyllableFirst = 0xAC00;
const uint32_t kSyllableLast = 0xD7A3;
const int kSyllables = 19 * 21 * 28;

/* UTF-8 encode one code point; returns the length. Only the ranges this test
 * uses, so no surrogate/overlong handling is needed here. */
int utf8(uint32_t c, char *out) {
    if (c < 0x80) {
        out[0] = (char)c;
        return 1;
    }
    if (c < 0x800) {
        out[0] = (char)(0xC0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 0x3F));
        return 2;
    }
    if (c < 0x10000) {
        out[0] = (char)(0xE0 | (c >> 12));
        out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[2] = (char)(0x80 | (c & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (c >> 18));
    out[1] = (char)(0x80 | ((c >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((c >> 6) & 0x3F));
    out[3] = (char)(0x80 | (c & 0x3F));
    return 4;
}

std::string one(uint32_t c) {
    char buf[4];
    int n = utf8(c, buf);
    return std::string(buf, (size_t)n);
}

int valid_at(uint32_t c) { return hunmin_hangul_valid(one(c).c_str()); }

int valid(const std::string &s) { return hunmin_hangul_valid(s.c_str()); }

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

/* 1. The property, generated from the requirement: for every cho, every jung and
 * every jongseong slot, the syllable the standard forms from that triple is
 * accepted. */
TEST(HangulValidTest, EveryAllowedTripleIsAccepted) {
    int checked = 0;
    int mismatched = 0;

    for (uint32_t cho = 0; cho < kChoCount; cho++) {
        for (uint32_t jung = 0; jung < kJungCount; jung++) {
            for (uint32_t jong = 0; jong <= kJongCount; jong++) {
                /* jong == 0 is the (cho + jung) shape; 1..27 are the 27
                 * jongseong of the (cho + jung + jong) shape. 28 slots. */
                const uint32_t expected =
                    kSyllableFirst + ((cho * kJungCount + jung) * 28) + jong;
                const std::string text = one(expected);
                checked++;
                if (hunmin_hangul_valid(text.c_str()) != 1) {
                    if (mismatched < 8) {
                        ADD_FAILURE() << "rejected cho#" << cho << " jung#" << jung
                                      << " jong#" << jong << " (" << text << ")";
                    }
                    mismatched++;
                }
            }
        }
    }
    EXPECT_EQ(kSyllables, checked);
    EXPECT_EQ(0, mismatched);
}

/* 2. The set is closed: walking the whole BMP, the ONLY code points accepted on
 * their own are the 11,172 syllables. This is what makes the first test mean
 * something — a predicate that answered 1 for everything would pass it. */
TEST(HangulValidTest, NothingOutsideTheBlockIsAcceptedAlone) {
    int outside_accepted = 0;

    for (uint32_t c = 1; c < 0x10000; c++) {
        if (c >= kSyllableFirst && c <= kSyllableLast) continue;
        if (c >= 0xD800 && c <= 0xDFFF) continue; /* not encodable as UTF-8 */
        if (valid_at(c)) {
            if (outside_accepted < 8) ADD_FAILURE() << "accepted U+" << std::hex << c;
            outside_accepted++;
        }
    }
    EXPECT_EQ(0, outside_accepted);

    int inside_rejected = 0;
    for (uint32_t c = kSyllableFirst; c <= kSyllableLast; c++) {
        if (!valid_at(c)) inside_rejected++;
    }
    EXPECT_EQ(0, inside_rejected) << "of " << kSyllables << " syllables";
}

/* 3. Jamo are the half-composed form, and no string containing one is valid:
 * alone, after a syllable, before one. */
TEST(HangulValidTest, NoJamoCanAppearInAValidString) {
    std::vector<uint32_t> jamo;
    for (uint32_t c = 0x1100; c <= 0x11FF; c++) jamo.push_back(c); /* conjoining */
    for (uint32_t c = 0x3131; c <= 0x318E; c++) jamo.push_back(c); /* compatibility */
    for (uint32_t c = 0xA960; c <= 0xA97C; c++) jamo.push_back(c); /* cho ext-A */
    for (uint32_t c = 0xD7B0; c <= 0xD7FB; c++) jamo.push_back(c); /* jung/jong ext-B */

    int wrong = 0;
    for (uint32_t j : jamo) {
        const std::string s = one(j);
        if (valid(s) != 0) {
            if (wrong < 8) ADD_FAILURE() << "jamo U+" << std::hex << j << " alone";
            wrong++;
        }
        if (valid("안녕" + s) != 0) {
            if (wrong < 8) ADD_FAILURE() << "jamo U+" << std::hex << j << " appended";
            wrong++;
        }
        if (valid(s + "안녕") != 0) {
            if (wrong < 8) ADD_FAILURE() << "jamo U+" << std::hex << j << " prepended";
            wrong++;
        }
    }
    EXPECT_EQ(0, wrong) << "over " << jamo.size() << " jamo";
}

/* 4. "Composed of syllables" means the run rule: concatenating valid syllables is
 * valid, and one foreign character anywhere breaks the whole string. */
TEST(HangulValidTest, RunsKeepValidityAndOneForeignCharacterBreaksIt) {
    const char *pieces[] = {"가", "각", "안녕", "뷁똠", "쀍"};
    for (const char *p : pieces) EXPECT_EQ(1, valid(p)) << p;

    /* A run built from a valid piece stays valid at every length. */
    std::string run;
    for (int i = 0; i < 32; i++) {
        run += "가";
        ASSERT_EQ(1, valid(run)) << "length " << i + 1;
    }

    /* The same run with one foreign character at the front, in the middle and at
     * the end is invalid: validity is a property of the whole string. */
    for (char foreign : {' ', '1', 'a', '.', '-', '\t'}) {
        EXPECT_EQ(0, valid(run + foreign)) << "'" << foreign << "' appended";
        EXPECT_EQ(0, valid(foreign + run)) << "'" << foreign << "' prepended";
        EXPECT_EQ(0, valid(run.substr(0, 4) + foreign + run.substr(4)))
            << "'" << foreign << "' in the middle";
    }
}

/* Empty and unreadable input is "not valid", never a crash: a caller asks before
 * it knows whether it has a buffer at all. */
TEST(HangulValidTest, EmptyNullAndMalformedAreZero) {
    EXPECT_EQ(0, hunmin_hangul_valid(nullptr));
    EXPECT_EQ(0, valid(""));
    EXPECT_EQ(0, valid("\xC3"));                 /* truncated 2-byte sequence */
    EXPECT_EQ(0, valid("\xED\xA0\x80"));         /* UTF-16 surrogate half */
    EXPECT_EQ(0, valid("\xC0\xAF"));             /* overlong "/" */
    EXPECT_EQ(0, valid("\x80"));                 /* stray continuation byte */
    EXPECT_EQ(0, valid("\xF8\x88\x80\x80\x80")); /* 5-byte form */
}

/* Old Hangul: hm_jamo_to_syllable composes only the modern 19/21/28, so an
 * archaic jongseong cannot become a precomposed syllable and the composer
 * renders it as jamo. Four shipped layouts (dubeolsik-yet, sebeolsik-yet,
 * ahnmatmae, sin3-p2-yet) therefore answer 0 here — the literal reading of
 * "well-formed MODERN syllable", recorded in libhunmin/src/hangul_valid.c. */
TEST(HangulValidTest, RejectsOldHangulBecauseItCannotPrecompose) {
    EXPECT_EQ(0, valid("안\xE1\x87\xAE")); /* 안 + U+11EE, an archaic jongseong */
    EXPECT_EQ(0, valid_at(0x11EE));
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