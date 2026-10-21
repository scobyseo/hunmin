/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The delivery seam's implementations (issue #119). The interface (struct
 * Delivery) and the TsfCore slot the implementations drive of live in
 * tsf.h - the shell owns the abstraction (DIP); this header declares the
 * concrete modes and the registration entry the shell calls at
 * construction.
 */
#ifndef HUNMIN_TSF_DELIVERY_H_
#define HUNMIN_TSF_DELIVERY_H_

#include "tsf.h"

#include <string>
#include <windows.h>

/* Registration names for UseDelivery(). */
#define HUNMIN_TSF_DELIVERY_NATIVE "native"
#define HUNMIN_TSF_DELIVERY_INTERIM "interim"

/* Register this component's implementations into the shell: both modes
 * (NativeDelivery, InterimDelivery) arrive as HunminDeliveryFactory's. */
void hunmin_tsf_deliveries_register(HunminDeliveryRegistry &registry);

#endif /* HUNMIN_TSF_DELIVERY_H_ */
