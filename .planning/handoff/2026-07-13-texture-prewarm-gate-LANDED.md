# 2026-07-13 — texture pre-warm = a loading-screen GATE fix (terrain warm-up drain) — landed + pushed

**READ FIRST after restart** (with the 07-12 handoff for the rest of the
arc). One evening session. `origin/master` = `e5404aaa7`
(GroundScene.cpp + TextureList.cpp).

**Bottom line:** the "texture pre-warm" backlog item resolved WITHOUT a new
preload system. The CONSULT-59 terrain warm-up stepper already pre-creates
the terrain texture set — but `GroundScene::isFinishedLoading` never waited
for it, and the arc's recent load-time fixes made the loading screen drop
fast enough that the warm-up TAIL (~390ms of terrain spec-map creates in
40ms alter slices) spilled past the screen into the world-entry frames.
Fix = hold the screen until the warm-up queue drains. x64 entry stalls
4 → 2 (both post-screen spill stalls eliminated); Kenny live: "Ran
beautifully" (x64) / "Buttery smooth" (Win32).

## 1. The conviction chain (repeatable workflow)

1. **Recon** (Explore agent): mapped every existing preload system —
   full writeup quoted in the session; keepers:
   - `AsynchronousLoader` pre-reads BYTES off-thread from a per-planet
     `misc/asynchronous_loader_data_*.iff` (ASYN) manifest; the GPU create
     half ALWAYS lands on the main thread (immediate context).
   - The `preloadAssets` chain stops at appearance templates —
     `MeshAppearanceTemplate` has no override; shaders+textures defer to
     first `createAppearance` (MeshAppearanceTemplate.cpp:171-193).
   - `PreloadedAssetManager` has first-class `[PreloadedAssets] texture`
     rows but is a fixed hand-curated startup list.
   - The terrain `PreloadManager` (ClientProceduralTerrainAppearanceTemplate)
     is the ONE place zone textures get truly pre-created
     (`terrainPreloadBudgetMs`, default 40ms/slice).
2. **Probe** (new, STANDING): `[ClientGraphics] logTextureCreates`
   (default OFF) → one `TEXCREATE %8.3fms <name>` line per named-texture
   cold create (TextureList::create around the `new Texture`). Tally +
   per-second bucketing against the report-log timestamps.
3. **Findings** (Tatooine zone-in, x64): 2,261 creates / ~2.05s total;
   worst single texture only ~19ms (stalls are BATCHES, not monsters);
   ~85% of create time already rode the budgeted loading-screen pumping
   stall-free; the stall class = 325 creates / ~390ms inside the entry
   window, dominated by terrain `*_spec.dds` — the warm-up tail.
4. **Root cause:** `terrainGenerationStabilized` is trivially TRUE before
   the warm-up completes (chunk requests only submit after
   `preloadShaders`), so the gate had a hole exactly where the tail spilled.

## 2. The fix (`e5404aaa7`)

- `GroundScene::isFinishedLoading` AND `GroundScene::updateCuiLoading`
  (display flag/string) now also require
  `ClientProceduralTerrainAppearance::isTerrainWarmupComplete()`.
- Convergence: while the screen holds, `updateLoading`'s else-branch (and
  terrain alter) keep pumping `updateTerrainWarmup` in budgeted slices.
- Space / no-procedural-terrain scenes: dynamic_cast null → passes
  trivially (same shape as the existing stabilized flag).
- No new config keys; no kill switch needed (revert = the commit).
  The screen simply holds a beat longer; the work total is unchanged.

## 3. Verification

- **x64** (telemetry smoke, sampler + probe): entry stalls 553/139/210/107
  → **562/137** — the post-screen spill stalls GONE; the warm-up's 1,000+
  creates (~716ms) ran entirely under the held screen with ZERO ≥100ms
  stalls. Kenny live: "Ran beautifully".
- **Win32**: boot gate clean, in-world, "Buttery smooth". Its remaining
  stalls are PRE-EXISTING gl05 classes, freshly attributed: 815ms =
  `Direct3d9_DynamicVertexBufferData::lock` during skeletal first-draw +
  591ms = the gl05 ctor-frame composite (cold opens, zlib, CommandTable,
  D3D9 texture uploads). Evidence for the gl05 sibling ticket.
- Probe cfg key added for the runs and REMOVED after (cfg parity intact;
  the probe code stays in-tree, default off).

## 4. WHERE TO RESUME — arc backlog

1. **GroundScene-ctor mega frame (~560ms both platforms)** — one-time work
   that runs BEFORE the loading screen exists (HUD creation/UIPage::Link,
   GameMusicManager::install, first GPU allocs, cold opens). A harder
   reframe: would need the loading screen up before scene construction, or
   moving HUD/music install into the budgeted loading phase.
2. **gl05 bytecode-cache sibling** — now with two convicted classes: shader
   compiles AND the D3D9 dynamic-VB lock skeletal first-draw stall.
3. preloadSomeAssets single-item overshoot (AsynchronousLoader routing) —
   still consciously deferred.
4. Driver-threading soak call (Kenny) → flip ConfigDirect3d11 default.
5. Probe strip pass after soaks (PortalCullProbe chatty; TEXCREATE +
   sampler are standing tools, NOT strip candidates).
6. Carried: Utinni v16 consumer rebind (Kenny's side), real-door trigger,
   ilm-extract audit, Debug-config 5-target refresh, CONSULT .out files
   parked in .planning/research/.
