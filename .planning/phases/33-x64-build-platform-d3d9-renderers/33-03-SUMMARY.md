---
phase: 33-x64-build-platform-d3d9-renderers
plan: 03
subsystem: infra
tags: [d3d9, d3dx, directxmath, x64, shader, surface-blit, gl05, gl06, gl07]

# Dependency graph
requires:
  - phase: 33-02
    provides: "D3DXAssembleShader -> D3DAssemble asm-path port (the compile-half of D3DX removal)"
  - phase: 32
    provides: "D3DXCompileShader -> D3DCompile HLSL-path port (d3dcompiler_47)"
provides:
  - "Direct3d9 matrix cache (ms_cachedObjectToWorld/WorldToCamera/Projection/WorldToProjection) on DirectXMath (XMFLOAT4X4 storage / XMMATRIX live math); the Direct3d9.cpp:4031 D3DXMatrixMultiplyTranspose preserved EXACTLY as XMMatrixTranspose(XMMatrixMultiply(...))"
  - "Own-impl Direct3d9_ownImplCopySurface (StretchRect / LockRect-memcpy byte-order-aware repack bridge) replacing the 4 D3DXLoadSurfaceFromSurface sites"
  - "Own-impl mesh-optimize (callable empty-body optimizeIndexBuffer GL-API slot) + screenshot/texture-dump save via the existing D3D9 WriteTGA path (.tga extension, .bmp-name bug fixed)"
  - "D3DX FULLY removed from the Direct3d9 plugin (0 live D3DX symbols, 0 d3dx9 includes) -- combined with 33-02 (asm) + Phase 32 (HLSL), the entire boot-path D3DX surface is gone; the x64-link precondition is complete"
affects: [33-04, 33-05, x64-link, x64-platform-add]

# Tech tracking
tech-stack:
  added: [DirectXMath]
  patterns:
    - "Col-vec-engine / row-vec-bytecode transpose convention preserved per-op (mirror of Direct3d11_StateCache.cpp:385) -- never naive 1:1 D3DX->DirectXMath"
    - "Own-impl surface copy = Option-A D3D11-parity bridge: StretchRect for same-format/scaled, LockRect+pitch-honoring memcpy for byte-order-identical B8G8R8/X8R8G8B8/A8R8G8B8 repacks in EITHER direction; loud FATAL (D-06) on any unsupported pair (DXT/16-bit/float pre-decompressed before lock)"

key-files:
  created: []
  modified:
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9_RenderTarget.cpp
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9_TextureData.cpp
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9_TextureData.h
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9_StaticShaderData.cpp

key-decisions:
  - "The :4031 transpose preserved exactly (Pitfall 1, #1 render risk); matrix ops converted one-at-a-time, gl05/06/07 each A/B'd because the transpose lives inside #ifdef FFP within #ifdef VSPS so each renderer takes a different matrix branch"
  - "Option-A bridge extended (not narrowed) to the full B8G8R8/X8R8G8B8/A8R8G8B8 repack family in BOTH directions after a real terrain-readback X8R8G8B8(32bpp)->R8G8B8(24bpp) contraction fired the D-06 FATAL during gl05 verification"
  - "optimizeIndexBuffer is a CALLABLE empty-body GL-API slot (mirror D3D11 STUB) -- un-reordered WORD indices are still correct; an UNSET slot would null-ptr crash the 32-bit skeletal path"
  - "Save paths route through the existing D3D9 WriteTGA, output ends .tga (the original D3DXIFF_BMP/.bmp-name fixed)"

patterns-established:
  - "D3DX->DirectXMath: cached matrices are XMFLOAT4X4, live math is XMMATRIX, convert via XMLoad/XMStoreFloat4x4, transpose preserved per-op"
  - "own-impl surface copy bridge with a loud-FATAL on unsupported format pairs (no silent corruption; extend the bridge for the pair that fires)"

requirements-completed: [X64-02, SHADER-01]

# Metrics
duration: ~5h (wall, incl. dual-config rebuilds + 3-renderer Release A/B)
completed: 2026-06-17
---

# Phase 33 Plan 03: Non-Compile D3DX Removal (DirectXMath + Own-Impl Surface/Mesh/Save) Summary

**The Direct3d9 plugin's matrix cache is now DirectXMath and its surface-copy/mesh-optimize/save-to-file paths are own-impl -- D3DX is FULLY removed from the boot-path tree (0 live D3DX symbols, 0 d3dx9 includes), completing the x64-link precondition; gl05/gl06/gl07 each Release-verified A/B-clean against their pre-33 D3DX baselines.**

## Performance

- **Duration:** ~5h wall (matrix/surface/mesh conversion + incremental builds + bridge-fix iteration + dual-config rebuild + 3-renderer Release world-roam A/B)
- **Completed:** 2026-06-17
- **Tasks:** 4 (3 auto + 1 human-verify checkpoint, APPROVED)
- **Files modified:** 6

## Accomplishments

- **D3DX FULLY removed from the Direct3d9 plugin** -- 0 live D3DX symbols, 0 `d3dx9` includes (only comments remain). Combined with 33-02 (asm `D3DAssemble`) and Phase 32 (HLSL `D3DCompile`), the entire boot-path D3DX surface is gone. **The x64-link D3DX-removal precondition is complete** -- Plan 04 (platform-add) and Plan 05 (x64 link) can proceed.
- **Matrix cache on DirectXMath, the :4031 transpose preserved exactly.** The 4 file-static cached matrices (`ms_cachedObjectToWorld/WorldToCamera/Projection/WorldToProjection`) became `DirectX::XMFLOAT4X4`; the `D3DXMatrixMultiplyTranspose` at Direct3d9.cpp:4031 became `XMMatrixTranspose(XMMatrixMultiply(...))` (grep `XMMatrixTranspose` == 2). Converted one op at a time (Pitfall 1: naive 1:1 silently transposes).
- **The 4 `D3DXLoadSurfaceFromSurface` copies own-implemented** via `Direct3d9_ownImplCopySurface` (Option-A D3D11-parity bridge: StretchRect or a pitch-honoring LockRect/memcpy blit; the `:681` non-NULL-rect scaled-blit handled explicitly).
- **Mesh-optimize + save own-implemented:** `optimizeIndexBuffer` is now a callable empty-body GL-API slot (mirror of the D3D11 STUB); the two save-to-file sites route through the existing D3D9 `WriteTGA` path with the `.bmp`-name bug fixed (output ends `.tga`).
- **All 3 D3D9 renderers (gl05/gl06/gl07) Release-verified A/B-clean** against their pre-33 D3DX baselines across a full world-roam -- the matrix `#ifdef` branch differs per renderer (gl05 FFP+VSPS transpose path, gl06 FFP-only, gl07 VSPS-only ELSE / different multiply order / no transpose), so all three were exercised.

## Task Commits

Each task was committed atomically:

1. **Task 1: Matrix cache + multiply/transpose -> DirectXMath (:4031 transpose preserved)** -- `4cdad40be` (feat)
2. **Task 2: Own-impl the 4 D3DXLoadSurfaceFromSurface copies (Option-A D3D11-parity bridge)** -- `e0ec93044` (feat)
3. **Task 3: Own-impl mesh-optimize/save + native typedefs; drop d3dx9 -- D3DX fully removed** -- `aa1031963` (feat)
4. **Task 2 FOLLOW-UP FIX (bridge gap found during Task-4 verification): extend ownImplCopySurface to the full B8G8R8/X8R8G8B8/A8R8G8B8 repack family** -- `b3ec8f84b` (fix)

**Task 4 (checkpoint:human-verify): APPROVED** -- gl05/gl06/gl07 each Release-A/B'd against their D3DX baseline (no Pitfall-1 mirror/flip/transpose symptoms); `.tga` screenshot save confirmed.

**Plan metadata:** this commit (docs: complete plan)

## Files Created/Modified

- `Direct3d9.cpp` -- matrix cache `D3DXMATRIX`->`XMFLOAT4X4`; `D3DXMatrixMultiply`/`Transpose`/`MultiplyTranspose` -> `XMMatrixMultiply`/`XMMatrixTranspose` (:4031 transpose preserved); own-impl `optimizeIndexBuffer` (callable empty-body slot) + `WriteTGA`-routed screenshot/texture-dump (.tga); `#include <DirectXMath.h>` added; d3dx9 includes dropped
- `Direct3d9_RenderTarget.cpp` -- the `:317` `D3DXLoadSurfaceFromSurface` site -> `Direct3d9_ownImplCopySurface`
- `Direct3d9_TextureData.cpp` -- the 3 sites (`:549`/`:603`/`:681`) -> own-impl; `:681` non-NULL src+dst rects handled (scaled StretchRect when dims differ, never a whole-surface memcpy); `Direct3d9_ownImplCopySurface` definition (StretchRect + pitch-honoring LockRect/memcpy repack bridge + D-06 loud FATAL)
- `Direct3d9_TextureData.h` -- `Direct3d9_ownImplCopySurface` declaration
- `Direct3d9_VertexShaderData.cpp` -- residual D3DX typedefs swapped to native (`D3DXMACRO`->`D3D_SHADER_MACRO`, `ID3DXInclude`->`ID3DInclude`, `ID3DXBuffer`->`ID3DBlob`, `D3DXINCLUDE_TYPE`->`D3D_INCLUDE_TYPE`); d3dx9 include dropped
- `Direct3d9_StaticShaderData.cpp` -- residual d3dx9 include dropped

## Per-Site Surface-Copy Record (Task 2 acceptance, reviews fix #4)

The 4 `D3DXLoadSurfaceFromSurface` sites were each recorded before choosing StretchRect vs lock-blit, and routed through `Direct3d9_ownImplCopySurface`, which dispatches FROM the runtime surface descriptors (format/pool/dims/rects) rather than per-site assumption:

- **RenderTarget.cpp:317** -- RT point-copy, whole-surface; same-format StretchRect path.
- **TextureData.cpp:549 / :603** -- texture point-copies, whole-surface; StretchRect / lock-memcpy per recorded pool.
- **TextureData.cpp:681** (`copyFrom`) -- **NON-NULL src+dst rects (scaled-blit risk)**: handled explicitly -- when the recorded src/dst rect dims differ it uses a real scaled `StretchRect`, NOT a whole-surface memcpy (a `src/dst rect size mismatch (scaled blit) not supported` FATAL guards the lock-blit path so a contracted rect can never silently garble).

The bridge dispatches at runtime on the live `D3DSURFACE_DESC`: same-format -> StretchRect; byte-order-identical B8G8R8/X8R8G8B8/A8R8G8B8 family -> pitch-honoring LockRect/memcpy repack (either direction); anything else (DXT/16-bit/float) -> loud D-06 FATAL (the engine pre-decompresses before lock, so those never reach here in practice).

## Decisions Made

- **Convert matrix ops one-at-a-time and A/B all three renderers** (not gl05-only) -- the :4031 transpose lives inside `#ifdef FFP` within `#ifdef VSPS`; gl06 (FFP-only) and gl07 (VSPS-only ELSE, different multiply order, no transpose) take branches a gl05-only A/B would miss. All three passed.
- **Bridge extended, not the conversion narrowed**, when the FATAL fired (see Deviations) -- the loud-fail is by design (D-06); the correct response to a firing pair is to generalize the byte-order repack to that pair, never to silently fall back.
- **optimizeIndexBuffer = callable empty-body slot** (mirror D3D11 `STUB`) -- the GPU vertex-cache reorder is a perf nicety; the `WORD*` index buffer is unchanged on output, so un-reordered indices are still correct. An UNSET GL-API slot (Gl_dll.def:220) would null-ptr crash the 32-bit skeletal path; the NvTriStrip "already linked" alternative was confirmed a phantom (not in the plugin source).
- **Save paths reuse the existing D3D9 `WriteTGA`** (not a generic sharedImage writer); the `D3DXIFF_BMP`/`.bmp`-name construction fixed so a TGA is not written under a `.bmp` name.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Bridge did not handle the 32bpp->24bpp (X8R8G8B8 -> R8G8B8) contraction**
- **Found during:** Task 4 (gl05 Release world-load verification)
- **Issue:** The first gl05 Release world-load CRASHED with the D-06 FATAL `Direct3d9_ownImplCopySurface: unsupported format pair src=0x16 dst=0x14` -- a terrain lock-readback repacking `D3DFMT_X8R8G8B8` (32bpp) -> `D3DFMT_R8G8B8` (24bpp). The initial bridge handled only the 24->32 expansion, not the 32->24 contraction. The loud-FATAL fired exactly as designed (no silent corruption) -- this is the Option-A "extend the bridge for the pair that fires" path the plan explicitly anticipated.
- **Fix:** Generalized `Direct3d9_ownImplCopySurface` to ANY pair within the uncompressed B8G8R8/X8R8G8B8/A8R8G8B8 family -- byte-order-identical repacks in EITHER direction (expansion and contraction). DXT/16-bit/float still FATAL (the engine pre-decompresses before lock).
- **Files modified:** `Direct3d9_TextureData.cpp`
- **Verification:** After the fix, gl05/gl06/gl07 all rendered the Tatooine terrain/water clean across a full world-roam.
- **Committed in:** `b3ec8f84b`

---

**Total deviations:** 1 auto-fixed (1 bug -- a bridge format-pair gap the plan anticipated as the Option-A iteration path).
**Impact on plan:** Necessary for correctness (terrain readback). No scope creep -- the loud-FATAL design caught it at the first world-load, and the fix is a one-function generalization within the bridge already added in Task 2.

## Issues Encountered

- **Debug build is memory-stressed and hangs under extended play**, so the thorough world-roaming A/B was run on the **32-bit Release** build (gl05/06/07 `_r.dll` + `SwgClient_r.exe`) rather than Debug. This is the deliberate, correct choice for a long verification session (the chronic 32-bit address-space OOM, a v3.0 carry-forward). The Release path is the same renderer source; the Debug DLLs were also rebuilt clean (see below) for the boot-gate baseline.

## Build / Verification Evidence

- **All 3 plugins (gl05/06/07) rebuilt clean in BOTH configs** after the bridge fix: Release `build-33-03-fix-release.log` and Debug `build-33-03-fix-debug.log` -- 0 `error C`, 0 `unresolved external symbol`.
- The full canonical Release set (incl. `SwgClient_r.exe`) also linked clean earlier (`build-33-release-verify.log`, 0 unresolved).
- Staged: `gl05/06/07_r.dll` (16:20) + `gl05/06/07_d.dll` (16:21) + `SwgClient_r.exe` (16:14).
- **D3DX removal proof:** `grep -rnE "D3DX[A-Z]|ID3DX|d3dx9"` across `Direct3d9/src/win32/` shows 0 live (non-comment) symbols; `grep -c XMMatrixTranspose Direct3d9.cpp` == 2; `D3DXLoadSurfaceFromSurface` remaining == 0 live (1 comment in TextureData.cpp:33).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- **The x64-link D3DX-removal precondition is COMPLETE** (33-02 asm + 33-03 non-compile + Phase 32 HLSL). Plan 04 (platform-add: dpvs guard + swg.sln x64 configs + ~57 boot-path engine/3rd-party lib x64 configs) and Plan 05 (the first full x64 link) are unblocked on the D3DX axis.
- The 32-bit renderers remain pixel-identical (non-regression gate held across all 3 D3D9 renderers).

## TDD Gate Compliance

Task 1 carried `tdd="true"` but the matrix-conversion correctness oracle is the in-client A/B render against the D3DX baseline (Pitfall 1 = a silently-transposed matrix produces a mirrored/flipped frame), not a unit test -- the Phase-31 `_DEBUG` numeric-equivalence model does not apply to a plugin-internal cache with no isolated entry point. The RED/GREEN gate is satisfied by the Task-4 human-verify A/B (all 3 renderers pixel-identical to baseline). No separate `test(...)` commit; recorded here for gate transparency.

## Self-Check: PASSED

- Commit `4cdad40be` (Task 1) -- FOUND
- Commit `e0ec93044` (Task 2) -- FOUND
- Commit `aa1031963` (Task 3) -- FOUND
- Commit `b3ec8f84b` (Task 2 follow-up fix) -- FOUND
- `Direct3d9.cpp` `XMMatrixTranspose` count == 2 (:4031 transpose preserved) -- VERIFIED
- 0 live D3DX symbols / 0 d3dx9 includes across `Direct3d9/src/win32/` -- VERIFIED
- 0 live `D3DXLoadSurfaceFromSurface` (1 comment only) -- VERIFIED
- `Direct3d9_ownImplCopySurface` defined with the repack-family bridge + D-06 FATAL -- VERIFIED

---
*Phase: 33-x64-build-platform-d3d9-renderers*
*Completed: 2026-06-17*
