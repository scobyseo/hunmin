/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Out-of-line bodies for the registration doubles (mock-tsf-registration.h).
 * The seam is a context-free function table, so the table's functions reach
 * the ACTIVE double set through the file-scope pointer below: one set per
 * process, installed scoped-object style.
 */
#include "mock-tsf-registration.h"

#include <cstdint>

namespace tsfmock {

/* ---- the registry double ---- */

std::wstring MockRegistry::RootName(HKEY root) {
    if (root == HKEY_CLASSES_ROOT)
        return L"HKCR";
    if (root == HKEY_LOCAL_MACHINE)
        return L"HKLM";
    if (root == HKEY_CURRENT_USER)
        return L"HKCU";
    return L"HK<other>";
}

LONG MockRegistry::CreateKey(HKEY root, const wchar_t *path, HKEY *out) {
    std::wstring full = RootName(root) + L"|" + (path ? path : L"");
    keys_.push_back(full);
    trace_.created.push_back(full);
    trace_.event(L"RegCreateKey");
    if (out)
        *out = (HKEY)(uintptr_t)keys_.size();
    return ERROR_SUCCESS;
}

LONG MockRegistry::OpenKey(HKEY root, const wchar_t *path, HKEY *out) {
    std::wstring full = RootName(root) + L"|" + (path ? path : L"");
    keys_.push_back(full);
    trace_.created.push_back(full);
    trace_.event(L"RegOpenKey");
    if (out)
        *out = (HKEY)(uintptr_t)keys_.size();
    return ERROR_SUCCESS;
}

std::wstring MockRegistry::PathFor(HKEY key) const {
    size_t index = (size_t)(uintptr_t)key;
    if (index == 0 || index > keys_.size())
        return L"<invalid handle>";
    return keys_[index - 1];
}

LONG MockRegistry::SetValue(HKEY key, const wchar_t *name, DWORD type, const void *data, DWORD bytes) {
    std::wstring value;
    if (type == REG_SZ && data) {
        value.assign(static_cast<const wchar_t *>(data));
    } else if (type == REG_DWORD && data && bytes >= sizeof(DWORD)) {
        value = std::to_wstring(*(static_cast<const DWORD *>(data)));
    } else {
        value = L"<type " + std::to_wstring(type) + L">";
    }
    trace_.writes.push_back(PathFor(key) + L"|" + (name ? name : L"(default)") + L"|" + value);
    trace_.event(L"RegSetValue");
    return ERROR_SUCCESS;
}

LONG MockRegistry::DeleteTree(HKEY root, const wchar_t *path) {
    trace_.deletes.push_back(RootName(root) + L"|" + (path ? path : L""));
    trace_.event(L"RegDeleteTree");
    return ERROR_SUCCESS;
}

/* ---- ITfInputProcessorProfileMgr ---- */

HRESULT MockInputProcessorProfileMgr::ActivateProfile(DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID guidProfile, HKL hkl,
                                                      DWORD dwFlags) {
    (void)dwProfileType;
    (void)hkl;
    trace_.event(L"ActivateProfile");
    activateLangid = langid;
    activateClsid = clsid;
    activateProfile = guidProfile;
    activateFlags = dwFlags;
    return S_OK;
}

HRESULT MockInputProcessorProfileMgr::UnregisterProfile(REFCLSID rclsid, LANGID langid, REFGUID guidProfile, DWORD dwFlags) {
    (void)langid;
    trace_.event(L"UnregisterProfile");
    unregisterProfileClsid = rclsid;
    unregisterProfileGuid = guidProfile;
    unregisterProfileFlags = dwFlags;
    return S_OK;
}

HRESULT MockInputProcessorProfileMgr::GetActiveProfile(REFGUID catid, TF_INPUTPROCESSORPROFILE *pProfile) {
    (void)catid;
    trace_.event(L"GetActiveProfile");
    if (!pProfile)
        return E_INVALIDARG;
    if (IsEqualGUID(activeClsid, CLSID_NULL)) {
        /* Nothing active: the host's honest answer before a profile is
         * selected, and the branch that sends the adapter to the legacy
         * ActivateLanguageProfile fallback. */
        ZeroMemory(pProfile, sizeof *pProfile);
        return S_FALSE;
    }
    ZeroMemory(pProfile, sizeof *pProfile);
    pProfile->clsid = activeClsid;
    pProfile->guidProfile = activeProfile;
    pProfile->langid = activeLangid;
    return S_OK;
}

/* ---- ITfInputProcessorProfiles ---- */

HRESULT MockInputProcessorProfiles::Register(REFCLSID rclsid) {
    trace_.event(L"Register");
    registeredClsid = rclsid;
    return S_OK;
}

HRESULT MockInputProcessorProfiles::Unregister(REFCLSID rclsid) {
    trace_.event(L"Unregister");
    unregisteredClsid = rclsid;
    return S_OK;
}

HRESULT MockInputProcessorProfiles::AddLanguageProfile(REFCLSID rclsid, LANGID langid, REFGUID guidProfile, const WCHAR *pchDesc, ULONG cchDesc,
                                                       const WCHAR *pchIconFile, ULONG cchFile, ULONG uIconIndex) {
    (void)rclsid;
    trace_.event(L"AddLanguageProfile");
    addedLangid = langid;
    addedProfile = guidProfile;
    addedDescription.assign(pchDesc ? std::wstring(pchDesc, cchDesc) : std::wstring());
    addedIconFile.assign(pchIconFile ? std::wstring(pchIconFile, cchFile == 0 ? wcslen(pchIconFile) : cchFile) : std::wstring());
    addedIconFileChars = cchFile;
    addedIconIndex = uIconIndex;
    return S_OK;
}

HRESULT MockInputProcessorProfiles::EnableLanguageProfile(REFCLSID rclsid, LANGID langid, REFGUID guidProfile, BOOL fEnable) {
    (void)rclsid;
    trace_.event(L"EnableLanguageProfile");
    enabledLangid = langid;
    enabledProfile = guidProfile;
    enabled = fEnable;
    return S_OK;
}

HRESULT MockInputProcessorProfiles::ActivateLanguageProfile(REFCLSID rclsid, LANGID langid, REFGUID guidProfiles) {
    trace_.event(L"ActivateLanguageProfile");
    activatedClsid = rclsid;
    activatedLangid = langid;
    activatedProfile = guidProfiles;
    activateLanguageProfileCalled = true;
    return S_OK;
}

HRESULT MockInputProcessorProfiles::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv)
        return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfInputProcessorProfiles)) {
        *ppv = static_cast<ITfInputProcessorProfiles *>(this);
    } else if (IsEqualIID(riid, IID_ITfInputProcessorProfileMgr) && mgr_) {
        /* The manager is EMBEDDED in the double set, so the caller's own
         * AddRef/Release has to move the MANAGER's count, not this one's:
         * adding this object's count made the adapter's Release() free a
         * subobject (STATUS_HEAP_CORRUPTION, measured in the first
         * registration case -- the same defect the session doubles had). */
        *ppv = static_cast<ITfInputProcessorProfileMgr *>(mgr_);
        mgr_->AddRef();
        return S_OK;
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

/* ---- ITfCategoryMgr ---- */

HRESULT MockCategoryMgr::RegisterCategory(REFCLSID rclsid, REFGUID rcatid, REFGUID rguid) {
    (void)rclsid;
    (void)rguid;
    trace_.event(L"RegisterCategory");
    registered.push_back(rcatid);
    return S_OK;
}

HRESULT MockCategoryMgr::UnregisterCategory(REFCLSID rclsid, REFGUID rcatid, REFGUID rguid) {
    (void)rclsid;
    (void)rguid;
    trace_.event(L"UnregisterCategory");
    unregistered.push_back(rcatid);
    return S_OK;
}

/* ---- the seam table, built from the active set ---- */

static RegistrationDoubles *g_doubles = nullptr;

static HRESULT SeamCoCreate(REFCLSID rclsid, REFIID riid, void **ppv) {
    if (!g_doubles || !ppv)
        return E_FAIL;
    IUnknown *object = nullptr;
    if (IsEqualCLSID(rclsid, CLSID_TF_InputProcessorProfiles))
        object = static_cast<ITfInputProcessorProfiles *>(&g_doubles->profiles);
    else if (IsEqualCLSID(rclsid, CLSID_TF_CategoryMgr))
        object = static_cast<ITfCategoryMgr *>(&g_doubles->categoryMgr);
    else {
        /* Recorded, not silently failed: the seam no longer knows a shell link
         * (the installer owns the Start-menu entry, ruling of 2026-10-30,
         * issue #146), so an unrecognised activation is how a re-added file
         * write shows up in a test. */
        g_doubles->trace.event(L"CoCreateUnknown");
        *ppv = nullptr;
        return REGDB_E_CLASSNOTREG;
    }
    return object->QueryInterface(riid, ppv);
}

static LONG SeamCreateKey(HKEY root, const wchar_t *path, HKEY *out) {
    return g_doubles ? g_doubles->registry.CreateKey(root, path, out) : ERROR_ACCESS_DENIED;
}

static LONG SeamOpenKey(HKEY root, const wchar_t *path, HKEY *out) {
    return g_doubles ? g_doubles->registry.OpenKey(root, path, out) : ERROR_ACCESS_DENIED;
}

static LONG SeamSetValue(HKEY key, const wchar_t *name, DWORD type, const void *data, DWORD bytes) {
    return g_doubles ? g_doubles->registry.SetValue(key, name, type, data, bytes) : ERROR_ACCESS_DENIED;
}

static void SeamCloseKey(HKEY key) {
    if (g_doubles)
        g_doubles->registry.CloseKey(key);
}

static LONG SeamDeleteTree(HKEY root, const wchar_t *path) { return g_doubles ? g_doubles->registry.DeleteTree(root, path) : ERROR_ACCESS_DENIED; }

RegistrationDoubles::RegistrationDoubles() : registry(trace), profileMgr(trace), profiles(trace, &profileMgr), categoryMgr(trace) {
    seam.co_create = SeamCoCreate;
    seam.reg_create_key = SeamCreateKey;
    seam.reg_open_key = SeamOpenKey;
    seam.reg_set_value = SeamSetValue;
    seam.reg_close_key = SeamCloseKey;
    seam.reg_delete_tree = SeamDeleteTree;
}

void RegistrationDoubles::Install() {
    g_doubles = this;
    hunmin_tsf_regseam_install(&seam);
}

RegistrationDoubles::~RegistrationDoubles() {
    if (g_doubles == this) {
        hunmin_tsf_regseam_install(nullptr);
        g_doubles = nullptr;
    }
}

} // namespace tsfmock
