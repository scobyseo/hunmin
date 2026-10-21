/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * FRAGMENT of tsf.cpp, not a standalone TU: the delivery seam
 * (issue #119) - Delivery/NativeDelivery/InterimDelivery and the interim
 * write machinery (the deferral measurement), the entry-settle
 * FlushPendingDocWrites, and the writer's static. Included INSIDE
 * CTextService's body.
 */
/* ---- The delivery seam (issue #119) ----
     *
     * The COMMON key handling (OnTestKeyDown/OnKeyDown) classifies the key
     * and feeds the engine; it never asks which mode it is in. What the
     * two modes DO with the engine's answer is the seam below:
     *
     * - NativeDelivery — the #98 composition path: the word rides a live
     *   ITfComposition (inline start/update/end per edit session). Modern
     *   hosts keep compositions alive (Win11 Notepad, Edge — measured).
     * - InterimDelivery — the #119 interim path for classic (default-sink)
     *   hosts: NO composition (nothing to terminate), the word is document
     *   text, rewritten per key through the control's own EM_* protocol.
     *   The doc model: [finalised word][in-flight span]; the span
     *   [preStart_, preStart_+preLen_) is the ONLY text the mode rewrites,
     *   always through ONE select + ONE EM_REPLACESEL (ReplaceSpan) —
     *   piecewise span edits accumulate (measured: 1+1+1+2+2+2 chars for
     *   7 keys; jamotong_ime's manual §8.2, artifact ㄱ가간...).
     *   NO overlay window (maintainer instruction 2026-10-20). The styled
     *   composition underline is NOT reproduced on this path (fInterimChar
     *   selection: SetSelection E_INVALIDARG measured on both host
     *   classes) — plain-text preedit is the accepted shape
     *   (docs/TSF-INTERIM-INPUT-SPIKE.md §4-§6).
     *
     * A host-driven OnCompositionTerminated that fires MID-WORD flips the
     * service native -> interim for its life (the detection measurement:
     * classic EDIT / msftedit RICHEDIT50W terminate every key; modern
     * hosts terminate only at real boundaries). HUNMIN_TSF_INTERIM=1
     * overrides detection (the old spike's escape hatch). */
struct Delivery {
    virtual ~Delivery() = default;
    /* The engine consumed a key (or a host termination fused the word):
         * the word state moved, the document must follow. */
    virtual void Consumed(ITfContext *pic) = 0;
    /* A word boundary: declined key, mode toggle, chord, passage key. */
    virtual void Boundary(ITfContext *pic) = 0;
    /* The Hanja candidate lookup anchor. */
    virtual std::string CandidateAnchor() = 0;
    /* A picked candidate commits as `replacement` (the fallback-aware
         * text the common side computed from the anchor). false = refused:
         * the buffers stay intact, nothing is delivered. */
    virtual bool CandidateCommit(ITfContext *pic, const std::wstring &replacement) = 0;
    /* Settle a posted doc write (the interim deferral; the native path
         * is synchronous — a no-op). */
    virtual void FlushPending() = 0;
};

struct NativeDelivery : Delivery {
    CTextService *svc;
    explicit NativeDelivery(CTextService *s) : svc(s) {}
    void Consumed(ITfContext *pic) override {
        svc->CompositionWrite(pic);
    }
    void Boundary(ITfContext *pic) override {
        svc->InsertBufferedWord(pic);
    }
    std::string CandidateAnchor() override {
        /* The whole buffered word: accumulated finalised syllables plus
             * the in-flight preedit — the multi-syllable lookup the
             * dictionary tables are keyed by (krdict words, idioms).
             * fcitx5 anchors to its preedit only because it commits each
             * syllable immediately. */
        std::string s = HunminWideToUtf8(svc->wordbuf_);
        if (svc->ic_) {
            const char *pre = hunmin_ic_preedit(svc->ic_);
            if (pre)
                s += pre;
        }
        return s;
    }
    bool CandidateCommit(ITfContext *pic, const std::wstring &replacement) override {
        if (svc->ic_)
            hunmin_ic_reset(svc->ic_); /* drop the composition the list anchored to */
        svc->wordbuf_ = replacement;
        svc->InsertBufferedWord(pic);
        return true;
    }
    void FlushPending() override {}
};

/* The interim mode's own state and writes. The deferral machinery
     * (measured defect): the EM_* writes must not run INSIDE a TSF
     * callback — the first inline write fired a re-entrant
     * ITfThreadMgrEventSink::OnSetFocus(NULL) storm (msftedit's TSF store
     * settling the out-of-band selection change) and every keystroke sent
     * while it ran was lost before reaching OnTestKeyDown. So the write is
     * POSTED and runs on the app's UI thread from a thread timer
     * (SetTimer with a NULL hwnd — fires when that thread pumps, i.e.
     * after the current key event has fully returned), or at the ENTRY of
     * the next key event, whichever comes first. One slot suffices: the
     * entry flush keeps it empty at every post. */
struct InterimDelivery : Delivery {
    CTextService *svc;
    explicit InterimDelivery(CTextService *s) : svc(s) {}

    HWND interimHwnd_ = nullptr; /* the edit control (resolved once) */
    LONG preStart_ = 0; /* doc offset where the span begins */
    size_t preLen_ = 0; /* the span's current char count */
    std::wstring pendingDeadCopy_; /* a fused copy awaiting deletion */
    enum PendingWrite { kPendingNone,
                        kPendingWrite,
                        kPendingDrain };
    PendingWrite pendingWriteKind_ = kPendingNone;
    static InterimDelivery *s_writer; /* one TIP instance per thread */
    static const UINT_PTR kWriteTimerId = 1;

    void Consumed(ITfContext *pic) override {
        (void)pic;
        Post(kPendingWrite);
    }
    void Boundary(ITfContext *pic) override {
        (void)pic;
        Post(kPendingDrain);
    }
    void FlushPending() override {
        Dispatch();
    }
    /* The one-char ruling (maintainer, 2026-10-20: "in interim mode,
         * only one char conversion should be supported"): the finalised
         * syllables are document text outside the span the mode rewrites,
         * so the anchor is the IN-FLIGHT preedit only, and a candidate
         * that is not exactly one character is refused whole — the
         * whole-word anchor's multi-char candidates cannot commit without
         * corrupting the word (measured: 안녕 + 安寧 → 안安寧). */
    std::string CandidateAnchor() override {
        const char *pre = svc->ic_ ? hunmin_ic_preedit(svc->ic_) : "";
        return std::string(pre ? pre : "");
    }
    bool CandidateCommit(ITfContext *pic, const std::wstring &replacement) override {
        (void)pic;
        if (replacement.size() != 1 || preLen_ == 0)
            return false; /* refused: the word stays intact */
        if (svc->ic_)
            hunmin_ic_reset(svc->ic_); /* drop the syllable the list anchored to */
        svc->wordbuf_ = replacement; /* the drain rewrites the span to it */
        Post(kPendingDrain);
        return true;
    }

    void Post(PendingWrite kind) {
        pendingWriteKind_ = kind;
        s_writer = this;
        SetTimer(nullptr, kWriteTimerId, 10, WriteTimerProc);
    }
    /* Runs the queued doc write. Called from the timer and from the
         * key events' entry (the mock tests drive it directly — they never
         * pump). */
    void Dispatch() {
        PendingWrite kind = pendingWriteKind_;
        pendingWriteKind_ = kPendingNone;
        KillTimer(nullptr, kWriteTimerId);
        if (kind == kPendingWrite)
            InterimWrite();
        else if (kind == kPendingDrain)
            InterimDrain();
    }
    static void CALLBACK WriteTimerProc(HWND, UINT, UINT_PTR, DWORD) {
        if (s_writer)
            s_writer->Dispatch();
    }

    /* The edit control the writes go through: the key events arrive on
         * the app's input thread, so GetFocus() is it — default-sink
         * contexts refuse IID_ITfContextOwner (measured). */
    HWND ResolveControl(ITfContext *pic) {
        if (interimHwnd_)
            return interimHwnd_;
        HWND hwnd = GetFocus();
        if (!hwnd && pic) {
            ITfContextOwner *owner = nullptr;
            if (SUCCEEDED(pic->QueryInterface(IID_ITfContextOwner, (void **)&owner)) && owner) {
                owner->GetWnd(&hwnd);
                owner->Release();
            }
        }
        interimHwnd_ = hwnd;
        hunmin_logger.log("interim: control hwnd=%p\n", (void *)hwnd);
        return hwnd;
    }

    /* The ONE write primitive: the doc span becomes exactly `text` —
         * one select, one EM_REPLACESEL. */
    void ReplaceSpan(HWND hwnd, const std::wstring &text) {
        EmSelect(hwnd, preStart_, preStart_ + (LONG)preLen_);
        EmReplace(hwnd, text.c_str());
        preLen_ = text.size();
        hunmin_logger.log("interim: span now %zu chars at %ld (caret=%ld doclen=%d focused=%d)\n",
                          text.size(), preStart_, EmCaret(hwnd),
                          GetWindowTextLengthW(hwnd), GetFocus() == hwnd);
    }

    /* A host termination fused the composition's text into the doc:
         * delete that dead copy and re-anchor the span where it sat. The
         * caret may still be unsettled when the termination fires
         * (measured both states); an unremovable copy is dropped and the
         * fresh-span anchor recovers. */
    void CaptureDeadCopy(const std::wstring &copy) {
        pendingDeadCopy_ = copy;
        preStart_ = 0;
        preLen_ = 0;
        Post(kPendingWrite);
    }
    void RemoveDeadCopy(HWND hwnd) {
        if (pendingDeadCopy_.empty())
            return;
        LONG caret = EmCaret(hwnd);
        LONG span = (LONG)pendingDeadCopy_.size();
        if (caret >= span) {
            EmSelect(hwnd, caret - span, caret);
            EmReplace(hwnd, L"");
            preStart_ = caret - span;
            hunmin_logger.log("interim: dead copy removed (%ld chars, preStart=%ld)\n",
                              span, preStart_);
        }
        pendingDeadCopy_.clear();
    }

    /* The finalised syllables closed by the engine since the last
         * write, taken out of the buffer. */
    std::wstring TakeFinalisedWord() {
        std::wstring f;
        f.swap(svc->wordbuf_);
        return f;
    }

    /* The interim consumed path: the newly finalised syllables land in
         * the document AHEAD of the in-flight span, then the span becomes
         * the engine's current preedit. */
    void InterimWrite() {
        HWND hwnd = ResolveControl(nullptr);
        if (!hwnd)
            return;
        RemoveDeadCopy(hwnd);
        /* A fresh word (no span in the doc) anchors at the live caret:
             * the app may have inserted passthrough text (the boundary
             * space) since the previous word closed. */
        if (preLen_ == 0)
            preStart_ = EmCaret(hwnd);
        std::wstring finalised = TakeFinalisedWord();
        if (!finalised.empty()) {
            EmSelect(hwnd, preStart_, preStart_);
            EmReplace(hwnd, finalised.c_str());
            preStart_ += (LONG)finalised.size();
            hunmin_logger.log("interim: appended %zu finalised chars\n", finalised.size());
        }
        std::wstring pre = HunminUtf8ToWide(svc->ic_ ? hunmin_ic_preedit(svc->ic_) : "");
        if (!pre.empty() || preLen_ > 0)
            ReplaceSpan(hwnd, pre);
    }

    /* The interim boundary drain: the in-flight span becomes the word
         * (never an insert at the anchor — the word is ALREADY document
         * text when the span is closed), then the bookkeeping closes and
         * the engine state drops. The drain runs from the TIMER, i.e.
         * AFTER the app inserted its boundary space and parked its own
         * caret: measured live, the span's EmSelect left the app's caret
         * INSIDE the word ahead of the space, so the following keys
         * landed before the space and the backspace deleted the wrong
         * char. The app's caret is not ours to move — saved and given
         * back. */
    void InterimDrain() {
        HWND hwnd = ResolveControl(nullptr);
        std::wstring word;
        word.swap(svc->wordbuf_);
        LONG userCaret = hwnd ? EmCaret(hwnd) : 0;
        if (hwnd && !word.empty() && preLen_ > 0) {
            ReplaceSpan(hwnd, word);
            preStart_ += (LONG)word.size();
        }
        preLen_ = 0;
        if (svc->ic_)
            hunmin_ic_reset(svc->ic_);
        if (hwnd)
            EmSelect(hwnd, userCaret, userCaret);
        hunmin_logger.log("interim boundary: word len=%zu preStart=%ld caret=%ld\n",
                          word.size(), preStart_, userCaret);
    }

    /* The class-aware EM primitives: EM_EXSETSEL/CHARRANGE for the
         * msftedit richedit, EM_SETSEL for plain EDIT. */
    enum { kEmRich = 0,
           kEmPlain = 1 };
    int ControlKind(HWND hwnd) {
        wchar_t cls[64] = {0};
        GetClassNameW(hwnd, cls, 64);
        return (wcsstr(cls, L"RICHEDIT") != nullptr) ? kEmRich : kEmPlain;
    }
    void EmSelect(HWND hwnd, LONG start, LONG end) {
        if (ControlKind(hwnd) == kEmRich) {
            CHARRANGE cr;
            cr.cpMin = start;
            cr.cpMax = end;
            SendMessageW(hwnd, EM_EXSETSEL, 0, (LPARAM)&cr);
        } else {
            SendMessageW(hwnd, EM_SETSEL, (WPARAM)start, (LPARAM)end);
        }
    }
    LONG EmCaret(HWND hwnd) {
        if (ControlKind(hwnd) == kEmRich) {
            CHARRANGE cr = {0, 0};
            SendMessageW(hwnd, EM_EXGETSEL, 0, (LPARAM)&cr);
            return (LONG)cr.cpMax;
        }
        DWORD a = 0, b = 0;
        SendMessageW(hwnd, EM_GETSEL, (WPARAM)&a, (LPARAM)&b);
        return (LONG)b;
    }
    void EmReplace(HWND hwnd, const wchar_t *t) {
        SendMessageW(hwnd, EM_REPLACESEL, FALSE, (LPARAM)t);
    }
};
