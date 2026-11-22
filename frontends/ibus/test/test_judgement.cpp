/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier D1, the CORPUS lane for the IBus frontend (0.4 T1, issue #288).
 *
 * FE10 says one corpus evaluates every frontend; this lane replays
 * tests/golden/judgement-r8.jsonl -- the file the shared-layer runner
 * (ctest engine-golden-judgement) and the fcitx5 and Android lanes already
 * assert -- through hunmin_state, so the new frontend is measured against the
 * corpus rather than against a second copy of its expectations.
 *
 * It runs every case, including the two `mode: latin` ones fcitx5's lane skips:
 * this frontend can SET the starting mode (hunmin_state_set_language), so no
 * case has to borrow the host's initial state. The corpus's `autoswitch` field
 * is likewise a VALUE and is written as one; pressing a toggle per case is the
 * measured defect fcitx5's lane records (issue #233) and is not repeated here.
 *
 * What it asserts per case, and why each half is load-bearing:
 *   - step_commits: the delivered text AND its timing. A frontend that buffers
 *     one key too long passes a final-text comparison and fails here.
 *   - mode_after: read back from the layer, not inferred.
 *   - the probe: the corpus states what the same keys deliver in each mode, so
 *     after the case we type them and compare -- a switch that changed the
 *     bookkeeping but not the layer's active layout fails there.
 *
 * What it does NOT cover: the IBus key translation (D2 asserts that), the
 * display (D2), or anything about a daemon and a client (D3, issue #292).
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "cJSON.h"

extern "C" {
#include "hunmin_state.h"
}

namespace {

struct Case {
    std::string name;
    std::string layout;
    std::string latin = "us-qwerty";
    std::string mode = "hangul";
    std::string mode_after = "hangul";
    int autoswitch = 1;
    std::string keys;
    std::string commit;
    std::vector<std::string> step_commits;
    bool has_probe = false;
    std::string probe_keys;
    std::string probe_hangul;
    std::string probe_latin;
};

std::string str(const cJSON *o, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? std::string(v->valuestring) : std::string();
}

std::string read_file(const char *path) {
    FILE *f = std::fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "ibus judgement lane: cannot open %s\n", path);
        std::exit(2);
    }
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return out;
}

/* One JSON object per line; the first line is provenance, not a case. */
std::vector<Case> load_corpus(const char *path) {
    const std::string body = read_file(path);
    std::vector<Case> cases;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t end = body.find('\n', pos);
        if (end == std::string::npos) end = body.size();
        const std::string line = body.substr(pos, end - pos);
        pos = end + 1;
        if (line.find("\"case\"") == std::string::npos) continue;
        cJSON *o = cJSON_Parse(line.c_str());
        if (!o) {
            std::fprintf(stderr, "ibus judgement lane: unparseable case line\n");
            continue;
        }
        Case c;
        c.name = str(o, "case");
        c.layout = str(o, "layout");
        if (!str(o, "latin").empty()) c.latin = str(o, "latin");
        c.mode = str(o, "mode");
        c.mode_after = str(o, "mode_after");
        c.keys = str(o, "keys");
        c.commit = str(o, "commit");
        const cJSON *as = cJSON_GetObjectItemCaseSensitive(o, "autoswitch");
        if (cJSON_IsNumber(as)) c.autoswitch = as->valueint;
        const cJSON *steps = cJSON_GetObjectItemCaseSensitive(o, "step_commits");
        if (cJSON_IsArray(steps)) {
            const cJSON *e = nullptr;
            cJSON_ArrayForEach(e, steps) {
                c.step_commits.emplace_back(cJSON_IsString(e) ? e->valuestring : "");
            }
        }
        const cJSON *probe = cJSON_GetObjectItemCaseSensitive(o, "probe");
        if (cJSON_IsObject(probe)) {
            c.has_probe = true;
            c.probe_keys = str(probe, "keys");
            c.probe_hangul = str(probe, "hangul");
            c.probe_latin = str(probe, "latin");
        }
        cases.push_back(std::move(c));
        cJSON_Delete(o);
    }
    return cases;
}

/* The corpus key alphabet, the same one hunmin-sim and the fcitx5 lane use: a
 * plain byte, "\s<char>" (that character with Shift held), "\b" (BackSpace),
 * "\\" (a literal backslash). An unknown escape is a corpus error, not a key. */
bool key_at(const std::string &s, size_t &i, hunmin_key_event &ev) {
    ev = hunmin_key_event{};
    ev.id = HUNMIN_KEY_TEXT;
    if (s[i] == '\\' && i + 1 < s.size()) {
        const char tag = s[i + 1];
        if (tag == 's' && i + 2 < s.size()) {
            ev.base = static_cast<unsigned char>(s[i + 2]);
            ev.shift = 1;
            i += 3;
            return true;
        }
        if (tag == 'b') {
            ev.base = 0x08;
            i += 2;
            return true;
        }
        if (tag == '\\') {
            ev.base = '\\';
            i += 2;
            return true;
        }
        std::fprintf(stderr, "ibus judgement lane: unknown key escape \\%c\n", tag);
        return false;
    }
    ev.base = static_cast<unsigned char>(s[i]);
    i += 1;
    return true;
}

class Recording {
public:
    void Attach(hunmin_state *state) {
        sink_.user = this;
        sink_.commit = [](void *user, const char *text) {
            static_cast<Recording *>(user)->committed_ += text;
        };
        sink_.preedit = [](void *, const char *, int) {};
        sink_.candidates = [](void *, int) {};
        hunmin_state_set_sink(state, &sink_);
    }
    const std::string &committed() const { return committed_; }
    void Clear() { committed_.clear(); }

private:
    hunmin_state_sink sink_{};
    std::string committed_;
};

class IbusJudgementCorpus : public ::testing::Test {
protected:
    void SetUp() override {
        const char *path = std::getenv("HUNMIN_JUDGEMENT_CORPUS");
        ASSERT_NE(path, nullptr) << "ctest sets HUNMIN_JUDGEMENT_CORPUS";
        corpus_ = load_corpus(path);
        ASSERT_FALSE(corpus_.empty());
    }
    std::vector<Case> corpus_;
};

TEST_F(IbusJudgementCorpus, EveryCaseReplaysThroughTheFrontend) {
    for (const Case &c : corpus_) {
        SCOPED_TRACE(c.name);
        hunmin_state *state = hunmin_state_new(nullptr, 0);
        ASSERT_NE(state, nullptr);
        Recording rec;
        rec.Attach(state);

        ASSERT_TRUE(hunmin_state_set_layouts(state, c.layout.c_str(), c.latin.c_str()))
            << "layout " << c.layout;
        /* A VALUE, written as one: a toggle per case would make the state depend
         * on how many cases ran before (issue #233). */
        hunmin_state_set_autoswitch(state, c.autoswitch);
        const int want_language =
            (c.mode == "latin") ? HUNMIN_LANGUAGE_LATIN : HUNMIN_LANGUAGE_HANGUL;
        EXPECT_EQ(hunmin_state_set_language(state, want_language), want_language)
            << "the corpus states the starting mode; this frontend can set it";

        size_t i = 0;
        size_t step = 0;
        while (i < c.keys.size()) {
            hunmin_key_event ev;
            ASSERT_TRUE(key_at(c.keys, i, ev));
            ev.candidates_open = hunmin_state_candidates_open(state);
            rec.Clear();
            hunmin_state_key(state, &ev);
            if (step < c.step_commits.size()) {
                EXPECT_EQ(rec.committed(), c.step_commits[step])
                    << "step " << step << " of " << c.step_commits.size();
            }
            ++step;
        }
        if (!c.step_commits.empty()) {
            EXPECT_EQ(step, c.step_commits.size()) << "one key per entry";
        }

        const int want_after =
            (c.mode_after == "latin") ? HUNMIN_LANGUAGE_LATIN : HUNMIN_LANGUAGE_HANGUL;
        EXPECT_EQ(hunmin_state_language(state), want_after) << "mode_after";

        if (c.has_probe) {
            const std::string want =
                (c.mode_after == "latin") ? c.probe_latin : c.probe_hangul;
            rec.Clear();
            size_t pi = 0;
            while (pi < c.probe_keys.size()) {
                hunmin_key_event ev;
                ASSERT_TRUE(key_at(c.probe_keys, pi, ev));
                ev.candidates_open = hunmin_state_candidates_open(state);
                hunmin_state_key(state, &ev);
            }
            /* The probe's word is delivered at its boundary key, which the
             * corpus includes; what accumulated is the reading. */
            EXPECT_EQ(rec.committed(), want) << "the probe's reading";
        }
        hunmin_state_free(state);
    }
}

}  // namespace