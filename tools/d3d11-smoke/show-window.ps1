# show-window.ps1 -- ShowWindow + SetWindowPos + SetForegroundWindow assist
# for the D3D11 plugin smoke. Originated in Phase 11 Plan 11-07 Iter-17,
# where the engine's normal ShowWindow(SW_SHOW) path didn't fire on launch
# (suspected interaction with Bloom config-disable / shader-cache fast-path
# / prior-run swap-chain state -- root cause deferred). Plan 11-08+ smokes
# carry the workaround forward until the engine-init path is investigated.
#
# Usage:
#   pwsh ./tools/d3d11-smoke/show-window.ps1
#   pwsh ./tools/d3d11-smoke/show-window.ps1 -Width 1280 -Height 720
#   pwsh ./tools/d3d11-smoke/show-window.ps1 -List          # enumerate only
#   pwsh ./tools/d3d11-smoke/show-window.ps1 -WindowHandle 0x000A1234
#
# The script:
#   1. Resolves the SwgClient_d process and ENUMERATES every top-level
#      window owned by its threads (EnumThreadWindows). This is more
#      reliable than MainWindowHandle (which depends on Windows shell
#      heuristics) or FindWindow (which depends on knowing the title).
#   2. Picks the largest visible-or-hidden window (engine windows
#      typically dominate any debug-stub windows by size).
#   3. Forces SW_SHOW, repositions to (X,Y), sizes to Width x Height,
#      pulls to foreground.

[CmdletBinding()]
param(
    [string]$ProcessName = 'SwgClient_d',
    [int]$X = 100,
    [int]$Y = 100,
    [int]$Width = 1024,
    [int]$Height = 768,
    [IntPtr]$WindowHandle = [IntPtr]::Zero,    # override target window
    [switch]$List                              # enumerate only, no ShowWindow
)

Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public class Win {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumThreadWindows(uint dwThreadId, EnumWindowsProc lpfn, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll", SetLastError = true)] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    public static List<IntPtr> WindowsForThread(uint tid) {
        var list = new List<IntPtr>();
        EnumThreadWindows(tid, (h, l) => { list.Add(h); return true; }, IntPtr.Zero);
        return list;
    }
}
"@

$SW_SHOW = 5

$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue
if (-not $proc) {
    Write-Host "Process '$ProcessName' not running. Launch stage/SwgClient_d.exe first." -ForegroundColor Yellow
    exit 1
}

Write-Host "Process: $($proc.Name) (PID $($proc.Id)) -- enumerating top-level windows per thread..."

$candidates = @()
foreach ($t in $proc.Threads) {
    $tid = [uint32]$t.Id
    foreach ($h in [Win]::WindowsForThread($tid)) {
        if ($h -eq [IntPtr]::Zero) { continue }
        $title = New-Object System.Text.StringBuilder 256
        [Win]::GetWindowText($h, $title, 256) | Out-Null
        $cls = New-Object System.Text.StringBuilder 256
        [Win]::GetClassName($h, $cls, 256) | Out-Null
        $rect = New-Object Win+RECT
        [Win]::GetWindowRect($h, [ref]$rect) | Out-Null
        $vis = [Win]::IsWindowVisible($h)
        $w = $rect.Right - $rect.Left
        $hgt = $rect.Bottom - $rect.Top
        $candidates += [PSCustomObject]@{
            hWnd  = "0x{0:X8}" -f $h.ToInt32()
            Class = $cls.ToString()
            Title = $title.ToString()
            W     = $w
            H     = $hgt
            Visible = $vis
            RawHandle = $h
        }
    }
}

$candidates = $candidates | Sort-Object -Property @{Expression = "W"; Descending = $true}, @{Expression = "H"; Descending = $true}

if ($candidates.Count -eq 0) {
    Write-Host "No top-level windows found for any thread of $ProcessName. Process is alive but has not called CreateWindowEx yet, OR has only message-only windows." -ForegroundColor Yellow
    exit 2
}

$candidates | Format-Table hWnd, Class, Title, W, H, Visible -AutoSize

if ($List) {
    Write-Host "List-only mode; not invoking ShowWindow."
    exit 0
}

$target = if ($WindowHandle -ne [IntPtr]::Zero) {
    ($candidates | Where-Object { $_.RawHandle -eq $WindowHandle }).RawHandle
} else {
    # Pick the largest window. Engine windows dominate any stub windows.
    $candidates[0].RawHandle
}

if (-not $target -or $target -eq [IntPtr]::Zero) {
    Write-Host "Could not resolve target window." -ForegroundColor Yellow
    exit 3
}

Write-Host "Targeting hWnd=$('0x{0:X8}' -f $target.ToInt32()) -- forcing SW_SHOW + reposition + foreground..."
[Win]::ShowWindow($target, $SW_SHOW) | Out-Null
[Win]::SetWindowPos($target, [IntPtr]::Zero, $X, $Y, $Width, $Height, 0) | Out-Null
[Win]::SetForegroundWindow($target) | Out-Null

$rect = New-Object Win+RECT
[Win]::GetWindowRect($target, [ref]$rect) | Out-Null
$vis = [Win]::IsWindowVisible($target)
Write-Host "After assist: pos=($($rect.Left),$($rect.Top)) size=$($rect.Right - $rect.Left)x$($rect.Bottom - $rect.Top) visible=$vis"
