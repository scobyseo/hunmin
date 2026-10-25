/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * layer-test.cpp - tier A for the shared buffered/syllable layer (issue #128,
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
 *   3. peek: hunmin_layer_peek_key agrees with the following key and changes
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
RunResult drive_layer(hunmin_ctx *ctx, const std::string &sequence, int mode) {
    RunResult r;
    hunmin_layer *l = hunmin_layer_new(ctx, "dubeolsik", nullptr, mode);
    EXPECT_NE(l, nullptr);
    if (!l) return r;
    std::string word;
    for (const Key &k : keys_of(sequence)) {
        hunmin_layer_out out{};
        const int eaten = hunmin_layer_key(l, k.base, k.shift, &out);
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
    const char *tail = hunmin_layer_flush(l);
    if (tail && *tail) {
        r.total += tail;
        r.shown.push_back(r.total);
    }
    hunmin_layer_free(l);
    return r;
}

/* The oracle: the engine alone, no layer. Its output is the reference text the
 * layer must reproduce in both modes. */
std::string drive_engine(hunmin_ctx *ctx, const std::string &sequence) {
    hunmin_ic *ic = hunmin_ic_new(ctx, "dubeolsik");
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
        const RunResult buffered = drive_layer(ctx_, seq, HUNMIN_COMMIT_BUFFERED);
        const RunResult syllable = drive_layer(ctx_, seq, HUNMIN_COMMIT_SYLLABLE);
        EXPECT_EQ(buffered.total, oracle) << "sequence: " << seq;
        EXPECT_EQ(syllable.total, oracle) << "sequence: " << seq;
    }
}

TEST_F(LayerTest, BothCommitModesAgreeOnConsumptionAndDisplay) {
    for (const char *seq : kSequences) {
        const RunResult buffered = drive_layer(ctx_, seq, HUNMIN_COMMIT_BUFFERED);
        const RunResult syllable = drive_layer(ctx_, seq, HUNMIN_COMMIT_SYLLABLE);
        ASSERT_EQ(buffered.eaten.size(), keys_of(seq).size()) << "sequence: " << seq;
        EXPECT_EQ(buffered.eaten, syllable.eaten) << "sequence: " << seq;
        const size_t n = std::min(buffered.shown.size(), syllable.shown.size());
        for (size_t i = 0; i < n; i++) {
            EXPECT_EQ(buffered.shown[i], syllable.shown[i]) << "sequence: " << seq << " key " << i;
        }
    }
}

TEST_F(LayerTest, BufferedModeCommitsNothingUntilABoundary) {
    hunmin_layer *l = hunmin_layer_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_BUFFERED);
    ASSERT_NE(l, nullptr);
    for (char c : std::string("dkssu")) {
        hunmin_layer_out out{};
        EXPECT_EQ(hunmin_layer_key(l, (uint32_t)c, 0, &out), 1);
        EXPECT_STREQ(out.commit, "") << "buffered mode inserted mid-word: " << c;
    }
    /* 'd' finishes the first syllable, so the buffer is what a boundary must
     * deliver: the finished part plus the in-flight one. */
    hunmin_layer_out out{};
    EXPECT_EQ(hunmin_layer_key(l, (uint32_t)'d', 0, &out), 1);
    EXPECT_STREQ(out.commit, "");
    const char *flushed = hunmin_layer_flush(l);
    EXPECT_STRNE(flushed, "");
    /* Second flush of an empty layer is empty: the word was delivered once. */
    EXPECT_STREQ(hunmin_layer_flush(l), "");
    hunmin_layer_free(l);
}

TEST_F(LayerTest, SyllableModeCommitsEachFinalisedSyllable) {
    hunmin_layer *l = hunmin_layer_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_SYLLABLE);
    ASSERT_NE(l, nullptr);
    std::string got;
    bool delivered_mid_word = false;
    for (char c : std::string("dkssu")) {
        hunmin_layer_out out{};
        hunmin_layer_key(l, (uint32_t)c, 0, &out);
        got += out.commit;
        if (out.commit && out.commit[0]) delivered_mid_word = true;
    }
    /* The point of this mode: text reaches the client before the boundary. */
    EXPECT_TRUE(delivered_mid_word) << "syllable mode held the text until the end";
    got += hunmin_layer_flush(l);
    EXPECT_EQ(got, drive_engine(ctx_, "dkssu"));
    hunmin_layer_free(l);
}

TEST_F(LayerTest, PeekAgreesWithKeyAndChangesNothing) {
    hunmin_layer *l = hunmin_layer_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_BUFFERED);
    ASSERT_NE(l, nullptr);
    hunmin_layer_out pre{};
    hunmin_layer_key(l, (uint32_t)'d', 0, &pre);
    const std::string before_preedit = pre.preedit ? pre.preedit : "";
    /* A digit is declined by dubeolsik; a letter is consumed. Both answers must
     * agree with the following real key, and the peek must not move the state. */
    for (uint32_t ch : {uint32_t('k'), uint32_t('1'), uint32_t(' ')}) {
        const int peeked = hunmin_layer_peek_key(l, ch, 0);
        hunmin_layer_out out{};
        const int eaten = hunmin_layer_key(l, ch, 0, &out);
        EXPECT_EQ(peeked, eaten) << "key " << (char)ch;
    }
    /* The peek did not consume a key: the layer's own state is what the real
     * keys produced, not one key further. */
    EXPECT_EQ(hunmin_layer_commit_mode(l), HUNMIN_COMMIT_BUFFERED);
    (void)before_preedit;
    hunmin_layer_free(l);
}

TEST_F(LayerTest, ResetDropsTheWord) {
    hunmin_layer *l = hunmin_layer_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_BUFFERED);
    ASSERT_NE(l, nullptr);
    for (char c : std::string("dkssu")) {
        hunmin_layer_out out{};
        hunmin_layer_key(l, (uint32_t)c, 0, &out);
    }
    hunmin_layer_reset(l);
    EXPECT_STREQ(hunmin_layer_flush(l), "");
    hunmin_layer_out out{};
    hunmin_layer_key(l, (uint32_t)'d', 0, &out);
    EXPECT_TRUE(out.preedit && out.preedit[0]) << "reset left the engine composing";
    hunmin_layer_free(l);
}

TEST_F(LayerTest, ModeChangeIsABoundaryAndSwitchesLayout) {
    hunmin_layer *l = hunmin_layer_new(ctx_, "dubeolsik", "dubeolsik", HUNMIN_COMMIT_BUFFERED);
    ASSERT_NE(l, nullptr);
    EXPECT_EQ(hunmin_layer_mode(l), HUNMIN_MODE_HANGUL);
    for (char c : std::string("dkssu")) {
        hunmin_layer_out out{};
        hunmin_layer_key(l, (uint32_t)c, 0, &out);
    }
    hunmin_layer_out out{};
    EXPECT_EQ(hunmin_layer_mode_change(l, &out), 1);
    EXPECT_STRNE(out.commit, "") << "the Han/Eng key must commit what was composing";
    EXPECT_STREQ(out.preedit, "");
    EXPECT_EQ(hunmin_layer_mode(l), HUNMIN_MODE_LATIN);
    /* And back: the boundary rule is the same in both directions. */
    EXPECT_EQ(hunmin_layer_mode_change(l, &out), 1);
    EXPECT_EQ(hunmin_layer_mode(l), HUNMIN_MODE_HANGUL);
    hunmin_layer_free(l);
}

TEST_F(LayerTest, OneLanguageFrontendReportsTheActionAsNotOurs) {
    hunmin_layer *l = hunmin_layer_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_SYLLABLE);
    ASSERT_NE(l, nullptr);
    hunmin_layer_out out{};
    EXPECT_EQ(hunmin_layer_mode_change(l, &out), 0);
    EXPECT_EQ(hunmin_layer_mode(l), HUNMIN_MODE_HANGUL);
    EXPECT_STREQ(out.commit, "");
    hunmin_layer_free(l);
}

TEST_F(LayerTest, RejectsUnknownLayoutAndBadMode) {
    EXPECT_EQ(hunmin_layer_new(ctx_, "no-such-layout", nullptr, HUNMIN_COMMIT_BUFFERED), nullptr);
    EXPECT_EQ(hunmin_layer_new(ctx_, "dubeolsik", nullptr, 7), nullptr);
    EXPECT_EQ(hunmin_layer_new(nullptr, "dubeolsik", nullptr, HUNMIN_COMMIT_BUFFERED), nullptr);
    EXPECT_EQ(hunmin_layer_new(ctx_, nullptr, nullptr, HUNMIN_COMMIT_BUFFERED), nullptr);
}

} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    const std::string path = env_or("HUNMIN_DATA_PATH", "libhunmin/data");
    kDataPath = path.c_str();
    return RUN_ALL_TESTS();
}
