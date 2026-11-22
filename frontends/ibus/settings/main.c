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

#include <string.h>

#include "settings.h"

#define SCHEMA_ID "org.freedesktop.ibus.engine.hunmin"

typedef struct {
    GSettings *gs;
    hunmin_ctx *ctx;
    GtkWidget *window;
    GtkWidget *hangul_combo;
    GtkWidget *latin_combo;
    GtkWidget *commit_combo;
    GtkWidget *autoswitch_check;
    GtkWidget *keys_list;
    GtkWidget *status;
} App;

/* ----------------------------------------------------------------- layout data */

/* FE12 / invariant I11: the picker reads the shipped list from the core. A
 * hardcoded list here would be a second place to edit per layout. */
static void fill_layouts(App *a, GtkWidget *combo, const char *mode, const char *selected) {
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo));
    const int n = a->ctx ? hunmin_ctx_layout_count(a->ctx) : 0;
    int active = -1;
    int row = 0;
    for (int i = 0; i < n; ++i) {
        if (!hunmin_ctx_layout_supported(a->ctx, i)) continue;
        const char *layout_mode = hunmin_ctx_layout_mode(a->ctx, i);
        if (g_strcmp0(layout_mode, mode) != 0) continue;
        const char *id = hunmin_ctx_layout_id(a->ctx, i);
        const char *name = hunmin_ctx_layout_name(a->ctx, i, NULL);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), id, name && *name ? name : id);
        if (g_strcmp0(id, selected) == 0) active = row;
        ++row;
    }
    if (active < 0 && row > 0) active = 0;
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
}

static void reload_layouts(App *a) {
    char *hangul = g_settings_get_string(a->gs, "hangul-layout");
    char *latin = g_settings_get_string(a->gs, "latin-layout");
    fill_layouts(a, a->hangul_combo, "hangul", hangul);
    fill_layouts(a, a->latin_combo, "latin", latin);
    g_free(hangul);
    g_free(latin);
}

/* ------------------------------------------------------------------ the rows */

static void on_hangul_changed(GtkComboBox *combo, gpointer user) {
    App *a = user;
    const char *id = gtk_combo_box_get_active_id(combo);
    if (id) g_settings_set_string(a->gs, "hangul-layout", id);
}

static void on_latin_changed(GtkComboBox *combo, gpointer user) {
    App *a = user;
    const char *id = gtk_combo_box_get_active_id(combo);
    if (id) g_settings_set_string(a->gs, "latin-layout", id);
}

/* The commit unit's two values, in the schema's own spelling. */
static void on_commit_changed(GtkComboBox *combo, gpointer user) {
    App *a = user;
    const char *id = gtk_combo_box_get_active_id(combo);
    if (id) g_settings_set_string(a->gs, "commit-unit", id);
}

static void on_autoswitch_toggled(GtkToggleButton *button, gpointer user) {
    App *a = user;
    g_settings_set_boolean(a->gs, "autoswitch", gtk_toggle_button_get_active(button));
}

/* ------------------------------------------------------------- the fifth row */

static char **stored_keys(App *a, gsize *n) {
    char **keys = g_settings_get_strv(a->gs, "switch-keys");
    *n = keys ? g_strv_length(keys) : 0;
    return keys;
}

static void store_keys(App *a, char **keys) {
    g_settings_set_strv(a->gs, "switch-keys", (const gchar *const *)keys);
}

static void refresh_keys(App *a);

static void on_remove_clicked(GtkButton *button, gpointer user) {
    App *a = user;
    const guint index = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "index"));
    gsize n = 0;
    char **keys = stored_keys(a, &n);
    if (index < n) {
        g_free(keys[index]);
        for (gsize j = index; j + 1 < n; ++j) keys[j] = keys[j + 1];
        keys[n - 1] = NULL;
        store_keys(a, keys);
    }
    g_strfreev(keys);
    refresh_keys(a);
}

static void refresh_keys(App *a) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(a->keys_list));
    for (GList *l = children; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    gsize n = 0;
    char **keys = stored_keys(a, &n);
    for (gsize i = 0; i < n; ++i) {
        /* Each entry is validated as it is SHOWN, so a value that would never
         * fire says so instead of looking stored. */
        bool ok = false;
        const char *reason = hunmin_ibus_stored_key_ok(keys[i], &ok);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *label = gtk_label_new(keys[i]);
        gtk_widget_set_hexpand(label, TRUE);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
        if (!ok) {
            GtkWidget *why = gtk_label_new(reason ? reason : "invalid");
            gtk_style_context_add_class(gtk_widget_get_style_context(why), "dim-label");
            gtk_box_pack_start(GTK_BOX(hbox), why, FALSE, FALSE, 0);
        }
        GtkWidget *remove = gtk_button_new_with_label("제거");
        g_object_set_data(G_OBJECT(remove), "index", GUINT_TO_POINTER((guint)i));
        g_signal_connect(remove, "clicked", G_CALLBACK(on_remove_clicked), a);
        gtk_box_pack_start(GTK_BOX(hbox), remove, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(row), hbox);
        gtk_list_box_insert(GTK_LIST_BOX(a->keys_list), row, -1);
        gtk_widget_show_all(row);
    }
    g_strfreev(keys);
}

/* Capture: the user presses a key and the gesture becomes the entry. TSF15's
 * rule — a binding is captured, never typed — and everything the capture decides
 * is spelled by the engine-side helper, not here. */
static gboolean on_capture_key(GtkWidget *widget, GdkEventKey *event, gpointer user) {
    App *a = user;
    (void)widget;
    if (event->keyval == GDK_KEY_Escape) {
        gtk_dialog_response(GTK_DIALOG(gtk_widget_get_toplevel(widget)), GTK_RESPONSE_CANCEL);
        return TRUE;
    }
    const guint state = event->state;
    char *entry = hunmin_ibus_key_entry_from_event(
        event->keyval, (state & GDK_SHIFT_MASK) != 0, (state & GDK_CONTROL_MASK) != 0,
        (state & GDK_MOD1_MASK) != 0, (state & GDK_SUPER_MASK) != 0,
        (state & GDK_HYPER_MASK) != 0);
    if (!entry) return TRUE;

    bool ok = false;
    const char *reason = hunmin_ibus_stored_key_ok(entry, &ok);
    if (!ok) {
        gtk_label_set_text(GTK_LABEL(g_object_get_data(G_OBJECT(a->window), "capture-hint")),
                           reason ? reason : "invalid");
        g_free(entry);
        return TRUE;
    }
    gtk_dialog_response(GTK_DIALOG(gtk_widget_get_toplevel(widget)), GTK_RESPONSE_ACCEPT);
    g_object_set_data_full(G_OBJECT(a->window), "captured", entry, g_free);
    return TRUE;
}

static void on_add_clicked(GtkButton *button, gpointer user) {
    App *a = user;
    (void)button;
    g_object_set_data(G_OBJECT(a->window), "captured", NULL);

    GtkWidget *dialog = gtk_dialog_new_with_buttons("한/영 키 입력", GTK_WINDOW(a->window),
                                                   GTK_DIALOG_MODAL, "취소", GTK_RESPONSE_CANCEL,
                                                   NULL);
    GtkWidget *hint = gtk_label_new("키를 누르세요 (ESC: 취소)");
    g_object_set_data(G_OBJECT(a->window), "capture-hint", hint);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hint, TRUE, TRUE,
                       12);
    /* Key events reach the dialog only if it asks for them. */
    gtk_widget_add_events(dialog, GDK_KEY_PRESS_MASK);
    g_signal_connect(dialog, "key-press-event", G_CALLBACK(on_capture_key), a);
    gtk_widget_show_all(dialog);
    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_object_set_data(G_OBJECT(a->window), "capture-hint", NULL);

    char *captured = g_object_get_data(G_OBJECT(a->window), "captured");
    if (response != GTK_RESPONSE_ACCEPT || !captured) return;

    /* Append unless the chord is already there: two entries that match the same
     * event are a duplicate a person cannot tell apart in the list. */
    gsize n = 0;
    char **keys = stored_keys(a, &n);
    bool present = false;
    for (gsize i = 0; i < n; ++i)
        if (g_strcmp0(keys[i], captured) == 0) present = true;
    if (!present) {
        char **grown = g_new0(char *, n + 2);
        for (gsize i = 0; i < n; ++i) grown[i] = keys[i];
        grown[n] = g_strdup(captured);
        store_keys(a, grown);
        g_free(grown);
    }
    for (gsize i = 0; i < n; ++i) g_free(keys[i]);
    g_free(keys);
    refresh_keys(a);
}

/* ------------------------------------------------------------------ the window */

static GtkWidget *labelled_row(GtkGrid *grid, int row, const char *text, GtkWidget *control,
                               const char *why) {
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    gtk_grid_attach(grid, control, 1, row, 1, 1);
    if (why) {
        GtkWidget *note = gtk_label_new(why);
        gtk_style_context_add_class(gtk_widget_get_style_context(note), "dim-label");
        gtk_widget_set_halign(note, GTK_ALIGN_START);
        gtk_label_set_line_wrap(GTK_LABEL(note), TRUE);
        gtk_grid_attach(grid, note, 1, row + 1, 1, 1);
    }
    return control;
}

static void build_window(App *a) {
    a->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(a->window), "hunmin 설정");
    gtk_window_set_default_size(GTK_WINDOW(a->window), 460, -1);
    g_signal_connect(a->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 16);
    gtk_container_add(GTK_CONTAINER(a->window), outer);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_box_pack_start(GTK_BOX(outer), grid, FALSE, FALSE, 0);

    /* Row 1-2: layouts. */
    a->hangul_combo = gtk_combo_box_text_new();
    labelled_row(GTK_GRID(grid), 0, "한글 배열", a->hangul_combo, NULL);
    a->latin_combo = gtk_combo_box_text_new();
    labelled_row(GTK_GRID(grid), 1, "영어 배열", a->latin_combo, NULL);

    /* Row 3: the commit unit, with the reason it is a preference at all. */
    a->commit_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(a->commit_combo), "word", "단어 단위");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(a->commit_combo), "syllable", "음절 단위");
    labelled_row(GTK_GRID(grid), 2, "커밋 단위", a->commit_combo,
                 "두 값의 차이는 언제 전달되는가이고, 전달되는 글자는 같습니다.");

    /* Row 4: the automatic switch. Its value is a REQUEST: the engine refuses ON
     * without the English word list (B3), and the schema's description says so —
     * this window does not restate the data gate, it shows whether the data is
     * here at all. */
    a->autoswitch_check = gtk_check_button_new_with_label("자동 한/영 전환");
    const bool data = a->ctx ? hunmin_ctx_autoswitch_data_present(a->ctx) : false;
    gtk_widget_set_sensitive(a->autoswitch_check, data);
    labelled_row(GTK_GRID(grid), 4, "자동 전환", a->autoswitch_check,
                 data ? NULL : "사용할 수 없음: 사전(modes/en-US.json)이 설치되어 있지 않습니다.");

    /* Row 5: the language keys. */
    a->keys_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(a->keys_list), GTK_SELECTION_NONE);
    GtkWidget *keys_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(keys_box), a->keys_list, FALSE, FALSE, 0);
    GtkWidget *add = gtk_button_new_with_label("키 추가");
    g_signal_connect(add, "clicked", G_CALLBACK(on_add_clicked), a);
    gtk_widget_set_halign(add, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(keys_box), add, FALSE, FALSE, 0);
    labelled_row(GTK_GRID(grid), 6, "한/영 키", keys_box,
                 "한 물리 키가 배열에 따라 다른 키심으로 오므로 보통 두 개가 필요합니다"
                 " (us: Alt_R, kr: Hangul). 이 키들은 IBus의 전역 트리거에서도 빠집니다.");

    a->status = gtk_label_new("");
    gtk_widget_set_halign(a->status, GTK_ALIGN_START);
    gtk_label_set_line_wrap(GTK_LABEL(a->status), TRUE);
    gtk_box_pack_start(GTK_BOX(outer), a->status, FALSE, FALSE, 0);

    GtkWidget *close = gtk_button_new_with_label("닫기");
    g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_widget_destroy), a->window);
    gtk_widget_set_halign(close, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(outer), close, FALSE, FALSE, 0);

    /* The rows are read once at build time; the engine is the live reader. A
     * change here is written immediately (no OK/Apply), which is what §FE17's
     * live-apply ruling means from the window's side. */
    reload_layouts(a);
    char *unit = g_settings_get_string(a->gs, "commit-unit");
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(a->commit_combo), unit);
    g_free(unit);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a->autoswitch_check),
                                 g_settings_get_boolean(a->gs, "autoswitch"));
    refresh_keys(a);

    g_signal_connect(a->hangul_combo, "changed", G_CALLBACK(on_hangul_changed), a);
    g_signal_connect(a->latin_combo, "changed", G_CALLBACK(on_latin_changed), a);
    g_signal_connect(a->commit_combo, "changed", G_CALLBACK(on_commit_changed), a);
    g_signal_connect(a->autoswitch_check, "toggled", G_CALLBACK(on_autoswitch_toggled), a);

    gtk_widget_show_all(a->window);
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    App a = {0};
    a.gs = g_settings_new(SCHEMA_ID);
    if (!a.gs) {
        g_printerr("hunmin-ibus-settings: schema %s is not installed\n", SCHEMA_ID);
        return 1;
    }
    /* The layout list is the core's, resolved from HUNMIN_DATA_PATH and then the
     * installed data directory — the same rule every other consumer follows. No
     * path is compiled in here. */
    a.ctx = hunmin_ctx_new(NULL, 0);

    build_window(&a);
    gtk_main();

    if (a.ctx) hunmin_ctx_free(a.ctx);
    g_clear_object(&a.gs);
    return 0;
}