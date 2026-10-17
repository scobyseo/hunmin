/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Mock TSF layer for tier F1 (issue #80): in-memory test doubles for the
 * interfaces frontends/tsf/src/tsf.cpp consumes, so the adapter's logic can be
 * driven by a ctest binary without a live TSF session. Nothing here touches
 * the registry, COM activation or a real window; every object keeps state in
 * plain member variables and implements exactly what tsf.cpp calls, stubbing
 * the rest with E_NOTIMPL so an adapter use of something new fails loudly
 * instead of silently succeeding.
 *
 * The document model is the heart of it: MockContext holds a UTF-16 string,
 * the selection is two anchors into it, and MockRange::SetText splices the
 * replacement in while recording the text. That is what makes the buffering
 * contract (docs/WINDOWS-TSF-BUFFERING.md, issue #73) observable: the harness
 * sees ONE insert per boundary event carrying the whole buffered word, not a
 * per-syllable drip.
 */
#pragma once
#include <msctf.h>
#include <windows.h>
#include <string>
#include <vector>

// The members the interfaces declare but the adapter never calls. Their
// exact signatures come from msctf.h; the compiler flags any drift. The
// macro lives at header scope: a per-class define/undef lapse was exactly
// what made every mock after the first one compile as an abstract class.
#define TSF_MOCK_STUB(name, ...)                                              \
    STDMETHOD(name)(__VA_ARGS__) override { return E_NOTIMPL; }

namespace tsfmock {

struct RefCount {
    LONG ref = 1;
    // The least-derived destructor must govern delete-through-base:
    // Release() does `delete this` on RefCount*, and a non-virtual dtor
    // freed only the base's 4-byte subobject of every mock allocation,
    // corrupting the debug heap on the first Release-to-zero.
    virtual ~RefCount() = default;
    ULONG AddRef() { return (ULONG)InterlockedIncrement(&ref); }
    ULONG Release() {
        ULONG r = (ULONG)InterlockedDecrement(&ref);
        if (r == 0)
            delete this;
        return r;
    }
};

class MockContext;

// One range bound to context anchors at creation time. The adapter's edit
// sessions do GetSelection -> SetText -> Collapse(TF_ANCHOR_END) -> SetSelection.
class MockRange : public ITfRange, public RefCount {
  public:
    MockRange(MockContext *ctx, LONG start, LONG end)
        : ctx_(ctx), start_(start), end_(end) {}
    // SetText splices pchText into the document at [start_, end_) and records
    // it. cch < 0 means NUL-terminated.
    STDMETHOD(SetText)(TfEditCookie ec, DWORD dwFlags, const WCHAR *pchText, LONG cch) override;
    STDMETHOD(Collapse)(TfEditCookie ec, TfAnchor aPos) override;

    LONG startForSelection() const { return start_; }
    LONG endForSelection() const { return end_; }

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfRange)) {
            *ppv = static_cast<ITfRange *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

// The members ITfRange declares but the adapter never calls. Their exact
// signatures come from msctf.h; the compiler flags any drift.
    TSF_MOCK_STUB(GetText, TfEditCookie ec, DWORD dwFlags, WCHAR *pchText, ULONG cchMax,
                  ULONG *pcch)
    TSF_MOCK_STUB(GetFormattedText, TfEditCookie ec, IDataObject **ppDataObject)
    TSF_MOCK_STUB(GetEmbedded, TfEditCookie ec, REFGUID rguidService, REFIID riid,
                  IUnknown **ppunk)
    TSF_MOCK_STUB(InsertEmbedded, TfEditCookie ec, DWORD dwFlags, IDataObject *pDataObject)
    TSF_MOCK_STUB(ShiftStart, TfEditCookie ec, LONG cchReq, LONG *pcch,
                  const TF_HALTCOND *pHalt)
    TSF_MOCK_STUB(ShiftEnd, TfEditCookie ec, LONG cchReq, LONG *pcch,
                  const TF_HALTCOND *pHalt)
    TSF_MOCK_STUB(ShiftStartToRange, TfEditCookie ec, ITfRange *pRange, TfAnchor aPos)
    TSF_MOCK_STUB(ShiftEndToRange, TfEditCookie ec, ITfRange *pRange, TfAnchor aPos)
    TSF_MOCK_STUB(ShiftStartRegion, TfEditCookie ec, TfShiftDir dir, BOOL *pfNoRegion)
    TSF_MOCK_STUB(ShiftEndRegion, TfEditCookie ec, TfShiftDir dir, BOOL *pfNoRegion)
    TSF_MOCK_STUB(IsEmpty, TfEditCookie ec, BOOL *pfEmpty)
    TSF_MOCK_STUB(IsEqualStart, TfEditCookie ec, ITfRange *pWith, TfAnchor aPos,
                  BOOL *pfEqual)
    TSF_MOCK_STUB(IsEqualEnd, TfEditCookie ec, ITfRange *pWith, TfAnchor aPos,
                  BOOL *pfEqual)
    TSF_MOCK_STUB(CompareStart, TfEditCookie ec, ITfRange *pWith, TfAnchor aPos,
                  LONG *plResult)
    TSF_MOCK_STUB(CompareEnd, TfEditCookie ec, ITfRange *pWith, TfAnchor aPos,
                  LONG *plResult)
    TSF_MOCK_STUB(AdjustForInsert, TfEditCookie ec, ULONG cchInsert, BOOL *pfInsertOk)
    TSF_MOCK_STUB(GetGravity, TfGravity *pgStart, TfGravity *pgEnd)
    TSF_MOCK_STUB(SetGravity, TfEditCookie ec, TfGravity gStart, TfGravity gEnd)
    TSF_MOCK_STUB(Clone, ITfRange **ppClone)
    TSF_MOCK_STUB(GetContext, ITfContext **ppContext)

  private:
    ~MockRange() = default;
    friend class MockContext;
    MockContext *ctx_;
    LONG start_;
    LONG end_;
};

class MockContextView : public ITfContextView, public RefCount {
  public:
    // GetTextExt fails on purpose: the mock has no screen geometry, which
    // drives UpdateOverlay into its no-rect caret fallback and, with no
    // caret in a console process, its hide-and-return exit. That keeps
    // overlay-window creation (real HWNDs, explicitly out of scope for this
    // issue) out of the test while every branch before it still runs.
    STDMETHOD(GetTextExt)(TfEditCookie ec, ITfRange *pRange, RECT *prc, BOOL *pfClipped) override {
        (void)ec;
        (void)pRange;
        (void)prc;
        (void)pfClipped;
        return E_FAIL;
    }

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfContextView)) {
            *ppv = static_cast<ITfContextView *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

    TSF_MOCK_STUB(GetRangeFromPoint, TfEditCookie ec, const POINT *ppt, DWORD dwFlags,
                  ITfRange **ppRange)
    TSF_MOCK_STUB(GetScreenExt, RECT *prc)
    TSF_MOCK_STUB(GetWnd, HWND *phwnd)

  private:
    ~MockContextView() = default;
};

// The document. RequestEditSession executes the session synchronously -- the
// TF_ES_SYNC contract the adapter relies on -- after recording it, so the
// harness reads insertions/session count straight after OnKeyDown returns.
class MockContext : public ITfContext, public RefCount {
  public:
    static constexpr TfEditCookie kCookie = 1;

    std::wstring doc;                       // the client document text
    std::vector<std::wstring> insertions;   // one entry per SetText, in order
    int sessionCount = 0;

    STDMETHOD(RequestEditSession)(TfClientId tid, ITfEditSession *pes, DWORD dwFlags,
                                  HRESULT *phrSession) override {
        (void)tid;
        if (phrSession)
            *phrSession = E_FAIL;
        if (!pes)
            return E_INVALIDARG;
        if (!(dwFlags & TF_ES_SYNC)) {
            // The adapter only ever asks for TF_ES_SYNC; refuse async so an
            // adapter change to non-sync surfaces as a test error, not a hang.
            if (phrSession)
                *phrSession = E_PENDING;
            return S_OK;
        }
        sessionCount++;
        if (phrSession)
            *phrSession = pes->DoEditSession(kCookie);
        return S_OK;
    }
    STDMETHOD(GetSelection)(TfEditCookie ec, ULONG ulIndex, ULONG ulCount,
                            TF_SELECTION *pSelection, ULONG *pcFetched) override {
        (void)ec;
        if (pcFetched)
            *pcFetched = 0;
        if (ulIndex != TF_DEFAULT_SELECTION || ulCount < 1 || !pSelection)
            return E_INVALIDARG;
        // The adapter reads .range only; .style stays the default selection
        // end, which is what a plain caret (collapse-by-default) has.
        TF_SELECTION sel = {};
        sel.range = new MockRange(this, selStart_, selEnd_);
        pSelection[0] = sel;
        if (pcFetched)
            *pcFetched = 1;
        return S_OK;
    }
    STDMETHOD(SetSelection)(TfEditCookie ec, ULONG ulCount,
                            const TF_SELECTION *pSelection) override {
        (void)ec;
        if (ulCount < 1 || !pSelection || !pSelection[0].range)
            return E_INVALIDARG;
        MockRange *r = static_cast<MockRange *>(pSelection[0].range);
        selStart_ = r->startForSelection();
        selEnd_ = r->endForSelection();
        return S_OK;
    }
    STDMETHOD(GetActiveView)(ITfContextView **ppView) override {
        if (!ppView)
            return E_INVALIDARG;
        *ppView = new MockContextView();
        return S_OK;
    }

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfContext)) {
            *ppv = static_cast<ITfContext *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

    TSF_MOCK_STUB(InWriteSession, TfClientId tid, BOOL *pfWriteSession)
    TSF_MOCK_STUB(GetStart, TfEditCookie ec, ITfRange **ppStart)
    TSF_MOCK_STUB(GetEnd, TfEditCookie ec, ITfRange **ppEnd)
    TSF_MOCK_STUB(EnumViews, IEnumTfContextViews **ppEnum)
    TSF_MOCK_STUB(GetStatus, TF_STATUS *pdcs)
    TSF_MOCK_STUB(GetProperty, REFGUID guidProp, ITfProperty **ppProp)
    TSF_MOCK_STUB(GetAppProperty, REFGUID guidProp, ITfReadOnlyProperty **ppProp)
    TSF_MOCK_STUB(TrackProperties, const GUID **prgProp, ULONG cProp,
                  const GUID **prgAppProp, ULONG cAppProp,
                  ITfReadOnlyProperty **ppProperty)
    TSF_MOCK_STUB(EnumProperties, IEnumTfProperties **ppEnum)
    TSF_MOCK_STUB(GetDocumentMgr, ITfDocumentMgr **ppDm)
    TSF_MOCK_STUB(CreateRangeBackup, TfEditCookie ec, ITfRange *pRange,
                  ITfRangeBackup **ppBackup)

  private:
    ~MockContext() = default;
    friend class MockRange;
    LONG selStart_ = 0;
    LONG selEnd_ = 0;
};

class MockDocumentMgr : public ITfDocumentMgr, public RefCount {
  public:
    explicit MockDocumentMgr(MockContext *top) : top_(top) {}
    STDMETHOD(GetTop)(ITfContext **ppic) override {
        if (!ppic)
            return E_INVALIDARG;
        *ppic = top_;
        if (top_)
            top_->AddRef();
        return S_OK;
    }
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfDocumentMgr)) {
            *ppv = static_cast<ITfDocumentMgr *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

    TSF_MOCK_STUB(CreateContext, TfClientId tidOwner, DWORD dwFlags, IUnknown *punk,
                  ITfContext **ppic, TfEditCookie *pecTextStore)
    TSF_MOCK_STUB(Push, ITfContext *pic)
    TSF_MOCK_STUB(Pop, DWORD dwFlags)
    TSF_MOCK_STUB(GetBase, ITfContext **ppic)
    TSF_MOCK_STUB(EnumContexts, IEnumTfContexts **ppEnum)

  private:
    ~MockDocumentMgr() = default;
    MockContext *top_;
};

// One object answering for ITfThreadMgr + ITfSource + ITfKeystrokeMgr, the
// three identities the adapter QIs out of it during Activate/Deactivate.
class MockThreadMgr : public ITfThreadMgr,
                      public ITfSource,
                      public ITfKeystrokeMgr,
                      public RefCount {
  public:
    ITfKeyEventSink *keySink = nullptr;
    BOOL keySinkForeground = FALSE;
    DWORD adviseCookie = 0;

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfThreadMgr))
            *ppv = static_cast<ITfThreadMgr *>(this);
        else if (IsEqualIID(riid, IID_ITfSource))
            *ppv = static_cast<ITfSource *>(this);
        else if (IsEqualIID(riid, IID_ITfKeystrokeMgr))
            *ppv = static_cast<ITfKeystrokeMgr *>(this);
        else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

    // ITfSource / ITfKeystrokeMgr -- the advise paths the harness asserts on.
    STDMETHOD(AdviseSink)(REFIID riid, IUnknown *punk, DWORD *pdwCookie) override {
        if (!IsEqualIID(riid, IID_ITfThreadMgrEventSink))
            return E_NOINTERFACE;
        eventSink_ = punk;
        if (pdwCookie)
            *pdwCookie = ++adviseCookie;
        return S_OK;
    }
    STDMETHOD(UnadviseSink)(DWORD dwCookie) override {
        (void)dwCookie;
        eventSink_ = nullptr;
        return S_OK;
    }
    STDMETHOD(AdviseKeyEventSink)(TfClientId tid, ITfKeyEventSink *pSink,
                                  BOOL fForeground) override {
        (void)tid;
        keySink = pSink;
        keySinkForeground = fForeground;
        return S_OK;
    }
    STDMETHOD(UnadviseKeyEventSink)(TfClientId tid) override {
        (void)tid;
        keySink = nullptr;
        return S_OK;
    }

    TSF_MOCK_STUB(Activate, TfClientId *ptid)
    TSF_MOCK_STUB(Deactivate, void)
    TSF_MOCK_STUB(CreateDocumentMgr, ITfDocumentMgr **ppdim)
    TSF_MOCK_STUB(EnumDocumentMgrs, IEnumTfDocumentMgrs **ppEnum)
    TSF_MOCK_STUB(GetFocus, ITfDocumentMgr **ppdimFocus)
    TSF_MOCK_STUB(SetFocus, ITfDocumentMgr *pdimFocus)
    TSF_MOCK_STUB(AssociateFocus, HWND hwnd, ITfDocumentMgr *pdimNew,
                  ITfDocumentMgr **ppdimPrev)
    TSF_MOCK_STUB(IsThreadFocus, BOOL *pfThreadFocus)
    TSF_MOCK_STUB(GetFunctionProvider, REFCLSID clsid, ITfFunctionProvider **ppFuncProv)
    TSF_MOCK_STUB(EnumFunctionProviders, IEnumTfFunctionProviders **ppEnum)
    TSF_MOCK_STUB(GetGlobalCompartment, ITfCompartmentMgr **ppCompMgr)
    TSF_MOCK_STUB(GetForeground, CLSID *pclsid)
    TSF_MOCK_STUB(TestKeyDown, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
    TSF_MOCK_STUB(TestKeyUp, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
    TSF_MOCK_STUB(KeyDown, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
    TSF_MOCK_STUB(KeyUp, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
    TSF_MOCK_STUB(GetPreservedKey, ITfContext *pic, const TF_PRESERVEDKEY *pprekey,
                  GUID *pguid)
    TSF_MOCK_STUB(IsPreservedKey, REFGUID rguid, const TF_PRESERVEDKEY *pprekey,
                  BOOL *pfRegistered)
    TSF_MOCK_STUB(PreserveKey, TfClientId tid, REFGUID rguid,
                  const TF_PRESERVEDKEY *prekey, const WCHAR *pchDesc, ULONG cchDesc)
    TSF_MOCK_STUB(UnpreserveKey, REFGUID rguid, const TF_PRESERVEDKEY *pprekey)
    TSF_MOCK_STUB(SetPreservedKeyDescription, REFGUID rguid, const WCHAR *pchDesc,
                  ULONG cchDesc)
    TSF_MOCK_STUB(GetPreservedKeyDescription, REFGUID rguid, BSTR *pbstrDesc)
    TSF_MOCK_STUB(SimulatePreservedKey, ITfContext *pic, REFGUID rguid, BOOL *pfEaten)

  private:
    ~MockThreadMgr() = default;
    IUnknown *eventSink_ = nullptr;
};

// Out-of-line SetText/Collapse: they need the complete MockContext type.
inline HRESULT
MockRange::SetText(TfEditCookie ec, DWORD dwFlags, const WCHAR *pchText, LONG cch) {
    (void)ec;
    (void)dwFlags;
    if (!pchText)
        return E_INVALIDARG;
    std::wstring text = (cch < 0) ? std::wstring(pchText) : std::wstring(pchText, (size_t)cch);
    start_ = static_cast<LONG>(start_ < 0 ? 0 : start_);
    if (start_ > (LONG)ctx_->doc.size())
        start_ = (LONG)ctx_->doc.size();
    if (end_ < start_ || end_ > (LONG)ctx_->doc.size())
        end_ = start_;
    // A SetText that replaces a span whose own content equals the new text is
    // a second delivery of the same word; record it anyway, the contract
    // assertion ("exactly one insert per boundary") is what catches it.
    ctx_->doc.replace((size_t)start_, (size_t)(end_ - start_), text);
    ctx_->insertions.push_back(text);
    end_ = start_ + static_cast<LONG>(text.size());
    return S_OK;
}

inline HRESULT MockRange::Collapse(TfEditCookie ec, TfAnchor aPos) {
    (void)ec;
    if (aPos == TF_ANCHOR_END)
        start_ = end_;
    else
        end_ = start_;
    return S_OK;
}

} // namespace tsfmock
