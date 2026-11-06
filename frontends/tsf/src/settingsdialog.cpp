/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The interim plain-Win32 settings dialog (issue #71). Plain Win32, no
 * frameworks, no .rc resource: the dialog template is built in memory so
 * the whole surface stays code and the MSVC-only tier keeps building
 * without an RC step.
 *
 * Rows (issue #71 requirement list):
 *   - hangul layout list + latin layout list, split by
 *     hunmin_ctx_layout_mode ("hangul"/"latin"), display name from
 *     hunmin_ctx_layout_name(ctx, i, "ko"), engine support from
 *     hunmin_ctx_layout_supported;
 *   - the mode-toggle key list row, editing the HangulEngKeys
 *     "v1:<vk>:<mods>;..." grammar (keybinding.h);
 *   - the automatic Han/Eng switch (issue #175, R13-정정 surface B): a checkbox
 *     whose label, key name and enablement all come from the ENGINE
 *     (hunmin_tsf_autoswitch_row), so the dialog spells neither the value nor
 *     the key, and says why it is disabled when the dictionary is absent.
 */
#include "settingsdialog.h"

#include "config.h"
#include "log.h"
#include "settings.h"
#include "settingsrows.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <ctffunc.h>
#include <dwmapi.h>
#include <imm.h>
#include <msctf.h>
#include <string>
#include <vector>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "UxTheme.lib")

/* Control ids (the in-memory template declares the same ids). */
#define IDC_HANGUL_LIST 1001
#define IDC_LATIN_LIST 1002
#define IDC_TOGGLE_EDIT 1003
#define IDC_FEATURE_ROW 1004
#define IDC_HANJA_EDIT 1005
#define IDC_TOGGLE_CAPTURE 1006
#define IDC_HANJA_CAPTURE 1007
#define IDC_COMMIT_LIST 1008
#define IDC_HINT_HANJA 2002
#define IDC_HINT_TOGGLE 2001

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
    int alen = WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out;
    if (alen > 0) {
        out.resize((size_t)alen);
        WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), &out[0], alen, nullptr, nullptr);
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
    /* The automatic-switch row (issue #175), derived from the engine before the
     * dialog opens; the checkbox shows its label/state and Apply writes it. */
    HunminTsfAutoswitchRow featureRow = {};
    /* The ids, in display order: the OK handler needs the id the user
     * picked, and CBS without sort keeps the added order. */
    bool applied = false;
    /* Key-capture mode (WinUI 3 parity): 0 = idle, else the id of the edit
     * the next recorded chord fills (IDC_TOGGLE_EDIT / IDC_HANJA_EDIT).
     * The dialog's own WM_KEYDOWN/WM_SYSKEYDOWN owns the keys while set. */
    int captureTarget = 0;
    /* Windows 11 look-and-feel state (measured HRESULTs land in the log). */
    bool dark = false;
    HFONT font = nullptr; /* the Segoe UI Variable upgrade, owned */
    HBRUSH editBrush = nullptr; /* dark edit-field background */
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

static void addItem(std::vector<WORD> &t, DWORD style, DWORD exStyle, WORD x, WORD y, WORD cx, WORD cy, WORD id, WORD atom, const wchar_t *title) {
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
#define TITLE_HANJA L"\uD55C\uC790 \uD0A4" /* 한자 키 — the candidate-window key label */
#define HINT_HANJA L"\uC608: v1:25:0" /* e.g.: v1:25:0 (25 = the Hanja key, the default) */
#define TITLE_COMMIT L"\uCEE4\uBC0B \uB2E8\uC704" /* 커밋 단위 — when text reaches the document */
#define COMMIT_WORD_ROW L"\uB2E8\uC5B4 \uB2E8\uC704 (\uD55C \uBC88\uC5D0)" /* 단어 단위 (한 번에) */
#define COMMIT_SYL_ROW L"\uC74C\uC808 \uB2E8\uC704" /* 음절 단위 */
/* The capture button's two labels (WinUI 3의 key-capture flow, interim
 * dialog parity): idle = click me; capturing = the next key records.
 * Escapes, not literals: MSVC reads this TU under the ANSI codepage (949). */
#define CAPTURE_PROMPT_IDLE L"\uD0A4 \uC785\uB825" /* 키 입력 */
#define CAPTURE_PROMPT_ACTIVE L"\uD0A4\uB974 \uB204\uB974\uC138\uC694\u2026 (ESC \uCDE8\uC18C)" /* 키를 누르세요… (ESC 취소) */

static std::vector<WORD> buildDialogTemplate() {
    std::vector<WORD> t;
    /* WS_VISIBLE is REQUIRED here, measured (2026-10-18, defect "the dialog
     * opens but shows nothing"): without it the modal dialog's window is
     * created visible=0 at WM_INITDIALOG and STAYS invisible for its whole
     * lifetime on this host — the user sees nothing. The About-dialog-boxes
     * doc claims the system "always displays a modal dialog box regardless
     * of whether the dialog box template specifies the WS_VISIBLE style"
     * (learn.microsoft.com win32 dlgbox about-dialog-boxes, owner section);
     * this host contradicts it under rundll32 with no owner, and the
     * template bit is what the docs say applies when the claim does not.
     * DS_CENTER centres the ownerless launcher window. */
    w32(t, DS_MODALFRAME | DS_CENTER | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE);
    w32(t, 0); /* exStyle */
    const size_t cditOff = t.size();
    t.push_back(0); /* cdit, patched below */
    t.push_back(0); /* x */
    t.push_back(0); /* y */
    t.push_back(200);
    t.push_back(174);
    t.push_back(0); /* menu: none */
    t.push_back(0); /* class: standard dialog */
    wstr(t, L"Hunmin settings");
    t.push_back(9); /* DS_SETFONT needs this size word, then the face */
    wstr(t, L"Segoe UI");
    /* Compact grid, tightened twice after the user's live run ("spacing
     * between widgets is too big"): 200x150 dlu, rows 13-14 dlu apart,
     * edit->hint 2 dlu. The 키 입력 button beside each key field enters the
     * capture mode (WinUI 3 parity: press the chord, the dialog records
     * it; ESC cancels). */
    addItem(t, WS_CHILD | WS_VISIBLE | WS_GROUP, 0, 10, 5, 86, 10, 0, ATOM_STATIC, TITLE_HANGUL);
    addItem(t, WS_CHILD | WS_VISIBLE, 0, 106, 5, 86, 10, 0, ATOM_STATIC, TITLE_LATIN);
    /* Layout combo boxes, one per slot. */
    addItem(t, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 10, 15, 86, 56, IDC_HANGUL_LIST, ATOM_COMBOBOX, L"");
    addItem(t, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 106, 15, 86, 56, IDC_LATIN_LIST, ATOM_COMBOBOX, L"");
    /* The mode-toggle key list row. */
    addItem(t, WS_CHILD | WS_VISIBLE, 0, 10, 42, 140, 10, 0, ATOM_STATIC, TITLE_TOGGLE);
    addItem(t, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0, 10, 52, 150, 12, IDC_TOGGLE_EDIT, ATOM_EDIT, L"");
    addItem(t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 164, 52, 30, 12, IDC_TOGGLE_CAPTURE, ATOM_BUTTON, CAPTURE_PROMPT_IDLE);
    addItem(t, WS_CHILD | WS_VISIBLE, 0, 10, 66, 178, 10, IDC_HINT_TOGGLE, ATOM_STATIC, HINT_TOGGLE);
    /* The Hanja/candidate key row (issue #67): the key that opens the
     * candidate window, default the bare physical Hanja key. */
    addItem(t, WS_CHILD | WS_VISIBLE, 0, 10, 79, 140, 10, 0, ATOM_STATIC, TITLE_HANJA);
    addItem(t, WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0, 10, 89, 150, 12, IDC_HANJA_EDIT, ATOM_EDIT, L"");
    addItem(t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 164, 89, 30, 12, IDC_HANJA_CAPTURE, ATOM_BUTTON, CAPTURE_PROMPT_IDLE);
    addItem(t, WS_CHILD | WS_VISIBLE, 0, 10, 103, 178, 10, IDC_HINT_HANJA, ATOM_STATIC, HINT_HANJA);
    /* The commit unit (issue #128): when the finished text reaches the
     * document. Same text either way, so this is a preference, not a mode. */
    addItem(t, WS_CHILD | WS_VISIBLE, 0, 10, 116, 140, 10, 0, ATOM_STATIC, TITLE_COMMIT);
    addItem(t, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 10, 126, 86, 56, IDC_COMMIT_LIST, ATOM_COMBOBOX, L"");
    /* The automatic-switch row (issue #175, R13-정정 surface B): a CHECKBOX that
     * reads and writes the engine's one state. The label, the key it advertises
     * and whether the control is usable all come from
     * hunmin_tsf_autoswitch_row(), so the dialog never spells "Ctrl+한/영" and
     * never decides on its own that the feature is unavailable. */
    addItem(t, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, 0, 6, 140, 186, 12, IDC_FEATURE_ROW, ATOM_BUTTON, L"");
    /* OK / Cancel. */
    addItem(t, WS_CHILD | WS_VISIBLE | WS_GROUP | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 100, 156, 44, 12, IDOK, ATOM_BUTTON, L"OK");
    addItem(t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 148, 156, 44, 12, IDCANCEL, ATOM_BUTTON, L"Cancel");
    /* The template declared exactly the items appended above: 7 statics +
     * 3 combos + 2 edits + 2 capture buttons + 1 checkbox + 2 buttons =
     * 17. (The count is derived from the add calls above; USER32 stops
     * parsing at cdit, so a wrong number here either truncates children or
     * reads garbage as the first item — the empty-dialog defect
     * instrumentation kept the initdialog kids count as the guard.) */
    t[cditOff] = 17;
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
        strncpy_s(s->cfg.hangulLayout, HUNMIN_TSF_LAYOUT_ID_CAP, s->hangulRows[(size_t)sel].id, _TRUNCATE);
    sel = (int)SendMessageW(GetDlgItem(h, IDC_LATIN_LIST), CB_GETCURSEL, 0, 0);
    if (sel >= 0 && (size_t)sel < s->latinRows.size())
        strncpy_s(s->cfg.latinLayout, HUNMIN_TSF_LAYOUT_ID_CAP, s->latinRows[(size_t)sel].id, _TRUNCATE);
    /* Commit unit (issue #128): index 0 is the buffered word, 1 the syllable. */
    sel = (int)SendMessageW(GetDlgItem(h, IDC_COMMIT_LIST), CB_GETCURSEL, 0, 0);
    strncpy_s(s->cfg.commitUnit, HUNMIN_TSF_COMMIT_UNIT_CAP, sel == 1 ? "syllable" : "word", _TRUNCATE);
    /* Toggle-key list, wide control text -> the store grammar. Fixed-cap
     * never truncates: a too-long list is exactly the store save's failure
     * path, surfaced below. */
    std::string keys = wideToStore(getEditText(h, IDC_TOGGLE_EDIT));
    if (keys.size() >= HUNMIN_TSF_TOGGLE_KEYS_CAP) {
        MessageBoxW(h, L"Toggle key list too long.", L"Hunmin settings", MB_OK | MB_ICONWARNING);
        return; /* stay on the dialog: the user fixes the value */
    }
    strncpy_s(s->cfg.toggleKeys, HUNMIN_TSF_TOGGLE_KEYS_CAP, keys.c_str(), _TRUNCATE);
    /* The candidate-window key (issue #67): ONE combo, same grammar. An
     * empty edit means "reset to the default" — the store save then writes
     * the default explicitly, so the resolved value always round-trips. */
    std::string hanjaKey = wideToStore(getEditText(h, IDC_HANJA_EDIT));
    if (hanjaKey.empty()) {
        HunminTsfConfig def;
        hunmin_tsf_config_load(nullptr, &def);
        hanjaKey = def.hanjaKey;
    }
    if (hanjaKey.size() >= HUNMIN_TSF_HANJA_KEY_CAP) {
        MessageBoxW(h, L"Hanja key binding too long.", L"Hunmin settings", MB_OK | MB_ICONWARNING);
        return; /* stay on the dialog: the user fixes the value */
    }
    strncpy_s(s->cfg.hanjaKey, HUNMIN_TSF_HANJA_KEY_CAP, hanjaKey.c_str(), _TRUNCATE);
    /* The automatic switch (issue #175): the checkbox writes the STORE's copy of
     * a state the engine owns; the adapter hands it to the layer on the next
     * Apply (hunmin_buffered_preedit_set_option), and the engine refuses it where
     * its data is absent either way. The control is disabled in that case, so a
     * refused ON is never recorded from here. */
    s->cfg.autoswitch = SendMessageW(GetDlgItem(h, IDC_FEATURE_ROW), BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
    if (hunmin_tsf_config_save(HKEY_CURRENT_USER, &s->cfg)) {
        s->applied = true;
        hunmin_logger.log("settings: saved hangul=%s latin=%s keys=%s hanja=%s autoswitch=%d\n", s->cfg.hangulLayout, s->cfg.latinLayout,
                          s->cfg.toggleKeys, s->cfg.hanjaKey, s->cfg.autoswitch);
        EndDialog(h, 1);
    } else {
        hunmin_logger.log("settings: save FAILED\n");
        /* The user's explicit Apply must never silently do nothing. */
        MessageBoxW(h, L"Failed to write the settings store.", L"Hunmin settings", MB_OK | MB_ICONWARNING);
    }
}

static void preselectCombo(HWND combo, const std::vector<HunminTsfLayoutRow> &rows, const char *storeId) {
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
 * No frameworks: plain dwmapi + uxtheme calls (the headers are at the top). */

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
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &k) != ERROR_SUCCESS)
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
    EnumFontFamiliesExW(
        dc, &lf,
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
    s->hrCorner = DwmSetWindowAttribute(h, DWMWA_WINDOW_CORNER_PREFERENCE, &attr, sizeof(attr));
    /* The Mica backdrop is GONE. Measured 2026-10-18 (defect "the dialog is
     * empty"): with DwmExtendFrameIntoClientArea(-1) + DWMSBT_MICA the
     * client is the DWM-composed surface and the dialog's own controls
     * render ghost-white over it in light mode — the dialog reads as
     * empty to the user (the PrintWindow capture proved contrasting text
     * does not survive the backdrop). The interim dialog exists as a
     * fallback surface; readability beats the Windows-11 look, which stays
     * the ruled settings-ui exe's job. What remains of the look: rounded
     * corners and, in dark mode, the dark title bar + dark child themes.
     * WM_ERASEBKGND (below) now paints a solid mode brush instead of
     * letting the backdrop show through. */
    if (s->dark) {
        const int ids[] = {IDC_HANGUL_LIST, IDC_LATIN_LIST, IDC_TOGGLE_EDIT, IDC_HANJA_EDIT, IDC_FEATURE_ROW};
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
    const wchar_t *want = fontFaceExists(L"Segoe UI Variable Display") ? L"Segoe UI Variable Display" : L"Segoe UI";
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
    hunmin_logger.log("settings w11: dark=%d corner=0x%lX darkAttr=0x%lX font=%ls\n", (int)s->dark, (unsigned long)s->hrCorner,
                      (unsigned long)s->hrDark, want);
}

/* ---- the key-capture hook (the maintainer instruction: set combinations
 * by pressing keys). The WM_KEYDOWN route was measured dead in this modal
 * dialog on this host — the build shows this hemmed untouchable — so the
 * recorder is a thread WH_KEYBOARD hook, which sees the TRUE physical VKs
 * before any IME processing (the physical 한/Hanja keys arrive as their
 * real VK here, not VK_PROCESSKEY). Keydown commits, chordless ESC
 * cancels, modifier-only keys stay armed; the hook swallows every
 * keyboard event while armed, so the IME cannot compose through the
 * capture. ---- */
static HHOOK g_captureHook = nullptr;
static HWND g_captureDlg = nullptr;

static bool captureIsModifier(WPARAM vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_MENU ||
           vk == VK_LMENU || vk == VK_RMENU || vk == VK_LWIN || vk == VK_RWIN || vk == VK_APPS || vk == VK_NUMLOCK;
}

/* Record the chord into the armed row and stand down. */
static void recordCapturedChord(HWND h, SettingsState *s, WPARAM vk) {
    unsigned mods = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000)
        mods |= HUNMIN_TSF_KEYMOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        mods |= HUNMIN_TSF_KEYMOD_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000)
        mods |= HUNMIN_TSF_KEYMOD_ALT;
    char combo[24];
    snprintf(combo, sizeof combo, "v1:%lu:%u", (unsigned long)vk, mods);
    HWND edit = GetDlgItem(h, s->captureTarget);
    if (edit) {
        if (s->captureTarget == IDC_TOGGLE_EDIT) {
            /* The toggle row is a LIST: the chord APPENDS, unless the
             * exact entry is already there (grammar
             * "v1:<vk>:<mods>;..."). */
            std::string keys = wideToStore(getEditText(h, IDC_TOGGLE_EDIT));
            if (keys.find(combo) == std::string::npos) {
                if (!keys.empty())
                    keys += ";";
                keys += combo;
            }
            std::wstring w(keys.begin(), keys.end());
            SetWindowTextW(edit, w.c_str());
        } else {
            /* The Hanja row is ONE binding: the chord REPLACES. */
            std::wstring w(combo, combo + strlen(combo));
            SetWindowTextW(edit, w.c_str());
        }
        hunmin_logger.log("settings: captured %s into %d\n", combo, s->captureTarget);
    }
    if (g_captureHook) {
        UnhookWindowsHookEx(g_captureHook);
        g_captureHook = nullptr;
    }
    SetWindowTextW(GetDlgItem(h, s->captureTarget == IDC_TOGGLE_EDIT ? IDC_TOGGLE_CAPTURE : IDC_HANJA_CAPTURE), CAPTURE_PROMPT_IDLE);
    s->captureTarget = 0;
}

/* Cancel: the button label restores, the edit keeps whatever it had. */
static void recordCapturedAbort(HWND h, SettingsState *s) {
    hunmin_logger.log("settings: capture cancelled\n");
    if (g_captureHook) {
        UnhookWindowsHookEx(g_captureHook);
        g_captureHook = nullptr;
    }
    SetWindowTextW(GetDlgItem(h, s->captureTarget == IDC_TOGGLE_EDIT ? IDC_TOGGLE_CAPTURE : IDC_HANJA_CAPTURE), CAPTURE_PROMPT_IDLE);
    s->captureTarget = 0;
}

static LRESULT CALLBACK captureKeyHook(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && g_captureDlg) {
        auto *s = (SettingsState *)GetWindowLongPtrW(g_captureDlg, GWLP_USERDATA);
        if (s && s->captureTarget != 0) {
            WPARAM vk = wp;
            bool up = (lp & (1u << 31)) != 0;
            bool chord = (GetKeyState(VK_SHIFT) & 0x8000) || (GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000);
            if (!up && vk == VK_ESCAPE && !chord) {
                recordCapturedAbort(g_captureDlg, s);
                return 1;
            }
            if (!up && !captureIsModifier(vk)) {
                recordCapturedChord(g_captureDlg, s, vk);
                return 1;
            }
            /* keyups and modifier keydowns: swallowed, capture stays armed */
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

/* Arm: install the hook and paint the prompt onto the targeted button. */
static void RecordCapturedChordArm(HWND h, SettingsState *s) {
    if (s->captureTarget == 0)
        return;
    g_captureDlg = h;
    if (!g_captureHook)
        g_captureHook = SetWindowsHookExW(WH_KEYBOARD, captureKeyHook, nullptr, GetCurrentThreadId());
    SetWindowTextW(GetDlgItem(h, s->captureTarget == IDC_TOGGLE_EDIT ? IDC_TOGGLE_CAPTURE : IDC_HANJA_CAPTURE), CAPTURE_PROMPT_ACTIVE);
    hunmin_logger.log("settings: hook capture armed target=%d hook=%p\n", s->captureTarget, (void *)g_captureHook);
}

static INT_PTR CALLBACK settingsProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto *s = (SettingsState *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (msg == WM_INITDIALOG && s == nullptr) {
        s = (SettingsState *)lp;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)s);
        for (const HunminTsfLayoutRow &r : s->hangulRows) {
            std::wstring d = utf8ToWide(r.name);
            d += L" (" + utf8ToWide(r.id) + L")";
            SendMessageW(GetDlgItem(h, IDC_HANGUL_LIST), CB_ADDSTRING, 0, (LPARAM)d.c_str());
        }
        for (const HunminTsfLayoutRow &r : s->latinRows) {
            std::wstring d = utf8ToWide(r.name);
            d += L" (" + utf8ToWide(r.id) + L")";
            SendMessageW(GetDlgItem(h, IDC_LATIN_LIST), CB_ADDSTRING, 0, (LPARAM)d.c_str());
        }
        preselectCombo(GetDlgItem(h, IDC_HANGUL_LIST), s->hangulRows, s->cfg.hangulLayout);
        preselectCombo(GetDlgItem(h, IDC_LATIN_LIST), s->latinRows, s->cfg.latinLayout);
        /* Commit unit (issue #128): two literal rows, index == the store value. */
        SendMessageW(GetDlgItem(h, IDC_COMMIT_LIST), CB_ADDSTRING, 0, (LPARAM)COMMIT_WORD_ROW);
        SendMessageW(GetDlgItem(h, IDC_COMMIT_LIST), CB_ADDSTRING, 0, (LPARAM)COMMIT_SYL_ROW);
        SendMessageW(GetDlgItem(h, IDC_COMMIT_LIST), CB_SETCURSEL, (WPARAM)(strcmp(s->cfg.commitUnit, "syllable") == 0 ? 1 : 0), 0);
        /* Toggle keys into the edit. */
        std::wstring w = utf8ToWide(s->cfg.toggleKeys);
        SetWindowTextW(GetDlgItem(h, IDC_TOGGLE_EDIT), w.c_str());
        /* The candidate-window key into its edit (issue #67). */
        std::wstring hk = utf8ToWide(s->cfg.hanjaKey);
        SetWindowTextW(GetDlgItem(h, IDC_HANJA_EDIT), hk.c_str());
        /* The automatic-switch row (issue #175): the engine's label (which names
         * the key it honours), the engine's value, and -- when B3's data gate
         * says the dictionary is not installed -- a DISABLED control whose label
         * says why. "off by choice" and "off, data missing" must be tellable
         * apart (R13-정정). */
        {
            HWND box = GetDlgItem(h, IDC_FEATURE_ROW);
            SetWindowTextW(box, utf8ToWide(s->featureRow.label).c_str());
            SendMessageW(box, BM_SETCHECK, (WPARAM)(s->featureRow.value ? BST_CHECKED : BST_UNCHECKED), 0);
            EnableWindow(box, s->featureRow.available ? TRUE : FALSE);
        }
        /* Windows 11 look: corners, dark title bar in dark mode, Variable
         * font. Logged HRESULTs are the measurement (see the manual
         * checklist). */
        applyWindows11Look(h, s);
        /* Force the window into view (defect measured twice on 2026-10-18:
         * the template's WS_VISIBLE reaches the window (initdialog logs
         * visible=0 the ancestor artefact) yet the ownerless rundll32
         * dialog never appeared ON SCREEN — PrintWindow proved the controls
         * were all there. Owned top-level + DS_CENTER is not enough under
         * this host, so: explicit ShowWindow, topmost, centred on the work
         * area, and foreground. */
        ShowWindow(h, SW_SHOWNORMAL);
        {
            RECT wa = {}, rc = {};
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
            GetWindowRect(h, &rc);
            int winW = rc.right - rc.left, ht = rc.bottom - rc.top;
            SetWindowPos(h, HWND_TOPMOST, wa.left + ((wa.right - wa.left) - winW) / 2, wa.top + ((wa.bottom - wa.top) - ht) / 2, 0, 0, SWP_NOSIZE);
            SetForegroundWindow(h);
            GetWindowRect(h, &rc);
            hunmin_logger.log("settings: initdialog rect=(%ld,%ld)-(%ld,%ld) visible=%d\n", (long)rc.left, (long)rc.top, (long)rc.right,
                              (long)rc.bottom, (int)IsWindowVisible(h));
        }
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
        /* 키 입력 buttons: BN_CLICKED arms the capture mode by installing
         * the thread WH_KEYBOARD hook (see captureKeyHook above): the
         * WM_KEYDOWN route was measured dead in this dialog (armed,
         * focused, no keydown ever arrived), so the hook is the recorder
         * and the dialog proc only paints the mode. Focus stays put. */
        if (LOWORD(wp) == IDC_TOGGLE_CAPTURE && HIWORD(wp) == BN_CLICKED) {
            s->captureTarget = IDC_TOGGLE_EDIT;
            RecordCapturedChordArm(h, s);
            return TRUE;
        }
        if (LOWORD(wp) == IDC_HANJA_CAPTURE && HIWORD(wp) == BN_CLICKED) {
            s->captureTarget = IDC_HANJA_EDIT;
            RecordCapturedChordArm(h, s);
            return TRUE;
        }
        break;
    case WM_GETDLGCODE:
        /* While capturing the dialog wants every key (arrows and Tab
         * included); the navigation behaviour stays untouched otherwise. */
        if (s && s->captureTarget != 0)
            return DLGC_WANTALLKEYS;
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
        /* Solid fill in dark mode; light mode keeps the default white —
         * no backdrop shows through any more (Mica removed, see
         * applyWindows11Look). */
        if (s && s->dark) {
            RECT rc = {};
            GetClientRect(h, &rc);
            FillRect((HDC)wp, &rc, s->darkBgBrush);
            return 1;
        }
        break;
    case WM_DESTROY:
        if (g_captureHook) {
            UnhookWindowsHookEx(g_captureHook);
            g_captureHook = nullptr;
        }
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
bool hunmin_tsf_settings_dialog_run(HWND parent) {
    SettingsState s;
    s.ctx = hunmin_ctx_from_module();
    HunminTsfLayoutRow buf[HUNMIN_TSF_SETTINGS_MAX_ROWS];
    int n = hunmin_tsf_rows_for_mode(s.ctx, true, buf, HUNMIN_TSF_SETTINGS_MAX_ROWS);
    s.hangulRows.assign(buf, buf + n);
    n = hunmin_tsf_rows_for_mode(s.ctx, false, buf, HUNMIN_TSF_SETTINGS_MAX_ROWS);
    s.latinRows.assign(buf, buf + n);
    hunmin_tsf_config_load(HKEY_CURRENT_USER, &s.cfg);
    /* The automatic-switch row (issue #175): resolved against the SAME layout
     * pair the adapter will build its layer from, so the engine's answer here
     * is the one the TIP will hold after Apply. Nothing about the feature is
     * decided in the dialog. */
    {
        std::string hangulId, latinId;
        hunmin_tsf_resolve_layout_slot(s.ctx, s.cfg.hangulLayout, "sin3-p2", true, &hangulId);
        hunmin_tsf_resolve_layout_slot(s.ctx, s.cfg.latinLayout, "us-qwerty", false, &latinId);
        hunmin_tsf_autoswitch_row(s.ctx, hangulId.c_str(), latinId.c_str(), s.cfg.autoswitch, &s.featureRow);
    }
    hunmin_logger.log("settings: open rows hangul=%zu latin=%zu autoswitch=%d available=%d key=%s\n", s.hangulRows.size(), s.latinRows.size(),
                      s.featureRow.value, s.featureRow.available, s.featureRow.key);
    std::vector<WORD> t = buildDialogTemplate();
    /* Template ground truth in the log: this stream is what USER32 parses,
     * and the empty-dialog defect report (2026-10-18) showed a created-but-
     * childless window — the dump here says whether the live bytes differ
     * from the layout the docs specify (style/exstyle DWORDs, cdit, x/y/cx/
     * cy, menu/class WORDs, title, DS_SETFONT size+face, then DWORD-aligned
     * DLGITEMTEMPLATEs). */
    {
        std::string hex;
        char piece[16];
        const size_t dump = t.size() < 30 ? t.size() : 30;
        for (size_t i = 0; i < dump; ++i) {
            snprintf(piece, sizeof piece, "%04X ", t[i]);
            hex += piece;
        }
        hunmin_logger.log("settings: template words=%zu head=%s\n", t.size(), hex.c_str());
    }
    /* The instance handle: from this TU's own address, not a module-name
     * lookup — the tier-F1 binary links this code and is not the TIP
     * module (tsf.cpp's GetModuleHandleW(L"hunmin-tsf.dll") trick does not
     * carry there). With a template pointer the instance is not deref'd,
     * but resolve it honestly anyway. */
    HMODULE hSelf = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)(const void *)&hunmin_tsf_settings_dialog_run, &hSelf);
    INT_PTR got = DialogBoxIndirectParamW((HINSTANCE)hSelf, (LPCDLGTEMPLATEW)t.data(), parent, settingsProc, (LPARAM)&s);
    if (got == -1)
        hunmin_logger.log("settings: DialogBoxIndirectParamW FAILED err=%lu\n", GetLastError());
    hunmin_logger.log("settings: dialog closed got=%ld applied=%d\n", (long)got, (int)s.applied);
    return s.applied;
}
