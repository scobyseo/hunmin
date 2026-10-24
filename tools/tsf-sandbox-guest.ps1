<#
  Runs inside the disposable Windows Sandbox. Copied to C:\hunmin\kit\run-live.ps1
  by tools/tsf-sandbox.ps1; reads case.txt, registers the staged TIP, runs the
  probe in batch mode, copies the logs back, unregisters, and writes done.txt.
#>
$ErrorActionPreference = 'Stop'

$kit = $PSScriptRoot
$out = 'C:\hunmin\out'
$work = 'C:\hunmin\work'
New-Item -ItemType Directory -Force -Path $out, $work | Out-Null
Copy-Item -Path (Join-Path $kit '*') -Destination $work -Recurse -Force -Exclude 'case.txt'

$case = @{}
Get-Content -LiteralPath (Join-Path $kit 'case.txt') -Encoding UTF8 | ForEach-Object {
    if ($_ -match '=') {
        $k, $v = $_ -split '=', 2
        $case[$k] = $v
    }
}

$dll = Join-Path $work 'hunmin-tsf.dll'
$probe = Join-Path $work 'ime-probe.exe'
$env:HUNMIN_DATA_DIR = Join-Path $work 'data'
$env:HUNMIN_TSF_HANGUL_LAYOUT = $case['layout']
$env:HUNMIN_TSF_DEBUG_LOG = '1'
$env:HUNMIN_TSF_LOG_DIR = $out

$regHr = $null
$probeHr = $null
try {
    $reg = Start-Process -FilePath "$env:SystemRoot\System32\regsvr32.exe" `
        -ArgumentList @('/s', $dll) -Wait -PassThru
    $regHr = $reg.ExitCode

    $probeArgs = @('--keys', $case['keys'], '--out', (Join-Path $out 'result.txt'))
    if ($case['expect']) { $probeArgs += @('--expect', $case['expect']) }
    Push-Location $work
    try {
        & $probe @probeArgs
        $probeHr = $LASTEXITCODE
    } finally {
        Pop-Location
    }
} finally {
    if (Test-Path -LiteralPath $dll) {
        Start-Process -FilePath "$env:SystemRoot\System32\regsvr32.exe" `
            -ArgumentList @('/s', '/u', $dll) -Wait | Out-Null
    }
    Get-ChildItem -LiteralPath $work -Filter '*.log' -ErrorAction SilentlyContinue |
        Copy-Item -Destination $out -Force -ErrorAction SilentlyContinue
    "regsvr32=$regHr`nprobe=$probeHr" | Set-Content -LiteralPath (Join-Path $out 'runner.txt') -Encoding UTF8
    'done' | Set-Content -LiteralPath (Join-Path $out 'done.txt') -Encoding UTF8
    if (-not (Test-Path -LiteralPath (Join-Path $kit 'keep-open'))) {
        & shutdown.exe /s /t 3
    }
}
