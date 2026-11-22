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
#include <gtk/gtk.h>
#include <ibus.h>

#include <hunmin.h>

#include "hunmin_state.h"
#include "settings.h"
#include "ui.h"

#define SCHEMA_ID "org.freedesktop.ibus.engine.hunmin"

/* `--print [row]` reads the five rows the way the ENGINE reads them and prints
 * them: no display, no GTK, no window. It exists because "the value I set is the
 * value the engine gets" is otherwise only observable by typing into a session,
 * and it is the reader side of the cross-process persistence case in
 * test_settings_persist.cpp. */
static int print_rows(const char *row) {
    hunmin_ibus_settings *s = hunmin_ibus_settings_new();
    if (!s) {
        g_printerr("hunmin-ibus-settings: schema %s is not installed\n", SCHEMA_ID);
        return 1;
    }
    hunmin_ctx *ctx = hunmin_ctx_new(NULL, 0);
    hunmin_state *state = hunmin_state_new(NULL, 0);
    if (state) hunmin_ibus_settings_apply(s, state);

    GSettings *gs = g_settings_new(SCHEMA_ID);
    /* Each row prints what the STORE holds — that is what a settings tool is
     * asked about — and the two rows the engine may answer differently print
     * their engine answer beside it. A refused value is then visible as a
     * disagreement rather than as a value that works. */
    const char *rows_str[] = {"hangul-layout", "latin-layout", "commit-unit", NULL};
    for (int i = 0; rows_str[i]; ++i) {
        if (row && g_strcmp0(row, rows_str[i]) != 0) continue;
        char *v = g_settings_get_string(gs, rows_str[i]);
        g_print("%s=%s\n", rows_str[i], v);
        g_free(v);
    }
    if (!row || g_strcmp0(row, "autoswitch") == 0)
        g_print("autoswitch=%s\n", g_settings_get_boolean(gs, "autoswitch") ? "true" : "false");
    if (!row || g_strcmp0(row, "switch-keys") == 0) {
        char **v = g_settings_get_strv(gs, "switch-keys");
        GString *out = g_string_new("switch-keys=");
        for (gsize i = 0; v && v[i]; ++i)
            g_string_append_printf(out, "%s%s", i ? "," : "", v[i]);
        g_print("%s\n", out->str);
        g_string_free(out, TRUE);
        g_strfreev(v);
    }
    /* The engine's answers for the two rows it can refuse: the layout it actually
     * loaded, and the switch state it actually holds (B3 may refuse ON). The
     * commit unit and the key list have no engine-side getter. */
    if (!row || g_strcmp0(row, "hangul-layout") == 0) {
        const char *resolved = state ? hunmin_state_hangul_layout_id(state) : NULL;
        g_print("engine-hangul-layout=%s\n", resolved && *resolved ? resolved : "(none)");
    }
    if (!row || g_strcmp0(row, "autoswitch") == 0) {
        int held = 0;
        const bool ok = state && hunmin_state_get_autoswitch(state, &held);
        g_print("engine-autoswitch=%s\n", ok && held ? "true" : "false");
    }
    /* Rejected values are part of what the store says, not a detail: a row whose
     * value the engine refused is not the row the person set. */
    if (!row) g_print("rejected=%d\n", hunmin_ibus_settings_rejected_count(s));

    g_clear_object(&gs);
    if (state) hunmin_state_free(state);
    if (ctx) hunmin_ctx_free(ctx);
    hunmin_ibus_settings_free(s);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && g_strcmp0(argv[1], "--print") == 0)
        return print_rows(argc >= 3 ? argv[2] : NULL);
    if (argc >= 2 && (g_strcmp0(argv[1], "--help") == 0 || g_strcmp0(argv[1], "-h") == 0)) {
        g_print("usage: hunmin-ibus-settings [--print [row]]\n"
                "  (no arguments)  open the settings window\n"
                "  --print [row]   print the stored rows as the engine reads them\n");
        return 0;
    }

    gtk_init(&argc, &argv);

    GSettings *gs = g_settings_new(SCHEMA_ID);
    if (!gs) {
        g_printerr("hunmin-ibus-settings: schema %s is not installed\n", SCHEMA_ID);
        return 1;
    }
    /* The layout list is the core's, resolved from HUNMIN_DATA_PATH and then the
     * installed data directory — the same rule every other consumer follows. No
     * path is compiled in here. */
    hunmin_ctx *ctx = hunmin_ctx_new(NULL, 0);
    hunmin_ibus_ui *ui = hunmin_ibus_ui_new(gs, ctx);
    gtk_main();

    hunmin_ibus_ui_free(ui);
    if (ctx) hunmin_ctx_free(ctx);
    g_clear_object(&gs);
    return 0;
}