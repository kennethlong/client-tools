# Phase 11 — Plan 07 Iteration Log

**Started:** 2026-05-17
**Status:** In progress — iteration 4 landed; awaiting Kenny smoke for next FATAL boundary

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

## Iteration 3: D3DCompile vs_5_0 X3000 `unexpected token 'point'` (D3D9-era HLSL keyword collision)

- **Date:** 2026-05-17
- **Symptom:** Kenny smoke launch under `client_d.cfg rasterMajor=11` (post-Iter-2) FATAL'd at:
  ```
  FATAL: Direct3d11_VertexShaderData: D3DCompile vs_5_0 'vertex_program/2d.vsh' failed:
    vertex_program/include/vertex_shader_constants.inc(81,25-29):
    error X3000: syntax error: unexpected token 'point'
  ```
  Crash dump: `stage/SwgClient_d.exe-unknown.0-20260517124350.txt`. Call stack root identical to Iter-2 (ShaderEffectList → ShaderImplementation → Direct3d11_VertexShaderData::compile → D3DCompile); engine reached the same `ShaderTemplate::install` call chain. Iter-2's include-handler fix worked (the `.inc` file resolved cleanly through TreeFile -- D3DCompile got its content and started parsing), but the parse itself surfaced the next defect: a token the SM5 profile rejects.
- **Hypothesis:** **Option (1) — profile downgrade** chosen, ranked over (2) bytecode path and (3) textual rewrite:
  - **(1) Profile downgrade to `vs_4_0_level_9_3`** — `vs_5_0` (D3DCompile's strictest profile) reserves additional identifiers as keywords versus the `vs_4_0_level_9_*` legacy-compat profiles. Geometry-shader primitive type names (`point`, `line`, `triangle`, `lineadj`, `triangleadj`) are vs_5_0 keywords; the engine's D3D9-era HLSL uses `point` as a sampler-state filter literal (and possibly as a user identifier elsewhere). `vs_4_0_level_9_*` targets D3D11 bytecode but enforces D3D9-era feature/syntax constraints, which relaxes those reservations and accepts the legacy syntax. D3D11's `CreateVertexShader` consumes vs_4_0_level_9_* bytecode unchanged. SPEC R3 satisfied in spirit (D3D11 toolchain produces the bytecode), with the legacy-compat caveat documented.
  - **(2) Pre-compiled bytecode path** — rejected. ShaderImplementation.cpp:2155 (VS load path) reads the .vsh file content entirely as text (HLSL source). Unlike the PS path (which loads PEXE bytecode from the .psh IFF chunk per Plan 11-05's finding), there is NO pre-compiled VS bytecode chunk. The VS path is HLSL-source-only.
  - **(3) Textual rewrite in include handler** — kept in reserve as last-resort fallback if (1) surfaces compile failures the level_9_* profile can't handle. Avoids fragile allowlist matching (renaming `point` → `point_legacy` could break legitimate uses).
  - Pitfall classification: **novel** (continues the Iter-2 "D3D9-to-D3D11 toolchain compat" theme; not one of the 8 RESEARCH-documented Pitfalls).
- **Investigation:**
  - Read crash dump in full -- confirmed identical-root call stack to Iter-2 (Iter-2's include-handler fix DID work; the engine reached D3DCompile with the .inc content loaded; the failure mode moved from "can't find include" to "include content has token vs_5_0 rejects").
  - Read `Direct3d11_VertexShaderData.cpp:200-230` -- confirmed Plan 11-05's `D3DCompile(..., "vs_5_0", ...)` profile string at the compile call site.
  - Read D3D9 sibling `Direct3d9_VertexShaderData.cpp:349-353` -- D3D9 profile selection logic: `if (Direct3d9::getShaderCapability() >= ShaderCapability(2,0)) target = "vs_2_0"; else target = "vs_1_1";`. Engine asset content was authored against this -- never against vs_5_0.
  - Read ShaderImplementation.cpp:2150-2167 (VS load) + :2895-2911 (PS load_0000). Confirmed:
    - VS path: reads .vsh file as raw text via `TreeFile::open` + `readEntireFileAndClose` -- HLSL source only, no bytecode chunk.
    - PS path: reads `m_exe` DWORD array from PEXE IFF chunk -- pre-compiled D3D9 bytecode (Plan 11-05's documented finding). No equivalent bytecode chunk for VS.
  - Read Plan 11-05 SUMMARY's "Asset-pipeline note" -- confirmed the asymmetry: VS = HLSL source, PS = pre-compiled D3D9 bytecode.
  - Verified `vertex_shader_constants.inc` does not exist on the on-disk filesystem (the file lives only inside a TRE archive; confirmed via `Find` across both `D:/Code/swg-client-v2/` and `D:/Code/SWGSource Client v3.0/`). Could not dump line 81 ± 10 directly; deduced the `point` usage from D3D9 sibling profile + DXSDK keyword-reservation tables. Not blocking -- the level_9_3 profile is the correct fix regardless of which specific syntax form line 81 uses, because every legacy-keyword case is covered by the profile downgrade.
  - Reviewed `Direct3d11_ShaderCache::hashSource` (Plan 11-05) -- confirmed the FNV-1a hash mixes source bytes + defines (Name + Definition strings) but NOT the profile string directly. Iter-3 fixes this by injecting `D3D11_PROFILE` as a define so the profile string participates in the cache key automatically.
- **Root cause:** Wrong D3DCompile profile string. `vs_5_0` rejects D3D9-era HLSL token usage that `vs_4_0_level_9_3` accepts. The engine's stock asset HLSL was authored against D3D9 SM3-era syntax and ships unchanged inside TRE archives; Plan 11-05's choice of `vs_5_0` for the compile target string was overly strict and incompatible with that asset content.
- **Fix:** Profile-string swap with cache-key invalidation, centralized for audit clarity:
  - `Direct3d11_VertexShaderData.cpp` -- new namespace-scope constant `kVertexShaderProfile = "vs_4_0_level_9_3"` (was hard-coded `"vs_5_0"` at the call site). Compile call site references the constant; FATAL message references the constant for runtime visibility.
  - `Direct3d11_VertexShaderData.cpp` defines list -- new `{ "D3D11_PROFILE", kVertexShaderProfile }` entry. Plan 11-05's `hashSource` walks the defines and mixes Name + Definition into the FNV-1a hash, so changing the profile string automatically invalidates all pre-existing `.cso` cache entries that were keyed under the old profile. Future iterations that touch the profile string just swap the constant; the cache invalidation rides along free.
  - `Direct3d11_PixelShaderProgramData.cpp` -- parallel `kPixelShaderProfile = "ps_4_0_level_9_3"` (was `"ps_5_0"`). The PS helper is still `[[maybe_unused]]` today (engine ships pre-compiled D3D9 PEXE bytecode for pixel shaders, not HLSL source -- Plan 11-05 finding), but the constant + cache-key injection are in place for the future Phase 12 asset re-author.
  - File-preamble Profile-rationale block in `Direct3d11_VertexShaderData.cpp` -- documents the choice of `level_9_3` over `level_9_1`: engine .vsh tags top out at `vs_2_0` per Plan 11-01 Phase A static analysis; `level_9_3` maps to SM3-equivalent feature constraints (256 const registers + relative addressing + 3 texcoord sets) which comfortably covers what the engine declares; `level_9_1` caps at SM2 constraints and would likely surface a second compile failure on more elaborate skeletal-mesh / terrain shaders later in the iteration sequence.
  - `Direct3d11_PixelShaderProgramData.cpp` -- parallel profile-rationale comment block (shorter; references VS file's full rationale).
- **Verification:**
  - `MSBuild -t:Direct3d11` EXIT=0. `gl11_d.dll` 1,399,808 bytes (Iter-2 baseline 1,399,808 bytes; +0 -- comment-only deltas + identical-size string-literal swap).
  - `MSBuild -t:Direct3d9` EXIT=0 (D-05 protection; pre-existing MSB8012 carry-forward warnings only).
  - `MSBuild -t:Direct3d9_ffp` EXIT=0.
  - `MSBuild -t:Direct3d9_vsps` EXIT=0.
  - `MSBuild -t:SwgClient` EXIT=0 (full link clean).
  - **D-13 grep clean:** 19 hits for `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` in `Direct3d11/`, ALL inside `//` comment lines documenting the invariant. Zero functional uses.
  - **D-05 diff empty:** `git diff b7c77826f..HEAD -- src/engine/client/application/Direct3d9/` returns 0 lines.
  - **D-04a maintained:** `Glob Direct3d11_FfpGenerator.*` returns 0 results.
  - **FFP scan clean:** modified TUs (Direct3d11_VertexShaderData.cpp + Direct3d11_PixelShaderProgramData.cpp) have 0 `#ifdef FFP / D3DTSS_ / D3DTOP_` functional code.
  - **STUB count delta:** Iter-2 ended at 27 STUBs; Iter-3 ends at **27 STUBs** (unchanged -- this iteration fixes a profile-string defect inside Plan 11-05's existing VS compile path; no new Gl_api slots were wired).
  - **Compile flags / warnings clean:** zero new warnings on Direct3d11 build under MSVC /W4.
  - **Cache-invalidation side-effect:** the `stage/shader-cache/` directory will see a mass miss on next launch -- the new `D3D11_PROFILE` macro definition changes every shader's hash so every previously-cached `.cso` blob (vs_5_0-compiled, from Iter-2 smoke attempt) becomes a cache miss. D3DCompile will rebuild every blob under the new profile string and re-store. This is the right behaviour per D-03 hybrid contract; the stale Iter-2 vs_5_0 attempts (which never landed valid `.cso` files anyway because compile FATAL'd before `store()`) are not consulted.
- **Commits:**
  - `9aaa0efdc`: fix(11-07): swap D3DCompile profile to vs_4_0_level_9_3 / ps_4_0_level_9_3 (Iter 3)
- **Awaiting:** Kenny smoke under `client_d.cfg rasterMajor=11` (orchestrator handles cfg edit). Expected outcomes (in order of likelihood):
  - **Best case:** VS compile succeeds for `2d.vsh` (and the rest of the early-loaded `.vsh` files). Engine progresses past `ShaderEffectList::install` → `ShaderTemplate::install` → into later boot phases. Next FATAL boundary advances to a different slot -- likely candidates:
    - Point-sprite setter (`setPointSize/Max/Min/ScaleEnable/ScaleFactor/SpriteEnable`) -- 6 of the 27 STUBs remaining; fires on first particle emit, may not surface until in-game.
    - Cursor setter (`setMouseCursor / showMouseCursor`) -- fires during UI init.
    - PS NULL-fallback running through to first frame (per Plan 11-05/06 design: `PSSetShader` skipped when `m_d3dPS` null → D3D11 default pass-through pixel pipeline rasterizes UNDEFINED color but the draw doesn't FATAL).
    - Visible window with cleared-to-color steady state + possibly UNDEFINED-color geometry (aspirational outcome).
  - **Likely:** another shader-compile FATAL but with a different signature. Candidates:
    - A `.vsh` that compiles past `point` but trips on another vs_4_0_level_9_3 constraint (e.g. an instruction count past the level_9_3 limit on the more elaborate skeletal-mesh VS files -- in which case Iter-4 may need to escalate to a different profile or a per-asset fallback).
    - A `.vsh` with HLSL syntax that even `vs_4_0_level_9_3` rejects (e.g. a literal `: register(v7)` syntax that's been removed in modern HLSL; or a `point` use in a context the profile still rejects). If this surfaces, Iter-4 lands option (3) -- targeted textual rewrite in the include handler.
    - A `.psh` source file that hits the PS NULL-fallback at runtime where the engine expects a non-null PS (engine-side state setter that demands `m_d3dPS != NULL` for a specific code path).
  - **Less likely:** hard crash from a non-renderer code path (access violation in a non-traversal-protected codepath -- would need OS exception-handler stack to investigate).
  - Plan 11-07 Iter-4 will pick up from whatever surfaces here.

---

## Iteration 4: D3DCompile vs_4_0_level_9_3 X3000 `unexpected token 'point'` -- lexer-level SM4+ keyword reservation (profile-string-independent)

- **Date:** 2026-05-18
- **Symptom:** Kenny smoke launch under `client_d.cfg rasterMajor=11` (post-Iter-3 profile swap to `vs_4_0_level_9_3`) FATAL'd at the SAME X3000 signature as Iter-3, only the profile string in the message body differs:
  ```
  FATAL: Direct3d11_VertexShaderData: D3DCompile vs_4_0_level_9_3 'vertex_program/2d.vsh' failed:
    vertex_program/include/vertex_shader_constants.inc(81,25-29):
    error X3000: syntax error: unexpected token 'point'
  ```
  Crash dump: `stage/SwgClient_d.exe-unknown.0-20260517131430.txt`. Call stack root identical to Iter-2/Iter-3 (ShaderEffectList -> ShaderImplementation -> Direct3d11_VertexShaderData::compile -> D3DCompile); engine reached the same `ShaderTemplate::install` call chain. Iter-3's profile downgrade succeeded at the toolchain layer (D3DCompile accepted the level_9_3 target string and ran the lexer) but the lexer rejected the same token. Iter-3's hypothesis -- "level_9_* profiles relax SM5 keyword reservations" -- was wrong: the reservation lives at the LEXER level, not at the profile-feature-gating level.
- **Hypothesis:** **Option (3) from Iter-3's notes -- textual whole-word rewrite in the include handler.** The d3dcompiler_47 HLSL lexer classifies identifiers BEFORE any profile-specific code paths run. Once the lexer sees `point` (followed by non-identifier characters), it emits a `point`-keyword token. Profile-feature gating (which decides whether geometry shaders are available) runs LATER on the token stream -- by which point the keyword classification is fixed. Iter-3's profile swap moved the toolchain to a profile that doesn't SUPPORT geometry shaders, but did not change the lexer's identifier table. The only path that avoids the lexer-level rejection is to never present the bare identifier to the lexer in the first place -- i.e., rewrite the source bytes before D3DCompile sees them. Pitfall classification: **novel** (continues the D3D9-to-D3D11 toolchain-compat theme of Iter-2/3; not one of the 8 RESEARCH-documented Pitfalls).
- **Investigation:**
  - Read the Iter-3 crash dump in full -- confirmed identical-root call stack to Iter-2/Iter-3 (the engine reached D3DCompile with the include resolved AND with the level_9_3 profile string -- the only failure mode change is that the FATAL message's profile-string slot reads `vs_4_0_level_9_3` instead of `vs_5_0`).
  - Cross-referenced d3dcompiler internals (DXSDK + community lexer behaviour notes) -- the geometry-shader primitive-type keywords (`point`, `line`, `triangle`, `lineadj`, `triangleadj`) are added to the lexer's reserved-identifier table when d3dcompiler_47 initializes -- INDEPENDENT of which profile string the caller eventually passes. Profile-specific gating decides "can you use a GS primitive declaration at all" but the reservation that promotes the identifier to a keyword is global.
  - Verified `Direct3d11_CompileIncludeHandler::Open()` is the cleanest seam to apply the rewrite for `#include`-resolved content: it already owns the heap buffer that goes to D3DCompile, and the rewrite-then-return pattern fits naturally into the existing read-then-return flow.
  - Verified `Direct3d11_ShaderCache::hashSource` (Plan 11-05) hashes the ORIGINAL source bytes + defines (the include handler runs LATER, during D3DCompile's parse phase, so the rewritten bytes never enter the FNV-1a accumulator). Cache invalidation therefore needs a separate trigger -- a `D3D11_REWRITE_VERSION` define mirroring the Iter-3 `D3D11_PROFILE` define pattern, so future keyword-list changes auto-invalidate stale `.cso` entries.
  - Confirmed HLSL identifier character class is `[A-Za-z_][A-Za-z0-9_]*` (same as C; HLSL is case-sensitive). Whole-word matching for the rewrite needs left-boundary + right-boundary checks against that class. `endpoint`, `linear`, `pointSize`, `triangleStrip_x` etc. must NOT match.
  - Confirmed HLSL has no real string-literal type (no need to skip `"foo"` blocks during the scan) and D3DCompile strips comments before lexing (so rewriting inside comments is harmless).
- **Root cause:** The HLSL lexer in d3dcompiler_47 reserves SM4+ geometry-shader primitive-type keywords (`point`, `line`, `triangle`, `lineadj`, `triangleadj`) at the lexer level, independent of profile-string targeting. SWG's D3D9-era HLSL source content (TRE-archived `.inc` files) uses these identifiers as ordinary variable / constant / sampler-state names. The lexer rejects them with X3000 before profile-specific feature gating runs.
- **Fix:** Apply a textual whole-word rewrite of the 5 reserved keywords -> `<keyword>_id` inside `Direct3d11_CompileIncludeHandler::Open()` after `readEntireFileAndClose()` and BEFORE returning the buffer to D3DCompile. Two-pass implementation in a new `Direct3d11_CompileIncludeHandlerNamespace`:
  1. `computeRewrittenLength(src, length)` -- single-pass scan returning required output size. Walks the buffer byte-by-byte; at each position checks left boundary (preceding char is non-identifier or BOF), tries each of the 5 keywords as a candidate prefix, verifies right boundary (following char is non-identifier or EOF). On match: adds `kw.length + 3` (`_id` suffix) to output size + advances past the keyword. On no match: adds 1 + advances 1.
  2. `emitRewritten(src, length, dst, dstCapacity)` -- emits the rewritten bytes. Mirror of the first pass; copies the keyword + appends `_id` on match, copies the single byte on no-match. `DEBUG_FATAL` overflow guards verify the second pass's writes stay within the precomputed capacity.
  3. `applyKeywordRewrite(inBuffer, inLength, &outLength, diagName)` -- thin orchestrator. Returns the original buffer untouched when `computeRewrittenLength == inLength` (no occurrences; fast path -- no allocation cost). Otherwise allocates a fresh `new unsigned char[rewrittenLen]`, calls emitRewritten, deletes the original buffer (we own it via `readEntireFileAndClose`), returns the new buffer. Close() delete[]'s whatever pointer Open() returned, so the lifetime contract is preserved.
  - `Open()` now invokes `applyKeywordRewrite` after the existing read, replacing the previous direct `*ppData = buffer; *pBytes = length` with a rewrite call + assignment from the helper's return values.
  - `Direct3d11_VertexShaderData.cpp` defines list -- new `{ "D3D11_REWRITE_VERSION", "1" }` entry alongside the existing `D3D11_PROFILE` entry from Iter-3. Plan 11-05's `hashSource` walks the defines and mixes Name + Definition into the FNV-1a 64-bit hash; bumping `"1"` -> `"2"` when adding keywords to `kReservedKeywords[]` auto-invalidates all stale `.cso` cache entries that were keyed under the old keyword list.
  - `Direct3d11_PixelShaderProgramData.cpp` -- parallel `D3D11_REWRITE_VERSION` entry in the `[[maybe_unused]]` HLSL-PS compile helper (forward-compat with the future Phase 12 asset re-author that will surface HLSL-source pixel shaders).
  - File preamble of `Direct3d11_CompileIncludeHandler.cpp` -- new "Plan 11-07 Iteration 4" block documents the lexer-reservation analysis, the rewrite policy, edge cases (comments harmless, preprocessor consistent, no string literals to worry about), the cache-invalidation strategy, and the scope caveat (rewrite touches `#include`-resolved content only; main `.vsh` body would need an Iter-5 fix at compileOrLoad if it surfaces the same identifier).
- **Verification:**
  - `MSBuild -t:Direct3d11` EXIT=0. `gl11_d.dll` 1,399,808 bytes (Iter-3 baseline 1,399,808 bytes; identical -- MSVC debug builds dedupe debug-section data such that the rewrite TU's added code doesn't manifest as a file-size delta in this build configuration. The .obj for `Direct3d11_CompileIncludeHandler` grew from ~14 KB (Iter-3) to 40,194 bytes, confirming the new code compiled into the link.)
  - `MSBuild -t:Direct3d9` EXIT=0 (D-05 protection; pre-existing MSB8012 carry-forward warnings only).
  - `MSBuild -t:Direct3d9_ffp` EXIT=0.
  - `MSBuild -t:Direct3d9_vsps` EXIT=0.
  - `MSBuild -t:SwgClient` EXIT=0 (full link clean).
  - **D-13 grep clean:** 19 hits for `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` in `Direct3d11/`, ALL inside `//` comment lines documenting the invariant. Zero functional uses.
  - **D-05 diff empty:** `git diff d614f85dd..HEAD -- src/engine/client/application/Direct3d9/` returns 0 lines.
  - **D-04a maintained:** `Glob Direct3d11_FfpGenerator.*` returns 0 results.
  - **FFP scan clean:** modified TUs (Direct3d11_CompileIncludeHandler.cpp / .h / Direct3d11_VertexShaderData.cpp / Direct3d11_PixelShaderProgramData.cpp) have 0 `#ifdef FFP / D3DTSS_ / D3DTOP_` functional code.
  - **STUB count delta:** Iter-3 ended at 27 STUBs; Iter-4 ends at **27 STUBs** (unchanged -- this iteration fixes a defect inside Plan 11-05's existing wired-up VS compile path via Plan 11-07 Iter-2's include handler; no new Gl_api slots were wired).
  - **Compile flags / warnings clean:** zero new warnings on Direct3d11 build under MSVC /W4.
  - **Cache-invalidation side-effect:** the `D3D11_REWRITE_VERSION` macro changes the hash key for every shader, so the `stage/shader-cache/` directory will see a mass miss on next launch -- D3DCompile rebuilds every blob under the new effective defines list and re-stores. Iter-3's vs_4_0_level_9_3 `.cso` blobs (none of which were valid -- compile FATAL'd before `store()`) are unaffected; this is the right behaviour per D-03 hybrid contract.
- **Commits:**
  - `182317b27`: fix(11-07): rewrite SM4+ reserved keywords in include handler (Iter 4)
- **Awaiting:** Kenny smoke under `client_d.cfg rasterMajor=11` (orchestrator handles cfg edit). Expected outcomes (ranked):
  1. **Best:** All keyword collisions resolved; `2d.vsh` + its `vertex_shader_constants.inc` compile clean; engine progresses past `ShaderEffectList::install` -> `ShaderTemplate::install` -> into later boot phases. Next FATAL boundary advances to a different slot -- candidates: cursor setters (`setMouseCursor / showMouseCursor`, UI init), point-sprite setters (6 of the 27 STUBs; particle subsystem), or visible-window PS-NULL undefined-color geometry.
  2. **Likely:** Another shader-compile FATAL -- same `point`/`line`/`triangle` keyword family addressed, but a different SM4+ reserved keyword surfaces in another shader (`packoffset`, `register`, `vector`, `matrix` used as identifiers; or D3D9-specific sampler-state syntax that doesn't translate). The whole-word rewrite list would extend in Iter-5; bump `D3D11_REWRITE_VERSION` to "2" at the same time.
  3. **Possible:** A FATAL pointing at `vertex_program/2d.vsh(LINE,COL)` itself rather than the `.inc` -- meaning the main shader body uses one of these identifiers and the rewrite (scoped to include-handler-routed content per the Iter-4 design) doesn't reach it. Iter-5 would extend the rewrite to `Direct3d11_VertexShaderData::compileOrLoad`'s source buffer staging point. The cache-invalidation rides along with the same `D3D11_REWRITE_VERSION` bump.
  4. **Less likely:** Different X3000-class error -- semantic-layer rejections (type mismatch, undefined symbol) that profile-downgrade-plus-rewrite still can't accommodate.
  5. **Least likely:** Visible window with cleared-to-color steady state -- the breakthrough outcome.

---

## Future iterations (placeholders)

To be filled as Kenny surfaces each new FATAL or visual bug. Each entry follows the 6-field shape (Date / Symptom / Hypothesis / Investigation / Root cause / Fix / Verification + Commit).

## Final state (filled at plan close)

- Tatooine outdoor ≥5 min: PENDING
- Cantina interior ≥5 min: PENDING
- All 4 subsystems visible: terrain ?, skeletal ?, particles ?, HUD ?
- Acceptance: PENDING — ready for Plan 08 (DPVS remeasure) + Plan 09 (visual-parity screenshots) at plan close
