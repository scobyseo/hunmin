/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The adapter's OWN tray badge (issue #103): the mode indicator (red "han"
 * on hangul side, dark "A" on latin — resources 2/3 of the TIP DLL) plus
 * the right-click "settings" entry, on a surface we own.
 *
 * Why this surface exists (measured, tests/manual-checklists/tsf-langbar-
 * badge.md): every system lane is closed on Windows 11 25H2 (26200) — the
 * langbar item's AddItem is refused (0x80004005, environmental), the
 * ITfSystemLangBarItemSink advice settles but the shell tray menu never
 * renders it, and the shell ignores a third-party TIP's conversion
 * compartment when drawing its badge. So the indicator travels on an icon
 * WE add ourselves (Shell_NotifyIconW) beside the clock, swapped on every
 * mode boundary; right-click shows the one-entry menu "Hunmin settings"
 * (the same label and launch target every other settings affordance uses)
 * and left-click toggles the mode through the callbacks the adapter
 * registers in Activate.
 *
 * One icon per process, and only the foreground process's: the shell
 * displays every Shell_NotifyIcon it is handed, a TIP is loaded into every
 * text-input process, and nothing deactivates a background process's TIP —
 * without a gate, badges would pile up one per host. The gate is
 * WM_ACTIVATEAPP on our own message window plus a foreground-process check
 * at creation: activate this process and the badge is (re-)added, take the
 * foreground away and it is removed. Deactivate removes it unconditionally.
 *
 * Harness gate: the tier-F1 mock mains define HUNMIN_TSF_TRAY_DISABLED
 * before including src/tsf.cpp, which includes this file — a console test
 * process must not create a real tray icon. tsf.cpp includes this file
 * (like src/keybinding.cpp) so the seam and the adapter share one TU.
 *
 * Threads: the functions must run on the TIP thread (Activate, key events,
 * Deactivate); the owner window's WndProc then runs on that same thread,
 * and the callbacks it fires (mode toggle, settings launch) never race the
 * adapter state they touch.
 */
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")

/* GitWindows/MinGW def modes: this file has no library manifest concerns —
 * it is compiled into hunmin-tsf.dll and the tier-F1 test exes, all MSVC or
 * llvm-mingw, and shell32 is a system import. Comment style per repo other
 * modules; the class of window (tool window, no taskbar entry) is hidden on
 * purpose and never shown. */

struct hunmin_tsf_tray_callbacks {
    void (*toggle)(void); /* left click on the badge */
    void (*open_settings)(void); /* the menu entry */
};

#ifndef HUNMIN_TSF_TRAY_DISABLED

namespace {

const UINT kTrayCallbackMsg = WM_APP + 0x68; /* one window, one callback msg */
const UINT kTrayIconUid = 1;

struct TrayState {
    HWND wnd = nullptr;
    NOTIFYICONDATAW nid = {};
    bool wnd_class_ready = false;
    bool added = false;
    bool promoted = false; /* the Win11 visible-in-taskbar write landed */
    HICON icon = nullptr;
    int hangul = 1; /* last requested mode; re-applied on WM_ACTIVATEAPP */
    hunmin_tsf_tray_callbacks cb = {nullptr, nullptr};
};
TrayState g_tray;

HMODULE TrayModule() {
    HMODULE m = GetModuleHandleW(L"hunmin-tsf.dll");
    if (!m)
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)(const void *)&g_tray, &m);
    return m;
}

HICON TrayIconFor(int hangul) {
    HMODULE m = TrayModule();
    if (!m)
        return nullptr;
    /* Small-icon metrics: the tray scales down anyway, but a 16-px class
     * icon avoids the blur a full-size HICON pick-up produces. */
    UINT cx = GetSystemMetrics(SM_CXSMICON);
    UINT cy = GetSystemMetrics(SM_CYSMICON);
    return (HICON)LoadImageW(m, MAKEINTRESOURCEW(hangul ? 2 : 3), IMAGE_ICON,
                             cx, cy, LR_DEFAULTCOLOR);
}

/* Win11's tray settings store (HKCU\Control Panel\NotifyIconSettings): one
 * subkey per icon-owning EXECUTABLE, `ExecutablePath` names it and
 * `IsPromotedDWORD` = 1 is the "visible in the taskbar" state the settings
 * page edits. An icon added by a process the user never promoted lands
 * HIDDEN in the overflow (the installer round's defect report), so the
 * badge promotes itself here, best effort: an older Windows without the
 * key, an access failure, or no matching entry yet are all logged and
 * retried on the next successful NIM_ADD, never fatal. The icon's owner is
 * the HOST process (ime-probe, wordpad, ...), not our DLL. */
bool TrayPromote(void) {
    wchar_t host[MAX_PATH];
    if (GetModuleFileNameW(nullptr, host, MAX_PATH) == 0)
        return false;
    HKEY root = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\NotifyIconSettings",
                      0, KEY_READ | KEY_WRITE, &root) != ERROR_SUCCESS)
        return false;
    bool ok = false;
    wchar_t name[128];
    for (DWORD i = 0;; ++i) {
        DWORD len = _countof(name);
        FILETIME ft;
        if (RegEnumKeyExW(root, i, name, &len, nullptr, nullptr, nullptr,
                          &ft) != ERROR_SUCCESS)
            break;
        HKEY k = nullptr;
        if (RegOpenKeyExW(root, name, 0, KEY_READ | KEY_WRITE, &k) != ERROR_SUCCESS)
            continue;
        wchar_t exe[MAX_PATH];
        DWORD sz = sizeof(exe);
        if (RegGetValueW(k, nullptr, L"ExecutablePath", RRF_RT_REG_SZ, nullptr,
                         exe, &sz) == ERROR_SUCCESS &&
            _wcsicmp(exe, host) == 0) {
            DWORD one = 1;
            if (RegSetValueExW(k, L"IsPromotedDWORD", 0, REG_DWORD,
                               (const BYTE *)&one, sizeof(one)) == ERROR_SUCCESS)
                ok = true;
        }
        RegCloseKey(k);
        if (ok)
            break;
    }
    RegCloseKey(root);
    return ok;
}

void TrayRemove(void) {
    if (g_tray.added) {
        if (g_tray.wnd)
            Shell_NotifyIconW(NIM_DELETE, &g_tray.nid);
        g_tray.added = false;
    }
}

bool IsForegroundProcess(void) {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (fg && GetWindowThreadProcessId(fg, &pid))
        return pid == GetCurrentProcessId();
    return false;
}

bool TrayAdd(void) {
    if (!g_tray.wnd || g_tray.added)
        return g_tray.wnd != nullptr;
    ZeroMemory(&g_tray.nid, sizeof(g_tray.nid));
    g_tray.nid.cbSize = sizeof(g_tray.nid);
    g_tray.nid.hWnd = g_tray.wnd;
    g_tray.nid.uID = kTrayIconUid;
    g_tray.nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.nid.uCallbackMessage = kTrayCallbackMsg;
    g_tray.nid.hIcon = g_tray.icon;
    lstrcpynW(g_tray.nid.szTip, L"Hunmin", _countof(g_tray.nid.szTip));
    if (!Shell_NotifyIconW(NIM_ADD, &g_tray.nid))
        return false;
    g_tray.added = true;
    /* Visible in the taskbar, not the overflow (the installer round's defect
     * report): the promotion write may also miss while the shell has not yet
     * created the entry for this host executable — retried on the next
     * add/update while unconfirmed. */
    if (!g_tray.promoted) {
        g_tray.promoted = TrayPromote();
        HMLOG("tray promote ok=%d\r\n", (int)g_tray.promoted);
    }
    return true;
}

void TrayShowChecked(void) {
    /* The visibility rule (one icon per process, foreground only):
     * initial Add while our process is not the foreground app is skipped —
     * the next WM_ACTIVATEAPP(TRUE) adds it, WM_ACTIVATEAPP(FALSE) removes. */
    if (!IsForegroundProcess()) {
        if (g_tray.added)
            TrayRemove();
        return;
    }
    if (!g_tray.added && !TrayAdd())
        return;
    /* The swap case (icon just replaced) rides the same MODIFY. */
    Shell_NotifyIconW(NIM_MODIFY, &g_tray.nid);
}

LRESULT CALLBACK TrayWndProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == kTrayCallbackMsg) {
        /* Classic (version 0) icon routing: the LOWORD of lParam is the
         * mouse message. The single-entry menu never needs WM_CONTEXTMENU. */
        switch ((UINT)lparam & 0xFFFF) {
        case WM_LBUTTONUP: {
            /* The toggle must apply to the edit the user was typing in (the
             * installer round's defect: the flip happened but the keystrokes
             * after the click landed nowhere, because the tray click hands
             * the foreground away). Remember the foreground window, flip,
             * and set it back — the tray host (Shell_TrayWnd) itself is not
             * a restore target. */
            HWND prev = GetForegroundWindow();
            wchar_t cls[64];
            bool prevIsTray = prev && GetClassNameW(prev, cls, 64) > 0 &&
                              wcscmp(cls, L"Shell_TrayWnd") == 0;
            if (g_tray.cb.toggle)
                g_tray.cb.toggle();
            if (prev && !prevIsTray && !SetForegroundWindow(prev))
                HMLOG("tray focus restore FAILED err=%lu\r\n", GetLastError());
            return 0;
        }
        case WM_RBUTTONUP: {
            /* The owner window takes the foreground for the menu's lifetime
             * (the Shell_NotifyIcon menu rule); the click lands through
             * TPM_RETURNCMD, no WM_COMMAND, no notification loop. */
            SetForegroundWindow(wnd);
            POINT pt;
            GetCursorPos(&pt);
            HMENU menu = CreatePopupMenu();
            if (!menu)
                return 0;
            const wchar_t *label = L"Hunmin \uC124\uC815 \uC5F4\uAE30";
            AppendMenuW(menu, MF_STRING, 1, label);
            int cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                     pt.x, pt.y, 0, wnd, nullptr);
            DestroyMenu(menu);
            PostMessageW(wnd, WM_NULL, 0, 0); /* the menu rule's settle beat */
            if (cmd == 1 && g_tray.cb.open_settings)
                g_tray.cb.open_settings();
            return 0;
        }
        default:
            return 0;
        }
    }
    if (msg == WM_ACTIVATEAPP) {
        if (wparam) {
            if (!g_tray.added)
                TrayAdd();
        } else {
            TrayRemove();
        }
        return 0;
    }
    if (msg == WM_DESTROY)
        TrayRemove();
    return DefWindowProcW(wnd, msg, wparam, lparam);
}

bool TrayEnsureWindow(void) {
    if (g_tray.wnd)
        return true;
    if (!g_tray.wnd_class_ready) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = TrayWndProc;
        wc.hInstance = TrayModule();
        wc.lpszClassName = L"HunminTsfTrayOwner";
        if (!RegisterClassW(&wc))
            return false;
        g_tray.wnd_class_ready = true;
    }
    /* WS_POPUP (never shown) + a tool-window look: the class is registered
     * without style extras, the window hidden, so nothing lands in the
     * alt-tab / taskbar space. */
    g_tray.wnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"HunminTsfTrayOwner",
                                 L"Hunmin", WS_POPUP, 0, 0, 0, 0, nullptr,
                                 nullptr, TrayModule(), nullptr);
    return g_tray.wnd != nullptr;
}

} /* namespace */

void hunmin_tsf_tray_set_callbacks(const hunmin_tsf_tray_callbacks *cbs) {
    if (cbs)
        g_tray.cb = *cbs;
}

void hunmin_tsf_tray_update(int hangul) {
    g_tray.hangul = hangul ? 1 : 0;
    if (!TrayEnsureWindow())
        return;
    HICON next = TrayIconFor(g_tray.hangul);
    if (!next)
        next = TrayIconFor(1); /* resource 1, the Sejong profile icon: the
                                * badge must never go dark on a stripped rc */
    if (!next)
        return;
    if (g_tray.icon && g_tray.icon != next)
        DestroyIcon(g_tray.icon);
    g_tray.icon = next;
    /* LoadImageW results are NOT shared handles; the swap above owns one and
     * the DestroyIcon half of the swap ran when it was replaced. */
    if (g_tray.added) {
        g_tray.nid.hIcon = g_tray.icon;
        Shell_NotifyIconW(NIM_MODIFY, &g_tray.nid);
        /* The promotion retry lane: the shell's settings entry for this
         * host executable may only exist by now (created after NIM_ADD). */
        if (!g_tray.promoted) {
            g_tray.promoted = TrayPromote();
            HMLOG("tray promote retry ok=%d\r\n", (int)g_tray.promoted);
        }
    } else {
        TrayShowChecked();
    }
}

void hunmin_tsf_tray_hide(void) { TrayRemove(); }

#else /* HUNMIN_TSF_TRAY_DISABLED: the console harness's no-op twin */

void hunmin_tsf_tray_set_callbacks(const hunmin_tsf_tray_callbacks *cbs) {
    (void)cbs;
}
void hunmin_tsf_tray_update(int hangul) { (void)hangul; }
void hunmin_tsf_tray_hide(void) {}

#endif /* HUNMIN_TSF_TRAY_DISABLED */
