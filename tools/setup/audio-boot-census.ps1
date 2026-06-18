<#
.SYNOPSIS
  Phase 35-04 audio boot-log census (Task 1, AUDIO-02 automatable half).

.DESCRIPTION
  Greps a captured SwgClient boot log for the five Miles 9.3b runtime signatures the
  35-04 plan gates on. Run AFTER booting the staged client to character select and
  capturing its log (see -LogPath). Honors the plan's acceptance thresholds.

  Signatures (Audio.cpp source of truth):
    - codec-probe false-fail  : "missing in"   (the REQUIRED/OPTIONAL codec-missing WARNING,
                                Audio.cpp:1329/1345 -- the plan calls this "redist missing").
                                Must be 0 for the REQUIRED tier.
    - driver-open success      : "Audio: Finished initializing" (Audio.cpp:1432). Must be >= 1.
    - D-04 off-switch ON       : "Miles is disabled" (Audio.cpp:1217). Must be 0.
    - reverb bus log           : "room_type bus_index" (Audio.cpp:1396). Must be >= 1.
    - warning-flood            : "Unable to create a valid sample id" (Audio.cpp:2871). Must be < 20.

.PARAMETER LogPath
  Path to the captured boot log. Default: the InstallTimer log the Debug client writes
  to Logs\SwgClient_*.log under the stage dir, OR a redirected stdout capture.

.EXAMPLE
  # Boot, capturing the engine log (run from the stage dir):
  #   cd D:\Code\swg-client-v2\stage-x64
  #   .\SwgClient_d.exe   # reads client_d.cfg (rasterMajor=5); play to char-select, then quit
  # Then census the newest Logs\SwgClient_*.log:
  pwsh tools\setup\audio-boot-census.ps1 -LogPath (Get-ChildItem stage-x64\Logs\SwgClient_*.log | Sort LastWriteTime | Select -Last 1).FullName
#>
param(
  [string]$LogPath
)

$ErrorActionPreference = 'Stop'

if (-not $LogPath) {
  $cand = Get-ChildItem -Path 'stage-x64\Logs\SwgClient_*.log','stage\Logs\SwgClient_*.log' -ErrorAction SilentlyContinue |
          Sort-Object LastWriteTime | Select-Object -Last 1
  if ($cand) { $LogPath = $cand.FullName }
}
if (-not $LogPath -or -not (Test-Path $LogPath)) {
  Write-Error "Boot log not found. Boot the client to char-select first, then pass -LogPath <SwgClient_*.log or captured stdout>."
  exit 2
}

Write-Host "=== Audio boot-log census: $LogPath ===" -ForegroundColor Cyan
$lines = Get-Content -Path $LogPath

function Count([string]$pat, [switch]$IgnoreCase) {
  $opts = if ($IgnoreCase) { 'IgnoreCase' } else { 'None' }
  ($lines | Select-String -Pattern $pat -SimpleMatch:$false).Count
}

$cMissing   = ($lines | Select-String -Pattern 'missing in').Count                       # codec-missing WARNING (== "redist missing")
$cInit      = ($lines | Select-String -Pattern 'Finished initializing').Count            # driver-open success
$cDisabled  = ($lines | Select-String -Pattern 'Miles is disabled' -CaseSensitive:$false).Count
$cRoom      = ($lines | Select-String -Pattern 'room_type bus_index').Count
$cFlood     = ($lines | Select-String -Pattern 'Unable to create a valid sample id').Count

$pass = $true
function Gate([string]$name, [bool]$ok, [string]$detail) {
  $tag = if ($ok) { '[PASS]' } else { '[FAIL]'; }
  $col = if ($ok) { 'Green' } else { 'Red' }
  Write-Host ("{0} {1}: {2}" -f $tag, $name, $detail) -ForegroundColor $col
  if (-not $ok) { $script:pass = $false }
}

Gate 'codec-probe (no REQUIRED missing)'  ($cMissing -eq 0)  "'missing in' count = $cMissing (want 0)"
Gate 'driver-open success'                ($cInit   -ge 1)   "'Finished initializing' count = $cInit (want >=1)"
Gate 'D-04 off-switch is OFF'             ($cDisabled -eq 0)  "'Miles is disabled' count = $cDisabled (want 0)"
Gate 'reverb bus_index=0 logged'         ($cRoom   -ge 1)   "'room_type bus_index' count = $cRoom (want >=1)"
Gate 'no warning-flood'                   ($cFlood  -lt 20)  "'Unable to create a valid sample id' count = $cFlood (want <20)"

Write-Host ""
if ($pass) {
  Write-Host "CENSUS: PASS -- ready to hand to the human four-dimension audio UAT." -ForegroundColor Green
  exit 0
} else {
  Write-Host "CENSUS: FAIL -- do NOT advance to the human UAT; report the failing gate." -ForegroundColor Red
  exit 1
}
