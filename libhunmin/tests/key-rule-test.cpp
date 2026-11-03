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

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

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

/* A data tree that holds the layouts and compose tables an ic needs and NOT the
 * modes word list the automatic switch needs (issue #169). An existing context
 * pins its own search dirs, so "the dictionary is not on the search path" has to
 * be a real tree rather than an unset variable: this fixtures stages one, and
 * HUNMIN_DATA_PATH still names the FULL data directory in a ctest run, so a
 * negative answer proves the context's own dirs are the search being consulted
 * -- the same resolution Android's sandbox depends on. */
class AutoswitchDataGateTest : public ::testing::Test {
  protected:
    /* A path of its own per test, not one shared literal.
     *
     * This used to stage into a fixed `…/hunmin-autoswitch-gate-no-modes`, and
     * a cold parallel suite run failed once in twelve with
     *   `could not stage a layouts-only data tree: File exists`
     * (measured 2026-11-03, issue #234). A fixed path under the shared temp dir
     * is a resource any second instance -- a rerun overlapping a leftover, a
     * parallel ctest job in another build dir -- can collide with, and the
     * failure then reads as a data-loading bug rather than as a test-isolation
     * one. The name carries the test and the pid, so the only tree this fixture
     * can collide with is its own, and every step reports what it was doing.
     */
    void SetUp() override {
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        /* pid would be the obvious disambiguator but this binary also builds on
         * Windows, where it is `_getpid` -- a steady-clock stamp plus an
         * in-process counter costs nothing and needs no header. */
        static unsigned long long seq = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        std::error_code ec;
        root_ = std::filesystem::temp_directory_path(ec) /
                ("hunmin-autoswitch-gate-no-modes-" + std::string(info->name()) + "-" +
                 std::to_string(stamp) + "-" + std::to_string(seq++));
        std::filesystem::remove_all(root_, ec);
        ec.clear();
        std::filesystem::create_directories(root_, ec);
        /* ASSERT_*, not EXPECT_*: a fatal assertion in SetUp skips the body
         * instead of running it against a half-staged tree. */
        ASSERT_FALSE(ec) << "could not create the staging root: " << ec.message();
        for (const char *sub : {"layouts", "compose"}) {
            ec.clear();
            std::filesystem::copy(std::filesystem::path(kDataPath) / sub, root_ / sub,
                                  std::filesystem::copy_options::recursive, ec);
            ASSERT_FALSE(ec) << "could not stage " << sub << ": " << ec.message();
        }
        dirs_ = root_.string();
        const char *dir = dirs_.c_str();
        ctx_ = hunmin_ctx_new(&dir, 1);
        ASSERT_NE(ctx_, nullptr) << "the staged tree does not load as a data dir";
        layer_ = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
        ASSERT_NE(layer_, nullptr);
    }
    void TearDown() override {
        if (layer_) hunmin_buffered_preedit_free(layer_);
        if (ctx_) hunmin_ctx_free(ctx_);
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }
    /* The staged path as a C string the context copies; kept as a member so it
     * outlives the ctx (the ctx copies it, so this is belt and braces). */
    std::filesystem::path root_;
    std::string dirs_;

    /* Write the staged dictionary -- the file this fixture deliberately does not
     * ship, so a case can choose which shape the gate sees. */
    void write_modes(const std::string &body) {
        std::error_code ec;
        std::filesystem::create_directories(root_ / "modes", ec);
        std::ofstream out((root_ / "modes" / "en-US.json").string(),
                          std::ios::binary | std::ios::trunc);
        out << body;
    }

    /* A context over the staged tree, so the presence answer is re-derived (the
     * cache is per context). Caller frees. */
    hunmin_ctx *fresh_ctx() {
        const char *dir = dirs_.c_str();
        return hunmin_ctx_new(&dir, 1);
    }

    hunmin_ctx *ctx_ = nullptr;
    hunmin_buffered_preedit *layer_ = nullptr;
};

TEST_F(AutoswitchDataGateTest, TheDataTheSwitchNeedsIsReportedAbsent) {
    EXPECT_FALSE(hunmin_ctx_autoswitch_data_present(ctx_))
        << "no modes/en-US.json on this context's search path";
    /* It is a query, not a write: asking twice answers the same and changes no
     * option. */
    EXPECT_FALSE(hunmin_ctx_autoswitch_data_present(ctx_));
    int v = 7;
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(layer_, "autoswitch", &v));
    EXPECT_EQ(v, 0) << "the B3 default, untouched by the query";
}

TEST_F(AutoswitchDataGateTest, EnablingItWhereTheDataIsAbsentIsRefused) {
    /* The whole point (B3, SPEC-MODES §R13-정정): a stale stored preference on a
     * machine without the data package must not half-enable the feature. */
    EXPECT_FALSE(hunmin_buffered_preedit_set_option(layer_, "autoswitch", 1));

    int v = -1;
    EXPECT_TRUE(hunmin_buffered_preedit_get_option(layer_, "autoswitch", &v))
        << "the NAME is known: the refusal is about the data, not the option";
    EXPECT_EQ(v, 0) << "off, data missing -- distinguishable from off by choice by "
                       "pairing this with the presence query";

    /* OFF is always writable: an install that lost its data must still be able to
     * record the preference, and a refusal must never wedge the value. */
    EXPECT_TRUE(hunmin_buffered_preedit_set_option(layer_, "autoswitch", 0));
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(layer_, "autoswitch", &v));
    EXPECT_EQ(v, 0);

    /* The key surface writes the SAME value, so it refuses too -- otherwise the
     * state could be switched ON from the keyboard while the settings row says
     * the feature cannot be enabled. A refusal is the unserviceable-platform
     * answer (-1): the key passes through and the platform half does not run. */
    int performed = 0;
    hunmin_special_host host{};
    host.user = &performed;
    host.perform = [](void *user, hunmin_special_function fn) { *(int *)user = (int)fn; };
    hunmin_buffered_preedit_special_host(layer_, &host);

    hunmin_key_event ev = ch('d');
    ev.language_key = 1;
    ev.mods = HUNMIN_MOD_CTRL; /* AUTOSWITCH's ruled default (Table A) */
    EXPECT_EQ(hunmin_buffered_preedit_special_peek(layer_, &ev), -1) << "test phase agrees with the down";
    EXPECT_EQ(hunmin_buffered_preedit_special(layer_, &ev), -1);
    EXPECT_EQ(performed, 0) << "a refused press runs no platform half";
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(layer_, "autoswitch", &v));
    EXPECT_EQ(v, 0) << "and moves nothing";

    /* A layout capability answer is a different question and stays available:
     * dubeolsik composes Hangul whether or not the dictionary is installed. */
    EXPECT_TRUE(hunmin_ctx_layout_autoswitch_capable(ctx_, 0));
}

TEST_F(AutoswitchDataGateTest, AnInstalledButBrokenDictionaryDoesNotCountAsData) {
    /* Enabling the feature on a list that does not parse would leave it ON and
     * silently never switching -- the confident-zero rule, applied to the gate.
     * A fresh context re-derives the answer (the cache is per context). */
    write_modes("[\"computer\", \"run"); /* truncated JSON: the load answers 0 too */
    hunmin_ctx *broken = fresh_ctx();
    ASSERT_NE(broken, nullptr);
    EXPECT_FALSE(hunmin_ctx_autoswitch_data_present(broken));
    hunmin_ctx_free(broken);

    /* Issue #173's ruling is the other side of the same line: the load SKIPS a
     * damaged ELEMENT and still answers 1, so one bad entry degrades to a
     * smaller dictionary rather than disabling the feature -- the gate must
     * agree with the load, or an install the engine reads fine would be refused
     * by the option. */
    write_modes("[\"computer\", 3, \"\", \"running\"]");
    hunmin_ctx *partial = fresh_ctx();
    ASSERT_NE(partial, nullptr);
    EXPECT_TRUE(hunmin_ctx_autoswitch_data_present(partial))
        << "a file the loader fills is a countable install";
    hunmin_ctx_free(partial);

    /* ...but a file that parses and yields NO word is not: the load answers 1
     * with an empty set, which is exactly the confident-zero state. */
    write_modes("[]");
    hunmin_ctx *empty = fresh_ctx();
    ASSERT_NE(empty, nullptr);
    EXPECT_FALSE(hunmin_ctx_autoswitch_data_present(empty));
    hunmin_ctx_free(empty);
}

/* The other half of the same gate: where the data IS on the search path the
 * option is accepted, so "off" there is off BY CHOICE and the settings row can
 * say so. */
TEST_F(KeyRuleTest, EnablingItWhereTheDataIsPresentIsAccepted) {
    EXPECT_TRUE(hunmin_ctx_autoswitch_data_present(ctx_)) << "libhunmin/data carries modes/en-US.json";
    int v = -1;
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v));
    ASSERT_EQ(v, 0) << "the B3 default before any write (off by choice, data present)";
    EXPECT_TRUE(hunmin_buffered_preedit_set_option(l_, "autoswitch", 1));
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v));
    EXPECT_EQ(v, 1);
}

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

/* ---- the hook is top-level: one rule, every shipped layout (R13, #170) ---- */

TEST_F(KeyRuleTest, TheHookFiresInEveryLayoutTheEngineShips) {
    /* R13's "identical whichever layout it is pressed in"
     * (docs/MODES-ENGINE-RULES.md §3) is a claim about the RULE, not about one
     * layout, so the assertion is a census over the shipped data rather than the
     * dubeolsik the fixture happens to build: every layout the engine can
     * instantiate answers the same two actions for the same event. A new layout
     * file fails here until it is placed, the way
     * HangulValidOldTest.LayoutCapabilityIsDerivedFromTheData does for the
     * validity rule.
     *
     * The reason this can be a census at all is the structural half of the
     * requirement: the key is not a layout row (0 of the 47 keys in every layout
     * are non-printable, tools/keymap-fixture.py), so the rule never asks the
     * data and no layout can spell it differently. */
    const int n = hunmin_ctx_layout_count(ctx_);
    ASSERT_GT(n, 0);

    int driven = 0;
    for (int i = 0; i < n; i++) {
        const char *id = hunmin_ctx_layout_id(ctx_, i);
        ASSERT_NE(id, nullptr) << i;
        if (!hunmin_ctx_layout_supported(ctx_, i)) continue; /* engine half not built: none today */
        hunmin_buffered_preedit *d = hunmin_buffered_preedit_new(ctx_, id, nullptr, HUNMIN_COMMIT_AT_BOUNDARY);
        ASSERT_NE(d, nullptr) << "layout " << id << " is supported but could not be instantiated";

        /* The single press is the language toggle, whatever the layout is. */
        hunmin_key_event single = ch('d');
        single.language_key = 1;
        EXPECT_EQ(hunmin_buffered_preedit_classify(d, &single), HUNMIN_KEY_LANGUAGE_TOGGLE) << id;

        /* The compound press is AUTOSWITCH, and it flips the autoswitch value --
         * NOT the language (the issue's second bullet). */
        hunmin_key_event chord = single;
        chord.mods = HUNMIN_MOD_CTRL; /* the ruled default, 2026-10-31 */
        EXPECT_EQ(hunmin_buffered_preedit_classify(d, &chord), HUNMIN_KEY_LANGUAGE_CHORD) << id;

        hunmin_special_host host{}; /* supports NULL = yes, perform NULL = resolved */
        hunmin_buffered_preedit_special_host(d, &host);
        EXPECT_EQ(hunmin_buffered_preedit_special(d, &chord), HUNMIN_SPECIAL_AUTOSWITCH) << id;
        int on = 7;
        ASSERT_TRUE(hunmin_buffered_preedit_get_option(d, "autoswitch", &on)) << id;
        EXPECT_EQ(on, 1) << id << ": the press flips the state the dialog reads";
        EXPECT_EQ(hunmin_buffered_preedit_language(d), HUNMIN_LANGUAGE_HANGUL)
            << id << ": AUTOSWITCH is not the language toggle";
        ASSERT_NE(hunmin_buffered_preedit_layout_id(d), nullptr);
        EXPECT_STREQ(hunmin_buffered_preedit_layout_id(d), id) << "the press must not switch layouts";

        hunmin_buffered_preedit_free(d);
        driven++;
    }
    /* Not "driven > 0": the point is that no supported layout is skipped. */
    EXPECT_EQ(driven, n) << "driven=" << driven << " of " << n << ": a layout the engine ships was skipped";
}

TEST_F(KeyRuleTest, TheHookIsDecidedBeforeTheBufferIsFed) {
    /* docs/MODES-ENGINE-RULES.md §3: "this hook must be processed before
     * inference -- pressing the hangul/latin key while inference is on is the
     * signal 'do not do this automatically'". Two halves, both measured here.
     *
     * ORDER. With a buffer in flight the press is answered from the EVENT:
     * classify() consults language_key before the feed/boundary branches, and
     * servicing it feeds the buffer nothing.
     *
     * INFERENCE. R8's judgement landed after this issue was written (#164), and
     * before that it ran on the toggle's own boundary: with `autoswitch` on and
     * `hello` in flight, `toggle_language()` delivered the judged Latin reading
     * ("hello") and left `judged = LATIN` for the frontend, so one press was
     * answered by the judgement as well as by its own action. R13 says the press
     * IS the signal not to do that automatically, so it is now a boundary that
     * does not judge (`boundary(l, false)`): the word goes out in the language
     * that was active, and the frontend is handed no judgement. Asserted as an
     * equality against the feature-off run rather than a typed string -- the
     * point is which text the press delivers, not how dubeolsik spells it -- and
     * against a control where a KEY boundary must still judge it, so a
     * dictionary that failed to load cannot make this case pass vacuously. */
    hunmin_key_result out{};
    for (char c : {'d', 'k'}) hunmin_buffered_preedit_process_key(l_, (uint32_t)c, 0, &out);
    ASSERT_STREQ(hunmin_buffered_preedit_composing(l_), "아") << "premise: a syllable is in flight";

    hunmin_key_event chord = ch('d');
    chord.language_key = 1;
    chord.mods = HUNMIN_MOD_CTRL;

    /* The rule reaches the hook although a syllable is composing, and reading it
     * changes nothing: classify() is the classifier, not a feed. */
    EXPECT_EQ(hunmin_buffered_preedit_classify(l_, &chord), HUNMIN_KEY_LANGUAGE_CHORD);
    EXPECT_STREQ(hunmin_buffered_preedit_composing(l_), "아");

    hunmin_special_host host{};
    hunmin_buffered_preedit_special_host(l_, &host);
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &chord), HUNMIN_SPECIAL_AUTOSWITCH);

    /* Nothing was fed and no word was finished: the key is not composition
     * input, and the in-flight syllable is exactly where it was. */
    EXPECT_STREQ(hunmin_buffered_preedit_composing(l_), "아");
    EXPECT_STREQ(hunmin_buffered_preedit_delivered(l_), "") << "the press produced no text";
    EXPECT_STREQ(hunmin_buffered_preedit_latin_text(l_), "");

    /* A two-language layer with the shipped dictionary loaded, because the
     * fixture's is single-language: without a Latin layout there is no reading
     * and the judgement can never fire, which would make the cases below pass
     * for the wrong reason. */
    auto pair = [this](int autoswitch) {
        hunmin_buffered_preedit *p =
            hunmin_buffered_preedit_new(ctx_, "dubeolsik", "us-qwerty", HUNMIN_COMMIT_AT_BOUNDARY);
        if (!p) return (hunmin_buffered_preedit *)nullptr;
        EXPECT_EQ(hunmin_buffered_preedit_load_english_shipped(p), 1) << "the shipped en_US list";
        EXPECT_TRUE(hunmin_buffered_preedit_set_option(p, "autoswitch", autoswitch));
        return p;
    };
    auto type = [](hunmin_buffered_preedit *p, const char *keys) {
        hunmin_key_result r{};
        std::string all;
        for (const char *k = keys; *k; ++k) {
            hunmin_buffered_preedit_process_key(p, (uint32_t)*k, 0, &r);
            if (r.commit) all += r.commit;
            (void)hunmin_buffered_preedit_take_judgement(p); /* the frontend reads per key */
        }
        return all;
    };

    /* CONTROL: the word `hello` is one the judgement decides, and a KEY boundary
     * decides it -- delivered in the judged language, with the answer left for
     * the frontend. Without this line, "no judgement on the press" would also
     * pass on a layer where the judgement never fires at all. */
    hunmin_buffered_preedit *control = pair(1);
    ASSERT_NE(control, nullptr);
    type(control, "hello");
    hunmin_key_result space{};
    hunmin_buffered_preedit_process_key(control, ' ', 0, &space);
    EXPECT_STREQ(space.commit, "hello") << "premise: the judgement fires on a key boundary";
    EXPECT_EQ(hunmin_buffered_preedit_take_judgement(control), HUNMIN_LANGUAGE_LATIN);
    hunmin_buffered_preedit_free(control);

    /* The feature-off run: the text the press delivers when nothing can judge.
     * This is what "committed in the language that was active" means. */
    hunmin_buffered_preedit *off = pair(0);
    ASSERT_NE(off, nullptr);
    type(off, "hello");
    hunmin_key_result off_out{};
    ASSERT_EQ(hunmin_buffered_preedit_toggle_language(off, &off_out), 1);
    const std::string as_typed = off_out.commit ? off_out.commit : "";
    ASSERT_FALSE(as_typed.empty());
    hunmin_buffered_preedit_free(off);

    /* THE PRESS, with the feature on. Same keys, same press: the text is the one
     * typed (not "hello"), and no judgement is left to act on. */
    hunmin_buffered_preedit *on = pair(1);
    ASSERT_NE(on, nullptr);
    type(on, "hello");
    /* A syllable is in flight (the whole word is only visible at the boundary:
     * `composing` is the in-flight syllable, and on this layer `display` shows
     * the judged language per key, E6's default). */
    ASSERT_FALSE(std::string(hunmin_buffered_preedit_composing(on)).empty())
        << "premise: a word is composing";

    hunmin_key_result press{};
    ASSERT_EQ(hunmin_buffered_preedit_toggle_language(on, &press), 1);
    EXPECT_EQ(as_typed, press.commit ? press.commit : "")
        << "the press delivers the word as typed, not the judged reading";
    EXPECT_EQ(hunmin_buffered_preedit_take_judgement(on), -1)
        << "the press leaves no judgement: inference did not decide this key";
    EXPECT_EQ(hunmin_buffered_preedit_language(on), HUNMIN_LANGUAGE_LATIN)
        << "the press's own action stands";
    hunmin_buffered_preedit_free(on);
}


/* ---- R13-정정's one on/off state (issue #165) ---------------------------- */

TEST_F(KeyRuleTest, TheAutoswitchOptionIsOneValueTheLayerReadsBack) {
    /* The requirement is one value the engine holds and a settings surface only
     * reads and writes (docs/SPEC-MODES.md §R13-정정), so the assertion is the
     * round trip: no option call at all answers the B3 default (OFF,
     * 2026-10-11), and every write is what the next read returns. */
    int v = 7;
    EXPECT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v)) << "the query is not a second mechanism";
    EXPECT_EQ(v, 0) << "OFF until the B3 data gate ships (2026-10-11 ruling)";

    ASSERT_TRUE(hunmin_buffered_preedit_set_option(l_, "autoswitch", 1));
    EXPECT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v));
    EXPECT_EQ(v, 1);

    ASSERT_TRUE(hunmin_buffered_preedit_set_option(l_, "autoswitch", 0));
    EXPECT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v));
    EXPECT_EQ(v, 0);

    /* The value contract is the one the layer already has for "caps": any
     * non-zero is ON, and an unknown NAME is the rejection -- false, with the
     * caller's int left exactly as it was. */
    ASSERT_TRUE(hunmin_buffered_preedit_set_option(l_, "autoswitch", 7));
    EXPECT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v));
    EXPECT_EQ(v, 1) << "coerced like every other option";

    v = 42;
    EXPECT_FALSE(hunmin_buffered_preedit_get_option(l_, "no-such-option", &v));
    EXPECT_EQ(v, 42) << "an unknown name must not write through the out parameter";
    EXPECT_FALSE(hunmin_buffered_preedit_set_option(l_, "no-such-option", 1));
    EXPECT_FALSE(hunmin_buffered_preedit_get_option(l_, "autoswitch", nullptr));

    /* The key surface beside the value is the engine's too (R13-정정's
     * "explainable" bullet): the dialog asks, it does not spell a chord. */
    EXPECT_EQ(hunmin_buffered_preedit_special_chord(l_, HUNMIN_SPECIAL_AUTOSWITCH), HUNMIN_MOD_CTRL);
}

TEST_F(KeyRuleTest, Issue207SyllableCommitForcesAutoswitchOffAndNamesTheReason) {
    /* A layer built with HUNMIN_COMMIT_PER_SYLLABLE has no autoswitch: a fresh
     * layer starts OFF, storing ON is still observed as OFF, and the reason
     * accessor names the policy (issue #207, obligation 2 + 3). */
    hunmin_buffered_preedit *syllable = hunmin_buffered_preedit_new(ctx_, "dubeolsik", nullptr, HUNMIN_COMMIT_PER_SYLLABLE);
    ASSERT_NE(syllable, nullptr);

    int v = 7;
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(syllable, "autoswitch", &v));
    EXPECT_EQ(v, 0) << "a fresh per-syllable layer starts OFF";

    ASSERT_TRUE(hunmin_buffered_preedit_set_option(syllable, "autoswitch", 1));
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(syllable, "autoswitch", &v));
    EXPECT_EQ(v, 0) << "stored ON is observed as OFF under syllable commit";

    const char *reason = hunmin_buffered_preedit_autoswitch_reason(syllable);
    ASSERT_NE(reason, nullptr);
    EXPECT_NE(strstr(reason, "syllable"), nullptr) << reason;
    EXPECT_NE(strstr(reason, "HUNMIN_COMMIT_PER_SYLLABLE"), nullptr) << reason;

    /* Boundary policy keeps the normal on/off semantics: no policy reason. */
    EXPECT_EQ(hunmin_buffered_preedit_autoswitch_reason(l_), nullptr);

    hunmin_buffered_preedit_free(syllable);
}

TEST_F(KeyRuleTest, TheAutoswitchStateSurvivesALanguageToggle) {
    /* The layer re-creates its ic on 한/영 (the two languages are two layouts),
     * so a preference stored on the old ic would silently revert to OFF the
     * first time the person switched language -- the "saved, reloaded, did
     * nothing" defect class. Pinned here because no frontend would show it: the
     * value is set once by a settings surface and read later. */
    hunmin_buffered_preedit *l =
        hunmin_buffered_preedit_new(ctx_, "dubeolsik", "us-qwerty", HUNMIN_COMMIT_AT_BOUNDARY);
    ASSERT_NE(l, nullptr);
    ASSERT_TRUE(hunmin_buffered_preedit_set_option(l, "autoswitch", 1));

    hunmin_key_result out{};
    EXPECT_EQ(hunmin_buffered_preedit_toggle_language(l, &out), 1) << "the pair switches";

    int v = -1;
    EXPECT_TRUE(hunmin_buffered_preedit_get_option(l, "autoswitch", &v));
    EXPECT_EQ(v, 1) << "a language toggle is not an option reset";
    EXPECT_EQ(hunmin_buffered_preedit_language(l), HUNMIN_LANGUAGE_LATIN);

    EXPECT_EQ(hunmin_buffered_preedit_toggle_language(l, &out), 1) << "and back";
    EXPECT_TRUE(hunmin_buffered_preedit_get_option(l, "autoswitch", &v));
    EXPECT_EQ(v, 1);

    /* hunmin_ic_reset() drops composition, not a person's preference. */
    hunmin_buffered_preedit_reset(l);
    EXPECT_TRUE(hunmin_buffered_preedit_get_option(l, "autoswitch", &v));
    EXPECT_EQ(v, 1);

    hunmin_buffered_preedit_free(l);
}

TEST_F(KeyRuleTest, TheAutoswitchChordFlipsTheOneValue) {
    /* Ruled 2026-11-01 (issue #165): the engine owns the transition. The chord
     * surface and the settings surface therefore write the same field, and a
     * frontend never has to decide what "toggle" means -- which is the whole
     * point of R13-정정's "one value". */
    hunmin_key_event ev = ch('d');
    ev.language_key = 1;
    ev.mods = HUNMIN_MOD_CTRL; /* AUTOSWITCH's ruled default (Table A) */

    int v = -1;
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v));
    ASSERT_EQ(v, 0) << "the B3 default before any press";

    int performed = 0;
    hunmin_special_host host{};
    host.user = &performed;
    host.perform = [](void *user, hunmin_special_function fn) { *(int *)user = (int)fn; };
    hunmin_buffered_preedit_special_host(l_, &host);

    /* The peek answers without changing anything: a platform test phase must not
     * flip the state before the key it is testing for. */
    EXPECT_EQ(hunmin_buffered_preedit_special_peek(l_, &ev), HUNMIN_SPECIAL_AUTOSWITCH);
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v));
    EXPECT_EQ(v, 0) << "a peek is not a press";

    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), HUNMIN_SPECIAL_AUTOSWITCH);
    EXPECT_EQ(performed, (int)HUNMIN_SPECIAL_AUTOSWITCH) << "the platform half still runs";
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v));
    EXPECT_EQ(v, 1) << "the engine flipped its own value";

    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), HUNMIN_SPECIAL_AUTOSWITCH);
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v));
    EXPECT_EQ(v, 0) << "and back: a flip, not a set";

    /* A refusal moves nothing. The host is withdrawn, so the press is `-1` and
     * the key passes through -- a platform that cannot service the function
     * must not change the state it could not act on. */
    ASSERT_TRUE(hunmin_buffered_preedit_set_option(l_, "autoswitch", 1));
    hunmin_buffered_preedit_special_host(l_, nullptr);
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), -1);
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v));
    EXPECT_EQ(v, 1) << "an unserviced press leaves the value alone";

    /* And the other function's chord touches nothing: only AUTOSWITCH owns this
     * state, so a VOICE press is not a second writer. */
    hunmin_buffered_preedit_special_host(l_, &host);
    ev.mods = HUNMIN_MOD_ALT; /* VOICE */
    EXPECT_EQ(hunmin_buffered_preedit_special(l_, &ev), HUNMIN_SPECIAL_VOICE);
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(l_, "autoswitch", &v));
    EXPECT_EQ(v, 1) << "VOICE is not a writer of the autoswitch value";
}

} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    const char *v = getenv("HUNMIN_DATA_PATH");
    kDataPath = v && *v ? v : "libhunmin/data";
    return RUN_ALL_TESTS();
}
