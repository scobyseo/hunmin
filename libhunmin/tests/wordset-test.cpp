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

/* ---------------------------------- R8's judgement at the boundary (#164)
 *
 * The engine judges and delivers the triggering word in the judged language;
 * the frontend performs the mode switch by reading
 * hunmin_buffered_preedit_take_judgement right after the key that ended the
 * word. So a case is a key sequence and three answers: the delivered text, the
 * mode the frontend is told to adopt, and whether a second read still answers.
 *
 * Assertions are equalities against a second run of the same keys rather than
 * typed Hangul: the point of this layer is that it does not change text unless
 * the judgement says so, and a hand-written syllable in the expectation is how
 * a test comes to assert the author's guess (dubeolsik `hello` is not `ㅗ디ㅣㅣㅐ`
 * by any rule this file could re-derive -- it is what the engine produces).
 *
 * The pairs are the formula's three lines, on words whose two readings this
 * tree can be asked about (docs/SPEC-MODES.md §R8):
 *
 *   hello   the Latin reading is an English word, the Hangul is not  -> switch
 *   one     same, at exactly R9's three key inputs                   -> switch
 *   on      same two-key word: R9's guard                            -> keep
 *   the     both readings valid (`the` is a word, `솓` is a syllable) -> keep
 *   kbs     neither valid                                            -> keep
 *   dkssud  the Hangul only (`안녕`)                                  -> keep
 */

namespace {

struct WordRun {
    std::string commit; /* everything the client would have received */
    int mode;           /* the last non-negative take_judgement, or -1 */
    int second;         /* a second read after the last one: must be -1 */
    bool twice;         /* a non-negative answer was seen twice */
};

/* Type `keys` the way a frontend does: deliver each out->commit, then ask for
 * the judgement of that key. Returns what the client saw and what the frontend
 * was told. */
WordRun type_and_judge(hunmin_buffered_preedit *l, const char *keys) {
    WordRun r{"", -1, -1, false};
    for (const char *p = keys; *p; ++p) {
        hunmin_key_result out{};
        hunmin_buffered_preedit_process_key(l, static_cast<uint32_t>(*p), 0, &out);
        if (out.commit && *out.commit) r.commit += out.commit;
        const int j = hunmin_buffered_preedit_take_judgement(l);
        if (j >= 0) {
            if (r.mode >= 0) r.twice = true;
            r.mode = j;
        }
    }
    r.second = hunmin_buffered_preedit_take_judgement(l);
    return r;
}

/* A two-language layer with the shipped dictionary loaded and `autoswitch` set
 * the way a settings UI would leave it. */
hunmin_buffered_preedit *pair_layer(hunmin_ctx *ctx, int autoswitch,
                                    int policy = HUNMIN_COMMIT_AT_BOUNDARY) {
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(ctx, "dubeolsik", "us-qwerty", policy);
    if (!l) return nullptr;
    hunmin_buffered_preedit_load_english_shipped(l);
    hunmin_buffered_preedit_set_option(l, "autoswitch", autoswitch);
    return l;
}

} // namespace

/* E6 (docs/SPEC-MODES.md §E6, ruled 2026-11-01: the maintainer chose this as the
 * DEFAULT — "옵션처리하자. 설정할 수 있게. default는 B로."). The option is
 * `judge_timing`: 1 = the judgement is shown at every key (the flicker the item
 * describes), 0 = only at the boundary. What must hold either way is that the
 * DELIVERED text does not move: the timing changes when a language is shown, never
 * what is committed or which mode the frontend is told to adopt. That equality is
 * the whole contract, so it is asserted as an equality between the two runs rather
 * than against typed syllables. */
namespace {

/* Does this preedit show the judged (Latin) language rather than the current one?
 * The reading is ASCII; a Hangul layout's rendering is syllables and jamo. */
bool shows_latin(const char *preedit) {
    for (const char *p = preedit ? preedit : ""; *p; ++p)
        if (*p >= 'a' && *p <= 'z') return true;
    return false;
}

/* How many times the displayed language changes while `keys` is typed, counting the
 * first key's answer as coming from the current mode (Hangul). */
int display_changes(hunmin_buffered_preedit *l, const char *keys) {
    bool was_latin = false;
    int changes = 0;
    for (const char *p = keys; *p; ++p) {
        hunmin_key_result out{};
        hunmin_buffered_preedit_process_key(l, static_cast<uint32_t>(*p), 0, &out);
        const bool latin = shows_latin(out.preedit);
        if (latin != was_latin) ++changes;
        was_latin = latin;
    }
    return changes;
}

} // namespace

/* The text a layout produces for a key sequence, with the same shift the test
 * presses -- the engine's own answer, for the layout the person was in. */
std::string render_with(hunmin_ctx *ctx, const char *layout, const std::string &keys, int shift_first) {
    hunmin_ic *ic = hunmin_ic_new(ctx, layout);
    std::string total;
    for (size_t i = 0; i < keys.size(); ++i) {
        const uint32_t shift = (shift_first && i == 0) ? 1u : 0u;
        if (hunmin_ic_process_key(ic, static_cast<uint32_t>(keys[i]), shift)) {
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

/* THE 0.3 GATE, in the maintainer's own example (ruled 2026-11-01: *"E1도 당연히
 * 지원해야지. 내 예제가 동작하는 것까지를 0.3 release gate로 보자."*).
 *
 * `English는 좋은 Language입니다` typed as ONE key stream with `autoswitch` on and
 * the shipped dictionary: every word must come out in the language the person meant,
 * which needs BOTH lines of R8's formula — Hangul to English for `English` and
 * `Language`, English to Hangul for `는`, `좋은` and `입니다` (E1).
 *
 * The expectation is the engine's own text for each word on the layout that word was
 * meant in (`render_with`), never typed Hangul and never a hand-written English
 * string: the test states what the person meant and lets the engine say what that
 * is. The spaces are delivered too, as they are in the stream.
 *
 * The frontend's half is modelled, not assumed: after each key the test reads
 * `take_judgement` and performs the switch when a mode comes back. */
TEST_F(WordSetTest, TheZeroThreeGateMixedSentenceWorksInOneStream) {
    struct Word {
        const char *keys;
        const char *layout; /* the layout the person was in for this word */
        int shift_first;    /* the two capitalised words */
        int expected_mode;  /* HUNMIN_LANGUAGE_*, or -1 for no judgement */
    };
    const Word words[] = {
        {"english", "us-qwerty", 1, HUNMIN_LANGUAGE_LATIN},
        {"sms", "dubeolsik", 0, HUNMIN_LANGUAGE_HANGUL},     /* 는 */
        {"whgdms", "dubeolsik", 0, -1},                      /* 좋은: Hangul in, Hangul out */
        {"language", "us-qwerty", 1, HUNMIN_LANGUAGE_LATIN},
        {"dlqslek", "dubeolsik", 0, HUNMIN_LANGUAGE_HANGUL}, /* 입니다 */
    };

    std::string expected;
    for (const Word &w : words) {
        expected += render_with(ctx_, w.layout, w.keys, w.shift_first);
        expected += ' ';
    }

    hunmin_buffered_preedit *l = pair_layer(ctx_, 1);
    ASSERT_NE(l, nullptr);
    std::string delivered;
    for (const Word &w : words) {
        const std::string keys = w.keys;
        for (size_t i = 0; i < keys.size(); ++i) {
            hunmin_key_result out{};
            const uint32_t shift = (w.shift_first && i == 0) ? 1u : 0u;
            hunmin_buffered_preedit_process_key(l, static_cast<uint32_t>(keys[i]), shift, &out);
            if (out.commit && *out.commit) delivered += out.commit;
            const int mode = hunmin_buffered_preedit_take_judgement(l);
            if (mode >= 0) {
                EXPECT_EQ(w.expected_mode, mode) << w.keys;
                hunmin_key_result switched{};
                hunmin_buffered_preedit_toggle_language(l, &switched);
            }
        }
        /* The space ends the word and reaches the client. */
        hunmin_key_result out{};
        hunmin_buffered_preedit_process_key(l, static_cast<uint32_t>(' '), 0, &out);
        if (out.commit && *out.commit) delivered += out.commit;
        const int mode = hunmin_buffered_preedit_take_judgement(l);
        if (mode >= 0) {
            EXPECT_EQ(w.expected_mode, mode) << w.keys << " at its space";
            hunmin_key_result switched{};
            hunmin_buffered_preedit_toggle_language(l, &switched);
        }
        delivered += ' ';
    }
    hunmin_buffered_preedit_free(l);

    EXPECT_EQ(expected, delivered)
        << "the mixed sentence did not come out in the languages the person meant";
}

TEST_F(WordSetTest, JudgeTimingIsAnOptionAndDefaultsToPerKey) {
    hunmin_buffered_preedit *l = pair_layer(ctx_, 1);
    ASSERT_NE(l, nullptr);
    int timing = -1;
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l, "judge_timing", &timing));
    EXPECT_EQ(1, timing) << "the maintainer's chosen default is per key";
    EXPECT_TRUE(hunmin_buffered_preedit_set_option(l, "judge_timing", 0));
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l, "judge_timing", &timing));
    EXPECT_EQ(0, timing);
    EXPECT_FALSE(hunmin_buffered_preedit_set_option(l, "judge_timing", 2)) << "only the two timings exist";
    hunmin_buffered_preedit_free(l);
}

TEST_F(WordSetTest, TheJudgementTimingChangesTheDisplayAndNeverTheText) {
    const char *words[] = {"hello", "alphabetically", "the", "kbs", "dkssud", "on"};
    for (const char *w : words) {
        hunmin_buffered_preedit *per_key = pair_layer(ctx_, 1);
        hunmin_buffered_preedit *boundary = pair_layer(ctx_, 1);
        ASSERT_NE(per_key, nullptr);
        ASSERT_NE(boundary, nullptr);
        ASSERT_TRUE(hunmin_buffered_preedit_set_option(per_key, "judge_timing", 1));
        ASSERT_TRUE(hunmin_buffered_preedit_set_option(boundary, "judge_timing", 0));

        const int per_key_changes = display_changes(per_key, w);
        const int boundary_changes = display_changes(boundary, w);
        const WordRun per_key_run = type_and_judge(per_key, " ");
        const WordRun boundary_run = type_and_judge(boundary, " ");

        EXPECT_EQ(boundary_run.commit, per_key_run.commit) << "the timing changed the delivered text: " << w;
        EXPECT_EQ(boundary_run.mode, per_key_run.mode) << "the timing changed the mode the frontend adopts: " << w;
        /* At the boundary only the word is shown in the current mode and the answer
         * arrives once, at the space: so nothing during typing may be Latin. */
        EXPECT_EQ(0, boundary_changes) << "boundary timing showed the judged language while typing: " << w;
        /* Per key shows whatever the judgement says at that key -- which is the
         * flicker the item names, not a defect: it is the chosen default. */
        EXPECT_GE(per_key_changes, 0) << w;
        hunmin_buffered_preedit_free(per_key);
        hunmin_buffered_preedit_free(boundary);
    }
}

TEST_F(WordSetTest, PerKeyTimingFlickersOnAnEnglishWordAndBoundaryTimingDoesNot) {
    /* The measured shape (docs/OPTIONS-MEASURED.md §BND-4), with R9's three-key
     * guard applied: the first two keys of any word are never judged, so a flip can
     * only start at key 3. `hello` therefore flips once (Hangul, Hangul, Hangul,
     * Latin, Latin); `alphabetically` flips back and forth 11 times. The count is a
     * property of the dictionary, not of this rule, so the assertion is the
     * property: a word that changes language inside itself. */
    hunmin_buffered_preedit *per_key = pair_layer(ctx_, 1);
    ASSERT_NE(per_key, nullptr);
    EXPECT_EQ(1, display_changes(per_key, "hello"))
        << "`hello` flips once, at the key where the guard stops applying";
    hunmin_buffered_preedit_free(per_key);

    hunmin_buffered_preedit *deep = pair_layer(ctx_, 1);
    ASSERT_NE(deep, nullptr);
    EXPECT_GT(display_changes(deep, "alphabetically"), 1) << "a long English word flips back";
    hunmin_buffered_preedit_free(deep);

    hunmin_buffered_preedit *boundary = pair_layer(ctx_, 1);
    ASSERT_NE(boundary, nullptr);
    ASSERT_TRUE(hunmin_buffered_preedit_set_option(boundary, "judge_timing", 0));
    EXPECT_EQ(0, display_changes(boundary, "hello")) << "the boundary timing has no mid-word flip";
    /* The same layer, one key later: `display_changes` typed the word, so the space
     * is what completes it. */
    const WordRun r = type_and_judge(boundary, " ");
    EXPECT_EQ("hello", r.commit);
    EXPECT_EQ(HUNMIN_LANGUAGE_LATIN, r.mode);
    hunmin_buffered_preedit_free(boundary);
}

TEST_F(WordSetTest, AutoswitchOffIsTheIdentityPath) {
    hunmin_buffered_preedit *off = pair_layer(ctx_, 0);
    ASSERT_NE(off, nullptr);
    hunmin_buffered_preedit *on = pair_layer(ctx_, 1);
    ASSERT_NE(on, nullptr);

    const WordRun a = type_and_judge(off, "hello ");
    EXPECT_EQ(-1, a.mode) << "OFF judges nothing";
    EXPECT_FALSE(a.twice);

    const WordRun b = type_and_judge(on, "hello ");
    EXPECT_EQ(HUNMIN_LANGUAGE_LATIN, b.mode) << "ON delivers the English word and says so";
    EXPECT_EQ("hello", b.commit) << "the word is delivered in the judged language";

    /* The identity the requirement asks for: with the switch off, the text is
     * exactly what it was before the feature existed -- which is the same keys
     * on a layer with no judgement data at all. */
    hunmin_buffered_preedit *bare = hunmin_buffered_preedit_new(ctx_, "dubeolsik", "us-qwerty",
                                                                HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(bare, nullptr);
    hunmin_buffered_preedit_set_option(bare, "autoswitch", 0);
    EXPECT_EQ(type_and_judge(bare, "hello ").commit, a.commit);

    hunmin_buffered_preedit_free(bare);
    hunmin_buffered_preedit_free(on);
    hunmin_buffered_preedit_free(off);
}

TEST_F(WordSetTest, TheFormulaSwitchesOnlyWhenTheTwoReadingsDisagree) {
    /* Switch only when exactly one side is valid; otherwise keep the current
     * mode (R8's third line, which is the common case, not a failure). */
    struct Case {
        const char *keys;
        int expected; /* HUNMIN_LANGUAGE_LATIN or -1 */
        const char *why;
    } cases[] = {
        {"hello ", HUNMIN_LANGUAGE_LATIN, "English word, not a syllable run"},
        {"one ", HUNMIN_LANGUAGE_LATIN, "English word, not a syllable run"},
        {"the ", -1, "both valid: `the` is a word and the syllable is well-formed"},
        {"kbs ", -1, "neither valid"},
        {"dkssud ", -1, "Hangul only"},
    };
    for (const Case &c : cases) {
        hunmin_buffered_preedit *l = pair_layer(ctx_, 1);
        ASSERT_NE(l, nullptr);
        const WordRun r = type_and_judge(l, c.keys);
        EXPECT_EQ(c.expected, r.mode) << c.keys << " -- " << c.why;
        EXPECT_FALSE(r.twice) << c.keys << ": one word, one answer";
        /* Keep means keep: the delivered text is what an OFF layer delivers. */
        if (c.expected < 0) {
            hunmin_buffered_preedit *off = pair_layer(ctx_, 0);
            ASSERT_NE(off, nullptr);
            EXPECT_EQ(type_and_judge(off, c.keys).commit, r.commit) << c.keys;
            hunmin_buffered_preedit_free(off);
        }
        hunmin_buffered_preedit_free(l);
    }
}

TEST_F(WordSetTest, R9sGuardCountsKeyInputs) {
    /* The same valid-English, invalid-Hangul pair at two and at three keys: the
     * guard is the only difference, so these two cases together are the
     * threshold. */
    hunmin_buffered_preedit *two = pair_layer(ctx_, 1);
    ASSERT_NE(two, nullptr);
    EXPECT_EQ(-1, type_and_judge(two, "on ").mode) << "two key inputs are not judged (R9)";
    hunmin_buffered_preedit_free(two);

    hunmin_buffered_preedit *three = pair_layer(ctx_, 1);
    ASSERT_NE(three, nullptr);
    EXPECT_EQ(HUNMIN_LANGUAGE_LATIN, type_and_judge(three, "one ").mode)
        << "three key inputs are (R9, ruled 2026-11-01)";
    hunmin_buffered_preedit_free(three);
}

TEST_F(WordSetTest, TheJudgementIsTakenOnceAndClearedByTheNextBoundary) {
    hunmin_buffered_preedit *l = pair_layer(ctx_, 1);
    ASSERT_NE(l, nullptr);
    const WordRun r = type_and_judge(l, "hello ");
    ASSERT_EQ(HUNMIN_LANGUAGE_LATIN, r.mode);
    EXPECT_EQ(-1, r.second) << "a second read must not apply the switch again";

    /* A word that judges nothing clears the previous answer: the frontend acts
     * on the word it was told about, never on the one before it. */
    hunmin_key_result out{};
    for (const char *p = "one "; *p; ++p)
        hunmin_buffered_preedit_process_key(l, static_cast<uint32_t>(*p), 0, &out);
    ASSERT_EQ(HUNMIN_LANGUAGE_LATIN, hunmin_buffered_preedit_take_judgement(l));
    for (const char *p = "the "; *p; ++p)
        hunmin_buffered_preedit_process_key(l, static_cast<uint32_t>(*p), 0, &out);
    EXPECT_EQ(-1, hunmin_buffered_preedit_take_judgement(l))
        << "the second word judged nothing, so nothing is left over from the first";
    hunmin_buffered_preedit_free(l);
}

TEST_F(WordSetTest, NoDictionaryMeansNoJudgement) {
    /* B3: data absent is a normal state, and the judgement abstains rather than
     * calling every word not-English. */
    hunmin_buffered_preedit *fresh = hunmin_buffered_preedit_new(ctx_, "dubeolsik", "us-qwerty",
                                                                 HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(fresh, nullptr);
    hunmin_buffered_preedit_set_option(fresh, "autoswitch", 1);
    const WordRun r = type_and_judge(fresh, "hello ");
    EXPECT_EQ(-1, r.mode);

    hunmin_buffered_preedit *off = pair_layer(ctx_, 0);
    ASSERT_NE(off, nullptr);
    EXPECT_EQ(type_and_judge(off, "hello ").commit, r.commit) << "the text is untouched";
    hunmin_buffered_preedit_free(off);
    hunmin_buffered_preedit_free(fresh);
}

TEST_F(WordSetTest, WhatTheJudgementCannotDecideItDeclines) {
    /* Three frontends cannot be judged the same way, so each declines out loud
     * rather than guessing: one language (no second reading), a Hangul slot that
     * composes no Hangul (R8-정정-3), and per-syllable delivery (the earlier
     * syllables are already in the client, R10). */
    hunmin_buffered_preedit *one_language = hunmin_buffered_preedit_new(
        ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(one_language, nullptr);
    hunmin_buffered_preedit_load_english_shipped(one_language);
    hunmin_buffered_preedit_set_option(one_language, "autoswitch", 1);
    EXPECT_EQ(-1, type_and_judge(one_language, "hello ").mode);
    hunmin_buffered_preedit_free(one_language);

    hunmin_buffered_preedit *latin_slot = hunmin_buffered_preedit_new(
        ctx_, "us-qwerty", "us-qwerty", HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(latin_slot, nullptr);
    hunmin_buffered_preedit_load_english_shipped(latin_slot);
    hunmin_buffered_preedit_set_option(latin_slot, "autoswitch", 1);
    EXPECT_EQ(-1, type_and_judge(latin_slot, "hello ").mode)
        << "a Hangul slot that composes no Hangul has no syllables to judge";
    hunmin_buffered_preedit_free(latin_slot);

    hunmin_buffered_preedit *per_syllable = pair_layer(ctx_, 1, HUNMIN_COMMIT_PER_SYLLABLE);
    ASSERT_NE(per_syllable, nullptr);
    EXPECT_EQ(-1, type_and_judge(per_syllable, "hello ").mode)
        << "per-syllable delivery has already handed the word over syllable by syllable";
    hunmin_buffered_preedit_free(per_syllable);
}

/* ---------------------------------- a file the schema forbids (#173)
 *
 * The maintainer's ruling: *"중간 수준인 일단 해석은 하되 경고를 출력하는 방식으로
 * 가자."* A found file that holds words outside the schema is LOADED -- the words
 * it does hold keep working -- and the loader names the bad elements on stderr,
 * so a truncated list cannot pass as complete. The refusal is
 * tools/doctor.py's half (ctest schema-contract); this asserts the runtime half.
 */

namespace {

std::size_t count_of(const std::string &hay, const std::string &needle) {
    std::size_t n = 0;
    for (std::size_t at = hay.find(needle); at != std::string::npos;
         at = hay.find(needle, at + needle.size()))
        ++n;
    return n;
}

} // namespace

TEST_F(WordSetTest, ASchemaViolatingFileLoadsAndSaysSoOutLoud) {
    /* A number and an object cannot be words; `""` is not one either. A capital
     * and a space are interpretable, so they are kept -- and the capital is even
     * findable, because the lookup folds case. All five are named. */
    ScratchFile mixed("hunmin-schema-violating.json",
                      "[\"computer\", 3, {\"w\":\"dog\"}, \"\", \"Computer\", \"ice cream\"]");
    ::testing::internal::CaptureStderr();
    const int rc = hunmin_buffered_preedit_load_english(layer_, mixed.c_str());
    const std::string log = ::testing::internal::GetCapturedStderr();

    EXPECT_EQ(1, rc) << "a damaged file degrades to fewer words, not to no feature";
    EXPECT_EQ(1, hunmin_buffered_preedit_english_ready(layer_));
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, "computer"));
    EXPECT_EQ(1, hunmin_buffered_preedit_english_valid(layer_, "Computer"));

    EXPECT_NE(std::string::npos, log.find("element 1 is not a word string")) << log;
    EXPECT_NE(std::string::npos, log.find("element 2 is not a word string")) << log;
    EXPECT_NE(std::string::npos, log.find("element 3 is not a word string")) << log;
    EXPECT_NE(std::string::npos, log.find("element 4 'Computer' is outside")) << log;
    EXPECT_NE(std::string::npos, log.find("element 5 'ice cream' is outside")) << log;

    /* The warning is about the file, not about the run: the shipped list is
     * shaped as the schema says, so loading it says nothing. */
    ::testing::internal::CaptureStderr();
    EXPECT_EQ(1, hunmin_buffered_preedit_load_english(layer_, shipped_list().c_str()));
    EXPECT_EQ("", ::testing::internal::GetCapturedStderr());
}

TEST_F(WordSetTest, AWhollyBrokenFileIsOneFactNotOneLinePerElement) {
    std::string body = "[";
    for (int i = 0; i < 12; i++) {
        if (i) body += ",";
        body += std::to_string(i);
    }
    body += "]";
    ScratchFile broken("hunmin-wholly-broken.json", body);

    ::testing::internal::CaptureStderr();
    EXPECT_EQ(1, hunmin_buffered_preedit_load_english(layer_, broken.c_str()));
    const std::string log = ::testing::internal::GetCapturedStderr();

    /* Eight named, then one line carrying the total -- 118k of these would
     * otherwise be 118k lines of stderr. */
    EXPECT_EQ(std::size_t{8}, count_of(log, "is not a word string")) << log;
    EXPECT_NE(std::string::npos, log.find("12 elements are outside the schema")) << log;
    EXPECT_EQ(0, hunmin_buffered_preedit_english_valid(layer_, "computer"))
        << "no word was added, and 1 still means loaded here -- which is why the "
           "warning carries the count";
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
