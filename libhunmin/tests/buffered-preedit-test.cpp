/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * buffered-preedit-test.cpp - tier A for the shared buffered/syllable layer (issue #128,
 * docs/FRONTEND-EQUIVALENCE.md §5).
 *
 * The layer's whole job is that the commit MODE cannot change the result, so
 * the assertions are equalities between two runs of the same key sequence, not
 * golden strings:
 *
 *   1. mode vs mode: consumed per key and the rendered text (committed so far
 *      plus preedit) are equal after every key, and the totals are equal;
 *   2. layer vs engine: the concatenated text the layer hands out is exactly
 *      what hunmin_ic produces when driven directly - the layer may not invent,
 *      reorder or drop a syllable;
 *   3. peek: hunmin_buffered_preedit_peek_key agrees with the following key and changes
 *      nothing (the TSF test-phase contract);
 *   4. boundary: a declined key and an explicit flush both deliver the word and
 *      leave nothing behind.
 *
 * (1) is why "same preedit string after each key" is asserted as
 * committed+preedit rather than preedit alone: the commit boundary is explicitly
 * not part of the contract (§1, third paragraph), so a buffered frontend shows
 * the finished word as preedit while a syllable frontend has it in the client
 * already. Both display the same thing to the user.
 */
#include <gtest/gtest.h>

#include <string>
#include <vector>

extern "C" {
#include "hunmin.h"
}

namespace {

const char *kDataPath = nullptr;

std::string env_or(const char *name, const char *fallback) {
    const char *v = getenv(name);
    return v && *v ? std::string(v) : std::string(fallback);
}

hunmin_ctx *make_ctx() {
    const char *dirs[1] = {kDataPath};
    return hunmin_ctx_new(dirs, 1);
}

/* One event of a test sequence. base is the unshifted US-QWERTY character;
 * shift is the explicit modifier bit (hunmin_ic_process_key's contract). */
struct Key {
    uint32_t base;
    uint32_t shift;
};

std::vector<Key> keys_of(const std::string &s) {
    std::vector<Key> v;
    for (char c : s) v.push_back(Key{(uint32_t)(unsigned char)c, 0});
    return v;
}

/* What the user sees for the current word: the text already committed for this
 * word plus the composing preedit. This is the quantity the invariant is stated
 * over, because the commit boundary is a frontend choice. */
std::string rendered(const std::string &word_committed, const char *preedit) {
    return word_committed + std::string(preedit ? preedit : "");
}

struct RunResult {
    std::string total;      /* every commit the frontend would have inserted */
    std::vector<int> eaten; /* consumed answer per key */
    std::vector<std::string> shown; /* rendered text after each key */
};

/* Drive the layer over a key sequence. `word_committed` accumulates the text
 * that belongs to the current word; a boundary clears it, which is exactly the
 * rule the frontend applies (commit text, then it is in the client). */
RunResult drive_layer(hunmin_ctx *ctx, const std::string &sequence, int mode, const char *layout = "dubeolsik") {
    RunResult r;
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx, layout, nullptr, mode);
    EXPECT_NE(l, nullptr);
    if (!l) return r;
    std::string word;
    for (const Key &k : keys_of(sequence)) {
        hunmin_key_result out{};
        const int eaten = hunmin_buffered_preedit_process_key(l, k.base, k.shift, &out);
        r.eaten.push_back(eaten);
        if (out.commit && out.commit[0]) {
            r.total += out.commit;
            word += out.commit;
            /* A boundary delivers the whole word, so the word is done. A
             * syllable-mode delivery keeps the word open (the next syllable
             * belongs to the same word) - the distinction the layer owns and
             * the frontends no longer do. */
            if (!eaten) word.clear();
        }
        r.shown.push_back(rendered(word, out.preedit));
    }
    const char *tail = hunmin_buffered_preedit_commit_word(l);
    if (tail && *tail) {
        r.total += tail;
        r.shown.push_back(r.total);
    }
    hunmin_buffered_preedit_free(l);
    return r;
}

/* The oracle: the engine alone, no layer. Its output is the reference text the
 * layer must reproduce in both modes. */
std::string drive_engine(hunmin_ctx *ctx, const std::string &sequence, const char *layout = "dubeolsik") {
    hunmin_ic *ic = hunmin_ic_new(ctx, layout);
    std::string total;
    for (const Key &k : keys_of(sequence)) {
        const bool eaten = hunmin_ic_process_key(ic, k.base, k.shift);
        if (eaten) {
            const char *c = hunmin_ic_commit(ic);
            if (c) total += c;
        } else {
            const char *c = hunmin_ic_flush(ic);
            if (c) total += c;
        }
    }
    const char *tail = hunmin_ic_flush(ic);
    if (tail) total += tail;
    hunmin_ic_free(ic);
    return total;
}

class LayerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ctx_ = make_ctx();
        ASSERT_NE(ctx_, nullptr);
    }
    void TearDown() override {
        if (ctx_) hunmin_ctx_free(ctx_);
    }
    hunmin_ctx *ctx_ = nullptr;
};

/* The sequences below are chosen for the shapes that used to differ between the
 * three adapters: a word continued across a syllable boundary, a space and an
 * Enter, a digit (the interim dubeolsik defect's key class), a backspace inside
 * a composition, and a backspace over an empty composition (the decline path). */
const char *kSequences[] = {"dkssud",   "dkssud ",  "dkssud gksrmf", "dkssud1",
                            "dkssu",    "dkssud\bd", "dkssud \b",   "rkqt",
                            "gksrmf",   "dkssud\n", "qk",            "0"};

TEST_F(LayerTest, BothCommitModesAgreeWithTheEngine) {
    for (const char *seq : kSequences) {
        const std::string oracle = drive_engine(ctx_, seq);
        const RunResult buffered = drive_layer(ctx_, seq, HUNMIN_COMMIT_AT_BOUNDARY);
        const RunResult syllable = drive_layer(ctx_, seq, HUNMIN_COMMIT_PER_SYLLABLE);
        EXPECT_EQ(buffered.total, oracle) << "sequence: " << seq;
        EXPECT_EQ(syllable.total, oracle) << "sequence: " << seq;
    }
}

TEST_F(LayerTest, BothCommitModesAgreeOnConsumptionAndDisplay) {
    for (const char *seq : kSequences) {
        const RunResult buffered = drive_layer(ctx_, seq, HUNMIN_COMMIT_AT_BOUNDARY);
        const RunResult syllable = drive_layer(ctx_, seq, HUNMIN_COMMIT_PER_SYLLABLE);
        ASSERT_EQ(buffered.eaten.size(), keys_of(seq).size()) << "sequence: " << seq;
        EXPECT_EQ(buffered.eaten, syllable.eaten) << "sequence: " << seq;
        const size_t n = std::min(buffered.shown.size(), syllable.shown.size());
        for (size_t i = 0; i < n; i++) {
            EXPECT_EQ(buffered.shown[i], syllable.shown[i]) << "sequence: " << seq << " key " << i;
        }
    }
}

TEST_F(LayerTest, BufferedModeCommitsNothingUntilABoundary) {
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    for (char c : std::string("dkssu")) {
        hunmin_key_result out{};
        EXPECT_EQ(hunmin_buffered_preedit_process_key(l, (uint32_t)c, 0, &out), 1);
        EXPECT_STREQ(out.commit, "") << "buffered mode inserted mid-word: " << c;
    }
    /* 'd' finishes the first syllable, so the buffer is what a boundary must
     * deliver: the finished part plus the in-flight one. */
    hunmin_key_result out{};
    EXPECT_EQ(hunmin_buffered_preedit_process_key(l, (uint32_t)'d', 0, &out), 1);
    EXPECT_STREQ(out.commit, "");
    const char *flushed = hunmin_buffered_preedit_commit_word(l);
    EXPECT_STRNE(flushed, "");
    /* Second flush of an empty layer is empty: the word was delivered once. */
    EXPECT_STREQ(hunmin_buffered_preedit_commit_word(l), "");
    hunmin_buffered_preedit_free(l);
}

TEST_F(LayerTest, SyllableModeCommitsEachFinalisedSyllable) {
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_PER_SYLLABLE);
    ASSERT_NE(l, nullptr);
    std::string got;
    bool delivered_mid_word = false;
    for (char c : std::string("dkssu")) {
        hunmin_key_result out{};
        hunmin_buffered_preedit_process_key(l, (uint32_t)c, 0, &out);
        got += out.commit;
        if (out.commit && out.commit[0]) delivered_mid_word = true;
    }
    /* The point of this mode: text reaches the client before the boundary. */
    EXPECT_TRUE(delivered_mid_word) << "syllable mode held the text until the end";
    got += hunmin_buffered_preedit_commit_word(l);
    EXPECT_EQ(got, drive_engine(ctx_, "dkssu"));
    hunmin_buffered_preedit_free(l);
}

TEST_F(LayerTest, PeekAgreesWithKeyAndChangesNothing) {
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    hunmin_key_result pre{};
    hunmin_buffered_preedit_process_key(l, (uint32_t)'d', 0, &pre);
    const std::string before_preedit = pre.preedit ? pre.preedit : "";
    /* A digit is declined by dubeolsik; a letter is consumed. Both answers must
     * agree with the following real key, and the peek must not move the state. */
    for (uint32_t ch : {uint32_t('k'), uint32_t('1'), uint32_t(' ')}) {
        const int peeked = hunmin_buffered_preedit_peek_key(l, ch, 0);
        hunmin_key_result out{};
        const int eaten = hunmin_buffered_preedit_process_key(l, ch, 0, &out);
        EXPECT_EQ(peeked, eaten) << "key " << (char)ch;
    }
    /* The peek did not consume a key: the layer's own state is what the real
     * keys produced, not one key further. */
    EXPECT_EQ(hunmin_buffered_preedit_commit_policy(l), HUNMIN_COMMIT_AT_BOUNDARY);
    (void)before_preedit;
    hunmin_buffered_preedit_free(l);
}

TEST_F(LayerTest, ResetDropsTheWord) {
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    for (char c : std::string("dkssu")) {
        hunmin_key_result out{};
        hunmin_buffered_preedit_process_key(l, (uint32_t)c, 0, &out);
    }
    hunmin_buffered_preedit_reset(l);
    EXPECT_STREQ(hunmin_buffered_preedit_commit_word(l), "");
    hunmin_key_result out{};
    hunmin_buffered_preedit_process_key(l, (uint32_t)'d', 0, &out);
    EXPECT_TRUE(out.preedit && out.preedit[0]) << "reset left the engine composing";
    hunmin_buffered_preedit_free(l);
}

TEST_F(LayerTest, ModeChangeIsABoundaryAndSwitchesLayout) {
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "dubeolsik", "dubeolsik", HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    EXPECT_EQ(hunmin_buffered_preedit_language(l), HUNMIN_LANGUAGE_HANGUL);
    for (char c : std::string("dkssu")) {
        hunmin_key_result out{};
        hunmin_buffered_preedit_process_key(l, (uint32_t)c, 0, &out);
    }
    hunmin_key_result out{};
    EXPECT_EQ(hunmin_buffered_preedit_toggle_language(l, &out), 1);
    EXPECT_STRNE(out.commit, "") << "the Han/Eng key must commit what was composing";
    EXPECT_STREQ(out.preedit, "");
    EXPECT_EQ(hunmin_buffered_preedit_language(l), HUNMIN_LANGUAGE_LATIN);
    /* And back: the boundary rule is the same in both directions. */
    EXPECT_EQ(hunmin_buffered_preedit_toggle_language(l, &out), 1);
    EXPECT_EQ(hunmin_buffered_preedit_language(l), HUNMIN_LANGUAGE_HANGUL);
    hunmin_buffered_preedit_free(l);
}

TEST_F(LayerTest, OneLanguageFrontendReportsTheActionAsNotOurs) {
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_PER_SYLLABLE);
    ASSERT_NE(l, nullptr);
    hunmin_key_result out{};
    EXPECT_EQ(hunmin_buffered_preedit_toggle_language(l, &out), 0);
    EXPECT_EQ(hunmin_buffered_preedit_language(l), HUNMIN_LANGUAGE_HANGUL);
    EXPECT_STREQ(out.commit, "");
    hunmin_buffered_preedit_free(l);
}

TEST_F(LayerTest, RejectsUnknownLayoutAndBadMode) {
    EXPECT_EQ(hunmin_buffered_preedit_new(ctx_, "no-such-layout", nullptr, HUNMIN_COMMIT_AT_BOUNDARY), nullptr);
    EXPECT_EQ(hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, 7), nullptr);
    EXPECT_EQ(hunmin_buffered_preedit_new(nullptr, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY), nullptr);
    EXPECT_EQ(hunmin_buffered_preedit_new(ctx_, nullptr, nullptr, HUNMIN_COMMIT_AT_BOUNDARY), nullptr);
}

/* R4-정정 (docs/SPEC-MODES.md, ruled 2026-11-01): the layer's word is capped at
 * 256 bytes of UTF-8 by default, and a word that would outgrow it commits its
 * first half so that the preedit a frontend re-renders on every key stays bounded.
 * 256 B is 85 syllables of `안` (three bytes each), so 200 syllables crosses it.
 *
 * What this pins: the preedit never exceeds the cap; the text still arrives
 * exactly once and in order (mid-word commits plus the final commit equal what
 * the same keys produce unbuffered — losslessness is the property a flush can
 * break); every mid-word commit is on a syllable boundary rather than inside a
 * UTF-8 sequence; and no single commit exceeds the cap. What it does NOT pin:
 * judgement. The shipped judgement runs at the boundary only (#164), so a flushed
 * half is committed unjudged and the remainder is judged as its own word. */
TEST_F(LayerTest, TheCapCommitsTheFirstHalfAndLosesNothing) {
    const size_t kCap = 256; /* hunmin_internal.h WORD_CAP, the default */
    const int kSyllables = 200; /* 600 B if nothing capped it */
    std::string sequence, expected;
    for (int i = 0; i < kSyllables; ++i) {
        sequence += "dks";
        expected += "안";
    }

    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    int deflt = 0;
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l, "word_cap", &deflt));
    EXPECT_EQ((size_t)deflt, kCap);

    std::string delivered;
    size_t mid_word_commits = 0, largest_preedit = 0, largest_commit = 0;
    bool whole_syllables = true;
    for (size_t i = 0; i < sequence.size(); ++i) {
        hunmin_key_result out{};
        ASSERT_EQ(hunmin_buffered_preedit_process_key(l, (uint32_t)sequence[i], 0, &out), 1) << "key " << i;
        const size_t pre = strlen(out.preedit ? out.preedit : "");
        if (pre > largest_preedit) largest_preedit = pre;
        if (out.commit && out.commit[0]) {
            const size_t c = strlen(out.commit);
            ++mid_word_commits;
            if (c > largest_commit) largest_commit = c;
            delivered += out.commit;
            if (c % 3 != 0) whole_syllables = false;
        }
    }
    const char *tail = hunmin_buffered_preedit_commit_word(l);
    if (tail) delivered += tail;
    hunmin_buffered_preedit_free(l);

    EXPECT_LE(largest_preedit, kCap) << "the preedit a frontend must render outgrew the cap";
    EXPECT_EQ(delivered, expected) << "the flush lost, duplicated or reordered text";
    EXPECT_GE(mid_word_commits, 2u) << "600 B cannot fit under a 256 B cap without splitting";
    EXPECT_LE(largest_commit, kCap) << "a half was larger than the cap";
    EXPECT_TRUE(whole_syllables) << "a cut split a UTF-8 sequence";
}

/* The cap is the layer's option, not a constant: `word_cap` in bytes, 0 = no cap.
 * A frontend that ships a settings row for it needs both directions to work, and a
 * value of 0 has to restore the pre-ruling behaviour exactly (no mid-word commit
 * however long the word), because that is what a frontend that never sets the
 * option gets on every build that predates the ruling. */
TEST_F(LayerTest, TheCapIsConfigurableAndZeroMeansNoCap) {
    std::string sequence;
    for (int i = 0; i < 200; ++i) sequence += "dks"; /* 600 B */

    auto run = [&](int cap_value, size_t *flushes, size_t *largest) {
        hunmin_buffered_preedit *l =
            hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
        EXPECT_NE(l, nullptr);
        if (!l) return std::string();
        EXPECT_TRUE(hunmin_buffered_preedit_set_option(l, "word_cap", cap_value));
        std::string delivered;
        *flushes = 0;
        *largest = 0;
        for (char c : sequence) {
            hunmin_key_result out{};
            hunmin_buffered_preedit_process_key(l, (uint32_t)c, 0, &out);
            const size_t pre = strlen(out.preedit ? out.preedit : "");
            if (pre > *largest) *largest = pre;
            if (out.commit && out.commit[0]) {
                ++*flushes;
                delivered += out.commit;
            }
        }
        delivered += hunmin_buffered_preedit_commit_word(l);
        hunmin_buffered_preedit_free(l);
        return delivered;
    };

    size_t flushes_small = 0, largest_small = 0, flushes_none = 0, largest_none = 0;
    const std::string text_small = run(64, &flushes_small, &largest_small);
    const std::string text_none = run(0, &flushes_none, &largest_none);
    const std::string oracle = drive_engine(ctx_, sequence);

    EXPECT_EQ(text_small, oracle) << "a 64 B cap lost text";
    EXPECT_EQ(text_none, oracle) << "no-cap mode lost text";
    EXPECT_GT(flushes_small, 0u) << "a 64 B cap never flushed a 600 B word";
    EXPECT_LE(largest_small, 64u) << "the 64 B cap did not bound the preedit";
    EXPECT_EQ(flushes_none, 0u) << "0 must mean no cap, and nothing else";
    EXPECT_EQ(largest_none, 600u) << "0 must let the word grow as it did before the ruling";
    /* A rejected value changes nothing: the option is bytes, not a sign. */
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    EXPECT_FALSE(hunmin_buffered_preedit_set_option(l, "word_cap", -1));
    int still = 0;
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l, "word_cap", &still));
    EXPECT_EQ(still, 256) << "a rejected value must not change the cap";
    EXPECT_FALSE(hunmin_buffered_preedit_get_option(l, "no-such-option", &still));
    hunmin_buffered_preedit_free(l);
}

/* The cap's split, evaluated piece by piece rather than in aggregate: each
 * mid-word commit must be the exact PREFIX of what the same keys produce, the
 * preedit left behind must be the exact continuation, the explicit flush
 * (`hunmin_buffered_preedit_commit_word`, which is what a frontend calls on focus
 * loss) must deliver exactly the remainder and leave the layer empty, and the layer
 * must still be usable afterwards.
 *
 * The expectation is the engine's own text (`drive_engine`, the oracle the other
 * layer tests use), never a typed-in literal: a split that reorders, repeats or
 * drops a syllable shows up as a mismatch at the character where it happened, which
 * an aggregate "totals are equal" check can hide. */
TEST_F(LayerTest, EachFlushedHalfIsTheNextPrefixAndTheRemainderFlushesCleanly) {
    /* A NON-UNIFORM word on purpose. 200 × `안` would be a string of one repeated
     * three-byte syllable, in which cutting one syllable off the head of the tail and
     * leaving a stale one behind is undetectable — measured: this test passed with
     * `word_flush_half` mutated to skip a syllable, until the sequence was varied. The
     * syllables below differ in shape and in key count (2 and 3 keys), so a shift, a
     * repeat or a loss changes the bytes. */
    const char *groups[] = {"dks", "rk", "dl", "tj", "gks"}; /* 안 가 이 서 핫 */
    std::string sequence;
    for (int i = 0; sequence.size() < 1200; ++i) sequence += groups[i % 5];
    const std::string expected = drive_engine(ctx_, sequence);
    ASSERT_EQ(expected.size() % 3, 0u);
    ASSERT_GE(expected.size(), 600u) << "the oracle is shorter than the typed bytes";

    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    size_t cursor = 0; /* how much of `expected` has been accounted for */
    size_t flushes = 0;
    for (size_t i = 0; i < sequence.size(); ++i) {
        hunmin_key_result out{};
        ASSERT_EQ(hunmin_buffered_preedit_process_key(l, (uint32_t)sequence[i], 0, &out), 1) << "key " << i;
        if (out.commit && out.commit[0]) {
            const std::string half = out.commit;
            ++flushes;
            EXPECT_EQ(half, expected.substr(cursor, half.size()))
                << "the flushed half is not the next prefix (flush " << flushes << ")";
            cursor += half.size();
        }
        /* The preedit is the FINISHED part still held plus the syllable composing, so
         * only the finished part is comparable with `expected` -- a half-composed
         * syllable is genuinely not a prefix of the finished one (`아` is not a prefix
         * of `안`), which is the layer's display contract and not a defect. Everything
         * before it must be one contiguous piece of `expected`. */
        const char *composing_v = hunmin_buffered_preedit_composing(l);
        const std::string composing = composing_v ? composing_v : "";
        const std::string shown = out.preedit ? out.preedit : "";
        ASSERT_GE(shown.size(), composing.size());
        const std::string held = shown.substr(0, shown.size() - composing.size());
        EXPECT_EQ(held, expected.substr(cursor, held.size()))
            << "the held word is not the remainder after " << cursor << " bytes (key " << i << ")";
    }
    ASSERT_GE(flushes, 2u) << "600 B could not fit under the 256 B cap without splitting";

    /* The explicit flush a frontend calls: exactly the remainder, once. */
    const std::string rest = hunmin_buffered_preedit_commit_word(l);
    EXPECT_EQ(rest, expected.substr(cursor)) << "the explicit flush did not deliver the remainder";
    cursor += rest.size();
    EXPECT_EQ(cursor, expected.size()) << "the pieces do not add up to what the keys produce";
    EXPECT_STREQ(hunmin_buffered_preedit_commit_word(l), "") << "the word was delivered twice";
    EXPECT_STREQ(hunmin_buffered_preedit_display(l), "") << "the layer still holds text after the flush";

    /* And it is still a usable layer, not a spent one: one syllable composes. */
    hunmin_key_result out{};
    EXPECT_EQ(hunmin_buffered_preedit_process_key(l, (uint32_t)'d', 0, &out), 1);
    EXPECT_TRUE(out.preedit && out.preedit[0]) << "the layer stopped composing after a capped word";
    hunmin_buffered_preedit_free(l);
}

/* ------------------------------------------------------- digit runs (R6)
 *
 * docs/SPEC-MODES.md §R6: in the ON state the composition buffer holds digit
 * runs too, because in sebeolsik the digit keys are not digits. Two shapes are
 * pinned here, measured 2026-11-01 (issue #171) before either case was written:
 *
 *   sin3-p2        a digit key resolves to a literal, and the ENGINE commits it
 *                  per key (`1` → commit "1", preedit ""). Buffered mode still
 *                  holds it: preedit `1`, `12`, `123`; the boundary delivers
 *                  the run whole. Without the layer's word buffer the digit run
 *                  would be in the client before the second digit was typed,
 *                  which is the break R6 names.
 *   sebeolsik-final the digit keys are jamo (`1`=ᇂ, `2`=ᆻ, …), so the run
 *                  composes as Hangul and the engine commits a syllable per key
 *                  exactly as it does for letters.
 *   symbol tier     sin3-p2 reaches `T == 101/102/103` through a key whose
 *                  expression enters the tier (measured: `j` = ᄋ then `k` =
 *                  tier 101, `l` = 102, `;` = 103), and the NEXT key resolves at
 *                  that tier (`1` → µ / ① / ⑪). The tier is single-shot — the
 *                  second digit of the run is a plain digit again — and a
 *                  tier-resolved character is committed like any other literal,
 *                  so the layer holds it for the same reason.
 *
 * What these do NOT pin: no digit escapes early because of being a digit. The
 * one early delivery there is, R4-정정's word cap, is counted in BYTES and is
 * therefore reached by a digit run at 256 keys rather than 85 syllables — the
 * last case pins that it is the same rule, not a second one.
 */
TEST_F(LayerTest, DigitRunIsHeldInTheBufferNotCommittedPerDigit) {
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "sin3-p2", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    const char *run = "123";
    std::string shown;
    for (const char *k = run; *k; ++k) {
        hunmin_key_result out{};
        EXPECT_EQ(hunmin_buffered_preedit_process_key(l, (uint32_t)*k, 0, &out), 1)
            << "a digit of a sebeolsik run was not composition input: " << *k;
        EXPECT_STREQ(out.commit, "") << "the digit escaped the buffer at key " << *k;
        shown = out.preedit ? out.preedit : "";
        EXPECT_EQ(shown, std::string(run, size_t(k - run) + 1)) << "the buffer does not hold the run so far";
    }
    /* The boundary is the declined key a frontend passes on: the whole run goes
     * out once, and nothing is left composing. */
    hunmin_key_result out{};
    EXPECT_EQ(hunmin_buffered_preedit_process_key(l, (uint32_t)' ', 0, &out), 0);
    EXPECT_STREQ(out.commit, run) << "the boundary did not deliver the digit run whole";
    EXPECT_STREQ(out.preedit, "");
    hunmin_buffered_preedit_free(l);
}

TEST_F(LayerTest, DigitResolvedAtASymbolTierIsHeldToo) {
    /* `j` `k` enters tier 101 in sin3-p2; `1` at that tier is µ, which is a
     * literal the engine commits — the same class as a plain digit. */
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "sin3-p2", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    for (const char *k = "jk"; *k; ++k) {
        hunmin_key_result out{};
        EXPECT_EQ(hunmin_buffered_preedit_process_key(l, (uint32_t)*k, 0, &out), 1) << "key " << *k;
        EXPECT_STREQ(out.commit, "") << "key " << *k << " delivered early";
    }
    hunmin_key_result out{};
    EXPECT_EQ(hunmin_buffered_preedit_process_key(l, (uint32_t)'1', 0, &out), 1);
    EXPECT_STREQ(out.commit, "") << "the tier-resolved digit escaped the buffer";
    EXPECT_EQ(std::string(out.preedit ? out.preedit : ""), "\u3147\u00b5") << "tier 101 for `1` is µ";
    out = hunmin_key_result{};
    EXPECT_EQ(hunmin_buffered_preedit_process_key(l, (uint32_t)' ', 0, &out), 0);
    EXPECT_EQ(std::string(out.commit ? out.commit : ""), "\u3147\u00b5");
    hunmin_buffered_preedit_free(l);
}

TEST_F(LayerTest, DigitRunsAgreeWithTheEngineInBothCommitModes) {
    for (const char *layout : {"sin3-p2", "sebeolsik-final"}) {
        for (const char *seq : {"1234567890", "dks123", "dks12", "jk1", "1"}) {
            const std::string oracle = drive_engine(ctx_, seq, layout);
            const RunResult buffered = drive_layer(ctx_, seq, HUNMIN_COMMIT_AT_BOUNDARY, layout);
            const RunResult syllable = drive_layer(ctx_, seq, HUNMIN_COMMIT_PER_SYLLABLE, layout);
            EXPECT_EQ(buffered.total, oracle) << layout << " " << seq << " (buffered)";
            EXPECT_EQ(syllable.total, oracle) << layout << " " << seq << " (syllable)";
        }
    }
}

TEST_F(LayerTest, TheCapCountsDigitRunsInBytesLikeSyllables) {
    /* The cap is R4-정정's, in bytes: 4 B is four digits but one syllable, so a
     * run of five digits flushes its first half at the fifth key. Nothing is
     * delivered before the cap, which is the part R6 cares about. */
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx_, "sin3-p2", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    ASSERT_TRUE(hunmin_buffered_preedit_set_option(l, "word_cap", 4));
    std::string delivered;
    std::string preedit;
    for (const char *k = "12345"; *k; ++k) {
        hunmin_key_result out{};
        ASSERT_EQ(hunmin_buffered_preedit_process_key(l, (uint32_t)*k, 0, &out), 1) << "key " << *k;
        delivered += out.commit ? out.commit : "";
        preedit = out.preedit ? out.preedit : "";
        const size_t held = delivered.size() + preedit.size();
        EXPECT_FALSE(*k < '5' && !delivered.empty()) << "a digit under the cap was delivered early";
        EXPECT_LE(held, 5u) << "the preedit outgrew the cap plus one key";
    }
    EXPECT_EQ(delivered, "12") << "the flush is the first half of a 5-byte word";
    EXPECT_EQ(preedit, "345") << "the tail keeps composing";
    EXPECT_EQ(delivered + hunmin_buffered_preedit_commit_word(l), std::string("12345"))
        << "the digits were lost or reordered across the flush";
    hunmin_buffered_preedit_free(l);
}

/* R15 (docs/SPEC-MODES.md §R15): the remedy for a word the switch took is the
 * switch itself, so turning it off may not cost anything -- not the word, and
 * not a byte. `hello` is a word the judgement reads as English, which is the
 * loss the requirement is about; the run that stays on is the control, and
 * without it this test would pass on any build where nothing ever switches. */
TEST_F(LayerTest, TurningTheSwitchOffMidWordLosesNothing) {
    const std::string seq = "hello ";
    const std::string oracle = drive_engine(ctx_, seq);

    /* flip_after = index of the key at which the person turns the switch off;
     * -1 keeps it on for the whole word (the control). */
    auto run = [&](int flip_after) {
        std::string total;
        hunmin_buffered_preedit *l =
            hunmin_buffered_preedit_new(ctx_, "dubeolsik", "us-qwerty", HUNMIN_COMMIT_AT_BOUNDARY);
        EXPECT_NE(l, nullptr);
        if (!l) return total;
        EXPECT_EQ(1, hunmin_buffered_preedit_load_english_shipped(l));
        EXPECT_TRUE(hunmin_buffered_preedit_set_option(l, "autoswitch", 1));
        int i = 0;
        for (char c : seq) {
            if (i == flip_after) {
                EXPECT_TRUE(hunmin_buffered_preedit_set_option(l, "autoswitch", 0));
                int held = -1;
                EXPECT_TRUE(hunmin_buffered_preedit_get_option(l, "autoswitch", &held));
                EXPECT_EQ(0, held) << "the switch refused to turn off mid-word";
            }
            hunmin_key_result out{};
            /* The space is declined and belongs to the client, so the consumed
             * answer is not asserted here -- the totals are what this case is
             * about, and a letter the layer silently declined would move them. */
            hunmin_buffered_preedit_process_key(l, (uint32_t)c, 0, &out);
            if (out.commit) total += out.commit;
            i++;
        }
        const char *tail = hunmin_buffered_preedit_commit_word(l);
        if (tail) total += tail;
        hunmin_buffered_preedit_free(l);
        return total;
    };

    const std::string stayed_on = run(-1);
    /* The control: with the switch on, this word really is taken (it leaves as
     * the Latin reading), which is why the requirement exists at all. */
    ASSERT_NE(stayed_on, oracle) << "the case never switches, so it proves nothing";
    EXPECT_EQ(stayed_on, std::string("hello"));

    /* The requirement: flipped before the boundary, the same keys deliver what
     * the engine delivers -- the fallback costs the word nothing. */
    EXPECT_EQ(run(3), oracle) << "turning the switch off mid-word lost or changed the word";

    /* And the display follows the switch back with no stale judged reading left
     * behind: after the flip it is the off path's display for the same prefix. */
    const std::string prefix = "hel";
    const RunResult off_prefix = drive_layer(ctx_, prefix, HUNMIN_COMMIT_AT_BOUNDARY);
    hunmin_buffered_preedit *l =
        hunmin_buffered_preedit_new(ctx_, "dubeolsik", "us-qwerty", HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    ASSERT_EQ(1, hunmin_buffered_preedit_load_english_shipped(l));
    ASSERT_TRUE(hunmin_buffered_preedit_set_option(l, "autoswitch", 1));
    for (char c : prefix) {
        hunmin_key_result out{};
        ASSERT_EQ(1, hunmin_buffered_preedit_process_key(l, (uint32_t)c, 0, &out));
    }
    ASSERT_TRUE(hunmin_buffered_preedit_set_option(l, "autoswitch", 0));
    ASSERT_FALSE(off_prefix.shown.empty());
    EXPECT_EQ(std::string(hunmin_buffered_preedit_display(l)), off_prefix.shown.back())
        << "the display kept a judged reading after the switch went off";
    hunmin_buffered_preedit_free(l);
}

} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    const std::string path = env_or("HUNMIN_DATA_PATH", "libhunmin/data");
    kDataPath = path.c_str();
    return RUN_ALL_TESTS();
}
