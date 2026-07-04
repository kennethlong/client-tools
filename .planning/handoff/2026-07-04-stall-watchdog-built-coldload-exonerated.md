# 2026-07-04 session — stall watchdog BUILT + cold-load creation-class EXONERATED

**Status: watchdog code DONE + verified end-to-end, UNCOMMITTED (one file: Game.cpp).
Armed in stage/client.cfg. Waiting on: Kenny's next organic play session → symbolize the
real stall dumps.** This continues the CONSULT-58 arc (see the 2026-07-03-PM handoff,
"Late-evening addendum 2") — the gl11 ring fix + flag-off soak.

---

## Arc context (1 paragraph)

CONSULT-58 fixed the gl11 dynamic-VB ring (cursor commit-at-unlock, `d09a62198`) → 19x
hitch-rate collapse with NV driver threading re-enabled. Remaining before closing the
churn-reduction todo: (1) multi-session flag-off soak w/ zone-ins → flip code default,
(2) convict the residual >100ms load-class pauses, (3) optional vsB0 cbuffer ring. This
session advanced (1) and (2) and built the instrument for finishing (2).

## Soak session 2 analyzed (stage/gl11-census.csv, overwritten each run — data was 2026-07-03 ~10:07pm)

- 3.3 min, gl11, flag=false. Steady-state median 12.1ms, p99 26.3, 8.7% >20ms — looks
  softer than session 1 (9.6ms / 0.6%) but the >20ms histogram is almost entirely
  20-25ms missed-vsync variance in a heavier scene (38-54 draws vs session 1); **no
  hang-class stutter**. Kenny's verdict: **gl11 now feels better than gl05/D3D9**.
- **Included a survived zone-in** (2.6s load frame ~idx 3896) with NO nvwgf2um crash —
  counts double for soak confidence.
- Soak log lives in the todo:
  [.planning/todos/pending/2026-07-03-gl11-map-discard-churn-reduction.md](../todos/pending/2026-07-03-gl11-map-discard-churn-reduction.md)

## Cold-load pauses: creation-class EXONERATED (the census columns did their job)

The >100ms stalls carry **ZERO or near-zero resource creates in the stalled frame**:
- Cleanest specimen: a **688ms mid-play frame with the same 37 draws as its neighbors
  and 0 tex/staging/shader/layout creates** — a pure non-graphics main-thread stall.
- The 2.6s zone frame: 0 creates in-frame; creates trail it (5 tex + 3 shaders the NEXT
  frame) as arriving assets — a symptom, not the cost (3 shaderCreates ≠ 258ms).
- Whole session: only 45 tex / 22 shader creates total; ~0 on normal frames.

**Verdict: pre-warm / off-thread D3D creation is the WRONG lever.** The stall is
upstream main-thread load work (TreeFile/disk/decompress/world-build). This converges
with the audio-pops todo (`2026-07-03-audio-pops-3d-sound-delays.md`), which already
suspected TreeFile-lock contention on the audio IO thread — same stalls, same suspect.

## Stall watchdog BUILT + verified (the conviction instrument)

**Where:** file-local `StallWatchdogNamespace` in
`src/engine/client/library/clientGame/src/shared/core/Game.cpp`, directly above
`Game::runGameLoopOnce`, + one heartbeat call `stallWatchdogFrameTick()` as its first
statement. Census-style single-file diagnostic — no new files, no vcxproj edits, no
shared headers → **no plugin ABI cascade** (exe-only rebuild).

**Behavior:**
- Watchdog thread (ABOVE_NORMAL prio) polls a per-frame QPC heartbeat every 20ms.
- Frame running past `[ClientGame] stallWatchdogMs` (0 = off, armed at **100**) →
  whole-process **MiniDumpNormal** (ALL thread stacks — audio thread comes free) as
  `stall-loop<N>-s<K>.mdmp` in the working dir (= stage/). Second sample (s2) if the
  same frame is still stalled at **5x threshold** (long zone stalls get two stacks).
- `stall-watchdog.log`: armed line + per-stall lines + **final total-duration line for
  every stall** (even past the dump budget → frequency data forever). Totals include
  dump-write time (MiniDumpWriteDump suspends the process while serializing).
- Guardrails: dump budget `[ClientGame] stallWatchdogMaxDumps` (armed at **6**);
  **unfocused** stalls (alt-tab/drag = message-pump blocks) log-only, no dump spent;
  first 8 boot frames ignored; loads the **SYSTEM dbghelp.dll** (NOT DebugHelp's
  `dbghelp_6.3.17.0.dll` instance) so the crash handler's **single-use OOM
  address-space reserve stays armed** for a real crash (DebugHelp::writeMiniDump
  releases it on first call — deliberately not reused).

**Verification (done, artifacts deleted):** built SwgClient Release/Win32 clean
(0 unresolved, staged 2026-07-03 10:46pm); ran the client at threshold=1ms — armed
line ✓, focus gating ✓, exactly 6 dumps then budget-spent lines ✓, total-duration
close-outs ✓, MDMP magic ✓, and **cdb symbolized a test dump against our staged PDBs**
(`C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe -z <dump>
-y 'D:\Code\swg-client-v2\stage;D:\Code\swg-client-v2\src\compile\win32\SwgClient\Release;srv*'
-c '~0k; q'` → main thread correctly in `Clock::limitFrameRate → Sleep`). Threshold
then restored to 100, test dumps + log deleted.

## Current local state

- **UNCOMMITTED: Game.cpp** (the watchdog). Commit when Kenny asks.
- **stage/client.cfg** (BOM-clean, verified `23 20`): gl11 (`rasterMajor=11`) +
  `preventDriverInternalThreading=false` (soak) + `censusLog=true` +
  `stallWatchdogMs=100` + `stallWatchdogMaxDumps=6`. Code default for the NV flag is
  still TRUE — flip only after several clean sessions incl. zone-ins.
- client_d.cfg does NOT have the watchdog keys (defaults off) — add if soaking Debug.
- Staged `SwgClient_r.exe` = HEAD + watchdog; `gl11_r.dll` = HEAD (9:52pm, unchanged
  since — correct, no cascade).
- Untracked as before: CONSULT-56/57 `.out` transcripts + CONSULT-57 diff snapshot.

## NEXT (in order)

1. ~~Kenny plays organically~~ **DONE 2026-07-04 AM** — 4 sessions, 24 dumps + log banked.
2. ~~Symbolize the real stall dumps~~ **DONE — CONVICTIONS BELOW.**
3. From the named call: fix the stall class (likely async-ify or pre-stage the blocking
   load) → re-soak → flip `preventDriverInternalThreading` code default to false →
   close the churn-reduction todo (its item 3, vsB0 cbuffer ring, is optional margin).
4. **Remove the diagnostics when convicted:** censusLog + watchdog cfg keys off (code
   can stay — both are config-gated no-ops when off).
5. Parked open items (unchanged from the 07-03-PM handoff): ilm-extract Legends-
   preference audit todo; maintainer Utinni v15 rebind + smoke (consumer-side); SSHT
   heap-corruption sibling (may be moot — a recurrence now writes a real mdmp).

---

## 2026-07-04 AM CONVICTIONS (symbolized from Kenny's 4 organic sessions)

Kenny's symptom report: big charselect bring-in pause, music glitches at opening, rough
first load (pausing/skipping), smooth once settled EXCEPT intermittent 3D-sound skips.
Every symptom maps to a named, dump-proven stall class:

**(1) World-entry mega-stall — 3.1–4.5s, reproduced 4/4 sessions** (loops 1421/1660/2225/2014).
Both time-samples in multiple sessions land in the SAME chain:
`GameNetwork::receiveCmdStartScene → Game::_setScene → GroundScene ctor → GroundScene::load
→ AppearanceTemplateList::createAppearance(<planet>.trn) → ClientProceduralTerrainAppearanceTemplate::create
→ preloadAssets → PreloadManager::PreloadManager` — which **synchronously fetches EVERY
shader-group family child + EVERY flora appearance (recursively preloaded) + every
radial-flora shader for the whole planet** (ClientProceduralTerrainAppearanceTemplate.cpp:53-108,
ungated, runs inside create() at :156). Per fetch: TreeFile::open (caught mid-CreateFileA
probing loose SearchPathA), IFF parse, texture create, and the gl11 ShaderEffect→
ShaderImplementation→**D3DCompile PS** chain (2225-s2 caught it inside
Direct3d11_PixelShaderProgramData ctor — including the STALE Plan 17-04.X PSRC-dump
fflush block, Direct3d11_PixelShaderProgramData.cpp:1757-1781, marked REMOVE, minor cost).

**(2) Post-entry burst — ~10 stalls of 100–800ms over the next ~10s** (loops 1422-1525 etc.).
`AsynchronousLoader::processCallbacks → MeshAppearanceTemplate::asynchronousLoadCallback →
ShaderPrimitiveSetTemplate::load → ShaderTemplateList/TextureList::fetch → Texture::load`.
The async loader reads bytes off-thread but the CALLBACK does the full template parse +
GPU resource creation on the main thread, and processCallbacks drains **unbudgeted** —
all pending callbacks per frame. (This also explains the census's "creates trail the
stall" signature.) Fix pattern precedent: `maxInteriorCreatesPerFrame`.

**(3) Charselect avatar bring-in — 371ms** (loop 545): first draw of the avatar through
`CuiWidget3dObjectListViewer::Render` → skeletal draw → first-use gl11 state/shader
creation. One sample — softer conviction, same load-on-first-use class.

**(4) Mid-play isolated stalls — 110–620ms, ~one per few minutes** (specimen loop 2117):
target change → `SwgCuiStatusGround::updateTargetName → LocalizationManager::fetchStringTable
→ LocalizedString::load → FileStreamer::File::read` — **synchronous string-table disk
fetch inside the UI render path** on first-target-of-a-type.

**(5) Audio ride-along:** in every stall dump the Miles threads are idle in
WaitForSingleObject — NOT blocked on TreeFile locks. Music/3D glitches during load =
main-thread starvation (Audio service pumps from the main loop). Settled-play census is
CLEAN (18.4k frames: median 10.1ms, p99 17.0ms, 0 frames >80ms) → the residual settled-play
3D-sound skipping is likely audio-thread/sample-side (the audio-pops todo), not frame
stalls — plus the sparse ~110ms one-offs of class (4).

**Recommended fix levers, in impact order:** (A) make the terrain PreloadManager fetch
list async/budgeted (it runs under the loading screen — spreading it keeps music alive
and the load-time similar); (B) per-frame time budget in AsynchronousLoader::processCallbacks;
(C) async/prefetch localization string tables off the target-change path; (D) delete the
stale PSRC-dump diagnostic block. Design consult (CONSULT-59) recommended before A/B.

Artifacts: `stage/stall-loop*.mdmp` + `stage/stall-watchdog.log` (keep until fixed);
symbolized stacks in the session scratchpad (cdb line above reproduces).

---

## 2026-07-04 PM — ALL FOUR LEVERS IMPLEMENTED (CONSULT-59 crew round)

Full design record + verification plan:
[CONSULT-59-loadstall-fixdesign-SYNTHESIS.md](../research/CONSULT-59-loadstall-fixdesign-SYNTHESIS.md)
(evidence pack + 4 consultant outputs alongside it). Key crew moment: Opus's D1 design
assumed chunk creation demand-fetches on miss; **Codex refuted it** (the appearance's
ShaderCache is a second eager planet-wide loader; getTextures has no miss-fetch), and my
read added that createClientChunk runs on the ClientTerrain WORKER thread — so the fix
uses strict phase ordering (warm up fully → only then submit chunk requests), not
concurrent fill.

**Touched files:**
- **A (terrain warm-up):** ClientProceduralTerrainAppearanceTemplate.{h,cpp} (capture-only
  ctor + budgeted step + GC guard), ClientProceduralTerrainAppearance_ShaderCache.{h,cpp}
  (preloadShaders no longer called by ctor, now public), ClientProceduralTerrainAppearance.{h,cpp}
  (updateTerrainWarmup pump in alter, createChunk + calculateLod + terrainGenerationStabilized
  gates), ConfigClientTerrain.{h,cpp} (`[ClientTerrain] terrainPreloadBudgetMs`, default 40,
  <=0 = old synchronous behavior), GroundScene.cpp (loading-block pump).
- **B (async-loader budget):** ConfigSharedFile.{h,cpp} + AsynchronousLoader.cpp
  (`[SharedFile] asynchronousLoaderCallbackTimeBudgetMs`, code default 0 = old behavior;
  drain-all bypass in remove() so shutdown can't spin). **Armed at 6 in stage/client.cfg.**
- **C (string tables):** ClientObject.{h,cpp} (queue table at endBaselines +
  preloadSomeLocalizedNameTables) + GroundScene.cpp loading pump (20ms/frame) →
  public LocalizationManager::fetchStringTable (memoized; `preload()` is private). Fixes the whole class (combat spam,
  tooltips, radial, chat bubbles — Sonnet's sweep). Residual: species first seen mid-play
  still demand-loads (rare; deliberate — don't move the sync load to spawn time).
- **D:** stale Plan 17-04.X PSRC dump block deleted (Direct3d11_PixelShaderProgramData.cpp).

## 2026-07-04 PM(2) — CONSULT-60: WorldSnapshot phased load IMPLEMENTED (the ~3s residual)

The loop-16982 class (WorldSnapshot::load IFF parse, ~3s in the GroundScene ctor) is
fixed the same way as the terrain preload: `load()` is now a cheap prologue; the node
parse (new WorldSnapshotReaderWriter::beginIncrementalLoad/stepIncrementalLoad, map
built per-subtree), per-AREA buildout tables, and sphere tree run in budgeted
`WorldSnapshot::loadStep()` pumped from GroundScene's loading block
(`[ClientGame] worldSnapshotParseBudgetMs`, default 40, <=0 = old sync behavior).
Gates: donePreloading/getLoadingPercent parse-aware (counters read as DONE mid-parse
otherwise — the loading screen would drop over a half-built world); update()/
preloadSomeAssets held while pending; **force-finish exactness valves** on
isClientCached/loadIfClientCached (miss-only; the int-max guard filters live ids)/
removeObject/moveObject/addObject/detailLevelChanged/findClosestCellIdFromWorldPosition
— any consumer needing complete data mid-parse finishes the parse synchronously (worst
case = old cost, exactly for the rare POB-interior login whose containment beats the
pumped parse). unload() cancels in-flight parse (quit-during-load / startScene chain).
Also: Audio::setLargePreMixBuffer 64→1024 fragments (~1s music mix-ahead during load).
Crew round CONSULT-60 (synthesis + 4 outputs in .planning/research/): all four
converged on chunked-main-thread; Codex killed the worker-thread option
(SharedBuildoutAreaManager/DataTableManager unlocked globals); queue-and-flush was
REJECTED for the valves (can't save the one-shot player endBaselines cell attach).
Built clean, staged 10:03 AM. Boot+login watchdog-verified stall-free (zero >100ms
frames — but that session never zoned in; NO auto-login exists). **UNCOMMITTED.
REMAINING: one real login + zone-in with watchdog armed** (expectations + regression
signatures in the CONSULT-60 synthesis §Verification; also in the worldsnapshot todo).

## 2026-07-04 PM(3) — CONSULT-61: the audio arc's ROOT CAUSE found (file-callback layer)

Kenny's next session ("door sound late, footsteps late, still popping in music") +
the armed audio-diag probe (caught mus_theme_tatooine PERMANENTLY starved from
zone-in) triggered a 4-crew round. **Codex's decisive find: `s_nextFileHandle`
started at 0 and Miles' async IO treats FileHandle==0 as "unopened → open by (empty)
name" — the entire 2026-06-18 title-music empty-name shim was scaffolding over that
one-character bug, and its one-slot global state corrupts every multi-stream scenario
(cross-stream offset/name swaps = decoder pops; mid-read substituted-handle closes =
permanently dead streams).** Full record:
[CONSULT-61-audio-popping-SYNTHESIS.md](../research/CONSULT-61-audio-popping-SYNTHESIS.md).
LANDED (staged 11:31 AM, UNCOMMITTED): handles start at 1; shim default OFF
(`[ClientAudio] titleMusicStreamFix` re-arms); critical section over the four file
callbacks (they were the CONSULT-56 unguarded-container class across main + Miles IO
threads); `streamBufferBytes=0` (the 1MB test coupled 18-56s of PRIME latency — Miles
has no prime cap; that was the "music started late"); probe now also logs VOLSTEP
lines (one-frame volume steps — Sonnet's code-confirmed pop candidate: 9.3b
AIL_set_sample_volume_levels has NO ramp and Sound2d volume-wander SNAPS at
interpolationRate=0; pre-existing, likely newly-dominant). FILED (next wave, Opus's
anatomy): late door/footsteps = cold synchronous first-touch sample load inside
playSound + event templates freed between one-shots (fix: keep-alive LRU pin +
interior pre-warm + drop the redundant duration-probe decode; handle cap formally a
red herring). NEXT: Kenny session → diag log decides (no starved streams expected;
VOLSTEP-at-pop = volume-snap conviction → ramp fix).

**ROUND 2 (staged 11:48 AM):** Kenny's session confirmed the dead-stream class CURED
(theme plays to completion) but zone-in still skipped with a big skip at the music
change. Watchdog dumps convicted: (1) `SwgCuiAvatarCreationHelper::stopMusic` +
`restartMusic` = original-SOE DELIBERATE 1s `Sleep(5)` blocking loops on the main
thread in the scene-change path (stall-loop1164, 1.47s) — now fire-and-forget; (2) the
loading pumps' own 1-SECOND slice budgets (WorldSnapshot preloadSomeAssets
cms_callbackTime 1.f, CachedFileManager 1000ms, SpacePreloadedAssetManager 1.f;
stall-loop1168, 1.38s) — all now 50ms. "3D sounds late 15-20s then settle" = the filed
cold-first-play wave (unchanged). See synthesis §Round 2.

---

**Kenny's live verify (2026-07-04 ~10:30): "buttery smooth after initial startup" —
zone-in freeze CONFIRMED FIXED. Residual: "music skipping in load-in screens" →
root-caused (see synthesis §Audio correction): the premix bump was the wrong layer;
the MUSIC STREAM buffer is Miles' 1-second default (`AIL_open_stream(...,0)`) and its
refills go through our TreeFile callbacks, which contend with the loading pumps' file
opens. FIX: stream buffer 0 → 1 MB (Audio.cpp:631). Rebuilt + staged; awaiting Kenny's
next load-in for the music verdict. Whole CONSULT-60 wave still uncommitted.**

---

**VERIFIED 2026-07-04 ~9:29 AM (live boot, watchdog armed):** built clean (0 unresolved),
booted, auto-logged into Mos Eisley end-to-end; loading screen held and dismissed
correctly (no gate regression, no black terrain); Kenny's felt verdict: **"pretty
smooth, music still glitches a little during startup."** Watchdog data for the whole
boot+zone-in: THREE stalls total (vs the AM baseline's 4.5s mega-stall + ~10-stall
burst + budget exhaustion): loop 16982 **3146ms = WorldSnapshot::load IFF parse**
(pre-existing cost previously hidden UNDER the terrain preload in the same ctor frame —
now the dominant residual, filed as
[2026-07-04-worldsnapshot-parse-startup-freeze.md](../todos/pending/2026-07-04-worldsnapshot-parse-startup-freeze.md));
loops 16997/17063 (626/938ms) = the new string-table pump paying for one big .stf each
under the loading screen (by design — replaces the mid-combat stall; noted in the same
todo). Terrain preload appears in NO stack — lever A convicted-and-cured.

Watchdog (Game.cpp) remains uncommitted from the AM session; commit the whole wave
together when Kenny says so.

**Verify expectations + regression signatures:** see SYNTHESIS §Verification plan
(loading screen must hold until terrain primed; black terrain tiles = gate regression;
never-dismissing loading screen = warm-up not pumping; zone-in hang = lock class).
Deferred tickets: TreeFile loose-searchPath stat-storm negative-cache; charselect 371ms
avatar first-draw.

## Regression signatures

- Watchdog itself: it is OFF unless `stallWatchdogMs>0`. If enabled and the client
  misbehaves: dumps flooding = threshold too low; a hang WHILE a dump writes is
  MiniDumpWriteDump suspending the process (expected, ~100-300ms per dump, budgeted 6).
- NV flag-off soak: an nvwgf2um crash = revert cfg `preventDriverInternalThreading=true`
  and keep the mdmp (it's data — the handler writes a real one now).
- CONSULT-56/57 fixes: zone-in HANG = lock regression; `destroyShader` WARNING in
  SwgClient_report.log = eviction accounting imbalance.
