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

namespace {
/* The interim path writes the document through EDIT-family window messages
 * (EM_REPLACESEL), so the host test that matters is the CONTROL CLASS, not a
 * guess about the host's TSF support: EDIT, RichEdit20W, RICHEDIT50W. */
bool IsClassicEditHost() {
    HWND focus = GetFocus();
    if (!focus)
        return false;
    wchar_t cls[64] = {0};
    if (!GetClassNameW(focus, cls, 64))
        return false;
    return _wcsnicmp(cls, L"EDIT", 4) == 0 || _wcsnicmp(cls, L"RICHEDIT", 8) == 0;
}
} /* namespace */

STDMETHODIMP CTextService::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition) {
    (void)ecWrite;
    (void)pComposition;
    hunmin_logger.log("OnCompositionTerminated comp=%p display=%zu self=%d\n", (void *)composition_, ComposeDisplay().size(), endingSelf_ ? 1 : 0);
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
    /* Classify the host before choosing the recovery. The interim path's
     * doc writes are EDIT-family window messages (EM_REPLACESEL), so it can
     * serve only a host whose control IS one of those. Measured 2026-10-28
     * (issue #144): Chromium terminates a composition ONCE, mid-word, while
     * its window is settling, and the focused window there is not an edit
     * control at all - the flip turned every later syllable into a write
     * into nothing (`interim: span now ... doclen=0`), while the SAME host
     * composed the whole word on the native path, leaving only the fused
     * copy behind (`ㅇ안녕 `). So:
     *   - an EDIT-family control => the per-key terminator #119 measured:
     *     flip to the interim path (unchanged);
     *   - anything else => a TSF-capable host whose termination is not a
     *     verdict about its control: stay native and hand the fused copy to
     *     the next composition, which REPLACES it instead of appending
     *     after it. */
    if (IsClassicEditHost()) {
        hunmin_logger.log("legacy detection: mid-word host termination (compText=%zu display=%zu) -> interim path\n", compText_.size(),
                          ComposeDisplay().size());
        legacyMode_ = true;
        UseDelivery(HUNMIN_TSF_DELIVERY_INTERIM);
        /* This host can no longer carry the automatic switch (ruled 2026-11-02):
         * the layer is told, so the option and the key path agree. */
        ApplyAutoswitch();
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
    } else {
        hunmin_logger.log("native recovery: mid-word host termination (compText=%zu) on a non-edit host -> native path, re-adopt the fused copy\n",
                          compText_.size());
        hostFusedCopy_ = compText_;
    }
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
                    /* A host that fused our last composition into its document
                     * (issue #144) keeps that copy: start this composition one
                     * range earlier so its first write REPLACES the copy
                     * instead of appending after it. */
                    svc_->AdoptHostFusedCopy(ec, sel.range);
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
                /* Publish the shown text BEFORE the host-visible writes
                 * (issue #222): the sink can fire re-entrantly from
                 * inside them, and it records what the composition
                 * showed. Publishing after left it holding the previous
                 * key's text - the next composition re-adopted the wrong
                 * copy, the verify refused it, and the key APPENDED
                 * (26 chars for a 7-key word). */
                svc_->compText_ = t_;
                hr = range->SetText(ec, 0, t_, -1);
                svc_->displayAttr_->Apply(ctx_, ec, range, endAfter_);
                /* Caret: the COLLAPSE must never touch the composition's
                 * own range (escaping a collapsed-at-end range would
                 * make the next rewrite splice AFTER the text instead
                 * of replacing it). Write mode keeps the selection on
                 * the composition; end mode walks a clone to the end.
                 * A host that ended the composition from inside this
                 * session (the sink ran above) gets the same clone
                 * collapse: the next session's GetSelection must be a
                 * caret after the fused copy - the shape re-adoption
                 * (#144) measures - and selecting the dead range again
                 * hands it the non-collapsed selection instead. */
                const bool hostEndedMidSession = (svc_->composition_ == nullptr);
                if (endAfter_ || hostEndedMidSession) {
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
            /* NO assignment of compText_ after the writes: if the sink
             * ran it already moved the shown text into hostFusedCopy_
             * and cleared compText_, and re-asserting t_ would resurrect
             * a shown text with no composition behind it (issue #222). */
            if (endAfter_) {
                /* Issue #119: our own end is flagged so the sink cannot
                 * mistake it for a host-driven termination (measured:
                 * the sink has never fired for a service-called
                 * EndComposition - the wrap is protection, not assumed
                 * behaviour). A host that already ended the composition
                 * mid-session fused this final text itself, which is the
                 * same committed state: there is nothing of ours left to
                 * end, and the end below must not run on a null
                 * composition. */
                if (svc_->composition_) {
                    svc_->endingSelf_ = true;
                    svc_->composition_->EndComposition(ec);
                    hunmin_logger.log("composition end text=%ls\n", t_);
                    svc_->composition_->Release();
                    svc_->composition_ = nullptr;
                    if (svc_->compCtx_) {
                        svc_->compCtx_->Release();
                        svc_->compCtx_ = nullptr;
                    }
                }
                /* The final text is committed, whichever side ended the
                 * composition: a pending fused copy here is stale, and
                 * the next composition must not re-adopt the word the
                 * boundary just committed. */
                svc_->hostFusedCopy_.clear();
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

/* Take back the text a non-edit host fused into its document when it
 * terminated our composition (issue #144). Returns true when the selection
 * range now starts at that copy; the document is then the arbiter, never the
 * bookkeeping: the range's own text must BE the copy, because the caret is
 * the app's to move and a blind backward shift would eat unrelated text. The
 * copy is consumed either way - a host that moved the caret keeps the residue
 * (the pre-#144 failure) instead of losing a character. */
bool CTextService::AdoptHostFusedCopy(TfEditCookie ec, ITfRange *sel) {
    std::wstring want;
    want.swap(hostFusedCopy_);
    if (want.empty() || !sel)
        return false;
    const LONG len = (LONG)want.size();
    /* The range the host hands back is normally a caret AFTER the fused
     * copy (the #144 shape), but a raced host can leave the terminated
     * composition's range selected, or the caret inside the copy. Measure
     * what the range already covers - GetText with capacity n reports
     * min(range length, n) - and shift the start only by what the copy
     * still needs: the copy ends at the range's END, so the target start
     * is end-len. The document's own text stays the arbiter. */
    LONG shift = -len;
    std::wstring probe((size_t)len + 1, L'\0');
    ULONG covered = 0;
    if (SUCCEEDED(sel->GetText(ec, 0, &probe[0], (ULONG)len + 1, &covered))) {
        if (covered == (ULONG)len && wcsncmp(probe.c_str(), want.c_str(), want.size()) == 0) {
            /* The selection already covers the copy exactly. */
            hunmin_logger.log("native recovery: re-adopted %ld fused chars into the next composition\n", len);
            return true;
        }
        if (covered > (ULONG)len) {
            /* The range outgrows the copy: it cannot be positioned onto
             * the copy by this protocol, and a -len shift here would
             * only be accepted when unrelated text happens to match. */
            hunmin_logger.log("native recovery: re-adopt refused (selection outgrows the fused copy)\n");
            return false;
        }
        if (covered == (ULONG)len)
            shift = 0; /* same length, different text: the verify must see it */
        else
            shift = (LONG)covered - len;
    }
    LONG moved = 0;
    if (FAILED(sel->ShiftStart(ec, shift, &moved, nullptr)) || moved != shift) {
        hunmin_logger.log("native recovery: re-adopt refused (shift moved=%ld of %ld)\n", moved, shift);
        return false;
    }
    std::wstring have(want.size() + 1, L'\0');
    ULONG got = 0;
    const HRESULT hr = sel->GetText(ec, 0, &have[0], (ULONG)want.size(), &got);
    if (FAILED(hr) || got != (ULONG)want.size() || wcsncmp(have.c_str(), want.c_str(), want.size()) != 0) {
        /* Not ours to take: give the anchor back where it was. */
        sel->ShiftStart(ec, moved, nullptr, nullptr);
        hunmin_logger.log("native recovery: re-adopt refused (hr=0x%lX got=%lu of %ld: the document does not carry the fused copy)\n",
                          (unsigned long)hr, (unsigned long)got, len);
        return false;
    }
    hunmin_logger.log("native recovery: re-adopted %ld fused chars into the next composition\n", len);
    return true;
}

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
    /* ...except when the HOST already put that same text in the document: its
     * own composition termination fuses what the composition showed and the
     * layer is deliberately not told (the ic keeps composing, issue #144). If a
     * boundary arrives before a new composition re-adopts that copy, the insert
     * below writes a SECOND copy of the same word -- the reported
     * `적어두자.적어두자.` (issue #175 comment, 2026-11-01). The fused copy IS the
     * layer's display by construction (`hostFusedCopy_ = compText_` at the
     * termination), so the layer's copy is dropped rather than delivered: ReqMode10
     * forbids rewriting text the client already holds, and any judgement that
     * wanted to rewrite it is cleared with the same reset. */
    if (!hostFusedCopy_.empty()) {
        hunmin_logger.log(" boundary: the host fused %zu chars, dropping the layer's copy\n", hostFusedCopy_.size());
        hostFusedCopy_.clear();
        ComposeReset();
        return;
    }
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
    /* The delivery that keeps text in the document owns HOW it lands there:
     * the interim path needs its span bookkeeping advanced, and writing at
     * the caret instead is what truncated the word under the syllable unit
     * (measured, tier F1 LegacyTest.CommitUnitSyllable*). */
    if (delivery_ && delivery_->OwnsDocumentText()) {
        delivery_->InsertText(pic, text);
        return;
    }
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
std::wstring CTextService::ComposeDisplay() { return preedit_ ? HunminUtf8ToWide(hunmin_buffered_preedit_display(preedit_)) : std::wstring(); }

std::wstring CTextService::ComposeInFlight() { return preedit_ ? HunminUtf8ToWide(hunmin_buffered_preedit_composing(preedit_)) : std::wstring(); }

std::wstring CTextService::TakeFinishedWord() { return preedit_ ? HunminUtf8ToWide(hunmin_buffered_preedit_take_word(preedit_)) : std::wstring(); }

void CTextService::ComposeReset() {
    if (preedit_)
        hunmin_buffered_preedit_reset(preedit_);
}

/* One composition-boundary drain shared by the declined key path and the mode
 * toggle (issue #66). The finalising is the LAYER's now (issue #128): its
 * commit_word() closes the in-flight syllable and hands back the whole word,
 * which InsertBufferedWord delivers once.
 *
 * ReqMode8's judgement is taken HERE, after the delivery and before the next key
 * (issue #175). It has to be this drain and not only the declined-key path: a
 * boundary key on TSF (space, Enter) is settled in OnTestKeyDown and PASSED, so
 * the host never calls the down phase and the declined branch never runs --
 * measured with tier F1's TsfConfigTest.TheAutomaticSwitch* before this call
 * existed, the mode never moved. take_judgement is consuming, so the redundant
 * call on the declined path costs one and does nothing. */
void CTextService::BoundaryFlush(ITfContext *pic) {
    hunmin_logger.log(" boundary, display=%zu\n", ComposeDisplay().size());
    delivery_->Boundary(pic);
    ApplyJudgement(pic);
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
    hunmin_logger.log("caret: preedit=%d have=%d hwndCaret=%p rc=(%ld,%ld,%ld,%ld) hrRect=0x%lX\n", hasPreeditRect_ ? 1 : 0, haveCaret ? 1 : 0,
                      (void *)gti.hwndCaret, (long)gti.rcCaret.left, (long)gti.rcCaret.top, (long)gti.rcCaret.right, (long)gti.rcCaret.bottom,
                      (unsigned long)hrRect);
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
