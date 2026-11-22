/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The settings store's private seam: the schema handle, the compiled key list,
 * and the two files' shared state. Not installed, not exported, and not part of
 * the frontend's counted adapter files — the store is plumbing two processes
 * share (the engine reads it, the window writes it), while `src/settings.c` is
 * the part that turns a stored row into an engine call.
 */
#ifndef HUNMIN_IBUS_STORE_PRIVATE_H_
#define HUNMIN_IBUS_STORE_PRIVATE_H_

#include <gio/gio.h>
#include <ibus.h>

#include "settings.h"

struct hunmin_ibus_settings {
    GSettings *gs;
    IBusHotkeyProfile *keys; /* compiled switch-keys; NULL when the list is empty */
    char *rejected;          /* first dropped entry, owned */
    int rejected_count;
    guint changed_id;
    void (*changed)(void *user); /* the live-apply callback, if any */
    void *changed_user;
};

/* Recompile the fifth row from `entries`, recording what the grammar refused. */
void hunmin_ibus_store_rebuild_keys(hunmin_ibus_settings *s, char **const entries, gsize n);

/* Take our keys out of the daemon's global trigger list, and give back what we
 * took earlier and no longer own (§IBC-4). Idempotent. */
void hunmin_ibus_store_reconcile_triggers(hunmin_ibus_settings *s, char **const keys,
                                          gsize nkeys);

/* The schema's own default for a key, as a string. Used where a stored value was
 * rejected, so the fallback is the documented default rather than a literal in C. */
char *hunmin_ibus_store_default_string(hunmin_ibus_settings *s, const char *key);

/* Record one rejected value for the window to show. */
void hunmin_ibus_store_record_rejection(hunmin_ibus_settings *s, const char *entry);

#endif /* HUNMIN_IBUS_STORE_PRIVATE_H_ */
