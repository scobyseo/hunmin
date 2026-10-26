<#
.SYNOPSIS
  Run one key sequence against Edge in a disposable Windows Sandbox and read the
  text back (issue #126, the "modern hosts unchanged" row).

.DESCRIPTION
  The sandbox image's modern TSF host is Edge; the probe (ime-probe.exe) is the
  RICHEDIT50W/EDIT host class and measures a different row. This script boots a
  sandbox that installs the INSTALLER package (the ruled vehicle for a sandbox
  round: the shipped shape, not a regsvr32 of a build-tree copy), opens Edge on
  a generated page, activates the profile with Win+Space, types -Keys one key at
  a time, and reports pass=1 from result.txt.

  The page mirrors its textarea into document.title as TXTV[<encoded>]ENDT, so
  the readback needs no UIA or CDP client and reports exactly what the page
  received. The TIP log is copied back beside the result, so the delivery path
  (native composition vs the interim path) is readable from the same run.

  -Keys escapes: \b backspace, \n enter, \t tab, \s<char> shift+char, \\ literal
  backslash. A space is a literal space. -Expect accepts \n, \r, \t, \\.

.EXAMPLE
  tools/tsf-edge-round.ps1 -Keys 'dkssud ' -Expect '안녕 ' -Layout dubeolsik

.NOTES
  The installer comes from tools/build-windows-installer.sh (build-msvc/installer);
  pass -Installer when it lives elsewhere. The sandbox image has no Notepad (see
  tests/manual-checklists/tsf-installer-fresh-vm.md), so Edge is the host here.
#>
[CmdletBinding()]
param(
    [string]$Keys = 'dkssud ',
    [string]$Expect = ([string][char]0xC548 + [char]0xB155 + ' '),
    [string]$Layout = 'dubeolsik',
    [string]$Installer = '',
    [int]$TimeoutSec = 300,
    [switch]$KeepOpen
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Resolve-Installer {
    param([string]$Path)
    if ($Path) {
        if (-not (Test-Path -LiteralPath $Path)) { throw "installer not found: $Path" }
        return (Resolve-Path -LiteralPath $Path).Path
    }
    $dir = Join-Path $repo 'build-msvc\installer'
    $found = @(Get-ChildItem -LiteralPath $dir -Filter 'hunmin-*-setup.exe' -ErrorAction SilentlyContinue)
    if ($found.Count -eq 0) {
        throw "no installer in $dir; build one with tools/build-windows-installer.sh"
    }
    return ($found | Sort-Object LastWriteTime | Select-Object -Last 1).FullName
}

$setup = Resolve-Installer $Installer
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'

function Wait-SandboxIdle {
    # A second Windows Sandbox launch fails ("duplicated instance") while the
    # previous one is still tearing down, and the guest's own `shutdown /t 5`
    # finishes AFTER done.txt lands. Wait for the workers to go, then give the
    # service ~10 s before the next launch (measured: back-to-back launches
    # produced an empty out folder and no guest at all).
    param([int]$TimeoutSec = 120)
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $alive = @(Get-Process -Name WindowsSandbox, vmmemWindowsSandbox -ErrorAction SilentlyContinue)
        if ($alive.Count -eq 0) { break }
        Start-Sleep -Seconds 2
    }
    $alive = @(Get-Process -Name WindowsSandbox, vmmemWindowsSandbox -ErrorAction SilentlyContinue)
    if ($alive.Count -gt 0) { Write-Host "warning: a sandbox worker is still alive; the next launch may fail" }
    Start-Sleep -Seconds 10
}

Wait-SandboxIdle
$run = Join-Path $repo "build-tsf-sandbox\edge-$stamp"
$kit = Join-Path $run 'kit'
$out = Join-Path $run 'out'
New-Item -ItemType Directory -Force -Path $kit, $out | Out-Null

# The page: the textarea's value IS the readback channel (document.title).
$page = @'
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>TXTV[]ENDT</title>
<style>textarea{width:600px;height:120px;font-size:22px}</style>
</head>
<body>
<textarea id="t" autofocus></textarea>
<script>
  const t = document.getElementById('t');
  t.focus();
  const sync = () => { document.title = 'TXTV[' + encodeURIComponent(t.value) + ']ENDT'; };
  t.addEventListener('input', sync);
  t.addEventListener('compositionupdate', sync);
  t.addEventListener('compositionend', sync);
  setInterval(sync, 100);
</script>
</body>
</html>
'@
$utf8 = New-Object System.Text.UTF8Encoding $false
[IO.File]::WriteAllText((Join-Path $kit 'target.html'), $page, $utf8)

# case.txt carries the parameters into the guest (UTF-8, one key=value per line).
[IO.File]::WriteAllText((Join-Path $kit 'case.txt'),
    "keys=$Keys`nlayout=$Layout`nexpect=$Expect`n", $utf8)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'tsf-edge-round-guest.ps1') `
    -Destination (Join-Path $kit 'run-live.ps1') -Force
if ($KeepOpen) { New-Item -ItemType File -Path (Join-Path $kit 'keep-open') | Out-Null }

$kitXml = [System.Security.SecurityElement]::Escape($kit)
$pkgXml = [System.Security.SecurityElement]::Escape((Split-Path -Parent $setup))
$outXml = [System.Security.SecurityElement]::Escape($out)
$wsb = @"
<Configuration>
  <MappedFolders>
    <MappedFolder>
      <HostFolder>$kitXml</HostFolder>
      <SandboxFolder>C:\hunmin\kit</SandboxFolder>
      <ReadOnly>true</ReadOnly>
    </MappedFolder>
    <MappedFolder>
      <HostFolder>$pkgXml</HostFolder>
      <SandboxFolder>C:\hunmin\pkg</SandboxFolder>
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
$wsbPath = Join-Path $run 'run.wsb'
[IO.File]::WriteAllText($wsbPath, $wsb, $utf8)

Write-Host "installer: $setup"
Write-Host "case:      keys='$Keys' layout=$Layout expect='$Expect'"
Write-Host "run:       $run"

Start-Process -FilePath (Join-Path $env:SystemRoot 'System32\WindowsSandbox.exe') -ArgumentList @($wsbPath)

$done = Join-Path $out 'done.txt'
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
    if (Test-Path -LiteralPath $done) { break }
    Start-Sleep -Seconds 1
}

$resultPath = Join-Path $out 'result.txt'
if (-not (Test-Path -LiteralPath $resultPath)) {
    Write-Host "TIMEOUT after ${TimeoutSec}s; inspect $run"
    exit 1
}
foreach ($name in @('result.txt', 'edge-round.txt')) {
    $p = Join-Path $out $name
    if (Test-Path -LiteralPath $p) {
        Write-Host "--- $name ---"
        Write-Host (Get-Content -LiteralPath $p -Raw -Encoding UTF8)
    }
}
$result = Get-Content -LiteralPath $resultPath -Raw -Encoding UTF8
if ($result -match 'pass=1') { exit 0 }
exit 1
