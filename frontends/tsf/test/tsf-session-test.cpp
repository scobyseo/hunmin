/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1, the session fragment (issue #107): what CTextService does to the
 * host on the way in and on the way out -- the two advice registrations, the
 * conversion compartment, and the language-bar item -- driven over the session
 * doubles in mock-tsf.h through MockThreadMgr.
 *
 * Before these doubles existed every one of those calls answered
 * E_NOINTERFACE, so Activate/Deactivate ran with the surfaces missing and no
 * test could see them. What is asserted below is the ADAPTER's behaviour
 * against a double that answers like a compliant host; what the real msctf
 * kernel does with the same calls stays device-bound.
 *
 * Two cases pin RECORDED DEFECTS (the event-sink unadvise and the langbar
 * RemoveItem). They assert today's behaviour deliberately, so a fix must flip
 * the expectation instead of passing silently - the comment on each names the
 * defect, and the PR reports both to the maintainer rather than deciding.
 */
#include <gtest/gtest.h>

#include <string>
#include <windows.h>

#include "../src/keybinding.h"
#include "../src/tsf.h"
#include "mock-tsf.h"
#include "tsf-keystate-mock.h"
#include "tsf-testdir.h"

class TsfSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        _putenv_s("HUNMIN_DATA_DIR", HUNMIN_TSF_DATA_DIR);
        /* Every slot pinned by the environment: the host's real HKCU store
         * must not decide which layout or toggle set this test drives. */
        _putenv_s(HUNMIN_TSF_ENV_HANGUL_LAYOUT, "dubeolsik");
        _putenv_s(HUNMIN_TSF_ENV_LATIN_LAYOUT, "us-qwerty");
        _putenv_s(HUNMIN_TSF_ENV_TOGGLE_KEYS, "");
        g_mockKeyState[VK_SHIFT & 0xFF] = 0;
        tsf_test_register_keystate();

        tm_ = new tsfmock::MockThreadMgr(); /* ref 1 */
        mctx_ = new tsfmock::MockContext();
        dm_ = new tsfmock::MockDocumentMgr(mctx_);
        mctx_->AddRef();
        svc_ = new CTextService();
    }

    void TearDown() override {
        svc_->Release();
        mctx_->Release();
        dm_->Release();
        tm_->Release();
    }

    HunminTsfSnapshot Snap() { return svc_->Snapshot(); }

    tsfmock::MockThreadMgr *tm_ = nullptr;
    tsfmock::MockContext *mctx_ = nullptr;
    tsfmock::MockDocumentMgr *dm_ = nullptr;
    CTextService *svc_ = nullptr;
};

/* Activate publishes the mode into the compartment the language bar reads:
 * GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, TF_CONVERSIONMODE_NATIVE on
 * the hangul side (TSF-CONV-COMP-67). */
TEST_F(TsfSessionTest, ActivatePublishesHangulToTheConversionCompartment) {
    ASSERT_EQ(svc_->Activate(tm_, 7), S_OK);
    ASSERT_EQ(tm_->compartmentMgr.asked.size(), (size_t)1) << "Activate asks for exactly one compartment";
    EXPECT_TRUE(IsEqualGUID(tm_->compartmentMgr.asked[0], GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION));

    ASSERT_EQ(tm_->compartmentMgr.writes.size(), (size_t)1);
    const tsfmock::CompartmentWrite &w = tm_->compartmentMgr.writes[0];
    EXPECT_TRUE(IsEqualGUID(w.guid, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION));
    EXPECT_EQ(w.vt, (VARTYPE)VT_I4) << "an I4, which is what the system surfaces read";
    EXPECT_EQ(w.value, (LONG)TF_CONVERSIONMODE_NATIVE) << "hangul side = native";
}

/* The mode boundary republishes it: the toggle writes 0 on the latin side,
 * which is what makes the tray/legacy bar follow the TIP's own mode. */
TEST_F(TsfSessionTest, TheModeToggleRepublishesTheLatinSide) {
    ASSERT_EQ(svc_->Activate(tm_, 7), S_OK);
    ASSERT_TRUE(Snap().hangulMode) << "the service starts in hangul mode";

    BOOL eaten = FALSE;
    svc_->OnTestKeyDown(mctx_, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    svc_->OnKeyDown(mctx_, HUNMIN_TSF_VIRTUAL_HANGEUL, 0, &eaten);
    ASSERT_EQ(eaten, TRUE) << "the bare Han/Eng key is consumed as the toggle";
    ASSERT_FALSE(Snap().hangulMode) << "the toggle reached the latin side";

    ASSERT_EQ(tm_->compartmentMgr.writes.size(), (size_t)2) << "Activate + one mode boundary";
    EXPECT_EQ(tm_->compartmentMgr.writes[1].value, 0L) << "latin side = 0";
    EXPECT_TRUE(IsEqualGUID(tm_->compartmentMgr.writes[1].guid, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION));
}

/* A host with no such compartment must not break Activate: the adapter logs
 * and carries on (its "conversion compartment GONE" branch). */
TEST_F(TsfSessionTest, AMissingConversionCompartmentDoesNotFailActivate) {
    tm_->compartmentMgr.present = false;
    EXPECT_EQ(svc_->Activate(tm_, 7), S_OK);
    EXPECT_TRUE(tm_->compartmentMgr.writes.empty()) << "nothing was written, and nothing threw";
}

/* Activate attaches THE one langbar item -- the tear-off the shell also reaches
 * through QueryInterface(IID_ITfLangBarItemButton). Identity, not just count:
 * a second item object would pass a count-only assertion. */
TEST_F(TsfSessionTest, ActivateAttachesTheOneLangBarItem) {
    ASSERT_EQ(svc_->Activate(tm_, 7), S_OK);
    ASSERT_EQ(tm_->langBarMgr.added.size(), (size_t)1) << "exactly one AddItem";

    ITfLangBarItemButton *item = nullptr;
    ASSERT_EQ(svc_->QueryInterface(IID_ITfLangBarItemButton, (void **)&item), S_OK);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ((void *)tm_->langBarMgr.added[0], (void *)static_cast<ITfLangBarItem *>(item)) << "the added item is the service's item, not a copy";
    item->Release();
}

/* The measured host fact (issue #67, 0x80004005 for any new item): a refused
 * AddItem leaves the service ACTIVE with no item attached. This is modelled
 * behaviour, labelled as the measured quirk, not the documented contract --
 * the default double answers S_OK. */
TEST_F(TsfSessionTest, ARefusedAddItemLeavesTheServiceActiveWithoutAnItem) {
    tm_->langBarMgr.addItemHr = E_FAIL; /* 0x80004005, as measured */
    EXPECT_EQ(svc_->Activate(tm_, 7), S_OK) << "the bar refusal is not an activation failure";
    EXPECT_TRUE(tm_->langBarMgr.added.empty());
    EXPECT_EQ(Snap().itemSink, nullptr) << "no sink: the bar never advised the item";
}

/* The key-sink pairing: AdvanceKeyEventSink is advised with the client id and
 * fForeground=TRUE, and Deactivate unadvises with the SAME client id. */
TEST_F(TsfSessionTest, DeactivateUnadvisesTheKeySinkWithTheSameClientId) {
    ASSERT_EQ(svc_->Activate(tm_, 7), S_OK);
    ASSERT_EQ(tm_->keySinkAdvises.size(), (size_t)1) << "one key-event sink advice";
    EXPECT_EQ(tm_->keySinkAdvises[0].tid, (TfClientId)7);
    EXPECT_EQ((void *)tm_->keySinkAdvises[0].sink, (void *)static_cast<ITfKeyEventSink *>(svc_));
    EXPECT_EQ(tm_->keySinkAdvises[0].foreground, TRUE) << "a foreground TIP";

    ASSERT_EQ(svc_->Deactivate(), S_OK);
    ASSERT_EQ(tm_->keySinkUnadvises.size(), (size_t)1) << "exactly one unadvise, no stray repeats";
    EXPECT_EQ(tm_->keySinkUnadvises[0], (TfClientId)7) << "the same client id it was advised with";
    EXPECT_EQ(tm_->keySink, nullptr) << "the manager no longer holds our sink";
}

/* RECORDED DEFECT (measured 2026-10-26, this fragment): Activate advises the
 * thread-manager EVENT sink, and Deactivate never unadvises it --
 * UnadviseSinks() handles only ITfKeystrokeMgr. The cookie is still live after
 * Deactivate returns. Asserted as today's behaviour so a fix is a deliberate
 * test change; reported in the PR as a finding, not ruled here. */
TEST_F(TsfSessionTest, DeactivateLeavesTheThreadManagerEventSinkAdvised) {
    ASSERT_EQ(svc_->Activate(tm_, 7), S_OK);
    ASSERT_EQ(tm_->eventSinkAdvises.size(), (size_t)1) << "the event sink IS advised";
    EXPECT_NE(tm_->adviseCookie, (DWORD)0);

    ASSERT_EQ(svc_->Deactivate(), S_OK);
    EXPECT_TRUE(tm_->eventSinkUnadvises.empty()) << "recorded defect: no UnadviseSink on the way out";
    EXPECT_NE(tm_->adviseCookie, (DWORD)0) << "recorded defect: the event-sink advice is still live";
}

/* RECORDED DEFECT (same measurement): Deactivate calls Detach() with a
 * threadMgr_ that UnadviseSinks() has already nulled, so
 * ITfLangBarItemMgr::RemoveItem is never called and the item is dropped without
 * telling the bar (src/tsf.cpp's own "recorded defect, not changed here" note).
 * Under a host that DID attach, the bar is left holding a removed item. */
TEST_F(TsfSessionTest, DeactivateNeverRemovesTheLangBarItem) {
    ASSERT_EQ(svc_->Activate(tm_, 7), S_OK);
    ASSERT_EQ(tm_->langBarMgr.added.size(), (size_t)1);

    ASSERT_EQ(svc_->Deactivate(), S_OK);
    EXPECT_TRUE(tm_->langBarMgr.removed.empty()) << "recorded defect: RemoveItem is never reached";
}

/* Both defects are invisible when the host never attached anything, which is
 * how they survived: the refusal variant walks the same Deactivate path and
 * must stay clean (no RemoveItem for an item that was never added). */
TEST_F(TsfSessionTest, DeactivateWithARefusedAddCallsNoRemoveItem) {
    tm_->langBarMgr.addItemHr = E_FAIL;
    ASSERT_EQ(svc_->Activate(tm_, 7), S_OK);
    ASSERT_EQ(svc_->Deactivate(), S_OK);
    EXPECT_TRUE(tm_->langBarMgr.removed.empty());
}
