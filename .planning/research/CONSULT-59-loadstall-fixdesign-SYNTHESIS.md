# CONSULT-59 SYNTHESIS — load-stall fix design (4-consultant round, 2026-07-04)

Evidence pack: [CONSULT-59-loadstall-fixdesign-EVIDENCE.md](CONSULT-59-loadstall-fixdesign-EVIDENCE.md)
(symbolized stall-watchdog dumps from Kenny's 4 organic sessions). Angles: Codex =
terrain-preload call graph; Cursor = AsynchronousLoader drain mechanics; Opus = lever A
design correctness; Sonnet = localization + lateral sweep + charselect.

## The productive split (and how it resolved)

Opus's recommended design (D1 incremental PreloadManager pump + 2 guards) assumed terrain
chunk creation demand-fetches shaders on miss. **Codex refuted with receipts**: the
appearance's own `ShaderCache` ctor is a SECOND eager planet-wide loader
(`preloadShaders()`, `_ShaderCache.cpp:179` — original dev `@todo avoid loading textures
for entire planet up front`), and `getTextures()` reads slots with **no miss-fetch**
(`_ShaderCache.cpp:288-311`). Deferring only the template preload would just move the
4.5s stall into the ShaderCache ctor. My own read added the threading fact both missed:
`createClientChunk` (→ `createTileShader` → `getTextures`) **runs on the ClientTerrain
worker thread** (`_LevelOfDetail.cpp:823`), so the unlocked `cache[][]` slots are only
safe under a populate-fully-before-first-use ordering — incremental fill concurrent with
generation would be a new CONSULT-56-class race.

## Landed design (all four levers, implemented 2026-07-04)

**A — budgeted terrain warm-up (the 3-4.5s mega-stall):**
- `PreloadManager` ctor now captures name lists only; `step(budgetMs)` fetches under
  `[ClientTerrain] terrainPreloadBudgetMs` (default 40; <=0 restores old synchronous
  behavior). Dtor unchanged → refcount-balanced on mid-preload teardown (Opus bullet 1).
- Opus Guard A: `garbageCollect()` no longer deletes a mid-preload manager (would drop
  partial pins + silently restart).
- ShaderCache ctor no longer calls `preloadShaders()`; the appearance calls it ONCE when
  the template preload completes (pure cache hits), **before any chunk request exists**
  (preserves the unlocked-slots ordering invariant).
- Gates: `alter()` pumps warm-up and skips `calculateLod` until primed; `createChunk`
  early-returns while un-primed (identical to the supported "request pending" state —
  all sync force-create paths funnel through it, `ProceduralTerrainAppearance.cpp:629/760/1048`);
  Opus Guard B landed inside `terrainGenerationStabilized()` (returns false until primed
  — otherwise the empty request map reads as "stabilized" and drops the loading screen
  early). GroundScene's loading block also pumps `updateTerrainWarmup()` (belt+braces)
  — no other GroundScene changes needed.

**B — AsynchronousLoader time budget (the 100-800ms post-entry burst):** per Cursor's
trace: new `[SharedFile] asynchronousLoaderCallbackTimeBudgetMs` (code default 0 =
unlimited; armed at 6 in stage/client.cfg), checked AFTER each fully-retired request
(early-break verified state-safe: FIFO deque, per-request TreeFile cache clear,
`alreadyCached` reset on retirement only, `ms_numberOfCachedBytes` back-pressure intact);
the postponed-requests re-signal block still always runs (skipping it = worker deadlock
under the 8MiB threshold); `remove()` sets a drain-all bypass so shutdown can't spin
forever (pre-existing hazard with the count budget, now fixed for both).

**C — localized-name string tables (the 110-620ms mid-play stalls):** per Sonnet:
`ClientObject::endBaselines` queues the object's name table
(`queueLocalizedNameTablePrefetch`, dedup set); GroundScene's loading pump calls
`preloadSomeLocalizedNameTables(20ms)` → public `LocalizationManager::fetchStringTable()`
(memoized; the manager holds its own ref until an explicit purge — Sonnet's suggested
`preload()` API turned out to be private, and the public fetch is equivalent here) so
tables are warm before the screen drops. Fixes the
whole class (combat spam CuiCombatManager:2134/1648, examine tooltips
ObjectAttributeManager:852, radial menus CuiRadialMenuManager:1908/2184, chat bubbles
CuiSpatialChatManager:233/341 — Sonnet's sweep), not just the status window.
Residual (accepted): a species first seen mid-play (not during any loading screen) still
demand-loads synchronously — rarer, and Sonnet's warning honored: we did NOT move the
sync load to spawn time; mid-play enqueues just wait for the next loading screen.
NOT done (rejected as too invasive): changing `getLocalizedName()`'s blocking contract.

**D — stale Plan 17-04.X PSRC dump block deleted** from
`Direct3d11_PixelShaderProgramData.cpp` (fopen/fwrite/fflush per unique HLSL PS inside
the ctor; was marked REMOVE).

## Deferred (filed, not done)

- **TreeFile loose-search-path stat-storm** (Opus dissent + the CreateFileA/SearchPathA
  stack sample): negative-lookup cache or TRE-first ordering would cut file-open cost for
  every load path. Separate ticket.
- **Charselect avatar 371ms first-draw** (class 3): Sonnet traced it
  (SwgCuiAvatarSelection auto-select already pre-warms as early as the architecture
  allows; residual = first-touch GPU state creation). One soft specimen — deferred unless
  it reproduces.
- Cursor's note: a single heavy callback can still exceed any budget (can't fix without
  splitting template `load()` — out of scope).

## Verification plan

Build Release/Win32 (Direct3d11 + SwgClient), 0 unresolved externals; boot to charselect
(boot gate); zone into Tatooine with watchdog armed (stallWatchdogMs=100 still on):
EXPECT no multi-second stall dump at world entry, loading screen holds until terrain
primed (no black/hole terrain), post-entry burst collapsed, music plays through loading.
Regression signatures: loading screen never dismisses = warm-up never completes (check
terrainPreloadBudgetMs>0 and that the GroundScene pump runs); black terrain tiles =
chunks built with unpopulated ShaderCache (gate regression); zone-in hang = lock class.
