/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1 (issue #71, the settings-dialog half): the TSF-native plumbing
 * (ITfFunctionProvider on the text service, ITfFnConfigure behind it) and
 * the pure dialog helpers (the layout-row split by
 * hunmin_ctx_layout_mode), against the real data dir.
 *
 * NOT in an automated test: the modal dialog loop itself (a blocking
 * window in a CI binary) and ITfFnConfigure::Show — those are the manual
 * checklist's surface (tests/manual-checklists/tsf-settings-window.md).
 *
 * Standalone TU (the include-.cpp seam is retired): the ctest entry
 * `tsf-settings-test` selects this group with
 * --gtest_filter=TsfSettingsTest.*; the rc-77 skip probe lives in
 * tools/tsf-run-if-data.py.
 *
 * GoogleTest form (issue #109, option 1b): native EXPECT_* assertions, the
 * CHECK counter gone.
 */
#include <gtest/gtest.h>

#include <cstring>
#include <ctffunc.h>
#include <string>
#include <windows.h>

#include "../src/settings.h"
#include "../src/tsf.h"
#include "mock-tsf.h"
#include "tsf-keystate-mock.h"
#include "tsf-testdir.h"

class TsfSettingsTest : public ::testing::Test {
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
        releaseService();
        if (ctx_) {
            hunmin_ctx_free(ctx_);
            ctx_ = nullptr;
        }
    }
    /* A live adapter over the mock layer: env must be pinned BEFORE this,
     * since Activate loads the config. */
    void activate() {
        tm_ = new tsfmock::MockThreadMgr();
        mctx_ = new tsfmock::MockContext();
        dm_ = new tsfmock::MockDocumentMgr(mctx_);
        mctx_->AddRef();
        svc_ = new CTextService();
        svc_->Activate(tm_, 1);
    }
    void releaseService() {
        if (!svc_)
            return;
        svc_->Release();
        mctx_->Release();
        dm_->Release();
        tm_->Release();
        svc_ = nullptr;
        mctx_ = nullptr;
        dm_ = nullptr;
        tm_ = nullptr;
    }

    HunminTsfSnapshot Snap() { return svc_->Snapshot(); }

    hunmin_ctx *ctx_ = nullptr;
    tsfmock::MockThreadMgr *tm_ = nullptr;
    tsfmock::MockContext *mctx_ = nullptr;
    tsfmock::MockDocumentMgr *dm_ = nullptr;
    CTextService *svc_ = nullptr;
};

/* 1. The layout-row split (the dialog's list source): hangul rows come
 * only from hangul-mode layouts, latin rows only from latin-mode ones,
 * the ids never overlap, and the stored fcitx5 defaults appear in the
 * right slots. */
TEST_F(TsfSettingsTest, LayoutRowSplit) {
    HunminTsfLayoutRow hang[HUNMIN_TSF_SETTINGS_MAX_ROWS];
    HunminTsfLayoutRow lat[HUNMIN_TSF_SETTINGS_MAX_ROWS];
    int nh = hunmin_tsf_rows_for_mode(ctx_, true, hang, HUNMIN_TSF_SETTINGS_MAX_ROWS);
    int nl = hunmin_tsf_rows_for_mode(ctx_, false, lat, HUNMIN_TSF_SETTINGS_MAX_ROWS);
    EXPECT_GT(nh, 0) << "the hangul half lists nothing";
    EXPECT_GT(nl, 0) << "the latin half lists nothing";
    bool hasDubeol = false, hasUsQwerty = false;
    for (int i = 0; i < nh; ++i) {
        const char *mode = hunmin_ctx_layout_mode(ctx_, hang[i].index);
        ASSERT_NE(mode, nullptr) << "row " << i << " in the hangul list has no mode";
        EXPECT_STREQ(mode, "hangul") << "row " << i << " in the hangul list is hangul-mode";
        if (strcmp(hang[i].id, "dubeolsik") == 0) {
            hasDubeol = true;
            EXPECT_EQ(hang[i].supported, 1) << "dubeolsik row reports engine support";
        }
    }
    for (int i = 0; i < nl; ++i) {
        const char *mode = hunmin_ctx_layout_mode(ctx_, lat[i].index);
        ASSERT_NE(mode, nullptr) << "row " << i << " in the latin list has no mode";
        EXPECT_STREQ(mode, "latin") << "row " << i << " in the latin list is latin-mode";
        if (strcmp(lat[i].id, "us-qwerty") == 0)
            hasUsQwerty = true;
    }
    EXPECT_TRUE(hasDubeol) << "the hangul list carries dubeolsik (the stored default slot)";
    EXPECT_TRUE(hasUsQwerty) << "the latin list carries us-qwerty";
    EXPECT_NE(hang[0].name[0], '\0') << "hangul rows carry a display name from hunmin_ctx_layout_name(..., ko)";
    EXPECT_NE(lat[0].name[0], '\0') << "latin rows carry a display name from hunmin_ctx_layout_name(..., ko)";
}

/* 2. The TSF-native plumbing on the live adapter (issue #71): the text
 * service QIs for ITfFunctionProvider, whose GetFunction hands the
 * ITfFnConfigure object (GetDisplayName + modal Show). */
TEST_F(TsfSettingsTest, FunctionProviderAndConfigureFunction) {
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
    _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "v1:21:1");
    activate();

    IUnknown *provUnk = nullptr;
    HRESULT hr = svc_->QueryInterface(IID_ITfFunctionProvider, (void **)&provUnk);
    EXPECT_EQ(hr, S_OK) << "QI(ITfFunctionProvider) on the text service";
    ASSERT_NE(provUnk, nullptr) << "QI(ITfFunctionProvider) returned no object";
    ITfFunctionProvider *prov = nullptr;
    hr = provUnk->QueryInterface(IID_ITfFunctionProvider, (void **)&prov);
    EXPECT_EQ(hr, S_OK) << "provider QI self-check";
    ASSERT_NE(prov, nullptr) << "provider QI self-check returned no object";
    GUID type = {};
    hr = prov->GetType(&type);
    EXPECT_EQ(hr, S_OK) << "provider GetType";
    EXPECT_NE(type.Data1, 0u) << "provider GetType (fresh GUID)";
    BSTR desc = nullptr;
    hr = prov->GetDescription(&desc);
    EXPECT_EQ(hr, S_OK) << "provider GetDescription";
    ASSERT_NE(desc, nullptr) << "provider GetDescription returned no BSTR";
    EXPECT_GT(SysStringLen(desc), 0u) << "provider GetDescription: empty BSTR";
    SysFreeString(desc);
    /* And a random IID is refused - the surface is not a grab-bag.
     * Distinct out-pointer: a failed QI must not clobber `prov`. */
    ITfFunctionProvider *wrong = (ITfFunctionProvider *)0x1;
    hr = prov->QueryInterface(IID_ITfKeyEventSink, (void **)&wrong);
    EXPECT_EQ(hr, E_NOINTERFACE) << "provider refuses unrelated interfaces";
    EXPECT_EQ(wrong, nullptr) << "a failed QI clobbered the out-pointer";

    /* GetFunction -> configure function, WHILE `prov` is still the
     * live reference (it is released below). */
    IUnknown *fnUnk = nullptr;
    hr = prov->GetFunction(GUID_NULL, IID_ITfFnConfigure, &fnUnk);
    EXPECT_EQ(hr, S_OK) << "GetFunction(IID_ITfFnConfigure)";
    prov->Release();
    prov = nullptr;
    ASSERT_NE(fnUnk, nullptr) << "GetFunction(IID_ITfFnConfigure) returned no object";
    ITfFnConfigure *fn = nullptr;
    hr = fnUnk->QueryInterface(IID_ITfFnConfigure, (void **)&fn);
    EXPECT_EQ(hr, S_OK) << "configure fn QI";
    ASSERT_NE(fn, nullptr) << "configure fn QI returned no object";
    ITfFunction *fnBase = nullptr;
    hr = fn->QueryInterface(IID_ITfFunction, (void **)&fnBase);
    EXPECT_EQ(hr, S_OK) << "ITfFnConfigure QIs its base ITfFunction";
    if (fnBase)
        fnBase->Release();
    BSTR name = nullptr;
    hr = fn->GetDisplayName(&name);
    EXPECT_EQ(hr, S_OK) << "ITfFnConfigure::GetDisplayName (the language-bar Tools label)";
    ASSERT_NE(name, nullptr) << "ITfFnConfigure::GetDisplayName returned no BSTR";
    EXPECT_GT(SysStringLen(name), 0u) << "ITfFnConfigure::GetDisplayName: empty BSTR";
    SysFreeString(name);
    fn->Release();
    fnUnk->Release();
}

/* 2b. R13-정정 surface B (issue #175): the settings row is DERIVED from the
 * engine. The dialog spells neither the value nor the key, so this case pins
 * what the engine supplies: the chord it holds for HUNMIN_SPECIAL_AUTOSWITCH
 * appears in the label, the label carries the platform's name for the physical
 * Han/Eng key, and the value it reports is the engine's answer -- a stored 1
 * that the engine refused (data absent) must read 0 with `available` 0, which
 * is how "off by choice" and "off, data missing" stay tellable apart. */
TEST_F(TsfSettingsTest, TheAutomaticSwitchRowComesFromTheEngine) {
    HunminTsfAutoswitchRow row;

    /* The installed tree: the data gate passes, the chord is the engine's
     * default (Ctrl), and the stored 1 round-trips through the layer. */
    hunmin_tsf_autoswitch_row(ctx_, "dubeolsik", "us-qwerty", 1, &row);
    EXPECT_EQ(row.available, 1) << "libhunmin/data carries modes/en-US.json";
    EXPECT_EQ(row.value, 1) << "the stored value reached the engine and came back";
    EXPECT_STREQ(row.key, "Ctrl+\xED\x95\x9C/\xEC\x98\x81") << "the chord is the engine's, the key name is the platform's (UTF-8 한/영)";
    EXPECT_NE(std::string(row.label).find(row.key), std::string::npos) << "the label names the key: '" << row.label << "'";
    /* Issue #215, ruled 2026-11-05: the dialog has no host handle, so the label
     * names the condition ("지원 앱에서만") instead of the host's delivery state. */
    EXPECT_STREQ(row.label,
                 "\xEC\x9E\x90\xEB\x8F\x99\x20\xED\x95\x9C\x2F\xEC\x98\x81\x20\xEC\xA0\x84\xED\x99\x98\x20\x28\xEC\xA7\x80\xEC\x9B\x90"
                 "\x20\xEC\x95\xB1\xEC\x97\x90\xEC\x84\x9C\xEB\xA7\x8C\x2C\x20\x43\x74\x72\x6C\x2B\xED\x95\x9C\x2F\xEC\x98\x81\x29")
        << "the label qualifies the host without guessing it: '" << row.label << "'";

    /* Stored OFF: the same row, unchecked -- the value is read, not assumed. */
    hunmin_tsf_autoswitch_row(ctx_, "dubeolsik", "us-qwerty", 0, &row);
    EXPECT_EQ(row.available, 1);
    EXPECT_EQ(row.value, 0) << "off by choice is a value the engine answers";

    /* No context at all is the "data missing" state as far as the row can tell:
     * the control is disabled AND the label says why (R13-정정: discoverability
     * applies to the disabled state too). */
    hunmin_tsf_autoswitch_row(nullptr, nullptr, nullptr, 1, &row);
    EXPECT_EQ(row.available, 0) << "no data gate answer means not available";
    EXPECT_EQ(row.value, 0) << "a stored 1 cannot be shown as ON when the data is absent";
    EXPECT_NE(std::string(row.label).find(row.key), std::string::npos) << "the disabled row still names the key: '" << row.label << "'";
    EXPECT_STREQ(row.label,
                 "\xEC\x9E\x90\xEB\x8F\x99\x20\xED\x95\x9C\x2F\xEC\x98\x81\x20\xEC\xA0\x84\xED\x99\x98\x20\x28\xEC\xA7\x80\xEC\x9B\x90"
                 "\x20\xEC\x95\xB1\xEC\x97\x90\xEC\x84\x9C\xEB\xA7\x8C\x2C\x20\x43\x74\x72\x6C\x2B\xED\x95\x9C\x2F\xEC\x98\x81\x29"
                 "\x20\xE2\x80\x94\x20\xEC\x82\xAC\xEC\xA0\x84\x20\xEC\x97\x86\xEC\x9D\x8C\x3A\x20\xEC\x82\xAC\xEC\x9A\xA9\xED\x95\xA0"
                 "\x20\xEC\x88\x98\x20\xEC\x97\x86\xEC\x9D\x8C")
        << "the disabled row says which data is missing: '" << row.label << "'";
    EXPECT_NE(row.label[0], '\0') << "a disabled row that says nothing is the defect this guards";
}

/* 3. ApplyConfigStore re-reads the store into the LIVE adapter (env
 * seam, never a registry write): layouts and toggle keys move without
 * a process restart. 4. The void* bridge (settings.cpp's Show path
 * re-applies via it) continues on the same live service, so one case
 * carries both halves in order. */
TEST_F(TsfSettingsTest, ApplyConfigStoreAndTheVoidPtrBridge) {
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
    _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
    _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "v1:21:1");
    activate();

    svc_->ApplyConfigStore();
    EXPECT_EQ(Snap().hangulId, "dubeolsik") << "applied hangul slot";
    EXPECT_EQ(Snap().latinId, "us-qwerty") << "applied latin slot";
    /* v1:21:1 (Han/Eng + Shift) + the bare 0x15: the configured chord ADDs
     * to the bare Han/Eng key. */
    EXPECT_EQ(Snap().toggleKeys.size(), (size_t)2);
    bool bareHangeul = false, shiftedHangeul = false;
    for (const HunminKeyCombo &c : Snap().toggleKeys) {
        if (c.vk == HUNMIN_TSF_VIRTUAL_HANGEUL && c.mods == 0)
            bareHangeul = true;
        if (c.vk == HUNMIN_TSF_VIRTUAL_HANGEUL && c.mods == HUNMIN_TSF_KEYMOD_SHIFT)
            shiftedHangeul = true;
    }
    EXPECT_TRUE(bareHangeul) << "the bare Han/Eng key survives every Apply";
    EXPECT_TRUE(shiftedHangeul) << "the configured v1:21:1 chord is present";
    EXPECT_NE(Snap().layer, nullptr) << "the engine context is rebuilt after Apply";

    /* 4. The void* bridge (settings.cpp's Show path re-applies via it). */
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "sebeolsik-3-90");
    hunmin_tsf_apply_config_store(svc_);
    EXPECT_EQ(Snap().hangulId, "sebeolsik-3-90") << "apply bridge re-read";
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "");
    hunmin_tsf_apply_config_store(nullptr); /* no crash on a dead service */

    /* Composition applied over the mock layer still types: 'r' composes
     * after the second Apply rebuilt ic_ with dubeolsik again. */
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
    svc_->ApplyConfigStore();
    BOOL eaten = FALSE;
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    svc_->OnKeyDown(mctx_, 'R', 0, &eaten);
    EXPECT_EQ(eaten, TRUE) << "post-Apply keydown not consumed";
    EXPECT_STREQ(hunmin_buffered_preedit_composing(Snap().layer), "\xE3\x84\xB1") << "post-Apply composition still works";
}
