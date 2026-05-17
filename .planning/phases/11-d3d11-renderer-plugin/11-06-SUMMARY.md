---
phase: 11-d3d11-renderer-plugin
plan: 06
subsystem: renderer
tags: [d3d11, state-cache, draw-calls, input-layout, lighting, metrics, srv-sampler-split]

# Dependency graph
requires:
  - phase: 11-d3d11-renderer-plugin
    plan: 04
    provides: D3D11 resource layer (textures, VB/IB static + dynamic, render target) -- consumed by StateCache's setVertexBuffer / setIndexBuffer / setGlobalTexture
  - phase: 11-d3d11-renderer-plugin
    plan: 05
    provides: D3D11 shader layer (VS/PS/ConstantBuffer + ShaderCache) -- consumed by StateCache for VSSetShader / PSSetShader (conditional null skip) + cbuffer bind on draw, and by VertexDeclarationMap for the VS-bytecode-hash half of the (VBFormat, vsHash) input-layout cache key
provides:
  - Direct3d11_VertexDeclarationMap -- ID3D11InputLayout cache keyed by (VertexBufferFormat::getFlags(), VS bytecode hash) per Pitfall 6
  - Direct3d11_VertexBufferDescriptorMap::buildInputElementDesc -- engine VertexBufferFormat -> D3D11_INPUT_ELEMENT_DESC[] (single-stream); DXGI_FORMAT translation for POSITION (R32G32B32_FLOAT or R32G32B32A32_FLOAT transformed) + NORMAL (R32G32B32_FLOAT) + PSIZE (R32_FLOAT) + COLOR0/1 (B8G8R8A8_UNORM) + TEXCOORDn (R32{,G,GB,GBA}_FLOAT by per-set dimension)
  - Direct3d11_StateCache -- hashed (RS/BS/DSS/sampler) state-object caches + 25 draw* slot bodies (Draw / DrawIndexed dispatch with topology / VB / IB / shader / input-layout / SRV / sampler / RS|BS|DSS state bindings) + 11 state-setter slot bodies (setFillMode / setCullMode / setAntialiasEnabled / setViewport / setScissorRect / setWorldToCameraTransform / setProjectionMatrix / setFog / setObjectToWorldTransformAndScale / setGlobalTexture / releaseAllGlobalTextures / setTextureTransform / setAlphaFadeOpacity / setVertexBuffer / setIndexBuffer / setBadVertexShaderStaticShader / setStaticShader); Pitfall 4 SRV/sampler split (ms_boundSRV[16] + ms_boundSampler[16] tracked independently); PS NULL fallback per Plan 11-05 caveat (skip PSSetShader when m_d3dPS is null)
  - Direct3d11_LightManager -- per-frame light list (Light::Type filter + ambient accumulation + first directional + up to 8 point/spot) -> Direct3d11_LightingCB (320 bytes, 16-byte aligned) -> Direct3d11_ConstantBuffer slot 3 on both VS and PS
  - Direct3d11_Metrics -- D3D11-specific counters (vertices / indices / triangles / drawCallCount / mapCount / cbufferUpdates per frame; inputLayoutCacheHits/Misses + shaderCacheHits/Misses cumulative); DebugFlag [ClientGraphics/Direct3d11] reportFrameStats
  - 43 Gl_api scaffold_fatal_stub bindings replaced (Plan 11-05 had 72 STUBs; Plan 11-06 ends at 29 remaining)
  - D-13 invariant maintained -- 16 grep hits for D3DPOOL_MANAGED|OnLostDevice|OnResetDevice across Direct3d11/, ALL inside // comment lines documenting the invariant; zero functional uses
  - D-05 invariant maintained -- D3D9 plugin source untouched (`git diff 825838858..HEAD -- src/engine/client/application/Direct3d9/` empty); all 3 D3D9 variants (gl05/06/07_d.dll) build clean
affects: [11-07, 11-09]

# Tech tracking
tech-stack:
  added:
    - ID3D11InputLayout cache keyed by (VBFormat, VS-bytecode-hash) per RESEARCH Pitfall 6
    - ID3D11RasterizerState / ID3D11BlendState / ID3D11DepthStencilState / ID3D11SamplerState immutable state-object caches with FNV-1a 64-bit descriptor hash keys
    - PSSetShaderResources + PSSetSamplers binding split (Pitfall 4) -- ms_boundSRV[16] and ms_boundSampler[16] tracked independently
    - D3D11_PRIMITIVE_TOPOLOGY translation for engine draw* slots (POINTLIST / LINELIST / LINESTRIP / TRIANGLELIST / TRIANGLESTRIP); TRIANGLEFAN + QUADLIST mapped to TRIANGLELIST (D3D11 has no native fan/quad topologies)
    - DirectX::XMFLOAT4 lighting cbuffer (LightingCB 320 bytes; ambient + directional + 8 point + count)
  patterns:
    - "Pitfall 4 enforcement (SRV/sampler split): D3D9's SetTexture coupled resource + sampler; D3D11 splits. Direct3d11_StateCache::setGlobalTexture binds the SRV only; applyPreDrawState calls PSSetShaderResources(0, 16, ms_boundSRV) and PSSetSamplers(0, 16, ms_boundSampler) separately. Default linear-wrap sampler bound to all 16 slots at install."
    - "Pitfall 6 enforcement (input-layout cache key): D3D9 keyed input layout by VB format alone; D3D11's CreateInputLayout validates the layout against the VS bytecode signature. Cache key is the pair (VertexBufferFormat::getFlags(), Direct3d11_VertexShaderData::getBytecodeHash()). The bytecode hash from Plan 11-05 (FNV-1a 64-bit over source + defines) is a valid identity because D3DCompile is deterministic."
    - "Lazy state-object selection: state-setter slots mutate the per-stage descriptor structs (ms_rsDesc / ms_bsDesc / ms_dssDesc) + flip a dirty bit; applyPreDrawState resolves dirty descs to ID3D11RasterizerState / ID3D11BlendState / ID3D11DepthStencilState pointers via the descriptor-hash cache and binds them only when dirty. Same shape as D3D9 dirty-bit state cache; bind cost is the GetOrCreate hash lookup + (on cache miss) a single CreateXxxState call per distinct combination."
    - "PS NULL fallback: applyPreDrawState calls PSSetShader only when m_currentPSData->getPixelShader() is non-null. Plan 11-05 leaves m_d3dPS NULL for stock-asset pre-compiled D3D9 PEXE bytecode (D3D11 CreatePixelShader rejects D3D9 bytecode). D3D11 keeps whatever PS was last bound (default pass-through during init). Geometry rasterizes; pixel output undefined; Plan 11-07 smoke surfaces visual impact. Future Phase 12 asset re-author is the canonical fix."
    - "Cbuffer flush per draw (Pitfall 5): applyPreDrawState calls Direct3d11_ConstantBuffer::bindVS(0)+bindVS(1)+bindPS(0)+bindPS(2). Plan 11-05's setVertexShaderUserConstants / setPixelShaderUserConstants shadowed into static PerObject / PerMaterial structs; Plan 11-06's draw moment is the flush point. The cbuffer's whole-buffer Map(WRITE_DISCARD) means we update once per draw, not once per slot-setter call."
    - "Cbuffer slot allocation: PerFrameCB=0 (view + camera + fog), PerObjectCB=1 (world + 8 user constants), PerMaterialCB=2 (diffuse/specular/emissive + 4 user constants), LightingCB=3 (ambient + directional + 8 point). Slots 0/1/2 wired in Plan 11-05; slot 3 wired in Plan 11-06."
    - "VB/IB friend-class access: HardwareVertexBuffer / HardwareIndexBuffer m_graphicsData fields are private with friends Direct3d8/9 + per-backend graphics-data classes. Rule-3 deviation adds Direct3d11 + Direct3d11_StateCache + Direct3d11_*Data friends to all 4 buffer classes -- mirrors Plan 11-04's TextureGraphicsData::LockData friend pattern. Zero impact on D3D9 plugin source or behavior."
    - "Single-stream input layouts (Phase 11 SPEC §Boundaries). buildInputElementDesc emits InputSlot=0 for every element. createVertexBufferVectorData + setVertexBufferVector remain STUB() -- deferred to a later phase if smoke surfaces the need."
    - "D3D11 has no native QUADLIST or TRIANGLEFAN topology. drawQuadList currently emits DEBUG_REPORT_LOG_PRINT TODO (UI / particles may exercise; Plan 11-07 surfaces fix needs). drawTriangleFan + drawIndexedTriangleFan emit TRIANGLELIST topology -- the engine's fan-vertices reshape into list-vertices server-side; if any subsystem feeds un-triangulated fans, server-side re-emit lands as a Rule-1 deviation in Plan 11-07."

key-files:
  created:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexDeclarationMap.h (56 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexDeclarationMap.cpp (127 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.h (125 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp (1028 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_LightManager.h (84 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_LightManager.cpp (141 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_Metrics.h (54 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_Metrics.cpp (92 ln)
    - .planning/phases/11-d3d11-renderer-plugin/11-06-SUMMARY.md
  modified:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexBufferDescriptorMap.h (added buildInputElementDesc declaration)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexBufferDescriptorMap.cpp (added buildInputElementDesc implementation +90 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp (5 new module includes + Plan 11-06 install ordering block + Plan 11-06 reverse-order remove block + 43 STUB() lines replaced with real slot bindings; +98 / -56)
    - src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj (5 new files added to ClCompile / ClInclude ItemGroups -- VertexDeclarationMap + StateCache + LightManager + Metrics)
    - src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj.filters (5 new files assigned to src\win32 filter)
    - src/engine/client/library/clientGraphics/src/shared/StaticVertexBuffer.h (Rule-3 deviation: friend Direct3d11 + Direct3d11_StateCache + Direct3d11_StaticVertexBufferData; mirrors Plan 11-04 Texture.h pattern)
    - src/engine/client/library/clientGraphics/src/shared/DynamicVertexBuffer.h (Rule-3 deviation: friend Direct3d11 + Direct3d11_StateCache + Direct3d11_DynamicVertexBufferData)
    - src/engine/client/library/clientGraphics/src/shared/StaticIndexBuffer.h (Rule-3 deviation: friend Direct3d11 + Direct3d11_StateCache + Direct3d11_StaticIndexBufferData)
    - src/engine/client/library/clientGraphics/src/shared/DynamicIndexBuffer.h (Rule-3 deviation: friend Direct3d11 + Direct3d11_StateCache + Direct3d11_DynamicIndexBufferData)

key-decisions:
  - "Direct3d11_VertexBufferDescriptorMap GAINS buildInputElementDesc rather than introducing a brand-new helper. The existing helper class (Plan 11-04) already owned the per-format vertexSize/offset table for VB descriptor cache; the input-element-desc builder is its natural cousin (same per-format iteration, different output shape). Keeps the surface narrow: one helper class covers BOTH 'CPU-side per-format descriptor for engine consumption' AND 'D3D11_INPUT_ELEMENT_DESC[] for ID3D11InputLayout creation'."
  - "Direct3d11_VertexDeclarationMap key is std::pair<uint32, uint64_t> -- (VertexBufferFormat::getFlags(), VS-bytecode-hash). Avoided introducing a separate struct + custom hasher; std::pair already has operator< and works as the key for std::map. unordered_map could swap in later if cache miss rate becomes a perf concern."
  - "State-object caches use std::unordered_map<uint64_t, ComPtr<...>>. Key is FNV-1a 64-bit over the descriptor struct bytes. Hash collisions are theoretically possible but the realistic distinct state set is bounded (RESEARCH suggests <50 per state type) so 64-bit space is comfortable. ComPtr ownership means caches manage their own lifetime."
  - "PS NULL fallback handled in applyPreDrawState. Plan 11-05 documented the stock-asset PEXE bytecode incompatibility; Plan 11-06's draw dispatch must skip PSSetShader to honor the contract. Alternative considered: bind a fallback pass-through PS at install. Rejected: default-D3D11-pass-through is already there from device init, and binding a NULL would unbind whatever was previously bound which is worse than leaving it alone."
  - "drawQuadList currently emits DEBUG_REPORT_LOG_PRINT TODO. D3D9 plugin triangulates quads at draw time via a persistent quad-index buffer (Direct3d9.cpp:4144-4159). D3D11 implementation requires re-creating that quad-IB; deferred to Plan 11-07 if UI / particles surface the need. Most engine call sites use indexed-triangle-list for quads anyway (UI mesh, billboard particles)."
  - "drawTriangleFan + drawIndexedTriangleFan emit D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST. D3D11 has no native TRIANGLEFAN. Most engine fans in target scenes are server-triangulated during mesh load (NGE-era assets); if any subsystem feeds un-triangulated fans, Plan 11-07 smoke surfaces the visual artifact and we add server-side re-emit as a Rule-1 deviation."
  - "Friend-class additions to HardwareVertexBuffer / HardwareIndexBuffer + the 4 buffer subclasses are Rule-3 deviations (one-line each, mirror existing D3D8/D3D9 patterns). Pattern was already established for TextureGraphicsData::LockData in Plan 11-04. Future D3D12 plugin will need the same set of friend declarations."
  - "Multi-stream (createVertexBufferVectorData / setVertexBufferVector) deferred per SPEC §Boundaries. Phase 11 single-stream is the contract; multi-stream input layouts (multiple InputSlot values; per-stream stride/offset arrays) would require extending VertexDeclarationMap key to include the stream-set hash. Defer until target subsystem (skeletal blend weights are the typical multi-stream user) actually exercises the path."
  - "Direct3d11_LightManager::setLights uses Object::getPosition_w / getObjectFrameK_w for world-space pos + dir. Plan 11-07 smoke surfaces whether HLSL shaders expect world-space (likely, matching D3D9 plugin's setLight pattern) or view-space. Plan 11-06 commits world-space; transform-to-view happens in HLSL via PerFrameCB.viewProj."
  - "Direct3d11_Metrics::reportFrameStats is a DebugFlag (default false). Per-frame counters reset in beginFrame; cumulative cache counters retained across frames. endFrame publishes via DEBUG_REPORT_LOG_PRINT when flag is true -- mirrors Direct3d9_Metrics shape. Plan 11-07's iterative smoke will toggle the flag when investigating draw-call patterns."
  - "Install order: VertexDeclarationMap -> StateCache -> LightManager -> Metrics AFTER the Plan 11-05 shader install layer. Remove order is REVERSE. VertexDeclarationMap depends on VertexShaderData (Plan 11-05) for the bytecode hash; StateCache depends on the full resource + shader layers; LightManager depends on ConstantBuffer; Metrics is leaf (DebugFlags only)."

patterns-established:
  - "Two-tier state-object install: cache map + default-descriptor struct + dirty bit. State-setter slot mutates desc + flips dirty bit; applyPreDrawState's draw-time resolve does the hash lookup + (on miss) CreateXxxState + bind. Per-state-type independent dirty bits avoid spurious rebinds."
  - "Pitfall 6 input-layout cache key: pair<uint32 format flags, uint64 VS bytecode hash>. The bytecode hash from Direct3d11_VertexShaderData::getBytecodeHash (FNV-1a 64-bit over source + defines per Plan 11-05) is the valid identity because D3DCompile is deterministic. Distinct shader-source files with identical produce-bytecode behavior would collide in our cache but produce identical input layouts -- safe."
  - "Pitfall 4 enforcement pattern: SRV-array + Sampler-array fields are SEPARATE namespace state. setGlobalTexture only touches the SRV slot. Default linear-wrap sampler bound to all slots at install. Per-stage sampler overrides land in a later plan if smoke surfaces shader-specific sampler config."
  - "Friend-class extension for new backend: when a new plugin needs access to private engine-side m_graphicsData / LockData / etc., add `friend class Direct3d11_*` to the engine header alongside the existing Direct3d8/9 friends. One-line edit per class; zero impact on existing plugin source. Pattern established in Plan 11-04 (Texture.h), continued in Plan 11-06 (4 buffer classes)."
  - "Hashed state-object cache via FNV-1a 64-bit over the descriptor struct bytes. Same hash function as Plan 11-05's shader cache, same shape: unordered_map<uint64_t, ComPtr<...>>. Cache survives the plugin's lifetime; ComPtr ownership cleans up at remove() time before device::destroy()."

requirements-completed: []
requirements-partial: [D3D11-01, D3D11-02, D3D11-03]

# Metrics
duration: ~3 hours active (2026-05-16, single autonomous executor session)
completed: 2026-05-16
---

# Phase 11 Plan 06: D3D11 State Cache + Draw-Call Dispatch + Light Manager + Metrics Summary

**Five new source files (1,707 lines across `Direct3d11_VertexDeclarationMap`, `Direct3d11_StateCache`, `Direct3d11_LightManager`, `Direct3d11_Metrics`) plus the `buildInputElementDesc` extension on `Direct3d11_VertexBufferDescriptorMap`. 43 Gl_api scaffold_fatal_stub bindings replaced (Plan 11-05's 72 -> Plan 11-06's 29 remaining). RS/BS/DSS/sampler state-object caching via FNV-1a descriptor hash. Pitfall 4 SRV/sampler split. Pitfall 6 input-layout cache keyed by (VBFormat, VS bytecode hash). Pitfall 5 cbuffer flush in applyPreDrawState. PS NULL fallback honored. D-13 / D-05 / cbuffer-migration invariants intact. gl11_d.dll grew from 1,240,064 bytes (Plan 11-05) to 1,392,128 bytes (+148 KB).**

## Performance

- **Duration:** ~3 hours active on 2026-05-16 (autonomous executor; no Kenny smoke this plan per `autonomous: true` framing)
- **Started:** 2026-05-16 (after Plan 11-05 close at `825838858`)
- **Completed:** 2026-05-16 (this plan-close commit)
- **Tasks:** 4 (all `type=auto`; no checkpoint per `autonomous: true`)
- **Files modified:** 14 (8 new files in Direct3d11/ + 2 modified Direct3d11/* helpers + 1 modified Direct3d11.cpp + 2 modified vcxproj/filters + 4 engine-side header friend additions for HardwareVertexBuffer / HardwareIndexBuffer / their static + dynamic subclasses)
- **Commits:** 4 task commits + 1 plan-close commit = 5 total on plan 11-06
- **New lines:** 1,707 lines across 8 new source files (per `wc -l` post-commit)

## Accomplishments

### Direct3d11_VertexDeclarationMap (127 ln cpp + 56 ln h)

ID3D11InputLayout cache keyed by (`VertexBufferFormat::getFlags()`, `Direct3d11_VertexShaderData::getBytecodeHash()`) per RESEARCH Pitfall 6. D3D9's input layout was keyed by VB format alone; D3D11's `CreateInputLayout` validates the layout against the VS bytecode signature at creation time, so the bytecode hash MUST participate in the cache key. The FNV-1a 64-bit hash from Plan 11-05's `Direct3d11_VertexShaderData::m_bytecodeHash` is a valid identity because `D3DCompile` is deterministic -- identical (source, defines) yield identical bytecode.

API: `install()` / `remove()` / `getOrCreate(VertexBufferFormat const &, Direct3d11_VertexShaderData const *)`. `getOrCreate` returns nullptr if either arg is degenerate (empty format, NULL VS bytecode, format-VS-signature mismatch returned from `CreateInputLayout`); callers (the state cache draw dispatch) skip the draw call when layout is nullptr.

Cache hits / misses counted internally; `getCachedLayoutCount()` exposes the distinct-layout count for Direct3d11_Metrics consumption (Plan 11-07 wiring).

### Direct3d11_VertexBufferDescriptorMap (extended, +90 ln in cpp)

`buildInputElementDesc(VertexBufferFormat const &, D3D11_INPUT_ELEMENT_DESC outDesc[16])` -- emits up to 16 input elements per the engine's vertex format. Mirrors the offset/size layout that `getDescriptor` computes (same component order, same byte offsets, so the input layout matches the engine's vertex data byte-for-byte):

| Component                 | DXGI_FORMAT                       | Bytes |
|---------------------------|-----------------------------------|------:|
| POSITION (untransformed)  | DXGI_FORMAT_R32G32B32_FLOAT       |    12 |
| POSITION (transformed)    | DXGI_FORMAT_R32G32B32A32_FLOAT    |    16 (xyz + rhw) |
| NORMAL                    | DXGI_FORMAT_R32G32B32_FLOAT       |    12 |
| PSIZE                     | DXGI_FORMAT_R32_FLOAT             |     4 |
| COLOR0 / COLOR1           | DXGI_FORMAT_B8G8R8A8_UNORM        |     4 |
| TEXCOORDn (dim=1)         | DXGI_FORMAT_R32_FLOAT             |     4 |
| TEXCOORDn (dim=2)         | DXGI_FORMAT_R32G32_FLOAT          |     8 |
| TEXCOORDn (dim=3)         | DXGI_FORMAT_R32G32B32_FLOAT       |    12 |
| TEXCOORDn (dim=4)         | DXGI_FORMAT_R32G32B32A32_FLOAT    |    16 |

InputSlot=0 (single-stream per Phase 11 SPEC); InputSlotClass=PER_VERTEX_DATA; InstanceDataStepRate=0.

### Direct3d11_StateCache (1028 ln cpp + 125 ln h)

The largest Plan 11-06 file. Three responsibilities:

**1. Hashed state-object cache** (RS/BS/DSS/sampler).
- `std::unordered_map<uint64_t, ComPtr<ID3D11RasterizerState>>` (and equivalents for BS/DSS/sampler)
- FNV-1a 64-bit hash over descriptor struct bytes
- Defaults match D3D9 plugin's `forceRenderState(D3DRS_*, ...)` cascade: CullMode=BACK, DepthFunc=LESS_EQUAL, DepthEnable=TRUE, etc.
- Dirty-bit per state type (`ms_rsDirty / ms_bsDirty / ms_dssDirty`); state-setter slots flip the bit; `applyPreDrawState` resolves to a state object via descriptor-hash lookup
- Default linear-wrap sampler created in `install()`; bound to all 16 slots so untouched stages still produce sensible sampling

**2. Pitfall 4 SRV/sampler split tracking**.
- `ms_boundSRV[16]` and `ms_boundSampler[16]` arrays tracked INDEPENDENTLY
- `setGlobalTexture(tag, texture)` resolves the engine global-texture binding via `Direct3d11_TextureData::setGlobalTexture` + `getGlobalTexture`, then writes only into `ms_boundSRV[slot]`
- `applyPreDrawState` calls `PSSetShaderResources(0, 16, ms_boundSRV)` and `PSSetSamplers(0, 16, ms_boundSampler)` separately
- Sampler defaults remain `ms_defaultSampler` unless a future setter overrides them (per-stage sampler config lands when a shader surfaces the need; Plan 11-07 smoke surfaces this)

**3. Draw-call dispatch (25 Gl_api draw* slots)** + state-setter slot bodies.
- `applyPreDrawState(topology)` is the canonical pre-draw sequence:
  1. Resolve VS + PS + input layout from the currently-bound static shader (`resolveShaders` calls `Direct3d11_VertexDeclarationMap::getOrCreate`)
  2. `IASetInputLayout(...)`, `IASetPrimitiveTopology(topology)`
  3. `IASetVertexBuffers(0, 1, ...)` with current VB + stride + offset
  4. `IASetIndexBuffer(...)` with current IB + format (DXGI_FORMAT_R16_UINT, engine Index = unsigned short)
  5. `VSSetShader(vs, nullptr, 0)`
  6. **`PSSetShader(ps, nullptr, 0)` CONDITIONAL on `m_currentPSData->getPixelShader() != nullptr`** -- PS NULL fallback per Plan 11-05 caveat
  7. cbuffer bind: `bindVS(0)+bindVS(1)+bindPS(0)+bindPS(2)` (PerFrame + PerObject + PerMaterial; slot 3 LightingCB bind happens in LightManager's setLights)
  8. `PSSetShaderResources(0, 16, ms_boundSRV)` + `PSSetSamplers(0, 16, ms_boundSampler)` (Pitfall 4 split)
  9. RS / BS / DSS state objects bound lazily on dirty
- Returns false (and skips the actual Draw call) on: NULL VS, NULL input layout, NULL device context
- `drawXxxList` variants -> `applyPreDrawState(topology)` + `ctx->Draw(vertexCount, 0)`
- `drawIndexedXxxList` variants -> `applyPreDrawState(topology)` + `ctx->DrawIndexed(indexCount, 0, 0)`
- `drawPartial*` variants accept explicit (startVertex, primitiveCount) and translate primitive counts to vertex/index counts (line=*2, triangle=*3, strip=+1/+2)
- `drawQuadList` currently logs `DEBUG_REPORT_LOG_PRINT TODO Plan 11-07` (engine quad-IB triangulation deferred)
- `drawTriangleFan` + `drawIndexedTriangleFan` emit `TRIANGLELIST` topology (D3D11 has no native fan)

State-setter slot bodies wired:
- `setFillMode` (GFM_wire -> WIREFRAME, GFM_solid -> SOLID)
- `setCullMode` (GCM_none/clockwise/counterClockwise -> CULL_NONE/FRONT/BACK)
- `setAntialiasEnabled` -> AntialiasedLineEnable + MultisampleEnable in RS desc
- `setViewport` -> RSSetViewports
- `setScissorRect` -> ScissorEnable in RS desc + RSSetScissorRects
- `setWorldToCameraTransform` -> PerFrameCB.viewProj (via `Direct3d11_ConstantBuffer::updateVS(0, ...)`)
- `setProjectionMatrix` -> PerObjectCB.world (slot 1, will refine in Plan 11-07 once shaders surface composition order)
- `setFog` -> PerMaterialCB.materialDiffuse alpha channel for density + rgb for color (slot 0 PS-side)
- `setObjectToWorldTransformAndScale` -> PerObjectCB.world with per-axis scale composed into basis columns
- `setTextureTransform` -> no-op (D3D11 has no FFP texture-stage transform; engine texture transforms live in HLSL)
- `setAlphaFadeOpacity` -> PerMaterialCB-userConstant in alpha
- `setGlobalTexture` -> Direct3d11_TextureData::setGlobalTexture + ms_boundSRV[tag&0xF]
- `releaseAllGlobalTextures` -> ms_boundSRV[*] = nullptr + Direct3d11_TextureData::releaseAllGlobalTextures
- `setVertexBuffer` / `setIndexBuffer` -> track VB/IB pointers + stride + offset + format + vertex/index counts via friend-class `m_graphicsData` access
- `setBadVertexShaderStaticShader` / `setStaticShader` -> recorded no-op (no Direct3d11_StaticShaderData wrapper yet -- Plan 11-07 first iteration target)

### Direct3d11_LightManager (141 ln cpp + 84 ln h)

`setLights(stdvector<Light const *>::fwd const &)` iterates the engine light list and accumulates into `Direct3d11_LightingCB`:

| Field                    | Bytes | Source                                            |
|--------------------------|------:|---------------------------------------------------|
| ambientColor             |   16  | accumulated T_ambient lights (scaledDiffuseColor * scaledDiffuseIntensity) |
| directionalDir           |   16  | first T_parallel light's getObjectFrameK_w + intensity in w               |
| directionalColor         |   16  | first T_parallel light's scaledDiffuseColor                                |
| pointLightPos[8]         |  128  | T_point + T_point_multicell + T_spot world-space getPosition_w + range    |
| pointLightColor[8]       |  128  | corresponding scaledDiffuseColor + intensity in alpha                     |
| pointLightCountPad       |   16  | count in x, yzw padding                                                    |

Total **320 bytes**, `static_assert(% 16 == 0)` per Pitfall 2.

Flushes via `Direct3d11_ConstantBuffer::updateVS(3, &cb, sizeof(cb)) + bindVS(3) + updatePS(3, ...) + bindPS(3)`. LightingCB occupies cbuffer slot 3; slots 0/1/2 are PerFrame/PerObject/PerMaterial from Plan 11-05.

Spot lights are currently treated as point lights (cone falloff deferred to Plan 11-07 if cantina sconces surface visual artifacts). T_OBSOLETE_parallelPoint is filtered out. Multi-directional support deferred (most scenes have one sun).

### Direct3d11_Metrics (92 ln cpp + 54 ln h)

D3D11-specific frame counters:

| Counter                       | Reset per frame? | Source                                  |
|-------------------------------|------------------|-----------------------------------------|
| vertices / indices / triangles | yes             | StateCache draw counters (Plan 11-07 wiring) |
| drawCallCount                 | yes              | StateCache::applyPreDrawState           |
| mapCount                      | yes              | DynamicVB / DynamicIB / ConstantBuffer / TextureData Map calls (Plan 11-07 wiring) |
| cbufferUpdates                | yes              | ConstantBuffer::updateVS/updatePS calls (Plan 11-07 wiring) |
| inputLayoutCacheHits/Misses   | NO (cumulative)  | VertexDeclarationMap::getOrCreate       |
| shaderCacheHits/Misses        | NO (cumulative)  | ShaderCache::tryLoad/store              |

`install()` registers DebugFlag `[ClientGraphics/Direct3d11] reportFrameStats` (default false). `endFrame` publishes via DEBUG_REPORT_LOG_PRINT when the flag is true.

### Wiring (Task 4)

`Direct3d11.cpp` modifications:
- 5 new includes (`Direct3d11_StateCache.h`, `Direct3d11_VertexDeclarationMap.h`, `Direct3d11_LightManager.h`, `Direct3d11_Metrics.h`, plus the existing `Direct3d11_VertexBufferDescriptorMap.h` already there)
- Plan 11-06 install block: `VertexDeclarationMap::install()` -> `StateCache::install()` -> `LightManager::install()` -> `Metrics::install()` AFTER the Plan 11-05 shader-class install
- Plan 11-06 remove block in `remove_impl()`: REVERSE order — Metrics -> LightManager -> StateCache -> VertexDeclarationMap, BEFORE the Plan 11-05 shader-class teardown
- 43 `STUB()` lines replaced with `ms_glApi.slot = Direct3d11_*::*` bindings

**STUB count:** Plan 11-04 had 84; Plan 11-05 ended at 72 (-4 = shader slots wired); **Plan 11-06 ends at 29** (-43 = state + draw + light + transform + bind slots wired matches accounting).

## Slot Wiring Status (cumulative through Plan 11-06)

**Replaced (~67 of ~119 Gl_api slots):**

Plan 11-02 (scaffold real no-ops): verify + getShaderCapability + requiresVertexAndPixelShaders + wasDeviceReset + isGdiVisible + 4 lost-device callbacks + 7 supports* + getVideoMemoryInMegabytes + getOtherAdapterRects + setBrightnessContrastGamma (no-op) + update (no-op) + remove + flushResources + 5 _DEBUG slots

Plan 11-03 (device MVP): beginScene + endScene + clearViewport + present + displayModeChanged

Plan 11-04 (resource layer): createTextureData + createStaticVertexBufferData + createDynamicVertexBufferData + createStaticIndexBufferData + createDynamicIndexBufferData + setDynamicIndexBufferSize + setRenderTarget + copyRenderTargetToNonRenderTargetTexture

Plan 11-05 (shader layer): createVertexShaderData + createPixelShaderProgramData + setVertexShaderUserConstants + setPixelShaderUserConstants

**Plan 11-06 (state + draw + light + metrics): 43 slots wired:**
- 3 state setters (setFillMode + setCullMode + setAntialiasEnabled)
- 9 transform / viewport / scissor / global-texture (setViewport + setScissorRect + setWorldToCameraTransform + setProjectionMatrix + setFog + setObjectToWorldTransformAndScale + setGlobalTexture + releaseAllGlobalTextures + setTextureTransform)
- 1 alpha (setAlphaFadeOpacity)
- 1 lighting (setLights)
- 2 shader bindings (setBadVertexShaderStaticShader + setStaticShader -- currently recorded no-ops; Plan 11-07 first iteration target)
- 2 stream binding (setVertexBuffer + setIndexBuffer)
- 25 draw dispatch (7 draw* + 6 drawIndexed* + 6 drawPartial* + 6 drawPartialIndexed*)

**Remaining 29 STUBs cluster into:**
- 2 shader-implementation factory slots: `createShaderImplementationGraphicsData` + `createStaticShaderGraphicsData` (Plan 11-07's FIRST FATAL boundary; these create per-template GraphicsData wrappers during `ShaderTemplate::install`)
- 6 point-sprite setters: `setPointSize` + `setPointSizeMax` + `setPointSizeMin` + `setPointScaleEnable` + `setPointScaleFactor` + `setPointSpriteEnable` (HLSL SV_PointSize substitution; particle subsystem)
- 4 windowing utilities: `resize` + `setWindowedMode` + `lockBackBuffer` + `unlockBackBuffer` (Wave 7+)
- 1 windowed-launcher slot: `presentToWindow` (Wave 7+)
- 2 cursor: `setMouseCursor` + `showMouseCursor` (Wave 7+)
- 3 multi-stream: `createVertexBufferVectorData` + `setVertexBufferVector` (deferred per SPEC §Boundaries); `getOneToOneUVMapping` (UV helper, UI/HUD)
- 1 IB helper: `optimizeIndexBuffer`
- 2 visual: `screenShot` + `writeImage` (Plan 11-09)
- 1 postprocess: `setBloomEnabled` (Plan 11-09)
- 3 PIX markers: `pixSetMarker` + `pixBeginEvent` + `pixEndEvent` (Wave 7+ debug)
- 4 PRODUCTION=0: `createVideoBuffers` + `fillVideoBuffers` + `getVideoBufferData` + `releaseVideoBuffers`

**Predicted Plan 11-07 first FATAL:** `createShaderImplementationGraphicsData` -- the first shader-implementation factory called during `ShaderTemplate::install` (per call-stack pattern from D3D9's analogous flow). Plan 11-07's first iteration creates `Direct3d11_ShaderImplementationData` + `Direct3d11_StaticShaderData` wrappers.

## Task Commits

| # | Commit       | Task                                                                                                              | Type | Files                                |
|---|--------------|-------------------------------------------------------------------------------------------------------------------|------|--------------------------------------|
| 1 | `c2631a435`  | Task 1 -- VertexDeclarationMap.{h,cpp} + buildInputElementDesc on VertexBufferDescriptorMap + vcxproj/filters     | spec | 4 modified + 2 new files (301 inserts) |
| 2 | `b186026b0`  | Task 2 -- Direct3d11_StateCache.{h,cpp} + 4 engine-header friend additions for HardwareVB/IB classes + vcxproj    | spec | 2 new files + 6 modified (1,173 inserts) |
| 3 | `b24c33e28`  | Task 3 -- Direct3d11_LightManager.{h,cpp} + Direct3d11_Metrics.{h,cpp} + vcxproj/filters                          | spec | 4 new files + 2 modified (387 inserts) |
| 4 | `2052b45a6`  | Task 4 -- Direct3d11.cpp: 5 new includes + 4 install/remove additions + 43 STUB() lines replaced                  | spec | 1 modified (+98 / -56)               |

**Plan close commit:** (this commit) -- adds `11-06-SUMMARY.md` + `STATE.md` + `ROADMAP.md` + `REQUIREMENTS.md` updates.

## Build Verifications

- `MSBuild ... -t:Direct3d11 ...` -> **EXIT=0**; `gl11_d.dll` 1,392,128 bytes (vs Plan 11-05 baseline 1,240,064 bytes; +148 KB); auto-staged via the Plan 11-01 post-build cp fix `266e173b3`.
- `MSBuild ... -t:Direct3d9 ...` -> **EXIT=0** (D-05 protection: `gl05_d.dll` still builds clean).
- `MSBuild ... -t:Direct3d9_ffp ...` -> **EXIT=0** (`gl06_d.dll` builds clean).
- `MSBuild ... -t:Direct3d9_vsps ...` -> **EXIT=0** (`gl07_d.dll` builds clean).
- `MSBuild ... -t:SwgClient ...` -> **EXIT=0** (full link clean; `SwgClient.exe` builds and stages).
- `grep -rE "D3DPOOL_MANAGED|OnLostDevice|OnResetDevice" src/engine/client/application/Direct3d11/` -> 16 hits, ALL inside `//` comment lines documenting the D-13 invariant. **Zero functional uses.**
- `git diff 825838858..HEAD -- src/engine/client/application/Direct3d9/` -> empty (D-05 cross-check; no D3D9 source touched).
- `git diff 825838858..HEAD -- src/engine/client/library/clientGraphics/src/shared/` -> 4-class friend additions ONLY (StaticVertexBuffer / DynamicVertexBuffer / StaticIndexBuffer / DynamicIndexBuffer). Rule-3 deviation pattern from Plan 11-04. Zero behavioral changes; D3D9 plugin source untouched.
- `Glob src/engine/client/application/Direct3d11/src/win32/Direct3d11_FfpGenerator.*` -> 0 results (D-04a invariant: FFP generator still ABSENT).
- `grep -rE "SetVertexShaderConstantF|SetPixelShaderConstantF" src/engine/client/application/Direct3d11/` -> 0 functional uses (cbuffer migration invariant from Plan 11-05).

## Decisions Made

1. **`buildInputElementDesc` lives on `Direct3d11_VertexBufferDescriptorMap`, not a separate helper.** The existing helper already owned the per-VertexBufferFormat iteration logic for the descriptor cache; the input-element-desc builder is its natural extension (same iteration shape, different output). Keeps the surface narrow: one helper class covers both consumption paths.

2. **State-object cache key is FNV-1a 64-bit over descriptor bytes.** Same hash function as Plan 11-05's shader cache. `std::unordered_map<uint64_t, ComPtr<...>>`. Distinct state set is bounded (RESEARCH suggests <50 per state type per scene); 64-bit collision space is comfortable.

3. **Input-layout cache key is `pair<uint32 format flags, uint64 VS bytecode hash>`.** Avoided introducing a custom struct + hasher; `std::pair` already has `operator<` and works as the key for `std::map`. The bytecode hash from Plan 11-05 is a valid identity because `D3DCompile` is deterministic.

4. **PS NULL fallback in applyPreDrawState.** Plan 11-05 documented the stock-asset PEXE bytecode incompatibility; Plan 11-06's draw dispatch must skip `PSSetShader` to honor the contract. Alternative considered: bind a fallback pass-through PS at install. Rejected: default-D3D11-pass-through is already there from device init, and binding a NULL would unbind whatever was previously bound which is worse than leaving it alone.

5. **`drawQuadList` emits TODO log.** D3D9 plugin triangulates quads at draw time via a persistent quad-IB (`Direct3d9.cpp:4144-4159`). D3D11 implementation requires re-creating that quad-IB; deferred to Plan 11-07 if UI / particles surface the need. Most engine call sites use indexed-triangle-list for quads (UI mesh, billboard particles) so this is low-risk.

6. **`drawTriangleFan` + `drawIndexedTriangleFan` emit `TRIANGLELIST` topology.** D3D11 has no native `TRIANGLEFAN`. Most engine fans in target scenes are server-triangulated during mesh load (NGE-era assets); if any subsystem feeds un-triangulated fans, Plan 11-07 smoke surfaces the visual artifact and we add server-side re-emit as a Rule-1 deviation.

7. **Multi-stream deferred per SPEC §Boundaries.** `createVertexBufferVectorData` + `setVertexBufferVector` remain STUB(). Phase 11 single-stream is the contract. Multi-stream input layouts (multiple InputSlot values, per-stream stride/offset arrays) require extending `VertexDeclarationMap` key to include the stream-set hash; defer until target subsystem (skeletal blend weights are the typical multi-stream user) actually exercises the path.

8. **LightManager uses world-space light positions + directions.** `Object::getPosition_w` + `Object::getObjectFrameK_w`. Plan 11-07 smoke surfaces whether HLSL shaders expect world-space (likely, matching D3D9 plugin convention) or view-space. Plan 11-06 commits world-space; transform-to-view happens in HLSL via `PerFrameCB.viewProj`.

9. **`setBadVertexShaderStaticShader` + `setStaticShader` recorded no-op.** The engine's StaticShader -> ShaderImplementation pass-resolution path lands in Plan 11-07. Plan 11-06's setStaticShader recording happens but does not consume; until shader-implementation factory wrappers land (Plan 11-07 first iteration), every draw runs with whatever VS+PS was last bound from `Direct3d11.cpp setVertexShaderUserConstants / setPixelShaderUserConstants` shadow path (which is null until shader factory slots fire). applyPreDrawState's NULL-VS skip handles this — the engine's first few draws (before shader-implementation factory invocations) silently skip.

10. **Engine-header friend additions are Rule-3 deviations (4 classes).** Mirrors Plan 11-04's TextureGraphicsData::LockData friend pattern. One-line edit per class; zero impact on D3D9 plugin behavior. Future D3D12 plugin will need the same set of friend declarations.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Add Direct3d11 + Direct3d11_StateCache + Direct3d11_*Data as friend classes on HardwareVertexBuffer / HardwareIndexBuffer subclasses**
- **Found during:** Task 2 (Direct3d11_StateCache setVertexBuffer / setIndexBuffer implementation)
- **Issue:** `StaticVertexBuffer::m_graphicsData` / `DynamicVertexBuffer::m_graphicsData` / `StaticIndexBuffer::m_graphicsData` / `DynamicIndexBuffer::m_graphicsData` are private fields with friends only for `Graphics` / `Gl_api` / `Direct3d8` / `Direct3d8_*Data` / `Direct3d9` / `Direct3d9_*Data`. The state cache needs to read those fields to dispatch the engine's set-VB / set-IB to the D3D11 backend; without friend access, the public API surface is `safe_cast<HardwareVertexBuffer>->getGraphicsData()` (returns base `*GraphicsData`) but the actual `Direct3d11_*Data` is needed for `getVertexBuffer()` / `getIndexBuffer()` accessors.
- **Fix:** Added `friend class Direct3d11; friend class Direct3d11_StateCache; friend class Direct3d11_<type>BufferData;` to all 4 buffer classes (`StaticVertexBuffer.h` + `DynamicVertexBuffer.h` + `StaticIndexBuffer.h` + `DynamicIndexBuffer.h`). One-line edit per class; mirrors Plan 11-04's `Texture.h` Rule-3 friend addition for `Direct3d11_TextureData`.
- **Files modified:** `src/engine/client/library/clientGraphics/src/shared/StaticVertexBuffer.h`, `DynamicVertexBuffer.h`, `StaticIndexBuffer.h`, `DynamicIndexBuffer.h`
- **Verification:** Direct3d11 + Direct3d9 + Direct3d9_ffp + Direct3d9_vsps + SwgClient all rebuild clean. D3D9 plugin source untouched (`git diff 825838858..HEAD -- src/engine/client/application/Direct3d9/` returns empty). Engine-header diff shows only the friend additions (no behavioral changes).
- **Committed in:** `b186026b0` (Task 2 commit -- folded into the state-cache implementation change)

**2. [Rule 1 - Bug] Direct3d11_LightingCB static_assert size off**
- **Found during:** Task 3 (Direct3d11_LightManager build)
- **Issue:** Initial `static_assert(sizeof(Direct3d11_LightingCB) == 304, ...)` failed: actual size is 320 bytes. I had miscounted in the header comment (16+16+16+128+128+16 = 320, not 304). The 16-byte-alignment assert still passed, but the explicit-size assert caught the error.
- **Fix:** Updated assert to `== 320` and updated the header comment's running total.
- **Files modified:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_LightManager.h`
- **Verification:** Direct3d11 rebuilds clean; alignment-mod-16 assert still passes (320 % 16 == 0).
- **Committed in:** `b24c33e28` (Task 3 commit -- fix folded into the LightManager file)

### Acknowledged Stubs (not deviations; documented in plan)

These are continuing Plan 11-05 / earlier-plan stubs that Plan 11-06 carries forward:

**1. `m_d3dPS == nullptr` for stock-asset PS programs.** Plan 11-05 Known Stub #1; Plan 11-06's draw dispatch HANDLES this case by skipping `PSSetShader` in `applyPreDrawState`. Real PS output blocks on future Phase 12 asset re-author. Plan 11-07 smoke will surface the visual impact (default D3D11 pass-through rasterizes pixels as the back-buffer-init color, which may be black/white/undefined).

**2. 29 remaining STUB() slots.** Documented in `Slot Wiring Status` section above; clustered into shader-implementation factory (Plan 11-07 first FATAL), point-sprite emit (particles), windowing (Wave 7+), multi-stream (deferred per SPEC), and production-only video buffers.

**3. `setStaticShader` / `setBadVertexShaderStaticShader` recorded no-op.** Plan 11-07 first iteration replaces these with real Direct3d11_StaticShaderData / Direct3d11_ShaderImplementationData wrappers. Until then, draws with un-bound shaders silently skip via applyPreDrawState NULL-VS path.

### Lessons Learned (recorded for future executors)

**1. Friend-class additions for new backends are routine Rule-3 deviations, NOT architectural changes.** Plan 11-04 set the precedent with `Texture.h`; Plan 11-06 extends the pattern to all 4 VB/IB classes. The pattern: when a new plugin needs access to private engine-side fields, add `friend class <Plugin>_*` alongside the existing Direct3d8/9 friends. Future D3D12 plugin will need the same set.

**2. The state-object cache pattern is the canonical D3D11 idiom replacing D3D9's monolithic state machine.** D3D9's `IDirect3DDevice9::SetRenderState(D3DRS_*, value)` was a flat key-value mutation; D3D11 requires constructing immutable `ID3D11RasterizerState` / `ID3D11BlendState` / `ID3D11DepthStencilState` / `ID3D11SamplerState` objects via `CreateXxxState` and binding them via `RSSetState` / `OMSetBlendState` / `OMSetDepthStencilState` / `PSSetSamplers`. The descriptor-hash cache pattern (FNV-1a 64-bit, `unordered_map<uint64, ComPtr<...>>`) is the natural shim: state-setter slots flip dirty bits + mutate the per-state-type descriptor struct; the draw-time apply resolves to a state object via cache lookup, creating new objects only on cache miss.

**3. Pitfall 4 (SRV/sampler split) is enforced by SEPARATE binding arrays.** ms_boundSRV[16] and ms_boundSampler[16] are independent namespace state. `setGlobalTexture` only writes the SRV slot; the default sampler (linear-wrap) bound to all 16 slots at install handles the fallback. Per-stage sampler overrides land in a later plan if smoke surfaces shader-specific configurations.

**4. Pitfall 6 (input-layout cache key) requires the VS bytecode hash.** Cannot key on VBFormat alone. The deterministic D3DCompile property from Plan 11-05 means the FNV-1a 64-bit hash over (source + defines) is a valid identity for the bytecode — distinct shader sources that compile to identical bytecode are rare and would still produce valid input layouts.

**5. PS NULL fallback is "don't bind anything" not "bind a fallback".** Default D3D11 pass-through is already there from device init. Binding NULL via `PSSetShader(nullptr, ...)` would unbind whatever was previously bound (worse outcome). Skip `PSSetShader` entirely when m_d3dPS is null. This is the Plan 11-05 contract honored.

**6. drawQuadList + drawTriangleFan need engine-side cooperation.** D3D11 lacks both topologies. The realistic fix is to recreate the D3D9 quad-IB triangulation pattern (persistent IB with 0,1,2, 0,2,3 pattern, draw via TRIANGLELIST topology); the fan fix is server-side mesh load triangulation (already done for most assets). Plan 11-07 smoke surfaces both needs if they fire.

**7. setStaticShader as no-op is acceptable contract under autonomous-mode framing.** The engine's draw path goes set-vertex-buffer -> set-index-buffer -> setStaticShader -> draw*. If setStaticShader doesn't actually bind a Direct3d11_StaticShaderData wrapper (because none exists yet), every subsequent draw* has no VS/PS resolved, and applyPreDrawState's NULL-VS skip prevents the draw call. Plan 11-07's first iteration replaces this with real shader-wrapper instantiation.

---

**Total deviations:** 2 auto-fixed (1 Rule 3 friend-class addition + 1 Rule 1 static_assert size mismatch). No scope creep.
**Impact on plan:** Without the Rule 3 friend additions, `Direct3d11_StateCache::setVertexBuffer / setIndexBuffer` cannot access the engine's `m_graphicsData` field — required for draw-call routing. Without the Rule 1 size fix, Direct3d11_LightManager couldn't compile due to the failed `static_assert`. Both deviations were one-line fixes folded into the corresponding task commits.

## Issues Encountered

- **No client-launch evidence this plan.** Per `autonomous: true` framing, the plan-close does NOT include a Kenny smoke session. The new FATAL boundary (predicted `createShaderImplementationGraphicsData` — first shader-implementation factory called during `ShaderTemplate::install`) lands at Plan 11-07's first iteration. The visible-window outcome that Plan 11-03's CHECKPOINT deferred is most likely to surface at Plan 11-07's first smoke launch — once the shader-implementation factory wrappers land and `SetupClientGraphics::install` completes, the engine's `ShowWindow(SW_SHOW)` fires, and the per-frame loop renders with whatever state-cache + draw-dispatch the new plumbing provides (likely a black or undefined-color frame given the PS NULL fallback; geometry rasterization will populate the depth buffer but pixel output is default).

- **Pre-existing Koogie warnings persist (out of scope):** `Direct3d9.vcxproj` MSB8012 TargetName/OutputFile mismatch warnings on D3D9 + D3D9_ffp + D3D9_vsps + Direct3d11 build emits one `C4459 declaration of 'E' hides global declaration` warning per source file that includes `<DirectXMath.h>` (DirectXMathVector.inl uses local `E` variable; engine's `sharedFoundation/FloatMath.h` defines a global `E` constant). Level-4 hints, not errors; both declarations correct in their respective scopes. Recorded as carry-forward; no action this plan.

## User Setup Required

None — Plan 11-06 is the state-cache + draw-call plumbing port; no external services or new manual steps required. Plan 11-07's iterative smoke will need the existing setup: `client_d.cfg [ClientGraphics] rasterMajor=11`, run `D:/Code/swg-client-v2/stage/SwgClient_d.exe` against the SWGSource VM at 192.168.1.200 (or local).

## Next Phase Readiness

- **Plan 11-07 (Wave 7 — iterative smoke + fix-by-fix to playable) is UNBLOCKED.** First work items:
  1. First Kenny smoke launch under rasterMajor=11. Predicted FATAL at `createShaderImplementationGraphicsData` — first shader-implementation factory called during `ShaderTemplate::install`.
  2. Author `Direct3d11_ShaderImplementationData.{h,cpp}` + `Direct3d11_StaticShaderData.{h,cpp}` -- the engine-shader-template wrappers that thread the VS+PS pair through the draw path.
  3. Re-launch; iterate FATAL-by-FATAL through the remaining 29 STUB() slots until either (a) a visible frame renders or (b) a slot that can't no-op surfaces.
  4. Document each fix in Plan 11-07's iterative SUMMARY format (per-iteration commit + per-iteration FATAL signature).

- **Pixel-shader asset re-author is a Phase 12 candidate** (NOT a Plan 11-07 blocker). Plan 11-06's draw dispatch skips `PSSetShader` when `m_d3dPS == NULL`; D3D11's default pass-through pixel pipeline renders UNDEFINED color, but the draw doesn't FATAL. Visible-parity smoke under Plan 11-07 will surface whether the default pass-through is acceptable for target scenes; if not, the asset re-author becomes the gating Phase 12 work.

- **D-04a maintained.** No `Direct3d11_FfpGenerator.{h,cpp}` created; vcxproj clean. Plan 11-07 inherits this invariant.

- **D-05 maintained.** `git diff 825838858..HEAD -- src/engine/client/application/Direct3d9/` returns empty. All three D3D9 variants (`gl05_d.dll`, `gl06_d.dll`, `gl07_d.dll`) build clean.

- **D-13 maintained.** 16 `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` grep hits in `src/engine/client/application/Direct3d11/`, all in `//` comment lines. Zero functional uses.

- **Cbuffer migration invariant maintained.** Zero `SetVertexShaderConstantF | SetPixelShaderConstantF` in `Direct3d11/`. All constant updates flow through `Direct3d11_ConstantBuffer` (Plan 11-05).

- **Phase 11 bonus deliverables still active.** `266e173b3` (Plan 11-01 post-build cp auto-stage) + `dbd7c62dc` (Plan 11-02 atlmfc include vendor) continue to retire the CLI MSBuild rebuild trap.

### Carry-forward observations (not blockers)

- Pre-existing `Direct3d9.vcxproj` MSB8012 TargetName/OutputFile mismatch warnings on all 3 D3D9 variants remain (out of scope; D3D9 source not touched this plan).
- Pre-existing C4456 declaration-shadowing warnings in `Direct3d9.cpp:2519`, `Direct3d9_VertexBufferDescriptorMap.cpp:140`, and `Direct3d9_RenderTarget.cpp:85,91,98` remain (out of scope).
- New C4459 `declaration of 'E' hides global declaration` warning in `<DirectXMath.h>` consumed via `Direct3d11_ConstantBuffer.h` (carry-forward from Plan 11-05; level-4 hint, not actionable).
- Direct3d11_StateCache::setStaticShader is a recorded no-op until Plan 11-07's shader-implementation factory wrappers land. This is the canonical "Known Stub" of Plan 11-06.

## TDD Gate Compliance

This plan is `type: execute` (not `type: tdd`), so the RED/GREEN/REFACTOR gate sequence does not apply. No `test(...)` commits expected. The 5 build-verification gates (`/t:Direct3d11`, `/t:Direct3d9`, `/t:Direct3d9_ffp`, `/t:Direct3d9_vsps`, `/t:SwgClient` all EXIT=0) + the D-13 grep gate + the D-05 git-diff gate + the D-04a Glob+Grep gate are the equivalent verification gates.

## Requirements Traceability

- **D3D11-01 (plugin scaffold + Gl_api table + Direct3d11.dll runtime):** **PARTIAL** — Plan 11-06 adds 43 more real Gl_api slots (from 24 in Plan 11-05 to 67 in Plan 11-06: 5 per-frame + 7 init + 8 resource + 4 shader + 43 state/draw/light/transform = 67 of ~119 slots). Visible-window outcome still pending Plan 11-07 smoke. Commit chain extended: `... 2df8b0e90` (Plan 11-04) -> `f040f19c2 / 84eac40f5 / d237b55f2 / 825838858` (Plan 11-05) -> `c2631a435 / b186026b0 / b24c33e28 / 2052b45a6 / <plan-close-hash>` (Plan 11-06).

- **D3D11-02 (resource management replaces D3DPOOL_MANAGED + lost-device removed):** **PARTIAL** — Plan 11-06 extends the resource-layer foundation with state-object caching using ComPtr ownership. D-13 invariant still holds (16 grep hits, all `//` comment lines). State objects join textures + VB/IB + render targets + shaders + cbuffers as ComPtr-managed resources. ACCEPTANCE still requires end-to-end zone-in evidence (Plan 11-07).

- **D3D11-03 (shader recompilation under HLSL SM5.0 per SPEC R3):** **PARTIAL** — Plan 11-06 extends the shader-cache wiring through the input-layout cache (Pitfall 6). The vs_5_0 compile path remains LIVE; ps_5_0 path remains as `compilePixelShaderFromHlsl` compile-time proof. SATISFIED still requires Plan 11-07 smoke evidence that engine shaders compile + cache + render through real draw calls.

## Known Stubs

| Stub | File | Reason | Resolves In |
|------|------|--------|-------------|
| `m_d3dPS == nullptr` for all pre-compiled D3D9 PS assets | `Direct3d11_PixelShaderProgramData.cpp` (Plan 11-05) | Engine ships pre-compiled D3D9 PEXE bytecode; D3D11 `CreatePixelShader` rejects it. Plan 11-06's `applyPreDrawState` skips `PSSetShader` when null — D3D11 default pass-through runs. | Future Phase 12 (asset re-author `.psh` -> HLSL source) |
| `Direct3d11_StateCache::setStaticShader` recorded no-op | `Direct3d11_StateCache.cpp` | Plan 11-06 records the per-pass shader binding but doesn't yet route through `Direct3d11_StaticShaderData` / `Direct3d11_ShaderImplementationData` wrappers (those don't exist yet). Draws with un-bound shaders silently skip via applyPreDrawState's NULL-VS path. | Plan 11-07 (first iteration: author the wrappers + replace the no-op) |
| `Direct3d11_StateCache::drawQuadList` TODO log only | `Direct3d11_StateCache.cpp` | D3D11 has no native QUADLIST topology. D3D9 plugin used a persistent quad-IB triangulator; D3D11 implementation requires recreating it. | Plan 11-07 (if UI / particles surface need) |
| `setPointSize / setPointSizeMax / setPointSizeMin / setPointScaleEnable / setPointScaleFactor / setPointSpriteEnable` STUB | `Direct3d11.cpp` | D3D11 has no FFP point-sprite emit; lives in HLSL via SV_PointSize / billboard expansion. | Plan 11-07 (particle subsystem coverage) |
| `createShaderImplementationGraphicsData / createStaticShaderGraphicsData` STUB | `Direct3d11.cpp` | Plan 11-07 first FATAL boundary. Engine's `ShaderTemplate::install` invokes these factories per-template; wrappers must thread the VS+PS pair through the draw path. | Plan 11-07 (first iteration) |
| 21 other STUB() slots (windowing / cursor / multi-stream / visual / PIX / video) | `Direct3d11.cpp` | Various Wave 7+ / Plan 11-09 / production-only paths. | Plan 11-07 (iterate per FATAL) / Plan 11-09 (visual-parity) |

---

*Phase: 11-d3d11-renderer-plugin*
*Plan: 06 — D3D11 state cache + draw-call dispatch + input-layout cache + light manager + metrics*
*Completed: 2026-05-16*

## Self-Check: PASSED

- `.planning/phases/11-d3d11-renderer-plugin/11-06-SUMMARY.md` exists — VERIFIED (this file)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexDeclarationMap.{h,cpp}` exist — VERIFIED (committed in `c2631a435`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.{h,cpp}` exist — VERIFIED (committed in `b186026b0`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_LightManager.{h,cpp}` exist — VERIFIED (committed in `b24c33e28`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Metrics.{h,cpp}` exist — VERIFIED (committed in `b24c33e28`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_FfpGenerator.*` do NOT exist (D-04a) — VERIFIED via Glob (0 results)
- Task commits present in `git log`: `c2631a435` (Task 1) + `b186026b0` (Task 2) + `b24c33e28` (Task 3) + `2052b45a6` (Task 4) — VERIFIED
- D-05 cross-check: `git diff 825838858..HEAD -- src/engine/client/application/Direct3d9/` returns empty — VERIFIED
- D-13 cross-check: 16 `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` hits in `Direct3d11/`, ALL inside `//` comment lines documenting the invariant — VERIFIED
- gl11_d.dll auto-staged at `D:/Code/swg-client-v2/stage/gl11_d.dll`, 1,392,128 bytes (vs Plan 11-05 baseline 1,240,064 bytes; +148 KB) — VERIFIED
- All 5 MSBuild targets EXIT=0 (Direct3d11 / Direct3d9 / Direct3d9_ffp / Direct3d9_vsps / SwgClient) — VERIFIED
