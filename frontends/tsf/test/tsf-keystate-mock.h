/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The tier-F1 key-state mock: the snapshot table every fragment drives and
 * the provider it registers into keybinding.h's slot (the seam the old
 * per-TU #define GetKeyState macro carried). Inline so every test TU shares
 * one symbol set; the REGISTER call happens in the fragment's Begin/SetUp.
 */
#ifndef HUNMIN_TSF_TEST_KEYSTATE_MOCK_H_
#define HUNMIN_TSF_TEST_KEYSTATE_MOCK_H_

#include <windows.h>

#include "../src/keybinding.h"

inline SHORT g_mockKeyState[256] = {};

inline SHORT tsf_mock_GetKeyState(int vk) {
    return g_mockKeyState[vk & 0xFF];
}

/* Idempotent: registering twice replaces the slot with the same fn. */
inline void tsf_test_register_keystate() {
    hunmin_tsf_keystate_register(&tsf_mock_GetKeyState);
}

#endif /* HUNMIN_TSF_TEST_KEYSTATE_MOCK_H_ */
