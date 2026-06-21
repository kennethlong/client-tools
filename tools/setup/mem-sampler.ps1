# mem-sampler.ps1 -- 32-bit/x64 memory-leak sampler for SwgClient.
#
# Method (per memory project_x64_not_impacted_by_32bit_mem_leak):
#   Sample PrivateMemorySize64 every N seconds and compare the FLOOR at the SAME
#   in-world state across cantina cycles -- NOT the in-cantina load spike. A real
#   leak is monotonic and never returns on cell exit; a healthy build returns memory
#   on each cantina EXIT and decelerates.
#
# Columns: iso8601, privateMB, workingSetMB, handles, threads, gdi
#   gdi = GetGuiResources(hProcess, 0) (GDI object count).
#
# Usage:
#   powershell -File tools/setup/mem-sampler.ps1 -Process SwgClient_r -IntervalSec 5 -OutCsv stage/mem-trace-win32.csv
#
param(
    [string]$Process = "SwgClient_r",
    [int]$IntervalSec = 5,
    [string]$OutCsv = "stage/mem-trace-win32.csv"
)

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Gui {
    [DllImport("user32.dll")] public static extern uint GetGuiResources(IntPtr hProcess, uint uiFlags);
}
"@

"timestamp,privateMB,workingSetMB,handles,threads,gdi" | Out-File -FilePath $OutCsv -Encoding ascii
Write-Host "Sampling '$Process' every ${IntervalSec}s -> $OutCsv (Ctrl-C to stop)"

while ($true) {
    $p = Get-Process -Name $Process -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($p) {
        $priv = [math]::Round($p.PrivateMemorySize64 / 1MB, 1)
        $ws   = [math]::Round($p.WorkingSet64 / 1MB, 1)
        $gdi  = [Gui]::GetGuiResources($p.Handle, 0)
        $ts   = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ss")
        $line = "$ts,$priv,$ws,$($p.HandleCount),$($p.Threads.Count),$gdi"
        $line | Out-File -FilePath $OutCsv -Append -Encoding ascii
        Write-Host "$ts  priv=${priv}MB  ws=${ws}MB  h=$($p.HandleCount)  t=$($p.Threads.Count)  gdi=$gdi"
    } else {
        Write-Host "(waiting for $Process ...)"
    }
    Start-Sleep -Seconds $IntervalSec
}
