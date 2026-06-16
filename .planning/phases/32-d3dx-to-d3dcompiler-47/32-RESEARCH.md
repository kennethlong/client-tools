# Phase 32: D3DX to d3dcompiler_47 - Research

**Researched:** 2026-06-16
**Domain:** D3D9 runtime shader-compile path (HLSL `D3DCompile` + asm `D3DAssemble`), D3DX removal, dual-renderer boot validation
**Confidence:** HIGH (codebase-verified) for the in-tree facts; HIGH for D3DAssemble export presence (dumpbin-verified); MEDIUM for D3DAssemble *dialect acceptance* (census gate exists precisely because this is not provable without a runtime test)

## Summary

Phase 32 takes the D3D9 runtime vertex-shader compile surface — a single file,
`Direct3d9_VertexShaderData.cpp`, with two call sites (HLSL `D3DXCompileShader` :477, asm
`D3DXAssembleShader` :567) — off the x64-hostile D3DX library and onto `d3dcompiler_47`
(`D3DCompile` / `D3DAssemble`). Phase 27 already attempted the *minimal* same-profile recompile
and was reverted (`c0f890875`) because `d3dcompiler_47` is strict where 2003-era `D3DXCompileShader`
was lenient: it reserves `point`/`line`/`triangle` as SM4+ keywords and rejects stacked
`: register()` bindings on struct members **regardless of target profile** (even at `vs_2_0`). The
locked decision (D-02) is to lift gl11's **full** Phase-11 shader-modernization treatment, not the
minimal recompile.

The single highest-value discovery: **the prior-WIP branch `d3d9-fixb-d3dcompile-wip` already
contains a complete, compiling port of the HLSL path** — a `Direct3d9_HlslRewrite.{h,cpp}`
(re-namespaced copy of the battle-tested `Direct3d11_HlslRewrite`, Rules A-F intact),
`D3DXCompileShader`→`D3DCompile` via reinterpret-cast at the call boundary (the D3DX/d3dcompiler
ABI is identical for `D3DXMACRO`/`ID3DXInclude`/`ID3DXBuffer`), the `register(vN)` omission in the
engine-injected `DECLARE_textureCoordinateSets` macro, and `D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY`.
That branch is "compiles, render-incomplete" per its own commit message — it is the *baseline to
finish*, not to re-derive. The asm path on that branch was left entirely on `D3DXAssembleShader`.

The second discovery de-risks D-03: **`D3DAssemble` IS exported by `d3dcompiler_47.dll`** (ordinal 1,
dumpbin-verified) and **the import symbol IS in `d3dcompiler.lib`** with signature
`HRESULT D3DAssemble(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, UINT, ID3DBlob**, ID3DBlob**)`.
However, `D3DAssemble` is **NOT declared in the public `d3dcompiler.h`** — the planner must declare
the prototype manually. Whether it accepts the SWG `.vsh` asm *dialect* (`TARGET` macro → `vs_1_1`/
`vs_2_0`, `dcl_position0`, `m4x4`, `oPos`/`oT0`, `v#`/`c#`/`b#` register aliases via `#include`) is
unprovable from static analysis — hence the D-06 census gate runs a representative sample through
`D3DAssemble` FIRST.

**Primary recommendation:** Resume from `d3d9-fixb-d3dcompile-wip` as the HLSL-path baseline (cherry-pick
or re-derive its `Direct3d9_HlslRewrite` + `D3DCompile` swap), finish it to *render-clean* (the WIP
stopped at "compiles"), and gate the asm→`D3DAssemble` port on a Wave-0 census probe that assembles 2-3
representative `.vsh` (`c_simple`, `tfcl`, `cloudlayer`) and renders them. If the dialect breaks, take
the D-03 fallback: asm stays on `D3DXAssembleShader` + Fix-A, split to a follow-up phase.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Port `D3DXCompileShader` → `D3DCompile` (`d3dcompiler_47`). Honor SHADER-01 as written.
  Explicitly **reject** Restoration's redist-d3dx9 shortcut (swap static `d3dx9.lib` → redist
  `d3dx9_NN.dll` + ship the DLL). The clean port is the goal even though Phase 27 proved it re-fights
  the gl11 HLSL battle. Higher-effort, higher-risk-of-revert path, chosen with eyes open.
- **D-02:** Lift gl11's **full** shader-modernization treatment into the gl05/gl07 `D3DCompile` path
  — NOT the minimal same-profile recompile (failed/reverted in Phase 27). Full treatment =
  `Direct3d11_HlslRewrite` rules A/B/C + `DECLARE_textureCoordinateSets`-without-register +
  cbuffer/register handling + `stage/override/` reauthored shaders + the generic VS fallback.
- **D-03:** Port the `//asm` `.vsh` path (`D3DXAssembleShader` :567) → `D3DAssemble` this phase too —
  full no-shortcuts. **HARD RISK / census-gated:** NO existence-proof `D3DAssemble` accepts the SWG asm
  dialect (Restoration never calls it). SC#1 census MUST validate a representative asm sample compiles +
  renders clean under `D3DAssemble` BEFORE committing the asm port. **Fallback if dialect breaks:** keep
  `//asm` on `D3DXAssembleShader` + Fix-A and split the asm port to its own follow-up phase.
- **D-04:** Phase 32 removes D3DX from the **compile path only** (HLSL + asm), per SC#2. The **Fix-A SEH
  guard drops entirely** once BOTH HLSL and asm paths are off D3DX and render clean (if the asm fallback
  is taken, Fix-A stays for the asm path only).
- **D-05 (Claude's call, user deferred):** The non-compile D3DX (mesh/matrix/surface) is left linked this
  phase but on the path to full removal via **own / DirectXMath implementations, NOT the redist d3dx9 DLL**.
  Sequence: (1) Phase 32 clears the compile path; (2) a dedicated follow-up converts
  `D3DXMATRIX`/`D3DXMatrixMultiply`/`D3DXMatrixTranspose` → `DirectXMath` and the mesh/surface helpers to
  own code — landing BEFORE the Phase-33 x64 link meets them. Do NOT fold this ~15-API non-shader refactor
  into Phase 32 (that scope-mixing got Phase 27 reverted).
- **D-06:** The asm/HLSL census (SC#1) is the **critical early de-risk gate** — runs FIRST, refreshes the
  Phase-27 census framing (190 `//hlsl` + 96 `//asm` of 286 `.vsh`), confirms `D3DAssemble` viability
  before committing the asm port. No VS may be silently nulled (no skipped draws).
- **D-07:** Validation = runtime **dual-renderer boot smoke** (`rasterMajor=5` AND `=11`, both boot to
  character select and render) + the **Tatooine bump/dot3 spot** (Fix-A reference scene, db83b0f5c). No
  automated shader-test framework exists; builds/boots/renders are the truth.

### Claude's Discretion
- D-05 incremental removal sequencing for non-compile D3DX (user: "I defer to you, but we want to get
  D3DX out eventually, you pick the best incremental path to that goal").
- Census methodology / refresh-vs-redo of the Phase-27 artifact framing.
- Which gl11 rewrite rules map to which failing shaders (let research/census drive the exact mapping,
  but the *intent* is wholesale lift per D-02).

### Deferred Ideas (OUT OF SCOPE)
- **Non-compile D3DX removal (mesh/matrix/surface, ~15 APIs)** — convert to DirectXMath/own-impl
  (NOT redist). A dedicated follow-up phase that must land BEFORE the Phase-33 x64 link. Flag for ROADMAP.
- **Asm-port fallback split** — if SC#1 census shows `D3DAssemble` can't handle the SWG asm dialect, the
  `//asm` path stays on `D3DXAssembleShader` + Fix-A this phase, asm→`D3DAssemble` becomes its own
  follow-up. (Contingency, not yet a committed deferral.)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SHADER-01 | The legacy D3DX shader-compile path is ported to `d3dcompiler_47` (`D3DCompile`) and D3DX is removed from the build (x64-hostile); both renderers compile/load their shaders correctly. (Carries v2.3-deferred HARD-05; Fix-A retained until the asm path is also off D3DX.) | HLSL port mechanics (Code Examples §1-2), gl11 rewrite-rule inventory (Architecture Patterns), `D3DAssemble` export verified + signature (Code Examples §3), D3DX-removal verification commands (Don't Hand-Roll + Pitfalls), dual-renderer validation (Validation Architecture) |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| HLSL `.vsh` runtime compile (`//hlsl`) | gl05/gl07 plugin (`Direct3d9_VertexShaderData.cpp` :477) | shared HLSL rewriter (`Direct3d9_HlslRewrite`) | The compile call site lives in the D3D9 renderer plugin; the textual modernization is shared utility logic |
| Asm `.vsh` runtime compile (`//asm`) | gl05/gl07 plugin (`Direct3d9_VertexShaderData.cpp` :567) | `D3DAssemble` (d3dcompiler_47) | D3D9 vertex-shader assembly; assembler is a runtime API call |
| `#include` resolution (TRE-archived `.inc`) | gl05/gl07 plugin (`IncludeHandler::Open` :144) | `TreeFile` (sharedFile) | Engine already routes includes through TreeFile, not Win32 FS |
| Reauthored `vs_2_0` HLSL overrides | `stage/override/vertex_program/*.vsh` (loose files) | `client.cfg` searchPath | gl11 already ships these; they shadow the TRE `//asm` originals for the HLSL path |
| Compiled bytecode → `IDirect3DVertexShader9` | gl05/gl07 plugin (`CreateVertexShader` :573) | D3D9 device | Unchanged — bytecode output is identical D3D9 token stream |
| Non-compile D3DX (mesh/matrix/surface) | gl05/gl07 plugin (`Direct3d9.cpp`, `_RenderTarget.cpp`, `_TextureData.cpp`) | — | OUT OF SCOPE (D-05); stays linked this phase |

**Note:** gl06 (`Direct3d9_ffp.vcxproj`) does **NOT** define the `VSPS` macro, so it does NOT compile
`Direct3d9_VertexShaderData.cpp` and has no runtime VS compile path. Only **gl05** (`Direct3d9.vcxproj`)
and **gl07** (`Direct3d9_vsps.vcxproj`) define `VSPS` — these are the two targets in scope. [VERIFIED: grep of vcxproj PreprocessorDefinitions]

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `d3dcompiler_47.dll` | v47 (D3D_COMPILER_VERSION 47) | `D3DCompile` (HLSL) + `D3DAssemble` (asm), x64-clean | The modern, x64-native HLSL compiler; gl11 has used it since Phase 11. [VERIFIED: dumpbin /exports — ordinals 1 (D3DAssemble), 4 (D3DCompile)] |
| `d3dcompiler.lib` | matches WK 10.0.19041.0 | Import lib for the above | Already in `AdditionalDependencies` of gl05/gl07 vcxproj; resolves from Windows Kit `Lib/.../um/x86/`. [VERIFIED: vcxproj grep + WK x86 lib exists] |
| `Direct3d11_HlslRewrite` (logic) | in-tree, Phase 11 | Rules A/B/C/D/E/F textual rewrite — the D-02 lift source | Battle-tested over 13+ iterations against the exact legacy-HLSL→strict-compiler problems gl05 now faces. [VERIFIED: read of .h + .cpp] |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `Direct3d9_HlslRewrite.{h,cpp}` | on branch `d3d9-fixb-d3dcompile-wip` | Re-namespaced copy of the gl11 rewriter (Rules A-F), already created | Cherry-pick / re-derive as the gl05/gl07 rewriter. [VERIFIED: `git ls-tree d3d9-fixb-d3dcompile-wip`] |
| `d3dx9.lib` | tracked (`directx9/lib/`) | Non-compile D3DX (mesh/matrix/surface) | STAYS linked this phase (D-05); only `D3DXCompileShader`/`D3DXAssembleShader` leave |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `D3DCompile` clean port (D-01) | Restoration's redist `d3dx9_NN.dll` swap | **REJECTED by D-01.** Lower effort but keeps an x64-shaped D3DX dependency the project explicitly does not want. Do not propose. |
| `D3DAssemble` for asm (D-03) | Keep `D3DXAssembleShader` + Fix-A | The **sanctioned fallback** if the census gate fails. Not a free choice — only on dialect-break. |
| Lift gl11 full treatment (D-02) | Minimal same-profile recompile | **FAILED in Phase 27, reverted (`c0f890875`). BANNED by D-02.** Do not re-derive. |

**Installation:** No procurement. `d3dcompiler_47.dll` + `d3dcompiler.lib` already staged/linked from
Phase 27. The d3dcompiler.lib link is currently an unused import (left in by the Phase-27 revert); this
phase makes it live.

**Version verification:** [VERIFIED 2026-06-16] `d3dcompiler_47.dll` present at
`src/external/3rd/library/directx9/lib/d3dcompiler_47.dll` (4,147,664 bytes, dated Jun 14). Public header
`C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um/d3dcompiler.h` declares `D3D_COMPILER_VERSION 47`.
The `directx9/include/` tree has **NO `d3dcompiler.h`** — the WK header is used (gl05/gl07 vcxproj lists
`directx9/include` first, but it falls through to the WK for d3dcompiler.h). [VERIFIED: ls + grep]

## Architecture Patterns

### System Architecture Diagram

```
                    Engine (clientGraphics)
                            │  ShaderImplementation::Pass::VertexShader::m_text
                            ▼
         ┌─────────────────────────────────────────────────────┐
         │  Direct3d9_VertexShaderData (gl05/gl07, VSPS-gated)   │
         │                                                       │
         │  ctor: parse first lines → m_hlsl / assembly flag,    │
         │        collect textureCoordinateSet tags              │
         │                            │                          │
         │              ┌─────────────┴─────────────┐            │
         │       m_hlsl │                            │ //asm      │
         │              ▼                            ▼            │
         │   createVertexShader (HLSL :398)   createVertexShader  │
         │   - build D3DXMACRO defines        (asm :496)          │
         │   - DECLARE_textureCoordinateSets  - vTextureCoord     │
         │     (currently emits :register(vN))  defines + TARGET  │
         │              │                            │            │
         │   ┌──────────▼───────────┐                │            │
         │   │ NEW: apply rewrite   │                │            │
         │   │ (Rules A/B/C/D/E/F)  │                │            │
         │   │ to main src + each   │                │            │
         │   │ #include (handler)   │                │            │
         │   └──────────┬───────────┘                │            │
         │              ▼                            ▼            │
         │   D3DXCompileShader ──► D3DCompile   D3DXAssembleShader │
         │   (Fix-A SEH today)     (NEW, FixB)  ──► D3DAssemble    │
         │              │                          (NEW, census-  │
         │              │                           gated; else   │
         │              │                           keep D3DX+FixA)│
         │              ▼                            ▼            │
         │        ID3DBlob bytecode ◄────────────────┘            │
         │              │                                         │
         └──────────────┼─────────────────────────────────────────┘
                        ▼
            IDirect3DVertexShader9 (CreateVertexShader :573 — unchanged)

   #include "vertex_program/modules/*.inc"  ──► IncludeHandler::Open
                                                 ──► TreeFile::open (TRE-archived)
                                                 ──► (NEW) rewrite before return

   stage/override/vertex_program/*.vsh (loose //hlsl vs_2_0 reauthored overrides)
        shadow the TRE //asm originals via client.cfg searchPath
        (the gl11 pattern; tfcl/tfcsl/c_simple/envmask_specmap_c already exist there)
```

### Recommended Work Structure
```
src/engine/client/application/Direct3d9/src/win32/
├── Direct3d9_VertexShaderData.cpp   # port target — both call sites
├── Direct3d9_HlslRewrite.h          # NEW (re-namespaced from gl11; on WIP branch)
└── Direct3d9_HlslRewrite.cpp        # NEW (Rules A-F)
src/engine/client/application/Direct3d9/build/win32/
├── Direct3d9.vcxproj                # gl05 — add HlslRewrite.cpp to ClCompile; drop d3dx9.lib? NO (D-05)
└── Direct3d9_vsps.vcxproj           # gl07 — same
stage/override/vertex_program/       # reauthored vs_2_0 overrides (already present)
```

### Pattern 1: D3DX→d3dcompiler ABI-compatible reinterpret at the call boundary
**What:** `D3DXMACRO`, `ID3DXInclude`, `ID3DXBuffer` are binary-layout-identical to their d3dcompiler
equivalents (`D3D_SHADER_MACRO`, `ID3DInclude`, `ID3DBlob`). The surrounding D3DX-typed code stays; only
the call boundary reinterpret-casts.
**When to use:** The HLSL `D3DCompile` swap — minimizes diff to a single call site (this is the WIP
branch's approach).
**Example:** see Code Examples §1.
**Caveat:** This is exactly what the WIP branch did; it *compiled* but rendered incomplete. The
reinterpret-cast is correct; the render-incompleteness is from the rewrite/register-semantics, not the ABI.

### Pattern 2: gl11's textual-rewrite ruleset (the D-02 lift)
The full ruleset from `Direct3d11_HlslRewrite.cpp` [VERIFIED: read]. Each rule and what it transforms:

- **Rule A — SM4+ reserved-keyword whole-word rename.** Matches whole-word `point`, `line`, `triangle`,
  `lineadj`, `triangleadj` → appends `_id`. SWG's D3D9-era HLSL uses these as ordinary identifiers; the
  modern lexer reserves them (GS primitive types) and rejects with X3000 *before* any profile gating.
  This is the FIRST error gl05 hits (X3000 `point` in `vertex_shader_constants.inc`).
- **Rule B — `:\s*[bcstuv]\d+\b` → match-length spaces.** Strips legacy struct-member register-binding
  shortcut (`: c4`, `: v0`). **Context-aware (Iter-11):** only fires on a SECOND-or-later `:` on the same
  logical line (the stacked-semantic struct-member shape `: POSITION0 : c4`). On the FIRST `:` of a line
  (a legal global register binding), it no-ops. Emits spaces to preserve column offsets for error messages.
- **Rule C — `:\s*register\s*\(\s*[bcstuv]\d+\s*\)` → match-length spaces.** Same as B for the explicit
  `register(...)` form. Tried BEFORE B (longer/more-specific match at the same `:`). Same context guard.
  This is the X3202 class (`location semantics cannot be specified on members`) — the second error gl05
  hits, from the engine-injected `DECLARE_textureCoordinateSets` macro's `: register(vN)`.
- **Rule D — structural cbuffer wrap (pre-pass, include-handler only).** Detects the contiguous run of
  GLOBAL `float<...> name : register(cN);` declarations in `vertex_shader_constants.inc` and wraps them in
  `cbuffer SwgVertexConstants : register(b0) { ... }`, converting each `register(cN)` → `packoffset(cN)`.
  Sidesteps X4016 (`overlapping register semantics 'c0'`). **D3D11-SPECIFIC — likely NOT needed for D3D9.**
  D3D9's `vs_2_0`/`vs_1_1` profiles DO honor `register(cN)` at file scope (that's the native D3D9 constant
  model); the X4016 only arises under SM4+ `$Globals` auto-promotion. The planner should test whether
  Rule D is needed at all for the D3D9 path (likely skip it).
- **Rule E — `#pragma def(vs, c95, ...)` line strip (pre-pass).** D3D9-era explicit float-register
  pre-fill. Under SM4+ FXC emits its own c#-mappings and the pragma collides → X4016. **D3D9-SPECIFIC
  QUESTION:** under `vs_2_0` the `def` instruction *is* native D3D9 — `#pragma def` may be legitimate.
  Census must determine whether D3DCompile-at-vs_2_0 accepts or rejects `#pragma def`. (registers.inc maps
  `c0_0`→`c95.x` etc., and the shaders reference these — if the pragma is stripped, those constants must
  come from elsewhere, matching the existing D3D9 runtime behavior.)
- **Rule F — specular `pow()` NaN guard (pre-pass, main-source/PS only).** Wraps `pow(saturate(dot(...)),
  materialSpecularPower)` against NaN when power==0. **D3D11-SPECIFIC — the D3D9 path runs the original
  bytecode and does NOT hit this NaN** (the comment in HlslRewrite.cpp:1016 says so explicitly: "D3D9
  runs the original ps_2_0 bytecode (pow(x,0)~0, no NaN); only the D3D11 recompile hits it"). For the
  D3D9 vertex path, Rule F is almost certainly inert — but it is also harmless (no match in VS sources).

**Net for D3D9:** Rules A, B, C are the load-bearing ones (the Phase-27 revert cascade was exactly
A→B/C). Rules D, E, F are D3D11-specific compensations for SM4+ `$Globals` promotion and are likely
inert or unnecessary at the `vs_2_0`/`vs_1_1` D3D9 target — **but the WIP branch copied all six verbatim
and it compiled.** The planner should start with the verbatim copy (proven to compile) and let the
census/render smoke determine if D/E need D3D9-specific divergence.

### Pattern 3: register(vN) omission in the engine-injected macro
The engine builds `DECLARE_textureCoordinateSets` at runtime (`createVertexShader` :433-462) with
`: register(v<N>)` on each struct member. gl11 omits the register entirely (`Direct3d11_VertexShaderData.cpp`
:751-790 — emits `: TEXCOORD<i>` only, no `: register`). The WIP branch made the same edit for D3D9
(`SCRATCH_STRING(";", 1)` instead of `: register(v...)`). **For D3D9 the question is whether dropping
`register(vN)` changes input binding** — under D3D9 the vertex declaration (`VSVR_textureCoordinateSet0 + i`)
drives the binding; the inline `register(vN)` may be load-bearing for D3D9 in a way it is not for D3D11
(where the input layout drives it). This is a render-correctness risk to test in the smoke. [VERIFIED:
read of both files + WIP diff]

### Anti-Patterns to Avoid
- **Minimal same-profile recompile (Phase 27).** BANNED by D-02. It hits the A→B/C cascade one error at a
  time and gets reverted.
- **Removing `d3dx9.lib` from the link this phase.** D-05/D-04: the non-compile D3DX (mesh/matrix/surface)
  STAYS. Only the compile-path D3DX *calls* leave. Pulling the lib breaks `Direct3d9.cpp`,
  `Direct3d9_RenderTarget.cpp`, `Direct3d9_TextureData.cpp`. [VERIFIED: grep — those 3 files use D3DXMesh/
  D3DXLoadSurfaceFromSurface/D3DXMatrixMultiply]
- **Stripping `register(cN)` on global declarations (over-strip).** The exact bug gl11's Iter-11
  context-aware rule fixed (X4016). If you port Rules B/C, port the `sawColonThisLine` guard with them.
- **Assuming `#pragma def` / Rule D/E behave identically on D3D9.** They were designed for SM4+ semantics.
  Verify against the `vs_2_0` target, do not blindly trust the D3D11 rationale.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Legacy-HLSL→strict-compiler textual rewrite | A new from-scratch rewriter | gl11's `Direct3d11_HlslRewrite` (or the WIP `Direct3d9_HlslRewrite` copy of it) | 13+ iterations of hard-won edge cases (context-aware register strip, column preservation, two-pass sizing, X4016 cbuffer wrap). Re-deriving = re-discovering Iter-7..13 bugs. |
| D3D9 asm assembler | A hand-rolled token assembler | `D3DAssemble` (d3dcompiler_47, ordinal 1) | The DLL already ships it; declaring + calling it is far less work than parsing `m4x4`/`dcl_*`/`oPos`. **Census-gate first** (D-03). |
| `#include` resolution for TRE-archived `.inc` | Win32 FS include handler | Existing `IncludeHandler::Open` → `TreeFile::open` | Already routes through the TRE virtual filesystem; D3DCompile's default `D3D_COMPILE_STANDARD_FILE_INCLUDE` would fail X1507 on TRE-resident headers (gl11 hit this in Iter-2). |
| D3DX→d3dcompiler type translation | Rewriting all D3DX-typed code | `reinterpret_cast` at the single call boundary (ABI-identical structs) | `D3DXMACRO`≡`D3D_SHADER_MACRO`, `ID3DXInclude`≡`ID3DInclude`, `ID3DXBuffer`≡`ID3DBlob`. The WIP branch proves this compiles. |

**Key insight:** The whole HLSL half of this phase is "finish the WIP branch," not "build new." The WIP
branch stalled at *render-incomplete*, so the genuinely new engineering is (a) making the rewritten HLSL
render *correctly* (register-semantics / texcoord-binding correctness), and (b) the census-gated asm port.

## Runtime State Inventory

> This is a code-and-build refactor with one runtime-data dimension: the shader source corpus lives in
> TRE archives + `stage/override/`, not in the repo.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | `.vsh`/`.inc` shader sources are inside TRE archives at `D:/Code/SWGSource Client v3.0/*.tre` (the client loads them via TreeFile). Loose reauthored `vs_2_0` HLSL overrides live in `stage/override/vertex_program/` (9 files: tfcl*, tfcsl*, c_simple, envmask_specmap_c) and shadow the TRE `//asm` originals. | No data migration. The rewrite is runtime/in-memory — the on-disk `.vsh` are unchanged. Census reads them via the extracted copies in `.planning/research/vsh-extract/`. |
| Live service config | None — no external service. | None. |
| OS-registered state | None. | None. |
| Secrets/env vars | None. Build uses `$env:MSBUILD` (already in `.claude/settings.json`). | None. |
| Build artifacts | `stage/gl05_d.dll` / `gl07_d.dll` (+ `_r`), `stage/d3dcompiler_47.dll` (auto-copied by postbuild). Shared-header ABI trap if any plugin-shared header changes (it should NOT here — the change is plugin-internal). | Rebuild gl05 + gl07 (`Direct3d9` + `Direct3d9_vsps` targets) + relink SwgClient. Postbuild auto-deploys to `stage/`. |

**The census corpus is already extracted:** `.planning/research/vsh-extract/` contains 12 `.vsh` (11
`//asm`, 1 `//hlsl`) + the shared `.inc` modules (`registers.inc`, `transform.inc`,
`vertex_shader_constants.inc`, `diffuse.inc`, `fog.inc`, `lighting_fog_setup.inc`,
`texture_coordinates.inc`, `c_ambient.inc`, etc.). This is the Phase-27 census artifact, refreshable per
D-06. [VERIFIED: ls + read]

## Common Pitfalls

### Pitfall 1: The Phase-27 error cascade (one-error-at-a-time rediscovery)
**What goes wrong:** Porting `D3DXCompileShader`→`D3DCompile` minimally surfaces errors serially:
X3000 (`point` keyword) → fix Rule A → X3202 (`register(vN)` on member) → fix Rule B/C + macro → X4016
(global register over-strip) → fix context-aware guard → ... Each rebuild reveals only the next error.
**Why it happens:** Modern `d3dcompiler_47` validates strictly; 2003 `D3DXCompileShader` was lenient.
**How to avoid:** Lift gl11's COMPLETE ruleset up front (D-02), not incrementally. The WIP branch already
did this (Rules A-F copied verbatim) — start there. Sequence the work as "land the full rewriter, then
debug render-correctness," not "add one rule per iteration." [VERIFIED: revert commit c0f890875 documents
the exact cascade]
**Warning signs:** A plan with a task per error code (X3000 task, X3202 task, ...) — that IS the reverted
Phase-27 shape.

### Pitfall 2: `D3DAssemble` accepts the SWG asm dialect — unverified
**What goes wrong:** `D3DAssemble` exists and links, but may reject `TARGET` (macro), `m4x4`/`m4x3`/`m3x3`,
`dcl_position0`, `oPos`/`oT0` output registers, or the `#if VERTEX_SHADER_VERSION >= 20` / `b#` boolean
registers — the SWG `.vsh` asm is canonical D3D9 vs_1_1/vs_2_0 assembly, but `D3DAssemble`'s accepted
grammar is undocumented (it is not even in the public header).
**Why it happens:** Restoration never calls `D3DAssemble` (it precompiles bytecode), so there is no
existence proof. `D3DAssemble` is an internal/legacy export.
**How to avoid:** D-06 census gate — Wave 0 assembles `c_simple.vsh`, `tfcl.vsh`, `cloudlayer.vsh` (a
trivial, a typical, and a complex shader) through `D3DAssemble` with the same `#include`/`#define` macro
setup the runtime uses, and renders them. **Pass = bytecode produced + CreateVertexShader succeeds +
renders.** **Fail = D3DAssemble returns error / wrong bytecode** → take the D-03 fallback.
**Warning signs:** `D3DAssemble` returns `E_FAIL`/`D3DERR_*` with no error blob; or produces bytecode that
`CreateVertexShader` rejects.
[VERIFIED: D3DAssemble exported (dumpbin ordinal 1) + import symbol in d3dcompiler.lib; dialect-acceptance ASSUMED-untested]

### Pitfall 3: Dropping `register(vN)` / `register(cN)` breaks the C++ constant-register contract
**What goes wrong:** The engine sets constants by explicit register (`SetVertexShaderConstantF(N, ...)`
expecting the shader reads `cN`). If the rewrite strips the binding and D3DCompile's auto-allocator picks
different registers, the shader reads wrong constants → wrong colors / no geometry / NaN, but compiles +
draws cleanly (no FATAL). gl11 mitigated this with the cbuffer push (Plan 11-08); D3D9 has no cbuffer —
it relies on the explicit `register(cN)` being honored.
**Why it happens:** D3D9's constant model IS explicit-register; stripping the binding under a profile that
honors it (vs_2_0) may be wrong where it was right for D3D11. **This is the most likely cause of the WIP
branch's "render-incomplete."**
**How to avoid:** Be surgical. Strip `register()` ONLY where the modern compiler rejects it (struct-member
stacked semantics — Rule B/C context-aware guard already does this). PRESERVE global `register(cN)` (the
constant contract). Test the render explicitly — bytecode that compiles is not bytecode that renders right.
**Warning signs:** Client boots and draws but geometry is wrong-colored / missing / black at the Tatooine
bump/dot3 spot (the D-07 reference scene). [CITED: Direct3d11_HlslRewrite.h:118-130 semantic-loss caveat]

### Pitfall 4: `/FORCE` masks unresolved `D3DAssemble` / `D3DCompile`
**What goes wrong:** SwgClient links under `/FORCE` — an unresolved `D3DAssemble`/`D3DCompile` external
downgrades to a warning and still emits a binary that crashes at first shader compile.
**How to avoid:** After building, grep the build log for `unresolved external symbol` (must be 0). The
gl05/gl07 plugin DLLs themselves do NOT link under /FORCE (only SwgClient.exe does), so a missing
`D3DAssemble` import would be a hard `LNK2019` in the plugin — but confirm anyway. [CITED: AGENTS.md build section]
**Warning signs:** Build "succeeds" but client FATALs at boot on the first `.vsh` load.

### Pitfall 5: `.cfg` UTF-8 BOM crash during validation
**What goes wrong:** Toggling `rasterMajor` in `stage/client.cfg` via PowerShell `Set-Content`/`Out-File`
adds a UTF-8 BOM that crashes the RELEASE client at boot (Debug masks it).
**How to avoid:** Edit `.cfg` with the Edit/Write tools or bash, never PS `Set-Content`. Diagnose with
`head -c 8 stage/client.cfg | xxd` (clean = `23 20`, bad = `ef bb bf`). [CITED: AGENTS.md cfg-safety]

## Code Examples

### §1. HLSL: D3DXCompileShader → D3DCompile via ABI reinterpret (the WIP-branch approach)
```cpp
// Source: branch d3d9-fixb-d3dcompile-wip, Direct3d9_VertexShaderData.cpp [VERIFIED]
// D3DXMACRO / ID3DXInclude / ID3DXBuffer are binary-compatible with
// D3D_SHADER_MACRO / ID3DInclude / ID3DBlob (identical layouts), so the
// surrounding D3DX-typed code stays; reinterpret only at the call boundary.
#include <d3dcompiler.h>   // adds D3DCompile + D3DCOMPILE_* flags

HRESULT compileVertexShaderViaD3DCompile(
    LPCSTR src, UINT srcLen, D3DXMACRO const *defines, LPD3DXINCLUDE includeHandler,
    LPCSTR functionName, LPCSTR profile, DWORD flags,
    LPD3DXBUFFER *outShader, LPD3DXBUFFER *outErrors)
{
    return D3DCompile(
        src, srcLen, NULL,
        reinterpret_cast<D3D_SHADER_MACRO const *>(defines),
        reinterpret_cast<ID3DInclude *>(includeHandler),
        functionName, profile, flags, 0,
        reinterpret_cast<ID3DBlob **>(outShader),
        reinterpret_cast<ID3DBlob **>(outErrors));
}
// Call site: target is "vs_2_0" (>= SM2.0 GPU) or "vs_1_1" (createVertexShader:391).
// flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY (relaxes D3D9-era HLSL).
// Apply Direct3d9_HlslRewrite::applyToMainSource to m_compileText first;
// includes are rewritten in IncludeHandler::Open.
```

### §2. The DECLARE_textureCoordinateSets register-omission edit
```cpp
// Source: branch d3d9-fixb-d3dcompile-wip, createVertexShader [VERIFIED]
// BEFORE (master :453-457) — emits ": register(vN)" on the struct member:
//   SCRATCH_STRING(" : register(v", 13);
//   SCRATCH_INT(VSVR_textureCoordinateSet0 + i);
//   SCRATCH_STRING(");", 2);
// AFTER (WIP) — omit the register binding; TEXCOORD<i> semantic alone binds input:
//   SCRATCH_STRING(";", 1);
// RISK (Pitfall 3): verify this does not break D3D9 input binding — under D3D9 the
// vertex declaration drives it, but confirm in the render smoke.
```

### §3. Asm: D3DXAssembleShader → D3DAssemble (manual prototype — NOT in public header)
```cpp
// D3DAssemble is exported by d3dcompiler_47.dll (ordinal 1) and present in
// d3dcompiler.lib, but NOT declared in d3dcompiler.h. Declare it manually.
// Signature [VERIFIED: dumpbin /headers d3dcompiler.lib decorated symbol]:
//   ?D3DAssemble@@YAJPBXKPBDPBU_D3D_SHADER_MACRO@@PAUID3DInclude@@IPAPAUID3D10Blob@@4@Z
extern "C" HRESULT WINAPI D3DAssemble(
    LPCVOID                  pSrcData,
    SIZE_T                   SrcDataSize,
    LPCSTR                   pSourceName,
    const D3D_SHADER_MACRO  *pDefines,
    ID3DInclude             *pInclude,
    UINT                     Flags,        // D3D_DISASM_* family / 0 — see census
    ID3DBlob               **ppCode,
    ID3DBlob               **ppErrorMsgs);

// NOTE the decorated symbol is C++-mangled (?D3DAssemble@@YAJ...), NOT extern "C".
// The `extern "C"` above will produce an UNDECORATED import that will NOT match.
// Two options for the planner to test in Wave 0:
//   (a) declare WITHOUT extern "C" (C++ linkage) so the mangled name matches
//       d3dcompiler.lib's exported C++ symbol, OR
//   (b) GetProcAddress("D3DAssemble") from d3dcompiler_47.dll at runtime
//       (the DLL exports the UNDECORATED name "D3DAssemble" — dumpbin /exports
//        shows plain "D3DAssemble", so GetProcAddress works and sidesteps the
//        mangling question entirely).
// Recommendation: prefer (b) GetProcAddress for robustness — the export table
// name is undecorated, the .lib symbol is mangled; GetProcAddress avoids the
// mismatch and is what census Wave 0 should validate first.
```

### §4. SWG asm dialect (what D3DAssemble must accept)
```
// Source: .planning/research/vsh-extract/cloudlayer.vsh + registers.inc [VERIFIED]
//asm
TARGET                                   // macro → vs_1_1 or vs_2_0 (engine #defines it)
#include "vertex_program/modules/registers.inc"   // v#/c#/b# register aliases
dcl_position0 vPosition                  // vPosition #define'd to v0
dcl_normal0   vNormal                    //           v3
dcl_texcoord0 vTextureCoordinateSet0     //           v7
m4x4 oPos, vPosition, cObjectWorldCameraProjectionMatrix   // c0
m4x3 r9.xyz, vPosition, cObjectWorldMatrix                 // c4
m3x3 r10.xyz, vNormal, cObjectWorldMatrix
dp3 r10.w, r10, r10
rsq r10.w, r10.w
mul r10.xyz, r10, r10.w
mov oT0, vTextureCoordinateSetMAIN       // oT0 = texcoord output register
// registers.inc has: #if VERTEX_SHADER_VERSION >= 20 ... b0..b7 boolean regs ... #endif
```
This is textbook D3D9 SM1/SM2 vertex assembly. The open question is purely whether `D3DAssemble`'s
parser accepts it (vs. some restricted internal grammar). The census proves it or triggers the fallback.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `D3DXCompileShader` (d3dx9, static-compiled from source) | `D3DCompile` (d3dcompiler_47) | DX SDK → Windows SDK transition (~2012); gl11 adopted Phase 11 | D3DX deprecated, x64-hostile; d3dcompiler is the supported, x64-clean path |
| `D3DXAssembleShader` | `D3DAssemble` (d3dcompiler_47) | same | Exists but undocumented; dialect compatibility unproven for SWG asm |
| Fix-A SEH guard around D3DXCompileShader (db83b0f5c) | Removed once both paths off D3DX (D-04) | this phase | The SEH guard exists ONLY because static-compiled D3DX faulted FP under the modern toolchain; D3DCompile is immune |
| vs_2_0 / vs_1_1 explicit targets | still supported by D3DCompile | — | vs_2_0 confirmed supported [CITED: walbourn.github.io — "SM2.0/3.0 profiles work on all D3D9.0c platforms"]; vs_1_1 deprecated (D3DCompile dropped vs_1_1/ps_1_x per the WIP comment — ASSUMED, verify against the `getShaderCapability() < 2.0` branch) |

**Deprecated/outdated:**
- `D3DXCompileShader` / `D3DXAssembleShader` / the entire d3dx9 redist — Microsoft deprecated D3DX with
  the move to the Windows SDK. No x64 static D3DX exists. [CITED: reference_restoration_binaries_intel memory]
- The Fix-A SEH guard — a workaround for a D3DX-specific FP fault, obsolete once D3DX leaves the compile path.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `D3DAssemble` accepts the SWG `.vsh` asm dialect (`TARGET`/`m4x4`/`dcl_*`/`oPos`/`b#`) | Pitfall 2, Code §3-4 | HIGH — if wrong, the asm port is impossible this phase; triggers the D-03 fallback (keep D3DXAssembleShader + Fix-A, split to follow-up). This is precisely why D-06 mandates the census gate FIRST. |
| A2 | vs_1_1 is dropped by D3DCompile (only vs_2_0+ compiles) | State of the Art | MEDIUM — if vs_1_1 shaders exist that must compile and D3DCompile rejects vs_1_1, those shaders need vs_2_0 promotion or stay on D3DX. The runtime branches to vs_1_1 only when `getShaderCapability() < 2.0` (ancient GPUs — likely never on a modern test machine). Verify the actual target taken at runtime. |
| A3 | Rules D/E/F are inert/unnecessary at the D3D9 vs_2_0 target | Architecture Pattern 2 | LOW-MEDIUM — if Rule E (`#pragma def` strip) is wrongly applied, D3D9 shaders that legitimately use `def` lose constants. The WIP branch copied all six and compiled; render-correctness untested. Census/smoke resolves. |
| A4 | The 190 //hlsl + 96 //asm of 286 .vsh Phase-27 framing is still accurate | (census refresh) | LOW — D-06 mandates a census refresh anyway. The extracted corpus in vsh-extract/ is a sample (12 files), not the full 286; the full census enumerates the TRE corpus. |
| A5 | Dropping `register(vN)` from DECLARE_textureCoordinateSets is safe for D3D9 input binding | Pattern 3, Pitfall 3 | MEDIUM — likely the WIP branch's render-incompleteness root cause. Under D3D9 the vertex declaration drives binding, but the inline register may still matter. Test in the Tatooine smoke. |
| A6 | d3dcompiler.lib already in the link path makes D3DCompile/D3DAssemble link cleanly | Standard Stack | LOW — verified the lib is in AdditionalDependencies; the symbol is present. Only the C++-mangled-vs-undecorated question (Code §3) needs the Wave-0 GetProcAddress fallback. |

## Open Questions

1. **Does D3DAssemble accept the SWG asm dialect?** (A1)
   - What we know: D3DAssemble is exported (ordinal 1) + in d3dcompiler.lib (mangled C++ symbol);
     undecorated export name "D3DAssemble" works via GetProcAddress. SWG asm is canonical D3D9 vs_1_1/2_0.
   - What's unclear: whether D3DAssemble's parser accepts `TARGET`-expanded profiles, `m4x4`/`dcl_*`/`oPos`,
     `#if VERTEX_SHADER_VERSION >= 20` boolean registers, and the `#include`/`#define` macro setup.
   - Recommendation: **Wave 0 / first task** — a standalone probe that runs `c_simple`/`tfcl`/`cloudlayer`
     through D3DAssemble (via GetProcAddress) with the runtime macro/include setup, compares bytecode to
     the D3DXAssembleShader output, and renders. This IS the D-06 census gate.

2. **Is Rule E (`#pragma def`) handling correct at vs_2_0?** (A3)
   - What we know: Rule E strips `#pragma def(vs, c95, ...)`; under SM4+ it collides with FXC's allocator.
     `def` is a native D3D9 instruction; `vs_2_0` may accept the pragma.
   - What's unclear: whether D3DCompile-at-vs_2_0 accepts `#pragma def`, and whether the c95.x/y/z/w
     constants the shaders reference come from the pragma or elsewhere at runtime.
   - Recommendation: test with Rule E enabled (verbatim copy) first; if a shader misses the c95 constants,
     investigate whether the engine sets them via SetVertexShaderConstantF.

3. **What is the WIP branch's render-incompleteness root cause?** (A5)
   - What we know: branch compiles, renders incomplete; the register-omission edit (Pattern 3) is the prime
     suspect per the semantic-loss caveat.
   - Recommendation: bring the WIP branch to a bootable state, capture the Tatooine bump/dot3 spot under
     rasterMajor=5, and diff against the Fix-A reference (db83b0f5c). Isolate whether register-semantics or
     the rewrite is the cause.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| `d3dcompiler_47.dll` | D3DCompile + D3DAssemble | ✓ | v47 (4.1 MB, Jun 14) | — |
| `d3dcompiler.lib` (import) | link | ✓ | WK 10.0.19041.0 um/x86 | — |
| `d3dcompiler.h` (public header) | D3DCompile decls + flags | ✓ (WK only; declares no D3DAssemble) | v47 | manual D3DAssemble prototype / GetProcAddress |
| `$env:MSBUILD` (VS18/v145) | build gl05/gl07 | ✓ | per .claude/settings.json | — |
| Game data TREs (`.vsh` corpus) | runtime shader load + census | ✓ | `D:/Code/SWGSource Client v3.0/` | extracted sample in `.planning/research/vsh-extract/` |
| `dumpbin` (D3DX-removal verify) | SC#2 | ✓ | VS 2022 14.44 (needs PATH for deps) | — |
| Branch `d3d9-fixb-d3dcompile-wip` | HLSL-port baseline | ✓ | local + origin | — |

**Missing dependencies with no fallback:** None.
**Missing dependencies with fallback:** `D3DAssemble` declaration — not in public header; use a manual
prototype (C++ linkage to match the mangled lib symbol) or `GetProcAddress` (undecorated export name).

## Validation Architecture

> nyquist_validation is not disabled in config (commit_docs:true, no explicit false). The phase has NO
> automated shader-test framework (D-07 locks this) — validation is runtime boot+render smoke.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None automated (D-07: "builds/boots/renders are the truth") |
| Config file | `stage/client.cfg` (Release) / `stage/client_d.cfg` (Debug) — `rasterMajor` selects renderer |
| Quick run command | Boot `stage/SwgClient_d.exe` (reads `client_d.cfg`) to character select |
| Full suite command | Dual-renderer: boot under `rasterMajor=5` (gl05) AND `=11` (gl11), render the Tatooine bump/dot3 spot |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Validation | Exists? |
|--------|----------|-----------|------------|---------|
| SHADER-01 (HLSL) | D3DCompile compiles all `//hlsl` `.vsh`; client boots char-select | manual smoke | rasterMajor=5 boot to char-select + render | ✅ manual |
| SHADER-01 (asm census) | D3DAssemble assembles representative `//asm` sample → valid bytecode → renders | manual probe (Wave 0 gate) | assemble c_simple/tfcl/cloudlayer, CreateVertexShader, render | ❌ Wave 0 probe needed |
| SHADER-01 (no skipped draws) | No VS silently nulled | manual smoke | enable DEBUG_REPORT; verify no compile FATAL / no missing geometry | ✅ via DEBUG build |
| SHADER-01 (D3DX removed from compile path) | dumpbin/grep shows no D3DXCompileShader/D3DXAssembleShader/D3DXMACRO in the compile path | static check | grep source + dumpbin imports | ✅ command-based |
| SHADER-01 (both renderers) | gl05 AND gl11 both render | manual smoke | toggle rasterMajor 5↔11, render same scene | ✅ manual |
| SHADER-01 (Fix-A retired) | SEH guard removed once both paths off D3DX | static check | grep `compileVertexShaderFpGuarded` / `__except` = 0 in the file | ✅ grep |

### Sampling Rate
- **Per task commit:** build gl05 + gl07 clean (grep log for 0 `unresolved external symbol`), boot to char-select under the relevant renderer.
- **Per wave merge:** dual-renderer boot (5 AND 11) + Tatooine bump/dot3 spot render.
- **Phase gate:** both renderers boot+render clean, dumpbin shows no compile-path D3DX, Fix-A grep-zero (if both paths ported).

### Wave 0 Gaps
- [ ] **D3DAssemble dialect probe** (the D-06 census gate) — assemble c_simple/tfcl/cloudlayer via
      D3DAssemble (GetProcAddress), CreateVertexShader, render. Pass/fail decides D-03 vs the fallback.
      THIS GATES THE ASM HALF OF THE PHASE AND MUST RUN FIRST.
- [ ] **Census refresh** — re-enumerate the TRE `.vsh` corpus (190 //hlsl + 96 //asm of 286), confirm the
      classification, identify any //asm shader using a construct the sample doesn't cover.
- [ ] **WIP-branch render-correctness baseline** — boot the `d3d9-fixb-d3dcompile-wip` HLSL port and
      characterize what renders / what is incomplete (the genuinely-new work beyond "compiles").

*(No test-file/conftest scaffolding — there is no shader-test framework by design (D-07).)*

## Security Domain

> This is a graphics-pipeline refactor of an offline single-player-asset compile path. No
> authentication, session, access-control, input-from-untrusted-network, or cryptography surface is
> touched. The `.vsh`/`.inc` inputs are trusted local game assets (TRE-resident), not attacker-controlled.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | no | — |
| V5 Input Validation | marginal | Shader source is trusted local asset; the textual rewriter must not buffer-overflow on malformed input (gl11's rewriter has DEBUG_FATAL bounds guards in emitRewritten — preserve them in the D3D9 copy) |
| V6 Cryptography | no | — |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Buffer overflow in textual rewriter (two-pass size/emit divergence) | Tampering / DoS | gl11's two-pass `computeRewrittenLength` + `emitRewritten` with `DEBUG_FATAL` overflow guards (preserve verbatim in the D3D9 copy) [VERIFIED: HlslRewrite.cpp:657,740] |
| Untrusted shader input | — | N/A — `.vsh` are trusted local game assets loaded via TreeFile from signed TRE archives |

## Sources

### Primary (HIGH confidence)
- Codebase (read/verified 2026-06-16):
  - `Direct3d9_VertexShaderData.cpp` (port target, both call sites, Fix-A guard)
  - `Direct3d11_HlslRewrite.{h,cpp}` (Rules A-F, the D-02 lift source)
  - `Direct3d11_VertexShaderData.cpp` :650-950 (DECLARE_textureCoordinateSets register-omission, D3DCompile call, profile, flags)
  - `Direct3d9.vcxproj` / `Direct3d9_vsps.vcxproj` / `Direct3d9_ffp.vcxproj` (link deps, VSPS macro scope, include/lib paths)
  - branch `d3d9-fixb-d3dcompile-wip` (prior HLSL port — `Direct3d9_HlslRewrite` + D3DCompile swap, "compiles render-incomplete")
  - revert commit `c0f890875` (Phase-27 cascade documentation)
  - `.planning/research/vsh-extract/` (census corpus: 12 .vsh + .inc modules)
- `dumpbin /exports d3dcompiler_47.dll` → D3DAssemble ordinal 1, D3DCompile ordinal 4 [VERIFIED]
- `dumpbin /headers d3dcompiler.lib` → D3DAssemble mangled symbol + signature [VERIFIED]
- `C:/Program Files (x86)/Windows Kits/10/Include/.../d3dcompiler.h` → D3D_COMPILER_VERSION 47, no D3DAssemble decl [VERIFIED]

### Secondary (MEDIUM confidence)
- [walbourn.github.io/hlsl-fxc-and-d3dcompile](https://walbourn.github.io/hlsl-fxc-and-d3dcompile/) — vs_2_0/SM2.0 supported by current D3DCompile on all D3D9.0c platforms; fx_* profiles deprecated
- [Microsoft Learn — Specifying Compiler Targets](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/specifying-compiler-targets) — vs_1_1/vs_2_0 listed as legacy targets
- Project memories: `project_hard05_d3dcompile_deferred_to_x64`, `project_phase27_d3dcompile_port_scope`, `project_d3d9_d3dxcompileshader_fp_crash`, `reference_restoration_binaries_intel` (via CONTEXT.md canonical_refs)

### Tertiary (LOW confidence)
- D3DAssemble dialect acceptance — NO authoritative source (undocumented API). Resolved only by the census probe.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — every library/version/path codebase-verified
- Architecture (gl11 rewrite rules): HIGH — read the full .h + .cpp
- HLSL port mechanics: HIGH — the WIP branch is a compiling reference; the ABI reinterpret is proven
- Asm port (D3DAssemble): MEDIUM — export + signature verified; dialect acceptance is the explicit census-gated unknown (by design)
- Pitfalls: HIGH — the Phase-27 cascade is documented in the revert commit; the semantic-loss risk is documented in gl11's own header
- Validation: HIGH — D-07 locks the method; the corpus + reference scene are identified

**Research date:** 2026-06-16
**Valid until:** ~2026-07-16 (stable; in-tree facts don't drift, the only fast-moving element is the
D3DAssemble dialect question which the phase resolves empirically)
