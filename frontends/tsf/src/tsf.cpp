#include "hunmin.h"
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <msctf.h>
#include <richedit.h>
#include <shlguid.h>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <shobjidl_core.h>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#pragma comment(lib, "Shlwapi.lib")

/* msctf.h pulls ctfutb.h (the langbar item interfaces); a guard for the
 * connection errors the item's ITfSource answers with — oaidl.h carries
 * these values on the SDK, but not every header chain includes it. */
#ifndef CONNECT_E_NOCONNECTION
#define CONNECT_E_NOCONNECTION ((HRESULT)0x80040200L)
#define CONNECT_E_ADVISELIMIT ((HRESULT)0x80040201L)
#define CONNECT_E_CANNOTCONNECT ((HRESULT)0x80040202L)
#endif

/* Debug logs land next to the DLL (build-msvc/Release inside the checkout),
 * not at a drive root: the registered InprocServer32 path is absolute, so the
 * DLL's own directory is the one clone-independent location every host
 * process can resolve, and it stays inside the workspace. Set
 * HUNMIN_TSF_LOG_DIR to override when that directory is read-only. */
bool hunmin_tsf_log_path(const char *fname, char *out, size_t cap) {
    wchar_t dirW[MAX_PATH];
    wchar_t envW[MAX_PATH];
    DWORD envAttr;
    if (GetEnvironmentVariableW(L"HUNMIN_TSF_LOG_DIR", envW, MAX_PATH) > 0 &&
        (envAttr = GetFileAttributesW(envW)) != INVALID_FILE_ATTRIBUTES &&
        (envAttr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
        lstrcpynW(dirW, envW, MAX_PATH);
    } else {
        HMODULE hMod = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (LPCWSTR)(const void *)&hunmin_tsf_log_path, &hMod) ||
            !hMod)
            return false;
        if (GetModuleFileNameW(hMod, dirW, MAX_PATH) == 0)
            return false;
        if (!PathRemoveFileSpecW(dirW))
            return false;
    }
    char dirA[MAX_PATH * 3];
    if (WideCharToMultiByte(CP_ACP, 0, dirW, -1, dirA, sizeof(dirA), nullptr, nullptr) == 0)
        return false;
    int n = snprintf(out, cap, "%s\\%s", dirA, fname);
    return n > 0 && (size_t)n < cap;
}
/* Maintainer instruction (2026-10-17): debug output is an option, not an
 * ambient side effect. The gate lives in tsf_log_open — the one function
 * every log site opens through — so each site's `if (dbg)` stays intact and
 * reads false while the switch is off: no file is created, nothing is
 * written. HUNMIN_TSF_DEBUG_LOG set to 1 / on / yes / true enables; the
 * variable is re-read per open, so a running session can be flipped without
 * a restart. Registration logging (DllRegisterServer) goes through the same
 * gate and stays silent when off. */
static bool tsf_log_enabled(void) {
    char v[16];
    DWORD n = GetEnvironmentVariableA("HUNMIN_TSF_DEBUG_LOG", v, (DWORD)sizeof(v));
    if (n == 0 || n >= sizeof(v))
        return false;
    for (DWORD i = 0; i < n && v[i]; ++i)
        v[i] = (char)tolower((unsigned char)v[i]);
    return strcmp(v, "1") == 0 || strcmp(v, "on") == 0 || strcmp(v, "yes") == 0 ||
           strcmp(v, "true") == 0;
}
static FILE *tsf_log_open(const char *fname, const char *mode = "a") {
    if (!tsf_log_enabled())
        return nullptr;
    char path[MAX_PATH];
    if (!hunmin_tsf_log_path(fname, path, sizeof(path)))
        return nullptr;
    return fopen(path, mode);
}

static void HMLOG(const char *fmt, ...) {
    FILE *f = tsf_log_open("hunmin-tsf-debug.log");
    if (!f)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fflush(f);
    fclose(f);
}
/* Issue #66: ONE data source wins — the first present in priority order:
 * (1) HUNMIN_DATA_DIR (explicit override), (2) <module>\data (the shipped
 * shape; the build stages libhunmin/data beside the DLL), (3) the repo
 * checkout climb (dev convenience, reached only when neither holds). The
 * earlier additive chain held the same tree twice once staging existed:
 * hunmin_ctx_layout_count() reported 30 for 15 staged files, because
 * hunmin_ctx_new's search path is additive by design — duplicate ids
 * across dirs are not deduped, so overlapping dirs are the caller's to
 * avoid. Measured 2026-10-17. Non-static (issue #71): the settings dialog
 * (settings.cpp) resolves data through this same function, so the dialog's
 * layout lists can never disagree with the adapter's about where data
 * came from. */
hunmin_ctx *hunmin_ctx_from_module() {
    wchar_t envW[MAX_PATH];
    wchar_t modDir[MAX_PATH];
    char dirs[2][MAX_PATH * 3];
    int argc = 0;
    FILE *dbg0 = tsf_log_open("hunmin-tsf-debug.log");

    HMODULE hMod = GetModuleHandleW(L"hunmin-tsf.dll");
    if (!hMod)
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)(const void *)hunmin_ctx_from_module,
                           &hMod);
    if (hMod)
        GetModuleFileNameW(hMod, modDir, MAX_PATH);

    if (GetEnvironmentVariableW(L"HUNMIN_DATA_DIR", envW, MAX_PATH) > 0 &&
        (GetFileAttributesW(envW) & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
        WideCharToMultiByte(CP_UTF8, 0, envW, -1, dirs[argc], sizeof(dirs[argc]), nullptr, nullptr);
        ++argc;
        if (dbg0) {
            fprintf(dbg0, "env data dir: %ls\n", envW);
            fflush(dbg0);
        }
    } else if (hMod) {
        PathRemoveFileSpecW(modDir);
        wchar_t candMod[MAX_PATH];
        PathCombineW(candMod, modDir, L"data");
        DWORD attrMod = GetFileAttributesW(candMod);
        if (dbg0)
            fprintf(dbg0, "candMod=%ls dir=%d\n", candMod,
                    (attrMod != INVALID_FILE_ATTRIBUTES) &&
                        ((attrMod & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY));
        if (attrMod != INVALID_FILE_ATTRIBUTES &&
            (attrMod & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
            /* Shipped shape: data beside the module. Do not also climb to the
             * repo — one tree in the search path, or every layout loads twice. */
            WideCharToMultiByte(CP_UTF8, 0, candMod, -1, dirs[argc], sizeof(dirs[argc]), nullptr, nullptr);
            ++argc;
        } else {
            /* Dev checkout: climb to the repo's libhunmin\data. */
            wchar_t repo[MAX_PATH];
            wcsncpy_s(repo, modDir, MAX_PATH);
            for (int i = 0; i < 4; ++i)
                PathRemoveFileSpecW(repo);
            wchar_t cand[2 * MAX_PATH];
            PathCombineW(cand, repo, L"libhunmin\\data");
            DWORD attrRepo = GetFileAttributesW(cand);
            if (dbg0)
                fprintf(dbg0, "climb=%ls dir=%d\n", cand,
                        (attrRepo != INVALID_FILE_ATTRIBUTES) &&
                            ((attrRepo & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY));
            if (attrRepo != INVALID_FILE_ATTRIBUTES &&
                (attrRepo & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
                WideCharToMultiByte(CP_UTF8, 0, cand, -1, dirs[argc], sizeof(dirs[argc]), nullptr, nullptr);
                ++argc;
            }
        }
    }
    if (dbg0) {
        fprintf(dbg0, "argc=%d hMod=%p\n", argc, (void *)hMod);
        fclose(dbg0);
    }

    if (argc == 0)
        return nullptr;
    const char *argv2[2] = {dirs[0], dirs[1]};
    return hunmin_ctx_new(argv2, argc);
}
/* Configured layout ids resolved against the loaded data (issue #66). The
 * fcitx5 defaults live in config.cpp; the adapter re-derives them here so a
 * ctx created from a partial data dir still gets a working layout. */
static const char *kHangulDefaultId = "sin3-p2";
static const char *kLatinDefaultId = "us-qwerty";

/* Accept a stored layout id only when the engine can instantiate it AND its
 * mode matches the slot (issue #71: the dialog lists layouts split by
 * hunmin_ctx_layout_mode, so a stale registry value naming a latin layout in
 * the hangul slot must degrade to the default, never to a silent mispair). */
static void resolveLayoutSlot(const hunmin_ctx *ctx, const std::string &stored,
                              const char *def, bool wantHangul, std::string *outId,
                              FILE *dbg) {
    const char *candidate = stored.c_str();
    if (candidate[0] == '\0')
        candidate = def;
    if (ctx) {
        for (int i = 0; i < hunmin_ctx_layout_count(ctx); ++i) {
            const char *id = hunmin_ctx_layout_id(ctx, i);
            if (!id || strcmp(id, candidate) != 0)
                continue;
            const char *mode = hunmin_ctx_layout_mode(ctx, i); /* "hangul"|"latin" */
            const bool modeOk =
                wantHangul ? (mode && strcmp(mode, "hangul") == 0)
                           : (mode && strcmp(mode, "latin") == 0);
            if (modeOk && hunmin_ctx_layout_supported(ctx, i)) {
                *outId = id;
                return;
            }
            if (dbg) {
                fprintf(dbg, "  config %s slot: id=%s mode=%s supported=%d -> rejected\n",
                        wantHangul ? "hangul" : "latin", id, mode ? mode : "?",
                        (int)hunmin_ctx_layout_supported(ctx, i));
            }
            break; /* found the id, wrong slot/build: fall to default */
        }
    }
    if (ctx)
        *outId = wantHangul ? kHangulDefaultId : kLatinDefaultId;
    if (dbg && *outId != candidate)
        fprintf(dbg, "  config %s slot: %s -> default %s\n",
                wantHangul ? "hangul" : "latin", candidate, outId->c_str());
}
/* The toggle-key matcher shares this TU (see frontends/tsf/CMakeLists.txt):
 * the harness's GetKeyState seam (#define GetKeyState before including this
 * file) must reach the matcher, not just the adapter. */
#include "keybinding.cpp"
#include "config.h"
#include "settings.h" /* ITfFunctionProvider branch + the apply bridge */
#include "candwindow.h"
/* UTF-8 <-> UTF-16 for the candidate path (issue #67): the suggest API is
 * UTF-8, the popup and the buffered word are wide. Replacements for the two
 * MultiByteToWideChar scatter patterns the adapter already carries — the
 * candidate flow needs them at three call sites and half-drawn double
 * conversions have burned this tree before (the embedded-NUL word buffer). */
static std::wstring HunminUtf8ToWide(const char *utf8) {
    if (!utf8 || !utf8[0])
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (n <= 1)
        return {};
    std::wstring w((size_t)n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], n);
    return w;
}
static std::string HunminWideToUtf8(const std::wstring &w) {
    if (w.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0,
                                nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr,
                        nullptr);
    return s;
}
/* The vk set the candidate window consumes while it is open: choose keys
 * (digits), motion (arrows, pages), Return/Esc. The OPEN/CLOSE toggle is the
 * configured combo, checked separately before this set; F9 is not here any
 * more by default (issue #67: the Hanja key is the default and F9 unbound).
 * Expressed once so OnTestKeyDown and the OnKeyDown window branch cannot
 * drift — the same shared-predicate rule the toggle binding uses. */
static bool hunmin_tsf_candkey_eats_test(unsigned long vk) {
    return vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_PRIOR ||
           vk == VK_NEXT || (vk >= VK_LEFT && vk <= VK_DOWN) ||
           (vk >= '0' && vk <= '9');
}

// Skeleton edit session for TSF text insertion. Real insertion lives
// inlined in OnKeyDown; this type documents the shape.
class HEditSession : public ITfEditSession {
  public:
    HEditSession() : ref_(1) {}
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppv = this;
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
    STDMETHOD(DoEditSession)(TfEditCookie ec) override {
        (void)ec;
        return S_OK;
    }

  private:
    LONG ref_;
};

/* Launch the settings surface: the WinUI 3 exe (the ruled surface, assets
 * icon) when it is deployed beside this DLL, the rundll32 entry to
 * HunminSettingsShow otherwise. A dedicated process: the TIP thread must
 * not pump a nested modal loop while TSF holds edit cookies. */
HRESULT hunmin_tsf_launch_settings(void *anchor) {
    HMODULE self = nullptr;
    wchar_t dll[MAX_PATH] = {0};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)anchor, &self) ||
        !GetModuleFileNameW(self, dll, MAX_PATH))
        return E_FAIL;
    wchar_t dllDir[MAX_PATH];
    lstrcpynW(dllDir, dll, MAX_PATH);
    PathRemoveFileSpecW(dllDir);
    wchar_t exe[MAX_PATH];
    exe[0] = 0;
    /* The settings exe's known layouts, first hit wins:
     * 1. <dllDir>\settings-ui\publish\ — the dev-tree publish shape
     *    (frontends/tsf/settings-ui/build-settings-ui.bat);
     * 2. <dllDir>\..\..\settings-ui\publish\ — the dev-checkout climb (DLL
     *    in frontends/tsf/build-msvc/Release);
     * 3. <dllDir>\settings-ui\ — the INSTALLED shape: the .iss stages the
     *    publish CONTENT directly under {app}\settings-ui (measured in the
     *    installer sandbox round: only the Start-menu shortcut pointed
     *    there, the launcher resolved neither publish shape and fell back
     *    to the rundll32 interim dialog). */
    static const wchar_t *const kSettingsExeCandidates[] = {
        L"%s\\settings-ui\\publish\\hunmin-tsf-settingsui.exe",
        L"%s\\..\\..\\settings-ui\\publish\\hunmin-tsf-settingsui.exe",
        L"%s\\settings-ui\\hunmin-tsf-settingsui.exe",
    };
    for (const wchar_t *fmt : kSettingsExeCandidates) {
        wchar_t cand[2 * MAX_PATH];
        wchar_t full[MAX_PATH];
        _snwprintf(cand, _countof(cand), fmt, dllDir);
        if (GetFullPathNameW(cand, MAX_PATH, full, nullptr) && PathFileExistsW(full)) {
            lstrcpynW(exe, full, MAX_PATH);
            break;
        }
    }
    wchar_t cmd[MAX_PATH + 64];
    SHELLEXECUTEINFOW sei = {sizeof sei};
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.nShow = SW_SHOWNORMAL;
    if (exe[0]) {
        sei.lpFile = exe;
        sei.lpParameters = nullptr;
    } else {
        _snwprintf(cmd, _countof(cmd), L"\"%s,HunminSettingsShow\"", dll);
        sei.lpFile = L"rundll32.exe";
        sei.lpParameters = cmd;
    }
    BOOL ok = ShellExecuteExW(&sei);
    FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
    if (dbg) {
        fprintf(dbg, "launch settings exe=%ls hr=%ld\n", exe, (long)GetLastError());
        fflush(dbg);
        fclose(dbg);
    }
    return ok ? S_OK : E_FAIL;
}

static const CLSID CLSID_HunminTSF = {0xF2C70001, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}};
/* GUID_LBI_INPUTMODE {2C77A81E-41CC-4178-A3A7-5F8A987568E6} — Windows 8+ and
 * Win11: the item the "input indicator" (the taskbar per-TIP badge) hosts in
 * the tray MUST carry this guidItem; any other item GUID is ignored by the
 * shell's hosted-item path (measured sibling jamotong TIP, hosted live).
 * The MSVC SDK declares GUID_LBI_INPUTMODE as an extern (ctffunc.h), so the
 * value would come from a link library; it is a private constant here so no
 * library set change is needed. The item's own earlier private GUID
 * F2C70003 (issue #67) never got past AddItem on 26200 and is retired. */
static const GUID HunminLbiInputModeGuid = {0x2C77A81E, 0x41CC, 0x4178, {0xA3, 0xA7, 0x5F, 0x8A, 0x98, 0x75, 0x68, 0xE6}};
/* The language profile GUID is shared state: registration enables it, so
 * unregistration must name the same GUID to remove it (issue #84). */
static const GUID HunminProfileGuid = {0xF2C70002, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}};
/* The display attribute the composition text carries (issue #98): a solid
 * underline over the undetermined text, app default colors. Registered with
 * ITfCategoryMgr under GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER in
 * DllRegisterServer; the document property GUID_PROP_ATTRIBUTE carries the
 * atom ITfCategoryMgr::RegisterGUID hands back for it. */
static const GUID HunminDisplayAttrGuid = {0xF2C70004, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}};

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

class CTextService : public ITfTextInputProcessor,
                     public ITfThreadMgrEventSink,
                     public ITfKeyEventSink,
                     public ITfCompositionSink,
                     public ITfLangBarItemButton,
                     public ITfSource,
                     public ITfDisplayAttributeProvider {
  public:
    CTextService()
        : ref_(1), threadMgr_(nullptr), clientId_(TF_CLIENTID_NULL), sinkCookie_(0), ctx_(nullptr), ic_(nullptr),
          hangulMode_(true), hasPreeditRect_(false), lastCtx_(nullptr),
          legacyMode_(false),
          composition_(nullptr), compCtx_(nullptr), displayAttrAtom_(0),
          hangulId_("sin3-p2"), latinId_("us-qwerty"), candActive_(false), candCount_(0), candCursor_(0),
          candTop_(0), hanjaCombo_(HunminKeyCombo{HUNMIN_TSF_VIRTUAL_HANJA, 0}) {}
    virtual ~CTextService() {
        if (composition_)
            composition_->Release();
        if (compCtx_) {
            compCtx_->Release();
            compCtx_ = nullptr;
        }
        if (ic_)
            hunmin_ic_free(ic_);
        if (ctx_)
            hunmin_ctx_free(ctx_);
        if (lastCtx_)
            lastCtx_->Release();
    }

    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor)) {
            *ppv = static_cast<ITfTextInputProcessor *>(this);
        } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
            *ppv = static_cast<ITfThreadMgrEventSink *>(this);
        } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
            *ppv = static_cast<ITfKeyEventSink *>(this);
        } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
            *ppv = static_cast<ITfCompositionSink *>(this);
        } else if (IsEqualIID(riid, IID_ITfFunctionProvider)) {
            /* Issue #71: the settings surface. QI'able on the live text
             * service object, exactly the TSF-native reachability
             * mechanism; GetFunction(IID_ITfFnConfigure) then hands out the
             * configure function (settings.cpp). */
            *ppv = hunmin_tsf_settings_new_provider(this);
            return S_OK;
        } else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
            /* Issue #98: the display-attribute provider the app-side
             * attribute manager looks for through the
             * GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER category. */
            *ppv = static_cast<ITfDisplayAttributeProvider *>(this);
        } else if (IsEqualIID(riid, IID_ITfLangBarItem) ||
                   IsEqualIID(riid, IID_ITfLangBarItemButton)) {
            /* ctfutb.h: the language-bar button. ITfLangBarItemButton
             * derives from ITfLangBarItem, so both IIDs bind this. */
            *ppv = static_cast<ITfLangBarItemButton *>(this);
            AddRef();
            return S_OK;
        } else if (IsEqualIID(riid, IID_ITfSource)) {
            /* language-bar.md: the item MUST expose ITfSource — when the
             * item is installed the language bar calls the item's
             * ITfSource::AdviseSink with IID_ITfLangBarItemSink to install
             * its change-notification sink. */
            *ppv = static_cast<ITfSource *>(this);
            AddRef();
            return S_OK;
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

    /* Issue #71: re-read the configuration store and rebuild the live
     * session — the settings dialog's Apply path, no process restart.
     * Activate() reuses it, so there is exactly one config-read + rebuild
     * in the adapter (never two that could drift). The buffered word is
     * untouched (a word boundary flush operates the same after Apply); the
     * IN-FLIGHT syllable of a running composition is lost by the ic
     * swap — acceptable for an explicit Apply, recorded in
     * tests/manual-checklists/tsf-settings-window.md. */
    void ApplyConfigStore() {
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg) {
            fprintf(dbg, "ApplyConfigStore pid=%lu\n", (unsigned long)GetCurrentProcessId());
            fflush(dbg);
        }
        HunminTsfConfig cfg;
        hunmin_tsf_config_load(HKEY_CURRENT_USER, &cfg);
        resolveLayoutSlot(ctx_, cfg.hangulLayout, kHangulDefaultId, true, &hangulId_, dbg);
        resolveLayoutSlot(ctx_, cfg.latinLayout, kLatinDefaultId, false, &latinId_, dbg);
        /* The mode-toggle set (Android KeyBinding semantics): configured
         * chords ADD, and the bare physical Han/Eng key is appended when
         * the store does not carry it — it must never go dead. */
        {
            HunminKeyCombo parsed[16];
            const int nParsed = hunmin_tsf_keybindings_parse(cfg.toggleKeys, parsed, 16);
            toggleKeys_.assign(parsed, parsed + nParsed);
            bool hangeulBare = false;
            for (const HunminKeyCombo &c : toggleKeys_)
                if (c.vk == HUNMIN_TSF_VIRTUAL_HANGEUL && c.mods == 0)
                    hangeulBare = true;
            if (!hangeulBare)
                toggleKeys_.push_back(HunminKeyCombo{HUNMIN_TSF_VIRTUAL_HANGEUL, 0});
            if (dbg) {
                fprintf(dbg, "  toggle keys=%zu (configured bits: %s)\n", toggleKeys_.size(),
                        cfg.toggleKeys[0] ? cfg.toggleKeys : "(none)");
                fflush(dbg);
            }
        }
        /* The candidate-window key (issue #67): ONE binding from the store,
         * default the bare physical Hanja key. An unparseable binding falls
         * back to the default rather than matching nothing — the candidate
         * window must stay reachable, and a parse failure must never
         * swallow a keystroke. */
        {
            HunminKeyCombo parsed[2];
            const int nParsed =
                hunmin_tsf_keybindings_parse(cfg.hanjaKey, parsed, 2);
            if (nParsed > 0) {
                hanjaCombo_ = parsed[0];
            } else {
                hanjaCombo_ = HunminKeyCombo{HUNMIN_TSF_VIRTUAL_HANJA, 0};
                if (dbg) {
                    fprintf(dbg, "  hanja key unparseable (%s) -> default VK_HANJA\n",
                            cfg.hanjaKey[0] ? cfg.hanjaKey : "(empty)");
                    fflush(dbg);
                }
            }
        }
        /* The candidate window belongs to a composition the ic swap below
         * drops: close it with Apply, like the mode toggle does (issue #67). */
        CloseCandidateWindow();
        if (ic_) {
            hunmin_ic_free(ic_);
            ic_ = nullptr;
        }
        if (ctx_)
            ic_ = hunmin_ic_new(ctx_, hangulMode_ ? hangulId_.c_str() : latinId_.c_str());
        if (dbg) {
            fprintf(dbg, "  applied hangul=%s latin=%s ic=%p\n", hangulId_.c_str(),
                    latinId_.c_str(), (void *)ic_);
            fclose(dbg);
        }
    }

    /* The langbar item's change notification (issue #103, the langbar-item
     * surface): every mode boundary notifies the kernel through the
     * ITfLangBarItemSink the language bar installs onto the item at
     * AddItem (language-bar.md) — the kernel then re-queries GetIcon/
     * GetText, showing the new 한/A badge with no host restart. */
    void UpdateLangBarItem() {
        if (itemSink_)
            itemSink_->OnUpdate(TF_LBI_ICON | TF_LBI_TEXT);
    }

    /* The mode-switch core (issue #103): the toggle key handler is the one
     * caller, and its context is live — flip, conversion compartment, the
     * langbar item's notification, the composition-boundary flush, and the
     * ic rebuild with the other slot's layout run in one place. */
    void ToggleModeCore(ITfContext *pic, FILE *dbg) {
        CloseCandidateWindow();
        BoundaryFlush(pic, dbg);
        hangulMode_ = !hangulMode_;
        UpdateConversionCompartment();
        UpdateLangBarItem();
        if (ic_) {
            hunmin_ic_free(ic_);
            ic_ = nullptr;
        }
        if (ctx_)
            ic_ = hunmin_ic_new(ctx_, hangulMode_ ? hangulId_.c_str() : latinId_.c_str());
        /* This gated line is the one evidence a mode boundary ran in this
         * host at all: the key path's own dbg line lives in its handler. */
        HMLOG("toggle core -> mode=%s ic=%p pic=%p\r\n",
              hangulMode_ ? "hangul" : "english", (void *)ic_, (void *)pic);
    }

    STDMETHOD(Activate)(ITfThreadMgr *ptm, TfClientId tid) override {
        threadMgr_ = ptm;
        clientId_ = tid;
        if (ptm)
            ptm->AddRef();
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg) {
            fprintf(dbg, "Activate pid=%lu tid=%u\n", (unsigned long)GetCurrentProcessId(), (unsigned)tid);
            fflush(dbg);
            fclose(dbg);
        }
        if (ptm) {
            ITfKeystrokeMgr *pks = nullptr;
            FILE *dbg2 = tsf_log_open("hunmin-tsf-debug.log");
            if (dbg2)
                fprintf(dbg2, "Advise threadmgr attempt\n"), fflush(dbg2);
            ITfSource *src = nullptr;
            HRESULT srch = ptm->QueryInterface(IID_ITfSource, (void **)&src);
            HRESULT tmHr = E_NOINTERFACE;
            if (SUCCEEDED(srch) && src) {
                tmHr = src->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink *>(this),
                                       &sinkCookie_);
                src->Release();
            }
            if (dbg2) {
                fprintf(dbg2, "  source hr=0x%lX threadmgr advise hr=0x%lX cookie=%lu\n", (unsigned long)srch,
                        (unsigned long)tmHr, (unsigned long)sinkCookie_);
                fflush(dbg2);
            }
            HRESULT qh = ptm->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pks);
            if (dbg2) {
                fprintf(dbg2, "Advise QI hr=0x%lX pks=%p\n", (unsigned long)qh, (void *)pks);
                fflush(dbg2);
            }
            if (SUCCEEDED(qh) && pks) {
                HRESULT ah = pks->AdviseKeyEventSink(clientId_, this, TRUE);
                if (dbg2) {
                    fprintf(dbg2, "AdviseKeyEventSink hr=0x%lX\n", (unsigned long)ah);
                    fflush(dbg2);
                }
                pks->Release();
            }
            if (dbg2)
                fclose(dbg2);
        }
        ctx_ = hunmin_ctx_from_module();
        FILE *dbg3 = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg3) {
            fprintf(dbg3, "  ctx_=%p\n", (void *)ctx_);
            fflush(dbg3);
        }

        /* Layout wiring (issue #66), now via ApplyConfigStore() (issue #71):
         * the same re-read + rebuild the settings dialog's Apply drives on
         * a LIVE service, without a process restart — acceptance is
         * identical because the config boundary resets exactly the state
         * Activate() had to do anyway; no separate activation-only copy
         * exists. Activate() starts hangulMode_=true. */
        ApplyConfigStore();
        /* Issue #119: the legacy path is DETECTED at runtime (a mid-word
         * host termination flips the service); HUNMIN_TSF_INTERIM=1 only
         * FORCES every context onto it up front (debug/probe escape
         * hatch). */
        {
            char v[8] = {0};
            interimForce_ = GetEnvironmentVariableA("HUNMIN_TSF_INTERIM", v, sizeof v) &&
                            (v[0] == '1' || _stricmp(v, "on") == 0 || _stricmp(v, "true") == 0);
            legacyMode_ = interimForce_;
            delivery_ = legacyMode_ ? (Delivery *)&interimDelivery_ : &nativeDelivery_;
            HMLOG("legacy force=%d detected-mode=%d\n", interimForce_ ? 1 : 0, legacyMode_ ? 1 : 0);
        }
        /* THE language-bar item (issue #103, the ruled langbar surface): the
         * mode indicator (icon 한 / A, text) AND the right-click config menu
         * ride our own ITfLangBarItemButton, added while the service is
         * active and removed in Deactivate (docs: language-bar.md, "Adding
         * Items to the Language Bar"). */
        {
            ITfLangBarItemMgr *plbm = nullptr;
            HRESULT hb = ptm->QueryInterface(IID_ITfLangBarItemMgr, (void **)&plbm);
            FILE *dbg4 = tsf_log_open("hunmin-tsf-debug.log");
            if (dbg4) {
                fprintf(dbg4, "langbar QI hr=0x%lX\n", (unsigned long)hb);
                fflush(dbg4);
            }
            if (SUCCEEDED(hb) && plbm) {
                HRESULT ai = plbm->AddItem(static_cast<ITfLangBarItem *>(this));
                if (SUCCEEDED(ai)) {
                    /* The mgr holds a reference to us; drop it only in
                     * Deactivate's RemoveItem, the release there. */
                    langbar_registered_ = true;
                }
                FILE *dbg5 = tsf_log_open("hunmin-tsf-debug.log");
                if (dbg5) {
                    fprintf(dbg5, "langbar AddItem hr=0x%lX\n", (unsigned long)ai);
                    fflush(dbg5);
                    fclose(dbg5);
                }
                plbm->Release();
            }
            if (dbg4)
                fclose(dbg4);
        }
        UpdateConversionCompartment();
        /* The item's first notification: the sink is only set once the
         * language bar advises it (the item's ITfSource::AdviseSink), so an
         * AddItem-less host (or the CTF kernel's NULL sink) simply sees a
         * badge re-read at the next mode boundary. */
        UpdateLangBarItem();
        if (dbg3) {
            if (ctx_)
                fprintf(dbg3, "  layout_count=%d\n", hunmin_ctx_layout_count(ctx_));
            fflush(dbg3);
            fclose(dbg3);
        }
        return S_OK;
    }
    STDMETHOD(Deactivate)() override {
        if (threadMgr_) {
            ITfKeystrokeMgr *pks = nullptr;
            if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pks)) && pks) {
                pks->UnadviseKeyEventSink(clientId_);
                pks->Release();
            }
            threadMgr_->Release();
            threadMgr_ = nullptr;
        }
        /* A live composition (issue #98) is finalised in its own context
         * first: the composed word stays there as committed text and the
         * buffers drain, so the plain-insert path below only handles what
         * is left when no composition could ever be started. */
        DetachComposition();
        if (lastCtx_ && ic_) {
            const char *flushed = hunmin_ic_flush(ic_);
            if (flushed && flushed[0]) {
                int flen = MultiByteToWideChar(CP_UTF8, 0, flushed, -1, nullptr, 0);
                if (flen > 1) {
                    size_t old = wordbuf_.size();
                    /* flen counts the NUL terminator; text only, as above. */
                    wordbuf_.resize(old + flen - 1);
                    MultiByteToWideChar(CP_UTF8, 0, flushed, -1, &wordbuf_[old], flen);
                }
            }
        }
        if (!wordbuf_.empty() && lastCtx_) {
            int wlen = (int)wordbuf_.size() + 1;
            wchar_t *wbuf = new wchar_t[wlen];
            wordbuf_.copy(wbuf, wordbuf_.size());
            wbuf[wordbuf_.size()] = 0;
            class DeactSession : public ITfEditSession {
              public:
                DeactSession(ITfContext *ctx, TfClientId cid, const wchar_t *t)
                    : ref_(1), ctx_(ctx), cid_(cid), t_(t) {}
                STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
                    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
                        *ppv = this;
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
                STDMETHOD(DoEditSession)(TfEditCookie ec) override {
                    TF_SELECTION sel;
                    ULONG fetched = 0;
                    if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) &&
                        fetched == 1 && sel.range) {
                        sel.range->SetText(ec, 0, t_, -1);
                        sel.range->Collapse(ec, TF_ANCHOR_END);
                        TF_SELECTION ns = sel;
                        ctx_->SetSelection(ec, 1, &ns);
                        sel.range->Release();
                    }
                    return S_OK;
                }

              private:
                LONG ref_;
                ITfContext *ctx_;
                TfClientId cid_;
                const wchar_t *t_;
            };
            DeactSession *ds = new DeactSession(lastCtx_, clientId_, wbuf);
            HRESULT hrS = S_OK;
            lastCtx_->RequestEditSession(clientId_, ds, TF_ES_SYNC | TF_ES_READWRITE, &hrS);
            ds->Release();
            delete[] wbuf;
        }
        wordbuf_.clear();
        CloseCandidateWindow();
        /* Drop the language-bar item; the langbar kernel releases its
         * reference to us, which is why RemoveItem must pass the SAME
         * pointer it got in AddItem. */
        if (langbar_registered_ && threadMgr_) {
            ITfLangBarItemMgr *plbm = nullptr;
            if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfLangBarItemMgr, (void **)&plbm)) && plbm) {
                plbm->RemoveItem(static_cast<ITfLangBarItem *>(this));
                plbm->Release();
            }
            langbar_registered_ = false;
            /* The kernel-provided sink goes with the item (language-bar.md:
             * the sink lives between AddItem and RemoveItem). */
            if (itemSink_) {
                itemSink_->Release();
                itemSink_ = nullptr;
            }
            itemSinkCookie_ = 0;
        }
        if (ic_) {
            hunmin_ic_free(ic_);
            ic_ = nullptr;
        }
        if (ctx_) {
            hunmin_ctx_free(ctx_);
            ctx_ = nullptr;
        }
        if (lastCtx_) {
            lastCtx_->Release();
            lastCtx_ = nullptr;
        }
        clientId_ = TF_CLIENTID_NULL;
        return S_OK;
    }
    /* ---- ctfutb.h: ITfLangBarItem (the base surface the manager calls),
     * then ITfLangBarItemButton. THE langbar surface for issue #103 (the
     * ruled lane: our own item, not the system-langbar extension): the
     * icon/text are the mode indicator, the right-click config menu
     * lands on the shared settings launch. ---- */
    STDMETHOD(GetInfo)(TF_LANGBARITEMINFO *pInfo) override {
        if (!pInfo)
            return E_INVALIDARG;
        ZeroMemory(pInfo, sizeof *pInfo);
        pInfo->clsidService = CLSID_HunminTSF;
        /* guidItem MUST be GUID_LBI_INPUTMODE: the Win8+/Win11 "input
         * indicator" (the per-TIP badge hosted in the taskbar tray) hosts a
         * langbar item only when this is its guidItem (measured sibling
         * project: any other item GUID is ignored by the shell's hosted-item
         * path). The older private GUID F2C70003 never got past AddItem. */
        pInfo->guidItem = HunminLbiInputModeGuid;
        /* BTN_BUTTON | SHOWNINTRAY — the shape measured live on this host
         * by the sibling jamotong TIP, whose own item is ACTUALLY hosted by
         * the tray input indicator with this exact pair: with BTN_BUTTON
         * alone+F2C70003 every AddItem answered E_FAIL (measured
         * 2026-10-18, the "reduced hypothesis" then truthed out to be
         * wrong-headed), so the tray-style bit returns as the sibling's
         * working shape documents it. */
        pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;
        pInfo->ulSort = 0;
        lstrcpynW(pInfo->szDescription, L"Hunmin \uC124\uC815", MAX_PATH);
        return S_OK;
    }
    STDMETHOD(GetStatus)(DWORD *pdwStatus) override {
        if (!pdwStatus)
            return E_INVALIDARG;
        *pdwStatus = 0;
        return S_OK;
    }
    STDMETHOD(Show)(BOOL fShow) override {
        (void)fShow;
        return S_OK;
    }
    STDMETHOD(GetTooltipString)(BSTR *pbstrToolTip) override {
        if (!pbstrToolTip)
            return E_INVALIDARG;
        *pbstrToolTip = SysAllocString(L"Hunmin \uC124\uC815 \uC5F4\uAE30");
        return *pbstrToolTip ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHOD(OnClick)(TfLBIClick click, POINT pt, const RECT *prcArea) override {
        (void)prcArea;
        if (click == TF_LBI_CLK_RIGHT) {
            /* The right-click CONFIG MENU (issue #103, the ruled langbar
             * surface). Measured sibling shape (BTN_BUTTON items): with
             * BTN_BUTTON, a right click arrives as OnClick(CLK_RIGHT) and
             * the bar's InitMenu/OnMenuSelect never fire — so the item
             * builds its own popup here, like the reference TIPs do.
             * Owner = the focused window (the indicator does not hand out
             * one); TPM_RETURNCMD|TPM_NONOTIFY resolve the choice from the
             * return value, no WM_COMMAND. */
            HMENU menu = CreatePopupMenu();
            if (menu) {
                const wchar_t *label = L"Hunmin \uC124\uC815 \uC5F4\uAE30";
                AppendMenuW(menu, MF_STRING, 1, label);
                POINT p = pt;
                HMONITOR mon = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
                if (mon) {
                    MONITORINFO mi;
                    mi.cbSize = sizeof(mi);
                    if (GetMonitorInfoW(mon, &mi)) {
                        if (p.x < mi.rcWork.left)
                            p.x = mi.rcWork.left;
                        if (p.x > mi.rcWork.right)
                            p.x = mi.rcWork.right;
                    }
                }
                HWND owner = GetFocus();
                if (!owner)
                    owner = GetForegroundWindow();
                int cmd = (int)TrackPopupMenu(menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
                                              p.x, p.y, 0, owner, nullptr);
                DestroyMenu(menu);
                if (cmd == 1)
                    return hunmin_tsf_launch_settings(static_cast<void *>(this));
            }
            return S_OK;
        }
        (void)pt;
        /* Left click = the command-button action (BTN_BUTTON). The settings
         * launch is a DEDICATED PROCESS, not a modal loop on this thread:
         * the TIP thread must not pump a nested message loop while TSF
         * holds edit cookies. */
        return hunmin_tsf_launch_settings(static_cast<void *>(this));
    }
    STDMETHOD(InitMenu)(ITfMenu *pMenu) override {
        /* The right-click popup: one entry, "<Hunmin settings>" (id = 1);
         * OnMenuSelect lands it on the settings window. */
        if (!pMenu)
            return E_FAIL;
        const wchar_t *label = L"Hunmin \uC124\uC815 \uC5F4\uAE30";
        return pMenu->AddMenuItem(1, 0, nullptr, nullptr, label,
                                  (ULONG)wcslen(label), nullptr);
    }
    STDMETHOD(OnMenuSelect)(UINT wID) override {
        if (wID != 1)
            return S_OK;
        return hunmin_tsf_launch_settings(nullptr);
    }
    /* ---- ITfSource (language-bar.md): the language bar installs its ---
     * ITfLangBarItemSink ONTO THE ITEM through here at AddItem; the item
     * later calls that sink's OnUpdate to notify GetIcon/GetText changes
     * (the mode badge) without a host restart. The sink direction is
     * KERNEL-to-item, never the reverse. ---- */
    STDMETHOD(AdviseSink)(REFIID riid, IUnknown *punk, DWORD *pdwCookie) override {
        if (!punk || !pdwCookie)
            return E_INVALIDARG;
        *pdwCookie = 0;
        if (!IsEqualIID(riid, IID_ITfLangBarItemSink))
            return CONNECT_E_CANNOTCONNECT;
        if (itemSink_)
            return CONNECT_E_ADVISELIMIT;
        ITfLangBarItemSink *sink = nullptr;
        if (FAILED(punk->QueryInterface(IID_ITfLangBarItemSink, (void **)&sink)) || !sink)
            return CONNECT_E_CANNOTCONNECT;
        itemSink_ = sink;
        itemSinkCookie_ = 1;
        *pdwCookie = itemSinkCookie_;
        return S_OK;
    }
    STDMETHOD(UnadviseSink)(DWORD dwCookie) override {
        if (dwCookie == itemSinkCookie_ && itemSink_) {
            itemSink_->Release();
            itemSink_ = nullptr;
            itemSinkCookie_ = 0;
            return S_OK;
        }
        return CONNECT_E_NOCONNECTION;
    }
    STDMETHOD(GetIcon)(HICON *phIcon) override {
        if (!phIcon)
            return E_INVALIDARG;
        /* The input MODE badge (discover: the user asked for the current
         * input mode in the bar): unit - red Hangul badge when the
         * hangul side is active, Latin 'A' badge in english mode; the
         * multi-size app icon (resource 1) remains the service's own
         * identity in the language list. LoadIconW cached-handle rules
         * apply; nothing to free. */
        HMODULE self = nullptr;
        const wchar_t *which =
            hangulMode_ ? MAKEINTRESOURCEW(2) : MAKEINTRESOURCEW(3);
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCWSTR)this, &self)) {
            *phIcon = LoadIconW(self, which);
            if (*phIcon)
                return S_OK;
            /* The mode badges (2/3) are non-critical: fall through to the
             * app icon when the mirroring rc did not compile them. */
            *phIcon = LoadIconW(self, MAKEINTRESOURCEW(1));
            if (*phIcon)
                return S_OK;
        }
        *phIcon = LoadIconW((HMODULE)NULL, IDI_APPLICATION);
        return *phIcon ? S_OK : E_FAIL;
    }
    STDMETHOD(GetText)(BSTR *pbstrText) override {
        if (!pbstrText)
            return E_INVALIDARG;
        /* the MODE as TEXT (the bar renders this on text-capable bars):
         * Hangul on hangul side / Latin on the latin side */
        *pbstrText = SysAllocString(hangulMode_ ? L"\uD55C" : L"A");
        return *pbstrText ? S_OK : E_OUTOFMEMORY;
    }

    /* ---- ITfDisplayAttributeProvider (issue #98): one attribute, enum'd ---- */
    STDMETHOD(EnumDisplayAttributeInfo)(IEnumTfDisplayAttributeInfo **ppEnum) override {
        if (!ppEnum)
            return E_INVALIDARG;
        *ppEnum = new HunminEnumDisplayAttrInfo();
        return S_OK;
    }
    STDMETHOD(GetDisplayAttributeInfo)(REFGUID guid, ITfDisplayAttributeInfo **ppInfo) override {
        if (!ppInfo)
            return E_INVALIDARG;
        *ppInfo = nullptr;
        if (!IsEqualGUID(guid, HunminDisplayAttrGuid))
            return E_FAIL;
        *ppInfo = new HunminDisplayAttrInfo();
        return S_OK;
    }

    STDMETHOD(OnInitDocumentMgr)(ITfDocumentMgr *pdm) override {
        (void)pdm;
        return S_OK;
    }
    STDMETHOD(OnUninitDocumentMgr)(ITfDocumentMgr *pdm) override {
        (void)pdm;
        return S_OK;
    }
    STDMETHOD(OnSetFocus)(ITfDocumentMgr *pdmFocus, ITfDocumentMgr *pdmPrevFocus) override {
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg) {
            fprintf(dbg, "TM-OnSetFocus pdmFocus=%p pdmPrev=%p modePid=%lu\n", (void *)pdmFocus, (void *)pdmPrevFocus,
                    (unsigned long)GetCurrentProcessId());
            if (pdmFocus) {
                ITfContext *pic = nullptr;
                HRESULT hrTop = pdmFocus->GetTop(&pic);
                fprintf(dbg, "  GetTop hr=0x%lX pic=%p\n", (unsigned long)hrTop, (void *)pic);
                if (pic)
                    pic->Release();
            }
            fflush(dbg);
            fclose(dbg);
        }
        (void)pdmFocus;
        (void)pdmPrevFocus;
        return S_OK;
    }
    STDMETHOD(OnPushContext)(ITfContext *pic) override {
        (void)pic;
        return S_OK;
    }
    STDMETHOD(OnPopContext)(ITfContext *pic) override {
        (void)pic;
        return S_OK;
    }

    // ITfKeyEventSink
    STDMETHOD(OnSetFocus)(BOOL fFocus) override {
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg) {
            fprintf(dbg, "KS-OnSetFocus fFocus=%d modePid=%lu\n", (int)fFocus, (unsigned long)GetCurrentProcessId());
            fflush(dbg);
            fclose(dbg);
        }
        (void)fFocus;
        return S_OK;
    }
    STDMETHOD(OnTestKeyDown)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override {
        (void)pic;
        (void)lParam;
        /* The previous key's deferred doc write runs before this key is
         * classified — a keystroke storm must not reorder the doc writes
         * behind the engine state. */
        FlushPendingDocWrites();
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        /* Shared predicate with OnKeyDown (the registration-work rule: test
         * and down are one decision). The configured set always contains the
         * bare Han/Eng key, so the physical key stays interested in both
         * modes. CapsLock is NOT here any more: it is no longer the mode
         * toggle (maintainer instruction: the Han/Eng key is the default),
         * and in Korean input mode it is ignored outright (the engine's
         * case-encoding convention). */
        BOOL eat = FALSE;
        if (hunmin_tsf_keybindings_match(toggleKeys_.data(), (int)toggleKeys_.size(),
                                         (unsigned)wParam, hunmin_tsf_keymods_from_state()))
            eat = TRUE;
        else if (candActive_ && hunmin_tsf_candkey_eats_test(wParam))
            /* While the candidate window is open its keys are ours, on the
             * same shared-predicate rule the toggle uses (test and down are
             * one decision). */
            eat = TRUE;
        else if (!hangulMode_)
            eat = FALSE;
        else if (hunmin_tsf_keybindings_match(&hanjaCombo_, 1, (unsigned)wParam,
                                              hunmin_tsf_keymods_from_state())) {
            /* Mirror the down decision exactly (no look-ahead lie): the
             * window opens when the buffered word has candidates — the
             * whole-word lookup, or issue #100's last-syllable fallback
             * when the whole word yields 0 — and in the zero-and-zero case
             * the key belongs to the client. The suggest lookup is a read
             * of tables already in memory. */
            std::pair<int, bool> look = HanjaListCount();
            eat = look.first > 0 ? TRUE : FALSE;
        } else if (hangulMode_ &&
                   (hunmin_tsf_keymods_from_state() &
                    (HUNMIN_TSF_KEYMOD_CTRL | HUNMIN_TSF_KEYMOD_ALT))) {
            /* Mirror of the down-side chord rule: a Ctrl/Alt chord belongs
             * to the app (Ctrl+A is not composition input) — test phase
             * answers the same uneaten the down phase will act on. */
            eat = FALSE;
        } else if (wParam >= 'A' && wParam <= 'Z') {
            eat = TRUE;
        } else if (wParam == VK_SPACE) {
            /* A boundary key is a key of passage (#95 contract): the app
             * inserts the space. MEASURED on msftedit: with eat=FALSE the
             * host NEVER calls OnKeyDown for the key — the test phase is
             * the TIP's only sight of it — so the boundary drain runs
             * HERE. The span bookkeeping is position-based, so the drain
             * is order-independent against the app's own space insertion,
             * and a host that DOES call the down re-runs the flush on the
             * already-empty buffer (a no-op). */
            BoundaryFlush(pic, dbg);
            eat = FALSE;
        } else if (wParam == VK_BACK) {
            /* Against in-flight text the engine consumes the key (the jamo
             * deletes). At an EMPTY in-flight the engine declines — and on
             * the interim path the key must then PASS, so the app deletes
             * the committed char (measured: eaten = the backspace does
             * nothing at all); the span bookkeeping self-heals at the next
             * write (the fresh-span anchor re-reads the live caret). The
             * native path keeps the key: its declined branch is the
             * #98-verified composition-close shape. */
            const char *pre = ic_ ? hunmin_ic_preedit(ic_) : "";
            eat = (legacyMode_ && pre[0] == '\0') ? FALSE : TRUE;
        }
        if (pfEat)
            *pfEat = eat;
        if (dbg) {
            fprintf(dbg, "OnTestKeyDown w=%lu eat=%d mode=%d\n", (unsigned long)wParam, (int)eat, (int)hangulMode_);
            fflush(dbg);
            fclose(dbg);
        }
        return S_OK;
    }
    STDMETHOD(OnKeyDown)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override {
        /* Same entry flush as the test phase: whichever callback the host
         * reaches first settles the previous key's doc write. */
        FlushPendingDocWrites();
        TrackKeyContext(pic);
        FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
        if (dbg) {
            fprintf(dbg, "OnKeyDown wParam=%lu ic=%p\n", (unsigned long)wParam, (void *)ic_);
            fflush(dbg);
        }
        if (pfEat)
            *pfEat = FALSE;
        if (!ic_) {
            if (dbg) {
                fprintf(dbg, " ic null -> pass\n");
                fclose(dbg);
            }
            return S_OK;
        }
        /* Each handler owns one key class and answers one question: is the
         * key ours (pfEat), and what state changes. The order IS the
         * precedence: the toggle, the mode passthrough, the candidate
         * window's own key, the open window's list keys, the keys of
         * passage, and finally the composition feed. */
        if (IsModeToggle(wParam)) {
            ApplyModeToggle(pic, wParam, lParam, pfEat, dbg);
            return S_OK;
        }
        if (!hangulMode_) {
            if (dbg)
                fclose(dbg);
            return S_OK; // passthrough English
        }
        if (IsCandidateToggle(wParam)) {
            ToggleCandidateWindow(pic, wParam, pfEat, dbg);
            return S_OK;
        }
        if (candActive_ && CandidateListKey(pic, wParam, pfEat, dbg))
            return S_OK;
        if (IsKeyOfPassage(wParam)) {
            DrainAndPass(pic, wParam, pfEat, dbg, "edit key");
            return S_OK;
        }
        if (wParam >= VK_F1 && wParam <= VK_F12) {
            DrainAndPass(pic, wParam, pfEat, dbg, "fkey");
            return S_OK;
        }
        FeedEngineKey(pic, wParam, pfEat, dbg);
        return S_OK;
    }

    void TrackKeyContext(ITfContext *pic) {
        if (lastCtx_ == pic)
            return;
        if (lastCtx_)
            lastCtx_->Release();
        lastCtx_ = pic;
        if (lastCtx_)
            lastCtx_->AddRef();
        if (composition_ && compCtx_ && compCtx_ != pic)
            DetachComposition();
    }

    bool IsModeToggle(WPARAM wParam) {
        return hunmin_tsf_keybindings_match(toggleKeys_.data(), (int)toggleKeys_.size(),
                                            (unsigned)wParam, hunmin_tsf_keymods_from_state()) != FALSE;
    }

    /* The mode toggle (issue #66 / #71): the configured bindings plus the
     * always-on bare Han/Eng key (VK_HANGEUL, the platform alternate no
     * rebinding can make dead — the Android rule). A toggle is a mode
     * boundary, i.e. a composition boundary (Jamotong manual §9.5; the
     * Android adapter commits at the mode switch). Auto-repeat is keydown
     * with the preceding-down bit set (lParam bit 30): the press must
     * toggle once, and repeats are swallowed rather than delivered to the
     * app. The steps themselves are the SHARED mode-switch core
     * (ToggleModeCore, issue #103), whose tail notifies the langbar item. */
    void ApplyModeToggle(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat, FILE *dbg) {
        if (pfEat)
            *pfEat = TRUE;
        const bool repeat = (lParam & (1 << 30)) != 0;
        if (!repeat)
            ToggleModeCore(pic, dbg);
        if (dbg) {
            fprintf(dbg, " toggle key vk=%lu repeat=%d -> %s (%s)\n",
                    (unsigned long)wParam, (int)repeat,
                    hangulMode_ ? "hangul" : "english",
                    hangulMode_ ? hangulId_.c_str() : latinId_.c_str());
            fclose(dbg);
        }
    }

    bool IsCandidateToggle(WPARAM wParam) {
        return hunmin_tsf_keybindings_match(&hanjaCombo_, 1, (unsigned)wParam,
                                            hunmin_tsf_keymods_from_state()) != FALSE;
    }

    /* The candidate-window key (issue #67). The configured binding (default:
     * the bare physical Hanja key) toggles the window. Interception sits
     * before any engine feed because it is not a printable key (fcitx5's
     * identical placement for F9, engine.cpp:204-223). Consumption is
     * conditional whether the window is open or, with a candidate-bearing
     * buffer, just opened; with neither, the key belongs to the client app
     * and passes through uneaten. */
    void ToggleCandidateWindow(ITfContext *pic, WPARAM wParam, BOOL *pfEat, FILE *dbg) {
        if (candActive_) {
            CloseCandidateWindow();
        } else {
            OpenCandidates();
            if (candActive_)
                UpdateCandidateWindow(pic);
        }
        if (dbg) {
            fprintf(dbg, " hanja key vk=%lu -> %s (src=%s count=%d)\n",
                    (unsigned long)wParam, candActive_ ? "open" : "closed",
                    candSource_.c_str(), candCount_);
            fflush(dbg);
            fclose(dbg);
        }
        if (pfEat)
            *pfEat = candActive_ ? TRUE : FALSE;
    }

    /* The open window's list keys (issue #67): Enter commits the cursor,
     * Esc closes, arrows/PageUp/PageDown move, digits 1..0 pick. A digit
     * past the end of the count closes the window and falls through like
     * any other unhandled key — an invisible window must never swallow a
     * keystroke. Returns true when the key was ours. */
    bool CandidateListKey(ITfContext *pic, WPARAM wParam, BOOL *pfEat, FILE *dbg) {
        bool handled = false;
        bool submit = false;
        int submitIdx = candCursor_;
        switch (wParam) {
        case VK_RETURN:
            submit = true;
            handled = true;
            break;
        case VK_ESCAPE:
            CloseCandidateWindow();
            handled = true;
            break;
        case VK_UP:
        case VK_LEFT:
            if (candCursor_ > 0) {
                --candCursor_;
                PageFollowCursor();
                UpdateCandidateWindow(pic);
            }
            handled = true;
            break;
        case VK_DOWN:
        case VK_RIGHT:
            if (candCursor_ < candCount_ - 1) {
                ++candCursor_;
                PageFollowCursor();
                UpdateCandidateWindow(pic);
            }
            handled = true;
            break;
        case VK_PRIOR:
            PageStep(-1);
            UpdateCandidateWindow(pic);
            handled = true;
            break;
        case VK_NEXT:
            PageStep(+1);
            UpdateCandidateWindow(pic);
            handled = true;
            break;
        default:
            /* Digit select: 1..9 pick the page-relative candidates 1..9,
             * 0 picks the tenth, mirroring fcitx5's label scheme. */
            if (wParam >= '0' && wParam <= '9') {
                int pos = (wParam == '0') ? 9 : (int)(wParam - '1');
                int idx = candTop_ + pos;
                if (pos >= 0 && idx < candCount_) {
                    submitIdx = idx;
                    submit = true;
                    handled = true;
                }
            }
            break;
        }
        if (submit)
            CommitCandidate(pic, submitIdx);
        if (!handled) {
            /* Anything else: close and fall through to the normal path, so
             * typing continues where the user left off instead of being
             * swallowed by an invisible window (fcitx5: the default rule).
             * The buffered word is untouched by the close itself. */
            CloseCandidateWindow();
            return false;
        }
        if (pfEat)
            *pfEat = TRUE;
        if (dbg) {
            fprintf(dbg, " candidate key vk=%lu cursor=%d count=%d\n", (unsigned long)wParam,
                    candCursor_, candCount_);
            fclose(dbg);
        }
        return true;
    }

    /* Keys of passage (measured on this host during the buffered-input
     * check: VK_LEFT..VK_NEXT + VK_INSERT/VK_DELETE all fell into the
     * printable fallback below, so each was translated into a layout
     * character — VK_UP 38 -> '&', DELETE 46 -> '.' — and EATEN). A cursor
     * move is a composition boundary: the boundary drain commits the
     * buffered word exactly once and the key passes through untouched.
     * Reached only while the candidate window is NOT open: when it is,
     * CandidateListKey owns these vk's for list motion (issue #67). */
    bool IsKeyOfPassage(WPARAM wParam) {
        switch (wParam) {
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_INSERT:
        case VK_DELETE:
            return true;
        default:
            /* The function-key block (measured with the unbound-F9 case of
             * issue #67: VK_F1..VK_F12 = 0x70..0x7B falls into the
             * printable fallback below, so an unbound F-key composed its
             * VK-value as a layout character — F9(0x78) typed 'x' = ㅈ into
             * the composition the user was watching. Same defect class,
             * same keys-of-passage answer. */
            return wParam >= VK_F1 && wParam <= VK_F12;
        }
    }

    void DrainAndPass(ITfContext *pic, WPARAM wParam, BOOL *pfEat, FILE *dbg,
                      const char *tag) {
        BoundaryFlush(pic, dbg);
        if (pfEat)
            *pfEat = FALSE;
        if (dbg) {
            fprintf(dbg, " %s vk=%lu -> boundary + passthrough\n", tag,
                    (unsigned long)wParam);
            fclose(dbg);
        }
    }

    /* The VK -> base half, and only that half (issue #106): the physical
     * key's UNSHIFTED US-QWERTY character. The shift column the adapter
     * used to carry is the engine's own map (data.c BASE/SHIFTED), selected
     * in the engine by the Shift bit that travels beside the base
     * (FRONTEND-CONTRACT.md A3'). */
    static uint32_t VkToBase(WPARAM wParam) {
        if (wParam >= 'A' && wParam <= 'Z')
            return (uint32_t)(wParam + 32); /* the lowercase half */
        if (wParam >= '0' && wParam <= '9')
            return (uint32_t)wParam;
        if (wParam == VK_SPACE)
            return ' ';
        if (wParam == VK_BACK)
            return 0x08;
        switch (wParam) {
        case VK_OEM_1:
            return ';';
        case VK_OEM_7:
            return '\'';
        case VK_OEM_COMMA:
            return ',';
        case VK_OEM_PERIOD:
            return '.';
        case VK_OEM_2:
            return '/';
        case VK_OEM_3:
            return '`';
        case VK_OEM_4:
            return '[';
        case VK_OEM_5:
            return '\\';
        case VK_OEM_6:
            return ']';
        case VK_OEM_MINUS:
            return '-';
        case VK_OEM_PLUS:
            return '=';
        default:
            if (wParam >= 0x20 && wParam <= 0x7E)
                return (uint32_t)wParam;
            return 0;
        }
    }

    /* Modifier keys are not text: never feed them to the engine (would
     * corrupt an in-flight syllable). CapsLock is no longer the mode toggle
     * (maintainer instruction: the Han/Eng key is the default), and in
     * Korean input mode the key is ignored outright: it passes through for
     * the OS to toggle, and the engine `caps` option it feeds is set per
     * key event beside the modifier snapshot (issue #112) — never into the
     * (base, shift) pair the engine resolves rows from. */
    static bool IsModifierVk(WPARAM wParam) {
        switch (wParam) {
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
        case VK_CAPITAL:
        case VK_TAB:
            return true;
        default:
            return false;
        }
    }

    /* Word buffering (issue #73, adapter-side; no libhunmin change):
     * hunmin_ic_commit() is a destructive read of every syllable the
     * engine finalised so far, so the adapter accumulates it instead of
     * inserting per syllable. The word size includes no NUL: the buffer
     * holds text only, or every later insert would stop at the embedded
     * NUL after the first syllable. */
    void HarvestFinalised() {
        const char *commit = hunmin_ic_commit(ic_);
        if (!commit || !commit[0])
            return;
        int clen = MultiByteToWideChar(CP_UTF8, 0, commit, -1, nullptr, 0);
        if (clen <= 1)
            return;
        size_t old = wordbuf_.size();
        wordbuf_.resize(old + clen - 1);
        MultiByteToWideChar(CP_UTF8, 0, commit, -1, &wordbuf_[old], clen);
    }

    /* The composition feed: the engine decides consumed/declined. A chord
     * (Ctrl/Alt) is application input, not composition input — measured
     * defect 2026-10-18: the letter inside a Ctrl/Alt chord still fed the
     * composition, Ctrl+A typed the preedit's next syllable instead of
     * select-all. The chord commits through the SAME boundary flush a mode
     * toggle drives and passes uneaten; the mode itself is NOT flipped
     * (ruled out 2026-10-18, issue #101: "will not switch mode; transient
     * processing is enough" — docs/WINDOWS-TSF-BUFFERING.md and
     * wiki/decisions/2026-10-18-tsf-chord-transient-not-persistent.md).
     * A decline is a word boundary for the buffering adapter (issue #73):
     * the boundary drain runs and the key passes (pfEat stays FALSE) —
     * the fcitx/android "used=FALSE => passthrough" contract. */
    void FeedEngineKey(ITfContext *pic, WPARAM wParam, BOOL *pfEat, FILE *dbg) {
        /* One snapshot of the modifier state for this event. The chord rule
         * below and the shift bit handed to the engine both read it, so the
         * decision and the bit can never disagree, and the mock harness's
         * GetKeyState seam (frontends/tsf/test/trace-emit.cpp) covers both
         * with one injection. */
        const unsigned int mods = hunmin_tsf_keymods_from_state();
        /* The engine caps option, fed per event beside the modifier snapshot
         * (issue #112, ruling "wire and make it common to all platform"):
         * one set per key event, 1 when the platform CapsLock is toggled on,
         * 0 when off — the shape fcitx5 has had all along (engine.cpp:327).
         * The option feeds the yethangul P variable only (hunmin.h:93); it
         * selects NO row — the Shift bit stays the only row transport
         * (the engine's case endpoint, hunmin_ic_process_key). */
        hunmin_ic_set_option(ic_, "caps", hunmin_tsf_capslock_from_state() ? 1 : 0);
        if (mods & (HUNMIN_TSF_KEYMOD_CTRL | HUNMIN_TSF_KEYMOD_ALT)) {
            CloseCandidateWindow();
            BoundaryFlush(pic, dbg);
            if (pfEat)
                *pfEat = FALSE;
            if (dbg) {
                fprintf(dbg, " ctrl/alt chord -> commit + passthrough\n");
                fclose(dbg);
            }
            return;
        }
        if (IsModifierVk(wParam)) {
            if (dbg) {
                fprintf(dbg, " modifier/pass key -> leave composition\n");
                fclose(dbg);
            }
            return;
        }
        /* The case endpoint is the engine's (issue #106): the unshifted base
         * plus the Shift bit from the snapshot above. */
        bool consumed = hunmin_ic_process_key(
            ic_, VkToBase(wParam), (mods & HUNMIN_TSF_KEYMOD_SHIFT) ? 1u : 0u);
        if (!consumed) {
            BoundaryFlush(pic, dbg);
            return;
        }
        HarvestFinalised();
        if (dbg) {
            fprintf(dbg, " consumed, word len=%zu preedit=%s\n", wordbuf_.size(),
                    hunmin_ic_preedit(ic_));
            fflush(dbg);
        }
        /* Issue #119: the legacy interim path when the host was detected
         * terminating compositions mid-word (or the env said so); the #98
         * native path otherwise stays as is. The doc write is POSTED —
         * never inline in a TSF callback (the measured focus-storm defect). */
        delivery_->Consumed(pic);
        if (pfEat)
            *pfEat = TRUE;
    }

    STDMETHOD(OnTestKeyUp)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override {
        (void)pic;
        (void)wParam;
        (void)lParam;
        if (pfEat)
            *pfEat = FALSE;
        return S_OK;
    }
    STDMETHOD(OnKeyUp)(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEat) override {
        (void)pic;
        (void)wParam;
        (void)lParam;
        if (pfEat)
            *pfEat = FALSE;
        return S_OK;
    }
    STDMETHOD(OnPreservedKey)(ITfContext *pic, const GUID &rguid, BOOL *pfEat) override {
        (void)pic;
        (void)rguid;
        if (pfEat)
            *pfEat = FALSE;
        return S_OK;
    }

    // ITfCompositionSink
    STDMETHOD(OnCompositionTerminated)(TfEditCookie ecWrite, ITfComposition *pComposition) override {
        (void)ecWrite;
        (void)pComposition;
        HMLOG("OnCompositionTerminated comp=%p wordbuf=%zu self=%d\n",
              (void *)composition_, wordbuf_.size(), endingSelf_ ? 1 : 0);
        if (endingSelf_) {
            /* Our own EndComposition (issue #98): the composed text is
             * document text by OUR decision, so the buffers drain and the
             * engine state resets — the same word must never be delivered
             * twice through the next boundary. */
            if (composition_) {
                composition_->Release();
                composition_ = nullptr;
            }
            if (compCtx_) {
                compCtx_->Release();
                compCtx_ = nullptr;
            }
            compText_.clear();
            wordbuf_.clear();
            if (ic_)
                hunmin_ic_reset(ic_);
            return S_OK;
        }
        /* Host-driven termination. A modern host terminates only at real
         * boundaries (focus loss, user click-away); when the state is
         * mid-word — the in-flight text was composed but the word is not
         * finished — the host is a classic (default-sink) one: the #119
         * measurement (rounds 11-14; the baseline round in
         * docs/TSF-INTERIM-INPUT-SPIKE.md) shows these hosts terminate per
         * key. Flip THIS SERVICE to the interim path for its life and
         * recover what this termination fused:
         *   - the engine's state is NOT ours to drop: the in-flight
         *     syllable is still live in the ic; the doc now holds a DEAD
         *     COPY of what the composition showed (compText_) — delete
         *     that copy from the control (EM), and the ic keeps composing
         *     from the same jamo state (no reset, no re-feed needed). */
        if (wordbuf_.empty() && compText_.empty()) {
            /* nothing mid-word: a real boundary under our own bookkeeping
             * must have run already — plain cleanup, old contract. */
            if (composition_) {
                composition_->Release();
                composition_ = nullptr;
            }
            if (compCtx_) {
                compCtx_->Release();
                compCtx_ = nullptr;
            }
            compText_.clear();
            return S_OK;
        }
        HMLOG("legacy detection: mid-word host termination (compText=%zu wordbuf=%zu) -> interim path\n",
              compText_.size(), wordbuf_.size());
        legacyMode_ = true;
        delivery_ = &interimDelivery_;
        /* What is dead in the doc = the WHOLE composition text (compText_:
         * nothing under this composition was ours to keep there — the
         * interim path rebuilds the word from the LIVE buffers and live
         * ic state). Delete the dead copy at the POSTED write — never
         * inline here: this handler runs inside the host's TSF callback,
         * where the EM writes fire the re-entrant focus storm that eats
         * keystrokes (measured). One path for every caret state: the
         * write re-measures the caret when the app's queue has settled
         * (the fused copy's caret was measured BOTH behind the span and
         * past it, settling differently per host). The ic keeps composing
         * from the same jamo state (no reset) and wordbuf_ keeps the
         * finalised syllables so the write re-appends them to the doc. */
        interimDelivery_.CaptureDeadCopy(compText_);
        /* A write posted by the terminated key is superseded: the host has
         * fused the composition text itself and the re-bookkeeping above
         * recomputes the span. The posted write runs as THIS flip's dead
         * copy removal instead. */
        interimDelivery_.CaptureDeadCopy(compText_);
        if (composition_) {
            composition_->Release();
            composition_ = nullptr;
        }
        if (compCtx_) {
            compCtx_->Release();
            compCtx_ = nullptr;
        }
        compText_.clear();
        return S_OK;
    }

    /* ---- Native composition preedit (issue #98) ----
     *
     * The engine's buffers reach the DOCUMENT while typing: the whole
     * buffered word (finalised syllables + in-flight preedit, issue #73)
     * sits in an ITfComposition started on the caret's empty selection
     * range, carried toward the app through one edit session per engine
     * transition, and underlined with GUID_PROP_ATTRIBUTE (the
     * display-attribute atom registered under HunminDisplayAttrGuid, the
     * attribute the provider above serves). A word boundary ends the
     * composition: the text fuses into the document as ordinary text and
     * the buffers empty. The floating overlay window has no remaining
     * preedit role (ruled 2026-10-20: the overlay keeps only the candidate
     * popup; the non-TSF-app fallback is dropped with the preedit path).
     *
     * If a host refuses ITfContextComposition (or the write session), the
     * commit mechanism degrades to the plain selection SetText the 0.2
     * adapter committed with — typed text still lands unstyled at the
     * boundary. The candidate popup keeps the caret-rect machinery. */
    class CompositionSession : public ITfEditSession {
      public:
        CompositionSession(ITfContext *ctx, CTextService *svc, TfClientId cid, const wchar_t *t, bool endAfter)
            : ref_(1), ctx_(ctx), svc_(svc), cid_(cid), t_(t), endAfter_(endAfter) {}
        STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
            if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
                *ppv = this;
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
        STDMETHOD(DoEditSession)(TfEditCookie ec) override {
            HRESULT hr = S_OK;
            if (!svc_->composition_) {
                /* First preedit key in this context: the empty selection
                 * range becomes the composition. A host that refuses the
                 * interface leaves composition_ null, and the caller's
                 * boundary path then falls back to the plain insert. */
                ITfContextComposition *cc = nullptr;
                if (SUCCEEDED(ctx_->QueryInterface(IID_ITfContextComposition, (void **)&cc)) && cc) {
                    TF_SELECTION sel;
                    ULONG fetched = 0;
                    if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) &&
                        fetched == 1 && sel.range) {
                        HRESULT hs = cc->StartComposition(
                            ec, sel.range, static_cast<ITfCompositionSink *>(svc_), &svc_->composition_);
                        if (SUCCEEDED(hs) && svc_->composition_) {
                            svc_->compCtx_ = ctx_;
                            ctx_->AddRef();
                        }
                        HMLOG("composition start hr=0x%lX comp=%p\n", (unsigned long)hs,
                              (void *)svc_->composition_);
                        sel.range->Release();
                    }
                    cc->Release();
                }
            }
            if (svc_->composition_) {
                ITfRange *range = nullptr;
                if (SUCCEEDED(svc_->composition_->GetRange(&range)) && range) {
                    /* An in-composition rewrite may CLEAR the range: an
                     * engine backspace can empty both buffers while the
                     * composition lives, and skipping the SetText left the
                     * stale preedit visible (measured at the seosi trace's
                     * backspace, issue #98; a real host shows the same
                     * ghost). Empty fuses nothing if the boundary ends it
                     * next. */
                    hr = range->SetText(ec, 0, t_, -1);
                    if (!endAfter_)
                        svc_->ApplyDisplayAttribute(ctx_, ec, range, false);
                    else
                        svc_->ApplyDisplayAttribute(ctx_, ec, range, true);
                    /* Caret: the COLLAPSE must never touch the composition's
                     * own range (escaping a collapsed-at-end range would
                     * make the next rewrite splice AFTER the text instead
                     * of replacing it). Write mode keeps the selection on
                     * the composition; end mode walks a clone to the end. */
                    if (endAfter_) {
                        ITfRange *caret = nullptr;
                        if (SUCCEEDED(range->Clone(&caret)) && caret) {
                            caret->Collapse(ec, TF_ANCHOR_END);
                            TF_SELECTION cs = {};
                            cs.range = caret;
                            cs.style.ase = TF_AE_END;
                            cs.style.fInterimChar = FALSE;
                            ctx_->SetSelection(ec, 1, &cs);
                            caret->Release();
                        }
                    } else {
                        TF_SELECTION ns = {};
                        ns.range = range;
                        ns.style.ase = TF_AE_END;
                        ns.style.fInterimChar = FALSE;
                        ctx_->SetSelection(ec, 1, &ns);
                    }
                    range->Release();
                }
                svc_->compText_ = t_;
                if (endAfter_) {
                    /* Issue #119: our own end is flagged so the sink cannot
                     * mistake it for a host-driven termination (measured:
                     * the sink has never fired for a service-called
                     * EndComposition — the wrap is protection, not assumed
                     * behaviour). */
                    svc_->endingSelf_ = true;
                    svc_->composition_->EndComposition(ec);
                    HMLOG("composition end text=%ls\n", t_);
                    svc_->composition_->Release();
                    svc_->composition_ = nullptr;
                    if (svc_->compCtx_) {
                        svc_->compCtx_->Release();
                        svc_->compCtx_ = nullptr;
                    }
                    svc_->compText_.clear();
                    svc_->endingSelf_ = false;
                }
            }
            return S_OK;
        }

      private:
        LONG ref_;
        ITfContext *ctx_;
        CTextService *svc_;
        TfClientId cid_;
        const wchar_t *t_;
        bool endAfter_;
    };

    /* The write half of the composition lifecycle: one edit session makes
     * the composition exist (if not yet) and shows the whole buffered word
     * in it, underlined. Called after every engine transition that a bound
     * key drives. */
    void CompositionWrite(ITfContext *pic) {
        if (!pic)
            return;
        std::wstring full = wordbuf_;
        if (ic_) {
            const char *pre = hunmin_ic_preedit(ic_);
            full += HunminUtf8ToWide(pre);
        }
        if (full.empty() && !composition_)
            return;
        CompositionSession *cs = new CompositionSession(pic, this, clientId_, full.c_str(), false);
        HRESULT hrC = S_OK;
        pic->RequestEditSession(clientId_, cs, TF_ES_SYNC | TF_ES_READWRITE, &hrC);
        cs->Release();
    }

    /* The end half: rewrites the composition range to the final text if it
     * differs, drops the display attribute, and calls EndComposition — the
     * composed text fuses into the document as ordinary text. Runs in the
     * composition's own context when called after a context switch. */
    void CompositionEnd(ITfContext *pic, const std::wstring &text) {
        ITfContext *ctx = (composition_ && compCtx_) ? compCtx_ : pic;
        if (!ctx)
            return;
        CompositionSession *cs = new CompositionSession(ctx, this, clientId_, text.c_str(), true);
        HRESULT hrE = S_OK;
        ctx->RequestEditSession(clientId_, cs, TF_ES_SYNC | TF_ES_READWRITE, &hrE);
        cs->Release();
    }

    /* A context/focus switch ends the live composition in its own context
     * (the word stays there as committed text); the buffers drain so the
     * next context starts clean — the old 0.2 boundary-drain contract,
     * pointed at the composition instead of the overlay. */
    void DetachComposition() {
        if (!composition_ || !compCtx_)
            return;
        const char *flushed = ic_ ? hunmin_ic_flush(ic_) : "";
        if (flushed && flushed[0]) {
            int flen = MultiByteToWideChar(CP_UTF8, 0, flushed, -1, nullptr, 0);
            if (flen > 1) {
                size_t old = wordbuf_.size();
                wordbuf_.resize(old + flen - 1);
                MultiByteToWideChar(CP_UTF8, 0, flushed, -1, &wordbuf_[old], flen);
            }
        }
        std::wstring text = wordbuf_;
        wordbuf_.clear();
        CompositionEnd(compCtx_, text);
    }

    /* Registers the composition-attribute GUID once and applies (or clears)
     * it over the composition range (msctf: providing display attributes —
     * the property value is the TfGuidAtom the app's attribute manager
     * resolves through our provider). Without a category manager (COM not
     * initialised in the process) the composition text simply renders
     * unstyled; logged, so the cause stays findable. */
    void ApplyDisplayAttribute(ITfContext *ctx, TfEditCookie ec, ITfRange *range, bool clear) {
        if (!ctx || !range)
            return;
        if (displayAttrAtom_ == 0) {
            ITfCategoryMgr *cm = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_ITfCategoryMgr, (void **)&cm)) &&
                cm) {
                HRESULT hrG = cm->RegisterGUID(HunminDisplayAttrGuid, &displayAttrAtom_);
                cm->Release();
                if (FAILED(hrG))
                    HMLOG("display attr RegisterGUID hr=0x%lX\n", (unsigned long)hrG);
            }
        }
        if (displayAttrAtom_ == 0)
            return; /* no styling channel: plain composition text */
        ITfProperty *prop = nullptr;
        if (SUCCEEDED(ctx->GetProperty(GUID_PROP_ATTRIBUTE, &prop)) && prop) {
            if (clear) {
                prop->Clear(ec, range);
            } else {
                VARIANT v;
                VariantInit(&v);
                v.vt = VT_I4;
                v.lVal = (LONG)displayAttrAtom_;
                prop->SetValue(ec, range, &v);
                VariantClear(&v);
            }
            prop->Release();
        }
    }

    LONG ref_;
    ITfThreadMgr *threadMgr_;
    TfClientId clientId_;
    DWORD sinkCookie_;
    hunmin_ctx *ctx_;
    hunmin_ic *ic_;
    bool hangulMode_;
    /* The resolved layout pair (issue #66): hangulMode_ switches between
     * these, config-read at Activate and re-read by the settings dialog
     * (#71). Defaults match the fcitx5 addon config. */
    std::string hangulId_, latinId_;
    /* Mode-toggle bindings (maintainer instruction: the Han/Eng key is the
     * default; additional keys configure through the store). Always carries
     * the bare physical Han/Eng key — the platform alternate that survives
     * every rebinding, the rule Android's KeyAction.HANGUL_ENG encodes. */
    std::vector<HunminKeyCombo> toggleKeys_;
    /* The candidate-window key (issue #67): ONE binding, default the bare
     * physical Hanja key, re-read by ApplyConfigStore with everything else.
     * F9 is deliberately NOT in the default set any more (maintainer
     * instruction), but a stored binding can name it. */
    HunminKeyCombo hanjaCombo_;
    RECT preeditRect_;
    bool hasPreeditRect_;
    ITfContext *lastCtx_;

    /* Native composition preedit (issue #98): the ITfComposition the
     * engine's in-flight text lives in, the context it belongs to, and the
     * text currently shown in it (the whole buffered word — finalized
     * syllables plus the in-flight preedit, issue #73's content — in the
     * document flow, underlined through GUID_PROP_ATTRIBUTE). */
    ITfComposition *composition_;
    ITfContext *compCtx_;
    std::wstring compText_;
    TfGuidAtom displayAttrAtom_;

    /* Finalised syllables held for insertion at the next boundary key
     * (issue #73); drained destructively by hunmin_ic_commit(). */
    std::wstring wordbuf_;

    bool interimForce_ = false;
    bool legacyMode_ = false; /* the mode flag; delivery_ carries the behaviour */
    bool endingSelf_ = false; /* between EndComposition entry and return */

    /* ---- The delivery seam (issue #119) ----
     *
     * The COMMON key handling (OnTestKeyDown/OnKeyDown) classifies the key
     * and feeds the engine; it never asks which mode it is in. What the
     * two modes DO with the engine's answer is the seam below:
     *
     * - NativeDelivery — the #98 composition path: the word rides a live
     *   ITfComposition (inline start/update/end per edit session). Modern
     *   hosts keep compositions alive (Win11 Notepad, Edge — measured).
     * - InterimDelivery — the #119 interim path for classic (default-sink)
     *   hosts: NO composition (nothing to terminate), the word is document
     *   text, rewritten per key through the control's own EM_* protocol.
     *   The doc model: [finalised word][in-flight span]; the span
     *   [preStart_, preStart_+preLen_) is the ONLY text the mode rewrites,
     *   always through ONE select + ONE EM_REPLACESEL (ReplaceSpan) —
     *   piecewise span edits accumulate (measured: 1+1+1+2+2+2 chars for
     *   7 keys; jamotong_ime's manual §8.2, artifact ㄱ가간...).
     *   NO overlay window (maintainer instruction 2026-10-20). The styled
     *   composition underline is NOT reproduced on this path (fInterimChar
     *   selection: SetSelection E_INVALIDARG measured on both host
     *   classes) — plain-text preedit is the accepted shape
     *   (docs/TSF-INTERIM-INPUT-SPIKE.md §4-§6).
     *
     * A host-driven OnCompositionTerminated that fires MID-WORD flips the
     * service native -> interim for its life (the detection measurement:
     * classic EDIT / msftedit RICHEDIT50W terminate every key; modern
     * hosts terminate only at real boundaries). HUNMIN_TSF_INTERIM=1
     * overrides detection (the old spike's escape hatch). */
    struct Delivery {
        virtual ~Delivery() = default;
        /* The engine consumed a key (or a host termination fused the word):
         * the word state moved, the document must follow. */
        virtual void Consumed(ITfContext *pic) = 0;
        /* A word boundary: declined key, mode toggle, chord, passage key. */
        virtual void Boundary(ITfContext *pic) = 0;
        /* The Hanja candidate lookup anchor. */
        virtual std::string CandidateAnchor() = 0;
        /* A picked candidate commits as `replacement` (the fallback-aware
         * text the common side computed from the anchor). false = refused:
         * the buffers stay intact, nothing is delivered. */
        virtual bool CandidateCommit(ITfContext *pic, const std::wstring &replacement) = 0;
        /* Settle a posted doc write (the interim deferral; the native path
         * is synchronous — a no-op). */
        virtual void FlushPending() = 0;
    };

    struct NativeDelivery : Delivery {
        CTextService *svc;
        explicit NativeDelivery(CTextService *s) : svc(s) {}
        void Consumed(ITfContext *pic) override {
            svc->CompositionWrite(pic);
        }
        void Boundary(ITfContext *pic) override {
            svc->InsertBufferedWord(pic);
        }
        std::string CandidateAnchor() override {
            /* The whole buffered word: accumulated finalised syllables plus
             * the in-flight preedit — the multi-syllable lookup the
             * dictionary tables are keyed by (krdict words, idioms).
             * fcitx5 anchors to its preedit only because it commits each
             * syllable immediately. */
            std::string s = HunminWideToUtf8(svc->wordbuf_);
            if (svc->ic_) {
                const char *pre = hunmin_ic_preedit(svc->ic_);
                if (pre)
                    s += pre;
            }
            return s;
        }
        bool CandidateCommit(ITfContext *pic, const std::wstring &replacement) override {
            if (svc->ic_)
                hunmin_ic_reset(svc->ic_); /* drop the composition the list anchored to */
            svc->wordbuf_ = replacement;
            svc->InsertBufferedWord(pic);
            return true;
        }
        void FlushPending() override {}
    };

    /* The interim mode's own state and writes. The deferral machinery
     * (measured defect): the EM_* writes must not run INSIDE a TSF
     * callback — the first inline write fired a re-entrant
     * ITfThreadMgrEventSink::OnSetFocus(NULL) storm (msftedit's TSF store
     * settling the out-of-band selection change) and every keystroke sent
     * while it ran was lost before reaching OnTestKeyDown. So the write is
     * POSTED and runs on the app's UI thread from a thread timer
     * (SetTimer with a NULL hwnd — fires when that thread pumps, i.e.
     * after the current key event has fully returned), or at the ENTRY of
     * the next key event, whichever comes first. One slot suffices: the
     * entry flush keeps it empty at every post. */
    struct InterimDelivery : Delivery {
        CTextService *svc;
        explicit InterimDelivery(CTextService *s) : svc(s) {}

        HWND interimHwnd_ = nullptr; /* the edit control (resolved once) */
        LONG preStart_ = 0; /* doc offset where the span begins */
        size_t preLen_ = 0; /* the span's current char count */
        std::wstring pendingDeadCopy_; /* a fused copy awaiting deletion */
        enum PendingWrite { kPendingNone,
                            kPendingWrite,
                            kPendingDrain };
        PendingWrite pendingWriteKind_ = kPendingNone;
        static InterimDelivery *s_writer; /* one TIP instance per thread */
        static const UINT_PTR kWriteTimerId = 1;

        void Consumed(ITfContext *pic) override {
            (void)pic;
            Post(kPendingWrite);
        }
        void Boundary(ITfContext *pic) override {
            (void)pic;
            Post(kPendingDrain);
        }
        void FlushPending() override {
            Dispatch();
        }
        /* The one-char ruling (maintainer, 2026-10-20: "in interim mode,
         * only one char conversion should be supported"): the finalised
         * syllables are document text outside the span the mode rewrites,
         * so the anchor is the IN-FLIGHT preedit only, and a candidate
         * that is not exactly one character is refused whole — the
         * whole-word anchor's multi-char candidates cannot commit without
         * corrupting the word (measured: 안녕 + 安寧 → 안安寧). */
        std::string CandidateAnchor() override {
            const char *pre = svc->ic_ ? hunmin_ic_preedit(svc->ic_) : "";
            return std::string(pre ? pre : "");
        }
        bool CandidateCommit(ITfContext *pic, const std::wstring &replacement) override {
            (void)pic;
            if (replacement.size() != 1 || preLen_ == 0)
                return false; /* refused: the word stays intact */
            if (svc->ic_)
                hunmin_ic_reset(svc->ic_); /* drop the syllable the list anchored to */
            svc->wordbuf_ = replacement; /* the drain rewrites the span to it */
            Post(kPendingDrain);
            return true;
        }

        void Post(PendingWrite kind) {
            pendingWriteKind_ = kind;
            s_writer = this;
            SetTimer(nullptr, kWriteTimerId, 10, WriteTimerProc);
        }
        /* Runs the queued doc write. Called from the timer and from the
         * key events' entry (the mock tests drive it directly — they never
         * pump). */
        void Dispatch() {
            PendingWrite kind = pendingWriteKind_;
            pendingWriteKind_ = kPendingNone;
            KillTimer(nullptr, kWriteTimerId);
            if (kind == kPendingWrite)
                InterimWrite();
            else if (kind == kPendingDrain)
                InterimDrain();
        }
        static void CALLBACK WriteTimerProc(HWND, UINT, UINT_PTR, DWORD) {
            if (s_writer)
                s_writer->Dispatch();
        }

        /* The edit control the writes go through: the key events arrive on
         * the app's input thread, so GetFocus() is it — default-sink
         * contexts refuse IID_ITfContextOwner (measured). */
        HWND ResolveControl(ITfContext *pic) {
            if (interimHwnd_)
                return interimHwnd_;
            HWND hwnd = GetFocus();
            if (!hwnd && pic) {
                ITfContextOwner *owner = nullptr;
                if (SUCCEEDED(pic->QueryInterface(IID_ITfContextOwner, (void **)&owner)) && owner) {
                    owner->GetWnd(&hwnd);
                    owner->Release();
                }
            }
            interimHwnd_ = hwnd;
            HMLOG("interim: control hwnd=%p\n", (void *)hwnd);
            return hwnd;
        }

        /* The ONE write primitive: the doc span becomes exactly `text` —
         * one select, one EM_REPLACESEL. */
        void ReplaceSpan(HWND hwnd, const std::wstring &text) {
            EmSelect(hwnd, preStart_, preStart_ + (LONG)preLen_);
            EmReplace(hwnd, text.c_str());
            preLen_ = text.size();
            HMLOG("interim: span now %zu chars at %ld (caret=%ld doclen=%d focused=%d)\n",
                  text.size(), preStart_, EmCaret(hwnd),
                  GetWindowTextLengthW(hwnd), GetFocus() == hwnd);
        }

        /* A host termination fused the composition's text into the doc:
         * delete that dead copy and re-anchor the span where it sat. The
         * caret may still be unsettled when the termination fires
         * (measured both states); an unremovable copy is dropped and the
         * fresh-span anchor recovers. */
        void CaptureDeadCopy(const std::wstring &copy) {
            pendingDeadCopy_ = copy;
            preStart_ = 0;
            preLen_ = 0;
            Post(kPendingWrite);
        }
        void RemoveDeadCopy(HWND hwnd) {
            if (pendingDeadCopy_.empty())
                return;
            LONG caret = EmCaret(hwnd);
            LONG span = (LONG)pendingDeadCopy_.size();
            if (caret >= span) {
                EmSelect(hwnd, caret - span, caret);
                EmReplace(hwnd, L"");
                preStart_ = caret - span;
                HMLOG("interim: dead copy removed (%ld chars, preStart=%ld)\n",
                      span, preStart_);
            }
            pendingDeadCopy_.clear();
        }

        /* The finalised syllables closed by the engine since the last
         * write, taken out of the buffer. */
        std::wstring TakeFinalisedWord() {
            std::wstring f;
            f.swap(svc->wordbuf_);
            return f;
        }

        /* The interim consumed path: the newly finalised syllables land in
         * the document AHEAD of the in-flight span, then the span becomes
         * the engine's current preedit. */
        void InterimWrite() {
            HWND hwnd = ResolveControl(nullptr);
            if (!hwnd)
                return;
            RemoveDeadCopy(hwnd);
            /* A fresh word (no span in the doc) anchors at the live caret:
             * the app may have inserted passthrough text (the boundary
             * space) since the previous word closed. */
            if (preLen_ == 0)
                preStart_ = EmCaret(hwnd);
            std::wstring finalised = TakeFinalisedWord();
            if (!finalised.empty()) {
                EmSelect(hwnd, preStart_, preStart_);
                EmReplace(hwnd, finalised.c_str());
                preStart_ += (LONG)finalised.size();
                HMLOG("interim: appended %zu finalised chars\n", finalised.size());
            }
            std::wstring pre = HunminUtf8ToWide(svc->ic_ ? hunmin_ic_preedit(svc->ic_) : "");
            if (!pre.empty() || preLen_ > 0)
                ReplaceSpan(hwnd, pre);
        }

        /* The interim boundary drain: the in-flight span becomes the word
         * (never an insert at the anchor — the word is ALREADY document
         * text when the span is closed), then the bookkeeping closes and
         * the engine state drops. The drain runs from the TIMER, i.e.
         * AFTER the app inserted its boundary space and parked its own
         * caret: measured live, the span's EmSelect left the app's caret
         * INSIDE the word ahead of the space, so the following keys
         * landed before the space and the backspace deleted the wrong
         * char. The app's caret is not ours to move — saved and given
         * back. */
        void InterimDrain() {
            HWND hwnd = ResolveControl(nullptr);
            std::wstring word;
            word.swap(svc->wordbuf_);
            LONG userCaret = hwnd ? EmCaret(hwnd) : 0;
            if (hwnd && !word.empty() && preLen_ > 0) {
                ReplaceSpan(hwnd, word);
                preStart_ += (LONG)word.size();
            }
            preLen_ = 0;
            if (svc->ic_)
                hunmin_ic_reset(svc->ic_);
            if (hwnd)
                EmSelect(hwnd, userCaret, userCaret);
            HMLOG("interim boundary: word len=%zu preStart=%ld caret=%ld\n",
                  word.size(), preStart_, userCaret);
        }

        /* The class-aware EM primitives: EM_EXSETSEL/CHARRANGE for the
         * msftedit richedit, EM_SETSEL for plain EDIT. */
        enum { kEmRich = 0,
               kEmPlain = 1 };
        int ControlKind(HWND hwnd) {
            wchar_t cls[64] = {0};
            GetClassNameW(hwnd, cls, 64);
            return (wcsstr(cls, L"RICHEDIT") != nullptr) ? kEmRich : kEmPlain;
        }
        void EmSelect(HWND hwnd, LONG start, LONG end) {
            if (ControlKind(hwnd) == kEmRich) {
                CHARRANGE cr;
                cr.cpMin = start;
                cr.cpMax = end;
                SendMessageW(hwnd, EM_EXSETSEL, 0, (LPARAM)&cr);
            } else {
                SendMessageW(hwnd, EM_SETSEL, (WPARAM)start, (LPARAM)end);
            }
        }
        LONG EmCaret(HWND hwnd) {
            if (ControlKind(hwnd) == kEmRich) {
                CHARRANGE cr = {0, 0};
                SendMessageW(hwnd, EM_EXGETSEL, 0, (LPARAM)&cr);
                return (LONG)cr.cpMax;
            }
            DWORD a = 0, b = 0;
            SendMessageW(hwnd, EM_GETSEL, (WPARAM)&a, (LPARAM)&b);
            return (LONG)b;
        }
        void EmReplace(HWND hwnd, const wchar_t *t) {
            SendMessageW(hwnd, EM_REPLACESEL, FALSE, (LPARAM)t);
        }
    };
    NativeDelivery nativeDelivery_{this};
    InterimDelivery interimDelivery_{this};
    Delivery *delivery_ = &nativeDelivery_;

    void FlushPendingDocWrites() {
        delivery_->FlushPending();
    }

    /* F9 Hanja candidates (issue #67). The window anchors to the WHOLE
     * buffered word — accumulated finalised syllables plus the in-flight
     * preedit — which is the multi-syllable lookup the dictionary tables
     * are keyed by (krdict words, idioms); fcitx5 anchors to its preedit
     * only because it commits each syllable immediately. */
    bool candActive_;
    int candCount_;
    int candCursor_; /* global 0-based cursor */
    int candTop_; /* first index of the visible 10-row page */
    std::string candSource_; /* the Hangul the list was looked up for */
    bool candFallback_ = false; /* the list came from the last-syllable fallback (issue #100) */
    HunminCandWindow candWnd_;
    /* ctfutb.h ITfLangBarItemButton state: true between AddItem and
     * RemoveItem; the manager owns exactly one extra reference. */
    bool langbar_registered_ = false;
    /* The ITfLangBarItemSink the language bar installed onto the item
     * (ITfSource::AdviseSink at AddItem — language-bar.md; released with
     * the item at Deactivate). OnUpdate(TF_LBI_ICON | TF_LBI_TEXT) through
     * it re-queries GetIcon/GetText, so the 한/A badge refreshes without a
     * host restart. */
    ITfLangBarItemSink *itemSink_ = nullptr;
    DWORD itemSinkCookie_ = 0;

    void UpdateConversionCompartment() {
        /* The compartment the language bar reads to draw its 中/英 badge
         * (GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
         * TF_CONVERSIONMODE_NATIVE — measured: the state the legacy bar
         * and the tray use). Setting it on every mode boundary is how a
         * TIP's own mode is reflected by the system surfaces. */
        if (!threadMgr_) {
            return;
        }
        ITfCompartmentMgr *cm = nullptr;
        if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfCompartmentMgr, (void **)&cm)) && cm) {
            ITfCompartment *c = nullptr;
            if (SUCCEEDED(cm->GetCompartment(
                    GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, &c)) &&
                c) {
                VARIANT v;
                VariantInit(&v);
                v.vt = VT_I4;
                v.lVal = hangulMode_ ? (LONG)TF_CONVERSIONMODE_NATIVE : 0L;
                c->SetValue(0, &v);
                VariantClear(&v);
                c->Release();
            } else {
                FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
                if (dbg) {
                    fprintf(dbg, " conversion compartment GONE\n");
                    fflush(dbg);
                    fclose(dbg);
                }
            }
            cm->Release();
        }
    }

    // Helper used both for declined/pass-through keys to flush the pending
    // composition exactly once, deleting the already-present composed text.
    class FlushSession : public ITfEditSession {
      public:
        FlushSession(ITfContext *ctx, CTextService *svc, TfClientId cid, const wchar_t *t)
            : ref_(1), ctx_(ctx), svc_(svc), cid_(cid), t_(t) {}
        STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
            if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
                *ppv = this;
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
        STDMETHOD(DoEditSession)(TfEditCookie ec) override {
            TF_SELECTION sel;
            ULONG fetched = 0;
            HRESULT hr = E_NOINTERFACE;
            if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) && fetched == 1 &&
                sel.range) {
                sel.range->SetText(ec, 0, t_, -1);
                sel.range->Collapse(ec, TF_ANCHOR_END);
                TF_SELECTION ns = sel;
                ctx_->SetSelection(ec, 1, &ns);
                sel.range->Release();
                hr = S_OK;
            }
            HMLOG("flush: insert hr=0x%lX\n", (unsigned long)hr);
            return S_OK;
        }

      private:
        LONG ref_;
        ITfContext *ctx_;
        CTextService *svc_;
        TfClientId cid_;
        const wchar_t *t_;
    };

    /* The insert half of the composition boundary: the whole buffered word
     * reaches the document in one edit session, exactly once, and the
     * buffer empties. With a live composition the word is ALREADY document
     * text (the composition range holds it, issue #98): the session ends
     * the composition, fusing the word in unstyled. Only a host that
     * refused ITfContextComposition falls back to the plain selection
     * SetText the 0.2 adapter committed with. */
    /* The NATIVE boundary insert (the native half of the delivery seam):
     * with a live composition the word is ALREADY document text (the
     * composition range holds it) — the session ends the composition,
     * fusing the word in unstyled. Only a host that refused
     * ITfContextComposition falls back to the plain selection SetText the
     * 0.2 adapter committed with. */
    void InsertBufferedWord(ITfContext *pic) {
        if (wordbuf_.empty())
            return;
        if (composition_) {
            CompositionEnd(pic, wordbuf_);
        } else {
            FlushSession *fs = new FlushSession(pic, this, clientId_, wordbuf_.c_str());
            HRESULT hrFlush = S_OK;
            if (pic)
                pic->RequestEditSession(clientId_, fs, TF_ES_SYNC | TF_ES_READWRITE, &hrFlush);
            fs->Release();
        }
        wordbuf_.clear();
    }

    /* One composition-boundary drain shared by the declined key path and the
     * mode toggle (issue #66): the in-flight syllable is flushed into the
     * buffered word, and the whole word is inserted exactly once before the
     * boundary key (or mode switch) proceeds. */
    void BoundaryFlush(ITfContext *pic, FILE *dbg) {
        const char *flushed = ic_ ? hunmin_ic_flush(ic_) : "";
        if (flushed && flushed[0]) {
            int flen = MultiByteToWideChar(CP_UTF8, 0, flushed, -1, nullptr, 0);
            if (flen > 1) {
                size_t old = wordbuf_.size();
                /* flen counts the NUL terminator; text only. */
                wordbuf_.resize(old + flen - 1);
                MultiByteToWideChar(CP_UTF8, 0, flushed, -1, &wordbuf_[old], flen);
            }
        }
        if (dbg) {
            fprintf(dbg, " boundary, word len=%zu preedit=%s\n", wordbuf_.size(),
                    ic_ ? hunmin_ic_preedit(ic_) : "");
            fflush(dbg);
        }
        delivery_->Boundary(pic);
    }

    /* --- F9 Hanja candidate window (issue #67) ---
     *
     * The engine serves the list (hunmin_suggest_count/text/detail/kind over
     * the hanja/idiom/krdict tables, keyed by the WHOLE Hangul word); the
     * adapter supplies everything else: the lookup text is the buffered
     * word, the keyboard flow mirrors fcitx5 (digits pick, Up/Down moves,
     * PageUp/PageDown pages, Return commits the cursor, Esc closes, any
     * other key closes and falls through so typing continues) and the
     * pop-up itself draws the rows.
     *
     * Page cap: kMaxCandidatePageSize, the value fcitx5's label ceiling
     * forces (ruled by the maintainer for issue #45 — engine.cpp:471-486:
     * 10 candidates survive, 11 abort). The TSF popup owns its own drawing
     * rather than fcitx5's label table, so it could show more rows than
     * that, but the SAME ruled number is kept: a word candidate list the
     * digits can reach is 1..0 per page, and 67-row popups measured ~2000px
     * tall on fcitx5's host. PageUp/PageDown are what the rest of the list
     * is behind. */
    static const int kMaxCandidatePageSize = 10;

    /* The Hangul the candidate window anchors to: accumulated finalised
     * syllables plus the in-flight preedit. Valid only while the window is
     * open — opening may not flush anything (fcitx5's openCandidates rule:
     * hunmin_ic_flush would commit the anchor out from under the list). */

    /* The last UTF-8 character of s, by a forward walk over the code points
     * (issue #100's "last syllable": in a composed word each character is a
     * syllable; for an interrupted composition the last character may be a
     * lone jamo, and that is the syllable the user was typing). Returns the
     * empty string only when s itself is empty. */
    static std::string LastSyllableOf(const std::string &s) {
        size_t start = 0;
        for (size_t i = 0; i < s.size();) {
            start = i;
            unsigned char b = (unsigned char)s[i];
            i += (b >= 0xF0) ? 4 : (b >= 0xE0) ? 3
                               : (b >= 0xC0)   ? 2
                                               : 1;
        }
        return s.substr(start);
    }

    /* The candidate-count decision BOTH phases share (test and down are one
     * decision): the whole buffered word is looked up first (the anchor the
     * dictionary tables are keyed by), and when that yields 0 while the
     * word has more than one syllable, the LAST syllable is looked up
     * instead (issue #100). The returned pair is (count, fallback): count 0
     * means the key belongs to the client in both phases; a fallback count
     * > 0 opens a syllable-anchored list (candFallback_). */
    std::pair<int, bool> HanjaListCount() {
        std::string word = delivery_->CandidateAnchor();
        if (ctx_ && !word.empty()) {
            int n = hunmin_suggest_count(ctx_, word.c_str());
            if (n > 0)
                return {n, false};
            std::string last = LastSyllableOf(word);
            if (last != word) { /* more than one character => more than one syllable */
                n = hunmin_suggest_count(ctx_, last.c_str());
                if (n > 0)
                    return {n, true};
            }
        }
        return {0, false};
    }

    bool OpenCandidates() {
        std::pair<int, bool> look = HanjaListCount();
        CountToState(look.first, look.second);
        return candActive_;
    }

    /* Recompute the list state from a (count, fallback) lookup answer. */
    void CountToState(int n, bool fallback) {
        candFallback_ = fallback;
        if (fallback) {
            candSource_ = LastSyllableOf(delivery_->CandidateAnchor());
        } else {
            candSource_ = delivery_->CandidateAnchor();
        }
        candCount_ = n;
        candActive_ = candCount_ > 0;
        candCursor_ = 0;
        candTop_ = 0;
    }

    void CloseCandidateWindow() {
        candActive_ = false;
        candCount_ = 0;
        candCursor_ = 0;
        candTop_ = 0;
        candSource_.clear();
        candWnd_.Hide();
    }

    void PageFollowCursor() {
        candTop_ = (candCursor_ / kMaxCandidatePageSize) * kMaxCandidatePageSize;
    }

    void PageStep(int dir) {
        int lastTop = candCount_ > 0
                          ? ((candCount_ - 1) / kMaxCandidatePageSize) * kMaxCandidatePageSize
                          : 0;
        int top = candTop_ + dir * kMaxCandidatePageSize;
        if (top < 0)
            top = 0;
        if (top > lastTop)
            top = lastTop;
        candTop_ = top;
        /* The cursor moves with the page when it would be left off it. */
        if (candCursor_ < candTop_)
            candCursor_ = candTop_;
        else if (candCursor_ >= candTop_ + kMaxCandidatePageSize)
            candCursor_ = candTop_ + kMaxCandidatePageSize - 1 < candCount_
                              ? candTop_ + kMaxCandidatePageSize - 1
                              : candCount_ - 1;
    }

    /* fcitx5 semantics (engine.cpp:160-176): choosing a candidate drops the
     * composition it was anchored to, then commits the Hanja in its place —
     * no surrounding-text surgery anywhere. The buffered Hangul has not
     * been typed into the document yet, so nothing needs removing: the
     * buffer itself is replaced by the chosen text, which is then inserted
     * through the normal boundary path. */
    void CommitCandidate(ITfContext *pic, int idx) {
        if (idx < 0 || idx >= candCount_ || !ctx_)
            return;
        /* Copy first: the pointer is valid only until the next
         * hunmin_suggest_* call on ctx. */
        std::string hanja(hunmin_suggest_text(ctx_, idx));
        if (hanja.empty())
            return;
        /* Capture the word anchor BEFORE the commit: on a last-syllable
         * fallback list (issue #100) the buffered word's own state is what
         * the replacement is computed from, and the commit resets the
         * in-flight preedit. The mode seam decides how (and whether) the
         * replacement text is delivered. */
        std::string replacement = hanja;
        if (candFallback_) {
            std::string word = delivery_->CandidateAnchor();
            std::string last = LastSyllableOf(word);
            /* Only last syllable replaced: the word prefix stays Hangul. */
            replacement = word.substr(0, word.size() - last.size()) + hanja;
        }
        std::wstring rep = HunminUtf8ToWide(replacement.c_str());
        bool committed = delivery_->CandidateCommit(pic, rep);
        CloseCandidateWindow();
        (void)committed;
    }

    void UpdateCandidateWindow(ITfContext *pic) {
        if (!candActive_ || !pic || !ic_) {
            candWnd_.Hide();
            return;
        }
        RECT rc;
        bool haveCaret = false;
        GUITHREADINFO gti;
        if (!AcquireCaretRect(pic, &rc, &haveCaret, &gti)) {
            /* No metrics, no caret: same hide path the overlay takes. The
             * candidate state stays open — a host that reports no window
             * geometry today may report it after the next focus, and the
             * next F9/arrow key redraws. */
            candWnd_.Hide();
            return;
        }
        HWND fontHost = haveCaret ? (gti.hwndFocus ? gti.hwndFocus : gti.hwndCaret) : nullptr;
        std::vector<std::wstring> lines;
        int end = candCount_ < candTop_ + kMaxCandidatePageSize
                      ? candCount_
                      : candTop_ + kMaxCandidatePageSize;
        for (int i = candTop_; i < end; ++i) {
            std::wstring text(HunminUtf8ToWide(hunmin_suggest_text(ctx_, i)));
            wchar_t label = (wchar_t)(((i - candTop_) == 9) ? '0' : ('1' + (i - candTop_)));
            std::wstring line = std::wstring(i == candCursor_ ? L">> " : L"   ") + label +
                                L". " + text;
            lines.push_back(line);
        }
        candWnd_.Show(rc, fontHost, lines);
    }

    /* The caret rect shared by the overlay and the candidate popup: one
     * read-only edit session against the selection (GetTextExt), falling
     * back to the thread's GUITHREADINFO caret. Screen coordinates out.
     * False means the host reported nothing drawable — the popup/overlay
     * hide path. */
    bool AcquireCaretRect(ITfContext *pic, RECT *rcOut, bool *haveCaretOut,
                          GUITHREADINFO *gtiOut) {
        hasPreeditRect_ = false;
        class RectSession : public ITfEditSession {
          public:
            RectSession(ITfContext *ctx, CTextService *svc) : ref_(1), ctx_(ctx), svc_(svc) {}
            STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
                if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
                    *ppv = this;
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
            STDMETHOD(DoEditSession)(TfEditCookie ec) override {
                ITfContextView *view = nullptr;
                if (SUCCEEDED(ctx_->GetActiveView(&view)) && view) {
                    TF_SELECTION sel;
                    ULONG fetched = 0;
                    if (SUCCEEDED(ctx_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) && fetched == 1 &&
                        sel.range) {
                        RECT rc;
                        BOOL clipped = FALSE;
                        if (SUCCEEDED(view->GetTextExt(ec, sel.range, &rc, &clipped))) {
                            svc_->preeditRect_ = rc;
                            svc_->hasPreeditRect_ = true;
                        }
                        sel.range->Release();
                    }
                    view->Release();
                }
                return S_OK;
            }

          private:
            LONG ref_;
            ITfContext *ctx_;
            CTextService *svc_;
        };
        RectSession *rs = new RectSession(pic, this);
        HRESULT hrRect = S_OK;
        pic->RequestEditSession(clientId_, rs, TF_ES_SYNC | TF_ES_READ, &hrRect);
        rs->Release();

        GUITHREADINFO gti = {};
        gti.cbSize = sizeof(gti);
        bool haveCaret = GetGUIThreadInfo(GetCurrentThreadId(), &gti) != FALSE && gti.hwndCaret &&
                         (gti.rcCaret.bottom > gti.rcCaret.top);
        *haveCaretOut = haveCaret;
        if (gtiOut)
            *gtiOut = gti;
        if (!hasPreeditRect_ && !haveCaret)
            return false;
        RECT rc = hasPreeditRect_ ? preeditRect_ : gti.rcCaret;
        if (haveCaret && gti.hwndCaret) {
            POINT topLeft = {gti.rcCaret.left, gti.rcCaret.top};
            POINT bottomRight = {gti.rcCaret.right, gti.rcCaret.bottom};
            ClientToScreen(gti.hwndCaret, &topLeft);
            ClientToScreen(gti.hwndCaret, &bottomRight);
            rc.left = topLeft.x;
            rc.top = topLeft.y;
            rc.right = bottomRight.x;
            rc.bottom = bottomRight.y;
        }
        *rcOut = rc;
        return true;
    }
};
CTextService::InterimDelivery *CTextService::InterimDelivery::s_writer = nullptr;

extern "C" HINSTANCE g_hunmin_hinst;
/* placeholder */

/* Issue #71 bridge, defined in the TU that owns CTextService: settings.cpp
 * (ITfFnConfigure::Show) hands the LIVE service back through a plain void*
 * because the two TUs' headers deliberately do not entangle. */
void hunmin_tsf_apply_config_store(void *svc) {
    if (!svc)
        return;
    static_cast<CTextService *>(svc)->ApplyConfigStore();
}
extern "C" BOOL DllRegisterServer() {
    // logging
    FILE *dbg = tsf_log_open("hunmin-reg-debug.log", "w");
    wchar_t guidStr[64];
    StringFromGUID2(CLSID_HunminTSF, guidStr, 64);
    if (dbg) {
        fwprintf(dbg, L"guid=%s\n", guidStr);
        fflush(dbg);
    }

    wchar_t dllPath[MAX_PATH];
    HINSTANCE hinst = g_hunmin_hinst;
    if (!hinst)
        hinst = GetModuleHandleW(L"hunmin-tsf.dll");
    GetModuleFileNameW(hinst, dllPath, MAX_PATH);
    if (dbg) {
        fwprintf(dbg, L"module=%s\n", dllPath);
        fflush(dbg);
    }

    // COM CLSID key registration
    HKEY hKey = nullptr;
    HKEY hSub = nullptr;
    wchar_t clsidPath[128];
    swprintf(clsidPath, 128, L"CLSID\\%s", guidStr);

    LSTATUS st = RegCreateKeyExW(HKEY_CLASSES_ROOT, clsidPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                                 &hKey, nullptr);
    if (st != ERROR_SUCCESS)
        return FALSE;

    wchar_t inprocPath[256];
    swprintf(inprocPath, 256, L"%s\\InprocServer32", clsidPath);
    st = RegCreateKeyExW(HKEY_CLASSES_ROOT, inprocPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hSub,
                         nullptr);
    if (st != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return FALSE;
    }

    RegSetValueExW(hSub, nullptr, 0, REG_SZ, (const BYTE *)dllPath, (DWORD)((wcslen(dllPath) + 1) * sizeof(wchar_t)));
    const wchar_t *model = L"Apartment";
    RegSetValueExW(hSub, L"ThreadingModel", 0, REG_SZ, (const BYTE *)model,
                   (DWORD)((wcslen(model) + 1) * sizeof(wchar_t)));

    RegCloseKey(hSub);
    RegCloseKey(hKey);

    // TSF TIP category registration (so it is enumerated in input settings).
    HKEY hTIP = nullptr;
    wchar_t tipPath[256];
    swprintf(tipPath, 256, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s", guidStr);
    LSTATUS tipSt = RegCreateKeyExW(HKEY_LOCAL_MACHINE, tipPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE,
                                    nullptr, &hTIP, nullptr);
    if (tipSt == ERROR_SUCCESS) {
        const wchar_t *tipName = L"Hunmin";
        RegSetValueExW(hTIP, nullptr, 0, REG_SZ, (const BYTE *)tipName,
                       (DWORD)((wcslen(tipName) + 1) * sizeof(wchar_t)));
        RegCloseKey(hTIP);
    }
    CoInitialize(nullptr);
    ITfInputProcessorProfiles *pProfile = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfInputProcessorProfiles, (LPVOID *)&pProfile);
    if (dbg) {
        fwprintf(dbg, L"pProfile CoCreate hr=0x%lX\n", (unsigned long)hr);
        fflush(dbg);
    }
    if (SUCCEEDED(hr) && pProfile) {
        HRESULT regHr = pProfile->Register(CLSID_HunminTSF);
        if (dbg) {
            fwprintf(dbg, L"pProfile->Register hr=0x%lX\n", (unsigned long)regHr);
            fflush(dbg);
        }
        static const GUID profileGuid = HunminProfileGuid;
        HMODULE hSelf = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)(const void *)DllRegisterServer, &hSelf);
        wchar_t iconPath[MAX_PATH] = L"";
        if (hSelf)
            GetModuleFileNameW(hSelf, iconPath, MAX_PATH);
        HRESULT addHr =
            pProfile->AddLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid, L"Hunmin", (ULONG)wcslen(L"Hunmin"),
                                         iconPath[0] ? iconPath : nullptr, 0, TRUE);
        if (dbg) {
            fwprintf(dbg, L"pProfile->AddLanguageProfile hr=0x%lX\n", (unsigned long)addHr);
            fflush(dbg);
        }
        HRESULT enableHr = pProfile->EnableLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid, TRUE);
        if (dbg) {
            fwprintf(dbg, L"pProfile->EnableLanguageProfile hr=0x%lX\n", (unsigned long)enableHr);
            fflush(dbg);
        }
        /* Icon index pin (measured 2026-10-18: the live profile answered
         * IconFile=<dll> IconIndex=1 — a slot with no icon — and the language
         * entry rendered BLANK; user report "still no icon on langbar").
         * AddLanguageProfile was called with uIconIndex=0 yet the stored
         * value read 1, so registration rewrites the stored index to 0
         * (resource id 1 IS the first icon of hunmin.ico). */
        {
            wchar_t lp3412[256];
            swprintf(lp3412, _countof(lp3412),
                     L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\LanguageProfile\\0x00000412\\%s",
                     guidStr, L"{F2C70002-0000-0000-0000-000000000002}");
            for (DWORD pass = 0; pass < 2; ++pass) {
                wchar_t ikey[192];
                if (pass == 0)
                    swprintf(ikey, _countof(ikey),
                             L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\LanguageProfile\\0x00000412",
                             guidStr);
                else
                    lstrcpynW(ikey, lp3412, _countof(ikey));
                HKEY hIk = nullptr;
                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, ikey, 0, KEY_SET_VALUE, &hIk) ==
                    ERROR_SUCCESS) {
                    DWORD zero = 0;
                    RegSetValueExW(hIk, L"IconIndex", 0, REG_DWORD, (const BYTE *)&zero,
                                   sizeof zero);
                    RegCloseKey(hIk);
                }
            }
        }
        /* Activation has to be asked for on the modern interface.
         *
         * ITfInputProcessorProfiles::ActivateLanguageProfile returns S_OK
         * while activating nothing beyond the calling process: measured on
         * this machine, GetActiveLanguageProfile then still answered S_FALSE
         * for hunmin AND for the Microsoft IME, and a fresh Notepad loaded
         * MSCTF.dll but never hunmin-tsf.dll -- no Activate(), no key sink.
         *
         * Measured for ITfInputProcessorProfileMgr::ActivateProfile: neither
         * TF_IPPMF_FORSESSION nor TF_IPPMF_FORSYSTEMALL (both with
         * TF_IPPMF_ENABLEPROFILE) persists past the registering process --
         * a fresh process still reports the Microsoft IME as active. What
         * they do record is the enablement/profile entry, so the profile is
         * selectable; selecting it (Win+Space) then loads and activates the
         * TIP in the focused process, measured end to end. The CTF default
         * assembly (HKCU ...CTF SortOrder index 0 / Assemblies Default) is
         * Windows Settings' store and is deliberately not written here.
         */
        ITfInputProcessorProfileMgr *pProfileMgr = nullptr;
        HRESULT mgrHr = pProfile->QueryInterface(IID_ITfInputProcessorProfileMgr, (LPVOID *)&pProfileMgr);
        if (dbg) {
            fwprintf(dbg, L"pProfile QI(ITfInputProcessorProfileMgr) hr=0x%lX\n", (unsigned long)mgrHr);
            fflush(dbg);
        }
        if (SUCCEEDED(mgrHr) && pProfileMgr) {
            HRESULT actHr =
                pProfileMgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, 0x0412, CLSID_HunminTSF, profileGuid,
                                             nullptr, TF_IPPMF_FORSYSTEMALL | TF_IPPMF_ENABLEPROFILE);
            if (dbg) {
                fwprintf(dbg, L"pProfileMgr->ActivateProfile hr=0x%lX\n", (unsigned long)actHr);
                fflush(dbg);
            }
            TF_INPUTPROCESSORPROFILE active;
            ZeroMemory(&active, sizeof(active));
            HRESULT getHr = pProfileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &active);
            if (dbg) {
                fwprintf(dbg, L"pProfileMgr->GetActiveProfile hr=0x%lX active.clsid={%08lX} active.profile=%08lX\n",
                         (unsigned long)getHr, (unsigned long)active.clsid.Data1,
                         (unsigned long)active.guidProfile.Data1);
                fflush(dbg);
            }
            if (FAILED(getHr) || !IsEqualGUID(active.clsid, CLSID_HunminTSF)) {
                /* Pre-Vista hosts, or a profile the session refused: the
                 * legacy call is the only thing left to try. */
                HRESULT legacyHr = pProfile->ActivateLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid);
                if (dbg) {
                    fwprintf(dbg, L"fallback ActivateLanguageProfile hr=0x%lX\n", (unsigned long)legacyHr);
                    fflush(dbg);
                }
            }
            pProfileMgr->Release();
        } else {
            HRESULT legacyHr = pProfile->ActivateLanguageProfile(CLSID_HunminTSF, 0x0412, profileGuid);
            if (dbg) {
                fwprintf(dbg, L"ActivateLanguageProfile (no ProfileMgr) hr=0x%lX\n", (unsigned long)legacyHr);
                fflush(dbg);
            }
        }
        pProfile->Release();
    }

    ITfCategoryMgr *pCatMgr = nullptr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (LPVOID *)&pCatMgr);
    if (dbg) {
        fwprintf(dbg, L"pCatMgr CoCreate hr=0x%lX\n", (unsigned long)hr);
        fflush(dbg);
    }
    if (SUCCEEDED(hr) && pCatMgr) {
        HRESULT c1 = pCatMgr->RegisterCategory(CLSID_HunminTSF, GUID_TFCAT_TIP_KEYBOARD, CLSID_HunminTSF);
        HRESULT c2 = pCatMgr->RegisterCategory(CLSID_HunminTSF, GUID_TFCAT_CATEGORY_OF_TIP, CLSID_HunminTSF);
        /* Issue #98: the display-attribute provider category — the app-side
         * attribute manager resolves the above provider object through it,
         * and the composition range's GUID_PROP_ATTRIBUTE value carries
         * HunminDisplayAttrGuid, which the provider serves. */
        HRESULT c3 = pCatMgr->RegisterCategory(CLSID_HunminTSF, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
                                               CLSID_HunminTSF);
        if (dbg) {
            fwprintf(dbg, L"RegisterCategory keyboard=0x%lX category=0x%lX dispattr=0x%lX\n",
                     (unsigned long)c1, (unsigned long)c2, (unsigned long)c3);
            fflush(dbg);
        }
        pCatMgr->Release();
    }
    /* The Start-menu entry (discoverability): "Hunmin IME 설정" under
     * Programs. The ruled surface (the settings-ui exe) wins when it is
     * deployed beside the DLL; the rundll32 entry into HunminSettingsShow
     * is the fallback that always works. Deleted in unregister, mirroring
     * the tree work. */
    {
        wchar_t dllDir[MAX_PATH];
        lstrcpynW(dllDir, dllPath, MAX_PATH);
        PathRemoveFileSpecW(dllDir);
        wchar_t pub[MAX_PATH];
        wchar_t exe[MAX_PATH];
        exe[0] = 0;
        /* Candidate A: deployment layout — <dll dir>\settings-ui\publish. */
        _snwprintf(pub, MAX_PATH, L"%s\\settings-ui\\publish\\hunmin-tsf-settingsui.exe", dllDir);
        if (PathFileExistsW(pub))
            lstrcpynW(exe, pub, MAX_PATH);
        if (!exe[0]) {
            /* Candidate B: this worktree's layout — repo\frontends\tsf\.. */
            _snwprintf(pub, MAX_PATH,
                       L"%s\\..\\..\\settings-ui\\publish\\hunmin-tsf-settingsui.exe", dllDir);
            wchar_t full[MAX_PATH];
            if (GetFullPathNameW(pub, MAX_PATH, full, nullptr) && PathFileExistsW(full))
                lstrcpynW(exe, full, MAX_PATH);
        }
        IShellLinkW *sl = nullptr;
        HRESULT lh = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&sl));
        if (dbg)
            fwprintf(dbg, L"shortcut CoCreate hr=0x%lX exe=%s\n", (unsigned long)lh,
                     exe[0] ? exe : L"<rundll32>");
        if (SUCCEEDED(lh) && sl) {
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
                PWSTR programs = nullptr;
                if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr,
                                                   &programs)) &&
                    programs) {
                    wchar_t link[MAX_PATH + 48];
                    _snwprintf(link, _countof(link), L"%s\\Hunmin IME \uC124\uC815.lnk",
                               programs);
                    if (SUCCEEDED(pf->Save(link, TRUE))) {
                        if (dbg)
                            fwprintf(dbg, L"shortcut saved %s\n", link);
                    } else if (dbg) {
                        fwprintf(dbg, L"shortcut save FAILED %s\n", link);
                    }
                    CoTaskMemFree(programs);
                }
                pf->Release();
            }
            sl->Release();
        }
    }
    CoUninitialize();
    return TRUE;
}

extern "C" BOOL DllUnregisterServer() {
    /* Mirror of DllRegisterServer, applied in reverse order (issue #84).
     * First the profile and category state the CTF COM services hold, then
     * the HKLM TIP tree under which AddLanguageProfile created subkeys, then
     * the COM CLSID tree. Windows Settings' own per-user CTF store (HKCU
     * SortOrder / Assemblies) is deliberately NOT touched: registration
     * never writes it (measured, docs/WINDOWS-TSF-REGISTRATION.md), so
     * unregistering must not either. Logging follows the register flow: only
     * tsf_log_open-mediated writes, to hunmin-reg-debug.log. */
    FILE *dbg = tsf_log_open("hunmin-reg-debug.log", "a");
    wchar_t guidStr[64];
    StringFromGUID2(CLSID_HunminTSF, guidStr, 64);
    if (dbg) {
        fwprintf(dbg, L"--- unregister begin guid=%s\n", guidStr);
        fflush(dbg);
    }

    CoInitialize(nullptr);
    ITfInputProcessorProfiles *pProfile = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfInputProcessorProfiles, (LPVOID *)&pProfile);
    if (dbg) {
        fwprintf(dbg, L"pProfile CoCreate hr=0x%lX\n", (unsigned long)hr);
        fflush(dbg);
    }
    if (SUCCEEDED(hr) && pProfile) {
        /* Reverse of the register order: the per-profile enablement entry the
         * ProfileMgr interface owns goes first (mirror of EnableLanguage-
         * Profile / ProfileMgr), then Unregister() for Register(). */
        ITfInputProcessorProfileMgr *pProfileMgr = nullptr;
        HRESULT mgrHr = pProfile->QueryInterface(IID_ITfInputProcessorProfileMgr, (LPVOID *)&pProfileMgr);
        if (dbg) {
            fwprintf(dbg, L"pProfile QI(ITfInputProcessorProfileMgr) hr=0x%lX\n", (unsigned long)mgrHr);
            fflush(dbg);
        }
        if (SUCCEEDED(mgrHr) && pProfileMgr) {
            HRESULT upHr = pProfileMgr->UnregisterProfile(CLSID_HunminTSF, 0x0412, HunminProfileGuid, 0);
            if (dbg) {
                fwprintf(dbg, L"pProfileMgr->UnregisterProfile hr=0x%lX\n", (unsigned long)upHr);
                fflush(dbg);
            }
            pProfileMgr->Release();
        }
        HRESULT unregHr = pProfile->Unregister(CLSID_HunminTSF);
        if (dbg) {
            fwprintf(dbg, L"pProfile->Unregister hr=0x%lX\n", (unsigned long)unregHr);
            fflush(dbg);
        }
        pProfile->Release();
    }

    ITfCategoryMgr *pCatMgr = nullptr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (LPVOID *)&pCatMgr);
    if (dbg) {
        fwprintf(dbg, L"pCatMgr CoCreate hr=0x%lX\n", (unsigned long)hr);
        fflush(dbg);
    }
    if (SUCCEEDED(hr) && pCatMgr) {
        HRESULT c1 = pCatMgr->UnregisterCategory(CLSID_HunminTSF, GUID_TFCAT_TIP_KEYBOARD, CLSID_HunminTSF);
        HRESULT c2 = pCatMgr->UnregisterCategory(CLSID_HunminTSF, GUID_TFCAT_CATEGORY_OF_TIP, CLSID_HunminTSF);
        /* Issue #98: mirror of the display-attribute provider registration. */
        HRESULT c3 = pCatMgr->UnregisterCategory(CLSID_HunminTSF, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
                                                 CLSID_HunminTSF);
        if (dbg) {
            fwprintf(dbg, L"UnregisterCategory keyboard=0x%lX category=0x%lX dispattr=0x%lX\n",
                     (unsigned long)c1, (unsigned long)c2, (unsigned long)c3);
            fflush(dbg);
        }
        pCatMgr->Release();
    }
    CoUninitialize();

    /* HKLM TIP tree: deleted AFTER the COM calls, because the registering
     * side created LanguageProfile\0x00000412\{profile} subkeys under it
     * through AddLanguageProfile. */
    wchar_t tipPath[256];
    swprintf(tipPath, 256, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s", guidStr);
    LSTATUS tipSt = RegDeleteTreeW(HKEY_LOCAL_MACHINE, tipPath);
    if (dbg) {
        fwprintf(dbg, L"RegDeleteTreeW(HKLM %s) st=0x%lX\n", tipPath, (unsigned long)tipSt);
        fflush(dbg);
    }

    /* COM CLSID tree -- the only deletion the previous implementation did. */
    wchar_t clsidPath[128];
    swprintf(clsidPath, 128, L"CLSID\\%s", guidStr);
    LSTATUS clsidSt = RegDeleteTreeW(HKEY_CLASSES_ROOT, clsidPath);
    if (dbg) {
        fwprintf(dbg, L"RegDeleteTreeW(HKCR %s) st=0x%lX\n", clsidPath, (unsigned long)clsidSt);
        fflush(dbg);
        fclose(dbg);
    }
    return TRUE;
}
extern "C" const CLSID CLSID_HunminTSF_Export = {
    0xF2C70001, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}};

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
    FILE *dbg = tsf_log_open("hunmin-tsf-debug.log");
    wchar_t str[64];
    StringFromGUID2(rclsid, str, 64);
    if (dbg) {
        fwprintf(dbg, L"DllGetClassObject rclsid=%s\n", str);
        fflush(dbg);
    }
    if (!IsEqualCLSID(rclsid, CLSID_HunminTSF)) {
        if (dbg) {
            fwprintf(dbg, L"  not registered CLSID\n");
            fclose(dbg);
        }
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    CClassFactory *factory = new CClassFactory();
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    if (dbg) {
        fwprintf(dbg, L"  hr=0x%lX\n", (unsigned long)hr);
        fclose(dbg);
    }
    return hr;
}
