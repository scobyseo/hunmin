/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef HUNMIN_IBUS_STATE_H_
#define HUNMIN_IBUS_STATE_H_

/*
 * The IBus engine's key path, host-independently.
 *
 * This file is the D1 boundary (issue #288): nothing here knows what IBus is.
 * It takes one already-translated hunmin_key_event, owns the shared
 * hunmin_buffered_preedit layer, and asks a hunmin_state_sink to show or
 * deliver what the layer produced. The engine object in hunmin_engine.c
 * implements that sink with libibus calls and does nothing else.
 *
 * Why the split exists at all: docs/DESIGN-FRONTEND.md §11 puts the key path's
 * decisions in the core and the host action in the frontend, and this frontend
 * has two hosts to answer for -- the IBus D-Bus surface a panel draws, and a
 * fake sink a test drives with no bus, no daemon and no display. Keeping the
 * second one possible is what makes the first one testable at tier D1.
 *
 * Text lifetimes: every string a sink receives is valid only for the duration
 * of that call. Copy it if it outlives the callback (the engine does).
 */

#include <stdbool.h>

#include <hunmin.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hunmin_state hunmin_state;

typedef struct hunmin_state_sink {
    void *user;
    /* Deliver text to the client. Never called with an empty string. */
    void (*commit)(void *user, const char *text);
    /* Show the composition. `cursor` is a character (not byte) index and is
     * never negative; text is "" when the preedit must be cleared. */
    void (*preedit)(void *user, const char *text, int cursor);
    /* The candidate list opened or closed. The content is read back through
     * hunmin_state_candidate_*() -- the same accessors the D1 sink uses, so
     * one list has one home. */
    void (*candidates)(void *user, int open);
} hunmin_state_sink;

/* dirs may be NULL/0, in which case the engine resolves HUNMIN_DATA_PATH and
 * then its compiled-in data directory, exactly as hunmin_ctx_new() documents.
 * NULL when no layout loads. */
hunmin_state *hunmin_state_new(const char *const *dirs, int ndirs);
void hunmin_state_free(hunmin_state *s);

/* The sink is copied; it must stay valid until replaced or until free. */
void hunmin_state_set_sink(hunmin_state *s, const hunmin_state_sink *sink);

/* The layout ids are the data's own (`dubeolsik`, `sin3-p2`, `us-qwerty`, ...).
 * A NULL latin id keeps the current Latin half; an EMPTY one makes the layer
 * single-language. A change flushes the word in flight before the new layer is
 * built. Returns false when the layer did not load. */
bool hunmin_state_set_layouts(hunmin_state *s, const char *hangul_id, const char *latin_id);
bool hunmin_state_set_commit_policy(hunmin_state *s, int policy);

/* Set the active language (HUNMIN_LANGUAGE_*), not toggle it. The layer exposes
 * a toggle; this performs it only when the state differs, so a caller stating a
 * mode per case cannot depend on what ran before it (the mistake the fcitx5
 * corpus lane records, issue #233). A word in flight is delivered first, as a
 * toggle's boundary requires. Returns the resulting language. */
int hunmin_state_set_language(hunmin_state *s, int language);

/* ReqMode13's on/off switch, pushed into the live layer and read back: the
 * return value is what the ENGINE holds, not what was asked for, so a settings
 * surface cannot claim a state the engine refused (B3 data gate). */
int hunmin_state_set_autoswitch(hunmin_state *s, int want);
bool hunmin_state_get_autoswitch(const hunmin_state *s, int *out);
bool hunmin_state_autoswitch_data_present(const hunmin_state *s);

/* Caps Lock as the layout's `caps` variable. The frontend owns the detection
 * (A3: the platform's lock bit is not the engine's business) and sets it
 * before feeding a key. */
void hunmin_state_set_caps(hunmin_state *s, int on);

/* One key, already translated. Returns true when the engine consumed it; a
 * false key belongs to the client and the word in flight has been delivered.
 * Language keys are events with ev->language_key set (FE13: where the platform
 * owns the trigger, the frontend says so and the engine performs the switch). */
int hunmin_state_key(hunmin_state *s, const hunmin_key_event *ev);

/* The Hanja window's open/close key (F9 in this tree's catalogue). Returns
 * true when it acted: an open list closes, and a preedit with candidates opens
 * one. A preedit with no candidates returns false so the key reaches the
 * client, the same rule fcitx5's D2 lane asserts. */
int hunmin_state_toggle_candidates(hunmin_state *s);

/* Focus change: the composing word is not lost, it is delivered, then the
 * composition is dropped. */
void hunmin_state_flush(hunmin_state *s);
void hunmin_state_reset(hunmin_state *s);

int hunmin_state_candidates_open(const hunmin_state *s);
int hunmin_state_candidate_count(const hunmin_state *s);
/* Commit candidate i (a panel click, not a key): drops the composition the
 * candidate came from, commits the chosen text, closes the list. Returns true
 * when i named a candidate. */
int hunmin_state_candidate_commit(hunmin_state *s, int i);
const char *hunmin_state_candidate_text(const hunmin_state *s, int i);
/* The gloss/reading the hanja tables carry. The IBus panel has no comment
 * slot, so the engine drops this today; it is exposed so the choice is a
 * frontend's and is measured rather than assumed (FE presentation difference). */
const char *hunmin_state_candidate_detail(const hunmin_state *s, int i);
/* Index into the list (not the page) of the highlighted candidate. */
int hunmin_state_candidate_cursor(const hunmin_state *s);

int hunmin_state_language(const hunmin_state *s); /* HUNMIN_LANGUAGE_* */
const char *hunmin_state_layout_id(const hunmin_state *s);
/* The configured Hangul layout id, available before the first key builds a
 * layer. Used by the opt-in trace's meta so a log states which layout it is
 * evidence about; hunmin_state_layout_id() is empty until a layer exists. */
const char *hunmin_state_hangul_layout_id(const hunmin_state *s);
const char *hunmin_state_preedit(const hunmin_state *s);

#ifdef __cplusplus
}
#endif

#endif /* HUNMIN_IBUS_STATE_H_ */