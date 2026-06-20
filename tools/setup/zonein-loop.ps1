<#
  zonein-loop.ps1 -- headless repro/regression harness for the gl11 x64 zone-in (#4 armor .mgn) and the
  shutdown-teardown crash (CuiWorkspace::updateMediatorEnabledStates).

  Each iteration, under cdb (catches the FIRST access violation -> full dump + stack):
    1. Launch SwgClient_r.exe with cfg-driven auto-login (swg/swg) + auto-select avatar -> world load
       (loop-autoconnect.cfg; skipIntro/skipSplash so no static startup pages).
    2. Let it zone in (-WalkAfter), then WALK a churn circuit (-WalkSeconds) through the city so
       marauder NPCs cross LOD boundaries / enter+leave load range while their .mgn loads pend --
       this is where #4 fires (a stationary char never churns -> 50/50 clean proved that).
    3. Clean-exit via "/quit" typed in chat (the proper exit -> Game::quit -> ExitChain teardown, the path
       that crashed). Fallback to WM_CLOSE if /quit doesn't take.
    4. cdb writes crashdumps\zonein-real.dmp on any AV; the loop preserves it as zonein-caught-iterN.dmp
       and stops. Surviving an iteration cleanly = both load-in and shutdown teardown were clean.

  Dumps are preserved (deleted once at start; existence after an iteration == caught).
#>
param(
  [int]$Iterations      = 30,
  # No zone-in detection exists (client writes no per-run log to tail), so timing is by offset:
  #   WalkAfter  = wait this long after launch before walking -- avatar is reliably in-world by then
  #               (covers cold load-in: .cso cleared + TRE standby evicted = slowest load).
  #   WalkSeconds= duration of the churn circuit. #4's likely trigger (CONSULT-37) is an async .mgn
  #               load COMPLETING while its object is torn down / LOD-evicted -- which a STATIONARY
  #               char never produces (50/50 clean proved that). Walking makes marauders cross LOD
  #               boundaries / enter+leave load range while their belt .mgn is mid-load.
  [int]$WalkAfter       = 18,
  [int]$WalkSeconds     = 16,
  [int]$ShutdownSeconds = 45
)

$ErrorActionPreference = 'Stop'
$root   = 'D:\Code\swg-client-v2'
$stage  = Join-Path $root 'stage-x64'
$exe    = Join-Path $stage 'SwgClient_r.exe'
$cdb    = 'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe'
$catch  = Join-Path $root 'tools\setup\zonein-loop-catch.cdb'
$dumpDir= Join-Path $stage 'crashdumps'
$dump   = Join-Path $dumpDir 'zonein-real.dmp'
$env:_NT_SYMBOL_PATH = $stage
if (-not (Test-Path $dumpDir)) { New-Item -ItemType Directory $dumpDir | Out-Null }

Add-Type @'
using System;using System.Runtime.InteropServices;
public class Win {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int n);
  [DllImport("user32.dll")] public static extern int  PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk,byte scan,uint flags,UIntPtr extra);
  public const uint WM_CLOSE = 0x0010;
  static void Tap(byte scan){ keybd_event(0,scan,0x0008,UIntPtr.Zero); System.Threading.Thread.Sleep(45);
                              keybd_event(0,scan,0x000A,UIntPtr.Zero); System.Threading.Thread.Sleep(45); }
  // type "/quit" + Enter via scan codes (Set 1). Enter opens chat; THEN backspace x12 to clear any stray
  // movement chars that leaked in (held-W trailing 'w's made "ww/quit" -> invalid -> no exit); then type.
  public static void SendQuit(){
    Tap(0x1C);                                          // Enter -> open/focus chat input
    for(int k=0;k<12;k++) Tap(0x0E);                    // Backspace x12 -> clear stray "ww" etc.
    byte[] seq = { 0x35, 0x10, 0x16, 0x17, 0x14, 0x1C }; // / q u i t Enter
    foreach(byte s in seq) Tap(s);
  }
  // Re-front the game window (focus drifts between bursts -- recipe trap).
  static void Front(IntPtr h){ ShowWindow(h,9); SetForegroundWindow(h); System.Threading.Thread.Sleep(250); }
  // Genuinely HELD key (down, dwell, up). Arrows need the extended-key bit (down 0x0009 / up 0x000B).
  static void HoldScan(byte scan, uint downF, uint upF, int ms){
    keybd_event(0,scan,downF,UIntPtr.Zero); System.Threading.Thread.Sleep(ms);
    keybd_event(0,scan,upF,UIntPtr.Zero);   System.Threading.Thread.Sleep(120);
  }
  // Walk a churn circuit for ~seconds: hold W forward, then alternate turning Right/Left. The point is
  // NOT to navigate -- it is to make NPCs (marauders) cross LOD boundaries / enter+leave load range while
  // their .mgn loads are still pending (Sonnet RANK-1 destroy-during-pending-load window). Re-front before
  // each leg so injected keys land in the game, not whatever stole focus.
  public static void WalkCircuit(IntPtr h, int seconds){
    Front(h); System.Threading.Thread.Sleep(2000);             // settle: window must own focus before 1st key
    long endT = Environment.TickCount + seconds*1000;
    byte[] turns = { 0x4D, 0x4B }; // Right, Left (Set-1 scan, extended)
    int t = 0;
    while (Environment.TickCount < endT){
      Front(h); HoldScan(0x11, 0x0008, 0x000A, 3500);            // W forward ~3.5s
      Front(h); HoldScan(turns[t++ % turns.Length], 0x0009, 0x000B, 900); // turn ~quarter
    }
    // release EVERY movement key so nothing buffers into the chat line when /quit opens it
    keybd_event(0,0x11,0x000A,UIntPtr.Zero); keybd_event(0,0x4B,0x000B,UIntPtr.Zero); keybd_event(0,0x4D,0x000B,UIntPtr.Zero);
    System.Threading.Thread.Sleep(600);
  }
}
'@

$cfgFileArgs = @('--', '@loop-autoconnect.cfg')
function Kill-All { Get-Process SwgClient_r,cdb -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue; Start-Sleep -Milliseconds 400 }

# --- Per-iteration cold-cache forcing (maximize the #4 async-.mgn race window) ---
# IMPORTANT (verified 2026-06-19): the BUILD clears NO disk cache.
#   * The .cso shader-cache is keyed by HLSL-source hash and is INTENTIONALLY persisted across
#     launches AND rebuilds (Direct3d11_ShaderCache.cpp:139 "Cache files... NOT deleted"). A C++-only
#     rebuild leaves every .cso warm. So clearing it here is NOT "what the build does" -- it is a
#     race-window AMPLIFIER: forcing D3DCompile every iter stalls the main thread longer, widening
#     the window where async .mgn loads pend (the cold-cache mechanism CONSULT-37 converged on).
#   * The REAL "first run after rebuild is cold" effect = incidental eviction of the TRE archives from
#     the Windows standby/file cache by the rebuild's heavy disk writes. The build never purges it.
#     To replicate faithfully, drop Sysinternals EmptyStandbyList.exe into tools\setup\ and run this
#     loop ELEVATED -- the guarded call below activates automatically. Without it, the .cso clear alone
#     still widens the window. (profiles/ is per-character UI state, unrelated -- deliberately untouched.)
$shaderCache = Join-Path $stage 'stage\shader-cache'   # x64 client writes .cso here (nested stage\)
$standbyTool = Join-Path $root 'tools\setup\EmptyStandbyList.exe'
function Force-ColdCache {
  if (Test-Path $shaderCache) {
    $n = (Get-ChildItem $shaderCache -Filter *.cso -ErrorAction SilentlyContinue).Count
    Remove-Item (Join-Path $shaderCache '*.cso') -Force -ErrorAction SilentlyContinue
    Write-Host ("  cold-cache: cleared {0} .cso (forces D3DCompile -> wider async window)" -f $n) -ForegroundColor DarkCyan
  }
  if (Test-Path $standbyTool) {
    # standbylist = evict the OS file cache (TRE archives) -> faithful "first run after rebuild" cold.
    & $standbyTool standbylist 2>$null | Out-Null
    Write-Host "  cold-cache: evicted OS standby list (TRE file cache cold)" -ForegroundColor DarkCyan
  }
}

Kill-All
if (Test-Path $dump) { Remove-Item $dump -Force }   # delete ONCE; existence later == a real catch
$caughtAt = 0
for ($i = 1; $i -le $Iterations; $i++) {
  Write-Host ("[{0:HH:mm:ss}] iter {1}/{2}: launch + auto-zone-in..." -f (Get-Date), $i, $Iterations)
  Force-ColdCache   # force every iteration cold (not just iter 1) to maximize the #4 repro chance
  $log = Join-Path $dumpDir ("zonein-iter{0}.log" -f $i)
  $cdbProc = Start-Process -FilePath $cdb -ArgumentList (@('-logo',$log,'-cf',$catch,$exe)+$cfgFileArgs) -WorkingDirectory $stage -PassThru

  # --- Phase A: zone-in + churn walk (where #4 can fire) ---
  $walked = $false; $sentQuit = $false; $sentClose = $false; $tQuit = $null
  $tLaunch  = Get-Date
  $deadline = (Get-Date).AddSeconds($WalkAfter + $WalkSeconds + $ShutdownSeconds + 10)
  while ((Get-Date) -lt $deadline) {
    if (Test-Path $dump) { break }                       # cdb caught an AV (load / walk / shutdown)
    if ($cdbProc.HasExited) { break }                    # client exited (clean /quit, or died)
    $elapsed = ((Get-Date) - $tLaunch).TotalSeconds
    $cli = Get-Process SwgClient_r -ErrorAction SilentlyContinue

    if (-not $walked -and $elapsed -ge $WalkAfter) {
      if ($WalkSeconds -le 0) {
        # #4 fires VERY EARLY in world load (user-confirmed), NOT in-game -> the walk is irrelevant to it.
        # WalkSeconds<=0 skips the walk: the early zone-in burst (where the [MGN-PROBE] sites fire) already
        # happened during the -WalkAfter dwell. Just /quit. Faster iterations = more early-load samples.
        Write-Host "  early-load dwell done (no walk); /quit"
      } elseif ($cli -and $cli.MainWindowHandle -ne 0) {
        Write-Host ("  in-world; walking {0}s circuit to churn NPC LODs" -f $WalkSeconds)
        [Win]::WalkCircuit($cli.MainWindowHandle, $WalkSeconds)   # blocking; a crash mid-walk is caught by cdb
      } else { Write-Host "  no client window (auto-connect failed? see $log)" }
      $walked = $true
    }
    elseif ($walked -and -not $sentQuit) {
      if ($cli -and $cli.MainWindowHandle -ne 0) {
        Write-Host "  /quit to exercise shutdown teardown"
        [Win]::ShowWindow($cli.MainWindowHandle,9)|Out-Null; [Win]::SetForegroundWindow($cli.MainWindowHandle)|Out-Null
        Start-Sleep -Milliseconds 700; [Win]::SendQuit()
      }
      $sentQuit = $true; $tQuit = Get-Date
    }
    elseif ($sentQuit -and -not $sentClose -and ((Get-Date) - $tQuit).TotalSeconds -ge 18) {
      if ($cli -and $cli.MainWindowHandle -ne 0) { Write-Host "  /quit didn't exit; WM_CLOSE fallback"; [Win]::PostMessage($cli.MainWindowHandle,[Win]::WM_CLOSE,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null }
      $sentClose = $true
    }
    Start-Sleep -Seconds 1
  }

  Start-Sleep -Seconds 2   # grace for cdb to finish writing the dump
  if (Test-Path $dump) {
    $kept = Join-Path $dumpDir ("zonein-caught-iter{0}.dmp" -f $i)
    Move-Item $dump $kept -Force
    Write-Host "  !! CAUGHT on iter $i -> $kept (stack in $log)" -ForegroundColor Red
    $caughtAt = $i; Kill-All; break
  }
  if (-not $cdbProc.HasExited) {
    # never exited and no AV = genuine shutdown hang; snapshot stacks before killing
    $c = Get-Process SwgClient_r -ErrorAction SilentlyContinue
    if ($c) { Write-Host "  !! shutdown HANG (no exit, no AV) -- snapshotting" -ForegroundColor Red
              & $cdb -pv -p $c.Id -lines -logo (Join-Path $dumpDir "hang-iter$i.log") -c "~*kb 40; .dump /ma $dumpDir\hang-iter$i.dmp; q" | Out-Null }
    $caughtAt = $i; Kill-All; break
  }
  Write-Host "  OK iter ${i}: zoned in + clean /quit shutdown" -ForegroundColor Green
  Kill-All
}

Kill-All
if ($caughtAt) { Write-Host ("CAUGHT on iteration {0}. See {1}\zonein-caught-iter{0}.dmp / hang-iter{0}.* + zonein-iter{0}.log" -f $caughtAt, $dumpDir) -ForegroundColor Red }
else          { Write-Host ("SURVIVED all {0} iterations -- zone-in + shutdown teardown clean." -f $Iterations) -ForegroundColor Green }
