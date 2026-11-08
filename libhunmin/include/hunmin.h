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
/* Whether the MODES JUDGEMENT can decide on this layout at all. 1 when the
 * layout composes Hangul — i.e. its keys emit conjoining jamo — and 0 for the
 * ones that do not (the Latin half: us-qwerty, dvorak, colemak), where a buffer
 * has no syllables for the judgement to be right or wrong about.
 *
 * Derived at load time from the layout's own keys and combination table, never
 * declared in the file: a new layout cannot forget to say so, and the answer
 * cannot drift from the data it is about. */
HUNMIN_EXPORT bool hunmin_ctx_layout_autoswitch_capable(const hunmin_ctx *ctx, int i);

/* B3 data gate for enabling autoswitch (SPEC-MODES ReqMode13;
 * DESIGN-CORE.md#b3-autoswitch-data-gate-and-default). 1 if hm_data_dirs finds
 * a usable modes word list (cached), else 0. Distinct from
 * hunmin_ctx_layout_autoswitch_capable() (layout emits jamo; no data). Pair
 * with get_option("autoswitch") for "off, chosen" vs "off, data missing". */
HUNMIN_EXPORT bool hunmin_ctx_autoswitch_data_present(const hunmin_ctx *ctx);

/* 1 when this layout can emit a jamo outside the modern 19/21/28 — the 옛글
 * layouts. Judge those with hunmin_hangul_valid_old() and everything else with
 * hunmin_hangul_valid(); using the wide rule where the layout cannot produce
 * such a buffer only makes the check weaker.
 *
 * Two routes reach the archaic jamo and both are measured from the data, not
 * assumed: a key row that maps straight to one (dubeolsik-yet's shift row,
 * sin3-p2-basic's 아래아 ᆞ) and a combination table that produces one
 * (sin3-p2-yet, whose keys are modern). */
HUNMIN_EXPORT bool hunmin_ctx_layout_old_hangul(const hunmin_ctx *ctx, int i);

/* true when this build's engine can instantiate the layout. */
HUNMIN_EXPORT bool hunmin_ctx_layout_supported(const hunmin_ctx *ctx, int i);
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

/* "caps" -> Caps Lock state for expression var P (yethangul modes).
 * "w"/"W"/"z"/"h"/"H" -> 전처리수식 override (pat.im/1136 §2-6), cleared by
 * hunmin_ic_reset (layout defaults return).
 * "autoswitch" -> ReqMode13 automatic Hangul/English switch state (SPEC-MODES
 * ReqMode13): OFF on a fresh ic, NOT cleared by hunmin_ic_reset (preference).
 * ON is refused (false, value left 0) when hunmin_ctx_autoswitch_data_present()
 * is 0 (B3 gate; DESIGN-CORE.md#b3-autoswitch-data-gate-and-default); OFF is
 * always accepted. Distinguish refusal from unknown name via get_option(). */
bool hunmin_ic_set_option(hunmin_ic *ic, const char *name, int value);

/* Reads back what hunmin_ic_set_option() writes; the same names, one home. The
 * frontends' settings surfaces read the value through this instead of keeping a
 * copy (ReqMode13: a per-frontend settings store is what this state exists to
 * avoid). Returns false for a name the engine does not know and leaves *out
 * untouched; the w/W/z/H/h overrides answer -1 for "not overridden, the layout
 * default", which is a value and not an unknown name. */
bool hunmin_ic_get_option(const hunmin_ic *ic, const char *name, int *out);

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

/* The layer's options: the ic's own ("caps", "autoswitch") pass through, and one
 * is the layer's. "word_cap" is the cap on the word this object holds for one
 * word, in BYTES of UTF-8 (docs/SPEC-MODES.md §ReqMode4, ruled 2026-11-01): when
 * the word plus the syllable still composing would exceed it, the word's first
 * half is delivered as that key's commit and the tail keeps composing, so what a
 * frontend must render stays bounded. 0 means no cap, which is what the default
 * was before the ruling. A lowered value applies from the next key. "judge_timing"
 * selects WHEN the modes judgement is shown (docs/SPEC-MODES.md §E6, ruled
 * 2026-11-01): 1 = at every key, so the word flips language while it is typed (the
 * flicker, and the default the maintainer chose), 0 = at the boundary only, so the
 * word stays in the current mode until the delimiter. It changes the display and
 * never the delivered text -- the word is committed once, in the language the
 * boundary decides -- and only those two values are accepted. Returns false for an
 * unknown name or a value out of range. */
HUNMIN_EXPORT bool hunmin_buffered_preedit_set_option(hunmin_buffered_preedit *l, const char *name, int value);

/* The layer's read of the same options: ReqMode13's settings surface asks this for
 * "autoswitch" and shows the answer, instead of keeping a copy of a state the
 * engine owns. Same contract as hunmin_ic_get_option() -- false for an unknown
 * name, *out untouched. The key that toggles it is
 * hunmin_buffered_preedit_special_chord(l, HUNMIN_SPECIAL_AUTOSWITCH), which is
 * also the engine's (a frontend never spells the chord itself). */
HUNMIN_EXPORT bool hunmin_buffered_preedit_get_option(const hunmin_buffered_preedit *l, const char *name, int *out);

/* Why autoswitch is off on this layer, if it is off for a reason the engine
 * itself imposes (issue #207): under HUNMIN_COMMIT_PER_SYLLABLE the buffer
 * delivers per syllable and autoswitch has nothing to hold, so the state is
 * forced off and this returns a string naming the policy. Returns NULL when
 * autoswitch is off for some other reason (a choice, or missing data) or when
 * it is on, so a frontend logs the line only where the reason applies. The
 * string is owned by the engine and lives as long as the process. */
HUNMIN_EXPORT const char *hunmin_buffered_preedit_autoswitch_reason(const hunmin_buffered_preedit *l);
HUNMIN_EXPORT const char *hunmin_buffered_preedit_get_autoswitch_reason(const hunmin_buffered_preedit *l);

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
    /* Flip the automatic Hangul/English switch (ReqMode13's state). */
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

/* ReqMode8's judgement, as the ENGINE's answer and the FRONTEND's action (ruled
 * 2026-11-01, issue #164: the engine judges, the frontend performs the switch).
 * Read it immediately after the key that ended a word; HUNMIN_LANGUAGE_* is the
 * mode to adopt now, and -1 means there is nothing to act on.
 *
 * The word has already been delivered in the judged language -- in
 * out->commit of that key, or from hunmin_buffered_preedit_commit_word -- since
 * ReqMode10 forbids reassembling text that is already in the client and the boundary
 * is the last moment before it gets there. So an adapter does exactly two
 * things: deliver the text it was handed, then switch its mode if this answers
 * a language. The switch itself stays the frontend's because it is the one half
 * a host can own: fcitx5's trigger does not belong to the addon (§FE13).
 *
 * Answers -1 (and the delivery is byte-identical to a build without this
 * feature) when: `autoswitch` is off (the default, B3); no word finished; the
 * word was shorter than ReqMode9's guard of three key inputs; the Hangul layout
 * cannot compose syllables (ReqMode8); no English dictionary is loaded (B3);
 * both readings agreed, which is ReqMode8's "keep the current mode"; or the layer is
 * in HUNMIN_COMMIT_PER_SYLLABLE, where the word's earlier syllables are already
 * in the client.
 *
 * Consuming: read once. A second read answers -1, and every later boundary
 * replaces the answer, so a mode cannot be applied twice or late. */
HUNMIN_EXPORT int hunmin_buffered_preedit_take_judgement(hunmin_buffered_preedit *l);
HUNMIN_EXPORT int hunmin_buffered_preedit_commit_policy(const hunmin_buffered_preedit *l);
HUNMIN_EXPORT const char *hunmin_buffered_preedit_layout_id(const hunmin_buffered_preedit *l); /* active layout id */

/* The LATIN READING of the word that last finished: the same keys replayed
 * through this layer's Latin layout (latin_layout_id at construction), which is
 * how ReqMode8's `english_valid` gets "the word the buffer would have produced under
 * the Latin layout". A Hangul layout cannot produce Latin -- it destroys the
 * text instead of transliterating it (measured: the 12 Hangul layouts emit no
 * Latin letter for any printable key -- hunmin-sim, every key id) -- so the keys are the only way back.
 *
 * Read it at the boundary, where the frontend already learns the word finished
 * (a declined key, `hunmin_buffered_preedit_commit_word`), and hand it to
 * `hunmin_buffered_preedit_english_valid`. Empty means "nothing to look up":
 * while a word is still composing, after a boundary that ended no word, and for
 * a layer built with one language only -- so a caller never looks up a stale
 * word, and a frontend without a Latin layout answers "not English" rather
 * than pretending (B3's fail-closed state, not an error).
 *
 * It decides nothing: no switch, no delivery, no mode change (ReqMode10). Valid until
 * the next call on this layer. Cost: one replay per finished word -- measured
 * 0.97-1.52 us for a 6-key word with the reading on against 0.54-0.67 us with
 * no Latin layout (three runs of 20,000 words each), so the reading itself is
 * ~0.5-0.9 us against a keystroke's 16 ms budget. */
HUNMIN_EXPORT const char *hunmin_buffered_preedit_latin_text(const hunmin_buffered_preedit *l);

/* ---------------------------------------------- english words (ReqMode8)
 *
 * The data half of the modes judgement: `english_valid` is "a lookup in the
 * ENGLISH DICTIONARY" (docs/SPEC-MODES.md ReqMode8), and the maintainer's ruling
 * of 2026-11-01 fixed the source -- hunspell en_US, with a user dictionary the
 * person can extend. docs/OPTIONS-MEASURED.md §EDW is the measurement behind
 * that choice.
 *
 * TWO SOURCES, ONE SET. The shipped list and the user's words are looked up
 * together, so `..._english_valid` answers without asking which file a word
 * came from. Loading is lazy (status 0 = untried, 1 = loaded, -1 = absent),
 * because B3 ships this data separately: an absent base dictionary is a normal
 * state in which the judgement must answer "not english", not fail.
 *
 * The USER file is plain text, one word per line, `#` for comments -- a person
 * edits it. It is deliberately not the JSON the shipped list uses.
 *
 * Reloading is the CALLER's decision: there is no file watching and no stat per
 * lookup, so a frontend calls ..._load_user_words again when its setting
 * changes. Add a word at runtime with ..._add_user_word and persist it in the
 * frontend's own store -- that is the "editing" half of the same mechanism.
 *
 * Nothing here decides to switch modes; the judgement that consumes it is not
 * implemented yet (issue: modes judgement).
 */

/* Path to the shipped list (libhunmin/data/modes/en-US.json). Returns 1 loaded,
 * -1 absent, 0 malformed. An absent file leaves the set empty and not ready. */
HUNMIN_EXPORT int hunmin_buffered_preedit_load_english(hunmin_buffered_preedit *l, const char *path);

/* The same load, but the SHIPPED list is FOUND rather than named: the search is
 * the one the hanja/krdict/idioms tables already use (the context's data dirs,
 * else HUNMIN_DATA_PATH, else the build default -- hm_data_dirs), so a frontend
 * never assembles "<dir>/modes/en-US.json" itself and a dictionary installed
 * beside the other data is found from any of them. Two searches are how two
 * data directories start to disagree, which is why this exists and why the
 * path-taking call above stays for a caller that has a path already.
 *
 * Returns 1 loaded, -1 when no searched directory holds the file (a normal
 * state under B3: the feature is off, and every lookup answers 0), 0 when the
 * file is found and malformed -- an installed-but-broken dictionary is an error,
 * never "no english words exist". */
HUNMIN_EXPORT int hunmin_buffered_preedit_load_english_shipped(hunmin_buffered_preedit *l);

/* Add the user's words from a text file (one per line, `#` comments). Returns
 * 1 loaded, -1 absent (also normal), 0 error. Merges; it does not replace. */
HUNMIN_EXPORT int hunmin_buffered_preedit_load_user_words(hunmin_buffered_preedit *l, const char *path);

/* Add one word at runtime (the frontend's editing path). Returns 1 accepted. */
HUNMIN_EXPORT int hunmin_buffered_preedit_add_user_word(hunmin_buffered_preedit *l, const char *word);

/* 1 when the word is in the shipped list or the user's words, 0 otherwise --
 * including while no dictionary is loaded, so a caller can ask before it has
 * decided whether the feature is available. */
HUNMIN_EXPORT int hunmin_buffered_preedit_english_valid(const hunmin_buffered_preedit *l, const char *word);

/* 1 when the shipped list is loaded, i.e. when the feature's data is present
 * (B3's gate). -1 is never reported here: audit that with the load call. */
HUNMIN_EXPORT int hunmin_buffered_preedit_english_ready(const hunmin_buffered_preedit *l);

/* ------------------------------------------- hangul validity (ReqMode8)
 *
 * The OTHER half of the modes judgement: `hangul_valid` is "the buffer composes
 * as Korean syllables of the shape (choseong + jungseong) or (choseong +
 * jungseong + jongseong) only" (docs/SPEC-MODES.md §ReqMode8, issue #166).
 *
 * It takes TEXT, not the layer, because there is nothing to look up: it is a
 * predicate over the composition rule, so no data directory can change its
 * answer and it is the same function for the fcitx5 adapter, Android, TSF and
 * a test. Pass a finished buffer (`..._preedit()` or `..._display()`), not a
 * sentence -- a space, digit, latin letter, punctuation or an in-flight jamo
 * all answer 0, since none of them is a well-formed syllable.
 *
 * Old Hangul answers 0 by construction: the composer's syllable step accepts
 * only the modern 19/21/28, so an archaic jongseong is rendered as jamo. The
 * four 옛글 layouts (dubeolsik-yet, sebeolsik-yet, ahnmatmae, sin3-p2-yet) are
 * therefore not hangul-valid -- the literal reading of "well-formed MODERN
 * syllable", and the single place to change it if the judgement should treat
 * old-Hangul buffers as Hangul.
 *
 * 0 for NULL and for malformed UTF-8. 1 for one syllable or a run of them.
 */
HUNMIN_EXPORT int hunmin_hangul_valid(const char *text);

/* The same shape rule with the EXTENDED (옛한글) jamo sets, for the layouts that
 * compose them — ask `hunmin_ctx_layout_old_hangul` which rule applies, and use
 * this one exactly there.
 *
 * The rule is unchanged; only the sets widen. A group is still (cho)(jung)(jong)
 * and a leading choseong is still required, so:
 *
 *   valid     `ᅀᅡ`  `ᅌᅵ`  `ᅀᅡᆶ`  `ᄀᆞ`   cho+jung — what the 옛글 layouts render for a
 *                                        finished syllable, since no precomposed
 *                                        form exists for these
 *   invalid   `랴ㅜㄷ`  `랴ㅜ`  `ㅜㄷ`     a jungseong with no leading cho is not a
 *                                        shape the composer emits (it writes the
 *                                        CF filler U+115F instead)
 *   invalid   `ᅀ`  `ㄱ`  `ㆍ`             an in-flight syllable: a cho with no jung
 *
 * Modern precomposed syllables are accepted here too, so this is a superset of
 * the function above; a caller that always used it would only lose the check
 * that a modern layout cannot produce jamo buffers in the first place.
 * 0 for NULL, for empty, for malformed UTF-8, and for anything that contains a
 * latin letter, digit, space or punctuation.
 */
HUNMIN_EXPORT int hunmin_hangul_valid_old(const char *text);

#ifdef __cplusplus
}
#endif
#endif
