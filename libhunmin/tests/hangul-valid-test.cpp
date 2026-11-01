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

/* Every jamo code point the engine can hold or show, in one place: conjoining
 * (U+1100-U+11FF), compatibility (U+3131-U+318E) and the two extension blocks.
 * A jamo is the half-composed form, never a syllable. */
std::vector<uint32_t> jamo_set() {
    std::vector<uint32_t> jamo;
    for (uint32_t c = 0x1100; c <= 0x11FF; c++) jamo.push_back(c);
    for (uint32_t c = 0x3131; c <= 0x318E; c++) jamo.push_back(c);
    for (uint32_t c = 0xA960; c <= 0xA97C; c++) jamo.push_back(c);
    for (uint32_t c = 0xD7B0; c <= 0xD7FB; c++) jamo.push_back(c);
    return jamo;
}

/* 3. Jamo are the half-composed form, and no string containing one is valid:
 * alone, after a syllable, before one. */
TEST(HangulValidTest, NoJamoCanAppearInAValidString) {
    const std::vector<uint32_t> jamo = jamo_set();

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

/* 4. A WHOLE unfinished syllable after a finished one is the shape a real buffer
 * ends in while the user is still typing — `랴ㅜㄷ` is the reported case. One
 * jamo is covered above; a run of them must be just as invalid, in every
 * ordered combination, alone and after a syllable. */
TEST(HangulValidTest, NoRunOfJamoIsEverValid) {
    /* the reported case, named, so a future widening cannot quietly accept it */
    EXPECT_EQ(1, valid("랴"));
    EXPECT_EQ(0, valid("랴ㅜㄷ"));
    EXPECT_EQ(0, valid("랴ㅜ"));
    EXPECT_EQ(0, valid("ㅜㄷ"));

    const std::vector<uint32_t> jamo = jamo_set();
    int wrong = 0;
    for (uint32_t a : jamo) {
        const std::string first = one(a);
        for (uint32_t b : jamo) {
            const std::string pair = first + one(b);
            if (valid(pair) != 0 || valid("랴" + pair) != 0) {
                if (wrong < 8) {
                    ADD_FAILURE() << "accepted U+" << std::hex << a << " U+" << b;
                }
                wrong++;
            }
        }
    }
    EXPECT_EQ(0, wrong) << "over " << jamo.size() << "^2 ordered pairs";
}

/* 5. "Composed of syllables" means the run rule: concatenating valid syllables is
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

/* ---- the widened rule, for the layouts that compose 옛한글 (ruled 2026-11-01:
 * "same shape rule, widened jamo sets").
 *
 * The sets are tables.c's hm_is_choseong/jungseong/jongseong — the conjoining
 * blocks including the two extension planes — and the group is still
 * (cho)(jung)(jong) with a leading cho required. */

std::vector<uint32_t> set_range(std::initializer_list<std::pair<uint32_t, uint32_t>> ranges) {
    std::vector<uint32_t> v;
    for (auto r : ranges)
        for (uint32_t c = r.first; c <= r.second; c++) v.push_back(c);
    return v;
}

std::vector<uint32_t> cho_set() {
    return set_range({{0x1100, 0x115F}, {0xA960, 0xA97C}});
}
std::vector<uint32_t> jung_set() {
    return set_range({{0x1160, 0x11A7}, {0xD7B0, 0xD7C6}});
}
std::vector<uint32_t> jong_set() {
    return set_range({{0x11A8, 0x11FF}, {0xD7CB, 0xD7FB}});
}

int old_valid(const std::string &s) { return hunmin_hangul_valid_old(s.c_str()); }

/* The ruled cases, named: the 옛글 shapes the engine renders for a finished
 * syllable are valid, and the shapes it never renders are not. */
TEST(HangulValidOldTest, TheRuledCases) {
    /* cho+jung and cho+jung+jong over the extended sets — what dubeolsik-yet and
     * ahnmatmae actually render (measured: \sa k -> ᅀᅡ, \sak\sf -> ᅀᅡᆶ,
     * \sg k -> ᅌᅵ, sin3-p2-basic k\sz -> ᄀᆞ). */
    for (const char *s : {"ᅀᅡ", "ᅌᅵ", "ᅀᅡᆶ", "ᄀᆞ"})
        EXPECT_EQ(1, old_valid(s)) << s;
    EXPECT_EQ(1, old_valid("\xE1\x85\x9F\xE1\x85\xA1")); /* CF filler + ㅏ */

    /* 랴ㅜㄷ and its parts: a jungseong with no leading cho is not a group the
     * composer emits (it writes the CF filler instead). */
    for (const char *s : {"랴ㅜㄷ", "랴ㅜ", "ㅜㄷ", "ᅡᆨ"})
        EXPECT_EQ(0, old_valid(s)) << s;

    /* in-flight syllables: a cho with no jung, or a lone jamo */
    for (const char *s : {"ᅀ", "ㄱ", "ㆍ", ""})
        EXPECT_EQ(0, old_valid(s)) << s;
    EXPECT_EQ(0, hunmin_hangul_valid_old(nullptr));

    /* modern input is a subset; non-Hangul never is */
    EXPECT_EQ(1, old_valid("안녕"));
    for (const char *s : {"hello", "1234", "안녕 하세요", "안녕."})
        EXPECT_EQ(0, old_valid(s)) << s;
}

/* The property, generated: every cho of the extended set opens a group, every
 * jung continues it, every jong closes it. If the sets were the modern 19/21/28
 * this fails on the first archaic letter, which is the regression this guards. */
TEST(HangulValidOldTest, EveryExtendedLetterFillsItsSlot) {
    const std::string jung = one(0x1161); /* ㅏ */
    const std::string jong = one(0x11A8); /* ㄱ */
    int bad = 0;
    for (uint32_t cho : cho_set())
        if (old_valid(one(cho) + jung) != 1) {
            if (bad++ < 8) ADD_FAILURE() << "cho U+" << std::hex << cho;
        }
    EXPECT_EQ(0, bad);

    const std::string cho = one(0x1100); /* ᄀ */
    for (uint32_t j : jung_set())
        if (old_valid(cho + one(j)) != 1) {
            if (bad++ < 8) ADD_FAILURE() << "jung U+" << std::hex << j;
        }
    EXPECT_EQ(0, bad);

    for (uint32_t j : jong_set())
        if (old_valid(cho + jung + one(j)) != 1) {
            if (bad++ < 8) ADD_FAILURE() << "jong U+" << std::hex << j;
        }
    EXPECT_EQ(0, bad);
}

/* And the negative half: no letter of the extended sets is valid on its own or
 * out of slot, so the rule is still a shape and not "any Hangul letter". */
TEST(HangulValidOldTest, NoLetterIsValidOnItsOwnOrOutOfSlot) {
    int wrong = 0;
    for (uint32_t c : cho_set())
        if (old_valid(one(c)) != 0) wrong++;
    for (uint32_t c : jung_set())
        if (old_valid(one(c)) != 0) wrong++;
    for (uint32_t c : jong_set())
        if (old_valid(one(c)) != 0) wrong++;
    EXPECT_EQ(0, wrong);
}

/* Which rule each shipped layout needs. Derived from the layout data at load
 * time, so this list is the assertion that the derivation still lands on the
 * same six layouts — and the count makes a NEW layout file fail here until
 * someone says where it stands. */
TEST(HangulValidOldTest, LayoutCapabilityIsDerivedFromTheData) {
    hunmin_ctx *ctx = make_ctx();
    ASSERT_NE(ctx, nullptr);
    const int n = hunmin_ctx_layout_count(ctx);
    EXPECT_EQ(15, n) << "a layout was added or removed: place it in the lists below";

    const char *old_layouts[] = {"ahnmatmae", "dubeolsik-yet", "sebeolsik-yet",
                                 "sin3-p2",    "sin3-p2-basic", "sin3-p2-yet"};
    const char *latin_only[] = {"colemak", "dvorak", "us-qwerty"};

    int old_seen = 0, latin_seen = 0;
    for (int i = 0; i < n; i++) {
        const char *id = hunmin_ctx_layout_id(ctx, i);
        ASSERT_NE(id, nullptr);
        const bool is_old = hunmin_ctx_layout_old_hangul(ctx, i);
        const bool composes = hunmin_ctx_layout_autoswitch_capable(ctx, i);
        bool named_old = false, named_latin = false;
        for (const char *o : old_layouts) named_old = named_old || !std::strcmp(id, o);
        for (const char *l : latin_only) named_latin = named_latin || !std::strcmp(id, l);

        EXPECT_EQ(named_old, is_old) << id << " old_hangul";
        EXPECT_EQ(!named_latin, composes) << id << " autoswitch_capable";
        /* an old-Hangul layout must still compose Hangul: the two answers are not
         * independent, and asking the wide rule about a layout that cannot produce
         * these buffers at all would be meaningless. */
        if (is_old) EXPECT_TRUE(composes) << id;
        if (is_old) old_seen++;
        if (named_latin) latin_seen++;
    }
    EXPECT_EQ(6, old_seen);
    EXPECT_EQ(3, latin_seen);
    EXPECT_EQ(n, old_seen + latin_seen + 6) << "the three lists must cover every layout";
    hunmin_ctx_free(ctx);
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