#include <windows.h>
#include <msctf.h>
#include <cwchar>
#include <cstdio>
#include "hunmin.h"
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

static void HMLOG(const char* fmt, ...) {
    FILE* f = fopen("C:\\\\hunmin-tsf-debug.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fflush(f); fclose(f);
}
static hunmin_ctx* hunmin_ctx_from_module() {
    wchar_t envW[MAX_PATH];
    wchar_t cand[MAX_PATH];
    char dirs[8][MAX_PATH * 3];
    char* argv[8];
    int argc = 0;
    FILE* dbg0 = fopen("C:\\hunmin-tsf-debug.log", "a");

    if (GetEnvironmentVariableW(L"HUNMIN_DATA_DIR", envW, MAX_PATH) > 0 &&
        (GetFileAttributesW(envW) & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
        WideCharToMultiByte(CP_UTF8, 0, envW, -1, dirs[argc], sizeof(dirs[argc]), nullptr, nullptr);
        argv[argc] = dirs[argc];
        ++argc;
        if (dbg0) { fprintf(dbg0, "env data dir: %ls\\n", envW); fflush(dbg0); }
    }

    HMODULE hMod = GetModuleHandleW(L"hunmin-tsf.dll");
    if (!hMod) GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                  (LPCWSTR)(const void*)hunmin_ctx_from_module, &hMod);
    wchar_t modDir[MAX_PATH];
    if (hMod && GetModuleFileNameW(hMod, modDir, MAX_PATH)) {
        PathRemoveFileSpecW(modDir);

        if (dbg0) { fprintf(dbg0, "modDir=%ls\\n", modDir); fflush(dbg0); }

        wchar_t candLocal[MAX_PATH];
        PathCombineW(candLocal, modDir, L"data");
        DWORD attrLocal = GetFileAttributesW(candLocal);
        if (dbg0) { fprintf(dbg0, "candLocal=%ls attr=0x%08lX dir=%d\\n", candLocal, (unsigned long)attrLocal, (attrLocal != INVALID_FILE_ATTRIBUTES) && ((attrLocal & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)); fflush(dbg0); }
        if (attrLocal != INVALID_FILE_ATTRIBUTES && (attrLocal & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
            WideCharToMultiByte(CP_UTF8, 0, candLocal, -1, dirs[argc], sizeof(dirs[argc]), nullptr, nullptr);
            argv[argc] = dirs[argc];
            ++argc;
        }

        wchar_t repo[MAX_PATH];
        wcsncpy_s(repo, modDir, MAX_PATH);
        for (int i = 0; i < 4; ++i) PathRemoveFileSpecW(repo);
        PathCombineW(cand, repo, L"libhunmin\\data");
        DWORD attrRepo = GetFileAttributesW(cand);
        if (dbg0) { fprintf(dbg0, "cand=%ls attr=0x%08lX dir=%d\\n", cand, (unsigned long)attrRepo, (attrRepo != INVALID_FILE_ATTRIBUTES) && ((attrRepo & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)); fflush(dbg0); }
        if (attrRepo != INVALID_FILE_ATTRIBUTES && (attrRepo & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
            WideCharToMultiByte(CP_UTF8, 0, cand, -1, dirs[argc], sizeof(dirs[argc]), nullptr, nullptr);
            argv[argc] = dirs[argc];
            ++argc;
        }
    }
    if (dbg0) { fprintf(dbg0, "argc=%d hMod=%p\\n", argc, (void*)hMod); fflush(dbg0); }
    if (dbg0) fclose(dbg0);

    if (argc == 0) return nullptr;
    return hunmin_ctx_new((const char* const*)argv, argc);
}
// Skeleton edit session for TSF text insertion. Real insertion lives
// inlined in OnKeyDown; this type documents the shape.
class HEditSession : public ITfEditSession {
public:
    HEditSession() : ref_(1) {}
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG r = (ULONG)InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }
    STDMETHOD(DoEditSession)(TfEditCookie ec) override { (void)ec; return S_OK; }
private:
    LONG ref_;
};

static const CLSID CLSID_HunminTSF =
    { 0xF2C70001, 0x0000, 0x0000, { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01 } };

class CTextService : public ITfTextInputProcessor,
                    public ITfThreadMgrEventSink,
                    public ITfKeyEventSink,
                    public ITfCompositionSink {
public:
    CTextService() : ref_(1), threadMgr_(nullptr), clientId_(TF_CLIENTID_NULL), sinkCookie_(0), ctx_(nullptr), ic_(nullptr), composition_(nullptr), hangulMode_(true) {}
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
        FILE* dbg = fopen("C:\\hunmin-tsf-debug.log", "a");
        if (dbg) { fprintf(dbg, "Activate pid=%lu tid=%u\\n", (unsigned long)GetCurrentProcessId(), (unsigned)tid); fflush(dbg); fclose(dbg); }
        if (ptm) {
            ITfKeystrokeMgr* pks = nullptr;
            FILE* dbg2 = fopen("C:\\\\hunmin-tsf-debug.log", "a");
            if (dbg2) fprintf(dbg2, "Advise threadmgr attempt\\n"), fflush(dbg2);
            ITfSource* src = nullptr;
            HRESULT srch = ptm->QueryInterface(IID_ITfSource, (void**)&src);
            HRESULT tmHr = E_NOINTERFACE;
            if (SUCCEEDED(srch) && src) {
                tmHr = src->AdviseSink(IID_ITfThreadMgrEventSink,
                                       static_cast<ITfThreadMgrEventSink*>(this),
                                       &sinkCookie_);
                src->Release();
            }
            if (dbg2) {
                fprintf(dbg2, "  source hr=0x%lX threadmgr advise hr=0x%lX cookie=%lu\\n",
                        (unsigned long)srch, (unsigned long)tmHr, (unsigned long)sinkCookie_);
                fflush(dbg2);
            }
            HRESULT qh = ptm->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pks);
            if (dbg2) { fprintf(dbg2, "Advise QI hr=0x%lX pks=%p\\n", (unsigned long)qh, (void*)pks); fflush(dbg2); }
            if (SUCCEEDED(qh) && pks) {
                HRESULT ah = pks->AdviseKeyEventSink(clientId_, this, TRUE);
                if (dbg2) { fprintf(dbg2, "AdviseKeyEventSink hr=0x%lX\\n", (unsigned long)ah); fflush(dbg2); }
                pks->Release();
            }
            if (dbg2) fclose(dbg2);
        }
        ctx_ = hunmin_ctx_from_module();
        FILE* dbg3 = fopen("C:\\hunmin-tsf-debug.log", "a");
        if (dbg3) { fprintf(dbg3, "  ctx_=%p\\n", (void*)ctx_); fflush(dbg3); }

        if (ctx_) ic_ = hunmin_ic_new(ctx_, "sin3-p2");
        if (dbg3) { fprintf(dbg3, "  ic_=%p after new (layout dubeolsik)\\n", (void*)ic_); fflush(dbg3); if (ctx_) fprintf(dbg3, "  layout_count=%d\\n", hunmin_ctx_layout_count(ctx_)); fflush(dbg3); fclose(dbg3); }
        return S_OK;
    }
    STDMETHOD(Deactivate)() override {
        if (threadMgr_) {
            ITfKeystrokeMgr* pks = nullptr;
            if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pks)) && pks) {
                pks->UnadviseKeyEventSink(clientId_);
                pks->Release();
            }
            threadMgr_->Release(); threadMgr_ = nullptr;
        }
        if (ic_) { hunmin_ic_free(ic_); ic_ = nullptr; }
        if (ctx_) { hunmin_ctx_free(ctx_); ctx_ = nullptr; }
        clientId_ = TF_CLIENTID_NULL;
        return S_OK;
    }

    STDMETHOD(OnInitDocumentMgr)(ITfDocumentMgr* pdm) override { (void)pdm; return S_OK; }
    STDMETHOD(OnUninitDocumentMgr)(ITfDocumentMgr* pdm) override { (void)pdm; return S_OK; }
    STDMETHOD(OnSetFocus)(ITfDocumentMgr* pdmFocus, ITfDocumentMgr* pdmPrevFocus) override {
        FILE* dbg = fopen("C:\\\\hunmin-tsf-debug.log", "a");
        if (dbg) {
            fprintf(dbg, "TM-OnSetFocus pdmFocus=%p pdmPrev=%p modePid=%lu\\n",
                    (void*)pdmFocus, (void*)pdmPrevFocus, (unsigned long)GetCurrentProcessId());
            if (pdmFocus) {
                ITfContext* pic = nullptr;
                HRESULT hrTop = pdmFocus->GetTop(&pic);
                fprintf(dbg, "  GetTop hr=0x%lX pic=%p\\n", (unsigned long)hrTop, (void*)pic);
                if (pic) pic->Release();
            }
            fflush(dbg); fclose(dbg);
        }
        (void)pdmFocus; (void)pdmPrevFocus; return S_OK;
    }
    STDMETHOD(OnPushContext)(ITfContext* pic) override { (void)pic; return S_OK; }
    STDMETHOD(OnPopContext)(ITfContext* pic) override { (void)pic; return S_OK; }

    // ITfKeyEventSink
    STDMETHOD(OnSetFocus)(BOOL fFocus) override { FILE* dbg = fopen("C:\\\\hunmin-tsf-debug.log", "a"); if (dbg) { fprintf(dbg, "KS-OnSetFocus fFocus=%d modePid=%lu\\n", (int)fFocus, (unsigned long)GetCurrentProcessId()); fflush(dbg); fclose(dbg); } (void)fFocus; return S_OK; }
    STDMETHOD(OnTestKeyDown)(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEat) override {
        (void)pic; (void)lParam;
        FILE* dbg = fopen("C:\\\\hunmin-tsf-debug.log", "a");
        BOOL eat = FALSE;
        if (wParam == VK_CAPITAL) eat = TRUE;
        else if (!hangulMode_) eat = FALSE;
        else if ((wParam >= 'A' && wParam <= 'Z') || wParam == VK_SPACE || wParam == VK_BACK) eat = TRUE;
        if (pfEat) *pfEat = eat;
        if (dbg) { fprintf(dbg, "OnTestKeyDown w=%lu eat=%d mode=%d\\n", (unsigned long)wParam, (int)eat, (int)hangulMode_); fflush(dbg); fclose(dbg); }
        return S_OK;
    }
    STDMETHOD(OnKeyDown)(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEat) override {
        (void)pic; (void)lParam;
        FILE* dbg = fopen("C:\\hunmin-tsf-debug.log", "a");
        if (dbg) { fprintf(dbg, "OnKeyDown wParam=%lu ic=%p\\n", (unsigned long)wParam, (void*)ic_); fflush(dbg); }
        if (pfEat) *pfEat = FALSE;
        if (!ic_) { if (dbg) { fprintf(dbg, " ic null -> pass\\n"); fclose(dbg);} return S_OK; }
        // Mode toggle with Caps Lock
        if (wParam == VK_CAPITAL) {
            hangulMode_ = !hangulMode_;
            if (ic_) { hunmin_ic_free(ic_); ic_ = nullptr; }
            if (ctx_) ic_ = hunmin_ic_new(ctx_, hangulMode_ ? "sin3-p2" : "us-qwerty");
            if (pfEat) *pfEat = TRUE;
            if (dbg) { fprintf(dbg, " mode toggle -> %s\\n", hangulMode_ ? "hangul" : "english"); fclose(dbg); }
            return S_OK;
        }
        if (!hangulMode_) {
            if (dbg) fclose(dbg);
            return S_OK; // passthrough English
        }
        char ch = 0;
        if (wParam >= 'A' && wParam <= 'Z') {
            ch = (GetKeyState(VK_SHIFT) < 0) ? (char)wParam : (char)(wParam + 32);
        } else if (wParam == VK_SPACE) {
            ch = ' ';
        } else if (wParam == VK_BACK) {
            ch = 0x08;
        } else if (wParam >= 0x20 && wParam <= 0x7E) {
            ch = (char)wParam;
        }
        // When ch == 0 (punctuation / any key no layout row covers), fall through
        // with the Zero entry: hunmin_ic_process -> save_commit -> false, so the
        // declined branch below flushes the pending syllable and passes the key.
        // Modifier keys (Shift/Ctrl/Alt/Win, Caps) are not text: never flush/passthrough
        // through the engine (would corrupt an in-flight syllable). Other unmapped
        // keys (punct, arrows, Enter, digits handled here as 0) fall through so the
        // declined branch flushes the pending composition and passes the key.
        if (wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU ||
            wParam == VK_LSHIFT || wParam == VK_RSHIFT ||
            wParam == VK_LCONTROL || wParam == VK_RCONTROL ||
            wParam == VK_LMENU || wParam == VK_RMENU ||
            wParam == VK_LWIN || wParam == VK_RWIN ||
            wParam == VK_CAPITAL || wParam == VK_TAB) {
            if (dbg) { fprintf(dbg, " modifier/pass key -> leave composition\\n"); fclose(dbg); }
            return S_OK;
        }
        bool consumed = hunmin_ic_process(ic_, (uint32_t)ch);
        if (consumed) {
            if (dbg) { fprintf(dbg, " consumed, preedit=%s\n", hunmin_ic_preedit(ic_)); fflush(dbg); }
            const char* commit = hunmin_ic_commit(ic_);
            if (commit && commit[0]) {
                int wlen = MultiByteToWideChar(CP_UTF8, 0, commit, -1, nullptr, 0);
                if (wlen > 1) {
                    wchar_t* wbuf = new wchar_t[wlen];
                    MultiByteToWideChar(CP_UTF8, 0, commit, -1, wbuf, wlen);
                    class EditSession : public ITfEditSession {
                    public:
                        EditSession(ITfContext* ctx, CTextService* svc, TfClientId cid, const wchar_t* t)
                            : ref_(1), ctx_(ctx), svc_(svc), cid_(cid), t_(t) {}
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
                            if (svc_->composition_) { svc_->composition_->EndComposition(ec); svc_->composition_ = nullptr; HMLOG("commit: ended composition\n"); }
                                                        if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) && fetched == 1 && sel.range) {
                                                            sel.range->SetText(ec, 0, t_, -1);
                                                            sel.range->Collapse(ec, TF_ANCHOR_END);
                                                            TF_SELECTION ns = sel;
                                                            ctx_->SetSelection(ec, 1, &ns);
                                                            sel.range->Release();
                                                            HMLOG("commit: inserted text\n");
                                                        }
                            return S_OK;
                        }
                    private:
                                            LONG ref_;
                                            ITfContext* ctx_;
                                            CTextService* svc_;
                                            TfClientId cid_;
                                            const wchar_t* t_;
                    };
                    EditSession* es = new EditSession(pic, this, clientId_, wbuf);
                    HRESULT hrSess = S_OK;
                    pic->RequestEditSession(clientId_, es, TF_ES_SYNC | TF_ES_READWRITE, &hrSess);
                    if (dbg) { fprintf(dbg, " commit edit hr=0x%lX commit=%s\\n", (unsigned long)hrSess, commit); fflush(dbg); }
                    es->Release();
                    delete[] wbuf;
                    UpdateComposition(pic);
                }
            }
            UpdateComposition(pic);
            if (pfEat) *pfEat = TRUE;
        } else {
            // Engine declined (space, empty backspace, unmapped): hunmin_ic_process
            // already saved the in-flight syllable into the commit buffer, but only
            // via save_commit. Flush reads it out so the frontend can deliver it,
            // then re-render the now-empty preedit. The original key is NOT eaten
            // (pfEat stays FALSE) and the client processes it -- matching the
            // fcitx/android "used=FALSE => passthrough" contract.
            const char* flushed = hunmin_ic_flush(ic_);
            if (dbg) { fprintf(dbg, " declined, flushed=%s preedit=%s\\n",
                               flushed ? flushed : "", hunmin_ic_preedit(ic_)); fflush(dbg); }
            if (flushed && flushed[0]) {
                int wlen = MultiByteToWideChar(CP_UTF8, 0, flushed, -1, nullptr, 0);
                if (wlen > 1) {
                    wchar_t* wbuf = new wchar_t[wlen];
                    MultiByteToWideChar(CP_UTF8, 0, flushed, -1, wbuf, wlen);
                    class FlushSession : public ITfEditSession {
                    public:
                        FlushSession(ITfContext* ctx, CTextService* svc, TfClientId cid, const wchar_t* t)
                            : ref_(1), ctx_(ctx), svc_(svc), cid_(cid), t_(t) {}
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
                            ITfRange* compRange = nullptr;
                            if (svc_ && svc_->composition_) {
                                if (SUCCEEDED(svc_->composition_->GetRange(&compRange)) && compRange) {
                                    compRange->SetText(ec, 0, L"", -1);
                                    compRange->Collapse(ec, TF_ANCHOR_START);
                                    TF_SELECTION ns = {}; ns.range = compRange;
                                    ctx_->SetSelection(ec, 1, &ns);
                                    compRange->Release();
                                    compRange = nullptr;
                                }
                                svc_->composition_->EndComposition(ec);
                                svc_->composition_ = nullptr;
                            }
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
                        CTextService* svc_;
                        TfClientId cid_;
                        const wchar_t* t_;
                    };
                    FlushSession* fs = new FlushSession(pic, this, clientId_, wbuf);
                    HRESULT hrFlush = S_OK;
                    pic->RequestEditSession(clientId_, fs, TF_ES_SYNC | TF_ES_READWRITE, &hrFlush);
                    fs->Release();
                    delete[] wbuf;
                }
            }
            UpdateComposition(pic);
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

    void UpdateComposition(ITfContext* pic) {
        if (!pic || !ic_) return;
        const char* pre = hunmin_ic_preedit(ic_);
        if (!pre) pre = "";
        int wlen = MultiByteToWideChar(CP_UTF8, 0, pre, -1, nullptr, 0);
        if (wlen <= 1) wlen = 1;
        wchar_t* wbuf = new wchar_t[wlen + 1];
        MultiByteToWideChar(CP_UTF8, 0, pre, -1, wbuf, wlen + 1);

        class CompSession : public ITfEditSession {
        public:
            CompSession(ITfContext* ctx, CTextService* svc, const wchar_t* text)
                : ref_(1), ctx_(ctx), svc_(svc), text_(text) {}
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
                if (svc_->composition_) {
                    if (text_ && text_[0]) {
                        ITfRange* range = nullptr;
                        if (SUCCEEDED(svc_->composition_->GetRange(&range)) && range) {
                            range->SetText(ec, 0, text_, -1);
                            range->Release();
                        }
                    } else {
                        svc_->composition_->EndComposition(ec);
                        svc_->composition_ = nullptr;
                    }
                    return S_OK;
                }
                if (!text_ || !text_[0]) return S_OK;
                TF_SELECTION sel; ULONG cFetched = 0;
                if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched)) && cFetched == 1 && sel.range) {
                    ITfContextComposition* pCtxComp = nullptr;
                    if (SUCCEEDED(ctx_->QueryInterface(IID_ITfContextComposition, (void**)&pCtxComp)) && pCtxComp) {
                        ITfComposition* comp = nullptr;
                        if (SUCCEEDED(pCtxComp->StartComposition(ec, sel.range, svc_, &comp)) && comp) {
                            ITfRange* range = nullptr;
                            if (SUCCEEDED(comp->GetRange(&range)) && range) {
                                range->SetText(ec, 0, text_, -1);
                                range->Release();
                            }
                            svc_->composition_ = comp;
                        }
                        pCtxComp->Release();
                    }
                    sel.range->Release();
                }
                return S_OK;
            }
        private:
            LONG ref_;
            ITfContext* ctx_;
            CTextService* svc_;
            const wchar_t* text_;
        };
        // NOTE: context edit cookie is TF_DEFAULT; sync request still needed.
        CompSession* es = new CompSession(pic, this, wbuf);
        HRESULT hr = S_OK;
        pic->RequestEditSession(clientId_, es, TF_ES_SYNC | TF_ES_READWRITE, &hr);
        es->Release();
        delete[] wbuf;
    }

private:
    LONG ref_;
    ITfThreadMgr* threadMgr_;
    TfClientId clientId_;
    DWORD sinkCookie_;
    hunmin_ctx* ctx_;
    hunmin_ic* ic_;
    ITfComposition* composition_;
    bool hangulMode_;

    // Helper used both for declined/pass-through keys to flush the pending
    // composition exactly once, deleting the already-present composed text.
    class FlushSession : public ITfEditSession {
    public:
        FlushSession(ITfContext* ctx, CTextService* svc, TfClientId cid, const wchar_t* t)
            : ref_(1), ctx_(ctx), svc_(svc), cid_(cid), t_(t) {}
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
            ITfRange* compRange = nullptr;
            if (svc_ && svc_->composition_) {
                if (SUCCEEDED(svc_->composition_->GetRange(&compRange)) && compRange) {
                    compRange->SetText(ec, 0, L"", -1);
                    compRange->Collapse(ec, TF_ANCHOR_START);
                    TF_SELECTION ns = {}; ns.range = compRange;
                    ctx_->SetSelection(ec, 1, &ns);
                    compRange->Release();
                    compRange = nullptr;
                }
                svc_->composition_->EndComposition(ec);
                svc_->composition_ = nullptr;
            }
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
        CTextService* svc_;
        TfClientId cid_;
        const wchar_t* t_;
    };

};
extern "C" HINSTANCE g_hunmin_hinst;
/* placeholder */
extern "C" BOOL DllRegisterServer() {
    // logging
    FILE* dbg = fopen("C:\\hunmin-reg-debug.log", "w");
    wchar_t guidStr[64];
    StringFromGUID2(CLSID_HunminTSF, guidStr, 64);
    if (dbg) { fwprintf(dbg, L"guid=%s\\n", guidStr); fflush(dbg); }

    wchar_t dllPath[MAX_PATH];
    HINSTANCE hinst = g_hunmin_hinst;
    if (!hinst) hinst = GetModuleHandleW(L"hunmin-tsf.dll");
    GetModuleFileNameW(hinst, dllPath, MAX_PATH);
    if (dbg) { fwprintf(dbg, L"module=%s\\n", dllPath); fflush(dbg); }

    // COM CLSID key registration
    HKEY hKey = nullptr;
    HKEY hSub = nullptr;
    wchar_t clsidPath[128];
    swprintf(clsidPath, 128, L"CLSID\\%s", guidStr);

    LSTATUS st = RegCreateKeyExW(HKEY_CLASSES_ROOT, clsidPath, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (st != ERROR_SUCCESS) return FALSE;

    wchar_t inprocPath[256];
    swprintf(inprocPath, 256, L"%s\\InprocServer32", clsidPath);
    st = RegCreateKeyExW(HKEY_CLASSES_ROOT, inprocPath, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hSub, nullptr);
    if (st != ERROR_SUCCESS) { RegCloseKey(hKey); return FALSE; }

    RegSetValueExW(hSub, nullptr, 0, REG_SZ, (const BYTE*)dllPath,
                   (DWORD)((wcslen(dllPath) + 1) * sizeof(wchar_t)));
    const wchar_t* model = L"Apartment";
    RegSetValueExW(hSub, L"ThreadingModel", 0, REG_SZ,
                   (const BYTE*)model, (DWORD)((wcslen(model) + 1) * sizeof(wchar_t)));

    RegCloseKey(hSub);
    RegCloseKey(hKey);

    // TSF TIP category registration (so it is enumerated in input settings).
    HKEY hTIP = nullptr;
    wchar_t tipPath[256];
    swprintf(tipPath, 256, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s", guidStr);
    LSTATUS tipSt = RegCreateKeyExW(HKEY_LOCAL_MACHINE, tipPath, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hTIP, nullptr);
    if (tipSt == ERROR_SUCCESS) {
        const wchar_t* tipName = L"Hunmin";
        RegSetValueExW(hTIP, nullptr, 0, REG_SZ, (const BYTE*)tipName,
                       (DWORD)((wcslen(tipName) + 1) * sizeof(wchar_t)));
        RegCloseKey(hTIP);
    }
    CoInitialize(nullptr);
    ITfInputProcessorProfiles* pProfile = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfiles, (LPVOID*)&pProfile);
    if (dbg) { fwprintf(dbg, L"pProfile CoCreate hr=0x%lX\\n", (unsigned long)hr); fflush(dbg); }
    if (SUCCEEDED(hr) && pProfile) {
        HRESULT regHr = pProfile->Register(CLSID_HunminTSF);
        if (dbg) { fwprintf(dbg, L"pProfile->Register hr=0x%lX\\n", (unsigned long)regHr); fflush(dbg); }
        static const GUID profileGuid =
            { 0xF2C70002,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02} };
        HMODULE hSelf = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           (LPCWSTR)(const void*)DllRegisterServer, &hSelf);
        wchar_t iconPath[MAX_PATH] = L"";
        if (hSelf) GetModuleFileNameW(hSelf, iconPath, MAX_PATH);
        HRESULT addHr = pProfile->AddLanguageProfile(
            CLSID_HunminTSF,
            0x0412,
            profileGuid,
            L"Hunmin",
            (ULONG)wcslen(L"Hunmin"),
            iconPath[0] ? iconPath : nullptr,
            0,
            TRUE);
        if (dbg) { fwprintf(dbg, L"pProfile->AddLanguageProfile hr=0x%lX\\n", (unsigned long)addHr); fflush(dbg); }
        HRESULT enableHr = pProfile->EnableLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid, TRUE);
        if (dbg) { fwprintf(dbg, L"pProfile->EnableLanguageProfile hr=0x%lX\\n", (unsigned long)enableHr); fflush(dbg); }
        HRESULT activateHr = pProfile->ActivateLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid);
        if (dbg) { fwprintf(dbg, L"pProfile->ActivateLanguageProfile hr=0x%lX\\n", (unsigned long)activateHr); fflush(dbg); }
        pProfile->Release();
    }

    ITfCategoryMgr* pCatMgr = nullptr;
    hr = CoCreateInstance(
        CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, (LPVOID*)&pCatMgr);
    if (dbg) { fwprintf(dbg, L"pCatMgr CoCreate hr=0x%lX\\n", (unsigned long)hr); fflush(dbg); }
    if (SUCCEEDED(hr) && pCatMgr) {
        HRESULT c1 = pCatMgr->RegisterCategory(
            CLSID_HunminTSF, GUID_TFCAT_TIP_KEYBOARD, CLSID_HunminTSF);
        HRESULT c2 = pCatMgr->RegisterCategory(
            CLSID_HunminTSF, GUID_TFCAT_CATEGORY_OF_TIP, CLSID_HunminTSF);
        if (dbg) { fwprintf(dbg, L"RegisterCategory keyboard=0x%lX category=0x%lX\\n", (unsigned long)c1, (unsigned long)c2); fflush(dbg); }
        pCatMgr->Release();
    }
    CoUninitialize();
    return TRUE;
}

extern "C" BOOL DllUnregisterServer() {
    wchar_t guidStr[64];
    StringFromGUID2(CLSID_HunminTSF, guidStr, 64);
    wchar_t clsidPath[128];
    swprintf(clsidPath, 128, L"CLSID\\%s", guidStr);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, clsidPath);
    return TRUE;
}
extern "C" const CLSID CLSID_HunminTSF_Export = { 0xF2C70001, 0x0000, 0x0000, { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01 } };

class CClassFactory : public IClassFactory {
public:
    CClassFactory() : ref_(1) {}
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory*>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG n = (ULONG)InterlockedDecrement(&ref_);
        if (n == 0) delete this;
        return n;
    }
    STDMETHOD(CreateInstance)(IUnknown* pOuter, REFIID riid, void** ppv) override {
        if (pOuter) return CLASS_E_NOAGGREGATION;
        CTextService* obj = new CTextService();
        HRESULT hr = obj->QueryInterface(riid, ppv);
        obj->Release();
        return hr;
    }
    STDMETHOD(LockServer)(BOOL lock) override { (void)lock; return S_OK; }
private:
    LONG ref_;
};

extern "C" HRESULT DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    FILE* dbg = fopen("C:\\hunmin-tsf-debug.log", "a");
    wchar_t str[64];
    StringFromGUID2(rclsid, str, 64);
    if (dbg) { fwprintf(dbg, L"DllGetClassObject rclsid=%s\\n", str); fflush(dbg); }
    if (!IsEqualCLSID(rclsid, CLSID_HunminTSF)) {
        if (dbg) { fwprintf(dbg, L"  not registered CLSID\\n"); fclose(dbg); }
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    CClassFactory* factory = new CClassFactory();
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    if (dbg) { fwprintf(dbg, L"  hr=0x%lX\\n", (unsigned long)hr); fclose(dbg); }
    return hr;
}
