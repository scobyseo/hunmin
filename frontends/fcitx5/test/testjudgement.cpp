/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier D1, the CORPUS lane (issue #178).
 *
 * docs/DESIGN-FRONTEND.md Part A §6.2.1 records the maintainer's ruling: every
 * frontend is evaluated against the same corpus, and a frontend's own
 * characteristics may differ only where §3 lists them. This lane is the fcitx5
 * half of that: it replays tests/golden/judgement-r8.jsonl -- the same file the
 * shared-layer runner (ctest engine-golden-judgement) asserts -- through the real
 * addon, so the frontend is measured against the corpus and not against a second
 * copy of the expectations.
 *
 * What the lane adds over the shared-layer runner is the two things only a real
 * frontend can get wrong: that the delivered text reaches the client with the
 * delivery TIMING the corpus states (step_commits), and that the mode the corpus
 * says the word ends in is the mode the addon is actually in.
 *
 * What it deliberately does not do, said out loud rather than skipped silently:
 *
 *   - A case whose `mode` is `latin` is not replayed here. The starting language
 *     belongs to the host, and this addon has no key that sets the layer's
 *     initial mode -- its bare Han/Eng press is fcitx5's own trigger, which
 *     switches input method, not the layer's slot. Mid-stream switches ARE
 *     covered (r8-mixed-sentence-gate flips four times), so the direction itself
 *     is not untested, only the initial state.
 *   - The corpus's `judged` list is not compared directly: fcitx5's test surface
 *     exposes no language read-back. The lane asserts the CONSEQUENCE instead --
 *     after each case it types the corpus's probe and checks that what comes out
 *     is the reading of `mode_after`. A frontend that took the judgement but
 *     failed to switch fails there.
 *
 * The engine's on/off state for the judgement (SPEC-MODES §E5, ruled 2026-10-31)
 * is a VALUE, and the corpus's `autoswitch` field states it per case. Before #178
 * this lane could not exist: the addon had no surface that could turn the
 * judgement on, so every case would have measured the OFF identity path.
 *
 * ⚠️ Corrected 2026-11-03 (issue #233): the first cut PRESSED Ctrl+한/영 once for
 * every case whose corpus row said `autoswitch: 1`. That chord is a TOGGLE of the
 * value (SPEC-MODES §ReqMode13 surface A; engine-d1's watcher case asserts ON ->
 * OFF), so one press per case alternated the state instead of setting it:
 * r9-three-keys-are-judged is the sixth case that presses it and got the value
 * OFF, failing with the Hangul reading `ㅏ됴` delivered where the corpus says
 * `key`. The lane therefore writes the state through `setConfig` — the value
 * surface B (the settings UI) writes — and the chord's flip stays asserted by
 * engine-d1, which is where it belongs.
 */
#include "testdir.h"
#include "testfrontend_public.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "cJSON.h"
#include "fcitx-config/rawconfig.h"
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/instance.h>

using namespace fcitx;

namespace {

Instance *g_instance = nullptr;
int g_failed = 0; /* set by the corpus loop, read by main */

/* One corpus case, as the file states it. */
struct Case {
    std::string name;
    std::string layout;
    std::string mode;
    std::string keys;
    std::string modeAfter;
    int autoswitch = 1;
    std::vector<std::string> stepCommits;
    bool hasProbe = false;
    std::string probeKeys;
    std::string probeHangul;
    std::string probeLatin;
};

std::string str(const cJSON *o, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? std::string(v->valuestring) : std::string();
}

std::string readFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "testjudgement: cannot open %s\n", path);
        std::exit(2);
    }
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return out;
}

/* The corpus is one JSON object per line; the first line is provenance. */
std::vector<Case> loadCorpus(const char *path) {
    const std::string body = readFile(path);
    std::vector<Case> cases;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t end = body.find('\n', pos);
        if (end == std::string::npos) end = body.size();
        const std::string line = body.substr(pos, end - pos);
        pos = end + 1;
        if (line.empty()) continue;
        cJSON *j = cJSON_Parse(line.c_str());
        if (!j) {
            std::fprintf(stderr, "testjudgement: bad JSON in %s: %s\n", path, line.c_str());
            std::exit(2);
        }
        if (!cJSON_GetObjectItemCaseSensitive(j, "meta")) {
            Case c;
            c.name = str(j, "case");
            c.layout = str(j, "layout");
            c.mode = str(j, "mode");
            c.keys = str(j, "keys");
            c.modeAfter = str(j, "mode_after");
            const cJSON *as = cJSON_GetObjectItemCaseSensitive(j, "autoswitch");
            if (cJSON_IsNumber(as)) c.autoswitch = as->valueint;
            const cJSON *steps = cJSON_GetObjectItemCaseSensitive(j, "step_commits");
            if (cJSON_IsArray(steps)) {
                const cJSON *e = nullptr;
                cJSON_ArrayForEach(e, steps) {
                    c.stepCommits.push_back(cJSON_IsString(e) ? std::string(e->valuestring) : std::string());
                }
            }
            const cJSON *p = cJSON_GetObjectItemCaseSensitive(j, "probe");
            if (cJSON_IsObject(p)) {
                c.hasProbe = true;
                c.probeKeys = str(p, "keys");
                c.probeHangul = str(p, "hangul");
                c.probeLatin = str(p, "latin");
            }
            cases.push_back(c);
        }
        cJSON_Delete(j);
    }
    return cases;
}

/* Send one key. `checkAccept` is used only where acceptance IS an expectation the
 * corpus states; the corpus's per-key rows carry the delivery, not the consumed
 * flag (that is the trace corpora's dimension, docs/trace-spec.md), and a
 * delimiter is legitimately declined, so asserting it here would invent a
 * requirement the file does not make. */
bool press(ICUUID uuid, const Key &key, bool checkAccept = false, bool expectAccepted = true) {
    auto *tf = g_instance->addonManager().addon("testfrontend");
    if (!tf) return false;
    const bool accepted = tf->call<ITestFrontend::sendKeyEvent>(uuid, key, false);
    if (checkAccept && accepted != expectAccepted) {
        std::fprintf(stderr, "testjudgement: key %s accepted=%d expected=%d\n", key.toString().c_str(),
                     static_cast<int>(accepted), static_cast<int>(expectAccepted));
        return false;
    }
    return true;
}

void expectCommit(const std::string &text) {
    auto *tf = g_instance->addonManager().addon("testfrontend");
    if (tf) tf->call<ITestFrontend::pushCommitExpectation>(text);
}

/* The key alphabet the corpus uses, identical to hunmin-sim's: a plain byte, or
 * "\s<char>" (that character with Shift held), "\b" (BackSpace), "\\" (a literal
 * backslash). Returns false on an unknown escape rather than inventing a key. */
bool keyAt(const std::string &s, size_t &i, Key &out) {
    const char *p = s.c_str() + i;
    if (p[0] == '\\' && p[1]) {
        if (p[1] == 's' && p[2]) {
            const char ch = p[2];
            const KeySym sym = static_cast<KeySym>(static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(ch))));
            out = Key(sym, KeyState::Shift);
            i += 3;
            return true;
        }
        if (p[1] == 'b') {
            out = Key(FcitxKey_BackSpace);
            i += 2;
            return true;
        }
        if (p[1] == '\\') {
            out = Key(static_cast<KeySym>('\\'));
            i += 2;
            return true;
        }
        std::fprintf(stderr, "testjudgement: unknown key escape \\%c\n", p[1]);
        return false;
    }
    out = Key(static_cast<KeySym>(static_cast<unsigned char>(p[0])));
    i += 1;
    return true;
}

/* Replay one case. Returns false when the case failed; the harness's own commit
 * assertion aborts the process on a text mismatch, so a returned false is the
 * lane's own (accepted-flag or probe) failure. */
bool runCase(ICUUID uuid, const Case &c, int &skipped) {
    if (c.mode == "latin") {
        std::printf("  skip %-44s starts in latin mode: the initial language is the host's\n", c.name.c_str());
        skipped++;
        return true;
    }
    auto *hunmin = g_instance->addonManager().addon("hunmin", true);
    if (!hunmin) return false;
    /* A FRESH layer per case, and this is the one place the lane works around a
     * frontend characteristic instead of replaying the corpus verbatim: the
     * layer's mode is state, a case states which mode it starts in, and this
     * addon has no key that sets it (its bare Han/Eng press is fcitx5's own
     * trigger, which switches input method rather than the layer's slot). So the
     * lane rebuilds the layer -- the addon rebuilds lazily on the next KEY when
     * the layout changed, hence the sentinel config plus a declined BackSpace on
     * each side. A fresh layer starts in Hangul with the autoswitch default OFF
     * (B3), which is what the corpus's `mode: hangul` cases and the chord below
     * assume. A frontend that could set the starting mode would not need this. */
    auto configure = [&](const std::string &layout) {
        RawConfig raw;
        raw.setValueByPath("HangulLayout", layout);
        raw.setValueByPath("LatinLayout", "us-qwerty");
        raw.setValueByPath("CommitBehavior", "word");
        /* The corpus's `autoswitch` is a VALUE, set as one (issue #233). The
         * chord that also changes it TOGGLES, so pressing it per case made the
         * state depend on how many cases had run before — see the header. */
        raw.setValueByPath("AutoSwitch", c.autoswitch ? "True" : "False");
        hunmin->setConfig(raw);
    };
    configure("us-qwerty");                       /* sentinel: differs from every case layout */
    if (!press(uuid, Key(FcitxKey_BackSpace))) return false; /* declined: forces the rebuild, types nothing */
    configure(c.layout);
    if (!press(uuid, Key(FcitxKey_BackSpace))) return false;
    /* Every key, with the delivery the corpus states for it: the expectation is
     * pushed before the key is pressed, so a text OR timing change fails. */
    size_t i = 0, step = 0;
    while (i < c.keys.size()) {
        Key key;
        if (!keyAt(c.keys, i, key)) return false;
        if (step < c.stepCommits.size() && !c.stepCommits[step].empty()) {
            expectCommit(c.stepCommits[step]);
        }
        if (!press(uuid, key)) return false;
        step++;
    }
    if (!c.stepCommits.empty() && step != c.stepCommits.size()) {
        std::fprintf(stderr, "testjudgement: %s: %zu keys against %zu step_commits\n", c.name.c_str(), step,
                     c.stepCommits.size());
        return false;
    }

    /* The mode, asserted through its consequence: the corpus carries what the
     * same probe keys deliver in each mode, computed by the engine. */
    if (c.hasProbe) {
        const std::string want = (c.modeAfter == "latin") ? c.probeLatin : c.probeHangul;
        expectCommit(want);
        size_t pi = 0;
        while (pi < c.probeKeys.size()) {
            Key key;
            if (!keyAt(c.probeKeys, pi, key)) return false;
            if (!press(uuid, key)) return false;
        }
    }
    return true;
}

} // namespace

static void scheduleEvent(Instance *instance) {
    instance->eventDispatcher().schedule([instance]() {
        auto *hunmin = instance->addonManager().addon("hunmin", true);
        FCITX_ASSERT(hunmin);

        auto group = instance->inputMethodManager().currentGroup();
        group.inputMethodList().clear();
        group.inputMethodList().push_back(InputMethodGroupItem("keyboard-us"));
        group.inputMethodList().push_back(InputMethodGroupItem("hunmin"));
        group.setDefaultInputMethod("");
        instance->inputMethodManager().setGroup(group);

        auto *testfrontend = instance->addonManager().addon("testfrontend");
        FCITX_ASSERT(testfrontend);
        auto uuid = testfrontend->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance->inputContextManager().findByUUID(uuid);
        FCITX_ASSERT(testfrontend->call<ITestFrontend::sendKeyEvent>(uuid, Key("Control+space"), false));
        FCITX_ASSERT(instance->inputMethod(ic) == "hunmin");

        /* Unbuffered: a failing commit expectation aborts inside the harness, and
         * buffered progress lines would be lost exactly when they are needed. */
        setvbuf(stdout, nullptr, _IONBF, 0);
        const auto cases = loadCorpus(TESTING_SOURCE_DIR "/tests/golden/judgement-r8.jsonl");
        std::printf("testjudgement: %zu cases from tests/golden/judgement-r8.jsonl\n", cases.size());

        int skipped = 0, failed = 0;
        for (const auto &c : cases) {
            const bool ok = runCase(uuid, c, skipped);
            std::printf("  %s %s\n", ok ? "ok  " : "FAIL", c.name.c_str());
            if (!ok) failed++;
        }

        std::printf("testjudgement: %zu ran, %d skipped, %d failed\n", cases.size() - skipped, skipped, failed);
        g_failed = failed;
        instance->deactivate();
    });
    instance->eventDispatcher().schedule([instance]() { instance->exit(); });
}

int main() {
    setenv("HUNMIN_DATA_PATH", TESTING_SOURCE_DIR "/libhunmin/data", 1);
    setupTestingEnvironmentPath(TESTING_BINARY_DIR, {TEST_ADDON_DIR}, {TEST_DATA_DIR});
    char arg0[] = "testjudgement";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testim,testfrontend,hunmin";
    char *argv[] = {arg0, arg1, arg2};
    fcitx::Log::setLogRule("default=5,hunmin=5");
    Instance instance(FCITX_ARRAY_SIZE(argv), argv);
    instance.addonManager().registerDefaultLoader(nullptr);
    g_instance = &instance;
    scheduleEvent(&instance);
    instance.exec();
    return g_failed == 0 ? 0 : 1;
}
