# 2026-07-09 — stall STACK SAMPLER landed (CONSULT-68) · shader-cache RAM preload + string-table buffering kill the zone-in burst · all pushed

**READ FIRST after restart.** One-morning session, one arc thread: the D3D11
perf arc's hitch-hunt (the 07-08 PM handoff §1 resume point). Built the
missing attribution tool first, and its FIRST session convicted two stall
classes that were then fixed, verified (3 clean world loads: 2 Kenny live,
1 smoke), committed, and pushed. `origin/master` = `eed72a74a`:

- `0280aee69` diag(watchdog): audio-safe stall stack sampler (Game.cpp only)
- `fc97dfebf` perf(d3d11): shader-cache RAM preload (ConfigDirect3d11.{h,cpp}
  + Direct3d11_ShaderCache.cpp — gl11 plugin-internal, NO ABI cascade)
- `eed72a74a` perf(file): TreeFileFactory whole-file buffering (TreeFile.cpp)

**Bottom line:** zone-in went from ~13 stalls / ~4s cumulative main-thread
freeze (plus a separate 1.2s charselect first-draw hitch) to **3 stalls**:
the intentional 1s crossfade mega (false positive) + 149ms + 300ms of
WorldSnapshot residual. Charselect hitch is GONE.

---

## 1. THE STACK SAMPLER (CONSULT-68) — the new standing attribution tool

**Why:** `stallWatchdogMaxDumps=0` (the 07-08 audio-dip fix) left the
watchdog with stall durations but no stacks — `MiniDumpWriteDump` freezes
every thread including the Miles mixer (0.2-1.3s/dump = the zone-in dips).

**How it works** (all in `Game.cpp`, `StallWatchdogNamespace`, ~1100-1500):
- Watchdog thread (existing 20ms tick) detects a stalled frame, then:
  suspend ONLY the main thread (it is already stalled), `GetThreadContext`,
  memcpy its committed stack pages into a static 256KB snapshot buffer,
  resume. **Suspend window is microseconds and touches ONLY syscalls +
  memcpy** (no CRT/heap/dbghelp — the suspended thread could hold those
  locks).
- Then, at leisure on the watchdog thread: `StackWalk64` over the SNAPSHOT
  (custom read-memory callback serves stack reads from the copy, module/
  unwind reads from live memory) + `SymGetSymFromAddr64`/`SymGetLineFromAddr64`
  → one log line per sample: `loop N stack @Xms: func@file.cpp:line < ...`.
  Uses the SYSTEM dbghelp.dll (own Sym session; DebugHelp's
  dbghelp_6.3.17.0.dll crash-handler instance untouched). PDBs found next to
  the exe (staged). PDB-driven walk = FPO-correct on x86 too.
- Re-samples every 20ms tick while the same frame stays stalled → a long
  stall becomes a time-weighted profile. Identical consecutive samples are
  run-length compressed (`stack x9 more identical (through 1072 ms)`).
- Config: `[ClientGame] stallWatchdogStackSamples` = max samples per stall,
  **default 100, 0 = off**. No cfg edit was needed (both cfgs already arm
  `stallWatchdogMs=100` + `stallWatchdogMaxDumps=0`). Arm line now reads
  `... budget 0 dumps, stack samples 100/stall`.
- Coexists with dumps: sampling runs regardless of dump budget; dump logic
  untouched.

**How to use:** have Kenny (or a smoke launch — auto-login goes all the way
into world) zone in, then read `stall-watchdog.log` AFTER the client exits
(fopen_s deny-share lock). Anchor at the LAST `stall watchdog armed` line.
Tally top frames with a `grep "stack @" | sed/sort | uniq -c` pass.

**Caveats / regression signatures:**
- Theoretical deadlock: main thread suspended while holding the ntdll
  inverted-function-table lock (only during module load). Never observed;
  kill switch = `stallWatchdogStackSamples=0`.
- First sample of a session pays PDB load on the watchdog thread (~100-500ms
  of MISSED TICKS, main thread unaffected).
- The 1s crossfade mega-stall shows as
  `SleepEx < SwgCuiManagerNamespace::Listener::receiveMessage@SwgCuiManager.cpp:350
  < ... GroundScene::init` — INTENTIONAL, don't chase it (07-08 handoff §1.1).

## 2. FIX A — gl11 shader-cache RAM preload (`fc97dfebf`)

**Conviction (sampler session 1):** `Direct3d11_ShaderCache::tryLoad` did a
synchronous `fopen`+`fread` of `<hash>.cso` per shader variant INSIDE
`applyPreDrawState` (the draw path). Charselect avatar first-draw = a
**single 1.2s fread** (cold file); zone-in showed ~7 more 100-160ms stalls
(VS variants, PS rewrites, radar first-draw). This closes the 07-08 §1.2
"charselect avatar first-draw hitch" ticket for gl11.

**Fix:** at `install()`, a `std::thread` slurps every 16-hex `.cso` in the
cache dir into `unordered_map<uint64_t, vector<unsigned char>>` (x64: 430
files/2MB; Win32: 1105/25MB). `tryLoad` = mutex-guarded map lookup →
`D3DCreateBlob` + memcpy. **After preload completes** (`ms_preloadDone`
acquire/release), a map miss is authoritative — the draw path NEVER touches
disk. During the brief preload window, misses fall back to the per-hit read.
`store()` writes the file AND inserts into the map (session coherence).
`remove()` joins the thread (wired at Direct3d11.cpp:285 — a joinable
std::thread destructed unjoined would terminate).

- **Kill switch:** `[Direct3d11] shaderCachePreload=false` (default ON).
- **Verify signature:** `SwgClient_report.log` →
  `Direct3d11_ShaderCache: preload complete (430 cached shaders in RAM)`.
- **Regression signatures:** shutdown hang = preload-thread join; memory
  bump = the map (2-25MB, accepted); anything shader-visual → flip the
  switch first.
- **Scope note:** gl11 only. Win32 `stage/` defaults `rasterMajor=5` (gl05)
  → no preload line there, by design. The **gl05 sibling ticket stays open**
  (Phase-32 bytecode-cache port, memory
  `project_phase32_d3dcompile_recompile_leak`).

## 3. FIX B — TreeFileFactory whole-file MemoryFile buffering (`eed72a74a`)

**Conviction (sampler, after fix A):** with ShaderCache gone the profile was
DOMINATED (31+ samples, 500-800ms stalls) by
`LocalizationManager::fetchStringTable` ←
`ClientObject::preloadSomeLocalizedNameTables` ← `GroundScene::updateLoading`.
Root cause = read amplification: `LocalizedString::load_0001` does 3-4 tiny
`fl.read()` calls PER STRING, and on a streamed `FileStreamerFile` each read
is a blocking `Gate::wait` round-trip to the FileStreamer I/O thread.

**Fix:** `TreeFile::TreeFileFactory::createFile` (TreeFile.cpp:1067) now
reads the whole file ONCE and returns a `MemoryFile(AbstractFile*)` wrapper
(existing sharedFile class; ctor does `readEntireFileAndClose`) — the parser
runs against memory. Falls back to a fresh streamed handle if buffering
fails. **This factory only feeds string-table loads** (SetupSharedGame's
`LocalizationManager::install` + the DataLint path in Game.cpp:2652), so no
other consumer semantics change. No kill switch (structural, tiny, fallback
built in); revert = the one commit. External localization lib UNTOUCHED.

## 4. VERIFICATION RECORD (all in `stage-x64/stall-watchdog.log`, sessions of 2026-07-09)

- **Baseline** (session armed 06:36:16, pre-fixes exe but WITH sampler):
  loop 699 charselect 1198ms = one ShaderCache fread; loop 1123 zone-in
  1753ms mega (crossfade + tail); burst loops 1124-1242 with 407/558/793ms
  string-table stalls + repeated ShaderCache opens. ~13 stalls total.
- **After fix A** (armed 06:45): ShaderCache frames = 0, charselect stall
  gone; profile now string-table dominated (the fix-B conviction data).
- **After both** (armed 06:51): **3 stalls** — 1365ms crossfade mega, 149ms
  (texture zlib inflate), 300ms (WorldSnapshot CollisionBuckets::build +
  template loads). LocalizedString samples 47+ → 1.
- Win32 boot gate (armed 06:53, gl05): alive at world entry, one 105ms
  CommandTable stall, 0 ShaderCache/LocalizedString samples.
- Kenny live: "logged into world, clean" ×2 + "Loaded into world smooth".

## 5. BUILD/STAGE/COMMIT STATE

- `master` == `origin/master` == `eed72a74a` (fetched before push; clean ff).
- Staged this session: **x64** SwgClient_r.exe + gl11_r.dll, **Win32**
  SwgClient_r.exe + gl11_r.dll — all Release, all link-gated (0 unresolved).
  gl05/06/07 untouched (no shared-header change; TreeFile.cpp is exe-side).
  Debug configs remain stale; canonical 5-target on both platforms still due
  at next natural close-out (carried from 07-08).
- No cfg edits this session (both new features default ON; watchdog keys
  unchanged).
- Memory files updated: `project_d3d11_peak_perf_arc_tre_runtime_reads`
  (sampler + both fixes), MEMORY.md index line.

## 6. GOTCHAS LEARNED THIS SESSION

- **Auto-login is armed in the staged cfgs** — a smoke `Start-Process` of
  SwgClient_r.exe goes ALL THE WAY into the world (~45s). Great for
  unattended zone-in telemetry; kill the process when done.
- **Win32 `stage/` runs gl05 by default** (rasterMajor=5) — don't expect
  gl11 log lines there; only fix B applies to that stack.
- The x64 exe intermediate lives at `src/compile/win32/SwgClient/x64/Release/`
  (NOT `Release_x64/`) — force-relink deletes must target that path.
- A single cold-file `fread` can cost >1s (charselect specimen) — "small
  file, one read" is NOT cheap under cold cache/AV scan; RAM-preload or
  batch patterns win.
- MSBuild `/v:m` logs via PowerShell redirect are UTF-16; `Select-String`
  handles them, and "error"-pattern hits in linker verbose output are mostly
  benign symbol names (`ErrorMessage`, `GetLastError`) — gate on
  `unresolved external symbol` count exactly.

## 7. WHERE TO RESUME — updated arc backlog

The 07-08 §1 list, updated:

1. ~~GroundScene::init residual~~ → **largely CLOSED** by fixes A+B. The
   floor is now: (a) the intentional 1s crossfade (reframe = optional
   polish: spread `SwgCuiManager.cpp:350`'s crossfade pump across frames
   instead of one Sleep loop inside a single frame), (b) ~300-450ms of
   WorldSnapshot object-creation (CollisionBuckets::build, template loads —
   the CONSULT-59/60 budgeted stepper overshooting; tighten budgets or move
   collision-bucket build off-thread if it ever matters).
2. ~~Charselect first-draw (gl11)~~ → **CLOSED** (fix A). gl05 sibling
   (Phase-32 bytecode-cache port) still open.
3. First-visit texture pre-warm (interior cold hitch; backlog).
4. `preventDriverInternalThreading=false` soak closure — unchanged; Kenny's
   call, then flip the code default in ConfigDirect3d11.cpp.
5. Probe/diag strip pass (after soaks): portal probe suite + samp-struct
   audio diag retire (07-08 §4). **The stack sampler is NOT in strip scope —
   it is the standing attribution tool** (log-only, zero cost when no
   stalls).
6. Audio backlog + real-door trigger brittleness + ilm-extract audit +
   maintainer Utinni v15 rebind (all carried, unchanged).

For any new hitch report: zone in once with the sampler armed (it already
is, both cfgs), read the last session in `stall-watchdog.log`, tally the
`stack @` lines. That's the whole workflow now.
