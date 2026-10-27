/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The DLL's COM surface (stage S4): the class factory, DllGetClassObject and
 * the registration exports. The registration helpers are file-static and one
 * per artifact, so DllRegisterServer reads as the ordered list of what it
 * writes; the COM CLSID, profile and display-attribute GUIDs live in
 * ids.cpp because more than this TU names them.
 */
#include "registration.h"

#include "ids.h"
#include "registrationseam.h" /* the outside-world seam (issue #107) */
#include "settingspath.h" /* the shared settings-exe resolver */
#include "tsf.h" /* CTextService */

#include <cwchar>
#include <msctf.h>
#include <shlobj.h>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <shobjidl_core.h>
#include <windows.h>
#pragma comment(lib, "Shlwapi.lib")

extern "C" HINSTANCE g_hunmin_hinst; /* owned by dllmain.cpp */

/* --- the seam in force (issue #107) ---
 * Every outside-world call below goes through this table. The default table IS
 * the platform call, so production behaviour is the same sequence of the same
 * calls; tier F1 installs doubles and drives THIS code (registrationseam.h). */

static HRESULT SeamCoCreate(REFCLSID rclsid, REFIID riid, void **ppv) { return CoCreateInstance(rclsid, nullptr, CLSCTX_INPROC_SERVER, riid, ppv); }

static LONG SeamCreateKey(HKEY root, const wchar_t *path, HKEY *out) {
    return RegCreateKeyExW(root, path, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, out, nullptr);
}

static LONG SeamOpenKey(HKEY root, const wchar_t *path, HKEY *out) { return RegOpenKeyExW(root, path, 0, KEY_SET_VALUE, out); }

static LONG SeamSetValue(HKEY key, const wchar_t *name, DWORD type, const void *data, DWORD bytes) {
    return RegSetValueExW(key, name, 0, type, (const BYTE *)data, bytes);
}

static void SeamCloseKey(HKEY key) { RegCloseKey(key); }

static LONG SeamDeleteTree(HKEY root, const wchar_t *path) { return RegDeleteTreeW(root, path); }

static HRESULT SeamProgramsFolder(wchar_t *out, DWORD cap) {
    PWSTR programs = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &programs);
    if (SUCCEEDED(hr) && programs)
        lstrcpynW(out, programs, (int)cap);
    if (programs)
        CoTaskMemFree(programs);
    return hr;
}

static const HunminRegSeam kPlatformSeam = {SeamCoCreate, SeamCreateKey, SeamOpenKey, SeamSetValue, SeamCloseKey, SeamDeleteTree, SeamProgramsFolder};

static const HunminRegSeam *g_regSeam = nullptr;

const HunminRegSeam *hunmin_tsf_regseam() { return g_regSeam ? g_regSeam : &kPlatformSeam; }

void hunmin_tsf_regseam_install(const HunminRegSeam *seam) { g_regSeam = seam; }

/* --- registration helpers (S4: this block becomes its own TU) ---
 * One helper per registration artifact, so DllRegisterServer reads as the
 * ordered list below and each artifact logs its own outcome. */

static bool RegisterComServer(const wchar_t *guidStr, const wchar_t *dllPath) {
    const HunminRegSeam *seam = hunmin_tsf_regseam();
    HKEY hKey = nullptr;
    wchar_t clsidPath[128];
    swprintf(clsidPath, 128, L"CLSID\\%s", guidStr);
    LSTATUS st = seam->reg_create_key(HKEY_CLASSES_ROOT, clsidPath, &hKey);
    if (st != ERROR_SUCCESS)
        return false;

    HKEY hSub = nullptr;
    wchar_t inprocPath[256];
    swprintf(inprocPath, 256, L"%s\\InprocServer32", clsidPath);
    st = seam->reg_create_key(HKEY_CLASSES_ROOT, inprocPath, &hSub);
    if (st != ERROR_SUCCESS) {
        seam->reg_close_key(hKey);
        return false;
    }

    seam->reg_set_value(hSub, nullptr, REG_SZ, dllPath, (DWORD)((wcslen(dllPath) + 1) * sizeof(wchar_t)));
    const wchar_t *model = L"Apartment";
    seam->reg_set_value(hSub, L"ThreadingModel", REG_SZ, model, (DWORD)((wcslen(model) + 1) * sizeof(wchar_t)));

    seam->reg_close_key(hSub);
    seam->reg_close_key(hKey);
    return true;
}

/* TSF TIP category registration (so it is enumerated in input settings). */
static void RegisterTipKey(const wchar_t *guidStr) {
    const HunminRegSeam *seam = hunmin_tsf_regseam();
    HKEY hTIP = nullptr;
    wchar_t tipPath[256];
    swprintf(tipPath, 256, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s", guidStr);
    LSTATUS tipSt = seam->reg_create_key(HKEY_LOCAL_MACHINE, tipPath, &hTIP);
    if (tipSt == ERROR_SUCCESS) {
        const wchar_t *tipName = L"Hunmin";
        seam->reg_set_value(hTIP, nullptr, REG_SZ, tipName, (DWORD)((wcslen(tipName) + 1) * sizeof(wchar_t)));
        seam->reg_close_key(hTIP);
    }
}

/* Icon index pin (measured 2026-10-18: the live profile answered
 * IconFile=<dll> IconIndex=1 - a slot with no icon - and the language entry
 * rendered BLANK; user report "still no icon on langbar"). AddLanguageProfile
 * was called with uIconIndex=0 yet the stored value read 1, so registration
 * rewrites the stored index to 0 (resource id 1 IS the first icon of
 * hunmin.ico). */
static void PinProfileIconIndex(const wchar_t *guidStr) {
    const HunminRegSeam *seam = hunmin_tsf_regseam();
    wchar_t lp3412[256];
    swprintf(lp3412, _countof(lp3412), L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\LanguageProfile\\0x00000412\\%s", guidStr,
             L"{F2C70002-0000-0000-0000-000000000002}");
    for (DWORD pass = 0; pass < 2; ++pass) {
        wchar_t ikey[192];
        if (pass == 0)
            swprintf(ikey, _countof(ikey), L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\LanguageProfile\\0x00000412", guidStr);
        else
            lstrcpynW(ikey, lp3412, _countof(ikey));
        HKEY hIk = nullptr;
        if (seam->reg_open_key(HKEY_LOCAL_MACHINE, ikey, &hIk) == ERROR_SUCCESS) {
            DWORD zero = 0;
            seam->reg_set_value(hIk, L"IconIndex", REG_DWORD, &zero, sizeof zero);
            seam->reg_close_key(hIk);
        }
    }
}

/* Activation has to be asked for on the modern interface.
 *
 * ITfInputProcessorProfiles::ActivateLanguageProfile returns S_OK while
 * activating nothing beyond the calling process: measured on this machine,
 * GetActiveLanguageProfile then still answered S_FALSE for hunmin AND for
 * the Microsoft IME, and a fresh Notepad loaded MSCTF.dll but never
 * hunmin-tsf.dll -- no Activate(), no key sink.
 *
 * Measured for ITfInputProcessorProfileMgr::ActivateProfile: neither
 * TF_IPPMF_FORSESSION nor TF_IPPMF_FORSYSTEMALL (both with
 * TF_IPPMF_ENABLEPROFILE) persists past the registering process -- a fresh
 * process still reports the Microsoft IME as active. What they do record is
 * the enablement/profile entry, so the profile is selectable; selecting it
 * (Win+Space) then loads and activates the TIP in the focused process,
 * measured end to end. The CTF default assembly (HKCU ...CTF SortOrder index
 * 0 / Assemblies Default) is Windows Settings' store and is deliberately not
 * written here.
 */
static void ActivateInputProcessorProfile(ITfInputProcessorProfiles *pProfile, REFGUID profileGuid) {
    ITfInputProcessorProfileMgr *pProfileMgr = nullptr;
    HRESULT mgrHr = pProfile->QueryInterface(IID_ITfInputProcessorProfileMgr, (LPVOID *)&pProfileMgr);
    hunmin_reg_logger.log(L"pProfile QI(ITfInputProcessorProfileMgr) hr=0x%lX\n", (unsigned long)mgrHr);
    if (SUCCEEDED(mgrHr) && pProfileMgr) {
        HRESULT actHr = pProfileMgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, 0x0412, CLSID_HunminTSF, profileGuid, nullptr,
                                                     TF_IPPMF_FORSYSTEMALL | TF_IPPMF_ENABLEPROFILE);
        hunmin_reg_logger.log(L"pProfileMgr->ActivateProfile hr=0x%lX\n", (unsigned long)actHr);
        TF_INPUTPROCESSORPROFILE active;
        ZeroMemory(&active, sizeof(active));
        HRESULT getHr = pProfileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &active);
        hunmin_reg_logger.log(L"pProfileMgr->GetActiveProfile hr=0x%lX active.clsid={%08lX} active.profile=%08lX\n", (unsigned long)getHr,
                              (unsigned long)active.clsid.Data1, (unsigned long)active.guidProfile.Data1);
        if (FAILED(getHr) || !IsEqualGUID(active.clsid, CLSID_HunminTSF)) {
            /* Pre-Vista hosts, or a profile the session refused: the legacy
             * call is the only thing left to try. */
            HRESULT legacyHr = pProfile->ActivateLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid);
            hunmin_reg_logger.log(L"fallback ActivateLanguageProfile hr=0x%lX\n", (unsigned long)legacyHr);
        }
        pProfileMgr->Release();
    } else {
        HRESULT legacyHr = pProfile->ActivateLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid);
        hunmin_reg_logger.log(L"ActivateLanguageProfile (no ProfileMgr) hr=0x%lX\n", (unsigned long)legacyHr);
    }
}

static void RegisterInputProcessorProfile(const wchar_t *guidStr) {
    ITfInputProcessorProfiles *pProfile = nullptr;
    HRESULT hr = hunmin_tsf_regseam()->co_create(CLSID_TF_InputProcessorProfiles, IID_ITfInputProcessorProfiles, (LPVOID *)&pProfile);

    hunmin_reg_logger.log(L"pProfile CoCreate hr=0x%lX\n", (unsigned long)hr);
    if (FAILED(hr) || !pProfile)
        return;

    HRESULT regHr = pProfile->Register(CLSID_HunminTSF);
    hunmin_reg_logger.log(L"pProfile->Register hr=0x%lX\n", (unsigned long)regHr);
    static const GUID profileGuid = HunminProfileGuid;
    HMODULE hSelf = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)(const void *)&RegisterInputProcessorProfile, &hSelf);
    wchar_t iconPath[MAX_PATH] = L"";
    if (hSelf)
        GetModuleFileNameW(hSelf, iconPath, MAX_PATH);
    HRESULT addHr = pProfile->AddLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid, L"Hunmin", (ULONG)wcslen(L"Hunmin"),
                                                 iconPath[0] ? iconPath : nullptr, 0, TRUE);
    hunmin_reg_logger.log(L"pProfile->AddLanguageProfile hr=0x%lX\n", (unsigned long)addHr);
    HRESULT enableHr = pProfile->EnableLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid, TRUE);
    hunmin_reg_logger.log(L"pProfile->EnableLanguageProfile hr=0x%lX\n", (unsigned long)enableHr);
    PinProfileIconIndex(guidStr);
    ActivateInputProcessorProfile(pProfile, profileGuid);
    pProfile->Release();
}

static void RegisterTipCategories() {
    ITfCategoryMgr *pCatMgr = nullptr;
    HRESULT hr = hunmin_tsf_regseam()->co_create(CLSID_TF_CategoryMgr, IID_ITfCategoryMgr, (LPVOID *)&pCatMgr);
    hunmin_reg_logger.log(L"pCatMgr CoCreate hr=0x%lX\n", (unsigned long)hr);
    if (FAILED(hr) || !pCatMgr)
        return;
    HRESULT c1 = pCatMgr->RegisterCategory(CLSID_HunminTSF, GUID_TFCAT_TIP_KEYBOARD, CLSID_HunminTSF);
    HRESULT c2 = pCatMgr->RegisterCategory(CLSID_HunminTSF, GUID_TFCAT_CATEGORY_OF_TIP, CLSID_HunminTSF);
    /* Issue #98: the display-attribute provider category - the app-side
     * attribute manager resolves the above provider object through it, and
     * the composition range's GUID_PROP_ATTRIBUTE value carries
     * HunminDisplayAttrGuid, which the provider serves. */
    HRESULT c3 = pCatMgr->RegisterCategory(CLSID_HunminTSF, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_HunminTSF);
    hunmin_reg_logger.log(L"RegisterCategory keyboard=0x%lX category=0x%lX dispattr=0x%lX\n", (unsigned long)c1, (unsigned long)c2,
                          (unsigned long)c3);
    pCatMgr->Release();
}

/* The Start-menu entry (discoverability): "Hunmin IME 설정" under Programs.
 * The ruled surface (the settings-ui exe) wins when it is deployed beside the
 * DLL; the rundll32 entry into HunminSettingsShow is the fallback that always
 * works. Deleted in unregister, mirroring the tree work. */
static void CreateStartMenuShortcut(const wchar_t *dllPath) {
    wchar_t dllDir[MAX_PATH];
    lstrcpynW(dllDir, dllPath, MAX_PATH);
    PathRemoveFileSpecW(dllDir);
    wchar_t exe[MAX_PATH];
    /* The shared resolver: the installed <dll>\settings-ui shape now wins
     * over checkout climbs, so an installed shortcut targets the WinUI exe
     * instead of falling through to rundll32. */
    hunmin_tsf_settings_exe_in_dir(dllDir, exe, MAX_PATH);
    IShellLinkW *sl = nullptr;
    HRESULT lh = hunmin_tsf_regseam()->co_create(CLSID_ShellLink, __uuidof(IShellLinkW), (void **)&sl);

    hunmin_reg_logger.log(L"shortcut CoCreate hr=0x%lX exe=%s\n", (unsigned long)lh, exe[0] ? exe : L"<rundll32>");
    if (FAILED(lh) || !sl)
        return;
    if (exe[0]) {
        sl->SetPath(exe);
    } else {
        sl->SetPath(L"rundll32.exe");
        wchar_t arg[MAX_PATH + 32];
        _snwprintf(arg, _countof(arg), L"\"%s,HunminSettingsShow\"", dllPath);
        sl->SetArguments(arg);
    }
    sl->SetWorkingDirectory(dllDir);
    if (exe[0])
        sl->SetIconLocation(exe, 0); /* the settings exe's own icon
                                      * (assets/hunmin.ico, ApplicationIcon) */
    IPersistFile *pf = nullptr;
    if (SUCCEEDED(sl->QueryInterface(IID_PPV_ARGS(&pf))) && pf) {
        wchar_t programs[MAX_PATH] = L"";
        if (SUCCEEDED(hunmin_tsf_regseam()->programs_folder(programs, MAX_PATH)) && programs[0]) {
            wchar_t link[MAX_PATH + 48];
            _snwprintf(link, _countof(link), L"%s\\Hunmin IME \uC124\uC815.lnk", programs);
            if (SUCCEEDED(pf->Save(link, TRUE))) {
                hunmin_reg_logger.log(L"shortcut saved %s\n", link);
            } else {
                hunmin_reg_logger.log(L"shortcut save FAILED %s\n", link);
            }
        }
        pf->Release();
    }
    sl->Release();
}

extern "C" BOOL DllRegisterServer() {
    // logging (the ONE syntax; the head truncates, the chain appends)
    hunmin_reg_logger.reset();
    wchar_t guidStr[64];
    StringFromGUID2(CLSID_HunminTSF, guidStr, 64);
    hunmin_reg_logger.log(L"guid=%s\n", guidStr);

    wchar_t dllPath[MAX_PATH];
    HINSTANCE hinst = g_hunmin_hinst;
    if (!hinst)
        hinst = GetModuleHandleW(L"hunmin-tsf.dll");
    GetModuleFileNameW(hinst, dllPath, MAX_PATH);
    hunmin_reg_logger.log(L"module=%s\n", dllPath);

    if (!RegisterComServer(guidStr, dllPath))
        return FALSE;
    RegisterTipKey(guidStr);
    CoInitialize(nullptr);
    RegisterInputProcessorProfile(guidStr);
    RegisterTipCategories();
    CreateStartMenuShortcut(dllPath);
    CoUninitialize();
    return TRUE;
}

static void UnregisterInputProcessorProfile() {
    ITfInputProcessorProfiles *pProfile = nullptr;
    HRESULT hr = hunmin_tsf_regseam()->co_create(CLSID_TF_InputProcessorProfiles, IID_ITfInputProcessorProfiles, (LPVOID *)&pProfile);
    hunmin_reg_logger.log(L"pProfile CoCreate hr=0x%lX\n", (unsigned long)hr);
    if (FAILED(hr) || !pProfile)
        return;
    /* Reverse of the register order: the per-profile enablement entry the
     * ProfileMgr interface owns goes first (mirror of EnableLanguageProfile /
     * ProfileMgr), then Unregister() for Register(). */
    ITfInputProcessorProfileMgr *pProfileMgr = nullptr;
    HRESULT mgrHr = pProfile->QueryInterface(IID_ITfInputProcessorProfileMgr, (LPVOID *)&pProfileMgr);
    hunmin_reg_logger.log(L"pProfile QI(ITfInputProcessorProfileMgr) hr=0x%lX\n", (unsigned long)mgrHr);
    if (SUCCEEDED(mgrHr) && pProfileMgr) {
        HRESULT upHr = pProfileMgr->UnregisterProfile(CLSID_HunminTSF, 0x0412, HunminProfileGuid, 0);
        hunmin_reg_logger.log(L"pProfileMgr->UnregisterProfile hr=0x%lX\n", (unsigned long)upHr);
        pProfileMgr->Release();
    }
    HRESULT unregHr = pProfile->Unregister(CLSID_HunminTSF);
    hunmin_reg_logger.log(L"pProfile->Unregister hr=0x%lX\n", (unsigned long)unregHr);
    pProfile->Release();
}

static void UnregisterTipCategories() {
    ITfCategoryMgr *pCatMgr = nullptr;
    HRESULT hr = hunmin_tsf_regseam()->co_create(CLSID_TF_CategoryMgr, IID_ITfCategoryMgr, (LPVOID *)&pCatMgr);
    hunmin_reg_logger.log(L"pCatMgr CoCreate hr=0x%lX\n", (unsigned long)hr);
    if (FAILED(hr) || !pCatMgr)
        return;
    HRESULT c1 = pCatMgr->UnregisterCategory(CLSID_HunminTSF, GUID_TFCAT_TIP_KEYBOARD, CLSID_HunminTSF);
    HRESULT c2 = pCatMgr->UnregisterCategory(CLSID_HunminTSF, GUID_TFCAT_CATEGORY_OF_TIP, CLSID_HunminTSF);
    /* Issue #98: mirror of the display-attribute provider registration. */
    HRESULT c3 = pCatMgr->UnregisterCategory(CLSID_HunminTSF, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_HunminTSF);
    hunmin_reg_logger.log(L"UnregisterCategory keyboard=0x%lX category=0x%lX dispattr=0x%lX\n", (unsigned long)c1, (unsigned long)c2,
                          (unsigned long)c3);
    pCatMgr->Release();
}

extern "C" BOOL DllUnregisterServer() {
    /* Mirror of DllRegisterServer, applied in reverse order (issue #84).
     * First the profile and category state the CTF COM services hold, then
     * the HKLM TIP tree under which AddLanguageProfile created subkeys, then
     * the COM CLSID tree. Windows Settings' own per-user CTF store (HKCU
     * SortOrder / Assemblies) is deliberately NOT touched: registration never
     * writes it (measured, docs/WINDOWS-TSF-REGISTRATION.md), so unregistering
     * must not either. Logging follows the register flow: only
     * hunmin_reg_logger.log() writes, to hunmin-reg-debug.log. */
    wchar_t guidStr[64];
    StringFromGUID2(CLSID_HunminTSF, guidStr, 64);
    hunmin_reg_logger.log(L"--- unregister begin guid=%s\n", guidStr);

    CoInitialize(nullptr);
    UnregisterInputProcessorProfile();
    UnregisterTipCategories();
    CoUninitialize();

    /* HKLM TIP tree: deleted AFTER the COM calls, because the registering
     * side created LanguageProfile\0x00000412\{profile} subkeys under it
     * through AddLanguageProfile. */
    wchar_t tipPath[256];
    swprintf(tipPath, 256, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s", guidStr);
    LSTATUS tipSt = hunmin_tsf_regseam()->reg_delete_tree(HKEY_LOCAL_MACHINE, tipPath);
    hunmin_reg_logger.log(L"RegDeleteTreeW(HKLM %s) st=0x%lX\n", tipPath, (unsigned long)tipSt);

    /* COM CLSID tree -- the only deletion the previous implementation did. */
    wchar_t clsidPath[128];
    swprintf(clsidPath, 128, L"CLSID\\%s", guidStr);
    LSTATUS clsidSt = hunmin_tsf_regseam()->reg_delete_tree(HKEY_CLASSES_ROOT, clsidPath);
    hunmin_reg_logger.log(L"RegDeleteTreeW(HKCR %s) st=0x%lX\n", clsidPath, (unsigned long)clsidSt);
    return TRUE;
}

extern "C" const CLSID CLSID_HunminTSF_Export = {0xF2C70001, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}};

class CClassFactory : public IClassFactory {
public:
    CClassFactory() : ref_(1) {}
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory *>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG n = (ULONG)InterlockedDecrement(&ref_);
        if (n == 0)
            delete this;
        return n;
    }
    STDMETHOD(CreateInstance)(IUnknown *pOuter, REFIID riid, void **ppv) override {
        if (pOuter)
            return CLASS_E_NOAGGREGATION;
        CTextService *obj = new CTextService();
        HRESULT hr = obj->QueryInterface(riid, ppv);
        obj->Release();
        return hr;
    }
    STDMETHOD(LockServer)(BOOL lock) override {
        (void)lock;
        return S_OK;
    }

private:
    LONG ref_;
};

extern "C" HRESULT DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
    wchar_t str[64];
    StringFromGUID2(rclsid, str, 64);
    {
        hunmin_logger.log("DllGetClassObject rclsid=%ls\n", str);
    }
    if (!IsEqualCLSID(rclsid, CLSID_HunminTSF)) {
        {
            hunmin_logger.log("  not registered CLSID\n");
        }
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    CClassFactory *factory = new CClassFactory();
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    {
        hunmin_logger.log("  hr=0x%lX\n", (unsigned long)hr);
    }
    return hr;
}
