/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * F9 Hanja candidate window (issue #67): implementation. See candwindow.h
 * for the contract. Constants are chosen so the popup behaves like the
 * overlay window already in tsf.cpp: same topmost/toolwindow/no-activate
 * style, same DPI-awareness trick, no text cursor activation.
 */
#include "candwindow.h"

#include <richedit.h>
#include <string>

void hunmin_tsf_host_logfont(HWND fontHost, LOGFONTW *outLf) {
    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    bool got = false;
    CHARFORMAT2W cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    if (fontHost && SendMessageW(fontHost, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf) &&
        (cf.dwMask & (CFM_FACE | CFM_SIZE)) == (CFM_FACE | CFM_SIZE)) {
        UINT dpi = fontHost ? GetDpiForWindow(fontHost) : 96;
        if (dpi == 0)
            dpi = 96;
        lf.lfHeight = -(int)((cf.yHeight * dpi + 720) / 1440);
        if (lf.lfHeight == 0)
            lf.lfHeight = -12;
        lf.lfWeight = (cf.dwEffects & CFE_BOLD) ? FW_BOLD : FW_NORMAL;
        lf.lfItalic = (cf.dwEffects & CFE_ITALIC) ? 1 : 0;
        wcscpy_s(lf.lfFaceName, LF_FACESIZE, cf.szFaceName);
        got = true;
    }
    if (!got && fontHost) {
        HFONT hf = (HFONT)SendMessageW(fontHost, WM_GETFONT, 0, 0);
        if (hf && GetObjectW(hf, sizeof(lf), &lf))
            got = true;
    }
    if (!got) {
        HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        if (hf)
            got = GetObjectW(hf, sizeof(lf), &lf) != 0;
    }
    if (!got)
        lf.lfHeight = -12;
    *outLf = lf;
}

HunminCandWindow::~HunminCandWindow() {
    if (wnd_)
        DestroyWindow(wnd_);
    if (font_)
        DeleteObject(font_);
}

void HunminCandWindow::Show(const RECT &anchor, HWND fontHost, const std::vector<std::wstring> &lines) {
    if (lines.empty())
        return;

    /* Text first: the window is sized from what it must draw. */
    std::wstring text;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i)
            text += L"\r\n";
        text += lines[i];
    }

    if (!wnd_) {
        LOGFONTW lf;
        hunmin_tsf_host_logfont(fontHost, &lf);
        font_ = CreateFontIndirectW(&lf);
        curLf_ = lf;
        DPI_AWARENESS_CONTEXT oldCtx = fontHost ? SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(fontHost)) : nullptr;
        wnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", nullptr, WS_POPUP | SS_LEFT, anchor.left,
                               anchor.bottom + 2, 10, 10, nullptr, nullptr, GetModuleHandleW(L"hunmin-tsf.dll"), nullptr);
        /* DPI awareness is a per-window creation context, like the overlay's:
         * set around creation so the popup's coordinate space matches the
         * client window the caret rect came from. */
        if (oldCtx)
            SetThreadDpiAwarenessContext(oldCtx);
        if (!wnd_)
            return;
        if (font_ && wnd_)
            SendMessageW(wnd_, WM_SETFONT, (WPARAM)font_, TRUE);
    }

    /* Metrics: the widest needed if the current font renders the popup. The
     * font may have to follow a new host font (the host window changed);
     * recreate only when the FACE actually changed so a per-keystroke
     * CreateFontIndirectW drift is avoided. */
    LOGFONTW lf;
    hunmin_tsf_host_logfont(fontHost, &lf);
    if (wnd_ && font_ && memcmp(&lf, &curLf_, sizeof(lf)) != 0) {
        HFONT next = CreateFontIndirectW(&lf);
        if (next) {
            SendMessageW(wnd_, WM_SETFONT, (WPARAM)next, TRUE);
            DeleteObject(font_);
            font_ = next;
            curLf_ = lf;
        }
    }

    HWND metric = fontHost ? fontHost : wnd_;
    HDC dc = GetDC(metric);
    SIZE lineSz = {}, maxSz = {};
    if (dc && font_) {
        HFONT oldFont = (HFONT)SelectObject(dc, font_);
        for (const std::wstring &l : lines) {
            GetTextExtentPoint32W(dc, l.c_str(), (int)l.size(), &lineSz);
            if (lineSz.cx > maxSz.cx)
                maxSz.cx = lineSz.cx;
            if (lineSz.cy > maxSz.cy)
                maxSz.cy = lineSz.cy;
        }
        SelectObject(dc, oldFont);
        ReleaseDC(metric, dc);
    }
    if (maxSz.cx < 1)
        maxSz.cx = 60;
    if (maxSz.cy < 1)
        maxSz.cy = 14;

    int pad = 4;
    MoveWindow(wnd_, anchor.left, anchor.bottom + 2, maxSz.cx + pad * 2, (int)lines.size() * maxSz.cy + pad * 2, TRUE);
    SetWindowTextW(wnd_, text.c_str());
    ShowWindow(wnd_, SW_SHOWNOACTIVATE);
}

void HunminCandWindow::Hide() {
    if (wnd_)
        ShowWindow(wnd_, SW_HIDE);
}
