/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * keydispatch.cpp - the key path: the test/down/up handlers, the passage
 * and modifier predicates, the mode toggle, the candidate-key routing and
 * the engine feed, relocated from tsf.cpp unchanged. These are member
 * definitions of CTextService (declared in tsf.h); the shell keeps the
 * lifecycle and document callbacks.
 */
#include "tsf.h"

#include "delivery.h" /* HUNMIN_TSF_DELIVERY_INTERIM */
#include "utf.h" /* HunminUtf8ToWide: the layer speaks UTF-8 */

#include <string>

STDMETHODIMP CTextService::OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) {
    (void)lParam;
    /* The previous key's deferred doc write runs before this key is
     * classified - a keystroke storm must not reorder the doc writes
     * behind the engine state. */
    FlushPendingDocWrites();
    /* A new classification invalidates the previous test-phase feed (the
     * host may not have called OnKeyDown for it). */
    testDeclinedKey_ = 0;
    /* Shared predicate with OnKeyDown (the registration-work rule: test
     * and down are one decision). The configured set always contains the
     * bare Han/Eng key, so the physical key stays interested in both
     * modes. CapsLock is NOT here any more: it is no longer the mode
     * toggle (maintainer instruction: the Han/Eng key is the default),
     * and in Korean input mode it is ignored outright (the engine's
     * case-encoding convention). */
    BOOL eat = FALSE;
    if (IsLanguageKeyVk(wParam))
        /* The compound press and the plain toggle are answered by the same
         * predicate the down uses (LanguageKeyEaten asks the engine's peek), so
         * a refused chord passes in both phases instead of being lost. */
        eat = LanguageKeyEaten(wParam);
    else if (IsModeToggle(wParam))
        eat = TRUE;
    else if (cand_->Active() && cand_->EatsListKey((unsigned long)wParam))
        /* While the candidate window is open its keys are ours, on the
         * same shared-predicate rule the toggle uses (test and down are
         * one decision). */
        eat = TRUE;
    else if (!hangulMode_ && !AutoswitchOn())
        /* The English passthrough (ruled 2026-11-02: with autoswitch ON the
         * engine keeps the keys so ReqMode8's E1 line can answer Hangul). Both key
         * phases answer the same way, or the host loses a key the down phase
         * would have claimed. */
        eat = FALSE;
    else if (hangulMode_ && hunmin_tsf_keybindings_match(&hanjaCombo_, 1, (unsigned)wParam, hunmin_tsf_keymods_from_state())) {
        /* Mirror the down decision exactly (no look-ahead lie): the
         * window opens when the buffered word has candidates - the
         * whole-word lookup, or issue #100's last-syllable fallback
         * when the whole word yields 0 - and in the zero-and-zero case
         * the key belongs to the client. The suggest lookup is a read
         * of tables already in memory. */
        bool fallback = false;
        eat = cand_->ListCount(&fallback) > 0 ? TRUE : FALSE;
    } else if (hunmin_tsf_keymods_from_state() & (HUNMIN_TSF_KEYMOD_CTRL | HUNMIN_TSF_KEYMOD_ALT)) {
        /* Mirror of the down-side chord rule: a Ctrl/Alt chord belongs
         * to the app (Ctrl+A is not composition input) - test phase
         * answers the same uneaten the down phase will act on. The gate is
         * mode-free: the English-passthrough branch above already returned
         * when the engine owns no keys, and with the switch ON the engine
         * owns Latin keys too, so a chord past the toggle must pass there
         * as well. Gating it to Hangul mode was the measured defect of
         * issue #222: in Latin mode with the switch on, Ctrl+Z test-eaten
         * (the letter branch below) while the down phase passed it. */
        eat = FALSE;
    } else if (wParam >= 'A' && wParam <= 'Z') {
        eat = TRUE;
    } else if (wParam == VK_SPACE || wParam == VK_RETURN) {
        /* A boundary key is a key of passage (#95 contract): the app
         * inserts the space / paragraph break. MEASURED on msftedit:
         * with eat=FALSE the host NEVER calls OnKeyDown for the key -
         * the test phase is the TIP's only sight of it - so the
         * boundary drain runs HERE. The span bookkeeping is
         * position-based, so the drain is order-independent against
         * the app's own insertion, and a host that DOES call the down
         * re-runs the flush on the already-empty buffer (a no-op).
         * Enter must be here for the same reason and one more: if the
         * span survives the break, the next syllable's span write
         * selects the OLD line and drags the caret back to it. */
        BoundaryFlush(pic);
        eat = FALSE;
    } else if (wParam == VK_BACK) {
        /* Against in-flight text the engine consumes the key (the jamo
         * deletes). At an EMPTY in-flight the engine declines - and on
         * the interim path the key must then PASS, so the app deletes
         * the committed char (measured: eaten = the backspace does
         * nothing at all); the span bookkeeping self-heals at the next
         * write (the fresh-span anchor re-reads the live caret). The
         * native path keeps the key: its declined branch is the
         * #98-verified composition-close shape. */
        const char *pre = preedit_ ? hunmin_buffered_preedit_display(preedit_) : "";
        eat = (legacyMode_ && pre[0] == '\0') ? FALSE : TRUE;
    } else if (preedit_ && (hangulMode_ || AutoswitchOn()) && !IsKeyOfPassage(wParam) && VkToBase(wParam) != 0 && !IsModifierVk(wParam)) {
        /* EVERY feedable key is interested in hangul mode, the OEM
         * punctuation class included. Sin3-p2's jamo live on this row
         * (; ' /;  measured 2026-10-21).
         *
         * The answer must be the ENGINE's answer, not "every feedable key
         * is ours": a classic default-sink host suppresses insertion after
         * a TRUE test claim and does not call OnKeyDown for a key the
         * down phase would decline, so claiming an unconsumed digit loses
         * it (the interim `dkssud1` defect). Peek on a copy of the engine
         * state: consumed -> claim, down feeds as before; declined -> feed
         * the real engine HERE (the decline itself commits the in-flight
         * syllable), drain the word ahead of the host's own insertion, and
         * pass. testDeclinedKey_ makes a down call a no-op so the key is
         * never fed twice. */
        const unsigned int mods = hunmin_tsf_keymods_from_state();
        hunmin_buffered_preedit_set_option(preedit_, "caps", hunmin_tsf_capslock_from_state() ? 1 : 0);
        const uint32_t base = VkToBase(wParam);
        const uint32_t shift = (mods & HUNMIN_TSF_KEYMOD_SHIFT) ? 1u : 0u;
        const bool consumed = hunmin_buffered_preedit_peek_key(preedit_, base, shift);
        hunmin_logger.log(" test feed peek base=%u shift=%u consumed=%d\n", (unsigned)base, (unsigned)shift, (int)consumed);
        if (consumed) {
            eat = TRUE;
        } else {
            BOOL fed = FALSE;
            FeedEngineKey(pic, wParam, &fed);
            testDeclinedKey_ = wParam;
            eat = fed ? TRUE : FALSE;
        }
    }
    if (pfEat)
        *pfEat = eat;
    hunmin_logger.log("OnTestKeyDown w=%lu eat=%d mode=%d\n", (unsigned long)wParam, (int)eat, (int)hangulMode_);
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) {
    /* Same entry flush as the test phase: whichever callback the host
     * reaches first settles the previous key's doc write. */
    FlushPendingDocWrites();
    TrackKeyContext(pic);
    hunmin_logger.log("OnKeyDown wParam=%lu layer=%p\n", (unsigned long)wParam, (void *)preedit_);
    if (pfEat)
        *pfEat = FALSE;
    if (testDeclinedKey_ == wParam) {
        /* OnTestKeyDown already fed this key and made the pass/claim
         * decision; the host is only now asking the down phase. */
        testDeclinedKey_ = 0;
        hunmin_logger.log(" test-phase feed already settled -> pass\n");
        return S_OK;
    }
    if (!preedit_) {
        hunmin_logger.log(" ic null -> pass\n");
        return S_OK;
    }
    /* Each handler owns one key class and answers one question: is the
     * key ours (pfEat), and what state changes. The order IS the
     * precedence: the toggle, the mode passthrough, the candidate
     * window's own key, the open window's list keys, the keys of
     * passage, and finally the composition feed. */
    if (IsLanguageKeyVk(wParam)) {
        /* The ruled two-part meaning (SPEC-MODES §E5, 2026-10-31): the bare
         * press (and any chord nothing is bound to) is the manual switch, a
         * chord with a function bound is that function. Which is which is the
         * engine's answer, not the adapter's. */
        ApplyLanguageKeyPress(pic, wParam, lParam, pfEat);
        return S_OK;
    }
    if (IsModeToggle(wParam)) {
        /* A configured chord that is NOT the language key (Shift+Space, Right
         * Alt): the plain toggle, exact-modifier matched as before. */
        ApplyModeToggle(pic, wParam, lParam, pfEat);
        return S_OK;
    }
    /* The English passthrough, ruled 2026-11-02: "autoswitch ON stops English
     * passthrough to support eng -> han support". With the switch on the engine
     * keeps seeing the keys in Latin mode too, so ReqMode8's E1 line can judge a word
     * typed there and answer Hangul. The delivered TEXT is unchanged either way
     * (the layer commits each Latin character as the host did, ReqMode5); what changes
     * is who delivers it. With the switch OFF this branch is exactly what
     * shipped, and the interim delivery keeps it too: that path cannot carry the
     * judgement (ruled 2026-11-02), so the feature is off there and the engine
     * is not given keys it cannot act on. */
    if (!hangulMode_ && !AutoswitchOn())
        return S_OK; // passthrough English
    /* The two candidate surfaces stay Hangul-mode things: a Hanja lookup of a
     * Latin word is not a question anyone asked, and opening a list in English
     * mode would make a passthrough key dead. */
    if (hangulMode_ && IsCandidateToggle(wParam)) {
        cand_->Toggle(pic, (unsigned long)wParam, pfEat);
        return S_OK;
    }
    if (hangulMode_ && cand_->Active() && cand_->ListKey(pic, (unsigned long)wParam, pfEat))
        return S_OK;
    if (IsKeyOfPassage(wParam)) {
        DrainAndPass(pic, wParam, pfEat, "edit key");
        return S_OK;
    }
    if (wParam >= VK_F1 && wParam <= VK_F12) {
        DrainAndPass(pic, wParam, pfEat, "fkey");
        return S_OK;
    }
    FeedEngineKey(pic, wParam, pfEat);
    return S_OK;
}

void CTextService::TrackKeyContext(ITfContext *pic) {
    if (lastCtx_ == pic)
        return;
    if (lastCtx_)
        lastCtx_->Release();
    lastCtx_ = pic;
    if (lastCtx_)
        lastCtx_->AddRef();
    if (composition_ && compCtx_ && compCtx_ != pic)
        DetachComposition();
}

bool CTextService::IsModeToggle(WPARAM wParam) {
    return hunmin_tsf_keybindings_match(toggleKeys_.data(), (int)toggleKeys_.size(), (unsigned)wParam, hunmin_tsf_keymods_from_state()) != FALSE;
}

bool CTextService::IsLanguageKeyVk(WPARAM wParam) {
    /* The VK of a configured language key, whatever modifiers are held: the
     * binding match above is EXACT (a bare binding does not fire under a
     * modifier, and that rule is what keeps Shift+Space distinct), so the
     * compound press -- the same physical key with a chord modifier -- is
     * recognised here and settled by the engine's table. Without this the
     * chord never reached the layer at all (measured: Ctrl+한/영 fell through
     * to the composition feed). */
    for (const HunminKeyCombo &c : toggleKeys_)
        /* Only a BARE entry names the language key. A configured chord such as
         * Shift+Space is a language-key ALIAS, not the physical key: its key is
         * Space, and treating Space as the language key would make the compound
         * press eat an ordinary space (measured). */
        if (c.vk == (unsigned int)wParam && c.mods == 0)
            return true;
    return false;
}

void CTextService::ApplyModeToggle(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) {
    if (pfEat)
        *pfEat = TRUE;
    const bool repeat = (lParam & (1 << 30)) != 0;
    if (!repeat)
        ToggleModeCore(pic);
    hunmin_logger.log(" toggle key vk=%lu repeat=%d -> %s (%s)\n", (unsigned long)wParam, (int)repeat, hangulMode_ ? "hangul" : "english",
                      hangulMode_ ? hangulId_.c_str() : latinId_.c_str());
}

/* The Han/Eng key's two positions (SPEC-MODES §E5, ruled 2026-10-31): the bare
 * press is the manual switch, a chord modifier held with it is the
 * special-function namespace. The engine holds the assignment -- which chord
 * carries which function -- so this translates the platform state into the
 * shared key event and lets the layer answer; a per-frontend table of modifiers
 * is exactly what ReqMode13 forbids.
 *
 * The event carries `language_key`, which is the flag the classifier reads
 * first: that is why a chord on the language key can never be mistaken for
 * composition input, and why Shift travels in its own field (Shift + 한/영 is a
 * special combination by ruling, while Shift + a character is still a row). */
hunmin_key_event CTextService::LanguageKeyEvent(WPARAM wParam) const {
    const unsigned int mods = hunmin_tsf_keymods_from_state();
    hunmin_key_event ev{};
    ev.base = VkToBase(wParam);
    ev.shift = (mods & HUNMIN_TSF_KEYMOD_SHIFT) ? 1u : 0u;
    if (mods & HUNMIN_TSF_KEYMOD_CTRL)
        ev.mods |= HUNMIN_MOD_CTRL;
    if (mods & HUNMIN_TSF_KEYMOD_ALT)
        ev.mods |= HUNMIN_MOD_ALT;
    ev.id = HUNMIN_KEY_TEXT;
    ev.language_key = 1;
    return ev;
}

/* The language key, in both of its positions (SPEC-MODES §E5, ruled
 * 2026-10-31). The engine answers which one this press is: a chord with a
 * function bound is that function, everything else -- the bare press, and a
 * chord nothing is bound to -- is the manual switch, so no press of the key is
 * ever dead. A chord that IS a function is consumed: performed when a host can
 * service it, passed to the client when none can (the ruling's degradation for
 * a platform without the feature: "it can just refuse to service"). */
void CTextService::ApplyLanguageKeyPress(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) {
    const hunmin_key_event ev = LanguageKeyEvent(wParam);
    if (hunmin_buffered_preedit_classify(preedit_, &ev) != HUNMIN_KEY_LANGUAGE_CHORD) {
        /* The bare press, and a chord nothing is bound to: the plain toggle. */
        ApplyModeToggle(pic, wParam, lParam, pfEat);
        return;
    }
    const int fn = hunmin_buffered_preedit_special(preedit_, &ev);
    if (fn > 0) {
        /* The function performed. It is a boundary like any other action: what
         * was composing belongs to the text before it. */
        BoundaryFlush(pic);
        cand_->Close();
        if (pfEat)
            *pfEat = TRUE;
        hunmin_logger.log(" langchord vk=%lu fn=%s -> performed\n", (unsigned long)wParam, hunmin_special_name((hunmin_special_function)fn));
        return;
    }
    /* -1: bound, but nothing here can service it (no host, or the host refused).
     * The key passes and nothing is swallowed. */
    if (pfEat)
        *pfEat = FALSE;
    hunmin_logger.log(" langchord vk=%lu -> refused by this platform, key passes\n", (unsigned long)wParam);
}

/* The same answer for the platform's "test" phase. Test and down are ONE
 * decision: a host that is told "mine" in the test but then not given the key in
 * the down drops the press, so the test asks the engine's own peek rather than
 * guessing from the binding (measured defect class: `dkssud1`). */
bool CTextService::LanguageKeyEaten(WPARAM wParam) {
    const hunmin_key_event ev = LanguageKeyEvent(wParam);
    if (hunmin_buffered_preedit_classify(preedit_, &ev) != HUNMIN_KEY_LANGUAGE_CHORD)
        return true; /* the plain toggle: ours */
    return hunmin_buffered_preedit_special_peek(preedit_, &ev) > 0;
}

int CTextService::SpecialSupports(void *user, hunmin_special_function fn) {
    (void)user;
    /* What THIS platform can do, answered honestly per function: a refusal is a
     * legal, measured answer and the key passes instead of being swallowed.
     * Voice input has no Windows implementation in this frontend yet (Android
     * has a recogniser, TSF has none), so VOICE refuses here; the automatic
     * switch's dispatch is serviceable -- its data gate is the 0.3 feature
     * (2026-10-12 ruling), not this binding. */
    switch (fn) {
    case HUNMIN_SPECIAL_VOICE:
        return 0;
    case HUNMIN_SPECIAL_AUTOSWITCH:
        return 1;
    default:
        return 0;
    }
}

void CTextService::SpecialPerform(hunmin_special_function fn) {
    /* The engine resolved the gesture and this is the platform half. VOICE never
     * arrives (supports() refuses it). AUTOSWITCH flips the state the engine
     * owns -- the state itself is 0.3's data-gated feature (issue #2), so what
     * lands here today is the read-back, recorded, exactly as Android's service
     * does (`toggleAutoHangulEnglish`, "not yet implemented"). */
    lastSpecial_ = (int)fn;
    if (fn == HUNMIN_SPECIAL_AUTOSWITCH && preedit_) {
        int value = 0;
        if (hunmin_buffered_preedit_get_option(preedit_, "autoswitch", &value)) {
            autoswitch_ = value;
            HunminTsfConfig cfg;
            hunmin_tsf_config_load(HKEY_CURRENT_USER, &cfg);
            cfg.autoswitch = value;
            hunmin_tsf_config_save(HKEY_CURRENT_USER, &cfg);
        }
    }
    hunmin_logger.log(" special perform fn=%s\n", hunmin_special_name(fn));
}

bool CTextService::IsCandidateToggle(WPARAM wParam) {
    return hunmin_tsf_keybindings_match(&hanjaCombo_, 1, (unsigned)wParam, hunmin_tsf_keymods_from_state()) != FALSE;
}

bool CTextService::IsKeyOfPassage(WPARAM wParam) {
    switch (wParam) {
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_INSERT:
    case VK_DELETE:
        return true;
    default:
        /* The function-key block (measured with the unbound-F9 case of
         * issue #67: VK_F1..VK_F12 = 0x70..0x7B falls into the
         * printable fallback below, so an unbound F-key composed its
         * VK-value as a layout character - F9(0x78) typed 'x' = ㅈ into
         * the composition the user was watching. Same defect class,
         * same keys-of-passage answer. */
        return wParam >= VK_F1 && wParam <= VK_F12;
    }
}

void CTextService::DrainAndPass(ITfContext *pic, WPARAM wParam, BOOL *pfEat, const char *tag) {
    BoundaryFlush(pic);
    if (pfEat)
        *pfEat = FALSE;
    hunmin_logger.log(" %s vk=%lu -> boundary + passthrough\n", tag, (unsigned long)wParam);
}

/* The VK -> base half, and only that half (issue #106): the physical
 * key's UNSHIFTED US-QWERTY character. The shift column the adapter
 * used to carry is the engine's own map (schema_loader.c BASE/SHIFTED), selected
 * in the engine by the Shift bit that travels beside the base
 * (DESIGN-FRONTEND.md Part B A3'). */
uint32_t CTextService::VkToBase(WPARAM wParam) {
    if (wParam >= 'A' && wParam <= 'Z')
        return (uint32_t)(wParam + 32); /* the lowercase half */
    if (wParam >= '0' && wParam <= '9')
        return (uint32_t)wParam;
    if (wParam == VK_SPACE)
        return ' ';
    if (wParam == VK_BACK)
        return 0x08;
    switch (wParam) {
    case VK_OEM_1:
        return ';';
    case VK_OEM_7:
        return '\'';
    case VK_OEM_COMMA:
        return ',';
    case VK_OEM_PERIOD:
        return '.';
    case VK_OEM_2:
        return '/';
    case VK_OEM_3:
        return '`';
    case VK_OEM_4:
        return '[';
    case VK_OEM_5:
        return '\\';
    case VK_OEM_6:
        return ']';
    case VK_OEM_MINUS:
        return '-';
    case VK_OEM_PLUS:
        return '=';
    default:
        if (wParam >= 0x20 && wParam <= 0x7E)
            return (uint32_t)wParam;
        return 0;
    }
}

/* Modifier keys are not text: never feed them to the engine (would
 * corrupt an in-flight syllable). CapsLock is no longer the mode toggle
 * (maintainer instruction: the Han/Eng key is the default), and in
 * Korean input mode the key is ignored outright: it passes through for
 * the OS to toggle, and the engine `caps` option it feeds is set per
 * key event beside the modifier snapshot (issue #112) - never into the
 * (base, shift) pair the engine resolves rows from. */
bool CTextService::IsModifierVk(WPARAM wParam) {
    switch (wParam) {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
    case VK_CAPITAL:
    case VK_TAB:
        return true;
    default:
        return false;
    }
}

/* The composition feed: the engine decides consumed/declined. A chord
 * (Ctrl/Alt) is application input, not composition input - measured
 * defect 2026-10-18: the letter inside a Ctrl/Alt chord still fed the
 * composition, Ctrl+A typed the preedit's next syllable instead of
 * select-all. The chord commits through the SAME boundary flush a mode
 * toggle drives and passes uneaten; the mode itself is NOT flipped
 * (ruled out 2026-10-18, issue #101: "will not switch mode; transient
 * processing is enough" - docs/WINDOWS-TSF-BUFFERING.md and
 * wiki/decisions/2026-10-18-tsf-chord-transient-not-persistent.md).
 * A decline is a word boundary for the buffering adapter (issue #73):
 * the boundary drain runs and the key passes (pfEat stays FALSE) -
 * the fcitx/android "used=FALSE => passthrough" contract. */
void CTextService::FeedEngineKey(ITfContext *pic, WPARAM wParam, BOOL *pfEat) {
    /* One snapshot of the modifier state for this event. The chord rule
     * below and the shift bit handed to the engine both read it, so the
     * decision and the bit can never disagree, and the harness's
     * key-state registration (keybinding.h) covers both with one
     * registered provider. */
    const unsigned int mods = hunmin_tsf_keymods_from_state();
    /* The engine caps option, fed per event beside the modifier snapshot
     * (issue #112, ruling "wire and make it common to all platform"):
     * one set per key event, 1 when the platform CapsLock is toggled on,
     * 0 when off - the shape fcitx5 has had all along (hunminadapter.cpp:327).
     * The option feeds the yethangul P variable only (hunmin.h:93); it
     * selects NO row - the Shift bit stays the only row transport
     * (the engine's case endpoint, hunmin_ic_process_key). */
    hunmin_buffered_preedit_set_option(preedit_, "caps", hunmin_tsf_capslock_from_state() ? 1 : 0);
    if (IsModifierVk(wParam)) {
        /* A bare modifier is not text and must not disturb the composition. */
        hunmin_logger.log(" modifier/pass key -> leave composition\n");
        return;
    }
    /* The key's MEANING is the layer's (issue #128, decision C): the adapter
     * maps the VK to (base, shift, mods) and the precedence -- chord, unmapped
     * -- is asserted once in tier A's engine-key-rule. The boundaries here used
     * to be two hand-written branches that had to agree with fcitx5's. */
    hunmin_key_event ev{};
    ev.base = VkToBase(wParam);
    ev.shift = (mods & HUNMIN_TSF_KEYMOD_SHIFT) ? 1u : 0u;
    ev.mods = (mods & (HUNMIN_TSF_KEYMOD_CTRL | HUNMIN_TSF_KEYMOD_ALT)) ? HUNMIN_MOD_CHORD : 0u;
    ev.id = HUNMIN_KEY_TEXT; /* a TSF key of passage never reaches this path */
    if (hunmin_buffered_preedit_classify(preedit_, &ev) != HUNMIN_KEY_FEED) {
        cand_->Close();
        BoundaryFlush(pic);
        if (pfEat)
            *pfEat = FALSE;
        hunmin_logger.log(" classifier: boundary -> commit + passthrough\n");
        return;
    }
    hunmin_key_result out{};
    const bool consumed = hunmin_buffered_preedit_process_key(preedit_, ev.base, ev.shift, &out) != 0;
    if (!consumed) {
        /* A key the engine declines is a boundary AND a pass-through: the layer
         * has already delivered the word in out.commit, which starts the
         * delivery exactly as the old BoundaryFlush did. It also ALREADY judged
         * that word (the decline is the boundary), so the judgement is taken
         * here and acted on -- a second delivery drain would ask the layer for a
         * word it no longer holds, and its boundary is what replaces the
         * judgement with -1 (issue #175: that is how the answer used to be
         * discarded before it was read). */
        if (out.commit && *out.commit)
            InsertText(pic, HunminUtf8ToWide(out.commit));
        /* The engine judged the word it just delivered; the mode change is the
         * frontend's half (ruled 2026-11-01, #164). Asked after the delivery so
         * the text inserted above is the one the judgement was made on. */
        ApplyJudgement(pic);
        return;
    }
    /* A per-syllable commit unit would hand text back on a consumed key; the
     * buffered unit (TSF's, see RebuildLayer) hands back nothing until the
     * boundary. Inserting whatever came back covers both without the adapter
     * knowing which unit is in use. */
    if (out.commit && *out.commit)
        InsertText(pic, HunminUtf8ToWide(out.commit));
    hunmin_logger.log(" consumed, display len=%zu preedit=%s\n", ComposeDisplay().size(), out.preedit ? out.preedit : "");
    /* Issue #119: the legacy interim path when the host was detected
     * terminating compositions mid-word (or the env said so); the #98
     * native path otherwise stays as is. The doc write is POSTED -
     * never inline in a TSF callback (the measured focus-storm defect). */
    delivery_->Consumed(pic);
    if (pfEat)
        *pfEat = TRUE;
}

STDMETHODIMP CTextService::OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) {
    (void)pic;
    (void)wParam;
    (void)lParam;
    if (pfEat)
        *pfEat = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) {
    (void)pic;
    (void)wParam;
    (void)lParam;
    if (pfEat)
        *pfEat = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnPreservedKey(ITfContext *pic, const GUID &rguid, BOOL *pfEat) {
    (void)pic;
    (void)rguid;
    if (pfEat)
        *pfEat = FALSE;
    return S_OK;
}
