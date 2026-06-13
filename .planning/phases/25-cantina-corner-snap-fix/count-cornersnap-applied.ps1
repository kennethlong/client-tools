# Phase 25 / HARD-02 acceptance metric (25-01 Task 2).
# Counts CORNERSNAP-APPLIED (actually-crossed) transitions per (frame, object).
# PASS = ZERO rows for the player NetworkId. Raw CORNERSNAP-PORTAL lines are NOT the metric
# (suppressed reverse attempts still emit CORNERSNAP-PORTAL before the guard blocks them).
#
# Usage:  .\count-cornersnap-applied.ps1 -Log capture-gl05.txt [-Player 1068034708]
param(
    [Parameter(Mandatory = $true)][string]$Log,
    [string]$Player  # optional: player NetworkId value-string to isolate; baseline player was 1068034708
)

$rows = Select-String -Path $Log -Pattern 'CORNERSNAP-APPLIED' |
    ForEach-Object {
        if ($_.Line -match 'frame (\d+) obj (\S+)') {
            [pscustomobject]@{ Frame = $matches[1]; Obj = $matches[2] }
        }
    } |
    Group-Object Frame, Obj |
    Where-Object { $_.Count -ge 3 } |
    Select-Object Name, Count

if ($Player) { $rows = $rows | Where-Object { $_.Name -match [regex]::Escape($Player) } }

Write-Output "=== Frames with >=3 APPLIED crossings (MUST be empty for PASS) ==="
if ($rows) { $rows | Format-Table -AutoSize } else { Write-Output "  (none) -- PASS" }

$suppressed = (Select-String -Path $Log -Pattern 'CORNERSNAP-SUPPRESSED' | Measure-Object).Count
$applied    = (Select-String -Path $Log -Pattern 'CORNERSNAP-APPLIED'    | Measure-Object).Count
$portal     = (Select-String -Path $Log -Pattern 'CORNERSNAP-PORTAL'     | Measure-Object).Count
Write-Output ""
Write-Output "Totals: APPLIED=$applied  SUPPRESSED=$suppressed  PORTAL(raw)=$portal"
Write-Output "(SUPPRESSED >= 1 proves the guard fired; >=3-APPLIED frame count is the pass/fail metric.)"
