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
#include <vector>
#include <windows.h>

#include "mock-tsf.h"
#include "tsf-keystate-mock.h"
#include "../src/tsf.h"

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

} /* namespace */

TEST(TsfLangbarItem, GetInfoCarriesTheHostedShape) {
    CTextService svc;
    ITfLangBarItemButton *item = nullptr;
    ASSERT_EQ(svc.QueryInterface(IID_ITfLangBarItemButton, (void **)&item), S_OK);
    ASSERT_NE(item, nullptr);

    TF_LANGBARITEMINFO info = {};
    ASSERT_EQ(item->GetInfo(&info), S_OK);
    EXPECT_TRUE(IsEqualGUID(info.guidItem, kGuidLbiInputMode))
        << "guidItem must be GUID_LBI_INPUTMODE: the shell input indicator "
           "hosts a langbar item only with this GUID";
    EXPECT_EQ(info.dwStyle,
              (DWORD)(TF_LBI_STYLE_BTN_MENU | TF_LBI_STYLE_SHOWNINTRAY))
        << "the ruled shape: the menu-button bit and the tray bit, no "
           "BTN_BUTTON";
    EXPECT_EQ(info.clsidService, CLSID_HunminTSF);
    item->Release();
}

TEST(TsfLangbarItem, QueryInterfaceAnswersITfSource) {
    CTextService svc;
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
    EXPECT_EQ(svc.QueryInterface(IID_ITfCompartmentMgr, &unknownIid), E_NOINTERFACE)
        << "an unrelated IID must still be refused";
}

TEST(TsfLangbarItem, AdviseSinkInstallsTheKernelSinkAndOnUpdateFlows) {
    CTextService svc;
    ITfSource *item = nullptr;
    ASSERT_EQ(svc.QueryInterface(IID_ITfSource, (void **)&item), S_OK);
    ASSERT_NE(item, nullptr);
    MockItemSink *sink = new MockItemSink();

    /* The sink direction is KERNEL-to-item (language-bar.md): a wrong IID
     * is refused without state, a good one stores the sink and returns
     * THIS implementation's cookie. */
    DWORD cookie = 0;
    EXPECT_EQ(item->AdviseSink(IID_ITfCompartment, sink, &cookie), CONNECT_E_CANNOTCONNECT)
        << "a non-ITfLangBarItemSink advice is refused";
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
    EXPECT_EQ(sink->updates[0], (DWORD)(TF_LBI_ICON | TF_LBI_TEXT))
        << "the badge rides icon+text (GetIcon/GetText re-queries)";

    item->UnadviseSink(cookie);
    svc.UpdateLangBarItem();
    EXPECT_EQ(sink->updates.size(), (size_t)1) << "no notification after the sink is gone";

    EXPECT_EQ(item->UnadviseSink(cookie), CONNECT_E_NOCONNECTION)
        << "a stale cookie is refused";
    item->Release();
    sink->Release(); /* the seat leaves with the test */
}

TEST(TsfLangbarItem, UnadviseSinkOnlyAnswersItsOwnCookie) {
    CTextService svc;
    ITfSource *item = nullptr;
    ASSERT_EQ(svc.QueryInterface(IID_ITfSource, (void **)&item), S_OK);
    ASSERT_NE(item, nullptr);
    MockItemSink *sink = new MockItemSink();
    DWORD cookie = 0;
    EXPECT_EQ(item->AdviseSink(IID_ITfLangBarItemSink, sink, &cookie), S_OK);
    EXPECT_EQ(item->UnadviseSink(7), CONNECT_E_NOCONNECTION)
        << "an unadvised cookie releases nothing";
    EXPECT_EQ(item->UnadviseSink(cookie), S_OK);
    EXPECT_EQ(item->UnadviseSink(cookie), CONNECT_E_NOCONNECTION) << "double unadvise is refused";
    item->Release();
    sink->Release();
}
