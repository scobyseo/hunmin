/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * FRAGMENT of tsf.cpp, not a standalone TU: the F9/configured-key Hanja
 * candidate flow (issue #67) - the lookup state helpers, the open/close
 * motion handlers, and the commit that replaces the anchor. Included
 * INSIDE CTextService's body at the site the flow occupies.
 */
/* --- F9 Hanja candidate window (issue #67) ---
     *
     * The engine serves the list (hunmin_suggest_count/text/detail/kind over
     * the hanja/idiom/krdict tables, keyed by the WHOLE Hangul word); the
     * adapter supplies everything else: the lookup text is the buffered
     * word, the keyboard flow mirrors fcitx5 (digits pick, Up/Down moves,
     * PageUp/PageDown pages, Return commits the cursor, Esc closes, any
     * other key closes and falls through so typing continues) and the
     * pop-up itself draws the rows.
     *
     * Page cap: kMaxCandidatePageSize, the value fcitx5's label ceiling
     * forces (ruled by the maintainer for issue #45 — engine.cpp:471-486:
     * 10 candidates survive, 11 abort). The TSF popup owns its own drawing
     * rather than fcitx5's label table, so it could show more rows than
     * that, but the SAME ruled number is kept: a word candidate list the
     * digits can reach is 1..0 per page, and 67-row popups measured ~2000px
     * tall on fcitx5's host. PageUp/PageDown are what the rest of the list
     * is behind. */
static const int kMaxCandidatePageSize = 10;

/* The Hangul the candidate window anchors to: accumulated finalised
     * syllables plus the in-flight preedit. Valid only while the window is
     * open — opening may not flush anything (fcitx5's openCandidates rule:
     * hunmin_ic_flush would commit the anchor out from under the list). */

/* The last UTF-8 character of s, by a forward walk over the code points
     * (issue #100's "last syllable": in a composed word each character is a
     * syllable; for an interrupted composition the last character may be a
     * lone jamo, and that is the syllable the user was typing). Returns the
     * empty string only when s itself is empty. */
static std::string LastSyllableOf(const std::string &s) {
    size_t start = 0;
    for (size_t i = 0; i < s.size();) {
        start = i;
        unsigned char b = (unsigned char)s[i];
        i += (b >= 0xF0) ? 4 : (b >= 0xE0) ? 3
                           : (b >= 0xC0)   ? 2
                                           : 1;
    }
    return s.substr(start);
}

/* The candidate-count decision BOTH phases share (test and down are one
     * decision): the whole buffered word is looked up first (the anchor the
     * dictionary tables are keyed by), and when that yields 0 while the
     * word has more than one syllable, the LAST syllable is looked up
     * instead (issue #100). The returned pair is (count, fallback): count 0
     * means the key belongs to the client in both phases; a fallback count
     * > 0 opens a syllable-anchored list (candFallback_). */
std::pair<int, bool> HanjaListCount() {
    std::string word = delivery_->CandidateAnchor();
    if (ctx_ && !word.empty()) {
        int n = hunmin_suggest_count(ctx_, word.c_str());
        if (n > 0)
            return {n, false};
        std::string last = LastSyllableOf(word);
        if (last != word) { /* more than one character => more than one syllable */
            n = hunmin_suggest_count(ctx_, last.c_str());
            if (n > 0)
                return {n, true};
        }
    }
    return {0, false};
}

bool OpenCandidates() {
    std::pair<int, bool> look = HanjaListCount();
    CountToState(look.first, look.second);
    return candActive_;
}

/* Recompute the list state from a (count, fallback) lookup answer. */
void CountToState(int n, bool fallback) {
    candFallback_ = fallback;
    if (fallback) {
        candSource_ = LastSyllableOf(delivery_->CandidateAnchor());
    } else {
        candSource_ = delivery_->CandidateAnchor();
    }
    candCount_ = n;
    candActive_ = candCount_ > 0;
    candCursor_ = 0;
    candTop_ = 0;
}

void CloseCandidateWindow() {
    candActive_ = false;
    candCount_ = 0;
    candCursor_ = 0;
    candTop_ = 0;
    candSource_.clear();
    candWnd_.Hide();
}

void PageFollowCursor() {
    candTop_ = (candCursor_ / kMaxCandidatePageSize) * kMaxCandidatePageSize;
}

void PageStep(int dir) {
    int lastTop = candCount_ > 0
                      ? ((candCount_ - 1) / kMaxCandidatePageSize) * kMaxCandidatePageSize
                      : 0;
    int top = candTop_ + dir * kMaxCandidatePageSize;
    if (top < 0)
        top = 0;
    if (top > lastTop)
        top = lastTop;
    candTop_ = top;
    /* The cursor moves with the page when it would be left off it. */
    if (candCursor_ < candTop_)
        candCursor_ = candTop_;
    else if (candCursor_ >= candTop_ + kMaxCandidatePageSize)
        candCursor_ = candTop_ + kMaxCandidatePageSize - 1 < candCount_
                          ? candTop_ + kMaxCandidatePageSize - 1
                          : candCount_ - 1;
}

/* fcitx5 semantics (engine.cpp:160-176): choosing a candidate drops the
     * composition it was anchored to, then commits the Hanja in its place —
     * no surrounding-text surgery anywhere. The buffered Hangul has not
     * been typed into the document yet, so nothing needs removing: the
     * buffer itself is replaced by the chosen text, which is then inserted
     * through the normal boundary path. */
void CommitCandidate(ITfContext *pic, int idx) {
    if (idx < 0 || idx >= candCount_ || !ctx_)
        return;
    /* Copy first: the pointer is valid only until the next
         * hunmin_suggest_* call on ctx. */
    std::string hanja(hunmin_suggest_text(ctx_, idx));
    if (hanja.empty())
        return;
    /* Capture the word anchor BEFORE the commit: on a last-syllable
         * fallback list (issue #100) the buffered word's own state is what
         * the replacement is computed from, and the commit resets the
         * in-flight preedit. The mode seam decides how (and whether) the
         * replacement text is delivered. */
    std::string replacement = hanja;
    if (candFallback_) {
        std::string word = delivery_->CandidateAnchor();
        std::string last = LastSyllableOf(word);
        /* Only last syllable replaced: the word prefix stays Hangul. */
        replacement = word.substr(0, word.size() - last.size()) + hanja;
    }
    std::wstring rep = HunminUtf8ToWide(replacement.c_str());
    bool committed = delivery_->CandidateCommit(pic, rep);
    CloseCandidateWindow();
    (void)committed;
}

void UpdateCandidateWindow(ITfContext *pic) {
    if (!candActive_ || !pic || !ic_) {
        candWnd_.Hide();
        return;
    }
    RECT rc;
    bool haveCaret = false;
    GUITHREADINFO gti;
    if (!AcquireCaretRect(pic, &rc, &haveCaret, &gti)) {
        /* No metrics, no caret: same hide path the overlay takes. The
             * candidate state stays open — a host that reports no window
             * geometry today may report it after the next focus, and the
             * next F9/arrow key redraws. */
        candWnd_.Hide();
        return;
    }
    HWND fontHost = haveCaret ? (gti.hwndFocus ? gti.hwndFocus : gti.hwndCaret) : nullptr;
    std::vector<std::wstring> lines;
    int end = candCount_ < candTop_ + kMaxCandidatePageSize
                  ? candCount_
                  : candTop_ + kMaxCandidatePageSize;
    for (int i = candTop_; i < end; ++i) {
        std::wstring text(HunminUtf8ToWide(hunmin_suggest_text(ctx_, i)));
        wchar_t label = (wchar_t)(((i - candTop_) == 9) ? '0' : ('1' + (i - candTop_)));
        std::wstring line = std::wstring(i == candCursor_ ? L">> " : L"   ") + label +
                            L". " + text;
        lines.push_back(line);
    }
    candWnd_.Show(rc, fontHost, lines);
}