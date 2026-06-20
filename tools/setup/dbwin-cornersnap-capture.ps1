# Non-debugger OutputDebugString capture (DBWIN reader, DebugView-style).
# Captures DEBUG_REPORT_LOG output WITHOUT attaching a debugger, so the
# Report.cpp:166 "OutputDebugString resets FPU precision under the debugger"
# HACK does NOT fire -> we observe TRUE float behavior of the door-snap.
#
# Writes ALL OutputDebugString lines to -RawLog and CORNERSNAP-only lines to -SnapLog.
param(
  [string]$RawLog  = "D:\Code\swg-client-v2\stage-x64\cornersnap-x64-raw.log",
  [string]$SnapLog = "D:\Code\swg-client-v2\stage-x64\cornersnap-x64-capture.log"
)

Add-Type -Namespace DbWin -Name Native -MemberDefinition @'
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
  public static extern IntPtr CreateFileMapping(IntPtr hFile, IntPtr lpAttrs, uint flProtect, uint dwMaxHi, uint dwMaxLo, string lpName);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern IntPtr MapViewOfFile(IntPtr hMap, uint dwAccess, uint hi, uint lo, UIntPtr dwBytes);
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
  public static extern IntPtr CreateEvent(IntPtr lpAttrs, bool bManualReset, bool bInitialState, string lpName);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool SetEvent(IntPtr hEvent);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern uint WaitForSingleObject(IntPtr hHandle, uint ms);
'@

$PAGE_READWRITE = 0x04
$FILE_MAP_READ  = 0x0004
$INVALID_HANDLE = [IntPtr]::Zero
$WAIT_TIMEOUT   = 258

$hMap = [DbWin.Native]::CreateFileMapping([IntPtr](-1), [IntPtr]::Zero, $PAGE_READWRITE, 0, 4096, "DBWIN_BUFFER")
if ($hMap -eq [IntPtr]::Zero) { Write-Error "CreateFileMapping DBWIN_BUFFER failed (another DebugView/monitor running?)"; exit 1 }
$pBuf = [DbWin.Native]::MapViewOfFile($hMap, $FILE_MAP_READ, 0, 0, [UIntPtr]::Zero)
if ($pBuf -eq [IntPtr]::Zero) { Write-Error "MapViewOfFile failed"; exit 1 }

$hReady = [DbWin.Native]::CreateEvent([IntPtr]::Zero, $false, $true,  "DBWIN_BUFFER_READY")  # auto-reset, initially signaled
$hData  = [DbWin.Native]::CreateEvent([IntPtr]::Zero, $false, $false, "DBWIN_DATA_READY")    # auto-reset, initially clear

"# DBWIN capture started $(Get-Date -Format o) (PID-tagged OutputDebugString)" | Out-File -FilePath $RawLog -Encoding ascii
"# CORNERSNAP-filtered capture started $(Get-Date -Format o)"                   | Out-File -FilePath $SnapLog -Encoding ascii
Write-Output "DBWIN monitor live. Raw -> $RawLog ; CORNERSNAP -> $SnapLog. Launch the game now."

[DbWin.Native]::SetEvent($hReady) | Out-Null
while ($true) {
  $r = [DbWin.Native]::WaitForSingleObject($hData, 1000)
  if ($r -eq $WAIT_TIMEOUT) { continue }
  $pid_   = [System.Runtime.InteropServices.Marshal]::ReadInt32($pBuf)
  $msg    = [System.Runtime.InteropServices.Marshal]::PtrToStringAnsi([IntPtr]($pBuf.ToInt64() + 4))
  [DbWin.Native]::SetEvent($hReady) | Out-Null
  if ($null -eq $msg) { continue }
  $line = ("[{0}] {1}" -f $pid_, $msg.TrimEnd("`r","`n"))
  Add-Content -Path $RawLog -Value $line -Encoding ascii
  if ($msg -match "CORNERSNAP") { Add-Content -Path $SnapLog -Value $line -Encoding ascii }
}
