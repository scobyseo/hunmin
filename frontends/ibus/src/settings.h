/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef HUNMIN_IBUS_SETTINGS_H_
#define HUNMIN_IBUS_SETTINGS_H_

/*
 * The five stored rows (docs/SPEC-FRONTEND.md §FE17, priced in
 * docs/OPTIONS-MEASURED.md §IBC): the schema is their only home, and this file
 * is the only reader of it.
 *
 * Two rules this file exists to keep:
 *
 *   the DEFAULT lives in the .gschema.xml, never here — a literal in this file
 *   is the second home that lets the shipped default and the documented default
 *   disagree;
 *
 *   a STORED value is validated before it reaches the engine or IBus's parser —
 *   the fifth row is user-typed text in IBus's hotkey grammar, and two of that
 *   grammar's corners (Release, Meta) would otherwise produce a binding that
 *   never fires or an IBUS-CRITICAL in the journal (§IBC-4's measurements).
 *
 * Nothing here is host-UI code: the engine calls apply() and matches keys, and a
 * settings window writes the same schema through GSettings.
 */

#include <stdbool.h>

#include <ibus.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hunmin_state;

typedef struct hunmin_ibus_settings hunmin_ibus_settings;

/* Opens org.freedesktop.ibus.engine.hunmin. NULL when the schema is not
 * installed (a build tree without GSETTINGS_SCHEMA_DIR, or a broken package):
 * the engine then runs the layer's own defaults, which is what it did before
 * this file existed. */
hunmin_ibus_settings *hunmin_ibus_settings_new(void);
void hunmin_ibus_settings_free(hunmin_ibus_settings *s);

/* Read all five rows and apply them to the state. Returns the number of rows
 * that were stored but rejected (an unloadable layout id, an unknown commit
 * unit, a key list entry the grammar refuses); 0 means everything applied as
 * stored. A rejected layout degrades to the schema default, never to a silent
 * mispair. */
int hunmin_ibus_settings_apply(hunmin_ibus_settings *s, struct hunmin_state *state);

/* The compiled fifth row, for hunmin_ibus_translate_key(). NULL when the
 * settings object is NULL (no store) or the stored list parsed to nothing. */
IBusHotkeyProfile *hunmin_ibus_settings_keys(const hunmin_ibus_settings *s);

/* True when (keyval, state) is one of the stored language keys. `state` is
 * IBus's own event state: Shift is part of a pattern, so a stored
 * `Shift+Hangul` needs the mask here, while the shared key event keeps shift in
 * its own field. CapsLock is ignored by IBus's matcher, which is what we want —
 * a binding must not break under Caps Lock. */
bool hunmin_ibus_settings_is_language_key(const hunmin_ibus_settings *s, guint keyval,
                                          guint state);

/* The first entry dropped by the last apply(), and how many: the settings
 * window shows these rather than a value that looks stored but is not. Never
 * NULL; empty when nothing was rejected. */
const char *hunmin_ibus_settings_rejected(const hunmin_ibus_settings *s);
int hunmin_ibus_settings_rejected_count(const hunmin_ibus_settings *s);

/* A change reaches the running engine: the callback runs after GSettings has
 * the new value, so it may call apply() and republish. */
void hunmin_ibus_settings_watch(hunmin_ibus_settings *s,
                                void (*changed)(void *user), void *user);

/* ---------------------------------------------------------------- the grammar
 *
 * Exposed because the settings window validates a CAPTURED key with the same
 * rule the engine applies to a stored one — one validator, two callers, rather
 * than the window's idea of "valid" drifting from the engine's.
 */

/* NULL when `entry` is a usable pattern, otherwise the reason it is not, in
 * words the settings window can show. Checks the two corners IBus's parser does
 * not report usefully (Release parses but can never fire here; Meta makes
 * ibushotkey.c reach an unimplemented branch and log a critical) plus the empty
 * case. Grammar errors proper (a misspelt keysym, `<Super>space`) are reported
 * by hunmin_ibus_stored_key_ok(). */
const char *hunmin_ibus_key_entry_reason(const char *entry);

/* Parse one entry the way the engine will: reason == NULL and *ok true when it
 * becomes a live binding. */
const char *hunmin_ibus_stored_key_ok(const char *entry, bool *ok);

#ifdef __cplusplus
}
#endif

#endif /* HUNMIN_IBUS_SETTINGS_H_ */