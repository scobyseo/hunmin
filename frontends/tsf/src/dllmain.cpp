#include <windows.h>
#include <cstdio>

extern "C" HINSTANCE g_hunmin_hinst = nullptr;

extern bool hunmin_tsf_log_path(const char *fname, char *out, size_t cap);

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hunmin_hinst = hModule;
        char logp[MAX_PATH];
        FILE *f =
            hunmin_tsf_log_path("hunmin-tsf-debug.log", logp, sizeof(logp)) ? fopen(logp, "a") : nullptr;
        if (f) { fprintf(f, "DllMain PROCESS_ATTACH pid=%lu hinst=%p\n", (unsigned long)GetCurrentProcessId(), (void*)hModule); fclose(f); }
    }
    (void)lpReserved;
    return TRUE;
}
