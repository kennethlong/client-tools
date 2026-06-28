# gl11 JTL Space — Crash + Empty-World — CONTEXT RESET HANDOFF (2026-06-27, eve)

**Why this file:** long investigation session; resetting context to re-approach fresh. This is the
authoritative state. Supersedes the framing in `2026-06-27-jtl-space-render-arc-REBOOT-CHECKPOINT.md`
(read that for the morning's data-completeness/camera work; this file carries the afternoon/evening
RenderDoc + crash findings). Companion memories: `project_jtl_space_world_empty_render`,
`project_nvwgf2um_null_deref_cui_text_d3d11_map`, `project_jtl_space_ship_data_ilm_exclusive`.

---

## TWO SEPARATE PROBLEMS (do not conflate)

### PROBLEM A — gl11 jump-to-lightspeed cutscene CRASHES (the current hard blocker)
**Signature:** `nvwgf2um!NVAPI_DirectMethods+0x9da4c` → `mov eax,[eax]` with **eax=0** (null-deref),
on an **NV driver worker thread** (`SetDependencyInfo`/`NVDEV_Thunk`), **0 engine frames**. The engine
SEH minidump fires (`stage/*.mdmp`) AND Windows WER dumps (`~/AppData/Local/CrashDumps/`).

**>>> DECISIVE ISOLATION (2026-06-27 eve): gl05 (D3D9) zones into space FINE, repeatedly; gl11 (D3D11)
crashes at the jump cutscene every time. So it is gl11/D3D11-SPECIFIC — NOT a generic/chronic NV driver
bug, NOT the scene data. It's OUR D3D11 renderer path feeding the driver something bad at the cutscene
render.** (gl11 renders the GROUND fine — cantina/Tatooine — so it's specifically the SPACE jump-cutscene
gl11 path: `Direct3d11_LightManager::setLights` / `Direct3d11_ConstantBuffer::updateVS` under
`DebugPortalCamera`.) This vindicates Kenny's "we're doing something" instinct. The "NOT ours" framing
in the older nvwgf2um memory is now OVERTURNED for this case — the driver null-derefs, but on something
gl11-specific that gl05 never does.

**What it is NOT (all ruled out this session):**
- NOT our recent changes — the **pure warm baseline** (planet override OFF, `disableAlphaTest=false`,
  i.e. the exact Capture42 config) **still crashed** (dump `...20260627232905.mdmp`).
- NOT the recompile burst — warm cache (no recompiles) crashes too.
- NOT fixed by the new driver: loaded driver IS **`32.0.16.1062`** (confirmed `lmvm nvwgf2um` in dump)
  — the morning's driver update did NOT fix it.
- NOT fixed by NVIDIA **Threaded Optimization = Off** (Kenny confirms it's already off).
- NOT a RenderDoc-only artifact, and NOT reliably avoided by RenderDoc — **Capture42/43/44 succeeded
  under RenderDoc earlier (luck), but it later crashed UNDER RenderDoc too.**
- NOT off-thread D3D resource creation — the `AsynchronousLoader` registers
  `asynchronousLoaderFetchNoCreate` for dds/sht/vsh/psh (`TextureList.cpp:151` etc.); the loader thread
  only warms the **file cache**, GPU resources are created on the **main thread**.
- NOT the Kessel asset / a wrong server TRE — a bad data file → wrong visuals or load FATAL, not an
  `nvwgf2um` null-deref. (Driver/GPU fault, not data-load.)

**WHERE it crashes (KEY narrowing, late in session):** Kenny observed the crash fires at the
**jump-to-lightspeed cutscene** (then he reloads to the safe spot on Tatooine ground). Corroborated by
the **main-thread stack** at crash time (the driver worker has 0 engine frames, but the main thread shows
what the engine was doing):
```
gl11_r!Direct3d11_ConstantBuffer::updateVS        (Direct3d11_ConstantBuffer.cpp:177)
gl11_r!Direct3d11_LightManager::setLights         (Direct3d11_LightManager.cpp:355)
SwgClient_r!ShaderPrimitiveSorter::Phase::draw -> popCell -> popCell
SwgClient_r!RenderWorld::drawScene                (RenderWorld.cpp:1052)
SwgClient_r!RenderWorldCamera::drawScene
SwgClient_r!DebugPortalCamera::drawScene          (DebugPortalCamera.cpp:333)  <-- jump-cutscene camera
SwgClient_r!Camera::renderScene -> ClientWorld::draw -> GroundScene::draw(2348) -> CuiIoWin::draw -> runGameLoopOnce
```
So it is **NOT generic zone-in** — it's the **jump-to-lightspeed (hyperspace) cutscene actively
rendering the scene** via `DebugPortalCamera`, and the driver dies while the main thread is in
**`Direct3d11_LightManager::setLights` -> constant-buffer `updateVS`**. Ties straight to the hyperspace
path (CONSULT-49; `d507019e4` guarded the membrane `CuiWidget3dObjectListViewer` on this same jump).
**Fresh-approach START POINT for A:** investigate `Direct3d11_LightManager::setLights` /
`Direct3d11_ConstantBuffer::updateVS` behavior during the hyperspace cutscene render (light count?
cbuffer size/Map pattern? a per-cell light update racing the driver?). Why `DebugPortalCamera` for the
jump cutscene is also worth a look. This is a much tighter target than "space resource burst."

**Status when paused:** went from intermittent → **consistent** ("very consistent now" per Kenny).
Kenny is testing whether it's a **hard gate vs flakiness**.

**Remaining live hypotheses for A (fresh-perspective candidates):**
1. A **malformed space asset** (a shader's DXBC or a mesh) in the *baseline* space set that the NV JIT
   /resource processor chokes on every time → consistent null-deref. (This is the surviving "it's the
   data" angle — aligns with Kenny's instinct, just client-side not server.)
2. A **driver race on the burst of D3D11 resource creation** at space zone-in (the scene creates many
   shaders/textures/buffers rapidly). Mitigation precedent exists: `maxInteriorCreatesPerFrame`
   throttle for interiors — a space equivalent could spread the burst.
3. Genuinely a driver bug with no clean engine fix → may need a different driver branch / GPU to A/B.

**Next diagnostic for A (not yet done):** turn on the D3D11 debug layer — `[Direct3d11]
enableDebugLayer=true` (config key already exists, ConfigDirect3d11.cpp:59-62; Release defaults false).
It should report the offending resource/API call right before the fault → catches a malformed asset if
that's it. (Needs the DXGI debug/SDK layers installed; there's a fallback path at Direct3d11_Device.cpp:438.)

### PROBLEM B — the 3D space WORLD renders EMPTY (root-caused, fix gated behind A)
When gl11 space DID load (Capture42, under RenderDoc), the **3D world is empty**: HUD renders perfectly,
3D area is the **white backbuffer clear** — no planets/celestials/asteroids/ship/skybox. Identical in
gl05 (renderer-agnostic).

**ROOT CAUSE (decisive, from Capture42 — `stage/Capture42.rdc`, 323 draws):**
- Space geometry **IS submitted and on-screen** — VS transforms a batch vertex to valid NDC (~0.15,
  0.59), passes depth (debug_vertex EID 233). NOT a cull, NOT a transform collapse, NOT camera (the
  `sbssp` identity-at-origin log is the normal object-local convention — red herring).
- The **pixel shader DISCARDS every fragment**: it samples its texture, gets valid RGB
  (e.g. 0.094,0.172,0.094) but **alpha = 0.0**, and the alpha-tested PS `clip(col.a - ref)` discards.
  `pixel_history` shows `shaderDiscarded`/`passed:false` on the batch draws (EID 233/1705/...).
- Net: textured space geometry → **texture alpha 0 → alpha-test discard → empty scene RT → white
  backbuffer shows through**. HUD draws straight to the backbuffer (fine).
- Renderer-agnostic fits: D3D11 emulates alpha-test in the PS (`clip`), D3D9 uses fixed-function — both
  discard the same α=0 sample. So it's an **asset/shader** issue, not a backend bug.

**The remaining open question for B:** *why is the sampled texture alpha 0 / should these draws even be
alpha-tested?* Candidates: (i) the space textures legitimately have low/zero alpha and these shaders
should **alpha-BLEND not alpha-TEST** (glows/billboards); (ii) the per-pass alpha-test enable/ref from
the `.sht` is wrong; (iii) a specific texture's alpha is genuinely 0. **NOT yet pinned** — the RenderDoc
MCP doesn't expose the SRV→texture-name link, which is the wall hit. Codex's discriminator: identify the
exact bound texture name on a failing draw.

**The clean test for B (built, ready, BLOCKED by A):** `[Direct3d11] disableAlphaTest=true` (see below)
forces alpha-test off at runtime. If the world appears → confirms B is alpha-test discard; if still
empty → it's a scene-RT→backbuffer composite issue. **Can't run it until A lets space load.**

---

## SECONDARY CONFIRMED FIND — ILM ships a wrong (Kessel) Tatooine planet
`appearance/planet_tatooine.pln` in **ILM_visuals.tre** references `shader/pln_kessel.sht` +
`shader/pln_cloud_kessel.sht` (Kessel!), whereas the correct patch version references
`shader/pln_tatooine_detail.sht` + `shader/pln_cloud2.sht`. ILM (searchTree priority **5**) **wins**
over the patches (base TREs via searchTOC at priority **0–3**) → the client loads ILM's mislabeled
planet. This is a real bug but it only affects the *main planet* (the moons aren't in ILM), so it is
**not** the empty-world cause — that's Problem B (the alpha-test discard hits everything).

**searchTree/searchTOC priority semantics (confirmed in TreeFile.cpp:118/133/287 + cfg):** higher
number wins, first hit returned. Order in the live `stage/client.cfg`:
`override(10) > loose searchPath(9) > swgsource_3.0(8) > disable_wayfar(7) > ILM_visuals(5) > base data/patch TREs via searchTOC(0–3)`.
So **ILM overrides the base data/patch TREs but is itself overridden by swgsource/loose/override.**

---

## STAGED STATE RIGHT NOW (what's on disk)

**Uncommitted source (this session) — all behavior-neutral when the flag is OFF:**
- `ConfigDirect3d11.{h,cpp}` — adds `[Direct3d11] disableAlphaTest` bool (default false) + getter.
- `Direct3d11_StaticShaderData.cpp` (~line 1703) — when the flag is on, forces the per-pass alpha-test
  **enable=false at RUNTIME** (cbuffer), i.e. `alphaTestEnable = m_alphaTestEnable && !getDisableAlphaTest()`.
  **Does NOT change the generated HLSL** → shader hashes unchanged → **no recompile burst**.
  (An earlier HLSL-edit version of this was REVERTED because it invalidated the whole PS cache →
  game-wide recompile → aggravated Problem A. `Direct3d11_PixelShaderProgramData.cpp` is back to original.)

**Binaries:** `stage/gl11_r.dll` rebuilt 18:17 (runtime gate, clean, 0 unresolved). `SwgClient_r.exe`
unchanged (19:44, carries the morning's committed fixes).

**Config (`stage/client.cfg`, gitignored, BOM-clean):**
- `rasterMajor=11` (gl11).
- `[Direct3d11] disableAlphaTest=true`  ← currently ON for the Problem-B test (harmless; only matters
  once space loads). Set false / remove to restore normal alpha test.
- `searchTree_00_5=ILM_visuals.tre`, `searchTree_00_8=swgsource_3.0.tre`, `searchPath_00_9` loose,
  `searchPath_00_10=stage/override`.

**Override fix #2 staged:** `stage/override/appearance/planet_tatooine.pln` (184 bytes, correct
Tatooine → `pln_tatooine_detail.sht`), priority 10, beats ILM's Kessel one. (Toggle via renaming to
`.disabled`.)

**Captures:** `stage/Capture42.rdc` (the analyzed one), `Capture43/44.rdc` (spares).

---

## CONSULTS (CONSULT-50, this session)
`.planning/research/CONSULT-50-EVIDENCE.md` + `-codex.out` + `-cursor.out`. Both converged: α=0-with-
valid-RGB is a **bad alpha plane in the *winning* DDS** or a **shared alpha-tested shader** (the RGB→
32-bit promotion forces α=0xFF, so it's not a swizzle bug); identical gl05/gl11 failure ⇒ asset/shader,
not backend. Cursor corrected the priority semantics (ILM=5 is a gap-filler vs swgsource=8). Codex's
decisive discriminator: **identify the exact SRV/texture bound to a failing draw** (the MCP wall).
`env_space_tato.dds` alpha checked = **fine** (opaque, ILM copy == base) → cleared as a suspect.

---

## SUGGESTED FRESH APPROACH (next session)
1. **Resolve Problem A first** (it gates everything). The gl05-vs-gl11 A/B is DONE: **gl05 fine, gl11
   crashes → it's the gl11/D3D11 path, not data/driver-generic.** So go straight at the gl11 cutscene
   render path:
   - **Prime suspects (from the main-thread stack):** `Direct3d11_LightManager::setLights`
     (LightManager.cpp:355) and `Direct3d11_ConstantBuffer::updateVS` (ConstantBuffer.cpp:177) during the
     hyperspace cutscene's `DebugPortalCamera` scene draw. gl11 renders the GROUND fine, so diff what's
     different at the SPACE cutscene: light count/config (the space env PARA directionals + dot3 snapshot),
     cbuffer size/`Map` pattern, or a per-cell light update. Look for an out-of-bounds cbuffer write, a
     stale/freed resource handed to the context, or a light array overrun feeding the driver garbage.
   - Turn on `[Direct3d11] enableDebugLayer=true` — the D3D11 debug layer should flag the bad
     resource/call right before the driver faults.
   - Why `DebugPortalCamera` is the cutscene camera is worth confirming (unexpected; CI_debugPortal).
2. **For Problem B**, if A is unblocked: flip `disableAlphaTest=true` (already set) → world appears or
   not. If you'd rather not wait on A: go offline — dump the alpha-test enable/ref from the space `.sht`
   files (cels_star_back.sht, starglow_radial.sht, the `.pln` surface shaders) and from the failing
   draw's StaticShader, to see whether alpha-test is even supposed to be on for these.
3. **Don't re-chase** (ruled out): hyperspace-deactivate (CONSULT-49), DPVS, distance/far-plane,
   PlanetAppearance-path-isolation, camera changes (FreeChaseCamera; flight uses CockpitCamera),
   server/TRE data gap (space_tatooine.trn is complete: 4 PLAN + STAR + 16 CELE), off-thread D3D
   creation, env_space_tato.dds.

**Useful commands:** crash dump `cdb -z <dump> -y stage -lines -c ".ecxr; kb 20; lmvm nvwgf2um; q"`
(cdb at `/c/Program Files (x86)/Windows Kits/10/Debuggers/x86/cdb.exe`). RenderDoc MCP on
`stage/Capture42.rdc` (open_capture / list_draws / debug_vertex / debug_pixel / pixel_history /
export_render_target / export_texture). TRE probes: `swg_pipeline.tre_reader` at
`D:/Code/swg-blender-plugin` (scratchpad scripts from this session show the patterns).
