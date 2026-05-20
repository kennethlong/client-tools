# show-window.ps1 -- ShowWindow + SetWindowPos + SetForegroundWindow assist
# for the D3D11 plugin smoke. Originated in Phase 11 Plan 11-07 Iter-17,
# where the engine's normal ShowWindow(SW_SHOW) path didn't fire on launch.
# Plan 11-08+ smokes carry the workaround forward until the engine-init
# path is investigated.
#
# Usage:
#   ./tools/d3d11-smoke/show-window.ps1
#   ./tools/d3d11-smoke/show-window.ps1 -List
#   ./tools/d3d11-smoke/show-window.ps1 -WindowHandle 0x000A1234
#   ./tools/d3d11-smoke/show-window.ps1 -Width 1280 -Height 720
#
# Note on type caching: Add-Type registers the helper class in the
# AppDomain. PowerShell will silently no-op a redefinition under the
# same class name, so this script uses a VERSIONED class name
# (WinSwgShow_v3) -- bump the suffix if you change the type surface.

[CmdletBinding()]
param(
    [string]$ProcessName = 'SwgClient_d',
    [int]$X = 100,
    [int]$Y = 100,
    [int]$Width = 1024,
    [int]$Height = 768,
    [IntPtr]$WindowHandle = [IntPtr]::Zero,
    [switch]$List
)

if (-not ('WinSwgShow_v3' -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class WinSwgShow_v3 {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpfn, IntPtr lParam);
    [DllImport("user32.dll", SetLastError=true)] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll", SetLastError=true)] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
}

$SW_SHOW = 5

$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue
if (-not $proc) {
    Write-Host "Process '$ProcessName' not running. Launch stage/SwgClient_d.exe first." -ForegroundColor Yellow
    exit 1
}

if ($proc.Count -gt 1) {
    Write-Host "Found $($proc.Count) processes named '$ProcessName' -- enumerating windows for all of them:" -ForegroundColor Yellow
    $proc | Select-Object Id, StartTime, @{N='CPU_s';E={[int]$_.CPU}}, @{N='WS_MB';E={[int]($_.WorkingSet64/1MB)}} | Format-Table -AutoSize
}
$targetPids = $proc.Id

Write-Host "Process: $($proc.Name)  PID(s): $($targetPids -join ', ')  -- enumerating top-level windows by PID..."

# Collect all top-level windows once, then filter by PID.
$script:collected = New-Object System.Collections.ArrayList
$cb = [WinSwgShow_v3+EnumWindowsProc]{
    param([IntPtr]$h, [IntPtr]$l)
    [uint32]$wpid = 0
    [WinSwgShow_v3]::GetWindowThreadProcessId($h, [ref]$wpid) | Out-Null
    if ($script:targetPidSet.Contains($wpid)) {
        $null = $script:collected.Add(@{ hWnd = $h; PID = $wpid })
    }
    return $true
}
$script:targetPidSet = New-Object 'System.Collections.Generic.HashSet[uint32]'
foreach ($id in $targetPids) { $null = $script:targetPidSet.Add([uint32]$id) }
[WinSwgShow_v3]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null

$candidates = @()
foreach ($entry in $script:collected) {
    $h = [IntPtr]$entry.hWnd
    $title = New-Object System.Text.StringBuilder 256
    [WinSwgShow_v3]::GetWindowText($h, $title, 256) | Out-Null
    $cls = New-Object System.Text.StringBuilder 256
    [WinSwgShow_v3]::GetClassName($h, $cls, 256) | Out-Null
    $rect = New-Object WinSwgShow_v3+RECT
    [WinSwgShow_v3]::GetWindowRect($h, [ref]$rect) | Out-Null
    $vis = [WinSwgShow_v3]::IsWindowVisible($h)
    $candidates += [PSCustomObject]@{
        PID     = $entry.PID
        hWnd    = "0x{0:X8}" -f $h.ToInt32()
        Class   = $cls.ToString()
        Title   = $title.ToString()
        W       = $rect.Right - $rect.Left
        H       = $rect.Bottom - $rect.Top
        Visible = $vis
        RawHandle = $h
    }
}

$candidates = $candidates | Sort-Object -Property @{Expression='W'; Descending=$true}, @{Expression='H'; Descending=$true}

if ($candidates.Count -eq 0) {
    Write-Host "No top-level windows found for any PID in [$($targetPids -join ', ')]. Process(es) alive but CreateWindowEx has not landed yet OR has only message-only windows." -ForegroundColor Yellow
    exit 2
}

$candidates | Format-Table PID, hWnd, Class, Title, W, H, Visible -AutoSize

if ($List) {
    Write-Host "List-only mode; not invoking ShowWindow."
    exit 0
}

$target = if ($WindowHandle -ne [IntPtr]::Zero) {
    ($candidates | Where-Object { $_.RawHandle -eq $WindowHandle }).RawHandle
} else {
    $candidates[0].RawHandle
}

if (-not $target -or $target -eq [IntPtr]::Zero) {
    Write-Host "Could not resolve target window." -ForegroundColor Yellow
    exit 3
}

Write-Host "Targeting hWnd=$('0x{0:X8}' -f $target.ToInt32()) -- forcing SW_SHOW + reposition + foreground..."
[WinSwgShow_v3]::ShowWindow($target, $SW_SHOW) | Out-Null
[WinSwgShow_v3]::SetWindowPos($target, [IntPtr]::Zero, $X, $Y, $Width, $Height, 0) | Out-Null
[WinSwgShow_v3]::SetForegroundWindow($target) | Out-Null

$rect = New-Object WinSwgShow_v3+RECT
[WinSwgShow_v3]::GetWindowRect($target, [ref]$rect) | Out-Null
$vis = [WinSwgShow_v3]::IsWindowVisible($target)
Write-Host "After assist: pos=($($rect.Left),$($rect.Top)) size=$($rect.Right - $rect.Left)x$($rect.Bottom - $rect.Top) visible=$vis"
