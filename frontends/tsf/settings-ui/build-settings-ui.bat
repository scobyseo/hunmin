@echo off
rem Build the settings exe (issue #71) into the publish layout beside the DLL:
rem   frontends/tsf/settings-ui/publish  <- what ITfFnConfigure / show-settings.bat
rem resolve (<exe dir>\settings-ui). Run from anywhere; paths are script-relative.
setlocal
set HERE=%~dp0
set MSBUILD="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
%MSBUILD% "%HERE%HunminTsfSettingsUI.csproj" ^
  -t:Restore;Publish ^
  -p:Configuration=Release ^
  -p:RuntimeIdentifier=win-x64 ^
  -p:SelfContained=true ^
  -p:WindowsAppSDKSelfContained=true ^
  -p:PublishDir="%HERE%publish"
endlocal
