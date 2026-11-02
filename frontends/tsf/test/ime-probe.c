/* ime-probe.c -- a minimal TSF-capable host app for the fresh-machine round
 * (issue #99), tests/manual-checklists/tsf-installer-fresh-vm.md.
 *
 * Why this exists: the sandbox image has no Notepad (the Store-app Notepad is
 * absent from Windows Sandbox, measured 2026-10-20), and the round's
 * interactive rows need a host app to type into. This is one window with one
 * text control, nothing else -- event mapping and presentation only; all
 * Hangul behaviour lives in the TIP being tested.
 *
 * The text control is RICHEDIT50W (msftedit.dll, an inbox system DLL, not a
 * Store app): like Notepad it is TSF-aware, so the TIP's native composition
 * (#98) renders INLINE in the control rather than in the system's floating
 * IMM32 compatibility window. If msftedit.dll is somehow missing, fall back
 * to a plain EDIT control: composition still commits, but the preedit
 * displays in the system floating window -- record which control was used.
 *
 * Batch mode (the Windows Sandbox live lane, tools/tsf-sandbox.ps1):
 *   ime-probe.exe --keys dkssud1 --expect <utf8> --out <file>
 * sends the keys with SendInput, reads the control text, writes key=value
 * lines (keys, text, expect, pass), and exits. The probe is built with
 * -DHUNMIN_PROBE_CONSOLE there: the sandbox runner waits on the process, and
 * the 2026-10-20 spike measured freshly built `/subsystem:windows` exes never
 * entering wWinMain on this host. The interactive windowed build is unchanged.
 *
 * Build (MSVC, statically linked CRT -- the clean image has no VC redist,
 * the same lesson as the /MT fix on the TIP itself):
 *   cl /nologo /W4 /O2 /DUNICODE /D_UNICODE ime-probe.c /Fe:ime-probe.exe ^
 *       /link /subsystem:windows user32.lib gdi32.lib ole32.lib uuid.lib shell32.lib
 * Batch build: the same with /DHUNMIN_PROBE_CONSOLE and /subsystem:console.
 * (There is no msctf.lib in the SDK: the TSF GUIDs resolve from uuid.lib,
 * which CMake adds by default on MSVC -- the TIP's own build relies on the
 * same umbrella.)
 *
 * Run it FROM the mapped folder: the TIP's debug log follows the host
 * process's working directory, so launching ime-probe.exe from the mapped
 * folder lands hunmin-tsf-debug.log there and it survives the uninstall --
 * the layout_count readback the previous round lost. Set HUNMIN_TSF_DEBUG_LOG=1
 * in the environment BEFORE launching (the round cmd does setx /M) so the
 * gated HMLOG lines -- layout_count among them -- are written at all.
 *
 * On start it also ACTIVATES the Hunmin profile for ko-KR through
 * ITfInputProcessorProfileMgr::ActivateProfile (the modern interface; the
 * legacy ActivateLanguageProfile is kept as a fallback). Round 7 (2026-10-20)
 * measured that activation did not survive the round: the ko-KR language list
 * was added AFTER the install, which can reset the active processor to the MS
 * IME default, and the driver's Win+Space taps never landed on Hunmin (raw
 * "dkssud" committed, tray showing only A and the Hangul glyph). Activation
 * here is the deterministic path; its result is logged, not assumed.
 */

#include <msctf.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

/* Same constants as the TIP itself (frontends/tsf/src/tsf.cpp). */
static const CLSID kHunminTsfClsid = {0xF2C70001, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}};
static const GUID kHunminProfileGuid = {0xF2C70002, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}};
#define HUNMIN_LANGID 0x0412

static void redirect_log_to_probe_dir(void) {
    /* The TIP logs next to the installed DLL by default (tsf.cpp
     * hunmin_tsf_log_path) -- inside the sandbox that is C:\Program Files,
     * unreachable from the host after the round. Point HUNMIN_TSF_LOG_DIR at
     * THIS exe's directory (the mapped folder) so the trace comes back, and
     * only for this process: other hosts' TIP instances keep logging
     * beside the DLL. Must run before the TIP loads (first key). */
    wchar_t exeW[MAX_PATH], dirW[MAX_PATH];
    wchar_t existing[MAX_PATH];

    /* The sandbox runner sets HUNMIN_TSF_LOG_DIR to the mapped out folder so
     * the TIP log survives the disposable guest. Honour it when present. */
    if (GetEnvironmentVariableW(L"HUNMIN_TSF_LOG_DIR", existing, MAX_PATH) > 0)
        return;

    DWORD n = GetModuleFileNameW(NULL, exeW, MAX_PATH);
    while (n > 0 && exeW[n - 1] != L'\\')
        n--;
    if (n == 0 || n >= MAX_PATH - 1)
        return;
    memcpy(dirW, exeW, n * sizeof(wchar_t));
    dirW[n] = 0;
    SetEnvironmentVariableW(L"HUNMIN_TSF_LOG_DIR", dirW);
    {
        /* Record where the TIP's own log will land, so a round log is
         * unambiguous about which copy to read. */
        FILE *l = _wfopen(L"ime-probe.log", L"a");
        if (l) {
            fprintf(l, "HUNMIN_TSF_LOG_DIR -> ");
            for (DWORD i = 0; i < n; i++)
                fputc(dirW[i] < 128 ? (int)dirW[i] : '?', l);
            fprintf(l, "\n");
            fclose(l);
        }
    }
}

static void activate_hunmin_profile(void) {
    FILE *log = _wfopen(L"ime-probe.log", L"a");
    HRESULT hr = E_FAIL, createHr = E_FAIL, mgrHr = E_FAIL;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    ITfInputProcessorProfiles *profiles = NULL;
    createHr = CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, &IID_ITfInputProcessorProfiles, (void **)&profiles);
    if (createHr == S_OK && profiles) {
        ITfInputProcessorProfileMgr *mgr = NULL;
        mgrHr = profiles->lpVtbl->QueryInterface(profiles, &IID_ITfInputProcessorProfileMgr, (void **)&mgr);
        if (mgrHr == S_OK && mgr) {
            /* The scope flags are the ones DllRegisterServer measured: a
             * process-local call without a scope fails on this host. The
             * fallback chain keeps the probe usable on older CTF versions. */
            hr = mgr->lpVtbl->ActivateProfile(mgr, TF_PROFILETYPE_INPUTPROCESSOR, HUNMIN_LANGID, &kHunminTsfClsid, &kHunminProfileGuid, NULL,
                                              TF_IPPMF_FORSESSION | TF_IPPMF_ENABLEPROFILE);
            if (FAILED(hr)) {
                hr = mgr->lpVtbl->ActivateProfile(mgr, TF_PROFILETYPE_INPUTPROCESSOR, HUNMIN_LANGID, &kHunminTsfClsid, &kHunminProfileGuid, NULL,
                                                  TF_IPPMF_FORSYSTEMALL | TF_IPPMF_ENABLEPROFILE);
            }
            if (FAILED(hr)) {
                hr = mgr->lpVtbl->ActivateProfile(mgr, TF_PROFILETYPE_INPUTPROCESSOR, HUNMIN_LANGID, &kHunminTsfClsid, &kHunminProfileGuid, NULL, 0);
            }
            mgr->lpVtbl->Release(mgr);
        }
        if (FAILED(hr)) {
            hr = profiles->lpVtbl->ActivateLanguageProfile(profiles, &kHunminTsfClsid, HUNMIN_LANGID, &kHunminProfileGuid);
        }
        profiles->lpVtbl->Release(profiles);
    } else {
        hr = createHr;
    }
    if (log) {
        fprintf(log, "ActivateProfile(Hunmin, 0x0412) hr=0x%08lX mgrHr=0x%08lX CoCreate hr=0x%08lX\n", (unsigned long)hr, (unsigned long)mgrHr,
                (unsigned long)createHr);
        fclose(log);
    }
}

#define WC_PROBE L"HunminImeProbeWnd"
#define IDC_EDIT 1
#define WM_APP_ACTIVATE (WM_APP + 1)

static HWND g_edit;
static HWND g_wnd;
static wchar_t g_auto_keys[1024];
static wchar_t g_auto_expect[1024];
static wchar_t g_auto_out[MAX_PATH];
static int g_auto_has_keys;
static int g_profileRequested;
static volatile LONG g_profileDone;
static void write_escaped(FILE *f, const wchar_t *s); /* defined below */

static wchar_t g_script[2048];
static wchar_t g_trace_path[MAX_PATH];
static int g_edit_rich;

static void copy_arg(wchar_t *dst, size_t cap, LPCWSTR src) { wcsncpy_s(dst, cap, src, _TRUNCATE); }

static void copy_unescaped_arg(wchar_t *dst, size_t cap, LPCWSTR src) {
    size_t o = 0;
    while (*src && o + 1 < cap) {
        if (*src == L'\\' && src[1]) {
            wchar_t e = src[1];
            src += 2;
            if (e == L'n')
                dst[o++] = L'\n';
            else if (e == L'r')
                dst[o++] = L'\r';
            else if (e == L't')
                dst[o++] = L'\t';
            else if (e == L'\\')
                dst[o++] = L'\\';
            else {
                dst[o++] = L'\\';
                if (o + 1 < cap)
                    dst[o++] = e;
            }
        } else {
            dst[o++] = *src++;
        }
    }
    dst[o] = 0;
}

static void parse_auto_args(void) {
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv)
        return;
    for (int i = 1; i + 1 < argc; ++i) {
        if (!wcscmp(argv[i], L"--keys")) {
            copy_arg(g_auto_keys, _countof(g_auto_keys), argv[++i]);
            g_auto_has_keys = 1;
        } else if (!wcscmp(argv[i], L"--expect")) {
            copy_unescaped_arg(g_auto_expect, _countof(g_auto_expect), argv[++i]);
        } else if (!wcscmp(argv[i], L"--out")) {
            copy_arg(g_auto_out, _countof(g_auto_out), argv[++i]);
        } else if (!wcscmp(argv[i], L"--script")) {
            copy_unescaped_arg(g_script, _countof(g_script), argv[++i]);
        } else if (!wcscmp(argv[i], L"--trace")) {
            copy_arg(g_trace_path, _countof(g_trace_path), argv[++i]);
        }
    }
    LocalFree(argv);
    if (!g_auto_out[0])
        wcscpy_s(g_auto_out, _countof(g_auto_out), L"probe-result.txt");
}

static WORD vk_for_char(wchar_t c) {
    if (c >= L'a' && c <= L'z')
        return (WORD)(L'A' + (c - L'a'));
    if (c >= L'A' && c <= L'Z')
        return (WORD)c;
    if (c >= L'0' && c <= L'9')
        return (WORD)c;
    if (c == L' ')
        return VK_SPACE;
    if (c == L'\n' || c == L'\r')
        return VK_RETURN;
    if (c == L'~')
        return VK_HANJA;
    return 0;
}

static BOOL next_key(const wchar_t **pp, WORD *vk, BOOL *shift) {
    const wchar_t *p = *pp;
    if (!*p)
        return FALSE;
    *shift = FALSE;
    if (*p == L'\\') {
        wchar_t e = p[1];
        if (!e)
            return FALSE;
        if (e == L'b') {
            *vk = VK_BACK;
            p += 2;
        } else if (e == L'n') {
            *vk = VK_RETURN;
            p += 2;
        } else if (e == L't') {
            *vk = VK_TAB;
            p += 2;
        } else if (e == L's' && p[2]) {
            *shift = TRUE;
            *vk = vk_for_char(p[2]);
            p += 3;
        } else {
            *vk = vk_for_char(e);
            p += 2;
        }
    } else {
        *vk = vk_for_char(*p);
        p += 1;
    }
    *pp = p;
    return TRUE;
}

static void send_key(WORD vk, BOOL shift) {
    INPUT in[4];
    int n = 0;
    ZeroMemory(in, sizeof(in));
    if (shift) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = VK_SHIFT;
        in[n].ki.wScan = (WORD)MapVirtualKeyW(VK_SHIFT, MAPVK_VK_TO_VSC);
        n++;
    }
    in[n].type = INPUT_KEYBOARD;
    in[n].ki.wVk = vk;
    in[n].ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    n++;
    in[n].type = INPUT_KEYBOARD;
    in[n].ki.wVk = vk;
    in[n].ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    in[n].ki.dwFlags = KEYEVENTF_KEYUP;
    n++;
    if (shift) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = VK_SHIFT;
        in[n].ki.wScan = (WORD)MapVirtualKeyW(VK_SHIFT, MAPVK_VK_TO_VSC);
        in[n].ki.dwFlags = KEYEVENTF_KEYUP;
        n++;
    }
    SendInput((UINT)n, in, sizeof(INPUT));
}

/* ---- measurement mode (issue #216) ---------------------------------------
 * The live round for the automatic switch needs numbers a text-only readback
 * cannot give: where the caret sits after each key, what one undo step
 * removes, and what becomes of a selection while the ENGINE is the one
 * delivering the characters. This mode replays a scripted key list and writes
 * one trace row per step, reading the control IN-PROCESS (EM_EXGETSEL,
 * GetCaretPos) rather than through cross-process messages.
 *
 *   ime-probe.exe --script "\sEnglishsms " --trace round.txt
 *
 * Script syntax: the ordinary --keys characters and escapes, plus %o tokens --
 *   %u Ctrl+Z   %y Ctrl+Y   %a Ctrl+A
 *   %l Left     %r Right    %h Shift+Left   %j Shift+Right
 *   %g Home     %e End      %m mark (a trace row, no input)
 * The TIP's policy still comes from the environment (HUNMIN_TSF_AUTOSWITCH,
 * HUNMIN_TSF_COMMIT_UNIT, HUNMIN_DATA_DIR), so one exe runs both rounds.
 */

#ifndef EM_EXGETSEL
#define EM_EXGETSEL (WM_USER + 52)
#endif

typedef struct probe_charrange {
    LONG cpMin;
    LONG cpMax;
} probe_charrange;

enum {
    TOK_KEY = 0,
    TOK_UNDO,
    TOK_REDO,
    TOK_SELALL,
    TOK_LEFT,
    TOK_RIGHT,
    TOK_SHIFTLEFT,
    TOK_SHIFTRIGHT,
    TOK_HOME,
    TOK_END,
    TOK_HANGUL,
    TOK_MARK
};

static void send_key_mod(WORD vk, BOOL shift, BOOL ctrl) {
    INPUT in[6];
    int n = 0;
    ZeroMemory(in, sizeof(in));
    if (ctrl) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = VK_CONTROL;
        in[n].ki.wScan = (WORD)MapVirtualKeyW(VK_CONTROL, MAPVK_VK_TO_VSC);
        n++;
    }
    if (shift) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = VK_SHIFT;
        in[n].ki.wScan = (WORD)MapVirtualKeyW(VK_SHIFT, MAPVK_VK_TO_VSC);
        n++;
    }
    in[n].type = INPUT_KEYBOARD;
    in[n].ki.wVk = vk;
    in[n].ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    n++;
    in[n].type = INPUT_KEYBOARD;
    in[n].ki.wVk = vk;
    in[n].ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    in[n].ki.dwFlags = KEYEVENTF_KEYUP;
    n++;
    if (shift) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = VK_SHIFT;
        in[n].ki.wScan = (WORD)MapVirtualKeyW(VK_SHIFT, MAPVK_VK_TO_VSC);
        in[n].ki.dwFlags = KEYEVENTF_KEYUP;
        n++;
    }
    if (ctrl) {
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = VK_CONTROL;
        in[n].ki.wScan = (WORD)MapVirtualKeyW(VK_CONTROL, MAPVK_VK_TO_VSC);
        in[n].ki.dwFlags = KEYEVENTF_KEYUP;
        n++;
    }
    SendInput((UINT)n, in, sizeof(INPUT));
}

/* The document as the control holds it, the selection, and the caret. A
 * caret-less read (GetCaretPos FALSE) is written as -1,-1 rather than 0,0 --
 * 0,0 is a real place and would read as a measurement. */
static void probe_state(wchar_t *text, int cap, long *sel_start, long *sel_end, POINT *caret) {
    GetWindowTextW(g_edit, text, cap);
    *sel_start = -1;
    *sel_end = -1;
    if (g_edit_rich) {
        probe_charrange cr;
        cr.cpMin = -1;
        cr.cpMax = -1;
        SendMessageW(g_edit, EM_EXGETSEL, 0, (LPARAM)&cr);
        *sel_start = cr.cpMin;
        *sel_end = cr.cpMax;
    } else {
        DWORD r = (DWORD)SendMessageW(g_edit, EM_GETSEL, 0, 0);
        *sel_start = (long)LOWORD(r);
        *sel_end = (long)HIWORD(r);
    }
    /* The caret rect of the THREAD that owns the focus window. GetCaretPos
     * answered 0,0 on every step of the first exploration run (2026-11-02)
     * while the selection advanced, so it is not the source read here. */
    caret->x = -1;
    caret->y = -1;
    {
        GUITHREADINFO gui;
        ZeroMemory(&gui, sizeof(gui));
        gui.cbSize = sizeof(gui);
        if (GetGUIThreadInfo(GetCurrentThreadId(), &gui) && gui.hwndCaret == g_edit) {
            caret->x = gui.rcCaret.left;
            caret->y = gui.rcCaret.top;
        }
    }
}

static void trace_row(FILE *f, int step, const wchar_t *tok) {
    wchar_t text[4096];
    long s = -1, e = -1;
    POINT pt;
    text[0] = 0;
    probe_state(text, (int)_countof(text), &s, &e, &pt);
    fwprintf(f, L"step=%d tok=", step);
    write_escaped(f, tok);
    fwprintf(f, L" len=%d sel=%ld,%ld caret=%ld,%ld text=", (int)wcslen(text), s, e, (long)pt.x, (long)pt.y);
    write_escaped(f, text);
    fwprintf(f, L"\n");
    fflush(f);
}

/* One script token: an ordinary key (via next_key, so the --keys escapes keep
 * working) or a %o control. Returns FALSE at the end of the script. */
static BOOL next_token(const wchar_t **pp, int *op, WORD *vk, BOOL *shift, wchar_t *shown) {
    const wchar_t *p = *pp;
    *op = TOK_KEY;
    *vk = 0;
    *shift = FALSE;
    shown[0] = 0;
    if (!*p)
        return FALSE;
    if (*p == L'%' && p[1]) {
        wchar_t c = p[1];
        p += 2;
        switch (c) {
        case L'u':
            *op = TOK_UNDO;
            break;
        case L'y':
            *op = TOK_REDO;
            break;
        case L'a':
            *op = TOK_SELALL;
            break;
        case L'l':
            *op = TOK_LEFT;
            break;
        case L'r':
            *op = TOK_RIGHT;
            break;
        case L'h':
            *op = TOK_SHIFTLEFT;
            break;
        case L'j':
            *op = TOK_SHIFTRIGHT;
            break;
        case L'g':
            *op = TOK_HOME;
            break;
        case L'e':
            *op = TOK_END;
            break;
        case L'k':
            *op = TOK_HANGUL;
            break;
        default:
            *op = TOK_MARK;
            break;
        }
        swprintf_s(shown, 8, L"%%%c", c);
    } else {
        if (!next_key(&p, vk, shift))
            return FALSE;
        swprintf_s(shown, 16, L"vk=%02X%s", (unsigned)*vk, *shift ? L"+shift" : L"");
    }
    *pp = p;
    return TRUE;
}

static void force_foreground(HWND wnd) {
    HWND fg = GetForegroundWindow();
    DWORD fgThread = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
    DWORD thisThread = GetCurrentThreadId();
    if (fgThread && fgThread != thisThread)
        AttachThreadInput(fgThread, thisThread, TRUE);
    SetForegroundWindow(wnd);
    SetFocus(g_edit);
    if (fgThread && fgThread != thisThread)
        AttachThreadInput(fgThread, thisThread, FALSE);
}

static void write_escaped(FILE *f, const wchar_t *s) {
    for (; *s; ++s) {
        if (*s == L'\n')
            fwprintf(f, L"\\n");
        else if (*s == L'\r')
            fwprintf(f, L"\\r");
        else if (*s == L'\t')
            fwprintf(f, L"\\t");
        else if (*s == L'\\')
            fwprintf(f, L"\\\\");
        else
            fwprintf(f, L"%lc", *s);
    }
}

static int text_equal(const wchar_t *a, const wchar_t *b) {
    for (;;) {
        if (*a == L'\r' && a[1] == L'\n')
            a++;
        if (*b == L'\r' && b[1] == L'\n')
            b++;
        if (*a != *b)
            return 0;
        if (!*a)
            return 1;
        a++;
        b++;
    }
}

static void write_auto_result(const wchar_t *text) {
    FILE *f = _wfopen(g_auto_out, L"w, ccs=UTF-8");
    if (!f)
        return;
    int pass = !g_auto_expect[0] || text_equal(text, g_auto_expect);
    fwprintf(f, L"keys=");
    write_escaped(f, g_auto_keys);
    fwprintf(f, L"\ntext=");
    write_escaped(f, text);
    fwprintf(f, L"\nexpect=");
    write_escaped(f, g_auto_expect);
    fwprintf(f, L"\npass=%d\n", pass);
    fclose(f);
}

static DWORD WINAPI auto_thread(LPVOID param) {
    (void)param;
    Sleep(800);
    ShowWindow(g_wnd, SW_SHOWNORMAL);
    /* A fresh Sandbox can still be bringing up Explorer when the first keys
     * are sent; wait for the window to own the foreground and for the TIP
     * activation posted by wWinMain to finish. Without this, the same probe
     * sometimes typed into no window and wrote an empty result (measured
     * 2026-10-24). */
    for (int i = 0; i < 50; ++i) {
        force_foreground(g_wnd);
        if (GetForegroundWindow() == g_wnd && g_profileDone)
            break;
        Sleep(100);
    }
    Sleep(200);
    {
        FILE *log = _wfopen(L"ime-probe.log", L"a");
        if (log) {
            fprintf(log, "auto foreground=%d profile=%ld\n", GetForegroundWindow() == g_wnd ? 1 : 0,
                    (long)InterlockedCompareExchange(&g_profileDone, 0, 0));
            fclose(log);
        }
    }
    const wchar_t *p = g_auto_keys;
    WORD vk = 0;
    BOOL shift = FALSE;
    while (next_key(&p, &vk, &shift)) {
        if (!vk)
            continue;
        send_key(vk, shift);
        Sleep(80);
    }
    Sleep(700);
    {
        wchar_t text[4096] = {0};
        GetWindowTextW(g_edit, text, (int)_countof(text));
        write_auto_result(text);
    }
    PostMessageW(g_wnd, WM_CLOSE, 0, 0);
    return 0;
}

/* The measurement run: the same bring-up as auto_thread (foreground guard,
 * profile activation), then one trace row per script token. Undo/redo get a
 * longer settle because the TIP's own edit session can land late. */
static DWORD WINAPI measure_thread(LPVOID param) {
    (void)param;
    Sleep(800);
    ShowWindow(g_wnd, SW_SHOWNORMAL);
    for (int i = 0; i < 50; ++i) {
        force_foreground(g_wnd);
        if (GetForegroundWindow() == g_wnd && g_profileDone)
            break;
        Sleep(100);
    }
    Sleep(200);
    {
        FILE *log = _wfopen(L"ime-probe.log", L"a");
        if (log) {
            fprintf(log, "measure foreground=%d profile=%ld scriptlen=%d\n", GetForegroundWindow() == g_wnd ? 1 : 0,
                    (long)InterlockedCompareExchange(&g_profileDone, 0, 0), (int)wcslen(g_script));
            fclose(log);
        }
    }
    FILE *tr = _wfopen(g_trace_path[0] ? g_trace_path : L"ime-probe-trace.txt", L"w, ccs=UTF-8");
    if (!tr) {
        PostMessageW(g_wnd, WM_CLOSE, 0, 0);
        return 0;
    }
    const wchar_t *p = g_script;
    int step = 0;
    trace_row(tr, step++, L"START");
    for (;;) {
        int op = TOK_KEY;
        WORD vk = 0;
        BOOL shift = FALSE;
        wchar_t shown[16];
        if (!next_token(&p, &op, &vk, &shift, shown))
            break;
        switch (op) {
        case TOK_KEY:
            if (vk)
                send_key_mod(vk, shift, FALSE);
            break;
        case TOK_UNDO:
            send_key_mod((WORD)L'Z', FALSE, TRUE);
            break;
        case TOK_REDO:
            send_key_mod((WORD)L'Y', FALSE, TRUE);
            break;
        case TOK_SELALL:
            send_key_mod((WORD)L'A', FALSE, TRUE);
            break;
        case TOK_LEFT:
            send_key_mod(VK_LEFT, FALSE, FALSE);
            break;
        case TOK_RIGHT:
            send_key_mod(VK_RIGHT, FALSE, FALSE);
            break;
        case TOK_SHIFTLEFT:
            send_key_mod(VK_LEFT, TRUE, FALSE);
            break;
        case TOK_SHIFTRIGHT:
            send_key_mod(VK_RIGHT, TRUE, FALSE);
            break;
        case TOK_HOME:
            send_key_mod(VK_HOME, FALSE, FALSE);
            break;
        case TOK_END:
            send_key_mod(VK_END, FALSE, FALSE);
            break;
        case TOK_HANGUL:
            /* The 한/영 key: the TIP's own toggle, not a document edit. */
            send_key_mod(VK_HANGUL, FALSE, FALSE);
            break;
        default:
            break;
        }
        if (op == TOK_UNDO || op == TOK_REDO)
            Sleep(350);
        else
            Sleep(90);
        trace_row(tr, step++, shown);
    }
    fclose(tr);
    PostMessageW(g_wnd, WM_CLOSE, 0, 0);
    return 0;
}

static void resize_edit(HWND wnd) {
    RECT rc;
    GetClientRect(wnd, &rc);
    MoveWindow(g_edit, 0, 0, rc.right, rc.bottom, TRUE);
}

static LRESULT CALLBACK probe_wndproc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        resize_edit(wnd);
        return 0;
    case WM_SETFOCUS:
        SetFocus(g_edit);
        return 0;
    case WM_APP_ACTIVATE:
        if (!g_profileRequested) {
            g_profileRequested = 1;
            activate_hunmin_profile();
            InterlockedExchange(&g_profileDone, 1);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(wnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show) {
    (void)prev;
    (void)cmd;

    parse_auto_args();
    redirect_log_to_probe_dir();

    /* msftedit.dll must be loaded before the RICHEDIT50W class exists. */
#ifdef FORCE_EDIT_HOST
    /* Bisect (issue #99 round 13): same window, plain EDIT control — the
     * system's default TSF sink instead of msftedit's. Isolates whether the
     * per-key composition termination is an msftedit behavior. */
    HMODULE msftedit = NULL;
    PCWSTR edit_class = L"EDIT";
#else
    HMODULE msftedit = LoadLibraryW(L"msftedit.dll");
    PCWSTR edit_class = msftedit ? L"RICHEDIT50W" : L"EDIT";
#endif

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = probe_wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wc.hIcon = LoadIcon(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = WC_PROBE;
    RegisterClassW(&wc);

    /* Title names the control actually used, so a round log is unambiguous. */
    HWND wnd = CreateWindowW(WC_PROBE,
                             msftedit ? L"Hunmin IME probe (RICHEDIT50W -- TSF inline composition)"
                                      : L"Hunmin IME probe (EDIT fallback -- system floating preedit)",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 400, NULL, NULL, inst, NULL);
    if (!wnd)
        return 1;

    g_edit = CreateWindowW(edit_class, NULL,
                           WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN, 0, 0, 0,
                           0, wnd, (HMENU)IDC_EDIT, inst, NULL);
    if (!g_edit)
        return 1;
    g_edit_rich = msftedit ? 1 : 0;

    SendMessageW(g_edit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    resize_edit(wnd);
    g_wnd = wnd;
    ShowWindow(wnd, show);
    SetFocus(g_edit);
    force_foreground(wnd);
    /* Activate only after the window is visible and focused: a profile
     * activation before the edit control owns a TSF document fails on this
     * host (measured 2026-10-24). The message loop runs it on this thread. */
    PostMessageW(wnd, WM_APP_ACTIVATE, 0, 0);

    if (g_script[0]) {
        HANDLE th = CreateThread(NULL, 0, measure_thread, NULL, 0, NULL);
        if (th)
            CloseHandle(th);
    } else if (g_auto_has_keys) {
        HANDLE th = CreateThread(NULL, 0, auto_thread, NULL, 0, NULL);
        if (th)
            CloseHandle(th);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

#ifdef HUNMIN_PROBE_CONSOLE
/* The batch lane builds /subsystem:console so the sandbox runner can wait on
 * the process. Keep the same window and message loop; only the entry point
 * differs. */
int main(void) { return (int)wWinMain(GetModuleHandleW(NULL), NULL, GetCommandLineW(), SW_SHOWNORMAL); }
#endif
