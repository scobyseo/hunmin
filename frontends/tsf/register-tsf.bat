@echo off
setlocal
taskkill /F /IM notepad.exe 2>nul
set "SCRIPTDIR=%~dp0"
pushd "%SCRIPTDIR%"
set "DLLDIR=%CD%\build-msvc\Release"
popd
set "DLL=%DLLDIR%\hunmin-tsf.dll"
if not exist "%DLL%" ( echo DLL not found: %DLL% & pause & exit /b 1 )
del /f /q "%DLLDIR%\hunmin-tsf-debug.log" "%DLLDIR%\hunmin-reg-debug.log" 2>nul
pushd "%DLLDIR%"
"C:\Windows\System32\regsvr32.exe" "%DLL%"
popd
echo(
echo [reg-debug.log]
type "%DLLDIR%\hunmin-reg-debug.log" 2>nul
echo(
echo Done. Press any key...
pause >nul