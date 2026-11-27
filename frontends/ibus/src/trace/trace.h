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
 * HUNMIN_TRACE_JUDGED(l) and HUNMIN_TRACE_MODE(l) ride on the event line: the
 * judgement the engine ACTED ON at that key and the language the layer is in
 * AFTER it (both HUNMIN_LANGUAGE_*). The commit alone cannot tell "the person
 * typed Hangul" from "the engine rewrote it", and the preedit alone cannot say
 * which mode rendered it — which are the two things Track 2 is about. Each line
 * also carries `t`, milliseconds since the log opened, because "the person
 * noticed and stopped" is only visible with time (docs/trace-spec.md). */
#ifdef HUNMIN_TRACE
void hunmin_ibus_trace_set_layout(const char *id);
void hunmin_ibus_trace_reset(void);
void hunmin_ibus_trace_commit(const char *text);
void hunmin_ibus_trace_preedit(const char *text);
void hunmin_ibus_trace_judged(int language);
void hunmin_ibus_trace_mode(int language);
void hunmin_ibus_trace_event(const hunmin_key_event *ev, int consumed, unsigned int raw);
#define HUNMIN_TRACE_SET_LAYOUT(id) hunmin_ibus_trace_set_layout(id)
#define HUNMIN_TRACE_RESET() hunmin_ibus_trace_reset()
#define HUNMIN_TRACE_COMMIT(t) hunmin_ibus_trace_commit(t)
#define HUNMIN_TRACE_PREEDIT(t) hunmin_ibus_trace_preedit(t)
#define HUNMIN_TRACE_JUDGED(l) hunmin_ibus_trace_judged(l)
#define HUNMIN_TRACE_MODE(l) hunmin_ibus_trace_mode(l)
#define HUNMIN_TRACE_EVENT(e, used, raw) hunmin_ibus_trace_event((e), (used), (raw))
#else
#define HUNMIN_TRACE_SET_LAYOUT(id) ((void)0)
#define HUNMIN_TRACE_RESET() ((void)0)
#define HUNMIN_TRACE_COMMIT(t) ((void)0)
#define HUNMIN_TRACE_PREEDIT(t) ((void)0)
#define HUNMIN_TRACE_JUDGED(l) ((void)0)
#define HUNMIN_TRACE_MODE(l) ((void)0)
#define HUNMIN_TRACE_EVENT(e, used, raw) ((void)0)
#endif

#endif
