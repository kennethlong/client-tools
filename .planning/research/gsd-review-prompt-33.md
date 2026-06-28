# Cross-AI Plan Review Request — Phase 33

You are an independent reviewer of implementation plans for a software phase. You have
**read-only access to the repository** at `D:\Code\swg-client-v2`. Read the artifacts named
below directly from disk and verify the plans' factual claims against the live tree where you
can. Be adversarial and specific: cite `file:line` or plan/task IDs. Do not rubber-stamp.

## Project (one paragraph)

`swg-client-v2` is a modernized Star Wars Galaxies game client. Pure MSBuild (no CMake),
trunk-based on `master`, toolset v145 (VS18/2026). It ships a 32-bit client today with two
renderer families: D3D9 plugins (gl05/gl06/gl07, selected by `rasterMajor=5/6/7`) and a D3D11
plugin (gl11, `rasterMajor=11`). The current milestone (v3.0) ports the whole client to native
**x64** across Phases 31–36. Phase 31 (64-bit correctness — asm→intrinsics, pointer/width
truncation, serialization audit) is COMPLETE; the in-scope tree compiles x64-clean as a
compile-only harness. Phase 32 (D3DX→d3dcompiler_47 on the HLSL compile path) is partially
landed. **Builds/boots/renders are the source of truth — planning docs lag reality.**

## Phase 33 goal & success criteria (from ROADMAP)

**Goal:** the first fully-linking native x64 client. Add the `x64` platform to `swg.sln` +
the boot-path `.vcxproj` subset, resolve every third-party dep as x64, finish removing the
residual D3DX dependency (the x64-link precondition), and get the D3D9 plugins (gl05/06/07) to
carry the x64 client to character select under rasterMajor 5/6/7 — without regressing the
shipped 32-bit build.

Success criteria (what must be TRUE):
1. `SwgClient_*.exe` builds as x64 (`dumpbin` machine x64); `x64` platform in swg.sln + every dependent `.vcxproj`.
2. All third-party deps resolve as x64; x64 client launches with no missing-DLL/load failure.
3. gl05/06/07 build x64 + x64 client boots to character select under rasterMajor 5/6/7.
4. The 32-bit build still boots under both renderers (no Win32 regression).

Requirements: **X64-01** (x64 platform + binaries), **X64-04** (3rd-party deps resolve x64),
**X64-02** (gl05/06/07 boot under rasterMajor 5/6/7).

## Locked decisions (review the MECHANICS, do NOT re-litigate the decisions)

- **D-01 HYBRID sourcing:** build x64 from in-tree source where it exists (zlib/dpvs/ui/
  udplibrary/tinyxml); LIFT RAD-proprietary binary-only middleware (bink/miles) from
  `D:\SWG Restoration\x64\` + generate import .libs; libxml2/pcre/jpeg have no in-tree build
  project → lift the Restoration x64 DLL + generate an import .lib. Universal fallback: any
  from-source build that stalls → grab the Restoration x64 DLL.
- **D-02 dpvs build-from-source:** full SOE source in-tree; guard `DPVS_CPU_X86` on `!_M_X64`
  so the x86 `__asm` is off and the portable-C fallbacks compile; add x64 config + a pointer-
  truncation pass. Not a binary lift.
- **D-03 platform-add scope:** boot-path subset ONLY (~53 engine libs + dpvs/zlib/ui/udplibrary/
  tinyxml + gl05/06/07 + SwgClient). Editors stay Win32-only.
- **D-04 finish D3DX removal BEFORE the x64 link:** (a) non-compile D3DX (matrix/mesh/surface,
  ~15 APIs) → DirectXMath/own-impl, NOT redist d3dx9; (b) asm `.vsh` path → D3DAssemble.
- **D-05 asm `.vsh` → D3DAssemble:** the Phase 32-01 dialect gate already PASSED byte-identical;
  gl11's `[P19VSFALLBACK]` does NOT transfer (D3D9 natively executes vs_1_1/vs_2_0 asm). Redist-
  d3dx9 fallback explicitly REJECTED (surface as a checkpoint, never silently).
- Miles audio (`mss64.dll`, 7.2e→9.3b) is **Phase 35** — the x64 client boots audio-SILENT via
  `#if _M_X64` stubs. gl11/D3D11 x64 is **Phase 34**. Door-snap/OOM verification is **Phase 36**.

## Artifacts to read (in the repo)

- `.planning/phases/33-x64-build-platform-d3d9-renderers/33-CONTEXT.md` — full decisions/scope
- `.planning/phases/33-x64-build-platform-d3d9-renderers/33-RESEARCH.md` — per-lib sourcing table, pitfalls, residual worklist, validation arch
- `.planning/phases/33-x64-build-platform-d3d9-renderers/33-PATTERNS.md` — exact existing blocks to copy
- `.planning/phases/33-x64-build-platform-d3d9-renderers/33-0{1,2,3,4,5,6}-PLAN.md` — the 6 plans under review
- `.planning/phases/33-x64-build-platform-d3d9-renderers/33-VALIDATION.md` — validation strategy
- `AGENTS.md` — build/run/renderer/cfg-safety conventions (MSBUILD env, /FORCE link-grep, /nodeReuse:false, rasterMajor map, shared-header ABI cascade)

The plans (waves): **01** x64 foundations (committed `x64-platform.props`, libxml2/pcre/jpeg x64
import-libs, tinyxml x64 build, staged-DLL checklist) · **02** asm→D3DAssemble + Fix-A
disposition (32-bit gl05 //asm render smoke) · **03** non-compile D3DX→DirectXMath/own-impl
(preserve the `:4031` transpose; gl05 Tatooine A/B) · **04** dpvs guard + x64 config + swg.sln
x64 + ~57 boot-path StaticLibrary x64 configs · **05** gl05/06/07 + SwgClient x64 link blocks +
Miles `#if _M_X64` stub + Bink binkw64 swap + first full x64 link (0 unresolved external) · **06**
stage-x64/ + dumpbin-x64 + x64 boot under rasterMajor 5/6/7 + 32-bit non-regression.

## Review instructions

For the phase as a whole AND per plan, provide:

1. **Summary** — one-paragraph assessment of whether these 6 plans, executed in order, achieve the phase goal.
2. **Strengths** — what is well-designed (bullets).
3. **Concerns** — issues/gaps/risks as bullets, each tagged **HIGH / MEDIUM / LOW**, with the plan/task ID or file:line it applies to.
4. **Suggestions** — specific, actionable improvements.
5. **Risk assessment** — overall LOW/MEDIUM/HIGH with justification.

Focus especially on (these are where this phase is most likely to be wrong):
- **Dependency ordering / wave correctness** — Plan 02/03 do the D3DX removal on the *still-32-bit* tree as the x64-link precondition; Plan 04 platform-adds; Plan 05 links. Is the ordering sound? Are the `depends_on` edges right? Is there a hidden cross-plan coupling (e.g. shared-header ABI cascade, the `Direct3d9_VertexShaderData.cpp` typedef ownership split between 02 and 03)?
- **The Phase-32 precondition** — Plans 02/03 assume the HLSL compile path already migrated to D3DCompile in "32-02". The ROADMAP shows Phase 32 plans 32-02..32-05 as NOT checked-off. Is Phase 33 resting on Phase-32 work that may not have landed? Is that a real blocker or just artifact drift?
- **Correctness risks** — the DirectXMath transpose convention (Pitfall 1, the `:4031` `D3DXMatrixMultiplyTranspose`); the 8 `D3DXLoadSurfaceFromSurface`→StretchRect/lock-blit point-copies (A3 — format/dimension assumptions); the mesh-optimize no-op (A4); D3DAssemble on the 96 //asm shaders not in the probed sample (no silent null-VS).
- **Link correctness** — the `/FORCE` trap (exit 0 ≠ clean link; grep `unresolved external symbol`==0); the import-lib machine-mismatch (Pitfall 6); the Miles `#if _M_X64` stub leaving ZERO `AIL_*` symbols in the x64 TU; the x64 OutDir isolation protecting the Win32 `stage/`.
- **Verifiability** — are the must_haves/acceptance_criteria checkable? Any green/skipped gate that wouldn't actually catch a regression? Are the human-verify checkpoints (02-T3, 03-T4, 06-T2, 06-T3) well-specified?
- **Scope** — anything missing for a *booting* x64 client that no plan covers? Anything over-engineered / out-of-scope creeping in?

Output structured markdown.
