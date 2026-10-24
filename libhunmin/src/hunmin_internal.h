/* internal structures shared by data.c / ic.c */
#ifndef HUNMIN_INTERNAL_H
#define HUNMIN_INTERNAL_H

#include "expr.h"
#include "hunmin.h"
#include "tables.h"
#include <stdlib.h>

typedef enum { HM_TYPE_JAMO, HM_TYPE_JAMO_YET, HM_TYPE_JASO, HM_TYPE_JASO_YET, HM_TYPE_ROMAJA } hm_engine_t;

typedef struct {
    uint32_t ab; /* (a<<16)|b */
    uint32_t r;
    uint8_t ssang;
} hm_combo;

typedef enum { HK_NONE = 0, HK_JAMO, HK_LIT, HK_CASE, HK_TIER, HK_NOOP, HK_SEQ } hm_kind;
typedef struct hm_val {
    hm_kind kind;
    uint8_t g; /* G-flag: shift-marked jong (state-3 composition gate) */
    uint8_t variant; /* right-side jung (only these chain-compose) */
    uint32_t code; /* HK_JAMO / HK_LIT / HK_TIER */
    hm_expr *cond; /* HK_CASE */
    struct hm_val *then, *els;
    uint32_t *seq; /* HK_SEQ: array of codepoints, terminated by 0 */
    size_t seqlen;
} hm_val;

static inline const hm_val *hm_resolvev(const hm_val *v, hm_state st) {
    while (v && v->kind == HK_CASE) v = hm_expr_eval(v->cond, st) ? v->then : v->els;
    return v;
}

/* Which half of a two-mode IME a layout belongs to. Data, not code: the
 * Android settings window splits its layout list on this (issue #12), and a
 * front-end that guessed from the name would drift from the file. */
typedef enum { HM_MODE_HANGUL = 0, HM_MODE_LATIN = 1 } hm_layout_mode;

typedef struct {
    int schema;
    char id[64];
    hm_layout_mode mode;
    char name_en[128];
    char name_ko[128];
    hm_engine_t type;
    bool supports_case;
    bool jongonly_fresh; /* modern ssang pairs need G after jong-only syllable (symbols/yethangul) */
    bool supported;
    int flags[128]; /* user flags from preprocessing defaults */
    hm_val keys[128]; /* char -> resolved value (kind NONE = absent) */
    hm_combo *combo;
    size_t ncombo;
} hm_layout;

struct hunmin_ctx {
    hm_layout *layouts;
    int n;
    /* Data directory this context was created from, or NULL when the caller
     * passed no explicit dirs (env HUNMIN_DATA_PATH / build default was used).
     * The suggest layer prefers this over the env var so a frontend that hands
     * the engine an explicit path -- the Android IME, which runs in an app
     * sandbox where HUNMIN_DATA_PATH is not set -- gets Hanja/idiom/krdict
     * tables from the same directory as its layouts. Copied, so the caller may
     * free its buffer; NUL-terminated. */
    char *data_dir;
};

/* data.c */
hunmin_ctx *hm_data_load(const char *const *dirs, int ndirs);
void hm_data_free(hunmin_ctx *ctx);
/* Shifted form of a US-QWERTY base character under the loader's map (0 when
 * there is none); used by hunmin_ic_process_key so the case endpoint has one
 * home. Not exported in the ABI. */
uint32_t hm_shifted_of(uint32_t base);

/* Split a colon-joined data-dir list without strtok, and Windows-drive-aware:
 * the ':' in "C:/x" is a drive marker, not a separator, so "C:/a" must never
 * split into "" and "/a". The rule is positional -- a ':' right after the
 * token's first character when that character is an ASCII letter and the next
 * char is '/' or '\\' is kept, every other ':' separates. Used by both
 * hm_data_load (data.c) and the suggest layer's resolve_dirs (suggest.c);
 * the recorded ctx->data_dir rejoins with ':' and re-parses consistently.
 * Returns the number of dirs (0..max); buffer must be writable and is NUL-
 * terminated in place. */
int hm_split_dirs(const char *env, char *buf, size_t bufsz, const char *dirs[], int max);

#endif
