@echo off
rem Launcher for the hunmin TSF settings UI (issue #71, maintainer ruling
rem 2026-10-17): the EXE surface is the WinUI 3 settings application
rem (frontends/tsf/settings-ui). The interim Win32 dialog (rundll32
rem HunminSettingsShow) stays as the fallback until it is removed.
rem
rem The DLL (and with it the exe directory) is resolved clone-independently:
rem the target beside this script (%~dp0build-msvc\) wins, else the registered
rem InprocServer32 path. Nothing here depends on a checkout location.
setlocal
set "DLL=%~dp0build-msvc\Release\hunmin-tsf.dll"
rem Ninja (single-config) puts the DLL one level up from Release/.
if not exist "%DLL%" set "DLL=%~dp0build-msvc\hunmin-tsf.dll"
if not exist "%DLL%" (
  for /f "tokens=2*" %%a in ('reg query "HKCR\CLSID\{F2C70001-0000-0000-0000-000000000001}\InprocServer32" /ve 2^>nul ^| findstr /i "REG_SZ"') do set "DLL=%%b"
)
if not exist "%DLL%" (
  echo hunmin-tsf.dll not found ^(build frontends/tsf first, or re-register the TIP^) 1>&2
  exit /b 1
)
for %%I in ("%DLL%") do set "DLLDIR=%%~dpI"
rem First hit wins == the DLL-side contract (docs/WINDOWS-TSF-CONFIG.md):
rem <dll dir>\settings-ui\publish\, else the dev-checkout <dll dir>\..\settings-ui\publish\.
set "EXE=%DLLDIR%settings-ui\publish\hunmin-tsf-settingsui.exe"
if not exist "%EXE%" set "EXE=%DLLDIR%..\settings-ui\publish\hunmin-tsf-settingsui.exe"
if exist "%EXE%" (
  start "" "%EXE%"
  exit /b 0
)
rem Exe fallback: the interim dialog (removed when the Win32 TU goes).
start "" rundll32 "%DLL%",HunminSettingsShow
endlocal
