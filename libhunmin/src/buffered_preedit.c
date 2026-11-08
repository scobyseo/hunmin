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
#include <stdio.h>
#include <string.h>

/* R9's threshold (docs/SPEC-MODES.md §R9, ruled 2026-11-01): three KEY INPUTS, the
 * unit the maintainer named the same day -- not characters, not syllables. Lives at
 * file scope because two readers share it: judge_boundary() decides with it, and
 * judged_display() may not show a judgement the guard would refuse. */
#define JUDGE_MIN_KEYS 3

struct hunmin_buffered_preedit {
    hunmin_ctx *ctx; /* borrowed: the caller owns the context */
    char *hangul_id; /* owned copies: a frontend's ids may be freed */
    char *latin_id; /* NULL when this frontend has one language only */
    int commit_policy; /* HUNMIN_COMMIT_* */
    bool hangul_mode;
    hunmin_ic *ic;
    char *word; /* buffered word (UTF-8), only used in BUFFERED mode */
    size_t wordlen, wordcap;
    size_t cap; /* WORD_CAP by default; 0 = no cap (option "word_cap", R4-정정) */
    char *commit_out; /* returned strings, owned, valid until the next call */
    size_t commitcap;
    char *preedit_out;
    size_t preeditcap;
    /* E5 function->chord mapping + host (docs/SPEC-MODES.md §E5;
     * DESIGN-CORE.md#e5-special-function-chords). Zero = not bound. */
    unsigned special_mods[HUNMIN_SPECIAL_LIMIT];
    hunmin_special_host special_host;
    bool special_host_set;
    hm_wordset *english; /* the modes judgement's data (R8-정정-2); NULL until asked for */
    /* R8's judgement, once, at the boundary (ruled 2026-11-01, issue #164: the
     * ENGINE judges, the frontend performs the switch). -1 = no judgement for
     * the frontend to act on; HUNMIN_LANGUAGE_* = the mode it must adopt. Read
     * once by hunmin_buffered_preedit_take_judgement(), so a mode cannot be
     * applied twice, and replaced at every boundary, so a finished word is
     * never acted on after the next one starts. */
    int judged;
    /* R8's english_valid reads the Latin text the buffer WOULD have produced,
     * and a Hangul layout cannot produce Latin (measured: the 12 Hangul layouts
     * emit no Latin letter for any printable key -- the text is destroyed, not
     * transliterated). So the reading is the same keys replayed through the frontend's
     * Latin layout, which is SPEC-MODES R2's shadow mechanism; the keys live
     * here because this object is what sees them. `keys` is the word in flight
     * and `latin_out` the reading of the word that last finished. */
    uint32_t *keys; /* interleaved (base, shift) of the word in flight */
    size_t keylen, keycap; /* in slots, not bytes */
    /* E6 (docs/SPEC-MODES.md §E6, ruled 2026-11-01): WHEN the judgement is shown.
     * 1 = per key (the default): the preedit answers in the judged language while
     * the word is still being typed, which is the flicker the item describes and
     * the behaviour the maintainer chose as the default ("default는 B로").
     * 0 = at the boundary only: the word is shown in the current mode and answers
     * once, at the delimiter. The delivered text is identical either way -- only
     * when it is shown differs -- and that equality is what the tier-A case pins. */
    int judge_timing;
    char *judged_out; /* the per-key display's reading, owned */
    size_t judgedcap;
    /* E1's half (docs/SPEC-MODES.md §R8, ruled 2026-11-01: "E1도 당연히 지원해야지"):
     * the Hangul reading of a word typed in Latin mode, which is what the symmetric
     * line of R8's formula judges. Same shape as latin_out and for the same reason --
     * a Latin layout cannot produce Hangul, so the reading is the keys replayed
     * through the frontend's Hangul layout. */
    char *hangul_out;
    size_t hangulcap;
    char *latin_out; /* the finished word's reading; "" while a word composes */
    size_t latincap;
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

static void keys_clear(hunmin_buffered_preedit *l) { l->keylen = 0; }

static bool keys_reserve(hunmin_buffered_preedit *l, size_t want) {
    size_t cap = l->keycap ? l->keycap : 32;
    while (cap < want) cap *= 2;
    if (cap == l->keycap) return true;
    uint32_t *p = realloc(l->keys, cap * sizeof *p);
    if (!p) return false;
    l->keys = p;
    l->keycap = cap;
    return true;
}

/* One consumed key of the word in flight. A key the engine DECLINES ends the
 * word and is not recorded: the reading is what the same keys produce on the
 * Latin layout, and the space that ended the word belongs to neither language. */
static void keys_push(hunmin_buffered_preedit *l, uint32_t base, uint32_t shift) {
    if (!keys_reserve(l, l->keylen + 2)) return;
    l->keys[l->keylen++] = base;
    l->keys[l->keylen++] = shift ? 1u : 0u;
}

/* The reading of the word that just finished, taken AT the boundary because
 * that is the one moment its keys are complete and the judgement runs. The key
 * log is cleared straight after, so a finished word cannot be judged twice.
 *
 * The commit is read after EVERY key, not once at the end: hunmin_ic_commit()
 * is destructive, and a single read after the loop keeps only the last key's
 * commit (measured in frontends/fcitx5/src/language_detect.cpp -- replaying
 * "kbs" returned "s"). The preedit is non-destructive, so one read at the end
 * catches whatever is still composing. */
static const char *replay_reading(const hunmin_buffered_preedit *l, const char *layout_id, char **buf, size_t *cap) {
    if (!reserve(buf, cap, 1)) return "";
    (*buf)[0] = 0;
    if (!layout_id || !l->ctx || !l->keylen) return *buf;
    hunmin_ic *shadow = hunmin_ic_new(l->ctx, layout_id);
    if (!shadow) return *buf;
    char *out = *buf;
    size_t n = 0;
    for (size_t i = 0; i + 1 < l->keylen; i += 2) {
        hunmin_ic_process_key(shadow, l->keys[i], l->keys[i + 1]);
        const char *c = hunmin_ic_commit(shadow);
        size_t cn = c ? strlen(c) : 0;
        if (cn && reserve(&out, cap, n + cn + 1)) {
            memcpy(out + n, c, cn + 1);
            n += cn;
        }
    }
    const char *pre = hunmin_ic_preedit(shadow);
    size_t pn = pre ? strlen(pre) : 0;
    if (pn && reserve(&out, cap, n + pn + 1)) {
        memcpy(out + n, pre, pn + 1);
        n += pn;
    }
    hunmin_ic_free(shadow);
    *buf = out; /* the appends may have moved it */
    return out;
}

static void latin_reading(hunmin_buffered_preedit *l) {
    replay_reading(l, l->latin_id, &l->latin_out, &l->latincap);
}

static void hangul_reading(hunmin_buffered_preedit *l) {
    replay_reading(l, l->hangul_id, &l->hangul_out, &l->hangulcap);
}

/* R8-정정-3: the shape rule is the same; the jamo sets widen on a layout that can
 * emit a jamo outside the modern 19/21/28. The answer is derived at load time
 * from that layout's own keys and combination table (schema_loader.c
 * compute_capability), never declared, so a new layout cannot forget it.
 *
 * The text judged is always the Hangul reading, which `hangul_id` produces -- in
 * Latin mode too, where the active ic is the Latin layout. So the rule is picked
 * from `hangul_id`, not from `l->ic->L`: asking the Latin layout would answer
 * "modern" for every word and mark a valid 옛한글 buffer invalid. The answer is
 * asked through `hunmin_ctx_layout_old_hangul`, the same seam a frontend uses. */
static int hangul_valid_for(const hunmin_buffered_preedit *l, const char *text) {
    bool old = false;
    if (l->ctx && l->hangul_id) {
        for (int i = 0; i < l->ctx->n; i++) {
            if (!strcmp(l->ctx->layouts[i].id, l->hangul_id)) {
                old = hunmin_ctx_layout_old_hangul(l->ctx, i);
                break;
            }
        }
    }
    return old ? hunmin_hangul_valid_old(text) : hunmin_hangul_valid(text);
}

static const char *set_commit(hunmin_buffered_preedit *l, const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    if (!reserve(&l->commit_out, &l->commitcap, n + 1)) return "";
    memcpy(l->commit_out, s, n + 1);
    return l->commit_out;
}

/* ------------------------------------------------------- the cap (R4-정정) */

/* set_commit for a LENGTH rather than a NUL-terminated string: the half being
 * committed is a prefix of the word buffer, which is not terminated at that
 * offset. NULL means the copy could not be made, so the caller can leave the word
 * intact instead of dropping text it had already decided to deliver. */
static const char *set_commit_n(hunmin_buffered_preedit *l, const char *s, size_t n) {
    if (!reserve(&l->commit_out, &l->commitcap, n + 1)) return NULL;
    memcpy(l->commit_out, s, n);
    l->commit_out[n] = 0;
    return l->commit_out;
}

/* R4-정정 cap overflow. Multi-place rationale:
 * docs/DESIGN-CORE.md#r4-정정-cap-word_flush_half. Returns commit output ("" if
 * under cap); on OOM leaves the word whole. */
static const char *word_flush_half(hunmin_buffered_preedit *l) {
    set_commit(l, "");
    /* The cap bounds what the frontend RENDERS, which is the word plus the
     * in-flight syllable, so the in-flight part counts against it. Comparing only
     * against the word would let the preedit past the cap by one syllable. */
    const char *inflight_text = hunmin_ic_preedit(l->ic);
    const size_t inflight = inflight_text ? strlen(inflight_text) : 0;
    if (!l->cap || l->wordlen + inflight <= l->cap) return l->commit_out;
    size_t half = l->wordlen / 2;
    while (half > 0 && ((unsigned char)l->word[half] & 0xc0) == 0x80) half--;
    if (!half) return l->commit_out;
    const char *out = set_commit_n(l, l->word, half);
    if (!out) return l->commit_out; /* out of memory: leave the word whole */
    memmove(l->word, l->word + half, l->wordlen - half);
    l->wordlen -= half;
    l->word[l->wordlen] = 0;
    keys_clear(l);
    return out;
}

/* E6 per-key judged display (SPEC-MODES E6). Multi-place rationale:
 * docs/DESIGN-CORE.md#e6-judged-display-judge_timing. Display only; no
 * delivery/switch/recorded judgement (R10). */
static const char *judged_display(hunmin_buffered_preedit *l, const char *shown_text) {
    if (l->judge_timing != 1) return NULL;
    if (!l->ic || !l->ic->autoswitch) return NULL;
    if (!l->latin_id) return NULL; /* one-language layer: no second reading */
    if (l->keylen / 2 < JUDGE_MIN_KEYS) return NULL; /* R9's guard, both directions */
    if (!hm_wordset_ready(l->english)) return NULL;

    if (l->hangul_mode) {
        /* The word on screen is Hangul; the other half is its Latin reading. */
        if (!l->ic->L || !l->ic->L->composes_hangul) return NULL;
        const char *reading = replay_reading(l, l->latin_id, &l->judged_out, &l->judgedcap);
        if (!reading || !*reading) return NULL;
        if (!hm_wordset_has(l->english, reading)) return NULL;
        if (hangul_valid_for(l, shown_text)) return NULL; /* both valid: keep the mode */
        return reading;
    }
    /* E1: the word on screen is Latin; the other half is its Hangul reading, and
     * the judgement fires when that reads as syllables and the Latin does not read
     * as a word. */
    const char *reading = replay_reading(l, l->hangul_id, &l->judged_out, &l->judgedcap);
    if (!reading || !*reading) return NULL;
    if (hm_wordset_has(l->english, shown_text)) return NULL; /* english_valid */
    if (!hangul_valid_for(l, reading)) return NULL;
    return reading;
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
    const char *judged = judged_display(l, l->preedit_out);
    return judged ? judged : l->preedit_out;
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
    /* The autoswitch state is the person's preference, not the layout's, so the
     * ic re-created here inherits it: without this, a settings UI writes ON and
     * the very next 한/영 press (toggle_language) silently drops it back to the
     * B3 default of OFF. The composition state does NOT travel -- that is the
     * boundary's job and it has already committed. */
    const int autoswitch = l->ic ? l->ic->autoswitch : 0;
    if (l->ic) {
        hunmin_ic_free(l->ic);
        l->ic = NULL;
    }
    if (layout_id) l->ic = hunmin_ic_new(l->ctx, layout_id);
    if (l->ic) l->ic->autoswitch = autoswitch;
}

/* The ruled defaults (2026-10-31, Table A): the two functions that exist get a
 * chord the engine owns, so a frontend that seeds nothing still agrees with the
 * other two. A frontend overrides one with hunmin_buffered_preedit_special_bind()
 * from its store; the dialog reads whichever is live. */
static const unsigned kSpecialDefaultMods[HUNMIN_SPECIAL_LIMIT] = {
    /* NONE */ 0u,
    /* VOICE */ HUNMIN_MOD_ALT,
    /* AUTOSWITCH */ HUNMIN_MOD_CTRL,
};

hunmin_buffered_preedit *hunmin_buffered_preedit_new(hunmin_ctx *ctx, const char *hangul_layout_id, const char *latin_layout_id, int commit_policy) {
    if (!ctx || !hangul_layout_id) return NULL;
    if (commit_policy != HUNMIN_COMMIT_AT_BOUNDARY && commit_policy != HUNMIN_COMMIT_PER_SYLLABLE) return NULL;
    hunmin_buffered_preedit *l = calloc(1, sizeof *l);
    if (!l) return NULL;
    l->ctx = ctx;
    l->cap = WORD_CAP;
    l->judge_timing = 1; /* E6 ruled 2026-11-01: per key by default */
    l->hangul_id = dup_or_null(hangul_layout_id);
    l->latin_id = dup_or_null(latin_layout_id);
    l->commit_policy = commit_policy;
    l->hangul_mode = true;
    l->judged = -1;
    memcpy(l->special_mods, kSpecialDefaultMods, sizeof l->special_mods);
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
    if (l->english) hm_wordset_free(l->english);
    free(l->hangul_id);
    free(l->latin_id);
    free(l->word);
    free(l->commit_out);
    free(l->preedit_out);
    free(l->keys);
    free(l->latin_out);
    free(l->judged_out);
    free(l->hangul_out);
    free(l);
}

/* ------------------------------------------------------------------- keys */

/* R8 judgement (SPEC-MODES R8/R8-정정/E1, R3; REQ-MAP R8). Multi-place
 * rationale: docs/DESIGN-CORE.md#r8-judgement--judge_boundary-implementation
 * (formula/ownership: docs/DESIGN-MODES.md#1).
 * Returns replacement text to deliver, or NULL to deliver the Hangul word. */

static const char *judge_boundary(hunmin_buffered_preedit *l, size_t keys) {
    /* Identity/fail-closed gates (B3/R13-정정, R9 key guard, missing data). */
    if (!l->ic || !l->ic->autoswitch) return NULL;
    if (!l->latin_id) return NULL;
    if (keys < JUDGE_MIN_KEYS) return NULL;
    if (!hm_wordset_ready(l->english)) return NULL;

    if (l->hangul_mode) {
        if (!l->ic->L || !l->ic->L->composes_hangul) return NULL; /* R8-정정-3 */
        if (!l->latin_out || !*l->latin_out) return NULL;
        const bool english_valid = hm_wordset_has(l->english, l->latin_out);
        const bool hangul_valid = hangul_valid_for(l, l->word ? l->word : "");
        if (!english_valid || hangul_valid) return NULL;
        l->judged = HUNMIN_LANGUAGE_LATIN;
        return l->latin_out;
    }

    /* E1 symmetric line (SPEC-MODES R8). */
    const char *reading = l->hangul_out;
    if (!reading || !*reading) return NULL;
    const bool english_valid = hm_wordset_has(l->english, l->word ? l->word : "");
    const bool hangul_valid = hangul_valid_for(l, reading);
    if (english_valid || !hangul_valid) return NULL;
    l->judged = HUNMIN_LANGUAGE_HANGUL;
    return reading;
}

/* The boundary. One place answers "the word is finished": the engine's own
 * flush finalises the in-flight syllable, and the mode decides whether the
 * client receives that syllable or the whole word.
 *
 * `judge` says whether R8's judgement (#164) may run here. It is false for one
 * caller and only one -- the Han/Eng press -- because R13 requires the hook to
 * be processed BEFORE inference (docs/DESIGN-MODES.md §3: "pressing the
 * hangul/latin key while inference is on is the signal 'do not do this
 * automatically'", issue #170). The readings are still taken when judging is
 * off: they are what `hunmin_buffered_preedit_latin_text()` reports for the word
 * that finished, which is a separate contract from the decision. */
static const char *boundary(hunmin_buffered_preedit *l, bool judge) {
    const char *flushed = hunmin_ic_flush(l->ic);
    if (!flushed) flushed = "";
    /* Above the delivery split on purpose: the reading and the cleared key log
     * belong to BOTH modes. PER_SYLLABLE mode keeps no word buffer, so a hook
     * below the split would leave the reading of half the frontends unwritten. */
    latin_reading(l);
    /* E1's other half, and it must be taken HERE: keys_clear() below is what stops a
     * finished word being judged twice, so a reading replayed after it is empty
     * (measured — the symmetric direction answered "no switch" on every word until
     * this moved). Only one direction needs it, so only that one pays for it. */
    if (!l->hangul_mode) hangul_reading(l);
    const size_t keys = l->keylen / 2; /* slots are (base, shift) pairs */
    keys_clear(l);
    /* Every boundary replaces the previous judgement: a finished word must not
     * be acted on after the next one starts, and a boundary that judges nothing
     * (a chord, a mode toggle, an abandoned word) clears the answer. */
    l->judged = -1;
    if (l->commit_policy == HUNMIN_COMMIT_PER_SYLLABLE) return set_commit(l, flushed);
    word_append(l, flushed);
    /* Judged while the word is still whole. PER_SYLLABLE mode cannot reach this
     * line: by the boundary its earlier syllables are already in the client,
     * and R10 forbids reassembling them, so the judgement has nothing to judge. */
    const char *judged = judge ? judge_boundary(l, keys) : NULL;
    if (judged) {
        word_clear(l);
        return set_commit(l, judged);
    }
    /* Deliberately NOT capped here: the boundary is where the whole word goes out
     * as one commit and the preedit is cleared, so splitting it would turn one
     * word into two commits for no bound gained. The cap exists for the preedit a
     * frontend keeps re-rendering key by key (word_flush_half). */
    const char *out = set_commit(l, l->word ? l->word : "");
    word_clear(l);
    return out;
}

/* ---------------------------------------------------- the symbol delimiter */

/* R4, the sentences this predicate exists for:
 *
 *   "Composition buffer = the whole span from after the immediately preceding
 *    space character or symbol until the next space character or symbol
 *    appears." ... "A symbol is also a termination condition."
 *
 * A space reaches the layer as a DECLINED key, so `process_key` already ends the
 * word there. A symbol does not: `.` is a mapped key on every Latin layout, the
 * engine consumes it, and its per-key commit carries the in-flight syllable plus
 * the symbol (`ic.c` process_jamo's literal branch). Without this the word
 * buffer simply swallowed it -- `jgz` + `.` became the word `jgz.`, which is
 * neither an English word nor a Hangul reading, so the judgement abstained and
 * the switch never happened (measured on a device 2026-11-07, SPEC-MODES R4).
 *
 * So the word keeps the symbol for delivery, and the JUDGEMENT runs on the text
 * before it: `음` + `.`, not `음.` judged as a word.
 *
 * WHAT COUNTS AS A SYMBOL, and what deliberately does not:
 *
 *   symbol        punctuation, the mathematical/currency blocks, the circled
 *                 and tier forms a sebeolsik digit row reaches (`①`, `⑪`).
 *   not symbol    ASCII letters and digits: they are the Latin reading's own
 *                 material, and R6 keeps a DIGIT RUN inside the buffer, so a
 *                 digit must not terminate anything.
 *   not symbol    Hangul, syllables and jamo: a consumed Hangul key continues
 *                 the word, which is the whole point of the buffer.
 *   not symbol    space and the control characters, which never arrive here
 *                 consumed.
 *   not symbol    the Latin-1/Greek/Cyrillic letter blocks. This repo's layouts
 *                 emit ASCII Latin, so a letter from those blocks is a symbol
 *                 tier's output only by accident; erring towards "letter" is
 *                 the safe direction, because a wrong "symbol" would cut a word
 *                 in half.
 */
static bool cp_is_symbol(uint32_t c) {
    if (c < 0x80) {
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return false;
        if (c < 0x21) return false; /* space and every control character */
        return true;
    }
    if (c >= 0x0080 && c <= 0x04ff) return false; /* Latin-1 supplement through Cyrillic: text, not symbol */
    if (c >= 0x1100 && c <= 0x11ff) return false; /* Hangul jamo */
    if (c >= 0x3130 && c <= 0x318f) return false; /* Hangul compatibility jamo */
    if (c >= 0xac00 && c <= 0xd7a3) return false; /* Hangul syllables */
    if (c >= 0xa960 && c <= 0xa97f) return false; /* Hangul jamo extended-A */
    if (c >= 0xd7b0 && c <= 0xd7ff) return false; /* Hangul jamo extended-B */
    /* NUMBER FORMS are not symbols, and this is R6 speaking: a sebeolsik digit
     * key resolves at a symbol tier to `µ`/`①`/`⑪` (and their super/subscript
     * and Roman forms), and R6 requires the digit RUN to stay in the buffer. A
     * tier form that terminated the word would deliver a digit run early --
     * measured: LayerTest.DigitResolvedAtASymbolTierIsHeldToo. */
    if (c >= 0x2070 && c <= 0x209f) return false; /* super/subscripts */
    if (c >= 0x2150 && c <= 0x218f) return false; /* number forms (Roman, fractions) */
    if (c >= 0x2460 && c <= 0x24ff) return false; /* enclosed alphanumerics ①⑪ */
    if (c >= 0x2776 && c <= 0x2793) return false; /* dingbat circled numbers */
    if (c >= 0x3248 && c <= 0x325f) return false; /* circled numbers, CJK */
    if (c >= 0x32b1 && c <= 0x32bf) return false; /* circled numbers, CJK, continued */
    if (c >= 0x1f100 && c <= 0x1f1ff) return false; /* enclosed alphanumeric supplement */
    return true;
}

/* One UTF-8 code point. Returns its length in bytes, or 0 when malformed. */
static size_t cp_next(const unsigned char *p, uint32_t *out) {
    uint32_t c = p[0];
    int extra;
    uint32_t min;
    if (c < 0x80) { *out = c; return 1; }
    if ((c & 0xe0) == 0xc0) { extra = 1; c &= 0x1f; min = 0x80; }
    else if ((c & 0xf0) == 0xe0) { extra = 2; c &= 0x0f; min = 0x800; }
    else if ((c & 0xf8) == 0xf0) { extra = 3; c &= 0x07; min = 0x10000; }
    else return 0;
    for (int i = 1; i <= extra; i++) {
        if ((p[i] & 0xc0) != 0x80) return 0;
        c = (c << 6) | (uint32_t)(p[i] & 0x3f);
    }
    if (c < min || (c >= 0xd800 && c <= 0xdfff)) return 0;
    *out = c;
    return (size_t)extra + 1;
}

/* Bytes of the trailing run of symbols in `s`, 0 when it does not end with one.
 * Malformed text stops the scan: cutting a word on a byte we cannot read is the
 * failure this exists to avoid. */
static size_t trailing_symbol_bytes(const char *s) {
    if (!s || !*s) return 0;
    const unsigned char *p = (const unsigned char *)s;
    size_t run_start = strlen(s), i = 0;
    while (p[i]) {
        uint32_t cp;
        size_t n = cp_next(p + i, &cp);
        if (!n) return 0;
        if (!cp_is_symbol(cp)) run_start = i + n;
        i += n;
    }
    return i > run_start ? i - run_start : 0;
}

int hunmin_buffered_preedit_process_key(hunmin_buffered_preedit *l, uint32_t base, uint32_t shift, hunmin_key_result *out) {
    if (out) {
        out->consumed = 0;
        out->commit = "";
        out->preedit = "";
    }
    if (!l || !l->ic) return 0;

    /* The ic's composing text, copied BEFORE the key: it is invalidated by the
     * next hunmin_ic_* call, and the symbol test below compares against it. */
    char *pre_before = NULL;
    if (l->commit_policy != HUNMIN_COMMIT_PER_SYLLABLE) {
        const char *pb = hunmin_ic_preedit(l->ic);
        if (pb && *pb) pre_before = dup_or_null(pb);
    }

    const bool consumed = hunmin_ic_process_key(l->ic, base, shift);
    if (consumed) {
        const char *c = drain_engine_commit(l);
        bool ended_by_symbol = false;
        if (l->commit_policy == HUNMIN_COMMIT_PER_SYLLABLE) {
            set_commit(l, c); /* deliver the syllable now */
        } else {
            word_append(l, c); /* hold it until the boundary */
            /* R4: "A symbol is also a termination condition." The symbol stays
             * in the delivered text, and the judgement runs on the word BEFORE
             * it -- `음` judged and delivered, then `.` -- because the whole
             * `음.` is not a word in either language and would abstain.
             *
             * The key was consumed, so no frontend will insert the symbol: it
             * rides in this commit (R10: the client ends up holding exactly the
             * text the person typed). */
            size_t symlen = trailing_symbol_bytes(l->word);
            if (symlen && symlen < (size_t)l->wordlen) {
                char *sym = malloc(symlen + 1);
                if (sym) {
                    memcpy(sym, l->word + l->wordlen - symlen, symlen);
                    sym[symlen] = 0;
                    l->wordlen -= symlen;
                    l->word[l->wordlen] = 0;
                    const char *word = boundary(l, true);
                    size_t n = strlen(word);
                    char *both = malloc(n + symlen + 1);
                    if (both) {
                        memcpy(both, word, n);
                        memcpy(both + n, sym, symlen + 1);
                        set_commit(l, both);
                        free(both);
                    } else {
                        set_commit(l, word);
                    }
                    free(sym);
                    ended_by_symbol = true;
                }
            }
            if (!ended_by_symbol) {
                /* R4-정정: a word past WORD_CAP delivers its first half here, so
                 * what the frontend must render stays bounded. "" for every real
                 * word. */
                (void)word_flush_half(l);
            }
        }
        if (!ended_by_symbol) keys_push(l, base, shift); /* the Latin reading's raw material */
    } else {
        /* A declined key IS the boundary: the engine finalises the syllable it
         * was holding, and the frontend passes the key to its client. Every
         * frontend used to spell this out itself. */
        (void)boundary(l, true);
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
    return boundary(l, true);
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
    keys_clear(l);
    l->judged = -1;
    if (l->latin_out) l->latin_out[0] = 0;
    if (l->commit_out) l->commit_out[0] = 0;
    if (l->preedit_out) l->preedit_out[0] = 0;
    if (l->judged_out) l->judged_out[0] = 0;
    if (l->hangul_out) l->hangul_out[0] = 0;
}

bool hunmin_buffered_preedit_set_option(hunmin_buffered_preedit *l, const char *name, int value) {
    if (!l || !l->ic || !name) return false;
    /* The word cap is the LAYER's, not the ic's: it bounds the text this object
     * holds for one word (docs/SPEC-MODES.md §R4-정정). Bytes, 0 = no cap. A
     * lowered cap applies from the next key, which keeps the setter free of the
     * "flush while the caller is mid-edit" question -- the word is flushed as soon
     * as the next key makes it exceed the new bound. */
    if (!strcmp(name, "word_cap")) {
        if (value < 0) return false;
        l->cap = (size_t)value;
        return true;
    }
    /* E6's option: 1 = judge at every key (the default), 0 = at the boundary only.
     * A change takes effect on the next rendered key; nothing already displayed is
     * rewritten, which is R10 seen from the display side. */
    if (!strcmp(name, "judge_timing")) {
        if (value != 0 && value != 1) return false;
        l->judge_timing = value;
        return true;
    }
    return hunmin_ic_set_option(l->ic, name, value);
}

bool hunmin_buffered_preedit_get_option(const hunmin_buffered_preedit *l, const char *name, int *out) {
    if (!l || !l->ic || !name || !out) return false;
    if (!strcmp(name, "word_cap")) {
        *out = (int)l->cap;
        return true;
    }
    if (!strcmp(name, "judge_timing")) {
        *out = l->judge_timing;
        return true;
    }
    /* Issue #207: syllable commit off-switches autoswitch. A layer built with
     * HUNMIN_COMMIT_PER_SYLLABLE has no autoswitch whatever was stored, so the
     * state reads as off instead of being left inert. */
    if (!strcmp(name, "autoswitch") && l->commit_policy == HUNMIN_COMMIT_PER_SYLLABLE) {
        *out = 0;
        return true;
    }
    return hunmin_ic_get_option(l->ic, name, out);
}

const char *hunmin_buffered_preedit_autoswitch_reason(const hunmin_buffered_preedit *l) {
    if (!l) return NULL;
    if (l->commit_policy == HUNMIN_COMMIT_PER_SYLLABLE) {
        return "autoswitch off: syllable commit policy (HUNMIN_COMMIT_PER_SYLLABLE)";
    }
    return NULL;
}

const char *hunmin_buffered_preedit_get_autoswitch_reason(const hunmin_buffered_preedit *l) {
    return hunmin_buffered_preedit_autoswitch_reason(l);
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
    const char *word = boundary(l, true);
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

/* The chord an event carries for the language key: the chord bits it maps onto
 * plus SHIFT when Shift was held. Both classify() and special() read this one
 * function, so the gesture the rule recognises and the gesture the dispatch
 * services cannot drift apart. */
static unsigned special_want(const hunmin_key_event *ev) {
    unsigned want = ev->mods & HUNMIN_MOD_CHORD;
    if (ev->shift) want |= HUNMIN_MOD_SHIFT;
    return want;
}

/* Is any function bound to this chord? Used by classify() so an unbound chord
 * degrades to the plain toggle instead of becoming a dead key. */
static bool special_bound(const hunmin_buffered_preedit *l, unsigned want) {
    for (int fn = HUNMIN_SPECIAL_NONE + 1; fn < HUNMIN_SPECIAL_LIMIT; ++fn)
        if (l->special_mods[fn] == want) return true;
    return false;
}

hunmin_key_action hunmin_buffered_preedit_classify(const hunmin_buffered_preedit *l, const hunmin_key_event *ev) {
    if (!l || !l->ic || !ev) return HUNMIN_KEY_BOUNDARY_PASS;
    /* The language action first: the Han/Eng key is the one key that is never
     * composition input, whatever else is held (a repeat guard is a platform
     * fact and stays in the adapter). */
    if (ev->language_key) {
        /* The compound press is whichever chord has a function bound to it
         * (ruled 2026-10-31, docs/SPEC-MODES.md §E5). SHIFT is admitted HERE and
         * only here: for characters it is a layout row (ev->shift selects a row),
         * while Shift + the language key is a special combination by ruling --
         * "if it is used with normal characters, work as capitalize. but with
         * special combination such as shift + han/eng, it should be able to do
         * special work."
         *
         * An unbound chord answers TOGGLE rather than CHORD: a platform with
         * nothing bound must not turn Shift+한/영 into a dead key. */
        if (special_want(ev) && special_bound(l, special_want(ev))) return HUNMIN_KEY_LANGUAGE_CHORD;
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

/* ------------------------------------------------------ special functions */
/* The binding lives here, not in the frontends (R13-정정: one value, three
 * readers), but it is empty until a frontend seeds it -- the platform owns its
 * key codes, the engine owns the assignment and the resolution. */

bool hunmin_buffered_preedit_special_bind(hunmin_buffered_preedit *l, hunmin_special_function fn, unsigned mods) {
    if (!l) return false;
    if (fn <= HUNMIN_SPECIAL_NONE || fn >= HUNMIN_SPECIAL_LIMIT) return false;
    /* Chord bits, plus SHIFT: Shift is a layout row for CHARACTERS, but with the
     * language key it is a special combination by ruling (2026-10-31). The
     * distinction lives in special_want(): the shift bit reaches a binding only
     * from the language key, never from an ordinary key's row. */
    mods &= (HUNMIN_MOD_CHORD | HUNMIN_MOD_SHIFT);
    l->special_mods[fn] = mods;
    return true;
}

unsigned hunmin_buffered_preedit_special_chord(const hunmin_buffered_preedit *l, hunmin_special_function fn) {
    if (!l || fn <= HUNMIN_SPECIAL_NONE || fn >= HUNMIN_SPECIAL_LIMIT) return 0;
    return l->special_mods[fn];
}

void hunmin_buffered_preedit_special_host(hunmin_buffered_preedit *l, const hunmin_special_host *host) {
    if (!l) return;
    if (host) {
        l->special_host = *host;
        l->special_host_set = true;
        return;
    }
    memset(&l->special_host, 0, sizeof l->special_host);
    l->special_host_set = false;
}

int hunmin_buffered_preedit_special_peek(const hunmin_buffered_preedit *l, const hunmin_key_event *ev) {
    if (!l || !ev || !ev->language_key) return 0;
    const unsigned want = special_want(ev);
    if (!want) return 0; /* the single press: the classifier answers TOGGLE */
    /* Lowest number wins when two functions are bound to one chord; the dialog
     * shows the binding, and the refusal below is what an unserviceable one
     * looks like. */
    for (int fn = HUNMIN_SPECIAL_NONE + 1; fn < HUNMIN_SPECIAL_LIMIT; ++fn) {
        if (l->special_mods[fn] != want) continue;
        if (!l->special_host_set) return -1;
        if (l->special_host.supports && !l->special_host.supports(l->special_host.user, (hunmin_special_function)fn)) return -1;
        /* B3 key-surface gate (DESIGN-CORE.md#b3-autoswitch-data-gate-and-default):
         * autoswitch chord cannot move state when modes data is absent. */
        if (fn == HUNMIN_SPECIAL_AUTOSWITCH && hm_modes_data_present(l->ctx) != 1) return -1;
        return fn;
    }
    return 0;
}

int hunmin_buffered_preedit_special(hunmin_buffered_preedit *l, const hunmin_key_event *ev) {
    /* One resolver, two entry points: the peek answers a platform "test" phase
     * and this performs. A second copy of the loop is how the gesture the test
     * claims and the gesture the down services would drift apart. */
    const int fn = hunmin_buffered_preedit_special_peek(l, ev);
    if (fn <= 0) return fn;
    /* B3/E5: engine owns AUTOSWITCH transition (DESIGN-CORE.md#b3.../#e5...);
     * only serviced press flips; refusal/not-ours leaves state unchanged. */
    if (fn == HUNMIN_SPECIAL_AUTOSWITCH && l->ic) l->ic->autoswitch = !l->ic->autoswitch;
    if (l->special_host.perform) l->special_host.perform(l->special_host.user, (hunmin_special_function)fn);
    return fn;
}

const char *hunmin_special_name(hunmin_special_function fn) {
    switch (fn) {
    case HUNMIN_SPECIAL_VOICE: return "voice";
    case HUNMIN_SPECIAL_AUTOSWITCH: return "autoswitch";
    case HUNMIN_SPECIAL_NONE:
    case HUNMIN_SPECIAL_LIMIT:
    default: return "";
    }
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

/* Han/Eng toggle (SPEC-MODES R13/FE7; DESIGN-FRONTEND.md Part A §4;
 * DESIGN-CORE.md#r8-judgement--judge_boundary-implementation). Boundary commits
 * current language's composing; re-creates engine for target layout. Does NOT
 * judge (R13): manual press overrides R8 inference; word delivered in active
 * language. */
int hunmin_buffered_preedit_toggle_language(hunmin_buffered_preedit *l, hunmin_key_result *out) {
    if (out) {
        out->consumed = 0;
        out->commit = "";
        out->preedit = "";
    }
    if (!l || !l->ic) return 0;
    const char *commit = boundary(l, false);
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

/* Take R8 judgement once (SPEC-MODES R8/R10; DESIGN-CORE.md#r8-judgement--judge_boundary-implementation).
 * Returns HUNMIN_LANGUAGE_* for frontend to adopt, or -1 if no action. Cleared
 * on read, replaced per boundary; PER_SYLLABLE => -1 (R10). Word text is
 * already delivered in commit; this is the mode half only. */
int hunmin_buffered_preedit_take_judgement(hunmin_buffered_preedit *l) {
    if (!l) return -1;
    const int judged = l->judged;
    l->judged = -1;
    return judged;
}

int hunmin_buffered_preedit_language(const hunmin_buffered_preedit *l) { return (l && l->hangul_mode) ? HUNMIN_LANGUAGE_HANGUL : HUNMIN_LANGUAGE_LATIN; }
int hunmin_buffered_preedit_commit_policy(const hunmin_buffered_preedit *l) { return l ? l->commit_policy : -1; }
const char *hunmin_buffered_preedit_layout_id(const hunmin_buffered_preedit *l) {
    if (!l) return NULL;
    return l->hangul_mode ? l->hangul_id : l->latin_id;
}

/* The Latin reading of the word that last finished -- R8's `english_valid`
 * input, already replayed, so a caller does no engine work of its own.
 * Empty while a word is composing and empty after a boundary that ended no
 * word, so a stale word is never judged twice; empty for a layer built with one
 * language, which is fail-closed and not an error (SPEC-MODES R8-정정-2). */
const char *hunmin_buffered_preedit_latin_text(const hunmin_buffered_preedit *l) {
    if (!l || !l->latin_out || l->keylen) return "";
    return l->latin_out;
}

/* ------------------------------------------------- english words (R8-정정) */

/* The set is created on the first call that needs it and stays for the life of
 * the layer, so a frontend may load the dictionary once at construction and
 * reload only the user's words when its setting changes. */
static hm_wordset *english_set(hunmin_buffered_preedit *l, bool create) {
    if (!l) return NULL;
    if (!l->english && create) l->english = hm_wordset_new();
    return l->english;
}

int hunmin_buffered_preedit_load_english(hunmin_buffered_preedit *l, const char *path) {
    hm_wordset *w = english_set(l, true);
    if (!w) return 0;
    if (!path || !*path) return -1;
    return hm_wordset_load_json(w, path);
}

int hunmin_buffered_preedit_load_english_shipped(hunmin_buffered_preedit *l) {
    hm_wordset *w = english_set(l, true);
    if (!w) return 0;
    /* The layer's context names the data dirs, so the caller names no path --
     * the same resolution the hanja/krdict/idioms tables use. Absent stays
     * absent: -1, and every lookup keeps answering 0 (B3, issue #167). */
    return hm_wordset_load_shipped(w, l->ctx);
}

int hunmin_buffered_preedit_load_user_words(hunmin_buffered_preedit *l, const char *path) {
    hm_wordset *w = english_set(l, true);
    if (!w) return 0;
    if (!path || !*path) return -1;
    return hm_wordset_load_words(w, path);
}

int hunmin_buffered_preedit_add_user_word(hunmin_buffered_preedit *l, const char *word) {
    hm_wordset *w = english_set(l, true);
    return w ? hm_wordset_add(w, word) : 0;
}

int hunmin_buffered_preedit_english_valid(const hunmin_buffered_preedit *l, const char *word) {
    if (!l || !hm_wordset_ready(l->english)) return 0;
    return hm_wordset_has(l->english, word);
}

int hunmin_buffered_preedit_english_ready(const hunmin_buffered_preedit *l) {
    return l ? hm_wordset_ready(l->english) : 0;
}
