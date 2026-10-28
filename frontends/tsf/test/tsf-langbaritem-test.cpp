/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1, the langbar-item fragment (issue #103, the ruled langbar
 * surface): the indicator + the right-click config menu ride our own
 * ITfLangBarItemButton. Since S6 that object is a TEAR-OFF: the shell QIs
 * it for the three bar IIDs, the item owns the shape. These cases white-box
 * the item shape the managed surfaces depend on — GetInfo's hosting keys
 * (GUID_LBI_INPUTMODE + BTN_MENU|SHOWNINTRAY, the ruled shape), the
 * item's ITfSource (the language bar installs ITS sink through it,
 * language-bar.md), and the OnUpdate notification the 한/A badge rides at
 * every mode boundary. Standalone TU: the include-.cpp seam is retired (DIP + SAP, docs/REFACTOR-TSF-COMPONENTS.md).
 */
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <windows.h>

#include "../src/tsf.h"
#include "mock-tsf.h"
#include "tsf-keystate-mock.h"

/* GUID_LBI_INPUTMODE (the constant declared extern in the SDK's ctffunc.h
 * and private in langbar.cpp): the item GUID the Win8+/Win11 input indicator
 * hosts. Matched here as a literal so the test cannot drift with the code
 * constant. */
static const GUID kGuidLbiInputMode = {0x2C77A81E, 0x41CC, 0x4178, {0xA3, 0xA7, 0x5F, 0x8A, 0x98, 0x75, 0x68, 0xE6}};

namespace {

/* The kernel-side sink double: records every OnUpdate mask the item sends. */
class MockItemSink : public ITfLangBarItemSink {
public:
    LONG ref_ = 1;
    std::vector<DWORD> updates;
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItemSink)) {
            *ppv = static_cast<ITfLangBarItemSink *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG r = (ULONG)InterlockedDecrement(&ref_);
        if (r == 0)
            delete this;
        return r;
    }
    STDMETHOD(OnUpdate)(DWORD dwFlags) override {
        updates.push_back(dwFlags);
        return S_OK;
    }
};

/* The language bar's menu sink double: language-bar.md says a BTN_MENU
 * item receives ITfMenu in InitMenu and the bar reports the choice through
 * OnMenuSelect. This records what the item adds. */
class MockMenu : public ITfMenu {
public:
    LONG ref_ = 1;
    struct Entry {
        UINT id;
        DWORD flags;
        std::wstring text;
    };
    std::vector<Entry> entries;

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfMenu)) {
            *ppv = static_cast<ITfMenu *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG r = (ULONG)InterlockedDecrement(&ref_);
        if (r == 0)
            delete this;
        return r;
    }
    STDMETHOD(AddMenuItem)(UINT uId, DWORD dwFlags, HBITMAP, HBITMAP, const WCHAR *pch, ULONG cch, ITfMenu **ppMenu) override {
        entries.push_back(Entry{uId, dwFlags, std::wstring(pch, cch)});
        if (ppMenu)
            *ppMenu = nullptr;
        return S_OK;
    }
};

/* The settings stand-in sits beside the test binary (the CMake dependency
 * builds it there), so no absolute path is baked into the test. */
std::wstring SiblingExe(const wchar_t *name) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path, n);
    size_t slash = p.find_last_of(L"\\/");
    return p.substr(0, slash + 1) + name;
}

/* Set for one test and restored, so a later test cannot inherit a stale
 * settings path. */
class ScopedEnv {
public:
    ScopedEnv(const wchar_t *name, const wchar_t *value) : name_(name) {
        wchar_t buf[32767];
        DWORD n = GetEnvironmentVariableW(name, buf, 32767);
        had_ = n > 0 && n < 32767;
        if (had_)
            old_.assign(buf, n);
        SetEnvironmentVariableW(name, value);
    }
    ~ScopedEnv() { SetEnvironmentVariableW(name_.c_str(), had_ ? old_.c_str() : nullptr); }

private:
    std::wstring name_;
    std::wstring old_;
    bool had_ = false;
};

} /* namespace */

/* The item is INSTALLED by UpdateLangBarItem(ptm) -- the ruled call site
 * (maintainer ruling 2026-10-29), the same seam Activate drives in a host:
 * QI ITfLangBarItemMgr on the thread manager, then AddItem. A case that needs
 * the item object runs it first. */
/* MockThreadMgr is refcounted with a private destructor (heap only, like the
 * session fixture's tm_), so a case holds it through this handle. */
struct MockTm {
    tsfmock::MockThreadMgr *ptr = new tsfmock::MockThreadMgr();
    ~MockTm() { ptr->Release(); }
    tsfmock::MockThreadMgr *get() const { return ptr; }
};

void InstallItem(CTextService &svc, tsfmock::MockThreadMgr *tm) {
    svc.UpdateLangBarItem(tm);
    ASSERT_EQ(tm->langBarMgr.added.size(), (size_t)1) << "the ruled install: AddItem reached the manager";
}

TEST(TsfLangbarItem, GetInfoCarriesTheHostedShape) {
    MockTm tm;
    CTextService svc;
    InstallItem(svc, tm.get());
    ITfLangBarItemButton *item = nullptr;
    ASSERT_EQ(svc.QueryInterface(IID_ITfLangBarItemButton, (void **)&item), S_OK);
    ASSERT_NE(item, nullptr);

    TF_LANGBARITEMINFO info = {};
    ASSERT_EQ(item->GetInfo(&info), S_OK);
    EXPECT_TRUE(IsEqualGUID(info.guidItem, kGuidLbiInputMode)) << "guidItem must be GUID_LBI_INPUTMODE: the shell input indicator "
                                                                  "hosts a langbar item only with this GUID";
    EXPECT_EQ(info.dwStyle, (DWORD)(TF_LBI_STYLE_BTN_MENU | TF_LBI_STYLE_SHOWNINTRAY))
        << "the ruled shape: the menu-button bit and the tray bit, no "
           "BTN_BUTTON (adding it on 2026-10-23 did not route the Win11 "
           "indicator's click either)";
    EXPECT_EQ(info.clsidService, CLSID_HunminTSF);
    item->Release();
}

/* language-bar.md's menu-button contract: InitMenu adds the items, the bar
 * calls OnMenuSelect with the chosen id. The item's one entry is the
 * settings launch (id 1). The launch itself is not asserted here (it spawns
 * a process); what this locks is the menu shape the user sees and the id
 * OnMenuSelect receives. */
TEST(TsfLangbarItem, InitMenuAddsTheOneSettingsEntry) {
    MockTm tm;
    CTextService svc;
    InstallItem(svc, tm.get());
    ITfLangBarItemButton *item = nullptr;
    ASSERT_EQ(svc.QueryInterface(IID_ITfLangBarItemButton, (void **)&item), S_OK);
    ASSERT_NE(item, nullptr);

    MockMenu menu;
    EXPECT_EQ(item->InitMenu(&menu), S_OK);
    ASSERT_EQ(menu.entries.size(), (size_t)1) << "one entry, the config menu";
    EXPECT_EQ(menu.entries[0].id, (UINT)1);
    EXPECT_EQ(menu.entries[0].flags, (DWORD)0) << "a plain string item";
    EXPECT_EQ(menu.entries[0].text, std::wstring(L"Hunmin \uC124\uC815 \uC5F4\uAE30"));

    /* An id the item did not add is ignored (the bar may own other ids in a
     * host that composes menus). */
    EXPECT_EQ(item->OnMenuSelect(2), S_OK);
    EXPECT_EQ(item->InitMenu(nullptr), E_FAIL) << "no menu to fill";
    item->Release();
}

/* The regression for the silent menu item: OnMenuSelect used to pass a null
 * anchor to hunmin_tsf_launch_settings, which then failed GetModuleHandleExW
 * and returned E_FAIL before launching anything. The noop exe stands in for
 * the WinUI surface, so the assertion is the real launch path (id 1 ->
 * ShellExecuteExW -> S_OK), not a mock of it. */
TEST(TsfLangbarItem, OnMenuSelectOneLaunchesTheSettingsSurface) {
    MockTm tm;
    std::wstring noop = SiblingExe(L"tsf-settings-noop.exe");
    ASSERT_NE(GetFileAttributesW(noop.c_str()), INVALID_FILE_ATTRIBUTES) << "tsf-settings-noop.exe is a build dependency of this test";
    ScopedEnv env(L"HUNMIN_TSF_SETTINGS_EXE", noop.c_str());

    CTextService svc;
    InstallItem(svc, tm.get());
    ITfLangBarItemButton *item = nullptr;
    ASSERT_EQ(svc.QueryInterface(IID_ITfLangBarItemButton, (void **)&item), S_OK);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->OnMenuSelect(1), S_OK) << "the settings id must reach the launcher";
    item->Release();
}

TEST(TsfLangbarItem, QueryInterfaceAnswersITfSource) {
    MockTm tm;
    CTextService svc;
    InstallItem(svc, tm.get());
    /* language-bar.md: the language bar QIs the item for ITfSource at
     * AddItem to install its sink — a missing answer broke the item lane
     * before the item code ever got a refusal diagnosis. */
    ITfSource *src = nullptr;
    EXPECT_EQ(svc.QueryInterface(IID_ITfSource, (void **)&src), S_OK);
    ASSERT_NE(src, nullptr);

    /* The TIP's QI returns the tear-off, and the tear-off answers the bar
     * item interfaces too. */
    ITfLangBarItemButton *same = nullptr;
    EXPECT_EQ(src->QueryInterface(IID_ITfLangBarItemButton, (void **)&same), S_OK);
    ASSERT_NE(same, nullptr);
    same->Release();
    src->Release();

    void *unknownIid = nullptr;
    EXPECT_EQ(svc.QueryInterface(IID_ITfCompartmentMgr, &unknownIid), E_NOINTERFACE) << "an unrelated IID must still be refused";
}

TEST(TsfLangbarItem, AdviseSinkInstallsTheKernelSinkAndOnUpdateFlows) {
    MockTm tm;
    CTextService svc;
    InstallItem(svc, tm.get());
    ITfSource *item = nullptr;
    ASSERT_EQ(svc.QueryInterface(IID_ITfSource, (void **)&item), S_OK);
    ASSERT_NE(item, nullptr);
    MockItemSink *sink = new MockItemSink();

    /* The sink direction is KERNEL-to-item (language-bar.md): a wrong IID
     * is refused without state, a good one stores the sink and returns
     * THIS implementation's cookie. */
    DWORD cookie = 0;
    EXPECT_EQ(item->AdviseSink(IID_ITfCompartment, sink, &cookie), CONNECT_E_CANNOTCONNECT) << "a non-ITfLangBarItemSink advice is refused";
    EXPECT_EQ(cookie, (DWORD)0);

    EXPECT_EQ(item->AdviseSink(IID_ITfLangBarItemSink, sink, &cookie), S_OK);
    EXPECT_EQ(cookie, (DWORD)1) << "the item's own single-sink cookie";
    /* sink keeps its construction reference for the WHOLE test: the item
     * holds its own QI reference (AdviseSink does not strip one), and
     * UnadviseSink releases exactly that — reading `sink` after dropping
     * the last seat here was a use-after-destroy. */

    /* A second advise is a single-sink limit, not a second registration. */
    MockItemSink *second = new MockItemSink();
    DWORD cookie2 = 9;
    EXPECT_EQ(item->AdviseSink(IID_ITfLangBarItemSink, second, &cookie2), CONNECT_E_ADVISELIMIT);
    EXPECT_EQ(cookie2, (DWORD)0);
    second->Release();

    /* The mode boundary the key path drives: flip + notify = the 한/A badge
     * re-read, without a host restart. No threadMgr/context needed — the
     * notification only touches the sink, and ToggleModeCore's tail IS the
     * notify (the old shape set the mode field and called UpdateLangBarItem
     * by hand). */
    svc.ToggleModeCore(nullptr);
    ASSERT_NE(svc.Snapshot().itemSink, nullptr) << "the advised sink is stored";
    ASSERT_EQ(sink->updates.size(), (size_t)1);
    EXPECT_EQ(sink->updates[0], (DWORD)(TF_LBI_ICON | TF_LBI_TEXT)) << "the badge rides icon+text (GetIcon/GetText re-queries)";

    item->UnadviseSink(cookie);
    svc.UpdateLangBarItem(nullptr);
    EXPECT_EQ(sink->updates.size(), (size_t)1) << "no notification after the sink is gone";

    EXPECT_EQ(item->UnadviseSink(cookie), CONNECT_E_NOCONNECTION) << "a stale cookie is refused";
    item->Release();
    sink->Release(); /* the seat leaves with the test */
}

TEST(TsfLangbarItem, UnadviseSinkOnlyAnswersItsOwnCookie) {
    MockTm tm;
    CTextService svc;
    InstallItem(svc, tm.get());
    ITfSource *item = nullptr;
    ASSERT_EQ(svc.QueryInterface(IID_ITfSource, (void **)&item), S_OK);
    ASSERT_NE(item, nullptr);
    MockItemSink *sink = new MockItemSink();
    DWORD cookie = 0;
    EXPECT_EQ(item->AdviseSink(IID_ITfLangBarItemSink, sink, &cookie), S_OK);
    EXPECT_EQ(item->UnadviseSink(7), CONNECT_E_NOCONNECTION) << "an unadvised cookie releases nothing";
    EXPECT_EQ(item->UnadviseSink(cookie), S_OK);
    EXPECT_EQ(item->UnadviseSink(cookie), CONNECT_E_NOCONNECTION) << "double unadvise is refused";
    item->Release();
    sink->Release();
}

/* The ruling's case: Activate's install half lives in UpdateLangBarItem(ptm).
 * One AddItem per installation (not one per mode boundary), through the manager
 * the thread manager hands out, and the installed item is the object the shell
 * reaches by QI. */
TEST(TsfLangbarItem, UpdateLangBarItemInstallsOnceThroughTheThreadManager) {
    MockTm tm;
    CTextService svc;
    svc.UpdateLangBarItem(tm.get());
    ASSERT_EQ(tm.get()->langBarMgr.added.size(), (size_t)1) << "the install reached ITfLangBarItemMgr::AddItem";

    ITfLangBarItemButton *item = nullptr;
    ASSERT_EQ(svc.QueryInterface(IID_ITfLangBarItemButton, (void **)&item), S_OK);
    EXPECT_EQ((void *)tm.get()->langBarMgr.added[0], (void *)static_cast<ITfLangBarItem *>(item)) << "the added item is the service's own";

    svc.UpdateLangBarItem(tm.get()); /* a mode boundary: a notification, not a second AddItem */
    EXPECT_EQ(tm.get()->langBarMgr.added.size(), (size_t)1) << "installed once, notified after";
    item->Release();
}
