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
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

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

/* ---------------------------------------------------- the loading path (#167)
 *
 * The shipped list is FOUND, not named: the same data-dir search the
 * hanja/krdict/idioms tables run. These cases drive that search through a
 * context, because the search is the only part that can be wrong -- a frontend
 * that handed the engine a data directory must not have to know where `modes/`
 * lives, and a machine without the dictionary must read "absent", never an
 * empty set that looks like "no english words exist".
 */

namespace {

/* A scratch DATA DIR the test owns. hunmin_ctx_new needs layouts, so this holds
 * the compose tables and one layout and nothing else -- the shape of a machine
 * with the engine's data but not the dictionary package (B3, issue #190). */
class ScratchDataDir {
public:
    explicit ScratchDataDir(const char *name) {
        path_ = scratch_dir() + "/" + name;
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_ + "/compose");
        std::filesystem::create_directories(path_ + "/layouts");
        const std::string from = kDataPath;
        for (const auto &e : std::filesystem::directory_iterator(from + "/compose")) {
            std::filesystem::copy_file(e.path(), path_ + "/compose/" + e.path().filename().string(),
                                       std::filesystem::copy_options::overwrite_existing);
        }
        std::filesystem::copy_file(from + "/layouts/dubeolsik.json", path_ + "/layouts/dubeolsik.json",
                                   std::filesystem::copy_options::overwrite_existing);
    }
    ~ScratchDataDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    ScratchDataDir(const ScratchDataDir &) = delete;
    ScratchDataDir &operator=(const ScratchDataDir &) = delete;

    const std::string &path() const { return path_; }

    /* Put a file under this dir, creating its parent directory. */
    void put(const std::string &rel, const std::string &body) {
        const std::string full = path_ + "/" + rel;
        std::filesystem::create_directories(full.substr(0, full.find_last_of('/')));
        std::ofstream out(full.c_str(), std::ios::binary | std::ios::trunc);
        out.write(body.data(), static_cast<std::streamsize>(body.size()));
    }

private:
    std::string path_;
};

/* One layer over a directory list; owned here so each case reads as one setup. */
struct LayerOn {
    explicit LayerOn(std::vector<const char *> dirs) {
        ctx = hunmin_ctx_new(dirs.data(), static_cast<int>(dirs.size()));
        if (ctx) layer = hunmin_buffered_preedit_new(ctx, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    }
    ~LayerOn() {
        if (layer) hunmin_buffered_preedit_free(layer);
        if (ctx) hunmin_ctx_free(ctx);
    }
    LayerOn(const LayerOn &) = delete;
    LayerOn &operator=(const LayerOn &) = delete;

    hunmin_ctx *ctx = nullptr;
    hunmin_buffered_preedit *layer = nullptr;
};

} // namespace

TEST_F(WordSetTest, ShippedListIsFoundThroughTheContextsDataDirs) {
    ASSERT_EQ(1, hunmin_buffered_preedit_load_english_shipped(layer_));
    EXPECT_EQ(1, hunmin_buffered_preedit_english_ready(layer_));

    /* The same set the path-taking call produces: one file, two ways in. */
    const char *dirs[1] = {kDataPath};
    LayerOn by_path(std::vector<const char *>(dirs, dirs + 1));
    ASSERT_NE(by_path.ctx, nullptr);
    ASSERT_NE(by_path.layer, nullptr);
    ASSERT_EQ(1, hunmin_buffered_preedit_load_english(by_path.layer, shipped_list().c_str()));

    const char *words[] = {"computer", "don't", "the", "kbs", "xyzzyy"};
    for (const char *w : words) {
        EXPECT_EQ(hunmin_buffered_preedit_english_valid(by_path.layer, w),
                  hunmin_buffered_preedit_english_valid(layer_, w))
            << w;
    }
}

TEST(EnglishLoadingPath, ADataDirWithoutTheDictionaryIsAbsentNotEmpty) {
    ScratchDataDir data("hunmin-nodict-data");
    LayerOn on(std::vector<const char *>(1, data.path().c_str()));
    ASSERT_NE(on.ctx, nullptr); /* its layouts loaded; the dictionary package did not */
    ASSERT_NE(on.layer, nullptr);

    EXPECT_EQ(-1, hunmin_buffered_preedit_load_english_shipped(on.layer));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_ready(on.layer));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(on.layer, "computer"));
}

TEST(EnglishLoadingPath, TheSearchWalksTheDirListInOrder) {
    ScratchDataDir data("hunmin-nodict-first");
    const char *dirs[2] = {data.path().c_str(), kDataPath};
    LayerOn on(std::vector<const char *>(dirs, dirs + 2));
    ASSERT_NE(on.ctx, nullptr);
    ASSERT_NE(on.layer, nullptr);

    EXPECT_EQ(1, hunmin_buffered_preedit_load_english_shipped(on.layer));
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(on.layer, "computer"));
}

TEST(EnglishLoadingPath, AMalformedShippedListIsAnErrorNotAnEmptyDictionary) {
    ScratchDataDir data("hunmin-broken-dict");
    data.put("modes/en-US.json", "[\"computer\", \"run");
    LayerOn on(std::vector<const char *>(1, data.path().c_str()));
    ASSERT_NE(on.ctx, nullptr);
    ASSERT_NE(on.layer, nullptr);

    /* Found and unreadable: an error, and the lookups still fail closed. */
    EXPECT_EQ(0, hunmin_buffered_preedit_load_english_shipped(on.layer));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_ready(on.layer));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(on.layer, "computer"));
}

TEST_F(WordSetTest, CaseAndHyphenPolicyIsTheSchemasNotTheLoadersMood) {
    ASSERT_EQ(1, hunmin_buffered_preedit_load_english_shipped(layer_));

    /* docs/SPEC-SCHEMA.md: the artefact is lowercase and the lookup folds ASCII
     * A-Z before it compares, so a capitalised buffer matches. */
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, "DON'T"));
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, "Don't"));
    /* An apostrophe or hyphen is part of the word. Nothing strips one, so this
     * is not the same word as `dont`. */
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(layer_, "dont"));
    /* No word in the shipped list begins or ends with one, and the lookup does
     * not trim, so a buffer that does is not a word. */
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(layer_, "-word"));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(layer_, "word-"));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(layer_, "ice cream"));
}

/* ------------------------------------------------ the Latin reading (#167)
 *
 * R8's english_valid reads "the word the buffer would have produced under the
 * Latin layout". A Hangul layout cannot produce Latin, so the reading is the
 * same keys replayed through the Latin half of the frontend's layout pair --
 * SPEC-MODES R2's shadow mechanism, which lived in the fcitx5 adapter alone
 * until this moved into the shared layer (issue #164 asks who owns the
 * JUDGEMENT; the reading is what it reads).
 *
 * Three properties are the contract, and each has a case: the reading is the
 * KEYS on the Latin layout (not a transliteration of the Hangul), it exists
 * only for a word that FINISHED (a composing word and a boundary that ended no
 * word both answer empty, so nothing stale is judged twice), and a layer with
 * one language answers empty rather than pretending (fail closed, B3).
 */

namespace {

/* Feed a key sequence at shift=0. */
void type_keys(hunmin_buffered_preedit *l, const char *keys) {
    for (const char *p = keys; *p; p++)
        hunmin_buffered_preedit_process_key(l, static_cast<uint32_t>(*p), 0, nullptr);
}

} // namespace

TEST_F(WordSetTest, TheLatinReadingIsTheSameKeysOnTheLatinLayout) {
    hunmin_buffered_preedit *pair =
        hunmin_buffered_preedit_new(ctx_, "dubeolsik", "us-qwerty", HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(pair, nullptr);
    ASSERT_EQ(1, hunmin_buffered_preedit_load_english_shipped(pair));

    /* Nothing has finished, so there is nothing to look up. */
    EXPECT_STREQ("", hunmin_buffered_preedit_latin_text(pair));

    type_keys(pair, "hello");
    /* A word in flight has no reading: R8 judges at the boundary, once. */
    EXPECT_STREQ("", hunmin_buffered_preedit_latin_text(pair));

    hunmin_buffered_preedit_process_key(pair, ' ', 0, nullptr); /* the boundary */
    EXPECT_STREQ("hello", hunmin_buffered_preedit_latin_text(pair));

    /* The composition the judgement makes, with no work of its own. */
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(pair, hunmin_buffered_preedit_latin_text(pair)));

    /* And the same keys that are NOT an English word read as what they are,
     * which the dictionary rejects -- the measured gap the ruling accepted. */
    type_keys(pair, "kbs");
    hunmin_buffered_preedit_process_key(pair, ' ', 0, nullptr);
    EXPECT_STREQ("kbs", hunmin_buffered_preedit_latin_text(pair));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(pair, hunmin_buffered_preedit_latin_text(pair)));

    hunmin_buffered_preedit_free(pair);
}

TEST_F(WordSetTest, ShiftSurvivesTheReplay) {
    hunmin_buffered_preedit *pair =
        hunmin_buffered_preedit_new(ctx_, "dubeolsik", "us-qwerty", HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(pair, nullptr);
    ASSERT_EQ(1, hunmin_buffered_preedit_load_english_shipped(pair));

    hunmin_buffered_preedit_process_key(pair, 'h', 1, nullptr); /* Shift+h */
    type_keys(pair, "ello");
    hunmin_buffered_preedit_process_key(pair, ' ', 0, nullptr);

    /* A capitalised buffer is a capitalised reading, and the lookup is what
     * folds it (docs/SPEC-SCHEMA.md). */
    EXPECT_STREQ("Hello", hunmin_buffered_preedit_latin_text(pair));
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(pair, hunmin_buffered_preedit_latin_text(pair)));

    hunmin_buffered_preedit_free(pair);
}

TEST_F(WordSetTest, ALayerWithOneLanguageHasNoReading) {
    ASSERT_EQ(1, hunmin_buffered_preedit_load_english_shipped(layer_));

    type_keys(layer_, "hello");
    hunmin_buffered_preedit_process_key(layer_, ' ', 0, nullptr);

    /* latin_layout_id is NULL here: a frontend that cannot show a Latin mode
     * answers "not English", not a translated guess. */
    EXPECT_STREQ("", hunmin_buffered_preedit_latin_text(layer_));
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(layer_, hunmin_buffered_preedit_latin_text(layer_)));
}

TEST_F(WordSetTest, TheReadingBelongsToTheFinishedWordOnly) {
    /* Both delivery modes: PER_SYLLABLE keeps no word buffer, so a reading
     * hooked below the delivery split would silently be missing there. */
    const int policies[] = {HUNMIN_COMMIT_AT_BOUNDARY, HUNMIN_COMMIT_PER_SYLLABLE};
    for (int policy : policies) {
        hunmin_buffered_preedit *pair =
            hunmin_buffered_preedit_new(ctx_, "dubeolsik", "us-qwerty", policy);
        ASSERT_NE(pair, nullptr) << policy;

        type_keys(pair, "hello");
        hunmin_buffered_preedit_process_key(pair, ' ', 0, nullptr);
        EXPECT_STREQ("hello", hunmin_buffered_preedit_latin_text(pair)) << policy;

        /* A boundary that ends no word clears it: the next space must not
         * re-judge "hello". */
        hunmin_buffered_preedit_process_key(pair, ' ', 0, nullptr);
        EXPECT_STREQ("", hunmin_buffered_preedit_latin_text(pair)) << policy;

        /* A new word in flight is not a reading yet. */
        type_keys(pair, "wor");
        EXPECT_STREQ("", hunmin_buffered_preedit_latin_text(pair)) << policy;
        hunmin_buffered_preedit_process_key(pair, ' ', 0, nullptr);
        EXPECT_STREQ("wor", hunmin_buffered_preedit_latin_text(pair)) << policy;

        /* reset() forgets it with everything else. */
        hunmin_buffered_preedit_reset(pair);
        EXPECT_STREQ("", hunmin_buffered_preedit_latin_text(pair)) << policy;

        hunmin_buffered_preedit_free(pair);
    }
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
