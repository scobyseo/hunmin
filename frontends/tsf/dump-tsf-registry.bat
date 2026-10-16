@echo off
setlocal
set "GUID={F2C70001-0000-0000-0000-000000000001}"
echo === CLSID ===
reg query "HKCR\CLSID\%GUID%" /s 2>&1
echo.
echo === CTF TIP ===
reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\%GUID%" /s 2>&1
echo.
echo === Assemblies 0412 ===
reg query "HKLM\SOFTWARE\Microsoft\CTF\Assemblies\0x0412\%GUID%" /s 2>&1
pause