/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The TSF shell: the frontend's COM exports, the CTextService
 * implementations, and the free functions the settings TU shares
 * (hunmin_ctx_from_module, hunmin_tsf_resolve_layout_slot, the settings-exe
 * resolver). The component seams live in tsf.h; the implementations
 * register from delivery.cpp (both delivery modes) and come in through
 * hunmin_tsf_hanja_candidates_new (the Hanja flow, candidates.cpp).
 */
#include "tsf.h"

#include "candidates.h"
#include "delivery.h"
#include "displayattr.h" /* the preedit styling channel (S7) */
#include "langbar.h" /* the tear-off language-bar item (S6) */
#include "settings.h" /* ITfFunctionProvider branch + the apply bridge */
#include "utf.h"

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <msctf.h>
#include <richedit.h>
#include <shlguid.h>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <shobjidl_core.h>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#pragma comment(lib, "Shlwapi.lib")

/* Debug logs land next to the DLL (build-msvc/Release inside the checkout),
 * not at a drive root: the registered InprocServer32 path is absolute, so the
 * DLL's own directory is the one clone-independent location every host
 * process can resolve, and it stays inside the workspace. Set
 * HUNMIN_TSF_LOG_DIR to override when that directory is read-only. */

/* Issue #66: ONE data source wins - the first present in priority order:
 * (1) HUNMIN_DATA_DIR (explicit override), (2) <module>\data (the shipped
 * shape; the build stages libhunmin/data beside the DLL), (3) the repo
 * checkout climb (dev convenience, reached only when neither holds). The
 * earlier additive chain held the same tree twice once staging existed:
 * hunmin_ctx_layout_count() reported 30 for 15 staged files, because
 * hunmin_ctx_new's search path is additive by design - duplicate ids
 * across dirs are not deduped, so overlapping dirs are the caller's to
 * avoid. Measured 2026-10-17. Non-static (issue #71): the settings dialog
 * (settingsdialog.cpp) resolves data through this same function, so the
 * dialog's layout lists can never disagree with the adapter's about where
 * the data came from. */
hunmin_ctx *hunmin_ctx_from_module() {
    wchar_t envW[MAX_PATH];
    wchar_t modDir[MAX_PATH];
    char dirs[2][MAX_PATH * 3];
    int argc = 0;

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
        hunmin_logger.log("env data dir: %ls\n", envW);
    } else if (hMod) {
        PathRemoveFileSpecW(modDir);
        wchar_t candMod[MAX_PATH];
        PathCombineW(candMod, modDir, L"data");
        DWORD attrMod = GetFileAttributesW(candMod);
        hunmin_logger.log("candMod=%ls dir=%d\n", candMod,
                          (attrMod != INVALID_FILE_ATTRIBUTES) &&
                              ((attrMod & FILE_ATTRIBUTE_DIRECTORY) ==
                               FILE_ATTRIBUTE_DIRECTORY));
        if (attrMod != INVALID_FILE_ATTRIBUTES &&
            (attrMod & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
            /* Shipped shape: data beside the module. Do not also climb to the
             * repo - one tree in the search path, or every layout loads twice. */
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
            hunmin_logger.log("climb=%ls dir=%d\n", cand,
                              (attrRepo != INVALID_FILE_ATTRIBUTES) &&
                                  ((attrRepo & FILE_ATTRIBUTE_DIRECTORY) ==
                                   FILE_ATTRIBUTE_DIRECTORY));
            if (attrRepo != INVALID_FILE_ATTRIBUTES &&
                (attrRepo & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
                WideCharToMultiByte(CP_UTF8, 0, cand, -1, dirs[argc], sizeof(dirs[argc]), nullptr, nullptr);
                ++argc;
            }
        }
    }
    hunmin_logger.log("argc=%d hMod=%p\n", argc, (void *)hMod);

    if (argc == 0)
        return nullptr;
    const char *argv2[2] = {dirs[0], dirs[1]};
    return hunmin_ctx_new(argv2, argc);
}

void hunmin_tsf_resolve_layout_slot(const hunmin_ctx *ctx,
                                    const std::string &stored,
                                    const char *def, bool wantHangul,
                                    std::string *outId) {
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
            hunmin_logger.log(
                "  config %s slot: id=%s mode=%s supported=%d -> rejected\n",
                wantHangul ? "hangul" : "latin", id, mode ? mode : "?",
                (int)hunmin_ctx_layout_supported(ctx, i));
            break; /* found the id, wrong slot/build: fall to default */
        }
    }
    if (ctx)
        *outId = wantHangul ? kHangulDefaultId : kLatinDefaultId;
    if (*outId != candidate)
        hunmin_logger.log("  config %s slot: %s -> default %s\n",
                          wantHangul ? "hangul" : "latin", candidate,
                          outId->c_str());
}

/* Launch the settings surface: the WinUI 3 exe (the ruled surface, assets
 * icon) when it is deployed beside this DLL, the rundll32 entry to
 * HunminSettingsShow otherwise. A dedicated process: the TIP thread must
 * not pump a nested modal loop while TSF holds edit cookies. */
HRESULT hunmin_tsf_launch_settings(void *anchor) {
    HMODULE self = nullptr;
    wchar_t dll[MAX_PATH] = {0};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)anchor, &self) ||
        !GetModuleFileNameW(self, dll, MAX_PATH))
        return E_FAIL;
    wchar_t dllDir[MAX_PATH];
    lstrcpynW(dllDir, dll, MAX_PATH);
    PathRemoveFileSpecW(dllDir);
    wchar_t exe[MAX_PATH];
    exe[0] = 0;
    /* The settings exe's known layouts, first hit wins:
     * 1. <dllDir>\settings-ui\publish\ - the dev-tree publish shape
     *    (frontends/tsf/settings-ui/build-settings-ui.bat);
     * 2. <dllDir>\..\..\settings-ui\publish\ - the dev-checkout climb (DLL
     *    in frontends/tsf/build-msvc/Release);
     * 3. <dllDir>\settings-ui\ - the INSTALLED shape: the .iss stages the
     *    publish CONTENT directly under {app}\settings-ui (measured in the
     *    installer sandbox round: only the Start-menu shortcut pointed
     *    there, the launcher resolved neither publish shape and fell back
     *    to the rundll32 interim dialog). */
    static const wchar_t *const kSettingsExeCandidates[] = {
        L"%s\\settings-ui\\publish\\hunmin-tsf-settingsui.exe",
        L"%s\\..\\..\\settings-ui\\publish\\hunmin-tsf-settingsui.exe",
        L"%s\\settings-ui\\hunmin-tsf-settingsui.exe",
    };
    for (const wchar_t *fmt : kSettingsExeCandidates) {
        wchar_t cand[2 * MAX_PATH];
        wchar_t full[MAX_PATH];
        _snwprintf(cand, _countof(cand), fmt, dllDir);
        if (GetFullPathNameW(cand, MAX_PATH, full, nullptr) && PathFileExistsW(full)) {
            lstrcpynW(exe, full, MAX_PATH);
            break;
        }
    }
    wchar_t cmd[MAX_PATH + 64];
    SHELLEXECUTEINFOW sei = {sizeof sei};
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.nShow = SW_SHOWNORMAL;
    if (exe[0]) {
        sei.lpFile = exe;
        sei.lpParameters = nullptr;
    } else {
        _snwprintf(cmd, _countof(cmd), L"\"%s,HunminSettingsShow\"", dll);
        sei.lpFile = L"rundll32.exe";
        sei.lpParameters = cmd;
    }
    BOOL ok = ShellExecuteExW(&sei);
    hunmin_logger.log("launch settings exe=%ls hr=%ld\n", exe, (long)GetLastError());
    return ok ? S_OK : E_FAIL;
}

CTextService::CTextService()
    : ref_(1), threadMgr_(nullptr), clientId_(TF_CLIENTID_NULL), sinkCookie_(0),
      ctx_(nullptr), ic_(nullptr), hangulMode_(true), hasPreeditRect_(false),
      lastCtx_(nullptr), composition_(nullptr), compCtx_(nullptr),
      displayAttr_(new DisplayAttr()), wordbuf_(), interimForce_(false), legacyMode_(false),
      endingSelf_(false), hangulId_(kHangulDefaultId),
      latinId_(kLatinDefaultId),
      hanjaCombo_(HunminKeyCombo{HUNMIN_TSF_VIRTUAL_HANJA, 0}) {
    /* Each component registers its own implementations; the shell
     * composes and owns them. A missing registration must be loud, not a
     * null-deref: die here rather than ship a TIP whose next key derefs
     * a null seam. */
    hunmin_tsf_deliveries_register(*this);
    if (deliveries_.empty() || !UseDelivery(HUNMIN_TSF_DELIVERY_NATIVE)) {
        hunmin_logger.log("FATAL: no delivery registered\n");
        abort();
    }
    cand_.reset(hunmin_tsf_hanja_candidates_new(*this));
    if (!cand_) {
        hunmin_logger.log("FATAL: candidate flow not built\n");
        abort();
    }
    langbar_ = hunmin_tsf_langbar_new(*this, CLSID_HunminTSF);
    if (!langbar_) {
        hunmin_logger.log("FATAL: language-bar item not built\n");
        abort();
    }
}

CTextService::~CTextService() {
    if (langbar_) {
        langbar_->OnRemoved();
        langbar_->Release();
        langbar_ = nullptr;
    }
    if (composition_)
        composition_->Release();
    if (compCtx_) {
        compCtx_->Release();
        compCtx_ = nullptr;
    }
    if (ic_)
        hunmin_ic_free(ic_);
    if (ctx_)
        hunmin_ctx_free(ctx_);
    if (lastCtx_)
        lastCtx_->Release();
}

STDMETHODIMP CTextService::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv)
        return E_INVALIDARG;
    /* The QI trace (the maintainer's request, the hosted-item
     * diagnosis): one line per QI, the asked IID's lead field and
     * the answered slot mask - the langbar's hosting decision is
     * where the item's shape gets judged, so the log names which
     * IID rounds the object actually gets asked for. */
    {
        hunmin_logger.log("QI riid=%08lX-%04X answered=",
                          (unsigned long)riid.Data1, (unsigned)riid.Data2);
        if (IsEqualIID(riid, IID_IUnknown) ||
            IsEqualIID(riid, IID_ITfTextInputProcessor))
            hunmin_logger.log("tip");
        else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
            hunmin_logger.log("tmesink");
        else if (IsEqualIID(riid, IID_ITfKeyEventSink))
            hunmin_logger.log("keysink");
        else if (IsEqualIID(riid, IID_ITfCompositionSink))
            hunmin_logger.log("compsink");
        else if (IsEqualIID(riid, IID_ITfFunctionProvider))
            hunmin_logger.log("fnprov");
        else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider))
            hunmin_logger.log("dispattr");
        else if (IsEqualIID(riid, IID_ITfLangBarItem))
            hunmin_logger.log("lbi");
        else if (IsEqualIID(riid, IID_ITfLangBarItemButton))
            hunmin_logger.log("lblbtn");
        else if (IsEqualIID(riid, IID_ITfSource))
            hunmin_logger.log("source");
        else
            hunmin_logger.log("none");
        hunmin_logger.log(" pid=%lu\n", (unsigned long)GetCurrentProcessId());
    }
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
         * configure function (settingsprovider.cpp). */
        *ppv = hunmin_tsf_settings_new_provider(this);
        return S_OK;
    } else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
        /* Issue #98: the display-attribute provider the app-side
         * attribute manager looks for through the
         * GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER category. */
        *ppv = static_cast<ITfDisplayAttributeProvider *>(this);
    } else if (IsEqualIID(riid, IID_ITfLangBarItem) ||
               IsEqualIID(riid, IID_ITfLangBarItemButton) ||
               IsEqualIID(riid, IID_ITfSource)) {
        /* S6: the language bar speaks to the TEAR-OFF item, not to the
         * text service. Its own QI answers IUnknown, ITfLangBarItem,
         * ITfLangBarItemButton and ITfSource, so the bar asks the item
         * (not this object) to AdviseSink its ITfLangBarItemSink. */
        if (!langbar_) {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        return langbar_->QueryInterface(riid, ppv);
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG)
CTextService::AddRef() { return (ULONG)InterlockedIncrement(&ref_); }
STDMETHODIMP_(ULONG)
CTextService::Release() {
    ULONG r = (ULONG)InterlockedDecrement(&ref_);
    if (r == 0)
        delete this;
    return r;
}

/* The mode-toggle set (Android KeyBinding semantics): configured chords
 * ADD, and the bare physical Han/Eng key is appended when the store does not
 * carry it - it must never go dead. */
void CTextService::LoadToggleKeys(const HunminTsfConfig &cfg) {
    HunminKeyCombo parsed[16];
    const int nParsed = hunmin_tsf_keybindings_parse(cfg.toggleKeys, parsed, 16);
    toggleKeys_.assign(parsed, parsed + nParsed);
    bool hangeulBare = false;
    for (const HunminKeyCombo &c : toggleKeys_)
        if (c.vk == HUNMIN_TSF_VIRTUAL_HANGEUL && c.mods == 0)
            hangeulBare = true;
    if (!hangeulBare)
        toggleKeys_.push_back(HunminKeyCombo{HUNMIN_TSF_VIRTUAL_HANGEUL, 0});
    hunmin_logger.log("  toggle keys=%zu (configured bits: %s)\n", toggleKeys_.size(),
                      cfg.toggleKeys[0] ? cfg.toggleKeys : "(none)");
}

/* The candidate-window key (issue #67): ONE binding from the store, default
 * the bare physical Hanja key. An unparseable binding falls back to the
 * default rather than matching nothing - the candidate window must stay
 * reachable, and a parse failure must never swallow a keystroke. */
void CTextService::LoadHanjaKey(const HunminTsfConfig &cfg) {
    HunminKeyCombo parsed[2];
    const int nParsed = hunmin_tsf_keybindings_parse(cfg.hanjaKey, parsed, 2);
    if (nParsed > 0) {
        hanjaCombo_ = parsed[0];
    } else {
        hanjaCombo_ = HunminKeyCombo{HUNMIN_TSF_VIRTUAL_HANJA, 0};
        hunmin_logger.log("  hanja key unparseable (%s) -> default VK_HANJA\n",
                          cfg.hanjaKey[0] ? cfg.hanjaKey : "(empty)");
    }
}

/* The engine instance for the CURRENT mode: the config boundary and the mode
 * toggle both swap it, so the free+new pair has one home. */
void CTextService::RebuildEngineIc() {
    if (ic_) {
        hunmin_ic_free(ic_);
        ic_ = nullptr;
    }
    if (ctx_)
        ic_ = hunmin_ic_new(ctx_, hangulMode_ ? hangulId_.c_str() : latinId_.c_str());
}

void CTextService::ApplyConfigStore() {
    hunmin_logger.log("ApplyConfigStore pid=%lu\n", (unsigned long)GetCurrentProcessId());
    HunminTsfConfig cfg;
    hunmin_tsf_config_load(HKEY_CURRENT_USER, &cfg);
    hunmin_tsf_resolve_layout_slot(ctx_, cfg.hangulLayout, kHangulDefaultId, true, &hangulId_);
    hunmin_tsf_resolve_layout_slot(ctx_, cfg.latinLayout, kLatinDefaultId, false, &latinId_);
    LoadToggleKeys(cfg);
    LoadHanjaKey(cfg);
    /* The candidate window belongs to a composition the ic swap below
     * drops: close it with Apply, like the mode toggle does (issue #67). */
    cand_->Close();
    RebuildEngineIc();
    hunmin_logger.log("  applied hangul=%s latin=%s ic=%p\n",
                      hangulId_.c_str(), latinId_.c_str(), (void *)ic_);
}

void CTextService::UpdateLangBarItem() {
    if (langbar_)
        langbar_->Update();
}

void CTextService::ToggleModeCore(ITfContext *pic) {
    cand_->Close();
    BoundaryFlush(pic);
    hangulMode_ = !hangulMode_;
    UpdateConversionCompartment();
    UpdateLangBarItem();
    RebuildEngineIc();
    /* This gated line is the one evidence a mode boundary ran in this
     * host at all: the key path's own log line lives in its handler. */
    hunmin_logger.log("toggle core -> mode=%s ic=%p pic=%p\r\n",
                      hangulMode_ ? "hangul" : "english", (void *)ic_, (void *)pic);
}

/* The two TSF advice registrations Activate needs: the thread-manager event
 * sink and the key-event sink. Both belong to the live session; Deactivate's
 * UnadviseSinks() undoes the key half. */
void CTextService::AdviseSinks() {
    if (!threadMgr_)
        return;
    ITfKeystrokeMgr *pks = nullptr;
    hunmin_logger.log("Advise threadmgr attempt\n");
    ITfSource *src = nullptr;
    HRESULT srch = threadMgr_->QueryInterface(IID_ITfSource, (void **)&src);
    HRESULT tmHr = E_NOINTERFACE;
    if (SUCCEEDED(srch) && src) {
        tmHr = src->AdviseSink(IID_ITfThreadMgrEventSink,
                               static_cast<ITfThreadMgrEventSink *>(this), &sinkCookie_);
        src->Release();
    }
    hunmin_logger.log("  source hr=0x%lX threadmgr advise hr=0x%lX cookie=%lu\n",
                      (unsigned long)srch, (unsigned long)tmHr,
                      (unsigned long)sinkCookie_);
    HRESULT qh = threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pks);
    hunmin_logger.log("Advise QI hr=0x%lX pks=%p\n", (unsigned long)qh, (void *)pks);
    if (SUCCEEDED(qh) && pks) {
        HRESULT ah = pks->AdviseKeyEventSink(clientId_, this, TRUE);
        hunmin_logger.log("AdviseKeyEventSink hr=0x%lX\n", (unsigned long)ah);
        pks->Release();
    }
}

void CTextService::UnadviseSinks() {
    if (!threadMgr_)
        return;
    ITfKeystrokeMgr *pks = nullptr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pks)) && pks) {
        pks->UnadviseKeyEventSink(clientId_);
        pks->Release();
    }
    threadMgr_->Release();
    threadMgr_ = nullptr;
}

/* Issue #119: the legacy path is DETECTED at runtime (a mid-word host
 * termination flips the service); HUNMIN_TSF_INTERIM=1 only FORCES every
 * context onto it up front (debug/probe escape hatch). */
void CTextService::DetectLegacyMode() {
    char v[8] = {0};
    interimForce_ = GetEnvironmentVariableA("HUNMIN_TSF_INTERIM", v, sizeof v) &&
                    (v[0] == '1' || _stricmp(v, "on") == 0 || _stricmp(v, "true") == 0);
    legacyMode_ = interimForce_;
    UseDelivery(legacyMode_ ? HUNMIN_TSF_DELIVERY_INTERIM : HUNMIN_TSF_DELIVERY_NATIVE);
    hunmin_logger.log("legacy force=%d detected-mode=%d\n", interimForce_ ? 1 : 0,
                      legacyMode_ ? 1 : 0);
}

STDMETHODIMP CTextService::Activate(ITfThreadMgr *ptm, TfClientId tid) {
    threadMgr_ = ptm;
    clientId_ = tid;
    if (ptm)
        ptm->AddRef();
    hunmin_logger.log("Activate pid=%lu tid=%u\n", (unsigned long)GetCurrentProcessId(), (unsigned)tid);
    AdviseSinks();
    ctx_ = hunmin_ctx_from_module();
    hunmin_logger.log("  ctx_=%p\n", (void *)ctx_);

    /* Layout wiring (issue #66), now via ApplyConfigStore() (issue #71):
     * the same re-read + rebuild the settings dialog's Apply drives on
     * a LIVE service, without a process restart - acceptance is
     * identical because the config boundary resets exactly the state
     * Activate() had to do anyway; no separate activation-only copy
     * exists. Activate() starts hangulMode_=true. */
    ApplyConfigStore();
    DetectLegacyMode();
    /* THE language-bar item (issue #103, the ruled langbar surface): the
     * mode indicator (icon 한/A, text) AND the right-click config menu
     * ride our own ITfLangBarItemButton, added while the service is
     * active and removed in Deactivate (docs: language-bar.md, "Adding
     * Items to the Language Bar"). */
    langbar_->Attach(ptm);
    UpdateConversionCompartment();
    /* The item's first notification: the sink is only set once the
     * language bar advises it (the item's ITfSource::AdviseSink), so an
     * AddItem-less host (or the CTF kernel's NULL sink) simply sees a
     * badge re-read at the next mode boundary. */
    UpdateLangBarItem();
    if (ctx_)
        hunmin_logger.log("  layout_count=%d\n", hunmin_ctx_layout_count(ctx_));
    return S_OK;
}

STDMETHODIMP CTextService::Deactivate() {
    UnadviseSinks();
    /* A live composition (issue #98) is finalised in its own context
     * first: the composed word stays there as committed text and the
     * buffers drain, so the plain-insert path below only handles what
     * is left when no composition could ever be started. */
    DetachComposition();
    if (lastCtx_)
        FlushEngineIntoWordbuf();
    if (!wordbuf_.empty() && lastCtx_)
        InsertBufferedWord(lastCtx_);
    wordbuf_.clear();
    cand_->Close();
    /* Remove the language-bar item. NOTE (recorded defect, not changed
     * here): UnadviseSinks() above has already nulled threadMgr_, so this
     * call is a no-op today - the same ordering the block had inline. */
    langbar_->Detach(threadMgr_);
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

STDMETHODIMP CTextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo **ppEnum) {
    if (!ppEnum)
        return E_INVALIDARG;
    *ppEnum = HunminDisplayAttrEnumNew();
    return S_OK;
}

STDMETHODIMP CTextService::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo **ppInfo) {
    if (!ppInfo)
        return E_INVALIDARG;
    *ppInfo = HunminDisplayAttrNewForGUID(guid);
    return *ppInfo ? S_OK : E_FAIL;
}

STDMETHODIMP CTextService::OnInitDocumentMgr(ITfDocumentMgr *pdm) {
    (void)pdm;
    return S_OK;
}

STDMETHODIMP CTextService::OnUninitDocumentMgr(ITfDocumentMgr *pdm) {
    (void)pdm;
    return S_OK;
}

STDMETHODIMP CTextService::OnSetFocus(ITfDocumentMgr *pdmFocus, ITfDocumentMgr *pdmPrevFocus) {
    hunmin_logger.log("TM-OnSetFocus pdmFocus=%p pdmPrev=%p modePid=%lu\n",
                      (void *)pdmFocus, (void *)pdmPrevFocus,
                      (unsigned long)GetCurrentProcessId());
    if (pdmFocus) {
        /* The view probe the trace reads; run regardless of the gate so
         * the pic half (its Release) can never leak behind an if. */
        ITfContext *pic = nullptr;
        HRESULT hrTop = pdmFocus->GetTop(&pic);
        hunmin_logger.log("  GetTop hr=0x%lX pic=%p\n", (unsigned long)hrTop,
                          (void *)pic);
        if (pic)
            pic->Release();
    }
    (void)pdmFocus;
    (void)pdmPrevFocus;
    return S_OK;
}

STDMETHODIMP CTextService::OnPushContext(ITfContext *pic) {
    (void)pic;
    return S_OK;
}

STDMETHODIMP CTextService::OnPopContext(ITfContext *pic) {
    (void)pic;
    return S_OK;
}

STDMETHODIMP CTextService::OnSetFocus(BOOL fFocus) {
    hunmin_logger.log("KS-OnSetFocus fFocus=%d modePid=%lu\n", (int)fFocus,
                      (unsigned long)GetCurrentProcessId());
    (void)fFocus;
    return S_OK;
}

STDMETHODIMP CTextService::OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) {
    (void)pic;
    (void)lParam;
    /* The previous key's deferred doc write runs before this key is
     * classified - a keystroke storm must not reorder the doc writes
     * behind the engine state. */
    FlushPendingDocWrites();
    /* Shared predicate with OnKeyDown (the registration-work rule: test
     * and down are one decision). The configured set always contains the
     * bare Han/Eng key, so the physical key stays interested in both
     * modes. CapsLock is NOT here any more: it is no longer the mode
     * toggle (maintainer instruction: the Han/Eng key is the default),
     * and in Korean input mode it is ignored outright (the engine's
     * case-encoding convention). */
    BOOL eat = FALSE;
    if (IsModeToggle(wParam))
        eat = TRUE;
    else if (cand_->Active() && cand_->EatsListKey((unsigned long)wParam))
        /* While the candidate window is open its keys are ours, on the
         * same shared-predicate rule the toggle uses (test and down are
         * one decision). */
        eat = TRUE;
    else if (!hangulMode_)
        eat = FALSE;
    else if (hunmin_tsf_keybindings_match(&hanjaCombo_, 1, (unsigned)wParam,
                                          hunmin_tsf_keymods_from_state())) {
        /* Mirror the down decision exactly (no look-ahead lie): the
         * window opens when the buffered word has candidates - the
         * whole-word lookup, or issue #100's last-syllable fallback
         * when the whole word yields 0 - and in the zero-and-zero case
         * the key belongs to the client. The suggest lookup is a read
         * of tables already in memory. */
        bool fallback = false;
        eat = cand_->ListCount(&fallback) > 0 ? TRUE : FALSE;
    } else if (hangulMode_ &&
               (hunmin_tsf_keymods_from_state() &
                (HUNMIN_TSF_KEYMOD_CTRL | HUNMIN_TSF_KEYMOD_ALT))) {
        /* Mirror of the down-side chord rule: a Ctrl/Alt chord belongs
         * to the app (Ctrl+A is not composition input) - test phase
         * answers the same uneaten the down phase will act on. */
        eat = FALSE;
    } else if (wParam >= 'A' && wParam <= 'Z') {
        eat = TRUE;
    } else if (wParam == VK_SPACE) {
        /* A boundary key is a key of passage (#95 contract): the app
         * inserts the space. MEASURED on msftedit: with eat=FALSE the
         * host NEVER calls OnKeyDown for the key - the test phase is
         * the TIP's only sight of it - so the boundary drain runs
         * HERE. The span bookkeeping is position-based, so the drain
         * is order-independent against the app's own space insertion,
         * and a host that DOES call the down re-runs the flush on the
         * already-empty buffer (a no-op). */
        BoundaryFlush(pic);
        eat = FALSE;
    } else if (wParam == VK_BACK) {
        /* Against in-flight text the engine consumes the key (the jamo
         * deletes). At an EMPTY in-flight the engine declines - and on
         * the interim path the key must then PASS, so the app deletes
         * the committed char (measured: eaten = the backspace does
         * nothing at all); the span bookkeeping self-heals at the next
         * write (the fresh-span anchor re-reads the live caret). The
         * native path keeps the key: its declined branch is the
         * #98-verified composition-close shape. */
        const char *pre = ic_ ? hunmin_ic_preedit(ic_) : "";
        eat = (legacyMode_ && pre[0] == '\0') ? FALSE : TRUE;
    } else if (hangulMode_ && !IsKeyOfPassage(wParam) && VkToBase(wParam) != 0 &&
               !IsModifierVk(wParam)) {
        /* EVERY feedable key is interested in hangul mode, the OEM
         * punctuation class included. Sin3-p2's jamo live on this row
         * (; ' /;  measured 2026-10-21: the test phase answer eat=FALSE
         * - and msftedit then NEVER calls OnKeyDown for an uninterested
         * key - so the layout's chosung emitted as raw symbols). The
         * DOWN phase's engine feed makes the real decision here: the
         * key composes when the engine consumes it, and when the engine
         * declines (a key this layout does not define) FeedEngineKey's
         * decline branch flushes the buffered word and lets the key pass
         * (its pfEat stays FALSE, the app inserts the symbol itself). */
        eat = TRUE;
    }
    if (pfEat)
        *pfEat = eat;
    hunmin_logger.log("OnTestKeyDown w=%lu eat=%d mode=%d\n",
                      (unsigned long)wParam, (int)eat, (int)hangulMode_);
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) {
    /* Same entry flush as the test phase: whichever callback the host
     * reaches first settles the previous key's doc write. */
    FlushPendingDocWrites();
    TrackKeyContext(pic);
    hunmin_logger.log("OnKeyDown wParam=%lu ic=%p\n", (unsigned long)wParam,
                      (void *)ic_);
    if (pfEat)
        *pfEat = FALSE;
    if (!ic_) {
        hunmin_logger.log(" ic null -> pass\n");
        return S_OK;
    }
    /* Each handler owns one key class and answers one question: is the
     * key ours (pfEat), and what state changes. The order IS the
     * precedence: the toggle, the mode passthrough, the candidate
     * window's own key, the open window's list keys, the keys of
     * passage, and finally the composition feed. */
    if (IsModeToggle(wParam)) {
        ApplyModeToggle(pic, wParam, lParam, pfEat);
        return S_OK;
    }
    if (!hangulMode_)
        return S_OK; // passthrough English
    if (IsCandidateToggle(wParam)) {
        cand_->Toggle(pic, (unsigned long)wParam, pfEat);
        return S_OK;
    }
    if (cand_->Active() && cand_->ListKey(pic, (unsigned long)wParam, pfEat))
        return S_OK;
    if (IsKeyOfPassage(wParam)) {
        DrainAndPass(pic, wParam, pfEat, "edit key");
        return S_OK;
    }
    if (wParam >= VK_F1 && wParam <= VK_F12) {
        DrainAndPass(pic, wParam, pfEat, "fkey");
        return S_OK;
    }
    FeedEngineKey(pic, wParam, pfEat);
    return S_OK;
}

void CTextService::TrackKeyContext(ITfContext *pic) {
    if (lastCtx_ == pic)
        return;
    if (lastCtx_)
        lastCtx_->Release();
    lastCtx_ = pic;
    if (lastCtx_)
        lastCtx_->AddRef();
    if (composition_ && compCtx_ && compCtx_ != pic)
        DetachComposition();
}

bool CTextService::IsModeToggle(WPARAM wParam) {
    return hunmin_tsf_keybindings_match(toggleKeys_.data(), (int)toggleKeys_.size(),
                                        (unsigned)wParam, hunmin_tsf_keymods_from_state()) != FALSE;
}

void CTextService::ApplyModeToggle(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) {
    if (pfEat)
        *pfEat = TRUE;
    const bool repeat = (lParam & (1 << 30)) != 0;
    if (!repeat)
        ToggleModeCore(pic);
    hunmin_logger.log(" toggle key vk=%lu repeat=%d -> %s (%s)\n",
                      (unsigned long)wParam, (int)repeat,
                      hangulMode_ ? "hangul" : "english",
                      hangulMode_ ? hangulId_.c_str() : latinId_.c_str());
}

bool CTextService::IsCandidateToggle(WPARAM wParam) {
    return hunmin_tsf_keybindings_match(&hanjaCombo_, 1, (unsigned)wParam,
                                        hunmin_tsf_keymods_from_state()) != FALSE;
}

bool CTextService::IsKeyOfPassage(WPARAM wParam) {
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
        return true;
    default:
        /* The function-key block (measured with the unbound-F9 case of
         * issue #67: VK_F1..VK_F12 = 0x70..0x7B falls into the
         * printable fallback below, so an unbound F-key composed its
         * VK-value as a layout character - F9(0x78) typed 'x' = ㅈ into
         * the composition the user was watching. Same defect class,
         * same keys-of-passage answer. */
        return wParam >= VK_F1 && wParam <= VK_F12;
    }
}

void CTextService::DrainAndPass(ITfContext *pic, WPARAM wParam, BOOL *pfEat,
                                const char *tag) {
    BoundaryFlush(pic);
    if (pfEat)
        *pfEat = FALSE;
    hunmin_logger.log(" %s vk=%lu -> boundary + passthrough\n", tag,
                      (unsigned long)wParam);
}

/* The VK -> base half, and only that half (issue #106): the physical
 * key's UNSHIFTED US-QWERTY character. The shift column the adapter
 * used to carry is the engine's own map (data.c BASE/SHIFTED), selected
 * in the engine by the Shift bit that travels beside the base
 * (FRONTEND-CONTRACT.md A3'). */
uint32_t CTextService::VkToBase(WPARAM wParam) {
    if (wParam >= 'A' && wParam <= 'Z')
        return (uint32_t)(wParam + 32); /* the lowercase half */
    if (wParam >= '0' && wParam <= '9')
        return (uint32_t)wParam;
    if (wParam == VK_SPACE)
        return ' ';
    if (wParam == VK_BACK)
        return 0x08;
    switch (wParam) {
    case VK_OEM_1:
        return ';';
    case VK_OEM_7:
        return '\'';
    case VK_OEM_COMMA:
        return ',';
    case VK_OEM_PERIOD:
        return '.';
    case VK_OEM_2:
        return '/';
    case VK_OEM_3:
        return '`';
    case VK_OEM_4:
        return '[';
    case VK_OEM_5:
        return '\\';
    case VK_OEM_6:
        return ']';
    case VK_OEM_MINUS:
        return '-';
    case VK_OEM_PLUS:
        return '=';
    default:
        if (wParam >= 0x20 && wParam <= 0x7E)
            return (uint32_t)wParam;
        return 0;
    }
}

/* Modifier keys are not text: never feed them to the engine (would
 * corrupt an in-flight syllable). CapsLock is no longer the mode toggle
 * (maintainer instruction: the Han/Eng key is the default), and in
 * Korean input mode the key is ignored outright: it passes through for
 * the OS to toggle, and the engine `caps` option it feeds is set per
 * key event beside the modifier snapshot (issue #112) - never into the
 * (base, shift) pair the engine resolves rows from. */
bool CTextService::IsModifierVk(WPARAM wParam) {
    switch (wParam) {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
    case VK_CAPITAL:
    case VK_TAB:
        return true;
    default:
        return false;
    }
}

/* Word buffering (issue #73, adapter-side; no libhunmin change):
 * hunmin_ic_commit() is a destructive read of every syllable the
 * engine finalised so far, so the adapter accumulates it instead of
 * inserting per syllable. The word size includes no NUL: the buffer
 * holds text only, or every later insert would stop at the embedded
 * NUL after the first syllable. */
void CTextService::HarvestFinalised() {
    const char *commit = hunmin_ic_commit(ic_);
    if (!commit || !commit[0])
        return;
    int clen = MultiByteToWideChar(CP_UTF8, 0, commit, -1, nullptr, 0);
    if (clen <= 1)
        return;
    size_t old = wordbuf_.size();
    wordbuf_.resize(old + clen - 1);
    MultiByteToWideChar(CP_UTF8, 0, commit, -1, &wordbuf_[old], clen);
}

/* The composition feed: the engine decides consumed/declined. A chord
 * (Ctrl/Alt) is application input, not composition input - measured
 * defect 2026-10-18: the letter inside a Ctrl/Alt chord still fed the
 * composition, Ctrl+A typed the preedit's next syllable instead of
 * select-all. The chord commits through the SAME boundary flush a mode
 * toggle drives and passes uneaten; the mode itself is NOT flipped
 * (ruled out 2026-10-18, issue #101: "will not switch mode; transient
 * processing is enough" - docs/WINDOWS-TSF-BUFFERING.md and
 * wiki/decisions/2026-10-18-tsf-chord-transient-not-persistent.md).
 * A decline is a word boundary for the buffering adapter (issue #73):
 * the boundary drain runs and the key passes (pfEat stays FALSE) -
 * the fcitx/android "used=FALSE => passthrough" contract. */
void CTextService::FeedEngineKey(ITfContext *pic, WPARAM wParam, BOOL *pfEat) {
    /* One snapshot of the modifier state for this event. The chord rule
     * below and the shift bit handed to the engine both read it, so the
     * decision and the bit can never disagree, and the harness's
     * key-state registration (keybinding.h) covers both with one
     * registered provider. */
    const unsigned int mods = hunmin_tsf_keymods_from_state();
    /* The engine caps option, fed per event beside the modifier snapshot
     * (issue #112, ruling "wire and make it common to all platform"):
     * one set per key event, 1 when the platform CapsLock is toggled on,
     * 0 when off - the shape fcitx5 has had all along (engine.cpp:327).
     * The option feeds the yethangul P variable only (hunmin.h:93); it
     * selects NO row - the Shift bit stays the only row transport
     * (the engine's case endpoint, hunmin_ic_process_key). */
    hunmin_ic_set_option(ic_, "caps", hunmin_tsf_capslock_from_state() ? 1 : 0);
    if (mods & (HUNMIN_TSF_KEYMOD_CTRL | HUNMIN_TSF_KEYMOD_ALT)) {
        cand_->Close();
        BoundaryFlush(pic);
        if (pfEat)
            *pfEat = FALSE;
        hunmin_logger.log(" ctrl/alt chord -> commit + passthrough\n");
        return;
    }
    if (IsModifierVk(wParam)) {
        hunmin_logger.log(" modifier/pass key -> leave composition\n");
        return;
    }
    /* The case endpoint is the engine's (issue #106): the unshifted base
     * plus the Shift bit from the snapshot above. */
    bool consumed = hunmin_ic_process_key(
        ic_, VkToBase(wParam), (mods & HUNMIN_TSF_KEYMOD_SHIFT) ? 1u : 0u);
    if (!consumed) {
        BoundaryFlush(pic);
        return;
    }
    HarvestFinalised();
    hunmin_logger.log(" consumed, word len=%zu preedit=%s\n", wordbuf_.size(),
                      hunmin_ic_preedit(ic_));
    /* Issue #119: the legacy interim path when the host was detected
     * terminating compositions mid-word (or the env said so); the #98
     * native path otherwise stays as is. The doc write is POSTED -
     * never inline in a TSF callback (the measured focus-storm defect). */
    delivery_->Consumed(pic);
    if (pfEat)
        *pfEat = TRUE;
}

STDMETHODIMP CTextService::OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) {
    (void)pic;
    (void)wParam;
    (void)lParam;
    if (pfEat)
        *pfEat = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) {
    (void)pic;
    (void)wParam;
    (void)lParam;
    if (pfEat)
        *pfEat = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnPreservedKey(ITfContext *pic, const GUID &rguid, BOOL *pfEat) {
    (void)pic;
    (void)rguid;
    if (pfEat)
        *pfEat = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition) {
    (void)ecWrite;
    (void)pComposition;
    hunmin_logger.log("OnCompositionTerminated comp=%p wordbuf=%zu self=%d\n",
                      (void *)composition_, wordbuf_.size(), endingSelf_ ? 1 : 0);
    if (endingSelf_) {
        /* Our own EndComposition (issue #98): the composed text is
         * document text by OUR decision, so the buffers drain and the
         * engine state resets - the same word must never be delivered
         * twice through the next boundary. */
        if (composition_) {
            composition_->Release();
            composition_ = nullptr;
        }
        if (compCtx_) {
            compCtx_->Release();
            compCtx_ = nullptr;
        }
        compText_.clear();
        wordbuf_.clear();
        if (ic_)
            hunmin_ic_reset(ic_);
        return S_OK;
    }
    /* Host-driven termination. A modern host terminates only at real
     * boundaries (focus loss, user click-away); when the state is
     * mid-word - the in-flight text was composed but the word is not
     * finished - the host is a classic (default-sink) one: the #119
     * measurement (rounds 11-14; the baseline round in
     * docs/TSF-INTERIM-INPUT-SPIKE.md) shows these hosts terminate per
     * key. Flip THIS SERVICE to the interim path for its life and
     * recover what this termination fused:
     *   - the engine's state is NOT ours to drop: the in-flight
     *     syllable is still live in the ic; the doc now holds a DEAD
     *     COPY of what the composition showed (compText_) - delete
     *     that copy from the control (EM), and the ic keeps composing
     *     from the same jamo state (no reset, no re-feed needed). */
    if (wordbuf_.empty() && compText_.empty()) {
        /* nothing mid-word: a real boundary under our own bookkeeping
         * must have run already - plain cleanup, old contract. */
        if (composition_) {
            composition_->Release();
            composition_ = nullptr;
        }
        if (compCtx_) {
            compCtx_->Release();
            compCtx_ = nullptr;
        }
        compText_.clear();
        return S_OK;
    }
    hunmin_logger.log("legacy detection: mid-word host termination (compText=%zu wordbuf=%zu) -> interim path\n",
                      compText_.size(), wordbuf_.size());
    legacyMode_ = true;
    UseDelivery(HUNMIN_TSF_DELIVERY_INTERIM);
    /* What is dead in the doc = the WHOLE composition text (compText_:
     * nothing under this composition was ours to keep there - the
     * interim path rebuilds the word from the LIVE buffers and live
     * ic state). Delete the dead copy at the POSTED write - never
     * inline here: this handler runs inside the host's TSF callback,
     * where the EM writes fire the re-entrant focus storm that eats
     * keystrokes (measured). One path for every caret state: the
     * write re-measures the caret when the app's queue has settled
     * (the fused copy's caret was measured BOTH behind the span and
     * past it, settling differently per host). The ic keeps composing
     * from the same jamo state (no reset) and wordbuf_ keeps the
     * finalised syllables so the write re-appends them to the doc. */
    delivery_->CaptureDeadCopy(compText_);
    /* A write posted by the terminated key is superseded: the host has
     * fused the composition text itself and the re-bookkeeping above
     * recomputes the span. The posted write runs as THIS flip's dead
     * copy removal instead. */
    delivery_->CaptureDeadCopy(compText_);
    if (composition_) {
        composition_->Release();
        composition_ = nullptr;
    }
    if (compCtx_) {
        compCtx_->Release();
        compCtx_ = nullptr;
    }
    compText_.clear();
    return S_OK;
}

/* ---- Native composition preedit (issue #98): machinery ----
 *
 * The engine's buffers reach the DOCUMENT while typing: the whole
 * buffered word (finalised syllables + in-flight preedit, issue #73)
 * sits in an ITfComposition started on the caret's empty selection
 * range, carried toward the app through one edit session per engine
 * transition, and underlined with GUID_PROP_ATTRIBUTE (the
 * display-attribute atom registered under HunminDisplayAttrGuid, the
 * attribute the provider serves). A word boundary ends the
 * composition: the text fuses into the document as ordinary text and
 * the buffers empty.
 */

/* One edit session makes the composition exist (if not yet) and shows
 * the whole buffered word in it, underlined. The nested member class
 * reaches the shell's private state through svc_ (the definition outside
 * the class keeps the session code where it was; the access rules of a
 * nested member class apply). */
class CTextService::CompositionSession : public ITfEditSession {
  public:
    CompositionSession(ITfContext *ctx, CTextService *svc, TfClientId cid, const wchar_t *t, bool endAfter)
        : ref_(1), ctx_(ctx), svc_(svc), cid_(cid), t_(t), endAfter_(endAfter) {}
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
        HRESULT hr = S_OK;
        if (!svc_->composition_) {
            /* First preedit key in this context: the empty selection
             * range becomes the composition. A host that refuses the
             * interface leaves composition_ null, and the caller's
             * boundary path then falls back to the plain insert. */
            ITfContextComposition *cc = nullptr;
            if (SUCCEEDED(ctx_->QueryInterface(IID_ITfContextComposition, (void **)&cc)) && cc) {
                TF_SELECTION sel;
                ULONG fetched = 0;
                if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) &&
                    fetched == 1 && sel.range) {
                    HRESULT hs = cc->StartComposition(
                        ec, sel.range, static_cast<ITfCompositionSink *>(svc_), &svc_->composition_);
                    if (SUCCEEDED(hs) && svc_->composition_) {
                        svc_->compCtx_ = ctx_;
                        ctx_->AddRef();
                    }
                    hunmin_logger.log("composition start hr=0x%lX comp=%p\n", (unsigned long)hs,
                                      (void *)svc_->composition_);
                    sel.range->Release();
                }
                cc->Release();
            }
        }
        if (svc_->composition_) {
            ITfRange *range = nullptr;
            if (SUCCEEDED(svc_->composition_->GetRange(&range)) && range) {
                /* An in-composition rewrite may CLEAR the range: an
                 * engine backspace can empty both buffers while the
                 * composition lives, and skipping the SetText left the
                 * stale preedit visible (measured at the seosi trace's
                 * backspace, issue #98; a real host shows the same
                 * ghost). Empty fuses nothing if the boundary ends it
                 * next. */
                hr = range->SetText(ec, 0, t_, -1);
                svc_->displayAttr_->Apply(ctx_, ec, range, endAfter_);
                /* Caret: the COLLAPSE must never touch the composition's
                 * own range (escaping a collapsed-at-end range would
                 * make the next rewrite splice AFTER the text instead
                 * of replacing it). Write mode keeps the selection on
                 * the composition; end mode walks a clone to the end. */
                if (endAfter_) {
                    ITfRange *caret = nullptr;
                    if (SUCCEEDED(range->Clone(&caret)) && caret) {
                        caret->Collapse(ec, TF_ANCHOR_END);
                        TF_SELECTION cs = {};
                        cs.range = caret;
                        cs.style.ase = TF_AE_END;
                        cs.style.fInterimChar = FALSE;
                        ctx_->SetSelection(ec, 1, &cs);
                        caret->Release();
                    }
                } else {
                    TF_SELECTION ns = {};
                    ns.range = range;
                    ns.style.ase = TF_AE_END;
                    ns.style.fInterimChar = FALSE;
                    ctx_->SetSelection(ec, 1, &ns);
                }
                range->Release();
            }
            svc_->compText_ = t_;
            if (endAfter_) {
                /* Issue #119: our own end is flagged so the sink cannot
                 * mistake it for a host-driven termination (measured:
                 * the sink has never fired for a service-called
                 * EndComposition - the wrap is protection, not assumed
                 * behaviour). */
                svc_->endingSelf_ = true;
                svc_->composition_->EndComposition(ec);
                hunmin_logger.log("composition end text=%ls\n", t_);
                svc_->composition_->Release();
                svc_->composition_ = nullptr;
                if (svc_->compCtx_) {
                    svc_->compCtx_->Release();
                    svc_->compCtx_ = nullptr;
                }
                svc_->compText_.clear();
                svc_->endingSelf_ = false;
            }
        }
        return S_OK;
    }

  private:
    LONG ref_;
    ITfContext *ctx_;
    CTextService *svc_;
    TfClientId cid_;
    const wchar_t *t_;
    bool endAfter_;
};

/* The write half of the composition lifecycle: one edit session makes
 * the composition exist (if not yet) and shows the whole buffered word
 * in it, underlined. Called after every engine transition that a bound
 * key drives. */
void CTextService::CompositionWrite(ITfContext *pic) {
    if (!pic)
        return;
    std::wstring full = wordbuf_;
    if (ic_) {
        const char *pre = hunmin_ic_preedit(ic_);
        full += HunminUtf8ToWide(pre);
    }
    if (full.empty() && !composition_)
        return;
    CompositionSession *cs = new CompositionSession(pic, this, clientId_, full.c_str(), false);
    HRESULT hrC = S_OK;
    pic->RequestEditSession(clientId_, cs, TF_ES_SYNC | TF_ES_READWRITE, &hrC);
    cs->Release();
}

/* The end half: rewrites the composition range to the final text if it
 * differs, drops the display attribute, and calls EndComposition - the
 * composed text fuses into the document as ordinary text. Runs in the
 * composition's own context when called after a context switch. */
void CTextService::CompositionEnd(ITfContext *pic, const std::wstring &text) {
    ITfContext *ctx = (composition_ && compCtx_) ? compCtx_ : pic;
    if (!ctx)
        return;
    CompositionSession *cs = new CompositionSession(ctx, this, clientId_, text.c_str(), true);
    HRESULT hrE = S_OK;
    ctx->RequestEditSession(clientId_, cs, TF_ES_SYNC | TF_ES_READWRITE, &hrE);
    cs->Release();
}

/* A context/focus switch ends the live composition in its own context
 * (the word stays there as committed text); the buffers drain so the
 * next context starts clean - the old 0.2 boundary-drain contract,
 * pointed at the composition instead of the overlay. */
void CTextService::DetachComposition() {
    if (!composition_ || !compCtx_)
        return;
    FlushEngineIntoWordbuf();
    std::wstring text = wordbuf_;
    wordbuf_.clear();
    CompositionEnd(compCtx_, text);
}

/* The NATIVE boundary insert (the native half of the delivery seam):
 * with a live composition the word is ALREADY document text (the
 * composition range holds it) - the session ends the composition,
 * fusing the word in unstyled. Only a host that refused
 * ITfContextComposition falls back to the plain selection SetText the
 * 0.2 adapter committed with. */
class CTextService::FlushSession : public ITfEditSession {
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
        hunmin_logger.log("flush: insert hr=0x%lX\n", (unsigned long)hr);
        return S_OK;
    }

  private:
    LONG ref_;
    ITfContext *ctx_;
    CTextService *svc_;
    TfClientId cid_;
    const wchar_t *t_;
};

void CTextService::InsertBufferedWord(ITfContext *pic) {
    if (wordbuf_.empty())
        return;
    if (composition_) {
        CompositionEnd(pic, wordbuf_);
    } else {
        FlushSession *fs = new FlushSession(pic, this, clientId_, wordbuf_.c_str());
        HRESULT hrFlush = S_OK;
        if (pic)
            pic->RequestEditSession(clientId_, fs, TF_ES_SYNC | TF_ES_READWRITE, &hrFlush);
        fs->Release();
    }
    wordbuf_.clear();
}

/* One composition-boundary drain shared by the declined key path and the
 * mode toggle (issue #66): the in-flight syllable is flushed into the
 * buffered word, and the whole word is inserted exactly once before the
 * boundary key (or mode switch) proceeds. */
/* The engine's in-flight syllable appended to the buffered word. Shared by
 * the boundary drain, the deactivation drain and the composition detach. */
void CTextService::FlushEngineIntoWordbuf() {
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
}

void CTextService::BoundaryFlush(ITfContext *pic) {
    FlushEngineIntoWordbuf();
    hunmin_logger.log(" boundary, word len=%zu preedit=%s\n", wordbuf_.size(),
                      ic_ ? hunmin_ic_preedit(ic_) : "");
    delivery_->Boundary(pic);
}

/* The caret rect the candidate popup anchors to: one read-only edit
 * session against the selection (GetTextExt), falling back to the
 * thread's GUITHREADINFO caret. Screen coordinates out. False means
 * the host reported nothing drawable - the popup/overlay hide path. */
class CTextService::RectSession : public ITfEditSession {
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

bool CTextService::CaretRect(ITfContext *pic, RECT *rcOut, bool *haveCaretOut,
                             GUITHREADINFO *gtiOut) {
    hasPreeditRect_ = false;
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

/* ---- the feature registry (DIP): the components' factories are
 * registered here, the shell constructs at registration and switches by
 * name. ---- */
void CTextService::RegisterDeliveryFactory(HunminDeliveryFactory *factory) {
    if (!factory)
        return;
    for (const DeliverySlot &slot : deliveries_)
        if (slot.name && strcmp(slot.name, factory->Name()) == 0)
            return; /* the component owns its names; a re-register adds nothing */
    DeliverySlot slot;
    slot.name = factory->Name();
    slot.impl.reset(factory->Create(*this));
    deliveries_.push_back(std::move(slot));
}

bool CTextService::UseDelivery(const char *name) {
    if (!name)
        return false;
    for (DeliverySlot &slot : deliveries_) {
        if (slot.name && strcmp(slot.name, name) == 0) {
            delivery_ = slot.impl.get();
            return true;
        }
    }
    return false;
}

/* The tier-F1 observation channel: one call, one struct. */
HunminTsfSnapshot CTextService::Snapshot() {
    HunminTsfSnapshot s;
    s.ctx = ctx_;
    s.ic = ic_;
    s.wordbuf = wordbuf_;
    s.hangulMode = hangulMode_;
    s.legacyMode = legacyMode_;
    s.interimForce = interimForce_;
    s.composition = composition_;
    s.itemSink = langbar_ ? langbar_->Sink() : nullptr;
    s.toggleKeys = toggleKeys_;
    s.hangulId = hangulId_;
    s.latinId = latinId_;
    if (cand_)
        cand_->Observe(&s);
    return s;
}

void CTextService::UpdateConversionCompartment() {
    /* The compartment the language bar reads to draw its 中/英 badge
     * (GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
     * TF_CONVERSIONMODE_NATIVE - measured: the state the legacy bar
     * and the tray use). Setting it on every mode boundary is how a
     * TIP's own mode is reflected by the system surfaces. */
    if (!threadMgr_) {
        return;
    }
    ITfCompartmentMgr *cm = nullptr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfCompartmentMgr, (void **)&cm)) && cm) {
        ITfCompartment *c = nullptr;
        if (SUCCEEDED(cm->GetCompartment(
                GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, &c)) &&
            c) {
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_I4;
            v.lVal = hangulMode_ ? (LONG)TF_CONVERSIONMODE_NATIVE : 0L;
            c->SetValue(0, &v);
            VariantClear(&v);
            c->Release();
        } else {
            hunmin_logger.log(" conversion compartment GONE\n");
        }
        cm->Release();
    }
}

/* Issue #71 bridge, defined in the TU that owns CTextService:
 * settingsprovider.cpp (ITfFnConfigure::Show) hands the LIVE service back
 * through a plain void* because the two TUs' headers deliberately do not
 * entangle. */
void hunmin_tsf_apply_config_store(void *svc) {
    if (!svc)
        return;
    static_cast<CTextService *>(svc)->ApplyConfigStore();
}
