/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The DLL's COM exports (stage S4, docs/REFACTOR-TSF-COMPONENTS.md). The
 * registration helpers stay private to registration.cpp; only the three
 * names hunmin-tsf.def exports cross the TU boundary.
 */
#ifndef HUNMIN_TSF_REGISTRATION_H_
#define HUNMIN_TSF_REGISTRATION_H_

#include <windows.h>

extern "C" {
HRESULT DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv);
BOOL DllRegisterServer();
BOOL DllUnregisterServer();
}

#endif /* HUNMIN_TSF_REGISTRATION_H_ */
