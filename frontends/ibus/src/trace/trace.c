/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* T2-1's personal key log for the IBus frontend: the same dialect fcitx5,
 * Android and TSF emit (docs/trace-spec.md §"Personal key log"). This lives
 * in its own directory and is compiled only under HUNMIN_TRACE, so it is
 * measurement infrastructure, not adapter logic: a shipped engine has no
 * sink, and the 700-line thin-adapter budget counts only the shipped files. */
static FILE *sink;
static char commit_buf[512];
static char preedit_buf[512];
/* The engine tells us the layout it actually resolved (not the requested env
 * id, which may have failed to load). The meta line is written on the first
 * key, after init, so this is always populated for a real run. */
static char layout_buf[64] = "";

static void trace_json(FILE *f, const char *s);

void hunmin_ibus_trace_set_layout(const char *id) {
    snprintf(layout_buf, sizeof layout_buf, "%s", id ? id : "");
}

static FILE *trace_sink(void) {
    if (!sink) {
        const char *path = getenv("HUNMIN_TRACE_OUT");
        sink = (path && *path) ? fopen(path, "a") : stderr;
        fputs("{\"meta\":{\"platform\":\"x11\",\"layout\":", sink);
        trace_json(sink, layout_buf);
        fputs(",\"case\":\"personal\",\"dialect\":\"personal-key\"}}\n", sink);
        fflush(sink);
    }
    return sink;
}

static void trace_json(FILE *f, const char *s) {
    fputc('"', f);
    for (; s && *s; ++s) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
    fputc('"', f);
}

void hunmin_ibus_trace_reset(void) {
    commit_buf[0] = preedit_buf[0] = '\0';
}

void hunmin_ibus_trace_commit(const char *text) {
    size_t used = strlen(commit_buf);
    if (text && *text && used + 1 < sizeof commit_buf)
        snprintf(commit_buf + used, sizeof commit_buf - used, "%s", text);
}

void hunmin_ibus_trace_preedit(const char *text) {
    snprintf(preedit_buf, sizeof preedit_buf, "%s", text ? text : "");
}

void hunmin_ibus_trace_event(const hunmin_key_event *ev, int consumed, unsigned int raw) {
    FILE *f = trace_sink();
    fprintf(f,
            "{\"in\":{\"base\":%u,\"shift\":%u,\"mods\":%u,\"id\":%u},"
            "\"commit\":",
            ev->base, ev->shift, ev->mods, (unsigned)ev->id);
    trace_json(f, commit_buf);
    fputs(",\"preedit\":", f);
    trace_json(f, preedit_buf);
    /* Diagnostic-only raw IBus keyval: base is 0 for every unmapped key, so
     * without this a Hangul press and a Right Alt press are indistinguishable
     * in the log. Off unless HUNMIN_TRACE_RAW is set. */
    if (getenv("HUNMIN_TRACE_RAW")) fprintf(f, ",\"raw\":%u", raw);
    fprintf(f, ",\"consumed\":%s}\n", consumed ? "true" : "false");
    fflush(f);
}
