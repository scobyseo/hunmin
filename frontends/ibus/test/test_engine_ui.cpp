/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier D2 for the IBus frontend (issue #288): what the engine publishes to the
 * host's UI.
 *
 * D1 asks the state object; this lane asks the real IBusEngine object through
 * the host's own transport. A private session bus (GTestDBus) takes the place
 * of the daemon, so every assertion here reads a D-Bus signal that a panel
 * would have received: CommitText, UpdatePreeditText and UpdateLookupTable,
 * deserialized with libibus's own API rather than by re-parsing our own shape.
 *
 * What this does NOT prove, and D3 (#292) is where it is measured: that a
 * daemon spawns the engine, that a client's text view receives the commit, and
 * that a panel draws the table at this page size. No display is touched here.
 *
 * The bus is private and torn down per test; GTestDBus needs dbus-daemon on
 * PATH (present on a normal Ubuntu install and on the CI runner).
 */
#include <gtest/gtest.h>

#include <ibus.h>

#include <algorithm>
#include <clocale>
#include <initializer_list>
#include <string>
#include <vector>

extern "C" {
#include "hunmin_engine.h"
#include "hunmin_state.h"
}

namespace {

struct PreeditEvent {
    std::string text;
    guint cursor = 0;
    bool visible = false;
};

struct TableEvent {
    bool visible = false;
    guint page_size = 0;
    guint cursor = 0;
    std::vector<std::string> candidates;
};

struct Captured {
    std::vector<std::string> commits;
    std::vector<PreeditEvent> preedits;
    std::vector<TableEvent> tables;
    std::vector<PreeditEvent> aux; /* FE16: auxiliary text events, same shape */
    int shows = 0;
    int hides = 0;
};

void on_signal(GDBusConnection *, const gchar *, const gchar *, const gchar *, const gchar *member,
               GVariant *params, gpointer user) {
    auto *cap = static_cast<Captured *>(user);
    if (g_strcmp0(member, "CommitText") == 0) {
        GVariant *serialized = nullptr;
        g_variant_get(params, "(v)", &serialized);
        IBusSerializable *object = ibus_serializable_deserialize_object(serialized);
        if (object && IBUS_IS_TEXT(object)) cap->commits.emplace_back(IBUS_TEXT(object)->text);
        if (object) g_object_unref(object);
        g_variant_unref(serialized);
    } else if (g_strcmp0(member, "UpdatePreeditText") == 0) {
        GVariant *serialized = nullptr;
        guint cursor = 0;
        gboolean visible = FALSE;
        g_variant_get(params, "(vubu)", &serialized, &cursor, &visible, nullptr);
        IBusSerializable *object = ibus_serializable_deserialize_object(serialized);
        PreeditEvent ev;
        if (object && IBUS_IS_TEXT(object)) ev.text = IBUS_TEXT(object)->text;
        ev.cursor = cursor;
        ev.visible = visible != FALSE;
        cap->preedits.push_back(ev);
        if (object) g_object_unref(object);
        g_variant_unref(serialized);
    } else if (g_strcmp0(member, "UpdateLookupTable") == 0) {
        GVariant *serialized = nullptr;
        gboolean visible = FALSE;
        g_variant_get(params, "(vb)", &serialized, &visible);
        IBusSerializable *object = ibus_serializable_deserialize_object(serialized);
        TableEvent ev;
        ev.visible = visible != FALSE;
        if (object && IBUS_IS_LOOKUP_TABLE(object)) {
            auto *table = IBUS_LOOKUP_TABLE(object);
            ev.page_size = ibus_lookup_table_get_page_size(table);
            ev.cursor = ibus_lookup_table_get_cursor_pos(table);
            const guint n = ibus_lookup_table_get_number_of_candidates(table);
            for (guint i = 0; i < n; ++i) {
                IBusText *candidate = ibus_lookup_table_get_candidate(table, i);
                ev.candidates.emplace_back(candidate ? candidate->text : "");
            }
        }
        cap->tables.push_back(ev);
        if (object) g_object_unref(object);
        g_variant_unref(serialized);
    } else if (g_strcmp0(member, "ShowLookupTable") == 0) {
        ++cap->shows;
    } else if (g_strcmp0(member, "HideLookupTable") == 0) {
        ++cap->hides;
    } else if (g_strcmp0(member, "UpdateAuxiliaryText") == 0) {
        GVariant *serialized = nullptr;
        gboolean visible = FALSE;
        g_variant_get(params, "(vb)", &serialized, &visible);
        IBusSerializable *object = ibus_serializable_deserialize_object(serialized);
        PreeditEvent ev;
        if (object && IBUS_IS_TEXT(object)) ev.text = IBUS_TEXT(object)->text;
        ev.visible = visible != FALSE;
        cap->aux.push_back(ev);
        if (object) g_object_unref(object);
        g_variant_unref(serialized);
    }
}

class IbusUiTest : public ::testing::Test {
protected:
    void SetUp() override {
        setlocale(LC_ALL, "");
        dbus_ = g_test_dbus_new(G_TEST_DBUS_NONE);
        g_test_dbus_up(dbus_);
        ibus_init();
        GError *error = nullptr;
        connection_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if (!connection_) {
            g_test_dbus_down(dbus_);
            g_clear_object(&dbus_);
            GTEST_SKIP() << "no private session bus: " << (error ? error->message : "?");
        }
        engine_ = ibus_engine_new_with_type(HUNMIN_TYPE_IBUS_ENGINE, "hunmin",
                                            "/org/freedesktop/IBus/Engine/hunmin", connection_);
        ASSERT_NE(engine_, nullptr);
        ASSERT_NE(hunmin_ibus_engine_state(HUNMIN_IBUS_ENGINE(engine_)), nullptr)
            << "the engine could not load layout data (HUNMIN_DATA_PATH)";
        subscription_ = g_dbus_connection_signal_subscribe(
            connection_, nullptr, "org.freedesktop.IBus.Engine", nullptr,
            "/org/freedesktop/IBus/Engine/hunmin", nullptr, G_DBUS_SIGNAL_FLAGS_NONE, on_signal,
            &captured_, nullptr);
        Pump();
    }

    void TearDown() override {
        if (subscription_) g_dbus_connection_signal_unsubscribe(connection_, subscription_);
        if (engine_) g_object_unref(engine_);
        if (connection_) g_object_unref(connection_);
        if (dbus_) {
            g_test_dbus_down(dbus_);
            g_object_unref(dbus_);
        }
    }

    /* A key as IBus delivers it: keyval, keycode, modifier state. Waits for the
     * signals it produced instead of sleeping a fixed time -- ~1 ms on a
     * private bus, and the D2 lane has to stay inside the 30 s inner loop. */
    gboolean Key(guint keyval, guint state = 0) {
        const size_t before = Events();
        last_used_ = IBUS_ENGINE_GET_CLASS(engine_)->process_key_event(engine_, keyval, 0, state);
        gint64 now = g_get_monotonic_time();
        const gint64 deadline = now + 2 * G_TIME_SPAN_SECOND; /* miss, not cost */
        gint64 quiet_at = 0;
        while (now < deadline) {
            const gboolean worked = g_main_context_iteration(nullptr, FALSE);
            now = g_get_monotonic_time();
            if (Events() != before && quiet_at == 0) quiet_at = now + 2 * G_TIME_SPAN_MILLISECOND;
            /* One signal in, and nothing further pending after a short quiet
             * window: this key has published everything it is going to. A
             * fixed sleep here was measured flaky under -j16, where the
             * scheduler, not the bus, is the slow part. */
            if (quiet_at != 0 && now >= quiet_at && !g_main_context_pending(nullptr)) break;
            if (!worked) g_usleep(200);
        }
        return last_used_;
    }

    void Type(const char *keys) {
        for (const char *p = keys; *p; ++p) Key(static_cast<unsigned char>(*p));
    }

    size_t Events() const {
        return captured_.commits.size() + captured_.preedits.size() + captured_.tables.size() +
               captured_.aux.size() + static_cast<size_t>(captured_.shows) +
               static_cast<size_t>(captured_.hides);
    }

    /* Setup-time drain: no key has been sent, so wait for whatever the engine
     * published while initialising (today: nothing). */
    void Pump() {
        const gint64 end = g_get_monotonic_time() + 50 * 1000;
        while (g_get_monotonic_time() < end) {
            g_main_context_iteration(nullptr, FALSE);
            g_usleep(500);
        }
    }

    GTestDBus *dbus_ = nullptr;
    GDBusConnection *connection_ = nullptr;
    IBusEngine *engine_ = nullptr;
    guint subscription_ = 0;
    Captured captured_;
    gboolean last_used_ = FALSE;
};

/* The preedit is the state's display and the cursor is a CHARACTER index; both
 * have to survive the trip through IBus, whose cursor_pos is character-based
 * (a byte count would be 3 for 「ㄱ」 and the panel would place the caret in the
 * wrong cell). */
TEST_F(IbusUiTest, PreeditIsPublishedWithACursorInCharacters) {
    EXPECT_TRUE(Key('r'));
    ASSERT_FALSE(captured_.preedits.empty());
    EXPECT_EQ(captured_.preedits.back().text, "ㄱ");
    EXPECT_EQ(captured_.preedits.back().cursor, 1u);
    EXPECT_TRUE(captured_.preedits.back().visible);

    EXPECT_TRUE(Key('k'));
    EXPECT_EQ(captured_.preedits.back().text, "가");
    EXPECT_EQ(captured_.preedits.back().cursor, 1u);

    captured_.preedits.clear();
    /* Start a clean word: the composition above is still in flight, and this
     * lane asserts the display of one word, not of a concatenation. */
    Key(IBUS_KEY_space);
    captured_.commits.clear();
    captured_.preedits.clear();
    Type("dkssud");
    ASSERT_FALSE(captured_.preedits.empty());
    EXPECT_EQ(captured_.preedits.back().text, "안녕");
    EXPECT_EQ(captured_.preedits.back().cursor, 2u) << "two characters, six bytes";
    EXPECT_TRUE(captured_.preedits.back().visible);
}

TEST_F(IbusUiTest, WordIsCommittedAtTheBoundaryAndThePreeditClears) {
    Type("rk");
    captured_.commits.clear();
    EXPECT_FALSE(Key(' ')) << "space is the client's key, not the engine's";
    ASSERT_FALSE(captured_.commits.empty());
    EXPECT_EQ(captured_.commits.back(), "가");
    ASSERT_FALSE(captured_.preedits.empty());
    EXPECT_EQ(captured_.preedits.back().text, "");
    EXPECT_FALSE(captured_.preedits.back().visible) << "the slot is cleared, not left stale";
}

/* FE13 on this platform: IBus delivers the Han/Eng keyval, so the engine owns
 * the trigger and answers it. The word in flight is delivered on the way. */
TEST_F(IbusUiTest, LanguageKeyvalSwitchesThroughTheHost) {
    Type("rk");
    captured_.commits.clear();
    EXPECT_TRUE(Key(IBUS_KEY_Hangul));
    ASSERT_FALSE(captured_.commits.empty());
    EXPECT_EQ(captured_.commits.back(), "가");
    captured_.commits.clear();
    EXPECT_TRUE(Key('r'));
    ASSERT_FALSE(captured_.preedits.empty());
    EXPECT_EQ(captured_.preedits.back().text, "r")
        << "the Latin half is still the layer's word: delivered at its boundary";
    captured_.commits.clear();
    EXPECT_FALSE(Key(IBUS_KEY_space)) << "the boundary key itself is the client's";
    ASSERT_FALSE(captured_.commits.empty());
    EXPECT_EQ(captured_.commits.back(), "r");
}

/* The lookup table is the host's candidate window: these are the fields a
 * panel pages and draws. 64 entries against a page of 9 is the pagination
 * path, not a one-page list. */
/* Regression for the HUNMIN_IBUS_LAYOUT init seam (2026-11-22, issue #292):
 * changing ONLY the Hangul layout must keep the Latin half. Passing NULL there
 * makes the layer single-language and the 한/영 key is silently declined while
 * flushing the word. The real-use trace log caught it; this pins it. */
TEST_F(IbusUiTest, LayoutOverrideKeepsTheLanguageKeyAlive) {
    hunmin_state *state = hunmin_ibus_engine_state(HUNMIN_IBUS_ENGINE(engine_));
    // Copy: set_layouts frees the id strings, so a borrowed pointer dangles.
    const std::string hangul = hunmin_state_hangul_layout_id(state);
    ASSERT_FALSE(hangul.empty());
    // NULL latin is the engine's D3 seam call: it must KEEP the Latin half.
    ASSERT_TRUE(hunmin_state_set_layouts(state, hangul.c_str(), NULL));
    ASSERT_EQ(hangul, hunmin_state_hangul_layout_id(state));

    /* Both spellings must work: Hangul 0xff31 on a Korean XKB layout, and
     * Alt_R 0xffea on "us", where the physical 한/영 key reports that instead.
     * Toggle back between the two halves so each starts from Hangul. */
    for (guint keyval : {IBUS_KEY_Hangul, IBUS_KEY_Alt_R}) {
        Type("rk");
        captured_.commits.clear();
        EXPECT_TRUE(Key(keyval))
            << "language keyval 0x" << std::hex << keyval << " was declined";
        ASSERT_FALSE(captured_.commits.empty());
        EXPECT_EQ(captured_.commits.back(), "가");
        EXPECT_EQ(hunmin_state_language(state), HUNMIN_LANGUAGE_LATIN);
        // Return to Hangul for the next spelling without another key dependency.
        captured_.commits.clear();
        EXPECT_TRUE(Key(keyval));
        ASSERT_EQ(hunmin_state_language(state), HUNMIN_LANGUAGE_HANGUL);
    }
}

TEST_F(IbusUiTest, LookupTableIsPublishedForTheHanjaList) {
    Type("rk");
    captured_.tables.clear();
    EXPECT_TRUE(Key(IBUS_KEY_Hangul_Hanja));
    ASSERT_FALSE(captured_.tables.empty());
    const TableEvent &table = captured_.tables.back();
    EXPECT_TRUE(table.visible);
    EXPECT_EQ(table.page_size, 9u);
    EXPECT_EQ(table.page_size, std::min<guint>(9u, table.candidates.size()))
        << "the page is the host ceiling, and a shorter list keeps its own size";
    ASSERT_GT(table.candidates.size(), 9u) << "the case must exercise pagination";
    EXPECT_EQ(table.candidates[0], "可");
    EXPECT_EQ(table.cursor, 0u);
    EXPECT_GT(captured_.shows, 0);

    EXPECT_TRUE(Key(IBUS_KEY_Escape));
    EXPECT_GT(captured_.hides, 0) << "closing hides the host window";
    ASSERT_FALSE(captured_.tables.empty());
    /* Escape keeps the composition (D1 asserts the text); the window is gone. */
    ASSERT_FALSE(captured_.preedits.empty());
    EXPECT_EQ(captured_.preedits.back().text, "가");
}

TEST_F(IbusUiTest, ReturnCommitsTheHighlightedCandidate) {
    Type("rk");
    ASSERT_TRUE(Key(IBUS_KEY_Hangul_Hanja));
    ASSERT_FALSE(captured_.tables.empty());
    const std::string second = captured_.tables.back().candidates.at(1);

    EXPECT_TRUE(Key(IBUS_KEY_Down));
    ASSERT_FALSE(captured_.tables.empty());
    EXPECT_EQ(captured_.tables.back().cursor, 1u) << "the panel is told where Return will land";

    captured_.commits.clear();
    EXPECT_TRUE(Key(IBUS_KEY_Return));
    ASSERT_FALSE(captured_.commits.empty());
    EXPECT_EQ(captured_.commits.back(), second);
    ASSERT_FALSE(captured_.preedits.empty());
    EXPECT_EQ(captured_.preedits.back().text, "");
    EXPECT_GT(captured_.hides, 0);
}

/* Half the frontend contract is the declined key: it must reach the client.
 * A lane that only asserts commits never sees this. */
TEST_F(IbusUiTest, DeclinedKeyReturnsFalseAndPublishesNothing) {
    captured_.commits.clear();
    EXPECT_FALSE(Key(IBUS_KEY_Return)) << "no composition, no candidate list: the client's key";
    EXPECT_TRUE(captured_.commits.empty());
    EXPECT_FALSE(Key(' '));
    EXPECT_TRUE(captured_.commits.empty());
}

TEST_F(IbusUiTest, CandidateKeyWithoutCandidatesReachesTheClient) {
    EXPECT_FALSE(Key(IBUS_KEY_Hangul_Hanja));
    EXPECT_EQ(captured_.shows, 0);
}

/* The translation itself, asserted where it lives: one rule for Caps Lock,
 * Shift and the keys that carry no character. */
TEST_F(IbusUiTest, KeyTranslationKeepsTheLayoutRowAndTheCapsInversionApart) {
    hunmin_key_event shifted = hunmin_ibus_translate_key('P', IBUS_SHIFT_MASK, 0);
    EXPECT_EQ(shifted.base, static_cast<uint32_t>('p'));
    EXPECT_EQ(shifted.shift, 1u) << "case selects the row; Shift is not a chord bit";
    EXPECT_EQ(shifted.mods, 0u);

    hunmin_key_event caps = hunmin_ibus_translate_key('P', IBUS_LOCK_MASK, 0);
    EXPECT_EQ(caps.base, static_cast<uint32_t>('p'));
    EXPECT_EQ(caps.shift, 0u) << "Caps Lock is the layout's `caps` variable, not a shifted row";
    EXPECT_EQ(caps.mods, 0u);

    hunmin_key_event backspace = hunmin_ibus_translate_key(IBUS_KEY_BackSpace, 0, 0);
    EXPECT_EQ(backspace.id, HUNMIN_KEY_TEXT);
    EXPECT_EQ(backspace.base, 0x08u);

    hunmin_key_event ctrl_a = hunmin_ibus_translate_key('a', IBUS_CONTROL_MASK, 0);
    EXPECT_EQ(ctrl_a.mods, HUNMIN_MOD_CTRL);
    EXPECT_EQ(ctrl_a.base, static_cast<uint32_t>('a'));

    hunmin_key_event hangul = hunmin_ibus_translate_key(IBUS_KEY_Hangul, 0, 0);
    EXPECT_EQ(hangul.language_key, 1);
}

/* FE16, option A (issue #319, maintainer ruling 2026-11-22): the highlighted
 * candidate's gloss is published as auxiliary text, tracks the cursor, and
 * clears with the list. The expectation is the STATE's own detail accessor,
 * not a hardcoded string -- one source for the gloss, the signal is what is
 * asserted. The candidate text itself must stay bare Hanja: option A's whole
 * point is that the shared candidate expectations do not move. */
TEST_F(IbusUiTest, CandidateGlossRidesTheAuxiliaryTextAndTracksTheCursor) {
    hunmin_state *state = hunmin_ibus_engine_state(HUNMIN_IBUS_ENGINE(engine_));
    Type("rk");
    captured_.aux.clear();
    ASSERT_TRUE(Key(IBUS_KEY_Hangul_Hanja));
    ASSERT_FALSE(captured_.aux.empty()) << "opening the list published no auxiliary text";
    const std::string gloss0 = hunmin_state_candidate_detail(state, 0);
    ASSERT_FALSE(gloss0.empty()) << "가's first candidate carries a gloss in the data";
    EXPECT_EQ(captured_.aux.back().text, gloss0);
    EXPECT_TRUE(captured_.aux.back().visible);
    /* The candidate text is unchanged by the gloss presentation. */
    ASSERT_FALSE(captured_.tables.empty());
    EXPECT_EQ(captured_.tables.back().candidates[0], "可");

    EXPECT_TRUE(Key(IBUS_KEY_Down));
    const std::string gloss1 = hunmin_state_candidate_detail(state, 1);
    ASSERT_FALSE(captured_.aux.empty());
    EXPECT_EQ(captured_.aux.back().text, gloss1) << "the gloss must track the cursor";
    EXPECT_NE(gloss1, gloss0) << "the case must distinguish the two candidates' glosses";

    /* Escape closes the list: the gloss goes with it. */
    EXPECT_TRUE(Key(IBUS_KEY_Escape));
    ASSERT_FALSE(captured_.aux.empty());
    EXPECT_FALSE(captured_.aux.back().visible);
    EXPECT_EQ(captured_.aux.back().text, "");

    /* Re-open and commit: the gloss clears on the commit path too. */
    ASSERT_TRUE(Key(IBUS_KEY_Hangul_Hanja));
    EXPECT_TRUE(Key(IBUS_KEY_Return));
    ASSERT_FALSE(captured_.aux.empty());
    EXPECT_FALSE(captured_.aux.back().visible);
}


/* The Hanja trigger's platform spellings (issue #319, maintainer instruction
 * 2026-11-22): F9 is the catalogue key, Hangul_Hanja the standard XKB Hanja
 * key (ibus-hangul's default), Control_R what the maintainer's physical Hanja
 * key reports. Each opens the list over a composing word, toggles it closed,
 * and with nothing composing reaches the client. */
TEST_F(IbusUiTest, HanjaTriggerSpellingsOpenAndToggleTheList) {
    for (guint keyval : {IBUS_KEY_Hangul_Hanja, IBUS_KEY_Control_R}) {
        Type("rk");
        captured_.tables.clear();
        EXPECT_TRUE(Key(keyval)) << "trigger 0x" << std::hex << keyval << " declined";
        ASSERT_FALSE(captured_.tables.empty());
        EXPECT_GT(captured_.tables.back().candidates.size(), 0u);
        EXPECT_GT(captured_.shows, 0);
        /* Second press toggles closed, composition kept. */
        EXPECT_TRUE(Key(keyval));
        EXPECT_GT(captured_.hides, 0);
        ASSERT_FALSE(captured_.preedits.empty());
        EXPECT_EQ(captured_.preedits.back().text, "가");
        /* Clean up for the next spelling. */
        Key(IBUS_KEY_space);
        captured_.commits.clear();
        captured_.preedits.clear();
    }
    /* No composition: the trigger is the client's key. */
    EXPECT_FALSE(Key(IBUS_KEY_Hangul_Hanja));
    EXPECT_FALSE(Key(IBUS_KEY_Control_R));
    /* F9 is EXCLUDED (maintainer, 2026-11-22): even over a composing word it
     * is the client's key -- applications map it to their own functions. */
    Type("rk");
    const int shows_before = captured_.shows;
    EXPECT_FALSE(Key(IBUS_KEY_F9)) << "F9 must reach the client on this frontend";
    EXPECT_EQ(captured_.shows, shows_before) << "F9 opened the list";
    Key(IBUS_KEY_space);
}

}  // namespace
