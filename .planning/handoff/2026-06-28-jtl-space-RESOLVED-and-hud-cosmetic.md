# JTL Space — RESOLVED + HUD cosmetic frontier (2026-06-28 CONTEXT-CLEAR HANDOFF)

**Why this file:** the JTL-space arc is fixed; resetting context. This is authoritative. Supersedes
`2026-06-27-gl11-space-crash-and-empty-world-RESET.md` and the REBOOT-CHECKPOINT. Resume the remaining
**HUD cosmetic** here. Companion memories: `project_jtl_space_world_empty_render` (the gameFeatures root
cause), `project_nvwgf2um_null_deref_cui_text_d3d11_map` (driver-race crash + fix),
`feedback_cursor_dissent_high_hit_rate`.

---

## TL;DR — space works now. Three root causes found + fixed this session.

1. **Empty/white space → magenta DEFAULT-SHADER planets — THE ROOT CAUSE: `gameFeatures` SKU-load bug.**
   `[Station] gameFeatures=15` omitted the **SpaceExpansionRetail (16)** bit. `ClientMain.cpp:264-274`
   ANDs `Game::getGameFeatureBits()` against `ClientGameFeature::{Base,SpaceExpansionRetail,...}` to
   build `skuBits`, which `TreeFile::install` (TreeFile.cpp:106-148) uses to load `searchTOC_NN_P`. With
   bit 16 unset, **sku1 (JTL) searchTOC never loaded** → all `data_sku1_*` space shaders/assets
   (planet/celestial/particle shaders, real nebula skyboxes) were unreachable → `could not open shader
   template` → magenta. **FIX: `[Station] gameFeatures=33297`** = Base 1 + SpaceExpansionRetail 16 +
   Episode3ExpansionRetail 512 + TrialsOfObiwanRetail 32768. Bit values: sharedGame
   `PlatformFeatureBits.h:19-34`. **Live-confirmed: space renders (planets/celestials/nebula/particles).**

2. **gl11 zone-in CRASH (the long blocker) — NV driver worker-thread RACE.** `nvwgf2um`/`nvwgf2umx`
   `SetDependencyInfo` null-deref on a driver worker thread, fed by our rapid `Map(WRITE_DISCARD)` churn.
   NOT exhaustion (x64 crashes identically), NOT asset/config (debug layer ran with all suspect TREs, 0
   validation errors), NOT app Map-misuse. **PROOF:** the D3D11 debug layer (which serializes driver
   work) makes it vanish on both platforms. **FIX: create the device with
   `D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS`** (Direct3d11_Device.cpp createDeviceFlags).
   Live-confirmed both Win32 + x64.

3. **Heap corruption ~14s into space (c0000374) — index-buffer USE-AFTER-FREE.** Found via full
   page-heap (`gflags /p /enable SwgClient_r.exe /full`): an AV inside `IASetIndexBuffer` from
   `Direct3d11_StateCache::drawTriangleFan`'s defensive rebind. `ms_currentIB` was a **raw
   `ID3D11Buffer*`** cached from a per-mesh static IB (`data->getIndexBuffer()`); when the owning mesh
   was evicted during space object-churn the cache dangled. **FIX: `ms_currentIB` is now a
   `ComPtr<ID3D11Buffer>`** (holds a ref) — StateCache.cpp. Live-confirmed: survives space under
   page-heap, no crash.

---

## CLEAN PR STATE (what to commit)

**Renderer code = exactly 2 files (verified `git diff --stat`):**
- `Direct3d11_Device.cpp` — `+= D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS` (crash fix #2).
- `Direct3d11_StateCache.cpp` — `ms_currentIB` raw ptr → `ComPtr<ID3D11Buffer>` + `.Get()`/`.Reset()` at
  use sites (UAF fix #3).

**Config fix (gitignored `stage/`+`stage-x64/client.cfg`, NOT in git):** `[Station] gameFeatures=33297`
(fix #1). This is the most important fix but lives in the gitignored cfg. **Decide for the PR whether to
also correct the tracked cfg template** (cfg header claims CMake `client.cfg.in`, but build is MSBuild —
verify if a tracked template exists/matters; otherwise document the value in the PR description).

**Reverted this session (diagnostics / non-fixes — `git checkout`ed, NOT in the PR):**
- `Direct3d11_LightManager.cpp` lighting-cbuffer cache-skip (CONSULT-51) — a perf idea that *relocated*
  the crash (proved it wasn't Map volume); not the fix.
- `Direct3d11_RenderTarget.cpp` RT/SRV PS-SRV-NULL before OMSetRenderTargets (Cursor hypothesis) — not
  the fix (the race was; the SRV-NULL didn't stop it).
- `Direct3d11_Device.cpp` Release debug-layer enable (restored `#ifdef _DEBUG`) — was a diagnostic.
- `ConfigDirect3d11.{h,cpp}` + `Direct3d11_StaticShaderData.cpp` `disableAlphaTest` (prior session) —
  moot once gameFeatures fixed the empty world.
- cfg: removed the `disableAlphaTest` + `enableDebugLayer` keys.

Both platforms rebuilt clean (0 unresolved) after the revert; `stage/` + `stage-x64/` gl11_r.dll current.

---

## BAND-AIDS still in `stage/` (gitignored) — now mostly redundant; cleanup = final polish

These were workarounds for the unloaded sku1; **kept so the client keeps working for the next session**,
but `gameFeatures=33297` now supplies the real assets:
- `stage/override/texture/*` (71 real base textures over ILM's 8x8 stubs) — **redundant** (base sku1
  serves them). Safe to delete *after* re-verifying space still renders.
- `stage/ilm_extract/` (2103 files, 2.4 GB; `searchPath_00_5`) — **keep only the ILM-EXCLUSIVE Scyk
  ship-chassis data** (`datatables/space/ship_chassis_scyk*` + the corrected `appearance/planet_tatooine.pln`
  Kessel override); the rest (textures/shaders) is now redundant. (Slim it, or repack a small clean TRE.)
  ⚠️ ILM's `planet_tatooine.pln` is the Kessel-mislabeled one; the corrected override must still win.
- Don't remove these blind — re-test space after, since we can't here.

---

## REMAINING OPEN ITEM (next session = crew consult): HUD cosmetic "cyan square"

**Symptom:** in space, the **minimap (radar) and the reticle gauges are framed by a semi-transparent
cyan rectangle/square** that shouldn't be there (Kenny: "looks like an alpha problem"). Captures:
`stage/Capture47.rdc` (1st-person cockpit) + `stage/Capture48.rdc` (3rd person).

**Traced (Capture47, draw EID 8080, RenderDoc PS-debug):** the frame quad outputs
`o0 = texture(gray ~0.63, ALPHA ~0.30) × vertexColor(0.42,0.80,0.78 = cyan, a=1)` → cyan @ ~0.3 alpha,
alpha-blended → the translucent cyan square. The **cyan is correct** (SWG space-HUD vertex-color theme);
the **alpha (~0.3 from the texture) is the bug** — the panel background shows where gl05 has it
invisible. Same shader frames the reticle gauges → **one shader/element type, one root** (matches
Kenny's "probably one shader being a few wrong"). Draws involved: EID ~8028/8054/8080 (CUI quads via
`CuiLayerRenderer` → `Graphics::drawQuadList` → `Direct3d11_StateCache::drawQuadList`).

**Next-session plan (crew consult):** the fix is a gl11 UI alpha/blend-parity tweak for the CUI panel
quads — likely the panel-background quad should be alpha-tested-away / the texture-alpha handling differs
from D3D9. Trace `Direct3d11_StateCache::drawQuadList` + the CUI panel's blend/alpha-test setup vs the
D3D9 reference path (RenderDoc can't capture D3D9 — reason from source). Cosmetic only; all heavy bugs
are fixed. NOTE: re-stage Capture47/48 if cleared.

---

## Key diagnostics that cracked it (reuse these)
- **Full page-heap** (`gflags /p /enable SwgClient_r.exe /full`; x64 for headroom) → caught the IB UAF
  at the exact write. Disable with `/p /disable` after.
- **`cdb -g -G -logo spacelog.txt`** (launch via `Start-Process -WorkingDirectory stage` so the
  debuggee cwd finds client.cfg) → captured the engine's own `could not open shader template …` warnings
  (they go to OutputDebugString, not the disk logs) — named the whole missing JTL shader set.
- **Crew consult split** (CONSULT-51): Opus/Sonnet/Codex → exhaustion (wrong); Cursor → dependency-graph
  corruption (right family — a race in SetDependencyInfo). [[feedback_cursor_dissent_high_hit_rate]].
  Consult files: `.planning/research/CONSULT-51-*`, `CONSULT-52-*`.
- TRE/SKU analysis: `swg_pipeline.tre_reader` at `D:/Code/swg-blender-plugin` (read_tre_entries gives
  name+length+offset; read_search_toc_header lists a .toc's tre_files). Scratchpad scripts show patterns.
