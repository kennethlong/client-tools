# Phase 32: D3DX to d3dcompiler_47 - Context

**Gathered:** 2026-06-16
**Status:** Ready for planning

<domain>
## Phase Boundary

Port the **D3D9 runtime shader-compile path** off the x64-hostile, modern-toolchain-faulting
D3DX dependency onto `D3DCompile` / `D3DAssemble` (`d3dcompiler_47`), and validate both renderers
still boot to character select and render in the **32-bit** build. This lands the v2.3-deferred
HARD-05 (SHADER-01) as a true prerequisite for the first x64 link (Phase 33).

**In scope:** the runtime D3D9 vertex-shader compile surface — a single file,
`src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp`, with two paths:
the HLSL path (`D3DXCompileShader`, :101) and the asm path (`D3DXAssembleShader`, :567).

**Out of scope (this phase):**
- The **non-compile** D3DX uses in the D3D9 plugin (mesh/matrix/surface — ~15 APIs: `D3DXMesh`,
  `D3DXMatrixMultiply`, `D3DXLoadSurfaceFromSurface`, `D3DXCreateMeshFVF`, …). These keep `d3dx9`
  linked but are a separate, larger non-shader refactor — captured as a staged follow-up (see Deferred).
- The x64 platform itself (Phase 33). Phase 32 validates the port in the **32-bit** build first.
- The offline ShaderBuilder tool compile sites (`ShaderBuilder/*` — pre-broken editor tools, not in
  the runtime/boot path).
- gl11 (D3D11) shader compile — already on `D3DCompile` since Phase 11; untouched here.

</domain>

<decisions>
## Implementation Decisions

### Compile mechanism (the strategic crux)
- **D-01:** Port `D3DXCompileShader` → `D3DCompile` (`d3dcompiler_47`). **Honor SHADER-01 as written.**
  Explicitly **reject** Restoration's redist-d3dx9 shortcut (swap static `d3dx9.lib` → redist
  `d3dx9_NN.dll` + ship the DLL). Rationale: Restoration took shortcuts the project does not want to
  take — the real, clean port is the goal even though Phase 27 proved it re-fights the gl11 HLSL
  battle. This is the higher-effort, higher-risk-of-revert path, chosen with eyes open.

### HLSL port approach
- **D-02:** Lift gl11's **full** shader-modernization treatment into the gl05/gl07 D3DCompile path —
  NOT the minimal same-profile recompile (that approach already failed and was reverted in Phase 27,
  because `d3dcompiler_47` itself reserves `point`/`line`/`triangle` and rejects `: register()` on
  struct members regardless of target profile). The full treatment = `Direct3d11_HlslRewrite`
  rules A/B/C + `DECLARE_textureCoordinateSets`-without-register + cbuffer/register handling +
  the `stage/override/` reauthored shaders + the generic VS fallback. This is the memory playbook's
  explicit recommendation for reopening at x64 ([[project_hard05_d3dcompile_deferred_to_x64]]).

### Asm path scope
- **D-03:** Port the `//asm` `.vsh` path (`D3DXAssembleShader` :567) → `D3DAssemble` **this phase
  too** — full no-shortcuts; take the entire compile surface off D3DX. **HARD RISK / census-gated:**
  there is NO existence-proof `D3DAssemble` accepts the SWG asm dialect (Restoration's renderer never
  calls it). The SC#1 census MUST validate a representative asm-shader sample compiles + renders clean
  under `D3DAssemble` BEFORE the phase commits. **Fallback if the dialect breaks:** keep `//asm` on
  `D3DXAssembleShader` + the Fix-A SEH guard and split the asm port to its own follow-up phase
  (SC#3 explicitly sanctions retaining Fix-A "until the asm path is also off D3DX").

### D3DX removal boundary + Fix-A retirement
- **D-04:** Phase 32 removes D3DX from the **compile path only** (HLSL + asm), per SC#2's scoping
  ("no D3DX dependency in the shader-compile path"). The **Fix-A SEH guard drops entirely** once BOTH
  HLSL and asm paths are off D3DX and render clean (if the asm fallback is taken, Fix-A stays for the
  asm path only).
- **D-05 (incremental-removal direction — Claude's call, user deferred):** The non-compile D3DX
  (mesh/matrix/surface) is left linked this phase but is on the path to full removal via **own /
  DirectXMath implementations, NOT the redist d3dx9 DLL**. Best incremental sequence to "D3DX out
  eventually": (1) Phase 32 clears the compile path — the real x64 blocker and the gl11-validated
  piece; (2) a dedicated follow-up converts `D3DXMATRIX`/`D3DXMatrixMultiply`/`D3DXMatrixTranspose`
  → `DirectXMath` (header-only, x64-native, near-trivial) and the mesh/surface helpers
  (`D3DXMesh`, `D3DXLoadSurfaceFromSurface`, `D3DXCreateMeshFVF`, `D3DXSaveTextureToFile`, …) to own
  code — landing BEFORE the x64 link in Phase 33 meets them. Rationale for not folding it into
  Phase 32: bundling a ~15-API non-shader refactor into a shader phase is exactly the scope-mixing
  that got Phase 27 reverted.

### Census & validation
- **D-06:** The asm/HLSL census (SC#1) is the **critical early de-risk gate** — runs FIRST, refreshes
  the Phase-27 census framing (190 `//hlsl` + 96 `//asm` of 286 `.vsh`), and must confirm `D3DAssemble`
  viability on the SWG asm dialect before committing the asm port. No VS may be silently nulled (no
  skipped draws).
- **D-07:** Validation = runtime **dual-renderer boot smoke** (`rasterMajor=5` AND `=11`, both still
  boot to character select and render) + the **Tatooine bump/dot3 spot** (the Fix-A reference scene,
  db83b0f5c). No automated shader-test framework exists; builds/boots/renders are the truth.

### Claude's Discretion
- D-05 incremental removal sequencing for non-compile D3DX (user: "I defer to you, but we want to get
  D3DX out eventually, you pick the best incremental path to that goal").
- Census methodology / refresh-vs-redo of the Phase-27 artifact framing.
- Which gl11 rewrite rules map to which failing shaders (let research/census drive the exact mapping,
  but the *intent* is wholesale lift per D-02).

### Folded Todos
- **Port D3D9 shader compile from D3DXCompileShader to D3DCompile (Fix B)**
  (`.planning/todos/.../2026-05-31-port-d3d9-shader-compile-to-d3dcompile.md`, score 0.6) — this IS
  Phase 32's core work (the Fix-B that supersedes the Phase-19/Fix-A SEH guard). Folded into scope.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope & requirements
- `.planning/ROADMAP.md` §"Phase 32: D3DX to d3dcompiler_47" — goal + 3 success criteria
- `.planning/REQUIREMENTS.md` — **SHADER-01** (the carry-forward HARD-05; locked compile-port intent)

### Prior-attempt history (MUST read — this phase reopens reverted work)
- `.planning/phases/27-d3dcompile-port/` — Phase 27 dir (artifacts were reverted; framing/inputs only)
- Revert commit `c0f890875` — the reverted Phase 27 D3DCompile swap (what NOT to repeat: minimal recompile)
- Memory `project_hard05_d3dcompile_deferred_to_x64` — WHY Phase 27 reverted + the reopen playbook
  ("lift gl11's FULL treatment, not just the rewrite")
- Memory `project_phase27_d3dcompile_port_scope` — the D-01-R in-place swap mechanics
  (`ID3DXInclude`→`ID3DInclude` w/ cache, `D3DXMACRO`→`D3D_SHADER_MACRO`, `ID3DXBuffer`→`ID3DBlob`,
  `D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY`)
- Memory `project_d3d9_d3dxcompileshader_fp_crash` — Fix-A SEH guard (db83b0f5c) being retired here
- Memory `reference_restoration_binaries_intel` — the rejected redist route + x64 D3DX reality
  (no x64 static D3DX; explicit `: register(cN)` shader contract de-risks same-layout recompile)

### Implementation target & reference
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp` — the port target
  (HLSL :101, asm :567, Fix-A SEH guard ~:482)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_HlslRewrite.*` — gl11 rewrite rules
  A/B/C to LIFT (the D-02 source of truth)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexShaderData.cpp` (~:756) —
  gl11's `DECLARE_textureCoordinateSets`-without-register handling + cbuffer/register + VS fallback
- `stage/override/` — gl11's reauthored override shaders (tfcl/tfcsl) — pattern to lift
- `src/external/3rd/library/directx9/lib/d3dcompiler_47.dll` (+ `.lib`) — tracked, already staged
- Parked branch `d3d9-fixb-d3dcompile-wip` (origin too) — prior WIP, an input not a baseline

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **gl11's entire Phase-11 shader-modernization stack** (`Direct3d11_HlslRewrite`,
  `Direct3d11_VertexShaderData` register/cbuffer/fallback handling, `stage/override/` shaders) — the
  D-02 lift source. The same legacy-HLSL→strict-compiler problems gl11 solved are what gl05/gl07 face.
- **Tracked `d3dcompiler_47.dll` + `d3dcompiler.lib`** — already staged from Phase 27; no procurement.
- **Phase-27 census framing** (190 //hlsl + 96 //asm of 286 .vsh) and A/B baseline framing.
- **Fix-A SEH guard** (`Direct3d9_VertexShaderData.cpp` ~:482) — to be removed once both paths off D3DX.

### Established Patterns
- Runtime D3D9 vertex-shader compile is **single-file, two-path** (HLSL + asm). D3D9 **pixel** shaders
  do NOT runtime-compile (pre-built PEXE) — out of scope.
- Shader overrides ship as loose files in `stage/override/` + a `client.cfg` searchPath entry.
- Shared-header ABI trap: if a shared header consumed by plugins changes, rebuild ALL plugin
  .vcxprojs (gl05/06/07/11) — see AGENTS.md.

### Integration Points
- `Direct3d9_VertexShaderData.cpp` compile call sites :101 / :567 — the surgical change surface.
- Build link/include path — D3DX removal from the compile path; `d3dcompiler.lib` add (already linked).
- `stage/` postbuild auto-deploy (gl0X_d.dll + SwgClient_*.exe) — validation runs from `stage/`.

</code_context>

<specifics>
## Specific Ideas

- "Restoration took shortcuts I don't want to take" — the guiding value for the whole phase: prefer the
  clean real port (D3DCompile/D3DAssemble + own-impl D3DX removal) over Restoration's redist-d3dx9
  shortcut, even at higher effort/risk.
- "We want to get D3DX out eventually — pick the best incremental path to that goal" — the directive
  behind D-05's staged removal (compile path now → DirectXMath/own-impl follow-up → zero D3DX before
  x64 link).

</specifics>

<deferred>
## Deferred Ideas

- **Non-compile D3DX removal (mesh/matrix/surface, ~15 APIs)** — convert `D3DXMATRIX` /
  `D3DXMatrixMultiply` / `D3DXMatrixTranspose` → `DirectXMath` (header-only, x64-native) and the
  mesh/surface helpers (`D3DXMesh`, `D3DXMESHOPT`, `D3DXLoadSurfaceFromSurface`, `D3DXCreateMeshFVF`,
  `D3DXSaveTextureToFile`, `D3DXSaveSurfaceToFile`) to own code. **Own-impl, NOT redist.** A dedicated
  follow-up phase that must land BEFORE the Phase-33 x64 link meets these symbols (no x64 static D3DX
  exists). → flag for ROADMAP (likely a new phase between 32 and 33, or explicit Phase-33 sub-scope).
- **Asm-port fallback split** — if the SC#1 census shows `D3DAssemble` cannot handle the SWG asm
  dialect, the `//asm` path stays on `D3DXAssembleShader` + Fix-A this phase, and the asm→`D3DAssemble`
  port becomes its own follow-up phase. (Contingency, not yet a committed deferral.)

### Reviewed Todos (not folded)
- `2026-06-13-64bit-x64-port.md` (score 0.6) — the parent x64 milestone, spans Phases 33-36; this
  phase is one prerequisite, not the x64 port itself. Not folded.
- `2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md` (score 0.6) — space-scene asset diff,
  informational/data-content; unrelated to the compile-path port. Not folded.

</deferred>

---

*Phase: 32-d3dx-to-d3dcompiler-47*
*Context gathered: 2026-06-16*
