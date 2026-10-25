/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "language_detect.h"

#include <fcitx-utils/log.h>

namespace fcitx {

/* Replay `word_` on `latinLayout` and return the text it produces.
 *
 * Two readers, both destructive, so each is read exactly once (FRONTEND-CONTRACT
 * §5 rule 1 is about one keypress, and this is the same hazard at word scale):
 * hunmin_ic_commit() for everything committed during the word and
 * hunmin_ic_preedit() for whatever is still composing when the word ends.
 * Reading them in the other order, or reading either twice, loses or duplicates
 * text. */
std::string LanguageDetector::judge(const std::string &latinLayout, size_t minLen) {
    std::string out;
    if (word_.size() < minLen || word_.empty()) {
        word_.clear();
        return out;
    }

    /* Fresh per judgement: a persistent shadow would carry this word's buffer
     * into the next one, and the reset that would fix it risks leaking a
     * w/W/z/h/H override across the boundary. ~3 us, so correctness wins. */
    hunmin_ic *shadow = hunmin_ic_new(ctx_, latinLayout.c_str());
    if (!shadow) {
        FCITX_ERROR() << "hunmin: language_detect: no such layout: " << latinLayout;
        word_.clear();
        return out;
    }

    for (uint32_t ch : word_) {
        hunmin_ic_process(shadow, ch);
        /* Read the commit after EVERY key, not once at the end.
         *
         * hunmin_ic_process() clears ic->commit on entry, so a single read
         * after the loop keeps only the last key's commit: replaying "kbs"
         * returned "s". Measured, and it is the same defect the sim --batch
         * mode had (fixed in cfe43f9) -- the destructive read has to happen
         * per keypress, which is exactly what the addon's own keyEvent does
         * for the Hangul context. */
        const char *committed = hunmin_ic_commit(shadow);
        if (committed) {
            out += committed;
        }
    }

    /* Preedit is non-destructive, so one read at the end catches whatever is
     * still composing when the word stops. */
    const char *preedit = hunmin_ic_preedit(shadow);
    if (preedit) {
        out += preedit;
    }

    hunmin_ic_free(shadow);
    word_.clear();
    return out;
}

} // namespace fcitx