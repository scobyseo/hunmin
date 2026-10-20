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
/* Issue #103 (ruled tray badge): the adapter's OWN tray surface. Included
 * like src/keybinding.cpp (not a target source) so the mock harness's
 * compile-out seam — HUNMIN_TSF_TRAY_DISABLED — reaches it in one TU. */
#include "trayicon.cpp"
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
    wchar_t pub[MAX_PATH];
    _snwprintf(pub, _countof(pub), L"%s\\settings-ui\\publish\\hunmin-tsf-settingsui.exe",
               dllDir);
    if (!PathFileExistsW(pub)) {
        _snwprintf(pub, _countof(pub),
                   L"%s\\..\\..\\settings-ui\\publish\\hunmin-tsf-settingsui.exe", dllDir);
        wchar_t full[MAX_PATH];
        if (GetFullPathNameW(pub, MAX_PATH, full, nullptr) && PathFileExistsW(full))
            _snwprintf(pub, _countof(pub), L"%s", full);
        else
            pub[0] = 0;
    }
    if (pub[0])
        lstrcpynW(exe, pub, MAX_PATH);
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
/* The language-bar item's own item id (ctfutb TF_LANGBARITEMINFO::guidItem;
 * a fresh slot in the same F2C7000x block, never equal to another TIP's). */
static const GUID HunminLangBarItemGuid = {0xF2C70003, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03}};
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

/* ---- the SYSTEM langbar menu extension (docs: language-bar.md,
 * "Extending System Language Bar Items"): TSF supports adding menu items to
 * EXISTING language bar menus — the mechanism for the user's "right-click on
 * the system lang bar -> setting" ask: obtain the item via GetItem, QI it for
 * ITfSource, AdviseSink(IID_ITfSystemLangBarItemSink, this); the extended
 * service calls InitMenu when its menu shows and OnMenuSelect with our id. ---- */
class HunminSystemLangBarMenuExtension;

wchar_t g_hunmin_ext_stub[] = L"hunmin-langbar-extension"; /* FROM_ADDRESS anchor */

/* Issue #103 (the tray badge): the badge's actions land back on the ACTIVE
 * service — a left-click runs the SAME mode-switch core the toggle key
 * drives — and on the shared settings launch. Declared here so the class
 * below can register them in Activate; defined after the class (the toggle
 * reaches a private member the class owns). g_activeTrayService's lifetime
 * matches the object's activation lifetime (set in Activate, cleared in
 * Deactivate). */
class CTextService;
static CTextService *g_activeTrayService = nullptr;
static void hunmin_tsf_tray_toggle_thunk(void);
static void hunmin_tsf_tray_settings_thunk(void);

class HunminSystemLangBarMenuExtension : public ITfSystemLangBarItemSink {
  public:
    LONG ref_ = 1;
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv) override {
        if (!ppv)
            return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfSystemLangBarItemSink)) {
            *ppv = static_cast<ITfSystemLangBarItemSink *>(this);
            InterlockedIncrement(&ref_);
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
    /* ITfSystemLangBarItemSink: the menu-button shape (docs: language-bar.md)
     * — one entry, then OnMenuSelect lands on the settings surface. */
    STDMETHOD(InitMenu)(ITfMenu *pMenu) override {
        if (!pMenu)
            return E_FAIL;
        const wchar_t *label = L"Hunmin \uC124\uC815 \uC5F4\uAE30";
        return pMenu->AddMenuItem(1, 0, nullptr, nullptr, label,
                                  (ULONG)wcslen(label), nullptr);
    }
    STDMETHOD(OnMenuSelect)(UINT wID) override {
        if (wID != 1)
            return S_OK;
        return hunmin_tsf_launch_settings((void *)&g_hunmin_ext_stub);
    }
};

class CTextService : public ITfTextInputProcessor,
                     public ITfThreadMgrEventSink,
                     public ITfKeyEventSink,
                     public ITfCompositionSink,
                     public ITfLangBarItemButton,
                     public ITfDisplayAttributeProvider {
  public:
    CTextService()
        : ref_(1), threadMgr_(nullptr), clientId_(TF_CLIENTID_NULL), sinkCookie_(0), ctx_(nullptr), ic_(nullptr),
          hangulMode_(true), overlayWnd_(nullptr), hasPreeditRect_(false), lastCtx_(nullptr),
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
        if (overlayWnd_)
            DestroyWindow(overlayWnd_);
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

    /* The tray badge update (issue #103, the ruled tray surface). Every
     * mode boundary goes through here so the badge can never drill from
     * the mode state the adapter actually holds. Tracked no-ops in the
     * mock harness (HUNMIN_TSF_TRAY_DISABLED). */
    void UpdateTrayIcon() {
        hunmin_tsf_tray_update(hangulMode_ ? 1 : 0);
    }

    /* The mode switch's shared core: the toggle key (OnKeyDown, focused
     * context) and the tray badge's left-click (no context) take the SAME
     * steps — composition boundary, flip, conversion compartment, badge,
     * ic rebuild with the other slot's layout. pic may be null (the tray
     * click): then the live composition, if any, ends in its own context
     * (DetachComposition — the same contract a context switch runs) and a
     * buffered word without an in-flight syllable is KEPT for the next
     * real boundary rather than dropped into a null-context insert. */
    void ToggleModeCore(ITfContext *pic, FILE *dbg) {
        CloseCandidateWindow();
        if (pic) {
            BoundaryFlush(pic, dbg);
        } else if (composition_ && compCtx_) {
            DetachComposition();
        } else if (dbg) {
            fprintf(dbg, " mode switch without a context: %zu buffered char(s) kept\n",
                    wordbuf_.size());
            fflush(dbg);
        }
        hangulMode_ = !hangulMode_;
        UpdateConversionCompartment();
        UpdateTrayIcon();
        if (ic_) {
            hunmin_ic_free(ic_);
            ic_ = nullptr;
        }
        if (ctx_)
            ic_ = hunmin_ic_new(ctx_, hangulMode_ ? hangulId_.c_str() : latinId_.c_str());
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
        /* The language-bar button (ctfutb.h, discoverable settings entry):
         * registered while the service is active; the desktop/floating bar
         * shows "Hunmin 설정", clicking opens the settings window. */
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
        /* The SYSTEM langbar menu extension (language-bar.md): advise our
         * entry onto the host thread's system items (언어/키보드/도움말... —
         * measured item list in this env); whichever accepts it will call
         * InitMenu for our "Hunmin 설정 열기". Failures are measured, not
         * fatal: the item-granting rule is per host build. */
        if (threadMgr_) {
            ITfLangBarItemMgr *plbm2 = nullptr;
            if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfLangBarItemMgr,
                                                     (void **)&plbm2)) &&
                plbm2) {
                static const GUID sysItems[] = {
                    {0x34745C63, 0xB2F0, 0x4784, {0x8B, 0x67, 0x5E, 0x12, 0xC8, 0x70, 0x1A, 0x31}},
                    {0x58C99D96, 0x2F9B, 0x42CE, {0x91, 0xBE, 0x37, 0xEF, 0x18, 0x60, 0xF8, 0x82}},
                    {0x190F67A2, 0x024F, 0x40D9, {0xAA, 0xBA, 0x43, 0x9F, 0x5C, 0x80, 0x8F, 0x67}},
                    {0xED9D5450, 0xEBE6, 0x4255, {0x82, 0x89, 0xF8, 0xA3, 0x1E, 0x68, 0x72, 0x28}},
                };
                if (!menExt_)
                    menExt_ = new HunminSystemLangBarMenuExtension();
                for (const GUID &g : sysItems) {
                    HRESULT gr = plbm2->GetItem(g, &barItemBuf_);
                    if (SUCCEEDED(gr) && barItemBuf_) {
                        ITfSource *src = nullptr;
                        if (SUCCEEDED(barItemBuf_->QueryInterface(IID_ITfSource,
                                                                  (void **)&src)) &&
                            src) {
                            HRESULT ar = src->AdviseSink(
                                IID_ITfSystemLangBarItemSink, menExt_, &menCookie_);
                            FILE *dbg6 = tsf_log_open("hunmin-tsf-debug.log");
                            if (dbg6) {
                                fprintf(dbg6, "sysmenu advise cls=%08lX hr=0x%lX\n",
                                        (unsigned long)g.Data1, (unsigned long)ar);
                                fflush(dbg6);
                                fclose(dbg6);
                            }
                            if (SUCCEEDED(ar)) {
                                menCookie_ = menCookie_; /* cookie recorded */
                                if (menSrc_)
                                    menSrc_->Release();
                                menSrc_ = src; /* released at Deactivate */
                                menExt_->AddRef();
                            } else {
                                src->Release();
                            }
                        }
                        barItemBuf_->Release();
                        barItemBuf_ = nullptr;
                    }
                }
                plbm2->Release();
            }
        }
        UpdateConversionCompartment();
        /* The tray badge lives with the service (issue #103): the callbacks
         * are registered BEFORE the first update, so a click can never fire
         * into nothing, and the pointer's lifetime matches the activation. */
        g_activeTrayService = this;
        {
            hunmin_tsf_tray_callbacks cbs;
            cbs.toggle = &hunmin_tsf_tray_toggle_thunk;
            cbs.open_settings = &hunmin_tsf_tray_settings_thunk;
            hunmin_tsf_tray_set_callbacks(&cbs);
        }
        UpdateTrayIcon();
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
        } /* The system-menu extension's documents (language-bar.md step 4):
         * UnadviseSink by ITS cookie, THEN the sink release. */
        if (menSrc_ && menExt_) {
            menSrc_->UnadviseSink(menCookie_);
            menSrc_->Release();
            menSrc_ = nullptr;
            menExt_->Release();
            menExt_ = nullptr;
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
        /* Issue #103: the tray badge leaves with the service — a profile
         * switch to another IME must not leave the icon standing; the next
         * Activate re-registers the callbacks and re-adds the icon. */
        if (g_activeTrayService == this)
            g_activeTrayService = nullptr;
        hunmin_tsf_tray_hide();
        clientId_ = TF_CLIENTID_NULL;
        return S_OK;
    }
    /* ---- ctfutb.h: ITfLangBarItem (the base surface the manager calls),
     * then ITfLangBarItemButton. The tooltip-names and the click handler
     * land the user on the settings surface (issue #67's four access
     * points measured: this bar button + the ITfFnConfigure chain + the
     * settings-ui exe are the ones a third-party TIP may legally reach;
     * the Settings app keyboard page and the taskbar indicator context
     * menu belong to the shell). ---- */
    STDMETHOD(GetInfo)(TF_LANGBARITEMINFO *pInfo) override {
        if (!pInfo)
            return E_INVALIDARG;
        ZeroMemory(pInfo, sizeof *pInfo);
        pInfo->clsidService = CLSID_HunminTSF;
        pInfo->guidItem = HunminLangBarItemGuid;
        /* BTN_BUTTON only: the earlier combo with HIDDENBYDEFAULT|
         * SHOWNINTRAY was in place while every AddItem answered E_FAIL
         * (measured 2026-10-18); the console kernel's BTN item carries
         * exactly 0x00010010, so the reduced style is the reduced
         * hypothesis — retry AddItem with it before blaming the kernel. */
        pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON;
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
        (void)click;
        (void)pt;
        (void)prcArea;
        /* The dedicated process keeps this thread free: the TIP thread must
         * not pump a nested modal loop while TSF holds edit cookies. */
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
        } else if ((wParam >= 'A' && wParam <= 'Z') || wParam == VK_SPACE || wParam == VK_BACK)
            eat = TRUE;
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
        (void)lParam;
        if (lastCtx_ != pic) {
            if (lastCtx_)
                lastCtx_->Release();
            lastCtx_ = pic;
            if (lastCtx_)
                lastCtx_->AddRef();
            if (composition_ && compCtx_ && compCtx_ != pic)
                DetachComposition();
        }
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
        // Mode-toggle keys (issue #66 / #71): the configured bindings plus the
        // always-on bare Han/Eng key (VK_HANGEUL, the platform alternate no
        // rebinding can make dead — the Android rule). A toggle is a mode
        // boundary, i.e. a composition boundary (Jamotong manual §9.5; the
        // Android adapter commits at the mode switch): the pending syllable
        // is flushed and the buffered word inserted before the engine is
        // rebuilt with the other slot's layout. Auto-repeat is keydown with
        // the preceding-down bit set (lParam bit 30): the press must toggle
        // once, and repeats are swallowed rather than delivered to the app.
        if (hunmin_tsf_keybindings_match(toggleKeys_.data(), (int)toggleKeys_.size(),
                                         (unsigned)wParam, hunmin_tsf_keymods_from_state())) {
            if (pfEat)
                *pfEat = TRUE;
            const bool repeat = (lParam & (1 << 30)) != 0;
            if (!repeat)
                /* The shared mode-switch core (issue #103): the tray
                 * badge's left-click runs the same steps with no context. */
                ToggleModeCore(pic, dbg);
            if (dbg) {
                fprintf(dbg, " toggle key vk=%lu repeat=%d -> %s (%s)\n",
                        (unsigned long)wParam, (int)repeat,
                        hangulMode_ ? "hangul" : "english",
                        hangulMode_ ? hangulId_.c_str() : latinId_.c_str());
                fclose(dbg);
            }
            return S_OK;
        }
        if (!hangulMode_) {
            if (dbg)
                fclose(dbg);
            return S_OK; // passthrough English
        }
        /* --- the candidate-window key (issue #67) ---
         *
         * The configured binding (default: the bare physical Hanja key)
         * toggles the window. Interception sits before any engine feed
         * because it is not a printable key (fcitx5's identical placement
         * for F9, engine.cpp:204-223). Consumption is conditional whether
         * the window is open or, with a candidate-bearing buffer, just
         * opened; with neither, the key belongs to the client app and
         * passes through uneaten. */
        if (hunmin_tsf_keybindings_match(&hanjaCombo_, 1, (unsigned)wParam,
                                         hunmin_tsf_keymods_from_state())) {
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
            return S_OK;
        }
        if (candActive_) {
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
                 * 0 picks the tenth, mirroring fcitx5's label scheme. A
                 * digit past the end of the count closes the window and
                 * falls through like any other unhandled key — an invisible
                 * window must never swallow a keystroke. */
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
            if (handled) {
                if (pfEat)
                    *pfEat = TRUE;
                if (dbg) {
                    fprintf(dbg, " candidate key vk=%lu cursor=%d count=%d\n", (unsigned long)wParam,
                            candCursor_, candCount_);
                    fclose(dbg);
                }
                return S_OK;
            }
            /* Anything else: close and fall through to the normal path, so
             * typing continues where the user left off instead of being
             * swallowed by an invisible window (fcitx5: the default rule).
             * The buffered word is untouched by the close itself. */
            CloseCandidateWindow();
        }
        /* Edit/navigation keys (measured on this host during the buffered-input
         * check: VK_LEFT..VK_NEXT + VK_INSERT/VK_DELETE all fall into the
         * 0x20-0x7E printable fallback below, so each was translated into a
         * layout character (VK_UP 38 -> '&', VK_DOWN 40 -> '(', DELETE 46 ->
         * '.', ...) and EATEN — the editor never saw them and stray punctuation
         * could land in the document. They are keys of passage: a cursor move
         * is a composition boundary, so the boundary drain commits the buffered
         * word exactly once and the key passes through untouched
         * (pfEat = FALSE, matching the OnTestKeyDown predicate which already
         * shows no interest in them). Reached only while the candidate window
         * is NOT open: when it is, the block above owns these vk's for
         * list motion (issue #67). */
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
        case VK_DELETE: {
            BoundaryFlush(pic, dbg);
            if (pfEat)
                *pfEat = FALSE;
            if (dbg) {
                fprintf(dbg, " edit key vk=%lu -> boundary + passthrough\n", (unsigned long)wParam);
                fclose(dbg);
            }
            return S_OK;
        }
        default:
            /* The function-key block (measured with the unbound-F9 case of
             * issue #67: VK_F1..VK_F12 = 0x70..0x7B falls into the
             * printable fallback below, so an unbound F-key composed its
             * VK-value as a layout character — F9(0x78) typed 'x' = ㅈ into
             * the composition the user was watching. Same defect class as
             * the edit keys above, same keys-of-passage answer: a drained
             * boundary and a pass-through. */
            if (wParam >= VK_F1 && wParam <= VK_F12) {
                BoundaryFlush(pic, dbg);
                if (pfEat)
                    *pfEat = FALSE;
                if (dbg) {
                    fprintf(dbg, " fkey vk=%lu -> boundary + passthrough\n", (unsigned long)wParam);
                    fclose(dbg);
                }
                return S_OK;
            }
            break;
        }
        /* --- the CTRL/ALT chord rule (measured defect 2026-10-18: the letter
         * inside a Ctrl/Alt chord still fed the composition — Ctrl+A typed
         * the preedit's next syllable instead of select-all) ---
         *
         * A chord is application input, not composition input: the pending
         * preedit commits through the SAME boundary flush a mode toggle
         * drives (the candidate window closes with its composition, #67),
         * and the key passes to the client app uneaten, in its chord sense.
         * The mode itself is NOT flipped: one chord selects all without
         * remaining more aggressive — the mode WORD switch on ctrl/alt — was
         * ruled out 2026-10-18, issue #101: "will not switch mode; transient
         * processing is enough". See docs/WINDOWS-TSF-BUFFERING.md and
         * wiki/decisions/2026-10-18-tsf-chord-transient-not-persistent.md.) */
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
         * selects NO row — the Shift bit below stays the only row transport
         * (the engine's case endpoint, hunmin_ic_process_key). */
        hunmin_ic_set_option(ic_, "caps", hunmin_tsf_capslock_from_state() ? 1 : 0);
        if (mods & (HUNMIN_TSF_KEYMOD_CTRL | HUNMIN_TSF_KEYMOD_ALT)) {
            if (pfEat)
                *pfEat = FALSE;
            CloseCandidateWindow();
            BoundaryFlush(pic, dbg);
            if (dbg) {
                fprintf(dbg, " ctrl/alt chord -> commit + passthrough\n");
                fclose(dbg);
            }
            return S_OK;
        }
        /* --- the VK -> base half, and only that half (issue #106) ---
         *
         * Before this, each branch below carried BOTH forms of its key:
         * the Shift bit picked between them here, over a second copy of the
         * US-QWERTY shift map (VK_0..VK_9 against a hardcoded
         * "0123456789"/")!@#$%^&*(" pair, eleven VK_OEM_* pairs). The engine
         * already owns that map -- libhunmin/src/data.c's BASE/SHIFTED, the
         * same table the loader uses to place a "shift" row -- and
         * hunmin_ic_process_key() exposes it as the case endpoint
         * (hunmin.h:66-84, issue #90's option B, docs/IC-INPUT-OPTIONS.md
         * §2-B). So what is left here is the half no host can supply: the
         * physical key's UNSHIFTED US-QWERTY character. The shift bit
         * travels beside it, and the shifted row is selected in the engine.
         *
         * This is the shape the Android adapter landed in #105 and the one
         * docs/FRONTEND-CONTRACT.md A3' describes. It carries the same
         * stated cost as fcitx5's assumption A1 and Android's registry: a
         * layout wanting a non-US physical key cannot be expressed, because
         * the table is authored in the US-QWERTY convention.
         *
         * CapsLock IS wired to the engine `caps` option — since issue #112's
         * ruling ("wire and make it common to all platform") it is set once
         * per key event beside the modifier snapshot above. What stays true
         * here is that CapsLock selects NO row: the (base, shift) pair below
         * is the only row transport, on every platform. */
        uint32_t base = 0;
        if (wParam >= 'A' && wParam <= 'Z') {
            base = (uint32_t)(wParam + 32); /* the lowercase half */
        } else if (wParam >= '0' && wParam <= '9') {
            base = (uint32_t)wParam;
        } else if (wParam == VK_SPACE) {
            base = ' ';
        } else if (wParam == VK_BACK) {
            base = 0x08;
        } else {
            switch (wParam) {
            case VK_OEM_1:
                base = ';';
                break;
            case VK_OEM_7:
                base = '\'';
                break;
            case VK_OEM_COMMA:
                base = ',';
                break;
            case VK_OEM_PERIOD:
                base = '.';
                break;
            case VK_OEM_2:
                base = '/';
                break;
            case VK_OEM_3:
                base = '`';
                break;
            case VK_OEM_4:
                base = '[';
                break;
            case VK_OEM_5:
                base = '\\';
                break;
            case VK_OEM_6:
                base = ']';
                break;
            case VK_OEM_MINUS:
                base = '-';
                break;
            case VK_OEM_PLUS:
                base = '=';
                break;
            default:
                if (wParam >= 0x20 && wParam <= 0x7E)
                    base = (uint32_t)wParam;
                break;
            }
        }
        // When base == 0 (punctuation / any key no layout row covers), fall through
        // with the Zero entry: hunmin_ic_process -> save_commit -> false, so the
        // declined branch below flushes the pending syllable and passes the key.
        // Modifier keys (Shift/Ctrl/Alt/Win, Caps) are not text: never flush/passthrough
        // through the engine (would corrupt an in-flight syllable). Other unmapped
        // keys (punct, arrows, Enter, digits handled here as 0) fall through so the
        // declined branch flushes the pending composition and passes the key.
        if (wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU || wParam == VK_LSHIFT ||
            wParam == VK_RSHIFT || wParam == VK_LCONTROL || wParam == VK_RCONTROL || wParam == VK_LMENU ||
            wParam == VK_RMENU || wParam == VK_LWIN || wParam == VK_RWIN || wParam == VK_CAPITAL || wParam == VK_TAB) {
            /* CapsLock is no longer the mode toggle (maintainer instruction:
             * the Han/Eng key is the default), and in Korean input mode the
             * key is ignored outright: it passes through for the OS to
             * toggle, and the engine `caps` option it feeds is set per key
             * event beside the modifier snapshot above (issue #112) — never
             * from this guard, and never into the (base, shift) pair the
             * engine resolves rows from. */
            if (dbg) {
                fprintf(dbg, " modifier/pass key -> leave composition\n");
                fclose(dbg);
            }
            return S_OK;
        }
        /* The case endpoint is the engine's (issue #106): the unshifted base
         * plus the Shift bit from the snapshot above. hunmin_ic_process_key
         * resolves the row through the loader's own US-QWERTY map, so the
         * shift column this adapter used to carry is deleted rather than
         * rewritten (FRONTEND-CONTRACT.md A3'). */
        bool consumed =
            hunmin_ic_process_key(ic_, base, (mods & HUNMIN_TSF_KEYMOD_SHIFT) ? 1u : 0u);
        if (consumed) {
            /* Word buffering (issue #73, adapter-side; no libhunmin change):
             * hunmin_ic_commit() is a destructive read of every syllable the
             * engine finalised so far, so the adapter accumulates it instead
             * of inserting per syllable. Insertion happens exactly once, on
             * the first boundary key (declined path below). */
            const char *commit = hunmin_ic_commit(ic_);
            if (commit && commit[0]) {
                int clen = MultiByteToWideChar(CP_UTF8, 0, commit, -1, nullptr, 0);
                if (clen > 1) {
                    size_t old = wordbuf_.size();
                    /* clen counts the NUL terminator; the buffer holds text
                     * only, or every later insert/overlay would stop at the
                     * embedded NUL after the first syllable. */
                    wordbuf_.resize(old + clen - 1);
                    MultiByteToWideChar(CP_UTF8, 0, commit, -1, &wordbuf_[old], clen);
                }
            }
            if (dbg) {
                fprintf(dbg, " consumed, word len=%zu preedit=%s\n", wordbuf_.size(), hunmin_ic_preedit(ic_));
                fflush(dbg);
            }
            CompositionWrite(pic);
            if (pfEat)
                *pfEat = TRUE;
        } else {
            /* Engine declined (space, empty backspace, unmapped): a word
             * boundary for the buffering adapter (issue #73). The engine has
             * already saved the in-flight syllable via save_commit; the
             * boundary drain flushes it, the adapter appends whatever word it
             * has been holding, and the whole word is inserted exactly once
             * before the boundary key passes through (pfEat stays FALSE) --
             * matching the fcitx/android "used=FALSE => passthrough"
             * contract. */
            BoundaryFlush(pic, dbg);
        }
        return S_OK;
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
        /* Logged as of the sandbox round that needed it (issue #99): this
         * handler was the only silent piece of the composition lifecycle,
         * and a host that terminates per keystroke hid here — every key
         * then evaluated its fresh/bare-jamo branch (round 10's trace). */
        HMLOG("OnCompositionTerminated comp=%p wordbuf=%zu\n",
              (void *)composition_, wordbuf_.size());
        /* The host terminated the composition (issue #98): the composed text
         * is now document text by the host's decision, so the buffers drain
         * and the engine state resets — the same word must never be
         * delivered twice through the next boundary. */
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

    void HideOverlay() {
        /* Ruled out with issue #98: the floating preedit window is gone for
         * good — the preedit rides the native composition. Kept as a no-op
         * so the teardown paths stay one call wide. */
        (void)overlayWnd_;
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
                    svc_->composition_->EndComposition(ec);
                    HMLOG("composition end text=%ls\n", t_);
                    svc_->composition_->Release();
                    svc_->composition_ = nullptr;
                    if (svc_->compCtx_) {
                        svc_->compCtx_->Release();
                        svc_->compCtx_ = nullptr;
                    }
                    svc_->compText_.clear();
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
    HWND overlayWnd_;
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
    /* The system-menu extension advised onto the host thread's system
     * langbar items (language-bar.md "Extending System Language Bar
     * Items"); one entry per advised item, each tracked for its own
     * UnadviseSink at Deactivate. */
    HunminSystemLangBarMenuExtension *menExt_ = nullptr;
    DWORD menCookie_ = 0;
    ITfSource *menSrc_ = nullptr;
    ITfLangBarItem *barItemBuf_ = nullptr;

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
        InsertBufferedWord(pic);
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
    std::string BufferedHangul() {
        std::string s = HunminWideToUtf8(wordbuf_);
        if (ic_) {
            const char *pre = hunmin_ic_preedit(ic_);
            if (pre)
                s += pre;
        }
        return s;
    }

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
        std::string word = BufferedHangul();
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
            candSource_ = LastSyllableOf(BufferedHangul());
        } else {
            candSource_ = BufferedHangul();
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
        /* Capture the word anchor BEFORE the reset: on a last-syllable
         * fallback list (issue #100) the buffered word's own state is what
         * the replacement is computed from, and after hunmin_ic_reset the
         * in-flight preedit is gone. */
        std::string replacement = hanja;
        if (candFallback_) {
            std::string word = BufferedHangul();
            std::string last = LastSyllableOf(word);
            /* Only last syllable replaced: the word prefix stays Hangul. */
            replacement = word.substr(0, word.size() - last.size()) + hanja;
        }
        if (ic_)
            hunmin_ic_reset(ic_); /* drop the composition the list anchored to */
        CloseCandidateWindow();
        wordbuf_ = HunminUtf8ToWide(replacement.c_str());
        InsertBufferedWord(pic);
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
/* The tray badge's registered callbacks (declared above the classes). One
 * routing point per process: the badge's icon window and callbacks are
 * shared state, and g_activeTrayService is the service that owns them. */
static void hunmin_tsf_tray_toggle_thunk(void) {
    if (g_activeTrayService)
        g_activeTrayService->ToggleModeCore(nullptr, nullptr);
}
static void hunmin_tsf_tray_settings_thunk(void) {
    hunmin_tsf_launch_settings((void *)&g_hunmin_ext_stub);
}

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
