/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The candidate flow implementation (issue #67 - the candidates are the
 * engine's suggest API; the flow was the adapter's). Defined against the
 * CandBackend seam: this TU knows nothing of CTextService. Ranked: the
 * lookup text is the buffered word, the keyboard flow mirrors fcitx5
 * (digits pick, Up/Down moves, PageUp/PageDown pages, Return commits the
 * cursor, Esc closes, any other key closes and falls through so typing
 * continues) and the pop-up itself draws the rows.
 *
 * Page cap: kMaxCandidatePageSize, the value fcitx5's label ceiling
 * forces (ruled by the maintainer for issue #45 - hunminadapter.cpp:471-486:
 * 10 candidates survive, 11 abort). The TSF popup owns its own drawing
 * rather than fcitx5's label table, so it could show more rows than
 * that, but the SAME ruled number is kept: a word candidate list the
 * digits can reach is 1..0 per page, and 67-row popups measured ~2000px
 * tall on fcitx5's host. PageUp/PageDown are what the rest of the list
 * is behind.
 */
#include "candidates.h"

#include "utf.h"

#include <string>
#include <utility>
#include <vector>
#include <windows.h>

static const int kMaxCandidatePageSize = 10;

/* The vk set the candidate window consumes while it is open: choose keys
 * (digits), motion (arrows, pages), Return/Esc. The OPEN/CLOSE toggle is the
 * configured combo, checked separately before this set; F9 is not here any
 * more by default (issue #67: the Hanja key is the default and F9 unbound).
 * Expressed once so the test and down halves cannot drift - the same
 * shared-predicate rule the toggle binding uses. */
static bool hunmin_tsf_candkey_eats_test(unsigned long vk) {
    return vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_PRIOR || vk == VK_NEXT || (vk >= VK_LEFT && vk <= VK_DOWN) || (vk >= '0' && vk <= '9');
}

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
        i += (b >= 0xF0) ? 4 : (b >= 0xE0) ? 3 : (b >= 0xC0) ? 2 : 1;
    }
    return s.substr(start);
}

class HunminCandidates : public Candidates {
public:
    explicit HunminCandidates(CandBackend &backend) : backend_(&backend) {}
    ~HunminCandidates() override = default;

    /* --- the flow state (the tier-F1 Snapshot reads these through the
     * shell) --- */
    bool candActive_ = false;
    int candCount_ = 0;
    int candCursor_ = 0; /* global 0-based cursor */
    int candTop_ = 0; /* first index of the visible 10-row page */
    std::string candSource_; /* the Hangul the list was looked up for */
    bool candFallback_ = false; /* the list came from the last-syllable fallback (issue #100) */
    HunminCandWindow candWnd_;

    /* --- Candidates --- */
    bool Active() const override { return candActive_; }

    /* The whole-word lookup the dictionary tables are keyed by; opening
     * may not flush anything (fcitx5's openCandidates rule:
     * hunmin_ic_flush would commit the anchor out from under the list). */
    void Open(ITfContext *pic) override {
        bool fallback = false;
        int n = ListCount(&fallback);
        CountToState(n, fallback);
        if (!candActive_)
            return;
        Update(pic);
    }

    void Close() override {
        candActive_ = false;
        candCount_ = 0;
        candCursor_ = 0;
        candTop_ = 0;
        candSource_.clear();
        candWnd_.Hide();
    }

    /* The candidate-window key (issue #67): the configured binding
     * (default the bare physical Hanja key) toggles the window. */
    void Toggle(ITfContext *pic, unsigned long vk, BOOL *pfEat) override {
        if (candActive_) {
            Close();
        } else {
            Open(pic);
        }
        hunmin_logger.log(" hanja key vk=%lu -> %s (src=%s count=%d)\n", vk, candActive_ ? "open" : "closed", candSource_.c_str(), candCount_);
        if (pfEat)
            *pfEat = candActive_ ? TRUE : FALSE;
    }

    /* The open window's list keys (issue #67): Enter commits the cursor,
     * Esc closes, arrows/PageUp/PageDown move, digits 1..0 pick. A digit
     * past the end of the count closes the window and falls through like
     * any other unhandled key - an invisible window must never swallow a
     * keystroke. Returns true when the key was ours. */
    bool ListKey(ITfContext *pic, unsigned long vk, BOOL *pfEat) override {
        bool handled = false;
        bool submit = false;
        int submitIdx = candCursor_;
        switch (vk) {
        case VK_RETURN:
            submit = true;
            handled = true;
            break;
        case VK_ESCAPE:
            Close();
            handled = true;
            break;
        case VK_UP:
        case VK_LEFT:
            if (candCursor_ > 0) {
                --candCursor_;
                PageFollowCursor();
                Update(pic);
            }
            handled = true;
            break;
        case VK_DOWN:
        case VK_RIGHT:
            if (candCursor_ < candCount_ - 1) {
                ++candCursor_;
                PageFollowCursor();
                Update(pic);
            }
            handled = true;
            break;
        case VK_PRIOR:
            PageStep(-1);
            Update(pic);
            handled = true;
            break;
        case VK_NEXT:
            PageStep(+1);
            Update(pic);
            handled = true;
            break;
        default:
            /* Digit select: 1..9 pick the page-relative candidates 1..9,
             * 0 picks the tenth, mirroring fcitx5's label scheme. */
            if (vk >= '0' && vk <= '9') {
                int pos = (vk == '0') ? 9 : (int)(vk - '1');
                int idx = candTop_ + pos;
                if (pos >= 0 && idx < candCount_) {
                    submitIdx = idx;
                    submit = true;
                    handled = true;
                }
            }
            break;
        }
        if (submit)
            Commit(pic, submitIdx);
        if (!handled) {
            /* Anything else: close and fall through to the normal path, so
             * typing continues where the user left off instead of being
             * swallowed by an invisible window (fcitx5: the default rule).
             * The buffered word is untouched by the close itself. */
            Close();
            return false;
        }
        if (pfEat)
            *pfEat = TRUE;
        hunmin_logger.log(" candidate key vk=%lu cursor=%d count=%d\n", vk, candCursor_, candCount_);
        return true;
    }

    bool EatsListKey(unsigned long vk) const override { return hunmin_tsf_candkey_eats_test(vk); }

    void Observe(HunminTsfSnapshot *out) override {
        if (!out)
            return;
        out->candActive = candActive_;
        out->candCount = candCount_;
        out->candCursor = candCursor_;
        out->candTop = candTop_;
        out->candFallback = candFallback_;
        out->candSource = candSource_;
    }

    /* The candidate-count decision BOTH phases share (test and down are
     * one decision): the whole buffered word is looked up first (the
     * anchor the dictionary tables are keyed by), and when that yields 0
     * while the word has more than one syllable, the LAST syllable is
     * looked up instead (issue #100). The returned pair is (count,
     * fallback): count 0 means the key belongs to the client in both
     * phases; a fallback count > 0 opens a syllable-anchored list. */
    int ListCount(bool *fallback) override {
        std::string word = backend_->ActiveDelivery()->CandidateAnchor();
        if (fallback)
            *fallback = false;
        hunmin_ctx *ctx = backend_->EngineContext();
        if (ctx && !word.empty()) {
            int n = hunmin_suggest_count(ctx, word.c_str());
            if (n > 0)
                return n;
            std::string last = LastSyllableOf(word);
            if (last != word) { /* more than one character => more than one syllable */
                n = hunmin_suggest_count(ctx, last.c_str());
                if (n > 0) {
                    if (fallback)
                        *fallback = true;
                    return n;
                }
            }
        }
        return 0;
    }

private:
    /* Recompute the list state from a (count, fallback) lookup answer. */
    void CountToState(int n, bool fallback) {
        candFallback_ = fallback;
        if (fallback) {
            candSource_ = LastSyllableOf(backend_->ActiveDelivery()->CandidateAnchor());
        } else {
            candSource_ = backend_->ActiveDelivery()->CandidateAnchor();
        }
        candCount_ = n;
        candActive_ = candCount_ > 0;
        candCursor_ = 0;
        candTop_ = 0;
    }

    void PageFollowCursor() { candTop_ = (candCursor_ / kMaxCandidatePageSize) * kMaxCandidatePageSize; }

    void PageStep(int dir) {
        int lastTop = candCount_ > 0 ? ((candCount_ - 1) / kMaxCandidatePageSize) * kMaxCandidatePageSize : 0;
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
            candCursor_ = candTop_ + kMaxCandidatePageSize - 1 < candCount_ ? candTop_ + kMaxCandidatePageSize - 1 : candCount_ - 1;
    }

    /* fcitx5 semantics (hunminadapter.cpp:160-176): choosing a candidate drops the
     * composition it was anchored to, then commits the Hanja in its place -
     * no surrounding-text surgery anywhere. The buffered Hangul has not
     * been typed into the document yet, so nothing needs removing: the
     * buffer itself is replaced by the chosen text, which is then inserted
     * through the normal boundary path. */
    void Commit(ITfContext *pic, int idx) {
        hunmin_ctx *ctx = backend_->EngineContext();
        if (idx < 0 || idx >= candCount_ || !ctx)
            return;
        /* Copy first: the pointer is valid only until the next
         * hunmin_suggest_* call on ctx. */
        std::string hanja(hunmin_suggest_text(ctx, idx));
        if (hanja.empty())
            return;
        /* Capture the word anchor BEFORE the commit: on a last-syllable
         * fallback list (issue #100) the buffered word's own state is what
         * the replacement is computed from, and the commit resets the
         * in-flight preedit. The mode seam decides how (and whether) the
         * replacement text is delivered. */
        std::string replacement = hanja;
        if (candFallback_) {
            std::string word = backend_->ActiveDelivery()->CandidateAnchor();
            std::string last = LastSyllableOf(word);
            /* Only last syllable replaced: the word prefix stays Hangul. */
            replacement = word.substr(0, word.size() - last.size()) + hanja;
        }
        std::wstring rep = HunminUtf8ToWide(replacement.c_str());
        backend_->ActiveDelivery()->CandidateCommit(pic, rep);
        Close();
    }

    void Update(ITfContext *pic) {
        if (!candActive_ || !pic || !backend_->Layer()) {
            candWnd_.Hide();
            return;
        }
        RECT rc;
        bool haveCaret = false;
        GUITHREADINFO gti;
        if (!backend_->CaretRect(pic, &rc, &haveCaret, &gti)) {
            /* No metrics, no caret: same hide path the overlay takes. The
             * candidate state stays open - a host that reports no window
             * geometry today may report it after the next focus, and the
             * next Hanja/arrow key redraws. */
            hunmin_logger.log("cand: no metrics -> window hidden (active=%d haveCaret=%d)\n", candActive_ ? 1 : 0, haveCaret ? 1 : 0);
            candWnd_.Hide();
            return;
        }
        HWND fontHost = haveCaret ? (gti.hwndFocus ? gti.hwndFocus : gti.hwndCaret) : nullptr;
        std::vector<std::wstring> lines;
        int end = candCount_ < candTop_ + kMaxCandidatePageSize ? candCount_ : candTop_ + kMaxCandidatePageSize;
        hunmin_ctx *ctx = backend_->EngineContext();
        for (int i = candTop_; i < end; ++i) {
            std::wstring text(HunminUtf8ToWide(hunmin_suggest_text(ctx, i)));
            wchar_t label = (wchar_t)(((i - candTop_) == 9) ? '0' : ('1' + (i - candTop_)));
            std::wstring line = std::wstring(i == candCursor_ ? L">> " : L"   ") + label + L". " + text;
            lines.push_back(line);
        }
        candWnd_.Show(rc, fontHost, lines);
    }

    CandBackend *backend_;
};

Candidates *hunmin_tsf_hanja_candidates_new(CandBackend &backend) { return new HunminCandidates(backend); }
