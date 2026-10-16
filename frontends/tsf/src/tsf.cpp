#include <windows.h>
#include <msctf.h>
#include "hunmin.h"

static const CLSID CLSID_HunminTSF =
    { 0xF2C70001, 0x0000, 0x0000, { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01 } };

class CTextService : public ITfTextInputProcessor,
                    public ITfThreadMgrEventSink,
                    public ITfKeyEventSink,
                    public ITfCompositionSink {
public:
    CTextService() : ref_(1), threadMgr_(nullptr), clientId_(TF_CLIENTID_NULL), ctx_(nullptr), ic_(nullptr), composition_(nullptr) {}
    virtual ~CTextService() { if (composition_) composition_->EndComposition(0); if (ic_) hunmin_ic_free(ic_); if (ctx_) hunmin_ctx_free(ctx_); }

    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor)) {
            *ppv = static_cast<ITfTextInputProcessor*>(this);
        } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
            *ppv = static_cast<ITfThreadMgrEventSink*>(this);
        } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
            *ppv = static_cast<ITfKeyEventSink*>(this);
        } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
            *ppv = static_cast<ITfCompositionSink*>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG r = (ULONG)InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }

    STDMETHOD(Activate)(ITfThreadMgr* ptm, TfClientId tid) override {
        threadMgr_ = ptm;
        clientId_ = tid;
        if (ptm) ptm->AddRef();
        ctx_ = hunmin_ctx_new(nullptr, 0);
        if (ctx_) ic_ = hunmin_ic_new(ctx_, "dubeolsik");
        return S_OK;
    }
    STDMETHOD(Deactivate)() override {
        if (threadMgr_) { threadMgr_->Release(); threadMgr_ = nullptr; }
        clientId_ = TF_CLIENTID_NULL;
        return S_OK;
    }

    STDMETHOD(OnInitDocumentMgr)(ITfDocumentMgr* pdm) override { (void)pdm; return S_OK; }
    STDMETHOD(OnUninitDocumentMgr)(ITfDocumentMgr* pdm) override { (void)pdm; return S_OK; }
    STDMETHOD(OnSetFocus)(ITfDocumentMgr* pdmFocus, ITfDocumentMgr* pdmPrevFocus) override {
        (void)pdmFocus; (void)pdmPrevFocus; return S_OK;
    }
    STDMETHOD(OnPushContext)(ITfContext* pic) override { (void)pic; return S_OK; }
    STDMETHOD(OnPopContext)(ITfContext* pic) override { (void)pic; return S_OK; }

    // ITfKeyEventSink
    STDMETHOD(OnSetFocus)(BOOL fFocus) override { (void)fFocus; return S_OK; }
    STDMETHOD(OnTestKeyDown)(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEat) override {
        (void)pic; (void)wParam; (void)lParam; if (pfEat) *pfEat = FALSE; return S_OK;
    }
    STDMETHOD(OnKeyDown)(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEat) override {
        (void)pic; (void)lParam;
        if (pfEat) *pfEat = FALSE;
        if (!ic_) return S_OK;
        char ch = 0;
        if (wParam >= 'A' && wParam <= 'Z') {
            ch = (GetKeyState(VK_SHIFT) < 0) ? (char)wParam : (char)(wParam + 32);
        } else if (wParam == VK_SPACE) {
            ch = ' ';
        } else if (wParam == VK_BACK) {
            ch = 0x08;
        }
        if (!ch) return S_OK;
        bool consumed = hunmin_ic_process(ic_, (uint32_t)ch);
        if (consumed) {
            const char* commit = hunmin_ic_commit(ic_);
            if (commit && commit[0]) {
                int wlen = MultiByteToWideChar(CP_UTF8, 0, commit, -1, nullptr, 0);
                if (wlen > 1) {
                    wchar_t* wbuf = new wchar_t[wlen];
                    MultiByteToWideChar(CP_UTF8, 0, commit, -1, wbuf, wlen);
                    class EditSession : public ITfEditSession {
                    public:
                        EditSession(ITfContext* ctx, TfClientId cid, const wchar_t* t)
                            : ref_(1), ctx_(ctx), cid_(cid), t_(t) {}
                        STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
                            if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
                                *ppv = this; AddRef(); return S_OK;
                            }
                            *ppv = nullptr; return E_NOINTERFACE;
                        }
                        STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
                        STDMETHOD_(ULONG, Release)() override {
                            ULONG r = (ULONG)InterlockedDecrement(&ref_);
                            if (r == 0) delete this;
                            return r;
                        }
                        STDMETHOD(DoEditSession)(TfEditCookie ec) override {
                            TF_SELECTION sel; ULONG fetched = 0;
                            if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) && fetched == 1 && sel.range) {
                                sel.range->SetText(ec, 0, t_, -1);
                                sel.range->Collapse(ec, TF_ANCHOR_END);
                                TF_SELECTION ns = sel;
                                ctx_->SetSelection(ec, 1, &ns);
                                sel.range->Release();
                            }
                            return S_OK;
                        }
                    private:
                        LONG ref_;
                        ITfContext* ctx_;
                        TfClientId cid_;
                        const wchar_t* t_;
                    };
                    EditSession* es = new EditSession(pic, clientId_, wbuf);
                    HRESULT hrSess = S_OK;
                    pic->RequestEditSession(clientId_, es, TF_ES_SYNC | TF_ES_READWRITE, &hrSess);
                    es->Release();
                    delete[] wbuf;
                }
            }
            if (pfEat) *pfEat = TRUE;
        }
        return S_OK;
    }

    STDMETHOD(OnTestKeyUp)(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEat) override {
        (void)pic; (void)wParam; (void)lParam; if (pfEat) *pfEat = FALSE; return S_OK;
    }
    STDMETHOD(OnKeyUp)(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEat) override {
        (void)pic; (void)wParam; (void)lParam; if (pfEat) *pfEat = FALSE; return S_OK;
    }
    STDMETHOD(OnPreservedKey)(ITfContext* pic, const GUID& rguid, BOOL* pfEat) override {
        (void)pic; (void)rguid; if (pfEat) *pfEat = FALSE; return S_OK;
    }

    // ITfCompositionSink
    STDMETHOD(OnCompositionTerminated)(TfEditCookie ecWrite, ITfComposition* pComposition) override {
        if (composition_ == pComposition) composition_ = nullptr;
        (void)ecWrite; (void)pComposition; return S_OK;
    }

private:
    LONG ref_;
    ITfThreadMgr* threadMgr_;
    TfClientId clientId_;
    hunmin_ctx* ctx_;
    hunmin_ic* ic_;
    ITfComposition* composition_;

};

extern "C" BOOL DllRegisterServer() { return TRUE; }
extern "C" BOOL DllUnregisterServer() { return TRUE; }
extern "C" const CLSID CLSID_HunminTSF_Export = { 0xF2C70001, 0x0000, 0x0000, { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01 } };

extern "C" HRESULT DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!IsEqualCLSID(rclsid, CLSID_HunminTSF)) return CLASS_E_CLASSNOTAVAILABLE;
    CTextService* obj = new CTextService();
    HRESULT hr = obj->QueryInterface(riid, ppv);
    obj->Release();
    return hr;
}
