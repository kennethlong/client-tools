# Phase 11: D3D11 Renderer Plugin — Pattern Map

**Mapped:** 2026-05-15
**Files analyzed:** 21 new + 2 engine-side surgical edits + 1 vcxproj
**Analogs found:** 21 / 21 (every new plugin file has a direct `Direct3d9/` sibling)
**Mapping strategy:** Phase 11 is wholly additive. The single dominant analog tree is `D:/Code/swg-client-v2/src/engine/client/application/Direct3d9/` — for every new file the planner must cite the same-basename D3D9 sibling for *contract shape and Gl_api semantics*, then apply the divergences listed in §Common Pitfalls of `11-RESEARCH.md` (ComPtr ownership, no `D3DPOOL_MANAGED`, `vs_5_0`/`ps_5_0` profile, `SV_POSITION` semantic injection, sampler/SRV split, flip-model swap chain, no FFP setters, no lost-device callbacks).

> **Per CONTEXT D-01:** D3D9 is a *design reference*, not a starting skeleton. Read for `Gl_api` slot intent and ms-namespace conventions; rewrite the implementation from a clean page.

---

## §Summary

| New file (under `src/engine/client/application/Direct3d11/src/win32/` unless noted) | Closest analog | Role | Match | Key divergence from analog |
|---|---|---|---|---|
| `Direct3d11.{h,cpp}` | `Direct3d9.cpp` (4764 ln), `Direct3d9.h` (98 ln) | entry point + ms_glApi populator | exact | No `#ifdef FFP/VSPS` gating (single variant); GetApi exports unchanged; install body builds `ID3D11Device`+`IDXGISwapChain1` instead of `IDirect3DDevice9` |
| `ConfigDirect3d11.{h,cpp}` | `ConfigDirect3d9.cpp` (229 ln), `.h` (61 ln) | plugin config | exact | Drop `getMaxVertexShaderVersion` / `getMaxPixelShaderVersion` (always SM5.0); add `getShaderCacheDir` (D-03) |
| `FirstDirect3d11.{h,cpp}` | `FirstDirect3d9.h` (18 ln) | precompiled-header stub | exact | Identical structure; rename guard, change include comment |
| `Direct3d11_Device.{h,cpp}` | inline inside `Direct3d9.cpp:952-…` `Direct3d9::install` | device + swap chain + RTV/DSV | role-match (D3D9 inlines this; D3D11 promotes it to a sibling per RESEARCH §Recommended Project Structure) | flip-model `IDXGISwapChain1`, `D3D_FEATURE_LEVEL_11_0`, `OMSetRenderTargets` rebind every frame (Pitfall 3); no lost-device |
| `Direct3d11_StateCache.{h,cpp}` | `Direct3d9_StateCache.cpp` (573 ln) | RS/BS/DSS/sampler dirty-bit cache | exact | Drop all `D3DTSS_*` setters (lines 159,162,382,383); split SRV cache from sampler cache (Pitfall 4); cbuffer replaces `SetVertexShaderConstantF`/`SetPixelShaderConstantF` (lines 526,561) |
| `Direct3d11_TextureData.{h,cpp}` | `Direct3d9_TextureData.cpp` (735 ln) | texture + view | exact | `ID3D11Texture2D` + `ID3D11ShaderResourceView` (two separate objects); DXGI_FORMAT translation table replaces `D3DFORMAT translationTable[]` (lines 33-54); no `D3DPOOL_MANAGED` |
| `Direct3d11_StaticVertexBufferData.{h,cpp}` | `Direct3d9_StaticVertexBufferData.cpp` (156 ln) | immutable VB | exact | `D3D11_USAGE_IMMUTABLE` + `D3D11_BIND_VERTEX_BUFFER`; no `IDirect3DVertexBuffer9::Lock`; pInitialData on create |
| `Direct3d11_DynamicVertexBufferData.{h,cpp}` | `Direct3d9_DynamicVertexBufferData.cpp` (280 ln) | dynamic VB ring | exact | `D3D11_USAGE_DYNAMIC` + `D3D11_MAP_WRITE_DISCARD` on wrap, `D3D11_MAP_WRITE_NO_OVERWRITE` on append (Pitfall 5); preserve the existing wrap/rotate logic |
| `Direct3d11_StaticIndexBufferData.{h,cpp}` | `Direct3d9_StaticIndexBufferData.cpp` (141 ln) | immutable IB | exact | `D3D11_BIND_INDEX_BUFFER`; index format passed to `IASetIndexBuffer` per draw |
| `Direct3d11_DynamicIndexBufferData.{h,cpp}` | `Direct3d9_DynamicIndexBufferData.cpp` (196 ln) | dynamic IB ring | exact | mirror dynamic VB pattern |
| `Direct3d11_VertexShaderData.{h,cpp}` | `Direct3d9_VertexShaderData.cpp` (573 ln) — esp. compile site at `:433` | VS compile + bytecode + input layout cache | exact | `D3DCompile` (not `D3DXCompileShader`); profile `vs_5_0`; `ID3D11InputLayout` keyed by `(VBFormat, VS bytecode SHA1)` (Pitfall 6); inject `POSITION→SV_POSITION` macro (Pitfall 1); no `_clearfp()` (D3DX-era FPU bug, gone) |
| `Direct3d11_PixelShaderProgramData.{h,cpp}` | `Direct3d9_PixelShaderProgramData.cpp` (50 ln) | PS compile + bytecode | exact | `D3DCompile` profile `ps_5_0`; `CreatePixelShader(blob)` |
| `Direct3d11_ConstantBuffer.{h,cpp}` | (NEW — no analog) | cbuffer wrapper replacing register-file setters | none | `ID3D11Buffer` with `D3D11_BIND_CONSTANT_BUFFER`; `XMFLOAT4X4` layout with 16-byte boundary discipline (Pitfall 2); replaces `SetVertexShaderConstantF` at `Direct3d9_StateCache.cpp:526` and `SetPixelShaderConstantF` at `:561` |
| `Direct3d11_ShaderCache.{h,cpp}` | (NEW — D-03 hybrid) | `.cso` disk cache, hash-keyed | none | source-hash → `<hash>.cso` on disk; load blob direct on hit, `D3DCompile`+write on miss; cache dir = Claude's-discretion per CONTEXT |
| `Direct3d11_FfpGenerator.{h,cpp}` | `Direct3d9_ShaderImplementationData.cpp` (lines 138-164 — the `D3DTSS_*` builders) | runtime `ps_5_0` generator | role-match — **CONDITIONAL on D-04 spike outcome** | Phase A (static grep) → Phase B (runtime instrumentation at the four FFP setter sites listed in §Critical Call-Site References) → either generate or descope per D-04a |
| `Direct3d11_RenderTarget.{h,cpp}` | `Direct3d9_RenderTarget.cpp` (341 ln) | offscreen RT management | exact | `ID3D11RenderTargetView` + matched `ID3D11ShaderResourceView` for read-back; no `D3DPOOL_*` |
| `Direct3d11_LightManager.{h,cpp}` | `Direct3d9_LightManager.cpp` (962 ln) | light list → shader constants | exact | constants flow through `Direct3d11_ConstantBuffer` not `SetVertexShaderConstantF`; SOE light-list shape unchanged |
| `Direct3d11_Metrics.{h,cpp}` | `Direct3d9_Metrics.cpp` (313 ln) | debug counters | exact | rename counters; drop FFP-specific counters (`D3DTSS_*`); add D3D11-specific counters (`Map`/`Unmap`, cbuffer updates) |
| `Direct3d11_VertexDeclarationMap.{h,cpp}` | `Direct3d9_VertexDeclarationMap.cpp` (253 ln) | `ID3D11InputLayout` cache | exact | key includes VS bytecode hash (Pitfall 6) — D3D9 keyed by VB format alone |
| `Direct3d11_VertexBufferDescriptorMap.{h,cpp}` | `Direct3d9_VertexBufferDescriptorMap.cpp` (167 ln) | VB-format-to-descriptor map | exact | descriptor describes `D3D11_INPUT_ELEMENT_DESC[]` instead of `D3DVERTEXELEMENT9[]` |
| `build/win32/Direct3d11.vcxproj` | `Direct3d9.vcxproj` (314 ln) | MSBuild project | exact | Single variant (no FFP/VSPS sister projects); `d3d9.lib;d3dx9.lib;dxerr9.lib` → `d3d11.lib;d3dcompiler.lib;dxgi.lib;dxguid.lib`; output `gl11_d.dll` (etc.); base address `0x60A00000` (Pitfall 7); drop `DIRECT3D9_EXPORTS;FFP;VSPS` defines, add `DIRECT3D11_EXPORTS` only |

**Plus 3 D3D9 plugin files with NO D3D11 analog** (intentional drop per D-01 / SPEC R2):
- `Direct3d9_StaticShaderData.{h,cpp}` — folded into `Direct3d11_StateCache` or inlined (D3D11 model-shader-state binding is simpler).
- `Direct3d9_VertexBufferVectorData.{h,cpp}` — multi-stream vertex buffer plumbing, only needed if multi-stream is exercised by target scenes (defer to subsystem-coverage plan).
- `Direct3d9_PixelShaderConstantRegisters.h` / `Direct3d9_VertexShaderConstantRegisters.h` / `Direct3d9_VertexShaderVertexRegisters.h` — register-file headers that do not survive the cbuffer migration.

---

## §File-by-file mapping

### `Direct3d11.cpp` + `Direct3d11.h`

**Analog:** `src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp` (4764 ln), `Direct3d9.h` (98 ln)

**FFP/VSPS gate (REMOVE — D3D11 is single-variant)** — `Direct3d9.cpp:93-95`:
```cpp
#if !defined(FFP) && !defined(VSPS)
#error must define FFP, VSPS, or both
#endif
```
D3D11 file omits this entire block. No preprocessor gating; the plugin compiles as a single `Direct3d11.dll` per CONTEXT D-01.

**Namespace skeleton + Gl_api forward decls** — `Direct3d9.cpp:99-228` (excerpt):
```cpp
namespace Direct3d9Namespace
{
    typedef void (*CallbackFunction)();
    typedef std::vector<CallbackFunction> CallbackFunctions;

    bool                               verify();
    void                               remove();
    void                               displayModeChanged();
    bool                               wasDeviceReset();
    void                               flushResources(bool fullReset);
    bool                               requiresVertexAndPixelShaders();
    // ... ~150 more forward declarations matching every Gl_api slot
    StaticVertexBufferGraphicsData *   createVertexBufferData(const StaticVertexBuffer &vertexBuffer);
    DynamicVertexBufferGraphicsData *  createVertexBufferData(const DynamicVertexBuffer &vertexBuffer);
    TextureGraphicsData *              createTextureData(const Texture &texture, const TextureFormat *runtimeFormats, int numberOfRuntimeFormats);
}
using namespace Direct3d9Namespace;
```
**D3D11 mirror:** `namespace Direct3d11Namespace { ... }` with the same forward decls; same `using namespace` line. Slot names are the contract (engine-side `Gl_api`), so they must match verbatim. Implementations diverge.

**GetApi export** — `Direct3d9.cpp:501-510`:
```cpp
extern "C" __declspec(dllexport) Gl_api const * GetApi();

Gl_api const * GetApi()
{
    ms_glApi.verify  = verify;
    ms_glApi.install = Direct3d9::install;
    return &ms_glApi;
}
```
**D3D11 mirror:** identical shape, `Direct3d9::install` → `Direct3d11::install`. The `__declspec(dllexport)` + `extern "C"` is the contract `Graphics.cpp:236` `GetProcAddress(ms_dll, "GetApi")` consumes — keep verbatim.

**ms_glApi slot population (~118 slots, FOLLOW VERBATIM SLOT NAMES)** — `Direct3d9.cpp:985-1137` excerpt:
```cpp
ms_glApi.remove                            = Direct3d9Namespace::remove;
ms_glApi.displayModeChanged                = displayModeChanged;
ms_glApi.getShaderCapability               = getShaderCapability;
ms_glApi.requiresVertexAndPixelShaders     = requiresVertexAndPixelShaders;
ms_glApi.wasDeviceReset                    = wasDeviceReset;
ms_glApi.addDeviceLostCallback             = addDeviceLostCallback;
ms_glApi.removeDeviceLostCallback          = removeDeviceLostCallback;
ms_glApi.addDeviceRestoredCallback         = addDeviceRestoredCallback;
ms_glApi.removeDeviceRestoredCallback      = removeDeviceRestoredCallback;
ms_glApi.flushResources                    = flushResources;
ms_glApi.beginScene                        = beginScene;
ms_glApi.endScene                          = endScene;
ms_glApi.present                           = present;
ms_glApi.setRenderTarget                   = setRenderTarget;
ms_glApi.screenShot                        = screenShot;
ms_glApi.setStaticShader                   = setStaticShader;
ms_glApi.createTextureData                 = createTextureData;
// ... draw* slots (drawPointList, drawLineList, drawTriangleList, drawIndexedTriangleList, etc.)
ms_glApi.pixSetMarker = pixSetMarker;
ms_glApi.pixBeginEvent = pixBeginEvent;
ms_glApi.pixEndEvent = pixEndEvent;
```
**D3D11 mirror:** copy slot names verbatim; right-hand-sides point to D3D11Namespace functions. RESEARCH §Code Examples lines 510-522 specifies that lost-device callback slots **must still be registered as no-ops** (the engine queries them):
```cpp
ms_glApi.addDeviceLostCallback     = [](Gl_api::CallbackFunction){};
ms_glApi.removeDeviceLostCallback  = [](Gl_api::CallbackFunction){};
ms_glApi.addDeviceRestoredCallback = [](Gl_api::CallbackFunction){};
ms_glApi.removeDeviceRestoredCallback = [](Gl_api::CallbackFunction){};
ms_glApi.wasDeviceReset            = []() { return false; };
```

**`requiresVertexAndPixelShaders` divergence** — D3D11 always returns true (no FFP):
```cpp
ms_glApi.requiresVertexAndPixelShaders = []() { return true; };
```

**`Direct3d11.h` shape** — mirror `Direct3d9.h:36-94`:
```cpp
class Direct3d9
{
public:
    static IDirect3D9 *         getDirect3d();
    static int                  getAdapter();
    static IDirect3DDevice9 *   getDevice();
    static int                  getShaderCapability();
    static int                  getVideoMemoryInMegabytes();
    static bool                 supportsPixelShaders();
    // ...
    static bool                 install(Gl_install * gl_install);
    static bool                 drawPrimitive();
    static void                 drawPrimitive(D3DPRIMITIVETYPE primitiveType, int startVertex, int primitiveCount);
};
```
**D3D11 mirror:** `class Direct3d11 { ... }` with `getDevice()` returning `ID3D11Device *`, `getContext()` returning `ID3D11DeviceContext *`; `supportsPixelShaders()`/`supportsVertexShaders()` always return true; drop `getMaxAnisotropy` enumeration code path (query DXGI instead).

---

### `ConfigDirect3d11.{h,cpp}`

**Analog:** `Direct3d9/src/win32/ConfigDirect3d9.h` (61 ln) + `.cpp` (229 ln)

**Class shape** — `ConfigDirect3d9.h:14-57`:
```cpp
class ConfigDirect3d9
{
public:
    enum VertexProcessingMode { VPM_default, VPM_hardware, VPM_software };
    static void install();
    static int  getAdapter();
    static bool getUseReferenceRasterizer();
    static bool getDisableVertexAndPixelShaders();
    static int  getShaderCapabilityOverride();
    static bool getAllowTearing();
    static int  getDynamicVertexBufferSize();
    static int  getDynamicIndexBufferSize();
    static int  getMaxVertexShaderVersion();
    static int  getMaxPixelShaderVersion();
    static bool getDiscardDynamicBuffersAtBeginningOfFrame();
    static VertexProcessingMode getVertexProcessingMode();
    static bool getAntiAlias();
};
```
**D3D11 mirror — keep:** `install`, `getAdapter`, `getDynamicVertexBufferSize`, `getDynamicIndexBufferSize`, `getDiscardDynamicBuffersAtBeginningOfFrame`, `getAntiAlias`. **Drop:** `getUseReferenceRasterizer` (D3D11 has WARP, name differently if needed), `getMaxVertexShaderVersion`/`getMaxPixelShaderVersion` (always SM5.0), `VertexProcessingMode` (D3D11 has no software vertex processing), `getDisableVertexAndPixelShaders` (no FFP fallback). **Add:** `getShaderCacheDir()` (D-03 location; default = `<stage>/shader-cache/`), `getEnableDebugLayer()` (D-08 fallback), `getPreferredAdapterIndex()`.

---

### `FirstDirect3d11.h`

**Analog:** `FirstDirect3d9.h` (18 ln, complete):
```cpp
#ifndef INCLUDED_FirstDirect3d9_H
#define INCLUDED_FirstDirect3d9_H

#define COMPILE_DLL 1
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/FirstSharedFoundation.h"

#endif
```
**D3D11 mirror:** identical shape; rename guard `INCLUDED_FirstDirect3d11_H`; identical relative include path (the new tree sits at the same depth: `Direct3d11/src/win32/`).

---

### `Direct3d11_Device.{h,cpp}`

**Analog:** the device-creation block inlined inside `Direct3d9.cpp:952-…` `Direct3d9::install`. RESEARCH §Recommended Project Structure promotes this to a sibling file in D3D11 (cleaner separation; cleaner D-02 protection — device init failures stay testable in isolation).

**Install path entry** — `Direct3d9.cpp:952-983`:
```cpp
bool Direct3d9::install(Gl_install *gl_install)
{
    DEBUG_FATAL(sizeof(PaddedVector) != sizeof(float) * 4, ("PaddedVector size bad"));
    NOT_NULL(gl_install);
    DEBUG_FATAL(ms_installed, ("already installed"));

    ConfigDirect3d9::install();
    ms_performanceTimer = new PerformanceTimer();
    ms_installed = true;
    ms_currentTime = 0.0f;

    // store the screen dimensions
    ms_width               = gl_install->width;
    ms_height              = gl_install->height;
    ms_windowed            = gl_install->windowed;
    ms_window              = gl_install->window;
    ms_engineOwnsWindow    = gl_install->engineOwnsWindow;
    ms_borderlessWindow    = gl_install->borderlessWindow;
    ms_windowX             = gl_install->windowX;
    ms_windowY             = gl_install->windowY;
    ms_windowedModeChanged = gl_install->windowedModeChanged;
    // ... then ms_glApi slot population (lines 985-1137)
}
```
**D3D11 mirror:** same `Direct3d11::install` shell; the device-creation step delegates to `Direct3d11_Device::create(gl_install->window, gl_install->width, gl_install->height, gl_install->windowed)`. RESEARCH §Pattern 1 (lines 173-235) gives the full ComPtr-based create body to copy. Critical divergence: `D3D_FEATURE_LEVEL_11_0` baseline (SPEC §Constraints), `DXGI_SWAP_EFFECT_FLIP_DISCARD` (RESEARCH Anti-Patterns), debug-layer detect-and-fallback (Pitfall 8).

**Lost-device callback registration (D3D9 only — DROP entirely)** — `Direct3d9.cpp:995-998`:
```cpp
ms_glApi.addDeviceLostCallback             = addDeviceLostCallback;
ms_glApi.removeDeviceLostCallback          = removeDeviceLostCallback;
ms_glApi.addDeviceRestoredCallback         = addDeviceRestoredCallback;
ms_glApi.removeDeviceRestoredCallback      = removeDeviceRestoredCallback;
```
D3D11 wires these to no-op lambdas (see `Direct3d11.cpp` excerpt above). Per SPEC R2 acceptance: zero `OnLostDevice` / `OnResetDevice` matches in `Direct3d11/` source.

---

### `Direct3d11_StateCache.{h,cpp}`

**Analog:** `Direct3d9_StateCache.cpp` (573 ln). Three regions to mirror, three to drop:

**KEEP — render-state defaults init** — `Direct3d9_StateCache.cpp:140-154`:
```cpp
RS(D3DRS_BLENDFACTOR,                       static_cast<DWORD>(0xffffffff));
RS(D3DRS_SRGBWRITEENABLE,                   0);
RS(D3DRS_DEPTHBIAS,                         0.0f);
RS(D3DRS_SEPARATEALPHABLENDENABLE,          FALSE);
RS(D3DRS_SRCBLENDALPHA,                     D3DBLEND_ONE);
RS(D3DRS_DESTBLENDALPHA,                    D3DBLEND_ZERO);
RS(D3DRS_BLENDOPALPHA,                      D3DBLENDOP_ADD);
```
**D3D11 mirror:** populate `D3D11_RASTERIZER_DESC`, `D3D11_BLEND_DESC`, `D3D11_DEPTH_STENCIL_DESC` defaults; create one immutable state object per unique combination (cache by struct hash), `RSSetState`/`OMSetBlendState`/`OMSetDepthStencilState` instead of per-key `SetRenderState`.

**KEEP — sampler defaults init** — `Direct3d9_StateCache.cpp:180-197`:
```cpp
for (int i = 0; i < cms_samplers; ++i)
{
    SSS(D3DSAMP_ADDRESSU,             D3DTADDRESS_WRAP);
    SSS(D3DSAMP_ADDRESSV,             D3DTADDRESS_WRAP);
    SSS(D3DSAMP_MAGFILTER,            D3DTEXF_POINT);
    SSS(D3DSAMP_MINFILTER,            D3DTEXF_POINT);
    SSS(D3DSAMP_MIPFILTER,            D3DTEXF_NONE);
    SSS(D3DSAMP_MAXANISOTROPY,        1);
    SSS(D3DSAMP_SRGBTEXTURE,          0);
}
```
**D3D11 mirror:** populate `D3D11_SAMPLER_DESC`, create one `ID3D11SamplerState` per unique combination, bind via `PSSetSamplers`. Critical (Pitfall 4): SRV cache and Sampler cache are **independent** — both must be tracked and bound.

**DROP — D3DTSS_* setters (FFP combiners — D-04 spike target)** — `Direct3d9_StateCache.cpp:159-177`:
```cpp
TSS(D3DTSS_COLOROP,               D3DTOP_DISABLE);
TSS(D3DTSS_COLORARG1,             D3DTA_TEXTURE);
TSS(D3DTSS_COLORARG2,             D3DTA_TEXTURE);
TSS(D3DTSS_ALPHAOP,               D3DTOP_DISABLE);
TSS(D3DTSS_ALPHAARG1,             D3DTA_TEXTURE);
TSS(D3DTSS_ALPHAARG2,             D3DTA_TEXTURE);
TSS(D3DTSS_BUMPENVMAT00,          0.0f);
// ... 12 more D3DTSS_* lines
```
**D3D11:** drop entirely. If D-04 spike Phase B finds runtime FFP usage, `Direct3d11_FfpGenerator` synthesizes equivalent `ps_5_0` shaders; if not, descope. The drop is the SPEC R2 zero-match acceptance criterion.

**DROP — VS/PS constant register setters (cbuffer migration target)** — `Direct3d9_StateCache.cpp:520-569`:
```cpp
void Direct3d9_StateCache::setVertexShaderConstants(int index, const void *data, int numberOfConstants)
{
    if (Direct3d9::supportsVertexShaders())
    {
        HRESULT result = ms_device->SetVertexShaderConstantF(index, reinterpret_cast<const float *>(data), numberOfConstants);
        FATAL_DX_HR("SetVertexShaderConstantF failed %s", result);
    }
}
// ...similar pattern at lines 542-549 (BOOL variant) and 557-569 (PS constants)
void Direct3d9_StateCache::setPixelShaderConstants(int index, const void *data, int numberOfConstants)
{
    if (Direct3d9::supportsPixelShaders())
    {
        HRESULT result = ms_device->SetPixelShaderConstantF(index, reinterpret_cast<const float *>(data), numberOfConstants);
        FATAL_DX_HR("SetPixelShaderConstant failed %s", result);
    }
}
```
**D3D11:** these become `Direct3d11_ConstantBuffer::update(slot, data, count)` calls, backed by `ID3D11Buffer` + `Map`/`memcpy`/`Unmap` + `VSSetConstantBuffers`/`PSSetConstantBuffers`. RESEARCH §Pattern 2 (lines 236-269) gives the cbuffer-update body. **Pitfall 2 alignment** is the silent-bug landmine.

**Restore-state pattern (KEEP shape, drop FFP entries)** — `Direct3d9_StateCache.cpp:382-385`:
```cpp
TSS(D3DTSS_COLOROP);
TSS(D3DTSS_COLORARG1);
TSS(D3DTSS_COLORARG2);
TSS(D3DTSS_ALPHAOP);
```
**D3D11:** drop the `TSS()` lines; keep the surrounding RS/SSS restore loops.

---

### `Direct3d11_TextureData.{h,cpp}`

**Analog:** `Direct3d9_TextureData.cpp` (735 ln)

**Includes + format table** — `Direct3d9_TextureData.cpp:1-54`:
```cpp
#include "FirstDirect3d9.h"
#include "Direct3d9_TextureData.h"
#include "Direct3d9.h"
#include "Direct3d9_Metrics.h"
#include "Direct3d9_StateCache.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/MemoryBlockManager.h"
#include "sharedFoundation/Os.h"
#include "clientGraphics/TextureFormatInfo.h"

#include <d3dx9tex.h>
#include <map>

const Tag TAG_ENVM = TAG(E,N,V,M);

Direct3d9_TextureData::PixelFormatInfo     Direct3d9_TextureData::ms_pixelFormatInfoArray[TF_Count];
MemoryBlockManager                        *Direct3d9_TextureData::ms_memoryBlockManager;
Direct3d9_TextureData::GlobalTextureList  *Direct3d9_TextureData::ms_globalTextureList;

static const D3DFORMAT translationTable[] =
{
    D3DFMT_A8R8G8B8,      // TF_ARGB_8888,
    D3DFMT_A4R4G4B4,      // TF_ARGB_4444,
    D3DFMT_A1R5G5B5,      // TF_ARGB_1555,
    D3DFMT_X8R8G8B8,      // TF_XRGB_8888,
    D3DFMT_R8G8B8,        // TF_RGB_888,
    D3DFMT_DXT1,          // TF_DXT1,
    D3DFMT_DXT3,          // TF_DXT3,
    D3DFMT_DXT5,          // TF_DXT5,
    D3DFMT_A8,            // TF_A_8,
    D3DFMT_L8,            // TF_L_8
    D3DFMT_A16B16G16R16F, // TF_ABGR_16F
    D3DFMT_A32B32G32R32F, // TF_ABGR_32F
    D3DFMT_UNKNOWN,       // TF_Count
    D3DFMT_UNKNOWN        // TF_Native
};
```
**D3D11 mirror:** identical structure; replace `<d3dx9tex.h>` with `<d3d11.h>`; replace `D3DFORMAT translationTable[]` with `DXGI_FORMAT translationTable[]` (e.g. `DXGI_FORMAT_B8G8R8A8_UNORM` for `TF_ARGB_8888`, `DXGI_FORMAT_BC1_UNORM` for `TF_DXT1`, `DXGI_FORMAT_BC3_UNORM` for `TF_DXT5`, `DXGI_FORMAT_R16G16B16A16_FLOAT` for `TF_ABGR_16F`). The `TF_*` enum is contract — engine-side; do not touch.

**Resource model divergence:** D3D9 pattern is single `IDirect3DTexture9*`. D3D11 needs the `ID3D11Texture2D` (storage) + `ID3D11ShaderResourceView` (bind) pair, owned by `ComPtr<>`. No `D3DPOOL_MANAGED` (use `D3D11_USAGE_IMMUTABLE` for static, `D3D11_USAGE_DYNAMIC` + `Map` for dynamic).

**KEEP:** the `MemoryBlockManager` allocation pattern (codebase-wide convention), `ExitChain` registration, `GlobalTextureList` global-texture lookup table.

---

### `Direct3d11_StaticVertexBufferData.{h,cpp}`

**Analog:** `Direct3d9_StaticVertexBufferData.cpp` (156 ln)

**MemoryBlockManager + new/delete pattern (KEEP verbatim, rename type)** — `Direct3d9_StaticVertexBufferData.cpp:23-53`:
```cpp
void Direct3d9_StaticVertexBufferData::install()
{
    ms_memoryBlockManager = new MemoryBlockManager("Direct3d9_StaticVertexBufferData", true, sizeof(Direct3d9_StaticVertexBufferData), 0, 0, 0);
}

void Direct3d9_StaticVertexBufferData::remove()
{
    delete ms_memoryBlockManager;
    ms_memoryBlockManager = NULL;
}

void *Direct3d9_StaticVertexBufferData::operator new(size_t size)
{
    UNREF(size);
    NOT_NULL(ms_memoryBlockManager);
    DEBUG_FATAL(size != sizeof(Direct3d9_StaticVertexBufferData), ("wrong new called"));
    return ms_memoryBlockManager->allocate();
}

void Direct3d9_StaticVertexBufferData::operator delete(void *memory)
{
    NOT_NULL(ms_memoryBlockManager);
    ms_memoryBlockManager->free(memory);
}
```
**D3D11 mirror:** identical shape, rename type to `Direct3d11_StaticVertexBufferData`. The `MemoryBlockManager` pattern is codebase-wide (Phase 9 STL port did not displace it); preserve it.

**Resource creation divergence:** D3D9 calls `IDirect3DDevice9::CreateVertexBuffer(usage=0, pool=D3DPOOL_MANAGED)`; D3D11 calls:
```cpp
D3D11_BUFFER_DESC bd = {};
bd.ByteWidth      = sizeBytes;
bd.Usage          = D3D11_USAGE_IMMUTABLE;
bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
bd.CPUAccessFlags = 0;
D3D11_SUBRESOURCE_DATA srd = { initialData, 0, 0 };
device->CreateBuffer(&bd, &srd, &m_d3dBuffer);
```

---

### `Direct3d11_DynamicVertexBufferData.{h,cpp}`

**Analog:** `Direct3d9_DynamicVertexBufferData.cpp` (280 ln)

**Static state — ring-buffer accounting (KEEP shape verbatim)** — `Direct3d9_DynamicVertexBufferData.cpp:22-32`:
```cpp
bool                     Direct3d9_DynamicVertexBufferData::ms_newFrame;
int                      Direct3d9_DynamicVertexBufferData::ms_size;
int                      Direct3d9_DynamicVertexBufferData::ms_used;
IDirect3DVertexBuffer9 * Direct3d9_DynamicVertexBufferData::ms_d3dVertexBuffer;
MemoryBlockManager *     Direct3d9_DynamicVertexBufferData::ms_memoryBlockManager;
int                      Direct3d9_DynamicVertexBufferData::ms_locksSinceBeginFrame;
int                      Direct3d9_DynamicVertexBufferData::ms_discardsSinceBeginFrame;
int                      Direct3d9_DynamicVertexBufferData::ms_locksSinceResourceCreation;
int                      Direct3d9_DynamicVertexBufferData::ms_discardsSinceResourceCreation;
```

**Install — videomem-tiered ring sizing (KEEP)** — `Direct3d9_DynamicVertexBufferData.cpp:36-65`:
```cpp
void Direct3d9_DynamicVertexBufferData::install()
{
    ms_memoryBlockManager = new MemoryBlockManager("Direct3d9_DynamicVertexBufferData", ...);
    bool ffp = !Direct3d9::supportsVertexShaders();
    if (ffp) { ms_size = 256*1024; }
    else
    {
        int videoMemory = Direct3d9::getVideoMemoryInMegabytes();
        if (videoMemory <= 16)      ms_size = 256*1024;
        else if (videoMemory <= 32) ms_size = 512*1024;
        else if (videoMemory <= 64) ms_size = 1024*1024;
        // ...
    }
}
```
**D3D11 mirror:** drop the `ffp` branch (always shader path); keep the videomem tiering (query DXGI `IDXGIAdapter::GetDesc` for `DedicatedVideoMemory`); rename `IDirect3DVertexBuffer9 *` → `ComPtr<ID3D11Buffer>`.

**Lock/Unlock divergence:** D3D9 `Lock(D3DLOCK_DISCARD/D3DLOCK_NOOVERWRITE)` → D3D11 `Map(D3D11_MAP_WRITE_DISCARD/D3D11_MAP_WRITE_NO_OVERWRITE)` per RESEARCH Pitfall 5 (lines 447-468) — full create + map excerpt provided there.

---

### `Direct3d11_StaticIndexBufferData.{h,cpp}` + `Direct3d11_DynamicIndexBufferData.{h,cpp}`

**Analogs:** `Direct3d9_StaticIndexBufferData.cpp` (141 ln) + `Direct3d9_DynamicIndexBufferData.cpp` (196 ln)

**Pattern:** structurally identical to the VB pair above; substitute `D3D11_BIND_INDEX_BUFFER`. Index format (`DXGI_FORMAT_R16_UINT` or `R32_UINT`) is supplied at `IASetIndexBuffer` time, not at create time — small but real divergence from D3D9's index-format-on-create.

---

### `Direct3d11_VertexShaderData.{h,cpp}`

**Analog:** `Direct3d9_VertexShaderData.cpp` (573 ln) — especially the runtime-compile site.

**Includes + namespace skeleton** — `Direct3d9_VertexShaderData.cpp:1-65`:
```cpp
#include "FirstDirect3d9.h"
#include "Direct3d9_VertexShaderData.h"
#ifdef VSPS

#include "ConfigDirect3d9.h"
#include "Direct3d9.h"
#include "Direct3d9_PixelShaderConstantRegisters.h"
#include "Direct3d9_VertexShaderConstantRegisters.h"
#include "Direct3d9_VertexShaderVertexRegisters.h"
#include "clientGraphics/ShaderCapability.h"
#include "fileInterface/AbstractFile.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/PersistentCrcString.h"

#include <map>
#include <vector>
#include <d3dx9.h>
#include <d3dx9shader.h>

namespace Direct3d9_VertexShaderDataNamespace
{
    struct Include { ... };
    class IncludeHandler : public ID3DXInclude
    {
        virtual HRESULT STDMETHODCALLTYPE Open(D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
        virtual HRESULT STDMETHODCALLTYPE Close(LPCVOID pData);
    };
    typedef std::vector<D3DXMACRO>  Defines;
};
```
**D3D11 mirror:** drop `#ifdef VSPS`; replace `<d3dx9.h>` + `<d3dx9shader.h>` with `<d3dcompiler.h>` + `<wrl/client.h>`; replace `ID3DXInclude` with `ID3DInclude`; replace `D3DXMACRO` with `D3D_SHADER_MACRO`; drop the three `*ConstantRegisters.h` / `*VertexRegisters.h` includes (cbuffer model has no register-file headers).

**Compile site (the heart of D-03)** — `Direct3d9_VertexShaderData.cpp:432-448`:
```cpp
IncludeHandler includeHandler;
ID3DXBuffer *error = NULL;
HRESULT result = D3DXCompileShader(m_compileText, m_compileTextLength, &(ms_defines.front()), &includeHandler, "main", target, 0, &compiledShader, &error, NULL);

// DBE - I was getting strange Float Invalid Operation Exceptions (0xC0000090) in the
// Debug Build on floating point instructions which I eventually traced back to the FPU
// being left in some bad state after a call to D3DXCompileShader (on a certain .vsh).
// I also found that calling '_clearfp()' cleared up the problem.
_clearfp();

FATAL(FAILED(result), ("Could not compile shader %s %d (%s)", m_vertexShader->m_fileName.getString(), HRESULT_CODE(result), error ? error->GetBufferPointer() : "none"));
if (error)
{
    DEBUG_REPORT_LOG_PRINT(true, ("%s", error->GetBufferPointer()));
    error->Release();
}
```
**D3D11 mirror:** `D3DCompile(m_compileText, m_compileTextLength, sourceName, definesArray, &includeHandler, "main", "vs_5_0", flags, 0, &compiledBlob, &errorBlob)` — matching shape, modern entry point, profile string `vs_5_0`. **Drop `_clearfp()`** — the D3DX FPU bug doesn't apply to `D3DCompile` (verify post-port if any debug FPU exceptions surface). FATAL/DEBUG_REPORT shape unchanged. Inject `POSITION→SV_POSITION` macro in `definesArray` (Pitfall 1).

**Wire to D-03 cache:** before calling `D3DCompile`, call `Direct3d11_ShaderCache::tryLoad(sourceHash, &cachedBlob)`; on hit skip compile and pass `cachedBlob` to `device->CreateVertexShader`. On miss, after `D3DCompile`, call `Direct3d11_ShaderCache::store(sourceHash, compiledBlob)` before `CreateVertexShader`.

---

### `Direct3d11_PixelShaderProgramData.{h,cpp}`

**Analog:** `Direct3d9_PixelShaderProgramData.cpp` (50 ln — small, mostly a thin wrapper)

**Pattern:** mirror compile path of `Direct3d11_VertexShaderData`; profile `ps_5_0`; `device->CreatePixelShader(blob)`; same shader-cache wiring.

---

### `Direct3d11_ConstantBuffer.{h,cpp}` (NEW — no D3D9 analog)

**Why new:** D3D9 stored constants in a numbered register file (`SetVertexShaderConstantF(index, data, count)`); D3D11 stores them in user-allocated `ID3D11Buffer` objects bound by slot.

**Replaces:** the two `Direct3d9_StateCache.cpp` call sites listed above (lines 526, 561).

**RESEARCH §Pattern 2 (lines 236-269)** gives the create + update + bind pattern in full. Critical: Pitfall 2 alignment — every cbuffer struct on the C++ side mirrors the HLSL `cbuffer` declaration exactly, with explicit `XMFLOAT4` padding to honor the 16-byte rule.

**Layout discipline:**
- One cbuffer per logical group (per-frame, per-object, per-material) — RESEARCH §Pattern 2 calls out the per-frame slot 0 / per-object slot 1 / per-material slot 2 convention.
- `static_assert(sizeof(MyCB) % 16 == 0)` on every struct.
- `XMFLOAT4X4` (column-major DirectXMath) not bare `float[16]` (Pitfall 2).

---

### `Direct3d11_ShaderCache.{h,cpp}` (NEW — D-03)

**Why new:** the D-03 hybrid compile-on-first-use + persistent disk cache flow. No D3D9 analog (D3D9 plugin compiles every shader every launch).

**API skeleton:**
```cpp
class Direct3d11_ShaderCache
{
public:
    static void install(char const *cacheDir);
    static void remove();
    static bool tryLoad(uint32 sourceHash, ID3DBlob **outBlob);   // hit: out is filled, returns true
    static void store(uint32 sourceHash, ID3DBlob *blob);         // write <hash>.cso
    static uint32 hashSource(char const *text, int length, D3D_SHADER_MACRO const *defines);
};
```

**Cache file format:** RESEARCH §Don't Hand-Roll line 388 — "the `.cso` blob is opaque & self-versioning to the runtime; just hash + dump + load. No format game." File path = `<cacheDir>/<sourceHash:8>.cso`. Defines participate in the hash.

**Cache directory:** Claude's-discretion per CONTEXT D-03; suggested `<stage>/shader-cache/`. Must not be checked into git (planner adds `.gitignore` line).

---

### `Direct3d11_FfpGenerator.{h,cpp}` (CONDITIONAL on D-04 spike)

**Analog (only if implementation greenlit):** `Direct3d9_ShaderImplementationData.cpp:138-164` — the D3DTSS_* per-stage state builder.

**FFP per-stage builder shape** — `Direct3d9_ShaderImplementationData.cpp:138-164`:
```cpp
void Direct3d9_ShaderImplementationData::Stage::construct(const ShaderImplementation::Pass::Stage &stage)
{
    const uint count = 9;
    m_textureStageState.reserve(count);

#define TSSM(tss, v, m) m_textureStageState.push_back(TextureStageState(tss, m[stage.v]))
#define TSSA(tss, v, m) m_textureStageState.push_back(TextureStageState(tss, m[stage.v] | (stage.v##Complement ? D3DTA_COMPLEMENT : 0)))
#define TSSC(tss, v, m) m_textureStageState.push_back(TextureStageState(tss, m[stage.v] | (stage.v##Complement ? D3DTA_COMPLEMENT : 0) | (stage.v##AlphaReplicate ? D3DTA_ALPHAREPLICATE : 0)))

    TSSM(D3DTSS_COLOROP,             m_colorOperation,                TextureOperation);
    TSSC(D3DTSS_COLORARG0,           m_colorArgument0,                TextureArgument);
    TSSC(D3DTSS_COLORARG1,           m_colorArgument1,                TextureArgument);
    TSSC(D3DTSS_COLORARG2,           m_colorArgument2,                TextureArgument);

    TSSM(D3DTSS_ALPHAOP,             m_alphaOperation,                TextureOperation);
    TSSA(D3DTSS_ALPHAARG0,           m_alphaArgument0,                TextureArgument);
    TSSA(D3DTSS_ALPHAARG1,           m_alphaArgument1,                TextureArgument);
    TSSA(D3DTSS_ALPHAARG2,           m_alphaArgument2,                TextureArgument);

    TSSM(D3DTSS_RESULTARG,           m_resultArgument,                TextureArgument);
}
```
**D3D11 generator:** if D-04a verdict = build, the generator reads the same `Stage` shape (it is engine-side data, contract-stable) and synthesizes a `ps_5_0` HLSL string for each unique `(m_colorOperation, m_alphaOperation, arg0/arg1/arg2)` tuple, then `D3DCompile`s it via `Direct3d11_ShaderCache`. Cover MODULATE + ADD + SELECTARG1 at minimum (SPEC R4 acceptance). RESEARCH §Architecture Diagram lines 128-139 sketches the generator flow.

**If D-04a verdict = descope:** this file does not exist; remove from plan responsibility set.

---

### `Direct3d11_RenderTarget.{h,cpp}`

**Analog:** `Direct3d9_RenderTarget.cpp` (341 ln)

**Namespace state shape** — `Direct3d9_RenderTarget.cpp:23-44`:
```cpp
namespace Direct3d9_RenderTargetNamespace
{
    int const              cms_bakedTextureMaxDimension = 512;

    bool                   ms_installed;
    bool                   ms_primaryTargetSet;

    IDirect3DSurface9     *ms_primarySurface;
    IDirect3DSurface9     *ms_primaryDepthStencilSurface;

    IDirect3DTexture9     *ms_systemMemoryTexture;
    IDirect3DSurface9     *ms_systemMemorySurface;

    IDirect3DTexture9     *ms_renderTargetTexture;
    IDirect3DSurface9     *ms_renderSurface;

    IDirect3DSurface9     *ms_userSurface;
}
```
**D3D11 mirror:** replace each `IDirect3DSurface9*` / `IDirect3DTexture9*` with `ComPtr<ID3D11Texture2D>` + paired `ComPtr<ID3D11RenderTargetView>` and (for sample-back paths) `ComPtr<ID3D11ShaderResourceView>`. The "render to baked-texture, then sample" flow needs the SRV; the "render to screen back-buffer" flow needs the RTV only. Drop the system-memory surface (D3D11 staging textures replace it).

---

### `Direct3d11_LightManager.{h,cpp}`

**Analog:** `Direct3d9_LightManager.cpp` (962 ln — large, but mostly engine-data-conversion)

**Includes + bootstrap** — `Direct3d9_LightManager.cpp:11-32`:
```cpp
#include "FirstDirect3d9.h"
#include "Direct3d9_LightManager.h"

#include "Direct3d9.h"
#include "Direct3d9_PixelShaderConstantRegisters.h"
#include "Direct3d9_StateCache.h"
#include "Direct3d9_VertexShaderConstantRegisters.h"
#include "clientGraphics/Light.h"
#include "clientGraphics/ShaderCapability.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/Profiler.h"
#include "sharedFoundation/Production.h"
#include "sharedMath/VectorRgba.h"

#include <limits>
#include <vector>
#include <algorithm>
```
**D3D11 mirror:** drop `*ConstantRegisters.h` includes; output flows through `Direct3d11_ConstantBuffer::update(perFrameSlot, &lightCB, sizeof(lightCB))` instead of `Direct3d9_StateCache::setVertexShaderConstants`. The light-list-iteration logic (Light::Type filtering, ambient accumulation, dot-product weighting) is engine-data-driven and unchanged.

---

### `Direct3d11_Metrics.{h,cpp}`

**Analog:** `Direct3d9_Metrics.cpp` (313 ln)

**Namespace state + counters** — `Direct3d9_Metrics.cpp:1-40`:
```cpp
#include "FirstDirect3d9.h"
#include "Direct3d9_Metrics.h"

#ifdef _DEBUG

#include "sharedFoundation/Clock.h"
#include "sharedDebug/DebugFlags.h"

namespace Direct3d9_MetricsNamespace
{
    bool  ms_debugReportDraw;
    bool  ms_debugReportDrawPerPass;
    bool  ms_debugReportTexture;
    bool  ms_debugReportVertexBuffer;
    bool  ms_debugReportIndexBuffer;
    bool  ms_debugReportTriangleRate;
    bool  ms_debugReportSetConstants;
}
using namespace Direct3d9_MetricsNamespace;

// draw statistics
int      Direct3d9_Metrics::vertices;
int      Direct3d9_Metrics::indices;
int      Direct3d9_Metrics::points;
int      Direct3d9_Metrics::lines;
```
**D3D11 mirror:** identical structure; drop FFP-related counters; add `ID3D11DeviceContext::Map`/`Unmap` count, cbuffer update count, `ID3D11InputLayout` cache hit/miss. Counter publishing flows through the same `DebugFlags::registerFlag` + `DEBUG_REPORT_LOG_PRINT` mechanism.

---

### `Direct3d11_VertexDeclarationMap.{h,cpp}`

**Analog:** `Direct3d9_VertexDeclarationMap.cpp` (253 ln)

**Cache shape** — `Direct3d9_VertexDeclarationMap.cpp:22-46`:
```cpp
namespace Direct3d9_VertexDeclarationMapNamespace
{
    class Key
    {
    public:
        Key(VertexBufferFormat const * const * vertexBufferFormat, int count);
        bool operator < (const Key &rhs) const;
    private:
        enum { MAX_VERTEX_BUFFERS = 2 };
        uint32 m_key[MAX_VERTEX_BUFFERS];
    };

    typedef std::map<Key, IDirect3DVertexDeclaration9 *> VertexDeclarationMap;

    VertexDeclarationMap *ms_vertexDeclarationMap;
    D3DVERTEXELEMENT9     ms_vertexElements[32];
    D3DVERTEXELEMENT9     ms_vertexElementsEnd = D3DDECL_END();
}
```
**D3D11 mirror — KEY DIVERGENCE (Pitfall 6):** the cache **must** key on `(VertexBufferFormat, vertex shader bytecode SHA1)` — `ID3D11InputLayout` is created against compiled VS bytecode and bound to that signature. D3D9 keyed on VB format alone; D3D11 cannot. Restructure `Key` to include a 20-byte SHA1 (or 8-byte truncated hash), bump `MAX_VERTEX_BUFFERS` if multi-stream is exercised, replace `IDirect3DVertexDeclaration9*` with `ComPtr<ID3D11InputLayout>`, replace `D3DVERTEXELEMENT9[]` with `D3D11_INPUT_ELEMENT_DESC[]`.

---

### `Direct3d11_VertexBufferDescriptorMap.{h,cpp}`

**Analog:** `Direct3d9_VertexBufferDescriptorMap.cpp` (167 ln)

**Pattern:** the descriptor structure tells the input-layout builder how to emit `D3D11_INPUT_ELEMENT_DESC[]` from `VertexBufferFormat`. Engine-side `VertexBufferFormat` is contract; only the D3D11-side conversion changes (`D3DDECLTYPE_FLOAT3` → `DXGI_FORMAT_R32G32B32_FLOAT`, etc.).

---

### `Direct3d11.vcxproj`

**Analog:** `Direct3d9/build/win32/Direct3d9.vcxproj` (314 ln)

Three configurations to mirror (Debug | Optimized | Release) — the entire vcxproj structure is the analog. Key block-by-block divergences:

**ProjectGuid** — generate a fresh GUID for `Direct3d11.vcxproj`; do NOT reuse `{E89A79A9-8488-4D42-BF88-B9833F5607C0}`.

**ConfigurationType + PlatformToolset (KEEP verbatim)** — vcxproj lines 22-39:
```xml
<ConfigurationType>DynamicLibrary</ConfigurationType>
<PlatformToolset>v145</PlatformToolset>
<UseOfMfc>false</UseOfMfc>
<CharacterSet>MultiByte</CharacterSet>
```
Per CONTEXT canonical-refs: `v145` toolset is the v2 standard, `WindowsTargetPlatformVersion=10.0` already provides D3D11 SDK headers (RESEARCH §Standard Stack lines 65-72).

**OutDir / IntDir** — vcxproj lines 60-77:
```xml
<OutDir>.\..\..\..\..\..\..\compile\win32\Direct3d9\Debug\</OutDir>
<IntDir>.\..\..\..\..\..\..\compile\win32\Direct3d9\Debug\</IntDir>
```
**D3D11:** `compile\win32\Direct3d11\Debug\` (etc.). Replace every `Direct3d9` segment with `Direct3d11`.

**AdditionalIncludeDirectories (DROP `external\3rd\library\directx9\include`; add nothing — Win SDK handles d3d11)** — vcxproj line 90:
```xml
<AdditionalIncludeDirectories>..\..\..\..\..\..\external\ours\library\fileInterface\include\public;..\..\..\..\..\..\external\3rd\library\directx9\include;..\..\..\..\..\..\external\3rd\library\libjpeg\include;..\..\..\..\..\client\library\clientGraphics\include\public;..\..\..\..\..\shared\library\sharedDebug\include\public;..\..\..\..\..\shared\library\sharedFile\include\public;..\..\..\..\..\shared\library\sharedFoundation\include\public;..\..\..\..\..\shared\library\sharedFoundationTypes\include\public;..\..\..\..\..\shared\library\sharedMath\include\public;..\..\..\..\..\shared\library\sharedMemoryManager\include\public;..\..\..\..\..\shared\library\sharedObject\include\public;..\..\..\..\..\shared\library\sharedSynchronization\include\public;..\..\src\shared;..\..\src\win32;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
```
**D3D11:** drop the `external\3rd\library\directx9\include` segment (RESEARCH Standard-Stack confirms Win SDK is sufficient); keep all engine-side include paths verbatim.

**PreprocessorDefinitions** — vcxproj line 91:
```xml
<PreprocessorDefinitions>_DEBUG;DEBUG_LEVEL=2;WIN32;_WINDOWS;_USRDLL;DIRECT3D9_EXPORTS;FFP;VSPS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
```
**D3D11:** drop `DIRECT3D9_EXPORTS;FFP;VSPS;`, add `DIRECT3D11_EXPORTS;`. Result: `_DEBUG;DEBUG_LEVEL=2;WIN32;_WINDOWS;_USRDLL;DIRECT3D11_EXPORTS;%(PreprocessorDefinitions)`.

**RuntimeLibrary (KEEP per-config)** — vcxproj line 94 / 152 / 211:
- Debug: `MultiThreadedDebug`
- Release: `MultiThreaded`
- Optimized: `MultiThreadedDebug`

**LanguageStandard (KEEP)** — vcxproj line 108:
```xml
<LanguageStandard>stdcpp20</LanguageStandard>
```
Per Phase 9 / Koogie's modernization. C++20 is the v2 standard.

**PrecompiledHeader (KEEP shape, rename file)** — vcxproj lines 97-99:
```xml
<PrecompiledHeader>Use</PrecompiledHeader>
<PrecompiledHeaderFile>FirstDirect3d9.h</PrecompiledHeaderFile>
<PrecompiledHeaderOutputFile>.\..\..\..\..\..\..\compile\win32\Direct3d9\Debug/Direct3d9.pch</PrecompiledHeaderOutputFile>
```
**D3D11:** `FirstDirect3d11.h`, `Direct3d11.pch`.

**AdditionalDependencies (CRITICAL DELTA)** — vcxproj line 115:
```xml
<AdditionalDependencies>libjpeg.lib;odbc32.lib;odbccp32.lib;winmm.lib;delayimp.lib;dxguid.lib;d3d9.lib;d3dx9.lib;ddraw.lib;dxerr9.lib;legacy_stdio_definitions.lib;%(AdditionalDependencies)</AdditionalDependencies>
```
**D3D11:** drop `d3d9.lib;d3dx9.lib;ddraw.lib;dxerr9.lib`; add `d3d11.lib;d3dcompiler.lib;dxgi.lib`. Keep `dxguid.lib` (provides COM interface UUIDs — used by `__uuidof` / `IID_PPV_ARGS`). Keep `libjpeg.lib`, `odbc32.lib`, `odbccp32.lib`, `winmm.lib`, `delayimp.lib`, `legacy_stdio_definitions.lib` (RESEARCH §Standard-Stack Supporting table confirms). Result:
```xml
<AdditionalDependencies>libjpeg.lib;odbc32.lib;odbccp32.lib;winmm.lib;delayimp.lib;dxguid.lib;d3d11.lib;d3dcompiler.lib;dxgi.lib;legacy_stdio_definitions.lib;%(AdditionalDependencies)</AdditionalDependencies>
```

**OutputFile** — vcxproj line 116:
```xml
<OutputFile>..\..\..\..\..\..\compile\win32\Direct3d9\Debug\gl05_d.dll</OutputFile>
```
**D3D11:** `gl11_d.dll` (Debug), `gl11_r.dll` (Release), `gl11_o.dll` (Optimized) per CONTEXT D-02.

**AdditionalLibraryDirectories (DROP directx9 path)** — vcxproj line 118:
```xml
<AdditionalLibraryDirectories>..\..\..\..\..\..\external\3rd\library\directx9\lib;..\..\..\..\..\..\external\3rd\library\libjpeg\lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
```
**D3D11:** drop `external\3rd\library\directx9\lib`; keep `libjpeg\lib`. Win SDK lib path is on the linker default search list (no explicit entry needed).

**BaseAddress (CRITICAL — Pitfall 7)** — vcxproj line 123:
```xml
<BaseAddress>0x62A00000</BaseAddress>
```
**D3D11:** pick non-overlapping `0x60A00000` (well below the existing `gl05_*` at `0x62A00000`); cross-check via `dumpbin /headers` against every DLL in `D:/Code/swg-client-v2/stage/`.

**ImportLibrary + ProgramDatabaseFile** — vcxproj lines 122-124:
```xml
<ProgramDatabaseFile>.\..\..\..\..\..\..\compile\win32\Direct3d9\Debug/gl05_d.pdb</ProgramDatabaseFile>
<BaseAddress>0x62A00000</BaseAddress>
<ImportLibrary>.\..\..\..\..\..\..\compile\win32\Direct3d9\Debug/gl05_d.lib</ImportLibrary>
```
**D3D11:** `gl11_d.pdb`, `gl11_d.lib` (Debug); `gl11_r.pdb`, `gl11_r.lib` (Release); `gl11_o.pdb`, `gl11_o.lib` (Optimized).

**TargetMachine + AdditionalOptions (KEEP)** — vcxproj lines 125-126:
```xml
<TargetMachine>MachineX86</TargetMachine>
<AdditionalOptions>/SAFESEH:NO %(AdditionalOptions)</AdditionalOptions>
```
32-bit target stays (Phase 9 / Phase 11 SPEC §Constraints — no 64-bit migration).

**POST_BUILD copy step (CRITICAL)** — vcxproj lines 132-135 / 191-194 / 247-250:
```xml
<PostBuildEvent>
  <Message>Copy debug dll</Message>
  <Command>copy $(TargetPath) ..\..\..\..\..\..\..\dev\win32\gl05_d.dll</Command>
</PostBuildEvent>
```
**D3D11:** `copy $(TargetPath) ..\..\..\..\..\..\..\dev\win32\gl11_d.dll` (Debug), `gl11_r.dll` (Release), `gl11_o.dll` (Optimized).

> **Operational note from RUNTIME STATE INVENTORY (RESEARCH line 404):** the existing `gl05_*.dll` / `gl06_*.dll` / `gl07_*.dll` are present in `D:/Code/swg-client-v2/stage/`, NOT `dev/win32/`. The plan author MUST confirm the current stage location used by `SwgClient_d.exe` and use the same convention for `gl11_d.dll`. The vcxproj copy target above mirrors D3D9's literal — verify before committing.

**`<PreBuildEventUseInBuild>false</PreBuildEventUseInBuild>` and `<PostBuildEventUseInBuild>false</PostBuildEventUseInBuild>`** are Debug-only (line 63-64) — replicate as-is; the POST_BUILD `<Command>` still runs in this codebase per Koogie's existing toolchain (verify post-port via build-log inspection).

**ItemGroup → ClCompile + ClInclude** — vcxproj lines 252-305: list every new `Direct3d11/src/win32/*.cpp` and `*.h` plus the shared subset:
- `..\..\src\shared\MemoryManagerHook.cpp` — KEEP (the v2 tree's mem-hook pattern)
- `..\..\src\shared\SetupDll.cpp` — KEEP
- `..\..\src\shared\WriteTga.cpp` — KEEP if `screenShot()` keeps TGA fallback; libjpeg path is primary

**ProjectReference (KEEP shape)** — vcxproj lines 306-311:
```xml
<ProjectReference Include="..\..\..\DllExport\build\win32\DllExport.vcxproj">
  <Project>{78041480-c2c5-42a1-a566-c57bc3100bbf}</Project>
  <ReferenceOutputAssembly>false</ReferenceOutputAssembly>
</ProjectReference>
```
The `DllExport` reference + delay-load is required for the `GetApi` export contract; carry over verbatim.

**Add to swg.sln:** the new `Direct3d11.vcxproj` must be inserted into `D:/Code/swg-client-v2/src/build/win32/swg.sln` next to the three D3D9 vcxproj entries — solution-file mechanics are MSBuild-standard; the planner spawns a sln-edit task in the appropriate plan.

---

## §Engine-side surgical edits

These are minimal edits to the engine — NOT structural changes. The plan author makes a single-commit edit at each site.

### 1. `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp` — add `TAG_DX11`

**Site:** line 65-66 (current):
```cpp
const Tag TAG_DX8 = TAG3(D,X,8);
const Tag TAG_DX9 = TAG3(D,X,9);
```

**Edit (per RESEARCH §Code Examples lines 530-534):** add a sibling line:
```cpp
const Tag TAG_DX11 = TAG3(D,1,1);   // 'D','1','1' — three-char tag, fits TAG3 macro
```

> **Tag-string verification:** `TAG3(D,1,1)` produces a four-byte tag with the three printable chars + a leading null. RESEARCH selected `D,1,1` over `D,X,B` after Context7-style verification — see RESEARCH lines 533. If the planner prefers `TAG3(D,X,B)` for readability, it works equally well; pick one and stay consistent.

### 2. `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp` — extend range check

**Site:** lines 209-215 (current):
```cpp
if (ms_rasterMajor >= 5 && ms_rasterMajor <= 7)
{
    GraphicsOptionTags::set(TAG_DX8, false);
    GraphicsOptionTags::set(TAG_DX9, true);
}
else
    DEBUG_FATAL(true, ("unknown rasterizer"));
```

**Edit (per RESEARCH §Code Examples lines 544-558):**
```cpp
if (ms_rasterMajor >= 5 && ms_rasterMajor <= 7)
{
    GraphicsOptionTags::set(TAG_DX8,  false);
    GraphicsOptionTags::set(TAG_DX9,  true);
    GraphicsOptionTags::set(TAG_DX11, false);
}
else if (ms_rasterMajor == 11)
{
    GraphicsOptionTags::set(TAG_DX8,  false);
    GraphicsOptionTags::set(TAG_DX9,  false);
    GraphicsOptionTags::set(TAG_DX11, true);
}
else
    DEBUG_FATAL(true, ("unknown rasterizer (rasterMajor=%d; supported: 5-7, 11)", ms_rasterMajor));
```

**Cross-codebase audit result (RESEARCH line 561):** `[VERIFIED: grep "TAG_DX9" returns only two hits, both in Graphics.cpp itself (line 66 def, line 212 set)]`. No other consumers exist. `TAG_DX11` is strictly additive — no terrain/skeletal/particle file needs to learn about it for Phase 11 to function.

### 3. NO OTHER ENGINE EDITS

`Graphics.cpp:184` (`ms_rasterMajor` read), `:194-200` (`DLL_NAME_FORMAT` macro), `:219-220` (`sprintf("./gl%02d_d.dll", ...)`+`LoadLibrary`), `:235-238` (`GetProcAddress("GetApi")`+`getApi()`) are **untouched** by Phase 11. The selection plumbing already works for `rasterMajor=11` once the range check accepts it.

---

## §Critical call-site references

### Cbuffer migration targets (D-01 + RESEARCH §Pattern 2)

These two call sites in `Direct3d9_StateCache.cpp` are the D3D9 register-file setters that have **no D3D11 equivalent** — replace with `Direct3d11_ConstantBuffer::update(...)` calls in the new plugin (NOT engine-side edits — these are the D3D9 plugin's internal implementation).

- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp:526` — `SetVertexShaderConstantF`
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp:561` — `SetPixelShaderConstantF`

**D3D11 plugin:** the D3D11 equivalents live in `Direct3d11_ConstantBuffer.cpp` (new file). The engine-side call (`ms_api->setVertexShaderUserConstants(...)`) is unchanged; the D3D11 implementation routes the data into a `D3D11_BIND_CONSTANT_BUFFER` buffer via `Map`/`memcpy`/`Unmap` then `VSSetConstantBuffers`/`PSSetConstantBuffers`.

### FFP setter sites — D-04 spike Phase B instrumentation targets

Per CONTEXT D-04 spike Phase B (mirrors Phase 10 D-15 throwaway pattern):

- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp:159` — `TSS(D3DTSS_COLOROP, D3DTOP_DISABLE)` (init-time default)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp:162` — `TSS(D3DTSS_ALPHAOP, D3DTOP_DISABLE)` (init-time default)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp:382` — `TSS(D3DTSS_COLOROP)` (restore-state)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp:383` — `TSS(D3DTSS_ALPHAOP)` (restore-state)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_ShaderImplementationData.cpp:147` — `TSSM(D3DTSS_COLOROP, m_colorOperation, ...)` (per-stage build-time)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_ShaderImplementationData.cpp:152` — `TSSM(D3DTSS_ALPHAOP, m_alphaOperation, ...)` (per-stage build-time)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_ShaderImplementationData.cpp:382` — `setTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_DISABLE)` (cascade-terminate)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_ShaderImplementationData.cpp:383` — `setTextureStageState(stage, D3DTSS_ALPHAOP, D3DTOP_DISABLE)` (cascade-terminate)

**Spike Phase B instrumentation pattern:** at each site, add `// THROWAWAY D-04` marker + a CSV-line emitter logging `(frame_no, scene_tag, material_name, stage_idx, op_type, op_value)`. Single throwaway commit per CONTEXT D-04 (mirrors Phase 10 plan 10-07 which removed 726 lines across 12 files). Revert in one atomic commit after spike write-up lands.

### Shader-source loader (engine-side, plugin receives PRE-LOADED text)

- `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp:2155` — `AbstractFile *file = TreeFile::open(m_fileName.getString(), AbstractFile::PriorityData, false);`
- `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp:2309` — second loader site (vertex-shader path)

**Important:** these are **engine-side** `TreeFile` reads. The plugin (`Direct3d11_VertexShaderData::compile(...)`) receives a pre-loaded text blob via the engine → `createVertexShaderData` → constructor path; it does NOT call `TreeFile::open` itself. RESEARCH §Architecture Diagram lines 119-124 sketches this. The D-03 hybrid cache lives entirely on the plugin side; engine has no awareness of it.

---

## §Recovery anchor

Phase 10's `git tag phase-10-instrumentation-pre-cleanup` is `[VERIFIED: git tag --list returns "phase-10-instrumentation-pre-cleanup"]` at commit `9f2ec3715` per CONTEXT.

**When this matters for Phase 11:** the SPEC R7 / CONTEXT D-12 DPVS remeasurement is `external-tools-first` (PIX / Nsight / GPUView). If — only if — the external-tool capture hits friction *and* Kenny approves a fallback to in-engine timestamp queries, the planner can retrieve the deleted instrumentation files at this tag for D3D11-port reference.

```bash
# Read-only inspection (no checkout):
git show phase-10-instrumentation-pre-cleanup:src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp
git show phase-10-instrumentation-pre-cleanup:.planning/phases/10-dpvs-culling-experiment/10-05-PLAN.md
# (Inspection only — Phase 10 closed, do NOT cherry-pick those instrumentation files into Phase 11
#  without a SPEC §Constraints exception.)
```

The instrumentation files at this tag are reference material, not source-of-truth. The default Phase 11 path is `Direct3d11_Metrics.cpp` GPU-side counters + external-tool capture per RESEARCH §Pattern 4 (lines 329-369).

---

## §Shared patterns

### Shared 1: Includes / pre-compiled-header convention

**Source:** every `Direct3d9_*.cpp` opens with the same shape (`Direct3d9_TextureData.cpp:9-22`):
```cpp
#include "FirstDirect3d9.h"            // PCH first — always
#include "Direct3d9_TextureData.h"     // own header second

#include "Direct3d9.h"                 // sibling-plugin includes
#include "Direct3d9_Metrics.h"
#include "Direct3d9_StateCache.h"
#include "sharedFoundation/ExitChain.h" // engine includes by-component path
#include "sharedFoundation/MemoryBlockManager.h"
#include "clientGraphics/TextureFormatInfo.h"

#include <d3dx9tex.h>                  // 3rd-party include
#include <map>                          // C++ stdlib include last
```
**Apply to:** every new `Direct3d11_*.cpp`. Replace `FirstDirect3d9.h` → `FirstDirect3d11.h`; replace `Direct3d9_*.h` → `Direct3d11_*.h`; engine include paths unchanged; `<d3dx9*>` → `<d3d11.h>` + `<d3dcompiler.h>` + `<wrl/client.h>`.

### Shared 2: MemoryBlockManager + custom new/delete

**Source:** `Direct3d9_StaticVertexBufferData.cpp:23-53` (excerpted in §File-by-file mapping above). Every `Direct3d9_*Data.cpp` resource type follows this pattern.
**Apply to:** every `Direct3d11_*Data.{h,cpp}` — keep the install/remove/operator-new/operator-delete shape verbatim, rename type only.

### Shared 3: ms_-prefixed namespace-static state

**Source:** ubiquitous across `Direct3d9_*Namespace { ... }` blocks (e.g. `Direct3d9_RenderTarget.cpp:23-44`).
**Apply to:** every D3D11 plugin file — `namespace Direct3d11_FooNamespace { ... }` with `ms_*` static state. ComPtr-typed where it owns COM resources (Pitfall convention from D-01).

### Shared 4: FATAL_DX_HR macro replacement

**Source:** `Direct3d9.h:34`:
```cpp
#define FATAL_DX_HR(a,b)       FATAL(FAILED(b), (a, DXGetErrorString9(b)))
```
**D3D11 mirror:** `<dxerr9.h>` is gone. Roll a private helper:
```cpp
inline char const *hresultString(HRESULT hr) { static char buf[256]; FormatMessageA(...); return buf; }
#define FATAL_DX_HR(msg, hr) FATAL(FAILED(hr), (msg, hresultString(hr)))
```
**Apply to:** define in `Direct3d11.h` next to the namespace forward decls; consume from every `Direct3d11_*.cpp`.

### Shared 5: PIX event markers (KEEP — RESEARCH §Architecture Patterns lines 38-44)

**Source:** `Direct3d9.cpp:1123-1125`:
```cpp
ms_glApi.pixSetMarker = pixSetMarker;
ms_glApi.pixBeginEvent = pixBeginEvent;
ms_glApi.pixEndEvent = pixEndEvent;
```
**D3D11:** the Win-PIX runtime PIX-on-Windows API replaces D3D9 PIX. Use `<pix3.h>` (header-only from `WinPixEventRuntime` NuGet) **only if Kenny adds the optional dep**; otherwise stub the slots to no-ops. SPEC R7 DPVS remeasurement uses **external** PIX captures, so PIX markers in source are nice-to-have, not required.

---

## §No analog found

Three new files have no D3D9 sibling — the planner uses RESEARCH.md patterns directly:

| New file | Why no analog | RESEARCH section to use |
|----------|---------------|--------------------------|
| `Direct3d11_ConstantBuffer.{h,cpp}` | D3D9 used a register file, not buffers | §Pattern 2 (lines 236-269) |
| `Direct3d11_ShaderCache.{h,cpp}` | D3D9 plugin has no persistent shader cache (recompiles every launch) | §Pattern 3 (lines 270-328) + §Don't Hand-Roll line 388 |
| `Direct3d11_FfpGenerator.{h,cpp}` (CONDITIONAL) | D3D9 used hardware FFP combiners, not generated shaders | §Architecture Diagram lines 128-139; spike result determines build-or-descope |

---

## §Metadata

**Analog search scope:** `D:/Code/swg-client-v2/src/engine/client/application/Direct3d9/` (entire tree — 41 source files), `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/{win32,shared}/` (engine-side integration sites), `D:/Code/swg-client-v2/src/build/win32/` (sln integration target).
**Files inspected by Read:** 17 (Direct3d9 family) + 1 (Graphics.cpp) + 1 (Direct3d9.vcxproj) + 4 (CONTEXT/SPEC/RESEARCH partial reads).
**Files inspected by Grep:** TAG_DX9 cross-codebase (RESEARCH-cited result preserved); TreeFile::open in ShaderImplementation.cpp; Direct3d9.cpp slot-population scan.
**Pattern extraction date:** 2026-05-15.
**Phase 10 recovery anchor verified:** `git tag --list "phase-10*"` returns `phase-10-instrumentation-pre-cleanup`.

---

## PATTERN MAPPING COMPLETE
