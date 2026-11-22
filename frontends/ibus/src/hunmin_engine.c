/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "hunmin_engine.h"

#include <hunmin.h>

#include <string.h>

#include "hunmin_state.h"

struct _HunminIbusEngine {
    IBusEngine parent_instance;
    hunmin_state *state;
    hunmin_state_sink sink;
};

struct _HunminIbusEngineClass {
    IBusEngineClass parent_class;
};

G_DEFINE_TYPE(HunminIbusEngine, hunmin_ibus_engine, IBUS_TYPE_ENGINE)

/* A page the host's panel can number. IBus's lookup table labels are the
 * panel's own 1..9 default (src/ibuslookuptable.h); this is a host ceiling,
 * not a preference, and the candidate-window gate is issue #292's job. */
#define HUNMIN_IBUS_PAGE_SIZE 9

/* ------------------------------------------------------------- the sink */

static void sink_commit(void *user, const char *text) {
    if (!text || !*text) return;
    ibus_engine_commit_text(IBUS_ENGINE(user), ibus_text_new_from_string(text));
}

static void sink_preedit(void *user, const char *text, int cursor) {
    IBusEngine *engine = IBUS_ENGINE(user);
    if (!text) text = "";
    IBusText *ibus_text = ibus_text_new_from_string(text);
    if (!*text) {
        /* Empty text clears the slot. PREEDIT_CLEAR, not PREEDIT_COMMIT: the
         * state already delivered the composing word when it cleared (flush,
         * boundary, candidate pick), and a commit here would insert it twice. */
        ibus_engine_update_preedit_text_with_mode(engine, ibus_text, 0, FALSE,
                                                  IBUS_ENGINE_PREEDIT_CLEAR);
        return;
    }
    ibus_engine_update_preedit_text_with_mode(engine, ibus_text, (guint)cursor, TRUE,
                                              IBUS_ENGINE_PREEDIT_CLEAR);
}

static void sink_candidates(void *user, int open) {
    IBusEngine *engine = IBUS_ENGINE(user);
    HunminIbusEngine *self = HUNMIN_IBUS_ENGINE(user);
    if (!open) {
        ibus_engine_hide_lookup_table(engine);
        return;
    }
    const int count = hunmin_state_candidate_count(self->state);
    if (count <= 0) {
        ibus_engine_hide_lookup_table(engine);
        return;
    }
    /* cursor_pos is the index in the whole table, not on the page:
     * ibus_engine_update_lookup_table_fast computes page_begin from it. */
    IBusLookupTable *table = ibus_lookup_table_new((guint)HUNMIN_IBUS_PAGE_SIZE,
                                                   (guint)hunmin_state_candidate_cursor(self->state),
                                                   TRUE, FALSE);
    if (!table) return;
    for (int i = 0; i < count; ++i)
        ibus_lookup_table_append_candidate(
            table, ibus_text_new_from_string(hunmin_state_candidate_text(self->state, i)));
    ibus_engine_update_lookup_table(engine, table, TRUE);
    ibus_engine_show_lookup_table(engine);
}

/* --------------------------------------------------------- key translation */

hunmin_key_event hunmin_ibus_translate_key(guint keyval, guint state, int candidates_open) {
    hunmin_key_event ev;
    memset(&ev, 0, sizeof ev);
    ev.candidates_open = candidates_open;

    switch (keyval) {
    case IBUS_KEY_BackSpace:
        ev.id = HUNMIN_KEY_TEXT;
        ev.base = 0x08; /* libhangul's backspace, the golden-trace convention */
        break;
    case IBUS_KEY_Return:
    case IBUS_KEY_KP_Enter:
        ev.id = HUNMIN_KEY_ENTER;
        break;
    case IBUS_KEY_Escape:
        ev.id = HUNMIN_KEY_ESCAPE;
        break;
    case IBUS_KEY_Up:
    case IBUS_KEY_KP_Up:
        ev.id = HUNMIN_KEY_UP;
        break;
    case IBUS_KEY_Down:
    case IBUS_KEY_KP_Down:
        ev.id = HUNMIN_KEY_DOWN;
        break;
    case IBUS_KEY_Left:
    case IBUS_KEY_KP_Left:
        ev.id = HUNMIN_KEY_LEFT;
        break;
    case IBUS_KEY_Right:
    case IBUS_KEY_KP_Right:
        ev.id = HUNMIN_KEY_RIGHT;
        break;
    case IBUS_KEY_Page_Up:
    case IBUS_KEY_KP_Page_Up:
        ev.id = HUNMIN_KEY_PAGE_UP;
        break;
    case IBUS_KEY_Page_Down:
    case IBUS_KEY_KP_Page_Down:
        ev.id = HUNMIN_KEY_PAGE_DOWN;
        break;
    case IBUS_KEY_Hangul:
        /* FE13: this platform hands the Han/Eng trigger to the engine, so the
         * engine says it is the language key and the state performs the
         * switch. A bare press only -- a chord is the same event with mods. */
        ev.language_key = 1;
        break;
    default:
        ev.id = HUNMIN_KEY_TEXT;
        /* Caps first, then case: X11 reports a Caps Locked letter as its
         * uppercase keyval with the lock bit set, exactly like fcitx5's
         * rawKey() path (docs/DESIGN-FRONTEND.md Part B A3). Lowercasing it
         * keeps the layout's shifted row off and lets the layout's `caps`
         * variable carry the inversion instead. */
        if ((state & IBUS_LOCK_MASK) && keyval >= 'A' && keyval <= 'Z')
            keyval = keyval + ('a' - 'A');
        if (keyval >= 'A' && keyval <= 'Z') {
            ev.shift = 1;
            ev.base = keyval + ('a' - 'A');
        } else if (keyval >= 0x20 && keyval <= 0x7E) {
            ev.base = keyval;
        } else {
            ev.base = 0; /* the platform mapped nothing: a boundary, not input */
        }
        break;
    }

    if (state & IBUS_CONTROL_MASK) ev.mods |= HUNMIN_MOD_CTRL;
    if (state & IBUS_MOD1_MASK) ev.mods |= HUNMIN_MOD_ALT;
    if (state & IBUS_SUPER_MASK) ev.mods |= HUNMIN_MOD_GUI;
    /* Shift is deliberately NOT in mods: it selects a layout row and travels
     * as the event's shift bit instead (hunmin.h, HUNMIN_MOD_SHIFT). */
    return ev;
}

/* ------------------------------------------------------------ the vfuncs */

static gboolean hunmin_process_key_event(IBusEngine *engine, guint keyval, guint keycode,
                                         guint state) {
    (void)keycode;
    HunminIbusEngine *self = HUNMIN_IBUS_ENGINE(engine);
    if (self->state == NULL) return FALSE;
    /* IBus delivers releases too; the key path is press-driven. */
    if (state & IBUS_RELEASE_MASK) return FALSE;
    /* A bare modifier is not composition input and must not end the word in
     * flight (fcitx5's adapter makes the same exception for Shift). */
    if (keyval == IBUS_KEY_Shift_L || keyval == IBUS_KEY_Shift_R) return FALSE;

    /* The Hanja window's open/close key. It is outside the printable window
     * the state feeds, so it is intercepted here -- presentation is this
     * object's half (the same split as fcitx5's F9 handling). */
    if (keyval == IBUS_KEY_F9) return hunmin_state_toggle_candidates(self->state) ? TRUE : FALSE;

    hunmin_state_set_caps(self->state, (state & IBUS_LOCK_MASK) ? 1 : 0);
    const hunmin_key_event ev = hunmin_ibus_translate_key(
        keyval, state, hunmin_state_candidates_open(self->state));
    return hunmin_state_key(self->state, &ev) ? TRUE : FALSE;
}

static void hunmin_focus_in(IBusEngine *engine) { (void)engine; }

static void hunmin_focus_out(IBusEngine *engine) {
    HunminIbusEngine *self = HUNMIN_IBUS_ENGINE(engine);
    if (self->state == NULL) return;
    /* Composing text is not lost on a focus change: deliver it, then drop the
     * composition (fcitx5's reset() contract). */
    hunmin_state_flush(self->state);
    hunmin_state_reset(self->state);
}

static void hunmin_reset(IBusEngine *engine) { hunmin_focus_out(engine); }

static void hunmin_page_up(IBusEngine *engine) {
    HunminIbusEngine *self = HUNMIN_IBUS_ENGINE(engine);
    if (self->state == NULL) return;
    hunmin_key_event ev = {0};
    ev.id = HUNMIN_KEY_PAGE_UP;
    ev.candidates_open = 1;
    hunmin_state_key(self->state, &ev);
}

static void hunmin_page_down(IBusEngine *engine) {
    HunminIbusEngine *self = HUNMIN_IBUS_ENGINE(engine);
    if (self->state == NULL) return;
    hunmin_key_event ev = {0};
    ev.id = HUNMIN_KEY_PAGE_DOWN;
    ev.candidates_open = 1;
    hunmin_state_key(self->state, &ev);
}

static void hunmin_cursor_up(IBusEngine *engine) {
    HunminIbusEngine *self = HUNMIN_IBUS_ENGINE(engine);
    if (self->state == NULL) return;
    hunmin_key_event ev = {0};
    ev.id = HUNMIN_KEY_UP;
    ev.candidates_open = 1;
    hunmin_state_key(self->state, &ev);
}

static void hunmin_cursor_down(IBusEngine *engine) {
    HunminIbusEngine *self = HUNMIN_IBUS_ENGINE(engine);
    if (self->state == NULL) return;
    hunmin_key_event ev = {0};
    ev.id = HUNMIN_KEY_DOWN;
    ev.candidates_open = 1;
    hunmin_state_key(self->state, &ev);
}

static void hunmin_candidate_clicked(IBusEngine *engine, guint index, guint button, guint state) {
    (void)button;
    (void)state;
    HunminIbusEngine *self = HUNMIN_IBUS_ENGINE(engine);
    if (self->state == NULL) return;
    hunmin_state_candidate_commit(self->state, (int)index);
}

/* ------------------------------------------------------------- lifecycle */

static void hunmin_ibus_engine_dispose(GObject *object) {
    HunminIbusEngine *self = HUNMIN_IBUS_ENGINE(object);
    if (self->state) {
        hunmin_state_free(self->state);
        self->state = NULL;
    }
    G_OBJECT_CLASS(hunmin_ibus_engine_parent_class)->dispose(object);
}

static void hunmin_ibus_engine_class_init(HunminIbusEngineClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    IBusEngineClass *engine_class = IBUS_ENGINE_CLASS(klass);
    object_class->dispose = hunmin_ibus_engine_dispose;
    engine_class->process_key_event = hunmin_process_key_event;
    engine_class->focus_in = hunmin_focus_in;
    engine_class->focus_out = hunmin_focus_out;
    engine_class->reset = hunmin_reset;
    engine_class->page_up = hunmin_page_up;
    engine_class->page_down = hunmin_page_down;
    engine_class->cursor_up = hunmin_cursor_up;
    engine_class->cursor_down = hunmin_cursor_down;
    engine_class->candidate_clicked = hunmin_candidate_clicked;
}

static void hunmin_ibus_engine_init(HunminIbusEngine *self) {
    /* NULL/0 dirs: the engine resolves HUNMIN_DATA_PATH and then the installed
     * data directory, the same rule every other hunmin consumer follows. No
     * path is compiled into this frontend. */
    self->state = hunmin_state_new(NULL, 0);
    self->sink.user = self;
    self->sink.commit = sink_commit;
    self->sink.preedit = sink_preedit;
    self->sink.candidates = sink_candidates;
    if (self->state) hunmin_state_set_sink(self->state, &self->sink);
}

struct hunmin_state *hunmin_ibus_engine_state(HunminIbusEngine *engine) {
    return engine ? engine->state : NULL;
}