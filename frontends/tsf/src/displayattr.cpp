/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * displayattr.cpp - the display-attribute provider and the styling
 * channel (docs/REFACTOR-TSF-COMPONENTS.md, S7). Relocated from tsf.cpp
 * unchanged: same attribute shape, same one-time GUID registration, same
 * logged failure when the category manager is unavailable.
 */
#include "displayattr.h"

#include "ids.h"
#include "log.h"

#include <commctrl.h> /* CLR_NONE */
#include <objbase.h>
#include <oleauto.h>

/* ITfDisplayAttributeInfo for HunminDisplayAttrGuid: one attribute, the
 * underlined-undetermined-text shape every Korean IME uses. */
class HunminDisplayAttrInfo : public ITfDisplayAttributeInfo {
  public:
    HunminDisplayAttrInfo() : ref_(1) {}
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfDisplayAttributeInfo)) {
            *ppv = static_cast<ITfDisplayAttributeInfo *>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG r = (ULONG)InterlockedDecrement(&ref_);
        if (r == 0)
            delete this;
        return r;
    }
    STDMETHOD(GetGUID)(GUID *pguid) override {
        if (!pguid)
            return E_INVALIDARG;
        *pguid = HunminDisplayAttrGuid;
        return S_OK;
    }
    STDMETHOD(GetDescription)(BSTR *pbstrDesc) override {
        if (!pbstrDesc)
            return E_INVALIDARG;
        *pbstrDesc = SysAllocString(L"Hunmin preedit");
        return *pbstrDesc ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHOD(GetAttributeInfo)(TF_DISPLAYATTRIBUTE *pda) override {
        if (!pda)
            return E_INVALIDARG;
        ZeroMemory(pda, sizeof *pda);
        /* Measured against Notepad rendering the MS IME beside ours: the app
         * honors the provider's colors, and the all-TF_CT_NONE shape (a
         * ZeroMemory init) made it paint its light-blue composition highlight
         * over the preedit. The ruled shape is the MS IME's: transparent
         * background, underline only, theme colors for text and underline.
         * CLR_NONE as an explicit COLORREF is the no-background encoding;
         * TF_CT_SYSCOLOR picks the theme's window text. */
        pda->crText.type = TF_CT_SYSCOLOR;
        pda->crText.nIndex = COLOR_WINDOWTEXT;
        pda->crBk.type = TF_CT_COLORREF;
        pda->crBk.cr = (COLORREF)CLR_NONE;
        pda->crLine.type = TF_CT_SYSCOLOR;
        pda->crLine.nIndex = COLOR_WINDOWTEXT;
        pda->lsStyle = TF_LS_SOLID;
        pda->fBoldLine = FALSE;
        pda->bAttr = TF_ATTR_INPUT;
        return S_OK;
    }
    STDMETHOD(SetAttributeInfo)(const TF_DISPLAYATTRIBUTE *pda) override {
        (void)pda;
        return E_NOTIMPL; /* the attribute is not user-editable here */
    }
    STDMETHOD(Reset)() override { return S_OK; }

  private:
    LONG ref_;
    virtual ~HunminDisplayAttrInfo() = default;
};

class HunminEnumDisplayAttrInfo : public IEnumTfDisplayAttributeInfo {
  public:
    HunminEnumDisplayAttrInfo() : ref_(1), pos_(0) {}
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IEnumTfDisplayAttributeInfo)) {
            *ppv = static_cast<IEnumTfDisplayAttributeInfo *>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    STDMETHOD_(ULONG, AddRef)() override { return (ULONG)InterlockedIncrement(&ref_); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG r = (ULONG)InterlockedDecrement(&ref_);
        if (r == 0)
            delete this;
        return r;
    }
    STDMETHOD(Clone)(IEnumTfDisplayAttributeInfo **ppEnum) override {
        if (!ppEnum)
            return E_INVALIDARG;
        HunminEnumDisplayAttrInfo *e = new HunminEnumDisplayAttrInfo();
        e->pos_ = pos_;
        *ppEnum = e;
        return S_OK;
    }
    STDMETHOD(Next)(ULONG ulCount, ITfDisplayAttributeInfo **rgInfo, ULONG *pcFetched) override {
        if (!rgInfo)
            return E_INVALIDARG;
        ULONG n = 0;
        while (n < ulCount && pos_ < 1) {
            rgInfo[n++] = new HunminDisplayAttrInfo();
            ++pos_;
        }
        if (pcFetched)
            *pcFetched = n;
        return n == ulCount ? S_OK : S_FALSE;
    }
    STDMETHOD(Reset)() override {
        pos_ = 0;
        return S_OK;
    }
    STDMETHOD(Skip)(ULONG ulCount) override {
        pos_ = (pos_ + ulCount > 1) ? 1 : pos_ + ulCount;
        return S_OK;
    }

  private:
    LONG ref_;
    ULONG pos_;
    virtual ~HunminEnumDisplayAttrInfo() = default;
};

DisplayAttr::DisplayAttr() : atom_(0) {}

DisplayAttr::~DisplayAttr() = default;

/* Registers the composition-attribute GUID once and applies (or clears)
 * it over the composition range (msctf: providing display attributes -
 * the property value is the TfGuidAtom the app's attribute manager
 * resolves through our provider). Without a category manager (COM not
 * initialised in the process) the composition text simply renders
 * unstyled; logged, so the cause stays findable. */
void DisplayAttr::Apply(ITfContext *ctx, TfEditCookie ec, ITfRange *range, bool clear) {
    if (!ctx || !range)
        return;
    if (atom_ == 0) {
        ITfCategoryMgr *cm = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_ITfCategoryMgr, (void **)&cm)) &&
            cm) {
            HRESULT hrG = cm->RegisterGUID(HunminDisplayAttrGuid, &atom_);
            cm->Release();
            if (FAILED(hrG))
                hunmin_logger.log("display attr RegisterGUID hr=0x%lX\n", (unsigned long)hrG);
        }
    }
    if (atom_ == 0)
        return; /* no styling channel: plain composition text */
    ITfProperty *prop = nullptr;
    if (SUCCEEDED(ctx->GetProperty(GUID_PROP_ATTRIBUTE, &prop)) && prop) {
        if (clear) {
            prop->Clear(ec, range);
        } else {
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_I4;
            v.lVal = (LONG)atom_;
            prop->SetValue(ec, range, &v);
            VariantClear(&v);
        }
        prop->Release();
    }
}

IEnumTfDisplayAttributeInfo *HunminDisplayAttrEnumNew() {
    return new HunminEnumDisplayAttrInfo();
}

ITfDisplayAttributeInfo *HunminDisplayAttrNewForGUID(REFGUID guid) {
    if (!IsEqualGUID(guid, HunminDisplayAttrGuid))
        return nullptr;
    return new HunminDisplayAttrInfo();
}
