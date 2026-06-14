---
phase: 27-d3dcompile-port
plan: 01
subsystem: infra
tags: [d3dcompile, d3dx9, d3dcompiler, shader, direct3d9, vsps, build, baseline]

# Dependency graph
requires:
  - phase: 27-d3dcompile-port (planning)
    provides: census + scope decisions (D-01-R HLSL-only, D-02-R asm stays on D3DXAssembleShader)
provides:
  - Committed asm-shader census scoping artifact (286 = 190 //hlsl + 96 //asm; 454 PEXE .psh out of scope) — HARD-05 Success Criterion 1
  - Tracked 32-bit d3dcompiler_47.dll + d3dcompiler.lib linked in gl05/gl07 (all 3 configs) with d3dx9.lib retained
  - Postbuild staging of d3dcompiler_47.dll into stage/
  - Pre-port A/B visual baseline (5 spots x D3D9/D3D11) for the Plan 03 parity verdict
affects: [27-02 (D3DCompile HLSL swap), 27-03 (dual-renderer parity verdict), x64-milestone (asm->D3DAssemble + full D3DX removal)]

# Tech tracking
tech-stack:
  added: [d3dcompiler.lib, d3dcompiler_47.dll (Win10 SDK x86 redist)]
  patterns:
    - "Tracked redist dll source in src/external/3rd/library/directx9/lib/ + per-config postbuild copy /Y into stage/ (mirrors d3dx9d.dll / stage/miles precedent)"
    - "Link prerequisites land BEFORE the compile-call swap; d3dx9.lib retained alongside d3dcompiler.lib (4 other gl05/gl07 files still use D3DX)"

key-files:
  created:
    - .planning/phases/27-d3dcompile-port/27-ASM-CENSUS.md
    - src/external/3rd/library/directx9/lib/d3dcompiler_47.dll
    - docs/research/phase27-baseline/BASELINE.md (+ 10 A/B screenshots)
  modified:
    - src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj
    - src/engine/client/application/Direct3d9/build/win32/Direct3d9_vsps.vcxproj

key-decisions:
  - "Census recorded first as committed artifact (HARD-05 SC1) — asm path explicitly scoped, not silently dropped"
  - "asm path (D3DXAssembleShader @ Direct3d9_VertexShaderData.cpp:567) STAYS on D3DXAssembleShader + SEH this phase (D-02-R); D3DAssemble port + full D3DX removal deferred to x64"
  - "d3dx9.lib retained in all configs — D3DX still used by Direct3d9.cpp + TextureData/RenderTarget/StaticShaderData; dropping it is an anti-pattern /FORCE would mask"
  - "Optimized (_o) config NOT built — pre-existing SAFESEH/LNK1281 quirk; link validated via Debug+Release only"

patterns-established:
  - "Pattern: redist dll provisioned via tracked source + per-config postbuild copy into gitignored stage/"
  - "Pattern: pre-port A/B baseline (gl05 rasterMajor=5 vs gl11 rasterMajor=11) captured before a renderer swap as the parity reference"

requirements-completed: [HARD-05]

# Metrics
duration: ~continuation finalize
completed: 2026-06-14
---

# Phase 27 Plan 01: D3DCompile Port Foundation Summary

**Recorded the 286-shader asm census (190 //hlsl + 96 //asm) as the HARD-05 census-first artifact, linked d3dcompiler.lib + staged the 32-bit d3dcompiler_47.dll in gl05/gl07 (d3dx9.lib retained, 0 unresolved externals), and captured a 5-spot pre-port A/B visual baseline — making the D3DCompile swap link- and run-able without any compile-call change yet.**

## Performance

- **Duration:** continuation finalize (Tasks 1-2 + Task 3 build done by prior executor; this agent committed the baseline + wrote the SUMMARY)
- **Completed:** 2026-06-14
- **Tasks:** 3
- **Files modified:** 2 vcxproj + 3 created artifacts (census, dll, baseline) + 10 baseline screenshots

## Accomplishments
- HARD-05 Success Criterion 1 satisfied: the asm-shader census is a committed scoping artifact PRODUCED FIRST, with the 96 //asm consumers explicitly scoped to stay on D3DXAssembleShader (not silently dropped by the port).
- d3dcompiler_47.dll (32-bit PE32 i386, 4,147,664 bytes) tracked in src/external/3rd/library/directx9/lib/ and staged into stage/ by postbuild; d3dcompiler.lib linked into gl05 + gl07 across all 3 configs; d3dx9.lib retained.
- gl05 (Direct3d9.vcxproj) + gl07 (Direct3d9_vsps.vcxproj) rebuilt clean Debug+Release with 0 unresolved external symbols — the lib add does not break the link even before the D3DCompile call exists.
- Pre-port A/B visual baseline captured (5 spots x D3D9/D3D11) — the reference Plan 03 diffs against.

## Task Commits

Each task was committed atomically:

1. **Task 1: Record the asm-shader census scoping artifact** - `1f41458a2` (docs)
2. **Task 2: Stage d3dcompiler_47.dll + link d3dcompiler.lib in gl05/gl07** - `1145e12ac` (build)
3. **Task 3: Pre-port A/B visual baseline (build half + capture)** - `1971a6024` (docs — baseline artifact; build half produced no source change, validated by the 0-unresolved rebuild)

**Plan metadata:** committed separately (this SUMMARY).

## Files Created/Modified
- `.planning/phases/27-d3dcompile-port/27-ASM-CENSUS.md` - Census scoping artifact: 286 unique .vsh = 190 //hlsl + 96 //asm; 454 .psh PEXE (out of scope); 0 read errors; 96-asm-consumer sample list; HARD-05 SC1 + D3DXAssembleShader scoping conclusion.
- `src/external/3rd/library/directx9/lib/d3dcompiler_47.dll` - Tracked 32-bit redist source for postbuild staging (Win10 SDK x86).
- `src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj` - gl05: d3dcompiler.lib added to AdditionalDependencies (3 configs, d3dx9.lib retained) + postbuild copy /Y of d3dcompiler_47.dll into stage/.
- `src/engine/client/application/Direct3d9/build/win32/Direct3d9_vsps.vcxproj` - gl07: same lib add + postbuild stage copy.
- `docs/research/phase27-baseline/BASELINE.md` (+ 10 screenshots) - Pre-port A/B reference: 5 pairs (char-select, black-wall distance, wall close-up detail/bump, sign decal, cantina interior) under rasterMajor=5 (D3D9) and =11 (D3D11).

## Decisions Made
- **Census first (HARD-05 SC1):** the artifact must demonstrate the port does not null the asm path. The 96 //asm consumers (a_detail*, saberbase, *_decal, ...) stay on D3DXAssembleShader + SEH this phase (D-02-R, revised 2026-06-14); the asm->D3DAssemble port and full D3DX removal are deferred to the x64 milestone. Plan 03 validates the asm path is left untouched.
- **d3dx9.lib retained in all configs:** D3DX matrix/surface APIs are still used by Direct3d9.cpp, Direct3d9_TextureData.cpp, Direct3d9_RenderTarget.cpp, Direct3d9_StaticShaderData.cpp. Dropping it would be masked by /FORCE — kept linked alongside d3dcompiler.lib.
- **gl06 (FFP) untouched:** VSPS is not defined there, so the VS compile branch is excluded — no d3dcompiler.lib needed (D-06).
- **Optimized (_o) config not built:** pre-existing SAFESEH/LNK1281 quirk; link validated via Debug+Release link-grep only (project convention).

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None during this finalize. The Task 3 human-verify checkpoint (baseline capture under both renderers) was a planned blocking gate, resolved by the orchestrator; cfg end-state restored to rasterMajor=11 per convention.

## Known Gaps
- **No ignited-saber frame in the baseline.** The decal (sign) and detail-map (wall close-up) //asm consumers ARE covered, plus the Tatooine bump/dot3 Fix-A class via the wall close-up. This is sufficient for Plan 03's "do the 96 //asm consumers still render" parity check, but a future capture could add an ignited-saber frame for completeness.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- HARD-05 Success Criterion 1 satisfied; the link + runtime prerequisites for D3DCompile exist (d3dcompiler.lib linked, d3dcompiler_47.dll staged) with no compile-call change yet.
- A pre-port visual baseline exists for the Plan 03 parity verdict.
- Plan 02 can now perform the in-place D3DXCompileShader -> D3DCompile swap on our own //hlsl shaders, then Plan 03 re-captures the same five spots and diffs against this baseline (asm path must remain on D3DXAssembleShader and still render).

---
*Phase: 27-d3dcompile-port*
*Completed: 2026-06-14*
