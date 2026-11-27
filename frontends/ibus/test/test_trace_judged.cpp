/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The personal key log's `judged` field (Track 2, issue #295's follow-up).
 *
 * Why this test exists: the commit alone cannot tell "the person typed Hangul"
 * from "the engine rewrote it", so the log needs the judgement the engine acted
 * on — and a log field that is only ever inspected by eye is the "compiled out"
 * claim this tree refuses to accept without a check (tools/check-trace-optout.py
 * is the other half: the field must NOT be in a shipped engine).
 *
 * This binary compiles hunmin_state.c WITH -DHUNMIN_TRACE=1 and trace.c beside
 * it, drives the same key path a frontend does, and reads back the file. It is
 * the only place the sink and the key path are exercised together on the host;
 * D3 runs the same engine through a real daemon.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include "hunmin_state.h"
#include "trace.h"
}

namespace {

size_t file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fclose(f);
    return n > 0 ? static_cast<size_t>(n) : 0;
}

std::string slurp(const char *path) {
    std::string out;
    FILE *f = fopen(path, "rb");
    if (!f) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return out;
}

/* Every line of the log that carries a `judged` value. */
int judged_lines(const std::string &log, const char *want) {
    const std::string needle = std::string("\"judged\":\"") + want + "\"";
    int n = 0;
    size_t pos = 0;
    while ((pos = log.find(needle, pos)) != std::string::npos) {
        ++n;
        pos += needle.size();
    }
    return n;
}

/* The sink opens its file ONCE per process and appends, so every test in this
 * binary shares one path and reads only what IT appended. The file is removed by
 * the environment below, not per test: removing it in TearDown would leave the
 * sink writing to an unlinked inode and the next test reading an empty file. */
class TraceFileAtExit : public ::testing::Environment {
  public:
    void TearDown() override { remove("/tmp/hunmin-trace-judged-test.jsonl"); }
};

class TraceJudged : public ::testing::Test {
  protected:
    void SetUp() override {
        const char *data = getenv("HUNMIN_DATA_PATH");
        ASSERT_NE(data, nullptr) << "HUNMIN_DATA_PATH is what the engine reads";
        ASSERT_EQ(0, setenv("HUNMIN_TRACE_OUT", path_.c_str(), 1));
        offset_ = file_size(path_.c_str());
        const char *dirs[1] = {data};
        s_ = hunmin_state_new(dirs, 1);
        ASSERT_NE(s_, nullptr);
        ASSERT_TRUE(hunmin_state_set_layouts(s_, "dubeolsik", "us-qwerty"));
        ASSERT_EQ(1, hunmin_state_set_autoswitch(s_, 1))
            << "the shipped English list must load, or nothing is ever judged";
    }
    void TearDown() override { hunmin_state_free(s_); }
    /* Only this test's lines: the sink is a process-wide append-only file. */
    std::string own_log() const {
        const std::string all = slurp(path_.c_str());
        return offset_ < all.size() ? all.substr(offset_) : std::string();
    }
    /* One key, in the order hunmin_engine.c drives it (the sequence IS the
     * contract: RESET clears the previous key's judgement, the state takes it,
     * MODE records the layer's language after it, the event line prints both).
     * Reproducing the four calls here is what lets the state path and the sink
     * be tested without an IBus bus; the engine's own call sites are the four
     * lines in hunmin_process_key_event(). */
    void type(const char *keys) {
        for (const char *p = keys; *p; ++p) {
            hunmin_key_event ev = {};
            ev.base = static_cast<uint32_t>(static_cast<unsigned char>(*p));
            ev.id = HUNMIN_KEY_TEXT;
            hunmin_ibus_trace_reset();
            const int used = hunmin_state_key(s_, &ev);
            hunmin_ibus_trace_mode(hunmin_state_language(s_));
            hunmin_ibus_trace_event(&ev, used, ev.base);
        }
    }
    std::string path_ = "/tmp/hunmin-trace-judged-test.jsonl";
    size_t offset_ = 0;
    hunmin_state *s_ = nullptr;
};

/* `hello` typed in Hangul mode: the engine judges the word Latin at the space
 * and rewrites it, so the boundary key's line carries the judgement. */
TEST_F(TraceJudged, TheBoundaryLineCarriesTheJudgementTheEngineActedOn) {
    type("hello ");
    const std::string log = own_log();
    ASSERT_FALSE(log.empty()) << "the sink wrote nothing";
    EXPECT_NE(std::string::npos, log.find("\"case\":\"personal\""))
        << "the meta line is the dialect's first line";
    EXPECT_EQ(1, judged_lines(log, "latin"))
        << "one word, one judgement, one line: log was\n" << log;
    /* Only the boundary key judged; the letters before it are not rewrites. */
    size_t first = log.find("\"judged\"");
    ASSERT_NE(first, std::string::npos);
    const size_t line_start = log.rfind('\n', first) + 1;
    const std::string line = log.substr(line_start, log.find('\n', line_start) - line_start);
    EXPECT_NE(std::string::npos, line.find("\"preedit\":\"\""))
        << "the judged line is the boundary key (nothing left composing): " << line;
    /* The two fields that make a flip readable: the mode after the key, and a
     * clock. Without `mode` a preedit cannot say which language rendered it;
     * without `t` "the person noticed and stopped" is invisible. */
    EXPECT_NE(std::string::npos, line.find("\"mode\":\"latin\"")) << line;
    EXPECT_NE(std::string::npos, line.find("\"t\":")) << line;
    EXPECT_NE(std::string::npos, log.find("\"started\":\""))
        << "the meta line carries wall-clock start, so a log can be aligned with a session";
}

/* The mode is on every key, and the clock is monotone: those are the two
 * fields a 도깨비 analysis needs (a flip is the SAME text rendered by the other
 * mode, and a repair is a DEL gap followed by the same keys). */
TEST_F(TraceJudged, EveryKeyCarriesTheModeAndAMonotoneClock) {
    type("gksrmf ");
    const std::string log = own_log();
    ASSERT_FALSE(log.empty());
    int hangul = 0, clocks = 0;
    long long last = -1;
    bool ordered = true;
    size_t pos = 0;
    while ((pos = log.find("\"t\":", pos)) != std::string::npos) {
        const long long t = atoll(log.c_str() + pos + 4);
        if (last >= 0 && t < last) ordered = false;
        last = t;
        ++clocks;
        pos += 4;
    }
    pos = 0;
    while ((pos = log.find("\"mode\":\"hangul\"", pos)) != std::string::npos) {
        ++hangul;
        pos += 15;
    }
    EXPECT_EQ(7, clocks) << "one clock per key (gksrmf + space): " << log;
    EXPECT_TRUE(ordered) << "the clock must not go backwards";
    EXPECT_EQ(7, hangul) << "한글 keys stay in Hangul mode: " << log;
}

/* A word that keeps its mode logs no judgement at all: the field marks a
 * rewrite, not every key. */
TEST_F(TraceJudged, AWordThatKeepsItsModeLogsNoJudgement) {
    type("gksrmf ");
    const std::string log = own_log();
    ASSERT_FALSE(log.empty());
    EXPECT_EQ(0, judged_lines(log, "latin") + judged_lines(log, "hangul"))
        << "한글 was typed and kept, so nothing was rewritten: log was\n" << log;
}

} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new TraceFileAtExit);
    return RUN_ALL_TESTS();
}