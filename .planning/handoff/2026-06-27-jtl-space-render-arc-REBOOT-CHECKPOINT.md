# JTL Space-Render Arc — REBOOT CHECKPOINT (2026-06-27)

**Why this file:** mid-investigation on JTL space rendering; rebooting to activate a new NVIDIA driver
(the session will break). Resume from here. Companion memory: `project_jtl_space_world_empty_render`,
`project_jtl_space_ship_data_ilm_exclusive`. Consult outputs: `.planning/research/CONSULT-48-*`, `CONSULT-49-*`.

---

## TL;DR — where we are

JTL space now **loads and is mostly working**: HUD/targeting/radar render, the **ship model renders**,
**jump-to-lightspeed survives**. The **one open bug**: the 3D space **WORLD is empty** — no planets,
celestials, or ships render; only the **starfield (skybox) + UI markers**. It's **identical in gl05 and
gl11** (renderer-agnostic). A 4-AI crew (CONSULT-49) ruled out the first hypothesis. The decisive next
measurement is a **gl11 RenderDoc capture of space** — which is blocked only by an NV driver crash that
**a reboot will fix** (driver installed but not yet loaded).

---

## DONE + COMMITTED today (do NOT redo)

| Fix | Commit | Verified |
|---|---|---|
| Char-select/object fade-in (gl11 alpha-blend during fade) | `fe92ab6b4` | ✅ user-verified |
| Nebula null lightning-appearance guard | `ac6ab3310` | built+deployed |
| Jump-to-lightspeed membrane null-texture guard (`CuiWidget3dObjectListViewer`) | `d507019e4` | ✅ jump survives |

**Config fix (in gitignored `stage/client.cfg`, NOT committed — local only):**
- `searchTree_00_5="D:/Code/SWGSource Client v3.0/ILM_visuals.tre"` — wires the ILM space-visual layer
  that holds the ship-chassis data (CONSULT-48). This is what made the **ship model render** (was a
  default placeholder). Without it, ships are placeholders. **Keep this line.**
- The earlier `searchPath_00_9="D:/Code/SWGSource Client v3.0/"` (CONSULT-47, GU21 HUD) — keep.
- `rasterMajor=11` is currently set (we were A/B-testing gl11). Set back to `5` for gl05 if desired.

`stage/SwgClient_r.exe` (Win32 Release) carries all the committed source fixes (rebuilt 7:44 PM).

---

## THE OPEN BUG: empty space world

**Symptom:** in space (real server entry, Scyk fighter from Tatooine station), you see starfield +
HUD + markers, but **no planets, celestials, or ships**. Same in gl05 + gl11.

**Ruled out (do not re-chase):**
- NOT renderer-specific (gl05 == gl11).
- NOT the Utinni offline harness (real server entry).
- NOT missing planet assets — `planet_tatooine.pln` (+moons), `cels_star_back.sht`, `starglow_radial.sht`,
  `stars_tato.tga` are all present + WIRED; `.pln` PlanetAppearance type IS registered
  (`SetupClientTerrain -> PlanetAppearanceTemplate::install`).
- NOT the hyperspace-deactivate (CONSULT-49, 3-of-4): `SpaceEnvironment` is a singleton recreated
  FRESH+active per zone arrival; station-launch no-ops the `dynamic_cast` on ground origin; stock SOE
  2004 code; and it's scope-mismatched (ships are server objects NOT in the disable list, so it can't
  explain missing ships). **DO NOT add `enableEnvironmentForHyperspace`.** (Sonnet dissented on emulator
  timing — a one-line neuter of `deactivateSpaceEnvironmentObjects()` at `HyperspaceIoWin.cpp:790` would
  test it, but it's superseded by the points below.)
- DPVS (Kenny flagged our `occlusionMode` tri-state mod): live cfg `[ClientGraphics/Dpvs]` has only
  `reportInstrumentation=true` -> occlusion defaults OFF, min-coverage cull defaults OFF
  (`ms_disableObjectMinimumCoverage=true`), only view-frustum cull on in release (can't hide in-view
  objects). Our DPVS mod is renderer-DEPENDENT but the symptom is renderer-agnostic -> argues against it.

**Best lead (where to look next):** stars + planets BOTH go through `RenderWorld::addWorldEnvironmentObject`
(`SpaceEnvironment.cpp` ~168 stars / ~241 planets) yet **stars render, planets don't**. Same submission
path, different result -> points at the **`PlanetAppearance` render itself** (or object transform/scale/
visibility), NOT a blanket cull (a cull would drop stars too). Files: `clientTerrain/.../appearance/
PlanetAppearance.cpp`, `CelestialObject.cpp`, `SpaceEnvironment.cpp`.

---

## NV DRIVER (the reboot reason)

gl11 crashes on space load with `nvwgf2um!...+0x9f81c` null-deref. Dump confirms the **loaded driver is
still `32.0.15.9186` (591.86)** — the new driver you installed **isn't active until reboot**. (Memory:
`project_nvwgf2um_null_deref_cui_text_d3d11_map`.) Threaded-Optimization-off didn't reliably stop it;
the driver update should. You can revert the Threaded-Optimization toggle after the new driver is live.

---

## NEXT STEPS (after reboot) — in order

1. **Verify the new driver loaded:** launch gl11 (`rasterMajor=11`), go to space. If it **survives the
   load** (no `nvwgf2um` dump in `stage/`), the driver fix worked. (A future dump, if any, would show a
   version != 591.86.)
2. **Grab a gl11 RenderDoc capture of space** — face the planet / the "Census" 500m target, F12/save,
   drop in `stage/` (any name). This is THE decisive measurement.
3. Hand the capture to Claude. Analysis splits the bug cleanly:
   - **Planet/ship DRAW CALLS present** in the frame -> geometry is *drawn but invisible* (transform/
     scale/alpha/PlanetAppearance) -> trace the draw + shader.
   - **No such draws** -> geometry *never submitted* (DPVS visibility / scene-population) -> instrument
     the submit path.
4. From there, fix the identified cause (one bug for planets/celestials; ships may be a second cause).

**Useful commands for resume:**
- Analyze a crash dump: `cdb -z stage/<dump>.mdmp -y stage -lines -c ".ecxr; kb 25; lmvm nvwgf2um"`
  (cdb at `/c/Program Files (x86)/Windows Kits/10/Debuggers/x86/cdb.exe`).
- RenderDoc analysis: the `renderdoc-mcp` MCP tools (open_capture / list_draws / pixel_history /
  get_pipeline_state / get_shader) — used for Capture40/41 earlier.
- TRE/asset wiring probes: `swg_pipeline.tre_reader` at `D:/Code/swg-blender-plugin` (scratchpad scripts
  from this session show the pattern).

---

## Open follow-ups (lower priority)
- gl11 space HUD parity: cyan-tinted brackets + dimmer starfield vs gl05 (cosmetic).
- `.planning/todos/pending/2026-06-26-d3d11-space-rendering-frontier.md` tracks the broader space frontier.
- Commit the `ILM_visuals` cfg wiring intentionally (it lives in gitignored `stage/client.cfg`; decide
  whether to promote into a tracked cfg template) once space is stable.
- **FreeChaseCamera interior zoom-cap mis-fires in walkable ship interiors** (latent, separate):
  `.planning/todos/pending/2026-06-27-freechasecamera-interior-zoomcap-space-guard.md`.

---

# === POST-REBOOT SESSION UPDATE (2026-06-27) ===

Reboot done. NV driver loaded: **`32.0.15.9186` → `32.0.16.1062`** (the crashing 591.86 is gone).
Canonical Win32 Release rebuild = clean no-op (0 unresolved), binaries confirmed current. cfg unchanged
(`rasterMajor=11`, `ILM_visuals.tre` + `searchPath_00_9` wired). **gl11 should now survive space load
— first thing to confirm.**

## Two leads chased to ground this session — BOTH closed

### 1. Server / TRE data gap — **RULED OUT (data is complete)**
- `D:/Code/swg-main` (server source) has the full `ship_chassis` tables → confirms the ILM split
  ([[project_jtl_space_ship_data_ilm_exclusive]]), nothing missing server-side.
- The ENTIRE space scene is ONE `STAT`-form file: `terrain/space_tatooine.trn`. Winning copy =
  `patch_15_02.tre` (highest patch; nothing in the searchTrees or `swgsource_3.0.tre` overrides it).
  Cracked it open — fully populated: `SKYB` nebula2; `STAR` `stars_tato.tga` **24000** stars; **4×
  `PLAN`** (`planet_tatooine.pln` + 3 moons, |dir| 369–547 m); **16× `CELE`** (`cels_star_back.sht` /
  `starglow_radial.sht`, |dir| 1.7–82 m). Planets defined right beside the star def that renders.
  (Probe scripts in the scratchpad; format from `SpaceTerrainAppearanceTemplate::load_0000`.)

### 2. Camera changes — **RULED OUT for the empty world** (good catch, but inert here)
- Both changes (`810b6c9a9`, `3549c7104`) are **`FreeChaseCamera.cpp` only**.
- Space flight uses **`CockpitCamera`** — `ShipStation_Pilot` → `CI_cockpit` at `GroundScene.cpp:2209`.
  `FreeChaseCamera` is the on-foot/walking camera, NOT active while piloting.
- Independent proof: the 24k `StarAppearance` stars (world-environment objects) DO render → the camera
  is not culling the world cell; a camera/cell bug would drop the stars too.
- Logged the one real (separate) finding: the interior zoom-cap mis-fires in walkable ship interiors —
  see the follow-up todo above.

## Two prior hypotheses in this file are now REFUTED (do not re-chase)
- **"stars render, planets don't, same `addWorldEnvironmentObject` path → `PlanetAppearance` suspect"**
  — FALSE. `StarAppearance`, `CelestialObject`, `PlanetAppearance`, and `SkyBox6SidedAppearance` ALL
  render via `LocalShaderPrimitive` + `ShaderPrimitiveSorter`; skybox+stars+celestials share the SAME
  `ms_worldEnvironmentObjects` list. No path isolates `PlanetAppearance`.
- **distance / far-plane cull** — FALSE. Failing objects span near (celestials 1.7 m) AND far (planets
  547 m). The white specks are the untextured 24k `StarAppearance` points, NOT the nebula2 skybox.

## The refined picture + LEADING hypothesis
**Discriminator = textured/alpha-blended billboards FAIL, untextured vertex-colored points RENDER.**
Works: `StarAppearance` (color-ramp points, no texture). Fails: `CelestialObject` (`.sht` billboard) +
`PlanetAppearance` (`.pln` sphere/halo).

**Kenny's de-anchor (LEADING):** "renderer-agnostic" is NOT proof of "not a renderer bug." BOTH
renderers have modified shader-compile paths — gl05 (Phase 32 `D3DXCompileShader`→`D3DCompile`,
Fix-A SEH, HlslRewrite, VS bytecode cache) and gl11 (asset-PS pipeline, ps.1.1). The same textured
`.sht`/`.pln` shaders could be failing to compile/bind in EACH renderer independently, same visible
result. Fits the textured-vs-untextured split exactly (trivial star shader survives both modified
paths; textured billboards don't). See [[feedback_renderdoc_d3d9_vs_d3d11_is_the_diagnostic]].

## NEXT — gl11 RenderDoc frame-grabs (Kenny grabbing), capture must answer
1. Do draws for `cels_star_back.sht` / `starglow_radial.sht` / `planet_tatooine.pln` exist at all?
   (submitted vs not) — `list_draws` / `search_shaders`.
2. If yes: REAL PS or fallback? Texture SRV bound (non-null)? Blend state + output alpha? —
   `pixel_history` → `get_draw_info` → `get_shader` → `get_pipeline_state`.
3. Cross-check the gl05 compile path for the same `.sht` (RenderDoc can't capture D3D9 — reason from
   shader-cache / compile logs): did the D3DCompile/HlslRewrite port drop these specific shaders?

Splits **drawn-but-invisible** (alpha/blend/texture) from **never-submitted** (alter / compile-fail / cull).
