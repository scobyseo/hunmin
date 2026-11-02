/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The mock's edit-control channel (issue #185). The interim delivery keeps the
 * word in the APPLICATION's document and rewrites it through the control's own
 * EDIT-family messages (EM_SETSEL / EM_GETSEL / EM_REPLACESEL), never through
 * TSF - so a harness that models only TSF (mock-tsf.h) cannot observe that
 * mode at all. This is the control it writes to.
 *
 * It is a real window of this process's own class, created offscreen and given
 * the thread's focus the way a host application gives focus to its edit
 * control (the interim writer resolves its target with GetFocus()), but it
 * implements only what delivery.cpp's interim writer sends - plus the text
 * queries its own log line evaluates - over a plain wstring. The document is
 * recorded the way MockContext::doc is, and every write is kept in order with
 * the span it replaced, so a case can assert both what the client holds and
 * how the writer got there.
 *
 * Deliberately NOT the system EDIT class. Two measured reasons: a single-line
 * EDIT without ES_AUTOHSCROLL refuses to grow past its own width, so a corpus
 * run fails on the control instead of on the adapter (see the note in
 * tsf-legacy-test.cpp), and an EDIT holds a caret, which would feed the
 * candidate window's GUITHREADINFO fallback a rect this harness has no screen
 * geometry for. A window of our class has neither property.
 *
 * One control per process, reset per corpus case (CTextService is rebuilt per
 * case, and its InterimDelivery resolves the control by focus).
 */
#pragma once
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

namespace tsfmock {

class MockEditControl {
public:
    /* One write through the control: the span that was selected when it
     * arrived (start == end for a write at the caret), and the text the
     * control now holds there. */
    struct Write {
        LONG start = 0;
        LONG end = 0;
        std::wstring text;
    };

    MockEditControl() = default;
    ~MockEditControl() { Destroy(); }
    MockEditControl(const MockEditControl &) = delete;
    MockEditControl &operator=(const MockEditControl &) = delete;

    /* Create the control and take the thread's focus. False when the class or
     * the window could not be created, or when the focus did not come with it
     * (GetFocus() is what the writer resolves, so a control without focus is
     * a harness that would silently measure nothing). */
    bool Create() {
        if (hwnd_)
            return true;
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = &MockEditControl::Proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kClassName;
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
        s_current = this;
        hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, kClassName, L"", WS_POPUP, -32000, -32000, 400, 40, nullptr, nullptr, wc.hInstance, nullptr);
        if (!hwnd_) {
            s_current = nullptr;
            return false;
        }
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        SetFocus(hwnd_);
        return GetFocus() == hwnd_;
    }

    void Destroy() {
        if (hwnd_) {
            if (GetFocus() == hwnd_)
                SetFocus(nullptr);
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        if (s_current == this)
            s_current = nullptr;
    }

    /* A fresh client per corpus case: empty document, caret at 0, no writes.
     * The caller creates a new CTextService per case, so the delivery's own
     * span bookkeeping starts from zero at the same instant. */
    void Reset() {
        doc_.clear();
        writes_.clear();
        selStart_ = selEnd_ = 0;
    }

    HWND hwnd() const { return hwnd_; }
    /* The document, the way MockContext::doc is: what a reader of the control
     * would see. */
    const std::wstring &doc() const { return doc_; }
    const std::vector<Write> &writes() const { return writes_; }
    size_t writeCount() const { return writes_.size(); }

private:
    static constexpr const wchar_t *kClassName = L"HunminMockEditControl";

    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        MockEditControl *self = s_current;
        if (self && self->hwnd_ == hwnd)
            return self->Handle(hwnd, msg, wp, lp);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    LONG Clamp(LONG pos) const {
        if (pos < 0)
            return 0;
        if (pos > (LONG)doc_.size())
            return (LONG)doc_.size();
        return pos;
    }

    /* The one splice the interim writer's two messages produce: the selected
     * span becomes `text` and the caret lands after it. */
    void Replace(LONG start, LONG end, const std::wstring &text) {
        if (end < start)
            end = start;
        start = Clamp(start);
        end = Clamp(end);
        doc_.replace((size_t)start, (size_t)(end - start), text);
        selStart_ = selEnd_ = start + (LONG)text.size();
    }

    LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        /* The writer's select: EM_SETSEL(start, end), with the real
         * control's (0, -1) select-all form. */
        case EM_SETSEL: {
            LONG start = (LONG)(INT_PTR)wp;
            LONG end = (LONG)lp;
            if (start == 0 && end == -1) {
                start = 0;
                end = (LONG)doc_.size();
            }
            selStart_ = Clamp(start);
            selEnd_ = Clamp(end);
            if (selEnd_ < selStart_)
                std::swap(selStart_, selEnd_);
            return 0;
        }
        /* The writer reads the caret through the DWORD pair (its EmCaret
         * passes both pointers); the real control also folds them into the
         * return value, so answer both. */
        case EM_GETSEL: {
            if (wp)
                *reinterpret_cast<DWORD *>(wp) = (DWORD)selStart_;
            if (lp)
                *reinterpret_cast<DWORD *>(lp) = (DWORD)selEnd_;
            return (LRESULT)MAKELONG((WORD)selStart_, (WORD)selEnd_);
        }
        /* The writer's one write primitive. Recorded with the span it
         * replaced, which is the observation a case asserting "the finalised
         * half was appended, then the span was rewritten" needs. */
        case EM_REPLACESEL: {
            const wchar_t *raw = reinterpret_cast<const wchar_t *>(lp);
            const std::wstring text = raw ? raw : L"";
            Write w;
            w.start = selStart_;
            w.end = selEnd_;
            w.text = text;
            writes_.push_back(w);
            Replace(selStart_, selEnd_, text);
            return 0;
        }
        case EM_GETLIMITTEXT:
            return limit_;
        /* delivery.cpp's log line calls GetWindowTextLengthW on every span
         * write; its arguments are evaluated whether or not logging is on. */
        case WM_GETTEXTLENGTH:
            return (LRESULT)doc_.size();
        case WM_GETTEXT: {
            if (!lp || wp == 0)
                return 0;
            size_t n = (size_t)wp - 1;
            if (n > doc_.size())
                n = doc_.size();
            if (n)
                std::memcpy(reinterpret_cast<void *>(lp), doc_.data(), n * sizeof(wchar_t));
            reinterpret_cast<wchar_t *>(lp)[n] = L'\0';
            return (LRESULT)n;
        }
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
    }

    static MockEditControl *s_current; /* the harness owns one control */

    HWND hwnd_ = nullptr;
    LONG selStart_ = 0;
    LONG selEnd_ = 0;
    LONG limit_ = 0x7FFFFFFE;
    std::wstring doc_;
    std::vector<Write> writes_;
};

inline MockEditControl *MockEditControl::s_current = nullptr;

} // namespace tsfmock