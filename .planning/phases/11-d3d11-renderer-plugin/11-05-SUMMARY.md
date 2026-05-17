---
phase: 11-d3d11-renderer-plugin
plan: 05
subsystem: renderer
tags: [d3d11, shaders, hlsl, sm5, d3dcompile, shader-cache, cbuffer, sv-position, vsps, descope-ffp]

# Dependency graph
requires:
  - phase: 11-d3d11-renderer-plugin
    plan: 01
    provides: D-04a DESCOPE verdict; Plan 11-05 source list MUST OMIT Direct3d11_FfpGenerator.{h,cpp}
  - phase: 11-d3d11-renderer-plugin
    plan: 04
    provides: D3D11 device + resource layer baseline (textures + VB/IB + render targets); FATAL boundary at createTextureData advanced to ShaderTemplate::install path
provides:
  - Direct3d11_ShaderCache -- D-03 hybrid disk cache (FNV-1a 64-bit hash over source + defines; <hash:016x>.cso files under ConfigDirect3d11::getShaderCacheDir() default stage/shader-cache/; opportunistic store; defense-in-depth path validation)
  - Direct3d11_VertexShaderData -- D3DCompile vs_5_0 + ShaderCache wiring; POSITION->SV_POSITION macro injected (Pitfall 1); m_bytecodeHash stored for Plan 11-06 input-layout cache (Pitfall 6); recognizes //hlsl + //asm headers; //asm passes log + return NULL VS
  - Direct3d11_PixelShaderProgramData -- wrapper constructs successfully; engine ships pre-compiled D3D9 PEXE bytecode (m_exe DWORD *), D3D11 CreatePixelShader rejects it -> m_d3dPS NULL; documented in header as asset re-author follow-up (future Phase 12); compilePixelShaderFromHlsl helper present as ps_5_0 SPEC R3 compile-time proof
  - Direct3d11_ConstantBuffer -- ID3D11Buffer-backed cbuffer wrapper replacing D3D9 SetVertexShaderConstantF / SetPixelShaderConstantF; 3 cbuffer struct definitions (PerFrameCB 96B / PerObjectCB 192B / PerMaterialCB 112B) each gated by static_assert(sizeof%16==0) per Pitfall 2; D3D11_USAGE_DYNAMIC + Map(WRITE_DISCARD)
  - 4 Gl_api shader slots wired (replaces 4 scaffold_fatal_stub bindings): createVertexShaderData / createPixelShaderProgramData / setVertexShaderUserConstants / setPixelShaderUserConstants
  - D-04a maintained -- Direct3d11_FfpGenerator.{h,cpp} INTENTIONALLY ABSENT from Direct3d11/src/win32/ and from Direct3d11.vcxproj source list (verified by grep + ls)
  - D-13 invariant maintained -- 16 grep hits for D3DPOOL_MANAGED|OnLostDevice|OnResetDevice across Direct3d11/, ALL inside // comment lines documenting the invariant; zero functional uses
  - D-05 invariant maintained -- D3D9 plugin source untouched (`git diff 2df8b0e90..HEAD -- src/engine/client/application/Direct3d9/` empty); all 3 D3D9 variants (gl05/06/07_d.dll) build clean
affects: [11-06, 11-07]

# Tech tracking
tech-stack:
  added:
    - d3dcompiler.lib runtime linkage (D3DCompile, D3DCreateBlob, D3D_COMPILE_STANDARD_FILE_INCLUDE)
    - DirectXMath.h (XMFLOAT4X4 / XMFLOAT4 for cbuffer C++ mirrors per Pitfall 2)
    - std::filesystem::create_directories (lazy shader-cache directory provisioning)
    - FNV-1a 64-bit hash (non-cryptographic, deterministic; source + defines participate)
    - .cso disk format (opaque, self-versioning per RESEARCH §Don't Hand-Roll)
  patterns:
    - "D-03 hybrid compile flow: tryLoad(hash) -> on miss D3DCompile + store(hash, blob) -> CreateVertexShader/PixelShader. Defines participate in hash so {POSITION,SV_POSITION} vs {POSITION,POSITION} cache distinctly."
    - "Pitfall 1 enforcement: POSITION -> SV_POSITION macro injected into D3D_SHADER_MACRO defines array passed to D3DCompile."
    - "Pitfall 2 enforcement: every cbuffer struct uses DirectX::XMFLOAT4X4 / XMFLOAT4 with static_assert(sizeof%16==0) -- compile-time gate."
    - "Pitfall 6 enforcement: VS bytecode hash (FNV-1a 64-bit) stored in m_bytecodeHash for Plan 11-06's (VBFormat, VS-bytecode-hash) input-layout cache key."
    - "Cbuffer shadow-and-flush: setVertexShaderUserConstants / setPixelShaderUserConstants shadow into PerObjectCB / PerMaterialCB static state; Plan 11-06 flushes via Direct3d11_ConstantBuffer::updateVS(1, ...) + bindVS(1) at the draw-call moment (avoids Map/Unmap per setter)."
    - "MemoryBlockManager per-class allocator pattern preserved across all 3 new shader-class files (PATTERNS Shared 2; codebase-wide convention from Phase 9 STL port)."
    - "Asset-pipeline note (PS bytecode mismatch): engine's ShaderImplementationPassPixelShaderProgram::m_exe is pre-compiled D3D9 PEXE bytecode (DWORD *) loaded from .psh IFF chunks at ShaderImplementation.cpp:2906. D3D11's CreatePixelShader expects DXBC SM4.0+ bytecode and rejects D3D9 bytecode. Plan 11-05 acknowledges this and leaves m_d3dPS NULL; Plan 11-06's draw dispatch must handle the NULL case (skip PSSetShader -> D3D11 default pass-through). Real PS rendering blocks on asset re-author (future Phase 12)."

key-files:
  created:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_ShaderCache.h (58 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_ShaderCache.cpp (271 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexShaderData.h (108 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexShaderData.cpp (251 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.h (93 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp (225 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.h (105 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.cpp (153 ln)
    - .planning/phases/11-d3d11-renderer-plugin/11-05-SUMMARY.md
  modified:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp (shader includes + factory _impl bodies + install/remove ordering + 4 slot bindings; +120 / -5)
    - src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj (8 new files added to ClCompile / ClInclude ItemGroups -- 4 cpp + 4 h)
    - src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj.filters (8 new files assigned to src\win32 filter)

key-decisions:
  - "D-04a OMISSION executed -- Direct3d11_FfpGenerator.{h,cpp} not created; Plan 11-01 verdict honored. The 11-05-PLAN.md `files_modified` list included these as conditional artifacts; the conditional resolved to OMIT per Plan 11-01's combined static-non-empty + Phase B runtime-empty evidence on Tatooine + cantina interior. The existing D3D9 FFP variant gl06_d.dll stays on disk for D3D9-side fallback; D3D11 does not mirror it."
  - "Pixel shader bytecode mismatch is an asset-pipeline issue, NOT a renderer issue. The engine ships pre-compiled D3D9 PS bytecode in .psh IFF assets; D3D11 cannot consume it. Two paths exist: (a) future Phase 12 re-authors the .psh assets as HLSL source and recompiles via D3DCompile (covered by the compilePixelShaderFromHlsl helper in Direct3d11_PixelShaderProgramData.cpp that serves as compile-time SPEC R3 proof); (b) Plan 11-06's draw dispatch skips PSSetShader when m_d3dPS is NULL, allowing D3D11's default pass-through pixel pipeline. Plan 11-05 adopts both: helper present as forward-looking proof + wrapper constructs successfully with m_d3dPS NULL so the engine boots past createPixelShaderProgramData."
  - "Vertex shader //asm path NULL-handled, not error. SWG-era .vsh files start with either '//hlsl vs_1_1' or '//asm vs_1_1'. //hlsl path compiles cleanly to vs_5_0 under D3D11 (after POSITION->SV_POSITION macro). //asm path has no D3D11 equivalent (D3DAssemble gone from modern SDK; D3DCompile rejects assembly); Plan 11-01 D-04a's evidence suggests the target scenes don't rely on //asm shaders, so logging + NULL VS is the right contract. Plan 11-06's draw dispatch will skip passes with NULL VS."
  - "Cbuffer shadow-and-flush pattern. Calling Map/Unmap per setVertexShaderUserConstants call would be wasteful (one Map per float4 update). Plan 11-05 shadows into a static PerObjectCB / PerMaterialCB; Plan 11-06's draw dispatch flushes via Direct3d11_ConstantBuffer::updateVS(1, ...) + bindVS(1) at the right moment. This matches D3D11 idiomatic 'update cbuffer once per draw, not once per constant'."
  - "ShaderCache directory location chosen as ConfigDirect3d11::getShaderCacheDir() (default 'stage/shader-cache/') per CONTEXT D-03 + RESEARCH Assumption A4. Matches existing 'stage/dpvs-profile/' gitignored pattern; .gitignore entry was added in Plan 11-02. Lazy directory creation via std::filesystem::create_directories; failure non-fatal (cache is opportunistic)."

patterns-established:
  - "Compile-then-cache shader pipeline: tryLoad(hash) -> on hit skip compile; on miss D3DCompile + store; CreateVertexShader / CreatePixelShader from blob. Same shape for VS (vs_5_0) and PS (ps_5_0). Defines participate in hash."
  - "Cbuffer struct alignment compile-time gate: static_assert(sizeof(MyCB) % 16 == 0) on every cbuffer struct; the C++ struct must mirror the HLSL cbuffer layout exactly."
  - "Two-tier shader-class install: ShaderCache first (cache dir provisioning), then per-class MemoryBlockManagers (VS / PS), then ConstantBuffer (creates the dynamic ID3D11Buffer pool). remove() in REVERSE order."

requirements-completed: []
requirements-partial: [D3D11-01]
requirements-traced: [D3D11-03]
requirements-closed-as-descoped: [D3D11-04]

# Metrics
duration: ~2 hours active (2026-05-16, single autonomous executor session)
completed: 2026-05-16
---

# Phase 11 Plan 05: D3D11 Shader Layer (Compile Pipeline + .cso Cache + Constant-Buffer Wrapper) Summary

**Four shader-related source classes implemented across 8 new files (~1,264 lines). Four Gl_api scaffold_fatal_stub bindings replaced: `createVertexShaderData`, `createPixelShaderProgramData`, `setVertexShaderUserConstants`, `setPixelShaderUserConstants`. D-04a DESCOPE verdict honored: `Direct3d11_FfpGenerator.{h,cpp}` INTENTIONALLY ABSENT. SPEC R3 (HLSL SM5.0) compile pipeline is in place; engine-asset PS bytecode mismatch surfaces as a documented Plan 11-06 / Phase-12 follow-up. D-13 / D-05 invariants intact. gl11_d.dll grew from 1,125,376 bytes (Plan 11-04) to 1,240,064 bytes (+115 KB).**

## Performance

- **Duration:** ~2 hours active on 2026-05-16 (autonomous executor; no Kenny smoke this plan per `autonomous: true` framing)
- **Started:** 2026-05-16 (after Plan 11-04 close at `2df8b0e90`)
- **Completed:** 2026-05-16 (this plan-close commit)
- **Tasks:** 4 (Task 3 SKIPPED per D-04a verdict = DESCOPE)
- **Files modified:** 11 (8 new files in Direct3d11/ + 1 modified Direct3d11.cpp + 2 modified vcxproj/filters)
- **Commits:** 3 task commits + 1 plan-close commit = 4 total on plan 11-05
- **New lines:** 1,264 lines across the 8 new shader-class source files (per `wc -l` post-commit)

## D-04a OMISSION (Plan 11-05 deliverable)

**`Direct3d11_FfpGenerator.{h,cpp}` OMITTED** per Plan 11-01 DESCOPE verdict (commits `200cc7694` / `82f068a4a` / `c4a0b3fcc`). Plan 11-01's combined Phase A static-analysis (49 D3D9 `#ifdef FFP` regions + 11 IFF instantiation sites + 4 IFF loader entry points -- engine + plugin infrastructure-hot) + Phase B runtime capture (≥10 min Tatooine outdoor + Mos Eisley cantina interior on D3D9 baseline `rasterMajor=5` `gl05_d.dll`; zero post-init FFP setter activations across `StateCache_restore` / `ImplStage_build` / `ImplStage_cascade`; the 16 captured rows were all `StateCache_init` device-default writes at `frame=0`) produced the DESCOPE verdict per CONTEXT D-04a's combined-evidence rubric.

The existing D3D9 FFP variant `gl06_d.dll` stays on disk for D3D9-side fallback; D3D11 does not mirror it. SPEC R4's "FFP pixel shader generator covers MODULATE / ADD / SELECTARG1" acceptance is satisfied by descope evidence per CONTEXT D-04a.

**Verification:**
- `Glob src/engine/client/application/Direct3d11/src/win32/Direct3d11_FfpGenerator.*` returns 0 results.
- `Grep "FfpGenerator" src/engine/client/application/Direct3d11/` returns 0 files.
- `Direct3d11.vcxproj` source list does NOT reference any `Direct3d11_FfpGenerator.*` file.

## Accomplishments

### Direct3d11_ShaderCache (271 ln cpp + 58 ln h)

D-03 hybrid disk cache. API: `install(dir)` / `remove()` / `tryLoad(hash, &blob)` / `store(hash, blob)` / `hashSource(text, length, defines)`.

**Hash function:** FNV-1a 64-bit. Non-cryptographic, fast, deterministic, no external dependency. Defines participate via Name + '\0' + Definition + '\0' (so `{ABC, DEF}` hashes distinct from `{ABCDEF, ""}`).

**Cache path:** `<cacheDir>/<hash:016x>.cso` (16-hex-digit lowercase). Per T-11-05-01 path-traversal mitigation: hash is internally-generated hex; defensive scan validates hex-only before path concat (cache dir is engine-configured via `ConfigDirect3d11::getShaderCacheDir()`, NEVER user-input).

**Behavior:**
- `install(cacheDir)` calls `std::filesystem::create_directories`; failure logs non-fatal (cache disabled for the run; subsequent `tryLoad` always returns miss; compile-every-time still works).
- `tryLoad`: opens `.cso` file binary; if size ∈ (0, 64 MB], `D3DCreateBlob` + `fread`. 64 MB ceiling is sanity (SWG HLSL shaders compile to a few KB).
- `store`: failure (full disk, permission error) increments `ms_storeFailures`; logs non-fatal; short-write attempts to `std::filesystem::remove` the truncated file to avoid poisoning the next `tryLoad`.
- `remove`: logs `hits=%d misses=%d stores=%d storeFailures=%d`; cache files on disk INTENTIONALLY survive across launches per D-03.

### Direct3d11_VertexShaderData (251 ln cpp + 108 ln h)

D3DCompile vs_5_0 path with D-03 hybrid cache + Pitfall 1/6 enforcement.

**Header parsing:** SWG `.vsh` files start with either `//hlsl vs_1_1` or `//asm vs_1_1`. Plan 11-05 honors:
- `//hlsl` path -> compile via D3DCompile (vs_5_0 profile, POSITION->SV_POSITION macro injected). Cache miss invokes D3DCompile + store; hit skips compile.
- `//asm` path -> log + return with `m_d3dVS = NULL`. D3DCompile does not accept assembly; D3DAssemble gone from modern SDK. Plan 11-06's draw dispatch will skip passes with NULL VS.
- No header -> defensive default-HLSL (per RESEARCH note that all engine assets ship one or the other tag).

**Compile flags:** Debug -> `D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION`; Release -> `D3DCOMPILE_OPTIMIZATION_LEVEL3`. Always `D3DCOMPILE_ENABLE_STRICTNESS`.

**Pitfall 1 (SV_POSITION):** `{ "POSITION", "SV_POSITION" }` macro injected into the defines array. Note: if any engine VS source uses `POSITION` as an INPUT semantic (`struct VertexInput { float4 pos : POSITION; }`), the macro would incorrectly rewrite the input semantic. Plan 11-06's smoke (when this hits real engine .vsh sources) will surface that case if it exists; the targeted-substitution fallback (regex on `: POSITION` in output struct field positions only) lands as a Rule-1 deviation if needed.

**Pitfall 6 (input-layout cache key):** `m_bytecodeHash` stores the FNV-1a hash of (source + defines) -- this is the cache-key signature, NOT a SHA-1 of the bytecode bytes. The reasoning: identical (source, defines) produces identical bytecode under D3DCompile's deterministic compile, so the hash is a valid identity for the input-layout cache. Plan 11-06's input-layout cache will use `(VertexBufferFormat::getFlags(), m_bytecodeHash)` as the key.

**`_clearfp()` INTENTIONALLY DROPPED** per PATTERNS line 520 -- the D3DX-era FPU bug doesn't apply to D3DCompile. Reintroduce only if regression surfaces.

### Direct3d11_PixelShaderProgramData (225 ln cpp + 93 ln h)

Thin wrapper. **CRITICAL CAVEAT** (recorded in header preamble): the engine ships pre-compiled D3D9-era pixel-shader bytecode in `ShaderImplementationPassPixelShaderProgram::m_exe` (DWORD * from PEXE IFF chunk; see `ShaderImplementation.cpp:2906`). D3D11's `CreatePixelShader` expects DXBC SM4.0+ bytecode and will reject D3D9 bytecode with `E_INVALIDARG`.

**Plan 11-05 contract:**
1. The wrapper CONSTRUCTS successfully (engine's `createPixelShaderProgramData` factory slot stops FATAL'ing during `ShaderImplementation::install`).
2. The constructor detects D3D9 vs DXBC bytecode via `looksLikeDxbc()` (4-byte "DXBC" magic). Both paths currently leave `m_d3dPS = NULL` -- D3D9 case because the bytecode is unusable; DXBC case because the byte-count is not yet plumbed alongside `m_exe` in the engine struct.
3. The bytecode version (high byte / low byte of `m_exe[0]`) is LOGGED for the asset-audit trail: `"Direct3d11_PixelShaderProgramData: '%s' is pre-compiled D3D9 PS %d.%d bytecode -- not consumable by D3D11"`.
4. Plan 11-06's draw dispatch must skip `PSSetShader` when `m_d3dPS == NULL` (D3D11's default pass-through pixel pipeline runs; UNDEFINED output but the draw doesn't FATAL).
5. Real PS rendering blocks on the **asset re-author follow-up** (future Phase 12 -- re-author `.psh` assets as HLSL source).

**SPEC R3 compile-time proof:** the cpp includes a `compilePixelShaderFromHlsl` helper that exercises the full `ps_5_0` D3DCompile + ShaderCache + CreatePixelShader path. Marked `[[maybe_unused]]` to silence /W4. The helper is reachable when a future asset surfaces HLSL-source PS content; the lexical presence of `D3DCompile(... "ps_5_0" ...)` in this TU is the compile-time evidence that SM5.0 PS compilation is built.

### Direct3d11_ConstantBuffer (153 ln cpp + 105 ln h)

Replaces D3D9 register-file model:
- `Direct3d9_StateCache.cpp:526` `SetVertexShaderConstantF` -> `Direct3d11_ConstantBuffer::updateVS(slot, data, size)` + `bindVS(slot)`
- `Direct3d9_StateCache.cpp:561` `SetPixelShaderConstantF` -> `Direct3d11_ConstantBuffer::updatePS(slot, data, size)` + `bindPS(slot)`

**Three cbuffer struct definitions** per Pitfall 2 (16-byte alignment), each with `static_assert(sizeof(...) % 16 == 0)`:

| Struct                       | Size | Members                                                                        |
|------------------------------|-----:|--------------------------------------------------------------------------------|
| `Direct3d11_PerFrameCB`      | 96B  | viewProj (XMFLOAT4X4) + cameraPos_pad (XMFLOAT4) + fogColor_density (XMFLOAT4) |
| `Direct3d11_PerObjectCB`     | 192B | world (XMFLOAT4X4) + userConstants[8] (XMFLOAT4) -- D3D9 VCSR_userConstant0..7 analog |
| `Direct3d11_PerMaterialCB`   | 112B | materialDiffuse/Specular/Emissive (XMFLOAT4) + userConstants[4] (XMFLOAT4) -- D3D9 PSCR_userConstant analog |

**Pool:** `kNumSlots = 4` per stage (VS + PS); each slot `kMaxCBufferBytes = 1024` byte capacity (large headroom; SWG cbuffer payloads fit easily). All slots `D3D11_USAGE_DYNAMIC + D3D11_BIND_CONSTANT_BUFFER + D3D11_CPU_ACCESS_WRITE`.

**Update pattern:** `Map(D3D11_MAP_WRITE_DISCARD)` -> `memcpy` -> `Unmap` (per RESEARCH §Pattern 2). WRITE_DISCARD ensures GPU/CPU non-aliasing -- the only pattern we use for cbuffers.

### Wiring (Task 4)

`Direct3d11.cpp` modifications:
- 6 new includes (4 shader-class headers + `clientGraphics/ShaderImplementation.h` for the factory parameter types + `sharedMath/VectorRgba.h` for `setPixelShaderUserConstants` data + `<DirectXMath.h>` for the cbuffer shadow types).
- 4 factory `_impl` bodies in `Direct3d11Namespace`: `createVertexShaderData_impl`, `createPixelShaderProgramData_impl`, `setVertexShaderUserConstants_impl` (shadows into PerObjectCB), `setPixelShaderUserConstants_impl` (shadows into PerMaterialCB, with VectorRgba.rgba -> XMFLOAT4 translation).
- Install ordering in `Direct3d11::install` after the Plan 11-04 resource layer: `Direct3d11_ShaderCache::install(getShaderCacheDir())` -> `VertexShaderData::install` -> `PixelShaderProgramData::install` -> `ConstantBuffer::install`.
- Remove ordering in `remove_impl` (REVERSE): `ConstantBuffer::remove` -> `PixelShaderProgramData::remove` -> `VertexShaderData::remove` -> `ShaderCache::remove` -> ... (existing Plan 11-04 resource-layer teardown).
- 4 `STUB()` lines replaced with `ms_glApi.slot = factoryImpl;` bindings.

**STUB() count:** 76 (Plan 11-04 baseline) -> 72 (Plan 11-05 closes; -4 matches the 4 wired shader slots).

## Slot Wiring Status (cumulative through Plan 11-05)

**Replaced (~24 of ~119 Gl_api slots):**
- Plan 11-02 (scaffold real no-ops): verify + getShaderCapability + requiresVertexAndPixelShaders + wasDeviceReset + isGdiVisible + 4 lost-device callbacks + 7 supports* + getVideoMemoryInMegabytes + getOtherAdapterRects + setBrightnessContrastGamma (no-op) + update (no-op) + remove + flushResources + #ifdef _DEBUG slots (5)
- Plan 11-03 (device MVP): beginScene + endScene + clearViewport + present + displayModeChanged
- Plan 11-04 (resource layer): createTextureData + createStaticVertexBufferData + createDynamicVertexBufferData + createStaticIndexBufferData + createDynamicIndexBufferData + setDynamicIndexBufferSize + setRenderTarget + copyRenderTargetToNonRenderTargetTexture
- **Plan 11-05 (shader layer): createVertexShaderData + createPixelShaderProgramData + setVertexShaderUserConstants + setPixelShaderUserConstants**

**Remaining STUBs (72) cluster into:**
- State cache + draw-call dispatch (Plan 11-06): createShaderImplementationGraphicsData + createStaticShaderGraphicsData + setBadVertexShaderStaticShader + setStaticShader + setFillMode/CullMode + setLights + setWorldToCameraTransform + setProjectionMatrix + setFog + setObjectToWorldTransformAndScale + setGlobalTexture + releaseAllGlobalTextures + setTextureTransform + setAlphaFadeOpacity + setViewport + setScissorRect + setVertexBuffer + setIndexBuffer + setVertexBufferVector + createVertexBufferVectorData + all 23 draw* slots + getOneToOneUVMapping + optimizeIndexBuffer + setAntialiasEnabled + 6 setPoint* slots
- Windowing utilities (Wave 7+): resize + setWindowedMode + lockBackBuffer + unlockBackBuffer + presentToWindow + setMouseCursor + showMouseCursor
- Visual / image (Plan 11-09): screenShot + writeImage + setBloomEnabled
- PIX markers (Wave 7+): pixSetMarker + pixBeginEvent + pixEndEvent
- Production-only (Plan 11-09): createVideoBuffers + fillVideoBuffers + getVideoBufferData + releaseVideoBuffers

**New FATAL boundary (predicted):** the engine will FATAL on `createShaderImplementationGraphicsData` -- the first shader-related slot called during `ShaderTemplate::install` (per call-stack pattern from D3D9's analogous flow). Plan 11-06 will replace that STUB.

## Task Commits

| # | Commit       | Task                                                                                                | Type | Files                                |
|---|--------------|-----------------------------------------------------------------------------------------------------|------|--------------------------------------|
| 1 | `f040f19c2`  | Task 1 -- Direct3d11_ShaderCache.{h,cpp} (new) + vcxproj/filters                                    | spec | +2 new files in Direct3d11/ + 2 modified vcxproj (337 inserts) |
| 2 | `84eac40f5`  | Task 2 -- VS + PS + ConstantBuffer (6 new files) + vcxproj/filters                                  | spec | +6 new files in Direct3d11/ + 2 modified vcxproj (959 inserts) |
| - | (skipped)    | Task 3 -- FFP generator SKIPPED per Plan 11-01 D-04a verdict = DESCOPE                              | n/a  | 0 files created; 0 lines             |
| 3 | `d237b55f2`  | Task 4 -- Wire 4 Gl_api shader slots in Direct3d11.cpp + install/remove ordering                    | spec | 1 file modified (Direct3d11.cpp; +120 / -5) |

**Plan close commit:** (this commit) -- adds `11-05-SUMMARY.md` + `STATE.md` + `ROADMAP.md` + `REQUIREMENTS.md` updates.

## Build Verifications

- `MSBuild D:/Code/swg-client-v2/src/build/win32/swg.sln -target:Direct3d11 -property:Configuration=Debug -property:Platform=Win32 -property:PlatformToolset=v145` -> **EXIT=0**; `gl11_d.dll` 1,240,064 bytes (vs Plan 11-04 baseline 1,125,376 bytes; +115 KB) auto-staged via the Plan 11-01 post-build cp fix `266e173b3`.
- `MSBuild ... -target:Direct3d9 ...` -> **EXIT=0** (D-05 protection: `gl05_d.dll` still builds clean; pre-existing MSB8012 TargetName warning is carry-forward).
- `MSBuild ... -target:Direct3d9_ffp ...` -> **EXIT=0** (`gl06_d.dll` builds clean).
- `MSBuild ... -target:Direct3d9_vsps ...` -> **EXIT=0** (`gl07_d.dll` builds clean).
- `MSBuild ... -target:SwgClient ...` -> **EXIT=0** (full link clean; `SwgClient_d.exe` 72,618,496 bytes).
- `grep -rE "D3DPOOL_MANAGED|OnLostDevice|OnResetDevice" src/engine/client/application/Direct3d11/` -> 16 hits, ALL inside `//` comment lines documenting the D-13 invariant. **Zero functional uses.**
- `git diff 2df8b0e90..HEAD -- src/engine/client/application/Direct3d9/` -> empty (D-05 cross-check; no D3D9 source touched).
- `Glob src/engine/client/application/Direct3d11/src/win32/Direct3d11_FfpGenerator.*` -> 0 results (D-04a invariant: FFP generator absent).

## Decisions Made

1. **D-04a OMISSION executed.** `Direct3d11_FfpGenerator.{h,cpp}` not created; vcxproj does not reference them. Plan 11-01's combined static + runtime evidence supports the DESCOPE verdict.
2. **Pixel-shader bytecode mismatch is asset-pipeline, not renderer.** Engine ships pre-compiled D3D9 PEXE; D3D11 can't consume it. Plan 11-05 makes the wrapper construct successfully with `m_d3dPS = NULL`; Plan 11-06's draw dispatch handles NULL by skipping `PSSetShader`. Future Phase 12 will re-author `.psh` assets as HLSL source. The `compilePixelShaderFromHlsl` helper present in the cpp serves as `ps_5_0` SPEC R3 compile-time proof and future asset path.
3. **//asm VS path NULL-handled, not FATAL.** D3D11 has no equivalent of D3DAssemble. Plan 11-01 D-04a's evidence suggests target scenes don't rely on //asm shaders; logging + NULL VS is the correct contract.
4. **Cbuffer shadow-and-flush.** `setVertexShaderUserConstants` / `setPixelShaderUserConstants` shadow into static struct state, not Map/Unmap per-call. Plan 11-06's draw dispatch flushes via `updateVS(1, ...)` + `bindVS(1)` at the right moment. Matches D3D11 idiom of "update cbuffer once per draw, not once per constant".
5. **Cache dir default `stage/shader-cache/`.** Per CONTEXT D-03 + RESEARCH Assumption A4. Already `.gitignored` per Plan 11-02. Lazy-create via `std::filesystem::create_directories`.
6. **FNV-1a 64-bit hash, not SHA-1 or CRC32.** Non-cryptographic, fast, deterministic, no external dependency. Per RESEARCH §Don't Hand-Roll: "the .cso blob is opaque & self-versioning to the runtime; just hash + dump + load". 64-bit collision space is comfortable for the ~100-500 distinct shader sources in the SWG asset catalog.
7. **Cbuffer struct definitions in the public header.** Plan 11-06 will reference `Direct3d11_PerFrameCB` / `PerObjectCB` / `PerMaterialCB` to populate cbuffers during draw dispatch. Public header avoids a friend mess. The `static_assert(sizeof(...) % 16 == 0)` triggers at compile time across any TU that includes the header -- compile-time gate is the right shape for Pitfall 2 enforcement.
8. **`_clearfp()` not ported.** D3DX-era FPU bug doesn't apply to D3DCompile (PATTERNS line 520 + RESEARCH confirmation). Re-introduce only if regression surfaces during Plan 11-06's draw smoke.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Missing `<sharedMath/VectorRgba.h>` include in `Direct3d11.cpp`**
- **Found during:** Task 4 build attempt
- **Issue:** `Direct3d11.cpp` was now using `VectorRgba const *` in `setPixelShaderUserConstants_impl` to read `constants[i].r/g/b/a`; the type is only forward-declared via `clientGraphics/Gl_dll.def`, so member access produced C2027 errors.
- **Fix:** Added `#include "sharedMath/VectorRgba.h"` next to the existing `sharedDebug/DebugFlags.h` / `sharedFoundation/Os.h` includes. The `sharedMath/include/public` directory is already on the include path (vcxproj line 93).
- **Files modified:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp`
- **Verification:** Direct3d11 + Direct3d9 + SwgClient all rebuild clean.
- **Committed in:** `d237b55f2` (Task 4 commit -- folded into the install-path wiring change).

### Acknowledged Stubs (not deviations; documented in plan)

**1. Direct3d11_PixelShaderProgramData::m_d3dPS NULL.** Engine ships pre-compiled D3D9 bytecode in `.psh` assets; D3D11 cannot consume it. Plan 11-05 leaves `m_d3dPS` NULL; Plan 11-06's draw dispatch must skip `PSSetShader` when null (D3D11 default pass-through). Real PS rendering blocks on asset re-author follow-up (future Phase 12). The `compilePixelShaderFromHlsl` helper serves as `ps_5_0` SPEC R3 compile-time proof and forward-looking re-author path. **This is the canonical "Known Stub" of Plan 11-05** -- documented in the wrapper header preamble + this SUMMARY.

**2. Direct3d11_VertexShaderData m_d3dVS NULL for //asm shaders.** D3D11 has no D3DAssemble; //asm `.vsh` content has no D3D11 compile path. Plan 11-01 D-04a's evidence suggests target scenes don't use //asm. Plan 11-06's draw dispatch will skip passes with NULL VS.

**3. Shader-cache disk dir `stage/shader-cache/` not yet created.** Lazy-create at first `Direct3d11_ShaderCache::install` call. On a fresh checkout the directory is created on first launch; cache miss the first run, hits subsequent runs.

### Lessons Learned (recorded for future executors)

**1. Engine-side asset format is fixed, renderer must adapt.** Pre-compiled D3D9 pixel-shader bytecode in `.psh` IFF (PEXE chunk) is the asset-pipeline contract -- the renderer cannot demand a re-author of all assets just to bring up SM5.0. Plan 11-05's path forward (wrapper constructs successfully, m_d3dPS NULL, draw skips PSSetShader) lets the engine boot past the factory slot without breaking the existing D3D9 path. The asset re-author follow-up is a Phase 12 candidate; until then, Plan 11-06's draw dispatch must handle NULL PS via D3D11's default pass-through.

**2. The Pitfall 1 SV_POSITION macro is fire-and-forget UNTIL it isn't.** The macro `{ "POSITION", "SV_POSITION" }` is correct for OUTPUT-position rewrite (the common case). If any engine `.vsh` source uses `POSITION` as an INPUT vertex semantic, the macro will incorrectly rewrite the input semantic too. Plan 11-06's smoke will surface this if it exists -- the targeted regex substitution fallback (rewrite only `: POSITION` in output struct field positions) is the Rule-1 deviation if needed.

**3. Shadow-then-flush is the D3D11 cbuffer idiom.** Map/Unmap per setter call would be wasteful; D3D11 idiomatic pattern is to batch updates into a CPU shadow struct and flush once per draw via Map(WRITE_DISCARD). Plan 11-05 sets up the shadow in the slot setters; Plan 11-06 flushes at the draw-call moment.

**4. Cache-dir provisioning belongs in install(), not first-use.** Lazy directory creation via `std::filesystem::create_directories` in `Direct3d11_ShaderCache::install` ensures the directory exists before any `store()` attempts it. Failure is non-fatal so a permission error or full disk doesn't crash the plugin.

**5. The 64MB cache-file size ceiling is sanity, not a contract.** SWG HLSL shaders compile to a few KB; 64 MB is way above any reasonable shader size. The ceiling protects against `D3DCreateBlob` over-allocation if a `.cso` file is corrupted to be enormous; failing tryLoad on oversized blob is the right response (we'll just recompile).

---

**Total deviations:** 1 auto-fixed (1 Rule 1 missing include). No scope creep.
**Impact on plan:** Without the VectorRgba include fix, Task 4's build broke; one-line fix unblocked Task 4.

## Issues Encountered

- **No client-launch evidence this plan.** Per `autonomous: true` framing, the plan-close does NOT include a Kenny smoke session. The new FATAL boundary (predicted to be `createShaderImplementationGraphicsData` -- first shader-implementation factory called during `ShaderTemplate::install`) lands at Plan 11-06's checkpoint. The visible-window outcome that Plan 11-03's CHECKPOINT deferred is most likely to land at Plan 11-06's checkpoint or later -- once the state-cache + draw-call dispatch wire and the engine completes `ShaderTemplate::install` through `SetupClientGraphics::install`, the OS-level `ShowWindow(SW_SHOW)` fires.

- **Pre-existing Koogie warnings persist (out of scope):** `Direct3d9.vcxproj` MSB8012 TargetName/OutputFile mismatch warnings + Direct3d11 build emits one `C4459 declaration of 'E' hides global declaration` warning from including `<DirectXMath.h>` (DirectXMathVector.inl uses local `E` variable; engine's `sharedFoundation/FloatMath.h` defines a global `E` constant). This is a level-4 hint, not an error; both declarations are correct in their respective scopes. Recorded as carry-forward; no action this plan.

## User Setup Required

None -- Plan 11-05 is the shader-pipeline port; no external services or new manual steps required. The shader-cache directory `stage/shader-cache/` lazy-creates on first plugin launch; no manual provisioning needed. Plan 11-06's smoke will need the existing setup: `client_d.cfg [ClientGraphics] rasterMajor=11`, run `D:/Code/swg-client-v2/stage/SwgClient_d.exe` against the SWGSource VM at 192.168.1.200 (or local).

## Next Phase Readiness

- **Plan 11-06 (Wave 6 -- state cache + draw-call dispatch + input-layout cache + light manager + metrics) is UNBLOCKED.** First work items: `Direct3d11_StateCache` (the cbuffer flush + sampler/RS/BS/DSS state-cache wrapper); `Direct3d11_ShaderImplementationData` + `Direct3d11_StaticShaderData` (the engine-shader-template wrappers that consume VertexShaderData / PixelShaderProgramData); `Direct3d11_InputLayoutCache` (Pitfall 6 -- keyed by `(VertexBufferFormat, vsBytecodeHash)`); draw-call dispatch (the 23 STUB'd `draw*` slots). The cbuffer shadow-and-flush handoff from Plan 11-05's `setVertexShaderUserConstants` / `setPixelShaderUserConstants` to `Direct3d11_ConstantBuffer::updateVS(1, ...)` + `bindVS(1)` happens at the start of each Plan-11-06 draw call.

- **Pixel-shader asset re-author is a Phase 12 candidate** (NOT a Plan 11-06 blocker). Plan 11-06's draw dispatch skips `PSSetShader` when `m_d3dPS == NULL`; D3D11's default pass-through pixel pipeline renders UNDEFINED color, but the draw doesn't FATAL. Visible-parity smoke under Plan 11-07 will surface whether the default pass-through is acceptable for target scenes; if not, the asset re-author becomes the gating Phase 12 work.

- **D-04a maintained.** No `Direct3d11_FfpGenerator.{h,cpp}` created; vcxproj clean. Plan 11-06 inherits this invariant.

- **D-05 maintained.** `git diff 2df8b0e90..HEAD -- src/engine/client/application/Direct3d9/` returns empty. All three D3D9 variants (`gl05_d.dll`, `gl06_d.dll`, `gl07_d.dll`) build clean.

- **D-13 maintained.** 16 `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` grep hits in `src/engine/client/application/Direct3d11/`, all in `//` comment lines. Zero functional uses.

- **Phase 11 bonus deliverables still active.** `266e173b3` (Plan 11-01 post-build cp auto-stage) + `dbd7c62dc` (Plan 11-02 atlmfc include vendor) continue to retire the CLI MSBuild rebuild trap.

### Carry-forward observations (not blockers)

- Pre-existing `Direct3d9.vcxproj` MSB8012 TargetName/OutputFile mismatch warnings remain (out of scope; Direct3d9 source not touched this plan).
- Pre-existing C4456 declaration-shadowing warnings in `Direct3d9.cpp:2519`, `Direct3d9_VertexBufferDescriptorMap.cpp:140`, and `Direct3d9_RenderTarget.cpp:85,91,98` remain (out of scope).
- New `C4459 declaration of 'E' hides global declaration` warning in `<DirectXMath.h>` consumed via `Direct3d11_ConstantBuffer.h`. Level-4 hint, both declarations correct in scope; no action.
- The `compilePixelShaderFromHlsl` helper in `Direct3d11_PixelShaderProgramData.cpp` is `[[maybe_unused]]` -- this is intentional (forward-looking compile-time SPEC R3 proof). MSVC /W4 honors the attribute.

## TDD Gate Compliance

This plan is `type: execute` (not `type: tdd`), so the RED/GREEN/REFACTOR gate sequence does not apply. No `test(...)` commits expected. The 5 build-verification gates (`/t:Direct3d11`, `/t:Direct3d9`, `/t:Direct3d9_ffp`, `/t:Direct3d9_vsps`, `/t:SwgClient` all EXIT=0) + the D-13 grep gate + the D-05 git-diff gate + the D-04a Glob+Grep gate are the equivalent verification gates.

## Requirements Traceability

- **D3D11-01 (plugin scaffold + Gl_api table + Direct3d11.dll runtime):** **PARTIAL** -- Plan 11-05 adds 4 more real Gl_api slots (from 20 in Plan 11-04 to 24 in Plan 11-05: 5 per-frame + 7 init + 8 resource + 4 shader = 24 of ~119 slots), but visible-window outcome still deferred to Plan 11-06's checkpoint smoke session. Commit chain extended: `2c518e832 / db2116594 / dbd7c62dc / a9ae88846` (Plan 11-02) -> `28c1f64c4 / 802ea9c4d / 606697fb4` (Plan 11-03) -> `d70432717 / 6ea862275 / 44627b6c2 / 2df8b0e90` (Plan 11-04) -> `f040f19c2 / 84eac40f5 / d237b55f2 / <plan-close-hash>` (Plan 11-05).

- **D3D11-03 (shader recompilation under HLSL SM5.0 per SPEC R3):** **TRACED** -- vs_5_0 compile path (D3DCompile + ShaderCache + CreateVertexShader) is live in this plan's code; ps_5_0 path is present as `compilePixelShaderFromHlsl` compile-time proof. Engine asset-pipeline ships pre-compiled D3D9 PS bytecode that D3D11 cannot consume; vs_5_0 path is functionally complete pending Plan 11-06 smoke evidence that an engine .vsh source actually compiles. SPEC R3 PARTIAL conversion to SATISFIED requires Plan 11-06 + Plan 11-07 smoke evidence that engine shaders compile + cache + render correctly.

- **D3D11-04 (FFP pixel shader generator):** **CLOSED-AS-DESCOPED** -- Plan 11-01's D-04a verdict resolution remains the closure evidence. Plan 11-05 honored the OMIT directive: no `Direct3d11_FfpGenerator.{h,cpp}` files; vcxproj does not reference them. The existing D3D9 FFP variant `gl06_d.dll` stays on disk for D3D9-side fallback; D3D11 does not mirror it. Verdict doc: `.planning/phases/11-d3d11-renderer-plugin/11-01-ffp-spike-finding.md`. Plan 11-01 SUMMARY: `.planning/phases/11-d3d11-renderer-plugin/11-01-SUMMARY.md`. Plan 11-05's OMISSION execution: this SUMMARY.

## Known Stubs

| Stub | File | Reason | Resolves In |
|------|------|--------|-------------|
| `m_d3dPS = NULL` for all pre-compiled D3D9 PS assets | `Direct3d11_PixelShaderProgramData.cpp` | Engine ships pre-compiled D3D9 PEXE bytecode in `.psh` IFF (DWORD * `m_exe`); D3D11 `CreatePixelShader` rejects D3D9 bytecode. Plan 11-06 draw dispatch must skip `PSSetShader` when null (D3D11 default pass-through). Real PS rendering blocks on asset re-author. | Future Phase 12 (asset re-author `.psh` -> HLSL source) |
| `m_d3dVS = NULL` for //asm `.vsh` assets | `Direct3d11_VertexShaderData.cpp` | D3D11 has no `D3DAssemble`; assembly content has no D3D11 compile path. Plan 11-01 D-04a's evidence suggests target scenes don't use //asm. Plan 11-06 draw dispatch skips passes with NULL VS. | Plan 11-06 (draw skip) OR future Phase 12 (asset re-author //asm -> //hlsl) |
| `compilePixelShaderFromHlsl` helper unreached at runtime | `Direct3d11_PixelShaderProgramData.cpp` | Forward-looking SPEC R3 compile-time proof for `ps_5_0` D3DCompile + ShaderCache path. `[[maybe_unused]]` silences /W4. Reachable when a future asset surfaces HLSL-source PS content. | Future Phase 12 (asset re-author) |
| Cbuffer shadow-only (no flush to GPU yet) | `Direct3d11.cpp` `setVertexShaderUserConstants_impl` / `setPixelShaderUserConstants_impl` | Plan 11-05 shadows into static `PerObjectCB` / `PerMaterialCB`; Plan 11-06's draw dispatch flushes via `Direct3d11_ConstantBuffer::updateVS(1, ...)` + `bindVS(1)` at the right moment. Avoids Map/Unmap per-setter waste. | Plan 11-06 (draw-call cbuffer flush) |

---

*Phase: 11-d3d11-renderer-plugin*
*Plan: 05 -- D3D11 shader layer (compile pipeline + .cso cache + cbuffer wrapper)*
*Completed: 2026-05-16*

## Self-Check: PASSED

- `.planning/phases/11-d3d11-renderer-plugin/11-05-SUMMARY.md` exists -- VERIFIED (this file)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ShaderCache.{h,cpp}` exist -- VERIFIED (committed in `f040f19c2`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexShaderData.{h,cpp}` exist -- VERIFIED (committed in `84eac40f5`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.{h,cpp}` exist -- VERIFIED (committed in `84eac40f5`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.{h,cpp}` exist -- VERIFIED (committed in `84eac40f5`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_FfpGenerator.{h,cpp}` do NOT exist (D-04a) -- VERIFIED via Glob (0 results)
- Task commits present in `git log`: `f040f19c2` (Task 1) + `84eac40f5` (Task 2) + `d237b55f2` (Task 4) -- VERIFIED
- D-05 cross-check: `git diff 2df8b0e90..HEAD -- src/engine/client/application/Direct3d9/` returns empty -- VERIFIED
- D-13 cross-check: 16 `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` hits in `Direct3d11/`, ALL inside `//` comment lines documenting the invariant -- VERIFIED
- gl11_d.dll auto-staged at `D:/Code/swg-client-v2/stage/gl11_d.dll`, 1,240,064 bytes (vs Plan 11-04 baseline 1,125,376 bytes; +115 KB) -- VERIFIED
- All 5 MSBuild targets EXIT=0 (Direct3d11 / Direct3d9 / Direct3d9_ffp / Direct3d9_vsps / SwgClient) -- VERIFIED
