/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1: the ONE test executable for the case groups (config, settings,
 * candidate, datadir, langbar, legacy) - issue #109, the merged shape. The
 * registered entries select groups with --gtest_filter at registration.
 *
 * Since the include-.cpp white-box seam was retired (DIP + SAP,
 * docs/REFACTOR-TSF-COMPONENTS.md), this file carries only the link-wide
 * definitions: the dllmain-owned hinst the shell references. main() comes
 * from GTest::gtest_main, linked in the CMake target; the fragments are
 * standalone TUs listed in the target's sources, the fixtures live in
 * tsf-candidate-fixture.h, and the key-state provider in
 * tsf-keystate-mock.h.
 */

/* Definition for the extern the shell (tsf.cpp) declares; dllmain.cpp
 * (which owns it in the DLL build) is not linked into the harness. */
#include <windows.h>

extern "C" HINSTANCE g_hunmin_hinst = nullptr;
