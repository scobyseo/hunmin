/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier D1 for the IBus frontend (issue #288): the key path with no IBus in it.
 *
 * These cases drive hunmin_state directly and record what it asks a host to
 * show. That is the point of the split: every rule asserted here is the
 * frontend's behaviour before any D-Bus, panel or daemon exists, so a red here
 * names the state object and a red at D2 names the mapping to IBus.
 *
 * What is deliberately NOT here (and not implied by a green run):
 *  - the IBus key translation itself (D2 asserts it through the real engine),
 *  - the lookup table's page size (a host ceiling; D2 sees the published
 *    table and issue #292's candidate-window gate measures the drawing),
 *  - caps/옛한글 compositions (a layout feature, measured at tier A).
 */
#include <gtest/gtest.h>

#include <hunmin.h>

#include <string>

extern "C" {
#include "hunmin_state.h"
}

namespace {

/* What a host would have been shown. One record per state object. */
struct Recording {
    std::string committed;  /* accumulated since the last Clear() */
    std::string preedit;
    int cursor = -1;
    int list_events = 0;
    bool list_open = false;

    void Clear() {
        committed.clear();
    }
};

void on_commit(void *user, const char *text) {
    static_cast<Recording *>(user)->committed += text;
}

void on_preedit(void *user, const char *text, int cursor) {
    auto *r = static_cast<Recording *>(user);
    r->preedit = text ? text : "";
    r->cursor = cursor;
}

void on_candidates(void *user, int open) {
    auto *r = static_cast<Recording *>(user);
    r->list_open = open != 0;
    ++r->list_events;
}

class IbusStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        state_ = hunmin_state_new(nullptr, 0);
        ASSERT_NE(state_, nullptr);
        sink_.user = &rec_;
        sink_.commit = on_commit;
        sink_.preedit = on_preedit;
        sink_.candidates = on_candidates;
        hunmin_state_set_sink(state_, &sink_);
    }

    void TearDown() override { hunmin_state_free(state_); }

    /* One US-QWERTY character as the adapter would hand it over: lowercase
     * base, shift as a separate bit. */
    int Key(char base, int shift = 0, unsigned mods = 0) {
        hunmin_key_event ev{};
        ev.id = HUNMIN_KEY_TEXT;
        ev.base = static_cast<unsigned char>(base);
        ev.shift = shift ? 1u : 0u;
        ev.mods = mods;
        ev.candidates_open = hunmin_state_candidates_open(state_);
        return hunmin_state_key(state_, &ev);
    }

    int Named(hunmin_key_id id) {
        hunmin_key_event ev{};
        ev.id = id;
        ev.candidates_open = hunmin_state_candidates_open(state_);
        return hunmin_state_key(state_, &ev);
    }

    int Language() {
        hunmin_key_event ev{};
        ev.id = HUNMIN_KEY_TEXT;
        ev.language_key = 1;
        return hunmin_state_key(state_, &ev);
    }

    void Type(const char *keys) {
        for (const char *p = keys; *p; ++p) Key(*p);
    }

    hunmin_state *state_ = nullptr;
    hunmin_state_sink sink_{};
    Recording rec_;
};

/* The buffered layer's contract: the word is delivered where it ends, not per
 * syllable, and the preedit shows it while it is still composing. */
TEST_F(IbusStateTest, ComposesAndDeliversTheWordAtTheBoundary) {
    Type("dkssud");
    EXPECT_EQ(rec_.preedit, "안녕") << "the whole word is the preedit";
    EXPECT_EQ(rec_.cursor, 2) << "the cursor counts characters, not bytes";
    EXPECT_TRUE(rec_.committed.empty()) << "nothing is delivered before the boundary";

    /* Space is a key the client owns: the layer declines it, and the word it
     * was holding becomes the boundary that delivers it. */
    rec_.Clear();
    EXPECT_EQ(Key(' '), 0) << "a declined key belongs to the client";
    EXPECT_EQ(rec_.committed, "안녕");
    EXPECT_EQ(rec_.preedit, "");
    EXPECT_EQ(rec_.cursor, 0);
}

TEST_F(IbusStateTest, BackspaceEditsTheComposition) {
    Type("rk");
    ASSERT_EQ(rec_.preedit, "가");
    rec_.Clear();
    EXPECT_EQ(Key('\b'), 1) << "backspace over a pending syllable is the engine's";
    EXPECT_EQ(rec_.preedit, "ㄱ");
    EXPECT_TRUE(rec_.committed.empty());
}

TEST_F(IbusStateTest, BackspaceOnAnEmptyCompositionIsDeclined) {
    EXPECT_EQ(Key('\b'), 0);
    EXPECT_TRUE(rec_.committed.empty());
    EXPECT_EQ(rec_.preedit, "");
}

/* FE13 for this frontend: the platform hands us the Han/Eng trigger, so the
 * key is the engine's and the switch is the layer's. The composing word is not
 * silently dropped -- it is delivered before the language changes. */
TEST_F(IbusStateTest, LanguageKeyDeliversTheWordAndSwitches) {
    Type("rk");
    ASSERT_EQ(rec_.preedit, "가");
    rec_.Clear();
    EXPECT_EQ(Language(), 1) << "the state owns the trigger on this frontend";
    EXPECT_EQ(rec_.committed, "가") << "the boundary the switch performs delivers it";
    EXPECT_EQ(rec_.preedit, "");
    EXPECT_EQ(hunmin_state_language(state_), HUNMIN_LANGUAGE_LATIN);
    EXPECT_EQ(std::string(hunmin_state_layout_id(state_)), "us-qwerty");

    rec_.Clear();
    EXPECT_EQ(Key('r'), 1);
    /* The Latin half delivers characters, but the LAYER still holds the word
     * until a boundary: same delivery model on both languages, which is what
     * DESIGN-FRONTEND §5 promises (the concatenation is the contract). */
    EXPECT_EQ(rec_.preedit, "r");
    EXPECT_EQ(hunmin_state_language(state_), HUNMIN_LANGUAGE_LATIN);
    rec_.Clear();
    EXPECT_EQ(Key(' '), 0);
    EXPECT_EQ(rec_.committed, "r");

    rec_.Clear();
    EXPECT_EQ(Language(), 1);
    EXPECT_EQ(hunmin_state_language(state_), HUNMIN_LANGUAGE_HANGUL);
    EXPECT_EQ(std::string(hunmin_state_layout_id(state_)), "dubeolsik");
}

/* ReqMode8's judgement is the shared layer's; this frontend only performs the
 * switch it reports. Both halves are asserted so neither can pass vacuously. */
TEST_F(IbusStateTest, AutoswitchDeliversTheWordInTheJudgedLanguage) {
    hunmin_state_set_autoswitch(state_, 1);
    int held = -1;
    ASSERT_TRUE(hunmin_state_get_autoswitch(state_, &held));
    ASSERT_EQ(held, 1) << "the English data ships with the tree; the gate must not refuse ON";

    rec_.Clear();
    Type("hello");
    EXPECT_EQ(rec_.preedit, "hello") << "ReqMode8's judged display while composing";
    EXPECT_EQ(hunmin_state_language(state_), HUNMIN_LANGUAGE_HANGUL);

    rec_.Clear();
    EXPECT_EQ(Key(' '), 0);
    EXPECT_EQ(rec_.committed, "hello") << "the judged reading, delivered once";
    EXPECT_EQ(hunmin_state_language(state_), HUNMIN_LANGUAGE_LATIN)
        << "the mode half of the judgement ran";
}

TEST_F(IbusStateTest, AutoswitchOffKeepsTheHangulReading) {
    rec_.Clear();
    Type("hello");
    rec_.Clear();
    EXPECT_EQ(Key(' '), 0);
    EXPECT_NE(rec_.committed, "hello") << "OFF is the identity path";
    EXPECT_EQ(hunmin_state_language(state_), HUNMIN_LANGUAGE_HANGUL);
}

TEST_F(IbusStateTest, CandidateListOpensForTheComposingWord) {
    Type("rk");
    ASSERT_EQ(rec_.preedit, "가");
    EXPECT_EQ(hunmin_state_toggle_candidates(state_), 1);
    ASSERT_GT(hunmin_state_candidate_count(state_), 0);
    EXPECT_EQ(std::string(hunmin_state_candidate_text(state_, 0)), "可")
        << "upstream frequency order, first entry";
    EXPECT_EQ(rec_.list_events, 1);
    EXPECT_TRUE(rec_.list_open);
    EXPECT_EQ(hunmin_state_candidate_cursor(state_), 0);

    /* Down walks the list; the cursor is what Return will commit. */
    EXPECT_EQ(Named(HUNMIN_KEY_DOWN), 1);
    EXPECT_EQ(hunmin_state_candidate_cursor(state_), 1);
    const std::string second = hunmin_state_candidate_text(state_, 1);
    ASSERT_FALSE(second.empty());

    rec_.Clear();
    EXPECT_EQ(Named(HUNMIN_KEY_ENTER), 1);
    EXPECT_EQ(rec_.committed, second) << "Return commits the highlighted candidate";
    EXPECT_EQ(rec_.preedit, "") << "the composition the candidate came from is dropped";
    EXPECT_FALSE(rec_.list_open);
    EXPECT_EQ(hunmin_state_candidate_count(state_), 0);
}

TEST_F(IbusStateTest, CandidateKeyWithoutCandidatesIsDeclined) {
    EXPECT_EQ(hunmin_state_toggle_candidates(state_), 0)
        << "F9 with nothing composing reaches the client";
    EXPECT_EQ(rec_.list_events, 0);
}

TEST_F(IbusStateTest, EscapeClosesTheListWithoutCommitting) {
    Type("rk");
    ASSERT_EQ(hunmin_state_toggle_candidates(state_), 1);
    rec_.Clear();
    EXPECT_EQ(Named(HUNMIN_KEY_ESCAPE), 1);
    EXPECT_TRUE(rec_.committed.empty());
    EXPECT_FALSE(rec_.list_open);
    EXPECT_EQ(rec_.preedit, "가") << "cancelling the window keeps the composition";
}

/* A focus change must not swallow the word (the Android invariant this tier
 * makes cheap to check on a new frontend): the adapter flushes, then resets.
 * reset() alone is the layer's silent drop. */
TEST_F(IbusStateTest, FlushDeliversTheCompositionThenResetDropsNothing) {
    Type("rk");
    rec_.Clear();
    hunmin_state_flush(state_);
    EXPECT_EQ(rec_.committed, "가");
    EXPECT_EQ(rec_.preedit, "");
    rec_.Clear();
    hunmin_state_reset(state_);
    EXPECT_TRUE(rec_.committed.empty());
}

/* A policy change rebuilds the layer; the word must survive the rebuild. */
TEST_F(IbusStateTest, CommitPolicyChangeDeliversTheWordFirst) {
    Type("rk");
    ASSERT_EQ(rec_.preedit, "가");
    rec_.Clear();
    ASSERT_TRUE(hunmin_state_set_commit_policy(state_, HUNMIN_COMMIT_PER_SYLLABLE));
    EXPECT_EQ(rec_.committed, "가");
    EXPECT_EQ(rec_.preedit, "");
    /* Per-syllable delivery: the syllable is delivered when the engine
     * finalises it, which the next key is what causes. */
    rec_.Clear();
    Type("rkrk");
    EXPECT_EQ(rec_.committed, "가") << "per-syllable policy delivers as it finalises";
}

}  // namespace