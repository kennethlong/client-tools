# Phase 11 — Plan 07 Iteration Log

**Started:** 2026-05-17
**Status:** In progress — iteration 2 landed; awaiting Kenny smoke for next FATAL boundary

## Iteration 1: Author Direct3d11_ShaderImplementationData + Direct3d11_StaticShaderData wrappers (createShaderImplementationGraphicsData FATAL boundary)

- **Date:** 2026-05-17
- **Predicted symptom:** FATAL at `createShaderImplementationGraphicsData` during `ShaderTemplate::install` per Plan 11-06 SUMMARY prediction. Engine calls `Graphics::createShaderImplementationGraphicsData(*this)` from `ShaderImplementation::load_0004 / 0005 / 0006 / 0007 / 0008 / 0009` (7 call sites). The plugin's slot was `STUB()` after Plan 11-06.
- **Hypothesis:** Two new GraphicsData wrapper classes are required:
  1. `Direct3d11_ShaderImplementationData` (inherits `ShaderImplementationGraphicsData`) — held per loaded `ShaderImplementation` asset
  2. `Direct3d11_StaticShaderData` (inherits `StaticShaderGraphicsData`) — held per `StaticShader` instance, must implement virtual `update(StaticShader const &)` + `getTextureSortKey() const`
  Both wrappers can be MINIMAL for Iteration 1 (construct successfully + record bind); per-pass state-apply work (BS/DSS overrides) and per-pass VS/PS lookup land in later iterations driven by visual symptoms. Pitfall classification: novel (not one of the 8 documented Pitfalls — this is "engine-side factory slot was STUB'd").
- **Investigation:**
  - Read Plan 11-06 SUMMARY's Predicted Plan 11-07 first FATAL note: "createShaderImplementationGraphicsData ... first shader-implementation factory called during ShaderTemplate::install."
  - Read `Direct3d9_ShaderImplementationData.{h,cpp}` (D3D9 reference). D3D9 sibling has TWO orthogonal apply paths gated by `#ifdef FFP` and `#ifdef VSPS`. D-04a omits FFP entirely; D3D11 only mirrors the VSPS shape (per-pass alpha-blend / z / color-write state recording + pixel-shader-program pointer).
  - Read `Direct3d9_StaticShaderData.{h,cpp}` (D3D9 reference). Static-shader wrapper resolves the engine's per-instance state at load time: vertex shader lookup, material struct, texture-factor data, alpha-test reference, stencil reference, fog mode, per-stage texture references. Iteration 1 omits these — pure-construct contract is sufficient to surface the NEXT FATAL.
  - Read engine-side `ShaderImplementation.h` base classes:
    - `ShaderImplementationGraphicsData` has 1 virtual: `~ShaderImplementationGraphicsData()`
    - `StaticShaderGraphicsData` has 3 virtuals: dtor + `update(StaticShader const &) = 0` + `getTextureSortKey() const = 0`
  - Verified `setBadVertexShaderStaticShader` + `setStaticShader` were already wired by Plan 11-06 (`Direct3d11_StateCache.cpp:791,800` — no-op recordings).
- **Root cause:** Two `STUB()` lines at `Direct3d11.cpp:583-584` (createShaderImplementationGraphicsData / createStaticShaderGraphicsData). The wrappers themselves did not yet exist in `Direct3d11/src/win32/`.
- **Fix:** Authored 4 new files (h + cpp pairs for ShaderImplementationData + StaticShaderData), wired into `Direct3d11.cpp` install path + `Direct3d11.vcxproj` + `.filters`. Both wrappers minimum-viable Iteration 1: construct + minimal accessors + no-op apply.
  - `Direct3d11_ShaderImplementationData.h` (~90 ln) — class definition + accessor surface.
  - `Direct3d11_ShaderImplementationData.cpp` (~115 ln) — install + remove + ctor + dtor + apply (no-op with documented per-pass mapping intent).
  - `Direct3d11_StaticShaderData.h` (~100 ln) — class definition + ms_active cache slots + virtual override surface.
  - `Direct3d11_StaticShaderData.cpp` (~165 ln) — install + remove + beginFrame + ctor + dtor + update + getTextureSortKey + apply (active-shader cache + no-op record).
  - `Direct3d11.cpp` — added 2 includes, 2 factory `_impl` bodies, install order (after `Direct3d11_Metrics::install()`), reverse-order in `remove_impl`, replaced 2 `STUB()` lines with real factory bindings.
  - `Direct3d11.vcxproj` + `.filters` — added 4 ItemGroup entries (2 ClCompile + 2 ClInclude).
- **Verification:**
  - `MSBuild -t:Direct3d11` EXIT=0. `gl11_d.dll` 1,397,760 bytes (Plan 11-06 was 1,392,128; +5,632 bytes).
  - `MSBuild -t:Direct3d9` EXIT=0 (D-05 protection — pre-existing MSB8012 carry-forward warnings only).
  - `MSBuild -t:SwgClient` EXIT=0.
  - D-13 grep clean: 0 non-comment hits for `D3DPOOL_MANAGED | OnLostDevice | OnResetDevice` in `Direct3d11/`.
  - D-05 diff empty: `git diff 358fe1a7e..HEAD -- src/engine/client/application/Direct3d9/` returns empty.
  - FFP scan clean: 0 `#ifdef FFP | D3DTSS_ | D3DTOP_` functional code in new files (4 comment lines documenting D-04a omission rationale only).
  - STUB count delta: Plan 11-06 ended at 29 STUBs; Iteration 1 removes 2 (createShaderImplementationGraphicsData + createStaticShaderGraphicsData) → **27 STUBs remaining**.
- **Commits:**
  - `972b02427`: feat(11-07): author Direct3d11_ShaderImplementationData + StaticShaderData wrappers (Iter 1)
  - `ff73b7e11`: feat(11-07): wire shader-implementation + static-shader factory slots (Iter 1)
- **Awaiting:** Kenny smoke under `client_d.cfg [ClientGraphics] rasterMajor=11`. Expected outcomes (in order of likelihood):
  - Most likely: another FATAL further down the engine's load chain. Candidates ranked from highest likelihood:
    1. Point-sprite setter (`setPointSize/Max/Min/ScaleEnable/ScaleFactor/SpriteEnable`) STUBs are 6 of the remaining 27 — fire from the particle subsystem on first emit; may not surface until in-game.
    2. Cursor setter (`setMouseCursor / showMouseCursor`) — fires during UI initialization; might land after `SetupClientGraphics::install` completes.
    3. Visual hard-edge: visible window without geometry (clear-to-color steady state) — most aspirational outcome.
  - Less likely: hard crash without FATAL — would indicate a non-FATAL invariant violation (access violation in a non-traversal-protected codepath). If this surfaces, the call stack from the OS exception handler is the next iteration's first investigation lead.

---

## Iteration 2: D3DCompile X1507 `failed to open source file: vertex_program/include/vertex_shader_constants.inc` (TRE-archived shader include)

- **Date:** 2026-05-17
- **Symptom:** Kenny smoke launch under `client_d.cfg rasterMajor=11` (post-Iter-1) FATAL'd at:
  ```
  FATAL: Direct3d11_VertexShaderData: D3DCompile vs_5_0 'vertex_program/2d.vsh' failed:
    D:\Code\swg-client-v2\stage\vertex_program\2d.vsh(3,10-61): error X1507:
    failed to open source file: 'vertex_program/include/vertex_shader_constants.inc'
  ```
  Crash dump: `stage/SwgClient_d.exe-unknown.0-20260517122231.txt`. Call stack: `ShaderEffectList::install:206/143 → ShaderEffect::install/load:154/97/39 → ShaderImplementationList:99/75 → ShaderImplementation:128/201/407/950/1029/1587/2201/2223/2259 → Graphics.cpp:1699 (createVertexShaderData factory) → Direct3d11_VertexShaderData::ctor → compileOrLoad → D3DCompile`. Triggered while loading `shader/2d_vertexcolor.sht` (per the `ShaderTemplate_Iff:` line at the dump tail). Engine reached `ShaderTemplate::install` further than Iter-1 (Iter-1 fixed the `createShaderImplementationGraphicsData` STUB; Iter-2 surfaces the next problem on the chain — actual shader source compile).
- **Hypothesis:** The error message format `<filepath>(line,col): error X1507` comes from D3DCompile's HLSL parser; the path prefix `D:\Code\swg-client-v2\stage\vertex_program\2d.vsh` is the "virtual filename" Plan 11-05 passed as the `pSourceName` parameter (D3DCompile prepends the source's directory when emitting error labels for relative `#include` directives). The actual `.vsh` source body was loaded correctly by the engine via `TreeFile::open(...)` and passed in via the `pSrcData` parameter — the engine itself does NOT touch `stage/vertex_program/` on disk (that path doesn't exist on the filesystem; `[SharedFile]` cfg points at the SWGSource v3.0 TRE bundle at `D:/Code/SWGSource Client v3.0/`). The `#include` failure is downstream: D3DCompile's parser hit `#include "vertex_program/include/vertex_shader_constants.inc"` on line 3 of `2d.vsh`, called back into its `pInclude` handler to resolve, and got NULL because Plan 11-05 passed `D3D_COMPILE_STANDARD_FILE_INCLUDE` (= sentinel `(ID3DInclude*)1` selecting the default Win32 FS handler that can't see TRE content). Pitfall classification: **novel** (not one of the 8 RESEARCH-documented Pitfalls; it's the D3D9-to-D3D11 include-handler porting analog. D3D9 had its own ID3DXInclude implementation at `Direct3d9_VertexShaderData.cpp:54-156` that Plan 11-05 did NOT port forward).
- **Investigation:**
  - Read crash dump in full — confirmed call stack lands inside `Direct3d11_VertexShaderData` (proves Iter-1's factory wrappers worked; the engine progressed through `createShaderImplementationGraphicsData` and into the next factory call `createVertexShaderData`).
  - Verified `stage/vertex_program/` does NOT exist on disk (`ls` returns "No such file or directory") — confirms hypothesis that `D:\Code\swg-client-v2\stage\vertex_program\2d.vsh` is a synthetic path D3DCompile constructed from Plan 11-05's `displayName` argument + the include-handler's path-resolution context, NOT a real filesystem read.
  - Read `Direct3d11_VertexShaderData.cpp:210-221` — confirmed `D3DCompile` called with `pInclude = D3D_COMPILE_STANDARD_FILE_INCLUDE`. Same pattern at `Direct3d11_PixelShaderProgramData.cpp:85-96` (the `[[maybe_unused]]` helper that exists as ps_5_0 SPEC R3 compile-time proof).
  - Read D3D9 sibling `Direct3d9_VertexShaderData.cpp:54-156` (IncludeHandler) — `Open()` calls `TreeFile::open(pFileName, AbstractFile::PriorityData, true)` and returns the file content via `AbstractFile::readEntireFileAndClose()` (or a cached buffer when `Direct3d9::engineOwnsWindow()`). `Close()` releases the heap buffer.
  - Read `TreeFile::open()` signature (`sharedFile/src/shared/TreeFile.h:85`): `static DLLEXPORT AbstractFile *open(const char *filename, AbstractFile::PriorityType, bool allowFail)`. Confirmed `AbstractFile` has `isOpen() / length() / read() / readEntireFileAndClose()` (returns `unsigned char *` heap-allocated via `new[]`).
  - Verified `sharedFile/include/public` is on the Direct3d11.vcxproj include path (line 93) — no vcxproj include-path addition needed.
  - Verified 0 references to `ID3DInclude` exist anywhere in `Direct3d11/` — no pre-existing handler to extend or replace.
- **Root cause:** Missing `ID3DInclude` implementation. Plan 11-05's `D3DCompile` call sites used `D3D_COMPILE_STANDARD_FILE_INCLUDE` (default Win32 FS resolver) instead of routing through the engine's `TreeFile` abstraction. SWG ships its `.inc` headers inside TRE archives that only `TreeFile` (and its `SearchTree` backing) can see; the on-disk `stage/` directory contains no `vertex_program/include/` subtree.
- **Fix:** Implemented `Direct3d11_CompileIncludeHandler` (new `.h,cpp` pair, ~200 lines) — stateless singleton class implementing `ID3DInclude::Open / Close`:
  - `Open()`: optionally strips a leading `../../` (defensive CLI-compiler-artifact mirror from D3D9 sibling); calls `TreeFile::open(name, AbstractFile::PriorityData, true)`; reads via `readEntireFileAndClose()` (returns heap-allocated `unsigned char *`); deletes the AbstractFile wrapper; returns `S_OK` with `*ppData / *pBytes` populated. Missing file returns `STG_E_FILENOTFOUND` so D3DCompile emits a clean X1507 message (rather than triggering an engine FATAL).
  - `Close()`: `delete[]` the heap buffer that `Open()` returned (D3DCompile contracts to call `Close()` once per `Open()`).
  - Treats `D3D_INCLUDE_LOCAL` (`#include "..."`) and `D3D_INCLUDE_SYSTEM` (`#include <...>`) identically (engine HLSL uses LOCAL exclusively).
  - Wired `Direct3d11_CompileIncludeHandler::getInstance()` into both D3DCompile call sites:
    1. `Direct3d11_VertexShaderData::compileOrLoad` (Iter-2 hot path; replaces the `D3D_COMPILE_STANDARD_FILE_INCLUDE` argument).
    2. `Direct3d11_PixelShaderProgramData::compilePixelShaderFromHlsl` (forward-compat; the helper is `[[maybe_unused]]` today but its contract is now correct for the future Phase 12 asset re-author).
  - Added 2 ClCompile + 2 ClInclude entries in `Direct3d11.vcxproj` + filters under `src\win32`.
- **Verification:**
  - `MSBuild -t:Direct3d11` EXIT=0; `gl11_d.dll` 1,399,808 bytes (Iter-1 baseline 1,397,760 bytes; +2,048 bytes for the new TU).
  - `MSBuild -t:Direct3d9` EXIT=0 (D-05 protection; pre-existing MSB8012 carry-forward warnings only).
  - `MSBuild -t:Direct3d9_ffp` EXIT=0.
  - `MSBuild -t:Direct3d9_vsps` EXIT=0.
  - `MSBuild -t:SwgClient` EXIT=0 (full link clean).
  - **D-13 grep clean:** 0 non-comment hits for `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` in `Direct3d11/` (all 19 hits are inside `//` documentation comments; the new TU's only mention is its own `Per CONTEXT D-13:` comment block at `Direct3d11_CompileIncludeHandler.h:26`).
  - **D-05 diff empty:** `git diff 3e33f4dc1..HEAD -- src/engine/client/application/Direct3d9/` returns 0 lines.
  - **D-04a maintained:** `Glob Direct3d11_FfpGenerator.*` returns 0 results.
  - **FFP scan clean:** new TU has 0 `#ifdef FFP / D3DTSS_ / D3DTOP_` functional code.
  - **STUB count delta:** Iter-1 ended at 27 STUBs; Iter-2 ends at **27 STUBs** (unchanged — this iteration fixes a defect inside Plan 11-05's existing wired-up VS compile path; no new Gl_api slots were wired).
  - **Compile flags / warnings clean:** zero new warnings on Direct3d11 build; `Direct3d11_CompileIncludeHandler.cpp` compiles silent under MSVC /W4.
- **Commits:**
  - `d191a9ac1`: feat(11-07): author Direct3d11_CompileIncludeHandler (Iter 2)
  - `d7e79d955`: fix(11-07): route D3DCompile include resolution through TreeFile (Iter 2)
- **Awaiting:** Kenny smoke under `client_d.cfg rasterMajor=11` (orchestrator handles cfg edit). Expected outcomes (in order of likelihood):
  - **Best case:** VS compile succeeds for `.vsh` assets that `#include` TRE-archived headers; engine progresses past `ShaderImplementation::install`; next FATAL boundary advances to a later slot — possibly:
    - Point-sprite setter (`setPointSize/Max/Min/ScaleEnable/ScaleFactor/SpriteEnable` — 6 of the 27 STUBs remaining; fires on first particle emit).
    - Cursor setter (`setMouseCursor / showMouseCursor` — fires during UI init).
    - PS NULL-fallback running through to first frame (per Plan 11-05/06 design: `PSSetShader` skipped when `m_d3dPS` null → D3D11 default pass-through pixel pipeline rasterizes UNDEFINED color but the draw doesn't FATAL).
    - Visible window with cleared-to-color steady state + possibly UNDEFINED-color geometry (aspirational outcome).
  - **Likely:** another shader-compile FATAL at a different `.vsh`. Candidates:
    - A `.vsh` with a `#include` path the include handler doesn't resolve (e.g., a path that needs different normalisation than the `../../` strip; or a path that doesn't exist in either the TRE archive or any overlay directory).
    - A `.vsh` with HLSL syntax that vs_5_0 rejects (e.g., legacy register-bound syntax `: register(v7)` that compiles under vs_1_1/vs_2_0 but not vs_5_0; or POSITION-as-input-semantic that gets corrupted by the macro per RESEARCH Pitfall 1 — Rule-1 deviation would land a targeted regex fallback).
    - A `.vsh` with `//asm` header (Plan 11-05 leaves NULL VS; Plan 11-06 draw-dispatch path skips it — but only if the shader is actually drawn. If the engine FATAL's on NULL VS during ShaderEffect::install validation rather than at draw time, that's a different fix path).
  - **Less likely:** hard crash from a different code path (access violation in non-traversal-protected codepath — would need OS exception-handler stack to investigate).

---

## Future iterations (placeholders)

To be filled as Kenny surfaces each new FATAL or visual bug. Each entry follows the 6-field shape (Date / Symptom / Hypothesis / Investigation / Root cause / Fix / Verification + Commit).

## Final state (filled at plan close)

- Tatooine outdoor ≥5 min: PENDING
- Cantina interior ≥5 min: PENDING
- All 4 subsystems visible: terrain ?, skeletal ?, particles ?, HUD ?
- Acceptance: PENDING — ready for Plan 08 (DPVS remeasure) + Plan 09 (visual-parity screenshots) at plan close
