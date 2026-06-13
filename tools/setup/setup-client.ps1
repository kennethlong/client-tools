# setup-client.ps1 -- generates stage/client_d.cfg + stage/client.cfg from the tracked
# tools/setup/client.cfg.template (PORT-01/PORT-02; successor to the dead CMake configure_file).
#
# It prompts for (or takes) the machine's TRE asset root, auto-substitutes this clone's own
# repo-relative stage/override path, substitutes the login server + renderer + resolution,
# validates the staged Miles codec set, and prints a next-steps banner.
#
# Usage:
#   ./tools/setup/setup-client.ps1                       # prompts for the TRE asset root
#   ./tools/setup/setup-client.ps1 -TreRoot "D:/Code/SWGSource Client v3.0/"
#   ./tools/setup/setup-client.ps1 -RasterMajor 5        # D3D9 gate
#   ./tools/setup/setup-client.ps1 -LoginServer 10.0.0.5 # other-network clone
#   ./tools/setup/setup-client.ps1 -NoPrompt -TreRoot ... # CI / non-interactive
#
# The generated stage/client*.cfg are NOT tracked (gitignored via stage/*); this template is.

[CmdletBinding()]
param(
    [string]$TreRoot,
    [int]$RasterMajor = 11,
    [string]$Resolution = '1920x1080',
    [string]$LoginServer = '192.168.1.200',
    [switch]$NoPrompt
)

$ErrorActionPreference = 'Stop'

# --- Resolve repo root + paths from the script location (D-08: no user input for these) ---------
$scriptDir = $PSScriptRoot
$repoRoot  = (Resolve-Path (Join-Path $scriptDir '..\..')).Path
$template  = Join-Path $scriptDir 'client.cfg.template'
$stageDir  = Join-Path $repoRoot 'stage'
$overrideAbs = (Join-Path $stageDir 'override')

if (-not (Test-Path -PathType Leaf $template)) {
    Write-Host "ERROR: template not found at $template" -ForegroundColor Red
    exit 1
}

# --- Prompt for / validate the TRE asset root (V5 input validation + cfg-breaking-char reject) --
if (-not $TreRoot -and -not $NoPrompt) {
    $TreRoot = Read-Host 'Enter your TRE asset root (e.g. D:/Code/SWGSource Client v3.0/)'
}
if (-not $TreRoot) {
    Write-Host "ERROR: -TreRoot is required (no value supplied and -NoPrompt set)." -ForegroundColor Red
    exit 1
}
# REVIEW/Codex LOW: reject cfg-breaking characters so an injected value cannot break/extend the cfg.
if ($TreRoot -match '["\r\n]') {
    Write-Host "ERROR: TRE root contains a double-quote, CR, or newline -- those would break the generated cfg. Rejected." -ForegroundColor Red
    exit 1
}
if (-not (Test-Path -PathType Container $TreRoot)) {
    Write-Host "ERROR: TRE root '$TreRoot' is not an existing directory. Rejected." -ForegroundColor Red
    exit 1
}
# Normalize to forward slashes with a trailing slash (TOCTreePath needs the trailing slash).
$treNorm = ($TreRoot -replace '\\','/').TrimEnd('/') + '/'

# --- Resolution parse -----------------------------------------------------------------------------
if ($Resolution -notmatch '^\s*(\d+)\s*[xX]\s*(\d+)\s*$') {
    Write-Host "ERROR: -Resolution '$Resolution' is not WIDTHxHEIGHT (e.g. 1920x1080)." -ForegroundColor Red
    exit 1
}
$resW = [int]$Matches[1]
$resH = [int]$Matches[2]

# --- Renderer validation (rasterMajor=9 is FATAL; valid set is 5/6/7/11) --------------------------
if ($RasterMajor -notin 5,6,7,11) {
    Write-Host "ERROR: -RasterMajor $RasterMajor is invalid (valid: 5=D3D9, 6=FFP, 7=VSPS, 11=D3D11)." -ForegroundColor Red
    exit 1
}

# --- Build the substituted TRE/TOC/Tree path values (quoted -- they contain spaces) ---------------
$treRootQuoted   = '"' + $treNorm + '"'
$tocPaths = @{
    '@TRE_ROOT_TOC0@'      = '"' + $treNorm + 'sku0_client.toc"'
    '@TRE_ROOT_TOC1@'      = '"' + $treNorm + 'sku1_client.toc"'
    '@TRE_ROOT_TOC2@'      = '"' + $treNorm + 'sku2_client.toc"'
    '@TRE_ROOT_TOC3@'      = '"' + $treNorm + 'sku3_client.toc"'
    '@TRE_ROOT_SNOW@'      = '"' + $treNorm + 'disable_wayfar_dearic_snow.tre"'
    '@TRE_ROOT_SWGSOURCE@' = '"' + $treNorm + 'swgsource_3.0.tre"'
}
$overrideQuoted  = '"' + (($overrideAbs -replace '\\','/')) + '"'

$rawTemplate = Get-Content -Raw $template

function New-Cfg {
    param([string]$rasterMajor, [int]$w, [int]$h, [string]$skipSplash, [string]$occlusionMode)
    $c = $rawTemplate
    # Order matters: substitute the longer @TRE_ROOT_*@ tokens BEFORE the bare @TRE_ROOT@.
    foreach ($k in $tocPaths.Keys) { $c = $c.Replace($k, $tocPaths[$k]) }
    $c = $c.Replace('@TRE_ROOT@',        $treRootQuoted)
    $c = $c.Replace('@OVERRIDE_PATH@',   $overrideQuoted)
    $c = $c.Replace('@LOGIN_SERVER@',    $LoginServer)
    $c = $c.Replace('@RASTER_MAJOR@',    $rasterMajor)
    $c = $c.Replace('@RESOLUTION_WIDTH@',  "$w")
    $c = $c.Replace('@RESOLUTION_HEIGHT@', "$h")
    $c = $c.Replace('@SKIP_SPLASH@',     $skipSplash)
    $c = $c.Replace('@OCCLUSION_MODE@',  $occlusionMode)
    return $c
}

if (-not (Test-Path -PathType Container $stageDir)) {
    New-Item -ItemType Directory -Path $stageDir | Out-Null
}

# Debug cfg (client_d.cfg): occlusionMode=auto preserves the Phase-23 posture for Debug devs.
$debugCfg   = New-Cfg -rasterMajor "$RasterMajor" -w $resW -h $resH -skipSplash 'true'  -occlusionMode 'auto'
# Release cfg (client.cfg): occlusionMode=off (shipped Option alpha default; D-02).
$releaseCfg = New-Cfg -rasterMajor "$RasterMajor" -w $resW -h $resH -skipSplash 'true'  -occlusionMode 'off'

$debugPath   = Join-Path $stageDir 'client_d.cfg'
$releasePath = Join-Path $stageDir 'client.cfg'
Set-Content -Path $debugPath   -Value $debugCfg   -Encoding ASCII
Set-Content -Path $releasePath -Value $releaseCfg -Encoding ASCII

# --- D-12a: validate the staged Miles codec set (warn, do not fail -- postbuild repopulates) ------
$milesDir = Join-Path $stageDir 'miles'
$milesNeeded = @('mssmp3.asi','mssogg.asi','Msseax.m3d','msssoft.m3d')
$milesMissing = @()
foreach ($f in $milesNeeded) {
    if (-not (Test-Path -PathType Leaf (Join-Path $milesDir $f))) { $milesMissing += $f }
}
if ($milesMissing.Count -gt 0) {
    Write-Host ""
    Write-Host "WARNING: stage/miles is missing codec files: $($milesMissing -join ', ')" -ForegroundColor Yellow
    Write-Host "         Build SwgClient -- the postbuild stages stage/miles from the vendored redist" -ForegroundColor Yellow
    Write-Host "         (src/external/3rd/library/miles-7.2e/redist). Without them: silent music + audio." -ForegroundColor Yellow
}

# --- Success summary + next-steps banner (REVIEW/Cursor LOW) ---------------------------------------
Write-Host ""
Write-Host "Wrote:" -ForegroundColor Green
Write-Host "  $debugPath   (rasterMajor=$RasterMajor, ${resW}x${resH}, occlusionMode=auto)"
Write-Host "  $releasePath (rasterMajor=$RasterMajor, ${resW}x${resH}, occlusionMode=off)"
Write-Host "  TRE root:     $treNorm"
Write-Host "  override:     $($overrideAbs -replace '\\','/')"
Write-Host "  loginServer:  $LoginServer"
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  1) Build SwgClient via `$env:MSBUILD (msbuild is not on PATH)."
Write-Host "  2) The postbuild stages stage/miles from the vendored redist."
Write-Host "  3) Launch stage/SwgClient_d.exe (reads client_d.cfg; rasterMajor=$RasterMajor)."
Write-Host "  D3D9 gate: re-run with -RasterMajor 5. D3D11: -RasterMajor 11 (default)."
