/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The interim plain-Win32 settings dialog (issue #71): one shared "edit the
 * store" run. The rundll32 export (settings.cpp) and, historically, the
 * ITfFnConfigure::Show fallback call this; the ruled surface is the WinUI 3
 * exe (settingslaunch.cpp), and this dialog is reachable only through the
 * export until it is removed.
 */
#ifndef HUNMIN_TSF_SETTINGSDIALOG_H_
#define HUNMIN_TSF_SETTINGSDIALOG_H_

#include <windows.h>

/* TRUE when the user applied and the store was written; false on cancel,
 * close, a failed save or a template failure. parent may be nullptr. */
bool hunmin_tsf_settings_dialog_run(HWND parent);

#endif /* HUNMIN_TSF_SETTINGSDIALOG_H_ */
