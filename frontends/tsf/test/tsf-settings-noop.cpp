/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The settings-surface stand-in for the tier-F1 menu test: OnMenuSelect(1)
 * must reach ShellExecuteExW, and spawning the real WinUI exe from a test is
 * both slow and visible. This process does nothing and exits, so the test
 * asserts the wiring (id 1 -> launch -> S_OK) without a window. It is a test
 * target only; the shipped DLL never refers to it.
 */
#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) { return 0; }
