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

/* --- Shared buffered / syllable preedit object (issue #128)
 *
 * One level above hunmin_ic, not a second engine. It owns the word buffer and
 * answers, for every frontend, the three questions the invariant in
 * docs/FRONTEND-EQUIVALENCE.md §1 is stated over: is this key consumed, what is
 * committed, what is the preedit. Frontends only map their physical key to
 * (base, shift) and render what this layer hands them.
 *
 * Two commit policies, identical text: BUFFERED holds the finalised syllables and
 * commits the whole word at the boundary; SYLLABLE commits each one as the
 * engine finalises it. The commit BOUNDARY is not part of the contract; the
 * concatenation is.
 *
 * A declined key is itself the boundary (the engine finalises the syllable it
 * held, and the key passes to the client) - so a frontend never has to know
 * when a word ends.
 */
typedef struct hunmin_buffered_preedit hunmin_buffered_preedit;

#define HUNMIN_COMMIT_AT_BOUNDARY 0
#define HUNMIN_COMMIT_PER_SYLLABLE 1

/* Which layout the layer is composing with (docs/FRONTEND-EQUIVALENCE.md §4). */
#define HUNMIN_LANGUAGE_HANGUL 0
#define HUNMIN_LANGUAGE_LATIN 1

/* What one key produced. commit is what to insert now ("" if nothing), preedit
 * is what to display ("" if nothing). Both strings are owned by the layer and
 * stay valid until the next hunmin_buffered_preedit_* call on it - the same lifetime rule
 * as hunmin_ic_commit(). */
typedef struct hunmin_key_result {
    int consumed;
    const char *commit;
    const char *preedit;
} hunmin_key_result;

/* hangul_layout_id is required; latin_layout_id may be NULL for a frontend
 * with one language (then hunmin_buffered_preedit_toggle_language reports "not ours").
 * Returns NULL for an unknown/unsupported layout or a bad commit_policy. */
HUNMIN_EXPORT hunmin_buffered_preedit *hunmin_buffered_preedit_new(hunmin_ctx *ctx, const char *hangul_layout_id, const char *latin_layout_id, int commit_policy);
HUNMIN_EXPORT void hunmin_buffered_preedit_free(hunmin_buffered_preedit *l);

/* Feed one key. Returns 1 when consumed. out may be NULL when the caller only
 * needs the decision (a platform "test" phase). */
HUNMIN_EXPORT int hunmin_buffered_preedit_process_key(hunmin_buffered_preedit *l, uint32_t base, uint32_t shift, hunmin_key_result *out);

/* Predict hunmin_buffered_preedit_process_key()'s consumed answer without changing any state. */
HUNMIN_EXPORT int hunmin_buffered_preedit_peek_key(const hunmin_buffered_preedit *l, uint32_t base, uint32_t shift);

/* Commit the buffered word and the in-flight syllable now (focus loss, a
 * language toggle, an edit key). Returns the text to insert, "" if none. */
HUNMIN_EXPORT const char *hunmin_buffered_preedit_commit_word(hunmin_buffered_preedit *l);

/* The display string for the word in flight, without consuming a key: a
 * candidate window or a Hanja lookup asks between keys. Same lifetime rule as
 * above. */
HUNMIN_EXPORT const char *hunmin_buffered_preedit_display(hunmin_buffered_preedit *l);

/* The text the LAST hunmin_buffered_preedit_* call delivered for the client,
 * "" when nothing. It is the out.commit field of that call, kept readable so a
 * binding that cannot return a struct through its own ABI (JNI: the Android
 * adapter) reads the same answer a C frontend gets. */
HUNMIN_EXPORT const char *hunmin_buffered_preedit_delivered(const hunmin_buffered_preedit *l);
HUNMIN_EXPORT void hunmin_buffered_preedit_reset(hunmin_buffered_preedit *l);
HUNMIN_EXPORT bool hunmin_buffered_preedit_set_option(hunmin_buffered_preedit *l, const char *name, int value);

/* External text (voice input, per PLAN.md invariant 5) through the layer: the
 * buffered word is delivered first, then the engine's pass-through appends the
 * text, so the order is "typed, then spoken" and nothing is composed twice.
 * Returns the concatenation. */
HUNMIN_EXPORT const char *hunmin_buffered_preedit_external_text(hunmin_buffered_preedit *l, const char *text);

/* --- The key rule (issue #128, decision 2026-10-25) ---
 *
 * Precedence is BEHAVIOUR, not presentation, so it lives here instead of being
 * spelled out in each adapter: three frontends used to carry three copies of
 * "a Ctrl chord is a boundary and the key passes", "an unmapped key is a
 * boundary", "the Han/Eng key is the language action", and each copy had its own
 * chance to drift.
 *
 * What stays with the frontend is everything the contract calls a frontend
 * difference (docs/FRONTEND-EQUIVALENCE.md §3): mapping a platform key to
 * (base, shift, mods), and saying whether a candidate list it owns is open and
 * whether this key is one that list consumes. The rule then answers what the
 * shared layer must do with it.
 */
typedef enum hunmin_key_action {
    /* Ours: feed it and take the commit/preedit the layer produces. */
    HUNMIN_KEY_FEED = 0,
    /* The word is finished (or the key is not ours): deliver the word now and
     * let the key reach the client. */
    HUNMIN_KEY_BOUNDARY_PASS = 1,
    /* The Han/Eng action: a boundary, then the other language takes over. */
    HUNMIN_KEY_LANGUAGE_TOGGLE = 2,
    /* A key of an open candidate list: the frontend routes it to the list. */
    HUNMIN_KEY_CANDIDATE = 3,
    /* The Han/Eng key pressed WITH a chord modifier held: the compound press,
     * which carries the frontend's special functions instead of toggling the
     * language (docs/SPEC-MODES.md §E5, ruled 2026-10-31). Appended after
     * CANDIDATE rather than inserted, because the values are ABI.
     *
     * SHIFT is admitted HERE and only here: with an ordinary character it
     * selects a layout row (`HUNMIN_MOD_SHIFT` is not part of the chord set),
     * while Shift + the language key is a special combination by ruling
     * (2026-10-31: "if it is used with normal characters, work as capitalize.
     * but with special combination such as shift + han/eng, it should be able
     * to do special work"). A binding on SHIFT alone therefore only ever fires
     * from the language key; no character event can reach it.
     *
     * An unbound chord stays the plain toggle, so a platform with nothing bound
     * never ends up with a dead Shift+한/영. */
    HUNMIN_KEY_LANGUAGE_CHORD = 4
} hunmin_key_action;

/* The modifier vocabulary a frontend maps its own state onto. SHIFT is
 * deliberately separate from the chord set: it selects a layout row, it does
 * not make the key the application's. */
#define HUNMIN_MOD_CTRL (1u << 0)
#define HUNMIN_MOD_ALT (1u << 1)
#define HUNMIN_MOD_GUI (1u << 2) /* Super / Win / Command */
#define HUNMIN_MOD_SHIFT (1u << 3)
#define HUNMIN_MOD_CHORD (HUNMIN_MOD_CTRL | HUNMIN_MOD_ALT | HUNMIN_MOD_GUI)

/* Keys that carry no text character. The layer names them so the candidate
 * list's key set can be shared instead of re-spelled per frontend; everything
 * else travels as `base` (the unshifted US-QWERTY character, 0x08 for
 * BackSpace, 0 when the platform key maps to nothing). */
typedef enum hunmin_key_id {
    HUNMIN_KEY_TEXT = 0, /* a character: read base */
    HUNMIN_KEY_UP = 1,
    HUNMIN_KEY_DOWN = 2,
    HUNMIN_KEY_LEFT = 3,
    HUNMIN_KEY_RIGHT = 4,
    HUNMIN_KEY_PAGE_UP = 5,
    HUNMIN_KEY_PAGE_DOWN = 6,
    HUNMIN_KEY_ENTER = 7,
    HUNMIN_KEY_ESCAPE = 8
} hunmin_key_id;

/* One key event, already translated into the shared vocabulary. */
typedef struct hunmin_key_event {
    uint32_t base; /* unshifted US-QWERTY character; 0 = the platform mapped nothing */
    uint32_t shift; /* nonzero when Shift is held (the engine's shift bit) */
    uint32_t mods; /* HUNMIN_MOD_* */
    hunmin_key_id id; /* HUNMIN_KEY_TEXT for a character event */
    /* The one thing only the frontend knows: whether ITS candidate window is
     * open. The keys that window consumes are named here, not there. */
    int candidates_open;
    /* The frontend matched its Han/Eng binding for this key. */
    int language_key;
} hunmin_key_event;

/* The precedence, in one place: language key, then the open candidate list's
 * keys, then a chord, then an unmapped key, then feed. */
HUNMIN_EXPORT hunmin_key_action hunmin_buffered_preedit_classify(const hunmin_buffered_preedit *l, const hunmin_key_event *ev);

/* ------------------------------------------------------- special functions */
/* The compound press (a chord modifier held with the Han/Eng key) carries a
 * named function instead of switching languages. The maintainer's words, which
 * this shape implements (2026-10-31): "each frontend can have default key code
 * and custom ones and provide it to engine. then engine processes them and
 * invoke frontend side interface which implements the feature or processes by
 * itself. if the feature is not supported at the platform, it can just refuse
 * to service."
 *
 * So the split is: the FRONTEND owns the platform key codes and the feature
 * implementations; the ENGINE owns the binding (function -> chord bits), the
 * resolution of one key event to one function, and the refusal. The engine
 * never sees a key code: it sees hunmin_key_event, already translated. */

typedef enum hunmin_special_function {
    /* Not a function: the event was not a bound compound press. */
    HUNMIN_SPECIAL_NONE = 0,
    /* Start voice input. Engine-blind: whoever can record implements it. */
    HUNMIN_SPECIAL_VOICE = 1,
    /* Flip the automatic Hangul/English switch (R13-정정's state). */
    HUNMIN_SPECIAL_AUTOSWITCH = 2,
    /* Bound for the table; append new functions ABOVE this line and bump it,
     * because the values are ABI and the table is sized by it. */
    HUNMIN_SPECIAL_LIMIT = 3
} hunmin_special_function;

/* What a frontend must provide for the engine to service a function: whether it
 * can (0 = refuse, and that is a legal answer -- a platform without a recorder
 * refuses VOICE rather than pretending) and how to do it. Either may be NULL;
 * a NULL supports means "yes", a NULL perform means "resolved, nothing done". */
typedef struct hunmin_special_host {
    void *user;
    int (*supports)(void *user, hunmin_special_function fn);
    void (*perform)(void *user, hunmin_special_function fn);
} hunmin_special_host;

/* Bind fn to a chord (HUNMIN_MOD_CTRL/ALT/GUI, or SHIFT for the Shift+한/영
 * combination -- Shift is a layout row for characters but a special combination
 * with the language key, by the 2026-10-31 ruling). mods == 0 unbinds. The engine
 * ships the ruled defaults (Table A: VOICE = Alt, AUTOSWITCH = Ctrl) so the three
 * frontends cannot disagree about a table nobody seeded; a frontend calls this
 * only to apply its own store, which is what keeps the value in ONE place on both
 * sides -- the platform keeps its key codes, the engine keeps the assignment. */
HUNMIN_EXPORT bool hunmin_buffered_preedit_special_bind(hunmin_buffered_preedit *l, hunmin_special_function fn, unsigned mods);
/* The chord currently bound to fn, or 0. This is what a settings dialog shows,
 * and on a fresh layer it is the engine's default. */
HUNMIN_EXPORT unsigned hunmin_buffered_preedit_special_chord(const hunmin_buffered_preedit *l, hunmin_special_function fn);
/* Register the implementation. NULL withdraws it (and then nothing is serviced). */
HUNMIN_EXPORT void hunmin_buffered_preedit_special_host(hunmin_buffered_preedit *l, const hunmin_special_host *host);

/* Resolve one key event and service it. Returns the function performed (> 0),
 * 0 when the event is not a bound compound press, or -1 when it IS one and
 * nothing could service it -- no host, or the host refused. The frontend passes
 * the key through on -1, which is how an unsupported platform degrades instead
 * of swallowing the press. Pure resolution first: the classifier stays pure, and
 * this is the one call that can invoke the host. */
HUNMIN_EXPORT int hunmin_buffered_preedit_special(hunmin_buffered_preedit *l, const hunmin_key_event *ev);

/* Predicts hunmin_buffered_preedit_special() WITHOUT calling the host: the same
 * three answers (function > 0, 0 not a chord, -1 bound but nothing would service
 * it), and no state changes. A frontend that must answer a platform "test" phase
 * before the real key event uses this, because test and down are one decision:
 * claiming a key in the test and passing it in the down loses it, which is a
 * measured defect class here (`dkssud1`, the interim default-sink host). Same
 * role as hunmin_ic_peek_key() and hunmin_buffered_preedit_peek_key(). */
HUNMIN_EXPORT int hunmin_buffered_preedit_special_peek(const hunmin_buffered_preedit *l, const hunmin_key_event *ev);

/* Stable identifier for a function ("voice", "autoswitch", "" for NONE). The
 * dialogs and the logs use this instead of spelling a name per frontend. */
HUNMIN_EXPORT const char *hunmin_special_name(hunmin_special_function fn);

/* The finished syllables alone, REMOVED from the layer (the in-flight syllable
 * stays composing). The interim delivery keeps the finished word as document
 * text and appends only what has newly finished, so it needs the two halves
 * separately and needs this one to advance. */
HUNMIN_EXPORT const char *hunmin_buffered_preedit_take_word(hunmin_buffered_preedit *l);

/* The syllable in flight alone, without the finished word and without a key.
 * The interim delivery mode keeps the finished word as document text and
 * rewrites only this, so the two halves the display joins have to be readable
 * separately (docs/FRONTEND-EQUIVALENCE.md §3.3). */
HUNMIN_EXPORT const char *hunmin_buffered_preedit_composing(hunmin_buffered_preedit *l);

/* The Han/Eng action (docs/FRONTEND-EQUIVALENCE.md §4): one abstract event, so
 * an adapter's raw platform code never reaches the engine. It is a boundary -
 * whatever was composing in the old language is committed before the switch.
 * Returns 1 when the mode changed; 0 when this frontend has one language only
 * (the boundary commit still lands in out). */
HUNMIN_EXPORT int hunmin_buffered_preedit_toggle_language(hunmin_buffered_preedit *l, hunmin_key_result *out);
HUNMIN_EXPORT int hunmin_buffered_preedit_language(const hunmin_buffered_preedit *l); /* HUNMIN_LANGUAGE_* */
HUNMIN_EXPORT int hunmin_buffered_preedit_commit_policy(const hunmin_buffered_preedit *l);
HUNMIN_EXPORT const char *hunmin_buffered_preedit_layout_id(const hunmin_buffered_preedit *l); /* active layout id */

#ifdef __cplusplus
}
#endif
#endif
