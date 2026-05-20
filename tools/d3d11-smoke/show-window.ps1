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
#   pwsh ./tools/d3d11-smoke/show-window.ps1 -ProcessName SwgClient -WindowTitle SwgClient
#
# Run AFTER launching stage/SwgClient_d.exe. The script:
#   1. Resolves the SwgClient_d process and its main window handle.
#   2. Falls back to FindWindow by class/title if MainWindowHandle is 0
#      (happens when the engine created the window without SW_SHOW).
#   3. Forces SW_SHOW, repositions to (100,100), sizes to Width x Height,
#      and pulls to foreground.

[CmdletBinding()]
param(
    [string]$ProcessName = 'SwgClient_d',
    [string]$WindowTitle = 'SwgClient',
    [int]$X = 100,
    [int]$Y = 100,
    [int]$Width = 1024,
    [int]$Height = 768
)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win {
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll", SetLastError = true)] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
}
"@

$SW_SHOW = 5
$HWND_TOP = [IntPtr]::Zero

$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue
if (-not $proc) {
    Write-Host "Process '$ProcessName' not running. Launch stage/SwgClient_d.exe first." -ForegroundColor Yellow
    exit 1
}

$hwnd = if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { $proc.MainWindowHandle }
        else { [Win]::FindWindow($null, $WindowTitle) }

if ($hwnd -eq [IntPtr]::Zero) {
    Write-Host "Could not resolve window handle. Process is alive but no window matches '$WindowTitle' yet -- engine may still be in CreateWindowEx; retry in a few seconds." -ForegroundColor Yellow
    exit 2
}

Write-Host "Process: $($proc.Name) (PID $($proc.Id))  hWnd=$hwnd  visible-before=$([Win]::IsWindowVisible($hwnd))"
[Win]::ShowWindow($hwnd, $SW_SHOW) | Out-Null
[Win]::SetWindowPos($hwnd, $HWND_TOP, $X, $Y, $Width, $Height, 0) | Out-Null
[Win]::SetForegroundWindow($hwnd) | Out-Null
Write-Host "After assist: pos=($X,$Y) size=${Width}x${Height} visible-after=$([Win]::IsWindowVisible($hwnd))"
