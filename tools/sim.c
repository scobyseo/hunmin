/* hunmin-sim — replay/inspect/check layouts. Usage:
 *   hunmin-sim --data DIR --layout ID --keys 'gqqw' [--trace]
 *   hunmin-sim --data DIR --check tests/golden/ref-dubeolsik.jsonl
 *   hunmin-sim --data DIR --judge-check tests/golden/judgement-r8.jsonl
 *   hunmin-sim --data DIR --off-check tests/golden/ref-dubeolsik.jsonl ...  (any number of corpora)
 *   hunmin-sim --data DIR --layout ID --latin LATIN --judge [--autoswitch N]
 *              [--start-latin] --keys '<case>' [--trace]
 * --check drives the engine with each event's "in" and compares commit and
 * preedit exactly (tier A parity gate).
 *
 * --judge moves the driver up one level: instead of hunmin_ic it drives
 * hunmin_buffered_preedit, the shared layer every frontend sits on
 * (docs/FRONTEND-EQUIVALENCE.md §5), and it performs the frontend's half of
 * SPEC-MODES R8 exactly as the adapters do — read take_judgement() after each
 * key, and switch language when it answers a mode. That is what makes a
 * judgement case replayable from the command line: the delivered text, the
 * judgement and the mode after the boundary are all observations of the layer.
 * --judge-check runs a corpus of those cases (tests/golden/judgement-*.jsonl).
 * --keys/--batch may carry an explicit shift on a key (issue #90,
 * hunmin_ic_process_key): "\\s<char>" is the base char with Shift held, "\\b"
 * is backspace, "\\" (two backslashes) is a literal backslash. Any other
 * escape is a load error, not a silently literalised backslash. Committed
 * callers never feed a raw backslash (it is excluded from every key
 * alphabet), so a key string without backslashes behaves exactly as before. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "hunmin.h"

/* Escape a string into a JSON string body (no surrounding quotes).
 *
 * Writes at most cap-1 bytes plus a NUL and stops early rather than truncating
 * mid-escape: a half-written \u sequence would be worse than a shortened
 * string, because it would still be invalid JSON. */
static void json_escape(const char *in, char *out, size_t cap) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 7 < cap; p++) {
        switch (*p) {
        case '"':
            out[o++] = '\\';
            out[o++] = '"';
            break;
        case '\\':
            out[o++] = '\\';
            out[o++] = '\\';
            break;
        case '\n':
            out[o++] = '\\';
            out[o++] = 'n';
            break;
        case '\r':
            out[o++] = '\\';
            out[o++] = 'r';
            break;
        case '\t':
            out[o++] = '\\';
            out[o++] = 't';
            break;
        default:
            if (*p < 0x20) o += (size_t)snprintf(out + o, cap - o, "\\u%04x", *p);
            else out[o++] = (char)*p;
        }
    }
    out[o] = 0;
}
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path, long *szp) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[sz] = 0;
    fclose(f);
    if (szp) *szp = sz;
    return buf;
}

/* expand Hangul syllables to jamo (canonical equivalence within our alphabet:
 * NFC "가" and jamo "가" compare equal — patim assertions mix the two) */
static void put_cp(char *o, size_t *n, size_t cap, uint32_t cp) {
    char b[4];
    int l;
    if (cp < 0x80) {
        b[0] = (char)cp;
        l = 1;
    } else if (cp < 0x800) {
        b[0] = (char)(0xC0 | cp >> 6);
        b[1] = (char)(0x80 | (cp & 0x3F));
        l = 2;
    } else if (cp < 0x10000) {
        b[0] = (char)(0xE0 | cp >> 12);
        b[1] = (char)(0x80 | (cp >> 6 & 0x3F));
        b[2] = (char)(0x80 | (cp & 0x3F));
        l = 3;
    } else {
        b[0] = (char)(0xF0 | cp >> 18);
        b[1] = (char)(0x80 | (cp >> 12 & 0x3F));
        b[2] = (char)(0x80 | (cp >> 6 & 0x3F));
        b[3] = (char)(0x80 | (cp & 0x3F));
        l = 4;
    }
    if (*n + (size_t)l < cap - 1) {
        for (int i = 0; i < l; i++) o[(*n)++] = b[i];
    }
}

static void hangul_canon(const char *s, char *out, size_t cap) {
    size_t o = 0;
    while (*s && o + 12 < cap) {
        unsigned char c = (unsigned char)*s;
        uint32_t cp;
        int l;
        if (c < 0x80) {
            cp = c;
            l = 1;
        } else if ((c >> 5) == 6) {
            cp = c & 0x1F;
            l = 2;
        } else if ((c >> 4) == 0xE) {
            cp = c & 0xF;
            l = 3;
        } else {
            cp = c & 7;
            l = 4;
        }
        for (int i = 1; i < l; i++) cp = (cp << 6) | ((unsigned char)s[i] & 0x3F);
        s += l;
        if (cp >= 0xAC00 && cp <= 0xD7A3) {
            uint32_t x = cp - 0xAC00, t = x % 28;
            put_cp(out, &o, cap, 0x1100 + x / 588);
            put_cp(out, &o, cap, 0x1161 + x % 588 / 28);
            if (t) put_cp(out, &o, cap, 0x11A8 + t - 1);
        } else {
            put_cp(out, &o, cap, cp);
        }
    }
    out[o] = 0;
}

/* Consume one keystroke event from *s and advance it: a plain byte, or a
 * backslash escape ("\s<char>" explicit shift driven through
 * hunmin_ic_process_key, "\b" backspace, "\\" a literal backslash). Returns 0,
 * or -1 after naming the bad escape — an unknown escape must fail loudly, not
 * become a backslash-then-character pair. */
static int keys_step(const char **s, uint32_t *ch, uint32_t *shift, const char *what) {
    const char *p = *s;
    if (p[0] == '\\' && p[1]) {
        if (p[1] == 's' && p[2]) {
            *ch = (unsigned char)p[2];
            *shift = 1;
            *s = p + 3;
            return 0;
        }
        if (p[1] == 'b') {
            *ch = 0x08;
            *shift = 0;
            *s = p + 2;
            return 0;
        }
        if (p[1] == '\\') {
            *ch = (unsigned char)'\\';
            *shift = 0;
            *s = p + 2;
            return 0;
        }
        fprintf(stderr,
                "%s: unknown key escape \\%c (\\\\ = literal, "
                "\\s<char> = shift, \\b = backspace)\n",
                what, p[1]);
        return -1;
    }
    *ch = (unsigned char)p[0];
    *shift = 0;
    *s = p + 1;
    return 0;
}

static int check_file(hunmin_ctx *ctx, const char *path, const char *force_layout) {
    char *txt = read_file(path, NULL);
    if (!txt) return 2;
    char *save = NULL;
    int line = 0, fails = 0;
    hunmin_ic *ic = NULL;

    for (char *s = strtok(txt, "\n"); s; s = strtok(NULL, "\n")) {
        line++;
        if (!*s) continue;
        cJSON *j = cJSON_Parse(s);
        if (!j) {
            fprintf(stderr, "%s:%d: bad JSON\n", path, line);
            fails++;
            continue;
        }
        const cJSON *meta = cJSON_GetObjectItemCaseSensitive(j, "meta");
        if (meta) {
            const cJSON *ca = cJSON_GetObjectItemCaseSensitive(meta, "caps");
            static const char *ovrs[] = {"w", "W", "z", "h", "H"};
            if (ic) {
                hunmin_ic_reset(ic); /* multi-case goldens: reset between cases */
                if (cJSON_IsNumber(ca)) hunmin_ic_set_option(ic, "caps", ca->valueint);
                for (int k = 0; k < 5; k++) {
                    const cJSON *cv = cJSON_GetObjectItemCaseSensitive(meta, ovrs[k]);
                    if (cJSON_IsNumber(cv)) hunmin_ic_set_option(ic, ovrs[k], cv->valueint);
                }
            }
            if (!ic) {
                const cJSON *lay = cJSON_GetObjectItemCaseSensitive(meta, "layout");
                const char *use = force_layout;
                if (!use && cJSON_IsString(lay) && lay->valuestring[0]) use = lay->valuestring;
                if (use) {
                    ic = hunmin_ic_new(ctx, use);
                    const cJSON *caps = cJSON_GetObjectItemCaseSensitive(meta, "caps");
                    if (ic && cJSON_IsNumber(caps)) hunmin_ic_set_option(ic, "caps", caps->valueint);
                    if (ic)
                        for (int k = 0; k < 5; k++) {
                            const cJSON *cv = cJSON_GetObjectItemCaseSensitive(meta, ovrs[k]);
                            if (cJSON_IsNumber(cv)) hunmin_ic_set_option(ic, ovrs[k], cv->valueint);
                        }
                }
                if (!use || !ic) {
                    fprintf(stderr, "%s: cannot resolve layout from meta/--layout\n", path);
                    cJSON_Delete(j);
                    fails++;
                    break;
                }
            }
            cJSON_Delete(j);
            continue;
        }
        const cJSON *in = cJSON_GetObjectItemCaseSensitive(j, "in");
        const cJSON *wcommit = cJSON_GetObjectItemCaseSensitive(j, "commit");
        const cJSON *wpre = cJSON_GetObjectItemCaseSensitive(j, "preedit");
        if (!ic) {
            fprintf(stderr, "%s: missing meta.layout (line 1) or --layout\n", path);
            cJSON_Delete(j);
            fails++;
            break;
        }
        uint32_t ch = 0x08; /* default backspace */
        const cJSON *jc = cJSON_GetObjectItemCaseSensitive(in, "ch");
        const cJSON *jk = cJSON_GetObjectItemCaseSensitive(in, "key");
        if (cJSON_IsString(jc) && jc->valuestring[0]) ch = (unsigned char)jc->valuestring[0];
        else if (cJSON_IsString(jk) && !strcmp(jk->valuestring, "backspace")) ch = 0x08;
        else {
            fprintf(stderr, "%s:%d: bad in\n", path, line);
            cJSON_Delete(j);
            fails++;
            continue;
        }

        hunmin_ic_process(ic, ch);
        const char *ac = hunmin_ic_commit(ic), *ap = hunmin_ic_preedit(ic);
        const cJSON *wtotal = cJSON_GetObjectItemCaseSensitive(j, "total");
        if (cJSON_IsString(wtotal)) { /* oracle mode: assert the rendered result of
                                       * this step: composing buffer, else the commit
                                       * (patim getComposed() semantics, canonicalized:
                                       * NFC syllables == jamo sequences here) */
            const char *whole = ap[0] ? ap : ac;
            char a_c[1200], b_c[1200];
            hangul_canon(whole, a_c, sizeof a_c);
            hangul_canon(wtotal->valuestring, b_c, sizeof b_c);
            if (strcmp(a_c, b_c)) {
                fprintf(stderr, "%s:%d: TOTAL FAIL in=%s expected=%s got=%s\n", path, line, jc && cJSON_IsString(jc) ? jc->valuestring : "bksp", b_c,
                        a_c);
                fails++;
                if (fails > 8) {
                    cJSON_Delete(j);
                    break;
                }
            }
            cJSON_Delete(j);
            continue;
        }
        const char *ec = cJSON_IsString(wcommit) ? wcommit->valuestring : "";
        const char *ep = cJSON_IsString(wpre) ? wpre->valuestring : "";
        if (strcmp(ac, ec) || strcmp(ap, ep)) {
            fprintf(stderr, "%s:%d: MISMATCH in=%s expected commit='%s' preedit='%s' got commit='%s' preedit='%s'\n", path, line,
                    jc && cJSON_IsString(jc) ? jc->valuestring : "backspace", ec, ep, ac, ap);
            fails++;
            if (fails > 5) {
                cJSON_Delete(j);
                break;
            }
        }
        cJSON_Delete(j);
    }
    if (ic) hunmin_ic_free(ic);
    free(txt);
    if (!fails) printf("OK  %s (%d events)\n", path, line - 1);
    return fails ? 1 : 0;
}

/* --------------------------------------------------------------- judgement
 *
 * SPEC-MODES R8 at the shared layer. The engine judges and the frontend
 * performs the switch (ruled 2026-11-01, #164), so a replay has to do what an
 * adapter does or it would report a judgement nobody acts on: after every key,
 * read take_judgement(), and when it answers a language, act on it. What is
 * observable from outside and what a golden case pins is therefore exactly
 * three facts — the text delivered, the judgements taken (in order), and the
 * mode the layer is left in. The preedit is deliberately not part of it: E6
 * makes the display an option, and the corpus may not depend on which one.
 */
static const char *lang_name(int v) {
    switch (v) {
    case HUNMIN_LANGUAGE_HANGUL:
        return "hangul";
    case HUNMIN_LANGUAGE_LATIN:
        return "latin";
    default:
        return "";
    }
}

typedef struct judge_case {
    hunmin_buffered_preedit *l;
    char delivered[4096];
    char judged[256]; /* language names, comma-joined, in the order taken */
    /* The last key's own answer, for --trace. Copied because the layer owns
     * both strings only until its next call, and the switch is another call. */
    char key_commit[1024];
    char key_preedit[1024];
} judge_case;

static void str_append(char *dst, size_t cap, const char *s) {
    size_t n = strlen(dst), m = s ? strlen(s) : 0;
    if (n + m >= cap) return; /* a case long enough to hit this is not a case */
    memcpy(dst + n, s ? s : "", m + 1);
}

static void copy(char *dst, size_t cap, const char *s) {
    if (!s) s = "";
    snprintf(dst, cap, "%s", s);
}

/* One key, and the frontend's answer to it. Returns the consumption decision. */
static int judge_step(judge_case *c, uint32_t base, uint32_t shift, int *judged_out) {
    hunmin_key_result out = {0, NULL, NULL};
    const int consumed = hunmin_buffered_preedit_process_key(c->l, base, shift, &out);
    copy(c->key_commit, sizeof c->key_commit, out.commit);
    copy(c->key_preedit, sizeof c->key_preedit, out.preedit);
    str_append(c->delivered, sizeof c->delivered, out.commit);
    const int mode = hunmin_buffered_preedit_take_judgement(c->l);
    if (judged_out) *judged_out = mode;
    if (mode >= 0) {
        const char *name = lang_name(mode);
        if (c->judged[0]) str_append(c->judged, sizeof c->judged, ",");
        str_append(c->judged, sizeof c->judged, name);
        hunmin_key_result sw = {0, NULL, NULL};
        hunmin_buffered_preedit_toggle_language(c->l, &sw);
        str_append(c->delivered, sizeof c->delivered, sw.commit);
    }
    return consumed;
}

/* A layer with the two languages the judgement needs, the shipped english list
 * (B3: absent data answers "not ready", and then nothing switches — which is
 * why --judge reports that state instead of pretending), and the ruling's
 * default OFF overridden only when the caller asked for a value. */
static hunmin_buffered_preedit *judge_layer(hunmin_ctx *ctx, const char *layout, const char *latin,
                                           int autoswitch, int start_latin) {
    hunmin_buffered_preedit *l = hunmin_buffered_preedit_new(
        ctx, layout, (latin && latin[0]) ? latin : NULL, HUNMIN_COMMIT_AT_BOUNDARY);
    if (!l) return NULL;
    hunmin_buffered_preedit_load_english_shipped(l);
    if (autoswitch >= 0) hunmin_buffered_preedit_set_option(l, "autoswitch", autoswitch ? 1 : 0);
    if (start_latin) {
        hunmin_key_result out = {0, NULL, NULL};
        hunmin_buffered_preedit_toggle_language(l, &out);
    }
    return l;
}

/* tests/golden/judgement-*.jsonl: one case per line.
 *
 *   {"meta": ...}                                  provenance, ignored
 *   {"case":"hello","req":"R8","layout":"dubeolsik","latin":"us-qwerty",
 *    "autoswitch":1,"mode":"hangul","keys":"hello ",
 *    "commit":"hello","judged":["latin"],"mode_after":"latin"}
 *
 * The first four fields are the INPUT a person is replaying; the last three are
 * what the engine answered and are what the checker asserts. `mode` is the mode
 * the case starts in (R8's symmetric line needs latin), `judged` is the
 * judgement taken at each boundary in order — empty when nothing switched,
 * which is R8's "keep the current mode" — and `mode_after` is the mode the
 * layer is left in. Comparing the last two as well as the first is what stops
 * a corpus from asserting a switch no adapter would perform.
 */
static int check_judge_file(hunmin_ctx *ctx, const char *path, const char *force_layout,
                            const char *force_latin, int force_autoswitch) {
    char *txt = read_file(path, NULL);
    if (!txt) return 2;
    int line = 0, fails = 0;

    for (char *s = strtok(txt, "\n"); s; s = strtok(NULL, "\n")) {
        line++;
        if (!*s) continue;
        cJSON *j = cJSON_Parse(s);
        if (!j) {
            fprintf(stderr, "%s:%d: bad JSON\n", path, line);
            fails++;
            continue;
        }
        if (cJSON_GetObjectItemCaseSensitive(j, "meta")) {
            cJSON_Delete(j);
            continue;
        }
        const cJSON *jc = cJSON_GetObjectItemCaseSensitive(j, "case");
        const cJSON *jl = cJSON_GetObjectItemCaseSensitive(j, "layout");
        const cJSON *jlat = cJSON_GetObjectItemCaseSensitive(j, "latin");
        const cJSON *jas = cJSON_GetObjectItemCaseSensitive(j, "autoswitch");
        const cJSON *jmode = cJSON_GetObjectItemCaseSensitive(j, "mode");
        const cJSON *jkeys = cJSON_GetObjectItemCaseSensitive(j, "keys");
        const char *name = cJSON_IsString(jc) ? jc->valuestring : "?";
        const char *layout = force_layout ? force_layout : (cJSON_IsString(jl) ? jl->valuestring : NULL);
        const char *latin = force_latin ? force_latin : (cJSON_IsString(jlat) ? jlat->valuestring : "us-qwerty");
        const int autoswitch = force_autoswitch >= 0 ? force_autoswitch : (cJSON_IsNumber(jas) ? jas->valueint : 1);
        const int start_latin = cJSON_IsString(jmode) && !strcmp(jmode->valuestring, "latin");
        if (!layout || !cJSON_IsString(jkeys)) {
            fprintf(stderr, "%s:%d: case '%s' has no layout or keys\n", path, line, name);
            cJSON_Delete(j);
            fails++;
            continue;
        }

        judge_case c;
        memset(&c, 0, sizeof c);
        c.l = judge_layer(ctx, layout, latin, autoswitch, start_latin);
        if (!c.l) {
            fprintf(stderr, "%s:%d: case '%s': cannot build a layer for %s/%s\n", path, line, name, layout, latin);
            cJSON_Delete(j);
            fails++;
            continue;
        }
        const cJSON *wsteps = cJSON_GetObjectItemCaseSensitive(j, "step_commits");
        int si = 0;
        for (const char *kp = jkeys->valuestring; *kp;) {
            uint32_t key = 0, shift = 0;
            if (keys_step(&kp, &key, &shift, "--judge-check")) {
                fails++;
                break;
            }
            judge_step(&c, key, shift, NULL);
            /* Delivery TIMING, per key, when a case needs it. R6 is the reason
             * this exists: a held digit run and a per-key-committed one deliver
             * the same final text, so the final text cannot tell them apart. */
            if (cJSON_IsArray(wsteps)) {
                const cJSON *want = cJSON_GetArrayItem(wsteps, si);
                if (!cJSON_IsString(want) || strcmp(want->valuestring, c.key_commit)) {
                    fprintf(stderr, "%s:%d: CASE FAIL '%s' step %d commit: expected '%s' got '%s'\n", path, line,
                            name, si, cJSON_IsString(want) ? want->valuestring : "(missing)", c.key_commit);
                    fails++;
                }
            }
            si++;
        }
        if (cJSON_IsArray(wsteps) && cJSON_GetArraySize(wsteps) != si) {
            fprintf(stderr, "%s:%d: CASE FAIL '%s': step_commits has %d entries for %d keys\n", path, line, name,
                    cJSON_GetArraySize(wsteps), si);
            fails++;
        }

        const cJSON *wcommit = cJSON_GetObjectItemCaseSensitive(j, "commit");
        const cJSON *wmode = cJSON_GetObjectItemCaseSensitive(j, "mode_after");
        const cJSON *wjudged = cJSON_GetObjectItemCaseSensitive(j, "judged");
        const char *ec = cJSON_IsString(wcommit) ? wcommit->valuestring : "";
        const char *em = cJSON_IsString(wmode) ? wmode->valuestring : "";
        char ej[256] = "";
        if (cJSON_IsArray(wjudged)) {
            const cJSON *e;
            cJSON_ArrayForEach(e, wjudged) {
                if (!cJSON_IsString(e)) continue;
                if (ej[0]) str_append(ej, sizeof ej, ",");
                str_append(ej, sizeof ej, e->valuestring);
            }
        }
        const char *got_mode = lang_name(hunmin_buffered_preedit_language(c.l));
        if (strcmp(c.delivered, ec) || strcmp(c.judged, ej) || strcmp(got_mode, em)) {
            fprintf(stderr,
                    "%s:%d: CASE FAIL '%s' (req %s) keys='%s'\n"
                    "    expected commit='%s' judged=[%s] mode_after='%s'\n"
                    "    got      commit='%s' judged=[%s] mode_after='%s'\n",
                    path, line, name,
                    cJSON_IsString(cJSON_GetObjectItemCaseSensitive(j, "req"))
                        ? cJSON_GetObjectItemCaseSensitive(j, "req")->valuestring
                        : "",
                    jkeys->valuestring, ec, ej, em, c.delivered, c.judged, got_mode);
            fails++;
        }
        hunmin_buffered_preedit_free(c.l);
        cJSON_Delete(j);
        if (fails > 8) break;
    }
    free(txt);
    if (!fails) printf("OK  %s (%d cases)\n", path, line - 1);
    return fails ? 1 : 0;
}

/* ------------------------------------------------- OFF identity (SPEC-MODES §4)
 *
 * docs/SPEC-MODES.md §4 states the invariant as a requirement rather than a
 * wish: "When it is off it must return to 100% of the previous behaviour. This
 * is an invariant. It is verifiable and must be verified." (issue #179)
 *
 * The oracle is the tier-A corpus itself: those expectations were recorded
 * before the judgement existed and --check still asserts them against
 * hunmin_ic, the engine the frontends sat on then. What this mode adds is the
 * SAME key sequences driven one level up, through hunmin_buffered_preedit --
 * the layer every frontend now sits on -- with `autoswitch` explicitly OFF and
 * the rest of the feature PRESENT (the Latin layout, the shipped English list,
 * R8's reading replayed at every boundary), so the option is the only thing
 * standing between the layer and a different delivery.
 *
 * What is compared, per key, byte for byte:
 *   - the text the client holds: everything delivered so far plus the preedit
 *     on screen. The layer legitimately moves text from the preedit into a
 *     later commit (HUNMIN_COMMIT_AT_BOUNDARY), so comparing this key's commit
 *     against the engine's would pin a delivery timing that is a frontend
 *     setting; the text a person sees is the invariant. Under
 *     HUNMIN_COMMIT_PER_SYLLABLE the per-key commit IS compared, because that
 *     policy promises the engine's own timing.
 *   - the consumed decision, because a key the engine declines is forwarded to
 *     the client and a key it consumes is not — that is the frontends' job;
 * and once at the end of a case, after both sides flush, the delivered totals.
 *
 * The same mode replays the x11 tier-C traces, tier C's inputs in
 * the platform's own dialect (keysym + mods), so the corpus that exists to make
 * the frontends agree also proves the layer does not move their text.
 *
 * What it does NOT cover: a frontend's own presentation (SPEC-MODES §3), the
 * Android/win32 trace dialects, and the client's handling of a declined key
 * (a space is inserted by the client; neither driver models that, and both are
 * declined identically, which the consumed comparison asserts).
 */
#define OFF_TEXT_CAP 24576
/* fcitx5's default LatinLayout, and the layout the frontends replay readings
 * through — the OFF path must be measured with the same second language the
 * ON path would use, or the reading is not even attempted. */
#define OFF_LATIN_LAYOUT "us-qwerty"
/* X11 modifier bits, the x11 trace dialect (docs/trace-spec.md). */
#define OFF_X11_SHIFT 0x01
#define OFF_X11_LOCK 0x04

typedef struct off_case {
    hunmin_ic *ic;                       /* the pre-feature engine: the oracle */
    hunmin_buffered_preedit *boundary;    /* the layer at HUNMIN_COMMIT_AT_BOUNDARY */
    hunmin_buffered_preedit *syllable;    /* the layer at HUNMIN_COMMIT_PER_SYLLABLE */
    char odel[OFF_TEXT_CAP];             /* delivered text, cumulative */
    char bdel[OFF_TEXT_CAP];
    char sdel[OFF_TEXT_CAP];
} off_case;

/* Appends or fails: a silently truncated accumulator would compare a shorter
 * text than the corpus drove, which is a check that shrinks instead of failing. */
static int off_append(char *dst, size_t cap, const char *s) {
    const size_t n = strlen(dst), m = s ? strlen(s) : 0;
    if (n + m >= cap) return -1;
    if (m) memcpy(dst + n, s, m);
    dst[n + m] = 0;
    return 0;
}

/* Concatenates `a` then `b` into dst, or answers -1 rather than truncating: two
 * halves that were each truncated the same way would compare equal while neither
 * is the text the corpus drove. */
static int off_join(char *dst, size_t cap, const char *a, const char *b) {
    const size_t na = a ? strlen(a) : 0, nb = b ? strlen(b) : 0;
    if (na + nb >= cap) return -1;
    if (na) memcpy(dst, a, na);
    if (nb) memcpy(dst + na, b, nb);
    dst[na + nb] = 0;
    return 0;
}

static void off_free(off_case *c) {
    if (c->ic) hunmin_ic_free(c->ic);
    if (c->boundary) hunmin_buffered_preedit_free(c->boundary);
    if (c->syllable) hunmin_buffered_preedit_free(c->syllable);
    memset(c, 0, sizeof *c);
}

/* The layer as a frontend builds it for the OFF path: both languages, the
 * shipped English list loaded, and `autoswitch` written to 0 AND read back. A
 * write the engine refused would leave the option ON, and then this whole check
 * would be measuring the ON path and calling it identity. */
static hunmin_buffered_preedit *off_layer(hunmin_ctx *ctx, const char *layout, int policy, int *held) {
    hunmin_buffered_preedit *l =
        hunmin_buffered_preedit_new(ctx, layout, OFF_LATIN_LAYOUT, policy);
    if (!l) return NULL;
    hunmin_buffered_preedit_load_english_shipped(l);
    hunmin_buffered_preedit_set_option(l, "autoswitch", 0);
    *held = -1;
    hunmin_buffered_preedit_get_option(l, "autoswitch", held);
    return l;
}

/* Every state a golden's meta line can set, applied to all three drivers so the
 * only difference between them is the level they sit at. -1 means "unchanged"
 * for caps; a flag override of -1 is the layout's default and is applied. */
static void off_apply(off_case *c, int caps, int *flags) {
    if (caps >= 0) {
        hunmin_ic_set_option(c->ic, "caps", caps);
        hunmin_buffered_preedit_set_option(c->boundary, "caps", caps);
        hunmin_buffered_preedit_set_option(c->syllable, "caps", caps);
    }
    if (!flags) return;
    static const char *names[] = {"w", "W", "z", "h", "H"};
    for (int k = 0; k < 5; k++) {
        if (flags[k] == -2) continue; /* absent from this case's meta */
        hunmin_ic_set_option(c->ic, names[k], flags[k]);
        hunmin_buffered_preedit_set_option(c->boundary, names[k], flags[k]);
        hunmin_buffered_preedit_set_option(c->syllable, names[k], flags[k]);
    }
}

/* Starts a case. The drivers are REBUILT only when the layout changes, and RESET
 * otherwise: a layer carries the shipped English list, and re-reading that file
 * for every case of a multi-case golden was measured at ~40 ms per case (the
 * whole check took 15 s). Reset returns the same text state -- composition,
 * word, keys, delivered bytes -- and leaves the option state in place. */
static int off_begin(off_case *c, char *held_layout, size_t held_cap, hunmin_ctx *ctx, const char *path,
                     int line, const char *layout, int caps, int *flags, int *ready_out) {
    const int same = held_layout[0] && !strcmp(held_layout, layout);
    if (same) {
        c->odel[0] = c->bdel[0] = c->sdel[0] = 0;
        hunmin_ic_reset(c->ic);
        hunmin_buffered_preedit_reset(c->boundary);
        hunmin_buffered_preedit_reset(c->syllable);
        off_apply(c, caps, flags);
        *ready_out = hunmin_buffered_preedit_english_ready(c->boundary);
        return 0;
    }
    off_free(c);
    c->ic = hunmin_ic_new(ctx, layout);
    int hb = -1, hs = -1;
    c->boundary = off_layer(ctx, layout, HUNMIN_COMMIT_AT_BOUNDARY, &hb);
    c->syllable = off_layer(ctx, layout, HUNMIN_COMMIT_PER_SYLLABLE, &hs);
    if (!c->ic || !c->boundary || !c->syllable) {
        fprintf(stderr, "%s:%d: cannot build the drivers for layout '%s' (a layer for %s)\n",
                path, line, layout, OFF_LATIN_LAYOUT);
        off_free(c);
        return -1;
    }
    if (hb != 0 || hs != 0) {
        fprintf(stderr,
                "%s:%d: 'autoswitch' reads back %d/%d after being written 0 -- the OFF path "
                "cannot be measured while the option refuses the value\n",
                path, line, hb, hs);
        off_free(c);
        return -1;
    }
    snprintf(held_layout, held_cap, "%s", layout);
    *ready_out = hunmin_buffered_preedit_english_ready(c->boundary);
    off_apply(c, caps, flags);
    return 0;
}

/* One key through all three drivers, and the comparisons §4 asks for. */
static int off_key(off_case *c, const char *path, int line, uint32_t base, uint32_t shift,
                   int caps, long *checks) {
    int fails = 0;
    /* A case that could not be built must fail here, not compare two empty
     * texts and report identity. */
    if (!c->ic || !c->boundary || !c->syllable) {
        fprintf(stderr, "%s:%d: no drivers for this key -- the case did not build\n", path, line);
        return 1;
    }
    if (caps >= 0) off_apply(c, caps, NULL);

    const int ocons = hunmin_ic_process_key(c->ic, base, shift) ? 1 : 0;
    /* The engine's commit read is destructive and its string is the engine's
     * own buffer: copy before anything else can rewrite it. */
    const char *oc = hunmin_ic_commit(c->ic);
    char ocopy[1200];
    snprintf(ocopy, sizeof ocopy, "%s", oc ? oc : "");
    char opcopy[1200];
    snprintf(opcopy, sizeof opcopy, "%s", hunmin_ic_preedit(c->ic));

    hunmin_key_result bres = {0, NULL, NULL}, sres = {0, NULL, NULL};
    const int bcons = hunmin_buffered_preedit_process_key(c->boundary, base, shift, &bres) ? 1 : 0;
    char bcommit[1200], bpre[OFF_TEXT_CAP];
    snprintf(bcommit, sizeof bcommit, "%s", bres.commit ? bres.commit : "");
    snprintf(bpre, sizeof bpre, "%s", bres.preedit ? bres.preedit : "");
    const int scons = hunmin_buffered_preedit_process_key(c->syllable, base, shift, &sres) ? 1 : 0;
    char scommit[1200], spre[OFF_TEXT_CAP];
    snprintf(scommit, sizeof scommit, "%s", sres.commit ? sres.commit : "");
    snprintf(spre, sizeof spre, "%s", sres.preedit ? sres.preedit : "");

    (*checks)++;
    if (ocons != bcons || ocons != scons)
        fprintf(stderr, "%s:%d: key 0x%02x: consumed engine=%d boundary=%d syllable=%d\n", path, line,
                base, ocons, bcons, scons);
    fails += (ocons != bcons) + (ocons != scons);

    if (off_append(c->odel, sizeof c->odel, ocopy) || off_append(c->bdel, sizeof c->bdel, bcommit) ||
        off_append(c->sdel, sizeof c->sdel, scommit)) {
        fprintf(stderr, "%s:%d: accumulated text passed %d bytes -- raise OFF_TEXT_CAP\n", path, line,
                OFF_TEXT_CAP);
        return fails + 1;
    }

    /* The visible text: what the client already holds plus what is composing. */
    char ovis[2 * OFF_TEXT_CAP], bvis[2 * OFF_TEXT_CAP], svis[2 * OFF_TEXT_CAP];
    if (off_join(ovis, sizeof ovis, c->odel, opcopy) || off_join(bvis, sizeof bvis, c->bdel, bpre) ||
        off_join(svis, sizeof svis, c->sdel, spre)) {
        fprintf(stderr, "%s:%d: visible text passed %d bytes -- raise OFF_TEXT_CAP\n", path, line,
                2 * OFF_TEXT_CAP);
        return fails + 1;
    }
    (*checks)++;
    if (strcmp(ovis, bvis)) {
        fprintf(stderr,
                "%s:%d: OFF text differs at the boundary policy, key 0x%02x\n"
                "    engine   ='%s'\n"
                "    layer    ='%s'\n",
                path, line, base, ovis, bvis);
        fails++;
    }
    (*checks)++;
    if (strcmp(ovis, svis)) {
        fprintf(stderr,
                "%s:%d: OFF text differs at the per-syllable policy, key 0x%02x\n"
                "    engine   ='%s'\n"
                "    layer    ='%s'\n",
                path, line, base, ovis, svis);
        fails++;
    }
    /* PER_SYLLABLE promises the engine's own delivery timing, so the per-key
     * commit is part of the contract there and not merely a setting. */
    (*checks)++;
    if (strcmp(ocopy, scommit)) {
        fprintf(stderr, "%s:%d: per-syllable commit differs at key 0x%02x: engine='%s' layer='%s'\n",
                path, line, base, ocopy, scommit);
        fails++;
    }
    return fails;
}

/* The end of a case: both sides flush what is still composing, and the totals
 * must still agree. A case whose text only agrees while it is mid-composition
 * has moved the last word somewhere rather than delivering it. */
static int off_flush(off_case *c, const char *path, int line, long *checks) {
    int fails = 0;
    const char *oc = hunmin_ic_flush(c->ic);
    char ocopy[1200];
    snprintf(ocopy, sizeof ocopy, "%s", oc ? oc : "");
    const char *bc = hunmin_buffered_preedit_commit_word(c->boundary);
    char bcopy[OFF_TEXT_CAP];
    snprintf(bcopy, sizeof bcopy, "%s", bc ? bc : "");
    const char *sc = hunmin_buffered_preedit_commit_word(c->syllable);
    char scopy[OFF_TEXT_CAP];
    snprintf(scopy, sizeof scopy, "%s", sc ? sc : "");
    if (off_append(c->odel, sizeof c->odel, ocopy) || off_append(c->bdel, sizeof c->bdel, bcopy) ||
        off_append(c->sdel, sizeof c->sdel, scopy)) {
        fprintf(stderr, "%s:%d: accumulated text passed %d bytes at the flush\n", path, line, OFF_TEXT_CAP);
        return fails + 1;
    }
    (*checks)++;
    if (strcmp(c->odel, c->bdel) || strcmp(c->odel, c->sdel)) {
        fprintf(stderr,
                "%s:%d: OFF delivered totals differ after the flush\n"
                "    engine           ='%s'\n"
                "    boundary policy  ='%s'\n"
                "    syllable policy  ='%s'\n",
                path, line, c->odel, c->bdel, c->sdel);
        fails++;
    }
    return fails;
}

/* One golden (.jsonl under tests/golden) or one trace (x11 .trace.in)
 * through the three drivers. Both formats carry their inputs in an `in` object and
 * a `meta` line that starts a case; only the key's spelling differs. */
static int check_off_files(hunmin_ctx *ctx, const char *const *paths, int npaths) {
    int fails = 0, files_ok = 0;
    long checks = 0, keys = 0;
    /* Across files: the layout the drivers were built for, so a corpus that
     * walks many cases of one layout pays for the English list once. */
    off_case c;
    memset(&c, 0, sizeof c);
    char held_layout[128] = "";

    for (int pi = 0; pi < npaths; pi++) {
        const char *path = paths[pi];
        char *txt = read_file(path, NULL);
        if (!txt) {
            fails++;
            continue;
        }
        int line = 0, file_fails = 0, cases = 0, started = 0, ready = 0, warned = 0;
        long file_keys = 0, file_checks = 0;
        /* A multi-case golden puts the layout in its FIRST meta line and only the
         * case name in the rest, so both the layout and every option state carry
         * forward until a meta line changes them -- the same inheritance
         * check_file() gets from keeping one ic across the reset. */
        char cur_layout[128] = "";
        int cur_caps = -1, cur_flags[5] = {-2, -2, -2, -2, -2};
        static const char *fnames[] = {"w", "W", "z", "h", "H"};
        /* hunmin_ic_reset() does not clear caps, so a driver carried over from
         * the previous file still holds that file's last state -- one trace's
         * locked record would capitalise the next file's cases. */
        if (held_layout[0]) {
            hunmin_ic_set_option(c.ic, "caps", 0);
            hunmin_buffered_preedit_set_option(c.boundary, "caps", 0);
            hunmin_buffered_preedit_set_option(c.syllable, "caps", 0);
        }
        for (char *s = strtok(txt, "\n"); s; s = strtok(NULL, "\n")) {
            line++;
            if (!*s) continue;
            cJSON *j = cJSON_Parse(s);
            if (!j) {
                fprintf(stderr, "%s:%d: bad JSON\n", path, line);
                file_fails++;
                continue;
            }
            const cJSON *meta = cJSON_GetObjectItemCaseSensitive(j, "meta");
            if (meta) {
                /* A meta line starts a case (multi-case goldens); the trace
                 * corpus repeats it once per file. */
                if (started && off_flush(&c, path, line, &file_checks)) file_fails++;
                const cJSON *lay = cJSON_GetObjectItemCaseSensitive(meta, "layout");
                if (cJSON_IsString(lay) && lay->valuestring[0])
                    snprintf(cur_layout, sizeof cur_layout, "%s", lay->valuestring);
                if (!cur_layout[0]) {
                    fprintf(stderr, "%s:%d: meta has no layout and no earlier one to inherit\n", path,
                            line);
                    file_fails++;
                    cJSON_Delete(j);
                    continue;
                }
                const cJSON *jcaps = cJSON_GetObjectItemCaseSensitive(meta, "caps");
                if (cJSON_IsNumber(jcaps)) cur_caps = jcaps->valueint;
                for (int k = 0; k < 5; k++) {
                    const cJSON *fv = cJSON_GetObjectItemCaseSensitive(meta, fnames[k]);
                    /* The reset below clears every override, and check_file()'s
                     * reset does the same, so an absent flag means the layout's
                     * default -- not the previous case's value. */
                    cur_flags[k] = cJSON_IsNumber(fv) ? fv->valueint : -2;
                }
                if (off_begin(&c, held_layout, sizeof held_layout, ctx, path, line, cur_layout,
                              cur_caps, cur_flags, &ready)) {
                    file_fails++;
                    started = 0;        /* the drivers are gone: the next keys are unattributable */
                    held_layout[0] = 0; /* and nothing may reuse them */
                    cJSON_Delete(j);
                    continue;
                }
                started = 1;
                cases++;
                /* B3 ships the English list with the data; without it
                 * `english_valid` cannot answer and the judgement would abstain
                 * for a reason that is not the option. Say the state once per
                 * file rather than per case. */
                if (ready != 1 && !warned) {
                    fprintf(stderr,
                            "note: %s: the shipped English list is not loaded (%d) -- the OFF path "
                            "is still compared byte for byte, but no word could have been judged\n",
                            path, ready);
                    warned = 1;
                }
                cJSON_Delete(j);
                continue;
            }
            if (!started) {
                fprintf(stderr, "%s:%d: key record before any meta line\n", path, line);
                file_fails++;
                cJSON_Delete(j);
                continue;
            }
            const cJSON *in = cJSON_GetObjectItemCaseSensitive(j, "in");
            if (!cJSON_IsObject(in)) {
                fprintf(stderr, "%s:%d: no in object\n", path, line);
                file_fails++;
                cJSON_Delete(j);
                continue;
            }
            uint32_t base = 0, shift = 0;
            int caps = -1;
            const cJSON *jch = cJSON_GetObjectItemCaseSensitive(in, "ch");
            const cJSON *jkey = cJSON_GetObjectItemCaseSensitive(in, "key");
            const cJSON *jks = cJSON_GetObjectItemCaseSensitive(in, "keysym");
            const cJSON *jkdc = cJSON_GetObjectItemCaseSensitive(in, "keycode");
            const cJSON *jmods = cJSON_GetObjectItemCaseSensitive(in, "mods");
            const unsigned mods = cJSON_IsNumber(jmods) ? (unsigned)jmods->valueint : 0u;
            if (cJSON_IsString(jch) && jch->valuestring[0]) {
                base = (unsigned char)jch->valuestring[0];
            } else if (cJSON_IsString(jkey) && !strcmp(jkey->valuestring, "backspace")) {
                base = 0x08;
            } else if (cJSON_IsNumber(jks)) {
                base = (uint32_t)jks->valueint;
                if (mods & OFF_X11_SHIFT) shift = 1;
                /* LockMask is the state at delivery, so it follows every record
                 * both ways -- a meta-level caps would let one locked key leave
                 * the rest of the case capitalised. */
                caps = (mods & OFF_X11_LOCK) ? 1 : 0;
            } else if (cJSON_IsNumber(jkdc)) {
                /* The Android dialect spells keys as KEYCODE_*, which is ASCII
                 * for the printable range this corpus uses; the named keys are
                 * not (backspace is 67), so it is not driven here. */
                fprintf(stderr, "%s:%d: android keycode records are not driven by --off-check\n", path,
                        line);
                file_fails++;
                cJSON_Delete(j);
                continue;
            } else {
                fprintf(stderr, "%s:%d: unrecognised key record\n", path, line);
                file_fails++;
                cJSON_Delete(j);
                continue;
            }
            file_fails += off_key(&c, path, line, base, shift, caps, &file_checks);
            file_keys++;
            cJSON_Delete(j);
            if (file_fails > 8) break;
        }
        if (started && off_flush(&c, path, line, &file_checks)) file_fails++;
        free(txt);
        keys += file_keys;
        checks += file_checks;
        fails += file_fails;
        if (!file_fails) {
            printf("OK  %s (%d cases, %ld keys)\n", path, cases, file_keys);
            files_ok++;
        }
        if (fails > 40) {
            fprintf(stderr, "stopping: more than 40 failing comparisons\n");
            break;
        }
    }
    off_free(&c);
    printf("OFF identity: %d/%d files, %ld keys, %ld comparisons (autoswitch=0, "
           "%s at both commit policies)\n",
           files_ok, npaths, keys, checks, OFF_LATIN_LAYOUT);
    return fails ? 1 : 0;
}

int main(int argc, char **argv) {
    const char *data = NULL, *layout = NULL, *keys = NULL, *checkp = NULL;
    const char *judgecheck = NULL, *latin = NULL;
    const char *offcheck[256];
    int noff = 0;
    int trace = 0, caps = 0, batch = 0, judge = 0, start_latin = 0, autoswitch = -1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--data") && ++i < argc) data = argv[i];
        else if (!strcmp(argv[i], "--layout") && ++i < argc) layout = argv[i];
        else if (!strcmp(argv[i], "--latin") && ++i < argc) latin = argv[i];
        else if (!strcmp(argv[i], "--keys") && ++i < argc) keys = argv[i];
        else if (!strcmp(argv[i], "--check") && ++i < argc) checkp = argv[i];
        else if (!strcmp(argv[i], "--judge-check") && ++i < argc) judgecheck = argv[i];
        else if (!strcmp(argv[i], "--off-check")) {
            while (i + 1 < argc && strncmp(argv[i + 1], "--", 2) && noff < 256) offcheck[noff++] = argv[++i];
        }
        else if (!strcmp(argv[i], "--autoswitch") && ++i < argc) autoswitch = atoi(argv[i]);
        else if (!strcmp(argv[i], "--caps")) caps = 1;
        else if (!strcmp(argv[i], "--judge")) judge = 1;
        else if (!strcmp(argv[i], "--start-latin")) start_latin = 1;
        else if (!strcmp(argv[i], "--trace")) trace = 1;
        else if (!strcmp(argv[i], "--batch")) batch = 1;
    }
    if (!data) {
        fprintf(stderr, "usage: hunmin-sim --data DIR (--layout ID --keys STR | --batch | --check FILE"
                        " | --judge-check FILE | --off-check FILE... "
                        "| --layout ID --latin LATIN --judge --keys STR)\n");
        return 2;
    }
    const char *dirs[1] = {data};
    hunmin_ctx *ctx = hunmin_ctx_new(dirs, 1);
    if (!ctx) {
        fprintf(stderr, "no layouts loaded from %s\n", data);
        return 2;
    }

    if (judgecheck) return check_judge_file(ctx, judgecheck, layout, latin, autoswitch);
    if (noff) return check_off_files(ctx, offcheck, noff);
    if (checkp) return check_file(ctx, checkp, layout);

    if (batch) {
        /* One keystroke string per input line; print the rendered result as JSON.
         *
         * A keystroke search needs hundreds of thousands of engine evaluations,
         * and one process spawn per keystroke costs ~3.9 ms -- process startup,
         * not the engine, dominates and a search takes hours. Reading many
         * keystrokes per process makes that cost amortise away. Context is
         * loaded once; each line gets a fresh ic so lines cannot influence each
         * other, and an empty line resets. */
        if (!layout) {
            fprintf(stderr, "need --layout for --batch\n");
            return 2;
        }
        char linebuf[4096];
        char *line = linebuf;
        size_t n;
        while (fgets(line, (int)sizeof(linebuf), stdin)) {
            n = strlen(line);
            while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
            hunmin_ic *bic = hunmin_ic_new(ctx, layout);
            if (!bic) {
                fprintf(stderr, "layout %s unavailable\n", layout);
                return 2;
            }
            if (caps) hunmin_ic_set_option(bic, "caps", 1);
            /* Accumulate the commit after EVERY key, not once at the end:
             * hunmin_ic_process() clears ic->commit on entry, so reading it once
             * after the loop silently dropped every commit except the last key's.
             * For nkf that reported preedit '가' with an empty commit, hiding the
             * ㅅ that key 2 had already committed -- which then made --batch
             * disagree with --trace. */
            char acc[1024];
            size_t acclen = 0;
            for (const char *kp = line; *kp;) {
                uint32_t key = 0, shift = 0;
                if (keys_step(&kp, &key, &shift, "--batch")) return 2;
                if (shift) hunmin_ic_process_key(bic, key, 1); /* explicit-modifier entry */
                else hunmin_ic_process(bic, key);
                const char *c = hunmin_ic_commit(bic);
                size_t cl = strlen(c);
                if (cl && acclen + cl < sizeof acc) {
                    memcpy(acc + acclen, c, cl);
                    acclen += cl;
                    acc[acclen] = 0;
                }
            }
            char kbuf[1024], cbuf[1024], pbuf[1024];
            json_escape(line, kbuf, sizeof kbuf);
            json_escape(acc, cbuf, sizeof cbuf);
            json_escape(hunmin_ic_preedit(bic), pbuf, sizeof pbuf);
            printf("{\"keys\":\"%s\",\"commit\":\"%s\",\"preedit\":\"%s\"}\n", kbuf, cbuf, pbuf);
            hunmin_ic_free(bic);
        }
        /* line aliases linebuf (a stack array): free()ing it was heap corruption
         * that aborted the process before stdout's buffer ever flushed, so
         * --batch silently exited 127/rc-0xC0000409 under msvcrt. */
        return 0;
    }

    if (judge) {
        /* The frontend's replay: the shared layer, the shipped english list, and
         * after every key the judgement read and acted on. Requires --keys. */
        if (!layout || !keys) {
            fprintf(stderr, "need --layout and --keys for --judge\n");
            return 2;
        }
        judge_case c;
        memset(&c, 0, sizeof c);
        c.l = judge_layer(ctx, layout, latin, autoswitch, start_latin);
        if (!c.l) {
            fprintf(stderr, "cannot build a layer for %s/%s\n", layout, latin ? latin : "(no latin layout)");
            return 2;
        }
        for (const char *s = keys; *s;) {
            uint32_t key = 0, shift = 0;
            int judged = -1;
            if (keys_step(&s, &key, &shift, "--judge")) return 2;
            const int consumed = judge_step(&c, key, shift, &judged);
            if (trace) {
                char kbuf[8], cbuf[1024], pbuf[1024];
                char kraw[2] = {(char)key, 0};
                json_escape(kraw, kbuf, sizeof kbuf);
                json_escape(c.key_commit, cbuf, sizeof cbuf);
                json_escape(c.key_preedit, pbuf, sizeof pbuf);
                printf("{\"in\":{\"ch\":\"%s\"%s},\"commit\":\"%s\",\"preedit\":\"%s\","
                       "\"consumed\":%s%s%s%s}\n",
                       kbuf, shift ? ",\"shift\":true" : "", cbuf, pbuf, consumed ? "true" : "false",
                       judged >= 0 ? ",\"judged\":\"" : "", judged >= 0 ? lang_name(judged) : "",
                       judged >= 0 ? "\"" : "");
            } else {
                printf("[%c%s] commit='%s' preedit='%s'%s\n", (unsigned char)key, shift ? "+" : "",
                       c.key_commit, c.key_preedit, judged >= 0 ? (judged == HUNMIN_LANGUAGE_LATIN ? " judged=latin" : " judged=hangul") : "");
            }
        }
        /* The three facts a judgement case pins, in the corpus's own vocabulary. */
        char dbuf[4200], jbuf[512];
        json_escape(c.delivered, dbuf, sizeof dbuf);
        jbuf[0] = 0;
        if (c.judged[0]) {
            /* c.judged is comma-joined; emit it as the corpus's array. */
            const char *p = c.judged;
            int first = 1;
            while (*p) {
                const char *comma = strchr(p, ',');
                const size_t n = comma ? (size_t)(comma - p) : strlen(p);
                char one[32];
                snprintf(one, sizeof one, "%s\"%.*s\"", first ? "" : ",", (int)n, p);
                str_append(jbuf, sizeof jbuf, one);
                first = 0;
                p += n;
                if (*p == ',') p++;
            }
        }
        printf("{\"delivered\":\"%s\",\"judged\":[%s],\"mode_after\":\"%s\"}\n", dbuf, jbuf,
               lang_name(hunmin_buffered_preedit_language(c.l)));
        hunmin_buffered_preedit_free(c.l);
        return 0;
    }

    if (!layout || !keys) {
        fprintf(stderr, "need --layout and --keys\n");
        return 2;
    }
    hunmin_ic *ic = hunmin_ic_new(ctx, layout);
    if (!ic) {
        fprintf(stderr, "layout %s unavailable\n", layout);
        return 2;
    }
    if (caps) hunmin_ic_set_option(ic, "caps", 1);
    /* The return value is the engine's CONSUMPTION decision, and --trace threw
     * it away. That made the trace unable to say anything about a key the
     * engine declined -- space above all, which is the one key whose whole
     * point is that it is not consumed. Consumers had to re-derive it from the
     * layout's key table (tools/gen-conformance.py:221 does exactly that),
     * which is a second, weaker source for something the engine already
     * answered. Emitting it here makes tier A the single source for
     * consumption as well as for commit/preedit, which is what the tier-D3
     * expectation needs: a declined key is forwarded to the client, and the
     * client is where the space character has to come from.
     *
     * An event with an explicit shift (the "\s<char>" escape) goes through
     * hunmin_ic_process_key -- the same (base, shift) triple tier C already
     * records for x11/win32/android. A plain byte stays on the legacy
     * case-encoding call, byte-for-byte. */
    for (const char *s = keys; *s;) {
        uint32_t key = 0, shift = 0;
        if (keys_step(&s, &key, &shift, "--keys")) return 2;
        bool consumed;
        if (shift) consumed = hunmin_ic_process_key(ic, key, 1);
        else consumed = hunmin_ic_process(ic, key);
        if (trace) {
            /* Every string in the line is escaped, not just commit and preedit.
             * The input key went through raw `%c`, so sin3-p2's shifted apostrophe
             * (shift+; , which commits '"') produced
             *   {"in":{"ch":"""},"commit":"ㄱ"","preedit":""}
             * -- invalid JSON, which breaks every consumer of --trace, including
             * tools/gen-conformance.py (tier C corpus) and tools/sin3p2-ksgen.py.
             * The engine was right; the emitter was not. Found while building a
             * keystroke generator that searches the layout's key table, whose
             * alphabet necessarily includes '"' and '\'. */
            char kbuf[8], cbuf[256], pbuf[256];
            char kraw[2] = {(char)key, 0};
            json_escape(kraw, kbuf, sizeof kbuf);
            json_escape(hunmin_ic_commit(ic), cbuf, sizeof cbuf);
            json_escape(hunmin_ic_preedit(ic), pbuf, sizeof pbuf);
            /* A shift key records itself in "in" so the trace row is self-describing
             * at the same native-event level tier C uses; rows without the escape
             * stay exactly as gen-conformance has always parsed them. */
            printf("{\"in\":{\"ch\":\"%s\"%s},\"commit\":\"%s\",\"preedit\":\"%s\","
                   "\"consumed\":%s}\n",
                   kbuf, shift ? ",\"shift\":true" : "", cbuf, pbuf, consumed ? "true" : "false");
        } else printf("[%c%s] commit='%s' preedit='%s'\n", (unsigned char)key, shift ? "+" : "", hunmin_ic_commit(ic), hunmin_ic_preedit(ic));
    }
    return 0;
}
