@echo off
rem the CLEAN-HOST install test (issue #99): run INSIDE Windows Sandbox.
rem Everything runs from the read-only mapped build tree.
set DLL=C:\Users\WDIGUtilityAccount2\Desktop\build-msvc\Release\hunmin-tsf.dll
set DLL=%USERPROFILE%\Desktop\build-msvc\Release\hunmin-tsf.dll
echo == %COMPUTERNAME% %DATE% %TIME% ==
reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-0000-0000-0000-000000000001}" 2>nul && echo TIP-TREE-PREEXISTING-UNEXPECTED || echo TIP-TREE-CLEAN-OK
regsvr32 /s "%DLL%"
echo regsvr32 rc=%errorlevel%
reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-0000-0000-0000-000000000001}\LanguageProfile\0x00000412" /v IconIndex
reg query HKCU\Software\Microsoft\CTF\SortOrder\AssemblyItem\0x00000412 2>nul | findstr /i Hunmin
echo == next: Win+Space to Hunmin in a text window and type dkssud ?
echo committext should be ¾È³ç (word candidates from the STAGED data),
echo the Sejong icon shows in the language list, the tray badge may not
echo switch (measured not honored for third-party TIPs on this build).
start notepad
