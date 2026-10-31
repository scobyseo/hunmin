/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * buffered_preedit.c - the shared buffered/syllable preedit object, one level above
 * hunmin_ic (issue #128, docs/FRONTEND-EQUIVALENCE.md §5).
 *
 * Why it exists: TSF, fcitx5 and Android each grew their own word buffer and
 * their own answer to "when is a word finished" - TSF drained its wordbuf_ on
 * a declined key, an edit key, a chord and a mode toggle; fcitx5 committed
 * per syllable and flushed at a decline; Android mirrored TSF's wordbuf in
 * Kotlin. Three copies of one rule drift, and the drift is visible as a key
 * that composes on one frontend and passes through on another. So the rule is
 * here, once, and a frontend only maps a physical key to (base, shift) and
 * renders what it is handed.
 *
 * What it does NOT do: it does not compose. Every syllable, tier, Caps Lock
 * and 전처리수식 decision is still the engine's. This object decides only when
 * the engine's destructive commit read is drained, and how much of it is
 * delivered for one key.
 *
 * Both commit modes are the same text, by construction: buffered mode appends
 * the same strings syllable mode would have delivered and hands them over at
 * the boundary instead, in order. A difference between the two would be a bug
 * in this file, which is what the tier-A tests assert.
 */
#include "hunmin_internal.h"

#include <stdlib.h>
#include <string.h>

struct hunmin_buffered_preedit {
    hunmin_ctx *ctx; /* borrowed: the caller owns the context */
    char *hangul_id; /* owned copies: a frontend's ids may be freed */
    char *latin_id; /* NULL when this frontend has one language only */
    int commit_policy; /* HUNMIN_COMMIT_* */
    bool hangul_mode;
    hunmin_ic *ic;
    char *word; /* buffered word (UTF-8), only used in BUFFERED mode */
    size_t wordlen, wordcap;
    char *commit_out; /* returned strings, owned, valid until the next call */
    size_t commitcap;
    char *preedit_out;
    size_t preeditcap;
};

/* ---------------------------------------------------------------- buffers */

static bool reserve(char **buf, size_t *cap, size_t need) {
    if (need <= *cap) return true;
    size_t n = *cap ? *cap : 64;
    while (n < need) n *= 2;
    char *p = realloc(*buf, n);
    if (!p) return false;
    *buf = p;
    *cap = n;
    return true;
}

static void word_clear(hunmin_buffered_preedit *l) {
    l->wordlen = 0;
    if (l->word) l->word[0] = 0;
}

/* Append to the buffered word. An allocation failure drops the text rather
 * than half-inserting it; the word is a display/commit buffer, and a truncated
 * word would commit a wrong syllable. */
static void word_append(hunmin_buffered_preedit *l, const char *s) {
    if (!s || !*s) return;
    size_t n = strlen(s);
    if (!reserve(&l->word, &l->wordcap, l->wordlen + n + 1)) return;
    memcpy(l->word + l->wordlen, s, n + 1);
    l->wordlen += n;
}

static const char *set_commit(hunmin_buffered_preedit *l, const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    if (!reserve(&l->commit_out, &l->commitcap, n + 1)) return "";
    memcpy(l->commit_out, s, n + 1);
    return l->commit_out;
}

/* What the frontend shows. In buffered mode that is the finished word plus the
 * in-flight syllable, which is exactly what the TSF adapter rendered before
 * (wordbuf_ + engine preedit); in syllable mode the finished syllables are
 * already in the client, so only the in-flight one is left to show. */
static const char *build_preedit(hunmin_buffered_preedit *l) {
    const char *pre = hunmin_ic_preedit(l->ic);
    if (!pre) pre = "";
    const char *word = (l->commit_policy == HUNMIN_COMMIT_AT_BOUNDARY && l->word) ? l->word : "";
    size_t wn = strlen(word), pn = strlen(pre);
    if (!reserve(&l->preedit_out, &l->preeditcap, wn + pn + 1)) return "";
    memcpy(l->preedit_out, word, wn);
    memcpy(l->preedit_out + wn, pre, pn + 1);
    return l->preedit_out;
}

/* The engine's commit read is destructive: whatever it produced since the last
 * read is delivered now, or it is lost. Both modes therefore read it on every
 * consumed key; they differ only in what they do with the text. */
static const char *drain_engine_commit(hunmin_buffered_preedit *l) {
    const char *c = hunmin_ic_commit(l->ic);
    return c ? c : "";
}

/* -------------------------------------------------------------- lifecycle */

static char *dup_or_null(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void attach_ic(hunmin_buffered_preedit *l, const char *layout_id) {
    if (l->ic) {
        hunmin_ic_free(l->ic);
        l->ic = NULL;
    }
    if (layout_id) l->ic = hunmin_ic_new(l->ctx, layout_id);
}

hunmin_buffered_preedit *hunmin_buffered_preedit_new(hunmin_ctx *ctx, const char *hangul_layout_id, const char *latin_layout_id, int commit_policy) {
    if (!ctx || !hangul_layout_id) return NULL;
    if (commit_policy != HUNMIN_COMMIT_AT_BOUNDARY && commit_policy != HUNMIN_COMMIT_PER_SYLLABLE) return NULL;
    hunmin_buffered_preedit *l = calloc(1, sizeof *l);
    if (!l) return NULL;
    l->ctx = ctx;
    l->hangul_id = dup_or_null(hangul_layout_id);
    l->latin_id = dup_or_null(latin_layout_id);
    l->commit_policy = commit_policy;
    l->hangul_mode = true;
    if (!l->hangul_id) {
        hunmin_buffered_preedit_free(l);
        return NULL;
    }
    /* A layout the engine cannot instantiate is a caller error, not a construction
     * failure: report it here instead of handing back an object whose first key
     * silently passes through. */
    attach_ic(l, l->hangul_id);
    if (!l->ic) {
        hunmin_buffered_preedit_free(l);
        return NULL;
    }
    return l;
}

void hunmin_buffered_preedit_free(hunmin_buffered_preedit *l) {
    if (!l) return;
    if (l->ic) hunmin_ic_free(l->ic);
    free(l->hangul_id);
    free(l->latin_id);
    free(l->word);
    free(l->commit_out);
    free(l->preedit_out);
    free(l);
}

/* ------------------------------------------------------------------- keys */

/* The boundary. One place answers "the word is finished": the engine's own
 * flush finalises the in-flight syllable, and the mode decides whether the
 * client receives that syllable or the whole word. */
static const char *boundary(hunmin_buffered_preedit *l) {
    const char *flushed = hunmin_ic_flush(l->ic);
    if (!flushed) flushed = "";
    if (l->commit_policy == HUNMIN_COMMIT_PER_SYLLABLE) return set_commit(l, flushed);
    word_append(l, flushed);
    const char *out = set_commit(l, l->word ? l->word : "");
    word_clear(l);
    return out;
}

int hunmin_buffered_preedit_process_key(hunmin_buffered_preedit *l, uint32_t base, uint32_t shift, hunmin_key_result *out) {
    if (out) {
        out->consumed = 0;
        out->commit = "";
        out->preedit = "";
    }
    if (!l || !l->ic) return 0;

    const bool consumed = hunmin_ic_process_key(l->ic, base, shift);
    if (consumed) {
        const char *c = drain_engine_commit(l);
        if (l->commit_policy == HUNMIN_COMMIT_PER_SYLLABLE) {
            set_commit(l, c); /* deliver the syllable now */
        } else {
            word_append(l, c); /* hold it until the boundary */
            set_commit(l, "");
        }
    } else {
        /* A declined key IS the boundary: the engine finalises the syllable it
         * was holding, and the frontend passes the key to its client. Every
         * frontend used to spell this out itself. */
        (void)boundary(l);
    }

    if (out) {
        out->consumed = consumed ? 1 : 0;
        out->commit = l->commit_out ? l->commit_out : "";
        out->preedit = build_preedit(l);
    }
    return consumed ? 1 : 0;
}

int hunmin_buffered_preedit_peek_key(const hunmin_buffered_preedit *l, uint32_t base, uint32_t shift) {
    if (!l || !l->ic) return 0;
    /* Same rule as hunmin_ic_peek_key: the state is all inside the ic, so a
     * by-value copy answers without consuming the key. ext8 is the original
     * ic's allocation and is never touched by process_key; clear it in the
     * copy so a future path cannot free or realloc it twice. */
    hunmin_ic copy = *l->ic;
    copy.ext8 = NULL;
    copy.extcap = 0;
    return hunmin_ic_process_key(&copy, base, shift) ? 1 : 0;
}

const char *hunmin_buffered_preedit_commit_word(hunmin_buffered_preedit *l) {
    if (!l || !l->ic) return "";
    return boundary(l);
}

const char *hunmin_buffered_preedit_display(hunmin_buffered_preedit *l) {
    if (!l || !l->ic) return "";
    return build_preedit(l);
}

const char *hunmin_buffered_preedit_delivered(const hunmin_buffered_preedit *l) {
    if (!l || !l->commit_out) return "";
    return l->commit_out;
}

void hunmin_buffered_preedit_reset(hunmin_buffered_preedit *l) {
    if (!l || !l->ic) return;
    hunmin_ic_reset(l->ic);
    word_clear(l);
    if (l->commit_out) l->commit_out[0] = 0;
    if (l->preedit_out) l->preedit_out[0] = 0;
}

bool hunmin_buffered_preedit_set_option(hunmin_buffered_preedit *l, const char *name, int value) {
    if (!l || !l->ic) return false;
    return hunmin_ic_set_option(l->ic, name, value);
}

/* ---------------------------------------------------------- external text */

const char *hunmin_buffered_preedit_external_text(hunmin_buffered_preedit *l, const char *text) {
    if (!l || !l->ic) return "";
    /* The word first: it belongs to the keyboard and was typed before the
     * recogniser produced anything. boundary() also finalises the in-flight
     * syllable into it, so the engine is left with nothing composing and the
     * passthrough call cannot compose a second syllable on top.
     *
     * The word lives in commit_out, which reserving may MOVE, so it is copied
     * aside before anything else allocates -- reading it afterwards would be a
     * use-after-realloc. */
    const char *word = boundary(l);
    const size_t wn = word ? strlen(word) : 0;
    char *held = NULL;
    if (wn) {
        held = malloc(wn + 1);
        if (!held) return word;
        memcpy(held, word, wn + 1);
    }
    const char *rest = hunmin_ic_commit_text(l->ic, text);
    const size_t rn = rest ? strlen(rest) : 0;
    if (!reserve(&l->commit_out, &l->commitcap, wn + rn + 1)) {
        free(held);
        return "";
    }
    if (wn) memcpy(l->commit_out, held, wn);
    if (rn) memcpy(l->commit_out + wn, rest, rn);
    l->commit_out[wn + rn] = 0;
    free(held);
    return l->commit_out;
}

/* ----------------------------------------------------------- the key rule */

/* The keys an open candidate list consumes, wherever it is drawn: navigation,
 * page, commit and cancel. Shared because the MEANING is shared -- each
 * frontend only says whether its list is open. */
static bool candidate_list_key(hunmin_key_id id) {
    switch (id) {
    case HUNMIN_KEY_UP:
    case HUNMIN_KEY_DOWN:
    case HUNMIN_KEY_LEFT:
    case HUNMIN_KEY_RIGHT:
    case HUNMIN_KEY_PAGE_UP:
    case HUNMIN_KEY_PAGE_DOWN:
    case HUNMIN_KEY_ENTER:
    case HUNMIN_KEY_ESCAPE:
        return true;
    default:
        return false;
    }
}

hunmin_key_action hunmin_buffered_preedit_classify(const hunmin_buffered_preedit *l, const hunmin_key_event *ev) {
    if (!l || !l->ic || !ev) return HUNMIN_KEY_BOUNDARY_PASS;
    /* The language action first: the Han/Eng key is the one key that is never
     * composition input, whatever else is held (a repeat guard is a platform
     * fact and stays in the adapter). */
    if (ev->language_key) {
        /* With a chord modifier beside it, the press is the COMPOUND one and
         * carries a special function instead of switching languages (ruled
         * 2026-10-31, docs/SPEC-MODES.md §E5). SHIFT is not a chord bit: it is
         * a layout row, and a shift-only press is still the toggle. */
        if (ev->mods & HUNMIN_MOD_CHORD) return HUNMIN_KEY_LANGUAGE_CHORD;
        return HUNMIN_KEY_LANGUAGE_TOGGLE;
    }
    /* Then the frontend's open list: while it is open, these keys are the
     * list's. A different key closes nothing here -- the frontend closes the
     * window and re-classifies, which is its presentation decision. */
    if (ev->candidates_open && candidate_list_key(ev->id)) return HUNMIN_KEY_CANDIDATE;
    /* A chord belongs to the application: Ctrl+A is not composition input, and
     * the SHIFT bit travelling beside base is a layout row, not a chord. */
    if (ev->mods & HUNMIN_MOD_CHORD) return HUNMIN_KEY_BOUNDARY_PASS;
    /* A named key with no list open is a boundary: Enter is a paragraph break,
     * an arrow moves the caret -- none of them is composition input. */
    if (ev->id != HUNMIN_KEY_TEXT) return HUNMIN_KEY_BOUNDARY_PASS;
    /* An unmapped character key is a boundary too: the word is delivered and
     * the key passes, which is how space and punctuation reach the client. */
    if (ev->base == 0) return HUNMIN_KEY_BOUNDARY_PASS;
    return HUNMIN_KEY_FEED;
}

const char *hunmin_buffered_preedit_take_word(hunmin_buffered_preedit *l) {
    if (!l || !l->ic) return "";
    const char *out = set_commit(l, l->word ? l->word : "");
    word_clear(l);
    return out;
}

const char *hunmin_buffered_preedit_composing(hunmin_buffered_preedit *l) {
    if (!l || !l->ic) return "";
    return hunmin_ic_preedit(l->ic);
}

/* ------------------------------------------------------------ Han/Eng mode
 *
 * docs/FRONTEND-EQUIVALENCE.md §4: Android delivers the Han/Eng key as a Kana
 * VK and the other frontends must reach the same abstract action. The action
 * is here, so a frontend maps its physical key to it and the raw platform code
 * never reaches the engine.
 *
 * The switch IS a boundary: whatever was composing belongs to the language that
 * was active and is committed before the other layout takes over. The engine is
 * re-created because the two languages are two layouts, which is what the
 * engine's own model says. */
int hunmin_buffered_preedit_toggle_language(hunmin_buffered_preedit *l, hunmin_key_result *out) {
    if (out) {
        out->consumed = 0;
        out->commit = "";
        out->preedit = "";
    }
    if (!l || !l->ic) return 0;
    const char *commit = boundary(l);
    const char *next = l->hangul_mode ? l->latin_id : l->hangul_id;
    if (!next) {
        /* One-language frontend: the key is not ours to perform. The commit
         * above still stands - the boundary happened. */
        if (out) {
            out->commit = commit;
            out->preedit = build_preedit(l);
        }
        return 0;
    }
    l->hangul_mode = !l->hangul_mode;
    attach_ic(l, next);
    word_clear(l);
    if (out) {
        out->consumed = 1;
        out->commit = commit;
        out->preedit = build_preedit(l);
    }
    return 1;
}

int hunmin_buffered_preedit_language(const hunmin_buffered_preedit *l) { return (l && l->hangul_mode) ? HUNMIN_LANGUAGE_HANGUL : HUNMIN_LANGUAGE_LATIN; }
int hunmin_buffered_preedit_commit_policy(const hunmin_buffered_preedit *l) { return l ? l->commit_policy : -1; }
const char *hunmin_buffered_preedit_layout_id(const hunmin_buffered_preedit *l) {
    if (!l) return NULL;
    return l->hangul_mode ? l->hangul_id : l->latin_id;
}
