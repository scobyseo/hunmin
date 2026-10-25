/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * composition.cpp - the native composition preedit machinery (issue #98)
 * and the boundary/caret edit sessions, relocated from tsf.cpp unchanged.
 * The edit-session classes are nested members of CTextService; defining
 * them here keeps the shell's declaration in tsf.h and the code in one
 * place. Delivery reaches the shell through its seam, the styling through
 * DisplayAttr, and the candidate flow through CaretRect.
 */
#include "tsf.h"

#include "delivery.h" /* HUNMIN_TSF_DELIVERY_INTERIM, CaptureDeadCopy */
#include "displayattr.h" /* DisplayAttr::Apply */
#include "utf.h" /* HunminUtf8ToWide */

#include <string>

STDMETHODIMP CTextService::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition) {
    (void)ecWrite;
    (void)pComposition;
    hunmin_logger.log("OnCompositionTerminated comp=%p display=%zu self=%d\n", (void *)composition_, ComposeDisplay().size(),
                      endingSelf_ ? 1 : 0);
    if (endingSelf_) {
        /* Our own EndComposition (issue #98): the composed text is
         * document text by OUR decision, so the buffers drain and the
         * engine state resets - the same word must never be delivered
         * twice through the next boundary. */
        if (composition_) {
            composition_->Release();
            composition_ = nullptr;
        }
        if (compCtx_) {
            compCtx_->Release();
            compCtx_ = nullptr;
        }
        compText_.clear();
        ComposeReset();
        return S_OK;
    }
    /* Host-driven termination. A modern host terminates only at real
     * boundaries (focus loss, user click-away); when the state is
     * mid-word - the in-flight text was composed but the word is not
     * finished - the host is a classic (default-sink) one: the #119
     * measurement (rounds 11-14; the baseline round in
     * docs/TSF-INTERIM-INPUT-SPIKE.md) shows these hosts terminate per
     * key. Flip THIS SERVICE to the interim path for its life and
     * recover what this termination fused:
     *   - the engine's state is NOT ours to drop: the in-flight
     *     syllable is still live in the ic; the doc now holds a DEAD
     *     COPY of what the composition showed (compText_) - delete
     *     that copy from the control (EM), and the ic keeps composing
     *     from the same jamo state (no reset, no re-feed needed). */
    if (ComposeDisplay().empty() && compText_.empty()) {
        /* nothing mid-word: a real boundary under our own bookkeeping
         * must have run already - plain cleanup, old contract. */
        if (composition_) {
            composition_->Release();
            composition_ = nullptr;
        }
        if (compCtx_) {
            compCtx_->Release();
            compCtx_ = nullptr;
        }
        compText_.clear();
        return S_OK;
    }
    hunmin_logger.log("legacy detection: mid-word host termination (compText=%zu display=%zu) -> interim path\n", compText_.size(),
                      ComposeDisplay().size());
    legacyMode_ = true;
    UseDelivery(HUNMIN_TSF_DELIVERY_INTERIM);
    /* What is dead in the doc = the WHOLE composition text (compText_:
     * nothing under this composition was ours to keep there - the
     * interim path rebuilds the word from the LIVE buffers and live
     * ic state). Delete the dead copy at the POSTED write - never
     * inline here: this handler runs inside the host's TSF callback,
     * where the EM writes fire the re-entrant focus storm that eats
     * keystrokes (measured). One path for every caret state: the
     * write re-measures the caret when the app's queue has settled
     * (the fused copy's caret was measured BOTH behind the span and
     * past it, settling differently per host). The ic keeps composing
     * from the same jamo state (no reset) and the layer keeps the
     * finalised syllables so the write re-appends them to the doc. */
    delivery_->CaptureDeadCopy(compText_);
    /* A write posted by the terminated key is superseded: the host has
     * fused the composition text itself and the re-bookkeeping above
     * recomputes the span. The posted write runs as THIS flip's dead
     * copy removal instead. */
    delivery_->CaptureDeadCopy(compText_);
    if (composition_) {
        composition_->Release();
        composition_ = nullptr;
    }
    if (compCtx_) {
        compCtx_->Release();
        compCtx_ = nullptr;
    }
    compText_.clear();
    return S_OK;
}

/* ---- Native composition preedit (issue #98): machinery ----
 *
 * The engine's buffers reach the DOCUMENT while typing: the whole
 * buffered word (finalised syllables + in-flight preedit, issue #73)
 * sits in an ITfComposition started on the caret's empty selection
 * range, carried toward the app through one edit session per engine
 * transition, and underlined with GUID_PROP_ATTRIBUTE (the
 * display-attribute atom registered under HunminDisplayAttrGuid, the
 * attribute the provider serves). A word boundary ends the
 * composition: the text fuses into the document as ordinary text and
 * the buffers empty.
 */

/* One edit session makes the composition exist (if not yet) and shows
 * the whole buffered word in it, underlined. The nested member class
 * reaches the shell's private state through svc_ (the definition outside
 * the class keeps the session code where it was; the access rules of a
 * nested member class apply). */
class CTextService::CompositionSession : public ITfEditSession {
public:
    CompositionSession(ITfContext *ctx, CTextService *svc, TfClientId cid, const wchar_t *t, bool endAfter)
        : ref_(1), ctx_(ctx), svc_(svc), cid_(cid), t_(t), endAfter_(endAfter) {}
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppv = this;
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
    STDMETHOD(DoEditSession)(TfEditCookie ec) override {
        HRESULT hr = S_OK;
        if (!svc_->composition_) {
            /* First preedit key in this context: the empty selection
             * range becomes the composition. A host that refuses the
             * interface leaves composition_ null, and the caller's
             * boundary path then falls back to the plain insert. */
            ITfContextComposition *cc = nullptr;
            if (SUCCEEDED(ctx_->QueryInterface(IID_ITfContextComposition, (void **)&cc)) && cc) {
                TF_SELECTION sel;
                ULONG fetched = 0;
                if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) && fetched == 1 && sel.range) {
                    HRESULT hs = cc->StartComposition(ec, sel.range, static_cast<ITfCompositionSink *>(svc_), &svc_->composition_);
                    if (SUCCEEDED(hs) && svc_->composition_) {
                        svc_->compCtx_ = ctx_;
                        ctx_->AddRef();
                    }
                    hunmin_logger.log("composition start hr=0x%lX comp=%p\n", (unsigned long)hs, (void *)svc_->composition_);
                    sel.range->Release();
                }
                cc->Release();
            }
        }
        if (svc_->composition_) {
            ITfRange *range = nullptr;
            if (SUCCEEDED(svc_->composition_->GetRange(&range)) && range) {
                /* An in-composition rewrite may CLEAR the range: an
                 * engine backspace can empty both buffers while the
                 * composition lives, and skipping the SetText left the
                 * stale preedit visible (measured at the seosi trace's
                 * backspace, issue #98; a real host shows the same
                 * ghost). Empty fuses nothing if the boundary ends it
                 * next. */
                hr = range->SetText(ec, 0, t_, -1);
                svc_->displayAttr_->Apply(ctx_, ec, range, endAfter_);
                /* Caret: the COLLAPSE must never touch the composition's
                 * own range (escaping a collapsed-at-end range would
                 * make the next rewrite splice AFTER the text instead
                 * of replacing it). Write mode keeps the selection on
                 * the composition; end mode walks a clone to the end. */
                if (endAfter_) {
                    ITfRange *caret = nullptr;
                    if (SUCCEEDED(range->Clone(&caret)) && caret) {
                        caret->Collapse(ec, TF_ANCHOR_END);
                        TF_SELECTION cs = {};
                        cs.range = caret;
                        cs.style.ase = TF_AE_END;
                        cs.style.fInterimChar = FALSE;
                        ctx_->SetSelection(ec, 1, &cs);
                        caret->Release();
                    }
                } else {
                    TF_SELECTION ns = {};
                    ns.range = range;
                    ns.style.ase = TF_AE_END;
                    ns.style.fInterimChar = FALSE;
                    ctx_->SetSelection(ec, 1, &ns);
                }
                range->Release();
            }
            svc_->compText_ = t_;
            if (endAfter_) {
                /* Issue #119: our own end is flagged so the sink cannot
                 * mistake it for a host-driven termination (measured:
                 * the sink has never fired for a service-called
                 * EndComposition - the wrap is protection, not assumed
                 * behaviour). */
                svc_->endingSelf_ = true;
                svc_->composition_->EndComposition(ec);
                hunmin_logger.log("composition end text=%ls\n", t_);
                svc_->composition_->Release();
                svc_->composition_ = nullptr;
                if (svc_->compCtx_) {
                    svc_->compCtx_->Release();
                    svc_->compCtx_ = nullptr;
                }
                svc_->compText_.clear();
                svc_->endingSelf_ = false;
            }
        }
        return S_OK;
    }

private:
    LONG ref_;
    ITfContext *ctx_;
    CTextService *svc_;
    TfClientId cid_;
    const wchar_t *t_;
    bool endAfter_;
};

/* The write half of the composition lifecycle: one edit session makes
 * the composition exist (if not yet) and shows the whole buffered word
 * in it, underlined. Called after every engine transition that a bound
 * key drives. */
void CTextService::CompositionWrite(ITfContext *pic) {
    if (!pic)
        return;
    std::wstring full = ComposeDisplay();
    if (full.empty() && !composition_)
        return;
    CompositionSession *cs = new CompositionSession(pic, this, clientId_, full.c_str(), false);
    HRESULT hrC = S_OK;
    pic->RequestEditSession(clientId_, cs, TF_ES_SYNC | TF_ES_READWRITE, &hrC);
    cs->Release();
}

/* The end half: rewrites the composition range to the final text if it
 * differs, drops the display attribute, and calls EndComposition - the
 * composed text fuses into the document as ordinary text. Runs in the
 * composition's own context when called after a context switch. */
void CTextService::CompositionEnd(ITfContext *pic, const std::wstring &text) {
    ITfContext *ctx = (composition_ && compCtx_) ? compCtx_ : pic;
    if (!ctx)
        return;
    CompositionSession *cs = new CompositionSession(ctx, this, clientId_, text.c_str(), true);
    HRESULT hrE = S_OK;
    ctx->RequestEditSession(clientId_, cs, TF_ES_SYNC | TF_ES_READWRITE, &hrE);
    cs->Release();
}

/* A context/focus switch ends the live composition in its own context
 * (the word stays there as committed text); the buffers drain so the
 * next context starts clean - the old 0.2 boundary-drain contract,
 * pointed at the composition instead of the overlay. */
void CTextService::DetachComposition() {
    if (!composition_ || !compCtx_)
        return;
    std::wstring text = HunminUtf8ToWide(hunmin_buffered_preedit_commit_word(preedit_));
    CompositionEnd(compCtx_, text);
}

/* The NATIVE boundary insert (the native half of the delivery seam):
 * with a live composition the word is ALREADY document text (the
 * composition range holds it) - the session ends the composition,
 * fusing the word in unstyled. Only a host that refused
 * ITfContextComposition falls back to the plain selection SetText the
 * 0.2 adapter committed with. */
class CTextService::FlushSession : public ITfEditSession {
public:
    FlushSession(ITfContext *ctx, CTextService *svc, TfClientId cid, const wchar_t *t) : ref_(1), ctx_(ctx), svc_(svc), cid_(cid), t_(t) {}
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppv = this;
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
    STDMETHOD(DoEditSession)(TfEditCookie ec) override {
        TF_SELECTION sel;
        ULONG fetched = 0;
        HRESULT hr = E_NOINTERFACE;
        if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) && fetched == 1 && sel.range) {
            sel.range->SetText(ec, 0, t_, -1);
            sel.range->Collapse(ec, TF_ANCHOR_END);
            TF_SELECTION ns = sel;
            ctx_->SetSelection(ec, 1, &ns);
            sel.range->Release();
            hr = S_OK;
        }
        hunmin_logger.log("flush: insert hr=0x%lX\n", (unsigned long)hr);
        return S_OK;
    }

private:
    LONG ref_;
    ITfContext *ctx_;
    CTextService *svc_;
    TfClientId cid_;
    const wchar_t *t_;
};

void CTextService::InsertBufferedWord(ITfContext *pic) {
    /* The layer's commit: word + in-flight syllable, delivered once. Reading it
     * also clears it, so the word cannot be inserted twice. */
    if (!preedit_)
        return;
    const std::wstring word = HunminUtf8ToWide(hunmin_buffered_preedit_commit_word(preedit_));
    if (word.empty())
        return;
    InsertText(pic, word);
}

/* Insert text that is not the layer's word: a chosen Hanja replacing the
 * composition. With a live composition the text fuses in its place; without
 * one it lands at the selection (a host that refused ITfContextComposition). */
void CTextService::InsertText(ITfContext *pic, const std::wstring &text) {
    if (text.empty())
        return;
    if (composition_) {
        CompositionEnd(pic, text);
        return;
    }
    FlushSession *fs = new FlushSession(pic, this, clientId_, text.c_str());
    HRESULT hrFlush = S_OK;
    if (pic)
        pic->RequestEditSession(clientId_, fs, TF_ES_SYNC | TF_ES_READWRITE, &hrFlush);
    fs->Release();
}

/* The seam's text readers (issue #128): the layer's display and its two
 * halves. All three are reads -- none of them consumes a key. */
std::wstring CTextService::ComposeDisplay() {
    return preedit_ ? HunminUtf8ToWide(hunmin_buffered_preedit_display(preedit_)) : std::wstring();
}

std::wstring CTextService::ComposeInFlight() {
    return preedit_ ? HunminUtf8ToWide(hunmin_buffered_preedit_composing(preedit_)) : std::wstring();
}

std::wstring CTextService::TakeFinishedWord() {
    return preedit_ ? HunminUtf8ToWide(hunmin_buffered_preedit_take_word(preedit_)) : std::wstring();
}

void CTextService::ComposeReset() {
    if (preedit_)
        hunmin_buffered_preedit_reset(preedit_);
}

/* One composition-boundary drain shared by the declined key path and the mode
 * toggle (issue #66). The finalising is the LAYER's now (issue #128): its
 * commit_word() closes the in-flight syllable and hands back the whole word,
 * which InsertBufferedWord delivers once. */
void CTextService::BoundaryFlush(ITfContext *pic) {
    hunmin_logger.log(" boundary, display=%zu\n", ComposeDisplay().size());
    delivery_->Boundary(pic);
}

/* The caret rect the candidate popup anchors to: one read-only edit
 * session against the selection (GetTextExt), falling back to the
 * thread's GUITHREADINFO caret. Screen coordinates out. False means
 * the host reported nothing drawable - the popup/overlay hide path. */
class CTextService::RectSession : public ITfEditSession {
public:
    RectSession(ITfContext *ctx, CTextService *svc) : ref_(1), ctx_(ctx), svc_(svc) {}
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppv = this;
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
    STDMETHOD(DoEditSession)(TfEditCookie ec) override {
        ITfContextView *view = nullptr;
        if (SUCCEEDED(ctx_->GetActiveView(&view)) && view) {
            TF_SELECTION sel;
            ULONG fetched = 0;
            if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) && fetched == 1 && sel.range) {
                RECT rc;
                BOOL clipped = FALSE;
                if (SUCCEEDED(view->GetTextExt(ec, sel.range, &rc, &clipped))) {
                    svc_->preeditRect_ = rc;
                    svc_->hasPreeditRect_ = true;
                }
                sel.range->Release();
            }
            view->Release();
        }
        return S_OK;
    }

private:
    LONG ref_;
    ITfContext *ctx_;
    CTextService *svc_;
};

bool CTextService::CaretRect(ITfContext *pic, RECT *rcOut, bool *haveCaretOut, GUITHREADINFO *gtiOut) {
    hasPreeditRect_ = false;
    RectSession *rs = new RectSession(pic, this);
    HRESULT hrRect = S_OK;
    pic->RequestEditSession(clientId_, rs, TF_ES_SYNC | TF_ES_READ, &hrRect);
    rs->Release();

    GUITHREADINFO gti = {};
    gti.cbSize = sizeof(gti);
    bool haveCaret = GetGUIThreadInfo(GetCurrentThreadId(), &gti) != FALSE && gti.hwndCaret && (gti.rcCaret.bottom > gti.rcCaret.top);
    *haveCaretOut = haveCaret;
    if (gtiOut)
        *gtiOut = gti;
    if (!hasPreeditRect_ && !haveCaret)
        return false;
    RECT rc = hasPreeditRect_ ? preeditRect_ : gti.rcCaret;
    if (haveCaret && gti.hwndCaret) {
        POINT topLeft = {gti.rcCaret.left, gti.rcCaret.top};
        POINT bottomRight = {gti.rcCaret.right, gti.rcCaret.bottom};
        ClientToScreen(gti.hwndCaret, &topLeft);
        ClientToScreen(gti.hwndCaret, &bottomRight);
        rc.left = topLeft.x;
        rc.top = topLeft.y;
        rc.right = bottomRight.x;
        rc.bottom = bottomRight.y;
    }
    *rcOut = rc;
    return true;
}
