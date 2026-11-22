/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * hunmin-ibus-settings — the IBus frontend's settings window (issue #327,
 * docs/SPEC-FRONTEND.md §FE17, priced in docs/OPTIONS-MEASURED.md §IBC).
 *
 * A separate process, and that is the design rather than an accident:
 *
 *   - the IBus panel launches it from Preferences, because the component XML
 *     names it in a <setup> element — the same hook ibus-hangul uses;
 *   - it is not the engine, so nothing here is measured against FE11's
 *     thin-adapter budget: this is presentation, which is what a budget that
 *     measures adapters does not measure (the same split TSF's settings exe
 *     makes);
 *   - every row it writes goes through GSettings, so the engine picks the change
 *     up through its own watcher and no restart is involved.
 *
 * What it deliberately does NOT do: decide what a key binding means. Capture
 * produces flags, hunmin_ibus_key_entry_from_event spells them, and
 * hunmin_ibus_stored_key_ok asks IBus — one validator, two callers, so the window
 * cannot hold a second opinion about "Alt_R" or about the two grammar corners
 * (Release, Meta) that this engine refuses.
 */

#ifndef HUNMIN_IBUS_SETTINGS_UI_H_
#define HUNMIN_IBUS_SETTINGS_UI_H_

/*
 * The window's widgets, separated from main() so a tier-D2 case can build them
 * and assert what they OFFER — which layouts, which commit units, which keys,
 * and what the store says after a change. GTK needs a display, so the case runs
 * under xvfb-run and skips (77) where none can be opened.
 *
 * Nothing here decides anything: the rows come from the store, the layout list
 * from the core, and a captured key is spelled by hunmin_ibus_key_entry_from_event.
 */

#include <gtk/gtk.h>
#include <hunmin.h>

#include "settings.h"

typedef struct hunmin_ibus_ui hunmin_ibus_ui;

/* `ctx` may be NULL (no data), in which case the layout combos are empty. */
hunmin_ibus_ui *hunmin_ibus_ui_new(GSettings *gs, hunmin_ctx *ctx);
void hunmin_ibus_ui_free(hunmin_ibus_ui *ui);
GtkWidget *hunmin_ibus_ui_window(hunmin_ibus_ui *ui);

/* "hangul" or "latin". */
GtkWidget *hunmin_ibus_ui_layout_combo(hunmin_ibus_ui *ui, const char *mode);
GtkWidget *hunmin_ibus_ui_commit_combo(hunmin_ibus_ui *ui);
GtkWidget *hunmin_ibus_ui_autoswitch(hunmin_ibus_ui *ui);
GtkWidget *hunmin_ibus_ui_keys_list(hunmin_ibus_ui *ui);

/* The capture path without a dialog: returns the number of entries added (0 for a
 * duplicate, a refused key, or a key with no keyval). `gdk_state` is the GTK
 * modifier state, which is what a real key event carries. */
int hunmin_ibus_ui_offer_key(hunmin_ibus_ui *ui, guint keyval, guint gdk_state);

/* Drop the entry at `index` from the store, as its row's button does. */
void hunmin_ibus_ui_remove_key_at(hunmin_ibus_ui *ui, guint index);

/* Re-read the store into the widgets. */
void hunmin_ibus_ui_refresh(hunmin_ibus_ui *ui);

#endif /* HUNMIN_IBUS_SETTINGS_UI_H_ */
