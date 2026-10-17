/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The minimal settings dialog (issue #71). Plain Win32, no frameworks, no
 * .rc resource: the dialog template is built in memory so the whole surface
 * stays code and the MSVC-only tier keeps building without an RC step.
 *
 * Rows (issue #71 requirement list):
 *   - hangul layout list + latin layout list, split by
 *     hunmin_ctx_layout_mode ("hangul"/"latin"), display name from
 *     hunmin_ctx_layout_name(ctx, i, "ko"), engine support from
 *     hunmin_ctx_layout_supported;
 *   - the mode-toggle key list row, editing the HangulEngKeys
 *     "v1:<vk>:<mods>;..." grammar (keybinding.h);
 *   - a feature row for the automatic Han/Eng switch, DISABLED and labelled
 *     with the recorded ruling's marker - display only, nothing is wired.
 */
#include "settings.h"

#include <ctffunc.h>
#include <msctf.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "config.h"

extern bool hunmin_tsf_log_path(const char *fname, char *out, size_t cap);

/* Control ids (the in-memory template declares the same ids). */
#define IDC_HANGUL_LIST 1001
#define IDC_LATIN_LIST 1002
#define IDC_TOGGLE_EDIT 1003
#define IDC_FEATURE_ROW 1004
#define IDC_HINT_TOGGLE 2001

/* The provider's own type GUID (ITfFunctionProvider::GetType): fresh, not
 * the TIP CLSID {F2C70001-...-0001} or the profile {F2C70002-...-0002}. */
static const GUID kProviderTypeGuid = {0xF2C70010, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10}};

static void logLine(const char *fmt, ...) {
    /* Debug log: same gate and file as the adapter's HMLOG (tsf.cpp), gated
     * by HUNMIN_TSF_DEBUG_LOG; the two TUs stay independent, so the gate
     * check is repeated rather than shared. */
    char v[16];
    DWORD n = GetEnvironmentVariableA("HUNMIN_TSF_DEBUG_LOG", v, (DWORD)sizeof(v));
    if (n == 0 || n >= sizeof(v))
        return;
    char path[MAX_PATH];
    if (!hunmin_tsf_log_path("hunmin-tsf-debug.log", path, (size_t)sizeof(path)))
        return;
    FILE *f = fopen(path, "a");
    if (!f)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fflush(f);
    fclose(f);
}

/* ---- the pure half: what Apply writes, what the rows contain ---- */

int hunmin_tsf_rows_for_mode(hunmin_ctx *ctx, bool hangul, HunminTsfLayoutRow *rows,
                             int cap) {
    int n = 0;
    if (ctx) {
        const int count = hunmin_ctx_layout_count(ctx);
        for (int i = 0; i < count && n < cap; ++i) {
            const char *mode = hunmin_ctx_layout_mode(ctx, i);
            if (!mode)
                continue;
            if (hangul ? strcmp(mode, "hangul") != 0 : strcmp(mode, "latin") != 0)
                continue;
            const char *id = hunmin_ctx_layout_id(ctx, i);
            if (!id || !id[0])
                continue;
            rows[n].index = i;
            strncpy(rows[n].id, id, HUNMIN_TSF_SETTINGS_TEXT_CAP - 1);
            rows[n].id[HUNMIN_TSF_SETTINGS_TEXT_CAP - 1] = '\0';
            const char *name = hunmin_ctx_layout_name(ctx, i, "ko");
            strncpy(rows[n].name, (name && name[0]) ? name : id,
                    HUNMIN_TSF_SETTINGS_TEXT_CAP - 1);
            rows[n].name[HUNMIN_TSF_SETTINGS_TEXT_CAP - 1] = '\0';
            rows[n].supported = hunmin_ctx_layout_supported(ctx, i) ? 1 : 0;
            ++n;
        }
    }
    return n;
}

/* UTF-8 (the data files' encoding) to wide, for a Win32 control. */
static std::wstring utf8ToWide(const char *s) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (wlen <= 1)
        return std::wstring();
    std::wstring out((size_t)wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &out[0], wlen);
    return out;
}

/* Wide control text to the store's byte string: the grammar ("v1:...") is
 * pure ASCII, so CP_ACP round-trips everything the dialog can produce. */
static std::string wideToStore(const std::wstring &w) {
    if (w.empty())
        return std::string();
    int alen = WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), nullptr, 0,
                                   nullptr, nullptr);
    std::string out;
    if (alen > 0) {
        out.resize((size_t)alen);
        WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), &out[0], alen,
                            nullptr, nullptr);
    }
    return out;
}

/* ---- the modal dialog, template built in memory ---- */

struct SettingsState {
    hunmin_ctx *ctx = nullptr;
    HunminTsfConfig cfg = {};
    std::vector<HunminTsfLayoutRow> hangulRows;
    std::vector<HunminTsfLayoutRow> latinRows;
    int hangulSel = -1; /* row index preselected; vector index == combo index */
    int latinSel = -1;
    /* The ids, in display order: the OK handler needs the id the user
     * picked, and CBS without sort keeps the added order. */
    bool applied = false;
    /* Windows 11 look-and-feel state (measured HRESULTs land in the log). */
    bool dark = false;
    HFONT font = nullptr;        /* the Segoe UI Variable upgrade, owned */
    HBRUSH editBrush = nullptr;  /* dark edit-field background */
    HBRUSH darkBgBrush = nullptr; /* dark text/label paint support */
    HRESULT hrCorner = E_FAIL;
    HRESULT hrDark = E_FAIL;
    HRESULT hrExtend = E_FAIL;
    HRESULT hrBackdrop = E_FAIL;
};

/* In-memory dialog templates are a WORD stream: DWORD alignment == even
 * WORD index. Writers below; each item's fields exactly as the DLGTEMPLATE
 * / DLGITEMTEMPLATE structures in the Windows SDK lay out. */
static void w32(std::vector<WORD> &t, DWORD v) {
    t.push_back((WORD)(v & 0xFFFF));
    t.push_back((WORD)(v >> 16));
}
static void wstr(std::vector<WORD> &t, const wchar_t *s) {
    while (*s)
        t.push_back((WORD)*s++);
    t.push_back(0);
}
static void walign(std::vector<WORD> &t) {
    if (t.size() & 1)
        t.push_back(0);
}

/* 0xFFFF-escaped predefined class atoms (winuser.h CreateDialogIndirect
 * note): Button 0x80, Edit 0x81, Static 0x82, ComboBox 0x85. */
#define ATOM_BUTTON 0x80
#define ATOM_EDIT 0x81
#define ATOM_STATIC 0x82
#define ATOM_COMBOBOX 0x85

static void addItem(std::vector<WORD> &t, DWORD style, DWORD exStyle, WORD x, WORD y,
                    WORD cx, WORD cy, WORD id, WORD atom, const wchar_t *title) {
    walign(t);
    w32(t, style);
    w32(t, exStyle);
    t.push_back(x);
    t.push_back(y);
    t.push_back(cx);
    t.push_back(cy);
    t.push_back(id);
    t.push_back(0xFFFF);
    t.push_back(atom);
    wstr(t, title);
    t.push_back(0); /* cdData: no creation data */
}

/* The dialog: 216x164 dialog units. Korean strings use L'\uXXXX' escapes
 * because MSVC reads this TU under the ANSI codepage (949). */
#define TITLE_HANGUL L"\uD55C\uAE00 \uC790\uD310" /* hangul layout */
#define TITLE_LATIN L"\uC601\uBB38 \uC790\uD310" /* latin layout */
#define TITLE_TOGGLE L"\uD55C/\uC605 \uC804\uD658 \uD0A4 \uBAA9\uB85D" /* toggle key list */
#define HINT_TOGGLE L"\uC608: v1:165:0;v1:32:1" /* e.g. */
#define FEATURE_ROW L"\uC790\uB3D9 \uD55C/\uC605 \uC804\uD658: \uBBF8\uC815 (0.3)"

static std::vector<WORD> buildDialogTemplate() {
    std::vector<WORD> t;
    /* No WS_VISIBLE here, and DS_SETFONT is REQUIRED because the font
     * block is written below: without the flag USER32 parses the size and
     * face words as the first item's bytes and every item (or the process)
     * breaks — measured on this host via a probe that varied the style
     * bits. DS_CENTER centres the ownerless launcher window. */
    w32(t, DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    w32(t, 0); /* exStyle */
    const size_t cditOff = t.size();
    t.push_back(0); /* cdit, patched below */
    t.push_back(0); /* x */
    t.push_back(0); /* y */
    t.push_back(216);
    t.push_back(164);
    t.push_back(0); /* menu: none */
    t.push_back(0); /* class: standard dialog */
    wstr(t, L"Hunmin settings");
    t.push_back(9); /* DS_SETFONT needs this size word, then the face */
    wstr(t, L"Segoe UI");
    const size_t first = t.size();
    /* Row labels. */
    addItem(t, WS_CHILD | WS_VISIBLE | WS_GROUP, 0, 10, 8, 90, 10, 0, ATOM_STATIC,
            TITLE_HANGUL);
    addItem(t, WS_CHILD | WS_VISIBLE, 0, 112, 8, 90, 10, 0, ATOM_STATIC, TITLE_LATIN);
    /* Layout combo boxes, one per slot. */
    addItem(t, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0,
            10, 20, 90, 60, IDC_HANGUL_LIST, ATOM_COMBOBOX, L"");
    addItem(t, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0,
            112, 20, 90, 60, IDC_LATIN_LIST, ATOM_COMBOBOX, L"");
    /* The mode-toggle key list row. */
    addItem(t, WS_CHILD | WS_VISIBLE, 0, 10, 88, 192, 10, 0, ATOM_STATIC, TITLE_TOGGLE);
    addItem(t, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0, 10,
            100, 192, 14, IDC_TOGGLE_EDIT, ATOM_EDIT, L"");
    addItem(t, WS_CHILD | WS_VISIBLE, 0, 10, 118, 192, 10, IDC_HINT_TOGGLE, ATOM_STATIC,
            HINT_TOGGLE);
    /* The feature row: display only, disabled, per the recorded ruling. */
    addItem(t, WS_CHILD | WS_VISIBLE, 0, 10, 132, 134, 10, 0, ATOM_STATIC, FEATURE_ROW);
    addItem(t, WS_CHILD | WS_VISIBLE | BS_AUTO3STATE | WS_DISABLED, 0, 148, 130, 10,
            12, IDC_FEATURE_ROW, ATOM_BUTTON, L"");
    /* OK / Cancel. */
    addItem(t, WS_CHILD | WS_VISIBLE | WS_GROUP | WS_TABSTOP | BS_DEFPUSHBUTTON, 0,
            104, 148, 48, 14, IDOK, ATOM_BUTTON, L"OK");
    addItem(t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 158, 148, 48,
            14, IDCANCEL, ATOM_BUTTON, L"Cancel");
    /* The template declared exactly the items appended above: 5 statics +
     * 2 combos + 1 edit + 1 checkbox + 2 buttons. */
    t[cditOff] = 11;
    return t;
}

static std::wstring getEditText(HWND h, int id) {
    HWND c = GetDlgItem(h, id);
    int len = GetWindowTextLengthW(c);
    std::wstring s((size_t)len + 1, L'\0');
    GetWindowTextW(c, &s[0], len + 1);
    s.resize((size_t)len);
    return s;
}

static void applyAndClose(HWND h, SettingsState *s) {
    /* Layout selections: the vector index == the combo item index (the
     * combo was filled in vector order without sort). */
    int sel = (int)SendMessageW(GetDlgItem(h, IDC_HANGUL_LIST), CB_GETCURSEL, 0, 0);
    if (sel >= 0 && (size_t)sel < s->hangulRows.size())
        strncpy(s->cfg.hangulLayout, s->hangulRows[(size_t)sel].id,
                HUNMIN_TSF_LAYOUT_ID_CAP - 1);
    sel = (int)SendMessageW(GetDlgItem(h, IDC_LATIN_LIST), CB_GETCURSEL, 0, 0);
    if (sel >= 0 && (size_t)sel < s->latinRows.size())
        strncpy(s->cfg.latinLayout, s->latinRows[(size_t)sel].id,
                HUNMIN_TSF_LAYOUT_ID_CAP - 1);
    /* Toggle-key list, wide control text -> the store grammar. Fixed-cap
     * never truncates: a too-long list is exactly the store save's failure
     * path, surfaced below. */
    std::string keys = wideToStore(getEditText(h, IDC_TOGGLE_EDIT));
    if (keys.size() >= HUNMIN_TSF_TOGGLE_KEYS_CAP) {
        MessageBoxW(h, L"Toggle key list too long.", L"Hunmin settings",
                    MB_OK | MB_ICONWARNING);
        return; /* stay on the dialog: the user fixes the value */
    }
    strncpy(s->cfg.toggleKeys, keys.c_str(), HUNMIN_TSF_TOGGLE_KEYS_CAP - 1);
    s->cfg.toggleKeys[HUNMIN_TSF_TOGGLE_KEYS_CAP - 1] = '\0';
    if (hunmin_tsf_config_save(HKEY_CURRENT_USER, &s->cfg)) {
        s->applied = true;
        logLine("settings: saved hangul=%s latin=%s keys=%s", s->cfg.hangulLayout,
                s->cfg.latinLayout, s->cfg.toggleKeys);
        EndDialog(h, 1);
    } else {
        logLine("settings: save FAILED");
        /* The user's explicit Apply must never silently do nothing. */
        MessageBoxW(h, L"Failed to write the settings store.", L"Hunmin settings",
                    MB_OK | MB_ICONWARNING);
    }
}

static void preselectCombo(HWND combo, const std::vector<HunminTsfLayoutRow> &rows,
                           const char *storeId) {
    for (size_t i = 0; i < rows.size(); ++i) {
        if (strcmp(rows[i].id, storeId) == 0) {
            SendMessageW(combo, CB_SETCURSEL, (WPARAM)i, 0);
            return;
        }
    }
}

/* ---- Windows 11 look-and-feel (maintainer instruction) ----
 * Every DWM call's HRESULT is logged: on an older host a failure is the
 * measured, graceful degradation, not a silent lie (the maintainers'
 * honesty rule and the standalone dialog still works without any of it).
 * No frameworks: plain dwmapi + uxtheme calls. */
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "UxTheme.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 3
#endif
#ifndef DWMSBT_MICA
#define DWMSBT_MICA 2
#endif

static bool systemAppsDark(void) {
    /* HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize:
     * AppsUseLightTheme 1/absent = light, 0 = dark (the dialog follows the
     * system APP theme, like the Settings app's own dark rendering). */
    HKEY k = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &k) != ERROR_SUCCESS)
        return false; /* absent key: treat as light (older hosts) */
    DWORD v = 1, type = 0, size = sizeof(v);
    RegQueryValueExW(k, L"AppsUseLightTheme", nullptr, &type, (BYTE *)&v, &size);
    RegCloseKey(k);
    return v == 0;
}

static bool fontFaceExists(const wchar_t *face) {
    struct Ctx {
        const wchar_t *face;
        bool found;
    } ctx = {face, false};
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    HDC dc = GetDC(nullptr);
    EnumFontFamiliesExW(dc, &lf,
                        [](const LOGFONTW *plf, const TEXTMETRICW *, DWORD, LPARAM lp) -> int {
                            auto *c = (Ctx *)lp;
                            if (_wcsicmp(plf->lfFaceName, c->face) == 0) {
                                c->found = true;
                                return 0;
                            }
                            return 1;
                        },
                        (LPARAM)&ctx, 0);
    ReleaseDC(nullptr, dc);
    return ctx.found;
}

static void setChildFont(HWND h, HFONT f) {
    for (HWND c = GetWindow(h, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
        SendMessageW(c, WM_SETFONT, (WPARAM)f, MAKELPARAM(TRUE, 0));
}

static void applyWindows11Look(HWND h, SettingsState *s) {
    s->dark = systemAppsDark();
    /* Theme constants for the control theming (attributes measured against
     * the 10.0.26100 SDK's dwmapi.h; the value written is what lands). */
    DWORD attr = 0;
    HRESULT hr = E_FAIL;
    /* dark mode: attribute 20 first, 19 the pre-22H2 constant */
    attr = s->dark ? 1 : 0;
    hr = DwmSetWindowAttribute(h, DWMWA_USE_IMMERSIVE_DARK_MODE, &attr, sizeof(attr));
    s->hrDark = hr;
    if (FAILED(hr))
        s->hrDark = DwmSetWindowAttribute(h, 19, &attr, sizeof(attr));
    /* rounded corners */
    attr = DWMWCP_ROUND;
    s->hrCorner = DwmSetWindowAttribute(h, DWMWA_WINDOW_CORNER_PREFERENCE, &attr,
                                        sizeof(attr));
    /* Mica backdrop: the frame must be stretched over the client first. */
    MARGINS m = {-1, -1, -1, -1};
    s->hrExtend = DwmExtendFrameIntoClientArea(h, &m);
    attr = DWMSBT_MICA;
    s->hrBackdrop = DwmSetWindowAttribute(h, DWMWA_SYSTEMBACKDROP_TYPE, &attr,
                                          sizeof(attr));
    /* The dark, Mica-backed surface needs matching control colors and the
     * dark control theme on the list/edit/checkbox controls. */
    if (s->dark) {
        const int ids[] = {IDC_HANGUL_LIST, IDC_LATIN_LIST, IDC_TOGGLE_EDIT,
                           IDC_FEATURE_ROW};
        for (int id : ids) {
            HWND c = GetDlgItem(h, id);
            if (c)
                SetWindowTheme(c, L"DarkMode_Explorer", nullptr);
        }
        s->editBrush = CreateSolidBrush(RGB(32, 32, 32));
        s->darkBgBrush = CreateSolidBrush(RGB(43, 43, 43)); /* no-Mica fallback fill */
    }
    /* Font: the template's Segoe UI is the fallback base; upgrade to the
     * Windows 11 Segoe UI Variable Display when it is installed. */
    HFONT base = (HFONT)SendMessageW(h, WM_GETFONT, 0, 0);
    LOGFONTW lf = {};
    if (base)
        GetObjectW(base, sizeof(lf), &lf);
    const wchar_t *want =
        fontFaceExists(L"Segoe UI Variable Display") ? L"Segoe UI Variable Display"
                                                     : L"Segoe UI";
    wchar_t have[LF_FACESIZE] = {0};
    wcscpy_s(have, lf.lfFaceName);
    if (_wcsicmp(have, want) != 0) {
        wcscpy_s(lf.lfFaceName, want);
        HFONT nf = CreateFontIndirectW(&lf);
        if (nf) {
            setChildFont(h, nf);
            s->font = nf; /* deleted on WM_DESTROY (dialog-owned) */
        }
    }
    logLine("settings w11: dark=%d corner=0x%lX darkAttr=0x%lX extend=0x%lX mica=0x%lX font=%ls",
            (int)s->dark, (unsigned long)s->hrCorner, (unsigned long)s->hrDark,
            (unsigned long)s->hrExtend, (unsigned long)s->hrBackdrop, want);
}

static INT_PTR CALLBACK settingsProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto *s = (SettingsState *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (msg == WM_INITDIALOG && s == nullptr) {
        s = (SettingsState *)lp;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)s);
        for (const HunminTsfLayoutRow &r : s->hangulRows) {
            std::wstring d = utf8ToWide(r.name);
            d += L" (" + utf8ToWide(r.id) + L")";
            SendMessageW(GetDlgItem(h, IDC_HANGUL_LIST), CB_ADDSTRING, 0,
                         (LPARAM)d.c_str());
        }
        for (const HunminTsfLayoutRow &r : s->latinRows) {
            std::wstring d = utf8ToWide(r.name);
            d += L" (" + utf8ToWide(r.id) + L")";
            SendMessageW(GetDlgItem(h, IDC_LATIN_LIST), CB_ADDSTRING, 0,
                         (LPARAM)d.c_str());
        }
        preselectCombo(GetDlgItem(h, IDC_HANGUL_LIST), s->hangulRows,
                       s->cfg.hangulLayout);
        preselectCombo(GetDlgItem(h, IDC_LATIN_LIST), s->latinRows, s->cfg.latinLayout);
        /* Toggle keys into the edit. */
        std::wstring w = utf8ToWide(s->cfg.toggleKeys);
        SetWindowTextW(GetDlgItem(h, IDC_TOGGLE_EDIT), w.c_str());
        /* Windows 11 look: corners, Mica, dark mode, Variable font. Logged
         * HRESULTs are the measurement (see the manual checklist). */
        applyWindows11Look(h, s);
        return TRUE;
    }
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            applyAndClose(h, s);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) {
            EndDialog(h, 0);
            return TRUE;
        }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        /* In dark mode the labels paint white over the Mica surface (no
         * background fill); light mode keeps the stock behavior. */
        if (s && s->dark) {
            HDC dc = (HDC)wp;
            SetTextColor(dc, RGB(255, 255, 255));
            SetBkMode(dc, TRANSPARENT);
            return (INT_PTR)GetStockObject(NULL_BRUSH);
        }
        break;
    case WM_CTLCOLOREDIT:
        if (s && s->dark && s->editBrush) {
            HDC dc = (HDC)wp;
            SetTextColor(dc, RGB(240, 240, 240));
            SetBkColor(dc, RGB(32, 32, 32));
            return (INT_PTR)s->editBrush;
        }
        break;
    case WM_ERASEBKGND:
        /* Mica succeeded: paint nothing, the backdrop shows through. Mica
         * failed (older host): fill dark so the window still renders as
         * dark-mode, degraded but honest. */
        if (s && s->dark) {
            if (SUCCEEDED(s->hrBackdrop))
                return 1;
            RECT rc = {};
            GetClientRect(h, &rc);
            FillRect((HDC)wp, &rc, s->darkBgBrush);
            return 1;
        }
        break;
    case WM_DESTROY:
        if (s && s->font)
            DeleteObject(s->font);
        if (s && s->editBrush)
            DeleteObject(s->editBrush);
        if (s && s->darkBgBrush)
            DeleteObject(s->darkBgBrush);
        return FALSE;
    case WM_CLOSE:
        EndDialog(h, 0);
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

/* One shared "edit the store" run: rows are filled from the engine data,
 * the current store preselects, Apply saves HKCU; returns TRUE when
 * applied. parent may be nullptr (the standalone rundll32 path has no
 * natural owner). */
static bool settingsDialogRun(HWND parent) {
    SettingsState s;
    s.ctx = hunmin_ctx_from_module();
    HunminTsfLayoutRow buf[HUNMIN_TSF_SETTINGS_MAX_ROWS];
    int n = hunmin_tsf_rows_for_mode(s.ctx, true, buf, HUNMIN_TSF_SETTINGS_MAX_ROWS);
    s.hangulRows.assign(buf, buf + n);
    n = hunmin_tsf_rows_for_mode(s.ctx, false, buf, HUNMIN_TSF_SETTINGS_MAX_ROWS);
    s.latinRows.assign(buf, buf + n);
    hunmin_tsf_config_load(HKEY_CURRENT_USER, &s.cfg);
    logLine("settings: open rows hangul=%zu latin=%zu", s.hangulRows.size(),
            s.latinRows.size());
    std::vector<WORD> t = buildDialogTemplate();
    /* The instance handle: from this TU's own address, not a module-name
     * lookup — the tier-F1 binary links this code and is not the TIP
     * module (tsf.cpp's GetModuleHandleW(L"hunmin-tsf.dll") trick does not
     * carry there). With a template pointer the instance is not deref'd,
     * but resolve it honestly anyway. */
    HMODULE hSelf = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)(const void *)&HunminSettingsShow, &hSelf);
    INT_PTR got = DialogBoxIndirectParamW((HINSTANCE)hSelf,
                                          (LPCDLGTEMPLATEW)t.data(), parent, settingsProc,
                                          (LPARAM)&s);
    if (got == -1)
        logLine("settings: DialogBoxIndirectParamW FAILED err=%lu", GetLastError());
    logLine("settings: dialog closed got=%ld applied=%d", (long)got, (int)s.applied);
    return s.applied;
}

/* ---- TSF-native plumbing ---- */

class HunminTsfConfigFn : public ITfFnConfigure {
  public:
    explicit HunminTsfConfigFn(IUnknown *svc) : ref_(1), svc_(svc) {
        if (svc_)
            svc_->AddRef();
    }
    virtual ~HunminTsfConfigFn() {
        if (svc_)
            svc_->Release();
    }
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfFunction) ||
            IsEqualIID(riid, IID_ITfFnConfigure)) {
            *ppv = static_cast<ITfFnConfigure *>(this);
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
    STDMETHOD(GetDisplayName)(BSTR *pbstrName) override {
        if (!pbstrName)
            return E_INVALIDARG;
        *pbstrName = SysAllocString(L"Hunmin \uC124\uC815 /* settings */");
        return *pbstrName ? S_OK : E_OUTOFMEMORY;
    }
    /* The contract the classic Text Services / property surfaces rely on:
     * do not return until the dialog closes. */
    STDMETHOD(Show)(HWND hwndParent, LANGID langid, REFGUID rguidProfile) override {
        (void)langid;
        (void)rguidProfile;
        logLine("ITfFnConfigure::Show pid=%lu", (unsigned long)GetCurrentProcessId());
        bool applied = settingsDialogRun(hwndParent);
        /* Live re-apply, issue #71 requirement: the running adapter re-reads
         * the store WITHOUT a process restart. Only when a live service is
         * attached underneath this function object. */
        if (applied && svc_)
            hunmin_tsf_apply_config_store(svc_);
        return S_OK;
    }

  private:
    LONG ref_;
    IUnknown *svc_;
};

class HunminTsfFunctionProvider : public ITfFunctionProvider {
  public:
    explicit HunminTsfFunctionProvider(IUnknown *svc) : ref_(1), svc_(svc) {
        if (svc_)
            svc_->AddRef();
    }
    virtual ~HunminTsfFunctionProvider() {
        if (svc_)
            svc_->Release();
    }
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfFunctionProvider)) {
            *ppv = static_cast<ITfFunctionProvider *>(this);
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
    STDMETHOD(GetType)(GUID *pguid) override {
        if (!pguid)
            return E_INVALIDARG;
        *pguid = kProviderTypeGuid;
        return S_OK;
    }
    STDMETHOD(GetDescription)(BSTR *pbstrDesc) override {
        if (!pbstrDesc)
            return E_INVALIDARG;
        *pbstrDesc = SysAllocString(L"Hunmin settings provider");
        return *pbstrDesc ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHOD(GetFunction)(REFGUID rguid, REFIID riid, IUnknown **out) override {
        (void)rguid;
        if (!out)
            return E_INVALIDARG;
        *out = nullptr;
        if (!IsEqualIID(riid, IID_ITfFnConfigure))
            return E_NOINTERFACE;
        HunminTsfConfigFn *fn = new HunminTsfConfigFn(svc_);
        HRESULT hr = fn->QueryInterface(riid, (void **)out);
        fn->Release();
        return hr;
    }

  private:
    LONG ref_;
    IUnknown *svc_;
};

IUnknown *hunmin_tsf_settings_new_provider(void *svc) {
    return new HunminTsfFunctionProvider((IUnknown *)svc);
}

/* Wndw note: settingsDialogRun is declared above; the standalone export is
 * a rundll32 signature and must stay C-linkage callable by name. */
void CALLBACK HunminSettingsShow(HWND hwnd, HINSTANCE hinst, LPSTR cmdLine,
                                 int nShow) {
    (void)hinst;
    (void)cmdLine;
    (void)nShow;
    logLine("HunminSettingsShow entry");
    settingsDialogRun(hwnd);
}
