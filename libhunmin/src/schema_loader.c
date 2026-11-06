/* schema_loader.c — load hunmin layout/compose JSON (docs/SPEC-SCHEMA.md). LGPL-2.1+ */
#include "cJSON.h"
#include "hunmin_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#define HM_DIRSEP '\\'
#else
#include <dirent.h>
#define HM_DIRSEP '/'
#endif

#define MAX_FILES 128
#define MAX_LAYOUTS 64
#define KEYSPACE 128
#define MAX_LINE 4096

static const char *SHIFTED[] = {"~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "{", "}", "|", ":", "\"", "<", ">", "?", NULL};
static const char *BASE[] = {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "[", "]", "\\", ";", "'", ",", ".", "/", NULL};

static uint32_t shifted_of(char c) {
    if (c >= 'a' && c <= 'z') return (uint32_t)(c - 'a' + 'A');
    for (int i = 0; BASE[i]; i++)
        if ((char)BASE[i][0] == c) return (uint32_t)SHIFTED[i][0];
    return 0;
}

/* Internal (non-ABI) form for ic.c: the shifted form of a base character
 * under the loader's own US-QWERTY map, 0 when the base has none. The public
 * wrapper hunmin_ic_process_key reuses this — no second US table anywhere
 * (docs/IC-INPUT-OPTIONS.md §0-4). */
uint32_t hm_shifted_of(uint32_t base) { return base < 128 ? shifted_of((char)base) : 0; }

static int cp_from_utf8(const char *s, uint32_t *cp) { /* -> consumed bytes */
    unsigned char c = (unsigned char)s[0];
    if (!c) return 0;
    if (c < 0x80) {
        *cp = c;
        return 1;
    }
    int n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
    *cp = c & (0x7F >> n);
    for (int i = 1; i < n; i++) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) return 0;
        *cp = (*cp << 6) | ((unsigned char)s[i] & 0x3F);
    }
    return n;
}

/* parse a schema-value into hm_val (deep). NULL on failure.
 * top=1 disables `case` at nesting levels the engine cannot resolve. */
static hm_val *load_value_deep(const cJSON *v, bool allow_case, int depth);
static void hm_val_clear_seq(hm_val *v);

static hm_val *val_new(void) {
    hm_val *v = calloc(1, sizeof *v);
    return v;
}

static hm_val *load_value_deep(const cJSON *v, bool allow_case, int depth) {
    if (depth > 8) return NULL;
    hm_val *out;
    if (cJSON_IsNull(v)) {
        out = val_new();
        return out; /* HK_NONE */
    }
    if (cJSON_IsString(v)) {
        const char *s = v->valuestring;
        uint32_t cp;
        out = val_new();
        if (!out) return NULL;
        if (s[0] == '=' && s[1]) {
            const char *rest = s + 1;
            int n = cp_from_utf8(rest, &cp);
            if (!cp || !n || rest[n]) {
                free(out);
                return NULL;
            }
            out->kind = HK_LIT;
            out->code = cp;
            return out;
        }
        int n = cp_from_utf8(s, &cp);
        if (!n || s[n] || !hm_is_jamo((ucschar)cp) || cp == 0x115F || cp == 0x1160) {
            free(out);
            return NULL;
        }
        out->kind = HK_JAMO;
        out->code = cp;
        return out;
    }
    if (cJSON_IsObject(v)) {
        out = val_new();
        if (!out) return NULL;
        const cJSON *c = cJSON_GetObjectItemCaseSensitive(v, "case");
        const cJSON *amb = cJSON_GetObjectItemCaseSensitive(v, "amb");
        const cJSON *seq = cJSON_GetObjectItemCaseSensitive(v, "seq");
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(v, "noop"))) {
            out->kind = HK_NOOP;
            return out;
        }
        const cJSON *g = cJSON_GetObjectItemCaseSensitive(v, "g");
        if (g && !c) {
            hm_val *inner = load_value_deep(g, allow_case, depth + 1);
            if (!inner) {
                free(out);
                return NULL;
            }
            inner->g = 1;
            free(out);
            return inner; /* flag lives on the leaf */
        }
        const cJSON *tier = cJSON_GetObjectItemCaseSensitive(v, "tier");
        if (tier && cJSON_IsNumber(tier)) {
            out->kind = HK_TIER;
            out->code = (uint32_t)tier->valueint;
            return out;
        }
        const cJSON *j = cJSON_GetObjectItemCaseSensitive(v, "j");
        if (j && cJSON_IsString(j) && !c) {
            uint32_t cp;
            int n = cp_from_utf8(j->valuestring, &cp);
            if (!n || j->valuestring[n] || !hm_is_jamo((ucschar)cp) || cp == 0x115F || cp == 0x1160) {
                free(out);
                return NULL;
            }
            out->kind = HK_JAMO;
            out->code = cp;
            const cJSON *vr = cJSON_GetObjectItemCaseSensitive(v, "variant");
            if (cJSON_IsTrue(vr)) out->variant = 1;
            return out;
        }
        if (seq) {
            if (!cJSON_IsArray(seq)) {
                free(out);
                return NULL;
            }
            size_t len = cJSON_GetArraySize(seq);
            if (len == 0) {
                free(out);
                return NULL;
            }
            uint32_t *arr = calloc(len + 1, sizeof *arr);
            if (!arr) {
                free(out);
                return NULL;
            }
            size_t idx = 0;
            const cJSON *item;
            cJSON_ArrayForEach(item, seq) {
                if (!cJSON_IsString(item)) {
                    free(arr);
                    free(out);
                    return NULL;
                }
                uint32_t cp;
                int n = cp_from_utf8(item->valuestring, &cp);
                if (!n || item->valuestring[n] || cp == 0) {
                    free(arr);
                    free(out);
                    return NULL;
                }
                arr[idx++] = cp;
            }
            arr[idx] = 0;
            out->kind = HK_SEQ;
            out->seq = arr;
            out->seqlen = idx;
            return out;
        }
        if (amb || !c) {
            free(out);
            return NULL;
        } /* amb later; only case here */
        if (!allow_case) {
            free(out);
            return NULL;
        }
        const cJSON *ci = cJSON_GetObjectItemCaseSensitive(c, "if");
        const cJSON *ct = cJSON_GetObjectItemCaseSensitive(c, "then");
        const cJSON *ce = cJSON_GetObjectItemCaseSensitive(c, "else");
        if (!cJSON_IsString(ci) || !ct || !ce) {
            free(out);
            return NULL;
        }
        out->cond = hm_expr_parse(ci->valuestring, NULL, 0);
        out->then = load_value_deep(ct, allow_case, depth + 1);
        out->els = load_value_deep(ce, allow_case, depth + 1);
        if (!out->cond || !out->then || !out->els) {
            hm_expr_free(out->cond);
            free(out->then);
            free(out->els);
            free(out);
            return NULL;
        }
        out->kind = HK_CASE;
        return out;
    }
    return NULL;
}

static int cmp_combo(const void *a, const void *b) {
    uint32_t x = ((const hm_combo *)a)->ab, y = ((const hm_combo *)b)->ab;
    return x < y ? -1 : x > y ? 1 : 0;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 1 << 20) {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)sz + 1);
    if (buf && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        buf = NULL;
    }
    if (buf) buf[sz] = 0;
    fclose(f);
    return buf;
}

static int ends_json(const char *n) {
    size_t l = strlen(n);
    return l > 5 && !strcmp(n + l - 5, ".json");
}

static int list_json(const char *dir, char names[MAX_FILES][MAX_LINE]) {
#if defined(_WIN32)
    WIN32_FIND_DATAA fd;
    char pattern[MAX_LINE];
    snprintf(pattern, sizeof(pattern), "%s\\*.json", dir);
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    int n = 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (ends_json(fd.cFileName) && n < MAX_FILES) {
                snprintf(names[n], MAX_LINE, "%s", fd.cFileName);
                n++;
            }
        }
    } while (FindNextFileA(h, &fd) && n < MAX_FILES);
    FindClose(h);
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(names[j], names[i]) < 0) {
                char t[MAX_LINE];
                size_t a = strlen(names[i]) + 1, b = strlen(names[j]) + 1;
                memcpy(t, names[i], a);
                memcpy(names[i], names[j], b);
                memcpy(names[j], t, a);
            }
    return n;
#else
    DIR *d = opendir(dir);
    if (!d) return -1;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < MAX_FILES)
        if (ends_json(e->d_name)) {
            snprintf(names[n], MAX_LINE, "%s", e->d_name);
            n++;
        }
    closedir(d);
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(names[j], names[i]) < 0) {
                char t[MAX_LINE];
                size_t a = strlen(names[i]) + 1, b = strlen(names[j]) + 1;
                memcpy(t, names[i], a);
                memcpy(names[i], names[j], b);
                memcpy(names[j], t, a);
            }
    return n;
#endif
}

static uint32_t utf8_first(const char *s) {
    uint32_t cp = 0;
    cp_from_utf8(s, &cp);
    return cp;
}

/* -------- compose registry -------- */
typedef struct {
    char id[64];
    hm_combo *items;
    size_t n;
} compose_rec;
static compose_rec comps[MAX_FILES];
static int ncomps;

static const compose_rec *compose_find(const char *id) {
    for (int i = 0; i < ncomps; i++)
        if (!strcmp(comps[i].id, id)) return &comps[i];
    return NULL;
}

static bool load_compose_file(const char *path) {
    char *txt = read_file(path);
    if (!txt) return false;
    cJSON *root = cJSON_Parse(txt);
    free(txt);
    if (!root) return false;
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    const cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "items");
    if (!cJSON_IsString(id) || !cJSON_IsArray(items)) {
        cJSON_Delete(root);
        return false;
    }
    size_t cnt = (size_t)cJSON_GetArraySize(items);
    char idname[64];
    snprintf(idname, sizeof idname, "%s", id->valuestring);
    hm_combo *arr = calloc(cnt ? cnt : 1, sizeof *arr);
    if (!arr) {
        cJSON_Delete(root);
        return false;
    }
    size_t n = 0;
    const cJSON *it;
    bool ok = true;
    cJSON_ArrayForEach(it, items) {
        uint32_t a, b, r;
        const cJSON *ja = cJSON_GetObjectItemCaseSensitive(it, "a");
        const cJSON *jb = cJSON_GetObjectItemCaseSensitive(it, "b");
        const cJSON *jr = cJSON_GetObjectItemCaseSensitive(it, "r");
        if (!cJSON_IsString(ja) || !hm_is_jamo((a = (utf8_first(ja->valuestring)))) || !cJSON_IsString(jb) ||
            !hm_is_jamo((b = (utf8_first(jb->valuestring)))) || !cJSON_IsString(jr) || !hm_is_jamo((r = (utf8_first(jr->valuestring))))) {
            ok = false;
            break;
        }
        arr[n].ab = (a << 16) | b;
        arr[n].r = r;
        n++;
    }
    cJSON_Delete(root);
    if (!ok) {
        free(arr);
        return false;
    }
    if (ncomps >= MAX_FILES) {
        free(arr);
        return false;
    }
    snprintf(comps[ncomps].id, sizeof comps[ncomps].id, "%s", idname);
    comps[ncomps].items = arr;
    comps[ncomps].n = 0;
    qsort(arr, n, sizeof *arr, cmp_combo);
    /* Deduplicate BEFORE sorting. Two compose tables may legitimately define the
     * same (a,b) pair differently -- sin3-p2-yet's mix and ohi tables disagree on
     * 39 pairs, e.g. 0x1100+0x1100 is both ssang-cho and ssang-jong depending on
     * which table you read -- and "first file wins" is the documented rule (the
     * layout lists its tables in precedence order).
     *
     * Sorting first and keeping the first of each run is NOT equivalent: qsort is
     * not stable, so which of two equal-key rows survives depends on the sort's
     * internal pivoting. That made the engine's output depend on the libc's
     * qsort, and it did: the same libhunmin.so build and the same data file
     * produced ㅃ on Android's bionic libc and ᄈᅠᆸ on glibc, because bionic and
     * glibc order equal keys differently. Found by replaying the tier-A goldens
     * on the emulator (p2-2-6-flags, sin3-p2-yet, h=0, "ee").
     *
     * Scan in file order first, so the winner is the earliest row of the earliest
     * table, then sort what is left. Both halves are now deterministic and the
     * libc is out of the answer. */
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        bool dup = false;
        for (size_t k = 0; k < w; k++)
            if (arr[k].ab == arr[i].ab) {
                dup = true;
                break;
            }
        if (!dup) arr[w++] = arr[i];
    }
    qsort(arr, w, sizeof *arr, cmp_combo);
    comps[ncomps].n = w;
    ncomps++;
    return true;
}

/* -------- layout load -------- */
static void hm_val_clear_seq(hm_val *v) {
    if (!v) return;
    if (v->kind == HK_SEQ) {
        free(v->seq);
        v->seq = NULL;
        v->seqlen = 0;
        v->kind = HK_NONE;
    }
}

static void hm_val_clear(hm_val *v) {
    if (!v) return;
    if (v->kind == HK_CASE) {
        hm_expr_free(v->cond);
        hm_val_clear(v->then);
        free(v->then);
        hm_val_clear(v->els);
        free(v->els);
        v->kind = HK_NONE;
        v->cond = NULL;
        v->then = v->els = NULL;
    } else if (v->kind == HK_SEQ) {
        hm_val_clear_seq(v);
    }
}

/* ------------------------------------------------------- layout capability
 *
 * The modes judgement asks whether a buffer composes into well-formed syllables.
 * Two of its questions are properties of the LAYOUT, not of one buffer, and both
 * are derived here from the layout's own data rather than declared in a field a
 * new file could forget:
 *
 *   composes_hangul   the layout emits conjoining jamo at all. False for the
 *                     Latin layouts (us-qwerty, dvorak, colemak), whose keys emit
 *                     `=x` literal text, so the judgement has nothing to decide
 *                     there.
 *   old_hangul        the layout can emit a jamo outside the modern 19/21/28.
 *                     Two routes reach one, and the derivation checks both:
 *                     a key row that maps straight to an archaic jamo, and a
 *                     combination table that produces one. Measured 2026-11-01:
 *                     all six shipped old-Hangul layouts reach it through a KEY
 *                     — including sin3-p2-yet, whose archaic vowels sit inside
 *                     `case` branches of its key rows — so the table route is
 *                     the second one rather than the only one. It is still a
 *                     real route: with sin3-p2-yet's keys scrubbed of ᆞ in a
 *                     scratch data copy, that layout still answers 1, because
 *                     its sin3-p2-yet-ohi table produces them.
 *                     Those layouts need hunmin_hangul_valid_old(); the rest must
 *                     not, because widening the rule there would accept shapes
 *                     their composer cannot produce.
 */
static bool modern_jamo(uint32_t c) {
    if (c == 0x11a7) return true; /* the no-jongseong filler, inside the jong set */
    return hm_is_choseong_conjoinable((ucschar)c) || hm_is_jungseong_conjoinable((ucschar)c) || hm_is_jongseong_conjoinable((ucschar)c);
}

static void note_jamo(uint32_t c, bool *any, bool *old) {
    if (!hm_is_jamo((ucschar)c)) return;
    *any = true;
    if (!modern_jamo(c)) *old = true;
}

static void val_jamo_scan(const hm_val *v, bool *any, bool *old) {
    if (!v) return;
    switch (v->kind) {
    case HK_JAMO: note_jamo(v->code, any, old); break;
    case HK_CASE:
        val_jamo_scan(v->then, any, old);
        val_jamo_scan(v->els, any, old);
        break;
    case HK_SEQ:
        for (size_t i = 0; i < v->seqlen; i++) note_jamo(v->seq[i], any, old);
        break;
    default: break; /* HK_LIT / HK_TIER / HK_NONE carry no jamo */
    }
}

static void compute_capability(hm_layout *l) {
    bool any = false, old = false;

    /* The keys decide whether the layout composes Hangul at all, and that answer
     * gates everything else: every shipped layout references a combination table
     * — the Latin layouts reference `modern` too — so scanning tables alone would
     * report that us-qwerty composes Hangul, which it cannot, because none of its
     * keys ever feeds a jamo into one. */
    for (int k = 0; k < KEYSPACE; k++) val_jamo_scan(&l->keys[k], &any, &old);

    if (any) {
        /* `r` is what the table produces, so it answers "can this layout emit X".
         * Needed for the layouts whose archaic vowels or ᇬ-family jongseong come
         * out of a combination rather than a key — proved by scrubbing one layout's
         * keys in a scratch data copy and watching this answer stay 1. */
        for (size_t i = 0; i < l->ncombo; i++) note_jamo(l->combo[i].r, &any, &old);
    } else {
        old = false; /* a layout that composes no Hangul cannot compose old Hangul */
    }
    l->composes_hangul = any;
    l->old_hangul = old;
}

static bool build_compose(const cJSON *root, hm_layout *l) {
    const cJSON *cps = cJSON_GetObjectItemCaseSensitive(root, "compose");
    size_t need = 0;
    if (cJSON_IsArray(cps)) {
        const cJSON *c;
        cJSON_ArrayForEach(c, cps) {
            if (!cJSON_IsString(c)) return false;
            const compose_rec *rec = compose_find(c->valuestring);
            if (!rec) {
                fprintf(stderr, "warn: %s: compose '%s' missing\n", l->id, c->valuestring);
                continue;
            }
            need += rec->n;
        }
    }
    if (!need) return true;
    l->combo = malloc(need * sizeof *l->combo);
    if (!l->combo) return false;
    size_t off = 0;
    const cJSON *c;
    cJSON_ArrayForEach(c, cps) {
        const compose_rec *rec = compose_find(c->valuestring);
        if (!rec) continue;
        /* Tables are concatenated in the layout's declared precedence order and
         * deduplicated here, so the earlier table still wins a disagreement. See
         * load_compose_file() for why this cannot be left to a sort. The nested
         * scan is O(n*w); n is a few hundred rows per layout, and correctness of the
         * first-wins rule is worth more here than the scan. */
        for (size_t i = 0; i < rec->n && off < need; i++) {
            bool dup = false;
            for (size_t k = 0; k < off; k++)
                if (l->combo[k].ab == rec->items[i].ab) {
                    dup = true;
                    break;
                }
            if (!dup) l->combo[off++] = rec->items[i];
        }
    }
    qsort(l->combo, off, sizeof *l->combo, cmp_combo);
    l->ncombo = off;
    for (size_t i = 0; i < l->ncombo; i++) { /* pure modern-jong domain */
        uint32_t a = l->combo[i].ab >> 16, b = l->combo[i].ab & 0xFFFF, r = l->combo[i].r;
        /* modern compound-batchim zone: ㄲ…ssang-sios (U+11A9..U+11BB). Old-gul
         * chains (ᇬ U+11EC, ᆰ..) stay outside → compose unguarded, per
         * pat.im §2-4-2 (받침 ㅇ+ㄱ→ᇬ) vs §2-3 (cq needs shift to extend). */
        l->combo[i].ssang = (a >= 0x11A8 && a <= 0x11FF && b >= 0x11A8 && b <= 0x11FF && r >= 0x11A9 && r <= 0x11BB);
    }
    return true;
}

static bool build_keys(const cJSON *root, hm_layout *l) {
    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(root, "keys");
    if (!cJSON_IsObject(keys) || !keys->child) return false;
    const cJSON *k;
    cJSON_ArrayForEach(k, keys) {
        const char *base = k->string;
        uint32_t plain_cp = 0;
        if (base[0] && base[1] == 0) {
            unsigned char u = (unsigned char)base[0];
            if (u >= 0x20 && u < 0x7F) plain_cp = u;
            else continue;
        } else if (!strcmp(base, "space")) plain_cp = ' ';
        else if (!strcmp(base, "enter")) plain_cp = '\r';
        else if (!strcmp(base, "backspace")) plain_cp = '\b';
        else continue;
        const cJSON *m;
        cJSON_ArrayForEach(m, k) {
            uint32_t code = plain_cp;
            if (!strcmp(m->string, "shift")) {
                if (!(code = shifted_of((char)plain_cp))) {
                    fprintf(stderr, "warn: %s: no shift for '%s'\n", l->id, base);
                    continue;
                }
            } else if (strcmp(m->string, "plain")) {
                continue; /* ctrl/alt reserved */
            }
            hm_val *nv = load_value_deep(m, true, 0);
            if (!nv) {
                fprintf(stderr, "warn: %s: key '%s' value not loadable in this build\n", l->id, base);
                continue;
            }
            if (nv->kind == HK_CASE && !l->supports_case) l->supported = false;
            hm_val_clear(&l->keys[code & (KEYSPACE - 1)]);
            l->keys[code & (KEYSPACE - 1)] = *nv;
            free(nv);
        }
    }
    return true;
}

static bool load_layout_file(hunmin_ctx *ctx, const char *path, int *idx) {
    char *txt = read_file(path);
    if (!txt) return false;
    cJSON *root = cJSON_Parse(txt);
    free(txt);
    if (!root) return false;
    const cJSON *j;
    bool ok = false;

    if (ctx->n >= MAX_LAYOUTS) {
        ok = false;
        goto out;
    }
    j = cJSON_GetObjectItemCaseSensitive(root, "schemaVersion");
    if (!cJSON_IsNumber(j) || j->valueint > 2) goto out;
    j = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (!cJSON_IsString(j)) goto out;

    hm_layout *l = &ctx->layouts[ctx->n];
    memset(l, 0, sizeof *l);
    snprintf(l->id, sizeof l->id, "%s", j->valuestring);
    l->type = HM_TYPE_JASO;
    l->supported = true;

    /* "mode": "hangul" | "latin". Absent means hangul, which is what every
     * layout was before the field existed and what all but us-qwerty still are.
     * An unrecognised value ALSO means hangul rather than a rejected file: a
     * typo in one layout should cost one mis-filed row in a settings list, not
     * that layout entirely. tools/doctor.py is the check that makes the typo
     * loud; see docs/SPEC-SCHEMA.md. */
    j = cJSON_GetObjectItemCaseSensitive(root, "mode");
    if (cJSON_IsString(j) && !strcmp(j->valuestring, "latin")) l->mode = HM_MODE_LATIN;

    const cJSON *fl = cJSON_GetObjectItemCaseSensitive(root, "flags");
    if (cJSON_IsObject(fl)) {
        const cJSON *f;
        cJSON_ArrayForEach(f, fl) if (cJSON_IsNumber(f) && f->string && !f->string[1]) l->flags[(unsigned char)f->string[0]] = f->valueint;
    }
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (cJSON_IsObject(name)) {
        const cJSON *en = cJSON_GetObjectItemCaseSensitive(name, "en");
        const cJSON *ko = cJSON_GetObjectItemCaseSensitive(name, "ko");
        if (cJSON_IsString(en)) snprintf(l->name_en, sizeof l->name_en, "%s", en->valuestring);
        if (cJSON_IsString(ko)) snprintf(l->name_ko, sizeof l->name_ko, "%s", ko->valuestring);
    }
    j = cJSON_GetObjectItemCaseSensitive(root, "jongonlyFresh");
    l->jongonly_fresh = cJSON_IsTrue(j);
    j = cJSON_GetObjectItemCaseSensitive(root, "engine");
    if (cJSON_IsString(j) && !strcmp(j->valuestring, "romaja")) {
        l->type = HM_TYPE_ROMAJA;
    } else if (cJSON_IsString(j) && !strcmp(j->valuestring, "h3")) {
        l->type = HM_TYPE_JASO;
        l->supports_case = true;
    }
    j = cJSON_GetObjectItemCaseSensitive(root, "libhangulType");
    if (cJSON_IsString(j) && l->type != HM_TYPE_ROMAJA) {
        const char *t = j->valuestring;
        l->type = !strcmp(t, "jamo")       ? HM_TYPE_JAMO
                  : !strcmp(t, "jamo-yet") ? HM_TYPE_JAMO_YET
                  : !strcmp(t, "jaso")     ? HM_TYPE_JASO
                  : !strcmp(t, "jaso-yet") ? HM_TYPE_JASO_YET
                                           : HM_TYPE_JASO;
    }

    if (!build_compose(root, l)) goto out;

    if (!build_keys(root, l)) goto out;
    compute_capability(l);
    ok = true;
    if (ok) {
        *idx = ctx->n;
        ctx->n++;
    }
out:
    if (!ok && ctx->layouts) free(ctx->layouts[ctx->n].combo);
    cJSON_Delete(root);
    return ok;
}

/* Split a colon-joined dir list, keeping Windows drive letters intact: the
 * ':' in "C:/x" marks the drive, not a separator. Positional rule -- a ':'
 * directly after the token's first character, when that character is an
 * ASCII letter and the next char is '/' or '\\', is part of the token.
 * Paths joined with '/' (schema_loader.c's snprintf "%s/layouts") still re-split
 * identically, so ctx->data_dir round-trips. Measured failure without it:
 * "C:/Workspace/..." split into "C" and "/Workspace/...", and every
 * opendir("C") failed -- "no layouts loaded" on Windows. */
int hm_split_dirs(const char *env, char *buf, size_t bufsz, const char *dirs[], int max) {
    if (!env || !*env || !buf || bufsz == 0 || !dirs || max <= 0) return 0;
    snprintf(buf, bufsz, "%s", env);
    size_t len = strlen(buf);
    if (len >= bufsz) return 0; /* truncated; a dir would end mid-character */
    int n = 0;
    const char *p = buf;
    while (*p && n < max) {
        const char *tok = p;
        dirs[n++] = tok;
        for (;;) {
            char c = *p;
            if (c == ':' && p == tok + 1 && ((tok[0] >= 'A' && tok[0] <= 'Z') || (tok[0] >= 'a' && tok[0] <= 'z')) && (p[1] == '/' || p[1] == '\\')) {
                p++; /* drive letter, keep scanning this token */
                continue;
            }
            if (c == ':' || c == '\0') break;
            p++;
        }
        if (*p == ':') *((char *)p++) = '\0'; /* end this token, start next */
    }
    return n;
}

/* The one search every data subdirectory shares (see hunmin_internal.h). Prefers
 * the dirs recorded on the context: a frontend that passed an explicit path --
 * the Android IME, in an app sandbox with no HUNMIN_DATA_PATH -- must get its
 * tables from the same place as its layouts, and the english word set must not
 * search somewhere else than the hanja tables do. */
int hm_data_dirs(const hunmin_ctx *ctx, const char *const **out) {
    static char envbuf[4096];
    static const char *mydirs[16];
    const char *env = (ctx && ctx->data_dir && *ctx->data_dir) ? ctx->data_dir : getenv("HUNMIN_DATA_PATH");
    if (!env || !*env) {
#ifdef HUNMIN_DEFAULT_DATA_DIR
        env = HUNMIN_DEFAULT_DATA_DIR;
#else
        env = "./libhunmin/data";
#endif
    }
    snprintf(envbuf, sizeof envbuf, "%s", env);
    int n = hm_split_dirs(env, envbuf, sizeof envbuf, mydirs, 16);
    *out = mydirs;
    return n;
}

hunmin_ctx *hm_data_load(const char *const *dirs, int ndirs) {
    const char *mydirs[16];
    char envbuf[4096];
    if (ndirs <= 0) {
        const char *env = getenv("HUNMIN_DATA_PATH");
        if (!env || !*env)
#ifdef HUNMIN_DEFAULT_DATA_DIR
            env = HUNMIN_DEFAULT_DATA_DIR;
#else
            env = "./libhunmin/data";
#endif
        snprintf(envbuf, sizeof envbuf, "%s", env);
        ndirs = hm_split_dirs(env, envbuf, sizeof envbuf, mydirs, 16);
        dirs = mydirs;
    }

    hunmin_ctx *ctx = calloc(1, sizeof *ctx);
    if (!ctx) return NULL;
    ctx->layouts = calloc(MAX_LAYOUTS, sizeof *ctx->layouts);
    if (!ctx->layouts) {
        free(ctx);
        return NULL;
    }

    /* Record the search dirs (colon-joined, the same form resolve_dirs parses)
     * so the suggest layer can find hanja/idioms/krdict from the same place as
     * the layouts, without relying on HUNMIN_DATA_PATH being set in-process. */
    {
        size_t cap = 1;
        for (int d = 0; d < ndirs; d++) cap += strlen(dirs[d]) + 1;
        ctx->data_dir = malloc(cap);
        if (ctx->data_dir) {
            size_t off = 0;
            ctx->data_dir[0] = '\0';
            for (int d = 0; d < ndirs; d++) {
                size_t len = strlen(dirs[d]);
                if (d) ctx->data_dir[off++] = ':';
                memcpy(ctx->data_dir + off, dirs[d], len + 1);
                off += len;
            }
        }
    }

    static char names[MAX_FILES][MAX_LINE];
    for (int d = 0; d < ndirs; d++) {
        char path[4608];
        /* compose first so layouts can reference */
        snprintf(path, sizeof path, "%s/compose", dirs[d]);
        int cn = list_json(path, names);
        for (int i = 0; i < cn; i++) {
            snprintf(path, sizeof path, "%s/compose/%s", dirs[d], names[i]);
            load_compose_file(path);
        }
        snprintf(path, sizeof path, "%s/layouts", dirs[d]);
        int ln = list_json(path, names);
        for (int i = 0; i < ln; i++) {
            char full[4608];
            snprintf(full, sizeof full, "%s/layouts/%s", dirs[d], names[i]);
            int idx;
            load_layout_file(ctx, full, &idx);
        }
    }
    if (ctx->n == 0) {
        hm_data_free(ctx);
        return NULL;
    }
    return ctx;
}

void hm_data_free(hunmin_ctx *ctx) {
    if (!ctx) return;
    for (int i = 0; i < ncomps; i++) free(comps[i].items);
    ncomps = 0;
    for (int i = 0; ctx->layouts && i < ctx->n; i++) {
        free(ctx->layouts[i].combo);
        for (int k = 0; k < KEYSPACE; k++) hm_val_clear(&ctx->layouts[i].keys[k]);
    }
    free(ctx->layouts);
    free(ctx->data_dir);
    free(ctx);
}
