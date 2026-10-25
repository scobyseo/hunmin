/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The delivery seam's implementations (issue #119): NativeDelivery (the
 * #98 composition path) and InterimDelivery (the #119 interim path for
 * classic default-sink hosts, the deferral machinery included). Defined
 * against the TsfCore slot and registered as factories into the shell
 * (DIP): this TU knows nothing of CTextService.
 *
 * The interim deferral measurement: the EM_* writes must not run INSIDE a
 * TSF callback - the first inline write fired a re-entrant
 * ITfThreadMgrEventSink::OnSetFocus(NULL) storm (msftedit's TSF store
 * settling the out-of-band selection change) and every keystroke sent
 * while it ran was lost before reaching OnTestKeyDown. So the write is
 * POSTED and runs on the app's UI thread from a thread timer, or at the
 * ENTRY of the next key event, whichever comes first.
 */
#include "delivery.h"

#include "utf.h"

#include <richedit.h> /* CHARRANGE, EM_EXSETSEL - the class-aware EM writes */
#include <windows.h>

/* ---- NativeDelivery: the #98 composition path ---- */

class NativeDelivery : public Delivery {
public:
    explicit NativeDelivery(TsfCore &core) : core_(&core) {}
    void Consumed(ITfContext *pic) override { core_->CompositionWrite(pic); }
    void Boundary(ITfContext *pic) override { core_->InsertBufferedWord(pic); }
    std::string CandidateAnchor() override {
        /* The whole buffered word: accumulated finalised syllables plus
         * the in-flight preedit - the multi-syllable lookup the
         * dictionary tables are keyed by (krdict words, idioms).
         * fcitx5 anchors to its preedit only because it commits each
         * syllable immediately. */
        return HunminWideToUtf8(core_->ComposeDisplay());
    }
    bool CandidateCommit(ITfContext *pic, const std::wstring &replacement) override {
        core_->ComposeReset(); /* drop the composition the list anchored to */
        /* The chosen Hanja is not the layer's word, so it does not go through
         * the layer: the composition is ended WITH it, which replaces the
         * composed text in place instead of appending after it. */
        core_->InsertText(pic, replacement);
        return true;
    }
    void FlushPending() override {}
    void CaptureDeadCopy(const std::wstring &) override {}
    TsfCore *core_;
};

/* ---- InterimDelivery: the #119 interim path ----
 *
 * The doc model: [finalised word][in-flight span]; the span
 * [preStart_, preStart_+preLen_) is the ONLY text the mode rewrites,
 * always through ONE select + ONE EM_REPLACESEL (ReplaceSpan) -
 * piecewise span edits accumulate (measured: 1+1+1+2+2+2 chars for
 * 7 keys; jamotong_ime's manual section 8.2, artifact ㄱ가간...).
 * NO overlay window (maintainer instruction 2026-10-20). The styled
 * composition underline is NOT reproduced on this path (fInterimChar
 * selection: SetSelection E_INVALIDARG measured on both host
 * classes) - plain-text preedit is the accepted shape
 * (docs/TSF-INTERIM-INPUT-SPIKE.md 4-6).
 */
class InterimDelivery : public Delivery {
public:
    explicit InterimDelivery(TsfCore &core) : core_(&core) {}

    HWND interimHwnd_ = nullptr; /* the edit control (resolved once) */
    LONG preStart_ = 0; /* doc offset where the span begins */
    size_t preLen_ = 0; /* the span's current char count */
    std::wstring pendingDeadCopy_; /* a fused copy awaiting deletion */
    std::wstring pendingReplacement_; /* a chosen Hanja replacing the span */
    enum PendingWrite { kPendingNone, kPendingWrite, kPendingDrain };
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
    void FlushPending() override { Dispatch(); }
    void CaptureDeadCopy(const std::wstring &copy) override {
        pendingDeadCopy_ = copy;
        preStart_ = 0;
        preLen_ = 0;
        Post(kPendingWrite);
    }
    /* The one-char ruling (maintainer, 2026-10-20: "in interim mode,
     * only one char conversion should be supported"): the finalised
     * syllables are document text outside the span the mode rewrites,
     * so the anchor is the IN-FLIGHT preedit only, and a candidate
     * that is not exactly one character is refused whole - the
     * whole-word anchor's multi-char candidates cannot commit without
     * corrupting the word (measured: 안녕 + 安寧 → 안安寧). */
    std::string CandidateAnchor() override {
        return HunminWideToUtf8(core_->ComposeInFlight());
    }
    bool CandidateCommit(ITfContext *pic, const std::wstring &replacement) override {
        (void)pic;
        if (replacement.size() != 1 || preLen_ == 0)
            return false; /* refused: the word stays intact */
        core_->ComposeReset(); /* drop the syllable the list anchored to */
        pendingReplacement_ = replacement; /* the drain rewrites the span to it */
        Post(kPendingDrain);
        return true;
    }

    bool OwnsDocumentText() const override { return true; }

    /* A commit arriving from the engine (the syllable unit delivers each
     * finalised syllable here; the buffered unit delivers only at the
     * boundary). The text REPLACES the span's current content and the span
     * advances past it, so the in-flight syllable lands after the delivered
     * text instead of on top of it. Measured defect before this: with the
     * per-syllable unit the document ended as the LAST syllable alone
     * ("안녕" typed, "녕" in the control) because the native InsertText path
     * wrote at the caret while the span still pointed at that range. */
    void InsertText(ITfContext *pic, const std::wstring &text) override {
        if (text.empty())
            return;
        HWND hwnd = ResolveControl(pic);
        if (!hwnd)
            return;
        RemoveDeadCopy(hwnd);
        if (preLen_ == 0 && preStart_ < 0)
            preStart_ = EmCaret(hwnd);
        ReplaceSpan(hwnd, text);
        preStart_ += (LONG)text.size();
        preLen_ = 0;
        hunmin_logger.log("interim: delivered %zu chars, span moves to %ld\n", text.size(), preStart_);
    }

    void Post(PendingWrite kind) {
        pendingWriteKind_ = kind;
        s_writer = this;
        SetTimer(nullptr, kWriteTimerId, 10, WriteTimerProc);
    }
    /* Runs the queued doc write. Called from the timer and from the
     * key events' entry (the mock tests drive it directly - they never
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
     * the app's input thread, so GetFocus() is it - default-sink
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

    /* The ONE write primitive: the doc span becomes exactly `text` -
     * one select, one EM_REPLACESEL. */
    void ReplaceSpan(HWND hwnd, const std::wstring &text) {
        EmSelect(hwnd, preStart_, preStart_ + (LONG)preLen_);
        EmReplace(hwnd, text.c_str());
        preLen_ = text.size();
        hunmin_logger.log("interim: span now %zu chars at %ld (caret=%ld doclen=%d focused=%d)\n", text.size(), preStart_, EmCaret(hwnd),
                          GetWindowTextLengthW(hwnd), GetFocus() == hwnd);
    }

    /* A host termination fused the composition's text into the doc:
     * delete that dead copy and re-anchor the span where it sat. The
     * caret may still be unsettled when the termination fires
     * (measured both states); an unremovable copy is dropped and the
     * fresh-span anchor recovers. */
    void RemoveDeadCopy(HWND hwnd) {
        if (pendingDeadCopy_.empty())
            return;
        LONG caret = EmCaret(hwnd);
        LONG span = (LONG)pendingDeadCopy_.size();
        if (caret >= span) {
            EmSelect(hwnd, caret - span, caret);
            EmReplace(hwnd, L"");
            preStart_ = caret - span;
            hunmin_logger.log("interim: dead copy removed (%ld chars, preStart=%ld)\n", span, preStart_);
        }
        pendingDeadCopy_.clear();
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
        std::wstring finalised = core_->TakeFinishedWord();
        if (!finalised.empty()) {
            EmSelect(hwnd, preStart_, preStart_);
            EmReplace(hwnd, finalised.c_str());
            preStart_ += (LONG)finalised.size();
            hunmin_logger.log("interim: appended %zu finalised chars\n", finalised.size());
        }
        std::wstring pre = core_->ComposeInFlight();
        if (!pre.empty() || preLen_ > 0)
            ReplaceSpan(hwnd, pre);
    }

    /* The interim boundary drain: the in-flight span becomes the word
     * (never an insert at the anchor - the word is ALREADY document
     * text when the span is closed), then the bookkeeping closes and
     * the engine state drops. The drain runs from the TIMER, i.e.
     * AFTER the app inserted its boundary space and parked its own
     * caret: measured live, the span's EmSelect left the app's caret
     * INSIDE the word ahead of the space, so the following keys
     * landed before the space and the backspace deleted the wrong
     * char. The app's caret is not ours to move - saved and given
     * back. */
    void InterimDrain() {
        HWND hwnd = ResolveControl(nullptr);
        std::wstring word = pendingReplacement_;
        pendingReplacement_.clear();
        if (word.empty())
            word = core_->TakeFinishedWord();
        LONG userCaret = hwnd ? EmCaret(hwnd) : 0;
        if (hwnd && !word.empty() && preLen_ > 0) {
            ReplaceSpan(hwnd, word);
            preStart_ += (LONG)word.size();
        }
        preLen_ = 0;
        core_->ComposeReset();
        if (hwnd)
            EmSelect(hwnd, userCaret, userCaret);
        hunmin_logger.log("interim boundary: word len=%zu preStart=%ld caret=%ld\n", word.size(), preStart_, userCaret);
    }

    /* The class-aware EM primitives: EM_EXSETSEL/CHARRANGE for the
     * msftedit richedit, EM_SETSEL for plain EDIT. */
    enum { kEmRich = 0, kEmPlain = 1 };
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
    void EmReplace(HWND hwnd, const wchar_t *t) { SendMessageW(hwnd, EM_REPLACESEL, FALSE, (LPARAM)t); }
    TsfCore *core_;
};

InterimDelivery *InterimDelivery::s_writer = nullptr;

namespace {
class NativeDeliveryFactory : public HunminDeliveryFactory {
public:
    const char *Name() const override { return HUNMIN_TSF_DELIVERY_NATIVE; }
    Delivery *Create(TsfCore &core) override { return new NativeDelivery(core); }
};
class InterimDeliveryFactory : public HunminDeliveryFactory {
public:
    const char *Name() const override { return HUNMIN_TSF_DELIVERY_INTERIM; }
    Delivery *Create(TsfCore &core) override { return new InterimDelivery(core); }
};
} /* namespace */

void hunmin_tsf_deliveries_register(HunminDeliveryRegistry &registry) {
    static NativeDeliveryFactory nativeFactory;
    static InterimDeliveryFactory interimFactory;
    registry.RegisterDeliveryFactory(&nativeFactory);
    registry.RegisterDeliveryFactory(&interimFactory);
}
