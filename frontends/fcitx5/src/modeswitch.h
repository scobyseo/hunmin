/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef FCITX5_HUNMIN_MODESWITCH_H_
#define FCITX5_HUNMIN_MODESWITCH_H_

#include <cstdint>
#include <hunmin.h>
#include <string>
#include <vector>

namespace fcitx {

/* R8 mode judgement, implemented as a shadow-layout adapter (SPEC-MODES R2).
 *
 * THE PROBLEM THIS SOLVES
 * -----------------------
 * A Hangul layout cannot produce Latin. Measured across all 12 Hangul layouts
 * in this repo, pressing all 47 key ids: zero Latin letters, jamo and digits
 * only. So when the user types English on sin3-p2, the Latin text is not
 * mistranslated -- it is destroyed, and there is nothing left to switch to.
 *
 * THE MECHANISM
 * -------------
 * No ABI change. The core has no way to change an input context's layout
 * (ic->L is assigned once in hunmin_ic_new), but it does not need one: this
 * adapter keeps a *second* input context on the Latin layout beside the Hangul
 * one, and replays the buffered keystrokes into it when composition completes.
 * That is SPEC-MODES R2's "English mode picks qwerty" implemented entirely on
 * the frontend side.
 *
 * THE BOUNDARY
 * ------------
 * hunmin_ic_preedit() returning empty IS the completion signal. No new API is
 * needed: preedit is non-destructive and readable as often as wanted. Note a
 * Hangul layout does not end composition by itself -- 'cf', 'cfm' and 'cfc' all
 * keep composing; only a separator such as space empties the preedit. So the
 * rule is "preedit went empty", not "the engine decided".
 *
 * WHY THE SHADOW CONTEXT IS BUILT FRESH PER WORD
 * ----------------------------------------------
 * A persistent shadow context would carry word N's buffer into word N+1. Rather
 * than reset it between words (which risks leaking a flag override across a
 * boundary), the shadow is created and destroyed per judgement. Measured cost
 * of one judgement: hunmin_ic_new + 8 keys + free is 3.48 us on sin3-p2 and
 * 2.85 us on us-qwerty, so a word costs about 6 us -- nothing against a
 * keystroke's 16 ms budget.
 *
 * WHAT IT DOES NOT DECIDE
 * -----------------------
 * This class judges and reports. It does not commit, does not touch the
 * client's surrounding text, and does not decide which Latin layout is
 * correct. R8's validity test, the mode actually delivered to the client, and
 * the Latin layout set are the maintainer's decisions; see
 * docs/SPEC-MODES.md R8 and R10.
 */
class ModeSwitchAdapter {
public:
    explicit ModeSwitchAdapter(hunmin_ctx *ctx) : ctx_(ctx) {}

    /* Feed every keystroke the Hangul context sees, so the shadow can replay
     * the word when it ends. Cheap: a push_back. */
    void record(uint32_t ch) { word_.push_back(ch); }

    /* The boundary: called when hunmin_ic_preedit() went empty.
     *
     * Returns the Latin text this word would have produced on `latinLayout`,
     * or an empty string when there is nothing to switch to. The caller
     * decides what to do with it -- R10 says switching is not retroactive, so
     * the Hangul may already be in the client and this is advice about the
     * NEXT keystroke, not a rewrite.
     *
     * `minLen` is R9's short-word guard: a word shorter than the threshold is
     * never switched, which also protects against a single stray keystroke
     * being treated as a one-letter English word. */
    std::string judge(const std::string &latinLayout, size_t minLen = 2);

    /* Drop the buffer without judging. For focus loss, layout change, or any
     * point where the word is abandoned rather than completed. */
    void discard() { word_.clear(); }

    bool empty() const { return word_.empty(); }
    size_t size() const { return word_.size(); }
    const std::vector<uint32_t> &word() const { return word_; }

private:
    hunmin_ctx *ctx_;
    std::vector<uint32_t> word_;
};

} // namespace fcitx

#endif // FCITX5_HUNMIN_MODESWITCH_H_