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

/* The one home of the schema's id. The key names are read through GSettings
 * keys, so a rename is a compile-time string change in this file plus the XML;
 * tests/test_settings.cpp is what notices if the two drift apart. */
#define HUNMIN_SCHEMA_ID "org.freedesktop.ibus.engine.hunmin"

/* The schema's own defaults, needed only where a stored value was REJECTED and
 * the engine has to fall back to something. Read from the schema at runtime
 * (g_settings_get_default_value) rather than written here as a literal: a
 * second copy of "sin3-p2" is the drift this file is built to avoid. */
struct hunmin_ibus_settings {
    GSettings *gs;
    IBusHotkeyProfile *keys; /* compiled switch-keys; NULL when the list is empty */
    char *rejected;          /* first dropped entry, owned */
    int rejected_count;
    guint changed_id;
    void (*changed)(void *user); /* the live-apply callback, if any */
    void *changed_user;
};

/* --------------------------------------------------------------- the grammar */

/* A term is exactly one '+' separated piece: comparing with strstr would also
 * reject a keysym that merely contains the word. */
static bool has_term(const char *entry, const char *want) {
    if (!entry || !*entry) return false;
    const char *p = entry;
    while (*p) {
        const char *end = strchr(p, '+');
        const size_t n = end ? (size_t)(end - p) : strlen(p);
        if (strlen(want) == n && strncmp(p, want, n) == 0) return true;
        if (!end) break;
        p = end + 1;
    }
    return false;
}

const char *hunmin_ibus_key_entry_reason(const char *entry) {
    if (!entry || !*entry) return "empty";
    /* IBus's own default trigger for the physical Han/Eng key is
     * "Alt+Release+Alt_R". The pattern parses, so nothing warns — but this
     * engine returns before classifying a release (hunmin_engine.c's
     * `state & IBUS_RELEASE_MASK` guard), so a stored Release entry is a
     * binding that can never fire. Measured 2026-11-22: parse ok, and it fires
     * only for an event this engine never classifies. */
    if (has_term(entry, "Release")) return "Release: this engine acts on key presses only";
    /* Measured 2026-11-22 against libibus 1.5.34 and 1.5.26 (identical
     * ibushotkey.h): "Meta+Hangul" makes ibushotkey.c reach an unimplemented
     * switch branch, which logs `IBUS-CRITICAL ... should not be reached` and
     * returns FALSE. Refused here so a stored value cannot put that in the
     * journal. */
    if (has_term(entry, "Meta")) return "Meta: IBus cannot express it as a modifier";
    return NULL;
}

const char *hunmin_ibus_stored_key_ok(const char *entry, bool *ok) {
    if (ok) *ok = false;
    const char *reason = hunmin_ibus_key_entry_reason(entry);
    if (reason) return reason;
    IBusHotkeyProfile *probe = ibus_hotkey_profile_new();
    if (!probe) return "no memory for a hotkey profile";
    const gboolean parsed =
        ibus_hotkey_profile_add_hotkey_from_string(probe, entry, g_quark_from_string(entry));
    g_object_unref(probe);
    if (!parsed) return "not an IBus hotkey: keysym or mod+keysym, names case-sensitive";
    if (ok) *ok = true;
    return NULL;
}

/* ------------------------------------------------------------------ the store */

static void record_rejection(hunmin_ibus_settings *s, const char *entry) {
    ++s->rejected_count;
    if (!s->rejected) s->rejected = g_strdup(entry ? entry : "");
}

/* Compile the fifth row. Rejections are counted, not fatal: a user's mistyped
 * entry must cost that one binding, not the whole surface (the B3 shape — a
 * missing thing is a normal state, not an error). */
static void rebuild_keys(hunmin_ibus_settings *s, char **const entries, gsize n) {
    if (s->keys) {
        g_object_unref(s->keys);
        s->keys = NULL;
    }
    IBusHotkeyProfile *profile = NULL;
    for (gsize i = 0; i < n; ++i) {
        bool ok = false;
        const char *reason = hunmin_ibus_stored_key_ok(entries[i], &ok);
        if (!ok) {
            record_rejection(s, entries[i]);
            g_debug("hunmin: switch-keys entry `%s` dropped: %s", entries[i], reason);
            continue;
        }
        if (!profile) profile = ibus_hotkey_profile_new();
        if (!profile) break;
        ibus_hotkey_profile_add_hotkey_from_string(profile, entries[i],
                                                   g_quark_from_string(entries[i]));
    }
    s->keys = profile;
}

hunmin_ibus_settings *hunmin_ibus_settings_new(void) {
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    if (!source) return NULL;
    GSettingsSchema *schema = g_settings_schema_source_lookup(source, HUNMIN_SCHEMA_ID, TRUE);
    if (!schema) return NULL;
    const gboolean installed = g_settings_schema_has_key(schema, "hangul-layout");
    g_settings_schema_unref(schema);
    if (!installed) return NULL;

    hunmin_ibus_settings *s = g_new0(hunmin_ibus_settings, 1);
    s->gs = g_settings_new(HUNMIN_SCHEMA_ID);
    if (!s->gs) {
        g_free(s);
        return NULL;
    }
    /* The key list is compiled here as well as in apply(): a caller that asks
     * for the profile before applying (the D2 translation cases do) gets the
     * stored rows, not an empty profile. */
    char **keys = g_settings_get_strv(s->gs, "switch-keys");
    rebuild_keys(s, keys, keys ? g_strv_length(keys) : 0);
    g_strfreev(keys);
    return s;
}

void hunmin_ibus_settings_free(hunmin_ibus_settings *s) {
    if (!s) return;
    if (s->changed_id && s->gs) g_signal_handler_disconnect(s->gs, s->changed_id);
    if (s->keys) g_object_unref(s->keys);
    g_clear_object(&s->gs);
    g_free(s->rejected);
    g_free(s);
}

/* The schema's default for a key, as a string. Used only where a stored value
 * was rejected, so a bad value cannot leave the engine on a layout it could not
 * load. Read from the schema rather than written here as a literal: a second
 * copy of "sin3-p2" is the drift this file exists to avoid. */
static char *schema_default_string(hunmin_ibus_settings *s, const char *key) {
    if (!s || !s->gs) return NULL;
    GVariant *value = g_settings_get_default_value(s->gs, key);
    char *out = value ? g_variant_dup_string(value, NULL) : NULL;
    if (value) g_variant_unref(value);
    return out;
}

static const char *commit_unit_to_policy(const char *unit) {
    /* The schema constrains the value to these two; an unknown one reaches here
     * from a hand-edited dconf db, which is why the fallback exists. */
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
        const char *bad = g_strdup_printf("%s/%s", hangul ? hangul : "", latin ? latin : "");
        record_rejection(s, bad);
        g_free((char *)bad);
        char *fallback_hangul = schema_default_string(s, "hangul-layout");
        char *fallback_latin = schema_default_string(s, "latin-layout");
        if (fallback_hangul && *fallback_hangul)
            hunmin_state_set_layouts(state, fallback_hangul, fallback_latin);
        g_free(fallback_hangul);
        g_free(fallback_latin);
    }
    g_free(hangul);
    g_free(latin);

    /* Commit unit. */
    char *unit = g_settings_get_string(s->gs, "commit-unit");
    const char *policy = commit_unit_to_policy(unit);
    if (!policy) {
        record_rejection(s, unit);
        char *fallback = schema_default_string(s, "commit-unit");
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

    /* The language keys. */
    char **keys = g_settings_get_strv(s->gs, "switch-keys");
    rebuild_keys(s, keys, keys ? g_strv_length(keys) : 0);
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