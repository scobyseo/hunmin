/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef HUNMIN_TSF_CANDWINDOW_H_
#define HUNMIN_TSF_CANDWINDOW_H_

/* F9 Hanja candidate window (issue #67) — pure presentation.
 *
 * One popup STATIC window listing the candidates the caller computed from
 * the engine's suggest API. This module knows neither the engine nor TSF:
 * the caller (src/tsf.cpp) builds the lines and the caret rect (the TSF
 * caret-rect fetch) and hands them over; choosing happens with the keyboard
 * in the adapter, not here — no mouse handling in the first cut.
 *
 * Out of the mock harness's scope on purpose: mock-tsf.h gives the harness
 * no screen geometry (ITfContextView::GetTextExt fails, a console process
 * has no caret), so UpdateCandidateWindow takes the hide path and Show is
 * never reached — the same seam that keeps the overlay window out of the
 * tier-F1 run.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>

#ifdef __cplusplus
}
#endif

/* Derive a LOGFONTW for an IME popup from the window the text will appear
 * beside: the richedit selection's face/size when the host reports one,
 * else the host's WM_GETFONT, else the default GUI font. Logs want the
 * derived LOGFONT either way (the overlay does the same derivation inline;
 * the popup needs it as a function because it owns a different window).
 * fontHost may be nullptr (headless/mock runs) — a filled LOGFONTW is still
 * returned, from DEFAULT_GUI_FONT, because the popup must keep measuring
 * text even without a host to mirror. */
void hunmin_tsf_host_logfont(HWND fontHost, LOGFONTW *outLf);

#ifdef __cplusplus
#include <string>
#include <vector>

class HunminCandWindow {
public:
    HunminCandWindow() = default;
    ~HunminCandWindow();

    HunminCandWindow(const HunminCandWindow &) = delete;
    HunminCandWindow &operator=(const HunminCandWindow &) = delete;

    /* anchor: the caret rect in screen coordinates (as the caret-rect
     * fetch in tsf.cpp produces); the list is drawn under it.
     * fontHost: the client window the text metrics should follow
     * (nullptr legal — falls back to DEFAULT_GUI_FONT).
     * lines: one entry per visible row, in draw order. */
    void Show(const RECT &anchor, HWND fontHost, const std::vector<std::wstring> &lines);
    void Hide();

private:
    HWND wnd_ = nullptr;
    HFONT font_ = nullptr; /* owned; replaced (then deleted) on every Show */
    LOGFONTW curLf_ = {}; /* the LOGFONTW font_ was created from */
};

#endif /* __cplusplus */
#endif /* HUNMIN_TSF_CANDWINDOW_H_ */
