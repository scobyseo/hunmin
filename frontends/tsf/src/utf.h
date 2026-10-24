/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * UTF-8 <-> UTF-16 for the candidate path (issue #67): the suggest API is
 * UTF-8, the popup and the buffered word are wide. The one conversion pair
 * every TSF-frontend site uses — extracted 2026-10-22 from the statics
 * tsf.cpp carried (the candidate flow alone needed them at three call
 * sites, and half-drawn double conversions have burned this tree before:
 * the embedded-NUL word buffer). Header-only; depends on nothing but the
 * Win32 conversion API.
 */
#ifndef HUNMIN_TSF_UTF_H_
#define HUNMIN_TSF_UTF_H_

#include <windows.h>

#include <string>

/* Wide string from a UTF-8 buffer; empty input or an empty decode is {}. */
inline std::wstring HunminUtf8ToWide(const char *utf8) {
    if (!utf8 || !utf8[0])
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (n <= 1)
        return {};
    std::wstring w((size_t)n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], n);
    return w;
}

/* UTF-8 from a wide string; empty input or a failed encode is "". */
inline std::string HunminWideToUtf8(const std::wstring &w) {
    if (w.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

#endif /* HUNMIN_TSF_UTF_H_ */
