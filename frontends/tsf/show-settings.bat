@echo off
rem Launcher for the hunmin TSF settings dialog (issue #71). Direct path,
rem no TSF plumbing needed; the dialog writes the HKCU store the running
rem adapter reads at its next Activate.
rem
rem The DLL is resolved clone-independently: the target beside this script
rem (%~dp0build-msvc\Release\hunmin-tsf.dll) wins, else the registered
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
pushd "%~dp0"
start "" rundll32 "%DLL%",HunminSettingsShow
popd
