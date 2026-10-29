/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The language-bar item as a tear-off COM object (issues #67/#103). The
 * behaviour moved here from CTextService unchanged; the shell forwards QI
 * for ITfLangBarItem/ITfLangBarItemButton/ITfSource to this object.
 */
#include "langbar.h"

#include "log.h"
#include "settings.h" /* hunmin_tsf_launch_settings */

#include <cstring>
#include <windows.h>

/* GUID_LBI_INPUTMODE {2C77A81E-41CC-4178-A3A7-5F8A987568E6} - Windows 8+ and
 * Win11: the item the "input indicator" (the taskbar per-TIP badge) hosts in
 * the tray MUST carry this guidItem; any other item GUID is ignored by the
 * shell's hosted-item path (measured sibling jamotong TIP, hosted live).
 * The MSVC SDK declares GUID_LBI_INPUTMODE as an extern (ctffunc.h), so the
 * value would come from a link library; it is a private constant here so no
 * library set change is needed. The item's own earlier private GUID
 * F2C70003 (issue #67) never got past AddItem on 26200 and is retired. */
static const GUID HunminLbiInputModeGuid = {0x2C77A81E, 0x41CC, 0x4178, {0xA3, 0xA7, 0x5F, 0x8A, 0x98, 0x75, 0x68, 0xE6}};

namespace {
/* A CODE address in this DLL, usable as the FROM_ADDRESS module anchor: a
 * member-function pointer is not a plain pointer, and the item object lives on
 * the heap (no module owns that address). log.h uses the same shape. */
void LangBarModuleAnchor() {}
} // namespace

LangBarItem::LangBarItem(LangBarHost &host, const CLSID &serviceClsid) : ref_(1), host_(host), serviceClsid_(serviceClsid) {}

STDMETHODIMP LangBarItem::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv)
        return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) || IsEqualIID(riid, IID_ITfLangBarItemButton)) {
        *ppv = static_cast<ITfLangBarItemButton *>(this);
    } else if (IsEqualIID(riid, IID_ITfSource)) {
        /* language-bar.md: the item MUST expose ITfSource - when the item is
         * installed the language bar calls the item's ITfSource::AdviseSink
         * with IID_ITfLangBarItemSink to install its change-notification
         * sink. */
        *ppv = static_cast<ITfSource *>(this);
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG)
LangBarItem::AddRef() { return (ULONG)InterlockedIncrement(&ref_); }

STDMETHODIMP_(ULONG)
LangBarItem::Release() {
    ULONG r = (ULONG)InterlockedDecrement(&ref_);
    if (r == 0)
        delete this;
    return r;
}

STDMETHODIMP LangBarItem::GetInfo(TF_LANGBARITEMINFO *pInfo) {
    if (!pInfo)
        return E_INVALIDARG;
    ZeroMemory(pInfo, sizeof *pInfo);
    pInfo->clsidService = serviceClsid_;
    /* guidItem MUST be GUID_LBI_INPUTMODE: the Win8+/Win11 "input
     * indicator" (the per-TIP badge hosted in the taskbar tray) hosts a
     * langbar item only when this is its guidItem (measured sibling
     * project: any other item GUID is ignored by the shell's hosted-item
     * path). The older private GUID F2C70003 never got past AddItem. */
    pInfo->guidItem = HunminLbiInputModeGuid;
    /* BTN_MENU | SHOWNINTRAY - the ruled shape (maintainer instruction
     * 2026-10-22: no BTN_BUTTON). Measured 2026-10-23: adding BTN_BUTTON
     * did NOT make the Win11 tray indicator route its click here either
     * (left-click opens the shell's language list, right-click dismisses
     * it, and neither OnClick nor InitMenu is called), so the extra bit
     * bought nothing and the ruling stands. A host that routes the
     * menu-button shape calls InitMenu/OnMenuSelect (language-bar.md);
     * OnClick stays implemented but is not advertised. */
    pInfo->dwStyle = TF_LBI_STYLE_BTN_MENU | TF_LBI_STYLE_SHOWNINTRAY;
    pInfo->ulSort = 0;
    lstrcpynW(pInfo->szDescription, L"Hunmin \uC124\uC815", MAX_PATH);
    /* The shape the bar judged, logged where the bar reads it: a live round
     * can then tell a refused item (AddItem hr) from an accepted item the
     * indicator never asks about (no GetIcon/GetText afterwards). */
    hunmin_logger.log("langbar GetInfo style=0x%X guid=%08lX\n", (unsigned)pInfo->dwStyle, (unsigned long)pInfo->guidItem.Data1);
    return S_OK;
}

STDMETHODIMP LangBarItem::GetStatus(DWORD *pdwStatus) {
    if (!pdwStatus)
        return E_INVALIDARG;
    *pdwStatus = 0;
    return S_OK;
}

STDMETHODIMP LangBarItem::Show(BOOL fShow) {
    (void)fShow;
    return S_OK;
}

STDMETHODIMP LangBarItem::GetTooltipString(BSTR *pbstrToolTip) {
    if (!pbstrToolTip)
        return E_INVALIDARG;
    *pbstrToolTip = SysAllocString(L"Hunmin \uC124\uC815 \uC5F4\uAE30");
    return *pbstrToolTip ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP LangBarItem::OnClick(TfLBIClick click, POINT pt, const RECT *prcArea) {
    (void)prcArea;
    hunmin_logger.log("langbar OnClick click=%d\n", (int)click);
    if (click == TF_LBI_CLK_RIGHT) {
        /* The right-click CONFIG MENU (issue #103, the ruled langbar
         * surface). Measured sibling shape (BTN_BUTTON items): with
         * BTN_BUTTON, a right click arrives as OnClick(CLK_RIGHT) and
         * the bar's InitMenu/OnMenuSelect never fire - so the item
         * builds its own popup here, like the reference TIPs do.
         * Owner = the focused window (the indicator does not hand out
         * one); TPM_RETURNCMD|TPM_NONOTIFY resolve the choice from the
         * return value, no WM_COMMAND. */
        HMENU menu = CreatePopupMenu();
        if (menu) {
            const wchar_t *label = L"Hunmin \uC124\uC815 \uC5F4\uAE30";
            AppendMenuW(menu, MF_STRING, 1, label);
            POINT p = pt;
            HMONITOR mon = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
            if (mon) {
                MONITORINFO mi;
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(mon, &mi)) {
                    if (p.x < mi.rcWork.left)
                        p.x = mi.rcWork.left;
                    if (p.x > mi.rcWork.right)
                        p.x = mi.rcWork.right;
                }
            }
            HWND owner = GetFocus();
            if (!owner)
                owner = GetForegroundWindow();
            int cmd =
                (int)TrackPopupMenu(menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, owner, nullptr);
            DestroyMenu(menu);
            if (cmd == 1)
                return hunmin_tsf_launch_settings();
        }
        return S_OK;
    }
    (void)pt;
    /* Left/other-click action, kept for a host that routes a button
     * click; the ruled style advertises no BTN_BUTTON. The settings launch
     * is a DEDICATED PROCESS, not a modal loop on this thread: the TIP
     * thread must not pump a nested message loop while TSF holds edit
     * cookies. */
    return hunmin_tsf_launch_settings();
}

STDMETHODIMP LangBarItem::InitMenu(ITfMenu *pMenu) {
    /* The menu-button popup (language-bar.md: InitMenu is called for a
     * BTN_MENU item; OnMenuSelect reports the choice): one entry,
     * "<Hunmin settings>" (id = 1). */
    hunmin_logger.log("langbar InitMenu\n");
    if (!pMenu)
        return E_FAIL;
    const wchar_t *label = L"Hunmin \uC124\uC815 \uC5F4\uAE30";
    return pMenu->AddMenuItem(1, 0, nullptr, nullptr, label, (ULONG)wcslen(label), nullptr);
}

STDMETHODIMP LangBarItem::OnMenuSelect(UINT wID) {
    hunmin_logger.log("langbar OnMenuSelect id=%u\n", (unsigned)wID);
    if (wID != 1)
        return S_OK;
    return hunmin_tsf_launch_settings();
}

STDMETHODIMP LangBarItem::AdviseSink(REFIID riid, IUnknown *punk, DWORD *pdwCookie) {
    if (!punk || !pdwCookie)
        return E_INVALIDARG;
    *pdwCookie = 0;
    /* THE hosting observable (language-bar.md): "when the item is installed,
     * the language bar installs an ITfLangBarItemSink sink by calling the
     * item's ITfSource::AdviseSink". A round that sees this line has a bar
     * that HOSTS the item; a round that does not has only AddItem's answer. */
    hunmin_logger.log("langbar AdviseSink riid=%08lX\n", (unsigned long)riid.Data1);
    if (!IsEqualIID(riid, IID_ITfLangBarItemSink))
        return CONNECT_E_CANNOTCONNECT;
    if (sink_)
        return CONNECT_E_ADVISELIMIT;
    ITfLangBarItemSink *sink = nullptr;
    if (FAILED(punk->QueryInterface(IID_ITfLangBarItemSink, (void **)&sink)) || !sink)
        return CONNECT_E_CANNOTCONNECT;
    sink_ = sink;
    sinkCookie_ = 1;
    *pdwCookie = sinkCookie_;
    return S_OK;
}

STDMETHODIMP LangBarItem::UnadviseSink(DWORD dwCookie) {
    if (dwCookie == sinkCookie_ && sink_) {
        sink_->Release();
        sink_ = nullptr;
        sinkCookie_ = 0;
        return S_OK;
    }
    return CONNECT_E_NOCONNECTION;
}

STDMETHODIMP LangBarItem::GetIcon(HICON *phIcon) {
    if (!phIcon)
        return E_INVALIDARG;
    /* The input MODE badge (discover: the user asked for the current
     * input mode in the bar): unit - red Hangul badge when the
     * hangul side is active, Latin 'A' badge in english mode; the
     * multi-size app icon (resource 1) remains the service's own
     * identity in the language list. LoadIconW cached-handle rules
     * apply; nothing to free. */
    /* GetIcon is only ever called BY a bar that renders the item: it is the
     * render-side observable, the sibling of GetInfo's AddItem answer. */
    HMODULE self = nullptr;
    const wchar_t *which = host_.HangulMode() ? MAKEINTRESOURCEW(2) : MAKEINTRESOURCEW(3);
    hunmin_logger.log("langbar GetIcon hangul=%d res=%d\n", host_.HangulMode() ? 1 : 0, (int)(INT_PTR)which);
    /* The module anchor must be a CODE address inside this DLL:
     * GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS resolves an address's module, and
     * this object lives on the heap, so anchoring on `this` made the call fail
     * every time and every icon fell through to IDI_APPLICATION -- measured
     * 2026-10-29 (issue #68): the tray entry showed the Windows default icon
     * whatever the mode. Same shape as log.h's PathFor anchor. */
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)(const void *)&LangBarModuleAnchor, &self)) {
        *phIcon = LoadIconW(self, which);
        if (*phIcon)
            return S_OK;
        /* The mode badges (2/3) are non-critical: fall through to the
         * app icon when the mirroring rc did not compile them. */
        *phIcon = LoadIconW(self, MAKEINTRESOURCEW(1));
        if (*phIcon)
            return S_OK;
    }
    *phIcon = LoadIconW((HMODULE)NULL, IDI_APPLICATION);
    return *phIcon ? S_OK : E_FAIL;
}

STDMETHODIMP LangBarItem::GetText(BSTR *pbstrText) {
    if (!pbstrText)
        return E_INVALIDARG;
    /* the MODE as TEXT (the bar renders this on text-capable bars):
     * Hangul on hangul side / Latin on the latin side */
    *pbstrText = SysAllocString(host_.HangulMode() ? L"\uD55C" : L"A");
    hunmin_logger.log("langbar GetText hangul=%d\n", host_.HangulMode() ? 1 : 0);
    return *pbstrText ? S_OK : E_OUTOFMEMORY;
}

void LangBarItem::Update() {
    if (sink_)
        sink_->OnUpdate(TF_LBI_ICON | TF_LBI_TEXT);
}

bool LangBarItem::Attach(ITfLangBarItemMgr *plbm) {
    if (!plbm)
        return false;
    const HRESULT ai = plbm->AddItem(this);
    if (SUCCEEDED(ai))
        attached_ = true;
    hunmin_logger.log("langbar AddItem hr=0x%lX\n", (unsigned long)ai);
    return attached_;
}

void LangBarItem::Detach(ITfLangBarItemMgr *plbm) {
    /* Logged before the guards: a live round has to tell "the detach was never
     * called", "called with no manager" (#145's defect, before the fix) and
     * "RemoveItem refused" apart, and only the first is visible from the
     * caller. */
    hunmin_logger.log("langbar detach attached=%d mgr=%p\n", attached_ ? 1 : 0, (void *)plbm);
    if (!attached_ || !plbm)
        return;
    const HRESULT ri = plbm->RemoveItem(this);
    hunmin_logger.log("langbar RemoveItem hr=0x%lX\n", (unsigned long)ri);
    attached_ = false;
    OnRemoved();
}

void LangBarItem::OnRemoved() {
    /* The kernel-provided sink goes with the item (language-bar.md:
     * the sink lives between AddItem and RemoveItem). */
    if (sink_) {
        sink_->Release();
        sink_ = nullptr;
    }
    sinkCookie_ = 0;
}

LangBarItem *hunmin_tsf_langbar_new(LangBarHost &host, const CLSID &serviceClsid) { return new LangBarItem(host, serviceClsid); }
