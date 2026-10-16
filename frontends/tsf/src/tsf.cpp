#include <windows.h>
#include <msctf.h>

// TODO: implement ITfTextInputProcessor / thread manager sink,
// referencing hunmin engine. Scaffold only.

extern "C" BOOL DllRegisterServer() { return TRUE; }
extern "C" BOOL DllUnregisterServer() { return TRUE; }
extern "C" const CLSID CLSID_HunminTSF = { 0 };