@echo off
taskkill /f /im explorer.exe 2>nul
start explorer.exe
taskkill /f /im notepad.exe 2>nul
taskkill /f /im ctfmon.exe 2>nul
set "SCRIPTDIR=%~dp0"
pushd "%SCRIPTDIR%"
set "DLLDIR=%CD%\build-msvc\Release"
popd
set "DLL=%DLLDIR%\hunmin-tsf.dll"
if not exist "%DLL%" ( echo DLL not found: %DLL% & pause & exit /b 1 )
pushd "%DLLDIR%"
"C:\Windows\System32\regsvr32.exe" /u "%DLL%"
popd
echo Done. Press any key...
pause >nul