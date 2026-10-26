<#
  Edge host round, guest half (issue #126). Copied to C:\hunmin\kit\run-live.ps1 by
  tools/tsf-edge-round.ps1 and run at sandbox logon.

  It installs the mapped INSTALLER package (the ruled vehicle for a sandbox
  round: the shipped shape, not a regsvr32 of a build-tree copy), launches Edge
  on the kit's target.html, activates the profile with Win+Space, types the case
  keys with SendInput, reads the text back off the window title, and writes
  result.txt beside the copied TIP log.

  Why the title: the page mirrors its textarea into document.title as
  TXTV[<urlencoded value>]ENDT, so the host needs no UIA/CDP client, and the
  readback is the value the page actually received.
#>
$ErrorActionPreference = 'Stop'

$kit = $PSScriptRoot
$out = 'C:\hunmin\out'
$work = 'C:\hunmin\work'
New-Item -ItemType Directory -Force -Path $out, $work | Out-Null
Copy-Item -Path (Join-Path $kit '*') -Destination $work -Recurse -Force

$case = @{}
Get-Content -LiteralPath (Join-Path $kit 'case.txt') -Encoding UTF8 | ForEach-Object {
    if ($_ -match '=') {
        $k, $v = $_ -split '=', 2
        $case[$k] = $v
    }
}

# Process-scope env: Edge inherits it from this shell. A `setx /M` written here
# does NOT reach a process started from this already-running shell (measured:
# the round ran the store default layout while setx reported success).
$env:HUNMIN_TSF_DEBUG_LOG = '1'
$env:HUNMIN_TSF_LOG_DIR = $out
$env:HUNMIN_TSF_HANGUL_LAYOUT = $case['layout']

$round = Join-Path $out 'edge-round.txt'
function Say($m) { $m | Add-Content -LiteralPath $round -Encoding UTF8 }

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class N {
    [StructLayout(LayoutKind.Sequential)]
    public struct MOUSEINPUT { public int dx; public int dy; public uint mouseData; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)]
    public struct KEYBDINPUT { public ushort wVk; public ushort wScan; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)]
    public struct HARDWAREINPUT { public uint uMsg; public ushort wParamL; public ushort wParamH; }
    [StructLayout(LayoutKind.Explicit)]
    public struct INPUTUNION {
        [FieldOffset(0)] public MOUSEINPUT mi;
        [FieldOffset(0)] public KEYBDINPUT ki;
        [FieldOffset(0)] public HARDWAREINPUT hi;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT { public uint type; public INPUTUNION u; }

    [DllImport("user32.dll", SetLastError = true)] static extern uint SendInput(uint n, INPUT[] p, int cb);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
    // CharSet.Unicode is load-bearing: without it the W entry point is called
    // with an ANSI-marshalled StringBuilder and every title reads one mangled
    // character (measured: the window was never matched).
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern short VkKeyScanW(char c);
    public delegate bool EnumProc(IntPtr h, IntPtr p);

    public static uint Key(ushort vk, bool up) {
        INPUT[] a = new INPUT[1];
        a[0].type = 1;
        a[0].u.ki.wVk = vk;
        a[0].u.ki.dwFlags = up ? 2u : 0u;
        return SendInput(1, a, Marshal.SizeOf(typeof(INPUT)));
    }
    public static void Tap(ushort vk) { Key(vk, false); Key(vk, true); }

    // The delegate must outlive the call: one created inline is collected
    // mid-enumeration and EnumWindows then reports nothing (measured).
    static EnumProc _cb;
    static IntPtr _found;
    static string _marker;
    static bool Match(IntPtr h, IntPtr p) {
        if (!IsWindowVisible(h)) return true;
        var cls = new StringBuilder(256);
        GetClassNameW(h, cls, 256);
        if (!cls.ToString().StartsWith("Chrome_WidgetWin")) return true;
        var t = new StringBuilder(1024);
        GetWindowTextW(h, t, 1024);
        if (t.ToString().Contains(_marker)) { _found = h; return false; }
        return true;
    }
    public static IntPtr FindByTitle(string marker) {
        _found = IntPtr.Zero;
        _marker = marker;
        _cb = new EnumProc(Match);
        EnumWindows(_cb, IntPtr.Zero);
        GC.KeepAlive(_cb);
        return _found;
    }
    public static string Title(IntPtr h) {
        var t = new StringBuilder(2048);
        GetWindowTextW(h, t, 2048);
        return t.ToString();
    }
    public static string Dump() {
        var list = new List<string>();
        EnumProc cb = delegate (IntPtr h, IntPtr p) {
            if (!IsWindowVisible(h)) return true;
            var cls = new StringBuilder(256);
            GetClassNameW(h, cls, 256);
            var t = new StringBuilder(512);
            GetWindowTextW(h, t, 512);
            if (t.Length > 0) list.Add(cls.ToString() + " | " + t.ToString());
            return true;
        };
        EnumWindows(cb, IntPtr.Zero);
        GC.KeepAlive(cb);
        return string.Join(Environment.NewLine, list);
    }
}
'@

function Key-Char([char]$ch) {
    $scan = [int][N]::VkKeyScanW($ch)
    $vk = $scan -band 0xFF
    $shift = (($scan -shr 8) -band 1) -eq 1
    if ($vk -le 0) { throw "no vk for $ch" }
    if ($shift) { [N]::Key(0x10, $false) | Out-Null }
    [N]::Key([uint16]$vk, $false) | Out-Null
    [N]::Key([uint16]$vk, $true) | Out-Null
    if ($shift) { [N]::Key(0x10, $true) | Out-Null }
}

function Write-Result($text, $error_) {
    $lines = @("keys=$($case['keys'])", "layout=$($case['layout'])", "text=$text", "expect=$($case['expect'])",
               "pass=" + $(if ($text -eq $case['expect']) { '1' } else { '0' }))
    if ($error_) { $lines += "error=$error_" }
    $lines | Set-Content -LiteralPath (Join-Path $out 'result.txt') -Encoding UTF8
}

$edge = $null
foreach ($c in @("$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe",
                 "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe")) {
    if (Test-Path -LiteralPath $c) { $edge = $c; break }
}
Say "edge=$edge"
try {
    $setup = Get-ChildItem -LiteralPath 'C:\hunmin\pkg' -Filter 'hunmin-*-setup.exe' | Select-Object -First 1
    if (-not $setup) { throw 'installer not mapped' }
    Say ("setup=" + $setup.FullName)
    $inst = Start-Process -FilePath $setup.FullName -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') -Wait -PassThru
    Say ("install rc=" + $inst.ExitCode)
    $app = Join-Path $env:ProgramFiles 'Hunmin IME'
    Say ("installed dll=" + (Test-Path -LiteralPath (Join-Path $app 'hunmin-tsf.dll')) +
         " data=" + (Test-Path -LiteralPath (Join-Path $app 'data\krdict\krdict.json')))
    if (-not $edge) { throw 'edge missing' }

    $page = 'file:///' + ((Join-Path $work 'target.html') -replace '\\', '/')
    Start-Process -FilePath $edge -ArgumentList @("--new-window", $page, '--no-first-run',
        '--no-default-browser-check', ('--user-data-dir=' + (Join-Path $work 'edgeprofile')))
    $hwnd = [IntPtr]::Zero
    for ($i = 0; $i -lt 120; $i++) {
        $hwnd = [N]::FindByTitle('TXTV[')
        if ($hwnd -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 500
    }
    if ($hwnd -eq [IntPtr]::Zero) {
        Say '--- visible windows ---'
        Say ([N]::Dump())
        throw 'no edge window'
    }
    Say ("window=0x{0:X} title={1}" -f $hwnd.ToInt64(), [N]::Title($hwnd))
    Start-Sleep -Seconds 2
    [N]::ShowWindow($hwnd, 9) | Out-Null
    [N]::SetForegroundWindow($hwnd) | Out-Null
    Start-Sleep -Seconds 1

    $tipLog = Join-Path $out 'hunmin-tsf-debug.log'
    $activated = $false
    for ($i = 0; $i -lt 6; $i++) {
        if ((Test-Path -LiteralPath $tipLog) -and
            ((Get-Content -LiteralPath $tipLog -Raw -Encoding UTF8) -match 'applied hangul=')) {
            Say "activated after $i cycle(s)"
            $activated = $true
            break
        }
        [N]::Key(0x5B, $false) | Out-Null
        [N]::Tap(0x20)
        [N]::Key(0x5B, $true) | Out-Null
        Start-Sleep -Milliseconds 2500
    }
    Say "activated=$activated"
    if (-not $activated) { throw 'profile not activated' }

    $typed = ''
    foreach ($ch in $case['keys'].ToCharArray()) {
        if ([N]::GetForegroundWindow() -ne $hwnd) { Say 'FOCUS-LOST'; break }
        Key-Char $ch
        $typed += $ch
        Start-Sleep -Milliseconds 350
    }
    Say "typed=$typed"
    Start-Sleep -Seconds 2

    $title = [N]::Title([N]::FindByTitle('TXTV['))
    Say "final-title=$title"
    $body = ''
    if ($title -match 'TXTV\[(.*?)\]ENDT') { $body = $Matches[1] }
    Write-Result ([System.Uri]::UnescapeDataString($body)) $null
} catch {
    Say ("EXCEPTION: " + $_.Exception.Message)
    Write-Result '' $_.Exception.Message
} finally {
    Get-ChildItem -LiteralPath $work -Filter '*.log' -ErrorAction SilentlyContinue |
        Copy-Item -Destination $out -Force -ErrorAction SilentlyContinue
    'done' | Set-Content -LiteralPath (Join-Path $out 'done.txt') -Encoding UTF8
    if (-not (Test-Path -LiteralPath (Join-Path $kit 'keep-open'))) {
        & shutdown.exe /s /t 5
    }
}
