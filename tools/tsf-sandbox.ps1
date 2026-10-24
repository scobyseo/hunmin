<#
.SYNOPSIS
  Build a TSF live-test kit and run one key sequence in Windows Sandbox.

.DESCRIPTION
  The dev host has a product install registered; a live TSF test must never
  touch it (docs/FRONTEND-EQUIVALENCE.md section 7). This script builds the
  engine and TIP with the static CRT, stages them beside libhunmin/data and
  ime-probe.exe, then boots a disposable Windows Sandbox with the kit as a
  read-only mapped folder and a writable out folder.

  The guest registers the staged DLL, runs the probe in batch mode, copies the
  TIP and probe logs back, unregisters, and writes done.txt. The host polls the
  out folder and reports pass=1 from result.txt.

  -Keys escapes: \b backspace, \n enter, \t tab, \s<char> shift+char, \\ literal
  backslash. A space is a literal space. -Expect accepts \n, \r, \t, \\.

.EXAMPLE
  tools\tsf-sandbox.ps1 -Keys dkssud -Expect 안녕 -Layout dubeolsik
  tools\tsf-sandbox.ps1 -Keys dkssud1 -Expect 안녕1 -NoBuild
#>
[CmdletBinding()]
param(
    [string]$Keys = 'dkssud gksrmf\b\bdl \b\n',
    [string]$Expect = ([string][char]0xC548 + [char]0xB155 + ' ' + [char]0xD55C + [char]0x3131 + [char]0xC774 + '\n'),
    [string]$Layout = 'dubeolsik',
    [int]$TimeoutSec = 180,
    [switch]$NoBuild,
    [switch]$KeepOpen
)

$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $repo 'build-tsf-sandbox'
$kitBuild = Join-Path $build 'kit'
$tipBuild = Join-Path $repo 'frontends\tsf\build-tsf-sandbox'

function Find-VcVars {
    if ($env:HUNMIN_VS2022_ROOT) {
        $candidate = Join-Path $env:HUNMIN_VS2022_ROOT 'VC\Auxiliary\Build\vcvarsall.bat'
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    $candidates = @(
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat'
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw 'vcvarsall.bat not found; set HUNMIN_VS2022_ROOT'
}

function Build-Kit {
    $vcvars = Find-VcVars
    New-Item -ItemType Directory -Force -Path $build, $kitBuild | Out-Null

    $staticLib1 = Join-Path $build 'Release\hunmin_static.lib'
    $staticLib2 = Join-Path $build 'libhunmin\Release\hunmin_static.lib'
    $tipDll = Join-Path $tipBuild 'Release\hunmin-tsf.dll'
    $deps = Join-Path $build 'deps.txt'
    $gtestSrc = Join-Path $repo 'build-msvc\_deps\googletest-src'
    $bat = Join-Path $build 'build-kit.bat'

    # The TSF test subdirectory links GTest::gtest_main even when BUILD_TESTING
    # is off (its add_subdirectory(test) is unconditional), so the target must
    # exist. Reuse the tree's existing FetchContent checkout when present;
    # otherwise FetchContent fetches it, as the normal build does.
    $lines = @(
        '@echo off',
        "cd /d `"$repo`"",
        "call `"$vcvars`" x64",
        'if errorlevel 1 exit /b 1',
        'set GTEST_ARG=',
        "if exist `"$gtestSrc`" set GTEST_ARG=-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=`"$gtestSrc`"",
        'echo == core: MSVC Release, static CRT ==',
        "cmake -S . -B `"$build`" -G `"Visual Studio 17 2022`" -A x64 -DHUNMIN_MSVC_STATIC_RUNTIME=ON -DBUILD_TESTING=ON %GTEST_ARG%",
        'if errorlevel 1 exit /b 1',
        "cmake --build `"$build`" --config Release --target hunmin_static",
        'if errorlevel 1 exit /b 1',
        'set STATIC_LIB=',
        "if exist `"$staticLib1`" set STATIC_LIB=$staticLib1",
        "if not defined STATIC_LIB if exist `"$staticLib2`" set STATIC_LIB=$staticLib2",
        'if not defined STATIC_LIB (echo hunmin_static.lib not found & exit /b 1)',
        'echo == TIP: standalone MSVC Release, static CRT ==',
        "cmake -S `"$repo\frontends\tsf`" -B `"$tipBuild`" -G `"Visual Studio 17 2022`" -A x64 -DHUNMIN_MSVC_STATIC_RUNTIME=ON -DBUILD_TESTING=ON %GTEST_ARG% -DHUNMIN_STATIC_LIB=`"%STATIC_LIB%`"",
        'if errorlevel 1 exit /b 1',
        "cmake --build `"$tipBuild`" --config Release --target hunmin-tsf",
        'if errorlevel 1 exit /b 1',
        'echo == CRT gate: no dynamic VC runtime in the TIP ==',
        "dumpbin -DEPENDENTS `"$tipDll`" > `"$deps`"",
        "findstr /I `"VCRUNTIME MSVCP MSVCR`" `"$deps`" && (echo dynamic VC runtime imported & exit /b 1)",
        'echo == probe: console entry, static CRT ==',
        "if not exist `"$kitBuild`" mkdir `"$kitBuild`"",
        "cl /nologo /W4 /O2 /DUNICODE /D_UNICODE /DHUNMIN_PROBE_CONSOLE `"$repo\frontends\tsf\test\ime-probe.c`" /Fo:`"$kitBuild\ime-probe.obj`" /Fe:`"$kitBuild\ime-probe.exe`" /MT /link /subsystem:console user32.lib gdi32.lib ole32.lib uuid.lib shell32.lib",
        'if errorlevel 1 exit /b 1'
    )

    [IO.File]::WriteAllText($bat, (($lines -join "`r`n") + "`r`n"), [Text.Encoding]::ASCII)
    & cmd.exe /c $bat
    if ($LASTEXITCODE -ne 0) { throw "TSF sandbox kit build failed; see $build\build-kit.bat" }

    if (-not (Test-Path -LiteralPath $tipDll)) { throw "hunmin-tsf.dll not found at $tipDll" }
    Copy-Item -LiteralPath $tipDll -Destination (Join-Path $kitBuild 'hunmin-tsf.dll') -Force
    Copy-Item -LiteralPath (Join-Path $repo 'libhunmin\data') -Destination (Join-Path $kitBuild 'data') -Recurse -Force

    Write-Host "kit: $kitBuild"
}

if (-not $NoBuild) { Build-Kit }

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$run = Join-Path $build "sandbox-$stamp"
$kitHost = Join-Path $run 'kit'
$outHost = Join-Path $run 'out'
New-Item -ItemType Directory -Force -Path $kitHost, $outHost | Out-Null

Copy-Item -Path (Join-Path $kitBuild '*') -Destination $kitHost -Recurse -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'tsf-sandbox-guest.ps1') -Destination (Join-Path $kitHost 'run-live.ps1') -Force
if ($KeepOpen) { New-Item -ItemType File -Path (Join-Path $kitHost 'keep-open') | Out-Null }

$caseText = "keys=$Keys`r`nlayout=$Layout`r`nexpect=$Expect`r`n"
$utf8 = New-Object System.Text.UTF8Encoding $false
[IO.File]::WriteAllText((Join-Path $kitHost 'case.txt'), $caseText, $utf8)

$kitXml = [System.Security.SecurityElement]::Escape($kitHost)
$outXml = [System.Security.SecurityElement]::Escape($outHost)
$wsbPath = Join-Path $run 'run.wsb'
$wsb = @"
<Configuration>
  <MappedFolders>
    <MappedFolder>
      <HostFolder>$kitXml</HostFolder>
      <SandboxFolder>C:\hunmin\kit</SandboxFolder>
      <ReadOnly>true</ReadOnly>
    </MappedFolder>
    <MappedFolder>
      <HostFolder>$outXml</HostFolder>
      <SandboxFolder>C:\hunmin\out</SandboxFolder>
      <ReadOnly>false</ReadOnly>
    </MappedFolder>
  </MappedFolders>
  <Networking>Disable</Networking>
  <ClipboardRedirection>Disable</ClipboardRedirection>
  <VideoInput>Disable</VideoInput>
  <AudioInput>Disable</AudioInput>
  <MemoryInMB>4096</MemoryInMB>
  <LogonCommand>
    <Command>powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\hunmin\kit\run-live.ps1</Command>
  </LogonCommand>
</Configuration>
"@
[IO.File]::WriteAllText($wsbPath, $wsb, $utf8)

$sandbox = Join-Path $env:WINDIR 'System32\WindowsSandbox.exe'
if (-not (Test-Path -LiteralPath $sandbox)) { throw "Windows Sandbox not found at $sandbox" }

Write-Host "case: keys=$Keys layout=$Layout expect=$Expect"
Write-Host "run:  $run"
Start-Process -FilePath $sandbox -ArgumentList $wsbPath | Out-Null

$done = Join-Path $outHost 'done.txt'
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
    if (Test-Path -LiteralPath $done) { break }
    Start-Sleep -Milliseconds 500
}

if (-not (Test-Path -LiteralPath $done)) {
    Write-Host "TIMEOUT after ${TimeoutSec}s; inspect $run"
    exit 2
}

$resultPath = Join-Path $outHost 'result.txt'
$runnerPath = Join-Path $outHost 'runner.txt'
if (Test-Path -LiteralPath $runnerPath) {
    Write-Host '--- runner ---'
    Get-Content -LiteralPath $runnerPath -Encoding UTF8
}
if (-not (Test-Path -LiteralPath $resultPath)) {
    Write-Host "FAIL: probe wrote no result; inspect $run"
    exit 1
}

$result = Get-Content -LiteralPath $resultPath -Encoding UTF8 -Raw
Write-Host '--- result ---'
Write-Host $result.Trim()
if ($result -match '(?m)^pass=1\s*$') { exit 0 }
exit 1
