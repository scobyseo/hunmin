/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The registration flow's one outside-world seam (issue #107).
 *
 * DllRegisterServer / DllUnregisterServer reach outside this process three
 * ways: COM activation (the two msctf services the flow drives), the
 * HKCR/HKLM registry trees, and the tree deletions. All three go through this
 * table, so tier F1 can run the SAME registration code against doubles.
 *
 * The Start-menu entry left this seam when the ruling of 2026-10-30 (issue
 * #146) gave it to the installer: the DLL writes no file any more, so there is
 * no file operation to double.
 *
 * The default table is the platform call itself - one indirection, no
 * behaviour change - and hunmin_tsf_regseam_install(NULL) restores it. That is
 * the shape hunmin_tsf_keystate_register() already has (keybinding.h): the
 * seam is not compiled out of the production build, because a #ifdef'd seam
 * runs different code than the one the tests prove.
 *
 * Why the registry is in the seam and not just CoCreateInstance (measured
 * 2026-10-26 on the dev host): registration writes HKCR\CLSID\{...} and
 * HKLM\SOFTWARE\Microsoft\CTF\TIP\{...} (elevation only), and
 * DllUnregisterServer DELETES both trees. An unseamed harness would either
 * fail on access or, on an elevated run, destroy the machine's real TIP
 * registration - so a test over the unseamed flow is a destructive test.
 */
#ifndef HUNMIN_TSF_REGISTRATIONSEAM_H_
#define HUNMIN_TSF_REGISTRATIONSEAM_H_

#include <windows.h>

typedef struct HunminRegSeam {
    /* COM activation for CLSID_TF_InputProcessorProfiles and
     * CLSID_TF_CategoryMgr (the platform table always asks for
     * CLSCTX_INPROC_SERVER). */
    HRESULT (*co_create)(REFCLSID rclsid, REFIID riid, void **ppv);
    /* RegCreateKeyExW(root, path, KEY_WRITE, REG_OPTION_NON_VOLATILE). */
    LONG (*reg_create_key)(HKEY root, const wchar_t *path, HKEY *out);
    /* RegOpenKeyExW(root, path, KEY_SET_VALUE). */
    LONG (*reg_open_key)(HKEY root, const wchar_t *path, HKEY *out);
    /* RegSetValueExW(key, name, type, data, bytes). */
    LONG (*reg_set_value)(HKEY key, const wchar_t *name, DWORD type, const void *data, DWORD bytes);
    void (*reg_close_key)(HKEY key);
    /* RegDeleteTreeW. */
    LONG (*reg_delete_tree)(HKEY root, const wchar_t *path);
} HunminRegSeam;

/* The table in force: the installed one, else the platform's. */
const HunminRegSeam *hunmin_tsf_regseam();
/* Install a table for the calling process; NULL restores the platform table.
 * The caller owns the table for as long as it stays installed. */
void hunmin_tsf_regseam_install(const HunminRegSeam *seam);

#endif /* HUNMIN_TSF_REGISTRATIONSEAM_H_ */
