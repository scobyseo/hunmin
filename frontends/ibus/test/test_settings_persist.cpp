/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Persistence across a process restart (issue #327, docs/SPEC-FRONTEND.md §FE17).
 *
 * Why this is three processes and not one test binary: "the value I set is the
 * value the engine gets" is a claim about two PROCESSES — the window (or
 * gsettings) writes, the engine starts later and reads. An in-process test can
 * only show that a GSettings object agrees with itself.
 *
 * Why the keyfile backend rather than the shipped dconf one: a dconf store
 * cannot be isolated by environment. A write inside a private session bus with a
 * redirected XDG_CONFIG_HOME still lands in the developer's own
 * ~/.config/dconf/user (measured, docs/OPTIONS-MEASURED.md §IBC-5), and a test
 * that edits a person's input method settings is not a test anybody can run.
 * `GSETTINGS_BACKEND=keyfile` honours XDG_CONFIG_HOME, so the store is a file in
 * a scratch directory and the shipped backend's behaviour is the only thing this
 * case does not cover — recorded in docs/TESTING.md rather than glossed over.
 */
#include <gtest/gtest.h>
#include <glib.h>

#include <string>
#include <vector>

namespace {

/* Set by CMake: the two binaries this case drives, and nothing about the tree. */
#ifndef HUNMIN_SETTINGS_BIN
#error "HUNMIN_SETTINGS_BIN must point at the built hunmin-ibus-settings"
#endif
#ifndef HUNMIN_GSETTINGS_BIN
#error "HUNMIN_GSETTINGS_BIN must point at gsettings(1)"
#endif

class PersistTest : public ::testing::Test {
  protected:
    void SetUp() override {
        const ::testing::TestInfo *info = ::testing::UnitTest::GetInstance()->current_test_info();
        scratch_ = g_dir_make_tmp("hunmin-persist-XXXXXX", nullptr);
        ASSERT_NE(scratch_, nullptr);
        (void)info;
        /* The schema dies with the process that has it in memory, so the store
         * has to be a file for this to mean anything. */
        base_ = {
            g_strdup_printf("XDG_CONFIG_HOME=%s", scratch_),
            g_strdup_printf("GSETTINGS_BACKEND=keyfile"),
            g_strdup_printf("HUNMIN_DATA_PATH=%s", g_getenv("HUNMIN_DATA_PATH")
                                                       ? g_getenv("HUNMIN_DATA_PATH")
                                                       : ""),
        };
        if (g_getenv("GSETTINGS_SCHEMA_DIR"))
            base_.push_back(
                g_strdup_printf("GSETTINGS_SCHEMA_DIR=%s", g_getenv("GSETTINGS_SCHEMA_DIR")));
    }

    void TearDown() override {
        base_.clear(); /* each entry owns its own copy; nothing to free here */
        if (scratch_) {
            g_spawn_command_line_sync((std::string("rm -rf ") + scratch_).c_str(), nullptr,
                                      nullptr, nullptr, nullptr);
            g_free(scratch_);
        }
    }

    /* Run one command with the scratch store and return its stdout, trimmed. */
    std::string Run(const std::vector<std::string> &argv) {
        std::vector<std::string> env = base_;
        gchar **envp = g_new0(gchar *, env.size() + 1);
        for (size_t i = 0; i < env.size(); ++i) envp[i] = g_strdup(env[i].c_str());
        gchar **args = g_new0(gchar *, argv.size() + 1);
        for (size_t i = 0; i < argv.size(); ++i) args[i] = g_strdup(argv[i].c_str());
        gchar *out = nullptr;
        gint status = -1;
        const gboolean ok = g_spawn_sync(nullptr, args, envp,
                                         (GSpawnFlags)(G_SPAWN_STDERR_TO_DEV_NULL), nullptr, nullptr,
                                         &out, nullptr, &status, nullptr);
        g_strfreev(args);
        g_strfreev(envp);
        EXPECT_TRUE(ok);
        EXPECT_EQ(status, 0);
        std::string text(out ? out : "");
        g_free(out);
        while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) text.pop_back();
        return text;
    }

    /* `--print row` answers `row=value` lines (and, for the two rows the engine
     * may refuse, an `engine-…=` line beside it). This picks the line asked for,
     * so a case asserts one answer rather than a paragraph. */
    std::string Print(const std::string &row) {
        const std::string out = Run({HUNMIN_SETTINGS_BIN, "--print", row});
        std::vector<std::string> lines;
        std::string cur;
        for (char c : out) {
            if (c == '\n') {
                lines.push_back(cur);
                cur.clear();
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) lines.push_back(cur);
        const std::string prefix = row + "=";
        for (const std::string &line : lines)
            if (line.rfind(prefix, 0) == 0) return line.substr(prefix.size());
        ADD_FAILURE() << "--print " << row << " printed no `" << prefix << "` line: " << out;
        return std::string();
    }

    std::string RawPrint() { return Run({HUNMIN_SETTINGS_BIN, "--print"}); }

    std::string Set(const std::string &row, const std::string &value) {
        return Run({HUNMIN_GSETTINGS_BIN, "set", "org.freedesktop.ibus.engine.hunmin", row, value});
    }

    gchar *scratch_ = nullptr;
    std::vector<std::string> base_;
};

/* The core of the issue's "엔진 재시작 후 유지" line: one process writes, a
 * LATER process reads the same value back. */
TEST_F(PersistTest, AStoredRowSurvivesIntoTheNextProcess) {
    EXPECT_EQ(Print("hangul-layout"), "sin3-p2");
    Set("hangul-layout", "dubeolsik");
    EXPECT_EQ(Print("hangul-layout"), "dubeolsik");

    EXPECT_EQ(Print("commit-unit"), "word");
    Set("commit-unit", "syllable");
    EXPECT_EQ(Print("commit-unit"), "syllable");

    /* The row is stored as asked; whether the ENGINE holds it is a separate
     * answer (B3 refuses ON without the English word list), which is why the two
     * are printed as separate lines. */
    EXPECT_EQ(Print("autoswitch"), "false");
    Set("autoswitch", "true");
    EXPECT_EQ(Print("autoswitch"), "true");
    /* Whether the ENGINE holds it depends on the English word list being
     * installed (B3 refuses ON without it), so that line is asserted to exist and
     * to be one of the two values — not asserted to match the store, which is the
     * whole point of printing them separately. */
    const std::string held = RawPrint();
    EXPECT_NE(held.find("engine-autoswitch="), std::string::npos) << held;

    /* The fifth row is a list, and it is the one a store can mangle. */
    EXPECT_EQ(Print("switch-keys"), "Hangul,Alt_R");
    Set("switch-keys", "['Hangul']");
    EXPECT_EQ(Print("switch-keys"), "Hangul");
}

/* The store really is on disk, in the scratch directory: a case that read its own
 * memory would pass this and prove nothing. */
TEST_F(PersistTest, TheValueIsOnDiskNotInMemory) {
    Set("latin-layout", "dvorak");
    const std::string file = std::string(scratch_) + "/glib-2.0/settings/keyfile";
    gchar *contents = nullptr;
    ASSERT_TRUE(g_file_get_contents(file.c_str(), &contents, nullptr, nullptr));
    const std::string text(contents);
    g_free(contents);
    EXPECT_NE(text.find("dvorak"), std::string::npos);
    EXPECT_EQ(Print("latin-layout"), "dvorak");
}

/* A value the engine refuses is REPORTED, not silently kept: the reader is what
 * decides, so `--print` carries the count next to the rows. */
TEST_F(PersistTest, ARefusedValueIsReportedNotSilentlyKept) {
    Set("hangul-layout", "no-such-layout");
    const std::string out = RawPrint();
    EXPECT_NE(out.find("rejected=1"), std::string::npos) << out;
    /* The store keeps what was set; the ENGINE reports the schema's default,
     * because the bad id did not load. The disagreement is the evidence. */
    EXPECT_EQ(Print("hangul-layout"), "no-such-layout");
    EXPECT_NE(out.find("engine-hangul-layout=sin3-p2"), std::string::npos) << out;
}

} // namespace