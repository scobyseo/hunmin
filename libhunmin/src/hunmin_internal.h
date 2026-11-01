/* internal structures shared by schema_loader.c / ic.c */
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
    /* Derived from this layout's own keys and combination table at load time
     * (schema_loader.c compute_capability), never declared: a layout that emits
     * conjoining jamo can be judged at all, and one that emits jamo outside the
     * modern 19/21/28 needs the widened rule. */
    bool composes_hangul;
    bool old_hangul;
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

#define STR_CAP 64 /* ucschar buffer capacity (libhangul parity) */
#define UTF8_CAP 256 /* getter output capacity */

typedef struct {
    ucschar cho, jung, jong;
    uint8_t jvar;
    ucschar stack[12];
    int index;
} hm_buf_t;

/* The input context state lives here rather than in ic.c because the shared
 * buffered/syllable layer (buffered_preedit.c) must read it: hunmin_buffered_preedit_peek_key copies
 * an ic by value to answer a platform "test" phase without consuming the key
 * (issue #128). Internal to the library -- never in the ABI. */
struct hunmin_ic {
    hm_layout *L;
    hm_buf_t b;
    ucschar commit[STR_CAP], preedit[STR_CAP];
    char commit8[UTF8_CAP], preedit8[UTF8_CAP];
    /* External-text passthrough output (hunmin_ic_commit_text). Not one of the
     * fixed buffers above: voice text is unbounded, and the point of the call is
     * that a sentence is not truncated to a syllable's worth. Owned, freed with
     * the ic, and handed out under the same lifetime rule as commit8. */
    char *ext8;
    size_t extcap;
    uint32_t T; /* symbol-tier state (0/101..103) */
    int caps; /* Caps Lock (expression var P) */
    int flag_ovr[128]; /* 전처리수식 override (w/W/z/h/H), -1 = layout */
    int flags_eff[128]; /* effective flags seen by expressions */
    bool opt_auto_reorder; /* libhangul ic_new defaults */
    bool opt_combi_on_double; /* false */
    bool opt_non_choseong_combi; /* true */
};

/* schema_loader.c */
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
 * hm_data_load (schema_loader.c) and the candidate layer's resolve_dirs (candidate_tables.c);
 * the recorded ctx->data_dir rejoins with ':' and re-parses consistently.
 * Returns the number of dirs (0..max); buffer must be writable and is NUL-
 * terminated in place. */
int hm_split_dirs(const char *env, char *buf, size_t bufsz, const char *dirs[], int max);

/* wordset.c -- the english word set behind R8-정정's `english_valid`
 * (docs/SPEC-MODES.md, ruling 2026-11-01: hunspell en_US base + user words).
 * Internal: the ABI surface is the hunmin_buffered_preedit_english* wrappers in
 * buffered_preedit.c, so this stays out of hunmin.h. */
typedef struct hm_wordset hm_wordset;
hm_wordset *hm_wordset_new(void);
void hm_wordset_free(hm_wordset *w);
/* 1 loaded, -1 absent (a normal state under B3's data gate), 0 a malformed
 * file -- a broken JSON list must not read as "no english words exist". */
int hm_wordset_load_json(hm_wordset *w, const char *path);
int hm_wordset_load_words(hm_wordset *w, const char *path);
int hm_wordset_add(hm_wordset *w, const char *word);
int hm_wordset_has(const hm_wordset *w, const char *word);
int hm_wordset_ready(const hm_wordset *w);

#endif
