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
#include <string>
#include <vector>
#include <windows.h>

// The members the interfaces declare but the adapter never calls. Their
// exact signatures come from msctf.h; the compiler flags any drift. The
// macro lives at header scope: a per-class define/undef lapse was exactly
// what made every mock after the first one compile as an abstract class.
#define TSF_MOCK_STUB(name, ...)                                                                                                                     \
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
class MockProperty;
class MockComposition;
class MockContextComposition;

// One range bound to context anchors at creation time. The adapter's edit
// sessions do GetSelection -> SetText -> Collapse(TF_ANCHOR_END) -> SetSelection.
// A range created INSIDE a composition (issue #98) records its writes on the
// composition channel, not the plain-insert channel — the buffering contract's
// "exactly one boundary insert" observation must stay about COMMITTED text.
class MockRange : public ITfRange, public RefCount {
public:
    MockRange(MockContext *ctx, LONG start, LONG end) : ctx_(ctx), start_(start), end_(end) {}
    MockRange(MockContext *ctx, LONG start, LONG end, bool inComposition) : ctx_(ctx), start_(start), end_(end), inComposition_(inComposition) {}
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
    TSF_MOCK_STUB(GetText, TfEditCookie ec, DWORD dwFlags, WCHAR *pchText, ULONG cchMax, ULONG *pcch)
    TSF_MOCK_STUB(GetFormattedText, TfEditCookie ec, IDataObject **ppDataObject)
    TSF_MOCK_STUB(GetEmbedded, TfEditCookie ec, REFGUID rguidService, REFIID riid, IUnknown **ppunk)
    TSF_MOCK_STUB(InsertEmbedded, TfEditCookie ec, DWORD dwFlags, IDataObject *pDataObject)
    TSF_MOCK_STUB(ShiftStart, TfEditCookie ec, LONG cchReq, LONG *pcch, const TF_HALTCOND *pHalt)
    TSF_MOCK_STUB(ShiftEnd, TfEditCookie ec, LONG cchReq, LONG *pcch, const TF_HALTCOND *pHalt)
    TSF_MOCK_STUB(ShiftStartToRange, TfEditCookie ec, ITfRange *pRange, TfAnchor aPos)
    TSF_MOCK_STUB(ShiftEndToRange, TfEditCookie ec, ITfRange *pRange, TfAnchor aPos)
    TSF_MOCK_STUB(ShiftStartRegion, TfEditCookie ec, TfShiftDir dir, BOOL *pfNoRegion)
    TSF_MOCK_STUB(ShiftEndRegion, TfEditCookie ec, TfShiftDir dir, BOOL *pfNoRegion)
    TSF_MOCK_STUB(IsEmpty, TfEditCookie ec, BOOL *pfEmpty)
    TSF_MOCK_STUB(IsEqualStart, TfEditCookie ec, ITfRange *pWith, TfAnchor aPos, BOOL *pfEqual)
    TSF_MOCK_STUB(IsEqualEnd, TfEditCookie ec, ITfRange *pWith, TfAnchor aPos, BOOL *pfEqual)
    TSF_MOCK_STUB(CompareStart, TfEditCookie ec, ITfRange *pWith, TfAnchor aPos, LONG *plResult)
    TSF_MOCK_STUB(CompareEnd, TfEditCookie ec, ITfRange *pWith, TfAnchor aPos, LONG *plResult)
    TSF_MOCK_STUB(AdjustForInsert, TfEditCookie ec, ULONG cchInsert, BOOL *pfInsertOk)
    TSF_MOCK_STUB(GetGravity, TfGravity *pgStart, TfGravity *pgEnd)
    TSF_MOCK_STUB(SetGravity, TfEditCookie ec, TfGravity gStart, TfGravity gEnd)
    // A real host hands back an independent range over the same span
    // (msctf ITfRange::Clone). The composition caret path (issue #98)
    // clones the composition range and collapses the CLONE to set the
    // post-commit caret; a stubbed Clone left the selection on the dead
    // composition's span, and the next composition rewrote committed
    // text instead of appending after it.
    STDMETHOD(Clone)(ITfRange **ppClone) override;
    TSF_MOCK_STUB(GetContext, ITfContext **ppContext)

private:
    ~MockRange() = default;
    friend class MockContext;
    friend class MockComposition;
    MockContext *ctx_;
    LONG start_;
    LONG end_;
    bool inComposition_ = false;
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

    TSF_MOCK_STUB(GetRangeFromPoint, TfEditCookie ec, const POINT *ppt, DWORD dwFlags, ITfRange **ppRange)
    TSF_MOCK_STUB(GetScreenExt, RECT *prc)
    TSF_MOCK_STUB(GetWnd, HWND *phwnd)

private:
    ~MockContextView() = default;
};

// The composition object: holds the range it was started on (composition
// channel), reports it back through GetRange, and ends the composition.
class MockComposition : public ITfComposition, public RefCount {
public:
    MockComposition(MockContext *ctx, MockRange *range) : ctx_(ctx), range_(range) {}
    STDMETHOD(GetRange)(ITfRange **ppRange) override {
        if (!ppRange)
            return E_INVALIDARG;
        *ppRange = range_;
        if (range_)
            range_->AddRef();
        return S_OK;
    }
    STDMETHOD(EndComposition)(TfEditCookie ecWrite) override; /* out-of-line */
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfComposition)) {
            *ppv = static_cast<ITfComposition *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

    TSF_MOCK_STUB(ShiftStart, TfEditCookie ecWrite, ITfRange *pNewStart)
    TSF_MOCK_STUB(ShiftEnd, TfEditCookie ecWrite, ITfRange *pNewEnd)

private:
    MockComposition() = default;
    ~MockComposition() {
        if (range_)
            range_->Release();
    }
    MockContext *ctx_;
    MockRange *range_; // the composition channel's range; owned here
};

// The display-attribute property channel: records SetValue/Clear.
class MockProperty : public ITfProperty, public RefCount {
public:
    explicit MockProperty(MockContext *ctx) : ref_(1), ctx_(ctx) {}
    STDMETHOD(FindRange)(TfEditCookie ec, ITfRange *pRange, ITfRange **ppRange, TfAnchor aPos) override {
        (void)ec;
        (void)pRange;
        (void)ppRange;
        (void)aPos;
        return E_NOTIMPL;
    }
    STDMETHOD(SetValueStore)(TfEditCookie ec, ITfRange *pRange, ITfPropertyStore *pPropStore) override {
        (void)ec;
        (void)pRange;
        (void)pPropStore;
        return E_NOTIMPL;
    }
    STDMETHOD(SetValue)(TfEditCookie ec, ITfRange *pRange, const VARIANT *pvarValue) override;
    STDMETHOD(Clear)(TfEditCookie ec, ITfRange *pRange) override;
    STDMETHOD(GetType)(GUID *pguid) override;
    STDMETHOD(EnumRanges)(TfEditCookie ec, IEnumTfRanges **ppEnum, ITfRange *pTargetRange) override {
        (void)ec;
        (void)ppEnum;
        (void)pTargetRange;
        return E_NOTIMPL;
    }
    STDMETHOD(GetValue)(TfEditCookie ec, ITfRange *pRange, VARIANT *pvarValue) override {
        (void)ec;
        (void)pRange;
        (void)pvarValue;
        return E_NOTIMPL;
    }
    STDMETHOD(GetContext)(ITfContext **ppContext) override {
        (void)ppContext;
        return E_NOTIMPL;
    }
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfProperty)) {
            *ppv = static_cast<ITfProperty *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

private:
    ~MockProperty() = default;
    LONG ref_;
    MockContext *ctx_;
};

// The document. RequestEditSession executes the session synchronously -- the
// TF_ES_SYNC contract the adapter relies on -- after recording it, so the
// harness reads insertions/session count straight after OnKeyDown returns.
class MockContext : public ITfContext, public ITfContextComposition, public RefCount {
public:
    static constexpr TfEditCookie kCookie = 1;

    std::wstring doc; // the client document text
    std::vector<std::wstring> insertions; // one entry per NON-composition SetText, in order
    int sessionCount = 0;

    // Composition channel (issue #98): what StartComposition/EndComposition
    // did, the rewritten composition text per SetText through a composition
    // range, and the GUID_PROP_ATTRIBUTE writes the adapter's provider applied.
    bool compositionActive = false;
    int compositionStarts = 0;
    int compositionEnds = 0;
    std::wstring compositionText; // last full composition-range content
    std::vector<std::wstring> compositionInsertions; // one per composition-range SetText
    int attrWrites = 0; // SetValue(TF composition range)
    int attrClears = 0; // Clear on finalize

    STDMETHOD(RequestEditSession)(TfClientId tid, ITfEditSession *pes, DWORD dwFlags, HRESULT *phrSession) override {
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
    STDMETHOD(GetSelection)(TfEditCookie ec, ULONG ulIndex, ULONG ulCount, TF_SELECTION *pSelection, ULONG *pcFetched) override {
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
    STDMETHOD(SetSelection)(TfEditCookie ec, ULONG ulCount, const TF_SELECTION *pSelection) override {
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

    /* ITfContextComposition (issue #98): ITfContextComposition derives
     * from IUnknown, so the context answers for it itself; the body is
     * out-of-line below the complete type (the MockRange::SetText shape). */
    STDMETHOD(StartComposition)(TfEditCookie ecWrite, ITfRange *pCompositionRange, ITfCompositionSink *pSink,
                                ITfComposition **ppComposition) override;

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfContext)) {
            *ppv = static_cast<ITfContext *>(this);
            AddRef();
            return S_OK;
        }
        if (IsEqualIID(riid, IID_ITfContextComposition)) {
            /* Issue #98: the composition half of the adapter's contract --
             * ITfContextComposition derives from IUnknown, so the context
             * answers for it itself. */
            *ppv = static_cast<ITfContextComposition *>(this);
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
    STDMETHOD(GetProperty)(REFGUID guidProp, ITfProperty **ppProp) override;
    TSF_MOCK_STUB(EnumCompositions, IEnumITfCompositionView **ppEnum)
    TSF_MOCK_STUB(FindComposition, TfEditCookie ecRead, ITfRange *pTestRange, IEnumITfCompositionView **ppEnum)
    TSF_MOCK_STUB(TakeOwnership, TfEditCookie ecWrite, ITfCompositionView *pComposer, ITfCompositionSink *pSink, ITfComposition **ppComposition)
    TSF_MOCK_STUB(GetAppProperty, REFGUID guidProp, ITfReadOnlyProperty **ppProp)
    TSF_MOCK_STUB(TrackProperties, const GUID **prgProp, ULONG cProp, const GUID **prgAppProp, ULONG cAppProp, ITfReadOnlyProperty **ppProperty)
    TSF_MOCK_STUB(EnumProperties, IEnumTfProperties **ppEnum)
    TSF_MOCK_STUB(GetDocumentMgr, ITfDocumentMgr **ppDm)
    TSF_MOCK_STUB(CreateRangeBackup, TfEditCookie ec, ITfRange *pRange, ITfRangeBackup **ppBackup)

private:
    ~MockContext() = default;
    friend class MockProperty;
    friend class MockComposition;
    friend class MockRange;
    friend class MockProperty;
    friend class MockComposition;
    friend class MockContextComposition;
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

    TSF_MOCK_STUB(CreateContext, TfClientId tidOwner, DWORD dwFlags, IUnknown *punk, ITfContext **ppic, TfEditCookie *pecTextStore)
    TSF_MOCK_STUB(Push, ITfContext *pic)
    TSF_MOCK_STUB(Pop, DWORD dwFlags)
    TSF_MOCK_STUB(GetBase, ITfContext **ppic)
    TSF_MOCK_STUB(EnumContexts, IEnumTfContexts **ppEnum)

private:
    ~MockDocumentMgr() = default;
    MockContext *top_;
};

// ---- Session doubles (issue #107): the surfaces the adapter reaches through
// the thread manager once it has one. Activate() writes the conversion
// compartment and attaches the language-bar item, and Deactivate() is expected
// to undo both; before these doubles every one of those calls answered
// E_NOINTERFACE and the paths ran unobserved.
class MockCompartmentMgr;

// One SetValue on a compartment: what the adapter asked the host to publish.
struct CompartmentWrite {
    GUID guid;
    VARTYPE vt;
    LONG value;
};

// ITfCompartment: the per-thread state the language bar reads (the 中/英
// badge). Every SetValue lands in the MANAGER's list: the compartment object is
// released before the assertion runs (a real GetCompartment hands back an owned
// object, and the adapter releases it).
class MockCompartment : public ITfCompartment, public RefCount {
public:
    MockCompartment(MockCompartmentMgr *mgr, REFGUID guid) : mgr_(mgr), guid_(guid) {}
    STDMETHOD(SetValue)(TfClientId tid, const VARIANT *pvarValue) override; /* out-of-line */
    STDMETHOD(GetValue)(VARIANT *pvarValue) override {
        if (!pvarValue)
            return E_INVALIDARG;
        *pvarValue = last_;
        return S_OK;
    }

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfCompartment)) {
            *ppv = static_cast<ITfCompartment *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

private:
    ~MockCompartment() = default;
    MockCompartmentMgr *mgr_;
    GUID guid_;
    VARIANT last_ = {};
};

// ITfCompartmentMgr: GetCompartment answers the conversion compartment the
// adapter writes at Activate / every mode toggle / ApplyConfigStore.
class MockCompartmentMgr : public ITfCompartmentMgr, public RefCount {
public:
    // present = FALSE models a host with no such compartment -- the adapter's
    // "conversion compartment GONE" branch.
    bool present = true;
    std::vector<CompartmentWrite> writes;
    std::vector<GUID> asked; // every GetCompartment guid, in order

    STDMETHOD(GetCompartment)(REFGUID rguid, ITfCompartment **ppcomp) override;
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfCompartmentMgr)) {
            *ppv = static_cast<ITfCompartmentMgr *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

    TSF_MOCK_STUB(ClearCompartment, TfClientId tid, REFGUID rguid)
    TSF_MOCK_STUB(EnumCompartments, IEnumGUID **ppEnum)

private:
    friend class MockThreadMgr; /* holds these by value */
    ~MockCompartmentMgr() = default;
};

// ITfLangBarItemMgr: what LangBarItem::Attach()/Detach() call. addItemHr models
// the MEASURED host fact from issue #67 -- on this host AddItem answered
// 0x80004005 (E_FAIL) for any new item, whatever the item's own shape, so the
// item never reached the bar. The contract's answer is S_OK; the quirk is opted
// into by a test that says so, never the default.
class MockLangBarItemMgr : public ITfLangBarItemMgr, public RefCount {
public:
    HRESULT addItemHr = S_OK;
    // Pointers only: a live language bar holds its own reference, and the item
    // belongs to the service here. Identity, order and count are what the
    // assertions read.
    std::vector<ITfLangBarItem *> added;
    std::vector<ITfLangBarItem *> removed;

    STDMETHOD(AddItem)(ITfLangBarItem *punk) override {
        if (FAILED(addItemHr))
            return addItemHr;
        added.push_back(punk);
        return S_OK;
    }
    STDMETHOD(RemoveItem)(ITfLangBarItem *punk) override {
        removed.push_back(punk);
        return S_OK;
    }

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItemMgr)) {
            *ppv = static_cast<ITfLangBarItemMgr *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

    TSF_MOCK_STUB(EnumItems, IEnumTfLangBarItems **ppEnum)
    TSF_MOCK_STUB(GetItem, REFGUID rguid, ITfLangBarItem **ppItem)
    TSF_MOCK_STUB(AdviseItemSink, ITfLangBarItemSink *punk, DWORD *pdwCookie, REFGUID rguidItem)
    TSF_MOCK_STUB(UnadviseItemSink, DWORD dwCookie)
    TSF_MOCK_STUB(GetItemFloatingRect, DWORD dwThreadId, REFGUID rguid, RECT *prc)
    TSF_MOCK_STUB(GetItemsStatus, ULONG ulCount, const GUID *prgguid, DWORD *pdwStatus)
    TSF_MOCK_STUB(GetItemNum, ULONG *pulCount)
    TSF_MOCK_STUB(GetItems, ULONG ulCount, ITfLangBarItem **ppItem, TF_LANGBARITEMINFO *pInfo, DWORD *pdwStatus, ULONG *pcFetched)
    TSF_MOCK_STUB(AdviseItemsSink, ULONG ulCount, ITfLangBarItemSink **ppunk, const GUID *pguidItem, DWORD *pdwCookie)
    TSF_MOCK_STUB(UnadviseItemsSink, ULONG ulCount, DWORD *pdwCookie)

private:
    friend class MockThreadMgr; /* holds these by value */
    ~MockLangBarItemMgr() = default;
};

// One object answering for ITfThreadMgr + ITfSource + ITfKeystrokeMgr, the
// three identities the adapter QIs out of it during Activate/Deactivate.
class MockThreadMgr : public ITfThreadMgr, public ITfSource, public ITfKeystrokeMgr, public RefCount {
public:
    ITfKeyEventSink *keySink = nullptr;
    BOOL keySinkForeground = FALSE;
    /* A host whose ITfSource::AdviseSink refuses: the service then holds no
     * cookie and must not unadvise one (issue #145). */
    HRESULT adviseSinkHr = S_OK;
    DWORD adviseCookie = 0;

    /* Issue #107: both advice registrations, recorded per call, so
     * the advise/unadvise pairing is an observation and not a claim. */
    struct KeySinkAdvice {
        TfClientId tid;
        ITfKeyEventSink *sink;
        BOOL foreground;
    };
    std::vector<KeySinkAdvice> keySinkAdvises;
    std::vector<TfClientId> keySinkUnadvises;
    std::vector<DWORD> eventSinkAdvises;
    std::vector<DWORD> eventSinkUnadvises;

    /* The session doubles the adapter reaches through QI (issue #107). */
    MockCompartmentMgr compartmentMgr;
    MockLangBarItemMgr langBarMgr;

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfThreadMgr))
            *ppv = static_cast<ITfThreadMgr *>(this);
        else if (IsEqualIID(riid, IID_ITfSource))
            *ppv = static_cast<ITfSource *>(this);
        else if (IsEqualIID(riid, IID_ITfKeystrokeMgr))
            *ppv = static_cast<ITfKeystrokeMgr *>(this);
        else if (IsEqualIID(riid, IID_ITfCompartmentMgr)) {
            /* The member doubles are EMBEDDED, not heap objects: their own
             * AddRef/Release pair is what the caller owns, so the ref must
             * move on the member. Adding this object's ref instead made the
             * adapter's Release() drop the member's count to 0 and free a
             * subobject of this one (measured: STATUS_HEAP_CORRUPTION,
             * 0xC0000374, in the first config case that activated). */
            *ppv = static_cast<ITfCompartmentMgr *>(&compartmentMgr);
            compartmentMgr.AddRef();
            return S_OK;
        } else if (IsEqualIID(riid, IID_ITfLangBarItemMgr)) {
            *ppv = static_cast<ITfLangBarItemMgr *>(&langBarMgr);
            langBarMgr.AddRef();
            return S_OK;
        } else {
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
        if (FAILED(adviseSinkHr))
            return adviseSinkHr;
        eventSink_ = punk;
        if (pdwCookie)
            *pdwCookie = ++adviseCookie;
        eventSinkAdvises.push_back(adviseCookie);
        return S_OK;
    }
    STDMETHOD(UnadviseSink)(DWORD dwCookie) override {
        eventSinkUnadvises.push_back(dwCookie);
        /* A cookie the manager never handed out is refused, as the real
         * ITfSource::UnadviseSink does -- so a test that reads this list
         * cannot mistake a stale cookie for a working unadvise. */
        if (adviseCookie == 0 || dwCookie != adviseCookie)
            return E_INVALIDARG;
        eventSink_ = nullptr;
        adviseCookie = 0;
        return S_OK;
    }
    /* ITfKeystrokeMgr::AdviseKeyEventSink has no cookie: the pairing is
     * (tid, sink), which is what the list records. */
    STDMETHOD(AdviseKeyEventSink)(TfClientId tid, ITfKeyEventSink *pSink, BOOL fForeground) override {
        keySink = pSink;
        keySinkForeground = fForeground;
        keySinkAdvises.push_back(KeySinkAdvice{tid, pSink, fForeground});
        return S_OK;
    }
    STDMETHOD(UnadviseKeyEventSink)(TfClientId tid) override {
        keySinkUnadvises.push_back(tid);
        if (!keySink)
            return E_INVALIDARG;
        keySink = nullptr;
        return S_OK;
    }

    TSF_MOCK_STUB(Activate, TfClientId *ptid)
    TSF_MOCK_STUB(Deactivate, void)
    TSF_MOCK_STUB(CreateDocumentMgr, ITfDocumentMgr **ppdim)
    TSF_MOCK_STUB(EnumDocumentMgrs, IEnumTfDocumentMgrs **ppEnum)
    TSF_MOCK_STUB(GetFocus, ITfDocumentMgr **ppdimFocus)
    TSF_MOCK_STUB(SetFocus, ITfDocumentMgr *pdimFocus)
    TSF_MOCK_STUB(AssociateFocus, HWND hwnd, ITfDocumentMgr *pdimNew, ITfDocumentMgr **ppdimPrev)
    TSF_MOCK_STUB(IsThreadFocus, BOOL *pfThreadFocus)
    TSF_MOCK_STUB(GetFunctionProvider, REFCLSID clsid, ITfFunctionProvider **ppFuncProv)
    TSF_MOCK_STUB(EnumFunctionProviders, IEnumTfFunctionProviders **ppEnum)
    TSF_MOCK_STUB(GetGlobalCompartment, ITfCompartmentMgr **ppCompMgr)
    TSF_MOCK_STUB(GetForeground, CLSID *pclsid)
    TSF_MOCK_STUB(TestKeyDown, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
    TSF_MOCK_STUB(TestKeyUp, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
    TSF_MOCK_STUB(KeyDown, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
    TSF_MOCK_STUB(KeyUp, WPARAM wParam, LPARAM lParam, BOOL *pfEaten)
    TSF_MOCK_STUB(GetPreservedKey, ITfContext *pic, const TF_PRESERVEDKEY *pprekey, GUID *pguid)
    TSF_MOCK_STUB(IsPreservedKey, REFGUID rguid, const TF_PRESERVEDKEY *pprekey, BOOL *pfRegistered)
    TSF_MOCK_STUB(PreserveKey, TfClientId tid, REFGUID rguid, const TF_PRESERVEDKEY *prekey, const WCHAR *pchDesc, ULONG cchDesc)
    TSF_MOCK_STUB(UnpreserveKey, REFGUID rguid, const TF_PRESERVEDKEY *pprekey)
    TSF_MOCK_STUB(SetPreservedKeyDescription, REFGUID rguid, const WCHAR *pchDesc, ULONG cchDesc)
    TSF_MOCK_STUB(GetPreservedKeyDescription, REFGUID rguid, BSTR *pbstrDesc)
    TSF_MOCK_STUB(SimulatePreservedKey, ITfContext *pic, REFGUID rguid, BOOL *pfEaten)

private:
    ~MockThreadMgr() = default;
    IUnknown *eventSink_ = nullptr;
};

// Out-of-line SetText/Collapse: they need the complete MockContext type.
inline HRESULT MockRange::SetText(TfEditCookie ec, DWORD dwFlags, const WCHAR *pchText, LONG cch) {
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
    if (inComposition_) {
        ctx_->compositionInsertions.push_back(text);
        // The composition range holds the WHOLE composition text: every
        // rewrite rewrites it all, so the last write is the content.
        ctx_->compositionText = text;
    } else {
        ctx_->insertions.push_back(text);
    }
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

inline HRESULT MockRange::Clone(ITfRange **ppClone) {
    if (!ppClone)
        return E_INVALIDARG;
    /* Independent range, same span, same channel (the real ITfRange::Clone
     * contract the composition caret path relies on). */
    *ppClone = new MockRange(ctx_, start_, end_, inComposition_);
    return S_OK;
}

// ---- Out-of-line definitions (issue #98): the bodies touch MockContext's
// complete type; declared above, defined here, the file's existing shape ----
inline HRESULT MockComposition::EndComposition(TfEditCookie ecWrite) {
    (void)ecWrite;
    // The composed text fuses into the document (it is already spliced
    // there by SetText); the composition stops being active.
    ctx_->compositionActive = false;
    ++ctx_->compositionEnds;
    return S_OK;
}

inline HRESULT MockContext::StartComposition(TfEditCookie ecWrite, ITfRange *pCompositionRange, ITfCompositionSink *pSink,
                                             ITfComposition **ppComposition) {
    (void)ecWrite;
    if (compositionActive) {
        // One composition at a time: the adapter must end one before
        // starting the next -- report failure so a contract break is loud.
        if (ppComposition)
            *ppComposition = nullptr;
        return E_FAIL;
    }
    if (!pCompositionRange || !ppComposition)
        return E_INVALIDARG;
    LONG start = 0, end = 0;
    if (MockRange *src = dynamic_cast<MockRange *>(pCompositionRange)) {
        start = src->startForSelection();
        end = src->endForSelection();
        if (end < start)
            end = start;
    }
    MockRange *r = new MockRange(this, start, end, true);
    MockComposition *c = new MockComposition(this, r);
    /* The range's initial reference passes to MockComposition (its dtor
     * releases it); no Release here. */
    compositionActive = true;
    ++compositionStarts;
    (void)pSink; /* the external-termination path is not exercised here */
    *ppComposition = c;
    return S_OK;
}

inline HRESULT MockContext::GetProperty(REFGUID guidProp, ITfProperty **ppProp) {
    if (!ppProp)
        return E_INVALIDARG;
    /* Only the display-attribute property exists in this mock; anything
     * else is not something the adapter asks for today. */
    if (!IsEqualGUID(guidProp, GUID_PROP_ATTRIBUTE))
        return E_NOTIMPL;
    *ppProp = new MockProperty(this);
    return S_OK;
}

inline HRESULT MockProperty::SetValue(TfEditCookie ec, ITfRange *pRange, const VARIANT *pvarValue) {
    (void)ec;
    (void)pRange;
    (void)pvarValue;
    ++ctx_->attrWrites;
    return S_OK;
}

inline HRESULT MockProperty::Clear(TfEditCookie ec, ITfRange *pRange) {
    (void)ec;
    (void)pRange;
    ++ctx_->attrClears;
    return S_OK;
}

inline HRESULT MockProperty::GetType(GUID *pguid) {
    if (!pguid)
        return E_INVALIDARG;
    *pguid = GUID_PROP_ATTRIBUTE;
    return S_OK;
}

// ---- Out-of-line bodies for the session doubles (issue #107): they touch a
// complete MockCompartmentMgr, the shape MockRange::SetText already uses.
inline HRESULT MockCompartmentMgr::GetCompartment(REFGUID rguid, ITfCompartment **ppcomp) {
    if (!ppcomp)
        return E_INVALIDARG;
    asked.push_back(rguid);
    if (!present) {
        *ppcomp = nullptr;
        return E_FAIL;
    }
    *ppcomp = new MockCompartment(this, rguid);
    return S_OK;
}

inline HRESULT MockCompartment::SetValue(TfClientId tid, const VARIANT *pvarValue) {
    (void)tid;
    if (!pvarValue)
        return E_INVALIDARG;
    last_ = *pvarValue;
    CompartmentWrite w;
    w.guid = guid_;
    w.vt = pvarValue->vt;
    w.value = pvarValue->vt == VT_I4 ? pvarValue->lVal : 0;
    mgr_->writes.push_back(w);
    return S_OK;
}

} // namespace tsfmock
