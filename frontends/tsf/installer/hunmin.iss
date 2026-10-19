; ---------------------------------------------------------------------------
; Hunmin IME 0.2 installer (issue #99) — Inno Setup 6 script.
;
; Built by tools/build-windows-installer.sh, which stages the release tree
; and invokes:
;   ISCC.exe /DAppVersion=<ver> /DStageDir=<stage> /O<outdir> hunmin.iss
;
; The staged tree carries:
;   hunmin-tsf.dll    the TIP. The engine is static-linked inside it (the
;                     top-level CMakeLists note: the static archive is not
;                     an installed artifact) and the CRT is static-linked
;                     (/MT, HUNMIN_MSVC_STATIC_RUNTIME=ON) so a clean
;                     Windows image needs no VC redist — the clean-VM field
;                     test root-caused the /MD build's MSVCP140.dll /
;                     VCRUNTIME140.dll imports to exactly that failure.
;   data\             libhunmin/data shipped whole. krdict stays its own
;                     directory tree (CC-BY-SA ShareAlike isolation), and
;                     the TIP loader resolves <module>\data first
;                     (hunmin_ctx_from_module, frontends/tsf/src/tsf.cpp),
;                     so the installed IME serves candidates from the
;                     installed tables.
;   settings-ui\      the self-contained WinUI 3 settings exe publish
;                     (issue #71's ruled settings surface; the interim
;                     dialog stays reachable as the rundll32 fallback).
;
; This file is UTF-8: the Start-menu entry name carries Korean.
; ---------------------------------------------------------------------------

#define AppName "Hunmin IME"
#ifndef AppVersion
#define AppVersion "0.2.0"
#endif
#ifndef StageDir
#define StageDir "..\..\..\build-msvc\installer\stage"
#endif

[Setup]
; Add/Remove Programs identity. Arbitrary but fixed: this is NOT the TIP's
; COM CLSID ({F2C70001-...}), which DllRegisterServer writes into HKCR/HKLM.
AppId={{2DC7D564-F8B2-49F2-9CF8-39B398AF9B63}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Seo Bon Keun
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
; The TIP DLL is 64-bit and registration runs the regsvr32 from {sys}:
; without 64-bit install mode {sys} resolves to SysWOW64 and the 32-bit
; regsvr32 would refuse to load the DLL.
ArchitecturesInstallIn64BitMode=x64compatible
; Registration writes HKLM (CLSID tree, CTF TIP tree, language profile) and
; the default dir is Program Files: the installer runs elevated.
PrivilegesRequired=admin
LicenseFile=licence-page.txt
SetupIconFile=..\assets\hunmin.ico
UninstallDisplayIcon={app}\hunmin-tsf.dll,0
OutputDir=.
OutputBaseFilename=hunmin-{#AppVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes

[Files]
Source: "{#StageDir}\hunmin-tsf.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\settings-ui\*"; DestDir: "{app}\settings-ui"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "licence-page.txt"; DestDir: "{app}"; DestName: "LICENCE.txt"; Flags: ignoreversion
Source: "..\..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
; The ruled settings surface (issue #99 item 3): the Start-menu entry opens
; the settings exe.
Name: "{group}\Hunmin IME 설정"; Filename: "{app}\settings-ui\hunmin-tsf-settingsui.exe"; WorkingDir: "{app}\settings-ui"

[UninstallRun]
; Mirrors registration (issue #99 item 3): DllUnregisterServer removes the
; full registered state since issue #84 — the CLSID tree, the HKLM CTF TIP
; tree, the two categories, the language profile. Inno runs [UninstallRun]
; entries BEFORE deleting files, so the DLL is still loadable here; the
; fresh-VM checklist re-measures the registry after this runs.
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\hunmin-tsf.dll"""; RunOnceId: "UnregHunminTIP"

[Code]
// The issue's registration step, run literally: regsvr32 on the INSTALLED
// DLL. DllRegisterServer writes the HKCR CLSID tree, the HKLM CTF TIP tree
// and the Korean language profile; the profile icon then points at the
// installed DLL's own path with icon index 0 (the pin the DLL itself
// rewrites after AddLanguageProfile, frontends/tsf/src/tsf.cpp). Silent
// first; on a nonzero exit, re-run visibly so the failing HRESULT is on
// screen, and fail the install loudly instead of leaving a half-installed
// IME whose profile points at a registration that never happened.
// Note: no line below may START with "[" — the section scanner splits
// sections on it before the Pascal compiler ever sees this block.
procedure CurStepChanged(CurStep: TSetupStep);
var
  DllPath, Params: string;
  ExitCode: Integer;
begin
  if CurStep <> ssPostInstall then
    Exit;
  DllPath := ExpandConstant('{app}\hunmin-tsf.dll');
  Params := '/s "' + DllPath + '"';
  if not Exec(ExpandConstant('{sys}\regsvr32.exe'), Params, '',
              SW_HIDE, ewWaitUntilTerminated, ExitCode) then
  begin
    MsgBox('regsvr32.exe could not be run; the IME is not registered.',
           mbCriticalError, MB_OK);
    RaiseException('regsvr32.exe could not be run');
  end;
  if ExitCode <> 0 then
  begin
    // Not silent, so the error dialog names the failing step.
    Exec(ExpandConstant('{sys}\regsvr32.exe'), '"' + DllPath + '"', '',
         SW_SHOW, ewWaitUntilTerminated, ExitCode);
    MsgBox(Format('Registration failed: regsvr32 exit code %d. ' +
                  'Nothing was registered; uninstall from ' +
                  'Add/Remove Programs to remove the copied files.', [ExitCode]),
           mbCriticalError, MB_OK);
    RaiseException(Format('regsvr32 exit code %d', [ExitCode]));
  end;
end;
