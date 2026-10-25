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

/* Export macros for shared library visibility.
 * On Windows, symbols are NOT exported by default — an explicit
 * __declspec(dllexport) is required when building the DLL and
 * __declspec(dllimport) when consuming it. On Linux/gcc, default
 * visibility already exports everything, but the attribute makes
 * the intent explicit and survives a -fvisibility=hidden build. */
/* A static consumer defines HUNMIN_STATIC so HUNMIN_EXPORT is empty.
 * dllimport on a symbol defined in the archive asks the linker for the
 * __imp_ thunks the archive does not contain. */
#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef HUNMIN_STATIC
#define HUNMIN_EXPORT
#elif defined(HUNMIN_BUILDING_DLL)
#define HUNMIN_EXPORT __declspec(dllexport)
#else
#define HUNMIN_EXPORT __declspec(dllimport)
#endif
#else
#define HUNMIN_EXPORT __attribute__((visibility("default")))
#endif

HUNMIN_EXPORT const char *hunmin_version_string(void);
HUNMIN_EXPORT int hunmin_abi_version(void);

typedef struct hunmin_ctx hunmin_ctx;
typedef struct hunmin_ic hunmin_ic;

/* Create a context scanning layout/compose JSON from ndirs search paths
 * (each dir contains layouts and compose json files, see docs/SPEC-SCHEMA.md).
 * dirs may be NULL/empty -> env HUNMIN_DATA_PATH (colon-separated).
 * Returns NULL if no valid layout is found. */
hunmin_ctx *hunmin_ctx_new(const char *const *dirs, int ndirs);
void hunmin_ctx_free(hunmin_ctx *ctx);

int hunmin_ctx_layout_count(const hunmin_ctx *ctx);
const char *hunmin_ctx_layout_id(const hunmin_ctx *ctx, int i);
/* lang: "en"|"ko"|NULL(=en). */
const char *hunmin_ctx_layout_name(const hunmin_ctx *ctx, int i, const char *lang);
/* true when this build's engine can instantiate the layout. */
bool hunmin_ctx_layout_supported(const hunmin_ctx *ctx, int i);
/* "hangul" | "latin" — which half of a two-mode IME this layout belongs to,
 * read from the layout JSON's "mode" field (docs/SPEC-SCHEMA.md). NULL for an
 * out-of-range index. A layout without the field reports "hangul", and so does
 * one whose value is not one of the two; tools/doctor.py is what makes a
 * misspelling fail the build instead. */
const char *hunmin_ctx_layout_mode(const hunmin_ctx *ctx, int i);

/* Returns NULL for unknown id or unsupported engine (e.g. romaja until P1b+). */
hunmin_ic *hunmin_ic_new(hunmin_ctx *ctx, const char *layout_id);
void hunmin_ic_free(hunmin_ic *ic);

/* Feed one US-QWERTY character event: case encodes shift, 0x08 is
 * backspace (identical convention to libhangul / tests/golden traces).
 * Returns true when consumed by the engine. */
bool hunmin_ic_process(hunmin_ic *ic, uint32_t ch);

/* Feed one key event as base character + explicit modifier (issue #90,
 * docs/IC-INPUT-OPTIONS.md §2-B option B). base is the *unshifted* US-QWERTY
 * character; shift is nonzero when Shift is held. The engine owns the case
 * endpoint: it reuses the loader's own US-QWERTY shift map, so no host parses
 * shift as a character bit — a frontend passes unshifted + the bit and the
 * CapsLock×Shift decision moves here (docs/SPEC-P2 §"IC input entry points").
 *
 * The dual-convention rule (specified in SPEC-P2, not invented per-host): a
 * character that is already the shifted form of a key (an uppercase letter, or
 * a shifted punctuation — a fcitx5-shaped host) is treated as already
 * resolved, and a shift bit on it selects nothing further: ch reaches the
 * engine as-is. Equivalently, the shift bit is only honoured when the base
 * character has a shifted form under the loader's map.
 *
 * The legacy case-encoding call hunmin_ic_process stays byte-for-byte valid:
 * fcitx5 and the tier C trace runners use it. The Android adapter passes
 * base + shift through this call instead (issue #105). */
bool hunmin_ic_process_key(hunmin_ic *ic, uint32_t base, uint32_t shift);

/* Predicts whether hunmin_ic_process_key(ic, base, shift) would be consumed,
 * WITHOUT changing ic: the return value is the same, and the commit, preedit,
 * tier and option state are left exactly as they were. A frontend that must
 * answer a platform "test" phase before the real key event uses this; it is a
 * read of the engine state, not a second engine and not a queued event. */
HUNMIN_EXPORT bool hunmin_ic_peek_key(const hunmin_ic *ic, uint32_t base, uint32_t shift);

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

void hunmin_ic_reset(hunmin_ic *ic); /* drop composition silently */
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

/* --- Shared buffered / syllable preedit layer (issue #128)
 *
 * One level above hunmin_ic, not a second engine. It owns the word buffer and
 * answers, for every frontend, the three questions the invariant in
 * docs/FRONTEND-EQUIVALENCE.md §1 is stated over: is this key consumed, what is
 * committed, what is the preedit. Frontends only map their physical key to
 * (base, shift) and render what this layer hands them.
 *
 * Two commit modes, identical text: BUFFERED holds the finalised syllables and
 * commits the whole word at the boundary; SYLLABLE commits each one as the
 * engine finalises it. The commit BOUNDARY is not part of the contract; the
 * concatenation is.
 *
 * A declined key is itself the boundary (the engine finalises the syllable it
 * held, and the key passes to the client) - so a frontend never has to know
 * when a word ends.
 */
typedef struct hunmin_layer hunmin_layer;

#define HUNMIN_COMMIT_BUFFERED 0
#define HUNMIN_COMMIT_SYLLABLE 1

/* Which layout the layer is composing with (docs/FRONTEND-EQUIVALENCE.md §4). */
#define HUNMIN_MODE_HANGUL 0
#define HUNMIN_MODE_LATIN 1

/* What one key produced. commit is what to insert now ("" if nothing), preedit
 * is what to display ("" if nothing). Both strings are owned by the layer and
 * stay valid until the next hunmin_layer_* call on it - the same lifetime rule
 * as hunmin_ic_commit(). */
typedef struct hunmin_layer_out {
    int consumed;
    const char *commit;
    const char *preedit;
} hunmin_layer_out;

/* hangul_layout_id is required; latin_layout_id may be NULL for a frontend
 * with one language (then hunmin_layer_mode_change reports "not ours").
 * Returns NULL for an unknown/unsupported layout or a bad commit_mode. */
HUNMIN_EXPORT hunmin_layer *hunmin_layer_new(hunmin_ctx *ctx, const char *hangul_layout_id, const char *latin_layout_id, int commit_mode);
HUNMIN_EXPORT void hunmin_layer_free(hunmin_layer *l);

/* Feed one key. Returns 1 when consumed. out may be NULL when the caller only
 * needs the decision (a platform "test" phase). */
HUNMIN_EXPORT int hunmin_layer_key(hunmin_layer *l, uint32_t base, uint32_t shift, hunmin_layer_out *out);

/* Predict hunmin_layer_key()'s consumed answer without changing any state. */
HUNMIN_EXPORT int hunmin_layer_peek_key(const hunmin_layer *l, uint32_t base, uint32_t shift);

/* Commit the buffered word and the in-flight syllable now (focus loss, a
 * mode switch, an edit key). Returns the text to insert, "" if none. */
HUNMIN_EXPORT const char *hunmin_layer_flush(hunmin_layer *l);
HUNMIN_EXPORT void hunmin_layer_reset(hunmin_layer *l);
HUNMIN_EXPORT bool hunmin_layer_set_option(hunmin_layer *l, const char *name, int value);

/* The Han/Eng action (docs/FRONTEND-EQUIVALENCE.md §4): one abstract event, so
 * an adapter's raw platform code never reaches the engine. It is a boundary -
 * whatever was composing in the old language is committed before the switch.
 * Returns 1 when the mode changed; 0 when this frontend has one language only
 * (the boundary commit still lands in out). */
HUNMIN_EXPORT int hunmin_layer_mode_change(hunmin_layer *l, hunmin_layer_out *out);
HUNMIN_EXPORT int hunmin_layer_mode(const hunmin_layer *l); /* HUNMIN_MODE_* */
HUNMIN_EXPORT int hunmin_layer_commit_mode(const hunmin_layer *l);
HUNMIN_EXPORT const char *hunmin_layer_layout(const hunmin_layer *l); /* active layout id */

#ifdef __cplusplus
}
#endif
#endif
