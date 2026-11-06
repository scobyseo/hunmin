/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Registration-flow doubles for tier F1 (issue #107): the in-memory stand-ins
 * for everything DllRegisterServer / DllUnregisterServer touch outside the
 * process, installed through the seam in src/registrationseam.h.
 *
 * What these record is the ADAPTER'S call order and parameters -- CLSIDs,
 * LANGID 0x0412, the profile GUID, the category GUIDs, the registry paths and
 * values. They do NOT represent what the real msctf
 * servers write to the registry: which COM call removes which tree stays the
 * elevated-cycle measurement in tests/manual-checklists/tsf-unregister-cleanup.md,
 * and a green test here neither closes it nor downgrades it.
 *
 * One double set per process (the seam is a context-free function table, so
 * the installed set is reached through a file-scope pointer). gtest runs its
 * cases sequentially, which is what makes that safe; install scoped-object
 * style (the constructor installs, the destructor restores the platform table).
 */
#pragma once
#include <cstddef>
#include <msctf.h>
#include <string>
#include <vector>
#include <windows.h>

#include "../src/registrationseam.h"
#include "mock-tsf.h"

/* COM stubs: most parameters carry interface signatures, not values; the file-scoped mute quiets MSVC C4100 for them only. */
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4100)
#endif

namespace tsfmock {

/* The ordered, cross-object log every double below appends to. Method names
 * are recorded as they are called, so ONE sequence expresses the whole
 * register flow (registry writes and COM calls interleaved as the code does
 * them). */
struct RegTrace {
    std::vector<std::wstring> events; // method names, in call order
    std::vector<std::wstring> created; // "<root>|<path>" per create/open
    std::vector<std::wstring> writes; // "<path>|<name>|<value>"
    std::vector<std::wstring> deletes; // "<root>|<path>"

    void event(const wchar_t *name) { events.push_back(name); }
    int count(const wchar_t *name) const {
        int n = 0;
        for (const std::wstring &e : events)
            if (e == name)
                ++n;
        return n;
    }
    /* Position of the first occurrence, or -1: the order assertions read this
     * ("UnregisterProfile before Unregister", "the TIP tree deleted after the
     * COM calls"). */
    std::ptrdiff_t indexOf(const wchar_t *name) const {
        for (size_t i = 0; i < events.size(); ++i)
            if (events[i] == name)
                return (std::ptrdiff_t)i;
        return -1;
    }
    bool has(const wchar_t *name) const { return indexOf(name) >= 0; }
};

/* The registry double: keys are fake HKEYs (index + 1 cast), so the adapter's
 * real handle plumbing is exercised without a single registry call. */
class MockRegistry {
public:
    explicit MockRegistry(RegTrace &trace) : trace_(trace) {}

    LONG CreateKey(HKEY root, const wchar_t *path, HKEY *out);
    LONG OpenKey(HKEY root, const wchar_t *path, HKEY *out);
    LONG SetValue(HKEY key, const wchar_t *name, DWORD type, const void *data, DWORD bytes);
    void CloseKey(HKEY key) { (void)key; }
    LONG DeleteTree(HKEY root, const wchar_t *path);

    static std::wstring RootName(HKEY root);

private:
    std::wstring PathFor(HKEY key) const;
    RegTrace &trace_;
    std::vector<std::wstring> keys_; // handle (index+1) -> "ROOT|path"
};

/* ITfInputProcessorProfileMgr: the modern activation interface. GetActiveProfile
 * answers the configured profile, so a test can drive both the "modern host
 * answered" and the "legacy fallback" branches of ActivateInputProcessorProfile. */
class MockInputProcessorProfileMgr : public ITfInputProcessorProfileMgr, public RefCount {
public:
    explicit MockInputProcessorProfileMgr(RegTrace &trace) : trace_(trace) {}

    /* What GetActiveProfile reports; empty CLSID = "nothing active" (S_FALSE). */
    CLSID activeClsid = GUID{};
    GUID activeProfile = GUID{};
    LANGID activeLangid = 0;

    /* Recorded parameters. */
    DWORD activateFlags = 0;
    CLSID activateClsid = GUID{};
    GUID activateProfile = GUID{};
    LANGID activateLangid = 0;
    CLSID unregisterProfileClsid = GUID{};
    GUID unregisterProfileGuid = GUID{};
    DWORD unregisterProfileFlags = 0;

    STDMETHOD(ActivateProfile)(DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID guidProfile, HKL hkl, DWORD dwFlags) override;
    STDMETHOD(UnregisterProfile)(REFCLSID rclsid, LANGID langid, REFGUID guidProfile, DWORD dwFlags) override;
    STDMETHOD(GetActiveProfile)(REFGUID catid, TF_INPUTPROCESSORPROFILE *pProfile) override;

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfInputProcessorProfileMgr)) {
            *ppv = static_cast<ITfInputProcessorProfileMgr *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

    TSF_MOCK_STUB(DeactivateProfile, DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID guidProfile, HKL hkl, DWORD dwFlags)
    TSF_MOCK_STUB(GetProfile, DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID guidProfile, HKL hkl, TF_INPUTPROCESSORPROFILE *pProfile)
    TSF_MOCK_STUB(EnumProfiles, LANGID langid, IEnumTfInputProcessorProfiles **ppEnum)
    TSF_MOCK_STUB(ReleaseInputProcessor, REFCLSID rclsid, DWORD dwFlags)
    TSF_MOCK_STUB(RegisterProfile, REFCLSID rclsid, LANGID langid, REFGUID guidProfile, const WCHAR *pchDesc, ULONG cchDesc, const WCHAR *pchIconFile,
                  ULONG cchFile, ULONG uIconIndex, HKL hklsubstitute, DWORD dwPreferredLayout, BOOL bEnabledByDefault, DWORD dwFlags)

private:
    friend struct RegistrationDoubles;
    ~MockInputProcessorProfileMgr() = default;
    RegTrace &trace_;
};

/* ITfInputProcessorProfiles: registration's legacy identity, and the object the
 * adapter QIs for the ProfileMgr above. */
class MockInputProcessorProfiles : public ITfInputProcessorProfiles, public RefCount {
public:
    MockInputProcessorProfiles(RegTrace &trace, MockInputProcessorProfileMgr *mgr) : trace_(trace), mgr_(mgr) {}

    /* Recorded parameters. */
    CLSID registeredClsid = GUID{};
    CLSID unregisteredClsid = GUID{};
    LANGID addedLangid = 0;
    GUID addedProfile = GUID{};
    std::wstring addedDescription;
    std::wstring addedIconFile;
    ULONG addedIconFileChars = 0;
    ULONG addedIconIndex = 0;
    LANGID enabledLangid = 0;
    GUID enabledProfile = GUID{};
    BOOL enabled = FALSE;
    CLSID activatedClsid = GUID{};
    LANGID activatedLangid = 0;
    GUID activatedProfile = GUID{};
    bool activateLanguageProfileCalled = false;

    STDMETHOD(Register)(REFCLSID rclsid) override;
    STDMETHOD(Unregister)(REFCLSID rclsid) override;
    STDMETHOD(AddLanguageProfile)(REFCLSID rclsid, LANGID langid, REFGUID guidProfile, const WCHAR *pchDesc, ULONG cchDesc, const WCHAR *pchIconFile,
                                  ULONG cchFile, ULONG uIconIndex) override;
    STDMETHOD(EnableLanguageProfile)(REFCLSID rclsid, LANGID langid, REFGUID guidProfile, BOOL fEnable) override;
    STDMETHOD(ActivateLanguageProfile)(REFCLSID rclsid, LANGID langid, REFGUID guidProfiles) override;

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override;

    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

    TSF_MOCK_STUB(RemoveLanguageProfile, REFCLSID rclsid, LANGID langid, REFGUID guidProfile)
    TSF_MOCK_STUB(EnumInputProcessorInfo, IEnumGUID **ppEnum)
    TSF_MOCK_STUB(GetDefaultLanguageProfile, LANGID langid, REFGUID catid, CLSID *pclsid, GUID *pguidProfile)
    TSF_MOCK_STUB(SetDefaultLanguageProfile, LANGID langid, REFCLSID rclsid, REFGUID guidProfiles)
    TSF_MOCK_STUB(GetActiveLanguageProfile, REFCLSID rclsid, LANGID *plangid, GUID *pguidProfile)
    TSF_MOCK_STUB(GetLanguageProfileDescription, REFCLSID rclsid, LANGID langid, REFGUID guidProfile, BSTR *pbstrProfile)
    TSF_MOCK_STUB(GetCurrentLanguage, LANGID *plangid)
    TSF_MOCK_STUB(ChangeCurrentLanguage, LANGID langid)
    TSF_MOCK_STUB(GetLanguageList, LANGID **ppLangId, ULONG *pulCount)
    TSF_MOCK_STUB(EnumLanguageProfiles, LANGID langid, IEnumTfLanguageProfiles **ppEnum)
    TSF_MOCK_STUB(IsEnabledLanguageProfile, REFCLSID rclsid, LANGID langid, REFGUID guidProfile, BOOL *pfEnable)
    TSF_MOCK_STUB(EnableLanguageProfileByDefault, REFCLSID rclsid, LANGID langid, REFGUID guidProfile, BOOL fEnable)
    TSF_MOCK_STUB(SubstituteKeyboardLayout, REFCLSID rclsid, LANGID langid, REFGUID guidProfile, HKL hKL)

private:
    friend struct RegistrationDoubles;
    ~MockInputProcessorProfiles() = default;
    RegTrace &trace_;
    MockInputProcessorProfileMgr *mgr_;
};

/* ITfCategoryMgr: records both category lists in call order, so the mirror
 * assertion compares them element by element. */
class MockCategoryMgr : public ITfCategoryMgr, public RefCount {
public:
    explicit MockCategoryMgr(RegTrace &trace) : trace_(trace) {}

    std::vector<GUID> registered;
    std::vector<GUID> unregistered;

    STDMETHOD(RegisterCategory)(REFCLSID rclsid, REFGUID rcatid, REFGUID rguid) override;
    STDMETHOD(UnregisterCategory)(REFCLSID rclsid, REFGUID rcatid, REFGUID rguid) override;

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfCategoryMgr)) {
            *ppv = static_cast<ITfCategoryMgr *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return RefCount::AddRef(); }
    STDMETHOD_(ULONG, Release)() override { return RefCount::Release(); }

    TSF_MOCK_STUB(EnumCategoriesInItem, REFGUID rguid, IEnumGUID **ppEnum)
    TSF_MOCK_STUB(EnumItemsInCategory, REFGUID rcatid, IEnumGUID **ppEnum)
    TSF_MOCK_STUB(FindClosestCategory, REFGUID rguid, GUID *pcatid, const GUID **ppcatidList, ULONG ulCount)
    TSF_MOCK_STUB(RegisterGUIDDescription, REFCLSID rclsid, REFGUID rguid, const WCHAR *pchDesc, ULONG cch)
    TSF_MOCK_STUB(UnregisterGUIDDescription, REFCLSID rclsid, REFGUID rguid)
    TSF_MOCK_STUB(GetGUIDDescription, REFGUID rguid, BSTR *pbstrDesc)
    TSF_MOCK_STUB(RegisterGUIDDWORD, REFCLSID rclsid, REFGUID rguid, DWORD dw)
    TSF_MOCK_STUB(UnregisterGUIDDWORD, REFCLSID rclsid, REFGUID rguid)
    TSF_MOCK_STUB(GetGUIDDWORD, REFGUID rguid, DWORD *pdw)
    TSF_MOCK_STUB(RegisterGUID, REFGUID rguid, TfGuidAtom *pguidatom)
    TSF_MOCK_STUB(GetGUID, TfGuidAtom guidatom, GUID *pguid)
    TSF_MOCK_STUB(IsEqualTfGuidAtom, TfGuidAtom guidatom, REFGUID rguid, BOOL *pfEqual)

private:
    friend struct RegistrationDoubles;
    ~MockCategoryMgr() = default;
    RegTrace &trace_;
};

/* The whole double set + the seam table built from it. The constructor wires
 * the table; Install()/the destructor swap the process's seam. */
struct RegistrationDoubles {
    RegTrace trace;
    MockRegistry registry;
    MockInputProcessorProfileMgr profileMgr;
    MockInputProcessorProfiles profiles;
    MockCategoryMgr categoryMgr;
    HunminRegSeam seam;

    RegistrationDoubles();
    ~RegistrationDoubles();
    void Install();
    /* The doubles as objects, for the assertions (the seam only carries
     * function pointers). */
    RegistrationDoubles *operator->() { return this; }
};

} // namespace tsfmock
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
