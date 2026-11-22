/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "hunmin_engine.h"

#include <hunmin.h>

#include <string.h>

#include "hunmin_state.h"
#include "trace/trace.h"

struct _HunminIbusEngine {
    IBusEngine parent_instance;
    hunmin_state *state;
    hunmin_state_sink sink;
    /* The host panel's language indicator. The engine has no own window; the
     * IBus property is where "which mode am I in" belongs, and the API exists
     * at the 1.5.26 floor (unlike ibus_engine_send_message). */
    IBusProperty *mode_prop;
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
    (void)user;
    HUNMIN_TRACE_COMMIT(text);
    if (!text || !*text) return;
    ibus_engine_commit_text(IBUS_ENGINE(user), ibus_text_new_from_string(text));
}

static void sink_preedit(void *user, const char *text, int cursor) {
    IBusEngine *engine = IBUS_ENGINE(user);
    HUNMIN_TRACE_PREEDIT(text);
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
    const int count = open ? hunmin_state_candidate_count(self->state) : 0;
    if (count <= 0) {
        ibus_engine_hide_lookup_table(engine);
        /* FE16: the gloss goes away with the list. */
        ibus_engine_update_auxiliary_text(engine, ibus_text_new_from_string(""), FALSE);
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
    /* FE16, option A (issue #319): the highlighted candidate's gloss rides
     * the panel's auxiliary label -- verified drawn at the 1.5.26 floor
     * (candidatepanel.c m_aux_label). This sink fires on every cursor
     * change, so the gloss tracks the cursor; the candidate TEXT stays bare
     * Hanja. An empty detail hides the label. */
    const char *detail =
        hunmin_state_candidate_detail(self->state, hunmin_state_candidate_cursor(self->state));
    ibus_engine_update_auxiliary_text(engine, ibus_text_new_from_string(detail),
                                      *detail ? TRUE : FALSE);
}

/* --------------------------------------------------------- key translation */

hunmin_key_event hunmin_ibus_translate_key(guint keyval, guint state, int candidates_open) {
    hunmin_key_event ev;
    memset(&ev, 0, sizeof ev);
    ev.candidates_open = candidates_open;
    /* FE13: both spellings of the Han/Eng key are ours. XKB "us" reports the
     * physical key (keycode 108) as Alt_R 0xffea; a Korean layout reports
     * Hangul 0xff31. Accepting both makes the engine layout-independent. A
     * bare press only -- a chord is the same event with modifier bits. */
    if (keyval == IBUS_KEY_Alt_R || keyval == IBUS_KEY_Hangul) {
        ev.language_key = 1;
        return ev;
    }

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

/* Record a key the engine did not take; the shared ev carries the raw keyval
 * as a TEXT event, the same convention fcitx5's personal log uses for F9. */
static int declined(guint keyval) {
    const hunmin_key_event ev = { .base = keyval, .id = HUNMIN_KEY_TEXT };
    HUNMIN_TRACE_EVENT(&ev, 0, keyval);
    return FALSE;
}

static gboolean hunmin_process_key_event(IBusEngine *engine, guint keyval, guint keycode,
                                         guint state) {
    (void)keycode;
    HunminIbusEngine *self = HUNMIN_IBUS_ENGINE(engine);
    HUNMIN_TRACE_RESET();
    if (self->state == NULL) return declined(keyval);
    /* IBus delivers releases too; the key path is press-driven. */
    if (state & IBUS_RELEASE_MASK) return declined(keyval);
    /* A bare modifier is not composition input and must not end the word in
     * flight (fcitx5's adapter makes the same exception for Shift). */
    if (keyval == IBUS_KEY_Shift_L || keyval == IBUS_KEY_Shift_R)
        return declined(keyval);

    /* The Hanja window's open/close key is outside the printable window the
     * state feeds; presentation is this object's half (fcitx5's F9 split). */
    if (keyval == IBUS_KEY_F9) {
        const int used = hunmin_state_toggle_candidates(self->state) ? TRUE : FALSE;
        const hunmin_key_event ev = { .base = keyval, .id = HUNMIN_KEY_TEXT };
        HUNMIN_TRACE_EVENT(&ev, used, keyval);
        return used;
    }

    hunmin_state_set_caps(self->state, (state & IBUS_LOCK_MASK) ? 1 : 0);
    const hunmin_key_event ev = hunmin_ibus_translate_key(
        keyval, state, hunmin_state_candidates_open(self->state));
    const int used = hunmin_state_key(self->state, &ev) ? TRUE : FALSE;
    HUNMIN_TRACE_EVENT(&ev, used, keyval);
    ibus_property_set_label(self->mode_prop, ibus_text_new_from_string(
        hunmin_state_language(self->state) == HUNMIN_LANGUAGE_LATIN ? "영" : "한"));
    ibus_engine_update_property(IBUS_ENGINE(self), self->mode_prop);
    return used;
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
    g_clear_object(&self->mode_prop);
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

    /* Host-visible mode indicator (IBus panel language bar). */
    self->mode_prop = ibus_property_new("hunmin-mode", PROP_TYPE_NORMAL,
        ibus_text_new_from_string("한"), NULL, ibus_text_new_from_string("mode"),
        TRUE, TRUE, PROP_STATE_UNCHECKED, NULL);
    g_object_ref_sink(self->mode_prop);  /* we keep our own ref across updates */
    IBusPropList *props = ibus_prop_list_new();
    ibus_prop_list_append(props, self->mode_prop);
    ibus_engine_register_properties(IBUS_ENGINE(self), props);
    g_object_unref(props);

    /* The layout config store is still an open ruling (docs/MILESTONES.md),
     * so the engine runs the state's defaults. HUNMIN_IBUS_LAYOUT is the D3
     * test seam: the daemon inherits it and so does this engine process,
     * letting the full-stack tier drive any of the case-file layouts without
     * the test choosing a frontend config backend on the maintainer's behalf.
     * An unset or unknown id leaves the defaults in place. */
    const char *layout = g_getenv("HUNMIN_IBUS_LAYOUT");
    if (layout && *layout && self->state)
        hunmin_state_set_layouts(self->state, layout, NULL);
    /* The opt-in log's meta states the layout the state actually resolved --
     * not the requested env id, which may have failed to load. Available here
     * before the first key builds a layer. */
    HUNMIN_TRACE_SET_LAYOUT(hunmin_state_hangul_layout_id(self->state));

    /* Evaluation-only switch (no config-store ruling yet): the real-use
     * script sets HUNMIN_IBUS_AUTOSWITCH so the D1-proven judgement path can
     * be felt by a person. Absent, the shipped default OFF is untouched. */
    if (g_getenv("HUNMIN_IBUS_AUTOSWITCH") && self->state)
        hunmin_state_set_autoswitch(self->state, 1);
}

struct hunmin_state *hunmin_ibus_engine_state(HunminIbusEngine *engine) {
    return engine ? engine->state : NULL;
}