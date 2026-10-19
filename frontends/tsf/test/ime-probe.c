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
 * Build (MSVC, statically linked CRT -- the clean image has no VC redist,
 * the same lesson as the /MT fix on the TIP itself):
 *   cl /nologo /W4 /O2 /DUNICODE /D_UNICODE ime-probe.c /Fe:ime-probe.exe ^
 *       /link /subsystem:windows user32.lib gdi32.lib ole32.lib uuid.lib
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
 * ITfInputProcessorProfiles::ActivateLanguageProfile -- the same call the
 * TIP's own DllRegisterServer makes (tsf.cpp). Round 7 (2026-10-20) measured
 * that activation did not survive the round: the ko-KR language list was
 * added AFTER the install, which can reset the active processor to the MS
 * IME default, and the driver's Win+Space taps never landed on Hunmin (raw
 * "dkssud" committed, tray showing only A and the Hangul glyph). Activation
 * here is the deterministic path; its result is logged, not assumed.
 */

#include <windows.h>
#include <msctf.h>
#include <stdio.h>

/* Same constants as the TIP itself (frontends/tsf/src/tsf.cpp). */
static const CLSID kHunminTsfClsid = {0xF2C70001, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}};
static const GUID kHunminProfileGuid = {0xF2C70002, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}};
#define HUNMIN_LANGID 0x0412

static void activate_hunmin_profile(void)
{
    FILE *log = _wfopen(L"ime-probe.log", L"a");
    HRESULT hr = E_FAIL, createHr = E_FAIL;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    ITfInputProcessorProfiles *profiles = NULL;
    createHr = CoCreateInstance(&CLSID_TF_InputProcessorProfiles, NULL,
                                CLSCTX_INPROC_SERVER,
                                &IID_ITfInputProcessorProfiles, (void **)&profiles);
    if (createHr == S_OK && profiles) {
        hr = profiles->lpVtbl->ActivateLanguageProfile(
            profiles, &kHunminTsfClsid, HUNMIN_LANGID, &kHunminProfileGuid);
        profiles->lpVtbl->Release(profiles);
    } else {
        hr = createHr;
    }
    if (log) {
        fprintf(log, "ActivateLanguageProfile(Hunmin, 0x0412) hr=0x%08lX CoCreate hr=0x%08lX\n",
                (unsigned long)hr, (unsigned long)createHr);
        fclose(log);
    }
}

#define WC_PROBE L"HunminImeProbeWnd"
#define IDC_EDIT 1

static HWND g_edit;

static void resize_edit(HWND wnd)
{
    RECT rc;
    GetClientRect(wnd, &rc);
    MoveWindow(g_edit, 0, 0, rc.right, rc.bottom, TRUE);
}

static LRESULT CALLBACK probe_wndproc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZE:
        resize_edit(wnd);
        return 0;
    case WM_SETFOCUS:
        SetFocus(g_edit);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(wnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    (void)prev; (void)cmd;

    /* msftedit.dll must be loaded before the RICHEDIT50W class exists. */
    HMODULE msftedit = LoadLibraryW(L"msftedit.dll");
    PCWSTR edit_class = msftedit ? L"RICHEDIT50W" : L"EDIT";

    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = probe_wndproc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursor(NULL, IDC_IBEAM);
    wc.hIcon         = LoadIcon(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = WC_PROBE;
    RegisterClassW(&wc);

    /* Title names the control actually used, so a round log is unambiguous. */
    HWND wnd = CreateWindowW(WC_PROBE,
        msftedit ? L"Hunmin IME probe (RICHEDIT50W -- TSF inline composition)"
                 : L"Hunmin IME probe (EDIT fallback -- system floating preedit)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 400,
        NULL, NULL, inst, NULL);
    if (!wnd)
        return 1;

    g_edit = CreateWindowW(edit_class, NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
        0, 0, 0, 0, wnd, (HMENU)IDC_EDIT, inst, NULL);
    if (!g_edit)
        return 1;

    SendMessageW(g_edit, WM_SETFONT,
        (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    resize_edit(wnd);
    activate_hunmin_profile();
    ShowWindow(wnd, show);
    SetFocus(g_edit);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
