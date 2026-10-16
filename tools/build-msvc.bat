@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
cd /d C:\Workspace\hunmin\windows-milestone-a-tsf-frontend-scaffold
cmake -S . -B build-msvc -G "Ninja" -DBUILD_TESTING=OFF
if errorlevel 1 exit /b 1
cmake --build build-msvc
if errorlevel 1 exit /b 1
cd /d C:\Workspace\hunmin\windows-milestone-a-tsf-frontend-scaffold\frontends\tsf
cmake -S . -B build-msvc -G "Ninja" -DHUNMIN_LIB=C:\Workspace\hunmin\windows-milestone-a-tsf-frontend-scaffold\build-msvc\hunmin.lib
if errorlevel 1 exit /b 1
cmake --build build-msvc