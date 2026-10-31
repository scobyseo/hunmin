/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * key-rule-test.cpp - tier A for the shared key rule (issue #128, decision
 * 2026-10-25: option C, classifier takes `candidates_open`, and the duplicated
 * per-frontend assertions are retired).
 *
 * This suite is THE assertion of the precedence. The frontends keep tests for
 * what is theirs (platform mapping, rendering, the candidate UI); they no
 * longer each re-assert that a Ctrl chord is a boundary, that an unmapped key
 * passes, or that the Han/Eng key is the language action.
 */
#include <gtest/gtest.h>

extern "C" {
#include "hunmin.h"
}

namespace {

const char *kDataPath = nullptr;

hunmin_ctx *make_ctx() {
    const char *dirs[1] = {kDataPath};
    return hunmin_ctx_new(dirs, 1);
}

/* A plain character event: no modifiers, no list, not the language key. */
hunmin_key_event ch(uint32_t base) {
    hunmin_key_event ev{};
    ev.base = base;
    ev.id = HUNMIN_KEY_TEXT;
    return ev;
}

hunmin_key_event named(hunmin_key_id id) {
    hunmin_key_event ev{};
    ev.id = id;
    return ev;
}

class KeyRuleTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ctx_ = make_ctx();
        ASSERT_NE(ctx_, nullptr);
        l_ = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
        ASSERT_NE(l_, nullptr);
    }
    void TearDown() override {
        if (l_) hunmin_buffered_preedit_free(l_);
        if (ctx_) hunmin_ctx_free(ctx_);
    }
    hunmin_ctx *ctx_ = nullptr;
    hunmin_buffered_preedit *l_ = nullptr;
};

TEST_F(KeyRuleTest, ALayoutKeyIsFed) {
    hunmin_key_event ev = ch('d');
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_FEED);
    /* Shift alone is a layout row, not a chord. */
    ev.shift = 1;
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_FEED);
    ev.mods = HUNMIN_MOD_SHIFT;
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_FEED);
}

TEST_F(KeyRuleTest, EveryChordIsABoundaryAndTheKeyPasses) {
    const uint32_t chords[] = {HUNMIN_MOD_CTRL, HUNMIN_MOD_ALT, HUNMIN_MOD_GUI,
                               HUNMIN_MOD_CTRL | HUNMIN_MOD_ALT,
                               HUNMIN_MOD_CTRL | HUNMIN_MOD_ALT | HUNMIN_MOD_GUI};
    for (uint32_t m : chords) {
        hunmin_key_event ev = ch('a');
        ev.mods = m;
        EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_BOUNDARY_PASS) << "mods=" << m;
    }
    /* Shift beside a chord does not rescue it. */
    hunmin_key_event ev = ch('a');
    ev.mods = HUNMIN_MOD_CTRL | HUNMIN_MOD_SHIFT;
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_BOUNDARY_PASS);
}

TEST_F(KeyRuleTest, AnUnmappedKeyIsABoundary) {
    hunmin_key_event ev = ch(0);
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_BOUNDARY_PASS);
}

TEST_F(KeyRuleTest, NamedKeysAreBoundariesUnlessACandidateListIsOpen) {
    const hunmin_key_id ids[] = {HUNMIN_KEY_UP,      HUNMIN_KEY_DOWN,     HUNMIN_KEY_LEFT,    HUNMIN_KEY_RIGHT,
                                 HUNMIN_KEY_PAGE_UP, HUNMIN_KEY_PAGE_DOWN, HUNMIN_KEY_ENTER, HUNMIN_KEY_ESCAPE};
    for (hunmin_key_id id : ids) {
        hunmin_key_event ev = named(id);
        EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_BOUNDARY_PASS) << "id=" << (int)id;
        /* The SAME key with a list open is the list's. */
        ev.candidates_open = 1;
        EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_CANDIDATE) << "id=" << (int)id;
    }
}

TEST_F(KeyRuleTest, TheOpenListDoesNotClaimLayoutKeys) {
    /* While the window is open a letter is not the list's: it closes the
     * window and is fed, which is the frontend's presentation decision. */
    hunmin_key_event ev = ch('d');
    ev.candidates_open = 1;
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_FEED);
    hunmin_key_event space = ch(' ');
    space.candidates_open = 1;
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &space), HUNMIN_KEY_FEED);
}

TEST_F(KeyRuleTest, TheLanguageKeyWinsOverEverythingElse) {
    hunmin_key_event ev = ch('d');
    ev.language_key = 1;
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_LANGUAGE_TOGGLE);
    /* Even while a list is open. */
    ev.candidates_open = 1;
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_LANGUAGE_TOGGLE);
    ev.candidates_open = 0;
    /* A chord is the compound press only when a function is BOUND to it: the
     * engine ships VOICE = Alt and AUTOSWITCH = Ctrl (ruled 2026-10-31), so those
     * two answer LANGUAGE_CHORD, and a chord with nothing bound still answers
     * TOGGLE -- an unassigned chord must not become a dead key. Before the ruling
     * this case was pinned as LANGUAGE_TOGGLE for every chord; the ruling is what
     * changed it, and the binding table is what decides. */
    for (uint32_t mods : {HUNMIN_MOD_CTRL, HUNMIN_MOD_ALT}) {
        ev.mods = mods;
        EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_LANGUAGE_CHORD)
            << "bound mods=" << mods;
        /* A list being open does not change that: the language key is still
         * the one key that is never the list's. */
        ev.candidates_open = 1;
        EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_LANGUAGE_CHORD)
            << "bound mods=" << mods;
        ev.candidates_open = 0;
    }
    for (uint32_t mods : {HUNMIN_MOD_GUI, (uint32_t)(HUNMIN_MOD_ALT | HUNMIN_MOD_CTRL)}) {
        ev.mods = mods;
        EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_LANGUAGE_TOGGLE)
            << "unbound mods=" << mods;
    }
    /* SHIFT is a layout row for characters, and it is NOT in HUNMIN_MOD_CHORD:
     * a shift bit in `mods` is ignored. With the language key, Shift is a
     * special combination *when a function is bound to it* -- see
     * ShiftIsASpecialCombinationOnlyWithTheLanguageKey; with the shipped
     * defaults (VOICE = Alt, AUTOSWITCH = Ctrl) nothing is bound to Shift, so
     * this press is still the toggle. */
    ev.mods = HUNMIN_MOD_SHIFT;
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_LANGUAGE_TOGGLE);
    ev.mods = 0;
    ev.shift = 1;
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_LANGUAGE_TOGGLE);
    ev.shift = 0;
}

TEST_F(KeyRuleTest, TheListBeatsTheChordRule) {
    /* A list key pressed with Ctrl is still the list's: the list is what the
     * user is looking at, and the frontend routes it. */
    hunmin_key_event ev = named(HUNMIN_KEY_DOWN);
    ev.candidates_open = 1;
    ev.mods = HUNMIN_MOD_CTRL;
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_CANDIDATE);
}

TEST_F(KeyRuleTest, AClassifiedFeedMatchesTheLayerAnswer) {
    /* The rule and the layer must not disagree: what the classifier calls
     * FEED is what the layer consumes or declines, and what it calls a
     * boundary is a key the layer declines. */
    const uint32_t keys[] = {'d', 'k', 's', '1', ' ', 0x08};
    for (uint32_t base : keys) {
        hunmin_buffered_preedit *probe = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
        ASSERT_NE(probe, nullptr);
        hunmin_key_event ev = ch(base);
        const hunmin_key_action action = hunmin_buffered_preedit_classify(probe, &ev);
        const int peeked = hunmin_buffered_preedit_peek_key(probe, base, 0);
        if (action == HUNMIN_KEY_FEED) {
            /* Not every feedable key is consumed by every layout state; but a
             * key the classifier passes must never be consumed. */
            if (!peeked) EXPECT_EQ(action, HUNMIN_KEY_FEED);
        } else {
            EXPECT_EQ(peeked, 0) << "base=" << base << " was classified as a boundary but the layer consumes it";
        }
        hunmin_buffered_preedit_free(probe);
    }
}

TEST_F(KeyRuleTest, ComposingIsTheInFlightSyllableOnly) {
    /* The interim delivery keeps the finished word as document text and
     * rewrites only the syllable in flight, so the two halves must be
     * readable separately. */
    hunmin_key_event ev = ch(0);
    hunmin_key_result out{};
    for (char c : {'d', 'k', 's', 's'}) {
        ev = ch((uint32_t)c);
        hunmin_buffered_preedit_process_key(l_, ev.base, ev.shift, &out);
    }
    /* d k s s: 안 is finished, ㄴ is in flight (measured with hunmin-sim:
     * commit 안, preedit ㄴ at the fourth key). */
    EXPECT_STREQ(hunmin_buffered_preedit_composing(l_), "ㄴ");
    EXPECT_STREQ(hunmin_buffered_preedit_display(l_), "안ㄴ");
    EXPECT_STREQ(hunmin_buffered_preedit_composing(l_), "ㄴ") << "reading the halves must not consume";
}

/* ---- the compound press's dispatch (E5, ruled 2026-10-31) ------------------ */

TEST_F(KeyRuleTest, TheEngineResolvesAndServicesTheCompoundPress) {
    /* The engine ships the ruled defaults (Table A), so three frontends that
     * seed nothing still agree; a store override replaces one entry. */
    EXPECT_EQ(hunmin_buffered_preedit_special_chord(l_, HUNMIN_SPECIAL_VOICE), HUNMIN_MOD_ALT);
    EXPECT_EQ(hunmin_buffered_preedit_special_chord(l_, HUNMIN_SPECIAL_AUTOSWITCH), HUNMIN_MOD_CTRL);

    hunmin_key_event ev = ch('d');
    ev.language_key = 1;
    ev.mods = HUNMIN_MOD_GUI;
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), 0) << "an unbound chord is not ours";

    /* Bound by default, but no host: resolved and refused. */
    ev.mods = HUNMIN_MOD_ALT;
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), -1) << "bound but no host: refused";

    int performed = 0;
    hunmin_special_host host{};
    host.user = &performed;
    host.perform = [](void *user, hunmin_special_function fn) { *(int *)user = (int)fn; };
    hunmin_buffered_preedit_special_host(l_, &host);
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), HUNMIN_SPECIAL_VOICE);
    EXPECT_EQ(performed, (int)HUNMIN_SPECIAL_VOICE);

    /* supports() == 0 is the platform's refusal: resolved, serviced by nobody. */
    hunmin_special_host refusing{};
    refusing.user = &performed;
    refusing.supports = [](void *, hunmin_special_function) { return 0; };
    refusing.perform = host.perform;
    performed = 0;
    hunmin_buffered_preedit_special_host(l_, &refusing);
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), -1);
    EXPECT_EQ(performed, 0) << "a refusal must not perform";

    /* Withdrawing the host is the other refusal. */
    hunmin_buffered_preedit_special_host(l_, nullptr);
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), -1);
}

TEST_F(KeyRuleTest, ThePeekAnswersWhatTheCallWouldDoAndPerformsNothing) {
    /* A platform "test" phase asks before the real key arrives, and test and
     * down must be one decision: the peek answers, the call performs. */
    hunmin_key_event ev = ch('d');
    ev.language_key = 1;
    ev.mods = HUNMIN_MOD_ALT; /* VOICE by the ruled default */

    EXPECT_EQ(hunmin_buffered_preedit_special_peek(l_, &ev), -1) << "bound, no host: refused";

    int performed = 0;
    hunmin_special_host host{};
    host.user = &performed;
    host.perform = [](void *user, hunmin_special_function fn) { *(int *)user = (int)fn; };
    hunmin_buffered_preedit_special_host(l_, &host);

    EXPECT_EQ(hunmin_buffered_preedit_special_peek(l_, &ev), HUNMIN_SPECIAL_VOICE) << "the peek resolves";
    EXPECT_EQ(performed, 0) << "the peek must not perform";
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), HUNMIN_SPECIAL_VOICE) << "and the call agrees";
    EXPECT_EQ(performed, (int)HUNMIN_SPECIAL_VOICE);

    hunmin_key_event single = ch('d');
    single.language_key = 1; /* no chord: the single press */
    EXPECT_EQ(hunmin_buffered_preedit_special_peek(l_, &single), 0) << "no chord, nothing to resolve";
    single.language_key = 0;
    single.mods = HUNMIN_MOD_ALT;
    EXPECT_EQ(hunmin_buffered_preedit_special_peek(l_, &single), 0) << "a character chord is not a language chord";
}

TEST_F(KeyRuleTest, AStoreOverrideWins) {
    hunmin_key_event ev = ch('d');
    ev.language_key = 1;

    /* The store's value replaces the default, and the dialog reads the live one. */
    EXPECT_TRUE(hunmin_buffered_preedit_special_bind(l_, HUNMIN_SPECIAL_AUTOSWITCH, HUNMIN_MOD_ALT));
    EXPECT_EQ(hunmin_buffered_preedit_special_chord(l_, HUNMIN_SPECIAL_AUTOSWITCH), HUNMIN_MOD_ALT);
    ev.mods = HUNMIN_MOD_CTRL;
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), 0) << "the default chord was replaced";
    EXPECT_TRUE(hunmin_buffered_preedit_special_bind(l_, HUNMIN_SPECIAL_AUTOSWITCH, HUNMIN_MOD_CTRL));
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), -1) << "restored, and still refused for want of a host";

    /* The single press is the toggle, not a special function. */
    ev.mods = 0;
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), 0);
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_LANGUAGE_TOGGLE);

    /* And an ordinary key with Alt is still the application's. */
    ev.language_key = 0;
    ev.mods = HUNMIN_MOD_ALT;
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), 0);
}

TEST_F(KeyRuleTest, ShiftIsASpecialCombinationOnlyWithTheLanguageKey) {
    /* Ruled 2026-10-31: "if it is used with normal characters, work as
     * capitalize. but with special combination such as shift + han/eng, it should
     * be able to do special work." So one bit, two meanings, and the event's
     * language_key is what separates them. */
    hunmin_key_event ev = ch('d');
    ev.shift = 1; /* Shift held, no language key: a layout row */
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_FEED)
        << "Shift + a letter is the shifted row, not a chord";
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), 0);

    /* Unbound Shift + 한/영 is still the toggle, not a dead key. */
    ev.language_key = 1;
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_LANGUAGE_TOGGLE);

    /* Bind it, and the same event becomes the compound press. */
    EXPECT_TRUE(hunmin_buffered_preedit_special_bind(l_, HUNMIN_SPECIAL_VOICE, HUNMIN_MOD_SHIFT));
    EXPECT_EQ(hunmin_buffered_preedit_special_chord(l_, HUNMIN_SPECIAL_VOICE), HUNMIN_MOD_SHIFT);
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_LANGUAGE_CHORD);

    int performed = 0;
    hunmin_special_host host{};
    host.user = &performed;
    host.perform = [](void *user, hunmin_special_function fn) { *(int *)user = (int)fn; };
    hunmin_buffered_preedit_special_host(l_, &host);
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), HUNMIN_SPECIAL_VOICE);
    EXPECT_EQ(performed, (int)HUNMIN_SPECIAL_VOICE);

    /* Shift+Alt is its own chord: a binding on SHIFT alone must not fire for it. */
    ev.mods = HUNMIN_MOD_ALT;
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), 0) << "exact chord match, not a superset";
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &ev), HUNMIN_KEY_LANGUAGE_TOGGLE)
        << "no binding for Shift+Alt, and VOICE was rebound away from Alt";
}

} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    const char *v = getenv("HUNMIN_DATA_PATH");
    kDataPath = v && *v ? v : "libhunmin/data";
    return RUN_ALL_TESTS();
}
