#include <windows.h>

#include "log.h"

extern "C" HINSTANCE g_hunmin_hinst = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hunmin_hinst = hModule;
        hunmin_logger.log(
            "DllMain PROCESS_ATTACH pid=%lu hinst=%p\n",
            (unsigned long)GetCurrentProcessId(), (void *)hModule);
    }

    (void)lpReserved;

    return TRUE;
}
