/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "settings.h"

#include <gio/gio.h>
#include <glib.h>
#include <string.h>

#include "hunmin_state.h"
#include "settings_private.h"

/* The schema constrains the value to these two; an unknown one reaches here from
 * a hand-edited dconf db, which is why the fallback exists. */
static const char *commit_unit_to_policy(const char *unit) {
    if (g_strcmp0(unit, "syllable") == 0) return "syllable";
    if (g_strcmp0(unit, "word") == 0) return "word";
    return NULL;
}

int hunmin_ibus_settings_apply(hunmin_ibus_settings *s, struct hunmin_state *state) {
    if (!s || !s->gs || !state) return 0;
    g_free(s->rejected);
    s->rejected = NULL;
    s->rejected_count = 0;

    /* Layouts. A stored id that does not load is a rejection AND a fallback:
     * the engine keeps working on the schema's default rather than on a layer
     * that does not exist. TSF resolves the same way (resolveLayoutSlot). */
    char *hangul = g_settings_get_string(s->gs, "hangul-layout");
    char *latin = g_settings_get_string(s->gs, "latin-layout");
    if (!hangul || !*hangul || !hunmin_state_set_layouts(state, hangul, latin)) {
        char *bad = g_strdup_printf("%s/%s", hangul ? hangul : "", latin ? latin : "");
        hunmin_ibus_store_record_rejection(s, bad);
        g_free(bad);
        char *h = hunmin_ibus_store_default_string(s, "hangul-layout");
        char *l = hunmin_ibus_store_default_string(s, "latin-layout");
        if (h && *h) hunmin_state_set_layouts(state, h, l);
        g_free(h);
        g_free(l);
    }
    g_free(hangul);
    g_free(latin);

    /* Commit unit. */
    char *unit = g_settings_get_string(s->gs, "commit-unit");
    const char *policy = commit_unit_to_policy(unit);
    if (!policy) {
        hunmin_ibus_store_record_rejection(s, unit);
        char *fallback = hunmin_ibus_store_default_string(s, "commit-unit");
        policy = commit_unit_to_policy(fallback);
        g_free(fallback);
    }
    hunmin_state_set_commit_policy(state, g_strcmp0(policy, "syllable") == 0
                                              ? HUNMIN_COMMIT_PER_SYLLABLE
                                              : HUNMIN_COMMIT_AT_BOUNDARY);
    g_free(unit);

    /* The automatic switch: the engine owns the value and may refuse ON (B3),
     * so the store's value is a request, not a promise. */
    hunmin_state_set_autoswitch(state, g_settings_get_boolean(s->gs, "autoswitch") ? 1 : 0);

    /* The language keys, and the daemon list they have to leave to be
     * reachable at all (§IBC-4). */
    char **keys = g_settings_get_strv(s->gs, "switch-keys");
    const gsize nkeys = keys ? g_strv_length(keys) : 0;
    hunmin_ibus_store_rebuild_keys(s, keys, nkeys);
    hunmin_ibus_store_reconcile_triggers(s, keys, nkeys);
    g_strfreev(keys);

    return s->rejected_count;
}

IBusHotkeyProfile *hunmin_ibus_settings_keys(const hunmin_ibus_settings *s) {
    return s ? s->keys : NULL;
}

bool hunmin_ibus_settings_is_language_key(const hunmin_ibus_settings *s, guint keyval,
                                          guint state) {
    if (!s || !s->keys || keyval == 0) return false;
    /* IBus's matcher ignores LOCK and MOD5 (measured 2026-11-22): Caps Lock
     * therefore cannot break a binding, and AltGr cannot distinguish one. */
    return ibus_hotkey_profile_lookup_hotkey(s->keys, keyval, state) != 0;
}

const char *hunmin_ibus_settings_rejected(const hunmin_ibus_settings *s) {
    return (s && s->rejected) ? s->rejected : "";
}

int hunmin_ibus_settings_rejected_count(const hunmin_ibus_settings *s) {
    return s ? s->rejected_count : 0;
}

static void on_changed(GSettings *gs, const gchar *key, gpointer data) {
    (void)gs;
    (void)key; /* every row is re-read by apply(); no per-key dispatch to keep in step */
    hunmin_ibus_settings *s = data;
    if (s->changed) s->changed(s->changed_user);
}

void hunmin_ibus_settings_watch(hunmin_ibus_settings *s, void (*changed)(void *user),
                                void *user) {
    if (!s || !s->gs || !changed) return;
    s->changed = changed;
    s->changed_user = user;
    if (s->changed_id) return; /* one connection, however many times it is watched */
    s->changed_id = g_signal_connect(s->gs, "changed", G_CALLBACK(on_changed), s);
}