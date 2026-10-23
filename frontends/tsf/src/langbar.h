/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The language-bar item (issues #67/#103): a TEAR-OFF COM object that
 * implements ITfLangBarItemButton (hence ITfLangBarItem) and ITfSource.
 * The shell QueryInterface returns it for those IIDs, so the TIP object no
 * longer IS the indicator: the shell keeps the TSF text-service identity,
 * the item owns the bar surface (GetInfo's hosting keys, the mode badge,
 * the right-click menu, the kernel's ITfLangBarItemSink) and reads only the
 * current mode through LangBarHost.
 *
 * The host contract lives in tsf.h with the other shell-owned seams.
 */
#ifndef HUNMIN_TSF_LANGBAR_H_
#define HUNMIN_TSF_LANGBAR_H_

#include "tsf.h"

#include <windows.h>
#include <msctf.h>

class LangBarItem : public ITfLangBarItemButton, public ITfSource {
  public:
    LangBarItem(LangBarHost &host, const CLSID &serviceClsid);

    /* IUnknown */
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    /* ITfLangBarItem */
    STDMETHOD(GetInfo)(TF_LANGBARITEMINFO *pInfo) override;
    STDMETHOD(GetStatus)(DWORD *pdwStatus) override;
    STDMETHOD(Show)(BOOL fShow) override;
    STDMETHOD(GetTooltipString)(BSTR *pbstrToolTip) override;

    /* ITfLangBarItemButton */
    STDMETHOD(OnClick)(TfLBIClick click, POINT pt, const RECT *prcArea) override;
    STDMETHOD(InitMenu)(ITfMenu *pMenu) override;
    STDMETHOD(OnMenuSelect)(UINT wID) override;
    STDMETHOD(GetIcon)(HICON *phIcon) override;
    STDMETHOD(GetText)(BSTR *pbstrText) override;

    /* ITfSource */
    STDMETHOD(AdviseSink)(REFIID riid, IUnknown *punk,
                          DWORD *pdwCookie) override;
    STDMETHOD(UnadviseSink)(DWORD dwCookie) override;

    /* The mode boundary's one notification. */
    void Update();

    /* The language-bar manager holds a reference only between these two
     * calls, so the attached state and the kernel sink live here, next to
     * the item the manager sees. TRUE from Attach when AddItem succeeded. */
    bool Attach(ITfThreadMgr *threadMgr);
    void Detach(ITfThreadMgr *threadMgr);
    /* Drop the kernel sink after ITfLangBarItemMgr::RemoveItem (Detach does
     * this; the shell's destructor calls it too, when no manager is left). */
    void OnRemoved();

    /* The tier-F1 observation channel. */
    ITfLangBarItemSink *Sink() const { return sink_; }

  private:
    ~LangBarItem() = default;

    LONG ref_;
    LangBarHost &host_;
    CLSID serviceClsid_;
    ITfLangBarItemSink *sink_ = nullptr;
    DWORD sinkCookie_ = 0;
    bool attached_ = false;
};

/* The tear-off: refcount 1 on return; the shell holds that reference and
 * releases it after Deactivate (the language-bar manager holds its own
 * reference only between AddItem and RemoveItem). */
LangBarItem *hunmin_tsf_langbar_new(LangBarHost &host, const CLSID &serviceClsid);

#endif /* HUNMIN_TSF_LANGBAR_H_ */
