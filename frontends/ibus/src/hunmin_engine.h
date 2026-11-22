/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef HUNMIN_IBUS_ENGINE_H_
#define HUNMIN_IBUS_ENGINE_H_

#include <ibus.h>
#include <hunmin.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The IBus half of the frontend: event mapping and presentation only
 * (AGENTS.md rule 4). The key path lives in hunmin_state.c, which knows nothing
 * about IBus; this object translates an IBus key event, drives that state, and
 * publishes what the state asks for on IBus's own D-Bus surface:
 *
 *   sink commit    -> ibus_engine_commit_text
 *   sink preedit   -> ibus_engine_update_preedit_text_with_mode
 *   sink lists     -> ibus_engine_update/show/hide_lookup_table
 *
 * Nothing here reads layout data, composes a syllable, buffers a word or
 * judges a language.
 */

#define HUNMIN_TYPE_IBUS_ENGINE (hunmin_ibus_engine_get_type())

/* Plain GType boilerplate, not G_DECLARE_FINAL_TYPE: the macro's autoptr
 * helpers are named after the parent type, and libibus declares no
 * glib_autoptr_clear_IBusEngine to pair with. */
typedef struct _HunminIbusEngine HunminIbusEngine;
typedef struct _HunminIbusEngineClass HunminIbusEngineClass;
GType hunmin_ibus_engine_get_type(void);
#define HUNMIN_IBUS_ENGINE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), HUNMIN_TYPE_IBUS_ENGINE, HunminIbusEngine))
#define HUNMIN_IS_IBUS_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), HUNMIN_TYPE_IBUS_ENGINE))

/* The state object the engine drives. Exposed for the tier-D2 harness and for
 * a future settings surface (the config backend is an open decision, see
 * docs/MILESTONES.md §0.4 Track 1); it is not part of a stable ABI. */
struct hunmin_state *hunmin_ibus_engine_state(HunminIbusEngine *engine);

/* The IBus key translation, exposed so tier D2 asserts the mapping itself
 * rather than a copy of it. Returns the shared vocabulary's event; the caller
 * sets `candidates_open` because only the state knows whether its list is up. */
hunmin_key_event hunmin_ibus_translate_key(guint keyval, guint state, int candidates_open);

#ifdef __cplusplus
}
#endif

#endif /* HUNMIN_IBUS_ENGINE_H_ */