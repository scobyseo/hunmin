/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * settingspath.h - the ONE settings-exe resolver (stage S). The WinUI
 * surface, the langbar launcher and the Start-menu shortcut all ask here,
 * so an installed (or overridden) exe cannot be found by one entry point
 * and missed by another. Wait policy stays with the caller: FnConfigure
 * waits on the process, the langbar does not.
 */
#ifndef HUNMIN_TSF_SETTINGSPATH_H_
#define HUNMIN_TSF_SETTINGSPATH_H_

#include <cstddef>

/* Candidate order (maintainer ruling 2026-10-22, "deployed-first"):
 *   env HUNMIN_TSF_SETTINGS_EXE >
 *   <dll>\settings-ui\publish >
 *   <dll>\settings-ui >
 *   <dll>\..\settings-ui\publish >
 *   <dll>\..\..\settings-ui\publish
 * Returns false when nothing exists (or the module path cannot be read).
 * exe is NUL-terminated and empty on failure. */
bool hunmin_tsf_settings_exe_find(const void *anchor, wchar_t *exe, size_t cap);

/* The candidate list for an already-known DLL directory, without the
 * environment override (the shortcut builder has the path at hand). */
bool hunmin_tsf_settings_exe_in_dir(const wchar_t *dllDir, wchar_t *exe,
                                    size_t cap);

#endif /* HUNMIN_TSF_SETTINGSPATH_H_ */
