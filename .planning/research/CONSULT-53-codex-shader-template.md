# CONSULT-53 / Codex — ANGLE: identify the shader template + its AUTHORED pass state

You are a repo tracer. Repo: `D:\Code\swg-client-v2` (SWG client, MSBuild). A D3D11 UI render bug is
fully characterized — see `.planning/research/CONSULT-53-EVIDENCE.md` (read it; treat its measured
RenderDoc values as LOCKED axioms — do NOT re-derive them, do NOT propose they are wrong).

## Your angle (do not stray into the other consultants' lanes)

Find, in the engine source + data, **which StaticShader (`.sht`) template renders the space cockpit
reticle / radar frame**, and what blend + alpha-test state that template's pass(es) are **authored**
with. We have the runtime D3D11 state (alpha-test OFF, alpha-blend ON, teal@0.229). We need the
**source-of-truth shader definition** so we can tell whether D3D11 is applying what the asset asks
for.

Concretely, trace and report:
1. The CUI widget class(es) that draw the space HUD reticle/radar (likely under
   `src/engine/client/library/clientUserInterface/.../SwgCuiHud*` or a space-HUD variant) and the
   exact shader name/path they fetch for the frame element.
2. That shader template's pass state as authored: `alphaBlendEnable`, blend source/dest/op,
   `alphaTestEnable`, `alphaTestReferenceValueTag`. Where does the shader/effect get parsed
   (`ShaderImplementation` / `StaticShaderTemplate` / `ShaderImplementationPass`)? Cite file:line.
3. Whether the reticle **ring** (legitimately visible) and the **frame** (the bug) are the same
   shader/texture batch or different — i.e. is "one shader, one root" actually true?
4. Any UI shader that has `m_alphaTestEnable = true` with a reference that WOULD discard a 0.229-alpha
   fragment — call it out by name.

Return: a concise findings list with file:line citations and the shader-template name(s). No fixes
yet — identification only.
