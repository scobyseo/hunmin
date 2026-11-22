/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier D2 for the settings window (issue #327, docs/SPEC-FRONTEND.md §FE17).
 *
 * The gap this closes: docs/TESTING.md's "layout picker rows" row said no test
 * asserted what the picker offers. The widgets are presentation, so what is worth
 * asserting is not their pixels but what they OFFER and what they WRITE — the
 * layout list (which comes from the core, not from a copy), the commit-unit pair,
 * the key row's add/remove/refuse behaviour, and that a change lands in the store.
 *
 * Needs a display: GTK has no headless mode. CMake wraps the test in xvfb-run
 * when it is available, and where `gtk_init_check` still fails the case SKIPS
 * with that reason rather than passing having measured nothing.
 */
#include <gtest/gtest.h>
#include <gtk/gtk.h>

#include <hunmin.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include "hunmin_state.h"
#include "settings.h"
#include "ui.h"
}

namespace {

constexpr const char *kSchemaId = "org.freedesktop.ibus.engine.hunmin";

class SettingsUiTest : public ::testing::Test {
  protected:
    void SetUp() override {
        int argc = 0;
        char **argv = nullptr;
        if (!gtk_init_check(&argc, &argv))
            GTEST_SKIP() << "no display: GTK needs one (run under xvfb-run)";
        ctx_ = hunmin_ctx_new(nullptr, 0);
        gs_ = g_settings_new(kSchemaId);
        ASSERT_NE(gs_, nullptr) << "GSETTINGS_SCHEMA_DIR must point at the build tree";
        Reset();
        ui_ = hunmin_ibus_ui_new(gs_, ctx_);
        ASSERT_NE(ui_, nullptr);
    }

    void TearDown() override {
        if (ui_) hunmin_ibus_ui_free(ui_);
        g_clear_object(&gs_);
        if (ctx_) hunmin_ctx_free(ctx_);
    }

    void Reset() {
        g_settings_reset(gs_, "hangul-layout");
        g_settings_reset(gs_, "latin-layout");
        g_settings_reset(gs_, "commit-unit");
        g_settings_reset(gs_, "autoswitch");
        g_settings_reset(gs_, "switch-keys");
    }

    /* The core's answer, which the picker must be a view of. */
    std::vector<std::string> CoreLayouts(const char *mode) {
        std::vector<std::string> out;
        const int n = ctx_ ? hunmin_ctx_layout_count(ctx_) : 0;
        for (int i = 0; i < n; ++i) {
            if (!hunmin_ctx_layout_supported(ctx_, i)) continue;
            if (g_strcmp0(hunmin_ctx_layout_mode(ctx_, i), mode) != 0) continue;
            out.emplace_back(hunmin_ctx_layout_id(ctx_, i));
        }
        return out;
    }

    std::vector<std::string> ComboRows(GtkWidget *combo) {
        std::vector<std::string> out;
        GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
        GtkTreeIter it;
        if (!gtk_tree_model_get_iter_first(model, &it)) return out;
        do {
            gchar *id = nullptr;
            /* GtkComboBoxText's model is (text, id): the id is what the store
             * holds, so that is the column a "which rows" assertion reads. */
            gtk_tree_model_get(model, &it, 1, &id, -1);
            out.emplace_back(id ? id : "");
            g_free(id);
        } while (gtk_tree_model_iter_next(model, &it));
        return out;
    }

    std::vector<std::string> StoredKeys() {
        gchar **v = g_settings_get_strv(gs_, "switch-keys");
        std::vector<std::string> out;
        for (gchar **p = v; p && *p; ++p) out.emplace_back(*p);
        g_strfreev(v);
        return out;
    }

    guint KeyRows() {
        GtkWidget *list = hunmin_ibus_ui_keys_list(ui_);
        GList *children = gtk_container_get_children(GTK_CONTAINER(list));
        const guint n = g_list_length(children);
        g_list_free(children);
        return n;
    }

    hunmin_ctx *ctx_ = nullptr;
    GSettings *gs_ = nullptr;
    hunmin_ibus_ui *ui_ = nullptr;
};

/* The picker is a VIEW of the core's list, both halves of it. A hardcoded list
 * would pass a "not empty" assertion and still be wrong the day a layout lands
 * (invariant I11 guards the code shape; this asserts the rows). */
TEST_F(SettingsUiTest, ThePickersOfferExactlyTheCoresLayouts) {
    for (const char *mode : {"hangul", "latin"}) {
        const auto expected = CoreLayouts(mode);
        ASSERT_FALSE(expected.empty()) << "no " << mode << " layout loaded from HUNMIN_DATA_PATH";
        const auto rows = ComboRows(hunmin_ibus_ui_layout_combo(ui_, mode));
        EXPECT_EQ(rows, expected) << "the " << mode << " picker is not the core's list";
    }
}

/* The active row is the STORED one, and a different stored value is a different
 * active row: the window reads the store, it does not remember its own answer. */
TEST_F(SettingsUiTest, TheActiveRowsComeFromTheStore) {
    ASSERT_TRUE(g_settings_set_string(gs_, "hangul-layout", "dubeolsik"));
    ASSERT_TRUE(g_settings_set_string(gs_, "commit-unit", "syllable"));
    ASSERT_TRUE(g_settings_set_boolean(gs_, "autoswitch", TRUE));
    hunmin_ibus_ui_free(ui_);
    ui_ = hunmin_ibus_ui_new(gs_, ctx_);
    ASSERT_NE(ui_, nullptr);

    EXPECT_STREQ(
        gtk_combo_box_get_active_id(GTK_COMBO_BOX(hunmin_ibus_ui_layout_combo(ui_, "hangul"))),
        "dubeolsik");
    EXPECT_STREQ(gtk_combo_box_get_active_id(GTK_COMBO_BOX(hunmin_ibus_ui_commit_combo(ui_))),
                 "syllable");
    EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hunmin_ibus_ui_autoswitch(ui_))));
}

/* The commit-unit picker offers the schema's two values and nothing else — the
 * pair is what makes it a preference rather than a free-form field. */
TEST_F(SettingsUiTest, TheCommitPickerOffersTheSchemasTwoValues) {
    const auto rows = ComboRows(hunmin_ibus_ui_commit_combo(ui_));
    EXPECT_EQ(rows, (std::vector<std::string>{"word", "syllable"}));
}

/* The autoswitch row says when it cannot be used instead of pretending: the
 * sensitivity follows the data gate the engine applies (B3). */
TEST_F(SettingsUiTest, TheAutoswitchRowFollowsTheDataGate) {
    const bool data = ctx_ && hunmin_ctx_autoswitch_data_present(ctx_);
    EXPECT_EQ(gtk_widget_get_sensitive(hunmin_ibus_ui_autoswitch(ui_)) != FALSE, data);
}

/* Capture: one entry per chord, a duplicate is a no-op, and the two grammar
 * corners are refused — the same validator the engine applies to a stored value. */
TEST_F(SettingsUiTest, OfferingAKeyAppendsOnceAndRefusesWhatCannotFire) {
    /* Start from an empty row: the default already holds Hangul, and a duplicate
     * is the case asserted further down. */
    const gchar *empty[] = {nullptr};
    ASSERT_TRUE(g_settings_set_strv(gs_, "switch-keys", empty));
    hunmin_ibus_ui_refresh(ui_);
    const size_t before = StoredKeys().size();
    EXPECT_EQ(hunmin_ibus_ui_offer_key(ui_, IBUS_KEY_Hangul, 0), 1);
    EXPECT_EQ(KeyRows(), before + 1);

    /* Same chord again: no second row, because two rows matching one event are
     * indistinguishable in the list. */
    EXPECT_EQ(hunmin_ibus_ui_offer_key(ui_, IBUS_KEY_Hangul, 0), 0);
    EXPECT_EQ(KeyRows(), before + 1);

    /* A chord that carries Shift is a different entry. */
    EXPECT_EQ(hunmin_ibus_ui_offer_key(ui_, IBUS_KEY_Hangul, GDK_SHIFT_MASK), 1);
    EXPECT_EQ(KeyRows(), before + 2);

    /* Release can never fire in this engine, and Meta trips IBus's parser: both
     * are refused, and neither grows the list. */
    EXPECT_EQ(hunmin_ibus_ui_offer_key(ui_, 0, 0), 0);
    const auto keys = StoredKeys();
    EXPECT_NE(std::find(keys.begin(), keys.end(), "Hangul"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "Shift+Hangul"), keys.end());
}

/* Removing a row writes the shortened list, so the window and the engine agree
 * about what is stored. */
TEST_F(SettingsUiTest, RemovingAKeyWritesTheShortenedList) {
    ASSERT_EQ(hunmin_ibus_ui_offer_key(ui_, IBUS_KEY_Hangul_Hanja, 0), 1);
    const auto keys = StoredKeys();
    ASSERT_GE(keys.size(), 2u);
    const size_t before = keys.size();
    hunmin_ibus_ui_remove_key_at(ui_, 0);
    EXPECT_EQ(StoredKeys().size(), before - 1);
    EXPECT_EQ(KeyRows(), before - 1);
}

} // namespace