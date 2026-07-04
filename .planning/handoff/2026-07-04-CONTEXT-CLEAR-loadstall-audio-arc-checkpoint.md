# 2026-07-04 CONTEXT-CLEAR checkpoint — load-stall + audio arc (CONSULT-59/60/61)

**READ FIRST after context clear.** One marathon session: Kenny's morning symptom
report → stall-watchdog dumps convicted every stall class → three crew rounds
(CONSULT-59/60/61) → fixes landed in waves. Zone-in went from a 4.5s freeze + burst to
buttery smooth; the audio layer's 8-year-class root cause was found. **A large verified
but UNCOMMITTED set is in the working tree — committing it is the first order of
business** (Kenny said "write a handoff and I will clear context"; he has NOT yet said
commit — ask or wait for his verdict on the last build first).

---

## 1. COMMITTED already (master, local — NOT pushed)

- `fa97caa2d` feat(diag): stall watchdog (Game.cpp; `[ClientGame] stallWatchdogMs=100`,
  `stallWatchdogMaxDumps=6` — armed in stage/client.cfg, still on).
- `b46a83871` perf(load): CONSULT-59 wave — budgeted terrain preload
  (`[ClientTerrain] terrainPreloadBudgetMs=40` default, <=0 = old sync; ShaderCache no
  longer preloads in its ctor — **populate-fully-before-first-chunk-request invariant,
  the terrain worker thread reads its slots unlocked — do NOT "optimize" to concurrent
  fill**), AsynchronousLoader wall-time budget
  (`[SharedFile] asynchronousLoaderCallbackTimeBudgetMs`, armed 6 in client.cfg),
  localized-name string-table prefetch (ClientObject::endBaselines queue +
  GroundScene loading pump 20ms/frame), stale gl11 PSRC-dump diagnostic deleted.
- `42109a3f7` docs: CONSULT-59 records.

## 2. UNCOMMITTED code (all built clean 0-unresolved, staged exe 11:48 AM, live-tested
   through several Kenny sessions — commit as 2-3 atomic commits when he gives the word)

**Wave A — WorldSnapshot phased load (CONSULT-60, VERIFIED "buttery smooth"):**
- `sharedUtility/WorldSnapshotReaderWriter.{h,cpp}` — beginIncrementalLoad /
  stepIncrementalLoad / insertSubtreeIntoNetworkIdMap (per-root-subtree map insert;
  sync load(filename) API untouched for server/tools).
- `clientGame/WorldSnapshot.{h,cpp}` — load() = cheap prologue (unload → heap Iff →
  beginIncrementalLoad → SYNC SharedBuildoutAreaManager::load [GroundScene:825 needs
  it] → phase init); budgeted `loadStep()` phases wsNodes (Iff freed at parse end) →
  buildout (per-AREA) → sphereTree (4096/batch) → done;
  `[ClientGame] worldSnapshotParseBudgetMs` (default 40, <=0 = old sync via
  finishLoadNow). Gates: donePreloading/getLoadingPercent parse-aware (counters are
  0/0 mid-parse = would read DONE and drop the loading screen);
  update()/preloadSomeAssets held while pending; **force-finish exactness valves**
  (finishLoadNow) on isClientCached/loadIfClientCached (miss-only, int-max guard
  filters live ids)/addObject/moveObject/removeObject/detailLevelChanged/
  findClosestCellIdFromWorldPosition; unload() cancels in-flight parse.
- `clientGame/ConfigClientGame.{h,cpp}` — the budget key.
- `clientGame/GroundScene.cpp` — loadStep pump line in the loading else-block
  (~:2081, before preloadSomeAssets).

**Wave B — audio arc (CONSULT-60 ring + CONSULT-61 root cause + round 2):**
- `clientAudio/Audio.cpp` (one file, many pieces):
  - **RadSS output ring 2048ms at device open** (`DIG_DS_FRAGMENT_CNT` before
    AIL_open_digital_driver — ring TotalMs is read AT OPEN, genericdig.cpp:339) +
    setLargePreMixBuffer **1500ms** (MUST stay < ring or SS_serve's
    remainder-goes-negative fill loop overruns unplayed audio = the crackle we
    shipped for ~30min at 1024-vs-256 default ring) + setNormal stays 16ms.
  - **CONSULT-61 ROOT CAUSE: `s_nextFileHandle` now starts at 1** — Miles async IO
    treats FileHandle==0 as "unopened → open by (EMPTY) name" (milesasync.cpp:440-443);
    handle 0 is why the 2026-06-18 title-music empty-name shim ever existed. Shim
    (`s_titleMusicStreamFix`) now **defaults OFF** (`[ClientAudio] titleMusicStreamFix`
    re-arms); its ONE-SLOT global state corrupted every multi-stream scenario
    (cross-stream offset/name swaps = pops; mid-read substituted-handle closes =
    permanently starved streams — the dead mus_theme_tatooine in audio-diag.log).
  - **Critical section over the four Miles file callbacks** (s_fileMap/counter/shim
    were unsynchronized across main + Miles IO threads — CONSULT-56 class).
  - `[ClientAudio] streamBufferBytes` (default 0 = stock; the 1MB experiment coupled
    18-56s of PRIME latency — Miles has NO prime cap, mssstrm.cpp:791-793 — reverted).
  - **audio-diag probe** (`[ClientAudio] audioDiagLog`, armed): STARVED edge lines
    (init-state STARVED at stream open = artifact; PERSISTENT starved = dead stream —
    but also normal end-of-track residue on completed non-looping music), VOLSTEP
    lines (one-frame volume steps >0.05 — Sonnet's unramped-volume pop candidate),
    500ms summaries (the chunks=X/8 field is bimodal/useless).
- `swgClientUserInterface/SwgCuiAvatarCreationHelper.cpp` — **round-2 conviction:
  stopMusic/restartMusic were DELIBERATE 1s `Sleep(5)+Audio::alter` blocking loops on
  the main thread in the scene-change path** (stall-loop1164, 1.47s = Kenny's "big
  skip on music change") — now fire-and-forget like CuiManager::stopMusic.
- `clientGame/SpacePreloadedAssetManager.cpp`, `clientGame/WorldSnapshot.cpp`
  (cms_callbackTime 1.f → 0.05f), `sharedUtility/CachedFileManager.cpp` (1000ms →
  50ms) — **the loading pumps' own internal ONE-SECOND slice budgets**
  (stall-loop1168, 1.38s) — total load work unchanged, frames stay ~50ms so the
  mixer queue survives.

**Planning docs modified/untracked:** handoff README + the 07-04 handoff (multiple
addenda), audio-pops + worldsnapshot todos, CONSULT-60/61 evidence/task/synthesis/.out
files (commit with the waves). CONSULT-56/57 .out leftovers stay untracked (historic).

## 3. Verification state (Kenny's reports, chronological)

1. Morning: 4.5s charselect freeze + rough first load + mid-play sound skips →
   ALL convicted by watchdog dumps, fixed in CONSULT-59 (committed).
2. "Felt pretty smooth, music still glitches during startup" → residual = WorldSnapshot
   3.1s parse → CONSULT-60 phased load.
3. "Buttery smooth after initial startup. Music skipping in load-in screens" →
   zone-in freeze CONFIRMED FIXED; music = audio arc.
4. "Audio still skips/crackles on world load + crackles in game" → convicted OUR
   premix-1024-vs-256ms-ring overrun (fixed: ring 2048 @ open, large=1500) + probe built.
5. "Door sound late, footsteps late, still popping" → CONSULT-61 crew round →
   handle-0 root cause + callback locks landed.
6. "Still skips on zone-in, big skip on music change; theme now plays to completion;
   3D sounds late 15-20s then settle" → dead-stream class CURED; round-2 convictions
   (blocking fades + 1s pump slices) fixed, **staged 11:48 — AWAITING Kenny's verdict.
   This is the current test.**

## 4. NEXT (in order)

1. **Kenny plays the 11:48 build**: EXPECT music-change skip GONE, zone-in skips gone
   or sub-100ms, watchdog near-silent through zone-in, no dead streams in
   audio-diag.log. 3D-sounds-late-for-15-20s persists BY DESIGN until item 3.
2. **Commit the whole uncommitted set** when Kenny says (suggest: wave A commit, wave
   B commit, docs commit — exact staged paths per §2; gate on exact staged set, the
   tree also has untracked CONSULT-56/57 leftovers that STAY untracked).
3. **Late-sample-start fix wave** (filed, designed — Opus's full anatomy in
   CONSULT-61-audio-popping-SYNTHESIS + the Opus task result): (a) keep-alive LRU pin
   of recently played event SoundTemplates (footsteps re-decompress EVERY step today —
   templates freed between one-shots), (b) pre-warm interior event sounds at load
   (CONSULT-59 name-table pattern), (c) compute sample duration from the WAV header
   instead of the throwaway AIL_set_named_sample_file probe (Audio.cpp:1640-1650,
   :4733-4751). maxSampleCount is a RED HERRING (exhaustion drops, never delays).
4. **If pops persist** in the 11:48 build: check audio-diag.log VOLSTEP lines at pop
   moments → convicts Sonnet's unramped-volume-snap class (9.3b
   AIL_set_sample_volume_levels is a bare assignment, wavefile.cpp:5161; Sound2d sets
   volume EVERY frame, volume-wander SNAPS at interpolationRate==0,
   Sound2d.cpp:553-556) → fix = engine-side volume lerp (max step/frame) in
   Audio::setSampleVolume or Sound2d. If pops WITHOUT VOLSTEP → Sonnet's provider/
   sample-rate matrix (getFrequency()=22050 hardcoded; test 44100) in the Sonnet
   consult result.
5. **Diagnostics removal when arcs close**: watchdog + census + audioDiag cfg keys off
   (all code config-gated no-ops when off). Then flip
   `preventDriverInternalThreading` code default false + close the gl11 churn todo
   (CONSULT-58 item — soak evidence is long since sufficient).
6. Parked: TreeFile loose-searchPath stat-storm negative-cache (next dominant load
   cost per Sonnet's CONSULT-60 sweep); charselect avatar 371ms first-draw; ilm-extract
   audit; maintainer Utinni v15 rebind; x64 rebuild of all this (only Win32 built
   today — canonical 5-target + x64 build needed before any close-out).

## 5. Regression signatures / gotchas

- Loading screen never dismisses → WorldSnapshot parse-pending gate stuck (check
  worldSnapshotParseBudgetMs>0 and the GroundScene pump); missing world buildings →
  buildout chunking dropped rows; player logged into a POB spawning OUTSIDE →
  containment valve failed (loadIfClientCached miss path).
- Zone-in ~2-4s single stall returns → a finishLoadNow valve fired (acceptable if
  rare — a POB-interior login whose containment beats the pumped parse; investigate
  if every login).
- Title/login music silent → re-arm `[ClientAudio] titleMusicStreamFix=true` (should
  NOT be needed — handle-0 was the real cause; if this happens the handle fix has a
  hole).
- Audio crackle during LOADING screens specifically → check setLargePreMixBuffer value
  vs the ring (large mix-ahead MUST be < DIG_DS_FRAGMENT_CNT ms set before device
  open; negative remainder = force-fill overrun).
- `stage/client.cfg` armed keys: stallWatchdogMs=100, stallWatchdogMaxDumps=6,
  censusLog=true, asynchronousLoaderCallbackTimeBudgetMs=6, audioDiagLog=true,
  streamBufferBytes=0. cfg is BOM-clean; NEVER write it with PS Set-Content.
- cdb symbolize line (x86, staged PDBs):
  `cdb.exe -z <dump> -y 'D:\Code\swg-client-v2\stage;D:\Code\swg-client-v2\src\compile\win32\SwgClient\Release;srv*' -c '~0k; q'`
- Old stall dumps cleared; current stage/stall-loop11xx dumps = the round-2 zone-in
  cluster (keep until Kenny confirms the 11:48 build).

## 6. Reference docs (all in .planning/research/)

- CONSULT-59-loadstall-fixdesign-{EVIDENCE,SYNTHESIS,codex.out,cursor.out} — terrain
  preload / async budget / string tables design record.
- CONSULT-60-worldsnapshot-parse-{EVIDENCE,SYNTHESIS,codex.out,cursor.out} — phased
  snapshot load + the audio ring/prime corrections.
- CONSULT-61-audio-popping-{EVIDENCE,SYNTHESIS,codex.out,cursor.out} — the audio root
  cause + round 2; Opus's late-sample-start anatomy and Sonnet's pop matrix live in
  this session's agent results, summarized in the SYNTHESIS (§remaining suspects).
- Memories updated: project_loadstall_consult59_fix_wave (+ MEMORY.md); an audio
  root-cause memory should be written at next session start if missing.

---

## 7. ROUND 3 (post-checkpoint, 07-04 PM) — Kenny's verdict on 11:48 + the 1:32 PM build

**Kenny's 11:48-build verdict (sessions at ~11:17 [old build, 1MB streamBuffer still
armed], ~13:02 [11:48 build, stock buffers]):** two pieces of music wrong duration, big
gap at the title→theme transition, popping just as bad or WORSE (mid-stream, NOT at
fades/volume changes — he calls the VOLSTEP/unramped-volume theory a probable red
herring; cutouts were <1s spurts; flow ORDER was still correct).

**MUSIC FLOW SPEC (Kenny, canonical — do not re-derive):** title `mus_title_lp.mp3`
loops through login/charselect AND ~4-5s into the zone-in screen (historically = the
old sync-load freeze window), THEN 1s fade-out, then the planet theme
(`mus_theme_tatooine.mp3`) plays ONCE to its natural end into the world. The flow was
NEVER broken before this fix wave; the ONLY original problem was mid-stream pops/
crackles. The 4-5s title overlap was an artifact of the load freeze we removed, so the
transition now fires immediately at zone-in click — order intact, timing compressed.

**Round-3 convictions + fixes (built clean 1:32 PM, staged, AWAITING Kenny):**
1. **CS-across-I/O crackle (mine, made popping worse):** the CONSULT-61 file-callback
   critical section was held across TreeFile::open AND full main-thread one-shot sample
   reads (footsteps re-read their file EVERY step) → Miles' IO thread blocked mid-
   music-stream-fill = crackle correlated with gameplay sounds. FIX: lock narrowed to
   map/shim state only; ALL TreeFile I/O now runs unlocked (safe: single consumer per
   handle; Miles syncs its IO thread against AIL_close_stream). Audio.cpp all four
   callbacks restructured.
2. **Ring/premix REVERTED TO STOCK** (DIG_DS_FRAGMENT_CNT override deleted → default
   256ms ring; setLargePreMixBuffer back to 64ms): the 1500ms load premix smeared every
   audible start/stop/fade by up to 1.5s across exactly the transition window (the
   "big gap"); its stall-riding justification is gone (CONSULT-59/60 fixed the stalls).
3. Kept: handle-base-1, shim default-off, audio-diag probe, 50ms pump slices,
   fire-and-forget stopMusic (the SCENE_CHANGED handler's own 1s Audio::alter pump loop
   in SwgCuiManager.cpp:350 still pumps the title fade — NOT a suspect).

**Evidence stash (13:02 session, audio-diag.log):** theme opens with VOLSTEP snap
0.004→0.750 (no fade-in); at +6.4s theme gets VOLSTEP 0.750→0.000 then 0.008→0.066
28ms later, coinciding with amb_desert_night→amb_desert swap ≈ load-end boundary —
one of Kenny's <1s cutout spurts. Not yet traced (GameMusicManager day/night or
_onFinishedLoading unSilence path?). Park unless it persists in the 1:32 build.

**Expected on Kenny's next test:** mid-stream crackle reduced/gone (fix 1), transition
gap gone (fix 2), title fades ~immediately at zone-in click (timing compressed vs the
freeze era — flag if he wants the 4-5s overlap restored deliberately). If the title
ever goes SILENT at login: handle fix has a hole → re-arm titleMusicStreamFix.
If crackle persists: it's the pre-existing class — next probes = ring-underrun
instrumentation or Sonnet's 22050-vs-44100 provider matrix (NOT volume).
