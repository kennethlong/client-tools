<#
  audio-stream-probe.ps1 -- launch the x64 client under cdb and log every Miles music-stream
  open/start to a file, to root-cause the 9.3b login-screen "first music sample" regression
  (Phase 35-04). The client runs normally (with its window); cdb only logs in the background.

  USAGE:
    1. Run this script (it launches stage-x64\SwgClient_d.exe under cdb).
    2. Let it reach the LOGIN screen. Confirm (as before) there's no music there.
    3. Wait ~5 seconds at the login screen, then QUIT the client (or close the window).
    4. The log is written to stage-x64\stream-probe.log -- hand that back.

  Pass -Win32 to probe the 32-bit stage\ client instead.
#>
param(
    [switch]$Win32
)

$ErrorActionPreference = 'Stop'
$root = 'D:\Code\swg-client-v2'

if ($Win32) {
    $cdb     = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe'
    $stage   = Join-Path $root 'stage'
    $exe     = Join-Path $stage 'SwgClient_d.exe'
} else {
    $cdb     = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe'
    $stage   = Join-Path $root 'stage-x64'
    $exe     = Join-Path $stage 'SwgClient_d.exe'
}

$script = Join-Path $root 'tools\setup\audio-stream-probe.cdb'
$log    = Join-Path $stage 'stream-probe.log'

if (-not (Test-Path $cdb))   { throw "cdb not found: $cdb" }
if (-not (Test-Path $exe))   { throw "client exe not found: $exe" }
if (-not (Test-Path $script)){ throw "cdb script not found: $script" }

# Symbols: the client's PDB sits next to the exe in stage.
$env:_NT_SYMBOL_PATH = $stage

Write-Host "Launching $exe under cdb..."
Write-Host "  -> Play to the LOGIN screen, wait ~5s, then quit the client."
Write-Host "  -> Log: $log"

Push-Location $stage
try {
    # NOTE: no -g. The -cf script must run at the INITIAL break so the deferred (bu)
    # breakpoints arm before audio install loads mss64 -- otherwise they bind too late
    # and miss the first music request (the whole point of the probe).
    & $cdb -logo $log -cf $script $exe
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "Done. Stream-open log written to: $log"
