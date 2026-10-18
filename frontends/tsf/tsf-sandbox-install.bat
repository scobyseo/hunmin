@echo off
rem the CLEAN-HOST install test (issue #99): run INSIDE Windows Sandbox.
set DLL=%USERPROFILE%\Desktop\build-msvc\Release\hunmin-tsf.dll
set HUNMIN_TSF_LOG_DIR=%TEMP%
set HUNMIN_TSF_DEBUG_LOG=1
echo == %COMPUTERNAME% %DATE% %TIME% ==
del /q "%TEMP%\hunmin-reg-debug.log" >nul 2>&1
reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-0000-0000-0000-000000000001}" 2>nul && echo TIP-TREE-PREEXISTING-UNEXPECTED || echo TIP-TREE-CLEAN-OK
regsvr32 "%DLL%"
echo regsvr32 rc=%errorlevel%
reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-0000-0000-0000-000000000001}\LanguageProfile\0x00000412" 2>&1
echo ----- registration log ("%TEMP%\hunmin-reg-debug.log") -----
type "%TEMP%\hunmin-reg-debug.log"
echo ----- end -----
echo next: close the registration dialog if one appeared, then
echo Win+Space to Hunmin and type dkssud in the text window.
start "" "%windir%\System32\notepad.exe"
