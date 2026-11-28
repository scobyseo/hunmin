/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* T2-1's personal key log for the IBus frontend: the same dialect fcitx5,
 * Android and TSF emit (docs/trace-spec.md §"Personal key log"). This lives
 * in its own directory and is compiled only under HUNMIN_TRACE, so it is
 * measurement infrastructure, not adapter logic: a shipped engine has no
 * sink, and the 700-line thin-adapter budget counts only the shipped files. */
static FILE *sink;
static char commit_buf[512];
static char preedit_buf[512];
/* The judgement the engine acted on for the key being traced: -1 none, else
 * HUNMIN_LANGUAGE_*. Set between HUNMIN_TRACE_RESET() and HUNMIN_TRACE_EVENT()
 * by the key path, so one line = one key = at most one rewrite. */
static int judged_lang = -1;
/* The layer's language AFTER the traced key (HUNMIN_LANGUAGE_*), -1 = unknown.
 * Preedit + mode is what makes a "도깨비" readable: the same text rendered by
 * the other language is the flip. */
static int mode_lang = -1;
/* Milliseconds since the log opened. "Noticed and stopped" (a long gap before a
 * repair) is only visible with a clock, and telling a typo from a repair needs
 * it — docs/trace-spec.md §"Personal key log". */
static long long opened_ms = -1;
/* The engine tells us the layout it actually resolved (not the requested env
 * id, which may have failed to load). The meta line is written on the first
 * key, after init, so this is always populated for a real run. */
static char layout_buf[64] = "";

static void trace_json(FILE *f, const char *s);

static long long now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000);
}

void hunmin_ibus_trace_set_layout(const char *id) {
    snprintf(layout_buf, sizeof layout_buf, "%s", id ? id : "");
}

static FILE *trace_sink(void) {
    if (!sink) {
        const char *path = getenv("HUNMIN_TRACE_OUT");
        sink = (path && *path) ? fopen(path, "a") : stderr;
        opened_ms = now_ms();
        fputs("{\"meta\":{\"platform\":\"x11\",\"layout\":", sink);
        trace_json(sink, layout_buf);
        char started[32] = "";
        const time_t wall = time(NULL);
        struct tm tm_utc;
        if (gmtime_r(&wall, &tm_utc) != NULL)
            strftime(started, sizeof started, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
        /* Which DATA this log was produced with. A replay of the same keys is only
         * faithful against the same layouts and dictionary: the dev lib defaults to
         * its own install prefix and the packaged one to /usr/share/hunmin, and the
         * two files differ (measured 2026-11-28, personal-log-flips found 18 flips in
         * the log and 229 in a replay that read the checkout's data). */
        const char *data_env = getenv("HUNMIN_DATA_PATH");
        fputs(",\"data\":\"", sink);
        trace_json(sink, data_env && *data_env ? data_env : "");
        fputs("\"", sink);
        fputs(",\"case\":\"personal\",\"dialect\":\"personal-key\",\"started\":\"", sink);
        fputs(started, sink);
        fputs("\"}}\n", sink);
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
    judged_lang = -1;
}

void hunmin_ibus_trace_commit(const char *text) {
    size_t used = strlen(commit_buf);
    if (text && *text && used + 1 < sizeof commit_buf)
        snprintf(commit_buf + used, sizeof commit_buf - used, "%s", text);
}

void hunmin_ibus_trace_preedit(const char *text) {
    snprintf(preedit_buf, sizeof preedit_buf, "%s", text ? text : "");
}

void hunmin_ibus_trace_judged(int language) {
    judged_lang = language;
}

void hunmin_ibus_trace_mode(int language) {
    mode_lang = language;
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
    /* The rewrite, spelled the way hunmin-sim's --judge --trace spells it, so a
     * reader (and tools/gen-conformance.py's consumer) needs one vocabulary. */
    if (judged_lang >= 0)
        fprintf(f, ",\"judged\":\"%s\"",
                judged_lang == HUNMIN_LANGUAGE_LATIN ? "latin" : "hangul");
    if (mode_lang >= 0)
        fprintf(f, ",\"mode\":\"%s\"",
                mode_lang == HUNMIN_LANGUAGE_LATIN ? "latin" : "hangul");
    /* The Han/Eng key: it is CONSUMED (the engine toggles), so a reader that
     * segmented words by `consumed` would glue it onto the next word. It is not
     * typing — it is the person changing their assumption — and the intent model
     * (docs/MODES-MEASUREMENT.md §1.6) needs it named. */
    if (ev->language_key) fputs(",\"lang\":true", f);
    if (opened_ms >= 0) fprintf(f, ",\"t\":%lld", now_ms() - opened_ms);
    fprintf(f, ",\"consumed\":%s}\n", consumed ? "true" : "false");
    fflush(f);
}
