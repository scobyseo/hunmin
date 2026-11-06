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
#include "settingspath.h"
#include "utf.h" /* HunminUtf8ToWide for the layer's commit text */ /* the shared settings-exe resolver */

#include <cstdio>
#include <cstring>
#include <cwchar>
#include <msctf.h>
#include <richedit.h>
#include <shlguid.h>
#include <shlobj_core.h>
#include <shobjidl_core.h>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

/* Debug logs land next to the DLL (build-msvc/Release inside the checkout),
 * not at a drive root: the registered InprocServer32 path is absolute, so the
 * DLL's own directory is the one clone-independent location every host
 * process can resolve, and it stays inside the workspace. Set
 * HUNMIN_TSF_LOG_DIR to override when that directory is read-only. */

/* Launch the settings surface: the WinUI 3 exe (the ruled surface, assets
 * icon) when it is deployed beside this DLL, the rundll32 entry to
 * HunminSettingsShow otherwise. A dedicated process: the TIP thread must
 * not pump a nested modal loop while TSF holds edit cookies. */
HRESULT hunmin_tsf_launch_settings() {
    /* The anchor is this function's own address: it is in this DLL by
     * construction. The former void* parameter let a caller pass null
     * (OnMenuSelect did), GetModuleHandleExW then failed, and the menu item
     * returned E_FAIL without launching anything. */
    const void *anchor = (const void *)&hunmin_tsf_launch_settings;
    HMODULE self = nullptr;
    wchar_t dll[MAX_PATH] = {0};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)anchor, &self) ||
        !GetModuleFileNameW(self, dll, MAX_PATH))
        return E_FAIL;
    /* Shared resolution (settingspath.cpp, deployed-first order). No exe
     * found leaves exe empty, which selects the rundll32 fallback. */
    wchar_t exe[MAX_PATH];
    hunmin_tsf_settings_exe_find(anchor, exe, MAX_PATH);
    wchar_t cmd[MAX_PATH + 64];
    SHELLEXECUTEINFOW sei = {sizeof sei};
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.nShow = SW_SHOWNORMAL;
    if (exe[0]) {
        sei.lpFile = exe;
        sei.lpParameters = nullptr;
    } else {
        _snwprintf_s(cmd, _countof(cmd), _TRUNCATE, L"\"%s,HunminSettingsShow\"", dll);
        sei.lpFile = L"rundll32.exe";
        sei.lpParameters = cmd;
    }
    BOOL ok = ShellExecuteExW(&sei);
    hunmin_logger.log("launch settings exe=%ls hr=%ld\n", exe, (long)GetLastError());
    return ok ? S_OK : E_FAIL;
}

CTextService::CTextService()
    : ref_(1), threadMgr_(nullptr), clientId_(TF_CLIENTID_NULL), sinkCookie_(0), ctx_(nullptr), preedit_(nullptr), hangulMode_(true),
      hasPreeditRect_(false), lastCtx_(nullptr), composition_(nullptr), compCtx_(nullptr), displayAttr_(new DisplayAttr()), interimForce_(false),
      legacyMode_(false), endingSelf_(false), hangulId_(kHangulDefaultId), latinId_(kLatinDefaultId),
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
    /* The language-bar item is NOT built here: it is created and installed by
     * UpdateLangBarItem(ptm), the ruled install site (maintainer ruling
     * 2026-10-29) -- the shell asks for the item only after the service adds
     * it, so nothing can QI it before then. */
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
    if (preedit_)
        hunmin_buffered_preedit_free(preedit_);
    if (ctx_)
        hunmin_ctx_free(ctx_);
    if (lastCtx_)
        lastCtx_->Release();
}

namespace {

/* The QI answer table (stage Q): the IIDs the object answers, the tag the
 * QI trace prints, and which half answers. One table feeds both the log
 * line and the dispatch, so a new interface cannot be answered without
 * being logged. */
enum QiKind { kQiThis, kQiSettingsProvider, kQiLangBar };

struct QiRoute {
    const IID *iid;
    const char *tag;
    QiKind kind;
    IUnknown *(*as)(CTextService *);
};

IUnknown *AsTip(CTextService *svc) { return static_cast<ITfTextInputProcessor *>(svc); }
IUnknown *AsThreadMgrSink(CTextService *svc) { return static_cast<ITfThreadMgrEventSink *>(svc); }
IUnknown *AsKeySink(CTextService *svc) { return static_cast<ITfKeyEventSink *>(svc); }
IUnknown *AsCompositionSink(CTextService *svc) { return static_cast<ITfCompositionSink *>(svc); }
IUnknown *AsDisplayAttrProvider(CTextService *svc) { return static_cast<ITfDisplayAttributeProvider *>(svc); }

const QiRoute kQiRoutes[] = {
    /* IID_IUnknown answers with the ITfTextInputProcessor subobject: the
     * class has no single IUnknown base to return. */
    {&IID_IUnknown, "tip", kQiThis, AsTip},
    {&IID_ITfTextInputProcessor, "tip", kQiThis, AsTip},
    {&IID_ITfThreadMgrEventSink, "tmesink", kQiThis, AsThreadMgrSink},
    {&IID_ITfKeyEventSink, "keysink", kQiThis, AsKeySink},
    {&IID_ITfCompositionSink, "compsink", kQiThis, AsCompositionSink},
    /* Issue #71: the settings surface. QI'able on the live text service
     * object, exactly the TSF-native reachability mechanism;
     * GetFunction(IID_ITfFnConfigure) then hands out the configure
     * function (settingsprovider.cpp). */
    {&IID_ITfFunctionProvider, "fnprov", kQiSettingsProvider, nullptr},
    /* Issue #98: the display-attribute provider the app-side attribute
     * manager looks for through the GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER
     * category. */
    {&IID_ITfDisplayAttributeProvider, "dispattr", kQiThis, AsDisplayAttrProvider},
    /* S6: the language bar speaks to the TEAR-OFF item, not to the text
     * service. Its own QI answers IUnknown, ITfLangBarItem,
     * ITfLangBarItemButton and ITfSource, so the bar asks the item (not
     * this object) to AdviseSink its ITfLangBarItemSink. */
    {&IID_ITfLangBarItem, "lbi", kQiLangBar, nullptr},
    {&IID_ITfLangBarItemButton, "lblbtn", kQiLangBar, nullptr},
    {&IID_ITfSource, "source", kQiLangBar, nullptr},
};

const QiRoute *FindQiRoute(REFIID riid) {
    for (const QiRoute &route : kQiRoutes)
        if (IsEqualIID(riid, *route.iid))
            return &route;
    return nullptr;
}

} // namespace

STDMETHODIMP CTextService::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv)
        return E_INVALIDARG;
    const QiRoute *route = FindQiRoute(riid);
    /* The QI trace (the maintainer's request, the hosted-item
     * diagnosis): one line per QI, the asked IID's lead field and
     * the answered slot mask - the langbar's hosting decision is
     * where the item's shape gets judged, so the log names which
     * IID rounds the object actually gets asked for. */
    hunmin_logger.log("QI riid=%08lX-%04X answered=%s pid=%lu\n", (unsigned long)riid.Data1, (unsigned)riid.Data2, route ? route->tag : "none",
                      (unsigned long)GetCurrentProcessId());
    if (!route) {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    switch (route->kind) {
    case kQiThis:
        *ppv = route->as(this);
        AddRef();
        return S_OK;
    case kQiSettingsProvider:
        *ppv = hunmin_tsf_settings_new_provider(this);
        return S_OK;
    case kQiLangBar:
        if (!langbar_) {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        return langbar_->QueryInterface(riid, ppv);
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
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
    hunmin_logger.log("  toggle keys=%zu (configured bits: %s)\n", toggleKeys_.size(), cfg.toggleKeys[0] ? cfg.toggleKeys : "(none)");
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
        hunmin_logger.log("  hanja key unparseable (%s) -> default VK_HANJA\n", cfg.hanjaKey[0] ? cfg.hanjaKey : "(empty)");
    }
}

/* The engine instance for the CURRENT mode: the config boundary and the mode
 * toggle both swap it, so the free+new pair has one home. */
void CTextService::RebuildLayer() {
    if (preedit_) {
        hunmin_buffered_preedit_free(preedit_);
        preedit_ = nullptr;
    }
    if (ctx_) {
        /* TWO languages, so the language action is the layer's (FE7): the layer
         * holds both ids and switches between them, which is what lets TSF,
         * fcitx5 and Android share one definition of a mode change. Until
         * 2026-10-25 this passed `nullptr` for the latin slot and did the
         * switch adapter-side, so the layer answered "not ours".
         *
         * The commit unit is the user's (issue #128, commitUnit_): "word" is the
         * default because TSF's native composition shows the whole word, and
         * "syllable" is the same text delivered earlier. */
        const int policy = (strcmp(commitUnit_, "syllable") == 0) ? HUNMIN_COMMIT_PER_SYLLABLE : HUNMIN_COMMIT_AT_BOUNDARY;
        preedit_ = hunmin_buffered_preedit_new(ctx_, hangulId_.c_str(), latinId_.c_str(), policy);
        if (preedit_) {
            /* R13-정정's ON/OFF (issue #175) is applied once per rebuild, before
             * anything can judge: the layer's state is the engine's, and this is
             * the one place the stored preference reaches it. The engine REFUSES
             * the value where its word list is absent (B3, #169) and this
             * frontend refuses it where the delivery cannot judge (2026-11-02),
             * so a stale stored 1 leaves the feature off rather than half-enabled
             * -- which is why the dialog reads the value back from the layer. */
            ApplyAutoswitch();
            /* R8's judgement needs the English list (R8-정정-2, ruled
             * 2026-11-01). Loaded per layer, from the same data dirs the
             * layouts came from; absent data is a normal state (B3) and the
             * judgement then abstains instead of calling every word not-English. */
            hunmin_buffered_preedit_load_english_shipped(preedit_);
            /* The engine resolves a compound press and calls back into this host
             * for the platform half (SPEC-MODES §E5, ruled 2026-10-31). It is
             * registered once per layer, beside the layer's construction, so a
             * rebuilt layer cannot end up with the previous host's state. */
            hunmin_special_host host{};
            host.user = this;
            host.supports = &CTextService::SpecialSupports;
            host.perform = [](void *user, hunmin_special_function fn) { static_cast<CTextService *>(user)->SpecialPerform(fn); };
            hunmin_buffered_preedit_special_host(preedit_, &host);
        }
        hangulMode_ = (hunmin_buffered_preedit_language(preedit_) == HUNMIN_LANGUAGE_HANGUL);
    }
}

void CTextService::ApplyConfigStore() {
    hunmin_logger.log("ApplyConfigStore pid=%lu\n", (unsigned long)GetCurrentProcessId());
    HunminTsfConfig cfg;
    hunmin_tsf_config_load(HKEY_CURRENT_USER, &cfg);
    /* The commit unit the layer is built with (issue #128), from the same
     * env -> registry -> default chain every other setting uses. */
    snprintf(commitUnit_, sizeof commitUnit_, "%s", cfg.commitUnit);
    autoswitch_ = cfg.autoswitch;
    hunmin_tsf_resolve_layout_slot(ctx_, cfg.hangulLayout, kHangulDefaultId, true, &hangulId_);
    hunmin_tsf_resolve_layout_slot(ctx_, cfg.latinLayout, kLatinDefaultId, false, &latinId_);
    LoadToggleKeys(cfg);
    LoadHanjaKey(cfg);
    /* The candidate window belongs to a composition the ic swap below
     * drops: close it with Apply, like the mode toggle does (issue #67). */
    cand_->Close();
    RebuildLayer();
    hunmin_logger.log("  applied hangul=%s latin=%s layer=%p\n", hangulId_.c_str(), latinId_.c_str(), (void *)preedit_);
}

void CTextService::UpdateLangBarItem(ITfThreadMgr *ptm) {
    /* THE install site (maintainer ruling 2026-10-29): the item is created
     * here, added here, and notified here. language-bar.md, "Adding Items to
     * the Language Bar": obtain ITfLangBarItemMgr with ITfThreadMgr::QueryInterface,
     * then call AddItem -- the manager belongs to the caller, not to the item. */
    if (!langbar_)
        langbar_ = hunmin_tsf_langbar_new(*this, CLSID_HunminTSF);
    if (!langbar_)
        return;
    if (!langbar_->Attached() && ptm) {
        ITfLangBarItemMgr *plbm = nullptr;
        const HRESULT q = ptm->QueryInterface(IID_ITfLangBarItemMgr, (void **)&plbm);
        hunmin_logger.log("langbar QI hr=0x%lX\n", (unsigned long)q);
        if (plbm) {
            langbar_->Attach(plbm);
            plbm->Release();
        }
    }
    /* Every later call is the badge refresh: OnUpdate(TF_LBI_ICON|TF_LBI_TEXT)
     * over the kernel sink the bar advised at AddItem. */
    langbar_->Update();
}

/* R13-정정's ON/OFF as this delivery can carry it (issue #175). Two gates sit
 * behind the one call:
 *   - the ENGINE's: hunmin_ic_set_option refuses 1 where the modes word list is
 *     absent (B3, #169), and the value reads back 0;
 *   - this FRONTEND's: the interim delivery moves the finished word into the
 *     document per key, so at its boundary the layer holds no whole word to
 *     judge (docs/FRONTEND-EQUIVALENCE.md §2.1, ruled 2026-11-02: "interim model
 *     cannot support autoswitch"). The option is therefore only ON while the
 *     native path is active, and every site that flips the delivery re-applies
 *     it -- otherwise a host that flips mid-word would keep a state it cannot
 *     honour, and the English passthrough ruling (same date) would stop feeding
 *     the app's own keys for a switch that can never fire. */
void CTextService::ApplyAutoswitch() {
    if (!preedit_)
        return;
    hunmin_buffered_preedit_set_option(preedit_, "autoswitch", AutoswitchOn() ? 1 : 0);
}

void CTextService::ApplyJudgement(ITfContext *pic) {
    /* R8, ruled 2026-11-01 (#164): the engine judges and delivers the word; the
     * frontend performs the switch. Asking costs one call and answers -1 in the
     * common case (the switch is off, the word was too short, or both readings
     * agreed), so this is safe to call on every key.
     *
     * The word is already in the client by the time this runs -- it arrived in
     * out.commit -- so nothing is inserted or rewritten here (R10). What is
     * left is the state the next key reads: the layer's slot, the conversion
     * compartment the host stores, and the langbar. */
    const int judged = hunmin_buffered_preedit_take_judgement(preedit_);
    if (judged < 0)
        return;
    if (judged == hunmin_buffered_preedit_language(preedit_))
        return; /* already there */
    hunmin_key_result out{};
    if (!hunmin_buffered_preedit_toggle_language(preedit_, &out))
        return;
    /* A one-language layer cannot reach here, but the boundary the switch
     * performs may still hold text; deliver it rather than drop it. */
    if (out.commit && out.commit[0])
        InsertText(pic, HunminUtf8ToWide(out.commit));
    hangulMode_ = (hunmin_buffered_preedit_language(preedit_) == HUNMIN_LANGUAGE_HANGUL);
    UpdateConversionCompartment();
    UpdateLangBarItem(threadMgr_);
    hunmin_logger.log(" autoswitch -> mode=%s\n", hangulMode_ ? "hangul" : "english");
}

void CTextService::ToggleModeCore(ITfContext *pic) {
    cand_->Close();
    /* The language action is the LAYER's (FE7): the boundary, the switch and
     * the other layout's ic attach, in one place, so the three frontends cannot
     * disagree about what a mode change is. The layer is the state afterwards --
     * rebuilding it here would reset the mode to the configured default, which
     * is why there is no RebuildLayer on this path. What stays adapter-side is
     * what the layer cannot know: the candidate window closing above, and the
     * host surfaces below. */
    hunmin_key_result out;
    const int switched = hunmin_buffered_preedit_toggle_language(preedit_, &out);
    if (out.commit && out.commit[0])
        InsertText(pic, HunminUtf8ToWide(out.commit));
    if (switched)
        hangulMode_ = (hunmin_buffered_preedit_language(preedit_) == HUNMIN_LANGUAGE_HANGUL);
    UpdateConversionCompartment();
    UpdateLangBarItem(threadMgr_);
    /* This gated line is the one evidence a mode boundary ran in this
     * host at all: the key path's own log line lives in its handler. */
    hunmin_logger.log("toggle core -> mode=%s ic=%p pic=%p\r\n", hangulMode_ ? "hangul" : "english", (void *)preedit_, (void *)pic);
}

/* The two TSF advice registrations Activate needs: the thread-manager event
 * sink and the key-event sink. Both belong to the live session; Deactivate's
 * UnadviseSinks() withdraws both (issue #145). */
void CTextService::AdviseSinks() {
    if (!threadMgr_)
        return;
    ITfKeystrokeMgr *pks = nullptr;
    hunmin_logger.log("Advise threadmgr attempt\n");
    ITfSource *src = nullptr;
    HRESULT srch = threadMgr_->QueryInterface(IID_ITfSource, (void **)&src);
    HRESULT tmHr = E_NOINTERFACE;
    if (SUCCEEDED(srch) && src) {
        tmHr = src->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink *>(this), &sinkCookie_);
        src->Release();
    }
    hunmin_logger.log("  source hr=0x%lX threadmgr advise hr=0x%lX cookie=%lu\n", (unsigned long)srch, (unsigned long)tmHr,
                      (unsigned long)sinkCookie_);
    HRESULT qh = threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pks);
    hunmin_logger.log("Advise QI hr=0x%lX pks=%p\n", (unsigned long)qh, (void *)pks);
    if (SUCCEEDED(qh) && pks) {
        HRESULT ah = pks->AdviseKeyEventSink(clientId_, this, TRUE);
        hunmin_logger.log("AdviseKeyEventSink hr=0x%lX\n", (unsigned long)ah);
        pks->Release();
    }
}

/* The undo of AdviseSinks(): both registrations, and nothing else. The
 * thread-manager REFERENCE is dropped separately (ReleaseThreadMgr) because
 * Deactivate still needs a live manager after the advice comes off -- the
 * language-bar item is removed through it. */
void CTextService::UnadviseSinks() {
    if (!threadMgr_)
        return;
    /* The event sink first. A cookie of 0 means AdviseSink was refused (or
     * never ran), and ITfSource::UnadviseSink would answer E_INVALIDARG for
     * it, so the registration is only undone when it was actually made. */
    if (sinkCookie_) {
        ITfSource *src = nullptr;
        if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, (void **)&src)) && src) {
            const HRESULT uhr = src->UnadviseSink(sinkCookie_);
            hunmin_logger.log("Unadvise threadmgr event sink cookie=%lu hr=0x%lX\n", (unsigned long)sinkCookie_, (unsigned long)uhr);
            src->Release();
        }
        sinkCookie_ = 0;
    }
    ITfKeystrokeMgr *pks = nullptr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pks)) && pks) {
        pks->UnadviseKeyEventSink(clientId_);
        pks->Release();
    }
}

/* The release half, so that Deactivate's teardown can keep using the manager
 * between unadvising and letting go of it (issue #145). */
void CTextService::ReleaseThreadMgr() {
    if (!threadMgr_)
        return;
    threadMgr_->Release();
    threadMgr_ = nullptr;
}

/* Issue #119: the legacy path is DETECTED at runtime (a mid-word host
 * termination flips the service); HUNMIN_TSF_INTERIM=1 only FORCES every
 * context onto it up front (debug/probe escape hatch). */
void CTextService::DetectLegacyMode() {
    char v[8] = {0};
    interimForce_ = GetEnvironmentVariableA("HUNMIN_TSF_INTERIM", v, sizeof v) && (v[0] == '1' || _stricmp(v, "on") == 0 || _stricmp(v, "true") == 0);
    legacyMode_ = interimForce_;
    UseDelivery(legacyMode_ ? HUNMIN_TSF_DELIVERY_INTERIM : HUNMIN_TSF_DELIVERY_NATIVE);
    /* The delivery decides whether the automatic switch can be honoured at all
     * (ruled 2026-11-02), so the layer's state is re-derived here as well as at
     * every rebuild. */
    ApplyAutoswitch();
    hunmin_logger.log("legacy force=%d detected-mode=%d autoswitch=%d\n", interimForce_ ? 1 : 0, legacyMode_ ? 1 : 0, AutoswitchOn() ? 1 : 0);
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
     * mode indicator (icon 한/A, text) AND the right-click config menu ride our
     * own ITfLangBarItemButton. The install itself is UpdateLangBarItem's
     * (maintainer ruling 2026-10-29): create the item, obtain
     * ITfLangBarItemMgr with ITfThreadMgr::QueryInterface, AddItem. */
    UpdateLangBarItem(ptm);
    UpdateConversionCompartment();
    if (ctx_)
        hunmin_logger.log("  layout_count=%d\n", hunmin_ctx_layout_count(ctx_));
    return S_OK;
}

STDMETHODIMP CTextService::Deactivate() {
    /* The advice comes off first, so no new event reaches a session that is
     * tearing down. The manager REFERENCE outlives it (ReleaseThreadMgr at
     * the end): the language-bar item is removed through that same manager
     * below, and a manager that is already gone cannot remove it. */
    UnadviseSinks();
    /* A live composition (issue #98) is finalised in its own context
     * first: the composed word stays there as committed text and the
     * buffers drain, so the plain-insert path below only handles what
     * is left when no composition could ever be started. */
    DetachComposition();
    // The layer finalises on commit_word, which InsertBufferedWord calls.
    if (lastCtx_)
        InsertBufferedWord(lastCtx_); /* commits nothing when the word is empty */
    cand_->Close();
    /* Remove the language-bar item while the manager is still alive, through a
     * manager obtained the same way Activate obtained its own (language-bar.md:
     * "either uses the same ITfLangBarItemMgr interface used to add the items
     * or obtains another instance"). Kept before ReleaseThreadMgr() -- issue
     * #145: this used to run after UnadviseSinks() had nulled the thread
     * manager, which made the call a no-op and left the bar holding an item
     * whose service had gone. */
    if (threadMgr_) {
        ITfLangBarItemMgr *plbm = nullptr;
        if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfLangBarItemMgr, (void **)&plbm)) && plbm) {
            langbar_->Detach(plbm);
            plbm->Release();
        } else {
            langbar_->Detach(nullptr);
        }
    }
    if (preedit_) {
        hunmin_buffered_preedit_free(preedit_);
        preedit_ = nullptr;
    }
    if (ctx_) {
        hunmin_ctx_free(ctx_);
        ctx_ = nullptr;
    }
    if (lastCtx_) {
        lastCtx_->Release();
        lastCtx_ = nullptr;
    }
    ReleaseThreadMgr();
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
    hunmin_logger.log("TM-OnSetFocus pdmFocus=%p pdmPrev=%p modePid=%lu\n", (void *)pdmFocus, (void *)pdmPrevFocus,
                      (unsigned long)GetCurrentProcessId());
    if (pdmFocus) {
        /* The view probe the trace reads; run regardless of the gate so
         * the pic half (its Release) can never leak behind an if. */
        ITfContext *pic = nullptr;
        HRESULT hrTop = pdmFocus->GetTop(&pic);
        hunmin_logger.log("  GetTop hr=0x%lX pic=%p\n", (unsigned long)hrTop, (void *)pic);
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
    hunmin_logger.log("KS-OnSetFocus fFocus=%d modePid=%lu\n", (int)fFocus, (unsigned long)GetCurrentProcessId());
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
    s.layer = preedit_;
    s.display = ComposeDisplay();
    s.composing = ComposeInFlight();
    /* The finished half is the display without its in-flight tail. Computed
     * rather than taken, because taking would advance the layer -- a snapshot
     * is a read. */
    if (s.composing.size() <= s.display.size())
        s.finished = s.display.substr(0, s.display.size() - s.composing.size());
    s.hangulMode = hangulMode_;
    s.legacyMode = legacyMode_;
    s.interimForce = interimForce_;
    /* The ENGINE's answer, not the stored one: a refused value reads 0 here. */
    int autoswitch = 0;
    s.autoswitch = (preedit_ && hunmin_buffered_preedit_get_option(preedit_, "autoswitch", &autoswitch)) ? autoswitch : 0;
    s.composition = composition_;
    s.itemSink = langbar_ ? langbar_->Sink() : nullptr;
    s.toggleKeys = toggleKeys_;
    s.lastSpecial = lastSpecial_;
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
        if (SUCCEEDED(cm->GetCompartment(GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, &c)) && c) {
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
