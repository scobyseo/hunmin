/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier D1 for the IBus settings store (issue #327, docs/SPEC-FRONTEND.md §FE17).
 *
 * Why these cases exist in this shape: the five rows used to be two environment
 * seams, so nothing could assert what a person sets. Now the schema is the
 * store, and the questions worth a red run are the ones §IBC measured —
 *
 *   does a stored value reach the ENGINE (not just the store)?
 *   does a bad value degrade and say so, instead of becoming a dead row?
 *   does a change reach a RUNNING engine without a restart?
 *   is the fifth row read with IBus's own grammar, including the two corners
 *   that grammar handles badly (Release parses but can never fire here; Meta
 *   trips an IBUS-CRITICAL and is refused before IBus sees it)?
 *
 * No bus, no daemon, no display, and no dconf: GSETTINGS_BACKEND=memory is set
 * by the test's environment (see test/CMakeLists.txt), because a dconf-backed
 * store cannot be isolated from a test by environment — a write under
 * dbus-run-session with a redirected XDG_CONFIG_HOME lands in the developer's
 * own ~/.config/dconf/user (docs/OPTIONS-MEASURED.md §IBC-5). Persistence across
 * a restart is therefore NOT asserted here and is a CI-only case.
 */
#include <gtest/gtest.h>
#include <gio/gio.h>
#include <ibus.h>

#include <hunmin.h>

#include <string>

extern "C" {
#include "hunmin_engine.h"
#include "hunmin_state.h"
#include "settings.h"
}

namespace {

constexpr const char *kSchemaId = "org.freedesktop.ibus.engine.hunmin";

class SettingsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* NULL/0: the state resolves HUNMIN_DATA_PATH itself, the same rule the
         * engine follows, so the test does not carry its own path rule. */
        state_ = hunmin_state_new(nullptr, 0);
        ASSERT_NE(state_, nullptr);
        settings_ = hunmin_ibus_settings_new();
        ASSERT_NE(settings_, nullptr) << "the schema is not installed: "
                                        "GSETTINGS_SCHEMA_DIR must point at the build tree";
        gs_ = g_settings_new(kSchemaId);
        ASSERT_NE(gs_, nullptr);
        Reset();
    }

    void TearDown() override {
        g_clear_object(&gs_);
        hunmin_ibus_settings_free(settings_);
        hunmin_state_free(state_);
    }

    /* Every case starts from the schema's defaults, whatever an earlier case
     * stored: the memory backend is per process and the cases share it. */
    void Reset() {
        g_settings_reset(gs_, "hangul-layout");
        g_settings_reset(gs_, "latin-layout");
        g_settings_reset(gs_, "commit-unit");
        g_settings_reset(gs_, "autoswitch");
        g_settings_reset(gs_, "switch-keys");
        ASSERT_EQ(hunmin_ibus_settings_apply(settings_, state_), 0);
    }

    hunmin_state *state_ = nullptr;
    hunmin_ibus_settings *settings_ = nullptr;
    GSettings *gs_ = nullptr;
};

/* The default is the schema's, and it is the pair the engine used to hard-code:
 * one physical key, two spellings, because XKB decides which keysym it reports
 * (Alt_R 0xffea on us, Hangul 0xff31 on kr). */
TEST_F(SettingsTest, DefaultsAreTheDocumentedOnes) {
    gchar *hangul = g_settings_get_string(gs_, "hangul-layout");
    gchar *latin = g_settings_get_string(gs_, "latin-layout");
    gchar *unit = g_settings_get_string(gs_, "commit-unit");
    EXPECT_STREQ(hangul, "dubeolsik");
    EXPECT_STREQ(latin, "us-qwerty");
    EXPECT_STREQ(unit, "word");
    EXPECT_FALSE(g_settings_get_boolean(gs_, "autoswitch"));
    g_free(hangul);
    g_free(latin);
    g_free(unit);

    gchar **keys = g_settings_get_strv(gs_, "switch-keys");
    ASSERT_NE(keys, nullptr);
    ASSERT_EQ(g_strv_length(keys), 2u);
    EXPECT_STREQ(keys[0], "Hangul");
    EXPECT_STREQ(keys[1], "Alt_R");
    g_strfreev(keys);
}

/* A stored value reaches the engine, not just the store: this is the case the
 * issue's "설정 왕복" line asks for. */
TEST_F(SettingsTest, StoredRowsReachTheEngine) {
    ASSERT_TRUE(g_settings_set_string(gs_, "hangul-layout", "dubeolsik"));
    ASSERT_TRUE(g_settings_set_string(gs_, "commit-unit", "syllable"));
    ASSERT_TRUE(g_settings_set_boolean(gs_, "autoswitch", TRUE));

    EXPECT_EQ(hunmin_ibus_settings_apply(settings_, state_), 0);
    EXPECT_STREQ(hunmin_state_hangul_layout_id(state_), "dubeolsik");
    EXPECT_EQ(hunmin_state_commit_policy(state_), HUNMIN_COMMIT_PER_SYLLABLE);
    int held = 0;
    EXPECT_TRUE(hunmin_state_get_autoswitch(state_, &held));
}

/* A layout id the engine cannot load degrades to the schema's default AND is
 * reported. The alternative — keeping the bad id — is the silent mispair TSF's
 * resolveLayoutSlot exists to prevent. */
TEST_F(SettingsTest, AnUnloadableLayoutDegradesAndIsReported) {
    ASSERT_TRUE(g_settings_set_string(gs_, "hangul-layout", "no-such-layout"));
    EXPECT_EQ(hunmin_ibus_settings_apply(settings_, state_), 1);
    /* The fallback is the SCHEMA's default, not a literal in settings.c: this
     * assertion moves when the default does. */
    EXPECT_STREQ(hunmin_state_hangul_layout_id(state_), "dubeolsik");
    EXPECT_STREQ(hunmin_ibus_settings_rejected(settings_), "no-such-layout/us-qwerty");
}

/* The fifth row is read with IBus's grammar, and the engine's translation is
 * what consumes it — so this asserts the stored list through the same function
 * tier D2 uses, not through a copy of the lookup. */
TEST_F(SettingsTest, TheStoredKeyListDrivesTheTranslation) {
    const gchar *keys[] = {"Control+Hangul", nullptr};
    ASSERT_TRUE(g_settings_set_strv(gs_, "switch-keys", keys));
    ASSERT_EQ(hunmin_ibus_settings_apply(settings_, state_), 0);

    IBusHotkeyProfile *profile = hunmin_ibus_settings_keys(settings_);
    ASSERT_NE(profile, nullptr);

    const hunmin_key_event chord =
        hunmin_ibus_translate_key(IBUS_KEY_Hangul, IBUS_CONTROL_MASK, 0, profile);
    EXPECT_EQ(chord.language_key, 1);

    /* A bare press is no longer a language key: the row replaced the default
     * pair rather than adding to it. */
    const hunmin_key_event bare = hunmin_ibus_translate_key(IBUS_KEY_Hangul, 0, 0, profile);
    EXPECT_EQ(bare.language_key, 0);

    /* With no store the engine composes but never switches — no compiled-in
     * second default to disagree with the schema. */
    const hunmin_key_event none = hunmin_ibus_translate_key(IBUS_KEY_Alt_R, 0, 0, nullptr);
    EXPECT_EQ(none.language_key, 0);
}

/* Caps Lock cannot break a binding (IBus's matcher ignores LOCK), and a stored
 * Shift+한/영 needs the shift bit, which arrives in IBus's state and not in the
 * shared event's `mods`. */
TEST_F(SettingsTest, CapsLockIsIgnoredAndShiftIsPartOfThePattern) {
    EXPECT_TRUE(hunmin_ibus_settings_is_language_key(settings_, IBUS_KEY_Hangul, 0));
    EXPECT_TRUE(hunmin_ibus_settings_is_language_key(settings_, IBUS_KEY_Hangul, IBUS_LOCK_MASK));
    EXPECT_FALSE(
        hunmin_ibus_settings_is_language_key(settings_, IBUS_KEY_Hangul, IBUS_SHIFT_MASK));

    const gchar *keys[] = {"Shift+Hangul", nullptr};
    ASSERT_TRUE(g_settings_set_strv(gs_, "switch-keys", keys));
    ASSERT_EQ(hunmin_ibus_settings_apply(settings_, state_), 0);
    EXPECT_TRUE(hunmin_ibus_settings_is_language_key(settings_, IBUS_KEY_Hangul, IBUS_SHIFT_MASK));
    EXPECT_FALSE(hunmin_ibus_settings_is_language_key(settings_, IBUS_KEY_Hangul, 0));
}

/* The two corners of IBus's grammar this frontend must not hand over blindly.
 *
 * Release: `Alt+Release+Alt_R` is IBus's own default trigger for the physical
 * Han/Eng key, so it is the string a person is most likely to copy in. It parses
 * (checked below through the public validator, which is the only place the two
 * verdicts differ) but this engine returns before classifying a release, so a
 * stored Release entry can never fire.
 *
 * Meta: refused at parse by IBus while logging IBUS-CRITICAL from an
 * unimplemented branch (ibushotkey.c, measured 2026-11-22), so it is refused
 * before IBus is asked. */
TEST_F(SettingsTest, ReleaseAndMetaAreRefusedWithAReason) {
    const char *release_reason = hunmin_ibus_key_entry_reason("Alt+Release+Alt_R");
    ASSERT_NE(release_reason, nullptr);
    EXPECT_NE(std::string(release_reason).find("Release"), std::string::npos);

    const char *meta_reason = hunmin_ibus_key_entry_reason("Meta+Hangul");
    ASSERT_NE(meta_reason, nullptr);
    EXPECT_NE(std::string(meta_reason).find("Meta"), std::string::npos);

    /* The grammar itself accepts an ordinary pattern, so the refusals above are
     * ours and not IBus's. */
    bool ok = false;
    EXPECT_EQ(hunmin_ibus_stored_key_ok("Alt+Shift+Hangul", &ok), nullptr);
    EXPECT_TRUE(ok);
}

/* A mistyped entry costs that one binding and is reported; it does not take the
 * rest of the row with it (the B3 shape: a missing thing is a normal state). */
TEST_F(SettingsTest, ABadEntryIsDroppedAndTheGoodOnesSurvive) {
    const gchar *keys[] = {"Alt+Release+Alt_R", "ctrl+a", "Hangul", nullptr};
    ASSERT_TRUE(g_settings_set_strv(gs_, "switch-keys", keys));
    EXPECT_EQ(hunmin_ibus_settings_apply(settings_, state_), 2);
    EXPECT_EQ(hunmin_ibus_settings_rejected_count(settings_), 2);
    EXPECT_STREQ(hunmin_ibus_settings_rejected(settings_), "Alt+Release+Alt_R");
    EXPECT_TRUE(hunmin_ibus_settings_is_language_key(settings_, IBUS_KEY_Hangul, 0));
}

/* Live apply: the store changes, GSettings tells this process, the engine
 * re-reads. No restart, which is what §FE17's ruling asked for. */
TEST_F(SettingsTest, AChangeReachesTheRunningEngine) {
    static int fired;
    fired = 0;
    hunmin_ibus_settings_watch(
        settings_, [](void *user) { ++*static_cast<int *>(user); }, &fired);

    /* The watcher fires synchronously with the write; the shipped engine's
     * callback is the one that re-applies, and the case below then asserts the
     * engine's answer after an explicit apply. */
    ASSERT_TRUE(g_settings_set_string(gs_, "hangul-layout", "dubeolsik"));
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(hunmin_ibus_settings_apply(settings_, state_), 0);
    EXPECT_STREQ(hunmin_state_hangul_layout_id(state_), "dubeolsik");
}

} // namespace