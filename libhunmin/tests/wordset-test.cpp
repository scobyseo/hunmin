/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * wordset-test.cpp - tier A for the english word set (SPEC-MODES R8-정정,
 * ruling 2026-11-01: hunspell en_US base + a user dictionary).
 *
 * The three states are the point, because B3 ships this data separately:
 *
 *   absent   the file is not there -> not ready, and every lookup answers 0.
 *            NOT a failure: the feature is off until its data is installed.
 *   loaded   the shipped list answers for a word an English dictionary has
 *            (`running`, `computer`) and still answers 0 for the Korean-typed
 *            latin strings MODES-DATA 4-7d names (`kbs`, `fifa`) -- that gap is
 *            the measured cost of the ruling, asserted here so it cannot be
 *            quietly filled in and forgotten.
 *   user     a user word (`kbs`) is found exactly where a base word is, from a
 *            file AND from the add-word call; the engine merges and never
 *            writes back, so persistence stays the frontend's.
 *
 * The malformed-file case asserts load 0 and a lookup of 0 rather than a crash:
 * a broken list must read as "no data", never as "no english words".
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

extern "C" {
#include "hunmin.h"
}

namespace {

const char *kDataPath = nullptr;

std::string scratch_dir() {
    const char *tmp = std::getenv("TEMP");
    if (!tmp) tmp = std::getenv("TMPDIR");
    return tmp ? std::string(tmp) : std::string(".");
}

/* A scratch file the test owns and removes: a user dictionary, or a malformed
 * one. Kept out of the source tree so a failing run leaves nothing behind. */
class ScratchFile {
public:
    ScratchFile(const char *name, const std::string &body) {
        path_ = scratch_dir() + "/" + name;
        std::ofstream out(path_.c_str(), std::ios::binary | std::ios::trunc);
        out.write(body.data(), static_cast<std::streamsize>(body.size()));
        out.close();
    }
    ~ScratchFile() { std::remove(path_.c_str()); }
    const char *c_str() const { return path_.c_str(); }

private:
    std::string path_;
};

class WordSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char *dirs[1] = {kDataPath};
        ctx_ = hunmin_ctx_new(dirs, 1);
        ASSERT_NE(ctx_, nullptr);
        layer_ = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr,
                                             HUNMIN_COMMIT_AT_BOUNDARY);
        ASSERT_NE(layer_, nullptr);
    }
    void TearDown() override {
        if (layer_) hunmin_buffered_preedit_free(layer_);
        if (ctx_) hunmin_ctx_free(ctx_);
    }

    std::string shipped_list() const { return std::string(kDataPath) + "/modes/en-US.json"; }

    hunmin_ctx *ctx_ = nullptr;
    hunmin_buffered_preedit *layer_ = nullptr;
};

} // namespace

TEST_F(WordSetTest, AbsentDictionaryIsNotReadyAndFindsNothing) {
    EXPECT_EQ(0, hunmin_buffered_preedit_english_ready(layer_));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(layer_, "computer"));

    /* -1 is "absent", the state B3 makes normal; it is not an error. */
    EXPECT_EQ(-1, hunmin_buffered_preedit_load_english(layer_, "/nonexistent/en-US.json"));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_ready(layer_));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(layer_, "computer"));
}

TEST_F(WordSetTest, MalformedFileLoadsZeroAndFindsNothing) {
    ScratchFile broken("hunmin-english-broken.json", "[\"computer\", \"run");
    EXPECT_EQ(0, hunmin_buffered_preedit_load_english(layer_, broken.c_str()));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_ready(layer_));
}

TEST_F(WordSetTest, ShippedListAnswersForEnglishAndNotForTheKoreanTypedGap) {
    ASSERT_EQ(1, hunmin_buffered_preedit_load_english(layer_, shipped_list().c_str()));
    EXPECT_EQ(1, hunmin_buffered_preedit_english_ready(layer_));

    const char *english[] = {"computer", "running", "dogs", "the", "her"};
    for (const char *w : english)
        EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, w)) << w;
    /* Case folds: the artefact is lowercased, the buffer is not. */
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, "Computer"));

    /* The measured gap the ruling accepted (MODES-DATA 4-7d): these are in no
     * English dictionary, and the user dictionary is the ruled answer. */
    const char *gap[] = {"kbs", "mbc", "fifa", "uefa", "nhk", "xyzzyy"};
    for (const char *w : gap)
        EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(layer_, w)) << w;
}

TEST_F(WordSetTest, UserFileIsFoundExactlyWhereABaseWordIs) {
    ASSERT_EQ(1, hunmin_buffered_preedit_load_english(layer_, shipped_list().c_str()));

    /* A person's file: a BOM, a comment, a blank line, CRLF, stray spaces. */
    ScratchFile user("hunmin-user-words.txt",
                     "\xEF\xBB\xBF# my words\r\n"
                     "\r\n"
                     "  kbs  \r\n"
                     "fifa # a broadcaster\r\n");
    ASSERT_EQ(1, hunmin_buffered_preedit_load_user_words(layer_, user.c_str()));

    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, "kbs"));
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, "fifa"));
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, "KBS"));
    /* The base list is not replaced by the user's words. */
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, "computer"));
}

TEST_F(WordSetTest, AddWordIsTheEditingPathAndPersistsNowhere) {
    ASSERT_EQ(1, hunmin_buffered_preedit_load_english(layer_, shipped_list().c_str()));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(layer_, "kbs"));

    EXPECT_EQ(1, hunmin_buffered_preedit_add_user_word(layer_, "kbs"));
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, "kbs"));
    /* Idempotent: adding twice is not an error and adds no second entry. */
    EXPECT_EQ(1, hunmin_buffered_preedit_add_user_word(layer_, "kbs"));
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, "kbs"));

    /* A fresh layer starts empty: the engine never writes the user's words into
     * the shipped directory, so persistence belongs to the frontend. */
    const char *dirs[1] = {kDataPath};
    hunmin_ctx *ctx = hunmin_ctx_new(dirs, 1);
    ASSERT_NE(ctx, nullptr);
    hunmin_buffered_preedit *other =
        hunmin_buffered_preedit_new(ctx, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(other, nullptr);
    ASSERT_EQ(1, hunmin_buffered_preedit_load_english(other, shipped_list().c_str()));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(other, "kbs"));
    hunmin_buffered_preedit_free(other);
    hunmin_ctx_free(ctx);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    kDataPath = std::getenv("HUNMIN_DATA_PATH");
    if (!kDataPath) {
        std::fprintf(stderr, "HUNMIN_DATA_PATH is not set; this test reads the "
                             "shipped tables, not a copy\n");
        return 1;
    }
    return RUN_ALL_TESTS();
}
