/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1 (issue #66, layout-wiring half): the configuration store
 * (frontends/tsf/src/config.h) and the adapter's slot resolution
 * (resolveLayoutSlot in src/tsf.cpp) against the real data dir.
 *
 * Registry hygiene: the round-trip case runs against an isolated throwaway
 * key (HKCU\Software\Hunmin\TSF-selftest, deleted on every exit path) and
 * never reads or writes the production store HKCU\Software\Hunmin\TSF — the
 * production store stays a manual-checklist surface, not a test side effect.
 *
 * Exit 0 clean, 1 assertion failed, 77 skip (engine data unavailable).
 *
 * GoogleTest form (issue #109, option 1b): one ctest entry is kept
 * (tsf-config-test); the original sections are TEST_F cases with native
 * EXPECT_* assertions (the old CHECK counter is gone), and the 77-skip
 * contract is decided in main(), before RUN_ALL_TESTS.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <windows.h>

#include "tsf-testdir.h"

extern "C" HINSTANCE g_hunmin_hinst = nullptr;

/* The GetKeyState seam, copied from trace-emit.cpp: the adapter (and now the
 * toggle matcher it includes) read the thread input state, so the harness
 * owns it. Applies over the whole TU, mock-tsf.h included first. */
SHORT tsf_mock_GetKeyState(int vk);
SHORT g_mockKeyState[256] = {};
SHORT tsf_mock_GetKeyState(int vk) {
    return g_mockKeyState[vk & 0xFF];
}
#define GetKeyState tsf_mock_GetKeyState

#include "mock-tsf.h"
#define private public
#include "../src/tsf.cpp"
#undef private
#undef GetKeyState

static std::string wideToUtf8Preview(const std::wstring &w) {
    std::string o;
    for (wchar_t c : w) {
        if (c < 0x80) {
            o += (char)c;
        } else if (c < 0x800) {
            o += (char)(0xC0 | (c >> 6));
            o += (char)(0x80 | (c & 0x3F));
        } else {
            o += (char)(0xE0 | (c >> 12));
            o += (char)(0x80 | ((c >> 6) & 0x3F));
        }
    }
    return o;
}

class TsfConfigTest : public ::testing::Test {
 protected:
    void SetUp() override {
        _putenv_s("HUNMIN_DATA_DIR", HUNMIN_TSF_DATA_DIR);
        const char *dirs[1] = {HUNMIN_TSF_DATA_DIR};
        ctx_ = hunmin_ctx_new(dirs, 1);
        ASSERT_NE(ctx_, nullptr) << "engine context unavailable (check HUNMIN_TSF_DATA_DIR)";
        g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    }
    void TearDown() override {
        if (ctx_) {
            hunmin_ctx_free(ctx_);
            ctx_ = nullptr;
        }
    }

    hunmin_ctx *ctx_ = nullptr;
    HunminTsfConfig cfg_ = {}; /* the round-trip case's buffer */
};

/* 1. The env seam wins over everything (the test-injection path). */
TEST_F(TsfConfigTest, EnvSeamWinsOverEverything) {
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "colemak");
    HunminTsfConfig cfg;
    hunmin_tsf_config_load(nullptr, &cfg);
    EXPECT_STREQ(cfg.hangulLayout, "dubeolsik") << "env override hangul";
    EXPECT_STREQ(cfg.latinLayout, "colemak") << "env override latin";
}

/* 2. An empty env value is not an override: defaults return (and with a
 * NULL root the registry is skipped entirely, so this is deterministic
 * regardless of what a developer's real store holds). */
TEST_F(TsfConfigTest, EmptyEnvRestoresDefaults) {
    HunminTsfConfig cfg;
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "");
    hunmin_tsf_config_load(nullptr, &cfg);
    EXPECT_STREQ(cfg.hangulLayout, kHangulDefaultId) << "default hangul";
    EXPECT_STREQ(cfg.latinLayout, kLatinDefaultId) << "default latin";
    /* The candidate key (issue #67): default = the bare Hanja key (0x19 = 25). */
    EXPECT_STREQ(cfg.hanjaKey, "v1:25:0") << "default hanja key";
    _putenv_s(HUNMIN_TSF_ENV_HANJA_KEY, "v1:120:0");
    hunmin_tsf_config_load(nullptr, &cfg);
    EXPECT_STREQ(cfg.hanjaKey, "v1:120:0") << "env override hanja key";
    _putenv_s(HUNMIN_TSF_ENV_HANJA_KEY, "");
    hunmin_tsf_config_load(nullptr, &cfg);
    EXPECT_STREQ(cfg.hanjaKey, "v1:25:0") << "empty env hanja key -> default";
}

/* 3. Slot resolution against the real data (issue #71's list split must
 * hold for stored values too). */
TEST_F(TsfConfigTest, SlotResolutionAgainstTheRealData) {
    std::string out;
    resolveLayoutSlot(ctx_, "dubeolsik", kHangulDefaultId, true, &out, nullptr);
    EXPECT_EQ(out, "dubeolsik") << "valid hangul id kept";
    resolveLayoutSlot(ctx_, "colemak", kHangulDefaultId, true, &out, nullptr);
    EXPECT_EQ(out, kHangulDefaultId) << "latin id in hangul slot -> default";
    resolveLayoutSlot(ctx_, "no-such-layout", kHangulDefaultId, true, &out, nullptr);
    EXPECT_EQ(out, kHangulDefaultId) << "unknown id -> default";
    resolveLayoutSlot(ctx_, "dubeolsik", kLatinDefaultId, false, &out, nullptr);
    EXPECT_EQ(out, kLatinDefaultId) << "hangul id in latin slot -> default";
}

/* 4. Save/load round trip against an isolated root. Env cleared first:
 * the loader prefers env over registry, and the store only wins when no
 * override stands. 5. An unterminated id is rejected, never truncated --
 * the same selftest key carries both, so one case owns the key's lifetime. */
TEST_F(TsfConfigTest, RegistryRoundTripAndUnterminatedId) {
    HKEY selftest = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Hunmin\\TSF-selftest", 0,
                        nullptr, 0, KEY_ALL_ACCESS, nullptr, &selftest, nullptr) !=
        ERROR_SUCCESS) {
        GTEST_SKIP() << "cannot create isolated registry root for the round trip";
    }
    HunminTsfConfig write = {};
    strcpy_s(write.hangulLayout, "sebeolsik-3-90");
    strcpy_s(write.latinLayout, "dvorak");
    strcpy_s(write.hanjaKey, "v1:120:0");
    EXPECT_EQ(hunmin_tsf_config_save(selftest, &write), TRUE) << "save returned FALSE";
    int rc = hunmin_tsf_config_load(selftest, &cfg_);
    EXPECT_GE(rc, 1) << "load from registry failed entirely";
    rc = hunmin_tsf_config_load(selftest, &cfg_);
    EXPECT_EQ(rc, 3) << "load from registry should report 3 registry-sourced slots "
                        "(the empty toggle list is not one of them)";
    EXPECT_STREQ(cfg_.hangulLayout, "sebeolsik-3-90") << "round trip hangul";
    EXPECT_STREQ(cfg_.latinLayout, "dvorak") << "round trip latin";
    EXPECT_STREQ(cfg_.hanjaKey, "v1:120:0") << "round trip hanja key";

    /* 5. An unterminated id is rejected, never truncated. */
    HunminTsfConfig bad = {};
    memset(&bad, 'x', sizeof(bad));
    EXPECT_EQ(hunmin_tsf_config_save(selftest, &bad), FALSE) << "unterminated id accepted";

    RegCloseKey(selftest);
    RegDeleteTreeA(HKEY_CURRENT_USER, "Software\\Hunmin\\TSF-selftest");
}

/* 6. Toggle bindings (Android KeyBinding.kt port): parse/match units. */
TEST_F(TsfConfigTest, KeybindingParseAndMatchUnits) {
    HunminKeyCombo got[8];
    // bare Han/Eng + bare Right Alt (165) + Shift+Space:
    EXPECT_EQ(hunmin_tsf_keybindings_parse("v1:21:0;v1:165:0;v1:32:1", got, 8), 3)
        << "parse of a 3-entry list";
    EXPECT_EQ(hunmin_tsf_keybindings_parse("v1:999:0;v1:-2:0;nonsense;v1:21:9",
                                           got, 8), 0)
        << "malformed entries parse to nothing (a bad binding never swallows a key)";
    EXPECT_EQ(hunmin_tsf_keybindings_parse("", got, 8), 0) << "empty list parses to none";
    (void)hunmin_tsf_keybindings_parse("v1:165:0", got, 8);
    EXPECT_EQ(hunmin_tsf_keybindings_match(got, 1, 165, HUNMIN_TSF_KEYMOD_SHIFT), 0)
        << "exact-modifier matching: a bare binding does not fire under a modifier";
    EXPECT_EQ(hunmin_tsf_keybindings_match(got, 1, 165, 0), 1)
        << "exact-modifier matching: the bare binding fires bare";
    EXPECT_EQ(hunmin_tsf_keymods_from_state(), 0) << "no modifiers in the clean mock state";
}

/* 6 (cont). Adapter behavior: activate over the mock layer with a configured
 * list, and drive the documented chain. The env pins every slot so
 * the host's real HKCU store cannot leak in (nondeterminism guard). One
 * case carries the whole drive: the keys chain state (toggle boundary ->
 * edit-key boundary -> document count). */
TEST_F(TsfConfigTest, AdapterKeyDriveOverTheMockLayer) {
    // Adapter behavior: activate over the mock layer with a configured
    // list, and drive the documented chain. The env pins every slot so
    // the host's real HKCU store cannot leak in (nondeterminism guard).
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
    // Right Alt (165) bare; Shift+Space also configured; event side bits.
    _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "v1:165:0;v1:32:1");

    tsfmock::MockThreadMgr *tm = new tsfmock::MockThreadMgr();
    tsfmock::MockContext *mctx = new tsfmock::MockContext();
    tsfmock::MockDocumentMgr *dm = new tsfmock::MockDocumentMgr(mctx);
    mctx->AddRef();
    CTextService *svc = new CTextService();
    svc->Activate(tm, 1);
    EXPECT_EQ(svc->toggleKeys_.size(), (size_t)3)
        << "Activate loads configured + bare Han/Eng";
    EXPECT_TRUE(svc->ctx_ && svc->ic_ && svc->hangulMode_) << "service starts in hangul mode";

    // 'r' composes in dubeolsik: consumed, preedit holds the cho.
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    BOOL eaten = FALSE;
    svc->OnTestKeyDown(mctx, 'R', 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "test keydown: an alpha key is interested in hangul mode";
    svc->OnKeyDown(mctx, 'R', 0, &eaten);
    /* dubeolsik 'r' = U+3131 (ㄱ); escapes, not literals: MSVC reads this
     * TU under the ANSI codepage and a UTF-8 literal is a C2001 (the same
     * failure suggest-test.c had when the baseline branch was measured). */
    EXPECT_EQ(eaten, TRUE) << "dubeolsik 'r' keydown not consumed";
    EXPECT_STREQ(hunmin_ic_preedit(svc->ic_), "\xE3\x84\xB1") << "dubeolsik 'r' -> preedit";

    // The bare Han/Eng key (VK_HANGEUL with NO modifier — the platform
    // alternate) toggles AND is a composition boundary: the pending
    // syllable is flushed and the word inserted exactly once.
    svc->OnTestKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "test keydown: the Han/Eng key is interested";
    svc->OnKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "bare Han/Eng keydown not consumed";
    EXPECT_FALSE(svc->hangulMode_) << "bare Han/Eng toggles to english";
    /* U+3131 wide-literal escape (the ANSI-codepage rule above). */
    EXPECT_TRUE(svc->wordbuf_.empty()) << "mode boundary flushes the held word";
    EXPECT_EQ(mctx->doc, std::wstring(1, L'\u3131')) << "mode boundary inserts once";

    // A repeat keydown (lParam bit 30) is swallowed without toggling.
    svc->OnKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0x40000000, &eaten);
    EXPECT_FALSE(svc->hangulMode_) << "auto-repeat does not toggle";

    // Shift+Space (32, mods=1) is a configured addition; bare Space is
    // not. In english mode the key passes through either way — the
    // assertion is the eat flag and the fact the mode unchanged.
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    svc->OnKeyDown(mctx, VK_SPACE, 0, &eaten);
    EXPECT_EQ(eaten, FALSE) << "bare space is not a toggle (exact mods rule)";
    EXPECT_FALSE(svc->hangulMode_) << "bare space is not a toggle (exact mods rule)";
    g_mockKeyState[VK_SHIFT & 0xFF] = 0x8000;
    svc->OnKeyDown(mctx, VK_SPACE, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "shift+space toggles back to hangul";
    EXPECT_TRUE(svc->hangulMode_) << "shift+space toggles back to hangul";
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;

    // CapsLock is no longer a toggle: it passes through.
    svc->OnTestKeyDown(mctx, VK_CAPITAL, 0, &eaten);
    EXPECT_EQ(eaten, FALSE) << "caps lock is not a toggle any more";
    BOOL eatenDown = FALSE;
    svc->OnKeyDown(mctx, VK_CAPITAL, 0, &eatenDown);
    EXPECT_EQ(eatenDown, FALSE) << "caps lock keydown passes through";
    EXPECT_TRUE(svc->hangulMode_) << "caps lock did not flip the mode";

    // Edit/navigation keys are keys of passage (measured regression, this
    // session: the 0x20-0x7E fallback translated VK_UP -> '&' etc. and
    // ate them). A pending composition is the boundary: the drain commits
    // it once, the editor receives the key itself, nothing is swallowed.
    svc->OnKeyDown(mctx, 'R', 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "compose before the edit key";
    {
        size_t insBefore = mctx->insertions.size();
        int endsBefore = mctx->compositionEnds;
        svc->OnTestKeyDown(mctx, VK_UP, 0, &eaten);
        EXPECT_EQ(eaten, FALSE) << "test keydown: the editor keeps VK_UP interest";
        svc->OnKeyDown(mctx, VK_UP, 0, &eaten);
        EXPECT_EQ(eaten, FALSE) << "VK_UP passes through (eat=FALSE)";
        /* Boundary delivery is exactly-once through ONE channel (the
         * trace runner's rule): with the composition live (the 'r' above
         * started it) the pending word fuses through the composition
         * end; only a host that refused compositions plain-inserts. */
        EXPECT_EQ(mctx->compositionEnds, endsBefore + 1)
            << "edit key is a boundary: one composition-fused delivery";
        EXPECT_EQ(mctx->compositionText, std::wstring(1, L'\u3131'))
            << "edit key is a boundary: the fused text is the commit";
        EXPECT_EQ(mctx->insertions.size(), insBefore)
            << "edit key is a boundary: no plain insert on the side";
        EXPECT_TRUE(svc->wordbuf_.empty()) << "nothing held after the edit-key boundary";
    }
    /* U+3131 wide-literal escape (the ANSI-codepage rule above); the two
     * boundary deliveries so far were the toggle boundary and this
     * edit-key boundary, both fused into the document. */
    EXPECT_EQ(mctx->doc, std::wstring(2, L'\u3131')) << "document holds 2 boundary deliveries";
    svc->OnKeyDown(mctx, VK_DELETE, 0, &eaten);
    EXPECT_EQ(eaten, FALSE) << "VK_DELETE also passes through";

    svc->Release();
    mctx->Release();
    dm->Release();
    tm->Release();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    /* The rc-77 skip contract (engine data unavailable): probe once, before
     * any assertion machinery runs. */
    _putenv_s("HUNMIN_DATA_DIR", HUNMIN_TSF_DATA_DIR);
    const char *dirs[1] = {HUNMIN_TSF_DATA_DIR};
    hunmin_ctx *probe = hunmin_ctx_new(dirs, 1);
    if (!probe) {
        fprintf(stderr, "skip: engine context unavailable (check HUNMIN_TSF_DATA_DIR)\n");
        return 77;
    }
    hunmin_ctx_free(probe);
    return RUN_ALL_TESTS();
}
