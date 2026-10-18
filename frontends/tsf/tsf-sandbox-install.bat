@echo off
rem the CLEAN-HOST install test (issue #99): run INSIDE Windows Sandbox.
set REDIST=%USERPROFILE%\Desktop\build-msvc\vc_redist.x64.exe
set DLL=%USERPROFILE%\Desktop\build-msvc\Release\hunmin-tsf.dll
set HUNMIN_TSF_LOG_DIR=%TEMP%
set HUNMIN_TSF_DEBUG_LOG=1
echo == %COMPUTERNAME% %DATE% %TIME% ==
del /q "%TEMP%\hunmin-reg-debug.log" >nul 2>&1
reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-0000-0000-0000-000000000001}" 2>nul && echo TIP-TREE-PREEXISTING-UNEXPECTED || echo TIP-TREE-CLEAN-OK
echo == the VC runtimes first (measured round 2: the DLL /MD-links
echo    MSVCP140/VCRUNTIME140(_1); a clean image has none of them -- the
echo    literal "지정된 모듈을 찾지 못했습니다" load failure) ==
"%REDIST%" /install /passive /norestart
echo vc_redist rc=%errorlevel%
regsvr32 "%DLL%"
echo regsvr32 rc=%errorlevel%
reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-0000-0000-0000-000000000001}\LanguageProfile\0x00000412" 2>&1
echo ----- registration log ("%TEMP%\hunmin-reg-debug.log") -----
type "%TEMP%\hunmin-reg-debug.log"
echo ----- end -----
start "" "%windir%\System32\notepad.exe"
