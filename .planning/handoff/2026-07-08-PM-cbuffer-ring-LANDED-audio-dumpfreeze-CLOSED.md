# 2026-07-08 PM — cbuffer NO_OVERWRITE ring LANDED (865×) · load-fade audio glitch CLOSED (watchdog dump freezes) · portal arc soak CLEAN

**READ FIRST after restart.** This session (2026-07-08 daytime) ran three
threads: (1) the **cbuffer NO_OVERWRITE ring** — the D3D11 perf arc's next
item per the 07-08 AM handoff — implemented, live-verified, committed;
(2) a Win32 audio-dropout report that unraveled into closing the
long-backlogged **load-fade "volume glitch"** on BOTH platforms — root cause
was the stall watchdog's own minidump writes, never a fade bug; (3) Kenny's
Win32 portal soak came back **clean → the portal see-through arc is fully
closed at six fixes / four defects.** Everything committed + pushed:
`origin/master` = `1f51476c5`.

---

## 1. WHERE TO RESUME — the D3D11 perf arc, remaining work

The cbuffer ring was the **last known structural per-draw D3D9-ism** in the
gl11 hot path (the 07-08 AM handoff's framing — confirmed: after the ring,
census steady-state shows no per-draw rename class left). What remains in
the arc is the **load/hitch class + soak closures**, in priority order:

1. **Zone-in `GroundScene::init` residual ~600ms** (known class; dumps in
   `stage-x64/logs-archive/`). Note from this session's forensics: part of
   the measured "stall" at scene change is the engine's INTENTIONAL 1-second
   music-crossfade loop in `SwgCuiManager.cpp` (~line 350 — pumps
   `Audio::alter`+`Sleep(5)` for 1.0s inside ONE main-loop frame). The
   watchdog counts it as a mega-stall; treat it as a false positive when
   symbolizing (stack: `GroundScene::init` → `emitSceneChange` →
   `SwgCuiManagerNamespace::Listener::receiveMessage` → `Sleep`). The REAL
   residual is the burst of 100-500ms stalls after it (asset/GPU-create
   class). Now that dumps are off (§3), re-measure with log-only telemetry
   before choosing targets.
2. **Charselect avatar first-draw hitch** (~371ms one specimen on gl11).
   Adjacent ticket for gl05: the Phase-32 bytecode-cache port
   (memory `project_phase32_d3dcompile_recompile_leak`).
3. **First-visit texture pre-warm** (interior cold hitch; backlog).
4. **`preventDriverInternalThreading=false` soak closure** — x64 has run
   clean since 07-06 with NV driver threading re-enabled; the VB ring
   (CONSULT-58) and now the cbuffer ring removed the rename churn that fed
   the original nvwgf2um race. If Kenny calls the soak done: flip the CODE
   default to false in ConfigDirect3d11.cpp and close the CONSULT-58 todo.
5. **Optional ring stacking:** a redundant-update skip (hash/memcmp the
   payload before appending) if census ever shows slots re-pushing identical
   data — steady-state today is vsB0≈draws (41/frame), i.e. one world-matrix
   push per draw, which is inherent engine flow, cheap on the ring, and NOT
   worth touching unless a heavier scene says otherwise.
6. **Probe/diag strip pass** (one commit, AFTER Kenny declares soaks done):
   portal probe suite (grep `PortalCullProbe`, `CONSULT-6`, `STUCK0`,
   `CELLSTATE`, `KILLDETAIL`), cfg keys portalCullProbe/stallWatchdog*/
   audioDiagLog/censusLog. ALSO in scope now: fix-or-retire the dead
   samp-struct audio diag reads (§4 trap).

So: **no big structural D3D11 rewrite is queued** — the arc's remaining work
is hitch-hunting with the (now dump-free) watchdog telemetry, two soak
closures, and cleanup. If Kenny wants a new deep-perf target, the census CSV
(now with cbVsDiscards/cbPsDiscards columns) on a heavy scene is the place
to look for the next dominant cost.

## 2. CBUFFER NO_OVERWRITE RING — LANDED (`1568807f3`)

- **What:** per-slot `Map(WRITE_DISCARD)` per cbuffer update (~90 driver
  renames/frame; vsB0 median 39) → ONE 1MB dynamic buffer per stage,
  1280-byte entries appended `Map(WRITE_NO_OVERWRITE)`, slots bound via
  `VS/PSSetConstantBuffers1` offset windows (80 constants). Rename only on
  ring wrap (~1 per 27 frames).
- **Correctness spine:** per-slot CPU shadows of the full zero-padded entry
  give (a) the P19_CBUF_ZERO_TAIL guarantee (unwritten registers read 0
  through c79) and (b) wrap safety — a mid-ring DISCARD renames the memory
  under the OTHER slots' live bindings, so the wrap path rewrites all live
  slots' shadows into the fresh allocation and rebinds them.
- **Feature probes at install** (any miss → legacy path unchanged, logged):
  `ID3D11DeviceContext1` QI, `D3D11_FEATURE_D3D11_OPTIONS`
  .ConstantBufferOffsetting AND .MapNoOverwriteOnDynamicConstantBuffer
  (plain 11.0 forbids NO_OVERWRITE maps on cbuffers), the 1MB CreateBuffer.
- **Kill switch:** `[Direct3d11] constantBufferRing=false` (default ON, no
  cfg edit needed). **Regression signature:** stale/garbage constants —
  flat cycling colors, wrongly-transformed objects → flip the switch first.
- **Gotcha fixed en route:** the pre-Present gamma pass (Device.cpp
  applyBcgPass) saved/restored PS b0 with raw `PSGet/PSSetConstantBuffers`,
  which silently DROPS the D3D11.1 FirstConstant offset → restore now goes
  through `Direct3d11_ConstantBuffer::bindPS(0)` (correct in both modes).
- **Verified:** report-log line `NO_OVERWRITE ring active…`; Kenny's live
  world session (15,057 frames): **484,672 updates → 560 renames = 865×**;
  census steady rows `vsB0=41, cbVsDiscards=0, cbPsDiscards=0`. Both
  platforms feel smooth (Kenny).
- Census CSV schema grew two trailing columns: `cbVsDiscards,cbPsDiscards`
  (legacy mode counts every update there; ring mode counts wraps — the
  column pair is the before/after metric).

## 3. LOAD-FADE AUDIO GLITCH — CLOSED (both platforms; `1f51476c5` + cfg)

The 07-04 backlog item "load-in dips = zone-in duck × title-fade
superposition (cosmetic)" is **closed as a misdiagnosis**. The real cause:
**the stall watchdog's minidump writes.** `MiniDumpWriteDump` freezes EVERY
thread (including the Miles mixer) 0.2-1.3s per dump, and the zone-in dump
burst lands exactly in the title-fade window (x64 specimen: 9 dumps
11:35:36-39 bracketing the 36.3→37.3 fade; Win32 equivalent burst confirmed
same morning). One defect, two accents: Win32/7.2a = hard DROPOUTS (short
mix-ahead), x64/9.3v = brief ATTENUATIONS (deeper buffering).

- **Fix (cfg, gitignored, applied both `stage/client.cfg` and
  `stage-x64/client.cfg`):** `stallWatchdogMaxDumps=0` = **LOG-ONLY mode** —
  threshold lines still flow (telemetry intact), no dump writes. Verified
  clean by Kenny on BOTH platforms same day ("sound clean all around").
  **Re-arming the dump budget = accepting zone-in audio dips as collateral**
  (both cfg comments say so). The 07-04 fade-pump dt clamp was itself a
  band-aid for these same dump freezes.
- **Eliminated en route** (Kenny asked "did we add volume clamps?" — no):
  title .snd decoded from `data_other_00.tre` → `soundCategory=8` =
  SC_backGroundMusic (duck-exempt, properly categorized); bg fade variable
  never dipped; no CATVOL at the transition; fade math runs in lockstep
  with the crossfade loop; VBR estimated-end pop ruled out by timing.
- **Separate real find — Win32 world-load dropouts** (pre-fix) were ALSO
  partly genuine main-thread stalls, but the dump writes amplified 100ms
  stalls into 1.7s freezes. With dumps off, Win32 is clean.

## 4. ⚠️ AUDIO DIAG TRAP + THE NEW VOLSET PROBE

- **TRAP (open follow-up):** the per-stream `VOLSTEP`/`STARVED`/`chunks`
  probes in `audioDiagUpdate` (Audio.cpp) read `stream->samp` fields through
  the COMPILE-TIME miles-9.3b (x64) / miles-7.2e (Win32) struct layouts.
  **Neither shipped runtime matches anymore** — x64/9.3v logs
  `chunks=-856759528` garbage, Win32/7.2a logs constant `0/0`. They were
  only ever valid on the retired 9.3b DLL. Do NOT trust those lines;
  fix-or-retire is queued with the probe-strip pass (§1.6).
- **NEW: `VOLSET` probe** (`1f51476c5`, in `Audio::setSampleVolume` stream
  branch, gated by the existing `[ClientAudio] audioDiagLog`): edge-logs OUR
  final computed volume at the `AIL_set_sample_volume_levels` handoff for
  music streams (|Δ| > 0.02) with every term of the product
  (`in/master/cat/global/bg`). No Miles struct reads → trustworthy on any
  runtime. This is now THE tool for "music volume did something weird":
  VOLSET lines present → the named term is guilty; silent while audible →
  the defect is inside the Miles DLL.
- Trust ranking for audio-diag.log lines: engine-side
  (VOLSET/STOPSOUND/DUCK/CATVOL/FILEOPEN/OPENCALL/CLOSE/EOSPOLL) = good;
  samp-struct (VOLSTEP/STARVED/chunks in heartbeats) = lies.

## 5. PORTAL ARC — FULLY CLOSED

Kenny's Win32 soak of the CONSULT-66 splitInstance fix came back clean
("Soak was clean") → the see-through arc closes at **six fixes / four
defects**. Probe suite stays ARMED as tripwires (per 07-08 AM handoff §3 —
regression signatures documented there). Strip pass queued (§1.6).

## 6. BUILD/STAGE/COMMIT STATE

- `master` == `origin/master` == `1f51476c5`:
  - `1568807f3` perf(d3d11): cbuffer NO_OVERWRITE ring (5 files, gl11
    plugin + config — plugin-internal headers, NO shared-header ABI cascade).
  - `1f51476c5` diag(audio): VOLSET probe (Audio.cpp only).
- **Staged this session:** x64 Release (gl11_r.dll + SwgClient_r.exe) and
  Win32 Release (gl11_r.dll + SwgClient_r.exe) — both carry ring + probe,
  both link-gated (0 unresolved). gl05/06/07 untouched (no shared-header
  change). Debug configs remain stale; canonical 5-target on both platforms
  still due at next natural close-out.
- **Cfg edits this session (gitignored, live in both staging dirs):**
  `stallWatchdogMaxDumps=0` (§3). Win32 `stage/client.cfg` and x64
  `stage-x64/client.cfg` are otherwise in key-parity (verified by
  key-only diff; only rasterMajor 5/11 differs by design).

## 7. GOTCHAS LEARNED THIS SESSION

- **Watchdog dump budget = audio dips.** Any future stall hunt that arms
  `stallWatchdogMaxDumps>0` will re-introduce zone-in dropouts/attenuations.
  Log-only first; arm dumps only when a stall class actually needs a stack.
- **The 1s scene-change crossfade loop is a watchdog false positive**
  (§1.1) — don't burn dump budget or analysis time on it.
- **`gl11-census.csv` and the other `fopen_s` diag files are DENY-SHARE
  locked** while the client runs (fopen_s default) — read them after the
  client exits; even PowerShell shared-read FileStream fails.
- **Git Bash `kill -0`/`tasklist` can't see native Windows PIDs** — use
  PowerShell `Get-Process -Id` for liveness checks of launched clients.
- **PS 5.1 native-arg quoting mangles embedded double quotes** in
  `git commit -m @'...'@` here-strings (the '"' sequences hit the native
  layer broken) → write the message to a file and `git commit -F` for any
  message containing quotes.
- Old audio-diag sessions accumulate in one file (append mode) — anchor
  parsing at the LAST `audio-diag armed` line; time-only filters match
  prior days.
- `stallWatchdogMaxDumps=0` semantics confirmed in code (Game.cpp ~1200):
  threshold gate is `stallWatchdogMs`; 0 dumps = budget exhausted
  immediately, logging unaffected.

## 8. OPEN ITEMS SNAPSHOT

- D3D11 perf arc backlog: §1 list (GroundScene::init residual, charselect
  first-draw, texture pre-warm, driver-threading soak closure, probe strip).
- Audio backlog (shrunk this session): late-sample-start wave +
  endOfSample2dCallBack Miles-thread map-walk audit (UAF-class) +
  duplicate ambients. Load-fade glitch/dips CLOSED (§3). Dead samp-struct
  diag reads → fix-or-retire (§4).
- Real-door trigger brittleness (gameplay polish, portal arc leftover).
- ilm-extract audit; maintainer Utinni v15 rebind (carry).
- Memory files updated this session:
  `project_audio_miles_handle0_callback_rootcause` (dump-freeze closure +
  diag trap), `project_d3d11_peak_perf_arc_tre_runtime_reads` (ring landed),
  MEMORY.md index lines.
