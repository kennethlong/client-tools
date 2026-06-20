# Debug session: gl11 (D3D11) x64 Release — crash zoning into Tatooine

**Opened:** 2026-06-19 · **Status:** OPEN — investigating · **Renderer:** gl11_r.dll (D3D11) x64 Release
**Exe:** `stage-x64\SwgClient_r.exe` reads `client.cfg` (rasterMajor=11)

## What happened
First-ever run of the freshly-built **x64 Release `gl11_r.dll`** (Direct3d11, built 2026-06-19; gl11 was
Debug|x64 only before — see [[project_phase34_gl11_x64_complete]]). Client booted clean on D3D11 (window
'Star Wars Galaxies', loaded gl11_r.dll + d3d11.dll), reached character-select, then **crashed while zoning
into Tatooine**.

## Crash dump (stage-x64\SwgClient_r.exe-unknown.0-20260619140908.txt)
- Exception **c0000005** (access violation)
- Player: **3471.33  4.08  -4858.23** on `terrain/tatooine.trn`
- `BytesAllocated: ~190 MB` (NOT an OOM — well under any ceiling)
- UpTime 1808; reached char-select (`characterlist_swg.txt` written)
- In-flight async load at crash time:
  - **`SkeletalMeshGeneratorTemplateAsync: appearance/mesh/armor_marauder_s01_belt_m_l3.mgn`**
  - ShaderTemplate_Iff: `shader/stco_tato_dtlbase_a1_adb13.sht`
  - MeshAppearanceTemplate: `appearance/mesh/thm_tato_imprv_wall_sml_s01_l0.msh`
  - (these are last-seen breadcrumbs per category, not necessarily the faulting object)

## Leading hypothesis
**Known pre-existing intermittent Tatooine crash — async `armor_marauder` belt `.mgn` load race; retry
usually succeeds. NOT D3D11-related.** Exact signature match: same `armor_marauder_s01_belt_m_l3.mgn`,
Tatooine, async skeletal-mesh path. See memory `project_intermittent_tatooine_crash_087a`.
→ If true, this is renderer-independent and not introduced by the gl11_r build.

## Verification plan
1. **(doing now) Relaunch under cdb, retry zone-in.** cdb catches the c0000005 second-chance and dumps the
   real faulting module + stack (`.ecxr; kb; lm a @rip`) — definitively answers gl11-specific vs pre-existing
   race. Script: `tools/setup/zonein-crash-catch.cdb`; log: `stage-x64\zonein-crash.log`; dump: `zonein-crash.dmp`.
   - If cdb shows the fault inside **gl11_r/d3d11** → gl11 regression, investigate the render path.
   - If fault is in the **async .mgn / skeletal** path (clientGame/clientGraphics non-renderer) → confirms the race.
   - If retry **zones in fine** → confirms intermittent race (retry-succeeds), close as pre-existing.
2. **Cross-check on gl05** (if needed): zone the same Tatooine spot under gl05 x64 Release. gl05 crash too =
   renderer-independent.

## Run log
- **Run 1 (2026-06-19, under cdb, `zonein-crash.log`): CLEAN — zoned into Tatooine/Mos Eisley, NO crash.**
  In-world stable (675 MB), gl11/D3D11 rendering correctly (player, banthas, buildings, mini-map). cdb log
  shows **zero first-chance AVs** — so cdb overhead was NOT masking anything this run; the race simply
  didn't fire. → **Crash is intermittent, NOT a deterministic gl11 regression.** gl11 x64 CAN zone into
  Tatooine. Strengthens the pre-existing-`.mgn`-race hypothesis (the 1st plain run crashed; this one didn't).

## Capture strategy (intermittent → need the right harness)
- cdb harness (`tools/setup/zonein-crash-catch.cdb`) is armed: lets handled first-chance AVs through (`gh`),
  dumps full stack + `zonein-crash.dmp` only on the **fatal** AV, then quits. Viable (low overhead confirmed).
- **WER LocalDumps (preferred, no perturbation) needs elevation.** Run ONCE in an elevated shell to make
  every future plain-client crash auto-write a full minidump to `stage-x64\crashdumps\`:
  ```
  reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\SwgClient_r.exe" /v DumpFolder /t REG_EXPAND_SZ /d "D:\Code\swg-client-v2\stage-x64\crashdumps" /f
  reg add "HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\SwgClient_r.exe" /v DumpType   /t REG_DWORD     /d 2 /f
  ```
  Then run the client normally, reproduce the crash (fast for Kenny), analyze the dump with `cdb -z`.
- Analyze a captured dump: `cdb -z stage-x64\zonein-crash.dmp` (or the WER dump) → `.ecxr; kb; lm a @rip` →
  if fault is in gl11_r/d3d11 = gl11 bug; if in the async skeletal/.mgn path (clientGame/clientGraphics) =
  confirms the race.

## #4 RESOLUTION — DEFENSIVE HARDENING 2026-06-19 (load-only race; repro exhausted, fixed by reading)
User insight that closed the loop: it is a **load-only** race; SSD + warm disk cache minimizes on-load
churn, so it surfaces mainly **after a full build flushes the disk cache** (cold load = wider async
window). Our cold-forcing (.cso clear + EmptyStandbyList TRE evict) narrowed but never hit the exact
timing in ~120 headless iters, and instrumentation PROVED the async `.mgn` path runs clean here. Decision:
**harden all three converged hazards + keep would-have-fired logging** (robust client now; any future
near-miss self-logs). All in `clientSkeletalAnimation` (.cpp only, no header -> no plugin ABI cascade):
- **I4** `SkeletalMeshGeneratorTemplate::asynchronousLoadCallback()` — clear `m_asynchronousLoadInProgress`
  on BOTH outcomes (was success-only -> a failed cold-load wedged the template forever). Uninitialized
  generators left queued for a successful retry. Logs `[MGN-PROBE] I4-WEDGE-AVERTED`.
- **RANK-1** `removeAsynchronouslyLoadedMeshGenerator()` — NULL-safe guard (the `NOT_NULL` deref'd NULL in
  Release if the pending list was already freed). Logs `[MGN-PROBE] RANK1-REMOVE-NULL-LIST`.
- **I3** `SkeletalMeshGenerator` — release-safe early-return guards on all 7 deref sites
  (applySkeletonModifications, addShaderPrimitives, getOcclusionLayer, createAppearance,
  addCustomizationVariables, getReferencedSkeletonTemplateCount/Name; were DEBUG_FATAL-only -> AV in
  Release on an unloaded template). Logs `[MGN-PROBE] I3-DEREF-AVERTED`.
- Retained FETCH-decision probe (logs first 8 mesh fetches ASYNC/SYNC) for future diagnosis.
- NOT yet committed; rebuilt clientSkeletalAnimation + relinked SwgClient_r x64 (clean link).
- If the crash recurs in normal play, grep the client log / cdb log for `[MGN-PROBE]` — the AVERTED line
  names exactly which hazard fired (turns the next near-miss into a definitive mechanism ID).

## #4 FINAL — converged redesign + 2 cross-AI review rounds + clean-room (2026-06-19)
The initial 3-hazard hardening was peer-reviewed (CONSULT-38, 4 AIs) which found it crash-safe at the
guarded sites but flagged the I4 retry as risky (multi-gen-list erase + wrong guard predicate). A design
convergence round (CONSULT-39, 4 AIs on non-overlapping slices) produced the full design; then a fresh
clean-room review (CONSULT-40, 4 AIs, MINIMAL context, told to treat comments as unverified) validated it
and caught one real bug the anchored rounds missed. Final shape (all in clientSkeletalAnimation, .cpp+.h,
no shared-header ABI cascade):
- **Predicate**: explicit `SkeletalMeshGenerator::m_isCreated` (set last in create()) replaces isLoaded()
  for isReadyForUse() + all 7 deref guards. Correct incl. zero-blend templates (m_blendValues stays NULL
  legitimately) and the async fixup window (isLoaded()==true before per-instance create()).
- **Iteration safety**: asynchronousLoadCallback fixup DETACHES the list (swap+NULL member) before iterating
  + fetch()/release() BOOKEND each queued generator across its create() -> no erase-during-iteration UB and
  no stale-pointer UAF if a reentrant create()->handleCustomizationModification tears down an owner.
- **I4**: clear m_asynchronousLoadInProgress on BOTH outcomes (no wedge).
- **Recovery**: create() ends with signalModified() -> marks owning appearance dirty (via registered
  meshGeneratorModifiedCallback) -> next render rebuilds (render-driven, not alter()). Heals an already-
  spawned NPC after a transient load.
- **Bounded retry**: m_loadAttemptCount (3 attempts) then m_loadPermanentlyFailed; template-side re-issue
  (createMeshGenerator re-kick doesn't fire for an already-cached consumer); re-kick AND enqueue both gated
  on !m_loadPermanentlyFailed (the CONSULT-40 clean-room bug fix); count reset on success.
- **Permanent-fail**: isReadyForUse() returns true so the not-ready MRU-pin livelock stops; guards still
  render nothing. Does NOT force eviction (gated on non-empty SP); generator persists invisible until NPC
  despawn = correct outcome for a genuinely missing asset.
- **RANK-1** NULL-safe removeAsynchronouslyLoadedMeshGenerator; **getOcclusionLayer** returns -1 (template
  default) not 0; FETCH probe wrapped `#if PRODUCTION==0`; `[MGN-PROBE] ...-AVERTED` logs kept (fire only on
  the bug condition -> self-reporting in normal play).
- Clean-room changes vs first cut: gate the enqueue on !m_loadPermanentlyFailed (Sonnet C2 = real bug);
  REVERTED the getSkeleton() readiness gate (contested lodIndex-vs-displayLodIndex; signalModified() already
  covers the heal; Opus showed skeleton vector is built independently so no NULL risk); corrected the
  overstated "re-fetch heals" comments.
- ACCEPTED LIMITATIONS (documented, not fixed): retry covers Iff::open() failure not load()/parse failure
  (rarer corrupt-asset case); a transient that only heals after 3 strikes stays invisible until the template
  is fully released + re-fetched. Both acceptable.
- VALIDATION: clean Release|x64 link (0 unresolved), 5/5 cold zone-ins clean, async path exercised
  (.mgn->ASYNC), 0 guard/retry/fail lines in steady state (hardening inert in normal operation). NOT yet
  committed.

## ROOT CAUSE (#1 shutdown crash) — PROVEN 2026-06-19 (iterator invalidation, NOT gl11/render, NOT the .mgn race)
Dump `stage-x64\crashdumps\hang-shutdown-mediators.dmp` (1.39 GB) + all-thread snapshot `hang-snapshot.log`.

**The fatal = a re-entrant `m_mediators` mutation during iteration in `CuiWorkspace::updateMediatorEnabledStates`.**
- Faulting instr (from dump): `std::_Tree iterator::operator++` inlined at `updateMediatorEnabledStates+0xc2`
  (CuiWorkspace.cpp:222): `cmp byte ptr [rcx+19h],0` with **rcx≈0x10** → `++it` is walking a **freed
  red-black-tree node** (`[node+0x19]` = MSVC `_Tree_node` _Isnil byte). Classic freed-node deref.
- Mechanism: the loop at **CuiWorkspace.cpp:222** (and the symmetric one at :204) iterates `m_mediators`
  (`std::set<CuiMediator*>`) and calls `mediator->deactivate()` (:229). One `deactivate()` re-enters
  `CuiWorkspace::removeMediator` → **`m_mediators->erase(&mediator)` (:339)** mid-loop → invalidates `it`
  → next `++it` derefs a freed node → **c0000005**.
- **The authors KNEW this is illegal**: `removeMediator` has `DEBUG_FATAL(m_iteratingEnabledStates, "Can't
  removeMediator during m_iteratingEnabledStates")` (:313) and `addMediator` the same (:260). But
  **`DEBUG_FATAL` is a no-op in the Release client** → in `SwgClient_r.exe` the illegal erase silently
  corrupts the set instead of asserting. Same "Debug guard masks Release UB" + "x64 MSVC STL exposes
  latent STLport UB" pattern as the combat-kill erase-iterator bug and the TextManager truncation.
- **Renderer-independent.** gl11 x64 is fine (it rendered Mos Eisley cleanly). The "crash starting to
  zone" (plain run) = the FIRST unhandled AV here; the "hang" (under cdb) = my catch script's `sxe -c "gh"`
  forced every AV to resume-and-refault → 2,503,927-AV storm → frozen UI + crackling audio. Same root, two faces.

## FIX APPLIED 2026-06-19 (CuiWorkspace.cpp:194 updateMediatorEnabledStates)
Snapshot `m_mediators` into a local `std::vector`, **`fetch()` each entry up-front + `release()` after**
(removeMediator calls `release()`, which can delete the object — refs keep snapshot pointers alive for the
whole pass), and **skip any mediator no longer in the live set** (`m_mediators->find(...)==end()`) so we
never act on a removed one. Both the activate (~:214) and deactivate (~:235) loops now iterate the snapshot.
Guards/flags left intact. Rebuilding SwgClient Release|x64. Verify: zone in repeatedly on gl11 + gl05 x64.

## Fix direction (original analysis)
Make `updateMediatorEnabledStates` iterate over a **snapshot** so re-entrant erase/insert during
`deactivate()`/`activate()` can't invalidate the loop. Minimal/safe, matches codebase:
```cpp
// copy first, then iterate the copy (both the activate loop ~:204 and deactivate loop ~:222)
std::vector<CuiMediator *> snapshot(m_mediators->begin(), m_mediators->end());
for (CuiMediator * mediator : snapshot) { ... }   // erase/insert on m_mediators is now safe
```
Keep the DEBUG_FATAL guards (they document intent). Verify: rebuild SwgClient_r, zone in repeatedly on
gl11 x64 + gl05 x64. Open the dump to confirm which mediator's `deactivate()` re-enters removeMediator
(`cdb -z hang-shutdown-mediators.dmp` → walk frames / dump `m_mediators`) before finalizing.

## STATUS 2026-06-19 (latest)
- #1 shutdown-teardown crash: **FIXED, re-confirmed** — snapshot+fetch/release in
  `updateMediatorEnabledStates`, **find()-guard removed** (the guard re-introduced a live-set traversal
  that crashed in `_Tree::find` on a freed node during teardown — caught by the loop on iter3). 20/20
  loop iterations zoned into Tatooine + `/quit` shutdown CLEAN. `SwgClient_r.exe` rebuilt (clean link).
- #3 headless auto-connect: **FIXED** (m_avatarListReceived gate + repopulate) AND the real blocker was
  the **unquoted cfg space** (`processKeys` truncates `avatarName=Little Bigman` -> `Little`); fixed by
  quoting in loop-autoconnect.cfg. `skipSplash=true` added (no static startup pages). `/quit` = clean exit.
- #4 armor `.mgn` zone-in: NOT reproduced in 20 zone-ins — rare; needs a longer hunt (loop running larger batch).
- #2 texture refcount FATAL: still OPEN (separate).
- NOT yet committed — fixes (#1 CuiWorkspace.cpp, #3 SwgCuiAvatarSelection.cpp+.h) are in the working tree.

## BUG SCOREBOARD (this debug session, 2026-06-19) — x64 exposing latent UB across several paths
1. **Mediator iterator-invalidation** (shutdown teardown / `updateMediatorEnabledStates`) — ✅ FIXED &
   confirmed (snapshot+fetch/release; user "loaded and quit cleanly"). Proven via the 2.5M-AV-storm dump.
2. **`Texture reference count has gone negative`** FATAL (`Texture::release` Texture.cpp:329) during
   CHARACTER-CREATION hair-model swaps (`hum_m_hair_s40`...). A texture double-release. OPEN, deeper
   (find the over-releaser). Blocks char creation. Separate workstream.
3. **Headless auto-connect races the character-enumerate reply** → avatar screen activates with an empty
   list → `refreshList` line ~429 pressed Create → landed on Character Creation instead of selecting the
   existing avatar. ✅ FIXED 2026-06-19 (SwgCuiAvatarSelection): added `m_avatarListReceived` (set in
   `onAvatarListChanged`), gated the empty→auto-create on it **only in auto-connect mode** (manual login
   unchanged), and added `refreshList(true)` on list-arrival so a late enumerate repopulates + auto-selects.
   Also a real product win: makes launcher auto-connect robust against the enumerate race.
4. **Armor `.mgn` zone-in crash** ("crashed starting to zone", `armor_marauder_s01_belt_m_l3.mgn`, Tatooine)
   — **THE PAINFUL ONE** (user: happens during normal load-in). User confirms it also occurs on **32-bit
   Release AND Debug, just rarely** → pre-existing intermittent race, NOT x64/gl11-introduced, DISTINCT from
   #1. c0000005, not yet stack-proven. **Goal of the loop = catch this with a real stack.**
   - **2026-06-19 COLD-LOOP RUN (50 iters): SURVIVED 50/50, #4 NOT reproduced.** Maximal cold forcing
     every iter (cleared ~187 `.cso` + EmptyStandbyList `standbylist` TRE OS-cache evict — both levers
     confirmed active in `crashdumps/zonein-loop-run.log`). Validated #1 fix (50/50 clean `/quit`, incl.
     2 WM_CLOSE fallbacks).
   - **CONFOUND identified:** the loop auto-selects ONE fixed avatar (Little Bigman) → one saved spot →
     **sits still 30s**. The breadcrumb asset (marauder belt) + the whole sibling family (`worrt_hue_l3`,
     `bth_m_hair`) are **NPC/creature skeletal meshes** that only stream during world POPULATION/MOVEMENT.
     A stationary character likely never requests them → the #4 path is never exercised. "Clean 50/50"
     ≈ "didn't load the faulting asset," NOT "bug fixed." Next: add post-zone-in MOVEMENT to force
     NPC/creature `.mgn` streaming, or pick a spot/character near marauder spawns.
   - **2026-06-19 COLD + CHURN-WALK RUN (30 iters): SURVIVED 30/30, #4 NOT reproduced.** Added a
     post-zone-in movement circuit (held W + alternating turns) to make marauders cross LOD boundaries
     while their belt `.mgn` loads pend. Walk visually confirmed (user watched avatar move). Running
     total: **~80 cold iters (50 stationary + 30 walking), ZERO catches** → #4 is rarer than ~1/80 even
     under maximal cold forcing + churn, OR the harness scenario doesn't hit the real trigger.
     - SUSPECT GAPS in the harness: (a) the fixed W+turn circuit likely jams against city buildings →
       avatar rotates more than it traverses → NPCs don't actually enter/leave load range; (b) the walk
       fires at +18s, AFTER the initial zone-in population burst (the original crashes were "starting to
       zone") → may miss the burst window AND only loads HIGHER LODs (getting closer), not the L3 FAR-LOD
       belt in the breadcrumb; (c) historical crashes depend on specific server population/timing not
       reproduced. NEXT (recommended): stop blind-looping; INSTRUMENT the suspect sites to LOG (Release-
       safe) when the precursor fires, to learn if the dangerous path is even exercised.
   - **2026-06-19 INSTRUMENTED RUN (probes in clientSkeletalAnimation, Release x64): DEFINITIVE.**
     Added Release-safe `[MGN-PROBE]` logging (REPORT_LOG -> OutputDebugString, captured by the loop's
     cdb per-iter logs) at the converged suspect sites + a FETCH-decision probe. Findings:
     - **Async `.mgn` path IS active** (FETCH probe: `astromech_r2_l0.mgn`/`desert_worm.mgn`/`flag.mgn`
       -> `isEnabled=1 bind=1 supportsAsync=1 -> ASYNC`; `.lmg` bodies -> SYNC). So we ARE exercising the
       crashing code path. (Settles consultant disagreement: `.mgn` registers supportsAsync=TRUE at
       SkeletalMeshGeneratorTemplate.cpp:1878; Sonnet's "false" was wrong.)
     - **40 cold early-load iters: ZERO I4-WEDGE, ZERO I3-DEREF, ZERO RANK1-DESTROY-PENDING, ZERO crash.**
       None of the three hypothesized races occur in the headless single-player Tatooine load. The async
       loads succeed (no openSuccess=false), the readiness gate holds (no unloaded deref), nothing is torn
       down mid-load. The static hazards are REAL but LATENT — not firing in any reproducible scenario.
     - Config note: `runtimeDisableAsynchronousLoader=true` in client.cfg is INERT in Release (`#ifdef
       _DEBUG` in AsynchronousLoader::isEnabled). Manifest `misc/asynchronous_loader_data_2.iff` exists
       in TRE patch_11_xx/12_xx; both renderers report shader-major 2 -> install succeeds.
   - **VERDICT on headless repro: EXHAUSTED.** ~120 iters across stationary/walking/cold/instrumented,
     0 catches, and now PROOF the async path runs clean here. The historical crashes happen in NORMAL
     interactive play (longer sessions, real LOD eviction over time, possibly Opus's I5 shader-primitive
     eviction which the breadcrumb mis-attributes to the .mgn) which the short-dwell headless loop does not
     reproduce. RECOMMENDED next: keep the INSTRUMENTED Release build in normal play -> the probes + dump
     will pinpoint the mechanism on the next NATURAL crash. (Probes are cheap + silent unless a precursor
     fires; safe to leave in.) Alternatively apply the defensive I4 fix (provable-by-reading) and move on.
   - Static analysis (CONSULT-37) converged on a PROVABLE-BY-READING defect regardless of repro:
     I4 asymmetric cleanup early-return in `SkeletalMeshGeneratorTemplate::asynchronousLoadCallback()`
     (`:3277-3285`) skips clearing `m_asynchronousLoadInProgress` + resolving `m_uninitializedMeshGenerators`
     on `!openSuccess`; plus I3 (consumer deref guards are `DEBUG_FATAL`-only) + non-atomic refcount.

## HEADLESS REPRO LOOP (to catch #4)
- `tools/setup/zonein-loop.ps1` + `zonein-loop-catch.cdb`; auto-connect cfg `stage-x64/loop-autoconnect.cfg`
  (swg/swg, cluster `swg`, avatar `Little Bigman`, skipIntro) loaded via `-- @loop-autoconnect.cfg`
  (overrides win; client.cfg untouched). Validated: auto-login → cluster works (reached GroundScene).
- cdb catch = `sxe -c "<dump>;q" av` (NO `gh` — clean runs have zero benign AVs, so first AV = the bug).
- Each iter: auto-zone-in (ZoneSeconds) → WM_CLOSE (exercises shutdown teardown) → expect clean exit;
  AV (dump appears) or shutdown-hang ⇒ caught, loop stops, artifacts in `stage-x64/crashdumps/`.
- BLOCKED until the #3 rebuild lands (then auto-select picks Little Bigman → real Tatooine zone-in → #4 chance).
- cfg discovery: full auto-connect chain is built-in — `[ClientGame]` loginClientID/loginClientPassword/
  autoConnectToLoginServer/autoConnectToCentralServer/centralServerName/autoConnectToGameServer/avatarName/
  skipIntro. Avatar match is `_stricmp(avatarName, fullName)` (SwgCuiAvatarSelection:612) — case-insensitive
  FULL name; the space in "Little Bigman" is NOT a problem (the earlier creation-screen was the #3 race).

## Notes
- Player spawn 3471,4,-4858 is the saved character location → relaunch + select character re-attempts
  the same zone-in (good for repro).
- This is the x64 Release gl11 path that had NEVER been exercised before today.
- **Capture lesson:** do NOT use `sxe -c "gh" av` for an AV that recurs — it converts a clean crash into an
  AV storm/hang. For the real fault use plain `sxe av` (break on FIRST chance) or WER LocalDumps.
