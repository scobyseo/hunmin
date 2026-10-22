/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The TSF-native settings plumbing (issue #71): ITfFunctionProvider handed
 * out by the text service's QI, and the ITfFnConfigure behind it whose Show
 * launches the ruled WinUI 3 surface and then re-applies the store to the
 * live adapter.
 */
#include "settings.h"

#include "log.h"
#include "settingslaunch.h"

#include <ctffunc.h>
#include <msctf.h>
#include <windows.h>

/* The provider's own type GUID (ITfFunctionProvider::GetType): fresh, not
 * the TIP CLSID {F2C70001-...-0001} or the profile {F2C70002-...-0002}. */
static const GUID kProviderTypeGuid = {0xF2C70010, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10}};

/* ---- TSF-native plumbing ---- */

class HunminTsfConfigFn : public ITfFnConfigure {
  public:
    explicit HunminTsfConfigFn(IUnknown *svc) : ref_(1), svc_(svc) {
        if (svc_)
            svc_->AddRef();
    }
    virtual ~HunminTsfConfigFn() {
        if (svc_)
            svc_->Release();
    }
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfFunction) ||
            IsEqualIID(riid, IID_ITfFnConfigure)) {
            *ppv = static_cast<ITfFnConfigure *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG r = (ULONG)InterlockedDecrement(&ref_);
        if (r == 0)
            delete this;
        return r;
    }
    STDMETHOD(GetDisplayName)(BSTR *pbstrName) override {
        if (!pbstrName)
            return E_INVALIDARG;
        *pbstrName = SysAllocString(L"Hunmin \uC124\uC815 /* settings */");
        return *pbstrName ? S_OK : E_OUTOFMEMORY;
    }
    /* The contract the classic Text Services / property surfaces rely on:
     * do not return until the dialog closes. Maintainer ruling 2026-10-17:
     * Show surfaces the WinUI 3 exe ONLY — the interim Win32 dialog is not
     * reachable here (it lives on via the rundll32 export until removed). */
    STDMETHOD(Show)(HWND hwndParent, LANGID langid, REFGUID rguidProfile) override {
        (void)langid;
        (void)rguidProfile;
        hunmin_logger.log("ITfFnConfigure::Show pid=%lu\n", (unsigned long)GetCurrentProcessId());
        bool launched = hunmin_tsf_settings_exe_run(hwndParent, nullptr, 0);
        /* Live re-apply, issue #71 requirement: after the exe's commit (its
         * exit), the running adapter re-reads the store WITHOUT a process
         * restart. A miss on the exe leaves the store untouched, and the
         * no-exe fact is logged rather than silently falling back to the
         * interim dialog. */
        if (launched && svc_)
            hunmin_tsf_apply_config_store(svc_);
        return S_OK;
    }

  private:
    LONG ref_;
    IUnknown *svc_;
};

class HunminTsfFunctionProvider : public ITfFunctionProvider {
  public:
    explicit HunminTsfFunctionProvider(IUnknown *svc) : ref_(1), svc_(svc) {
        if (svc_)
            svc_->AddRef();
    }
    virtual ~HunminTsfFunctionProvider() {
        if (svc_)
            svc_->Release();
    }
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfFunctionProvider)) {
            *ppv = static_cast<ITfFunctionProvider *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG r = (ULONG)InterlockedDecrement(&ref_);
        if (r == 0)
            delete this;
        return r;
    }
    STDMETHOD(GetType)(GUID *pguid) override {
        if (!pguid)
            return E_INVALIDARG;
        *pguid = kProviderTypeGuid;
        return S_OK;
    }
    STDMETHOD(GetDescription)(BSTR *pbstrDesc) override {
        if (!pbstrDesc)
            return E_INVALIDARG;
        *pbstrDesc = SysAllocString(L"Hunmin settings provider");
        return *pbstrDesc ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHOD(GetFunction)(REFGUID rguid, REFIID riid, IUnknown **out) override {
        (void)rguid;
        if (!out)
            return E_INVALIDARG;
        *out = nullptr;
        if (!IsEqualIID(riid, IID_ITfFnConfigure))
            return E_NOINTERFACE;
        HunminTsfConfigFn *fn = new HunminTsfConfigFn(svc_);
        HRESULT hr = fn->QueryInterface(riid, (void **)out);
        fn->Release();
        return hr;
    }

  private:
    LONG ref_;
    IUnknown *svc_;
};

IUnknown *hunmin_tsf_settings_new_provider(void *svc) {
    return new HunminTsfFunctionProvider((IUnknown *)svc);
}
