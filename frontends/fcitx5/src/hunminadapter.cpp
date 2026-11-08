/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Thin fcitx5 adapter: platform KeyEvent <-> hunmin key ids (US-QWERTY
 * character convention, docs/trace-spec.md), commit/preedit presentation.
 * All composition logic lives in libhunmin — keep this file small (rule #4).
 */
#include "hunminadapter.h"
#include <cstdlib>
#include <cstring>
#include <fcitx-config/iniparser.h>
#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/misc.h>
#include <fcitx-utils/textformatflags.h>
#include <fcitx/candidatelist.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>

namespace fcitx {

#ifdef HUNMIN_TRACE
/* Tier C trace mode (docs/trace-spec.md step 2), enabled at build time with
 * -DHUNMIN_TRACE=ON and compiled out entirely otherwise, so a packaged addon
 * carries no trace sink and no getenv on the key path.
 *
 * This lives in the SHIPPED addon rather than only in the test harness because
 * D1/D2/Tier C already dlopen the installed libhunmin.so: the binary under test
 * is the binary that ships. A trace mode that existed only in a test build
 * would be testing a different adapter than the one released. */
namespace {

FILE *traceSink() {
    static FILE *sink = nullptr;
    static bool opened = false;
    if (!opened) {
        opened = true;
        const char *path = getenv("HUNMIN_TRACE_OUT");
        sink = (path && *path) ? fopen(path, "a") : nullptr;
        if (!sink) {
            sink = stderr;
        }
    }
    return sink;
}

void traceEvent(const char *kind, const std::string &text) {
    FILE *f = traceSink();
    fprintf(f, "{\"kind\":\"%s\",\"text\":\"", kind);
    for (char c : text) {
        if (c == '"' || c == '\\') {
            fputc('\\', f);
        }
        fputc(c, f);
    }
    fputs("\"}\n", f);
    fflush(f);
}

} // namespace
#endif /* HUNMIN_TRACE */

/* Platform symbol -> the shared key identity (issue #128). A table rather than
 * a switch because the mapping is data: the same keys on every platform, and the
 * adapter's job is only to name them. Everything absent here is either a
 * character (handled by the caller) or a key the platform mapped to nothing.
 *
 * Outside the trace guard on purpose: this is key routing, not tracing. */
struct SymKeyId {
    KeySym sym;
    hunmin_key_id id;
};
const SymKeyId kSymKeyIds[] = {
    {FcitxKey_Up, HUNMIN_KEY_UP},
    {FcitxKey_KP_Up, HUNMIN_KEY_UP},
    {FcitxKey_Down, HUNMIN_KEY_DOWN},
    {FcitxKey_KP_Down, HUNMIN_KEY_DOWN},
    {FcitxKey_Left, HUNMIN_KEY_LEFT},
    {FcitxKey_KP_Left, HUNMIN_KEY_LEFT},
    {FcitxKey_Right, HUNMIN_KEY_RIGHT},
    {FcitxKey_KP_Right, HUNMIN_KEY_RIGHT},
    {FcitxKey_Page_Up, HUNMIN_KEY_PAGE_UP},
    {FcitxKey_KP_Page_Up, HUNMIN_KEY_PAGE_UP},
    {FcitxKey_Page_Down, HUNMIN_KEY_PAGE_DOWN},
    {FcitxKey_KP_Page_Down, HUNMIN_KEY_PAGE_DOWN},
    {FcitxKey_Return, HUNMIN_KEY_ENTER},
    {FcitxKey_KP_Enter, HUNMIN_KEY_ENTER},
    {FcitxKey_Escape, HUNMIN_KEY_ESCAPE},
};

class HunminState;

/* A candidate that commits when chosen.
 *
 * DisplayOnlyCandidateWord would be wrong here: its select() is a no-op,
 * which is correct for a window that only mirrors text but means a chosen
 * Hanja would be silently dropped. fcitx5 calls select() with the input
 * context, which is how the word reaches back to the HunminState that owns
 * the window.
 *
 * select() is defined out of line, after HunminState, because it casts to
 * HunminState and a dynamic_cast needs the complete type. */
class HunminCandidateWord : public CandidateWord {
public:
    HunminCandidateWord(Text text, Text comment, HunminAdapter *engine, InputContext *ic, std::string hanja);

    void select(InputContext *inputContext) const override;

    const std::string &hanja() const { return hanja_; }

private:
    HunminAdapter *engine_;
    InputContext *ic_;
    std::string hanja_;
};

class HunminState : public InputContextProperty {
public:
    HunminState(HunminAdapter *engine, InputContext *ic) : engine_(engine), ic_(ic) {}

    ~HunminState() override {
        if (preedit_) {
            hunmin_buffered_preedit_free(preedit_);
        }
    }

    /* --- Hanja candidates (P3) ---
     *
     * F9 is handled here, not in the engine: libhangul's own layout routes it
     * through the engine's own process path, but F9 is outside the printable ASCII range
     * the engine accepts, so an adapter that wants a Hanja window must
     * intercept it. That does NOT require swallowing keys the engine declined
     * -- F9 is matched before the printable-ASCII test, so `used` is never
     * consulted for it and the tail's conditional filterAndAccept() suffices.
     * (An earlier revision consumed unconditionally, on a false premise;
     * reverted in 05e51dc.)
     *
     * Nothing here reads or rewrites surrounding text -- see invariant I9 and
     * PLAN.md's P3 note. The candidates come from the engine's own preedit and
     * choosing one replaces the composition rather than re-inserting text.
     * The candidate list is held per-context, because it belongs to the
     * preedit that produced it and must not leak across focus changes. */
    bool openCandidates() {
        const char *shown = hunmin_buffered_preedit_display(preedit_);
        if (!shown || !*shown) {
            return false;
        }
        candidateCount_ = hunmin_suggest_count(engine_->ctx(), shown);
        if (candidateCount_ <= 0) {
            return false;
        }
        /* Keep the Hangul we looked up: a boundary flush would commit it,
         * and the window has to stay anchored to the preedit, not to a
         * committed string. */
        candidateSource_ = shown;
        updateCandidateList();
        return true;
    }

    void closeCandidates() {
        candidateCount_ = 0;
        candidateSource_.clear();
        if (preedit_) {
            // nullptr clears the window. CandidateList is abstract, so there
            // is no empty value to assign.
            ic_->inputPanel().setCandidateList(nullptr);
        }
    }

    void updateCandidateList();

    bool candidateActive() const { return candidateCount_ > 0; }

    /* Commit a chosen Hanja in place of the preedit it came from.
     *
     * The candidate list is anchored to candidateSource_, the Hangul that was
     * composing when the window opened. That Hangul is still the engine's
     * preedit, so committing the Hanja and dropping the composition is all
     * that has to happen -- no deleteSurroundingText, which PLAN.md's P3 note
     * rules out for the F9 flow. */
    void commitCandidate(const std::string &hanja) {
        if (preedit_) {
            // Drop what was composing first, so the preedit the client sees
            // afterwards is empty rather than the source Hangul.
            hunmin_buffered_preedit_reset(preedit_);
        }
        closeCandidates();
        ic_->commitString(hanja);
        updateUI();
    }

    const std::string &candidateSource() const { return candidateSource_; }

    void keyEvent(const std::string &layout, KeyEvent &keyEvent) {
        ensureIc(layout);
        if (!preedit_) {
            return; // bad layout id: key passes through to the client
        }

        auto sym = keyEvent.key().sym();
        if (sym == FcitxKey_Shift_L || sym == FcitxKey_Shift_R) {
            return;
        }

        // CapsLock. Note key() cannot be used here: Key::normalize() rebuilds
        // states_ down to Ctrl/Alt/Super/Mod3 and drops CapsLock entirely, so
        // the normalized key reports sym='k' with no CapsLock bit. rawKey()
        // keeps it. The uppercase sym still arrives there, so feed the layout
        // the unshifted form and expose P via the caps option (SPEC-P2.md).
        const bool caps = keyEvent.rawKey().states().test(KeyState::CapsLock);
        if (caps && sym >= 'A' && sym <= 'Z') {
            sym = static_cast<KeySym>(sym + 'a' - 'A');
        }

        /* F9 opens the Hanja window for the current preedit (P3): presentation,
         * so it stays here. Consumption is conditional on candidates, not on
         * what the engine thought -- an open window closes, a preedit with
         * candidates opens one, and anything else returns unconsumed so F9
         * reaches the client. */
        if (sym == FcitxKey_F9) {
            if (candidateActive()) {
                closeCandidates();
            } else if (!openCandidates()) {
                return; // no candidates: F9 belongs to the client
            }
            updateUI();
            keyEvent.filterAndAccept();
            return;
        }

        /* Translate the platform key into the shared vocabulary ONCE. What the
         * key MEANS is the layer's answer (issue #128, decision C): the adapter
         * maps sym + modifier state to (base, shift, mods, key id) and reports
         * whether its own candidate window is open; the precedence (language
         * key, open list, chord, unmapped, feed) lives in libhunmin and is
         * asserted once, in tier A's engine-key-rule. */
        hunmin_key_event ev{};
        const KeyStates states = keyEvent.key().states();
        if (states.test(KeyState::Ctrl)) ev.mods |= HUNMIN_MOD_CTRL;
        if (states.test(KeyState::Alt)) ev.mods |= HUNMIN_MOD_ALT;
        if (states.test(KeyState::Super) || states.test(KeyState::Hyper)) ev.mods |= HUNMIN_MOD_GUI;
        /* Shift is deliberately NOT in mods: it selects a layout row, and it
         * travels as the engine's shift bit instead. */
        for (const SymKeyId &row : kSymKeyIds)
            if (row.sym == sym) {
                ev.id = row.id;
                break;
            }
        // A character (base 0 = the platform mapped nothing). 0x08 is BackSpace,
        // the same convention as libhangul and the golden traces.
        if (ev.id == HUNMIN_KEY_TEXT && sym == FcitxKey_BackSpace) ev.base = 0x08;
        else if (ev.id == HUNMIN_KEY_TEXT && sym >= 0x20 && sym <= 0x7E) ev.base = static_cast<uint32_t>(sym);
        /* An uppercase symbol means the row was shifted; the engine resolves
         * the case endpoint from (base, shift) itself. The layer logs the same
         * pair for ReqMode8's Latin reading, so no adapter-side copy of the key is
         * kept here any more (the detector that needed one is retired). */
        if (ev.base >= 'A' && ev.base <= 'Z') {
            ev.shift = 1;
            ev.base = ev.base + 'a' - 'A';
        }
        ev.candidates_open = candidateActive() ? 1 : 0;

        const hunmin_key_action action = hunmin_buffered_preedit_classify(preedit_, &ev);

        if (action == HUNMIN_KEY_CANDIDATE) {
            auto list = ic_->inputPanel().candidateList();
            switch (sym) {
            case FcitxKey_Up:
            case FcitxKey_Left:
            case FcitxKey_KP_Up:
            case FcitxKey_KP_Left:
                if (list && list->toCursorMovable()) {
                    list->toCursorMovable()->prevCandidate();
                }
                updateUI();
                keyEvent.filterAndAccept();
                return;
            case FcitxKey_Down:
            case FcitxKey_Right:
            case FcitxKey_KP_Down:
            case FcitxKey_KP_Right:
                if (list && list->toCursorMovable()) {
                    list->toCursorMovable()->nextCandidate();
                }
                updateUI();
                keyEvent.filterAndAccept();
                return;
            case FcitxKey_Page_Up:
            case FcitxKey_KP_Page_Up:
                if (list && list->toPageable() && list->toPageable()->hasPrev()) {
                    list->toPageable()->prev();
                }
                updateUI();
                keyEvent.filterAndAccept();
                return;
            case FcitxKey_Page_Down:
            case FcitxKey_KP_Page_Down:
                if (list && list->toPageable() && list->toPageable()->hasNext()) {
                    list->toPageable()->next();
                }
                updateUI();
                keyEvent.filterAndAccept();
                return;
            case FcitxKey_Return:
            case FcitxKey_KP_Enter: {
                // Commit whatever the cursor is on. The cursor starts at 0,
                // which is the highest-frequency candidate, so a plain Return
                // after F9 picks the best match -- the common case.
                int idx = list ? list->cursorIndex() : 0;
                if (idx < 0) {
                    idx = 0;
                }
                if (list && idx < list->size()) {
                    const CandidateWord &word = list->candidate(idx);
                    auto *hanmin = dynamic_cast<HunminCandidateWord *>(const_cast<CandidateWord *>(&word));
                    if (hanmin) {
                        commitCandidate(hanmin->hanja());
                    }
                }
                keyEvent.filterAndAccept();
                return;
            }
            case FcitxKey_Escape:
                closeCandidates();
                updateUI();
                keyEvent.filterAndAccept();
                return;
            default:
                break;
            }
        }

        if (action == HUNMIN_KEY_BOUNDARY_PASS) {
            /* A chord, or a key the platform mapped to nothing: the layer's
             * boundary is where the word is finished, and the key belongs to
             * the client. The same flush + pass this adapter used to spell out
             * in two separate branches. */
            flush();
            updateUI();
            return;
        }
        /* HUNMIN_KEY_LANGUAGE_TOGGLE is unreachable here: fcitx5 owns its own
         * trigger, so this adapter always reports language_key = 0. §4's shared
         * action is what removes that asymmetry. */

        // Any other key closes an open window and falls through, so typing
        // continues where the user left off instead of being swallowed by an
        // invisible window.
        if (candidateActive()) {
            closeCandidates();
            updateUI();
        }

        /* The shared layer owns consumption, commit and preedit for every
         * frontend (docs/DESIGN-FRONTEND.md §5); this adapter keeps no buffer
         * of its own, and the user's CommitBehavior reaches it as an option. */
        hunmin_buffered_preedit_set_option(preedit_, "caps", caps ? 1 : 0);
        hunmin_key_result out{};
        const bool used = hunmin_buffered_preedit_process_key(preedit_, ev.base, ev.shift, &out) != 0;
        /* Both strings are borrowed until the next hunmin_* call, and
         * applyJudgement() below makes two of them (take_judgement, then a
         * possible toggle that re-creates the ic). On a boundary key that both
         * delivers a word and switches language, reading `out.commit` after
         * that yields nothing -- measured with `hello` + a switch, the word
         * vanished from the client. Copy first. */
        const std::string justDelivered = out.commit ? std::string(out.commit) : std::string();
        const std::string justComposing = out.preedit ? std::string(out.preedit) : std::string();
        /* The judgement belongs to the word that just ended: ask before
         * anything else can start the next word. Its text joins the same
         * delivery after the word (ReqMode10: the word is already the judged
         * language, nothing is rewritten). */
        const std::string switched = applyJudgement();

        if (!justDelivered.empty() || !switched.empty()) {
            const std::string delivered = justDelivered + switched;
#ifdef HUNMIN_TRACE
            traceEvent("commit", delivered.c_str());
#endif
            ic_->commitString(delivered.c_str());
        }
#ifdef HUNMIN_TRACE
        traceEvent("preedit", justComposing.c_str());
        traceEvent(used ? "consumed" : "passed", "");
#endif
        updateUI();
        // Only consume what the layer actually took: a declined key belongs to
        // the client (docs/DESIGN-FRONTEND.md Part B §5 rule 3).
        if (used) {
            keyEvent.filterAndAccept();
        }
    }

    /* ReqMode8's judgement, the MODE half (docs/SPEC-MODES.md; ruled 2026-11-01,
     * issue #164): the engine judged the finished word and delivered it in the
     * judged language; this performs the switch, the same layer call TSF and
     * Android make. fcitx5 registers one input method for the addon (ReqMode2),
     * so the addon's mode IS the layer's slot -- no host language can disagree
     * with it, unlike the two-IM arrangement FE13 describes.
     *
     * -1 means there is nothing to act on; which conditions produce it is the
     * engine's business, not this adapter's. Asked once per key, like the other
     * two frontends, so the answer cannot outlive the word it belongs to. */
    std::string applyJudgement() {
        if (!preedit_) {
            return {};
        }
        const int judged = hunmin_buffered_preedit_take_judgement(preedit_);
        if (judged < 0 || judged == hunmin_buffered_preedit_language(preedit_)) {
            return {};
        }
        hunmin_key_result out{};
        if (!hunmin_buffered_preedit_toggle_language(preedit_, &out)) {
            return {};
        }
        /* Copied before the next engine call, for the same reason the caller
         * copies `justDelivered` before this function runs: `out.commit` is
         * borrowed until the following hunmin_* call, and layout_id() below is
         * one. */
        const std::string boundary = (out.commit && *out.commit) ? std::string(out.commit) : std::string();
        FCITX_INFO() << "hunmin: autoswitch -> " << hunmin_buffered_preedit_layout_id(preedit_);
        updateUI();
        /* Whatever the boundary the switch performs was holding. Empty in the
         * normal case -- the word that triggered the switch arrived in the
         * caller's own out.commit -- but it belongs to the same delivery, so it
         * is returned rather than committed here (one commit point per key). */
        return boundary;
    }

    /* ReqMode13's on/off state, pushed into the live layer and read back: this
     * returns what the ENGINE holds, not what the caller asked for, so the row
     * shows the engine's answer. The engine's refusals (see its B3 data gate)
     * are not re-implemented here. */
    int applyAutoswitch(int want) {
        if (!preedit_) {
            return want; // no layer yet: ensureIc() seeds one from the stored value
        }
        hunmin_buffered_preedit_set_option(preedit_, "autoswitch", want);
        int held = want;
        if (!hunmin_buffered_preedit_get_option(preedit_, "autoswitch", &held)) {
            held = want; // unknown name: nothing to read back, keep the request
        }
        return held;
    }

    bool hasLayer() const { return preedit_ != nullptr; }

    void reset() {
        if (preedit_) {
            hunmin_buffered_preedit_reset(preedit_);
        }
        updateUI();
    }

    void flush() {
        if (!preedit_) {
            return;
        }
        if (const char *s = hunmin_buffered_preedit_commit_word(preedit_); s && *s) {
#ifdef HUNMIN_TRACE
            traceEvent("commit", s);
#endif
            ic_->commitString(s);
        }
    }

private:
    void ensureIc(const std::string &layout) {
        // per-item profile layout wins; empty means "no override", in which
        // case the addon-wide default from conf/hunmin.conf applies.
        const std::string &want = layout.empty() ? *engine_->config().hangulLayout : layout;
        /* The input unit is part of the identity of a live layer, not just a
         * constructor argument: a user who switches CommitBehavior gets a new
         * layer on the next key, exactly as a layout change does. Leaving it
         * out here meant the preference saved, reloaded, and did nothing
         * (measured by the D1 test that switches it). */
        const int policy = (*engine_->config().commitBehavior == "syllable") ? HUNMIN_COMMIT_PER_SYLLABLE : HUNMIN_COMMIT_AT_BOUNDARY;
        if (preedit_ && want == layout_ && policy == policy_) {
            return;
        }
        if (preedit_) {
            flush();
            hunmin_buffered_preedit_free(preedit_);
            preedit_ = nullptr;
        }
        layout_ = want;
        /* The user's choice (config.h CommitBehavior), not a hardcoded policy:
         * both units exist in the shared layer and are selectable, because a
         * buffered word is not what every client wants to show. Anything but
         * the literal "syllable" means the buffered default -- a typo must not
         * silently switch the unit. */
        policy_ = policy;
        /* TWO languages (ruled 2026-11-01, issue #164): the pair is what lets
         * the layer read the word on the Latin layout, judge it and switch its
         * own slot -- until this, the Latin id was NULL and every one of those
         * answers "not ours". The Latin half is the user's choice in config.h
         * (LatinLayout), the same option the ReqMode8 detector used to judge on. */
        const std::string latin = *engine_->config().latinLayout;
        preedit_ = hunmin_buffered_preedit_new(engine_->ctx(), want.c_str(), latin.c_str(), policy_);
        if (!preedit_) {
            FCITX_ERROR() << "hunmin: unsupported layout: " << want;
        } else {
            hunmin_buffered_preedit_special_host(preedit_, engine_->specialHost());
            /* ReqMode8's `english_valid` reads this list (ReqMode8, ruled 2026-11-01);
             * absent data is a normal state (B3) and the judgement then
             * abstains rather than calling every word not-English. */
            hunmin_buffered_preedit_load_english_shipped(preedit_);
            /* The preference belongs to the person, not to the layer: a rebuilt
             * layer (config apply, layout change) starts from the engine's
             * default OFF, so the stored value is re-applied here. What the
             * engine accepts is recorded back, because the data gate can refuse
             * ON and the row must not claim a state the engine does not hold
             * (issue #176's fourth checklist item). */
            engine_->recordAutoswitch(applyAutoswitch(*engine_->config().autoswitch ? 1 : 0));
        }
    }

    void updateUI() {
        /* The preedit, not the panel, is the thing that gets replaced: an
         * empty preedit must actively clear whichever preedit slot was set
         * last time, or the previous composition stays on screen after the
         * client already consumed it. This is why inputPanel().reset() is not
         * simply called at the top -- that would also wipe the candidate
         * window, which must outlive the preedit it was opened from. */
        if (preedit_) {
            const char *shown = hunmin_buffered_preedit_display(preedit_);
            bool useClient = ic_->capabilityFlags().test(CapabilityFlag::Preedit);
            Text text;
            if (shown && *shown) {
                text.append(shown);
                text.setCursor(std::strlen(shown));
            }
            /* An empty Text clears the slot, so one call covers both cases. */
            if (useClient) {
                ic_->inputPanel().setClientPreedit(text);
            } else {
                ic_->inputPanel().setPreedit(text);
            }
        }
        ic_->updatePreedit();
        ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    HunminAdapter *engine_;
    InputContext *ic_;
    hunmin_buffered_preedit *preedit_ = nullptr;
    int policy_ = -1; /* HUNMIN_COMMIT_*, the policy preedit_ was built with */
    std::string layout_;
    int candidateCount_ = 0;
    std::string candidateSource_;
};

HunminCandidateWord::HunminCandidateWord(Text text, Text comment, HunminAdapter *engine, InputContext *ic, std::string hanja)
    : CandidateWord(std::move(text)), engine_(engine), ic_(ic), hanja_(std::move(hanja)) {
    setComment(std::move(comment));
}

void HunminCandidateWord::select(InputContext *inputContext) const {
    auto *state = inputContext ? inputContext->property("hunminState") : nullptr;
    if (auto *hunmin = dynamic_cast<HunminState *>(state)) {
        hunmin->commitCandidate(hanja_);
    }
}

/* The largest page the F9 candidate window will ask fcitx5 to draw.
 *
 * NOT ours to choose freely: it is fcitx5's label ceiling. A
 * CommonCandidateList that never calls setLabels() holds exactly
 * `regularLabelSize = 10` labels (fcitx5-5.1.19
 * src/lib/fcitx/candidatelist.cpp:28-41), classicui reads one label per
 * candidate on the page (src/ui/classic/inputwindow.cpp:392-405), and
 * CommonCandidateList::label() throws std::invalid_argument past the end
 * (:556-565). Nothing catches it, so exceeding this aborts fcitx5.
 *
 * Ruled by the maintainer 2026-10-13 (issue #45): cap the page, rather than
 * keep the full list on one page and supply labels. Exceeding it was measured:
 * 10 candidates survive F9, 11 abort, 67 abort. If fcitx5's ceiling ever moves,
 * this constant is where to look, and tools/repro-f9-candidates.sh is what
 * settles it. */
constexpr int kMaxCandidatePageSize = 10;

void HunminState::updateCandidateList() {
    /* CommonCandidateList is the concrete list fcitx5 documents for this
     * ("a common simple candidate list that serves most of the purpose");
     * ModifiableCandidateList and CandidateList are abstract, and
     * setCandidateList takes ownership via unique_ptr. */
    auto list = std::make_unique<CommonCandidateList>();
    for (int i = 0; i < candidateCount_; i++) {
        Text text(hunmin_suggest_text(engine_->ctx(), i));
        /* A bare syllable candidate has no gloss; its kind is the label
         * that distinguishes it from a word or idiom entry. */
        std::string detail(hunmin_suggest_detail(engine_->ctx(), i));
        if (detail.empty()) {
            detail = hunmin_suggest_kind(engine_->ctx(), i);
        }
        list->append(std::make_unique<HunminCandidateWord>(text, Text(detail), engine_, ic_, hunmin_suggest_text(engine_->ctx(), i)));
    }
    /*
     * Vertical, cursor on the first, and a page that never exceeds
     * kMaxCandidatePageSize.
     *
     * A syllable can carry up to 64 Hanja, and 간 resolves to 67 once the
     * krdict word senses are prepended. Laid out horizontally they ran off the
     * screen and the tail was unreachable without the mouse; the layout hint is
     * the whole fix for that.
     *
     * The page size used to be the whole candidate count, "so the whole list is
     * drawn at once". That aborted fcitx5 on any syllable with 11 or more
     * candidates: classicui reads one label per candidate on the page
     * (fcitx5-5.1.19 src/ui/classic/inputwindow.cpp:392-405) and
     * CommonCandidateList::label() throws past the label vector, which
     * fillLabels() pads only to `regularLabelSize = 10`
     * (src/lib/fcitx/candidatelist.cpp:28-41, 556-565). The exception is
     * uncaught, so the whole input method died with SIGABRT. Measured, not
     * assumed: 10 candidates survive, 11 abort, 67 abort, and 67 candidates at
     * fcitx5's default page size of 5 do not -- so the page size is the trigger
     * and the layout hint is not (Horizontal throws at the same index).
     * tools/repro-f9-candidates.sh reproduces it; no ctest tier could, because
     * engine-d2-ui loads no UI addon and engine-d3-* disables classicui. See
     * wiki/raw/measurements/2026-10-13-f9-candidate-window-abort.md.
     *
     * Capping costs nothing that was working: moveCursor() rotates over
     * totalSize() unless cursorKeepInSamePage is set (it is not), so Up/Down
     * still reaches every candidate -- measured, and the note is here because
     * the opposite claim reads just as plausibly. What the cap does give up is
     * the whole list in one column, which did not fit on a display anyway:
     * 10 candidates measured 336x334, so 67 rows would be roughly 2000px tall
     * against a 1024px screen. PageDown reaches the rest.
     *
     * The cursor starts on the first candidate rather than unselected: after F9
     * the user's intent is to pick one, and Return on an unselected cursor would
     * have to guess.
     */
    list->setPageSize(candidateCount_ < kMaxCandidatePageSize ? candidateCount_ : kMaxCandidatePageSize);
    list->setGlobalCursorIndex(0);
    list->setLayoutHint(CandidateLayoutHint::Vertical);
    ic_->inputPanel().setCandidateList(std::move(list));
}


int HunminAdapter::specialSupports(void * /*user*/, hunmin_special_function fn) {
    /* fcitx5 has no voice recorder yet, so VOICE is refused. The engine's
     * contract is that 0 is a legal answer -- the frontend degrades rather
     * than pretending. Adding the recorder later means only filling in this
     * function, not adding a new frontend surface. */
    return fn == HUNMIN_SPECIAL_AUTOSWITCH ? 1 : 0;
}

void HunminAdapter::specialPerform(void * /*user*/, hunmin_special_function fn) {
    (void)fn;
    /* AUTOSWITCH is a no-op here: the engine already flipped the layer's slot
     * when it serviced the chord. The host exists so the engine can resolve
     * the chord at all; the actual switch is inside libhunmin. */
}

HunminAdapter::HunminAdapter(Instance *instance) : instance_(instance), factory_([this](InputContext &ic) { return new HunminState(this, &ic); }) {
    // Data path: HUNMIN_DATA_PATH first (tests, source trees), then the
    // installed share/hunmin default.
    ctx_ = hunmin_ctx_new(nullptr, 0);
    if (!ctx_) {
        const char *dirs[] = {HUNMIN_DEFAULT_DATA_DIR};
        ctx_ = hunmin_ctx_new(dirs, 1);
    }
    if (!ctx_) {
        FCITX_ERROR() << "hunmin: no layout data found (HUNMIN_DATA_PATH, " << HUNMIN_DEFAULT_DATA_DIR << ")";
    }
    // Hand the configtool its layout list. This has to happen after ctx_ is
    // built (the ids and names come from it) and before the first getConfig(),
    // which is when dumpDescription() runs. Re-injected in reloadConfig() too,
    // because the data directory can change under a config reload -- a stale
    // annotation would show layouts the engine can no longer open.
    refreshLayoutList();
    /* Registered BEFORE reloadConfig(): the reload pushes the stored autoswitch
     * value into every live layer (issue #176), and asking a context for a
     * property whose factory is not yet registered aborts fcitx5
     * (InputContextManager::property asserts factory->manager_ == this). */
    instance_->inputContextManager().registerProperty("hunminState", &factory_);

    /* Route 2: a PreInputMethod watcher that claims Ctrl+Hangul for autoswitch
     * while hunmin is the active IM. The cheap guard (instance->inputMethod(ic)
     * == "hunmin") is what makes this a per-IM chord, not a global hotkey.
     * Route 1 (the user removes Hangul from TriggerKeys) stays available and
     * is required on hosts whose physical keyboard has no Hangul key. */
    probeWatcher_ = instance_->watchEvent(
        EventType::InputContextKeyEvent,
        EventWatcherPhase::PreInputMethod,
        [this](Event &event) {
            auto *ke = dynamic_cast<KeyEvent *>(&event);
            if (!ke || !ke->inputContext()) {
                return;
            }
            /* Guard: only answer while hunmin is the active IM. Without this
             * the watcher would steal Ctrl+Hangul from every other IM on the
             * desktop, which is the behaviour route 2 must not have. */
            if (instance_->inputMethod(ke->inputContext()) != "hunmin") {
                return;
            }
            /* Match Ctrl+Hangul only. Shift is not included here: the 2026-10-31
             * ruling reserves Shift+한/영 for a future manual-toggle chord, and
             * this watcher only services AUTOSWITCH (fn == 2). */
            KeyStates ctrl(KeyState::Ctrl);
            if (ke->rawKey().sym() != FcitxKey_Hangul || !(ke->rawKey().states() & ctrl)) {
                return;
            }
            /* The throwaway layer is only to confirm the chord is bound and
             * serviceable (data gate, host present). The actual toggle is
             * pushed into every live layer by pushAutoswitch, which is what
             * the settings row also calls -- one path, one value. */
            hunmin_key_event ev{};
            ev.mods |= HUNMIN_MOD_CTRL;
            ev.language_key = 1;
            int serviceable = 0;
            if (ctx_) {
                if (hunmin_buffered_preedit *probe = hunmin_buffered_preedit_new(this->ctx(), this->config().hangulLayout->c_str(), this->config().latinLayout->c_str(), HUNMIN_COMMIT_AT_BOUNDARY)) {
                    hunmin_buffered_preedit_special_host(probe, specialHost());
                    const int fn = hunmin_buffered_preedit_special_peek(probe, &ev);
                    if (fn > 0) serviceable = 1;
                    hunmin_buffered_preedit_free(probe);
                }
            }
            if (serviceable) {
                /* Toggle the stored value and push into every live layer.
                 * pushAutoswitch returns what the engine actually holds, so
                 * a data-gated refusal leaves the row honest. */
                const int want = !(this->config().autoswitch.value() ? 1 : 0);
                const int held = pushAutoswitch(want);
                (void)held;
                safeSaveAsIni(this->config_, "conf/hunmin.conf");
                refreshAutoswitchRow();
                instance_->showCustomInputMethodInformation(ke->inputContext(),
                    *config_.autoswitch ? "Autoswitch enabled" : "Autoswitch disabled");
                FCITX_INFO() << "hunmin: Ctrl+Hangul autoswitch toggled, value=" << (*config_.autoswitch ? "ON" : "OFF");
                ke->filterAndAccept();
            }
        });
    reloadConfig();
}

void HunminAdapter::activate(const InputMethodEntry &, InputContextEvent &event) {
    if (auto *state = event.inputContext()->propertyFor(&factory_)) {
        state->reset();
    }
}

void HunminAdapter::deactivate(const InputMethodEntry &entry, InputContextEvent &event) { reset(entry, event); }

void HunminAdapter::keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }
    auto *state = keyEvent.inputContext()->propertyFor(&factory_);
    state->keyEvent(layoutFor(entry), keyEvent);
}

std::string HunminAdapter::layoutFor(const InputMethodEntry &entry) const {
    std::string layout = instance_->inputMethodManager().currentGroup().layoutFor(entry.uniqueName());
    // Per-profile layout wins; empty means "no override", so use the global
    // hangulLayout config as the default Hangul layout.
    if (layout.empty()) {
        layout = *config_.hangulLayout;
    }
    return layout;
}

void HunminAdapter::reset(const InputMethodEntry &, InputContextEvent &event) {
    if (auto *state = event.inputContext()->propertyFor(&factory_)) {
        state->flush(); // composing text is not lost on focus change
        state->reset();
    }
}

void HunminAdapter::refreshLayoutList() {
    if (!ctx_) {
        return;
    }
    std::vector<std::pair<std::string, std::string>> hangulLayouts;
    std::vector<std::pair<std::string, std::string>> latinLayouts;
    const int count = hunmin_ctx_layout_count(ctx_);
    hangulLayouts.reserve(static_cast<size_t>(count));
    latinLayouts.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; i++) {
        const char *id = hunmin_ctx_layout_id(ctx_, i);
        if (!id) {
            continue;
        }
        // Skip unsupported engine kinds.
        if (!hunmin_ctx_layout_supported(ctx_, i)) {
            continue;
        }
        const char *ko = hunmin_ctx_layout_name(ctx_, i, "ko");
        const char *mode = hunmin_ctx_layout_mode(ctx_, i);
        std::string label = ko ? ko : id;
        if (mode && std::string(mode) == "latin") {
            latinLayouts.emplace_back(id, label);
        } else {
            // Default to hangul for layouts without mode or unknown mode.
            hangulLayouts.emplace_back(id, label);
        }
    }
    config_.hangulLayout.annotation().setLayouts(std::move(hangulLayouts));
    config_.latinLayout.annotation().setLayouts(std::move(latinLayouts));
}

void HunminAdapter::reloadConfig() {
    refreshLayoutList();
    readAsIni(config_, "conf/hunmin.conf");
    /* A stored preference can outlive the data package that enables it. The
     * engine's gate is what decides whether it can be held, so the row's two
     * answers are read first and then the value is pushed and reconciled
     * (issue #176). Order matters: pushAutoswitch() consults the presence this
     * refresh records, so doing it the other way round would reconcile a valid
     * stored ON to OFF on the first load. */
    refreshAutoswitchRow();
    pushAutoswitch(*config_.autoswitch ? 1 : 0);
}

int HunminAdapter::pushAutoswitch(int want) {
    /* With no live layer there is nothing to ask, so the gate answers directly:
     * it is the same answer the option writer gives (issue #169 computes it once
     * per context for exactly this pair of readers). */
    int held = (want && autoswitchDataPresent_) ? 1 : 0;
    instance_->inputContextManager().foreach ([&](InputContext *ic) {
        if (auto *s = state(ic); s && s->hasLayer()) {
            /* The live layer is asked, not assumed: the engine refuses ON where
             * the word list is missing. */
            held = s->applyAutoswitch(want);
        }
        return true;
    });
    if ((held != 0) != *config_.autoswitch) {
        config_.autoswitch.setValue(held != 0);
    }
    return held;
}

void HunminAdapter::recordAutoswitch(int held) {
    const bool value = held != 0;
    if (*config_.autoswitch != value) {
        config_.autoswitch.setValue(value);
    }
}

void HunminAdapter::refreshAutoswitchRow() {
    autoswitchDataPresent_ = ctx_ && hunmin_ctx_autoswitch_data_present(ctx_);
    /* The key is the engine's, and it lives on a LAYER (special_bind), not on the
     * context, so the row reads it from a probe built the way ensureIc() builds
     * one. A rebuilt layer starts from the engine's default table, so the probe
     * answers what a fresh key press would honour -- which is the only thing the
     * row is entitled to advertise. */
    if (ctx_) {
        if (hunmin_buffered_preedit *probe = hunmin_buffered_preedit_new(ctx_, config_.hangulLayout->c_str(), config_.latinLayout->c_str(), HUNMIN_COMMIT_AT_BOUNDARY)) {
            if (const unsigned chord = hunmin_buffered_preedit_special_chord(probe, HUNMIN_SPECIAL_AUTOSWITCH)) {
                autoswitchChord_ = chord;
            }
            hunmin_buffered_preedit_free(probe);
        }
    }
    config_.autoswitch.annotation().setChord(autoswitchChord_);
    config_.autoswitch.annotation().setDataPresent(autoswitchDataPresent_);
}

void HunminAdapter::setConfig(const fcitx::RawConfig &rawConfig) {
    config_.load(rawConfig, true);
    /* The dialog's tick reaches the engine here and what comes back is what is
     * stored: fcitx5's config surface is the persistence, the engine's option is
     * the one value both surfaces read (ReqMode13's "no second copy"). */
    pushAutoswitch(*config_.autoswitch ? 1 : 0);
    instance_->inputContextManager().foreach ([this](InputContext *ic) {
        state(ic)->reset(); // in-flight composition dropped; layout swaps
        return true; // lazily on the next key (ensureIc)
    });
    safeSaveAsIni(config_, "conf/hunmin.conf");
}

HunminState *HunminAdapter::state(InputContext *ic) { return ic->propertyFor(&factory_); }

} // namespace fcitx

FCITX_ADDON_FACTORY_V2(hunmin, fcitx::HunminAdapterFactory);
