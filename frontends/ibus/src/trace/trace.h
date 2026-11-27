/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef HUNMIN_IBUS_TRACE_H
#define HUNMIN_IBUS_TRACE_H

#include <hunmin.h>

/* Opt-in T2-1 personal key log (docs/trace-spec.md). Compiled only with
 * -DHUNMIN_TRACE=ON; the macros are no-ops in a shipped engine, so the key
 * path carries no sink and no getenv unless deliberately built with one.
 *
 * HUNMIN_TRACE_JUDGED(l) rides on the event line: it is the judgement the engine
 * ACTED ON at that key (HUNMIN_LANGUAGE_*), the only record of a rewrite — the
 * commit alone cannot tell "the person typed Hangul" from "the engine rewrote
 * it", which is the event T2 is about. */
#ifdef HUNMIN_TRACE
void hunmin_ibus_trace_set_layout(const char *id);
void hunmin_ibus_trace_reset(void);
void hunmin_ibus_trace_commit(const char *text);
void hunmin_ibus_trace_preedit(const char *text);
void hunmin_ibus_trace_judged(int language);
void hunmin_ibus_trace_event(const hunmin_key_event *ev, int consumed, unsigned int raw);
#define HUNMIN_TRACE_SET_LAYOUT(id) hunmin_ibus_trace_set_layout(id)
#define HUNMIN_TRACE_RESET() hunmin_ibus_trace_reset()
#define HUNMIN_TRACE_COMMIT(t) hunmin_ibus_trace_commit(t)
#define HUNMIN_TRACE_PREEDIT(t) hunmin_ibus_trace_preedit(t)
#define HUNMIN_TRACE_JUDGED(l) hunmin_ibus_trace_judged(l)
#define HUNMIN_TRACE_EVENT(e, used, raw) hunmin_ibus_trace_event((e), (used), (raw))
#else
#define HUNMIN_TRACE_SET_LAYOUT(id) ((void)0)
#define HUNMIN_TRACE_RESET() ((void)0)
#define HUNMIN_TRACE_COMMIT(t) ((void)0)
#define HUNMIN_TRACE_PREEDIT(t) ((void)0)
#define HUNMIN_TRACE_JUDGED(l) ((void)0)
#define HUNMIN_TRACE_EVENT(e, used, raw) ((void)0)
#endif

#endif
