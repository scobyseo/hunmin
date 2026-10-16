#include <windows.h>
#include <cstdio>

extern "C" HINSTANCE g_hunmin_hinst = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hunmin_hinst = hModule;
        FILE* f = fopen("C:\\hunmin-tsf-debug.log", "a");
        if (f) { fprintf(f, "DllMain PROCESS_ATTACH pid=%lu hinst=%p\\n", (unsigned long)GetCurrentProcessId(), (void*)hModule); fclose(f); }
    }
    (void)lpReserved;
    return TRUE;
}