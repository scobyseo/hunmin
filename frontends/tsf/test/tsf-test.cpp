/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1: the ONE test executable for the four runner groups (config,
 * settings, candidate, datadir) -- issue #109, the merged shape. Four ctest
 * entries remain, one per group, selected with --gtest_filter at
 * registration; the rc-77 skip probes live in tools/tsf-run-if-data.py,
 * outside this binary (gtest has no skip that exits nonzero).
 *
 * Why one TU: every runner white-boxes the adapter by including
 * ../src/tsf.cpp, so N executables compiled the heavy TU N times. The seam
 * lives here exactly once and the case-group fragments are included below
 * it; the fragments are not standalone TUs.
 *
 * The GetKeyState seam: the adapter (and the toggle matcher it includes)
 * read the thread input state, so the harness owns it. Applies over the
 * whole TU, mock-tsf.h included first.
 *
 * Exit 0 clean, 1 assertion failed, 77 skip (data absent -- the wrapper's
 * probe; a data tree present but unparseable is a red SetUp, not a skip).
 */
#include <gtest/gtest.h>

#include <windows.h>

#include "tsf-testdir.h"

extern "C" HINSTANCE g_hunmin_hinst = nullptr;

SHORT g_mockKeyState[256] = {};
SHORT tsf_mock_GetKeyState(int vk) { return g_mockKeyState[vk & 0xFF]; }
#define GetKeyState tsf_mock_GetKeyState
/* No real tray icon from a console test process (issue #103's seam). */

#include "mock-tsf.h"
#define private public
#include "../src/tsf.cpp"
#undef private
#undef GetKeyState

#include "tsf-config-test.cpp"
#include "tsf-settings-test.cpp"
#include "tsf-candidate-test.cpp"
#include "tsf-langbaritem-test.cpp"
#include "tsf-datadir-test.cpp"
#include "tsf-legacy-test.cpp"
