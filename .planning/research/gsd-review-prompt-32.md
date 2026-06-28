# Cross-AI Plan Review Request — Phase 32 (ROUND 2, revised draft)

You are independently reviewing implementation plans for a software-engineering phase.
Provide structured, adversarial feedback on plan quality, completeness, and risk. Be specific
and cite plan IDs / file:line. Do NOT rubber-stamp — your value is catching what the others miss.

> NOTE: These plans are a **revised draft** (edited after an earlier review pass). Review the
> **current state on disk** on its own merits. Do not assume any prior feedback was or wasn't
> addressed — judge what is written now.

## Project Context

**whitengold** — modernisation port of the leaked Star Wars Galaxies client (NGE-era ~2010) off
VS2005 onto MSVC/C++20/MSBuild. Two renderers ship and must both stay bootable to character select:
D3D9 (plugins gl05/gl06/gl07, `rasterMajor=5/6/7`) and D3D11 (gl11, `rasterMajor=11`). Current
milestone **v3.0 = x64 port**. Phase 32 is a **prerequisite** for the first x64 link (Phase 33):
take the D3D9 runtime shader-compile path off the x64-hostile legacy **D3DX** library and onto
`d3dcompiler_47` (`D3DCompile` for HLSL, `D3DAssemble` for asm), validated in the **32-bit** build
first.

Critical history: **Phase 27 already attempted** the minimal same-profile `D3DXCompileShader`→
`D3DCompile` recompile and was **REVERTED** (commit `c0f890875`) because `d3dcompiler_47` is strict
where 2003-era D3DX was lenient (reserves `point`/`line`/`triangle` keywords → X3000; rejects
stacked `: register()` on struct members → X3202; etc.). gl11 (D3D11) already solved these exact
problems in Phase 11 via a textual-rewrite pass (`Direct3d11_HlslRewrite`, Rules A–F). The locked
decision is to **lift gl11's full rewrite treatment** into the D3D9 path, NOT re-attempt the minimal
recompile. A prior WIP branch `d3d9-fixb-d3dcompile-wip` already has the HLSL port "compiling but
render-incomplete."

## Artifacts to Review (READ THESE FROM DISK)

All under `D:\Code\swg-client-v2\.planning\phases\32-d3dx-to-d3dcompiler-47\`:
- `32-CONTEXT.md` — locked decisions D-01..D-07
- `32-RESEARCH.md` — domain research, pitfalls, code examples, assumptions log
- `32-VALIDATION.md` — validation strategy (no automated shader-test framework; manual render smoke)
- `32-01-PLAN.md` — **Wave 0**: census + D3DAssemble dialect probe (the de-risk GATE)
- `32-02-PLAN.md` — **Wave 1**: lift gl11 rewriter + port HLSL path to D3DCompile
- `32-03-PLAN.md` — **Wave 2 (PASS branch)**: port asm path to D3DAssemble — CONDITIONAL on gate PASS
- `32-04-PLAN.md` — **Wave 2 (FAIL branch)**: keep asm on D3DXAssembleShader+Fix-A — CONDITIONAL on gate FAIL
- `32-05-PLAN.md` — **Wave 3**: dumpbin/grep D3DX-removal proof + Fix-A retirement + dual-renderer validation

Also relevant (read if useful): the actual port target
`src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp`, the gl11 source
of truth `src/engine/client/application/Direct3d11/src/win32/Direct3d11_HlslRewrite.{h,cpp}`, the
WIP branch `d3d9-fixb-d3dcompile-wip`, and `.planning/REQUIREMENTS.md` (SHADER-01).

## Requirement Under Test

**SHADER-01:** "The legacy D3DX shader-compile path is ported to `d3dcompiler_47` (`D3DCompile`) and
**D3DX is removed from the x64 build** (D3DX is x64-hostile); both renderers compile/load their
shaders correctly **under x64**. (Carries the v2.3-deferred HARD-05; the Fix-A SEH guard is retained
until the asm path is also off D3DX.)"

Phase 32 success criteria (ROADMAP): SC#1 = compile path ported, no VS silently nulled; SC#2 = D3DX
removed from the shader-compile path (dumpbin/grep proof), non-compile D3DX (mesh/matrix/surface,
~23 call sites) **retained** (D-05); SC#3 = both renderers render clean in the 32-bit build.

## Review Instructions

For the plan set as a whole and per-plan where relevant, provide:

1. **Summary** — one-paragraph assessment of the plan set.
2. **Strengths** — what's well-designed (bullets).
3. **Concerns** — issues, gaps, risks (bullets, each tagged severity HIGH / MEDIUM / LOW).
4. **Suggestions** — specific, actionable improvements (bullets).
5. **Risk Assessment** — overall LOW / MEDIUM / HIGH with justification.

Focus on: missing edge cases / error handling; dependency & wave ordering; the conditional Wave-2
branching (gate PASS→32-03 vs FAIL→32-04) — is it sound and unambiguous?; scope creep vs the
Phase-27-revert lesson; whether the plans actually achieve SHADER-01 given it says "removed from the
**x64** build / under **x64**" but Phase 32 validates in **32-bit** and deliberately keeps `d3dx9.lib`
linked; the render-correctness risk (register-semantics / `register(vN)` omission) the WIP branch
stalled on; and any factual claims in the plans that look wrong or unverifiable.

Output your review in markdown.
