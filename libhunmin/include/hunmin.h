#ifndef HUNMIN_H
#define HUNMIN_H

/* libhunmin — portable Hangul input engine. LGPL-2.1+ (see LICENSE).
 * ABI policy: append-only; layout identity is always a string. */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HUNMIN_ABI_VERSION 1

const char *hunmin_version_string(void);
int hunmin_abi_version(void);

typedef struct hunmin_ctx hunmin_ctx;
typedef struct hunmin_ic  hunmin_ic;

/* Create a context scanning layout/compose JSON from ndirs search paths
 * (each dir contains layouts and compose json files, see docs/SPEC-SCHEMA.md).
 * dirs may be NULL/empty -> env HUNMIN_DATA_PATH (colon-separated).
 * Returns NULL if no valid layout is found. */
hunmin_ctx *hunmin_ctx_new(const char *const *dirs, int ndirs);
void hunmin_ctx_free(hunmin_ctx *ctx);

int hunmin_ctx_layout_count(const hunmin_ctx *ctx);
const char *hunmin_ctx_layout_id(const hunmin_ctx *ctx, int i);
/* lang: "en"|"ko"|NULL(=en). */
const char *hunmin_ctx_layout_name(const hunmin_ctx *ctx, int i,
                                   const char *lang);
/* true when this build's engine can instantiate the layout. */
bool hunmin_ctx_layout_supported(const hunmin_ctx *ctx, int i);

/* Returns NULL for unknown id or unsupported engine (e.g. romaja until P1b+). */
hunmin_ic *hunmin_ic_new(hunmin_ctx *ctx, const char *layout_id);
void hunmin_ic_free(hunmin_ic *ic);

/* Feed one US-QWERTY character event: case encodes shift, 0x08 is
 * backspace (identical convention to libhangul / tests/golden traces).
 * Returns true when consumed by the engine. */
bool hunmin_ic_process(hunmin_ic *ic, uint32_t ch);

/* UTF-8 of what the last hunmin_ic_process produced; a destructive read
 * (cleared on read — deliver it once, it will not repeat), "" if none.
 * Valid until the next hunmin_ic_* call on this ic.
 * preedit is non-destructive (state display, re-readable). */
const char *hunmin_ic_commit(hunmin_ic *ic);
const char *hunmin_ic_preedit(hunmin_ic *ic);

/* "caps" -> Caps Lock state for expression var P (yethangul modes);
 * "w"/"W"/"z"/"h"/"H" -> 전처리수식 override (pat.im/1136 §2-6), cleared by
 * hunmin_ic_reset (layout defaults return). */
bool hunmin_ic_set_option(hunmin_ic *ic, const char *name, int value);

void hunmin_ic_reset(hunmin_ic *ic);       /* drop composition silently */
const char *hunmin_ic_flush(hunmin_ic *ic); /* commit in-flight syllable */

/* --- External text pass-through (PLAN.md invariant 5) ---
 *
 * Voice input, and any other text arriving from outside the keyboard, must
 * reach the client as-is and must NOT be fed through the jamo pipeline: doing
 * that would compose it a second time. This call therefore does not touch the
 * composition buffer, the tier state, or the 전처리수식 flags.
 *
 * Any in-flight composition is committed first, so the two texts come out in
 * the order the user produced them (typed, then spoke) instead of interleaved.
 *
 * Unlike hunmin_ic_commit() the text is neither routed through the fixed
 * 64-char engine buffer nor truncated: a spoken sentence is longer than a
 * syllable, and STR_CAP is a libhangul-parity artefact of single-keystroke
 * composition. The returned string is owned by the ic and stays valid until the
 * next hunmin_ic_* call on it -- the same lifetime rule as hunmin_ic_commit().
 *
 * Returns "" (never NULL) when ic is NULL, text is NULL/empty, or text is not
 * valid UTF-8; malformed input is rejected rather than committed as mojibake. */
const char *hunmin_ic_commit_text(hunmin_ic *ic, const char *text);

/* --- Hanja candidates (P3) ---
 *
 * Lookup is by the whole Hangul syllable or word, never by a single
 * character, and never reads surrounding text: the three tables are
 * keyed by the committed Hangul itself (docs/SPEC-SCHEMA.md "Hanja
 * files"). Order is upstream frequency order, not re-derived.
 *
 * Returns the number of candidates now visible, 0 when the input is not
 * in any table or no table was loaded. Candidates are addressed by
 * index, 0-based, and stay valid until the next hunmin_suggest_* call on
 * this ctx -- the same lifetime rule as hunmin_ic_commit(). The ic is not
 * consulted: a candidate list depends only on ctx and the lookup text. */

int hunmin_suggest_count(hunmin_ctx *ctx, const char *hangul);

/* i in [0, count). Out of range, or NULL ctx -> "" (never NULL), so a
 * frontend can render unconditionally. */
const char *hunmin_suggest_text(hunmin_ctx *ctx, int i);
const char *hunmin_suggest_detail(hunmin_ctx *ctx, int i);

/* "hanja" | "idiom" | "word" -- which table a candidate came from, or
 * "" if out of range. Lets a frontend label the list without inferring it
 * from the text. */
const char *hunmin_suggest_kind(hunmin_ctx *ctx, int i);

#ifdef __cplusplus
}
#endif
#endif
