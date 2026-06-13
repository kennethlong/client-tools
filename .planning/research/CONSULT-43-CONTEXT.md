# CONSULT-43 — Cantina door-crossing positional snap (D3D9-only)

Repo: `D:\Code\swg-client-v2` — SWG client, MSBuild, 32-bit. You can read the source directly.
Full investigation history: `.planning/debug/cantina-corner-snap.md` (Cycles 1-10).

## LOCKED ground-truth — measured/observed in-game. Do NOT contradict or re-derive these.

1. Walking THROUGH the Mos Eisley cantina door (exiting) visibly SNAPS/teleports the
   player ~85-90% of crossings. BOTH the front (main entrance) and back doors. Interior
   traversal between the cantina's sub-cells (foyer1 / foyer2 / cantina) does NOT snap.
2. The snap is a one-frame POSITIONAL jump (character/camera visibly jumps position),
   not a render-only flicker of otherwise-stationary geometry.
3. It reproduces under `rasterMajor=5` (gl05) AND `rasterMajor=7` (gl07) — both D3D9
   renderer plugins — in BOTH Debug and Release builds. It does NOT reproduce under
   `rasterMajor=11` (gl11, the D3D11 plugin): D3D11 is clean, no door snap.
4. It is NOT a collision-resolution event: exits with no wall contact produce ZERO
   collision-resolve activity (instrumented, CORNERSNAP-RESOLVE) yet still snap.
5. Geometry: the cantina interior floor sits ~4 m ABOVE the exterior terrain at the
   doorway — interior floor world-Y ≈ 5.1, exterior terrain world-Y ≈ 1.06.
6. Present/vsync is NOT the cause: forcing the D3D9 path to vsync-ON did not remove it
   (if anything slightly worse); D3D11 runs vsync-ON and is clean. Frame-rate caps
   (30 vs 250 FPS) did not change it either.
7. **It is NOT a renderer or config setting.** Two OTHER clients that do NOT have the
   door snap — SWGEmu (the unmodified original SOE client) and SWG Restoration (built on
   the SWGSource codebase, same lineage as ours) — BOTH run `rasterMajor=7` (gl07 /
   D3D9-VSPS) with `useSafeRenderer=0`: the SAME renderer config that snaps in our client.
   So the difference is in OUR CODE/BUILD, not in any cfg key or renderer choice. (Our
   client is a SWGSource derivative; the relevant upstream is SWG-Source/client-tools.)
   Corollary: our D3D11 being clean is our build's present/timestep happening to mask the
   transient — NOT evidence that D3D9 is inherently the cause.

## Context leads — from OUR source analysis. TREAT AS UNVERIFIED; confirm OR refute.

- The view/world/projection matrices are believed pushed to the GPU through a single
  shared path `Camera::applyState()` (`src/engine/client/library/clientGraphics/src/shared/Camera.cpp:629-637`,
  `Graphics::setWorldToCameraTransform`) that BOTH backends consume — i.e. the camera
  transform is believed identical across renderers. **If this is NOT actually identical
  in the D3D9 vs D3D11 backends at a portal frame, that is a key finding.**
- The player's per-frame position/cell at a portal goes through
  `CellProperty::Notification::positionChanged` (portal crossing) and
  `Object::getTransform_o2w` (cell-attached vs world-cell resolution,
  `sharedObject/.../object/Object.cpp`). Floor height is owned by floor-following /
  ground-snapping (`Footprint`, sharedCollision), not the XZ collision response.
- Renderer plugins: D3D9 = `src/engine/client/application/Direct3d9` (gl05/gl06/gl07
  variants share the Direct3d9 base); D3D11 = `.../Direct3d11` (gl11).

## Additional lead — build / FP-precision divergence (verify or refute)

- The two no-snap reference clients differ from ours in shader-compile DirectX: Restoration
  ships `d3dcompiler_47.dll` (modern Win SDK, no legacy d3dx9) and has 32-bit AND x64 builds;
  SWGEmu uses older SOE plugin builds. OUR client still uses the legacy `D3DXCompileShader`
  (old DX9 SDK) path and is 32-bit only.
- This codebase has a documented history of **FPU control-word / float-precision disturbance
  from legacy D3DX** (D3DXCompileShader FP crash; "OutputDebugString resets FPU precision to
  53-bit"). Engine FPU mask was measured fpcw=0x007f.
- The INTERIOR corner snap was shown to be **codegen-fragile**: the SAME source built with
  MSVC v143 did NOT snap, built with v145 DID (zero source changes between the two builds).
- Hypothesis worth testing: the snap is a **floating-point-precision / codegen fragility in
  the portal-exit position/transform math**, tripped by our build (v145 + legacy-D3DX FPU
  state) at the largest-magnitude portal (the ~4 m door), and not tripped by the
  SWGSource-lineage builds. If so, the discriminator is whether the portal-exit Y math is
  sensitive to FPU precision mode / float associativity, and whether forcing a stable FPU
  control word (or the D3DCompile port) removes it.

## BANNED — already built, runtime-tested, FALSIFIED. Do NOT propose these as the fix.

- A same-frame portal cell "ping-pong" re-entrancy guard in `CellProperty::positionChanged`.
  Built, tested, REVERTED — it desynced cell membership from position (interior rendered
  as skybox).
- A fix in `CollisionResolve::resolveCollisions` (the `resetPos` rewind). Built, tested —
  did NOT remove the door snap (per LOCKED fact #4 the snap is collision-independent).

## The shared question (your angle narrows it — stay in your lane)

Why does exiting the cantina door produce a one-frame POSITIONAL snap under the D3D9
backends (gl05/gl07) but NOT under D3D11 (gl11), given the simulation is shared, it is
collision-independent (#4), and the camera matrix is believed shared? Where in the
cell→world position / transform / render handoff does the D3D9 path INTRODUCE — or fail
to ABSORB — the ~4 m one-frame discontinuity that D3D11 does not?

## Output format

Concise. (a) Your mechanism hypothesis. (b) Specific file:line evidence. (c) ONE empirical
discriminator (a probe/log/test) that would confirm or refute it. If you conclude the
"matrices identical across backends" lead is wrong, say so with evidence — that is a
welcome finding, not a violation.
