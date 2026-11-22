/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier C for the IBus frontend (issue #316): replay a platform-native trace
 * (docs/trace-spec.md) through the frontend's OWN key path and emit what it
 * did, so tools/trace-check.py can diff it against the same expectation the
 * fcitx5 lane is held to.
 *
 * The corpus is shared, not copied: tests/conformance/x11/*.trace.in carries
 * X11 keysyms and modifier masks, which are exactly what IBus delivers to
 * process_key_event (IBUS_*_MASK are the X11 masks). Each event goes through
 * hunmin_ibus_translate_key -- the real translation, not a re-derivation --
 * and then hunmin_state_key, the real key path. The commit is OBSERVED on the
 * recording sink, which is stronger than the fcitx5 emitter's expectation
 * queue: there is no queue to echo, so a wrong commit cannot pass silently.
 *
 * The commit unit follows the trace's meta.commit_mode, exactly as the fcitx5
 * emitter does: tier A records per-syllable delivery, and a case whose golden
 * commits mid-stream only replays faithfully under the policy tier A recorded.
 * Absent, this frontend's own default (word) applies.
 *
 * Usage: ibus-trace-emit <case>.trace.in   (HUNMIN_DATA_PATH must point at
 * libhunmin/data; ctest sets it, as for every IBus lane).
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "cJSON.h"

extern "C" {
#include "hunmin_engine.h"
#include "hunmin_state.h"
}

namespace {

std::string jsonEscape(const std::string &s) {
    std::string o;
    for (char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                o += buf;
            } else {
                o += c;
            }
        }
    }
    return o;
}

/* What the sink was told to deliver for the key being replayed. The state
 * calls commit at most once per key on this path, but accumulate rather than
 * assume: a dropped or duplicated delivery must show up in the trace. */
struct Recording {
    std::string committed;
    static void OnCommit(void *user, const char *text) {
        static_cast<Recording *>(user)->committed += text ? text : "";
    }
    static void OnPreedit(void *, const char *, int) {}
    static void OnCandidates(void *, int) {}
};

std::string strField(const cJSON *o, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? std::string(v->valuestring) : std::string();
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: ibus-trace-emit <case>.trace.in\n");
        return 2;
    }
    FILE *f = std::fopen(argv[1], "rb");
    if (!f) {
        std::fprintf(stderr, "ibus-trace-emit: cannot open %s\n", argv[1]);
        return 2;
    }
    std::string body;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) body.append(buf, n);
    std::fclose(f);

    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t end = body.find('\n', pos);
        if (end == std::string::npos) end = body.size();
        std::string line = body.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty()) lines.push_back(line);
    }
    if (lines.empty() || lines[0].find("\"meta\"") == std::string::npos) {
        std::fprintf(stderr, "ibus-trace-emit: line 1 must be meta (docs/trace-spec.md)\n");
        return 2;
    }
    cJSON *meta = cJSON_Parse(lines[0].c_str());
    if (!meta) {
        std::fprintf(stderr, "ibus-trace-emit: unparseable meta line\n");
        return 2;
    }
    const std::string layout = strField(cJSON_GetObjectItemCaseSensitive(meta, "meta"), "layout");
    const std::string commitMode = strField(cJSON_GetObjectItemCaseSensitive(meta, "meta"), "commit_mode");
    cJSON_Delete(meta);
    if (layout.empty()) {
        std::fprintf(stderr, "ibus-trace-emit: meta has no layout\n");
        return 2;
    }

    hunmin_state *state = hunmin_state_new(nullptr, 0);
    if (!state) {
        std::fprintf(stderr, "ibus-trace-emit: no layout data (HUNMIN_DATA_PATH)\n");
        return 77;
    }
    if (!hunmin_state_set_layouts(state, layout.c_str(), "us-qwerty")) {
        std::fprintf(stderr, "ibus-trace-emit: layout %s did not load\n", layout.c_str());
        hunmin_state_free(state);
        return 2;
    }
    if (commitMode == "syllable") hunmin_state_set_commit_policy(state, HUNMIN_COMMIT_PER_SYLLABLE);

    Recording rec;
    hunmin_state_sink sink{};
    sink.user = &rec;
    sink.commit = Recording::OnCommit;
    sink.preedit = Recording::OnPreedit;
    sink.candidates = Recording::OnCandidates;
    hunmin_state_set_sink(state, &sink);

    /* The meta line is echoed verbatim: the checker reads the case's identity
     * and delivery model from it, and this runner does not get to restate it. */
    std::printf("%s\n", lines[0].c_str());

    for (size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].find("\"meta\"") != std::string::npos) continue;
        cJSON *o = cJSON_Parse(lines[i].c_str());
        if (!o) {
            std::fprintf(stderr, "ibus-trace-emit: unparseable event line %zu\n", i + 1);
            hunmin_state_free(state);
            return 2;
        }
        const cJSON *in = cJSON_GetObjectItemCaseSensitive(o, "in");
        const cJSON *ks = cJSON_GetObjectItemCaseSensitive(in, "keysym");
        const cJSON *mods = cJSON_GetObjectItemCaseSensitive(in, "mods");
        const guint keyval = cJSON_IsNumber(ks) ? (guint)ks->valuedouble : 0;
        const guint stateBits = cJSON_IsNumber(mods) ? (guint)mods->valuedouble : 0;
        cJSON_Delete(o);

        rec.committed.clear();
        int used;
        if (keyval == IBUS_KEY_F9) {
            /* The engine's own F9 split: presentation is the engine object's
             * half, outside the printable window the state feeds. */
            used = hunmin_state_toggle_candidates(state);
        } else {
            /* Exactly what hunmin_process_key_event does before the state:
             * Caps Lock is the layout's variable, detected from the lock bit. */
            hunmin_state_set_caps(state, (stateBits & IBUS_LOCK_MASK) ? 1 : 0);
            const hunmin_key_event ev =
                /* No store in the emitter: the corpus cases carry no 한/영 key
                 * (measured: no tests/conformance/x11/*.in event does), and a
                 * compiled-in default here would be a second home for the
                 * schema's value. */
                hunmin_ibus_translate_key(keyval, stateBits, hunmin_state_candidates_open(state),
                                          nullptr);
            used = hunmin_state_key(state, &ev);
        }
        const char *preedit = hunmin_state_preedit(state);
        std::printf("{\"in\": {\"keysym\": %u, \"mods\": %u}, \"commit\": \"%s\", "
                    "\"preedit\": \"%s\", \"consumed\": %s}\n",
                    keyval, stateBits, jsonEscape(rec.committed).c_str(),
                    jsonEscape(preedit ? preedit : "").c_str(), used ? "true" : "false");
    }
    hunmin_state_free(state);
    return 0;
}
