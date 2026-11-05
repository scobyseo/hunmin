/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1, the judgement CORPUS lane (issue #180, E7).
 *
 * docs/FRONTEND-EQUIVALENCE.md §6.2.1 records the maintainer's ruling: every
 * frontend is evaluated against the same corpus, and a frontend's own
 * characteristics may differ only where §3 lists them. This lane is the TSF
 * half of that: it replays tests/golden/judgement-r8.jsonl -- the same file
 * the shared-layer runner, the fcitx5 lane (engine-d1-judgement) and the
 * Android lane (JudgementCorpusTest) assert -- through the real adapter over
 * the mock TSF layer, so TSF is measured against the corpus and not against
 * a second copy of the expectations.
 *
 * What the lane asserts per case, mirroring the other two lanes:
 *   - the delivered text, asserted cumulatively (the corpus's commit): TSF
 *     native delivery inserts each completed syllable as it is typed, so
 *     the corpus's step_commits timing -- the x11/Android dialect -- is not
 *     TSF's observable; the total is, named as §3's allowed difference
 *     rather than passed silently;
 *   - the mode after the boundary, read directly: Snapshot().hangulMode is
 *     the adapter's own read-back, which the fcitx5 lane cannot reach and so
 *     asserts through the corpus probe instead. TSF takes the stronger form
 *     (the mode AND its probe consequence);
 *   - the corpus probe after the case, typed in whatever mode the case ended
 *     in, delivering the reading the corpus says that mode gives.
 *
 * Two corpus fields are NOT compared directly, said out loud rather than
 * passed silently:
 *   - `judged`: the mock surface has no judgement read-back (the engine
 *     answers take_judgement to the adapter, not to the test), so the lane
 *     asserts the judgement's OBSERVABLE: the mode change plus which reading
 *     is delivered. A lane that took the judgement but failed to switch
 *     fails on the mode assertion -- the same consequence fcitx5 takes.
 *   - r4-nothing-is-judged-in-flight carries no probe (it ends mid-word);
 *     the mode read-back is the assertion for it, as in the Android lane.
 *
 * Issue #212: every case is replayed in BOTH delivery modes (§2). The
 * interim replay forces HUNMIN_TSF_INTERIM=1 and reads the client through
 * mock-tsf-edit.h's MockEditControl (the interim writer's channel, as in
 * trace-emit), settling each posted write with FlushPendingDocWrites().
 * Its expectations are NOT the corpus's: §2.1 (ruled 2026-11-02, "interim
 * model cannot support autoswitch") makes the layer answer autoswitch=0
 * there, so a case the corpus judges (`judged` non-empty) must stay in its
 * starting mode -- the named difference, asserted, not skipped. For a
 * hangul-mode judged case the delivered text is the Hangul reading (the
 * corpus carries no autoswitch-off reading for it, so the lane asserts it
 * differs from the judged commit); for a latin-mode judged case the keys
 * are the app's passthrough (§2.2's interim arm), the mock emulates no app,
 * and the control must stay empty. Cases the judgement does not move must
 * deliver the corpus commit through either delivery -- that is §2's
 * same-text rule, and it is what makes the interim replay an equivalence
 * measurement rather than a second native run.
 *
 * A case whose `mode` is `latin` IS replayed here: unlike the fcitx5 addon,
 * the TSF service owns its language and has a manual switch (the bare
 * Han/Eng key), so the lane toggles into Latin before typing. The toggle
 * is a VALUE set for the case, not state inherited from the previous one --
 * each case builds a fresh service, and the corpus's `autoswitch` field is
 * written to the adapter's option store as one (the #233 lesson from the
 * fcitx5 lane: a toggle makes the measurement depend on what ran before).
 */

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <windows.h>

#include "cJSON.h"

#include "../src/config.h"
#include "../src/tsf.h"
#include "mock-tsf.h"
#include "mock-tsf-edit.h"
#include "tsf-keystate-mock.h"
#include "tsf-testdir.h"

namespace {

std::string wideToUtf8(const std::wstring &w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::string readFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return std::string();
    std::string out;
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return out;
}

std::string corpusPath() {
    /* HUNMIN_TSF_DATA_DIR is "<repo>/libhunmin/data"; the corpus sits at
     * "<repo>/tests/golden/judgement-r8.jsonl". MSVC's fopen takes forward
     * slashes, so no separator surgery is needed. */
    std::string data = HUNMIN_TSF_DATA_DIR;
    const std::string tail = "/libhunmin/data";
    size_t at = data.rfind(tail);
    if (at == std::string::npos) return std::string();
    return data.substr(0, at) + "/tests/golden/judgement-r8.jsonl";
}

struct CorpusCase {
    std::string name;
    std::string layout;
    std::string latin;
    std::string mode;
    std::string keys;
    std::string modeAfter;
    std::string commit;
    int autoswitch = 1;
    int judgedCount = 0;
    bool hasProbe = false;
    std::string probeKeys;
    std::string probeHangul;
    std::string probeLatin;
};

std::string jstr(const cJSON *o, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) ? std::string(v->valuestring) : std::string();
}

std::vector<CorpusCase> loadCorpus(const std::string &path) {
    const std::string body = readFile(path.c_str());
    EXPECT_FALSE(body.empty()) << "judgement corpus not found at " << path;
    std::vector<CorpusCase> cases;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t end = body.find('\n', pos);
        if (end == std::string::npos) end = body.size();
        const std::string line = body.substr(pos, end - pos);
        pos = end + 1;
        if (line.empty()) continue;
        cJSON *j = cJSON_Parse(line.c_str());
        if (!j) {
            EXPECT_NE(j, nullptr) << "bad JSON in the corpus: " << line;
            continue;
        }
        if (!cJSON_GetObjectItemCaseSensitive(j, "meta")) {
            CorpusCase c;
            c.name = jstr(j, "case");
            c.layout = jstr(j, "layout");
            c.latin = jstr(j, "latin");
            c.mode = jstr(j, "mode");
            c.keys = jstr(j, "keys");
            c.modeAfter = jstr(j, "mode_after");
            c.commit = jstr(j, "commit");
            const cJSON *as = cJSON_GetObjectItemCaseSensitive(j, "autoswitch");
            if (cJSON_IsNumber(as)) c.autoswitch = as->valueint;
            const cJSON *jd = cJSON_GetObjectItemCaseSensitive(j, "judged");
            if (cJSON_IsArray(jd)) c.judgedCount = cJSON_GetArraySize(jd);
            const cJSON *p = cJSON_GetObjectItemCaseSensitive(j, "probe");
            if (cJSON_IsObject(p)) {
                c.hasProbe = true;
                c.probeKeys = jstr(p, "keys");
                c.probeHangul = jstr(p, "hangul");
                c.probeLatin = jstr(p, "latin");
            }
            cases.push_back(c);
        }
        cJSON_Delete(j);
    }
    return cases;
}

/* One key of the corpus's alphabet, identical to the fcitx5 lane's: a plain
 * byte (typed as its upper-case virtual key), "\s<char>" (that character
 * with Shift held), "\b" (BackSpace), "\\" (a literal backslash). */
struct KeyPress {
    WPARAM vk;
    bool shift;
};

bool keyAt(const std::string &s, size_t &i, KeyPress &out) {
    const char *p = s.c_str() + i;
    if (p[0] == '\\' && p[1]) {
        if (p[1] == 's' && p[2]) {
            out.vk = (WPARAM)std::toupper((unsigned char)p[2]);
            out.shift = true;
            i += 3;
            return true;
        }
        if (p[1] == 'b') {
            out.vk = (WPARAM)VK_BACK;
            out.shift = false;
            i += 2;
            return true;
        }
        if (p[1] == '\\') {
            out.vk = (WPARAM)'\\';
            out.shift = false;
            i += 2;
            return true;
        }
        return false;
    }
    out.vk = (WPARAM)std::toupper((unsigned char)p[0]);
    out.shift = false;
    i += 1;
    return true;
}

class TsfJudgementTest : public ::testing::Test {
    friend void RunCase(TsfJudgementTest *, const CorpusCase &, bool);
protected:
    void SetUp() override {
        _putenv_s("HUNMIN_DATA_DIR", HUNMIN_TSF_DATA_DIR);
        const char *dirs[1] = {HUNMIN_TSF_DATA_DIR};
        ctx_ = hunmin_ctx_new(dirs, 1);
        ASSERT_NE(ctx_, nullptr) << "engine context unavailable (check HUNMIN_TSF_DATA_DIR)";
        g_mockKeyState[VK_SHIFT & 0xFF] = 0;
        tsf_test_register_keystate();
    }
    void TearDown() override {
        if (ctx_) {
            hunmin_ctx_free(ctx_);
            ctx_ = nullptr;
        }
    }

    hunmin_ctx *ctx_ = nullptr;

    struct Service {
        tsfmock::MockThreadMgr *tm = nullptr;
        tsfmock::MockContext *mctx = nullptr;
        tsfmock::MockDocumentMgr *dm = nullptr;
        CTextService *svc = nullptr;
        tsfmock::MockEditControl *edit = nullptr;
        ~Service() {
            if (svc) svc->Release();
            if (mctx) mctx->Release();
            if (dm) dm->Release();
            if (tm) tm->Release();
        }
    };

    /* The document a delivery writes: the mock's TSF document under
     * native, the edit control under interim (trace-emit: DocText). */
    std::wstring DocOf(Service &s, bool interim) { return interim ? s.edit->doc() : s.mctx->doc; }

    /* Send one corpus key through test+down, Shift held for \s keys. Returns
     * the text the delivery's document gained from it (UTF-8). Under the
     * interim delivery the write is posted, so settle it before reading --
     * exactly as trace-emit and the live host's next key event do. */
    std::string press(Service &s, bool interim, const KeyPress &k) {
        const std::wstring before = DocOf(s, interim);
        g_mockKeyState[VK_SHIFT & 0xFF] = k.shift ? (SHORT)0x8000 : (SHORT)0;
        BOOL eat = FALSE;
        s.svc->OnTestKeyDown(s.mctx, k.vk, 0, &eat);
        s.svc->OnKeyDown(s.mctx, k.vk, 0, &eat);
        if (interim) s.svc->FlushPendingDocWrites();
        g_mockKeyState[VK_SHIFT & 0xFF] = 0;
        const std::wstring after = DocOf(s, interim);
        if (after.size() < before.size()) return std::string("\x01");
        return wideToUtf8(after.substr(before.size()));
    }

    std::string type(Service &s, bool interim, const std::string &keys,
                     const std::string &what) {
        std::string delivered;
        size_t i = 0;
        while (i < keys.size()) {
            KeyPress k;
            EXPECT_TRUE(keyAt(keys, i, k)) << "unknown key escape in " << what;
            delivered += press(s, interim, k);
        }
        return delivered;
    }

    HunminTsfSnapshot Snap(CTextService *s) { return s->Snapshot(); }

    /* A fresh service per case, with every env seam pinned so the host's
     * real HKCU store cannot leak in. The case's layout pair and autoswitch
     * VALUE are written before Activate. */
    void makeService(Service &s, const CorpusCase &c, bool interim) {
        _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, c.layout.c_str());
        _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, c.latin.c_str());
        _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, c.autoswitch ? "1" : "0");
        _putenv_s(HUNMIN_TSF_ENV_COMMIT_UNIT, "word");
        _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "v1:165:0");
        _putenv_s("HUNMIN_TSF_INTERIM", interim ? "1" : "");

        s.tm = new tsfmock::MockThreadMgr();
        s.mctx = new tsfmock::MockContext();
        s.dm = new tsfmock::MockDocumentMgr(s.mctx);
        s.mctx->AddRef();
        if (interim) {
            /* The interim writer resolves its control by focus (issue
             * #185): one control per process, reset per case -- the same
             * harness trace-emit runs both modes through. */
            static tsfmock::MockEditControl edit;
            ASSERT_TRUE(edit.Create()) << "the mock edit control could not be created";
            edit.Reset();
            s.edit = &edit;
        }
        s.svc = new CTextService();
        s.svc->Activate(s.tm, 1);
        g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    }

    void clearEnv() {
        _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "");
        _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "");
        _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "");
        _putenv_s(HUNMIN_TSF_ENV_COMMIT_UNIT, "");
        _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "");
        _putenv_s("HUNMIN_TSF_INTERIM", "");
    }
};

/* One case under one delivery. Returns void; failures carry SCOPED_TRACE. */
void RunCase(TsfJudgementTest *t, const CorpusCase &c, bool interim) {
    SCOPED_TRACE(std::string(interim ? "interim" : "native") + " case " + c.name);
    TsfJudgementTest::Service s;
    t->makeService(s, c, interim);
    if (interim) {
        EXPECT_TRUE(t->Snap(s.svc).legacyMode) << "the interim run is not on the interim delivery";
        EXPECT_EQ(t->Snap(s.svc).autoswitch, 0)
            << "§2.1: the interim delivery must answer autoswitch off at the layer";
    }
    ASSERT_TRUE(t->Snap(s.svc).hangulMode) << "the service starts in hangul mode";

    if (c.mode == "latin") {
        BOOL eat = FALSE;
        s.svc->OnTestKeyDown(s.mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eat);
        s.svc->OnKeyDown(s.mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eat);
        if (interim) s.svc->FlushPendingDocWrites();
        ASSERT_FALSE(t->Snap(s.svc).hangulMode) << "the toggle entered latin mode";
    }

    /* §2.1's named difference, in one place: under the interim delivery a
     * judged case cannot switch, so its expected mode is its starting mode;
     * every other case (unjudged, or autoswitch already off) must match the
     * corpus mode_after through either delivery. */
    const bool judged = c.judgedCount > 0;
    const bool expectSwitch = !interim || !judged;
    const std::string wantMode = expectSwitch ? c.modeAfter : c.mode;

    t->type(s, interim, c.keys, "case keys");
    const bool bounded = !c.keys.empty() && c.keys.back() == ' ';
    if (bounded) {
        const std::string total = wideToUtf8(t->DocOf(s, interim));
        if (interim && c.mode == "latin") {
            /* §2.2 interim arm: latin-mode keys are the app's passthrough
             * and the mock emulates no app, so the control stays empty --
             * named, measured, not skipped. */
            EXPECT_EQ(total, "")
                << "interim latin-mode passthrough: the control must stay empty (the app's text is not the mock's)";
        } else if (interim && judged) {
            /* §2.1: the word is delivered in the language it was typed in
             * (the Hangul reading), never the judged commit. */
            EXPECT_NE(total, c.commit)
                << "interim judged case delivered the judged text anyway -- the switch fired where §2.1 says it cannot";
            EXPECT_FALSE(total.empty()) << "interim judged case delivered nothing at all";
        } else {
            EXPECT_EQ(total, c.commit) << "the case delivered the wrong total text";
        }
    } else {
        EXPECT_EQ(c.commit, "") << "a case with no trailing boundary but a commit the lane cannot place";
    }

    EXPECT_EQ(t->Snap(s.svc).hangulMode, wantMode == "hangul")
        << "the case ended in the wrong mode (want " << wantMode << ")";

    if (c.hasProbe) {
        if (interim && wantMode == "latin") {
            /* The probe keys are the app's too: nothing is delivered. */
            EXPECT_EQ(t->type(s, interim, c.probeKeys, "probe"), "")
                << "interim latin-mode probe delivered through the engine";
        } else {
            const std::string want = (wantMode == "latin") ? c.probeLatin : c.probeHangul;
            EXPECT_EQ(t->type(s, interim, c.probeKeys, "probe"), want)
                << "the probe did not deliver the " << wantMode << " reading";
        }
    }

    t->clearEnv();
}

TEST_F(TsfJudgementTest, CorpusReplayNative) {
    const std::string path = corpusPath();
    const std::vector<CorpusCase> cases = loadCorpus(path);
    ASSERT_FALSE(cases.empty()) << "the corpus replayed nothing -- an unloaded file is not a green lane";
    for (const CorpusCase &c : cases) RunCase(this, c, false);
}

TEST_F(TsfJudgementTest, CorpusReplayInterim) {
    const std::string path = corpusPath();
    const std::vector<CorpusCase> cases = loadCorpus(path);
    ASSERT_FALSE(cases.empty()) << "the corpus replayed nothing -- an unloaded file is not a green lane";
    for (const CorpusCase &c : cases) RunCase(this, c, true);
}

} // namespace
