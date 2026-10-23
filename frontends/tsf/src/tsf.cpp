/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The TSF shell: the CTextService implementations and the settings-exe
 * resolver. The data source and the layout-slot check live in datadir.cpp
 * (shared with the dialog and the tests). The component seams live in
 * tsf.h; the implementations
 * register from delivery.cpp (both delivery modes) and come in through
 * hunmin_tsf_hanja_candidates_new (the Hanja flow, candidates.cpp).
 */
#include "tsf.h"

#include "candidates.h"
#include "delivery.h"
#include "displayattr.h" /* the provider forwards + DisplayAttr construction */
#include "langbar.h" /* the tear-off language-bar item (S6) */
#include "settings.h" /* ITfFunctionProvider branch + the apply bridge */

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
