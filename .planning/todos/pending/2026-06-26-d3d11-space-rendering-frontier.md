---
created: 2026-06-26
title: D3D11 (gl11) space/JTL rendering parity frontier — triage + fix
area: client graphics / Direct3d11 / space (JTL) rendering
next_action: working the magenta nebula PS first (fallback-PS → failing .sht → PS-gen fix). RenderDoc space capture needed. Then planets env-map, radar, scene wash, debris.
files:
  - src/engine/client/library/clientGame/src/shared/space/  (NebulaManagerClient, space scene)
  - src/engine/client/application/Direct3d11/  (PS gen / asset-PS pipeline; fallback PS)
  - shaders: shader/pt_nebulae_gas_*.sht (nebula gas), planet/skybox shaders, space radar
references:
  - memory: project_d3d11_ffp_combiner_cascade  (magenta == fallback pixel shader; RenderDoc pixel_history→draw→get_shader recipe)
  - memory: project_d3d11_multitexture_srv_slot_swap, project_d3d11_world_view_anomalies_phase19  (asset-PS pipeline patterns)
  - .planning/todos/pending/2026-06-13-test-jtl-space-rendering-post-v2.2.md  (the predicted JTL render-test pass — this IS it)
  - feedback: renderdoc_d3d9_vs_d3d11_is_the_diagnostic
status: research
priority: medium (space now reachable via the cfg fix + Utinni; visuals broadly broken)
---

## What this is

Space was EXPLICITLY EXCLUDED from the earlier D3D11 phases (Phase 11). Now that space loads
(2026-06-26 cfg searchPath fix + Utinni offline scene load), the space view is broadly broken
(screenShot0407/0408, space_yavin4 via Utinni).

**CORRECTION 2026-06-26 (ground-truth de-anchor): NOT a D3D11 parity bug.** Kenny loaded the SAME
scene under gl05 (D3D9, rasterMajor=5) and it shows the SAME problems. So these are renderer-AGNOSTIC.
Root cause is almost certainly the **Utinni offline single-player load producing a structurally
incomplete space scene**: the space environment (player SHIP, celestials/planets, asteroids, sensors/
radar, space lighting) is SERVER/object-driven (ClientAsteroidManager, ship objects, celestial data,
ShipTargeting) and a no-server single-player load never creates it. The "naked avatar, no ship" is
the tell. Both renderers draw the same incomplete scene -> the deficiency is in the SCENE/DATA, not
the renderer. (I initially mis-framed this as a gl11 PS-fallback and built a nebula //hlsl override
before the gl05 A/B -- see below.)

Implication: the **offline Utinni harness can't validate space RENDERING** (it can for ground, which
is self-contained client data). Real renderer parity issues (if any) need a REAL space entry
(connected server zone-in). Reframes the 2026-06-13 JTL-render-test todo: that test needs a server
space session, not the editor scene-load.

## Triage (from screenShot0407/0408)

1. **Player = naked humanoid, no ship** — NOT a render bug. Utinni offline load drops the ground
   avatar into the space terrain without spawning a ship/POB. Expected for the harness; ignore.
2. **Planets render as reflective chrome spheres** (striped env-map showing a landscape reflection)
   — skybox/celestial-sphere shader wrong in gl11 (likely cube/env-map misbind).
3. **Pink/purple nebula cloud** — INITIALLY mis-read as fallback-PS magenta. But the texture
   (pt_nebulae_gas_4_2.dds) IS present, gl05 shows the same, and gl05 compiles ps.1.1 natively -> the
   pink is most likely the REAL nebula color, not a bug. I built //hlsl overrides for the nebula PS
   (e_nebula_emisadd.psh / a_modulate.psh, in stage/override/pixel_program/ + _build_nebula_overrides.py)
   BEFORE the gl05 A/B -- HELD UNCOMMITTED, unverified. Harmless (faithful asm mirror, PEXE preserved
   for gl05) and a legit gl11 reauthor IF a real gl11 nebula fallback is ever confirmed by capture,
   but NOT validated as a needed fix. Revisit only with a real space entry + capture.
4. **Radar/minimap = noise** — space sensor/radar render garbage.
5. **Green/teal whole-scene wash** — wrong space fog/lighting/post tint.
6. **Floating black debris squares** — broken particles or asteroid/debris draws.

## Approach
One at a time, RenderDoc-driven (gl11 capture → pixel_history → draw → get_shader → PS-gen fix),
highest-diagnostic-value first. Likely shared root causes with the known asset-PS / texture-bind /
FFP-combiner work. Open a consult with neutral capture evidence (not hypothesis) when a finding needs
cross-check.

## Done when
- Each triage item is either fixed (gl11 matches gl05 reference) or documented as out-of-scope
  (e.g. #1 harness artifact);
- a space zone renders without fallback-PS magenta / garbage radar / wrong wash.
