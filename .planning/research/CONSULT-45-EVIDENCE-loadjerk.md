# CONSULT-45 — zone-in load "jerk": our modern rebuild vs retail/SWGEmu

## Evidence (treat as GIVEN — measured this session, do not re-derive)

**Repo:** `D:\Code\swg-client-v2` — a modern **MSVC v145 / C++20** rebuild of the 2003 SWG client
source (MSBuild, no CMake). Engine code under `src/engine/`, game under `src/game/`.

**Symptom:** zoning into the Mos Eisley cantina interior (and world load generally) produces visible
**per-frame hitches/stutter ("jerk") during the load window**. Reproduces on **all renderers**
(gl05/gl06/gl07 = D3D9, gl11 = D3D11) and **both bitness** (Win32 + x64), Debug and Release (worse in
Debug). It is a per-frame **time stall**, not an out-of-memory problem.

**Reference:** the **SWGEmu** client (a retail-derived, optimized binary) zoning into the same cantina
is **MUCH less jerky**. Retail/SWGEmu ships **precompiled shader bytecode** (no runtime shader compile).

### Confirmed facts
- **A.** Our `//hlsl` **vertex shaders compile at RUNTIME via `D3DCompile` on the main thread** during
  zone-in (`src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp`,
  `createVertexShader`, ~line 684; ~44 compiles on first cantina load). **Pixel shaders are
  precompiled** (`Direct3d9_PixelShaderProgramData` creates from `m_exe` bytecode). The D3D11 path also
  runtime-compiles (`Direct3d11_ShaderCache`).
- **B.** A **process-global in-memory VS bytecode cache** exists (commit `ff02a367e`). It eliminates
  **re-entry** recompiles but NOT the **first-ever** load (all misses). It is **NOT persisted to disk**.
- **C.** Memory is NOT the bottleneck for the jerk: the Debug client plateaus **flat ~1.5 GB**;
  `/LARGEADDRESSAWARE` was just enabled (≈4 GB). The jerk is CPU/main-thread time, not paging-to-OOM.
- **D.** Player movement is **dt-scaled (frame-rate independent)**; run speed is **server-driven**
  (`getRunSpeed()`), ~2× SWGEmu by intent (server config) — **keep it**. Relevant only as an amplifier
  (faster movement = you reach not-yet-streamed space sooner = the hitch is more visible).

- **E. Allocator.** We link **no third-party allocator** (no SmartHeap/Hoard/mimalloc — grep clean).
  The engine uses SOE's own `src/engine/shared/library/sharedMemoryManager/src/shared/MemoryManager.cpp`
  — a custom **free-list-tree** allocator (size-indexed `FreeBlock`: smaller/same/larger/parent) with
  built-in debug instrumentation: `DO_TRACK` (per-alloc owner return-address capture, **default 0/off**)
  and `DO_GUARDS` (overrun guard bytes). On Windows/MSVC the `DO_TRACK` owner attribution is **dead**
  (`DebugHelp::getCallStack` returns 0). This allocator is in the per-object zone-in hot path.

### Build/runtime differences from the original VS2003 retail binary
VS2003 → MSVC v145/C++20; STLport → modern MSVC STL; `D3DXCompileShader` → `D3DCompile` (Phase 32, to
fix a modern-toolchain D3DX FP-exception crash); `/FORCE` link; `D3DCREATE_FPU_PRESERVE` just added.

## The question
Characterize, in DETAIL, what in our client's zone-in path differs from retail and causes the
main-thread hitches, and propose concrete fixes. Fundamentals are understood (runtime VS compile is a
confirmed contributor; a disk bytecode cache is a candidate). We want the **details** and especially
the **open question of synchronous vs background asset loading**. Each consultant has a distinct angle
(below) — stay on yours; convergence from independent angles is the signal.
