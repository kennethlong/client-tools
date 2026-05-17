---
phase: 11-d3d11-renderer-plugin
plan: 04
subsystem: renderer
tags: [d3d11, resources, textures, vertex-buffer, index-buffer, render-target, comptr]

# Dependency graph
requires:
  - phase: 11-d3d11-renderer-plugin
    plan: 03
    provides: D3D11 device + DXGI flip-model swap chain + clear-to-color MVP; FATAL boundary at createTextureData (clean baseline at HEAD 606697fb4)
provides:
  - Direct3d11_TextureData -- ID3D11Texture2D/Cube/Texture3D + paired ID3D11ShaderResourceView; full DXGI_FORMAT translation table covering all 17 TF_* engine formats + 2 sentinels
  - Direct3d11_StaticVertexBufferData / Direct3d11_StaticIndexBufferData -- USAGE_DEFAULT VB/IB + UpdateSubresource on unlock
  - Direct3d11_DynamicVertexBufferData / Direct3d11_DynamicIndexBufferData -- USAGE_DYNAMIC ring buffers with Map WRITE_DISCARD on wrap / WRITE_NO_OVERWRITE on append (Pitfall 5)
  - Direct3d11_RenderTarget -- persistent 512x512 BIND_RENDER_TARGET|BIND_SHADER_RESOURCE bake surface + setRenderTarget routing for user-supplied textures + CopySubresourceRegion read-back (no system-memory intermediate)
  - Direct3d11_VertexBufferDescriptorMap -- per-VertexBufferFormat descriptor cache (no D3DFVF deps; D3D11 uses ID3D11InputLayout in Plan 11-05/06)
  - 8 Gl_api factory / re-target / setSize slots wired (replaces 8 scaffold_fatal_stub bindings) -- createTextureData / createStaticVertexBufferData / createDynamicVertexBufferData / createStaticIndexBufferData / createDynamicIndexBufferData / setDynamicIndexBufferSize / setRenderTarget / copyRenderTargetToNonRenderTargetTexture
  - Engine-side Texture.h: 'friend class Direct3d11_TextureData' on TextureGraphicsData::LockData (Rule 3 deviation; mirrors existing Direct3d8/Direct3d9 friend declarations)
  - D-13 invariant maintained -- zero functional D3DPOOL_MANAGED / OnLostDevice / OnResetDevice in Direct3d11/ source
  - D-05 invariant maintained -- D3D9 plugin source untouched; gl05_d.dll still builds clean
affects: [11-05, 11-06, 11-07, 11-09]

# Tech tracking
tech-stack:
  added:
    - ID3D11Texture2D / ID3D11Texture3D + ID3D11ShaderResourceView (ComPtr ownership)
    - ID3D11Buffer (4 use cases: static VB, dynamic VB ring, static IB, dynamic IB ring)
    - ID3D11RenderTargetView (RTV) on Direct3d11_TextureData and on Direct3d11_RenderTarget bake surface
    - Staging textures (D3D11_USAGE_STAGING + CPU_ACCESS_WRITE/READ) for engine lock/unlock translation
    - DXGI_FORMAT translation table for 17 TF_* engine formats + CheckFormatSupport probe in install
    - IDXGIAdapter::GetDesc DedicatedVideoMemory query for VB ring sizing
    - Microsoft::WRL::ComPtr ownership across all 12 new files (Pattern D-01)
  patterns:
    - "Staging-texture lock/unlock pattern: USAGE_DEFAULT resources can't be Mapped directly; lock() allocates a CPU buffer (or staging texture); unlock() blits via UpdateSubresource (buffers) or CopySubresourceRegion (textures). Engine signature (lock returns void*) is preserved verbatim."
    - "Dynamic ring buffer pattern (RESEARCH Pitfall 5): wrap -> Map(WRITE_DISCARD), reset used=0; append -> Map(WRITE_NO_OVERWRITE), advance used. Static `ms_used` accounting mirrors D3D9 plugin's pattern (size/used/newFrame/lock counters)."
    - "BIND_RENDER_TARGET|BIND_SHADER_RESOURCE on the same texture: D3D11 idiom for render-then-sample. Replaces D3D9's render-target-texture + system-memory-texture pair; CopySubresourceRegion replaces D3D9 GetRenderTargetData + D3DXLoadSurfaceFromSurface."
    - "Engine TextureFormatInfo::setSupported probe via CheckFormatSupport (replaces D3D9's CheckDeviceFormat). Marks unsupported formats so the engine falls through to alternate runtimeFormats."
    - "MemoryBlockManager per-class allocator pattern preserved verbatim across all 5 new resource classes (PATTERNS Shared 2; codebase-wide convention from Phase 9 STL port)."
    - "ms_-prefixed namespace state for ring-buffer accounting -- same shape as D3D9 plugin (PATTERNS Shared 3) but typed for ComPtr."
    - "No createRenderTarget Gl_api slot: render-target textures are produced via createTextureData when Texture::isRenderTarget()=true; BindFlags include BIND_RENDER_TARGET so the same ID3D11Texture2D serves as RTV + SRV. setRenderTarget routes draws to it."

key-files:
  created:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_TextureData.h (122 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_TextureData.cpp (614 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_RenderTarget.h (37 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_RenderTarget.cpp (299 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticVertexBufferData.h (87 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticVertexBufferData.cpp (170 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.h (108 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.cpp (280 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticIndexBufferData.h (76 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticIndexBufferData.cpp (135 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicIndexBufferData.h (100 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicIndexBufferData.cpp (211 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexBufferDescriptorMap.h (38 ln; helper)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexBufferDescriptorMap.cpp (151 ln; helper)
    - .planning/phases/11-d3d11-renderer-plugin/11-04-SUMMARY.md
  modified:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp (8 STUB bindings replaced with factory slot bindings + factory_impl bodies + install/remove ordering)
    - src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj (14 new files added to ClCompile / ClInclude ItemGroups -- 7 cpp + 7 h)
    - src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj.filters (14 new files assigned to src\win32 filter)
    - src/engine/client/library/clientGraphics/src/shared/Texture.h (Rule 3 deviation: 'friend class Direct3d11_TextureData' added to TextureGraphicsData::LockData; mirrors existing Direct3d8/Direct3d9 friend declarations; zero impact on D3D9 plugin behaviour)

key-decisions:
  - "Translation table covers ALL 17 TF_* engine formats + 2 sentinels (not just the 10 the plan listed). TF_RGB_888 / TF_DXT2 / TF_DXT4 / TF_P_8 map to DXGI_FORMAT_UNKNOWN; CheckFormatSupport in install() marks them unsupported and engine falls through to alternate runtimeFormats. TF_RGB_565 / TF_RGB_555 get real DXGI mappings (B5G6R5_UNORM / B5G5R5A1_UNORM)."
  - "Static buffer USAGE = D3D11_USAGE_DEFAULT (not USAGE_IMMUTABLE) because the engine fills the buffer via lock() AFTER CreateBuffer (mesh / texture load path). IMMUTABLE requires initial-data on create. Engine lock() returns a CPU staging block; unlock() blits via UpdateSubresource. Same rationale applies to static textures."
  - "Dynamic VB ring sized via IDXGIAdapter::GetDesc.DedicatedVideoMemory (replaces D3D9's getVideoMemoryInMegabytes). Always-shader path (no FFP branch per Plan 11-01 D-04a DESCOPE)."
  - "Dynamic IB ring uses parameterless createDynamicIndexBufferData(); ring size controlled by setDynamicIndexBufferSize Gl_api slot which calls Direct3d11_DynamicIndexBufferData::setSize -- recreates the ring at the new size. Default 256 KB / sizeof(Index) until engine calls setSize."
  - "Index format DXGI_FORMAT_R16_UINT supplied at IASetIndexBuffer time per draw call (D3D11 divergence from D3D9's index-format-on-create). Engine Index = unsigned short per Graphics.def. IASetIndexBuffer wires in Plan 11-06."
  - "Direct3d11_RenderTarget owns a persistent 512x512 BGRA8 baked-texture surface with both BIND_RENDER_TARGET and BIND_SHADER_RESOURCE -- one ID3D11Texture2D, dual usage. CopySubresourceRegion copies into the user texture on bake-end (no system-memory intermediate; D3D11 vs D3D9 divergence)."
  - "No Gl_api createRenderTarget slot: render-target textures are produced via createTextureData when Texture::isRenderTarget()=true. The Plan 11-04 spec narrative referenced 'createRenderTarget' in error; the actual factory surface is the texture-creation path. setRenderTarget routes draws; copyRenderTargetToNonRenderTargetTexture handles read-back."
  - "Rule 3 deviation: add Direct3d11_TextureData as a friend class of TextureGraphicsData::LockData. The engine-side LockData fields (m_pixelData / m_pitch / m_slicePitch / m_reserved / m_cubeFace / m_format) are private with friends Texture, Direct3d8_TextureData, Direct3d9_TextureData. Adding Direct3d11_TextureData mirrors the existing pattern; zero impact on D3D9 plugin behaviour or any other call site. This is a one-line engine header edit (Texture.h:35) that the Plan 11-04 spec did not anticipate."
  - "Direct3d11_VertexBufferDescriptorMap is a tiny standalone helper -- mirrors Direct3d9_VertexBufferDescriptorMap minus the dead D3DFVF_TEXTUREFORMAT lookup table. The Direct3d9 D3DFVF entries were unused in the FVF-codepath itself; preserving the descriptor cache (per-format vertexSize / offset table) is correct for both backends. D3D11 vertex declaration (ID3D11InputLayout) lands in Plan 11-05/06 -- not here."

patterns-established:
  - "Resource-class install in dependency order: descriptor-map first (consumed by VB/IB ctors), then per-resource-class MemoryBlockManagers, finally the render-target manager. remove() in REVERSE order so ComPtr Release runs while the device is still alive."
  - "Factory _impl thin wrapper pattern (Direct3d11Namespace::createXxx_impl) calling `new Direct3d11_XxxData(args)`: keeps Gl_api binding free of class-name leakage into the slot-binding section of Direct3d11::install. Reads cleanly alongside the existing STUB() macro lines."
  - "Engine-side LockData friend-class addition is the canonical Rule-3 deviation for any new TextureData backend; we set the precedent (D3D11) and expect any future D3D12 plugin to add the same one-line friend."

requirements-completed: []
requirements-partial: [D3D11-02]
requirements-traced: [D3D11-01]

# Metrics
duration: ~3 hours active (2026-05-17, single autonomous executor session)
completed: 2026-05-17
---

# Phase 11 Plan 04: D3D11 Resource Layer (Textures + Vertex/Index Buffers + Render Targets) Summary

**Six resource types implemented across 14 new source files; 8 Gl_api scaffold_fatal_stub bindings replaced with real factory slots. The plugin can now construct and hold the texture and geometry resources the engine pushes in. D-13 / D-05 invariants intact; gl11_d.dll grew from 1,046,016 bytes (Plan 11-03) to 1,125,376 bytes (+77 KB). Visible-window evidence deferred to Plan 11-05's checkpoint smoke session per plan's autonomous-mode framing.**

## Performance

- **Duration:** ~3 hours active across 2026-05-17 (autonomous executor; no Kenny smoke this plan)
- **Started:** 2026-05-17 (after Plan 11-03 close at `606697fb4`)
- **Completed:** 2026-05-17 (this plan-close commit)
- **Tasks:** 3 auto (no checkpoint per `autonomous: true`)
- **Files modified:** 16 (14 new files in Direct3d11/ + 1 modified Direct3d11.cpp + 1 engine-side Texture.h friend-class addition + 2 modified vcxproj/filters)
- **Commits:** 3 task commits + 1 plan-close commit = 4 total on plan 11-04
- **New lines:** 2,909 lines across the 14 new resource source files (per `wc -l` post-commit)

## Accomplishments

### Direct3d11_TextureData (614 ln cpp + 122 ln h)

Owns `ID3D11Texture2D` (or `ID3D11Texture3D` for volume maps) + paired `ID3D11ShaderResourceView` via `ComPtr`. Cube maps use `ArraySize=6` + `D3D11_RESOURCE_MISC_TEXTURECUBE`; SRV uses `D3D11_SRV_DIMENSION_TEXTURECUBE`.

**DXGI_FORMAT translation table** covers all 17 `TF_*` engine formats + 2 sentinels (`static_assert` checks length matches `TF_Count + 2`). Mapping highlights:

| TextureFormat   | DXGI_FORMAT                        | Notes                                                            |
|-----------------|------------------------------------|------------------------------------------------------------------|
| TF_ARGB_8888    | DXGI_FORMAT_B8G8R8A8_UNORM         | matches engine PackedArgb convention                             |
| TF_ARGB_4444    | DXGI_FORMAT_B4G4R4A4_UNORM         | Win8+; install probe marks unsupported on older                  |
| TF_ARGB_1555    | DXGI_FORMAT_B5G5R5A1_UNORM         | --                                                               |
| TF_XRGB_8888    | DXGI_FORMAT_B8G8R8X8_UNORM         | --                                                               |
| TF_RGB_888      | DXGI_FORMAT_UNKNOWN                | no 24bpp packed in DXGI; engine promotes via runtimeFormats list |
| TF_RGB_565      | DXGI_FORMAT_B5G6R5_UNORM           | --                                                               |
| TF_RGB_555      | DXGI_FORMAT_B5G5R5A1_UNORM         | closest DXGI; alpha bit unused                                   |
| TF_DXT1         | DXGI_FORMAT_BC1_UNORM              | --                                                               |
| TF_DXT2         | DXGI_FORMAT_UNKNOWN                | premultiplied BC2; no native DXGI variant; engine promotes       |
| TF_DXT3         | DXGI_FORMAT_BC2_UNORM              | --                                                               |
| TF_DXT4         | DXGI_FORMAT_UNKNOWN                | premultiplied BC3; engine promotes                               |
| TF_DXT5         | DXGI_FORMAT_BC3_UNORM              | --                                                               |
| TF_A_8          | DXGI_FORMAT_A8_UNORM               | --                                                               |
| TF_L_8          | DXGI_FORMAT_R8_UNORM               | sample with .rrr swizzle in HLSL (Plan 11-05 generators)         |
| TF_P_8          | DXGI_FORMAT_UNKNOWN                | palettized normal-map; D3D11 has no P_8; engine promotes         |
| TF_ABGR_16F     | DXGI_FORMAT_R16G16B16A16_FLOAT     | --                                                               |
| TF_ABGR_32F     | DXGI_FORMAT_R32G32B32A32_FLOAT     | --                                                               |
| TF_Count        | DXGI_FORMAT_UNKNOWN                | sentinel                                                         |
| TF_Native       | DXGI_FORMAT_UNKNOWN                | sentinel                                                         |

`install()` runs `CheckFormatSupport(D3D11_FORMAT_SUPPORT_TEXTURE2D)` against every functional entry and calls `TextureFormatInfo::setSupported()` so the engine knows which formats it can request at runtime. Mirrors D3D9 plugin's `CheckDeviceFormat` probe shape.

**USAGE / BindFlags matrix** per resource type:

| Use case        | USAGE                     | BindFlags                                                 | CPUAccessFlags          |
|-----------------|---------------------------|-----------------------------------------------------------|-------------------------|
| Static texture  | `D3D11_USAGE_DEFAULT`     | `D3D11_BIND_SHADER_RESOURCE`                              | 0                       |
| Dynamic texture | `D3D11_USAGE_DYNAMIC`     | `D3D11_BIND_SHADER_RESOURCE`                              | `D3D11_CPU_ACCESS_WRITE`|
| Render target   | `D3D11_USAGE_DEFAULT`     | `D3D11_BIND_SHADER_RESOURCE \| D3D11_BIND_RENDER_TARGET`  | 0                       |

`USAGE_IMMUTABLE` would be ideal for static textures but requires initial data at create-time. The engine fills the resource via `lock()` AFTER `CreateBuffer`/`CreateTexture2D` (mesh / texture load path), so `IMMUTABLE` is not workable. The staging-texture pattern (`D3D11_USAGE_STAGING` + `CPU_ACCESS_WRITE/READ`) handles `lock()` returns; `unlock()` blits via `CopySubresourceRegion` for textures or `UpdateSubresource` for buffers.

Per Pitfall 4: SRV is independent from sampler -- this file creates the SRV; sampler bind lives in the state-cache plan (Plan 11-06).

`MemoryBlockManager` + `GlobalTextureList` preserved per `PATTERNS Shared 2` / codebase-wide convention.

### Direct3d11_StaticVertexBufferData + Direct3d11_StaticIndexBufferData (170+135 ln cpp)

Per-mesh `ComPtr<ID3D11Buffer>` (USAGE_DEFAULT + BIND_VERTEX_BUFFER / BIND_INDEX_BUFFER). Engine `lock()` returns a CPU staging block sized for the buffer; `unlock()` blits via `UpdateSubresource(0, nullptr, ...)`. Read-only locks return zeroed memory (engine never reads back from USAGE_DEFAULT buffers in practice; defensive zero prevents stale-memory reads).

MemoryBlockManager pattern verbatim from D3D9.

### Direct3d11_DynamicVertexBufferData + Direct3d11_DynamicIndexBufferData (280+211 ln cpp)

One process-wide `D3D11_USAGE_DYNAMIC` + `D3D11_CPU_ACCESS_WRITE` ring buffer per resource type; per-mesh data objects reserve slices via `lock()`.

**Ring sizing:**
- VB ring sized via `IDXGIAdapter::GetDesc.DedicatedVideoMemory` (replaces D3D9's `getVideoMemoryInMegabytes`). Tiers: ≤16 MB → 256 KB; ≤32 MB → 512 KB; ≤64 MB → 1 MB; else → 2 MB. RTX 3060 = 12 GB → 2 MB ring.
- IB ring default 256 KB / `sizeof(Index)` indices; `setDynamicIndexBufferSize` Gl_api slot reconfigures via `Direct3d11_DynamicIndexBufferData::setSize` (CreateBuffer fresh ring at new size).

**Lock pattern** (per RESEARCH Pitfall 5):
- Wrap (`used + length > size` OR `forceDiscard` OR `beginFrame`-discard): `Map(D3D11_MAP_WRITE_DISCARD)`, reset `used = 0`.
- Append: `Map(D3D11_MAP_WRITE_NO_OVERWRITE)`, advance `used`.

Lock/discard counters mirror D3D9 plugin shape (`ms_locksSinceBeginFrame`, `ms_discardsSinceBeginFrame`, etc.) -- diagnostic value when the dynamic-buffer ring blows up.

Index format `DXGI_FORMAT_R16_UINT` is supplied at `IASetIndexBuffer` time per draw call (D3D11 divergence from D3D9's index-format-on-create) -- `IASetIndexBuffer` wires in Plan 11-06.

`MemoryBlockManager` allocator for the per-mesh objects; static `ComPtr<ID3D11Buffer>` for the process-wide ring.

### Direct3d11_RenderTarget (299 ln cpp + 37 ln h)

Persistent 512x512 `DXGI_FORMAT_B8G8R8X8_UNORM` baked-texture surface created in `install()` with `BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE`. One `ID3D11Texture2D`, dual usage -- writable as RTV during bake, samplable as SRV after.

**`setRenderTarget(texture, cubeFace, mipmapLevel)` two paths:**
1. **User texture is render-target type** -- create transient `ComPtr<ID3D11RenderTargetView>` directly on the user texture's `ID3D11Texture2D`, bind via `OMSetRenderTargets`.
2. **User texture is ordinary** -- bind the persistent 512x512 baked-surface RTV; track copy-back metadata (user-texture pointer, cube face, mip).

**`copyRenderTargetToNonRenderTargetTexture()`:**
1. User-texture render-target path -- nothing to copy; release transient RTV and restore primary.
2. User-texture ordinary path -- `CopySubresourceRegion(userTex, baked512, srcBox)`; release tracking state; restore primary.

Drops D3D9's `D3DPOOL_SYSTEMMEM` intermediate -- D3D11 `CopySubresourceRegion` does GPU-to-GPU copy. Per Plan 11-04 spec note: deferred staging-texture path for `screenShot` equivalent is Plan 11-09 territory.

`setRenderTargetToPrimary()` reuses `Direct3d11_Device::beginScene` to rebind back-buffer RTV+DSV (Pitfall 3) -- avoids drift between the two code paths.

### Direct3d11_VertexBufferDescriptorMap (151 ln cpp + 38 ln h)

Standalone helper, no D3D9-class analog by that exact name (D3D9 ships a near-identical `Direct3d9_VertexBufferDescriptorMap`). Caches `VertexBufferDescriptor` (per-format vertex size + field offsets) keyed by `VertexBufferFormat::getFlags()`. Lazy-creates on first lookup.

Drops the dead D3DFVF_TEXTUREFORMAT lookup table -- D3D11 uses `ID3D11InputLayout` for vertex declaration, which lands in Plan 11-05/06.

### Wiring (Task 3)

`Direct3d11.cpp` modifications:

- 7 new includes for resource-class headers
- 8 factory `_impl` thin wrappers in `Direct3d11Namespace` (createTextureData / createStaticVertexBufferData / createDynamicVertexBufferData / createStaticIndexBufferData / createDynamicIndexBufferData / setDynamicIndexBufferSize / setRenderTarget / copyRenderTargetToNonRenderTargetTexture)
- Install ordering in `Direct3d11::install` after `Direct3d11_Device::create`: VertexBufferDescriptorMap → TextureData → StaticVB → DynamicVB → StaticIB → DynamicIB → RenderTarget
- Remove ordering in `remove_impl` (REVERSE): RenderTarget → TextureData → DynamicIB → StaticIB → DynamicVB → StaticVB → VertexBufferDescriptorMap → Device
- 8 `STUB()` lines replaced with `ms_glApi.slot = factoryImpl;` bindings

**Slot count after Plan 11-04:** 76 `STUB()` lines remain (Plan 11-03 baseline 84; -8 = matches 8 wired slots). Remaining STUBs cluster around: state-cache / draw-call slots (Plan 11-06), shader slots (Plan 11-05), windowing utilities (`lockBackBuffer` / `presentToWindow` / `setMouseCursor` -- Wave 7 + later), and `screenShot` / `writeImage` / `setBloomEnabled` (Plan 11-09).

## Task Commits

| # | Commit       | Task                                                                                                     | Type | Files                                          |
|---|--------------|----------------------------------------------------------------------------------------------------------|------|------------------------------------------------|
| 1 | `d70432717`  | Task 1 -- Direct3d11_TextureData + Direct3d11_RenderTarget (new) + vcxproj/filters + Texture.h friend    | spec | +4 new files in Direct3d11/ + 2 modified vcxproj + 1 modified Texture.h (1,089 inserts) |
| 2 | `6ea862275`  | Task 2 -- Static/Dynamic VB + IB (8 new files) + VertexBufferDescriptorMap helper (2 new files) + vcxproj/filters | spec | +10 new files in Direct3d11/ + 2 modified vcxproj (1,396 inserts) |
| 3 | `44627b6c2`  | Task 3 -- Wire 8 Gl_api factory slots in Direct3d11.cpp + install/remove ordering                        | spec | 1 file modified (Direct3d11.cpp; +104 / -9)    |

**Plan close commit:** (this commit) -- adds `11-04-SUMMARY.md` + `STATE.md` + `ROADMAP.md` + `REQUIREMENTS.md` updates.

## Build Verifications

- `MSBuild D:/Code/swg-client-v2/src/build/win32/swg.sln /t:Direct3d11 /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` → EXIT=0; `gl11_d.dll` 1,125,376 bytes (vs Plan 11-03 baseline 1,046,016 bytes; +77 KB) auto-staged via the Plan 11-01 post-build cp fix `266e173b3`.
- `MSBuild ... /t:Direct3d9 ...` → EXIT=0 (D-05 protection: `gl05_d.dll` still builds clean).
- `MSBuild ... /t:SwgClient ...` → EXIT=0 (full link clean; `SwgClient_d.exe` 72,618,496 bytes).
- `grep -rE "D3DPOOL_MANAGED|OnLostDevice|OnResetDevice" src/engine/client/application/Direct3d11/` → 14 hits, ALL inside `//` comment lines documenting the D-13 invariant. Zero functional uses.
- `git diff 606697fb4..HEAD -- src/engine/client/application/Direct3d9/` → empty (D-05 cross-check; no D3D9 source touched).

## Decisions Made

1. **Translation table covers ALL 17 TF_* engine formats + 2 sentinels** (not just the 10 functional formats the plan listed). Static_assert enforces the length matches `TF_Count + 2`. `TF_RGB_888` / `TF_DXT2` / `TF_DXT4` / `TF_P_8` map to `DXGI_FORMAT_UNKNOWN`; `CheckFormatSupport` in `install()` marks them unsupported so the engine falls through to the alternate runtimeFormats. `TF_RGB_565` and `TF_RGB_555` get real DXGI mappings (`B5G6R5_UNORM` and `B5G5R5A1_UNORM`) -- the engine's TextureFormatInfo table determines actual support at runtime.

2. **Static buffer/texture USAGE = D3D11_USAGE_DEFAULT (not USAGE_IMMUTABLE).** The engine fills resources via `lock()` AFTER `CreateBuffer` / `CreateTexture2D` (mesh / texture load path); IMMUTABLE requires initial data at create-time and is not workable. The staging-texture / staging-buffer pattern handles `lock()` returns; `unlock()` blits via `UpdateSubresource` (buffers) or `CopySubresourceRegion` (textures).

3. **Dynamic ring sizing via DXGI DedicatedVideoMemory.** Replaces D3D9 `Direct3d9::getVideoMemoryInMegabytes`. Always-shader path (no FFP branch per Plan 11-01 D-04a DESCOPE verdict).

4. **Dynamic IB uses parameterless createDynamicIndexBufferData() per Gl_api signature.** Ring size controlled via `setDynamicIndexBufferSize` slot calling `setSize` which CreateBuffers a fresh ring. Default 256 KB / sizeof(Index) until engine calls setSize. (The plan-author template suggested `createIndexBufferData(DynamicIndexBuffer)` -- the actual Gl_api signature in `Gl_dll.def:182` is `DynamicIndexBufferGraphicsData *(*createDynamicIndexBufferData)()` with no args.)

5. **Render-target dual-bind pattern.** Persistent 512x512 baked surface with `BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE`. One ID3D11Texture2D, dual usage. `CopySubresourceRegion` replaces D3D9's `GetRenderTargetData` + `D3DXLoadSurfaceFromSurface` -- no system-memory intermediate.

6. **No Gl_api createRenderTarget slot.** Render-target textures are produced via `createTextureData` when `Texture::isRenderTarget()=true`. The Plan 11-04 narrative referenced "createRenderTarget" in error; the actual factory surface for RT textures is the texture-creation path. `setRenderTarget` routes draws to it; `copyRenderTargetToNonRenderTargetTexture` handles read-back. (Slot count: 8 wired, not 6+ as the plan estimated. The plan counted `createRenderTarget` and `createVertexBufferData` Static + Dynamic as separate slots; reality is `setRenderTarget` + `copyRenderTargetToNonRenderTargetTexture` + 2 createVertexBuffer + 2 createIndexBuffer + 1 setDynamicIndexBufferSize + 1 createTextureData = 8.)

7. **Direct3d11_VertexBufferDescriptorMap helper.** Tiny standalone file mirroring D3D9's `Direct3d9_VertexBufferDescriptorMap` minus the dead D3DFVF lookup. The descriptor cache (per-format vertexSize + offset table) is backend-agnostic; D3D11 vertex declaration via `ID3D11InputLayout` lands in Plan 11-05/06.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Add Direct3d11_TextureData as friend of TextureGraphicsData::LockData**
- **Found during:** Task 1 first build attempt
- **Issue:** `Direct3d11_TextureData::lock` / `unlock` need to write `LockData::m_pixelData` / `m_pitch` / `m_slicePitch` / `m_reserved` / `m_cubeFace` / `m_format` to communicate the locked region back to the engine. These fields are `private` in `TextureGraphicsData::LockData` with friends only for `Texture`, `Direct3d8_TextureData`, `Direct3d9_TextureData`. Compiler reported 17 C2248 access-control errors.
- **Fix:** Added `friend class Direct3d11_TextureData;` on Texture.h:35, mirroring the existing pattern from Direct3d8/Direct3d9. One-line engine header edit; zero impact on Direct3d9 plugin source or behaviour.
- **Files modified:** `src/engine/client/library/clientGraphics/src/shared/Texture.h`
- **Verification:** Direct3d11 + Direct3d9 + SwgClient all rebuild clean. D3D9 plugin source untouched (`git diff 606697fb4..HEAD -- src/engine/client/application/Direct3d9/` returns empty).
- **Committed in:** `d70432717` (Task 1 commit -- Rule 3 deviation noted in commit body)

**2. [Rule 1 - Bug] static_assert off-by-one on translationTable length**
- **Found during:** Task 1 second build attempt (after Rule 3 fix)
- **Issue:** Original `static_assert` said `sizeof(translationTable) / sizeof(translationTable[0]) == (TF_Count + 1)`, but the table has 19 entries (TF_ARGB_8888..TF_ABGR_32F = 17, plus TF_Count + TF_Native = 2 sentinels). The enum order is `TF_Count` = 17, `TF_Native` = 18, so table length must be `TF_Count + 2` = 19. Compile-time assert fired.
- **Fix:** Updated the static_assert to `(TF_Count + 2)` with comment explaining the +2 rationale.
- **Files modified:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_TextureData.cpp`
- **Verification:** Direct3d11 rebuild clean.
- **Committed in:** `d70432717` (Task 1 commit -- fix applied before staging)

### Lessons Learned (recorded for future executors)

**1. Engine-side `friend` declarations are routine, not architectural.** Adding a backend-specific friend declaration to a private engine-side struct mirrors the existing pattern for previous backends (Direct3d8 / Direct3d9) and is a Rule 3 deviation, not a Rule 4 architectural change. The pattern is established; future D3D12 plugins will need the same friend-class addition.

**2. Gl_api signatures must be read directly, not inferred from plan templates.** The plan template suggested `createIndexBufferData(DynamicIndexBuffer)` for the dynamic IB factory; the actual signature in `Gl_dll.def:182` is `createDynamicIndexBufferData()` (no args). Reading the live `Gl_dll.def` first saved a round of refactor.

**3. D3D11 has no createRenderTarget Gl_api slot.** RT textures are produced via createTextureData with `Texture::isRenderTarget()=true`. The Plan 11-04 spec narrative used "createRenderTarget" loosely; the actual factory surface is the texture path. `setRenderTarget` + `copyRenderTargetToNonRenderTargetTexture` are the only RT-specific Gl_api slots, and both wire to `Direct3d11_RenderTarget` static methods.

**4. USAGE_IMMUTABLE is NOT workable for engine-driven static-resource creation.** The engine creates the resource then calls `lock()` to fill it -- IMMUTABLE requires initial data on `CreateBuffer`/`CreateTexture2D`. USAGE_DEFAULT + `UpdateSubresource` (buffers) / `CopySubresourceRegion` (textures) is the correct pattern.

---

**Total deviations:** 2 auto-fixed (1 Rule 3 friend-class addition + 1 Rule 1 static_assert off-by-one). No scope creep; both required to complete Task 1.
**Impact on plan:** Without the Rule 3 friend addition, `Direct3d11_TextureData::lock` cannot communicate with the engine. Without the Rule 1 static_assert fix, the translation table can't compile.

## Issues Encountered

- **No client-launch evidence this plan.** Per `autonomous: true` framing, the plan-close does NOT include a Kenny smoke session. Visible-window evidence (whether the dark-blue back buffer becomes observable after `SetupClientGraphics::install` completes) is deferred to Plan 11-05's checkpoint. Plan 11-05's smoke will reveal whether the new FATAL boundary is a shader slot (`createVertexShaderData` / `createPixelShaderProgramData` -- both still STUB'd) or a state/draw slot, and whether `ShowWindow(SW_SHOW)` fires after the resource layer reports successful install.

- **Pre-existing Koogie warnings persist (out of scope):** `Direct3d9.vcxproj` MSB8012 TargetName/OutputFile mismatch + C4456 declaration-shadowing warnings in `Direct3d9.cpp:2519` / `Direct3d9_VertexBufferDescriptorMap.cpp:140` + `Direct3d9_RenderTarget.cpp` (3 hresult-shadow C4456) -- all carry-forward from Plan 11-03. Direct3d9 source not touched this plan.

## User Setup Required

None -- Plan 11-04 is the resource-layer port; no external services or new manual steps required. Plan 11-05's smoke will need the existing setup: `client_d.cfg [ClientGraphics] rasterMajor=11`, run `D:/Code/swg-client-v2/stage/SwgClient_d.exe` against the SWGSource VM at 192.168.1.200 (or local).

## Next Phase Readiness

- **Plan 11-05 (Wave 5 -- shader layer) is UNBLOCKED.** First work items: HLSL `vs_5_0`/`ps_5_0` recompile via `D3DCompile` (Pitfall 1 `POSITION→SV_POSITION` macro), `.cso` disk cache (D-03), cbuffer wrapper (Pitfall 2 alignment), and -- per Plan 11-01 D-04a DESCOPE verdict -- NO `Direct3d11_FfpGenerator`. The next FATAL boundary (where Plan 11-05's smoke will land) is most likely one of: `createVertexShaderData` / `createPixelShaderProgramData` (engine-shader-template install path, runs during `ShaderTemplateList::install` per the call stack pattern), or `createShaderImplementationGraphicsData` / `createStaticShaderGraphicsData` (which may fire earlier than the per-program slots).
- **Plan 11-06 (Wave 6 -- state cache + draw dispatch + input layout) is dependent on Plan 11-05's cbuffer + input-layout primitives.** Plan 11-04's `Direct3d11_VertexBufferDescriptorMap` is the descriptor-cache half of input layout; Plan 11-05/06 add the `ID3D11InputLayout` half.
- **D-05 maintained.** `git diff 606697fb4..HEAD -- src/engine/client/application/Direct3d9/` returns empty. `gl05_d.dll` still builds + loads clean.
- **D-13 maintained.** 14 `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` grep hits in `src/engine/client/application/Direct3d11/`, all in `//` comment lines. Zero functional uses.
- **Phase 11 bonus deliverables still active.** `266e173b3` (Plan 11-01 post-build cp auto-stage) + `dbd7c62dc` (Plan 11-02 atlmfc include vendor) continue to retire the CLI MSBuild rebuild trap.

### Carry-forward observations (not blockers)

- Pre-existing `Direct3d9.vcxproj` MSB8012 TargetName/OutputFile mismatch warnings remain (out of scope; Direct3d9 source not touched this plan).
- Pre-existing C4456 declaration-shadowing warnings in `Direct3d9.cpp:2519`, `Direct3d9_VertexBufferDescriptorMap.cpp:140`, and `Direct3d9_RenderTarget.cpp:85,91,98` remain (out of scope; Direct3d9 source not touched this plan).
- The Plan 11-04 spec narrative referenced 6 resource types; the actual implementation is 6 functional resource types (Texture, RT, StaticVB, DynamicVB, StaticIB, DynamicIB) across 5 resource classes (Direct3d11_TextureData covers both texture + render-target-texture creation; setRenderTarget routes via Direct3d11_RenderTarget). The plan's "6 resource type pairs" was conceptually correct; the implementation is 5 class files + 1 helper file = 14 new source files total.

## TDD Gate Compliance

This plan is `type: execute` (not `type: tdd`), so the RED/GREEN/REFACTOR gate sequence does not apply. No `test(...)` commits expected. The 3 build-verification gates (`/t:Direct3d11`, `/t:Direct3d9`, `/t:SwgClient` all EXIT=0) + the D-13 grep gate are the equivalent verification gates.

## Requirements Traceability

- **D3D11-01 (plugin scaffold + Gl_api table + Direct3d11.dll runtime):** **PARTIAL** -- unchanged from Plan 11-03 framing; Plan 11-04 adds 8 more real Gl_api slots (from 12 in Plan 11-03 to 20 in Plan 11-04: 5 per-frame + 7 init + 8 resource = 20 of ~119 slots), but visible-window outcome still deferred to Plan 11-05's checkpoint smoke session. Commit chain extended: `2c518e832 / db2116594 / dbd7c62dc / a9ae88846` (Plan 11-02) → `28c1f64c4 / 802ea9c4d / 606697fb4` (Plan 11-03) → `d70432717 / 6ea862275 / 44627b6c2 / <plan-close-hash>` (Plan 11-04).
- **D3D11-02 (resource management replaces D3DPOOL_MANAGED + lost-device removed):** **PARTIAL** -- Plan 11-04 lands the real resource layer (textures, render targets, static + dynamic VB/IB) with ComPtr ownership, USAGE_DEFAULT / USAGE_DYNAMIC patterns, and Map(WRITE_DISCARD/NO_OVERWRITE) ring-buffer semantics. D-13 invariant still holds (14 grep hits, all `//` comment lines). The remaining work for D3D11-02 ACCEPTANCE is end-to-end zone-in evidence (Plan 11-07 smoke or earlier), not more source code. Promoting from TRACED → PARTIAL.
- **D3D11-03 (shader recompilation under HLSL SM5.0 per SPEC R3):** NOT touched this plan; Plan 11-05 (Wave 5) territory.

---

*Phase: 11-d3d11-renderer-plugin*
*Plan: 04 -- D3D11 resource layer (textures + VB/IB + render targets)*
*Completed: 2026-05-17*

## Self-Check: PASSED

- `.planning/phases/11-d3d11-renderer-plugin/11-04-SUMMARY.md` exists -- VERIFIED (this file)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_TextureData.{h,cpp}` exist -- VERIFIED (committed in `d70432717`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_RenderTarget.{h,cpp}` exist -- VERIFIED (committed in `d70432717`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticVertexBufferData.{h,cpp}` exist -- VERIFIED (committed in `6ea862275`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.{h,cpp}` exist -- VERIFIED (committed in `6ea862275`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticIndexBufferData.{h,cpp}` exist -- VERIFIED (committed in `6ea862275`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicIndexBufferData.{h,cpp}` exist -- VERIFIED (committed in `6ea862275`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexBufferDescriptorMap.{h,cpp}` exist -- VERIFIED (committed in `6ea862275`)
- Task commits present in `git log`: `d70432717` (Task 1) + `6ea862275` (Task 2) + `44627b6c2` (Task 3) -- VERIFIED
- D-05 cross-check: `git diff 606697fb4..HEAD -- src/engine/client/application/Direct3d9/` returns empty -- VERIFIED
- D-13 cross-check: 14 `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` hits in `Direct3d11/`, ALL inside `//` comment lines documenting the invariant -- VERIFIED
- gl11_d.dll auto-staged at `D:/Code/swg-client-v2/stage/gl11_d.dll`, 1,125,376 bytes (vs Plan 11-03 baseline 1,046,016 bytes; +77 KB) -- VERIFIED
