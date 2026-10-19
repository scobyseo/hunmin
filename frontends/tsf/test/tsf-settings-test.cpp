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
 * Exit 0 clean, 1 assertion failed, 77 skip (engine data unavailable).
 *
 * GoogleTest form (issue #109, option 1b): one ctest entry is kept
 * (tsf-settings-test); the 77-skip contract is decided in main(), before
 * RUN_ALL_TESTS. The CHECK macro is backed by ADD_FAILURE() so every
 * original message stays verbatim and non-fatal, exactly as the failure
 * counter did.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <ctffunc.h>
#include <windows.h>

#include "tsf-testdir.h"

extern "C" HINSTANCE g_hunmin_hinst = nullptr;

/* The GetKeyState seam, as in tsf-config-test.cpp: the adapter read the
 * thread input state. */
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

/* printf-style format → a gtest failure line. Every original CHECK message
 * survives unchanged; only the reporting moves from a failures counter to
 * the framework's per-case result. */
static std::string tsf_fmt(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return std::string(buf);
}
#define CHECK(cond, ...)               \
    do {                               \
        if (!(cond)) {                 \
            ADD_FAILURE() << tsf_fmt(__VA_ARGS__); \
        }                              \
    } while (0)

class TsfSettingsTest : public ::testing::Test {
 protected:
    void SetUp() override {
        _putenv_s("HUNMIN_DATA_DIR", HUNMIN_TSF_DATA_DIR);
        const char *dirs[1] = {HUNMIN_TSF_DATA_DIR};
        ctx_ = hunmin_ctx_new(dirs, 1);
        ASSERT_NE(ctx_, nullptr) << "engine context unavailable (check HUNMIN_TSF_DATA_DIR)";
        g_mockKeyState[VK_SHIFT & 0xFF] = 0;
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
    CHECK(nh > 0 && nl > 0, "both halves list something (hangul=%d latin=%d)", nh, nl);
    bool hasDubeol = false, hasUsQwerty = false;
    for (int i = 0; i < nh; ++i) {
        const char *mode = hunmin_ctx_layout_mode(ctx_, hang[i].index);
        CHECK(mode && strcmp(mode, "hangul") == 0, "row %d in the hangul list is hangul-mode",
              i);
        CHECK(strcmp(hang[i].id, "dubeolsik") != 0 || hang[i].supported == 1,
              "dubeolsik row reports engine support");
        if (strcmp(hang[i].id, "dubeolsik") == 0)
            hasDubeol = true;
    }
    for (int i = 0; i < nl; ++i) {
        const char *mode = hunmin_ctx_layout_mode(ctx_, lat[i].index);
        CHECK(mode && strcmp(mode, "latin") == 0, "row %d in the latin list is latin-mode", i);
        if (strcmp(lat[i].id, "us-qwerty") == 0)
            hasUsQwerty = true;
    }
    CHECK(hasDubeol, "the hangul list carries dubeolsik (the stored default slot)");
    CHECK(hasUsQwerty, "the latin list carries us-qwerty");
    CHECK(hang[0].name[0] != '\0' && lat[0].name[0] != '\0',
          "rows carry a display name from hunmin_ctx_layout_name(..., ko)");
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
    CHECK(hr == S_OK && provUnk, "QI(ITfFunctionProvider) on the text service: 0x%lX",
          (unsigned long)hr);
    if (provUnk) {
        ITfFunctionProvider *prov = nullptr;
        hr = provUnk->QueryInterface(IID_ITfFunctionProvider, (void **)&prov);
        CHECK(hr == S_OK && prov, "provider QI self-check: 0x%lX", (unsigned long)hr);
        if (prov) {
            GUID type = {};
            hr = prov->GetType(&type);
            CHECK(hr == S_OK && type.Data1 != 0, "provider GetType (fresh GUID)");
            BSTR desc = nullptr;
            hr = prov->GetDescription(&desc);
            CHECK(hr == S_OK && desc && SysStringLen(desc) > 0, "provider GetDescription");
            if (desc)
                SysFreeString(desc);
            /* And a random IID is refused - the surface is not a grab-bag.
             * Distinct out-pointer: a failed QI must not clobber `prov`. */
            ITfFunctionProvider *wrong = (ITfFunctionProvider *)0x1;
            hr = prov->QueryInterface(IID_ITfKeyEventSink, (void **)&wrong);
            CHECK(hr == E_NOINTERFACE && wrong == nullptr,
                  "provider refuses unrelated interfaces");

            /* GetFunction -> configure function, WHILE `prov` is still the
             * live reference (it is released below). */
            IUnknown *fnUnk = nullptr;
            hr = prov->GetFunction(GUID_NULL, IID_ITfFnConfigure, &fnUnk);
            CHECK(hr == S_OK && fnUnk, "GetFunction(IID_ITfFnConfigure): 0x%lX",
                  (unsigned long)hr);
            prov->Release();
            if (fnUnk) {
                ITfFnConfigure *fn = nullptr;
                hr = fnUnk->QueryInterface(IID_ITfFnConfigure, (void **)&fn);
                CHECK(hr == S_OK && fn, "configure fn QI: 0x%lX", (unsigned long)hr);
                if (fn) {
                    ITfFunction *fnBase = nullptr;
                    hr = fn->QueryInterface(IID_ITfFunction, (void **)&fnBase);
                    CHECK(hr == S_OK && fnBase, "ITfFnConfigure QIs its base ITfFunction");
                    if (fnBase)
                        fnBase->Release();
                    BSTR name = nullptr;
                    hr = fn->GetDisplayName(&name);
                    CHECK(hr == S_OK && name && SysStringLen(name) > 0,
                          "ITfFnConfigure::GetDisplayName (the language-bar Tools label)");
                    if (name)
                        SysFreeString(name);
                    fn->Release();
                }
                fnUnk->Release();
            }
        } else {
            provUnk->Release();
        }
    }
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
    CHECK(svc_->hangulId_ == "dubeolsik", "applied hangul slot: got %s",
          svc_->hangulId_.c_str());
    CHECK(svc_->latinId_ == "us-qwerty", "applied latin slot: got %s", svc_->latinId_.c_str());
    CHECK(svc_->toggleKeys_.size() == 2, /* v1:21:1 (Han/Eng + Shift) + the bare 0x15 */
          "configured chord ADDs to the bare Han/Eng key");
    bool bareHangeul = false, shiftedHangeul = false;
    for (const HunminKeyCombo &c : svc_->toggleKeys_) {
        if (c.vk == HUNMIN_TSF_VIRTUAL_HANGEUL && c.mods == 0)
            bareHangeul = true;
        if (c.vk == HUNMIN_TSF_VIRTUAL_HANGEUL && c.mods == HUNMIN_TSF_KEYMOD_SHIFT)
            shiftedHangeul = true;
    }
    CHECK(bareHangeul, "the bare Han/Eng key survives every Apply");
    CHECK(shiftedHangeul, "the configured v1:21:1 chord is present");
    CHECK(svc_->ic_ != nullptr, "the engine context is rebuilt after Apply");

    /* 4. The void* bridge (settings.cpp's Show path re-applies via it). */
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "sebeolsik-3-90");
    hunmin_tsf_apply_config_store(svc_);
    CHECK(svc_->hangulId_ == "sebeolsik-3-90", "apply bridge re-read: got %s",
          svc_->hangulId_.c_str());
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "");
    hunmin_tsf_apply_config_store(nullptr); /* no crash on a dead service */

    /* Composition applied over the mock layer still types: 'r' composes
     * after the second Apply rebuilt ic_ with dubeolsik again. */
    _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
    svc_->ApplyConfigStore();
    BOOL eaten = FALSE;
    g_mockKeyState[VK_SHIFT & 0xFF] = 0;
    svc_->OnKeyDown(mctx_, 'R', 0, &eaten);
    CHECK(eaten == TRUE &&
              std::string(hunmin_ic_preedit(svc_->ic_)).compare("\xE3\x84\xB1") == 0,
          "post-Apply composition still works (got %s)", hunmin_ic_preedit(svc_->ic_));
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
