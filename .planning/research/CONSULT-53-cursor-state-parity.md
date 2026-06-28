# CONSULT-53 / Cursor — ANGLE: D3D9-vs-D3D11 state-translation parity diff

You are the detailed file:line reader. Repo: `D:\Code\swg-client-v2` (SWG client, MSBuild). A D3D11
UI render bug is fully characterized — see `.planning/research/CONSULT-53-EVIDENCE.md` (read it;
treat its measured RenderDoc values as LOCKED axioms — do NOT re-derive them).

## Your angle (do not stray into the other consultants' lanes)

Diff the **D3D9 vs D3D11 renderer state-apply paths** for a UI StaticShader pass, byte-for-byte, and
find where they diverge such that the frame fragment (teal texel, alpha 0.229) ends up **visible in
D3D11 but invisible in D3D9**. RenderDoc cannot capture D3D9 — reason purely from source.

Compare these specific sites and report exact divergences with file:line:
1. **Alpha-test.** D3D11: `Direct3d11_StaticShaderData.cpp:1691-1706` (resolves enable+ref into PS
   cbuffer). D3D9: `Direct3d9_StaticShaderData.cpp` ~740-760 (and wherever it sets
   `D3DRS_ALPHATESTENABLE` / `D3DRS_ALPHAREF` / `D3DRS_ALPHAFUNC`). Does D3D9 enable alpha-test for
   this UI pass where D3D11 reads enable=0? Is there a default/fallback that differs (e.g. D3D9
   leaves a prior ALPHAREF set, or a global alpha-test default the D3D11 path never replicates)?
2. **Blend.** D3D11: `Direct3d11_StaticShaderData.cpp:1664-1682` (`setAlphaBlendEnable` +
   `setAlphaBlendFactors`). D3D9 equivalent (`Direct3d9_ShaderImplementationData.cpp` ~255-261 and
   `Direct3d9.cpp` ~4089-4094). Same enable + factors for a UI pass?
3. **The alpha-FADE override** (`Direct3d11_StaticShaderData.cpp:1735-1764`) — could
   `getAlphaFadeEnabled()` be spuriously true during the HUD pass and force blend ON / mask alpha
   color-write differently than D3D9?
4. The known intentional **Compare[] table swap** (`Direct3d11_StaticShaderData.cpp:185-186`,
   memory `d3d9_compare_table_swap`) — is alpha-test compare routed through that swap, and would it
   matter here?

Return: the single most likely state divergence, with the exact D3D9 and D3D11 file:line pair that
proves it, and what the correct D3D11 value should be. You have a high hit-rate when you dissent from
consensus — if you think the root is NOT a state-translation diff, say so and name what it is.
