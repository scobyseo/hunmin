#include "hunmin.h"
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <msctf.h>
#include <richedit.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <windows.h>
#pragma comment(lib, "Shlwapi.lib")

/* Debug logs land next to the DLL (build-msvc/Release inside the checkout),
 * not at a drive root: the registered InprocServer32 path is absolute, so the
 * DLL's own directory is the one clone-independent location every host
 * process can resolve, and it stays inside the workspace. Set
 * HUNMIN_TSF_LOG_DIR to override when that directory is read-only. */
bool hunmin_tsf_log_path(const char *fname, char *out, size_t cap) {
    wchar_t dirW[MAX_PATH];
    wchar_t envW[MAX_PATH];
    DWORD envAttr;
    if (GetEnvironmentVariableW(L"HUNMIN_TSF_LOG_DIR", envW, MAX_PATH) > 0 &&
        (envAttr = GetFileAttributesW(envW)) != INVALID_FILE_ATTRIBUTES &&
        (envAttr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
        lstrcpynW(dirW, envW, MAX_PATH);
    } else {
        HMODULE hMod = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (LPCWSTR)(const void *)&hunmin_tsf_log_path, &hMod) ||
            !hMod)
            return false;
        if (GetModuleFileNameW(hMod, dirW, MAX_PATH) == 0)
            return false;
        if (!PathRemoveFileSpecW(dirW))
            return false;
    }
    char dirA[MAX_PATH * 3];
    if (WideCharToMultiByte(CP_ACP, 0, dirW, -1, dirA, sizeof(dirA), nullptr, nullptr) == 0)
        return false;
    int n = snprintf(out, cap, "%s\\%s", dirA, fname);
    return n > 0 && (size_t)n < cap;
}
/* Maintainer instruction (2026-10-17): debug output is an option, not an
 * ambient side effect. The gate lives in tsf_log_open — the one function
 * every log site opens through — so each site's `if (dbg)` stays intact and
 * reads false while the switch is off: no file is created, nothing is
 * written. HUNMIN_TSF_DEBUG_LOG set to 1 / on / yes / true enables; the
 * variable is re-read per open, so a running session can be flipped without
 * a restart. Registration logging (DllRegisterServer) goes through the same
 * gate and stays silent when off. */
static bool tsf_log_enabled(void) {
    char v[16];
    DWORD n = GetEnvironmentVariableA("HUNMIN_TSF_DEBUG_LOG", v, (DWORD)sizeof(v));
    if (n == 0 || n >= sizeof(v))
        return false;
    for (DWORD i = 0; i < n && v[i]; ++i)
        v[i] = (char)tolower((unsigned char)v[i]);
    return strcmp(v, "1") == 0 || strcmp(v, "on") == 0 || strcmp(v, "yes") == 0 ||
           strcmp(v, "true") == 0;
}
static FILE *tsf_log_open(const char *fname, const char *mode = "a") {
    if (!tsf_log_enabled())
        return nullptr;
    char path[MAX_PATH];
    if (!hunmin_tsf_log_path(fname, path, sizeof(path)))
        return nullptr;
    return fopen(path, mode);
}

static void HMLOG(const char *fmt, ...) {
    FILE *f = tsf_log_open("hunmin-tsf-debug.log");
    if (!f)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fflush(f);
    fclose(f);
}
/* Issue #66: ONE data source wins — the first present in priority order:
 * (1) HUNMIN_DATA_DIR (explicit override), (2) <module>\data (the shipped
 * shape; the build stages libhunmin/data beside the DLL), (3) the repo
 * checkout climb (dev convenience, reached only when neither holds). The
 * earlier additive chain held the same tree twice once staging existed:
 * hunmin_ctx_layout_count() reported 30 for 15 staged files, because
 * hunmin_ctx_new's search path is additive by design — duplicate ids
 * across dirs are not deduped, so overlapping dirs are the caller's to
 * avoid. Measured 2026-10-17. Non-static (issue #71): the settings dialog
 * (settings.cpp) resolves data through this same function, so the dialog's
 * layout lists can never disagree with the adapter's about where data
 * came from. */
hunmin_ctx *hunmin_ctx_from_module() {
    wchar_t envW[MAX_PATH];
    wchar_t modDir[MAX_PATH];
    char dirs[2][MAX_PATH * 3];
    int argc = 0;
    FILE *dbg0 = tsf_log_open("hunmin-tsf-debug.log");

    HMODULE hMod = GetModuleHandleW(L"hunmin-tsf.dll");
    if (!hMod)
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)(const void *)hunmin_ctx_from_module,
                           &hMod);
    if (hMod)
        GetModuleFileNameW(hMod, modDir, MAX_PATH);

    if (GetEnvironmentVariableW(L"HUNMIN_DATA_DIR", envW, MAX_PATH) > 0 &&
        (GetFileAttributesW(envW) & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
        WideCharToMultiByte(CP_UTF8, 0, envW, -1, dirs[argc], sizeof(dirs[argc]), nullptr, nullptr);
        ++argc;
        if (dbg0) {
            fprintf(dbg0, "env data dir: %ls\n", envW);
            fflush(dbg0);
        }
    } else if (hMod) {
        PathRemoveFileSpecW(modDir);
        wchar_t candMod[MAX_PATH];
        PathCombineW(candMod, modDir, L"data");
        DWORD attrMod = GetFileAttributesW(candMod);
        if (dbg0)
            fprintf(dbg0, "candMod=%ls dir=%d\n", candMod,
                    (attrMod != INVALID_FILE_ATTRIBUTES) &&
                        ((attrMod & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY));
        if (attrMod != INVALID_FILE_ATTRIBUTES &&
            (attrMod & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
            /* Shipped shape: data beside the module. Do not also climb to the
             * repo — one tree in the search path, or every layout loads twice. */
            WideCharToMultiByte(CP_UTF8, 0, candMod, -1, dirs[argc], sizeof(dirs[argc]), nullptr, nullptr);
            ++argc;
        } else {
            /* Dev checkout: climb to the repo's libhunmin\data. */
            wchar_t repo[MAX_PATH];
            wcsncpy_s(repo, modDir, MAX_PATH);
            for (int i = 0; i < 4; ++i)
                PathRemoveFileSpecW(repo);
            wchar_t cand[2 * MAX_PATH];
            PathCombineW(cand, repo, L"libhunmin\\data");
            DWORD attrRepo = GetFileAttributesW(cand);
            if (dbg0)
                fprintf(dbg0, "climb=%ls dir=%d\n", cand,
                        (attrRepo != INVALID_FILE_ATTRIBUTES) &&
                            ((attrRepo & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY));
            if (attrRepo != INVALID_FILE_ATTRIBUTES &&
                (attrRepo & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
                WideCharToMultiByte(CP_UTF8, 0, cand, -1, dirs[argc], sizeof(dirs[argc]), nullptr, nullptr);
                ++argc;
            }
        }
    }
    if (dbg0) {
        fprintf(dbg0, "argc=%d hMod=%p\n", argc, (void *)hMod);
        fclose(dbg0);
    }

    if (argc == 0)
        return nullptr;
    const char *argv2[2] = {dirs[0], dirs[1]};
    return hunmin_ctx_new(argv2, argc);
}
/* Configured layout ids resolved against the loaded data (issue #66). The
 * fcitx5 defaults live in config.cpp; the adapter re-derives them here so a
 * ctx created from a partial data dir still gets a working layout. */
static const char *kHangulDefaultId = "sin3-p2";
static const char *kLatinDefaultId = "us-qwerty";

/* Accept a stored layout id only when the engine can instantiate it AND its
 * mode matches the slot (issue #71: the dialog lists layouts split by
 * hunmin_ctx_layout_mode, so a stale registry value naming a latin layout in
 * the hangul slot must degrade to the default, never to a silent mispair). */
static void resolveLayoutSlot(const hunmin_ctx *ctx, const std::string &stored,
                              const char *def, bool wantHangul, std::string *outId,
                              FILE *dbg) {
    const char *candidate = stored.c_str();
    if (candidate[0] == '\0')
        candidate = def;
    if (ctx) {
        for (int i = 0; i < hunmin_ctx_layout_count(ctx); ++i) {
            const char *id = hunmin_ctx_layout_id(ctx, i);
            if (!id || strcmp(id, candidate) != 0)
                continue;
            const char *mode = hunmin_ctx_layout_mode(ctx, i); /* "hangul"|"latin" */
            const bool modeOk =
                wantHangul ? (mode && strcmp(mode, "hangul") == 0)
                           : (mode && strcmp(mode, "latin") == 0);
            if (modeOk && hunmin_ctx_layout_supported(ctx, i)) {
                *outId = id;
                return;
            }
            if (dbg) {
                fprintf(dbg, "  config %s slot: id=%s mode=%s supported=%d -> rejected\n",
                        wantHangul ? "hangul" : "latin", id, mode ? mode : "?",
                        (int)hunmin_ctx_layout_supported(ctx, i));
            }
            break; /* found the id, wrong slot/build: fall to default */
        }
    }
    if (ctx)
        *outId = wantHangul ? kHangulDefaultId : kLatinDefaultId;
    if (dbg && *outId != candidate)
        fprintf(dbg, "  config %s slot: %s -> default %s\n",
                wantHangul ? "hangul" : "latin", candidate, outId->c_str());
}
/* The toggle-key matcher shares this TU (see frontends/tsf/CMakeLists.txt):
 * the harness's GetKeyState seam (#define GetKeyState before including this
 * file) must reach the matcher, not just the adapter. */
#include "keybinding.cpp"
#include "config.h"
#include "settings.h" /* ITfFunctionProvider branch + the apply bridge */
#include "candwindow.h"
/* UTF-8 <-> UTF-16 for the candidate path (issue #67): the suggest API is
 * UTF-8, the popup and the buffered word are wide. Replacements for the two
 * MultiByteToWideChar scatter patterns the adapter already carries — the
 * candidate flow needs them at three call sites and half-drawn double
 * conversions have burned this tree before (the embedded-NUL word buffer). */
static std::wstring HunminUtf8ToWide(const char *utf8) {
    if (!utf8 || !utf8[0])
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (n <= 1)
        return {};
    std::wstring w((size_t)n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], n);
    return w;
}
static std::string HunminWideToUtf8(const std::wstring &w) {
    if (w.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0,
                                nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr,
                        nullptr);
    return s;
}
/* The vk set the candidate window consumes while it is open: choose keys
 * (digits), motion (arrows, pages), Return/Esc closes, and F9 toggles again.
 * Expressed once so OnTestKeyDown and the OnKeyDown window branch cannot
 * drift — the same shared-predicate rule the toggle binding uses. */
static bool hunmin_tsf_candkey_eats_test(unsigned long vk) {
    return vk == VK_F9 || vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_PRIOR ||
           vk == VK_NEXT || (vk >= VK_LEFT && vk <= VK_DOWN) ||
           (vk >= '0' && vk <= '9');
}

// Skeleton edit session for TSF text insertion. Real insertion lives
// inlined in OnKeyDown; this type documents the shape.
class HEditSession : public ITfEditSession {
  public:
    HEditSession() : ref_(1) {}
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppv = this;
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
    STDMETHOD(DoEditSession)(TfEditCookie ec) override {
        (void)ec;
        return S_OK;
    }

  private:
    LONG ref_;
};

static const CLSID CLSID_HunminTSF = {0xF2C70001, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}};
/* The language profile GUID is shared state: registration enables it, so
 * unregistration must name the same GUID to remove it (issue #84). */
static const GUID HunminProfileGuid = {0xF2C70002, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}};

class CTextService : public ITfTextInputProcessor,
                     public ITfThreadMgrEventSink,
                     public ITfKeyEventSink,
                     public ITfCompositionSink {
  public:
    CTextService()
        : ref_(1), threadMgr_(nullptr), clientId_(TF_CLIENTID_NULL), sinkCookie_(0), ctx_(nullptr), ic_(nullptr),
          hangulMode_(true), overlayWnd_(nullptr), hasPreeditRect_(false), lastCtx_(nullptr),
          hangulId_("sin3-p2"), latinId_("us-qwerty"), candActive_(false), candCount_(0), candCursor_(0),
          candTop_(0) {}
    virtual ~CTextService() {
        if (overlayWnd_)
            DestroyWindow(overlayWnd_);
        if (ic_)
            hunmin_ic_free(ic_);
        if (ctx_)
            hunmin_ctx_free(ctx_);
        if (lastCtx_)
            lastCtx_->Release();
    }

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor)) {
            *ppv = static_cast<ITfTextInputProcessor *>(this);
        } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
            *ppv = static_cast<ITfThreadMgrEventSink *>(this);
        } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
            *ppv = static_cast<ITfKeyEventSink *>(this);
        } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
            *ppv = static_cast<ITfCompositionSink *>(this);
        } else if (IsEqualIID(riid, IID_ITfFunctionProvider)) {
            /* Issue #71: the settings surface. QI'able on the live text
             * service object, exactly the TSF-native reachability
             * mechanism; GetFunction(IID_ITfFnConfigure) then hands out the
             * configure function (settings.cpp). */
            *ppv = hunmin_tsf_settings_new_provider(this);
            return S_OK;
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
        if (r == 0)
            delete this;
        return r;
    }

    /* Issue #71: re-read the configuration store and rebuild the live
     * session — the settings dialog's Apply path, no process restart.
     * Activate() reuses it, so there is exactly one config-read + rebuild
     * in the adapter (never two that could drift). The buffered word is
     * untouched (a word boundary flush operates the same after Apply); the
     * IN-FLIGHT syllable of a running composition is lost by the ic
     * swap — acceptable for an explicit Apply, recorded in
     * tests/manual-checklists/tsf-settings-window.md. */
    void ApplyConfigStore() {
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg) {
            fprintf(dbg, "ApplyConfigStore pid=%lu\n", (unsigned long)GetCurrentProcessId());
            fflush(dbg);
        }
        HunminTsfConfig cfg;
        hunmin_tsf_config_load(HKEY_CURRENT_USER, &cfg);
        resolveLayoutSlot(ctx_, cfg.hangulLayout, kHangulDefaultId, true, &hangulId_, dbg);
        resolveLayoutSlot(ctx_, cfg.latinLayout, kLatinDefaultId, false, &latinId_, dbg);
        /* The mode-toggle set (Android KeyBinding semantics): configured
         * chords ADD, and the bare physical Han/Eng key is appended when
         * the store does not carry it — it must never go dead. */
        {
            HunminKeyCombo parsed[16];
            const int nParsed = hunmin_tsf_keybindings_parse(cfg.toggleKeys, parsed, 16);
            toggleKeys_.assign(parsed, parsed + nParsed);
            bool hangeulBare = false;
            for (const HunminKeyCombo &c : toggleKeys_)
                if (c.vk == HUNMIN_TSF_VIRTUAL_HANGEUL && c.mods == 0)
                    hangeulBare = true;
            if (!hangeulBare)
                toggleKeys_.push_back(HunminKeyCombo{HUNMIN_TSF_VIRTUAL_HANGEUL, 0});
            if (dbg) {
                fprintf(dbg, "  toggle keys=%zu (configured bits: %s)\n", toggleKeys_.size(),
                        cfg.toggleKeys[0] ? cfg.toggleKeys : "(none)");
                fflush(dbg);
            }
        }
        /* The candidate window belongs to a composition the ic swap below
         * drops: close it with Apply, like the mode toggle does (issue #67). */
        CloseCandidateWindow();
        if (ic_) {
            hunmin_ic_free(ic_);
            ic_ = nullptr;
        }
        if (ctx_)
            ic_ = hunmin_ic_new(ctx_, hangulMode_ ? hangulId_.c_str() : latinId_.c_str());
        if (dbg) {
            fprintf(dbg, "  applied hangul=%s latin=%s ic=%p\n", hangulId_.c_str(),
                    latinId_.c_str(), (void *)ic_);
            fclose(dbg);
        }
    }

    STDMETHOD(Activate)(ITfThreadMgr *ptm, TfClientId tid) override {
        threadMgr_ = ptm;
        clientId_ = tid;
        if (ptm)
            ptm->AddRef();
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg) {
            fprintf(dbg, "Activate pid=%lu tid=%u\n", (unsigned long)GetCurrentProcessId(), (unsigned)tid);
            fflush(dbg);
            fclose(dbg);
        }
        if (ptm) {
            ITfKeystrokeMgr *pks = nullptr;
            FILE *dbg2 = tsf_log_open("hunmin-tsf-debug.log");
            if (dbg2)
                fprintf(dbg2, "Advise threadmgr attempt\n"), fflush(dbg2);
            ITfSource *src = nullptr;
            HRESULT srch = ptm->QueryInterface(IID_ITfSource, (void **)&src);
            HRESULT tmHr = E_NOINTERFACE;
            if (SUCCEEDED(srch) && src) {
                tmHr = src->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink *>(this),
                                       &sinkCookie_);
                src->Release();
            }
            if (dbg2) {
                fprintf(dbg2, "  source hr=0x%lX threadmgr advise hr=0x%lX cookie=%lu\n", (unsigned long)srch,
                        (unsigned long)tmHr, (unsigned long)sinkCookie_);
                fflush(dbg2);
            }
            HRESULT qh = ptm->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pks);
            if (dbg2) {
                fprintf(dbg2, "Advise QI hr=0x%lX pks=%p\n", (unsigned long)qh, (void *)pks);
                fflush(dbg2);
            }
            if (SUCCEEDED(qh) && pks) {
                HRESULT ah = pks->AdviseKeyEventSink(clientId_, this, TRUE);
                if (dbg2) {
                    fprintf(dbg2, "AdviseKeyEventSink hr=0x%lX\n", (unsigned long)ah);
                    fflush(dbg2);
                }
                pks->Release();
            }
            if (dbg2)
                fclose(dbg2);
        }
        ctx_ = hunmin_ctx_from_module();
        FILE *dbg3 = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg3) {
            fprintf(dbg3, "  ctx_=%p\n", (void *)ctx_);
            fflush(dbg3);
        }

        /* Layout wiring (issue #66), now via ApplyConfigStore() (issue #71):
         * the same re-read + rebuild the settings dialog's Apply drives on
         * a LIVE service, without a process restart — acceptance is
         * identical because the config boundary resets exactly the state
         * Activate() had to do anyway; no separate activation-only copy
         * exists. Activate() starts hangulMode_=true. */
        ApplyConfigStore();
        if (dbg3) {
            if (ctx_)
                fprintf(dbg3, "  layout_count=%d\n", hunmin_ctx_layout_count(ctx_));
            fflush(dbg3);
            fclose(dbg3);
        }
        return S_OK;
    }
    STDMETHOD(Deactivate)() override {
        if (threadMgr_) {
            ITfKeystrokeMgr *pks = nullptr;
            if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pks)) && pks) {
                pks->UnadviseKeyEventSink(clientId_);
                pks->Release();
            }
            threadMgr_->Release();
            threadMgr_ = nullptr;
        }
        if (lastCtx_ && ic_) {
            const char *flushed = hunmin_ic_flush(ic_);
            if (flushed && flushed[0]) {
                int flen = MultiByteToWideChar(CP_UTF8, 0, flushed, -1, nullptr, 0);
                if (flen > 1) {
                    size_t old = wordbuf_.size();
                    /* flen counts the NUL terminator; text only, as above. */
                    wordbuf_.resize(old + flen - 1);
                    MultiByteToWideChar(CP_UTF8, 0, flushed, -1, &wordbuf_[old], flen);
                }
            }
        }
        if (!wordbuf_.empty() && lastCtx_) {
            int wlen = (int)wordbuf_.size() + 1;
            wchar_t *wbuf = new wchar_t[wlen];
            wordbuf_.copy(wbuf, wordbuf_.size());
            wbuf[wordbuf_.size()] = 0;
            class DeactSession : public ITfEditSession {
              public:
                DeactSession(ITfContext *ctx, TfClientId cid, const wchar_t *t)
                    : ref_(1), ctx_(ctx), cid_(cid), t_(t) {}
                STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
                    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
                        *ppv = this;
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
                STDMETHOD(DoEditSession)(TfEditCookie ec) override {
                    TF_SELECTION sel;
                    ULONG fetched = 0;
                    if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) &&
                        fetched == 1 && sel.range) {
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
                ITfContext *ctx_;
                TfClientId cid_;
                const wchar_t *t_;
            };
            DeactSession *ds = new DeactSession(lastCtx_, clientId_, wbuf);
            HRESULT hrS = S_OK;
            lastCtx_->RequestEditSession(clientId_, ds, TF_ES_SYNC | TF_ES_READWRITE, &hrS);
            ds->Release();
            delete[] wbuf;
        }
        wordbuf_.clear();
        CloseCandidateWindow();
        HideOverlay();
        if (ic_) {
            hunmin_ic_free(ic_);
            ic_ = nullptr;
        }
        if (ctx_) {
            hunmin_ctx_free(ctx_);
            ctx_ = nullptr;
        }
        if (lastCtx_) {
            lastCtx_->Release();
            lastCtx_ = nullptr;
        }
        clientId_ = TF_CLIENTID_NULL;
        return S_OK;
    }

    STDMETHOD(OnInitDocumentMgr)(ITfDocumentMgr *pdm) override {
        (void)pdm;
        return S_OK;
    }
    STDMETHOD(OnUninitDocumentMgr)(ITfDocumentMgr *pdm) override {
        (void)pdm;
        return S_OK;
    }
    STDMETHOD(OnSetFocus)(ITfDocumentMgr *pdmFocus, ITfDocumentMgr *pdmPrevFocus) override {
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg) {
            fprintf(dbg, "TM-OnSetFocus pdmFocus=%p pdmPrev=%p modePid=%lu\n", (void *)pdmFocus, (void *)pdmPrevFocus,
                    (unsigned long)GetCurrentProcessId());
            if (pdmFocus) {
                ITfContext *pic = nullptr;
                HRESULT hrTop = pdmFocus->GetTop(&pic);
                fprintf(dbg, "  GetTop hr=0x%lX pic=%p\n", (unsigned long)hrTop, (void *)pic);
                if (pic)
                    pic->Release();
            }
            fflush(dbg);
            fclose(dbg);
        }
        (void)pdmFocus;
        (void)pdmPrevFocus;
        return S_OK;
    }
    STDMETHOD(OnPushContext)(ITfContext *pic) override {
        (void)pic;
        return S_OK;
    }
    STDMETHOD(OnPopContext)(ITfContext *pic) override {
        (void)pic;
        return S_OK;
    }

    // ITfKeyEventSink
    STDMETHOD(OnSetFocus)(BOOL fFocus) override {
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg) {
            fprintf(dbg, "KS-OnSetFocus fFocus=%d modePid=%lu\n", (int)fFocus, (unsigned long)GetCurrentProcessId());
            fflush(dbg);
            fclose(dbg);
        }
        (void)fFocus;
        return S_OK;
    }
    STDMETHOD(OnTestKeyDown)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override {
        (void)pic;
        (void)lParam;
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        /* Shared predicate with OnKeyDown (the registration-work rule: test
         * and down are one decision). The configured set always contains the
         * bare Han/Eng key, so the physical key stays interested in both
         * modes. CapsLock is NOT here any more: it is no longer the mode
         * toggle (maintainer instruction: the Han/Eng key is the default),
         * and in Korean input mode it is ignored outright (the engine's
         * case-encoding convention). */
        BOOL eat = FALSE;
        if (hunmin_tsf_keybindings_match(toggleKeys_.data(), (int)toggleKeys_.size(),
                                         (unsigned)wParam, hunmin_tsf_keymods_from_state()))
            eat = TRUE;
        else if (candActive_ && hunmin_tsf_candkey_eats_test(wParam))
            /* While the candidate window is open its keys are ours, on the
             * same shared-predicate rule the toggle uses (test and down are
             * one decision). */
            eat = TRUE;
        else if (!hangulMode_)
            eat = FALSE;
        else if (wParam == VK_F9) {
            /* Mirror the down decision exactly (no look-ahead lie): the
             * window opens only when the buffered word has candidates, and
             * in the zero-candidate case F9 belongs to the client. The
             * suggest lookup is a read of tables already in memory. */
            eat = (ctx_ && !BufferedHangul().empty() &&
                   hunmin_suggest_count(ctx_, BufferedHangul().c_str()) > 0)
                      ? TRUE
                      : FALSE;
        } else if ((wParam >= 'A' && wParam <= 'Z') || wParam == VK_SPACE || wParam == VK_BACK)
            eat = TRUE;
        if (pfEat)
            *pfEat = eat;
        if (dbg) {
            fprintf(dbg, "OnTestKeyDown w=%lu eat=%d mode=%d\n", (unsigned long)wParam, (int)eat, (int)hangulMode_);
            fflush(dbg);
            fclose(dbg);
        }
        return S_OK;
    }
    STDMETHOD(OnKeyDown)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override {
        (void)lParam;
        if (lastCtx_ != pic) {
            if (lastCtx_)
                lastCtx_->Release();
            lastCtx_ = pic;
            if (lastCtx_)
                lastCtx_->AddRef();
        }
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg) {
            fprintf(dbg, "OnKeyDown wParam=%lu ic=%p\n", (unsigned long)wParam, (void *)ic_);
            fflush(dbg);
        }
        if (pfEat)
            *pfEat = FALSE;
        if (!ic_) {
            if (dbg) {
                fprintf(dbg, " ic null -> pass\n");
                fclose(dbg);
            }
            return S_OK;
        }
        // Mode-toggle keys (issue #66 / #71): the configured bindings plus the
        // always-on bare Han/Eng key (VK_HANGEUL, the platform alternate no
        // rebinding can make dead — the Android rule). A toggle is a mode
        // boundary, i.e. a composition boundary (Jamotong manual §9.5; the
        // Android adapter commits at the mode switch): the pending syllable
        // is flushed and the buffered word inserted before the engine is
        // rebuilt with the other slot's layout. Auto-repeat is keydown with
        // the preceding-down bit set (lParam bit 30): the press must toggle
        // once, and repeats are swallowed rather than delivered to the app.
        if (hunmin_tsf_keybindings_match(toggleKeys_.data(), (int)toggleKeys_.size(),
                                         (unsigned)wParam, hunmin_tsf_keymods_from_state())) {
            if (pfEat)
                *pfEat = TRUE;
            const bool repeat = (lParam & (1 << 30)) != 0;
            if (!repeat) {
                /* A mode switch is a composition boundary: the candidate
                 * window belongs to the composition being flushed, so it
                 * closes with it (issue #67). */
                CloseCandidateWindow();
                BoundaryFlush(pic, dbg);
                hangulMode_ = !hangulMode_;
                if (ic_) {
                    hunmin_ic_free(ic_);
                    ic_ = nullptr;
                }
                if (ctx_)
                    ic_ = hunmin_ic_new(ctx_, hangulMode_ ? hangulId_.c_str() : latinId_.c_str());
            }
            if (dbg) {
                fprintf(dbg, " toggle key vk=%lu repeat=%d -> %s (%s)\n",
                        (unsigned long)wParam, (int)repeat,
                        hangulMode_ ? "hangul" : "english",
                        hangulMode_ ? hangulId_.c_str() : latinId_.c_str());
                fclose(dbg);
            }
            return S_OK;
        }
        if (!hangulMode_) {
            if (dbg)
                fclose(dbg);
            return S_OK; // passthrough English
        }
        /* --- F9 Hanja candidate window (issue #67) ---
         *
         * F9 is intercepted before any engine feed because it is not a
         * printable key (fcitx5's identical placement, engine.cpp:204-223).
         * Consumption is conditional whether the window is open or, with a
         * candidate-bearing buffer, just opened; with neither, F9 belongs
         * to the client app and passes through uneaten. */
        if (wParam == VK_F9) {
            if (candActive_) {
                CloseCandidateWindow();
            } else {
                OpenCandidates();
                if (candActive_)
                    UpdateCandidateWindow(pic);
            }
            if (dbg) {
                fprintf(dbg, " F9 -> %s (src=%s count=%d)\n", candActive_ ? "open" : "closed",
                        candSource_.c_str(), candCount_);
                fflush(dbg);
                fclose(dbg);
            }
            if (pfEat)
                *pfEat = candActive_ ? TRUE : FALSE;
            return S_OK;
        }
        if (candActive_) {
            bool handled = false;
            bool submit = false;
            int submitIdx = candCursor_;
            switch (wParam) {
            case VK_RETURN:
                submit = true;
                handled = true;
                break;
            case VK_ESCAPE:
                CloseCandidateWindow();
                UpdateOverlay(pic);
                handled = true;
                break;
            case VK_UP:
            case VK_LEFT:
                if (candCursor_ > 0) {
                    --candCursor_;
                    PageFollowCursor();
                    UpdateCandidateWindow(pic);
                }
                handled = true;
                break;
            case VK_DOWN:
            case VK_RIGHT:
                if (candCursor_ < candCount_ - 1) {
                    ++candCursor_;
                    PageFollowCursor();
                    UpdateCandidateWindow(pic);
                }
                handled = true;
                break;
            case VK_PRIOR:
                PageStep(-1);
                UpdateCandidateWindow(pic);
                handled = true;
                break;
            case VK_NEXT:
                PageStep(+1);
                UpdateCandidateWindow(pic);
                handled = true;
                break;
            default:
                /* Digit select: 1..9 pick the page-relative candidates 1..9,
                 * 0 picks the tenth, mirroring fcitx5's label scheme. A
                 * digit past the end of the count closes the window and
                 * falls through like any other unhandled key — an invisible
                 * window must never swallow a keystroke. */
                if (wParam >= '0' && wParam <= '9') {
                    int pos = (wParam == '0') ? 9 : (int)(wParam - '1');
                    int idx = candTop_ + pos;
                    if (pos >= 0 && idx < candCount_) {
                        submitIdx = idx;
                        submit = true;
                        handled = true;
                    }
                }
                break;
            }
            if (submit)
                CommitCandidate(pic, submitIdx);
            if (handled) {
                if (pfEat)
                    *pfEat = TRUE;
                if (dbg) {
                    fprintf(dbg, " candidate key vk=%lu cursor=%d count=%d\n", (unsigned long)wParam,
                            candCursor_, candCount_);
                    fclose(dbg);
                }
                return S_OK;
            }
            /* Anything else: close and fall through to the normal path, so
             * typing continues where the user left off instead of being
             * swallowed by an invisible window (fcitx5: the default rule).
             * The buffered word is untouched by the close itself. */
            CloseCandidateWindow();
            UpdateOverlay(pic);
        }
        /* Edit/navigation keys (measured on this host during the buffered-input
         * check: VK_LEFT..VK_NEXT + VK_INSERT/VK_DELETE all fall into the
         * 0x20-0x7E printable fallback below, so each was translated into a
         * layout character (VK_UP 38 -> '&', VK_DOWN 40 -> '(', DELETE 46 ->
         * '.', ...) and EATEN — the editor never saw them and stray punctuation
         * could land in the document. They are keys of passage: a cursor move
         * is a composition boundary, so the boundary drain commits the buffered
         * word exactly once and the key passes through untouched
         * (pfEat = FALSE, matching the OnTestKeyDown predicate which already
         * shows no interest in them). Reached only while the candidate window
         * is NOT open: when it is, the block above owns these vk's for
         * list motion (issue #67). */
        switch (wParam) {
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_INSERT:
        case VK_DELETE:
            BoundaryFlush(pic, dbg);
            if (pfEat)
                *pfEat = FALSE;
            if (dbg) {
                fprintf(dbg, " edit key vk=%lu -> boundary + passthrough\n", (unsigned long)wParam);
                fclose(dbg);
            }
            return S_OK;
        default:
            break;
        }
        char ch = 0;
        if (wParam >= 'A' && wParam <= 'Z') {
            ch = (GetKeyState(VK_SHIFT) < 0) ? (char)wParam : (char)(wParam + 32);
        } else if (wParam >= '0' && wParam <= '9') {
            static const char unshifted[] = "0123456789";
            static const char shifted[] = ")!@#$%^&*(";
            int i = (int)(wParam - '0');
            ch = (GetKeyState(VK_SHIFT) < 0) ? shifted[i] : unshifted[i];
        } else if (wParam == VK_SPACE) {
            ch = ' ';
        } else if (wParam == VK_BACK) {
            ch = 0x08;
        } else {
            bool shift = GetKeyState(VK_SHIFT) < 0;
            switch (wParam) {
            case VK_OEM_1:
                ch = shift ? ':' : ';';
                break;
            case VK_OEM_7:
                ch = shift ? '"' : '\'';
                break;
            case VK_OEM_COMMA:
                ch = shift ? '<' : ',';
                break;
            case VK_OEM_PERIOD:
                ch = shift ? '>' : '.';
                break;
            case VK_OEM_2:
                ch = shift ? '?' : '/';
                break;
            case VK_OEM_3:
                ch = shift ? '~' : '`';
                break;
            case VK_OEM_4:
                ch = shift ? '{' : '[';
                break;
            case VK_OEM_5:
                ch = shift ? '|' : '\\';
                break;
            case VK_OEM_6:
                ch = shift ? '}' : ']';
                break;
            case VK_OEM_MINUS:
                ch = shift ? '_' : '-';
                break;
            case VK_OEM_PLUS:
                ch = shift ? '+' : '=';
                break;
            default:
                if (wParam >= 0x20 && wParam <= 0x7E)
                    ch = (char)wParam;
                break;
            }
        }
        // When ch == 0 (punctuation / any key no layout row covers), fall through
        // with the Zero entry: hunmin_ic_process -> save_commit -> false, so the
        // declined branch below flushes the pending syllable and passes the key.
        // Modifier keys (Shift/Ctrl/Alt/Win, Caps) are not text: never flush/passthrough
        // through the engine (would corrupt an in-flight syllable). Other unmapped
        // keys (punct, arrows, Enter, digits handled here as 0) fall through so the
        // declined branch flushes the pending composition and passes the key.
        if (wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU || wParam == VK_LSHIFT ||
            wParam == VK_RSHIFT || wParam == VK_LCONTROL || wParam == VK_RCONTROL || wParam == VK_LMENU ||
            wParam == VK_RMENU || wParam == VK_LWIN || wParam == VK_RWIN || wParam == VK_CAPITAL || wParam == VK_TAB) {
            /* CapsLock is no longer the mode toggle (maintainer instruction:
             * the Han/Eng key is the default), and in Korean input mode it is
             * IGNORED outright: the engine's input convention encodes the
             * shifted row in the case of the fed char (hunmin.h "case
             * encodes shift"), so no caps-derived case or state may leak into
             * hunmin_ic_process. The fed char derives from the VK and the
             * Shift bit alone below; the caps state is never read. CapsLock
             * passes through for the OS to toggle. The engine caps option is
             * deliberately NOT wired from this frontend (fcitx5 does, for the
             * yethangul P variable, engine.cpp:327 — recorded in
             * docs/WINDOWS-TSF-CONFIG.md as a parity difference awaiting a
             * maintainer decision). */
            if (dbg) {
                fprintf(dbg, " modifier/pass key -> leave composition\n");
                fclose(dbg);
            }
            return S_OK;
        }
        bool consumed = hunmin_ic_process(ic_, (uint32_t)ch);
        if (consumed) {
            /* Word buffering (issue #73, adapter-side; no libhunmin change):
             * hunmin_ic_commit() is a destructive read of every syllable the
             * engine finalised so far, so the adapter accumulates it instead
             * of inserting per syllable. Insertion happens exactly once, on
             * the first boundary key (declined path below). */
            const char *commit = hunmin_ic_commit(ic_);
            if (commit && commit[0]) {
                int clen = MultiByteToWideChar(CP_UTF8, 0, commit, -1, nullptr, 0);
                if (clen > 1) {
                    size_t old = wordbuf_.size();
                    /* clen counts the NUL terminator; the buffer holds text
                     * only, or every later insert/overlay would stop at the
                     * embedded NUL after the first syllable. */
                    wordbuf_.resize(old + clen - 1);
                    MultiByteToWideChar(CP_UTF8, 0, commit, -1, &wordbuf_[old], clen);
                }
            }
            if (dbg) {
                fprintf(dbg, " consumed, word len=%zu preedit=%s\n", wordbuf_.size(), hunmin_ic_preedit(ic_));
                fflush(dbg);
            }
            UpdateOverlay(pic);
            if (pfEat)
                *pfEat = TRUE;
        } else {
            /* Engine declined (space, empty backspace, unmapped): a word
             * boundary for the buffering adapter (issue #73). The engine has
             * already saved the in-flight syllable via save_commit; the
             * boundary drain flushes it, the adapter appends whatever word it
             * has been holding, and the whole word is inserted exactly once
             * before the boundary key passes through (pfEat stays FALSE) --
             * matching the fcitx/android "used=FALSE => passthrough"
             * contract. */
            BoundaryFlush(pic, dbg);
        }
        return S_OK;
    }

    STDMETHOD(OnTestKeyUp)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override {
        (void)pic;
        (void)wParam;
        (void)lParam;
        if (pfEat)
            *pfEat = FALSE;
        return S_OK;
    }
    STDMETHOD(OnKeyUp)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override {
        (void)pic;
        (void)wParam;
        (void)lParam;
        if (pfEat)
            *pfEat = FALSE;
        return S_OK;
    }
    STDMETHOD(OnPreservedKey)(ITfContext *pic, const GUID &rguid, BOOL *pfEat) override {
        (void)pic;
        (void)rguid;
        if (pfEat)
            *pfEat = FALSE;
        return S_OK;
    }

    // ITfCompositionSink
    STDMETHOD(OnCompositionTerminated)(TfEditCookie ecWrite, ITfComposition *pComposition) override {
        (void)ecWrite;
        (void)pComposition;
        return S_OK;
    }

    void HideOverlay() {
        if (overlayWnd_)
            ShowWindow(overlayWnd_, SW_HIDE);
    }

    void UpdateOverlay(ITfContext *pic) {
        if (!pic || !ic_) {
            HideOverlay();
            return;
        }
        const char *pre = hunmin_ic_preedit(ic_);
        /* Overlay shows the whole buffered word (issue #73): accumulated
         * finalised syllables plus the in-flight preedit. */
        size_t wword = wordbuf_.size();
        int wlen = MultiByteToWideChar(CP_UTF8, 0, pre, -1, nullptr, 0);
        if (wlen < 0)
            wlen = 0;
        if (wword + (size_t)wlen <= 1) {
            HideOverlay();
            return;
        }
        wchar_t *wbuf = new wchar_t[wword + (size_t)wlen];
        memcpy(wbuf, wordbuf_.c_str(), wword * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, pre, -1, wbuf + wword, wlen);

        RECT rc;
        bool haveCaret = false;
        GUITHREADINFO gti;
        if (!AcquireCaretRect(pic, &rc, &haveCaret, &gti)) {
            delete[] wbuf;
            HideOverlay();
            return;
        }

        if (!overlayWnd_) {
            HWND fontWindow = haveCaret ? (gti.hwndFocus ? gti.hwndFocus : gti.hwndCaret) : nullptr;
            DPI_AWARENESS_CONTEXT oldCtx = nullptr;
            if (fontWindow)
                oldCtx = SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(fontWindow));
            overlayWnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", wbuf,
                                          WS_POPUP | SS_LEFTNOWORDWRAP, rc.left, rc.top, 1, 1, nullptr, nullptr,
                                          GetModuleHandleW(L"hunmin-tsf.dll"), nullptr);
            if (oldCtx)
                SetThreadDpiAwarenessContext(oldCtx);
        }

        HFONT hFont = nullptr;
        HWND fontHwnd = haveCaret ? (gti.hwndFocus ? gti.hwndFocus : gti.hwndCaret) : nullptr;
        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize = sizeof(cf);
        if (fontHwnd && SendMessageW(fontHwnd, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf) &&
            (cf.dwMask & (CFM_FACE | CFM_SIZE)) == (CFM_FACE | CFM_SIZE)) {
            LOGFONTW lf;
            ZeroMemory(&lf, sizeof(lf));
            UINT dpi = fontHwnd ? GetDpiForWindow(fontHwnd) : 96;
            if (dpi == 0)
                dpi = 96;
            lf.lfHeight = -(int)((cf.yHeight * dpi + 720) / 1440);
            if (lf.lfHeight == 0)
                lf.lfHeight = -12;
            lf.lfWeight = (cf.dwEffects & CFE_BOLD) ? FW_BOLD : FW_NORMAL;
            lf.lfItalic = (cf.dwEffects & CFE_ITALIC) ? 1 : 0;
            wcscpy_s(lf.lfFaceName, LF_FACESIZE, cf.szFaceName);
            hFont = CreateFontIndirectW(&lf);
        }
        if (!hFont && fontHwnd)
            hFont = (HFONT)SendMessageW(fontHwnd, WM_GETFONT, 0, 0);
        if (!hFont)
            hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        LOGFONTW lf = {};
        GetObjectW(hFont, sizeof(lf), &lf);
        UINT srcDpi = fontHwnd ? GetDpiForWindow(fontHwnd) : 0;
        UINT dstDpi = overlayWnd_ ? GetDpiForWindow(overlayWnd_) : 0;

        HWND metricWindow = fontHwnd ? fontHwnd : overlayWnd_;
        HDC dc = GetDC(metricWindow);
        SIZE sz = {};
        if (dc) {
            HFONT oldFont = (HFONT)SelectObject(dc, hFont);
            GetTextExtentPoint32W(dc, wbuf, (int)wcslen(wbuf), &sz);
            SelectObject(dc, oldFont);
            ReleaseDC(metricWindow, dc);
        }
        wchar_t fontClass[128] = L"?";
        if (fontHwnd)
            GetClassNameW(fontHwnd, fontClass, 128);
        HMLOG(
            "UpdateOverlay: haveCaret=%d fontHwnd=%p class=%ls lfFace=%ls lfHeight=%ld srcDpi=%u dstDpi=%u sz=%dx%d\n",
            (int)haveCaret, (void *)fontHwnd, fontClass, lf.lfFaceName, (long)lf.lfHeight, srcDpi, dstDpi, (int)sz.cx,
            (int)sz.cy);
        if (hFont && overlayWnd_)
            SendMessageW(overlayWnd_, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (sz.cx < 1)
            sz.cx = 10;
        if (sz.cy < 1)
            sz.cy = 14;
        int pad = 6;
        int caretH = rc.bottom - rc.top;
        if (caretH < 1)
            caretH = sz.cy;
        int y = rc.top + (caretH - sz.cy) / 2;
        if (y < rc.top)
            y = rc.top;
        MoveWindow(overlayWnd_, rc.left, y, sz.cx + pad, sz.cy + pad, TRUE);
        SetWindowTextW(overlayWnd_, wbuf);
        ShowWindow(overlayWnd_, SW_SHOWNA);

        delete[] wbuf;
    }

  private:
    LONG ref_;
    ITfThreadMgr *threadMgr_;
    TfClientId clientId_;
    DWORD sinkCookie_;
    hunmin_ctx *ctx_;
    hunmin_ic *ic_;
    bool hangulMode_;
    /* The resolved layout pair (issue #66): hangulMode_ switches between
     * these, config-read at Activate and re-read by the settings dialog
     * (#71). Defaults match the fcitx5 addon config. */
    std::string hangulId_, latinId_;
    /* Mode-toggle bindings (maintainer instruction: the Han/Eng key is the
     * default; additional keys configure through the store). Always carries
     * the bare physical Han/Eng key — the platform alternate that survives
     * every rebinding, the rule Android's KeyAction.HANGUL_ENG encodes. */
    std::vector<HunminKeyCombo> toggleKeys_;
    HWND overlayWnd_;
    RECT preeditRect_;
    bool hasPreeditRect_;
    ITfContext *lastCtx_;

    /* Finalised syllables held for insertion at the next boundary key
     * (issue #73); drained destructively by hunmin_ic_commit(). */
    std::wstring wordbuf_;

    /* F9 Hanja candidates (issue #67). The window anchors to the WHOLE
     * buffered word — accumulated finalised syllables plus the in-flight
     * preedit — which is the multi-syllable lookup the dictionary tables
     * are keyed by (krdict words, idioms); fcitx5 anchors to its preedit
     * only because it commits each syllable immediately. */
    bool candActive_;
    int candCount_;
    int candCursor_; /* global 0-based cursor */
    int candTop_; /* first index of the visible 10-row page */
    std::string candSource_; /* the Hangul the list was looked up for */
    HunminCandWindow candWnd_;

    // Helper used both for declined/pass-through keys to flush the pending
    // composition exactly once, deleting the already-present composed text.
    class FlushSession : public ITfEditSession {
      public:
        FlushSession(ITfContext *ctx, CTextService *svc, TfClientId cid, const wchar_t *t)
            : ref_(1), ctx_(ctx), svc_(svc), cid_(cid), t_(t) {}
        STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
            if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
                *ppv = this;
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
        STDMETHOD(DoEditSession)(TfEditCookie ec) override {
            TF_SELECTION sel;
            ULONG fetched = 0;
            HRESULT hr = E_NOINTERFACE;
            if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) && fetched == 1 &&
                sel.range) {
                sel.range->SetText(ec, 0, t_, -1);
                sel.range->Collapse(ec, TF_ANCHOR_END);
                TF_SELECTION ns = sel;
                ctx_->SetSelection(ec, 1, &ns);
                sel.range->Release();
                hr = S_OK;
            }
            HMLOG("flush: insert hr=0x%lX\n", (unsigned long)hr);
            return S_OK;
        }

      private:
        LONG ref_;
        ITfContext *ctx_;
        CTextService *svc_;
        TfClientId cid_;
        const wchar_t *t_;
    };

    /* The insert half of the composition boundary: the whole buffered word
     * reaches the document in one edit session, exactly once, and the
     * buffer empties. Shared by the declined-key drain (below) and the
     * candidate commit (the chosen Hanja replaces the whole buffered word —
     * the Hangul it anchors to is never inserted). */
    void InsertBufferedWord(ITfContext *pic) {
        if (!wordbuf_.empty() && pic) {
            FlushSession *fs = new FlushSession(pic, this, clientId_, wordbuf_.c_str());
            HRESULT hrFlush = S_OK;
            pic->RequestEditSession(clientId_, fs, TF_ES_SYNC | TF_ES_READWRITE, &hrFlush);
            fs->Release();
            wordbuf_.clear();
        }
    }

    /* One composition-boundary drain shared by the declined key path and the
     * mode toggle (issue #66): the in-flight syllable is flushed into the
     * buffered word, and the whole word is inserted exactly once before the
     * boundary key (or mode switch) proceeds. */
    void BoundaryFlush(ITfContext *pic, FILE *dbg) {
        const char *flushed = ic_ ? hunmin_ic_flush(ic_) : "";
        if (flushed && flushed[0]) {
            int flen = MultiByteToWideChar(CP_UTF8, 0, flushed, -1, nullptr, 0);
            if (flen > 1) {
                size_t old = wordbuf_.size();
                /* flen counts the NUL terminator; text only. */
                wordbuf_.resize(old + flen - 1);
                MultiByteToWideChar(CP_UTF8, 0, flushed, -1, &wordbuf_[old], flen);
            }
        }
        if (dbg) {
            fprintf(dbg, " boundary, word len=%zu preedit=%s\n", wordbuf_.size(),
                    ic_ ? hunmin_ic_preedit(ic_) : "");
            fflush(dbg);
        }
        InsertBufferedWord(pic);
        UpdateOverlay(pic);
    }

    /* --- F9 Hanja candidate window (issue #67) ---
     *
     * The engine serves the list (hunmin_suggest_count/text/detail/kind over
     * the hanja/idiom/krdict tables, keyed by the WHOLE Hangul word); the
     * adapter supplies everything else: the lookup text is the buffered
     * word, the keyboard flow mirrors fcitx5 (digits pick, Up/Down moves,
     * PageUp/PageDown pages, Return commits the cursor, Esc closes, any
     * other key closes and falls through so typing continues) and the
     * pop-up itself draws the rows.
     *
     * Page cap: kMaxCandidatePageSize, the value fcitx5's label ceiling
     * forces (ruled by the maintainer for issue #45 — engine.cpp:471-486:
     * 10 candidates survive, 11 abort). The TSF popup owns its own drawing
     * rather than fcitx5's label table, so it could show more rows than
     * that, but the SAME ruled number is kept: a word candidate list the
     * digits can reach is 1..0 per page, and 67-row popups measured ~2000px
     * tall on fcitx5's host. PageUp/PageDown are what the rest of the list
     * is behind. */
    static const int kMaxCandidatePageSize = 10;

    /* The Hangul the candidate window anchors to: accumulated finalised
     * syllables plus the in-flight preedit. Valid only while the window is
     * open — opening may not flush anything (fcitx5's openCandidates rule:
     * hunmin_ic_flush would commit the anchor out from under the list). */
    std::string BufferedHangul() {
        std::string s = HunminWideToUtf8(wordbuf_);
        if (ic_) {
            const char *pre = hunmin_ic_preedit(ic_);
            if (pre)
                s += pre;
        }
        return s;
    }

    bool OpenCandidates() {
        candSource_ = BufferedHangul();
        candCount_ = (ctx_ && !candSource_.empty())
                         ? hunmin_suggest_count(ctx_, candSource_.c_str())
                         : 0;
        candActive_ = candCount_ > 0;
        candCursor_ = 0;
        candTop_ = 0;
        return candActive_;
    }

    void CloseCandidateWindow() {
        candActive_ = false;
        candCount_ = 0;
        candCursor_ = 0;
        candTop_ = 0;
        candSource_.clear();
        candWnd_.Hide();
    }

    void PageFollowCursor() {
        candTop_ = (candCursor_ / kMaxCandidatePageSize) * kMaxCandidatePageSize;
    }

    void PageStep(int dir) {
        int lastTop = candCount_ > 0
                          ? ((candCount_ - 1) / kMaxCandidatePageSize) * kMaxCandidatePageSize
                          : 0;
        int top = candTop_ + dir * kMaxCandidatePageSize;
        if (top < 0)
            top = 0;
        if (top > lastTop)
            top = lastTop;
        candTop_ = top;
        /* The cursor moves with the page when it would be left off it. */
        if (candCursor_ < candTop_)
            candCursor_ = candTop_;
        else if (candCursor_ >= candTop_ + kMaxCandidatePageSize)
            candCursor_ = candTop_ + kMaxCandidatePageSize - 1 < candCount_
                              ? candTop_ + kMaxCandidatePageSize - 1
                              : candCount_ - 1;
    }

    /* fcitx5 semantics (engine.cpp:160-176): choosing a candidate drops the
     * composition it was anchored to, then commits the Hanja in its place —
     * no surrounding-text surgery anywhere. The buffered Hangul has not
     * been typed into the document yet, so nothing needs removing: the
     * buffer itself is replaced by the chosen text, which is then inserted
     * through the normal boundary path. */
    void CommitCandidate(ITfContext *pic, int idx) {
        if (idx < 0 || idx >= candCount_ || !ctx_)
            return;
        /* Copy first: the pointer is valid only until the next
         * hunmin_suggest_* call on ctx. */
        std::string hanja(hunmin_suggest_text(ctx_, idx));
        if (hanja.empty())
            return;
        if (ic_)
            hunmin_ic_reset(ic_); /* drop the composition the list anchored to */
        CloseCandidateWindow();
        wordbuf_ = HunminUtf8ToWide(hanja.c_str());
        InsertBufferedWord(pic);
        UpdateOverlay(pic);
    }

    void UpdateCandidateWindow(ITfContext *pic) {
        if (!candActive_ || !pic || !ic_) {
            candWnd_.Hide();
            return;
        }
        RECT rc;
        bool haveCaret = false;
        GUITHREADINFO gti;
        if (!AcquireCaretRect(pic, &rc, &haveCaret, &gti)) {
            /* No metrics, no caret: same hide path the overlay takes. The
             * candidate state stays open — a host that reports no window
             * geometry today may report it after the next focus, and the
             * next F9/arrow key redraws. */
            candWnd_.Hide();
            return;
        }
        HWND fontHost = haveCaret ? (gti.hwndFocus ? gti.hwndFocus : gti.hwndCaret) : nullptr;
        std::vector<std::wstring> lines;
        int end = candCount_ < candTop_ + kMaxCandidatePageSize
                      ? candCount_
                      : candTop_ + kMaxCandidatePageSize;
        for (int i = candTop_; i < end; ++i) {
            std::wstring text(HunminUtf8ToWide(hunmin_suggest_text(ctx_, i)));
            wchar_t label = (wchar_t)(((i - candTop_) == 9) ? '0' : ('1' + (i - candTop_)));
            std::wstring line = std::wstring(i == candCursor_ ? L">> " : L"   ") + label +
                                L". " + text;
            lines.push_back(line);
        }
        candWnd_.Show(rc, fontHost, lines);
    }

    /* The caret rect shared by the overlay and the candidate popup: one
     * read-only edit session against the selection (GetTextExt), falling
     * back to the thread's GUITHREADINFO caret. Screen coordinates out.
     * False means the host reported nothing drawable — the popup/overlay
     * hide path. */
    bool AcquireCaretRect(ITfContext *pic, RECT *rcOut, bool *haveCaretOut,
                          GUITHREADINFO *gtiOut) {
        hasPreeditRect_ = false;
        class RectSession : public ITfEditSession {
          public:
            RectSession(ITfContext *ctx, CTextService *svc) : ref_(1), ctx_(ctx), svc_(svc) {}
            STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
                if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
                    *ppv = this;
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
            STDMETHOD(DoEditSession)(TfEditCookie ec) override {
                ITfContextView *view = nullptr;
                if (SUCCEEDED(ctx_->GetActiveView(&view)) && view) {
                    TF_SELECTION sel;
                    ULONG fetched = 0;
                    if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) && fetched == 1 &&
                        sel.range) {
                        RECT rc;
                        BOOL clipped = FALSE;
                        if (SUCCEEDED(view->GetTextExt(ec, sel.range, &rc, &clipped))) {
                            svc_->preeditRect_ = rc;
                            svc_->hasPreeditRect_ = true;
                        }
                        sel.range->Release();
                    }
                    view->Release();
                }
                return S_OK;
            }

          private:
            LONG ref_;
            ITfContext *ctx_;
            CTextService *svc_;
        };
        RectSession *rs = new RectSession(pic, this);
        HRESULT hrRect = S_OK;
        pic->RequestEditSession(clientId_, rs, TF_ES_SYNC | TF_ES_READ, &hrRect);
        rs->Release();

        GUITHREADINFO gti = {};
        gti.cbSize = sizeof(gti);
        bool haveCaret = GetGUIThreadInfo(GetCurrentThreadId(), &gti) != FALSE && gti.hwndCaret &&
                         (gti.rcCaret.bottom > gti.rcCaret.top);
        *haveCaretOut = haveCaret;
        if (gtiOut)
            *gtiOut = gti;
        if (!hasPreeditRect_ && !haveCaret)
            return false;
        RECT rc = hasPreeditRect_ ? preeditRect_ : gti.rcCaret;
        if (haveCaret && gti.hwndCaret) {
            POINT topLeft = {gti.rcCaret.left, gti.rcCaret.top};
            POINT bottomRight = {gti.rcCaret.right, gti.rcCaret.bottom};
            ClientToScreen(gti.hwndCaret, &topLeft);
            ClientToScreen(gti.hwndCaret, &bottomRight);
            rc.left = topLeft.x;
            rc.top = topLeft.y;
            rc.right = bottomRight.x;
            rc.bottom = bottomRight.y;
        }
        *rcOut = rc;
        return true;
    }
};
extern "C" HINSTANCE g_hunmin_hinst;
/* placeholder */

/* Issue #71 bridge, defined in the TU that owns CTextService: settings.cpp
 * (ITfFnConfigure::Show) hands the LIVE service back through a plain void*
 * because the two TUs' headers deliberately do not entangle. */
void hunmin_tsf_apply_config_store(void *svc) {
    if (!svc)
        return;
    static_cast<CTextService *>(svc)->ApplyConfigStore();
}
extern "C" BOOL DllRegisterServer() {
    // logging
    FILE *dbg = tsf_log_open("hunmin-reg-debug.log", "w");
    wchar_t guidStr[64];
    StringFromGUID2(CLSID_HunminTSF, guidStr, 64);
    if (dbg) {
        fwprintf(dbg, L"guid=%s\n", guidStr);
        fflush(dbg);
    }

    wchar_t dllPath[MAX_PATH];
    HINSTANCE hinst = g_hunmin_hinst;
    if (!hinst)
        hinst = GetModuleHandleW(L"hunmin-tsf.dll");
    GetModuleFileNameW(hinst, dllPath, MAX_PATH);
    if (dbg) {
        fwprintf(dbg, L"module=%s\n", dllPath);
        fflush(dbg);
    }

    // COM CLSID key registration
    HKEY hKey = nullptr;
    HKEY hSub = nullptr;
    wchar_t clsidPath[128];
    swprintf(clsidPath, 128, L"CLSID\\%s", guidStr);

    LSTATUS st = RegCreateKeyExW(HKEY_CLASSES_ROOT, clsidPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                                 &hKey, nullptr);
    if (st != ERROR_SUCCESS)
        return FALSE;

    wchar_t inprocPath[256];
    swprintf(inprocPath, 256, L"%s\\InprocServer32", clsidPath);
    st = RegCreateKeyExW(HKEY_CLASSES_ROOT, inprocPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hSub,
                         nullptr);
    if (st != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    RegSetValueExW(hSub, nullptr, 0, REG_SZ, (const BYTE *)dllPath, (DWORD)((wcslen(dllPath) + 1) * sizeof(wchar_t)));
    const wchar_t *model = L"Apartment";
    RegSetValueExW(hSub, L"ThreadingModel", 0, REG_SZ, (const BYTE *)model,
                   (DWORD)((wcslen(model) + 1) * sizeof(wchar_t)));

    RegCloseKey(hSub);
    RegCloseKey(hKey);

    // TSF TIP category registration (so it is enumerated in input settings).
    HKEY hTIP = nullptr;
    wchar_t tipPath[256];
    swprintf(tipPath, 256, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s", guidStr);
    LSTATUS tipSt = RegCreateKeyExW(HKEY_LOCAL_MACHINE, tipPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE,
                                    nullptr, &hTIP, nullptr);
    if (tipSt == ERROR_SUCCESS) {
        const wchar_t *tipName = L"Hunmin";
        RegSetValueExW(hTIP, nullptr, 0, REG_SZ, (const BYTE *)tipName,
                       (DWORD)((wcslen(tipName) + 1) * sizeof(wchar_t)));
        RegCloseKey(hTIP);
    }
    CoInitialize(nullptr);
    ITfInputProcessorProfiles *pProfile = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfInputProcessorProfiles, (LPVOID *)&pProfile);
    if (dbg) {
        fwprintf(dbg, L"pProfile CoCreate hr=0x%lX\n", (unsigned long)hr);
        fflush(dbg);
    }
    if (SUCCEEDED(hr) && pProfile) {
        HRESULT regHr = pProfile->Register(CLSID_HunminTSF);
        if (dbg) {
            fwprintf(dbg, L"pProfile->Register hr=0x%lX\n", (unsigned long)regHr);
            fflush(dbg);
        }
        static const GUID profileGuid = HunminProfileGuid;
        HMODULE hSelf = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)(const void *)DllRegisterServer, &hSelf);
        wchar_t iconPath[MAX_PATH] = L"";
        if (hSelf)
            GetModuleFileNameW(hSelf, iconPath, MAX_PATH);
        HRESULT addHr =
            pProfile->AddLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid, L"Hunmin", (ULONG)wcslen(L"Hunmin"),
                                         iconPath[0] ? iconPath : nullptr, 0, TRUE);
        if (dbg) {
            fwprintf(dbg, L"pProfile->AddLanguageProfile hr=0x%lX\n", (unsigned long)addHr);
            fflush(dbg);
        }
        HRESULT enableHr = pProfile->EnableLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid, TRUE);
        if (dbg) {
            fwprintf(dbg, L"pProfile->EnableLanguageProfile hr=0x%lX\n", (unsigned long)enableHr);
            fflush(dbg);
        }
        /* Activation has to be asked for on the modern interface.
         *
         * ITfInputProcessorProfiles::ActivateLanguageProfile returns S_OK
         * while activating nothing beyond the calling process: measured on
         * this machine, GetActiveLanguageProfile then still answered S_FALSE
         * for hunmin AND for the Microsoft IME, and a fresh Notepad loaded
         * MSCTF.dll but never hunmin-tsf.dll -- no Activate(), no key sink.
         *
         * Measured for ITfInputProcessorProfileMgr::ActivateProfile: neither
         * TF_IPPMF_FORSESSION nor TF_IPPMF_FORSYSTEMALL (both with
         * TF_IPPMF_ENABLEPROFILE) persists past the registering process --
         * a fresh process still reports the Microsoft IME as active. What
         * they do record is the enablement/profile entry, so the profile is
         * selectable; selecting it (Win+Space) then loads and activates the
         * TIP in the focused process, measured end to end. The CTF default
         * assembly (HKCU ...CTF SortOrder index 0 / Assemblies Default) is
         * Windows Settings' store and is deliberately not written here.
         */
        ITfInputProcessorProfileMgr *pProfileMgr = nullptr;
        HRESULT mgrHr = pProfile->QueryInterface(IID_ITfInputProcessorProfileMgr, (LPVOID *)&pProfileMgr);
        if (dbg) {
            fwprintf(dbg, L"pProfile QI(ITfInputProcessorProfileMgr) hr=0x%lX\n", (unsigned long)mgrHr);
            fflush(dbg);
        }
        if (SUCCEEDED(mgrHr) && pProfileMgr) {
            HRESULT actHr =
                pProfileMgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, 0x0412, CLSID_HunminTSF, profileGuid,
                                             nullptr, TF_IPPMF_FORSYSTEMALL | TF_IPPMF_ENABLEPROFILE);
            if (dbg) {
                fwprintf(dbg, L"pProfileMgr->ActivateProfile hr=0x%lX\n", (unsigned long)actHr);
                fflush(dbg);
            }
            TF_INPUTPROCESSORPROFILE active;
            ZeroMemory(&active, sizeof(active));
            HRESULT getHr = pProfileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &active);
            if (dbg) {
                fwprintf(dbg, L"pProfileMgr->GetActiveProfile hr=0x%lX active.clsid={%08lX} active.profile=%08lX\n",
                         (unsigned long)getHr, (unsigned long)active.clsid.Data1,
                         (unsigned long)active.guidProfile.Data1);
                fflush(dbg);
            }
            if (FAILED(getHr) || !IsEqualGUID(active.clsid, CLSID_HunminTSF)) {
                /* Pre-Vista hosts, or a profile the session refused: the
                 * legacy call is the only thing left to try. */
                HRESULT legacyHr = pProfile->ActivateLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid);
                if (dbg) {
                    fwprintf(dbg, L"fallback ActivateLanguageProfile hr=0x%lX\n", (unsigned long)legacyHr);
                    fflush(dbg);
                }
            }
            pProfileMgr->Release();
        } else {
            HRESULT legacyHr = pProfile->ActivateLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid);
            if (dbg) {
                fwprintf(dbg, L"ActivateLanguageProfile (no ProfileMgr) hr=0x%lX\n", (unsigned long)legacyHr);
                fflush(dbg);
            }
        }
        pProfile->Release();
    }

    ITfCategoryMgr *pCatMgr = nullptr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (LPVOID *)&pCatMgr);
    if (dbg) {
        fwprintf(dbg, L"pCatMgr CoCreate hr=0x%lX\n", (unsigned long)hr);
        fflush(dbg);
    }
    if (SUCCEEDED(hr) && pCatMgr) {
        HRESULT c1 = pCatMgr->RegisterCategory(CLSID_HunminTSF, GUID_TFCAT_TIP_KEYBOARD, CLSID_HunminTSF);
        HRESULT c2 = pCatMgr->RegisterCategory(CLSID_HunminTSF, GUID_TFCAT_CATEGORY_OF_TIP, CLSID_HunminTSF);
        if (dbg) {
            fwprintf(dbg, L"RegisterCategory keyboard=0x%lX category=0x%lX\n", (unsigned long)c1, (unsigned long)c2);
            fflush(dbg);
        }
        pCatMgr->Release();
    }
    CoUninitialize();
    return TRUE;
}

extern "C" BOOL DllUnregisterServer() {
    /* Mirror of DllRegisterServer, applied in reverse order (issue #84).
     * First the profile and category state the CTF COM services hold, then
     * the HKLM TIP tree under which AddLanguageProfile created subkeys, then
     * the COM CLSID tree. Windows Settings' own per-user CTF store (HKCU
     * SortOrder / Assemblies) is deliberately NOT touched: registration
     * never writes it (measured, docs/WINDOWS-TSF-REGISTRATION.md), so
     * unregistering must not either. Logging follows the register flow: only
     * tsf_log_open-mediated writes, to hunmin-reg-debug.log. */
    FILE *dbg = tsf_log_open("hunmin-reg-debug.log", "a");
    wchar_t guidStr[64];
    StringFromGUID2(CLSID_HunminTSF, guidStr, 64);
    if (dbg) {
        fwprintf(dbg, L"--- unregister begin guid=%s\n", guidStr);
        fflush(dbg);
    }

    CoInitialize(nullptr);
    ITfInputProcessorProfiles *pProfile = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfInputProcessorProfiles, (LPVOID *)&pProfile);
    if (dbg) {
        fwprintf(dbg, L"pProfile CoCreate hr=0x%lX\n", (unsigned long)hr);
        fflush(dbg);
    }
    if (SUCCEEDED(hr) && pProfile) {
        /* Reverse of the register order: the per-profile enablement entry the
         * ProfileMgr interface owns goes first (mirror of EnableLanguage-
         * Profile / ProfileMgr), then Unregister() for Register(). */
        ITfInputProcessorProfileMgr *pProfileMgr = nullptr;
        HRESULT mgrHr = pProfile->QueryInterface(IID_ITfInputProcessorProfileMgr, (LPVOID *)&pProfileMgr);
        if (dbg) {
            fwprintf(dbg, L"pProfile QI(ITfInputProcessorProfileMgr) hr=0x%lX\n", (unsigned long)mgrHr);
            fflush(dbg);
        }
        if (SUCCEEDED(mgrHr) && pProfileMgr) {
            HRESULT upHr = pProfileMgr->UnregisterProfile(CLSID_HunminTSF, 0x0412, HunminProfileGuid, 0);
            if (dbg) {
                fwprintf(dbg, L"pProfileMgr->UnregisterProfile hr=0x%lX\n", (unsigned long)upHr);
                fflush(dbg);
            }
            pProfileMgr->Release();
        }
        HRESULT unregHr = pProfile->Unregister(CLSID_HunminTSF);
        if (dbg) {
            fwprintf(dbg, L"pProfile->Unregister hr=0x%lX\n", (unsigned long)unregHr);
            fflush(dbg);
        }
        pProfile->Release();
    }

    ITfCategoryMgr *pCatMgr = nullptr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (LPVOID *)&pCatMgr);
    if (dbg) {
        fwprintf(dbg, L"pCatMgr CoCreate hr=0x%lX\n", (unsigned long)hr);
        fflush(dbg);
    }
    if (SUCCEEDED(hr) && pCatMgr) {
        HRESULT c1 = pCatMgr->UnregisterCategory(CLSID_HunminTSF, GUID_TFCAT_TIP_KEYBOARD, CLSID_HunminTSF);
        HRESULT c2 = pCatMgr->UnregisterCategory(CLSID_HunminTSF, GUID_TFCAT_CATEGORY_OF_TIP, CLSID_HunminTSF);
        if (dbg) {
            fwprintf(dbg, L"UnregisterCategory keyboard=0x%lX category=0x%lX\n", (unsigned long)c1, (unsigned long)c2);
            fflush(dbg);
        }
        pCatMgr->Release();
    }
    CoUninitialize();

    /* HKLM TIP tree: deleted AFTER the COM calls, because the registering
     * side created LanguageProfile\0x00000412\{profile} subkeys under it
     * through AddLanguageProfile. */
    wchar_t tipPath[256];
    swprintf(tipPath, 256, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s", guidStr);
    LSTATUS tipSt = RegDeleteTreeW(HKEY_LOCAL_MACHINE, tipPath);
    if (dbg) {
        fwprintf(dbg, L"RegDeleteTreeW(HKLM %s) st=0x%lX\n", tipPath, (unsigned long)tipSt);
        fflush(dbg);
    }

    /* COM CLSID tree -- the only deletion the previous implementation did. */
    wchar_t clsidPath[128];
    swprintf(clsidPath, 128, L"CLSID\\%s", guidStr);
    LSTATUS clsidSt = RegDeleteTreeW(HKEY_CLASSES_ROOT, clsidPath);
    if (dbg) {
        fwprintf(dbg, L"RegDeleteTreeW(HKCR %s) st=0x%lX\n", clsidPath, (unsigned long)clsidSt);
        fflush(dbg);
        fclose(dbg);
    }
    return TRUE;
}
extern "C" const CLSID CLSID_HunminTSF_Export = {
    0xF2C70001, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}};

class CClassFactory : public IClassFactory {
  public:
    CClassFactory() : ref_(1) {}
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory *>(this);
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
        if (n == 0)
            delete this;
        return n;
    }
    STDMETHOD(CreateInstance)(IUnknown *pOuter, REFIID riid, void **ppv) override {
        if (pOuter)
            return CLASS_E_NOAGGREGATION;
        CTextService *obj = new CTextService();
        HRESULT hr = obj->QueryInterface(riid, ppv);
        obj->Release();
        return hr;
    }
    STDMETHOD(LockServer)(BOOL lock) override {
        (void)lock;
        return S_OK;
    }

  private:
    LONG ref_;
};

extern "C" HRESULT DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
    FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
    wchar_t str[64];
    StringFromGUID2(rclsid, str, 64);
    if (dbg) {
        fwprintf(dbg, L"DllGetClassObject rclsid=%s\n", str);
        fflush(dbg);
    }
    if (!IsEqualCLSID(rclsid, CLSID_HunminTSF)) {
        if (dbg) {
            fwprintf(dbg, L"  not registered CLSID\n");
            fclose(dbg);
        }
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    CClassFactory *factory = new CClassFactory();
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    if (dbg) {
        fwprintf(dbg, L"  hr=0x%lX\n", (unsigned long)hr);
        fclose(dbg);
    }
    return hr;
}
