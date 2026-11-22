/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * hunmin-ibus -- the IBus engine process.
 *
 * Registration is the component XML next to this binary (installed into
 * /usr/share/ibus/component, or pointed at with IBUS_COMPONENT_PATH), and the
 * daemon spawns this executable with --ibus. Everything that behaves is in
 * hunmin_engine.c and hunmin_state.c; this file only joins the bus.
 */
#include <ibus.h>

#include "hunmin_engine.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    ibus_init();

    IBusBus *bus = ibus_bus_new();
    if (!bus || !ibus_bus_is_connected(bus)) {
        g_printerr("hunmin-ibus: cannot reach the IBus daemon\n");
        return 1;
    }
    g_object_ref_sink(bus);

    IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(bus));
    if (!factory) return 1;
    ibus_factory_add_engine(factory, "hunmin", HUNMIN_TYPE_IBUS_ENGINE);

    /* The bus name the component XML's <name> declares. A second engine of the
     * same name is a packaging or session error, not something to retry. */
    if (!ibus_bus_request_name(bus, "org.freedesktop.IBus.Hunmin", 0)) {
        g_printerr("hunmin-ibus: org.freedesktop.IBus.Hunmin is already owned\n");
        return 1;
    }
    ibus_main();
    return 0;
}