# CONSULT-37 — Dark walls/buildings in D3D11 (lateral / falsify-the-frame angle)

Read the actual source in `D:\Code\swg-client-v2`; cite file:line. Your job is to be the skeptic — actively try to REFUTE the prevailing explanation, and look where the others won't.

## Repo
SWG client. D3D11 renderer plugin (`src/engine/client/application/Direct3d11/`) ported from D3D9 (`src/engine/client/application/Direct3d9/`).

## The prevailing (UNVERIFIED) explanation I want you to stress-test
"The dark walls are **precalculated-vertex-lit** geometry. They get an empty dynamic light list (size 0). D3D9 renders them via a full-white ambient (1,1,1,0) × baked vertex colors; D3D11 lacks that path AND its asset world VS has no COLOR vertex input, so the baked lighting can't be read → too dark."

## Observed facts (independent of the explanation)
- D3D11: some building/wall geometry is **much too dark — but NOT pure black** (some light reaches it). Same geometry is correct in D3D9.
- Some dark draws log an empty dynamic light list (size 0) in D3D11; correct objects log 3 lights.

## Your task (ONLY the lateral/refutation angle)
1. **Falsify the precalc framing.** Is the dark geometry actually `containsPrecalculatedVertexLighting()`? Find another explanation for `listSz=0` that has nothing to do with precalc: e.g. light **culling/range/attenuation** dropping all lights for far/large bounding volumes, **DPVS/visibility** quirks, draw-order/state-leak, wrong **shader template / LOD child** selected, a transform/normal-space bug making N·L negative (back-faces of the lighting, not back-faces of geometry), or a cbuffer that's stale from a previous draw.
2. **The VB side.** Do these meshes actually carry a baked **color vertex stream** in their asset/MGN/mesh data? Trace how the D3D11 **input layout** is built from the shader/VB declaration — could a declared COLOR element be getting **dropped or mismatched** (vs D3D9) rather than never existing? Independently confirm or refute "the D3D11 world VS has no COLOR input."
3. **"Dark not black" as a clue.** If ambient (~0.13) reaches them but directional does not, that's a different bug than precalc-baked-color. Argue which observed symptom (dark vs black, listSz=0 vs 3) is consistent with which root cause. Where could the 3-light path be silently producing near-zero diffuse (normal/tangent transform, light-direction space mismatch, dot3 producing negatives clamped to 0)?
4. Name the **single cheapest experiment** (a log line, a forced constant, a one-draw RenderDoc capture) that would DISTINGUISH the precalc-baked-color hypothesis from the missing-directional / wrong-normal hypothesis.

Output: a ranked list of alternative root causes with the evidence for/against each, an explicit verdict on whether the precalc framing survives scrutiny, and the one discriminating experiment. Be contrarian where the code supports it.
