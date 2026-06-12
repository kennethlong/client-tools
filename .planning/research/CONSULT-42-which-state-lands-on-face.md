# CONSULT-42 — D3D11 NPC face BLUE, D3D9 PINK, with IDENTICAL light colors. Which state lands on her face?

Date 2026-06-09. Branch `koogie-msvc-cpp20-base` (D:\Code\swg-client-v2). Plugin: Direct3d11; ref: Direct3d9.
You are one of 4 independent consultants. Read SOURCE; reach your OWN conclusion. The §4 candidates are
to be confirmed/refuted, not assumed. RenderDoc + runtime dumps below are locked ground truth.

## 1. SYMPTOM
NPC (cantina) face renders bluish in D3D11; warm/pink (correct, matches her shoulders) in D3D9. Body
is correct in both. Same NPC, same spot.

## 2. WHAT IS ALREADY RULED OUT (by hardware/runtime data — do NOT revisit)
- NOT alpha/translucency (fixed + verified, CONSULT-40).
- NOT a Red<->Blue channel swap, NOT a light-COLOR bug. A runtime dump of the per-draw light snapshot
  in BOTH renderers shows the interior cantina light colors are **byte-for-byte IDENTICAL**:
    medical light: sun=(0.298,0.422,0.425) ambient=(0.202,0.355,0.413) fill0=(0.005,0.187,0.227)
    cell fill:     sun=(0.244,0.244,0.244) ambient=(0.344,0.342,0.345) fill0=(0.106,0.159,0.220)
  Both D3D9 (`Direct3d9_LightManager` selectLights dump) and D3D11 (`Direct3d11_StaticShaderData::apply`
  per-dot3-draw dump) produce these same values. So the light DATA is shared and identical.
- NOT the PS dot3 hemispheric c1/c3/c4 (CONSULT-41 Opus: PS add is ~+0.03 warm; blue is in VS `v1`).
- NOT a register/slot swap (CONSULT-41 Cursor: all D3D9-parity).

## 3. LOCKED GROUND TRUTH
- Her face draw (RenderDoc Capture34, D3D11): VS=2208/PS=2224 (dot3 skinned char). PS input
  **v1 (VS vertex lighting) = (0.524, 0.537, 0.707)** — blue-dominant. diffuse texel warm (0.97,0.94,0.94).
  Output = texel x v1 = (0.506,0.502,0.660) blue.
- Available lighting STATES this session (same in both renderers): warm terrain/exterior sun (~0.90,0.87,0.88,
  R>B), the cyan medical light (0.298,0.422,0.425), and the gray-sun+blue-fill state (sun 0.244 gray,
  fill0=(0.106,0.159,0.220) blue).
- D3D9 face = PINK => its `v1` is warm. D3D11 face = BLUE => its `v1` is from a blue/cyan state.
- So: **same light data, but a different lighting STATE is active when her face draws** — D3D9 warm, D3D11 blue.

## 4. CANDIDATE MECHANISMS (confirm/refute; the light colors are NOT the bug)
(a) **Snapshot stickiness** — CONSULT-38 made the D3D11 dot3 snapshot `s_pixelDot3State` PERSIST across
    empty/sunless `setLights({})` calls (needed for black walls). A nearby BLUE interior light's snapshot
    could stay "stuck" and be read by her face draw, where D3D9 refreshes lighting per-object.
    Key D3D11: `Direct3d11_LightManager.cpp` setLights (~:71-236, the `if(psSlot)` persistence at
    :84-90 dot3 + :207-235 parallel), getPixelDot3State; `Direct3d11_StaticShaderData::apply` reads d3.
(b) **Selection cascade** — D3D11 selectLights cascade (black-walls fix, `Direct3d11_LightManager.cpp:100-175`)
    picks the cyan medical light into psSlot/parallel for her cell where D3D9 `selectLights`
    (`Direct3d9_LightManager.cpp:404-440`) picks the warm sun. (Same light list, different winner.)
(c) **Neither** — her face's OWN cell lights are genuinely the blue-fill state in BOTH, and D3D9 differs
    elsewhere: cell assignment, full-ambient (`Direct3d9_StaticShaderData.cpp:776,955` setFullAmbientOn —
    NO D3D11 equivalent), or the VS vertex-light path (D3D11 HLSL `if(!dot3)` skip per CONSULT-41 Sonnet).

## 5. YOUR ANGLE (answer ONLY your section)
### A — CODEX (dataflow/order trace)
Trace the ORDER of setLights vs the character/skin DRAW in D3D11 vs D3D9. Does the snapshot her face reads
come from HER object's lights or a PRIOR object's (sticky)? State the first line where D3D11's per-object
lighting refresh diverges from D3D9's, such that a blue state survives onto her face.
### B — CURSOR (sticky-snapshot detail)
Pin the exact lines where `s_pixelDot3State` (and the parallel/ambient it feeds c16/c20-23) PERSISTS vs
UPDATES across an empty/sunless `setLights`. Can a prior blue interior light's parallelDiffuseColor/ambient
remain and be read by her face draw? Contrast with D3D9's per-object `selectLights` rebuild
(`Direct3d9_LightManager.cpp:380-477` clears ms_currentLights each call). Cite file:line.
### C — SONNET (lateral/refute)
Try to REFUTE stickiness. Enumerate: (a) sticky, (b) cascade, (c) her legit cell-light is blue + D3D9
differs via full-ambient/cell-assignment/VS-path. Which survives the §2/§3 facts? Is there a 4th mechanism?
### D — OPUS (numeric)
From v1=(0.524,0.537,0.707) and the three candidate states (warm sun ~0.90,0.87,0.88; cyan medical
0.298,0.422,0.425; gray-sun+blue-fill 0.244 / fill 0.106,0.159,0.220), back-solve WHICH state (and which
combo of ambient+parallel-fill+dot3-sun) produces v1. Identify her face's actual active state, and whether
it is a state that should NOT be on her face (=> sticky/wrong-selection) vs her legit cell light.

## 6. DELIVERABLE (per angle)
(1) finding w/ file:line; (2) confirm/refute each §4 candidate; (3) minimal D3D9-parity fix + exact location
(must NOT regress the CONSULT-38 black-walls stickiness or the 3-directional black-walls feed); (4) the one
decisive measurement (e.g. log the active snapshot AT the skinned-character draw specifically in both).
