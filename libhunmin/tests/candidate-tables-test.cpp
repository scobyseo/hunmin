/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier A: the Hanja candidate API (P3).
 *
 * These assertions are drawn from the data files themselves, not from what the
 * code happens to produce: the expected first candidate for 수 is 水 because
 * libhunmin/data/hanja/hanja.json lists it first, and 가화만사성's Hanja and
 * gloss are the ones in krdict.json. A converter regression that reordered or
 * dropped an entry therefore fails here rather than being papered over.
 *
 * The empty-string and NULL contracts matter as much as the values: a frontend
 * renders the list unconditionally, so an out-of-range index must not crash it.
 *
 * GoogleTest form (issue #109, option 1b): one original section = one TEST_F
 * case, and the executable is still ONE ctest entry (engine-candidates), because
 * registration shape -- not the framework -- is what the 30 s inner-loop
 * budget is sensitive to (docs/TEST-FRAMEWORK-OPTIONS.md §2).
 */
#include "libhunmin-test.h"

#include <gtest/gtest.h>

#include <cstring>

namespace {

/* v2 detail is "ko | en" built from the libhangul comment and the Unihan
 * kDefinition; all three fields are asserted so a converter regression that
 * drops a gloss fails here (docs/HANJA-GLOSS-DESIGN.md). */
void expect(hunmin_ctx *ctx, int i, const char *kind, const char *text, const char *detail) {
    const char *k = hunmin_suggest_kind(ctx, i);
    const char *t = hunmin_suggest_text(ctx, i);
    const char *d = hunmin_suggest_detail(ctx, i);
    EXPECT_STREQ(k, kind) << "[" << i << "] kind";
    EXPECT_STREQ(t, text) << "[" << i << "] text";
    if (detail) EXPECT_STREQ(d, detail) << "[" << i << "] detail";
}

/* The index of the first "hanja" candidate of a list, or -1. */
int first_hanja_index(hunmin_ctx *ctx, int n) {
    for (int i = 0; i < n; i++)
        if (!strcmp(hunmin_suggest_kind(ctx, i), "hanja")) return i;
    return -1;
}

class SuggestTest : public hunmin_test::CtxTest {};

/* --- a single syllable: word senses first (they carry a meaning), then the
 * syllable's Hanja candidates in upstream frequency order --- */
TEST_F(SuggestTest, SyllableHanjaInUpstreamFrequencyOrder) {
    int n = hunmin_suggest_count(ctx_, "수");
    ASSERT_GT(n, 0) << "수 has candidates";

    /* 水 must be the first *hanja* candidate: hanja.json lists it first. */
    int first_hanja = first_hanja_index(ctx_, n);
    ASSERT_GE(first_hanja, 0) << "수 has a hanja candidate";
    expect(ctx_, first_hanja, "hanja", "水", "물 수, 고를 수 | water, liquid, lotion, juice");
    expect(ctx_, first_hanja + 1, "hanja", "手", "손 수, 잡을 수, 칠 수 | hand");
    expect(ctx_, first_hanja + 2, "hanja", "數",
           "헤아릴 수, 수죄할 수, 몇 수, 이치 수, 자주 수 | "
           "number; several; count; fate");
}

/* 한 leads with 韓 in hanja.json (verified: 韓 漢 寒 限 閑 恨). */
TEST_F(SuggestTest, HanLeadsWithHan) {
    int n = hunmin_suggest_count(ctx_, "한");
    ASSERT_GT(n, 0) << "한 has candidates";
    int first_hanja = first_hanja_index(ctx_, n);
    ASSERT_GE(first_hanja, 0) << "한 has a hanja candidate";
    expect(ctx_, first_hanja, "hanja", "韓", "나라 이름 한, 한나라 한 | fence; surname; Korea");
}

/* --- a word with a gloss: exactly what krdict.json says --- */
TEST_F(SuggestTest, WordGlossMatchesKrdict) {
    int n = hunmin_suggest_count(ctx_, "가화만사성");
    ASSERT_GE(n, 1) << "가화만사성 has a candidate";
    expect(ctx_, 0, "word", "家和萬事成", "가정이 화목하면 모든 일이 잘 이루어짐.");
}

/* 금시초문 is in both tables -- krdict carries 今始初聞 with its gloss and
 * idioms carries 今時初聞 with the other reading. Four syllables means the
 * idiom table is consulted first, so it wins and the krdict entry follows.
 * Both Hanja forms are real, which is why the data keeps them apart. */
TEST_F(SuggestTest, IdiomWinsOverTheKrdictEntry) {
    int n = hunmin_suggest_count(ctx_, "금시초문");
    ASSERT_GE(n, 2) << "금시초문 resolves in both tables";
    expect(ctx_, 0, "idiom", "今時初聞", "금시초문");
    expect(ctx_, 1, "word", "今始初聞", "어떤 이야기를 지금 처음으로 들음.");
}

/* --- a four-character idiom, keyed by its Hangul reading --- */
TEST_F(SuggestTest, IdiomKeyedByHangulReading) {
    int n = hunmin_suggest_count(ctx_, "일석이조");
    ASSERT_GE(n, 1) << "일석이조 has a candidate";
    expect(ctx_, 0, "idiom", "一石二鳥", "일석이조");
    /* The same idiom also sits in krdict with its own gloss; the idiom table is
     * queried first, so the idiom entry must win rather than being shadowed. */
    EXPECT_STREQ(hunmin_suggest_kind(ctx_, 0), "idiom") << "idiom outranks the krdict entry for the same word";
    /* the same idiom under its Hanja is not how the table is keyed: a lookup
     * by Hanja must not silently resolve */
    EXPECT_EQ(hunmin_suggest_count(ctx_, "一石二鳥"), 0) << "idioms are keyed by Hangul, not Hanja";
}

/* --- ordering and the idiom gate ---
 *
 * Idioms are queried before words, so a four-character idiom with its own
 * reading is not shadowed by the krdict entry for the same word (금시초문
 * is in both, and the idiom wins).
 *
 * The syllable >= 4 gate on the idiom table cannot be exercised against
 * this data: all 2,318 readings are exactly four syllables, so no lookup
 * would reach the idiom table with a shorter key even if the gate were
 * removed. It is kept as a guard for a future table, not as tested
 * behaviour, and a mutation that deletes it survives the suite -- recorded
 * here so that is a known gap rather than a surprise.
 */
TEST_F(SuggestTest, OneSyllableLookupKeepsItsWordEntries) {
    /* krdict is not length-gated: it holds 527 one-character entries and they
     * are real words -- 壽 "a long life" and 手 "a game of go" for 수. A gate at
     * two syllables would hide them. Glossed senses lead the list. */
    int n = hunmin_suggest_count(ctx_, "수");
    ASSERT_GT(n, 0) << "수 has candidates";
    bool has_word = false;
    for (int i = 0; i < n; i++)
        if (!strcmp(hunmin_suggest_kind(ctx_, i), "word")) has_word = true;
    EXPECT_TRUE(has_word) << "수 keeps its one-syllable word entries (壽, 手)";
    EXPECT_STREQ(hunmin_suggest_kind(ctx_, 0), "word") << "glossed word senses lead a one-syllable lookup";
}

/* --- misses and the never-NULL contract --- */
TEST_F(SuggestTest, MissesAndTheNeverNullContract) {
    EXPECT_EQ(hunmin_suggest_count(ctx_, "없는말xyz"), 0) << "unknown input yields 0";
    EXPECT_EQ(hunmin_suggest_count(ctx_, ""), 0) << "empty input yields 0";
    EXPECT_EQ(hunmin_suggest_count(ctx_, nullptr), 0) << "NULL input yields 0";
    EXPECT_EQ(hunmin_suggest_count(nullptr, "수"), 0) << "NULL ctx yields 0";
    EXPECT_EQ(hunmin_suggest_text(ctx_, -1)[0], 0) << "negative index -> empty";
    EXPECT_EQ(hunmin_suggest_text(ctx_, 9999)[0], 0) << "past-the-end -> empty";
    EXPECT_EQ(hunmin_suggest_detail(ctx_, 9999)[0], 0) << "past-the-end detail -> empty";
    EXPECT_EQ(hunmin_suggest_kind(ctx_, 9999)[0], 0) << "past-the-end kind -> empty";
}

/* --- the list is replaced, not appended to, by the next lookup --- */
TEST_F(SuggestTest, ListIsReplacedNotAppended) {
    int a = hunmin_suggest_count(ctx_, "수");
    EXPECT_NE(hunmin_suggest_count(ctx_, "한"), 0) << "한 still resolves";
    EXPECT_EQ(hunmin_suggest_count(ctx_, "수"), a) << "repeat lookup is stable";
    EXPECT_NE(hunmin_suggest_text(ctx_, 0)[0], 0) << "count() re-populated the list";
}

} // namespace
