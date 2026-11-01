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
#include <msctf.h>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

#include "config.h" /* HunminTsfConfig, hunmin_tsf_config_load */
#include "datadir.h" /* hunmin_ctx_from_module, the layout-slot check */
#include "hunmin.h"
#include "ids.h" /* CLSID_HunminTSF and the shared GUIDs (ids.cpp) */
#include "keybinding.h" /* HunminKeyCombo, hunmin_tsf_keybindings_* */
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

/* ---- The seams ---- */

/* The tier-F1 observation channel: one call, one struct - the fields the
 * white-box tests read directly before the include-.cpp seam was retired.
 * Read-only; the values are the adapter's live state at call time.
 * Declared before the interfaces: Candidates::Observe references it. */
struct HunminTsfSnapshot {
    hunmin_ctx *ctx = nullptr;
    hunmin_buffered_preedit *layer = nullptr;
    std::wstring display; /* what the user sees composing */
    std::wstring composing; /* the syllable in flight alone */
    std::wstring finished; /* display minus composing: held, not yet delivered */
    bool hangulMode = true;
    bool legacyMode = false;
    bool interimForce = false;
    /* R13-정정's ON/OFF as the ENGINE answers it (issue #175). Read it here
     * rather than from the store: the engine vetoes the value where the
     * judgement's data is absent (B3), and that refusal has to be visible. */
    int autoswitch = 0;
    ITfComposition *composition = nullptr;
    ITfLangBarItemSink *itemSink = nullptr;
    std::vector<HunminKeyCombo> toggleKeys;
    /* The last special function the engine resolved and this frontend
     * performed (0 = none yet). The dialogs and the tier F1 cases read it
     * instead of parsing a log line. */
    int lastSpecial = 0;
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
    /* The shared layer (issue #128): it owns the word and answers commit and
     * preedit. The adapter keeps no buffer of its own -- `wordbuf_` and the
     * accessor that handed it out are gone, which is what this seam change is
     * for.
     *
     * The two halves are exposed separately because the interim delivery keeps
     * the finished word as DOCUMENT text and rewrites only the syllable in
     * flight, while the native delivery shows the whole display in one
     * composition. */
    virtual hunmin_buffered_preedit *Layer() = 0;
    virtual std::wstring ComposeDisplay() = 0; /* finished word + in-flight */
    virtual std::wstring ComposeInFlight() = 0; /* the syllable in flight */
    virtual std::wstring TakeFinishedWord() = 0; /* the finished half, removed */
    virtual void ComposeReset() = 0;
    virtual void CompositionWrite(ITfContext *pic) = 0;
    /* Deliver the layer's word now (the boundary). */
    virtual void InsertBufferedWord(ITfContext *pic) = 0;
    /* Insert text that is not the layer's (a chosen Hanja replacing the
     * composition). */
    virtual void InsertText(ITfContext *pic, const std::wstring &text) = 0;
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
    virtual bool CandidateCommit(ITfContext *pic, const std::wstring &replacement) = 0;
    /* Settle a posted doc write (the interim deferral; the native path
     * is synchronous - a no-op). */
    virtual void FlushPending() = 0;
    /* A host termination fused the composition's text into the doc:
     * the mode decides what to do with the dead copy. */
    virtual void CaptureDeadCopy(const std::wstring &copy) = 0;
    /* True when this delivery keeps the text in the DOCUMENT and rewrites it
     * through its own protocol, so the engine's commit must be delivered HERE
     * rather than through the composition path. The interim delivery does;
     * the native one writes through a live ITfComposition instead. */
    virtual bool OwnsDocumentText() const { return false; }
    /* Deliver committed text on a delivery that owns the document. Only
     * called when OwnsDocumentText() is true. */
    virtual void InsertText(ITfContext *pic, const std::wstring &text) { (void)pic; (void)text; }
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
    virtual hunmin_buffered_preedit *Layer() = 0;
    /* The caret rect shared by the candidate popup: one read-only edit
     * session against the selection (GetTextExt), falling back to the
     * thread's GUITHREADINFO caret. Screen coordinates out. False means
     * the host reported nothing drawable - the popup hide path. */
    virtual bool CaretRect(ITfContext *pic, RECT *rcOut, bool *haveCaretOut, GUITHREADINFO *gtiOut) = 0;
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

class LangBarItem;
class DisplayAttr;

/* The langbar item's READ seam on the shell (S6): the item owns the bar
 * surface, the shell owns the mode. One question, so the item never sees
 * the engine, the composition or the settings state. */
struct LangBarHost {
    virtual ~LangBarHost() = default;
    virtual bool HangulMode() const = 0;
};

/* The shell: COM object identity unchanged, the seams implemented with
 * one-line forwards. Owns the TSF callbacks, the composition write
 * machinery, the langbar item, and the settings bridge. */
class CTextService : public ITfTextInputProcessor,
                     public ITfThreadMgrEventSink,
                     public ITfKeyEventSink,
                     public ITfCompositionSink,
                     public ITfDisplayAttributeProvider,
                     public TsfCore,
                     public CandBackend,
                     public HunminDeliveryRegistry,
                     public LangBarHost {
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

    /* The langbar item's change notification (issue #103) AND its install site
     * (maintainer ruling 2026-10-29): the caller passes the thread manager it
     * holds, the function obtains ITfLangBarItemMgr from it, adds the item, and
     * notifies the kernel sink the bar installed at AddItem. */
    void UpdateLangBarItem(ITfThreadMgr *ptm);

    /* The language-toggle core (issue #103): flip, conversion compartment,
     * langbar notification, the composition-boundary flush, and the ic
     * rebuild with the other slot's layout, in one place. */
    void ToggleModeCore(ITfContext *pic);

    /* R8's automatic switch (issue #164, ruled 2026-11-01): the ENGINE judges at
     * the boundary and has already delivered the word in the judged language;
     * this performs the MODE half -- the layer's switch, then the two host
     * surfaces that show it. Called after every key, and it does nothing when
     * the engine has no judgement. */
    void ApplyJudgement(ITfContext *pic);

    STDMETHOD(Activate)(ITfThreadMgr *ptm, TfClientId tid) override;
    STDMETHOD(Deactivate)() override;
    STDMETHOD(EnumDisplayAttributeInfo)(IEnumTfDisplayAttributeInfo **ppEnum) override;
    STDMETHOD(GetDisplayAttributeInfo)(REFGUID guid, ITfDisplayAttributeInfo **ppInfo) override;
    STDMETHOD(OnInitDocumentMgr)(ITfDocumentMgr *pdm) override;
    STDMETHOD(OnUninitDocumentMgr)(ITfDocumentMgr *pdm) override;
    STDMETHOD(OnSetFocus)(ITfDocumentMgr *pdmFocus, ITfDocumentMgr *pdmPrevFocus) override;
    STDMETHOD(OnPushContext)(ITfContext *pic) override;
    STDMETHOD(OnPopContext)(ITfContext *pic) override;
    STDMETHOD(OnSetFocus)(BOOL fFocus) override;
    STDMETHOD(OnTestKeyDown)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override;
    STDMETHOD(OnKeyDown)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override;
    STDMETHOD(OnTestKeyUp)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override;
    STDMETHOD(OnKeyUp)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override;
    STDMETHOD(OnPreservedKey)(ITfContext *pic, const GUID &rguid, BOOL *pfEat) override;
    STDMETHOD(OnCompositionTerminated)(TfEditCookie ecWrite, ITfComposition *pComposition) override;

    /* --- the seams, implemented --- */
    /* TsfCore */
    hunmin_buffered_preedit *Layer() override { return preedit_; }
    std::wstring ComposeDisplay() override;
    std::wstring ComposeInFlight() override;
    std::wstring TakeFinishedWord() override;
    void ComposeReset() override;
    void InsertText(ITfContext *pic, const std::wstring &text) override;
    void CompositionWrite(ITfContext *pic) override;
    void InsertBufferedWord(ITfContext *pic) override;
    /* CandBackend */
    Delivery *ActiveDelivery() override { return delivery_; }
    hunmin_ctx *EngineContext() override { return ctx_; }
    bool CaretRect(ITfContext *pic, RECT *rcOut, bool *haveCaretOut, GUITHREADINFO *gtiOut) override;
    /* HunminDeliveryRegistry */
    void RegisterDeliveryFactory(HunminDeliveryFactory *factory) override;
    bool UseDelivery(const char *name) override;
    /* LangBarHost */
    bool HangulMode() const override { return hangulMode_; }

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
    /* Issue #144: take back the text a non-edit host fused into its document
     * when it terminated our composition, so the next composition replaces it
     * rather than appending after it. */
    bool AdoptHostFusedCopy(TfEditCookie ec, ITfRange *sel);

    /* Activate/Deactivate halves: the two advice registrations, the
     * delivery-mode detection, and the engine flush the boundary paths
     * share. UnadviseSinks() withdraws the advice only; ReleaseThreadMgr()
     * drops the manager reference, last, after the lang-bar item is off. */
    void AdviseSinks();
    void UnadviseSinks();
    void ReleaseThreadMgr();
    void DetectLegacyMode();

    void LoadToggleKeys(const HunminTsfConfig &cfg);
    void LoadHanjaKey(const HunminTsfConfig &cfg);
    void RebuildLayer();

    bool IsModeToggle(WPARAM wParam);
    /* The same key with ANY modifiers held: the compound press's entry point. */
    bool IsLanguageKeyVk(WPARAM wParam);
    void ApplyModeToggle(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat);
    /* The compound press (SPEC-MODES §E5, ruled 2026-10-31): a chord modifier
     * held with the Han/Eng key carries a special function instead of switching
     * languages. The BINDING is the engine's value, so the adapter translates
     * (mods, shift) into the shared key event and asks the layer which function
     * this gesture carries -- it never tabulates modifiers itself. */
    hunmin_key_event LanguageKeyEvent(WPARAM wParam) const;
    void ApplyLanguageKeyPress(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat);
    bool LanguageKeyEaten(WPARAM wParam);
    /* The host the engine calls back into for a performed function: what this
     * platform can do (voice input does not exist here yet) and how. */
    static int SpecialSupports(void *user, hunmin_special_function fn);
    void SpecialPerform(hunmin_special_function fn);
    bool IsCandidateToggle(WPARAM wParam);
    bool IsKeyOfPassage(WPARAM wParam);
    void DrainAndPass(ITfContext *pic, WPARAM wParam, BOOL *pfEat, const char *tag);
    static uint32_t VkToBase(WPARAM wParam);
    static bool IsModifierVk(WPARAM wParam);
    void HarvestFinalised();
    void FeedEngineKey(ITfContext *pic, WPARAM wParam, BOOL *pfEat);
    void CompositionEnd(ITfContext *pic, const std::wstring &text);
    void DetachComposition();
    void UpdateConversionCompartment();
    void BoundaryFlush(ITfContext *pic);

    LONG ref_;
    ITfThreadMgr *threadMgr_;
    TfClientId clientId_;
    DWORD sinkCookie_;
    hunmin_ctx *ctx_;
    /* The shared buffered/syllable layer (issue #128): owns the word, the
     * boundary and the commit unit. Nothing adapter-side buffers text. */
    hunmin_buffered_preedit *preedit_;
    bool hangulMode_;
    /* The resolved layout pair (issue #66): hangulMode_ switches between
     * these, config-read at Activate and re-read by the settings dialog
     * (#71). Defaults match the fcitx5 addon config. */
    std::string hangulId_, latinId_;
    /* The commit unit the layer is built with (issue #128): "word" or
     * "syllable", from HunminTsfConfig. ApplyConfigStore refreshes it. */
    char commitUnit_[HUNMIN_TSF_COMMIT_UNIT_CAP] = "word";
    /* R13-정정's ON/OFF as the layer was last told it (issue #175): the
     * engine's value, not a second setting -- the dialog reads the engine and
     * the engine vetoes this where its data is absent (B3, #169). */
    int autoswitch_ = 0;
    /* Mode-toggle bindings (maintainer instruction: the Han/Eng key is the
     * default; additional keys configure through the store). Always carries
     * the bare physical Han/Eng key - the platform alternate that survives
     * every rebinding, the rule Android's KeyAction.HANGUL_ENG encodes. */
    std::vector<HunminKeyCombo> toggleKeys_;
    /* The last special function the host performed (the snapshot's
     * lastSpecial): 0 = none, otherwise a hunmin_special_function. */
    int lastSpecial_ = 0;
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
    /* Issue #144: a host that is NOT a classic edit control terminated our
     * composition mid-word and fused the composition text into its own
     * document. The native path stays (the interim path's EM writes reach
     * nothing there); the next composition re-adopts this copy - it starts
     * one range earlier, so its first write replaces the copy instead of
     * appending after it. Consumed by the next composition start. */
    std::wstring hostFusedCopy_;
    /* The preedit styling channel (S7): owns the GUID atom. */
    std::unique_ptr<DisplayAttr> displayAttr_;

    bool interimForce_ = false;
    bool legacyMode_ = false; /* the mode flag; delivery_ carries the behaviour */
    bool endingSelf_ = false; /* between EndComposition entry and return */
    /* OnTestKeyDown already fed this key because the engine declined it:
     * the host may still call OnKeyDown, which must not feed it a second
     * time. Cleared at the start of every test-phase classification. */
    WPARAM testDeclinedKey_ = 0;

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

    /* The tear-off language-bar item (S6): the shell holds one reference
     * from construction; the manager holds another only between AddItem
     * and RemoveItem. */
    LangBarItem *langbar_ = nullptr;
};

#endif /* HUNMIN_TSF_TSF_H_ */
