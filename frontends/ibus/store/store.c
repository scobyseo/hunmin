/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "settings_private.h"

#include <gio/gio.h>
#include <glib.h>
#include <string.h>

#define HUNMIN_SCHEMA_ID "org.freedesktop.ibus.engine.hunmin"

/* The daemon's own trigger list. §IBC-4's ruling: the language-key row removes
 * its keys from here, because a key in this list never reaches an engine. */
#define HUNMIN_DAEMON_SCHEMA_ID "org.freedesktop.ibus.general.hotkey"
#define HUNMIN_DAEMON_TRIGGER_KEY "trigger"
/* What we took, so we can put it back. */
#define HUNMIN_MANAGED_KEY "managed-triggers"

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

char *hunmin_ibus_key_entry_from_event(guint keyval, bool shift, bool control, bool alt,
                                       bool super, bool hyper) {
    if (keyval == 0) return NULL;
    char *base = g_strdup(ibus_keyval_name(keyval));
    if (!base) return NULL;
    /* Fixed order, so one chord has one spelling and a duplicate is a duplicate
     * rather than a second entry that matches the same event. */
    GString *out = g_string_new(NULL);
    if (control) g_string_append(out, "Control+");
    if (shift) g_string_append(out, "Shift+");
    if (alt) g_string_append(out, "Alt+");
    if (super) g_string_append(out, "Super+");
    if (hyper) g_string_append(out, "Hyper+");
    g_string_append(out, base);
    g_free(base);
    return g_string_free(out, FALSE);
}

/* ------------------------------------------------- the daemon's trigger list */

/* The keysym of an entry: IBus's grammar is `modifier+…+keysym`, so the last
 * '+' separated term is the key.
 *
 * Matching is by KEY, not by (modifiers, key): the daemon's entry for the
 * physical Han/Eng key on XKB `us` is `Alt+Release+Alt_R` while the row stores
 * `Alt_R` (measured, §IBC-1), so a modifier-exact match would leave that entry
 * in place and the row would name a key the engine never receives. The cost of
 * matching by key is that a daemon binding differing only in modifiers goes too;
 * it is recorded in managed-triggers and restored with the rest. */
static const char *entry_keysym(const char *entry) {
    if (!entry) return "";
    const char *last = strrchr(entry, '+');
    return last ? last + 1 : entry;
}

static bool keysym_owned(char **const keys, gsize n, const char *keysym) {
    for (gsize i = 0; i < n; ++i)
        if (g_strcmp0(entry_keysym(keys[i]), keysym) == 0) return true;
    return false;
}

static void write_if_changed(GSettings *gs, const char *key, GPtrArray *list) {
    char **cur = g_settings_get_strv(gs, key);
    const gsize ncur = cur ? g_strv_length(cur) : 0;
    gboolean differs = ncur != list->len;
    for (gsize i = 0; !differs && i < ncur; ++i)
        differs = g_strcmp0(cur[i], g_ptr_array_index(list, i)) != 0;
    /* Only what changed is written: every engine start runs this, and a write to
     * the daemon's schema is a write to the person's settings. */
    if (differs) g_settings_set_strv(gs, key, (const gchar *const *)list->pdata);
    g_strfreev(cur);
}

/* Give back what we took that is no longer ours, then take what is ours. Both
 * halves are idempotent, so an engine restart cannot accumulate removals. */
void hunmin_ibus_store_reconcile_triggers(hunmin_ibus_settings *s, char **const keys, gsize nkeys) {
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    if (!source) return;
    GSettingsSchema *schema = g_settings_schema_source_lookup(source, HUNMIN_DAEMON_SCHEMA_ID, TRUE);
    if (!schema) return;
    const gboolean present = g_settings_schema_has_key(schema, HUNMIN_DAEMON_TRIGGER_KEY);
    g_settings_schema_unref(schema);
    if (!present) return;

    GSettings *daemon = g_settings_new(HUNMIN_DAEMON_SCHEMA_ID);
    if (!daemon) return;
    GPtrArray *trigger = g_ptr_array_new_null_terminated(0, g_free, TRUE);
    GPtrArray *managed = g_ptr_array_new_null_terminated(0, g_free, TRUE);
    char **v = g_settings_get_strv(daemon, HUNMIN_DAEMON_TRIGGER_KEY);
    for (gsize i = 0; v && v[i]; ++i) g_ptr_array_add(trigger, g_strdup(v[i]));
    g_strfreev(v);
    v = g_settings_get_strv(s->gs, HUNMIN_MANAGED_KEY);
    for (gsize i = 0; v && v[i]; ++i) g_ptr_array_add(managed, g_strdup(v[i]));
    g_strfreev(v);

    /* 1. Restore every recorded entry whose key is no longer ours. */
    for (guint i = 0; i < managed->len;) {
        const char *entry = g_ptr_array_index(managed, i);
        if (keysym_owned(keys, nkeys, entry_keysym(entry))) {
            ++i;
            continue;
        }
        gboolean known = FALSE;
        for (guint j = 0; j < trigger->len && !known; ++j)
            known = g_strcmp0(g_ptr_array_index(trigger, j), entry) == 0;
        if (!known) g_ptr_array_add(trigger, g_strdup(entry));
        g_ptr_array_remove_index(managed, i);
    }
    /* 2. Take every entry whose key is ours, recording it first. */
    for (guint i = 0; i < trigger->len;) {
        const char *entry = g_ptr_array_index(trigger, i);
        if (!keysym_owned(keys, nkeys, entry_keysym(entry))) {
            ++i;
            continue;
        }
        gboolean recorded = FALSE;
        for (guint j = 0; j < managed->len && !recorded; ++j)
            recorded = g_strcmp0(g_ptr_array_index(managed, j), entry) == 0;
        if (!recorded) g_ptr_array_add(managed, g_strdup(entry));
        g_ptr_array_remove_index(trigger, i);
    }

    write_if_changed(daemon, HUNMIN_DAEMON_TRIGGER_KEY, trigger);
    write_if_changed(s->gs, HUNMIN_MANAGED_KEY, managed);
    g_ptr_array_unref(trigger);
    g_ptr_array_unref(managed);
    g_object_unref(daemon);
}

/* ------------------------------------------------------------------ the store */

void hunmin_ibus_store_record_rejection(hunmin_ibus_settings *s, const char *entry) {
    ++s->rejected_count;
    if (!s->rejected) s->rejected = g_strdup(entry ? entry : "");
}

/* Compile the fifth row. Rejections are counted, not fatal: a user's mistyped
 * entry must cost that one binding, not the whole surface (the B3 shape — a
 * missing thing is a normal state, not an error). */
void hunmin_ibus_store_rebuild_keys(hunmin_ibus_settings *s, char **const entries, gsize n) {
    if (s->keys) {
        g_object_unref(s->keys);
        s->keys = NULL;
    }
    IBusHotkeyProfile *profile = NULL;
    for (gsize i = 0; i < n; ++i) {
        bool ok = false;
        const char *reason = hunmin_ibus_stored_key_ok(entries[i], &ok);
        if (!ok) {
            hunmin_ibus_store_record_rejection(s, entries[i]);
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
    hunmin_ibus_store_rebuild_keys(s, keys, keys ? g_strv_length(keys) : 0);
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
char *hunmin_ibus_store_default_string(hunmin_ibus_settings *s, const char *key) {
    if (!s || !s->gs) return NULL;
    GVariant *value = g_settings_get_default_value(s->gs, key);
    char *out = value ? g_variant_dup_string(value, NULL) : NULL;
    if (value) g_variant_unref(value);
    return out;
}

