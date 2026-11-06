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
 * key (HKCU\Software\Hunmin\TSF-selftest, deleted on every exit path). One
 * case cannot avoid the production store HKCU\Software\Hunmin\TSF — the
 * AUTOSWITCH chord goes through CTextService::SpecialPerform, which saves to
 * HKEY_CURRENT_USER by design (that is a real user's path) — so the fixture
 * snapshots those six values before every case and puts them back in
 * TearDown (production-store-guard.h). Measured 2026-11-06: without the
 * restore, this file flipped Autoswitch on every run and the tier-F1 win32
 * corpus alternated pass/fail with it.
 *
 * Standalone TU (the include-.cpp seam is retired): the ctest entry
 * `tsf-config-test` selects this group with
 * --gtest_filter=TsfConfigTest.*; the rc-77 skip probe lives in
 * tools/tsf-run-if-data.py.
 *
 * GoogleTest form (issue #109, option 1b): native EXPECT_* assertions, the
 * CHECK counter gone.
 */
#include <gtest/gtest.h>

#include <cctype>
#include <cstring>
#include <string>
#include <windows.h>

#include "../src/tsf.h"
#include "mock-tsf.h"
#include "production-store-guard.h"
#include "tsf-keystate-mock.h"
#include "tsf-testdir.h"

class TsfConfigTest : public ::testing::Test {
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
        /* The AUTOSWITCH chord case writes the production store through
         * SpecialPerform -> config_save(HKEY_CURRENT_USER). Put back what was
         * there before the test touched it -- the fixture snapshot (a member,
         * so it was taken before SetUp) holds the raw registry values. */
        store_.Restore();
        if (ctx_) {
            hunmin_ctx_free(ctx_);
            ctx_ = nullptr;
        }
    }

    HunminTsfSnapshot Snap(CTextService *s) { return s->Snapshot(); }

    tsfmock::ProductionStoreSnapshot store_;
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
    /* R13-정정's ON/OFF (issue #175): OFF is the B3 default, and the flag is
     * spelled "1" -- anything else in the store is OFF (fail-closed). */
    EXPECT_EQ(cfg.autoswitch, 0) << "autoswitch defaults OFF (B3)";
    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "1");
    hunmin_tsf_config_load(nullptr, &cfg);
    EXPECT_EQ(cfg.autoswitch, 1) << "env override reaches the flag";
    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "yes");
    hunmin_tsf_config_load(nullptr, &cfg);
    EXPECT_EQ(cfg.autoswitch, 0) << "a typo must not enable the feature";
    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "");
    hunmin_tsf_config_load(nullptr, &cfg);
    EXPECT_EQ(cfg.autoswitch, 0) << "empty env autoswitch -> default";
}

/* 3. Slot resolution against the real data (issue #71's list split must
 * hold for stored values too). */
TEST_F(TsfConfigTest, SlotResolutionAgainstTheRealData) {
    std::string out;
    hunmin_tsf_resolve_layout_slot(ctx_, "dubeolsik", kHangulDefaultId, true, &out);
    EXPECT_EQ(out, "dubeolsik") << "valid hangul id kept";
    hunmin_tsf_resolve_layout_slot(ctx_, "colemak", kHangulDefaultId, true, &out);
    EXPECT_EQ(out, kHangulDefaultId) << "latin id in hangul slot -> default";
    hunmin_tsf_resolve_layout_slot(ctx_, "no-such-layout", kHangulDefaultId, true, &out);
    EXPECT_EQ(out, kHangulDefaultId) << "unknown id -> default";
    hunmin_tsf_resolve_layout_slot(ctx_, "dubeolsik", kLatinDefaultId, false, &out);
    EXPECT_EQ(out, kLatinDefaultId) << "hangul id in latin slot -> default";
}

/* 4. Save/load round trip against an isolated root. Env cleared first:
 * the loader prefers env over registry, and the store only wins when no
 * override stands. 5. An unterminated id is rejected, never truncated --
 * the same selftest key carries both, so one case owns the key's lifetime. */
TEST_F(TsfConfigTest, RegistryRoundTripAndUnterminatedId) {
    HKEY selftest = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Hunmin\\TSF-selftest", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &selftest, nullptr) !=
        ERROR_SUCCESS) {
        GTEST_SKIP() << "cannot create isolated registry root for the round trip";
    }
    HunminTsfConfig write = {};
    strcpy_s(write.hangulLayout, "sebeolsik-3-90");
    strcpy_s(write.latinLayout, "dvorak");
    strcpy_s(write.hanjaKey, "v1:120:0");
    write.autoswitch = 1; /* R13-정정's ON/OFF (issue #175) */
    EXPECT_EQ(hunmin_tsf_config_save(selftest, &write), TRUE) << "save returned FALSE";
    int rc = hunmin_tsf_config_load(selftest, &cfg_);
    EXPECT_GE(rc, 1) << "load from registry failed entirely";
    rc = hunmin_tsf_config_load(selftest, &cfg_);
    EXPECT_EQ(rc, 3) << "load from registry should report 3 registry-sourced slots "
                        "(the empty toggle list is not one of them)";
    EXPECT_STREQ(cfg_.hangulLayout, "sebeolsik-3-90") << "round trip hangul";
    EXPECT_STREQ(cfg_.latinLayout, "dvorak") << "round trip latin";
    EXPECT_STREQ(cfg_.hanjaKey, "v1:120:0") << "round trip hanja key";
    EXPECT_EQ(cfg_.autoswitch, 1) << "round trip autoswitch (R13-정정's ON/OFF)";

    /* 5. An unterminated id is rejected, never truncated. */
    HunminTsfConfig bad = {};
    memset(&bad, 'x', sizeof(bad));
    EXPECT_EQ(hunmin_tsf_config_save(selftest, &bad), FALSE) << "unterminated id accepted";

    RegCloseKey(selftest);
    RegDeleteTreeA(HKEY_CURRENT_USER, "Software\\Hunmin\\TSF-selftest");
}

/* 5b. Sin3-p2's chosung row lives on the OEM punctuation keys (the user's
 * live bug report, sandbox round 5, 2026-10-21): '/',';' and the apostrophe
 * were answered eat=FALSE at the TEST phase, and msftedit then NEVER calls
 * OnKeyDown for an uninterested key — the layout's jamo reached the app as
 * raw symbols. The test phase now declares interest for every feedable key
 * in hangul mode; the DOWN phase's engine feed decides compose-or-pass. */
TEST_F(TsfConfigTest, Sin3P2ChosungOnTheOemRow) {
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "sin3-p2");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
    tsfmock::MockThreadMgr *tm = new tsfmock::MockThreadMgr();
    tsfmock::MockContext *mctx = new tsfmock::MockContext();
    tsfmock::MockDocumentMgr *dm = new tsfmock::MockDocumentMgr(mctx);
    mctx->AddRef();
    CTextService *svc = new CTextService();
    svc->Activate(tm, 1);
    ASSERT_TRUE(Snap(svc).ctx && Snap(svc).layer) << "engine context on sin3-p2";

    BOOL eaten = FALSE;
    /* The test phase is now INTERESTED in the punctuation (: VkToBase maps
     * VK_OEM_1 = ';'). U+3142 wide-escape (the ANSI-codepage rule). */
    svc->OnTestKeyDown(mctx, VK_OEM_1, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "sin3-p2 chosung ';' interested in hangul mode";
    svc->OnKeyDown(mctx, VK_OEM_1, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "engine consumes ';' -> compose";
    EXPECT_STREQ(hunmin_buffered_preedit_composing(Snap(svc).layer), "\xE3\x85\x82") << "OEM_1 composes ㅂ like a chosung, not a symbol";

    /* The same class on the other two reported keys. */
    svc->OnTestKeyDown(mctx, VK_OEM_2, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "'/' interested";
    svc->OnKeyDown(mctx, VK_OEM_2, 0, &eaten);
    EXPECT_EQ(eaten, TRUE);
    svc->OnTestKeyDown(mctx, VK_OEM_7, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "the apostrophe interested";
    svc->OnKeyDown(mctx, VK_OEM_7, 0, &eaten);
    EXPECT_EQ(eaten, TRUE);

    /* And the DOWN phase still passes a key the layout does NOT define —
     * that contract is the existing decline branch (BoundaryFlush + pass),
     * unchanged by the interest answer; the '=' probe stays eaten here but
     * its own consume decision is the engine's. */
    svc->OnTestKeyDown(mctx, VK_OEM_PLUS, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "'=' is feedable too (interested)";

    svc->Release();
    mctx->Release();
    dm->Release();
    tm->Release();
}

/* 6. Toggle bindings (Android KeyBinding.kt port): parse/match units. */
TEST_F(TsfConfigTest, KeybindingParseAndMatchUnits) {
    HunminKeyCombo got[8];
    // bare Han/Eng + bare Right Alt (165) + Shift+Space:
    EXPECT_EQ(hunmin_tsf_keybindings_parse("v1:21:0;v1:165:0;v1:32:1", got, 8), 3) << "parse of a 3-entry list";
    EXPECT_EQ(hunmin_tsf_keybindings_parse("v1:999:0;v1:-2:0;nonsense;v1:21:9", got, 8), 0)
        << "malformed entries parse to nothing (a bad binding never swallows a key)";
    EXPECT_EQ(hunmin_tsf_keybindings_parse("", got, 8), 0) << "empty list parses to none";
    (void)hunmin_tsf_keybindings_parse("v1:165:0", got, 8);
    EXPECT_EQ(hunmin_tsf_keybindings_match(got, 1, 165, HUNMIN_TSF_KEYMOD_SHIFT), 0)
        << "exact-modifier matching: a bare binding does not fire under a modifier";
    EXPECT_EQ(hunmin_tsf_keybindings_match(got, 1, 165, 0), 1) << "exact-modifier matching: the bare binding fires bare";
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
    EXPECT_EQ(Snap(svc).toggleKeys.size(), (size_t)3) << "Activate loads configured + bare Han/Eng";
    EXPECT_TRUE(Snap(svc).ctx && Snap(svc).layer && Snap(svc).hangulMode) << "service starts in hangul mode";

    // 'r' composes in dubeolsik: consumed, preedit holds the cho.
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    BOOL eaten = FALSE;
    svc->OnTestKeyDown(mctx, 'R', 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "test keydown: an alpha key is interested in hangul mode";
    svc->OnKeyDown(mctx, 'R', 0, &eaten);
    /* dubeolsik 'r' = U+3131 (ㄱ); escapes, not literals: MSVC reads this
     * TU under the ANSI codepage and a UTF-8 literal is a C2001 (the same
     * failure candidate-tables-test.cpp had when the baseline branch was measured). */
    EXPECT_EQ(eaten, TRUE) << "dubeolsik 'r' keydown not consumed";
    EXPECT_STREQ(hunmin_buffered_preedit_composing(Snap(svc).layer), "\xE3\x84\xB1") << "dubeolsik 'r' -> preedit";

    // The bare Han/Eng key (VK_HANGEUL with NO modifier — the platform
    // alternate) toggles AND is a composition boundary: the pending
    // syllable is flushed and the word inserted exactly once. A bare press is
    // the manual switch (SPEC-MODES §E5, ruled 2026-10-31); a chord modifier
    // with it is the special functions, pinned in the case below.
    svc->OnTestKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "test keydown: the Han/Eng key is interested";
    svc->OnKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "bare Han/Eng keydown not consumed";
    EXPECT_FALSE(Snap(svc).hangulMode) << "bare Han/Eng toggles to english";
    /* U+3131 wide-literal escape (the ANSI-codepage rule above). */
    EXPECT_TRUE(Snap(svc).finished.empty()) << "mode boundary flushes the held word";
    EXPECT_EQ(mctx->doc, std::wstring(1, L'\u3131')) << "mode boundary inserts once";

    // A repeat keydown (lParam bit 30) and the compound press are the two
    // other outcomes of a held Han/Eng key; they are pinned in
    // HanEngHoldIsAPrefixNotAToggle (the mode side here must stay english for
    // the shift+space case below).

    // Shift+Space (32, mods=1) is a configured addition; bare Space is
    // not. In english mode the key passes through either way — the
    // assertion is the eat flag and the fact the mode unchanged.
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    svc->OnKeyDown(mctx, VK_SPACE, 0, &eaten);
    EXPECT_EQ(eaten, FALSE) << "bare space is not a toggle (exact mods rule)";
    EXPECT_FALSE(Snap(svc).hangulMode) << "bare space is not a toggle (exact mods rule)";
    g_mockKeyState[VK_SHIFT & 0xFF] = (SHORT)-32768;
    svc->OnKeyDown(mctx, VK_SPACE, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "shift+space toggles back to hangul";
    EXPECT_TRUE(Snap(svc).hangulMode) << "shift+space toggles back to hangul";
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;

    // CapsLock is no longer a toggle: it passes through.
    svc->OnTestKeyDown(mctx, VK_CAPITAL, 0, &eaten);
    EXPECT_EQ(eaten, FALSE) << "caps lock is not a toggle any more";
    BOOL eatenDown = FALSE;
    svc->OnKeyDown(mctx, VK_CAPITAL, 0, &eatenDown);
    EXPECT_EQ(eatenDown, FALSE) << "caps lock keydown passes through";
    EXPECT_TRUE(Snap(svc).hangulMode) << "caps lock did not flip the mode";

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
        EXPECT_EQ(mctx->compositionEnds, endsBefore + 1) << "edit key is a boundary: one composition-fused delivery";
        EXPECT_EQ(mctx->compositionText, std::wstring(1, L'\u3131')) << "edit key is a boundary: the fused text is the commit";
        EXPECT_EQ(mctx->insertions.size(), insBefore) << "edit key is a boundary: no plain insert on the side";
        EXPECT_TRUE(Snap(svc).finished.empty()) << "nothing held after the edit-key boundary";
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
/* The Han/Eng key's two positions (SPEC-MODES §E5, ruled 2026-10-31): the bare
 * press is the manual switch, and a chord modifier held with it is the
 * special-function namespace -- engine-resolved, frontend-performed. Three
 * outcomes, each pinned here because they are what the assignment has to keep
 * apart:
 *   1. a chord with a function bound AND a host that services it -> performed,
 *      and the mode does NOT switch;
 *   2. a chord bound to a function this platform refuses (VOICE has no Windows
 *      implementation) -> the key PASSES, in both the test and the down phase;
 *   3. a chord with nothing bound -> the engine degrades it to the plain
 *      toggle, so no key is ever dead.
 * The binding itself is the engine's value: this case reads it back through
 * hunmin_buffered_preedit_special_chord(), which is what the settings dialog
 * shows (R13-정정: the dialog advertises what is honoured). */
TEST_F(TsfConfigTest, TheHanEngKeyCarriesTheSpecialFunctionOnAChord) {
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
    _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "v1:165:0");

    tsfmock::MockThreadMgr *tm = new tsfmock::MockThreadMgr();
    tsfmock::MockContext *mctx = new tsfmock::MockContext();
    tsfmock::MockDocumentMgr *dm = new tsfmock::MockDocumentMgr(mctx);
    mctx->AddRef();
    CTextService *svc = new CTextService();
    svc->Activate(tm, 1);
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    ASSERT_TRUE(Snap(svc).hangulMode) << "the service starts in hangul mode";
    BOOL eaten = FALSE;

    /* The engine's ruled defaults, read through the live layer: VOICE = Alt,
     * AUTOSWITCH = Ctrl. This is the dialog's source of truth. */
    hunmin_buffered_preedit *layer = Snap(svc).layer;
    ASSERT_NE(layer, nullptr);
    EXPECT_EQ(hunmin_buffered_preedit_special_chord(layer, HUNMIN_SPECIAL_VOICE), HUNMIN_MOD_ALT);
    EXPECT_EQ(hunmin_buffered_preedit_special_chord(layer, HUNMIN_SPECIAL_AUTOSWITCH), HUNMIN_MOD_CTRL);

    /* 1. Ctrl + 한/영: AUTOSWITCH is bound and this host services it. */
    g_mockKeyState[VK_CONTROL & 0xFF] = (SHORT)-32768;
    svc->OnTestKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "a serviceable chord is claimed in the test phase";
    svc->OnKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "and consumed in the down";
    EXPECT_TRUE(Snap(svc).hangulMode) << "a chord is NOT a mode switch";
    EXPECT_EQ(Snap(svc).lastSpecial, (int)HUNMIN_SPECIAL_AUTOSWITCH) << "the engine resolved AUTOSWITCH and the host performed it";
    g_mockKeyState[VK_CONTROL & 0xFF] = 0;

    /* 2. Alt + 한/영: VOICE is bound but this platform has no recogniser, so the
     * host refuses -- and a refused chord must not be claimed by the test, or
     * the host never delivers the down at all (the `dkssud1` defect class). */
    g_mockKeyState[VK_MENU & 0xFF] = (SHORT)-32768;
    svc->OnTestKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    EXPECT_EQ(eaten, FALSE) << "a refused chord is not claimed in the test";
    svc->OnKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    EXPECT_EQ(eaten, FALSE) << "the refused chord passes in the down too";
    EXPECT_TRUE(Snap(svc).hangulMode) << "a refused chord is not a mode switch either";
    g_mockKeyState[VK_MENU & 0xFF] = 0;

    /* 3. Ctrl+Alt + 한/영: nothing is bound to that chord, so the engine
     * degrades it to the plain toggle rather than leaving a dead key. */
    g_mockKeyState[VK_CONTROL & 0xFF] = (SHORT)-32768;
    g_mockKeyState[VK_MENU & 0xFF] = (SHORT)-32768;
    svc->OnTestKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "an unbound chord is still ours (it is a toggle)";
    svc->OnKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    EXPECT_EQ(eaten, TRUE);
    EXPECT_FALSE(Snap(svc).hangulMode) << "an unbound chord is the plain toggle";
    g_mockKeyState[VK_CONTROL & 0xFF] = 0;
    g_mockKeyState[VK_MENU & 0xFF] = 0;

    svc->Release();
    mctx->Release();
    dm->Release();
    tm->Release();
}

/* R13-정정's surface A, performed by the ADAPTER (issue #175). The ruling of
 * 2026-11-01 (#164) splits the work: the ENGINE judges and delivers the word,
 * the FRONTEND adopts the mode it answers. This case pins the adapter half for
 * the two buffers the issue names -- one English-valid, one Hangul-valid -- so
 * the DECISION is asserted and not merely that a switch can happen:
 *   `english` typed in dubeolsik is a dictionary word and is not well-formed
 *   Hangul, so the mode must move to Latin; `sms` (는) is well-formed Hangul,
 *   so the mode must stay.
 * Both verdicts are tier A's own for the same buffers on the shipped dictionary
 * (wordset-test, TheZeroThreeGateMixedSentenceWorksInOneStream) -- nothing is
 * typed here. The committed text is the same either way: the judgement changes
 * the MODE, never the word (R10).
 *
 * The boundary is a SPACE, which on a real TSF host reaches the adapter in the
 * TEST phase (the key passes, so the host never calls the down phase -- the
 * `dkssud1` shape): that is why the judgement has to be taken at the boundary
 * drain, not only where a key is declined. */
TEST_F(TsfConfigTest, TheAutomaticSwitchFlipsTheModeAtTheBoundary) {
    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "1");
    _putenv_s(HUNMIN_TSF_ENV_COMMIT_UNIT, "word");
    _putenv_s("HUNMIN_TSF_INTERIM", "");
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
    _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "v1:165:0");

    tsfmock::MockThreadMgr *tm = new tsfmock::MockThreadMgr();
    tsfmock::MockContext *mctx = new tsfmock::MockContext();
    tsfmock::MockDocumentMgr *dm = new tsfmock::MockDocumentMgr(mctx);
    mctx->AddRef();
    CTextService *svc = new CTextService();
    svc->Activate(tm, 1);
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    ASSERT_TRUE(Snap(svc).hangulMode) << "the service starts in hangul mode";

    /* The stored preference reached the engine's ONE state (R13-정정: the
     * frontend holds no copy, it reads the engine's answer). */
    hunmin_buffered_preedit *layer = Snap(svc).layer;
    ASSERT_NE(layer, nullptr);
    int on = 0;
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(layer, "autoswitch", &on)) << "the layer answers the option R13-정정 names";
    EXPECT_EQ(on, 1) << "the stored value reached the layer";

    auto type = [&](const char *keys) {
        for (const char *p = keys; *p; ++p) {
            BOOL eat = FALSE;
            svc->OnTestKeyDown(mctx, (WPARAM)std::toupper((unsigned char)*p), 0, &eat);
            svc->OnKeyDown(mctx, (WPARAM)std::toupper((unsigned char)*p), 0, &eat);
        }
        BOOL eat = FALSE;
        svc->OnTestKeyDown(mctx, VK_SPACE, 0, &eat);
        EXPECT_EQ(eat, FALSE) << "a boundary key passes (it is the app's space)";
        svc->OnKeyDown(mctx, VK_SPACE, 0, &eat);
    };

    /* 1. A Hangul-valid buffer: 는 is well-formed Hangul, so R8 answers "keep
     * the current mode" and nothing switches. */
    type("sms");
    EXPECT_TRUE(Snap(svc).hangulMode) << "a Hangul-valid word must not switch the mode";

    /* 2. An English-valid buffer: `english` reads as a dictionary word and is
     * not well-formed Hangul, so the mode must be Latin from the next key. */
    type("english");
    EXPECT_FALSE(Snap(svc).hangulMode) << "an English-valid word must switch the mode to Latin";

    /* The word landed in the client, in the language the judgement decided,
     * and nothing was reassembled (R10): 는 then english. */
    EXPECT_EQ(mctx->doc, std::wstring(L"\uB294") + L"english") << "the judged words are the delivered text";

    /* Process-wide seams back to their defaults: this binary runs every case in
     * one process and a sibling must not inherit autoswitch ON (the measured
     * leak trap: a case that passes under --gtest_filter and fails in the suite). */
    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "");
    _putenv_s("HUNMIN_TSF_INTERIM", "");

    svc->Release();
    mctx->Release();
    dm->Release();
    tm->Release();
}

/* The English -> Hangul direction, ruled 2026-11-02: *"autoswitch ON stops
 * English passthrough to support eng -> han support"*. Before this the adapter
 * returned at `if (!hangulMode_) return S_OK;` and the engine never saw a key
 * typed in Latin mode, so R8's E1 line (and the 0.3 gate sentence) could not
 * fire on TSF at all.
 *
 * One service, two words: `english` switches Hangul -> Latin (the direction the
 * previous case pins), then `sms` typed IN Latin mode must switch back and
 * deliver 는 -- the engine's own answer for those keys on dubeolsik, which is
 * what the Hangul reading of a Latin-mode buffer is (tier A:
 * WordSetTest.TheZeroThreeGateMixedSentenceWorksInOneStream). The second word
 * only reaches the engine because the passthrough stopped.
 */
TEST_F(TsfConfigTest, TheEnglishDirectionIsJudgedOnceTheSwitchIsOn) {
    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "1");
    _putenv_s(HUNMIN_TSF_ENV_COMMIT_UNIT, "word");
    _putenv_s("HUNMIN_TSF_INTERIM", "");
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
    _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "v1:165:0");

    tsfmock::MockThreadMgr *tm = new tsfmock::MockThreadMgr();
    tsfmock::MockContext *mctx = new tsfmock::MockContext();
    tsfmock::MockDocumentMgr *dm = new tsfmock::MockDocumentMgr(mctx);
    mctx->AddRef();
    CTextService *svc = new CTextService();
    svc->Activate(tm, 1);
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    ASSERT_TRUE(Snap(svc).hangulMode) << "the service starts in hangul mode";

    auto type = [&](const char *keys, BOOL *testEatOut) {
        for (const char *p = keys; *p; ++p) {
            BOOL eat = FALSE;
            svc->OnTestKeyDown(mctx, (WPARAM)std::toupper((unsigned char)*p), 0, &eat);
            if (testEatOut)
                *testEatOut = eat;
            svc->OnKeyDown(mctx, (WPARAM)std::toupper((unsigned char)*p), 0, &eat);
        }
        BOOL eat = FALSE;
        svc->OnTestKeyDown(mctx, VK_SPACE, 0, &eat);
        svc->OnKeyDown(mctx, VK_SPACE, 0, &eat);
    };

    type("english", nullptr);
    ASSERT_FALSE(Snap(svc).hangulMode) << "Hangul -> Latin: the first direction still works";

    /* The ruling's observable: a Latin-mode key is OURS now, so the engine sees
     * it (the test phase claims it instead of passing it to the app). */
    BOOL eat = FALSE;
    type("sms", &eat);
    EXPECT_TRUE(eat) << "with autoswitch ON a Latin-mode key is fed, not passed through";
    EXPECT_TRUE(Snap(svc).hangulMode) << "Latin -> Hangul: the word reads as Hangul, so the mode moves back";
    EXPECT_EQ(mctx->doc, std::wstring(L"english") + L"\uB294")
        << "the judged words are the delivered text, in the language the judgement decided";

    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "");
    svc->Release();
    mctx->Release();
    dm->Release();
    tm->Release();
}

/* The same keys with the switch OFF: the passthrough is exactly what shipped
 * (R5's per-character English, delivered by the host, judged by nobody), so the
 * ruling above did not change the feature-off path. */
TEST_F(TsfConfigTest, ThePassthroughStaysWhenTheSwitchIsOff) {
    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "0");
    _putenv_s(HUNMIN_TSF_ENV_COMMIT_UNIT, "word");
    _putenv_s("HUNMIN_TSF_INTERIM", "");
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
    _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "v1:165:0");

    tsfmock::MockThreadMgr *tm = new tsfmock::MockThreadMgr();
    tsfmock::MockContext *mctx = new tsfmock::MockContext();
    tsfmock::MockDocumentMgr *dm = new tsfmock::MockDocumentMgr(mctx);
    mctx->AddRef();
    CTextService *svc = new CTextService();
    svc->Activate(tm, 1);
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    EXPECT_EQ(Snap(svc).autoswitch, 0) << "the engine holds OFF";

    /* Type in Hangul mode, then toggle to Latin with the 한/영 key: with the
     * switch off, a Latin-mode letter must be the app's again. */
    for (const char *p = "sms"; *p; ++p) {
        BOOL eat = FALSE;
        svc->OnTestKeyDown(mctx, (WPARAM)std::toupper((unsigned char)*p), 0, &eat);
        svc->OnKeyDown(mctx, (WPARAM)std::toupper((unsigned char)*p), 0, &eat);
    }
    BOOL spaceEat = TRUE;
    svc->OnTestKeyDown(mctx, VK_SPACE, 0, &spaceEat);
    svc->OnKeyDown(mctx, VK_SPACE, 0, &spaceEat);
    EXPECT_TRUE(Snap(svc).hangulMode) << "the switch is off: nothing judges";

    BOOL langEat = FALSE;
    svc->OnTestKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &langEat);
    svc->OnKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &langEat);
    ASSERT_FALSE(Snap(svc).hangulMode) << "the manual switch still reaches Latin";

    BOOL eat = TRUE;
    svc->OnTestKeyDown(mctx, 'D', 0, &eat);
    EXPECT_FALSE(eat) << "switch OFF: a Latin-mode key passes to the app (R5, unchanged)";

    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "");
    svc->Release();
    mctx->Release();
    dm->Release();
    tm->Release();
}

/* Ruled 2026-11-02: "interim model cannot support autoswitch". The delivery that
 * cannot judge what it delivers must not hold the state either -- otherwise the
 * settings row would advertise a switch that cannot fire, and the newly-fed key
 * path would stop the app's own English insertion for nothing. */
TEST_F(TsfConfigTest, TheInterimDeliveryDoesNotHoldTheSwitch) {
    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "1");
    _putenv_s("HUNMIN_TSF_INTERIM", "1");
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
    _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "v1:165:0");

    tsfmock::MockThreadMgr *tm = new tsfmock::MockThreadMgr();
    tsfmock::MockContext *mctx = new tsfmock::MockContext();
    tsfmock::MockDocumentMgr *dm = new tsfmock::MockDocumentMgr(mctx);
    mctx->AddRef();
    CTextService *svc = new CTextService();
    svc->Activate(tm, 1);
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    ASSERT_TRUE(Snap(svc).legacyMode) << "the interim delivery is the mode under test";

    /* The store says 1, the LAYER says 0: the frontend refused it. */
    hunmin_buffered_preedit *layer = Snap(svc).layer;
    ASSERT_NE(layer, nullptr);
    int on = -1;
    ASSERT_TRUE(hunmin_buffered_preedit_get_option(layer, "autoswitch", &on));
    EXPECT_EQ(on, 0) << "the interim delivery cannot judge, so it does not hold the switch";
    EXPECT_EQ(Snap(svc).autoswitch, 0) << "the snapshot reports the engine's answer, not the store's";

    /* And the key path agrees: a Latin-mode key is still the app's. */
    BOOL langEat = FALSE;
    svc->OnTestKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &langEat);
    svc->OnKeyDown(mctx, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &langEat);
    ASSERT_FALSE(Snap(svc).hangulMode) << "the manual switch still reaches Latin";
    BOOL eat = TRUE;
    svc->OnTestKeyDown(mctx, 'D', 0, &eat);
    EXPECT_FALSE(eat) << "interim + refused switch: the passthrough stands";

    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "");
    _putenv_s("HUNMIN_TSF_INTERIM", "");
    svc->Release();
    mctx->Release();
    dm->Release();
    tm->Release();
}

/* The native-path half of the duplicate reported in issue #175's comment
 * (2026-11-01): a word in flight, the host terminates the composition, then a
 * boundary arrives before any new composition re-adopts the fused copy. The
 * host's termination already put the display in the document, so the boundary
 * must add NOTHING -- the reported duplicate was `적어두자.적어두자.`.
 *
 * The assertion is an INVARIANT (the document does not grow), not a typed word:
 * what matters is that the second delivery is gone, whatever the layout renders.
 */
TEST_F(TsfConfigTest, HostTerminationThenBoundaryKeepsTheWordOnce) {
    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "0");
    _putenv_s(HUNMIN_TSF_ENV_COMMIT_UNIT, "word");
    _putenv_s("HUNMIN_TSF_INTERIM", "");
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "sin3-p2");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
    _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "v1:165:0");
    tsfmock::MockThreadMgr *tm = new tsfmock::MockThreadMgr();
    tsfmock::MockContext *mctx = new tsfmock::MockContext();
    tsfmock::MockDocumentMgr *dm = new tsfmock::MockDocumentMgr(mctx);
    mctx->AddRef();
    CTextService *svc = new CTextService();
    svc->Activate(tm, 1);
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    ASSERT_FALSE(Snap(svc).legacyMode) << "this case is the native path";

    auto key = [&](WPARAM vk) {
        BOOL eat = FALSE;
        svc->OnTestKeyDown(mctx, vk, 0, &eat);
        svc->OnKeyDown(mctx, vk, 0, &eat);
    };
    for (const char *p = "DKSSUD"; *p; ++p)
        key((WPARAM)*p);
    const std::wstring fused = mctx->doc;
    ASSERT_FALSE(fused.empty()) << "the composition text is document text in this harness";

    /* The host's mid-word termination (focus loss). */
    mctx->HostTerminateComposition();
    svc->OnCompositionTerminated(tsfmock::MockContext::kCookie, nullptr);

    /* The next key is a boundary: the word is already in the document. */
    key(VK_SPACE);
    EXPECT_EQ(mctx->doc, fused) << "the boundary must not re-deliver what the host already fused (doc len " << mctx->doc.size() << ")";

    _putenv_s("HUNMIN_TSF_INTERIM", "");
    svc->Release();
    mctx->Release();
    dm->Release();
    tm->Release();
}

/* The punctuation pair from the report, on the native path: `?` and `,` are
 * LAYOUT KEYS on sin3-p2 (consumed into the word), so Enter is the key that
 * delivers the word and it must pass in BOTH key phases. The expectation is the
 * engine's own display -- the layout renders the punctuation its own way, and a
 * hand-typed glyph asserts the wrong thing.
 */
TEST_F(TsfConfigTest, QuestionAndCommaThenEnterOnTheNativePath) {
    _putenv_s(HUNMIN_TSF_ENV_AUTOSWITCH, "0");
    _putenv_s(HUNMIN_TSF_ENV_COMMIT_UNIT, "word");
    _putenv_s("HUNMIN_TSF_INTERIM", "");
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "sin3-p2");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
    _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "v1:165:0");
    tsfmock::MockThreadMgr *tm = new tsfmock::MockThreadMgr();
    tsfmock::MockContext *mctx = new tsfmock::MockContext();
    tsfmock::MockDocumentMgr *dm = new tsfmock::MockDocumentMgr(mctx);
    mctx->AddRef();
    CTextService *svc = new CTextService();
    svc->Activate(tm, 1);
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    ASSERT_FALSE(Snap(svc).legacyMode) << "this case is the native path";

    const WPARAM puncts[] = {VK_OEM_2, VK_OEM_COMMA};
    for (int i = 0; i < 2; ++i) {
        for (const char *p = "DKSSUD"; *p; ++p) {
            BOOL eat = FALSE;
            svc->OnTestKeyDown(mctx, (WPARAM)*p, 0, &eat);
            svc->OnKeyDown(mctx, (WPARAM)*p, 0, &eat);
        }
        BOOL punctEat = FALSE;
        svc->OnTestKeyDown(mctx, puncts[i], 0, &punctEat);
        svc->OnKeyDown(mctx, puncts[i], 0, &punctEat);
        EXPECT_TRUE(punctEat) << "sin3-p2 composes this punctuation: it belongs to the word (case " << i << ")";
        const std::wstring shown = Snap(svc).display;
        ASSERT_FALSE(shown.empty()) << "the punctuation must still be composing with the word (case " << i << ")";

        BOOL testEat = TRUE, downEat = TRUE;
        svc->OnTestKeyDown(mctx, VK_RETURN, 0, &testEat);
        svc->OnKeyDown(mctx, VK_RETURN, 0, &downEat);
        EXPECT_EQ(testEat, FALSE) << "Enter passes at the test phase, so the app inserts the break (case " << i << ")";
        EXPECT_EQ(downEat, FALSE) << "and the down phase agrees (case " << i << ")";
        EXPECT_NE(mctx->doc.find(shown), std::wstring::npos) << "the word with its punctuation landed (case " << i << ")";
    }

    _putenv_s("HUNMIN_TSF_INTERIM", "");
    svc->Release();
    mctx->Release();
    dm->Release();
    tm->Release();
}
