/* ic.c — hunmin input context. Behavior is a faithful port of
 * libhangul 0.2.0's hangul_ic_process_jamo()/process_jaso() (LGPL-2.1+),
 * verified event-by-event against tests/golden/ref-*.jsonl generated from
 * the reference engine. Tables come from tables.c (generated). */
#include "hunmin_internal.h"
#include "tables.h"

#include <stdio.h>
#include <string.h>

#define CF 0x115F /* choseong filler */
#define JF 0x1160 /* jungseong filler */

/* hm_buf_t and struct hunmin_ic now live in hunmin_internal.h: the shared
 * buffered/syllable layer reads the same state (issue #128). */

static void buf_clear(hm_buf_t *b) {
    b->cho = b->jung = b->jong = 0;
    b->jvar = 0;
    b->index = -1;
}

static void buf_push(hm_buf_t *b, ucschar ch) {
    if (hm_is_choseong(ch)) b->cho = ch;
    else if (hm_is_jungseong(ch)) b->jung = ch;
    else if (hm_is_jongseong(ch)) b->jong = ch;
    if (b->index < 11) b->stack[++b->index] = ch;
}

static ucschar buf_pop(hm_buf_t *b) { return b->index >= 0 ? b->stack[b->index--] : 0; }
static ucschar buf_peek(hm_buf_t *b) { return b->index >= 0 ? b->stack[b->index] : 0; }

static void flags_rebuild(hunmin_ic *ic);

static int jaso_to_string(ucschar cho, ucschar jung, ucschar jong, ucschar *out, int cap) {
    int n = 0;
    ucschar ch;
#define P(x)                                                                                                                                         \
    do {                                                                                                                                             \
        if (n < cap - 1) out[n++] = (x);                                                                                                             \
    } while (0)
    if (cho) {
        if (jung) {
            ch = hm_jamo_to_syllable(cho, jung, jong);
            if (ch) P(ch);
            else {
                P(cho);
                P(jung);
                if (jong) P(jong);
            }
        } else {
            if (jong) {
                P(cho);
                P(JF);
                P(jong);
            } else {
                ch = hm_jamo_to_cjamo(cho);
                if (hm_is_cjamo(ch)) P(ch);
                else {
                    P(cho);
                    P(JF);
                }
            }
        }
    } else if (jung) {
        if (jong) {
            P(CF);
            P(jung);
            P(jong);
        } else {
            ch = hm_jamo_to_cjamo(jung);
            if (hm_is_cjamo(ch)) P(ch);
            else {
                P(CF);
                P(jung);
            }
        }
    } else if (jong) {
        ch = hm_jamo_to_cjamo(jong);
        if (hm_is_cjamo(ch)) P(ch);
        else {
            P(CF);
            P(JF);
            P(jong);
        }
    }
#undef P
    out[n] = 0;
    return n;
}

static const hm_combo *layout_combine_find(const hm_layout *L, ucschar a, ucschar b) {
    uint32_t key = ((a & 0xFFFF) << 16) | (b & 0xFFFF);
    size_t lo = 0, hi = L->ncombo;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (L->combo[mid].ab < key) lo = mid + 1;
        else hi = mid;
    }
    if (lo < L->ncombo && L->combo[lo].ab == key) return &L->combo[lo];
    return NULL;
}

static ucschar layout_combine(const hm_layout *L, ucschar a, ucschar b) {
    const hm_combo *c = layout_combine_find(L, a, b);
    return c ? c->r : 0;
}

static ucschar ic_cho_to_jong(hunmin_ic *ic, ucschar cho) {
    ucschar jong = hm_choseong_to_jongseong(cho);
    if (hm_is_jongseong_conjoinable(jong)) return jong;
    if (ic->L->type == HM_TYPE_JAMO_YET || ic->L->type == HM_TYPE_JASO_YET) return jong;
    return 0;
}

static ucschar ic_combine(hunmin_ic *ic, ucschar first, ucschar second) {
    if (!ic->opt_combi_on_double && ic->L->type == HM_TYPE_JAMO && first == second && hm_is_jamo_conjoinable(first)) return 0;
    ucschar r = layout_combine(ic->L, first, second);
    if (!ic->opt_non_choseong_combi && hm_is_choseong(first) && hm_is_choseong(second) && hm_is_jongseong(r)) return 0;
    return r;
}

static void save_preedit(hunmin_ic *ic) { jaso_to_string(ic->b.cho, ic->b.jung, ic->b.jong, ic->preedit, STR_CAP); }

/* append buffer rendering to commit, then clear buffer (libhangul:
 * save_commit_string keeps existing commit text) */
static void save_commit(hunmin_ic *ic) {
    int i = 0;
    while (i < STR_CAP - 1 && ic->commit[i]) i++;
    ucschar tmp[STR_CAP];
    jaso_to_string(ic->b.cho, ic->b.jung, ic->b.jong, tmp, STR_CAP);
    for (int j = 0; tmp[j] && i < STR_CAP - 1; j++) ic->commit[i++] = tmp[j];
    ic->commit[i] = 0;
    buf_clear(&ic->b);
}

static void append_commit(hunmin_ic *ic, ucschar ch) {
    int i = 0;
    while (i < STR_CAP - 1 && ic->commit[i]) i++;
    if (i < 63) {
        ic->commit[i++] = ch;
        ic->commit[i] = 0;
    }
}

static bool ic_push(hunmin_ic *ic, ucschar c) {
    if (!hm_is_jamo(c)) {
        ic->preedit[0] = 0;
        save_commit(ic);
        return false;
    }
    buf_push(&ic->b, c);
    return true;
}

/* libhangul idiom (hangulinputcontext.c): a failed push flushes the buffer, so
 * the caller retries with the same or the original jamo — the second attempt can
 * succeed post-flush. Kept verbatim for parity; never "simplify" it. */
static bool push_any(hunmin_ic *ic, ucschar a, ucschar b) { return ic_push(ic, a) || ic_push(ic, b); }

static bool process_jamo(hunmin_ic *ic, ucschar ch) {
    hm_buf_t *b = &ic->b;
    ucschar jong, combined;

    if (!hm_is_jamo(ch) && ch > 0) {
        save_commit(ic);
        append_commit(ic, ch);
        save_preedit(ic);
        return true;
    }

    if (b->jong) {
        if (hm_is_choseong(ch)) {
            jong = ic_cho_to_jong(ic, ch);
            combined = ic_combine(ic, b->jong, jong);
            if (hm_is_jongseong(combined)) {
                if (!push_any(ic, combined, ch)) return false;
            } else {
                save_commit(ic);
                if (!ic_push(ic, ch)) return false;
            }
        } else if (hm_is_jungseong(ch)) {
            ucschar pop = buf_pop(b);
            ucschar peek = buf_peek(b);
            if (hm_is_jongseong(peek)) {
                ucschar cho = hm_jongseong_get_diff(peek, b->jong);
                if (cho == 0) {
                    save_commit(ic);
                    if (!ic_push(ic, ch)) return false;
                } else {
                    b->jong = peek;
                    save_commit(ic);
                    ic_push(ic, cho);
                    if (!ic_push(ic, ch)) return false;
                }
            } else {
                b->jong = 0;
                save_commit(ic);
                ic_push(ic, hm_jongseong_to_choseong(pop));
                if (!ic_push(ic, ch)) return false;
            }
        } else {
            goto flush; /* non-jamo while composing: commit + not-consumed
                         * (libhangul flush semantics, incl. ch == 0 null) */
        }
    } else if (b->jung) {
        if (hm_is_choseong(ch)) {
            if (b->cho) {
                jong = ic_cho_to_jong(ic, ch);
                if (hm_is_jongseong(jong)) {
                    if (!push_any(ic, jong, ch)) return false;
                } else {
                    save_commit(ic);
                    if (!ic_push(ic, ch)) return false;
                }
            } else if (ic->opt_auto_reorder) {
                if (!push_any(ic, ch, ch)) return false;
            } else {
                save_commit(ic);
                if (!ic_push(ic, ch)) return false;
            }
        } else if (hm_is_jungseong(ch)) {
            combined = ic_combine(ic, b->jung, ch);
            if (hm_is_jungseong(combined)) {
                if (!ic_push(ic, combined)) return false;
            } else {
                save_commit(ic);
                if (!ic_push(ic, ch)) return false;
            }
        } else {
            goto flush;
        }
    } else if (b->cho) {
        if (hm_is_choseong(ch)) {
            combined = ic_combine(ic, b->cho, ch);
            if (hm_is_jongseong(combined)) {
                b->cho = 0;
                ucschar pop = buf_pop(b);
                ic_push(ic, hm_choseong_to_jongseong(pop));
            }
            if (!push_any(ic, combined, ch)) return false;
        } else {
            if (!push_any(ic, ch, ch)) return false;
        }
    } else {
        if (!ic_push(ic, ch)) return false;
    }

    save_preedit(ic);
    return true;

flush:
    ic->preedit[0] = 0;
    save_commit(ic);
    return false;
}

static bool process_jaso(hunmin_ic *ic, ucschar ch, bool vg, bool vj) {
    hm_buf_t *b = &ic->b;

    if (hm_is_choseong(ch)) {
        if (!b->cho) {
            /* lone 받침 composing + choseong input: try the data-driven
             * 받침-extension first (ist UnitMix rows keyed (jong, choseong) — the
             * 2024 윗글 gG/eE/vV 받침 ㄸ/ㅃ/ㅉ rule; the choseong-typed result
             * replaces the 받침). G-gate mirrors the jong path. No combo falls
             * through to the normal commit+push below. */
            if (b->jong && !b->jung) {
                ucschar r = ic_combine(ic, b->jong, ch);
                if (ic->L->jongonly_fresh && !vg && r) {
                    const hm_combo *c = layout_combine_find(ic->L, b->jong, ch);
                    if (c && c->ssang) r = 0;
                }
                if (r) {
                    if (buf_peek(b) == b->jong) buf_pop(b);
                    b->jong = 0;
                    if (!push_any(ic, r, r)) return false;
                    save_preedit(ic);
                    return true;
                }
            }
            if (ic->opt_auto_reorder) {
                if (!push_any(ic, ch, ch)) return false;
            } else {
                if (b->jung || b->jong) {
                    save_commit(ic);
                    if (!ic_push(ic, ch)) return false;
                } else {
                    if (!push_any(ic, ch, ch)) return false;
                }
            }
        } else {
            ucschar r = 0;
            if (hm_is_choseong(buf_peek(b))) r = ic_combine(ic, b->cho, ch);
            if (r) {
                if (!push_any(ic, r, r)) return false;
            } else {
                save_commit(ic);
                if (!ic_push(ic, ch)) return false;
            }
        }
    } else if (hm_is_jungseong(ch)) {
        /* h3 layouts: only right-side variant vowels (U+1F5-504 lines, lifted with
         * {"j","variant"}) absorb a following vowel — patim "ㅗㅏ" test requires
         * shift-V ㅗ then shift-F ㅏ to stay uncombined. */
        bool was_jvar = b->jvar;
        b->jvar = vj;
        if (ic->L->supports_case && b->jung && !was_jvar) {
            save_commit(ic);
            if (!ic_push(ic, ch)) return false;
            save_preedit(ic);
            return true;
        }
        if (!b->jung) {
            if (ic->opt_auto_reorder) {
                if (!push_any(ic, ch, ch)) return false;
            } else {
                if (b->jong) {
                    save_commit(ic);
                    if (!ic_push(ic, ch)) return false;
                } else {
                    if (!push_any(ic, ch, ch)) return false;
                }
            }
        } else {
            ucschar r = 0;
            if (hm_is_jungseong(buf_peek(b))) r = ic_combine(ic, b->jung, ch);
            if (r) {
                if (!push_any(ic, r, r)) return false;
            } else {
                save_commit(ic);
                if (!push_any(ic, ch, ch)) return false;
            }
        }
    } else if (hm_is_jongseong(ch)) {
        if (!b->jong) {
            if (!push_any(ic, ch, ch)) return false;
        } else {
            ucschar r = 0;
            if (hm_is_jongseong(buf_peek(b))) r = ic_combine(ic, b->jong, ch);
            /* G-gate, pair-aware (docs §2-3 vs §2-4-2): after a jong-only syllable,
             * modern compound-batchim extension needs the shift mark (cq→ㄱ,ㅅ while
             * cQ→ㄳ); old-gul chains (ㅇ+ㄱ→ᇬ) compose ungated. Non-h3 layouts
             * (dubeolsik/390, libhangul parity) are untouched. */
            if (ic->L->jongonly_fresh && !vg && !b->cho && !b->jung && r) {
                const hm_combo *c = layout_combine_find(ic->L, b->jong, ch);
                if (c && c->ssang) r = 0;
            }
            if (r) {
                if (!push_any(ic, r, r)) return false;
            } else {
                save_commit(ic);
                if (!push_any(ic, ch, ch)) return false;
            }
        }
    } else if (ch > 0) {
        save_commit(ic);
        append_commit(ic, ch);
    } else {
        save_commit(ic);
        return false;
    }
    save_preedit(ic);
    return true;
}

static int utf8_encode(ucschar cp, char *o) {
    if (cp < 0x80) {
        o[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        o[0] = (char)(0xC0 | cp >> 6);
        o[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        o[0] = (char)(0xE0 | cp >> 12);
        o[1] = (char)(0x80 | (cp >> 6 & 0x3F));
        o[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    o[0] = (char)(0xF0 | cp >> 18);
    o[1] = (char)(0x80 | (cp >> 12 & 0x3F));
    o[2] = (char)(0x80 | (cp >> 6 & 0x3F));
    o[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static char *to_utf8(const ucschar *s, char *out, size_t cap) {
    size_t o = 0;
    for (; *s; s++) {
        int n = utf8_encode(*s, out + o);
        if (o + (size_t)n >= cap - 1) break;
        o += (size_t)n;
    }
    out[o] = 0;
    return out;
}

/* process_romaja — verbatim port of libhangul hangul_ic_process_romaja()
 * (romanization automaton: letters double as jamo sources, 'x' marks cieuc,
 * impossible-composition paths inject ieung/EU fillers). Parity is verified
 * by tests/golden/ref-romaja.jsonl + parity-sweep. Do not "simplify". */
static bool process_romaja(hunmin_ic *ic, int ascii, ucschar ch) {
    hm_buf_t *b = &ic->b;
    ucschar jong, combined;

    if (!hm_is_jamo(ch) && ch > 0) {
        save_commit(ic);
        append_commit(ic, ch);
        save_preedit(ic);
        return true;
    }

    if (ascii >= 'A' && ascii <= 'Z') { save_commit(ic); }

    if (b->jong) {
        if (ascii == 'x' || ascii == 'X') {
            ch = 0x110c;
            save_commit(ic);
            if (!ic_push(ic, ch)) return false;
        } else if (hm_is_choseong(ch) || hm_is_jongseong(ch)) {
            jong = hm_is_jongseong(ch) ? ch : ic_cho_to_jong(ic, ch);
            combined = ic_combine(ic, b->jong, jong);
            if (hm_is_jongseong(combined)) {
                if (!push_any(ic, combined, ch)) return false;
            } else {
                save_commit(ic);
                if (!ic_push(ic, ch)) return false;
            }
        } else if (hm_is_jungseong(ch)) {
            if (b->jong == 0x11bc) {
                save_commit(ic);
                b->cho = 0x110b;
                ic_push(ic, ch);
            } else {
                ucschar pop = buf_pop(b);
                ucschar peek = buf_peek(b);
                if (hm_is_jungseong(peek)) {
                    if (pop == 0x11aa) {
                        b->jong = 0x11a8;
                        pop = 0x11ba;
                    } else {
                        b->jong = 0;
                    }
                    save_commit(ic);
                    ic_push(ic, hm_jongseong_to_choseong(pop));
                    if (!ic_push(ic, ch)) return false;
                } else {
                    ucschar cho2 = 0, jong2 = 0;
                    hm_jongseong_decompose(b->jong, &jong2, &cho2);
                    b->jong = jong2;
                    save_commit(ic);
                    ic_push(ic, cho2);
                    if (!ic_push(ic, ch)) return false;
                }
            }
        } else {
            goto flush;
        }
    } else if (b->jung) {
        if (hm_is_choseong(ch)) {
            if (b->cho) {
                jong = ic_cho_to_jong(ic, ch);
                if (hm_is_jongseong(jong)) {
                    if (!push_any(ic, jong, ch)) return false;
                } else {
                    save_commit(ic);
                    if (!ic_push(ic, ch)) return false;
                }
            } else if (ic->opt_auto_reorder) {
                if (!push_any(ic, ch, ch)) return false;
            } else {
                save_commit(ic);
                if (!ic_push(ic, ch)) return false;
            }
        } else if (hm_is_jungseong(ch)) {
            combined = ic_combine(ic, b->jung, ch);
            if (hm_is_jungseong(combined)) {
                if (!ic_push(ic, combined)) return false;
            } else {
                save_commit(ic);
                b->cho = 0x110b;
                if (!ic_push(ic, ch)) return false;
            }
        } else if (hm_is_jongseong(ch)) {
            if (!push_any(ic, ch, ch)) return false;
        } else {
            goto flush;
        }
    } else if (b->cho) {
        if (hm_is_choseong(ch)) {
            combined = ic_combine(ic, b->cho, ch);
            if (combined == 0) {
                b->jung = 0x1173;
                ic->preedit[0] = 0;
                save_commit(ic);
                if (!ic_push(ic, ch)) return false;
            } else if (!push_any(ic, combined, ch)) {
                return false;
            }
        } else if (hm_is_jongseong(ch)) {
            b->jung = 0x1173;
            save_commit(ic);
            if (ascii == 'x' || ascii == 'X') ch = 0x110c;
            if (!ic_push(ic, ch)) return false;
        } else {
            if (!push_any(ic, ch, ch)) return false;
        }
    } else {
        if (ascii == 'x' || ascii == 'X') ch = 0x110c;
        if (!ic_push(ic, ch)) {
            return false;
        } else if (b->cho == 0 && b->jung != 0) {
            b->cho = 0x110b;
        }
    }

    save_preedit(ic);
    return true;

flush:
    ic->preedit[0] = 0;
    save_commit(ic);
    return false;
}

/* ---------------- public ---------------- */

hunmin_ic *hunmin_ic_new(hunmin_ctx *ctx, const char *layout_id) {
    if (!ctx || !layout_id) return NULL;
    for (int i = 0; i < ctx->n; i++) {
        if (strcmp(ctx->layouts[i].id, layout_id)) continue;
        if (!ctx->layouts[i].supported) return NULL;
        hunmin_ic *ic = calloc(1, sizeof *ic);
        if (!ic) return NULL;
        ic->L = &ctx->layouts[i];
        ic->ctx = ctx;
        buf_clear(&ic->b);
        memset(ic->flag_ovr, -1, sizeof ic->flag_ovr);
        flags_rebuild(ic);
        ic->opt_auto_reorder = false;
        ic->opt_combi_on_double = false;
        ic->opt_non_choseong_combi = true;
        return ic;
    }
    return NULL;
}

void hunmin_ic_free(hunmin_ic *ic) {
    if (!ic) return;
    free(ic->ext8);
    free(ic);
}

bool hunmin_ic_process(hunmin_ic *ic, uint32_t ch) {
    if (!ic) return false;
    ic->preedit[0] = 0;
    ic->commit[0] = 0;

    if (ch == 0x08) {
        hm_buf_t *b = &ic->b;
        if (b->index >= 0) {
            ucschar popped = buf_pop(b);
            if (b->index >= 0) {
                ucschar pk = buf_peek(b);
                if (hm_is_choseong(popped)) {
                    b->cho = hm_is_choseong(pk) ? pk : 0;
                } else if (hm_is_jungseong(popped)) {
                    b->jung = hm_is_jungseong(pk) ? pk : 0;
                } else if (hm_is_jongseong(popped)) {
                    b->jong = hm_is_jongseong(pk) ? pk : 0;
                }
            } else {
                buf_clear(b);
            }
            save_preedit(ic);
            return true;
        }
        return false;
    }

    uint32_t prevT = ic->T;
    ic->T = 0; /* tier is single-shot; every key ends it */
    const hm_val *rv = NULL;
    if (ch < 128) {
        hm_state st = {.D = ic->b.cho,
                       .E = ic->b.jung,
                       .F = ic->b.jong,
                       .T = prevT,
                       .P = ic->caps,
                       .flags = ic->flags_eff,
                       .J = ic->L->supports_case && ic->b.jung && ic->b.jvar};
        rv = hm_resolvev(&ic->L->keys[ch], st);
    }
    if (rv && rv->kind == HK_NOOP) { /* swallowed inside tier */
        save_preedit(ic);
        return true;
    }
    if (rv && rv->kind == HK_TIER) {
        ic->T = rv->code; /* entering a tier: consume, no text */
        ic->preedit[0] = 0; /* keep rendering as-is */
        save_preedit(ic);
        return true;
    }
    if (rv && rv->kind == HK_SEQ) {
        ic->preedit[0] = 0;
        save_commit(ic); /* flush any pending composition first */
        for (size_t i = 0; i < rv->seqlen && rv->seq[i]; i++) append_commit(ic, rv->seq[i]);
        save_preedit(ic);
        return true;
    }
    ucschar m = (rv && (rv->kind == HK_JAMO || rv->kind == HK_LIT)) ? (ucschar)rv->code : 0;
    /* unmapped (0) mirrors libhangul null entries exactly */
    bool r;
    switch (ic->L->type) {
    case HM_TYPE_JASO:
    case HM_TYPE_JASO_YET: r = process_jaso(ic, m, rv && rv->g, rv && rv->variant); break;
    case HM_TYPE_ROMAJA: r = process_romaja(ic, (int)ch, m); break;
    default: r = process_jamo(ic, m); break;
    }
    return r;
}

bool hunmin_ic_process_key(hunmin_ic *ic, uint32_t base, uint32_t shift) {
    if (!ic) return false;
    /* The case endpoint lives here, not in the frontend: the loader's own
     * US-QWERTY map (schema_loader.c, one home ??docs/IC-INPUT-OPTIONS.md §0-4) turns
     * (base, shift) into the same ch the legacy call would have been handed.
     * When the base has NO shifted form, the character is treated as already
     * resolved and the bit selects nothing further ??that is the
     * fcitx5-shaped-host rule specified in
     * docs/SPEC-P2.md §"IC input entry points", not invented per-host. */
    if (shift) {
        uint32_t s = hm_shifted_of(base);
        if (s) return hunmin_ic_process(ic, s);
    }
    return hunmin_ic_process(ic, base);
}

bool hunmin_ic_peek_key(const hunmin_ic *ic, uint32_t base, uint32_t shift) {
    if (!ic) return false;
    /* The state the prediction depends on is all inside the struct; L points
     * into the immutable ctx layout. The external-text pointer is owned by the
     * original ic and is not used by process_key, so clear it in the copy to
     * keep a future path from freeing or reallocating it twice. */
    hunmin_ic copy = *ic;
    copy.ext8 = NULL;
    copy.extcap = 0;
    return hunmin_ic_process_key(&copy, base, shift);
}

static void flags_rebuild(hunmin_ic *ic) {
    for (int i = 0; i < 128; i++) ic->flags_eff[i] = ic->flag_ovr[i] >= 0 ? ic->flag_ovr[i] : ic->L->flags[i];
}

bool hunmin_ic_set_option(hunmin_ic *ic, const char *name, int value) {
    if (!ic || !name) return false;
    if (!strcmp(name, "caps")) {
        ic->caps = !!value;
        return true;
    }
    /* ReqMode13's one on/off state: the settings UI writes it and reads it back
     * here, and the key surface reads the same field, so the two surfaces can
     * only agree. Coerced by !! like "caps" -- the layer's option contract is
     * one name-to-int channel, not a per-option value grammar.
     *
     * B3's data gate (issue #169): ON is refused where the data the judgement
     * needs is not installed, because a switch flipped ON on such a machine has
     * nothing to judge with and silently does nothing -- the state would read
     * enabled while the feature is absent. OFF is always accepted: an install
     * that lost its data must still be able to write the preference down. A
     * refusal is distinguishable from an unknown name by the get_option() pair,
     * which answers this name either way, and by the presence query the caller
     * used to draw the row (hunmin_ctx_autoswitch_data_present()). */
    if (!strcmp(name, "autoswitch")) {
        if (value && !hunmin_ctx_autoswitch_data_present(ic->ctx)) {
            ic->autoswitch = 0;
            return false;
        }
        ic->autoswitch = !!value;
        return true;
    }
    /* 전처리수식 variables (pat.im/1136 §2-6): w W z h H */
    if (name[0] && !name[1] && strchr("wWzHh", name[0])) {
        ic->flag_ovr[(unsigned char)name[0]] = !!value;
        flags_rebuild(ic);
        return true;
    }
    return false;
}

bool hunmin_ic_get_option(const hunmin_ic *ic, const char *name, int *out) {
    if (!ic || !name || !out) return false;
    if (!strcmp(name, "caps")) {
        *out = ic->caps;
        return true;
    }
    if (!strcmp(name, "autoswitch")) {
        *out = ic->autoswitch;
        return true;
    }
    /* The 전처리수식 overrides answer their stored value, -1 included: "the
     * layout's default" is a state a reader wants to see (hunmin_ic_reset
     * restoring it is what makes -1 meaningful), so it is a value and not an
     * unknown name. */
    if (name[0] && !name[1] && strchr("wWzHh", name[0])) {
        *out = ic->flag_ovr[(unsigned char)name[0]];
        return true;
    }
    return false;
}

const char *hunmin_ic_commit(hunmin_ic *ic) {
    if (!ic) return "";
    const char *s = to_utf8(ic->commit, ic->commit8, sizeof ic->commit8);
    ic->commit[0] = 0; /* destructive read: a commit is delivered exactly once;
                        * hunmin_ic_flush relies on this to not re-emit */
    return s;
}

const char *hunmin_ic_preedit(hunmin_ic *ic) {
    if (!ic) return "";
    return to_utf8(ic->preedit, ic->preedit8, sizeof ic->preedit8);
}

void hunmin_ic_reset(hunmin_ic *ic) {
    if (!ic) return;
    ic->preedit[0] = ic->commit[0] = 0;
    buf_clear(&ic->b);
    ic->T = 0; /* pending symbol tier is input state; must not survive reset */
    memset(ic->flag_ovr, -1, sizeof ic->flag_ovr);
    flags_rebuild(ic);
}

const char *hunmin_ic_flush(hunmin_ic *ic) {
    if (!ic) return "";
    ic->preedit[0] = 0;
    save_commit(ic);
    return hunmin_ic_commit(ic);
}

/* --- external text passthrough ---
 *
 * Validate first, commit second. A partially-accepted string is worse than a
 * rejected one: the recogniser's output would reach the client with a hole in
 * the middle of it and nothing to indicate that.
 *
 * The decoder is spelled out here rather than shared with schema_loader.c's
 * cp_from_utf8(), which is static there and used for compose tables under a
 * different assumption (short, well-formed, trusted input). This one is fed
 * untrusted recogniser output, so it enforces what the loader never has to: no
 * truncated tail, no overlong forms, no surrogates, nothing past U+10FFFF. The
 * truncated tail is the case that matters -- speech APIs cut off mid-UTF-8 on a
 * timeout, and accepting the prefix would commit a broken string.
 */
static int utf8_valid(const char *s, size_t *len) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    for (; p[n];) {
        unsigned char c = p[n];
        int need = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
        uint32_t cp = c & (0x7Fu >> need);
        for (int i = 1; i <= need; i++) {
            if ((p[n + i] & 0xC0) != 0x80) return 0; /* truncated, or not a continuation */
            cp = (cp << 6) | (p[n + i] & 0x3F);
        }
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
        /* Overlong: a codepoint encoded in more bytes than it needs. A client that
         * reads these as Latin-1 sees one string, one that validates sees another. */
        static const uint32_t min_cp[4] = {0, 0x80, 0x800, 0x10000};
        if (need && cp < min_cp[need]) return 0;
        n += (size_t)need + 1;
    }
    *len = n;
    return 1;
}

const char *hunmin_ic_commit_text(hunmin_ic *ic, const char *text) {
    if (!ic) return "";
    size_t tlen = 0;
    if (!text || !*text || !utf8_valid(text, &tlen)) return "";

    /* Whatever was being typed goes out first, so the client receives it before
     * the spoken text rather than the two interleaved. Reading it out also clears
     * it, which is what stops the composition reappearing as a preedit under the
     * new text. */
    const char *pending = hunmin_ic_flush(ic);
    size_t plen = strlen(pending);

    size_t need = plen + tlen + 1;
    if (need > ic->extcap) {
        char *p = realloc(ic->ext8, need);
        /* Out of memory: still hand back the typed text rather than losing it. */
        if (!p) return pending;
        ic->ext8 = p;
        ic->extcap = need;
    }
    memcpy(ic->ext8, pending, plen);
    memcpy(ic->ext8 + plen, text, tlen);
    ic->ext8[plen + tlen] = 0;
    return ic->ext8;
}

/* ---------------- ctx public wrappers (in schema_loader.c's file, kept here) ------- */
int hunmin_ctx_layout_count(const hunmin_ctx *ctx) { return ctx ? ctx->n : 0; }
const char *hunmin_ctx_layout_id(const hunmin_ctx *ctx, int i) { return ctx && i >= 0 && i < ctx->n ? ctx->layouts[i].id : NULL; }
const char *hunmin_ctx_layout_name(const hunmin_ctx *ctx, int i, const char *lang) {
    if (!ctx || i < 0 || i >= ctx->n) return NULL;
    const hm_layout *l = &ctx->layouts[i];
    if (lang && !strcmp(lang, "ko") && l->name_ko[0]) return l->name_ko;
    return l->name_en[0] ? l->name_en : l->id;
}
bool hunmin_ctx_layout_supported(const hunmin_ctx *ctx, int i) { return ctx && i >= 0 && i < ctx->n ? ctx->layouts[i].supported : false; }
const char *hunmin_ctx_layout_mode(const hunmin_ctx *ctx, int i) {
    if (!ctx || i < 0 || i >= ctx->n) return NULL;
    return ctx->layouts[i].mode == HM_MODE_LATIN ? "latin" : "hangul";
}
bool hunmin_ctx_layout_autoswitch_capable(const hunmin_ctx *ctx, int i) {
    /* Derived at load from the layout's keys and combination table
     * (schema_loader.c compute_capability): a layout whose keys emit no
     * conjoining jamo has no syllables for the judgement to be right or wrong
     * about, which is the Latin layouts and nothing else. */
    return ctx && i >= 0 && i < ctx->n && ctx->layouts[i].composes_hangul;
}
bool hunmin_ctx_layout_old_hangul(const hunmin_ctx *ctx, int i) {
    /* 1 when this layout can emit a jamo outside the modern 19/21/28. The caller
     * then judges with hunmin_hangul_valid_old(); every other layout uses the
     * modern rule, because widening it there would accept shapes its composer
     * cannot produce. */
    return ctx && i >= 0 && i < ctx->n && ctx->layouts[i].old_hangul;
}
bool hunmin_ctx_autoswitch_data_present(const hunmin_ctx *ctx) {
    /* The data half of ReqMode13's on/off switch (docs/SPEC-MODES.md §ReqMode13,
     * the B3 bullet): the judgement needs the modes word list, so the feature is
     * enable-able only where that list is installed -- and "installed" is asked
     * through the same search every data subdirectory uses (hm_data_dirs), so no
     * frontend assembles "<dir>/modes/en-US.json" itself. The cache the answer
     * is read from is not observable state: the cast only spares every caller a
     * search, and it is the reason the option writer and a settings row cannot
     * disagree. */
    return hm_modes_data_present((hunmin_ctx *)ctx) == 1;
}
hunmin_ctx *hunmin_ctx_new(const char *const *dirs, int ndirs) { return hm_data_load(dirs, ndirs); }
void hunmin_ctx_free(hunmin_ctx *ctx) { hm_data_free(ctx); }
