/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * tsf.h - the TSF shell's contract header (docs/REFACTOR-TSF-COMPONENTS.md).
 *
 * The shell OWNS the abstractions (DIP): the feature components depend on
 * the interfaces here, never on the shell class, and REGISTER their
 * implementations into the shell:
 *
 *   - delivery.h/.cpp  defines Delivery's implementations ("native",
 *     "interim") and registers both with hunmin_tsf_deliveries_register();
 *     the shell switches modes with UseDelivery().
 *   - candidates.h/.cpp defines the Hanja candidate flow (issue #67) over
 *     the CandBackend seam; hunmin_tsf_hanja_candidates_new() builds it.
 *
 * Including a .cpp is banned in both directions; every component rides its
 * own header. The tier-F1 mock harness links the compiled TUs and reads the
 * shell's state through Snapshot() - the observation channel that replaced
 * the #define-private-public seam.
 */
#ifndef HUNMIN_TSF_TSF_H_
#define HUNMIN_TSF_TSF_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#include <msctf.h>

#include "hunmin.h"
#include "keybinding.h" /* HunminKeyCombo, hunmin_tsf_keybindings_* */
#include "config.h" /* HunminTsfConfig, hunmin_tsf_config_load */
#include "log.h"

/* msctf.h pulls ctfutb.h (the langbar item interfaces); a guard for the
 * connection errors the item's ITfSource answers with - oaidl.h carries
 * these values on the SDK, but not every header chain includes it. */
#ifndef CONNECT_E_NOCONNECTION
#define CONNECT_E_NOCONNECTION ((HRESULT)0x80040200L)
#define CONNECT_E_ADVISELIMIT ((HRESULT)0x80040201L)
#define CONNECT_E_CANNOTCONNECT ((HRESULT)0x80040202L)
#endif

/* Configured layout ids resolved against the loaded data (issue #66). The
 * fcitx5 defaults live in config.cpp; the adapter re-derives them here so a
 * ctx created from a partial data dir still gets a working layout. */
inline constexpr char kHangulDefaultId[] = "sin3-p2";
inline constexpr char kLatinDefaultId[] = "us-qwerty";

/* Accept a stored layout id only when the engine can instantiate it AND its
 * mode matches the slot (issue #71: the dialog lists layouts split by
 * hunmin_ctx_layout_mode, so a stale registry value naming a latin layout in
 * the hangul slot must degrade to the default, never to a silent mispair).
 * Defined in tsf.cpp; the tier-F1 config test drives it directly. */
void hunmin_tsf_resolve_layout_slot(const hunmin_ctx *ctx,
                                    const std::string &stored,
                                    const char *def, bool wantHangul,
                                    std::string *outId);

/* ---- The seams ---- */

/* The tier-F1 observation channel: one call, one struct - the fields the
 * white-box tests read directly before the include-.cpp seam was retired.
 * Read-only; the values are the adapter's live state at call time.
 * Declared before the interfaces: Candidates::Observe references it. */
struct HunminTsfSnapshot {
    hunmin_ctx *ctx = nullptr;
    hunmin_ic *ic = nullptr;
    std::wstring wordbuf;
    bool hangulMode = true;
    bool legacyMode = false;
    bool interimForce = false;
    ITfComposition *composition = nullptr;
    ITfLangBarItemSink *itemSink = nullptr;
    std::vector<HunminKeyCombo> toggleKeys;
    std::string hangulId, latinId;
    bool candActive = false;
    int candCount = 0;
    int candCursor = 0; /* global 0-based cursor */
    int candTop = 0; /* first index of the visible 10-row page */
    bool candFallback = false;
    std::string candSource;
};

/* The slot the shell's behaviour plugs into: what a delivery
 * implementation may drive of the shell. */
struct TsfCore {
    virtual ~TsfCore() = default;
    virtual hunmin_ic *Ic() = 0;
    virtual std::wstring &Wordbuf() = 0;
    virtual void CompositionWrite(ITfContext *pic) = 0;
    virtual void InsertBufferedWord(ITfContext *pic) = 0;
};

/* The mode-delivery seam (issue #119). The COMMON key handling classifies
 * the key and feeds the engine; it never asks which mode it is in. What
 * the two modes DO with the engine's answer is this seam:
 *
 *   - NativeDelivery - the #98 composition path: the word rides a live
 *     ITfComposition (inline start/update/end per edit session). Modern
 *     hosts keep compositions alive (Win11 Notepad, Edge - measured).
 *   - InterimDelivery - the #119 interim path for classic (default-sink)
 *     hosts: NO composition (nothing to terminate), the word is document
 *     text, rewritten per key through the control's own EM_* protocol.
 *
 * A host-driven OnCompositionTerminated that fires MID-WORD flips the
 * service native -> interim for its life; CaptureDeadCopy therefore sits
 * on the interface (the interim mode owns the write; the native mode is a
 * no-op). */
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
    virtual bool CandidateCommit(ITfContext *pic,
                                 const std::wstring &replacement) = 0;
    /* Settle a posted doc write (the interim deferral; the native path
     * is synchronous - a no-op). */
    virtual void FlushPending() = 0;
    /* A host termination fused the composition's text into the doc:
     * the mode decides what to do with the dead copy. */
    virtual void CaptureDeadCopy(const std::wstring &copy) = 0;
};

struct HunminDeliveryFactory {
    virtual ~HunminDeliveryFactory() = default;
    virtual const char *Name() const = 0;
    virtual Delivery *Create(TsfCore &core) = 0;
};

/* The registration interface (DIP): feature implementations register
 * their factories; the shell constructs, owns and switches them. */
struct HunminDeliveryRegistry {
    virtual ~HunminDeliveryRegistry() = default;
    /* Register one factory; the registry keeps the pointer (the factory
     * objects are static in the registering TU). */
    virtual void RegisterDeliveryFactory(HunminDeliveryFactory *factory) = 0;
    /* Construct (first use) and activate the named delivery; returns
     * false when no factory registered under that name. */
    virtual bool UseDelivery(const char *name) = 0;
};

/* The candidate flow's read seam on the shell: the active delivery (the
 * anchor/commit routing is mode-owned), the engine context, the ic, and
 * the caret-rect fetch. */
struct CandBackend {
    virtual ~CandBackend() = default;
    virtual Delivery *ActiveDelivery() = 0;
    virtual hunmin_ctx *EngineContext() = 0;
    virtual hunmin_ic *EngineIc() = 0;
    /* The caret rect shared by the candidate popup: one read-only edit
     * session against the selection (GetTextExt), falling back to the
     * thread's GUITHREADINFO caret. Screen coordinates out. False means
     * the host reported nothing drawable - the popup hide path. */
    virtual bool CaretRect(ITfContext *pic, RECT *rcOut, bool *haveCaretOut,
                           GUITHREADINFO *gtiOut) = 0;
};

/* The candidate flow the shell drives (issue #67): the open/close/motion
 * handlers, the commit that replaces the anchor, and the shared
 * test/down predicate. Single implementation; the shell constructs it
 * through the candidates.h factory. */
struct Candidates {
    virtual ~Candidates() = default;
    virtual bool Active() const = 0;
    virtual void Open(ITfContext *pic) = 0;
    virtual void Close() = 0;
    /* The toggle handler: opens, closes, or is refused for a
     * candidate-less anchor; eats exactly when the window answered. */
    virtual void Toggle(ITfContext *pic, unsigned long vk, BOOL *pfEat) = 0;
    /* The open window's list keys (Enter/Esc/arrows/pages/digits).
     * Returns true when the key was ours; a refused key closes the
     * window and falls through so typing continues. */
    virtual bool ListKey(ITfContext *pic, unsigned long vk, BOOL *pfEat) = 0;
    /* While the window is open, its keys are ours in the test phase -
     * the same shared-predicate rule the toggle binding uses. */
    virtual bool EatsListKey(unsigned long vk) const = 0;
    /* The candidate-count decision BOTH phases share (test and down are
     * one decision): the whole buffered word is looked up first, and when
     * that yields 0 while the word has more than one syllable, the LAST
     * syllable is looked up instead (issue #100). *fallback returns true
     * when the list would anchor to the last syllable. */
    virtual int ListCount(bool *fallback) = 0;
    /* The tier-F1 observation channel: fold the flow's own state into
     * the shell's snapshot (the cand* fields). */
    virtual void Observe(HunminTsfSnapshot *out) = 0;
};

/* The service CLSID, shared with the langbar-item shape test
 * (tsf-langbaritem-test.cpp reads GetInfo's clsidService against it). */
extern const CLSID CLSID_HunminTSF;

/* The shell: COM object identity unchanged, the seams implemented with
 * one-line forwards. Owns the TSF callbacks, the composition write
 * machinery, the langbar item, and the settings bridge. */
class CTextService : public ITfTextInputProcessor,
                     public ITfThreadMgrEventSink,
                     public ITfKeyEventSink,
                     public ITfCompositionSink,
                     public ITfLangBarItemButton,
                     public ITfSource,
                     public ITfDisplayAttributeProvider,
                     public TsfCore,
                     public CandBackend,
                     public HunminDeliveryRegistry {
  public:
    CTextService();
    virtual ~CTextService();

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    /* Issue #71: re-read the configuration store and rebuild the live
     * session - the settings dialog's Apply path, no process restart.
     * Activate() reuses it, so there is exactly one config-read + rebuild
     * in the adapter (never two that could drift). */
    void ApplyConfigStore();

    /* The langbar item's change notification (issue #103): every mode
     * boundary notifies the kernel through the ITfLangBarItemSink the
     * language bar installs onto the item at AddItem. */
    void UpdateLangBarItem();

    /* The mode-switch core (issue #103): flip, conversion compartment,
     * langbar notification, the composition-boundary flush, and the ic
     * rebuild with the other slot's layout, in one place. */
    void ToggleModeCore(ITfContext *pic);

    STDMETHOD(Activate)(ITfThreadMgr *ptm, TfClientId tid) override;
    STDMETHOD(Deactivate)() override;
    STDMETHOD(GetInfo)(TF_LANGBARITEMINFO *pInfo) override;
    STDMETHOD(GetStatus)(DWORD *pdwStatus) override;
    STDMETHOD(Show)(BOOL fShow) override;
    STDMETHOD(GetTooltipString)(BSTR *pbstrToolTip) override;
    STDMETHOD(OnClick)(TfLBIClick click, POINT pt, const RECT *prcArea) override;
    STDMETHOD(InitMenu)(ITfMenu *pMenu) override;
    STDMETHOD(OnMenuSelect)(UINT wID) override;
    STDMETHOD(AdviseSink)(REFIID riid, IUnknown *punk, DWORD *pdwCookie) override;
    STDMETHOD(UnadviseSink)(DWORD dwCookie) override;
    STDMETHOD(GetIcon)(HICON *phIcon) override;
    STDMETHOD(GetText)(BSTR *pbstrText) override;
    STDMETHOD(EnumDisplayAttributeInfo)(
        IEnumTfDisplayAttributeInfo **ppEnum) override;
    STDMETHOD(GetDisplayAttributeInfo)(
        REFGUID guid, ITfDisplayAttributeInfo **ppInfo) override;
    STDMETHOD(OnInitDocumentMgr)(ITfDocumentMgr *pdm) override;
    STDMETHOD(OnUninitDocumentMgr)(ITfDocumentMgr *pdm) override;
    STDMETHOD(OnSetFocus)(ITfDocumentMgr *pdmFocus,
                          ITfDocumentMgr *pdmPrevFocus) override;
    STDMETHOD(OnPushContext)(ITfContext *pic) override;
    STDMETHOD(OnPopContext)(ITfContext *pic) override;
    STDMETHOD(OnSetFocus)(BOOL fFocus) override;
    STDMETHOD(OnTestKeyDown)(ITfContext *pic, WPARAM wParam, LPARAM lParam,
                             BOOL *pfEat) override;
    STDMETHOD(OnKeyDown)(ITfContext *pic, WPARAM wParam, LPARAM lParam,
                         BOOL *pfEat) override;
    STDMETHOD(OnTestKeyUp)(ITfContext *pic, WPARAM wParam, LPARAM lParam,
                           BOOL *pfEat) override;
    STDMETHOD(OnKeyUp)(ITfContext *pic, WPARAM wParam, LPARAM lParam,
                       BOOL *pfEat) override;
    STDMETHOD(OnPreservedKey)(ITfContext *pic, const GUID &rguid,
                              BOOL *pfEat) override;
    STDMETHOD(OnCompositionTerminated)(TfEditCookie ecWrite,
                                       ITfComposition *pComposition) override;

    /* --- the seams, implemented --- */
    /* TsfCore */
    hunmin_ic *Ic() override { return ic_; }
    std::wstring &Wordbuf() override { return wordbuf_; }
    void CompositionWrite(ITfContext *pic) override;
    void InsertBufferedWord(ITfContext *pic) override;
    /* CandBackend */
    Delivery *ActiveDelivery() override { return delivery_; }
    hunmin_ctx *EngineContext() override { return ctx_; }
    hunmin_ic *EngineIc() override { return ic_; }
    bool CaretRect(ITfContext *pic, RECT *rcOut, bool *haveCaretOut,
                   GUITHREADINFO *gtiOut) override;
    /* HunminDeliveryRegistry */
    void RegisterDeliveryFactory(HunminDeliveryFactory *factory) override;
    bool UseDelivery(const char *name) override;

    /* The observation channel (tier F1). */
    HunminTsfSnapshot Snapshot();

    /* Bookkeeping the key path shares: the context of the live key stream
     * (the composition moves with it) and the entry settle of a posted
     * doc write. */
    void TrackKeyContext(ITfContext *pic);
    void FlushPendingDocWrites() { delivery_->FlushPending(); }

  private:
    /* The composition write machinery (issue #98): one edit session
     * class makes the composition exist and shows the whole buffered
     * word in it, underlined. */
    class CompositionSession;
    /* The boundary insert and the caret-rect fetch edit sessions. */
    class FlushSession;
    class RectSession;

    bool IsModeToggle(WPARAM wParam);
    void ApplyModeToggle(ITfContext *pic, WPARAM wParam, LPARAM lParam,
                         BOOL *pfEat);
    bool IsCandidateToggle(WPARAM wParam);
    bool IsKeyOfPassage(WPARAM wParam);
    void DrainAndPass(ITfContext *pic, WPARAM wParam, BOOL *pfEat,
                      const char *tag);
    static uint32_t VkToBase(WPARAM wParam);
    static bool IsModifierVk(WPARAM wParam);
    void HarvestFinalised();
    void FeedEngineKey(ITfContext *pic, WPARAM wParam, BOOL *pfEat);
    void CompositionEnd(ITfContext *pic, const std::wstring &text);
    void DetachComposition();
    void ApplyDisplayAttribute(ITfContext *ctx, TfEditCookie ec,
                               ITfRange *range, bool clear);
    void UpdateConversionCompartment();
    void BoundaryFlush(ITfContext *pic);

    LONG ref_;
    ITfThreadMgr *threadMgr_;
    TfClientId clientId_;
    DWORD sinkCookie_;
    hunmin_ctx *ctx_;
    hunmin_ic *ic_;
    bool hangulMode_;
    /* The resolved layout pair (issue #66): hangulMode_ switches between
     * these, config-read at Activate and re-read by the settings dialog
     * (#71). Defaults match the fcitx5 addon config. */
    std::string hangulId_, latinId_;
    /* Mode-toggle bindings (maintainer instruction: the Han/Eng key is the
     * default; additional keys configure through the store). Always carries
     * the bare physical Han/Eng key - the platform alternate that survives
     * every rebinding, the rule Android's KeyAction.HANGUL_ENG encodes. */
    std::vector<HunminKeyCombo> toggleKeys_;
    /* The candidate-window key (issue #67): ONE binding, default the bare
     * physical Hanja key, re-read by ApplyConfigStore with everything else.
     * F9 is deliberately NOT in the default set any more (maintainer
     * instruction), but a stored binding can name it. */
    HunminKeyCombo hanjaCombo_;
    RECT preeditRect_;
    bool hasPreeditRect_;
    ITfContext *lastCtx_;

    /* Native composition preedit (issue #98): the ITfComposition the
     * engine's in-flight text lives in, the context it belongs to, and the
     * text currently shown in it. */
    ITfComposition *composition_;
    ITfContext *compCtx_;
    std::wstring compText_;
    TfGuidAtom displayAttrAtom_;

    /* Finalised syllables held for insertion at the next boundary key
     * (issue #73); drained destructively by hunmin_ic_commit(). */
    std::wstring wordbuf_;

    bool interimForce_ = false;
    bool legacyMode_ = false; /* the mode flag; delivery_ carries the behaviour */
    bool endingSelf_ = false; /* between EndComposition entry and return */

    /* The registered features (DIP): factories registered by the
     * components, the shell constructs/owns the instances. */
    struct DeliverySlot {
        const char *name = nullptr;
        std::unique_ptr<Delivery> impl;
    };
    std::vector<HunminDeliveryFactory *> registry_;
    std::vector<DeliverySlot> deliveries_;
    Delivery *delivery_ = nullptr;
    std::unique_ptr<Candidates> cand_;

    /* ctfutb.h ITfLangBarItemButton state: true between AddItem and
     * RemoveItem; the manager owns exactly one extra reference. */
    bool langbar_registered_ = false;
    /* The ITfLangBarItemSink the language bar installed onto the item
     * (ITfSource::AdviseSink at AddItem - language-bar.md; released with
     * the item at Deactivate). */
    ITfLangBarItemSink *itemSink_ = nullptr;
    DWORD itemSinkCookie_ = 0;
};

#endif /* HUNMIN_TSF_TSF_H_ */
