/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Thin fcitx5 adapter: platform KeyEvent <-> hunmin key ids (US-QWERTY
 * character convention, docs/trace-spec.md), commit/preedit presentation.
 * All composition logic lives in libhunmin — keep this file small (rule #4).
 */
#include "engine.h"
#include "modeswitch.h"
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
#include <cstdlib>
#include <cstring>

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
#endif

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
    HunminCandidateWord(Text text, Text comment, HunminEngine *engine,
                        InputContext *ic, std::string hanja);

    void select(InputContext *inputContext) const override;

    const std::string &hanja() const { return hanja_; }

private:
    HunminEngine *engine_;
    InputContext *ic_;
    std::string hanja_;
};


class HunminState : public InputContextProperty {
public:
    HunminState(HunminEngine *engine, InputContext *ic)
        : engine_(engine), ic_(ic), switcher_(engine->ctx()) {}

    ~HunminState() override {
        if (hunminIc_) {
            hunmin_ic_free(hunminIc_);
        }
    }

    /* --- Hanja candidates (P3) ---
     *
     * F9 is handled here, not in the engine: libhangul's own layout routes it
     * through hunmin_ic_process, but F9 is outside the printable ASCII range
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
        const char *preedit = hunmin_ic_preedit(hunminIc_);
        if (!preedit || !*preedit) {
            return false;
        }
        candidateCount_ = hunmin_suggest_count(engine_->ctx(), preedit);
        if (candidateCount_ <= 0) {
            return false;
        }
        /* Keep the Hangul we looked up: hunmin_ic_flush() would commit it,
         * and the window has to stay anchored to the preedit, not to a
         * committed string. */
        candidateSource_ = preedit;
        updateCandidateList();
        return true;
    }

    void closeCandidates() {
        candidateCount_ = 0;
        candidateSource_.clear();
        if (hunminIc_) {
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
        if (hunminIc_) {
            // Drop what was composing first, so the preedit the client sees
            // afterwards is empty rather than the source Hangul.
            hunmin_ic_reset(hunminIc_);
        }
        closeCandidates();
        ic_->commitString(hanja);
        updateUI();
    }

    const std::string &candidateSource() const { return candidateSource_; }

    void keyEvent(const std::string &layout, KeyEvent &keyEvent) {
        ensureIc(layout);
        if (!hunminIc_) {
            return; // bad layout id: key passes through to the client
        }

        auto sym = keyEvent.key().sym();
        if (sym == FcitxKey_Shift_L || sym == FcitxKey_Shift_R) {
            return;
        }
        // Modifier chords (Ctrl/Alt/Super combos) belong to the client;
        // commit what is composing and let the key through. Shift is
        // deliberately NOT here: layouts encode shift as uppercase rows.
        KeyStates modifiers;
        modifiers |= KeyState::Ctrl;
        modifiers |= KeyState::Alt;
        modifiers |= KeyState::Super;
        modifiers |= KeyState::Hyper;
        if (keyEvent.key().states() & modifiers) {
            flush();
            updateUI();
            return;
        }

        // F9 opens the Hanja window for the current preedit (P3). Handled
        // here rather than in the engine because F9 is not a printable ASCII
        // character: the `sym >= 0x20 && sym <= 0x7E` test below would send it
        // to flush() and the client, so hunmin_ic_process() never sees it.
        //
        // Consumption is conditional here too, on candidates rather than on
        // what the engine thought: an open window closes, a preedit with
        // candidates opens one, and anything else returns unconsumed so F9
        // reaches the client. Nothing about this needs the unconditional
        // filterAndAccept() that used to sit at the tail of this method.
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

        /*
         * Candidate-window keys, once a window is open.
         *
         * The window is ours, so these keys are ours: without this block an
         * arrow key fell through to the `sym >= 0x20 && sym <= 0x7E` test
         * below and reached the engine as a character (or flushed), which is
         * why choosing a Hanja needed the mouse. fcitx5 does not intercept
         * these on its own -- a CursorMovableCandidateList only provides
         * prevCandidate()/nextCandidate(); calling them is the input method's
         * job -- so each key is handled explicitly and consumed.
         *
         * Arrows move the cursor rather than the page: the list is one page
         * (see updateCandidateList), so Up/Down is the whole motion and
         * Left/Right mirror it for a horizontal habit. PageUp/PageDown exist
         * because PageableCandidateList offers them and a user reaching for
         * them should not get a character in the document.
         */
        if (candidateActive()) {
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
                if (list && list->toPageable() &&
                    list->toPageable()->hasPrev()) {
                    list->toPageable()->prev();
                }
                updateUI();
                keyEvent.filterAndAccept();
                return;
            case FcitxKey_Page_Down:
            case FcitxKey_KP_Page_Down:
                if (list && list->toPageable() &&
                    list->toPageable()->hasNext()) {
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
                    auto *hanmin = dynamic_cast<HunminCandidateWord *>(
                        const_cast<CandidateWord *>(&word));
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
            // Any other key closes the window and falls through to the normal
            // path below, so typing continues where the user left off instead
            // of being swallowed by an invisible window.
            closeCandidates();
            updateUI();
        }

        // CapsLock. Note key() cannot be used here: Key::normalize() rebuilds
        // states_ down to Ctrl/Alt/Super/Mod3 and drops CapsLock entirely, so
        // the normalized key reports sym='k' with no CapsLock bit. rawKey()
        // keeps it. The uppercase sym still arrives there, so feed the layout
        // the unshifted form and expose P via the caps option (SPEC-P2.md).
        bool caps = keyEvent.rawKey().states().test(KeyState::CapsLock);
        if (caps && sym >= 'A' && sym <= 'Z') {
            sym = static_cast<KeySym>(sym + 'a' - 'A');
        }
        hunmin_ic_set_option(hunminIc_, "caps", caps ? 1 : 0);

        uint32_t ch;
        if (sym == FcitxKey_BackSpace) {
            ch = 0x08; // same convention as libhangul / golden traces
        } else if (sym >= 0x20 && sym <= 0x7E) {
            ch = static_cast<uint32_t>(sym);
        } else {
            flush(); // unmapped keysym: commit, then pass to the client
            updateUI();
            return;
        }

        bool used = hunmin_ic_process(hunminIc_, ch);
        switcher_.record(ch); // R8 adapter; see src/modeswitch.h

        if (used) {
            if (const char *commit = hunmin_ic_commit(hunminIc_);
                commit && *commit) {
#ifdef HUNMIN_TRACE
                traceEvent("commit", commit);
#endif
                ic_->commitString(commit);
            }
        } else {
            // Engine declined (space, unmapped row): flush emits the pending
            // commit buffer *and* the in-flight syllable in one string,
            // because hunmin_ic_flush appends via save_commit then reads out
            // through hunmin_ic_commit. Both readers are destructive, so the
            // two calls must never happen for the same keypress.
            flush();
            switcher_.judge(*engine_->config().latinLayout); // the boundary
        }
#ifdef HUNMIN_TRACE
        // Emitted before updateUI(), which does not disturb the engine state.
        // hunmin_ic_commit is destructive and was already read above when used,
        // so this second read legitimately yields "" -- the commit itself was
        // traced at the point it was delivered.
        traceEvent("preedit", hunminIc_ ? hunmin_ic_preedit(hunminIc_) : "");
        traceEvent(used ? "consumed" : "passed", "");
#endif
        updateUI();
        // Only consume what the engine actually took. A key it declined
        // (space, backspace on an empty buffer, an unmapped keysym) belongs to
        // the client, and that is how it reaches it. This is FRONTEND-CONTRACT
        // §5 rule 3.
        if (used) {
            keyEvent.filterAndAccept();
        }
    }

    void reset() {
        if (hunminIc_) {
            hunmin_ic_reset(hunminIc_);
        }
        switcher_.discard();
        updateUI();
    }

    void flush() {
        if (!hunminIc_) {
            return;
        }
        if (const char *s = hunmin_ic_flush(hunminIc_); s && *s) {
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
        const std::string &want =
            layout.empty() ? *engine_->config().layout : layout;
        if (hunminIc_ && want == layout_) {
            return;
        }
        if (hunminIc_) {
            flush();
            hunmin_ic_free(hunminIc_);
            hunminIc_ = nullptr;
            switcher_.discard();
        }
        layout_ = want;
        hunminIc_ = hunmin_ic_new(engine_->ctx(), want.c_str());
        if (!hunminIc_) {
            FCITX_ERROR() << "hunmin: unsupported layout: " << want;
        }
    }

    void updateUI() {
        /* The preedit, not the panel, is the thing that gets replaced: an
         * empty preedit must actively clear whichever preedit slot was set
         * last time, or the previous composition stays on screen after the
         * client already consumed it. This is why inputPanel().reset() is not
         * simply called at the top -- that would also wipe the candidate
         * window, which must outlive the preedit it was opened from. */
        if (hunminIc_) {
            const char *preedit = hunmin_ic_preedit(hunminIc_);
            bool useClient = ic_->capabilityFlags().test(CapabilityFlag::Preedit);
            Text text;
            if (preedit && *preedit) {
                text.append(preedit);
                text.setCursor(std::strlen(preedit));
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

    HunminEngine *engine_;
    InputContext *ic_;
    ModeSwitchAdapter switcher_;
    hunmin_ic *hunminIc_ = nullptr;
    std::string layout_;
    int candidateCount_ = 0;
    std::string candidateSource_;
};

HunminCandidateWord::HunminCandidateWord(Text text, Text comment,
                                         HunminEngine *engine,
                                         InputContext *ic, std::string hanja)
    : CandidateWord(std::move(text)), engine_(engine), ic_(ic),
      hanja_(std::move(hanja)) {
    setComment(std::move(comment));
}

void HunminCandidateWord::select(InputContext *inputContext) const {
    auto *state = inputContext ? inputContext->property("hunminState")
                               : nullptr;
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
        list->append(std::make_unique<HunminCandidateWord>(
            text, Text(detail), engine_, ic_,
            hunmin_suggest_text(engine_->ctx(), i)));
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
    list->setPageSize(candidateCount_ < kMaxCandidatePageSize
                          ? candidateCount_
                          : kMaxCandidatePageSize);
    list->setGlobalCursorIndex(0);
    list->setLayoutHint(CandidateLayoutHint::Vertical);
    ic_->inputPanel().setCandidateList(std::move(list));
}

HunminEngine::HunminEngine(Instance *instance)
    : instance_(instance),
      factory_([this](InputContext &ic) { return new HunminState(this, &ic); }) {
    // Data path: HUNMIN_DATA_PATH first (tests, source trees), then the
    // installed share/hunmin default.
    ctx_ = hunmin_ctx_new(nullptr, 0);
    if (!ctx_) {
        const char *dirs[] = {HUNMIN_DEFAULT_DATA_DIR};
        ctx_ = hunmin_ctx_new(dirs, 1);
    }
    if (!ctx_) {
        FCITX_ERROR() << "hunmin: no layout data found (HUNMIN_DATA_PATH, "
                      << HUNMIN_DEFAULT_DATA_DIR << ")";
    }
    // Hand the configtool its layout list. This has to happen after ctx_ is
    // built (the ids and names come from it) and before the first getConfig(),
    // which is when dumpDescription() runs. Re-injected in reloadConfig() too,
    // because the data directory can change under a config reload -- a stale
    // annotation would show layouts the engine can no longer open.
    refreshLayoutList();
    reloadConfig();
    instance_->inputContextManager().registerProperty("hunminState",
                                                      &factory_);
}

void HunminEngine::activate(const InputMethodEntry &,
                            InputContextEvent &event) {
    if (auto *state = event.inputContext()->propertyFor(&factory_)) {
        state->reset();
    }
}

void HunminEngine::deactivate(const InputMethodEntry &entry,
                              InputContextEvent &event) {
    reset(entry, event);
}

void HunminEngine::keyEvent(const InputMethodEntry &entry,
                              KeyEvent &keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }
    auto *state = keyEvent.inputContext()->propertyFor(&factory_);
    state->keyEvent(layoutFor(entry), keyEvent);
}

std::string HunminEngine::layoutFor(const InputMethodEntry &entry) const {
    return instance_->inputMethodManager()
        .currentGroup()
        .layoutFor(entry.uniqueName());
}

void HunminEngine::reset(const InputMethodEntry &, InputContextEvent &event) {
    if (auto *state = event.inputContext()->propertyFor(&factory_)) {
        state->flush(); // composing text is not lost on focus change
        state->reset();
    }
}

void HunminEngine::refreshLayoutList() {
    if (!ctx_) {
        return;
    }
    std::vector<std::pair<std::string, std::string>> layouts;
    const int count = hunmin_ctx_layout_count(ctx_);
    layouts.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; i++) {
        const char *id = hunmin_ctx_layout_id(ctx_, i);
        if (!id) {
            continue;
        }
        // The ko name is the picker label: every layout carries one in its
        // JSON (SPEC-SCHEMA.md) and doctor.py warns when either is missing.
        // Skip only what the build genuinely cannot open -- an engine kind we
        // do not implement would be a row that fails at keystroke time.
        if (!hunmin_ctx_layout_supported(ctx_, i)) {
            continue;
        }
        const char *ko = hunmin_ctx_layout_name(ctx_, i, "ko");
        layouts.emplace_back(id, ko ? ko : id);
    }
    config_.layout.annotation().setLayouts(std::move(layouts));
}

void HunminEngine::reloadConfig() {
    refreshLayoutList();
    readAsIni(config_, "conf/hunmin.conf");
}

void HunminEngine::setConfig(const fcitx::RawConfig &rawConfig) {
    config_.load(rawConfig, true);
    instance_->inputContextManager().foreach([this](InputContext *ic) {
        state(ic)->reset(); // in-flight composition dropped; layout swaps
        return true;        // lazily on the next key (ensureIc)
    });
    safeSaveAsIni(config_, "conf/hunmin.conf");
}

HunminState *HunminEngine::state(InputContext *ic) {
    return ic->propertyFor(&factory_);
}

} // namespace fcitx

FCITX_ADDON_FACTORY_V2(hunmin, fcitx::HunminEngineFactory);
