# Phase 11: D3D11 Renderer Plugin — Research

**Researched:** 2026-05-15
**Domain:** Direct3D 9 → Direct3D 11 renderer-plugin port (Win32, MSBuild v145, C++20)
**Confidence:** HIGH

## Summary

Phase 11 ships a parallel `Direct3d11.dll` plugin that satisfies the same engine-side `Gl_api` function-pointer contract that `Direct3d9.dll` satisfies today. The work is wholly additive on top of Koogie's modernized v2 tree (`D:/Code/swg-client-v2/`, branch `koogie-msvc-cpp20-base`, MSBuild via `swg.sln` with `/p:PlatformToolset=v145 /p:Configuration=Debug /p:Platform=Win32` — NOT CMake). All required environmental pieces are already in place: Win SDK 10.0.26100.0 ships `d3d11.h` / `d3dcompiler.h` / `d3d11.lib` / `d3dcompiler.lib`; both 32-bit `d3dcompiler_47.dll` (System32 view of WoW64) and 64-bit are on disk. There is no in-tree `swg-engine-prototype`, no D3D11 reference; the SOE repo on disk and the public community-disclosed `swgemu/engine3` repository do not contain a D3D11 client renderer port.

The Gl_api contract in this codebase is **120 function-pointer slots** declared in `Gl_dll.def` (the SPEC's "119" is close — the count varies by ±1-2 depending on whether you count the typedef'd `CallbackFunction` and the `PRODUCTION==0`-gated video-capture entries, and the existing `Direct3d9.cpp` populates **118** of those slots in its `GetApi()`/`install()` install path). The plugin contract is dead simple: **one exported `extern "C" __declspec(dllexport) Gl_api const * GetApi()`**, which `clientGraphics/Graphics.cpp` resolves at line 236 after `LoadLibrary("./gl%02d_d.dll", rasterMajor)` at line 220. Renderer selection is already a single integer config key (`rasterMajor`); per CONTEXT D-02, Phase 11 extends the range check at `Graphics.cpp:209-215` to also accept `11` and adds a `TAG_DX11` sibling to `TAG_DX9` (`TAG_DX9` is local to `Graphics.cpp:66` only — no cross-codebase audit required). Output DLL is `gl11_d.dll` (Debug), `gl11_r.dll` (Release), `gl11_o.dll` (Optimized) per the existing `DLL_NAME_FORMAT` macro at `Graphics.cpp:194-200`; staging is via vcxproj POST_BUILD copy to `..\..\..\..\..\..\..\dev\win32\` (NOT `build/bin/Debug/` as the SPEC's wording implied — the actual D3D9 output paths show `compile/win32/Direct3d9/Debug/gl05_d.dll` then post-build copies to a `dev/win32/` staging dir).

The hard work is **replacing the D3D9 idiom set wholesale** per CONTEXT D-01 (clean rewrite, D3D9 as design reference only): explicit `ID3D11Device` + `ID3D11DeviceContext` lifetime via `Microsoft::WRL::ComPtr`, `IDXGISwapChain1` flip-model swap chain (`DXGI_SWAP_EFFECT_FLIP_DISCARD`, ≥2 back buffers), constant buffers replacing `SetVertexShaderConstantF` / `SetPixelShaderConstantF` (the only two register-setter sites in the entire D3D9 plugin: `Direct3d9_StateCache.cpp:526,561`), `D3D11_QUERY_TIMESTAMP` + `D3D11_QUERY_TIMESTAMP_DISJOINT` for the SPEC R7 DPVS remeasurement, sampler-state vs. texture-state separation, and a shader-compile pipeline using `D3DCompile` (from `d3dcompiler.lib` / `d3dcompiler_47.dll`) targeting `vs_5_0` / `ps_5_0` per CONTEXT D-03 (hybrid: compile on first use, persist `.cso` blobs to a per-shader cache, recompile on source-hash change). HLSL semantic migration is bounded — `Direct3d9_VertexShaderData.cpp:351-353` is the *only* shader-target string site (`vs_2_0` / `vs_1_1` selected by `getShaderCapability()`); shader source is loaded as `.vsh` / `.psh` files from the TRE asset archives via `TreeFile::open` at `ShaderImplementation.cpp:2155`. `SV_POSITION` migration applies inside the shader text (or a wrapper macro injected before `D3DCompile`) — this is where most of the per-shader correctness work lands.

The FFP question (D3D11-04) is genuinely open: D3D9 `ShaderImplementationPassFixedFunctionPipeline` is a sibling field on every Pass alongside `m_vertexShader` / `m_pixelShader` (`ShaderImplementation.h:262`), and the FFP code path uses `D3DTSS_*` setters at five concrete sites (`Direct3d9_StateCache.cpp:159,162,382,383` and `Direct3d9_ShaderImplementationData.cpp:147,152,382,383`). Per CONTEXT D-04 the first Phase 11 plan is a two-phase spike (static analysis of asset-side shader templates + optional runtime instrumentation) that decides whether the runtime HLSL pixel-shader generator gets built or descoped. The runtime-instrumentation pattern follows the THROWAWAY shape proven in Phase 10 plan 10-07 (single revert-shaped commit, `// THROWAWAY D-04` comment markers).

The DPVS remeasurement (SPEC R7) has a clean recovery path that wasn't called out in the SPEC: Phase 10 closeout left a **git tag `phase-10-instrumentation-pre-cleanup` at commit `9f2ec3715`** (per `10-SUMMARY.md`). Phase 11 has three escalating options for re-instrumentation: (a) `git revert <D-15-cleanup-commit>` to restore the entire D3D9 instrumentation harness (then port the `Direct3d9.cpp:4621-4709` query-pool to D3D11 `ID3D11Query`), (b) cherry-pick only the engine-side scaffolding (CSV writer + run-label + F10/F11 keybind + `/setrunlabel`) and write the D3D11 query-pool fresh, (c) skip restored instrumentation entirely and use external tools (PIX is NOT installed on this machine; Nsight Graphics is the natural NVIDIA-friendly D3D11 default given the RTX 3060). The SPEC and CONTEXT both lean toward external tools to avoid re-instrumentation work; option (c) is the recommendation, with PIX-on-Windows installed alongside Phase 11 for the capture session.

**Primary recommendation:** Build the plugin in this wave order — (1) FFP spike per D-04, (2) plugin scaffold + Gl_api stub table + `gl11_d.dll` produced + range-check edit at `Graphics.cpp:209-215` + boots-to-FATAL-on-first-call, (3) D3D11 device + DXGI flip-model swap chain + clear-to-color minimum viable boot (proves DLL contract), (4) resource layer (textures, vertex/index buffers, render targets), (5) shader layer (D3DCompile + cache + SV_POSITION wrapper macro injection), (6) state cache + draw-call wiring, (7) subsystem coverage (terrain → skeletal → particles → HUD), (8) DPVS external-tool capture + verdict, (9) visual-parity screenshots + `comparison-notes.md`. D3D9 plugin stays buildable end-to-end (D-05); per-plan inner-loop gate is a `Direct3d9.vcxproj` build, per-wave gate is a `SwgClient_d.exe` boot under `rasterMajor=5`.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Renderer DLL selection | Engine (`clientGraphics/Graphics.cpp`) | Config (`client.cfg`) | Single integer key `rasterMajor` already drives `LoadLibrary("./gl%02d_d.dll", ...)`; no new plumbing needed. |
| `Gl_api` contract surface | Plugin (`Direct3d11.dll`) | Engine (consumer) | Plugin exports one `GetApi()` returning a populated 120-function-pointer table; engine calls through `ms_api->` indirection. Engine never knows the underlying API. |
| D3D11 device + swap chain | Plugin (`Direct3d11.dll`) | OS (`DXGI`, `d3d11.dll`) | Win32 HWND comes in via `Gl_install` struct populated by engine; plugin owns the `ID3D11Device` + `IDXGISwapChain1`. |
| HLSL shader compile | Plugin (runtime via `d3dcompiler_47.dll`) | Filesystem (cache dir) | D-03 hybrid: compile on first use, persist `.cso` to disk, recompile on source-hash change. Asset-side `.vsh`/`.psh` source comes from TRE archives via `TreeFile::open` (engine-side). |
| FFP combiner emulation | Plugin (runtime HLSL generator) — IF spike says yes | Plugin (state cache) | D3D11 has no fixed-function pipeline; D-04 spike decides whether to build a `ps_5_0` generator or descope FFP entirely. |
| Texture / VB / IB resource lifetime | Plugin (`ID3D11Texture2D` / `ID3D11Buffer` + `ComPtr`) | — | No `D3DPOOL_MANAGED` (D3D11 doesn't have it); explicit Map/Unmap or staging-buffer copies for dynamic resources. |
| GPU profiling / DPVS remeasure | External tool (PIX or Nsight) | — | SPEC R7 + CONTEXT D-12: external-only per the spec; in-engine `D3D11_QUERY_TIMESTAMP` is the fallback if external capture hits friction. |
| Material → state-cache binding | Plugin (`Direct3d11_StateCache` analog) | Engine (`ShaderImplementation` consumer) | All texture-stage / blend / depth state lives in the plugin; the engine only feeds high-level `StaticShader` + `ShaderImplementation` data. |

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Direct3D 11 (`d3d11.h`, `d3d11.lib`) | Win SDK 10.0.26100.0 (system) | Core D3D11 device + immediate context + resource creation | The native target API. `[VERIFIED: ls C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um/d3d11.h]` |
| DXGI (`dxgi1_2.h`, `dxgi.lib`) | Win SDK 10.0.26100.0 | Swap chain (flip model), adapter enumeration, format negotiation | Required for `CreateSwapChainForHwnd` (modern, Win8+/Win10-compatible). `[CITED: learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model]` |
| D3DCompiler (`d3dcompiler.h`, `d3dcompiler.lib`, `d3dcompiler_47.dll`) | Win SDK 10.0.26100.0 + system | Runtime HLSL compile to bytecode (`vs_5_0` / `ps_5_0`) | Direct replacement for `D3DXCompileShader` per D-03 hybrid. `d3dcompiler_47.dll` ships in `C:/Windows/SysWOW64/` (32-bit, the target build) and `C:/Windows/System32/` (64-bit). `[VERIFIED: ls of system DLL paths]` |
| `Microsoft::WRL::ComPtr` (`wrl/client.h`) | Win SDK (header-only) | RAII for `IUnknown` COM resource ownership | Idiomatic D3D11 ownership wrapper per D-01. `[CITED: walbourn.github.io/anatomy-of-direct3d-11-create-device]` |
| DirectXMath (`DirectXMath.h`) | Win SDK (header-only) | Matrix / vector math compatible with constant buffers | XMFLOAT4X4 et al. provide proper packing/alignment for cbuffers; the engine's `Transform` / `Vector` / `GlMatrix4x4` types still convert in (see `Direct3d9.cpp:430-433` cached `D3DXMATRIX` slots — replace with `XMFLOAT4X4`). `[CITED: learn.microsoft.com/en-us/windows/uwp/gaming/port-the-shader-config]` |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `d3dcompiler_47.dll` (runtime) | OS-bundled | Provides `D3DCompile` symbols at runtime | The 32-bit copy in `C:/Windows/SysWOW64/d3dcompiler_47.dll` ships with every Win10/11 install; **do NOT vendor it alongside `gl11_d.dll`** — system-supplied is the cleanest path for Phase 11 (matches "no mandatory new dependencies" SPEC §Constraints). Link `d3dcompiler.lib` for the import lib. `[VERIFIED]` |
| `dxgi.lib`, `dxguid.lib` | Win SDK | DXGI factory + GUID symbols | Standard linker line; `dxguid.lib` provides COM interface UUIDs needed when using `IID_PPV_ARGS` / `__uuidof`. `[CITED: walbourn.github.io/anatomy-of-direct3d-11-create-device]` |
| Existing `legacy_stdio_definitions.lib` | already linked | C-runtime stdio shim | Already in `Direct3d9.vcxproj` link line; carry over to `Direct3d11.vcxproj`. |
| Existing `libjpeg.lib` | vendored at `external/3rd/library/libjpeg/lib` | JPEG screenshot output | `Direct3d9.vcxproj` links it for `screenShot()`; `Direct3d11.vcxproj` needs the same for the `Gl_api::screenShot` slot. |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `Microsoft::WRL::ComPtr` | Hand-rolled `Release()` + `NULL`-check pattern (matches D3D9 code's existing `IDirect3DDevice9 *ms_device` style) | ComPtr is the textbook D3D11 idiom and removes ~half the failure modes around exception-safety + early-return. D-01 explicitly calls for ComPtr. **Recommendation: use ComPtr.** |
| Runtime D3DCompile (D-03 hybrid) | Pre-compile shader catalog into `.cso` files at build time and ship them | Build-time compile would require an asset-pipeline rebuild step and a list of every shader file in the TRE archives. D-03 explicitly chose hybrid for "smallest delta from current D3D9 runtime-compile model." **Honor D-03.** |
| `D3DCompile` from `d3dcompiler_47.dll` | DXC (`dxcompiler.dll`, `dxc.exe`) → DXIL | DXC targets SM6+ for D3D12; D3D11 wants `vs_5_0`/`ps_5_0` DXBC. `D3DCompile` is the right tool for SM5.0 + D3D11. `[VERIFIED: Context7 listing — DXC is for D3D12]` |
| Win SDK `d3d11.h` (system) | Vendor D3D11 SDK headers alongside `external/3rd/library/directx9/include/` | Win SDK is fully sufficient for D3D 11.0 — the legacy DXSDK headers are stale and the legacy `D3DX*` library is gone. **Use system Win SDK; do NOT vendor.** Cleanest match to existing `WindowsTargetPlatformVersion=10.0` in every vcxproj. `[VERIFIED: Direct3d9.vcxproj line 19]` |
| `IDXGISwapChain` (legacy bitblt model) | `IDXGISwapChain1` flip model (`DXGI_SWAP_EFFECT_FLIP_DISCARD`, ≥2 buffers) | Flip model is the recommended path on Win10+ for performance, lower power, and parity with windowed/borderless modes the SPEC accepts. Legacy bitblt is path-of-least-resistance and would work but leaves perf on the table. **Use flip model.** `[CITED: learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model]` |

**Installation:** No install step needed. All headers + libs are present:
```
Headers : C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um/d3d11.h
Headers : C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um/d3dcompiler.h
Libs    : C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x86/d3d11.lib
Libs    : C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x86/d3dcompiler.lib
Runtime : C:/Windows/SysWOW64/d3dcompiler_47.dll  (32-bit, target build)
```

**Version verification:** Win SDK 10.0.26100.0 is the platform; D3D11 itself is contractually 11.0 (per SPEC §Constraints). No external package install needed; all four versions above are machine-resident `[VERIFIED 2026-05-15]`.

## Architecture Patterns

### System Architecture Diagram

```
client.cfg [rasterMajor=11]
        │
        ▼
SwgClient_d.exe ──► clientGraphics/Graphics::install()
                          │   reads ms_rasterMajor (line 184)
                          │   range-check accepts 11 (line 209-215, EDIT)
                          │   sets TAG_DX11 (line 211-213, EDIT)
                          │   sprintf "./gl%02d_d.dll" (line 219) → "./gl11_d.dll"
                          │   LoadLibrary (line 220)
                          │   GetProcAddress("GetApi") (line 236)
                          │   ms_api = getApi()
                          │   ms_api->verify() (line 245)
                          │   ms_api->install(&gl_install) (line 277)
                          ▼
                     gl11_d.dll : extern "C" GetApi()
                          │
                          ▼
                  Direct3d11Namespace::install(Gl_install*)
                          │   ms_glApi.<all 120 slots> = ...
                          │   ms_device = ID3D11Device   (D3D_FEATURE_LEVEL_11_0)
                          │   ms_context = ID3D11DeviceContext (immediate)
                          │   ms_swapChain = IDXGISwapChain1 (FLIP_DISCARD, n=2)
                          │   ms_backBufferRTV / ms_depthStencilDSV
                          ▼
                  Engine calls flow through ms_api-> indirection
                       │
            ┌──────────┼─────────────┬──────────────┬──────────────┐
            ▼          ▼             ▼              ▼              ▼
       beginScene   draw* ops     setStaticShader  createTextureData  present
                          │
                          ▼
                  ┌─────────────┐
                  │ State cache │  ── tracks dirty bits, batches state changes
                  └─────────────┘
                          │
                          ▼
                  ┌─────────────────────────────────────────┐
                  │ Shader compile pipeline (D-03 hybrid)   │
                  │ ──────────────────────────────────────  │
                  │ TreeFile::open("foo.vsh") → text        │
                  │ hash = sha1(text + #defines)            │
                  │ if exists(cache/<hash>.cso): load blob  │
                  │ else: D3DCompile → write blob to cache  │
                  │ device->CreateVertexShader(blob)        │
                  └─────────────────────────────────────────┘
                          │
                          ▼
                  ┌─────────────────────────────────────────┐
                  │ FFP combiner emulation (D-04 conditional)│
                  │ ──────────────────────────────────────  │
                  │ IF spike says any FFP activation:       │
                  │   For each (m_colorOperation,           │
                  │             m_alphaOperation,           │
                  │             arg0/arg1/arg2) tuple →     │
                  │   generate ps_5_0 source string,        │
                  │   D3DCompile, cache by tuple-hash,      │
                  │   bind via PSSetShader at draw time     │
                  │ ELSE: descope, no generator             │
                  └─────────────────────────────────────────┘
```

### Recommended Project Structure

```
src/engine/client/application/Direct3d11/
├── build/win32/
│   └── Direct3d11.vcxproj              # MSBuild project (single variant — no FFP/VSPS split)
└── src/
    ├── shared/
    │   └── FirstDirect3d11.{h,cpp}     # Precompiled header (matches FirstDirect3d9 pattern)
    └── win32/
        ├── Direct3d11.{h,cpp}          # Plugin entry, GetApi, install/remove, ms_glApi table
        ├── ConfigDirect3d11.{h,cpp}    # Plugin-private config keys
        ├── Direct3d11_Device.{h,cpp}   # Device + swap chain + RTV/DSV creation/teardown
        ├── Direct3d11_StateCache.{h,cpp}  # Tracks RS/BS/DSS/sampler state, batches changes
        ├── Direct3d11_TextureData.{h,cpp}     # ID3D11Texture2D + SRV
        ├── Direct3d11_StaticVertexBufferData.{h,cpp}    # ID3D11Buffer (IMMUTABLE)
        ├── Direct3d11_DynamicVertexBufferData.{h,cpp}   # ID3D11Buffer (DYNAMIC + Map/Unmap NO_OVERWRITE)
        ├── Direct3d11_StaticIndexBufferData.{h,cpp}
        ├── Direct3d11_DynamicIndexBufferData.{h,cpp}
        ├── Direct3d11_VertexShaderData.{h,cpp}    # D3DCompile vs_5_0, CreateVertexShader, input layout cache
        ├── Direct3d11_PixelShaderProgramData.{h,cpp}  # D3DCompile ps_5_0, CreatePixelShader
        ├── Direct3d11_ConstantBuffer.{h,cpp}      # Replaces SetVertexShaderConstantF / SetPixelShaderConstantF
        ├── Direct3d11_ShaderCache.{h,cpp}         # D-03 .cso disk cache (hash-keyed)
        ├── Direct3d11_FfpGenerator.{h,cpp}        # D-04 conditional — runtime ps_5_0 generator
        ├── Direct3d11_RenderTarget.{h,cpp}
        ├── Direct3d11_LightManager.{h,cpp}
        ├── Direct3d11_Metrics.{h,cpp}
        ├── Direct3d11_VertexDeclarationMap.{h,cpp}    # ID3D11InputLayout cache
        └── Direct3d11_VertexBufferDescriptorMap.{h,cpp}
```

### Pattern 1: Device + flip-model swap chain creation (replaces `IDirect3D9::CreateDevice`)

**What:** D3D11 device, immediate context, and DXGI flip-model swap chain in one go.
**When to use:** Inside `Direct3d11::install(Gl_install *gl_install)` — the install path that the engine calls via `ms_api->install(&gl_install)` at `Graphics.cpp:277`.

```cpp
// Source: walbourn.github.io/anatomy-of-direct3d-11-create-device
//         + learn.microsoft.com/.../for-best-performance--use-dxgi-flip-model
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;  // requires Graphics Tools optional feature
#endif

D3D_FEATURE_LEVEL featureLevels[] = {
    D3D_FEATURE_LEVEL_11_0,   // SPEC §Constraints baseline
};
D3D_FEATURE_LEVEL fl;

ComPtr<ID3D11Device>        device;
ComPtr<ID3D11DeviceContext> context;
HRESULT hr = D3D11CreateDevice(
    nullptr,                        // default adapter
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,                        // no software rasterizer
    createDeviceFlags,
    featureLevels, _countof(featureLevels),
    D3D11_SDK_VERSION,
    &device, &fl, &context);
FATAL(FAILED(hr) || fl < D3D_FEATURE_LEVEL_11_0,
      ("D3D11CreateDevice failed or feature level < 11.0 (got %x)", fl));

// Swap chain via DXGI flip model
ComPtr<IDXGIDevice>   dxgiDevice;
ComPtr<IDXGIAdapter>  adapter;
ComPtr<IDXGIFactory2> factory;
device.As(&dxgiDevice);
dxgiDevice->GetAdapter(&adapter);
adapter->GetParent(IID_PPV_ARGS(&factory));

DXGI_SWAP_CHAIN_DESC1 desc = {};
desc.Width            = gl_install->width;
desc.Height           = gl_install->height;
desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;   // matches engine's PackedArgb conventions
desc.SampleDesc.Count = 1;                            // MSAA not in flip-model swap chain (resolve manually)
desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
desc.BufferCount      = 2;                            // flip model requires ≥ 2
desc.Scaling          = DXGI_SCALING_STRETCH;
desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
desc.AlphaMode        = DXGI_ALPHA_MODE_UNSPECIFIED;

ComPtr<IDXGISwapChain1> swapChain;
factory->CreateSwapChainForHwnd(device.Get(), gl_install->window,
    &desc, nullptr, nullptr, &swapChain);

// Disable Alt-Enter — engine handles fullscreen toggle via setWindowedMode hook
factory->MakeWindowAssociation(gl_install->window, DXGI_MWA_NO_ALT_ENTER);
```

`[CITED: walbourn.github.io/anatomy-of-direct3d-11-create-device]` `[CITED: learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model]`

### Pattern 2: Constant buffer replaces `SetVertexShaderConstantF` / `SetPixelShaderConstantF`

**What:** D3D11 has no register-by-index constant setter; constants are uploaded as a `ID3D11Buffer` (cbuffer) and bound to a slot.
**When to use:** Every site that today calls `ms_device->SetVertexShaderConstantF(index, ...)` (`Direct3d9_StateCache.cpp:526`) or `SetPixelShaderConstantF(index, ...)` (`:561`). These are the *only* two register-setter sites in the entire D3D9 plugin — replacing them is bounded.

```cpp
// Source: learn.microsoft.com/en-us/windows/uwp/gaming/port-the-shader-config
// HLSL side (in the .vsh source loaded from TRE):
cbuffer PerObject : register(b0)
{
    float4x4 worldViewProj;
    float4   userConstants[8];   // mirrors Graphics::setVertexShaderUserConstants
};

// C++ side:
struct PerObject {
    DirectX::XMFLOAT4X4 worldViewProj;
    DirectX::XMFLOAT4   userConstants[8];
};
PerObject  ms_perObjectCpu;
ComPtr<ID3D11Buffer> ms_perObjectGpu;

CD3D11_BUFFER_DESC bd(sizeof(PerObject), D3D11_BIND_CONSTANT_BUFFER);
device->CreateBuffer(&bd, nullptr, &ms_perObjectGpu);

// Per-frame / per-draw update:
context->UpdateSubresource(ms_perObjectGpu.Get(), 0, nullptr, &ms_perObjectCpu, 0, 0);
ID3D11Buffer* cbs[] = { ms_perObjectGpu.Get() };
context->VSSetConstantBuffers(0, 1, cbs);
context->PSSetConstantBuffers(0, 1, cbs);
```

**Critical packing rule:** HLSL packs to 4-byte boundaries provided no field crosses a 16-byte boundary. Use `DirectX::XMFLOAT4X4` / `XMFLOAT4` types in C++ for natural alignment. Mismatched layout → silently-wrong shader output. `[CITED: learn.microsoft.com/en-us/windows/uwp/gaming/port-the-shader-config]`

### Pattern 3: HLSL shader recompile via `D3DCompile` with `.cso` cache (D-03 hybrid)

**What:** Read shader source from TRE archive, compile to bytecode at runtime, persist bytecode to disk for warm starts.
**When to use:** Every `Gl_api::createVertexShaderData` / `createPixelShaderProgramData` call. Replaces the `D3DXCompileShader` site at `Direct3d9_VertexShaderData.cpp:433`.

```cpp
// Source: learn.microsoft.com/en-us/windows/win32/direct3dhlsl/d3dcompile
//         + CONTEXT D-03 hybrid model
#include <d3dcompiler.h>

struct CachedShader {
    std::vector<uint8_t> bytecode;
    uint64_t             sourceHash;
};

CachedShader compileOrLoadVS(char const * sourceText, size_t sourceLen,
                             D3D_SHADER_MACRO const * defines)
{
    uint64_t hash = computeHash(sourceText, sourceLen, defines);
    std::string cachePath = std::format("{}/{:016x}.cso", ms_shaderCacheDir, hash);

    CachedShader out;
    if (loadFromDisk(cachePath, out) && out.sourceHash == hash)
        return out;   // cache hit

    ComPtr<ID3DBlob> bytecode, errors;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    HRESULT hr = D3DCompile(
        sourceText, sourceLen,
        "shader",         // virtual filename (for error messages)
        defines,          // mirrors D3DXMACRO array used by D3D9
        D3D_COMPILE_STANDARD_FILE_INCLUDE,   // simple #include resolver
        "main",
        "vs_5_0",
        flags, 0,
        &bytecode, &errors);
    FATAL(FAILED(hr), ("D3DCompile failed: %s",
        errors ? (char*)errors->GetBufferPointer() : "no error blob"));

    out.bytecode.assign((uint8_t*)bytecode->GetBufferPointer(),
                        (uint8_t*)bytecode->GetBufferPointer() + bytecode->GetBufferSize());
    out.sourceHash = hash;
    writeToDisk(cachePath, out);
    return out;
}

// Then create the shader object:
ComPtr<ID3D11VertexShader> vs;
device->CreateVertexShader(out.bytecode.data(), out.bytecode.size(), nullptr, &vs);
```

**Cache directory recommendation (Claude's discretion per CONTEXT):** `D:/Code/swg-client-v2/stage/shader-cache/` — sits alongside the existing `stage/dpvs-profile/` (gitignored) and survives across launches without crossing into git. **Add `stage/shader-cache/` to `.gitignore`** in the same plan that lands the cache writer. Naming: `<sha1-hex>.cso`. Manifest file `_index.json` mapping logical shader name → current hash (optional; the per-file `.cso` is self-contained).

### Pattern 4: D3D11 timestamp queries for DPVS GPU-cost remeasurement (SPEC R7 fallback)

**What:** Three queries — two `D3D11_QUERY_TIMESTAMP` bracketing the work + one `D3D11_QUERY_TIMESTAMP_DISJOINT` for frequency + disjoint flag.
**When to use:** ONLY if external-tool capture (PIX / Nsight) is unworkable for SPEC R7. Phase 10's CPU-side `QueryPerformanceCounter` measurement was the authoritative signal anyway (`gpu_us=0` was the *correct* answer for CPU-only DPVS work — see `docs/recon/10-dpvs-profiling.md` "Note on `gpu_us = 0`").

```cpp
// Source: therealmjp.github.io/posts/profiling-in-dx11-with-queries
//         + learn.microsoft.com/.../d3d11-d3d11_query_data_timestamp_disjoint
D3D11_QUERY_DESC desc = {};
desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
ComPtr<ID3D11Query> qDisjoint;
device->CreateQuery(&desc, &qDisjoint);

desc.Query = D3D11_QUERY_TIMESTAMP;
ComPtr<ID3D11Query> qBegin, qEnd;
device->CreateQuery(&desc, &qBegin);
device->CreateQuery(&desc, &qEnd);

// Frame N — issue:
context->Begin(qDisjoint.Get());
context->End(qBegin.Get());
// ... DPVS work here ...
context->End(qEnd.Get());
context->End(qDisjoint.Get());

// Frame N + 2 (or later) — read:
D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
UINT64 t0, t1;
while (context->GetData(qDisjoint.Get(), &disjointData,
                         sizeof(disjointData), 0) == S_FALSE) { /* spin */ }
context->GetData(qBegin.Get(), &t0, sizeof(t0), 0);
context->GetData(qEnd.Get(),   &t1, sizeof(t1), 0);

if (!disjointData.Disjoint && disjointData.Frequency != 0) {
    double us = double(t1 - t0) * 1e6 / double(disjointData.Frequency);
    // record us
}
```

**Recovery shortcut:** Phase 10 closeout left a `git tag phase-10-instrumentation-pre-cleanup at 9f2ec3715`. If Phase 11 wants to reuse the engine-side scaffolding (CSV writer, run-label sanitizer, F10/F11 keybind, `/setrunlabel` console command), `git revert <D-15-cleanup-commit>` restores everything; only the D3D9-specific query-pool at `Direct3d9.cpp:4621-4709` needs reauthoring under `ID3D11Query`. `[VERIFIED: 10-SUMMARY.md "Phase 11 recovery anchor"]`

### Anti-Patterns to Avoid

- **Copying the `Direct3d9/` source tree wholesale.** Per CONTEXT D-01: D3D9 idioms (lost-device, `D3DPOOL_MANAGED`, register-based constants, FFP combiners deeply baked into file shape) carry forward exactly what D3D11-02 acceptance excludes. Read D3D9 *for design intent on each Gl_api function*, but author Direct3d11 from a clean page.
- **Vendoring `d3dcompiler_47.dll` next to `gl11_d.dll`.** The DLL ships with every Win10/11 install (verified at `C:/Windows/SysWOW64/d3dcompiler_47.dll`). Vendoring duplicates a system DLL and causes versioning confusion. **Link `d3dcompiler.lib` and let the OS resolve the DLL.**
- **Re-instrumenting the source for DPVS without an external-tool first attempt.** SPEC §Constraints + R7 + CONTEXT D-12 explicitly say external tools (PIX / Nsight / GPUView) come first. The git tag `phase-10-instrumentation-pre-cleanup` is a safety net, not the default.
- **MSAA inside the flip-model swap chain.** Flip-model swap chains do not support MSAA buffers directly; if MSAA is needed, render to an MSAA `ID3D11Texture2D` and `ResolveSubresource()` into the swap-chain back buffer before `Present()`. `[CITED: walbourn.github.io/care-and-feeding-of-modern-swapchains]`
- **Using fixed sampler-bound texture state (D3D9 `SetSamplerState` per stage).** D3D11 separates resource view (`ID3D11ShaderResourceView`) from sampler (`ID3D11SamplerState`) — bind both via `PSSetShaderResources` + `PSSetSamplers`. The state cache must track both sides independently.
- **Mixing immediate context calls from non-render threads.** The immediate context (`ID3D11DeviceContext` returned by `D3D11CreateDevice`) is not thread-safe. SWG's render thread is single-threaded — keep it that way. **Do not introduce a deferred context** for Phase 11 (out of scope per SPEC).
- **Forgetting `OMSetRenderTargets()` after every `Present()` on flip-model.** Flip model unbinds the back buffer after present; rebinding is required before drawing into it next frame. `[CITED: learn.microsoft.com/.../for-best-performance--use-dxgi-flip-model]`

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| COM resource lifetime | Custom `Release()` + `NULL` boilerplate | `Microsoft::WRL::ComPtr<T>` | RAII + exception-safe + half the failure modes around early-return paths. Standard since D3D11 shipped. |
| HLSL → bytecode compile | Custom HLSL parser / lexer | `D3DCompile` from `d3dcompiler.lib` | The reference compiler. Honors all official extensions, semantics, optimization levels. |
| Matrix layout for cbuffers | Hand-packed `float[16]` | `DirectX::XMFLOAT4X4` | Native packing/alignment for the 16-byte cbuffer rule. Hand-packing is the #1 silent-shader-bug source. |
| Swap chain (legacy bitblt) | `DXGI_SWAP_EFFECT_DISCARD` | `DXGI_SWAP_EFFECT_FLIP_DISCARD` (≥2 buffers) | Flip model is the modern recommendation; legacy bitblt is a dead path on Win10+. |
| Shader-source disk cache | Hand-rolled mmap + format-versioning | `D3DCompile` → raw `ID3DBlob` bytes + per-file `.cso` write/read | The `.cso` blob is opaque & self-versioning to the runtime; just hash + dump + load. No format game. |
| Adapter / format enumeration | Hard-coded format lists | `IDXGIFactory::EnumAdapters` + `CheckFeatureSupport` | The D3D9 plugin's giant `getMatchingColorAlphaFormats` switch (`Direct3d9.cpp:716-802`) is gone in D3D11; query DXGI for what the device supports. |
| Lost-device recovery | `IDirect3DDevice9::TestCooperativeLevel` polling + Reset | **Nothing — D3D11 doesn't lose its device on Alt-Tab** | D3D11 only "loses" the device on hardware-level catastrophe (`DXGI_ERROR_DEVICE_REMOVED` from `Present()`). Recovery there is process-restart-class, not in scope per SPEC §Boundaries. |

**Key insight:** D3D11 is where 15 years of engine evolution finally settles into a small, regular API. The D3D9 plugin's worst code (lost-device dance, format-fallback chains, FFP combiner state, `D3DPOOL_MANAGED` heuristics) all simply disappears in D3D11 — there's nothing to port forward. The D3D11 plugin should be measurably *smaller* than the D3D9 plugin once feature-equivalent, not larger.

## Runtime State Inventory

> Phase 11 is mostly *additive new code*, not a rename or refactor. But there are a few runtime-state questions worth answering explicitly so the planner doesn't trip on them.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | **`local_machine_options.iff` persists `[ClientGraphics] rasterMajor` per-machine** — switching to D3D11 may persist back to `5` if the engine writes the value. Verify by inspection: scan `LocalMachineOptionManager::registerOption` calls in `Graphics.cpp:249-255` (does NOT register `rasterMajor` in current code — `[VERIFIED: Graphics.cpp:249-255 registers brightness/contrast/gamma/screenShotFormat/screenShotQuality/windowX/windowY only — NOT rasterMajor]`). So switching is config-only via `client.cfg`, not auto-persisted. Operationally clean. | None — config edit only. |
| Live service config | None — Phase 11 has no n8n / Datadog / Tailscale / external-service touch. | None. |
| OS-registered state | None — no Windows Task Scheduler / launchd / pm2 / Docker entries reference any renderer DLL by name. | None. |
| Secrets / env vars | None — the renderer plugin reads no environment variables. | None. |
| Build artifacts | **Existing `compile/win32/Direct3d9/Debug/gl05_d.dll`** + `gl06_d.dll` + `gl07_d.dll` (FFP / VSPS variants) live in the build tree and the `dev/win32/` POST_BUILD copy target. **Verified present in `D:/Code/swg-client-v2/stage/`:** `gl05_d.dll`, `gl05_r.dll`, `gl06_d.dll`, `gl06_r.dll`, `gl07_d.dll`, `gl07_r.dll`. Phase 11 adds `gl11_d.dll` / `gl11_r.dll` / `gl11_o.dll` to the same tree. **Stage location resolution:** `Direct3d9.vcxproj` POST_BUILD command `copy $(TargetPath) ..\..\..\..\..\..\..\dev\win32\gl05_d.dll` (line 134) — i.e., `dev/win32/` not `build/bin/Debug/`. Plan author MUST confirm the stage location currently used by `SwgClient_d.exe` (likely `D:/Code/swg-client-v2/stage/` per the existing DLLs) and use the same convention for `gl11_d.dll`. | New `Direct3d11.vcxproj` POST_BUILD copy targets the same staging dir. |

## Common Pitfalls

### Pitfall 1: SV_POSITION semantic divergence between vertex shader output and pixel shader input

**What goes wrong:** D3D9 HLSL allows `float4 pos : POSITION` on both vertex output and pixel input. D3D11 requires the vertex shader output to be `: SV_POSITION` (a system-value semantic), AND the pixel shader's first input must be `: SV_POSITION` if the position is consumed. Compiler error if not; silent wrong-output if you map an old `: POSITION` field through.
**Why it happens:** Legacy semantic name `POSITION` doesn't get the system-value treatment in SM4+. The runtime needs the SV_ marker to know to do screen-space transform.
**How to avoid:** Inject a wrapper macro into the source text before `D3DCompile`:
```cpp
D3D_SHADER_MACRO defines[] = {
    {"POSITION", "SV_POSITION"},   // legacy alias
    {nullptr, nullptr},
};
```
This works as long as the original `.vsh` source uses `POSITION` strictly as the output position semantic. If it's used elsewhere (e.g. as a vertex *input* semantic in `struct VertexInput`), the macro substitution will incorrectly rewrite the input semantic too — verify with a grep over the asset shader catalog during the spike.
**Warning signs:** Vertex shader compiles but pixel shader fails with `error X4502: invalid input semantic 'POSITION'`. Or compiled output is solid black.
`[CITED: learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics]`

### Pitfall 2: Constant buffer alignment mismatch silently corrupts shader inputs

**What goes wrong:** `cbuffer` packing is 4-byte aligned with the constraint that no field crosses a 16-byte boundary. A C++ struct `{ float a; XMFLOAT4 b; }` packs `a` at offset 0, then *pads to 16 bytes* before `b` — but a careless plain struct gets `b` at offset 4, which the GPU reads as offset 0 of the next 16-byte chunk. Result: shader sees wrong values for every field after the misaligned one.
**Why it happens:** D3D9 didn't have this rule; D3D9 packed fields tightly. Migrating an existing `D3DXMATRIX` + `float[8]` pair without thinking introduces silent shader bugs.
**How to avoid:** Always use `DirectX::XMFLOAT4X4` and `DirectX::XMFLOAT4` (or pad with `float4 _padN`) on the C++ side. Mirror the HLSL `cbuffer` declaration exactly. `static_assert(sizeof(MyCB) == sizeof_in_hlsl)` if possible.
**Warning signs:** Shader output is partly correct (first field is right) then visually corrupt. Debug-layer output is silent (alignment is "valid", just wrong).
`[CITED: learn.microsoft.com/en-us/windows/uwp/gaming/port-the-shader-config — "HLSL packs data into 4-byte boundaries, provided that it doesn't cross a 16-byte boundary"]`

### Pitfall 3: Forgetting to rebind back-buffer RTV after every `Present()` in flip model

**What goes wrong:** With `DXGI_SWAP_EFFECT_FLIP_DISCARD`, the swap chain unbinds the back buffer after `Present()`. Next frame's draw calls write to a null render target (or a previous-frame buffer if the cache happens to be stale) — visually black, no warning.
**Why it happens:** Legacy bitblt model didn't unbind. Flip model is more correct but new code forgets the rebind.
**How to avoid:** At the start of every frame's `beginScene()` (the `Gl_api::beginScene` slot), call `context->OMSetRenderTargets(1, &ms_backBufferRTV, ms_depthStencilDSV.Get())`. Make this unconditional.
**Warning signs:** First frame renders correctly, all subsequent frames black. D3D11 debug layer prints `"OMSetRenderTargets: Resource being set to OM RenderTarget slot 0 is still bound on input!"` *or* nothing.
`[CITED: learn.microsoft.com/.../for-best-performance--use-dxgi-flip-model]`

### Pitfall 4: D3D11 sampler state and SRV are independent — both must be bound

**What goes wrong:** D3D9's `IDirect3DDevice9::SetTexture(stage, tex)` set both the resource and the sampler in one call. D3D11 splits them: `PSSetShaderResources` for the SRV, `PSSetSamplers` for the sampler. Forget either side and you sample garbage / nothing.
**Why it happens:** API split is real and the existing D3D9 state cache (`Direct3d9_StateCache.cpp` lines 156-197 — `D3DSAMP_*` via the `SSS` macro) treats samplers as part of the texture binding.
**How to avoid:** The `Direct3d11_StateCache` analog must track SRV bindings (`SRV[16]` per stage) AND sampler bindings (`Sampler[16]` per stage) as orthogonal caches. The `Gl_api::setGlobalTexture` slot binds the SRV; the implicit sampler defaults must come from the shader-implementation pass data (the `D3DSAMP_*` settings the D3D9 code reads from `.iff` shader templates).
**Warning signs:** Textures appear correctly when fresh-bound but wrong when state is dirty across draws. Debug layer warns `"PSSetShaderResources: Resource has no shader resource view to bind"`.
`[CITED: D3D11 fundamentals — verified by inspection of the D3D9 state cache split]`

### Pitfall 5: Map/Unmap with `D3D11_MAP_WRITE_DISCARD` vs `D3D11_MAP_WRITE_NO_OVERWRITE` confusion in dynamic VB/IB

**What goes wrong:** D3D9's `D3DUSAGE_DYNAMIC` + `D3DLOCK_DISCARD` / `D3DLOCK_NOOVERWRITE` semantics map to D3D11's `D3D11_USAGE_DYNAMIC` + `D3D11_MAP_WRITE_DISCARD` / `D3D11_MAP_WRITE_NO_OVERWRITE` — but the rules around when each is valid (only on `D3D11_USAGE_DYNAMIC` resources, only with `CPU_ACCESS_WRITE`, NO_OVERWRITE only valid for VB/IB on D3D 11.0 — constant buffers got it in 11.1) trip new code consistently.
**Why it happens:** The D3D9 dynamic-buffer pattern (large ring-buffer with rolling write offsets, DISCARD when wrap, NO_OVERWRITE between wraps) is the right pattern for D3D11 too — but the create-time flags differ.
**How to avoid:** Read `Direct3d9_DynamicVertexBufferData.cpp` for the existing wrap/rotate logic, then translate the create call:
```cpp
D3D11_BUFFER_DESC bd = {};
bd.ByteWidth      = ringSize;
bd.Usage          = D3D11_USAGE_DYNAMIC;
bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
device->CreateBuffer(&bd, nullptr, &ms_dynVB);

// On wrap:
D3D11_MAPPED_SUBRESOURCE mapped;
context->Map(ms_dynVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
// On non-wrap append:
context->Map(ms_dynVB.Get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped);
```
**Warning signs:** Dynamic geometry visibly tearing or showing ghost data from the previous frame. Debug layer warns about CPU/GPU contention.
`[CITED: D3D11 documentation — verified via D3D9 source pattern in Direct3d9_DynamicVertexBufferData.cpp]`

### Pitfall 6: Input layout (`ID3D11InputLayout`) must match BOTH the vertex buffer layout AND the vertex shader signature

**What goes wrong:** D3D9 `IDirect3DVertexDeclaration9` describes only the vertex buffer layout. D3D11 `ID3D11InputLayout` is created against a *compiled vertex shader bytecode* and must match the shader's input signature exactly. Use the wrong shader's bytecode at create-time and `CreateInputLayout` returns `E_INVALIDARG` or — worse — succeeds with subtly-wrong slot mappings.
**Why it happens:** The D3D9 plugin's `Direct3d9_VertexDeclarationMap.cpp` caches declarations keyed by VB format alone. D3D11 keys must be `(VB format, vertex shader bytecode hash)`.
**How to avoid:** Cache `ID3D11InputLayout` keyed by the compound `(VertexBufferFormat, vertex shader bytecode SHA1)` — the same shader compiled with different `#defines` becomes different bytecode and needs its own input layout.
**Warning signs:** Drawing seems to work but vertex attributes (color, UV) are shifted or scrambled. Debug layer warns `"CreateInputLayout: vertex shader signature mismatch"`.
`[CITED: D3D11 fundamentals]`

### Pitfall 7: 32-bit pointer base addresses and DLL load contention (existing v2 quirk)

**What goes wrong:** The existing `Direct3d9.vcxproj` sets `<BaseAddress>0x62A00000</BaseAddress>` (line 123). 32-bit address space is constrained; if `gl11_d.dll`'s base overlaps another DLL the loader rebases (with a one-time perf cost, harmless but noisy in WinDbg).
**Why it happens:** The codebase predates ASLR-by-default; the explicit base addresses are residue from the 2005-era memory map.
**How to avoid:** Pick a non-overlapping base for `gl11_d.dll` — e.g. `0x60A00000` (well below the existing `gl05_*` at `0x62A00000`). Cross-check against `dumpbin /headers` of every DLL in `stage/` to confirm no clash.
**Warning signs:** None at runtime — purely cosmetic. Worth doing right.

### Pitfall 8: D3D11 debug layer requires the "Graphics Tools" Windows optional feature

**What goes wrong:** `D3D11CreateDevice` with `D3D11_CREATE_DEVICE_DEBUG` returns `DXGI_ERROR_SDK_COMPONENT_MISSING` if the Graphics Tools optional feature isn't installed on this Win10/11 system.
**Why it happens:** The debug runtime moved out of the legacy DXSDK into a Windows optional feature.
**How to avoid:** Detect-and-fallback: try `D3D11CreateDevice` with `D3D11_CREATE_DEVICE_DEBUG`; if it returns `DXGI_ERROR_SDK_COMPONENT_MISSING`, retry without the flag and emit a `DEBUG_REPORT_LOG_PRINT` saying "install Graphics Tools optional feature for D3D11 debug-layer messages". Do not FATAL.
**Warning signs:** Debug builds fail to create device with `DXGI_ERROR_SDK_COMPONENT_MISSING` HRESULT. Release builds unaffected.
`[CITED: walbourn.github.io/anatomy-of-direct3d-11-create-device]`

## Code Examples

### `Gl_api` table population (the install path)

```cpp
// Source: existing Direct3d9.cpp:505-1136 pattern, applied to D3D11
extern "C" __declspec(dllexport) Gl_api const * GetApi();

Gl_api ms_glApi;   // namespace-scoped, populated lazily

Gl_api const * GetApi()
{
    ms_glApi.verify  = Direct3d11Namespace::verify;
    ms_glApi.install = Direct3d11::install;
    return &ms_glApi;
}

// Inside Direct3d11::install(Gl_install*) — populate the remaining 118 slots:
ms_glApi.remove                            = Direct3d11Namespace::remove;
ms_glApi.displayModeChanged                = Direct3d11Namespace::displayModeChanged;
ms_glApi.getShaderCapability               = Direct3d11Namespace::getShaderCapability;
ms_glApi.requiresVertexAndPixelShaders     = []() { return true; };  // D3D11 has no FFP
// ... 115 more slots, mirror Direct3d9.cpp:985-1136 verbatim for the SLOT NAMES,
//     but with D3D11-native implementations on the right-hand side.

// Lost-device callbacks must STILL be registered as no-ops (engine queries them):
ms_glApi.addDeviceLostCallback     = [](Gl_api::CallbackFunction){};
ms_glApi.removeDeviceLostCallback  = [](Gl_api::CallbackFunction){};
ms_glApi.addDeviceRestoredCallback = [](Gl_api::CallbackFunction){};
ms_glApi.removeDeviceRestoredCallback = [](Gl_api::CallbackFunction){};
ms_glApi.wasDeviceReset            = []() { return false; };
```

### Engine-side range-check edit (the only edit to engine code per D-02 + D-02a)

```cpp
// File: src/engine/client/library/clientGraphics/src/win32/Graphics.cpp
// Current (line 65-66):
const Tag TAG_DX8 = TAG3(D,X,8);
const Tag TAG_DX9 = TAG3(D,X,9);
// EDIT: add a sibling
const Tag TAG_DX11 = TAG3(D,1,1);   // 'D','1','1' — three-char tag, fits TAG3 macro

// Current (line 209-215):
if (ms_rasterMajor >= 5 && ms_rasterMajor <= 7)
{
    GraphicsOptionTags::set(TAG_DX8, false);
    GraphicsOptionTags::set(TAG_DX9, true);
}
else
    DEBUG_FATAL(true, ("unknown rasterizer"));

// EDIT to:
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

**`TAG_DX9` cross-codebase audit result:** `[VERIFIED: grep "TAG_DX9" returns ONLY two hits, both in `Graphics.cpp` itself (line 66 def, line 212 set)]`. There are no `GraphicsOptionTags::get(TAG_DX9)` callers in the rest of the codebase that would need updating. The new `TAG_DX11` is a strictly-additive symbol; existing tag consumers (terrain fallback, etc.) don't need to learn about it for Phase 11 to work.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Legacy DXSDK headers (`D3DX9`, June 2010 redist) | Win SDK system headers (`d3d11.h`, `dxgi.h`, `d3dcompiler.h`) | Win 8 / Win SDK 8.0 (~2012) | The `D3DX*` library is gone. `DirectXMath` + `DirectXTex` + `DirectXTK` replace its bits. SWG's existing `D3DXMATRIX` cached transforms become `XMFLOAT4X4`. |
| `D3DXCompileShader` from `d3dx9.lib` (DXSDK) | `D3DCompile` from `d3dcompiler.lib` (Win SDK) | Win SDK 8.0 (~2012) | Same compile semantics, different entry point. Profile strings update: `vs_2_0` → `vs_5_0`. |
| `IDirect3DSwapChain9` bitblt swap | `IDXGISwapChain1` flip model (`DXGI_SWAP_EFFECT_FLIP_DISCARD`, ≥2 buffers) | Win 10 (recommended); legacy bitblt deprecated 2018 | Better perf, lower power, parity with windowed/borderless modes; MSAA must resolve manually. |
| Hand-rolled `Release()` + null-check | `Microsoft::WRL::ComPtr<T>` | Win SDK 8.0 (`wrl/client.h`) | RAII for COM; standard since D3D11 shipped. |
| D3D9 `IDirect3DQuery9` `D3DQUERYTYPE_TIMESTAMP` | `ID3D11Query` `D3D11_QUERY_TIMESTAMP` + `D3D11_QUERY_TIMESTAMP_DISJOINT` | D3D11.0 | Same three-query pattern (begin/end timestamp + disjoint frequency); slightly different API spelling. Phase 10 instrumentation pattern ports cleanly. |
| `IDirect3DDevice9::SetTexture(stage, tex)` (couples SRV + sampler) | `PSSetShaderResources` + `PSSetSamplers` (independent) | D3D10/11 | State cache must track them separately. |

**Deprecated/outdated:**
- **`d3dx9.lib`, `d3dx9.dll`:** Deprecated 2012, redistribution discontinued. SWG's vendored `external/3rd/library/directx9/lib/d3dx9.lib` stays for D3D9 plugin only — not needed by D3D11 plugin.
- **`D3DXCompileShader`:** Replaced by `D3DCompile`. Same use case, different DLL.
- **`D3DPOOL_MANAGED`:** No equivalent in D3D11. Resource creation is explicit (`D3D11_USAGE_*` + `D3D11_BIND_*` + `D3D11_CPU_ACCESS_*`).
- **Lost-device pattern (`OnLostDevice` / `OnResetDevice`):** Doesn't exist in D3D11. The only reset-class event is `DXGI_ERROR_DEVICE_REMOVED` on hardware catastrophe (process restart). Per SPEC §Boundaries.
- **Fixed-function pipeline (`SetRenderState(D3DRS_*)`, `SetTextureStageState(stage, D3DTSS_*)`):** Removed in D3D10. Replaced by HLSL pixel shaders. D-04 spike decides whether SWG needs runtime emulation.
- **Vertex/pixel shader profiles `vs_1_1`/`vs_2_0`/`ps_1_1`/`ps_2_0`:** Replaced by `vs_5_0`/`ps_5_0`. Mid-tier `vs_4_0_level_9_3` exists for backward-compat with feature-level 9 hardware — **not relevant for SPEC §Constraints D3D 11.0 baseline.**

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| D3D11-01 | New `Direct3d11` plugin producing `Direct3d11.dll` satisfying the existing 119-function `Gl_api` table | §Standard Stack (Win SDK headers/libs verified present), §Architecture Patterns Pattern 1 (device + swap chain), §Code Examples (Gl_api install pattern + range-check edit). Slot count is **120 in `Gl_dll.def`** (`[VERIFIED: grep returned 120 function-pointer declarations]`); `Direct3d9.cpp` populates **118** of them in its install path (`[VERIFIED: 118 `ms_glApi.\w+ =` matches]`); SPEC's "119" is approximate but the slot list itself is authoritative — implement against `Gl_dll.def`. |
| D3D11-02 | Resource management uses `ID3D11Device` + `ID3D11DeviceContext`, no `D3DPOOL_MANAGED`, no lost-device | §Don't Hand-Roll (lost-device pattern removed in D3D11), §Common Pitfalls 5 (USAGE flag translation). D-01 clean-rewrite rationale + Pitfall 1 acceptance grep target both supported. |
| D3D11-03 | All shaders compile under HLSL SM5.0 (`vs_5_0` / `ps_5_0`) with `SV_POSITION` semantics | §Architecture Patterns Pattern 2 (cbuffer migration), Pattern 3 (D3DCompile + cache), §Common Pitfalls 1 (SV_POSITION) + 2 (alignment). Single shader-target site at `Direct3d9_VertexShaderData.cpp:351-353` to update. |
| D3D11-04 | FFP pixel shader generator — conditional on D-04 spike outcome | §Domain (FFP class hierarchy at `ShaderImplementation.h:262, 310-358`; existing FFP setter sites at `Direct3d9_StateCache.cpp:159,162` + `Direct3d9_ShaderImplementationData.cpp:147,152,382-383`). D-04 two-phase spike + D-04a verdict threshold provide the structure. |
| D3D11-05 | Tatooine outdoor + Mos Eisley cantina interior render under D3D11 with terrain / skeletal / particles / HUD subsystems | All four subsystems consume the same `Gl_api` surface — once the table is populated, the engine drives them. The visual-parity verification per SPEC R6 is screenshot-based ("substantially similar"); Phase 9 closeout proved the engine boots to Tatooine cleanly under D3D9 (baseline). |

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Windows SDK headers (`d3d11.h`, `dxgi.h`, `d3dcompiler.h`) | All Phase 11 compile | ✓ | 10.0.26100.0 | — |
| Windows SDK libs (`d3d11.lib`, `dxgi.lib`, `dxguid.lib`, `d3dcompiler.lib`) | All Phase 11 link | ✓ | 10.0.26100.0 (x86) | — |
| `d3dcompiler_47.dll` (32-bit runtime) | D-03 hybrid runtime compile | ✓ | System (Win11, SysWOW64) | — |
| `d3dcompiler_47.dll` (64-bit, dev tools) | none for plugin (target is Win32) | ✓ | System32 | — |
| MSBuild + v145 toolset (VS 2026 / VS 2022 v17.x build tools) | Build entry per CONTEXT canonical refs | ✓ | (assumed — Phase 9 closeout proved swg.sln builds end-to-end) | — |
| GPU with D3D 11.0 feature level | Runtime device creation | ✓ | NVIDIA RTX 3060 (per `docs/recon/10-dpvs-profiling.md`) | — |
| PIX on Windows | SPEC R7 DPVS remeasurement (preferred) | ✗ | — | Nsight Graphics (NVIDIA, free, supports D3D11), GPUView (in Windows ADK), or in-engine `ID3D11Query` timestamps via Phase 10 recovery anchor |
| NVIDIA Nsight Graphics | SPEC R7 DPVS remeasurement (Nsight branch) | unknown — not on PATH | — | Free download from NVIDIA developer site; viable for an RTX 3060 |
| Windows ADK / GPUView | SPEC R7 DPVS remeasurement (GPUView branch) | unknown | — | Free; Windows Performance Toolkit ships with the ADK |
| Graphics Tools optional Windows feature | D3D11 debug layer (`D3D11_CREATE_DEVICE_DEBUG`) | unknown | — | Plugin must detect-and-fallback per Pitfall 8 — debug layer is dev-only nice-to-have, not blocking |

**Missing dependencies with no fallback:** None — Phase 11 can execute against the current machine state.

**Missing dependencies with fallback:**
- **PIX:** Plan author should install PIX-on-Windows during the SPEC R7 plan (free MSDN download). Nsight Graphics is a viable alternative. If neither is acceptable, restore Phase 10 instrumentation per the recovery-anchor pattern in §Architecture Patterns Pattern 4.

## Validation Architecture

> Validation framework for Phase 11 per `workflow.nyquist_validation: true` in `.planning/config.json`.

### Test Framework

| Property | Value |
|----------|-------|
| Framework | **MSBuild (build-pass)** + **direct binary inspection** (`dumpbin /exports`, `dumpbin /headers`, `dumpbin /imports`) + **runtime smoke** (boot SwgClient_d.exe + reach char-select + zone-in to Tatooine + travel to cantina). No unit-test framework in this codebase. |
| Config file | None — MSBuild is the test runner. |
| Quick run command | `MSBuild D:/Code/swg-client-v2/src/build/win32/swg.sln /t:Direct3d11 /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` |
| Full suite command | `MSBuild D:/Code/swg-client-v2/src/build/win32/swg.sln /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` (full client) |
| Phase gate | Build-pass + boot-to-cantina smoke. Visual-parity screenshots are the user-driven SPEC R6 acceptance. |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|--------------|
| D3D11-01 | Plugin scaffold builds, exports `GetApi`, populates Gl_api table | build-pass + binary inspection | `MSBuild ... /t:Direct3d11` then `dumpbin /exports stage/gl11_d.dll \| findstr GetApi` | ❌ Wave 0 (vcxproj does not yet exist) |
| D3D11-01 | Engine-side range-check accepts `rasterMajor=11` | build-pass + smoke | `MSBuild ... /t:clientGraphics` (no FATAL on link), then runtime: launch with `client.cfg [ClientGraphics] rasterMajor=11` and observe LoadLibrary success in log | ❌ Wave 0 |
| D3D11-01 | D3D9 plugin still builds clean (D-05) | build-pass | `MSBuild ... /t:Direct3d9` after every Phase 11 commit; expect EXIT=0 | ✓ Existing |
| D3D11-02 | Zero matches for `D3DPOOL_MANAGED`, `OnLostDevice`, `OnResetDevice` in `Direct3d11/` | grep | `grep -rE "D3DPOOL_MANAGED\|OnLostDevice\|OnResetDevice" src/engine/client/application/Direct3d11/` returns 0 hits | ❌ Wave 0 (Direct3d11/ doesn't exist) |
| D3D11-02 | Plugin survives Alt-Tab + window resize for ≥5 min | manual-only | manual smoke session | (manual — no automation possible without scripted input + GPU passthrough) |
| D3D11-03 | All asset shaders compile with `vs_5_0` / `ps_5_0` | runtime smoke + log inspection | Launch under D3D11; `findstr "D3DCompile failed" stage/log-*.txt` returns no matches across a Tatooine + cantina session | ❌ Wave 0 (compile pipeline doesn't exist) |
| D3D11-03 | Runtime device negotiates feature level ≥ 11.0 | startup log assertion | Plugin's install path emits `DEBUG_REPORT_LOG_PRINT(true, ("D3D11 device created at feature level %x", fl));`; verify in log | ❌ Wave 0 |
| D3D11-04 | FFP spike result documented as a Phase 11 plan artifact | doc-exists check | `test -f .planning/phases/11-d3d11-renderer-plugin/11-XX-ffp-spike-finding.md` | ❌ Wave 0 |
| D3D11-05 | Tatooine outdoor + cantina interior render under D3D11 ≥5 min each without crash | manual-only | manual smoke session | (manual) |
| D3D11-05 | Visual-parity screenshots committed | doc-exists check | `test -d docs/recon/11-d3d11-screenshots/ && test -f docs/recon/11-d3d11-screenshots/comparison-notes.md` | ❌ Wave 0 |
| SPEC R7 | DPVS verdict + decision documented | doc-exists check | `test -f docs/recon/11-dpvs-d3d11-remeasure.md` | ❌ Wave 0 |
| SPEC R7 | If verdict diverges from Option α: source edit at `RenderWorld.cpp:902-908` | conditional grep | If verdict = "keep" or "scene-aware": `grep "OCCLUSION_CULLING" src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` returns ≥1 hit | (conditional) |

### Sampling Rate

- **Per task commit:** `MSBuild ... /t:Direct3d11` (incremental build of plugin only) — fast inner loop.
- **Per wave merge:** `MSBuild ... /t:Direct3d9` (D-05 protection) + `MSBuild ... /t:Direct3d11` + `MSBuild ... /t:SwgClient_d` (full link).
- **Per wave merge (boot gate):** Launch SwgClient_d.exe under `rasterMajor=5` (D3D9 fallback), reach char-select, exit cleanly. THEN under `rasterMajor=11`, repeat — the criterion grows over the phase as functionality lands (clear-to-color → terrain → cantina interior).
- **Phase gate:** Full Tatooine outdoor + Mos Eisley cantina interior smoke per SPEC §Acceptance Criteria, plus the four reference screenshots per SPEC R6, plus the DPVS remeasurement doc per SPEC R7.

### Wave 0 Gaps

- [ ] `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj` — produces `gl11_d.dll`, references all 21 source files listed in §Recommended Project Structure
- [ ] `src/engine/client/application/Direct3d11/src/win32/Direct3d11.{h,cpp}` — plugin entry, `GetApi`, install/remove
- [ ] `src/engine/client/application/Direct3d11/src/win32/FirstDirect3d11.{h,cpp}` — precompiled header (matches `FirstDirect3d9` pattern)
- [ ] `src/build/win32/swg.sln` — add `Direct3d11.vcxproj` reference (one new `Project(...) = "Direct3d11", "..."` line + matching `EndProject` + `GlobalSection(ProjectConfigurationPlatforms)` entries for Debug|Win32 / Release|Win32 / Optimized|Win32)
- [ ] `stage/shader-cache/` directory + `.gitignore` entry — D-03 disk cache (lazy-create on first compile)
- [ ] `docs/recon/11-d3d11-screenshots/` directory — SPEC R6 reference screenshot home (lazy-create on first capture)
- [ ] `docs/recon/11-dpvs-d3d11-remeasure.md` — SPEC R7 verdict doc (created in the DPVS remeasure plan, not Wave 0)

*(No test-framework gaps — this codebase has never had a unit-test framework; verification is build-pass + binary inspection + manual smoke per the precedent established in Phases 7, 9, and 10.)*

## Project Constraints (from CLAUDE.md)

`./CLAUDE.md` does not exist in `D:/Code/swg-client-v2/` `[VERIFIED: ls returned "no such file"]`. Project-level rules come from `.planning/STATE.md` "Accumulated Context" + memory `feedback_dont_modify_koogie_changes.md`:

- **Active source / build / runtime tree is `D:/Code/swg-client-v2/` branch `koogie-msvc-cpp20-base`.** whitengold (`D:/Code/swg-client/`) is research history; no source edits there.
- **Build entry point is MSBuild on `D:/Code/swg-client-v2/src/build/win32/swg.sln` with `/p:PlatformToolset=v145 /p:Configuration=Debug /p:Platform=Win32`** — NOT CMake (Koogie did not port to CMake).
- **Static CRT (`/MTd` Debug, `/MT` Release) per Phase 9 closeout** — `Direct3d11.vcxproj` matches the existing `Direct3d9.vcxproj` runtime-library settings.
- **Don't modify Koogie's source-level changes without strong reason** — the Direct3d11 plugin is wholly NEW code, so this rule is naturally satisfied; the only engine-side edit (Graphics.cpp range-check at line 209) is additive, not modifying Koogie's diagnostic patches.
- **Source-edit budget is OPEN in v2** per memory `feedback_source_edits_allowed.md` (recorded during Phase 9 / Phase 10).
- **D3D9 plugin must continue to build and load** (D-05 + SPEC §Constraints + D3D11-01 acceptance).
- **No lost-device recovery code** (D3D11 has no equivalent; SPEC §Out-of-Scope explicit).

## Security Domain

> `security_enforcement: true`, `security_asvs_level: 1` per `.planning/config.json`. Phase 11 is a graphics-renderer port — security surface is small but non-zero (filesystem write for shader cache, dynamic library load).

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | (no auth surface in renderer plugin) |
| V3 Session Management | no | (no session state) |
| V4 Access Control | no | (no access-controlled resources) |
| V5 Input Validation | yes (low) | Shader source loaded from TRE archives is trusted (asset-pipeline contract); D-03 cache filenames must reject path-traversal before writing. |
| V6 Cryptography | no | No cryptographic operation; the shader-source SHA1 hash for cache-keying is a non-cryptographic content fingerprint, not a security control. |
| V7 Error Handling | yes | `D3DCompile` error blob and `HRESULT` failures should be logged but not surfaced as user-visible PII (none here, but principle holds). |
| V12 Files / Resources | yes | Shader cache write path must be sanitized — D-03 cache dir is `<configured>/shader-cache/<hash>.cso` where `<hash>` is hex-only and `<configured>` is engine-controlled (never user input). |

### Known Threat Patterns for D3D11 renderer plugin

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| DLL search-order hijack | Tampering / Elevation of Privilege | Use `LoadLibraryEx` with `LOAD_LIBRARY_SEARCH_DEFAULT_DIRS` (engine already does this — Phase 11 doesn't change it); `Direct3d11.dll` lives in the same staging dir as `SwgClient_d.exe`. |
| Path traversal in shader cache write | Tampering | Cache filenames are derived from a hex hash + literal `.cso` extension; never user-controlled. The cache directory itself is engine-configured, not user-configured. Validate hash is `[0-9a-f]+` before path concat. |
| `D3DCompile` parsing memory corruption | DoS / Elevation of Privilege via malicious shader source | Shader source comes from TRE archives signed by the asset pipeline. Treat shader source as trusted; `D3DCompile` is itself a hardened MS-shipped library. No additional mitigation needed for trusted asset path. |
| `DXGI_ERROR_DEVICE_REMOVED` after driver crash | DoS | FATAL with a clear message ("GPU device removed — check driver / restart client"). No recovery attempt per SPEC §Boundaries. |
| Resource exhaustion via runaway shader cache growth | DoS (local) | Cache write is bounded by the number of distinct shaders in the asset catalog (closed set). Optionally cap at N MB with LRU eviction; not strictly required for SPEC. |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The Win SDK 10.0.26100.0 D3D11 headers/libs are sufficient for D3D 11.0 feature level (no DXSDK June 2010 needed) | §Standard Stack | LOW — Win SDK 8.0+ has been the official D3D11 home for 13+ years; this is well-trodden. |
| A2 | The plugin's stage path will be `D:/Code/swg-client-v2/stage/` (where `gl05_d.dll` lives today), via a vcxproj POST_BUILD `copy` analogous to `Direct3d9.vcxproj:134` | §Runtime State Inventory + §Architecture Patterns | LOW — verified stage dir contains `gl05_d.dll` etc.; the existing `Direct3d9.vcxproj` POST_BUILD copies to `..\..\..\..\..\..\..\dev\win32\` which the plan author should resolve to actual disk path before authoring `Direct3d11.vcxproj`. |
| A3 | `TAG3(D,1,1)` macro accepts non-letter tag characters (the digit `1`) | §Code Examples | MEDIUM — should verify by reading `Tag.h` macro definition during plan authoring. If not, alternative spelling `TAG_DXB` (B = 11 in hex) is acceptable. |
| A4 | The D-03 hybrid shader-cache write to `stage/shader-cache/` is acceptable (gitignored, persistent per-machine) | §Architecture Patterns Pattern 3 + §Validation Architecture | LOW — matches the existing `stage/dpvs-profile/` (gitignored) pattern; Claude's discretion per CONTEXT. |
| A5 | NVIDIA Nsight Graphics is the right SPEC R7 capture tool given the RTX 3060 + no PIX install | §Environment Availability | MEDIUM — researcher recommendation; user should confirm tool choice during the DPVS-remeasure plan. PIX is the official SPEC suggestion; Nsight is just as valid for D3D11 on NVIDIA hardware. |
| A6 | The `Direct3d9.cpp:118` populated slots is the authoritative count of "real" Gl_api entries the engine consumes (ignoring the typedef'd CallbackFunction pseudo-slot and accounting for the `PRODUCTION==0`-gated video-capture entries) | §Summary | LOW — implementer cross-checks against `Gl_dll.def` declarations and populates every declared slot, so the count delta doesn't matter functionally. The "119 vs 120 vs 118" disagreement is bookkeeping, not contract. |
| A7 | Asset-side `.vsh` / `.psh` files are HLSL source text (NOT D3D9 binary `vs.1.1` assembly) | §Architecture Patterns + §Common Pitfalls | LOW-MEDIUM — confirmed by inspection: `Direct3d9_VertexShaderData.cpp:356 if (m_hlsl)` and the surrounding code shows the HLSL path is the primary one and the asm path is a vestige. Spike Phase A should still verify the no-HLSL fraction (probably zero) when scanning asset templates. |
| A8 | The flip-model swap chain works with the current engine-owned-window (`gl_install->engineOwnsWindow`) path | §Architecture Patterns Pattern 1 | LOW — flip model works for any HWND; the `engineOwnsWindow` distinction only affects who handles `WM_*` messages, not swap chain compatibility. |

## Open Questions

1. **`TAG_DX11` exact spelling** (per A3 above)
   - What we know: D-02a says "TAG_DX11 (or equivalent — researcher picks the cleanest seam to add the tag)." Existing pattern is `TAG3(D,X,9)` for `TAG_DX9`.
   - What's unclear: Whether `TAG3` accepts digit characters or only letters. The existing tags use `D,X,8` and `D,X,9` so digits in position 3 are clearly fine, but `D,1,1` is digit-in-position-2.
   - Recommendation: Verify by reading `engine/shared/library/sharedFoundation/include/public/sharedFoundation/Tag.h` during plugin scaffold plan authoring; if `TAG3(D,1,1)` doesn't compile, fall back to `TAG_DXB` (`TAG3(D,X,B)` — 'B' = 11 in hex notation as a mnemonic) or `TAG_DX_` (with underscore). Decision is cosmetic.

2. **Stage path resolution** (per A2 above)
   - What we know: Existing `gl05_d.dll` etc. live in `D:/Code/swg-client-v2/stage/`. `Direct3d9.vcxproj:134` POST_BUILD copies to `..\..\..\..\..\..\..\dev\win32\`. Either the relative path resolves to `stage/`, or `dev/win32/` and `stage/` are different dirs that both end up populated.
   - What's unclear: Whether the v2 build conventions copy from `dev/win32/` to `stage/` automatically, or if the runtime loads from `dev/win32/` directly.
   - Recommendation: Plan author runs `find D:/Code/swg-client-v2 -name "gl05_d.dll"` to locate every copy and confirms which one `SwgClient_d.exe` actually loads (probably the one in its own directory). Use the same convention for `gl11_d.dll`.

3. **DPVS remeasurement: external-tool vs. instrumentation-restore decision point** (per A5 + SPEC R7)
   - What we know: SPEC R7 + CONTEXT D-12 lean strongly toward external tools. PIX is suggested. Phase 10 left a recovery-anchor git tag.
   - What's unclear: Whether external-tool capture can produce data comparable enough to Phase 10's CSV format to support the same `analysis.py` aggregation, OR whether Phase 11 needs to define a fresh aggregation script for external-tool output.
   - Recommendation: First plan in the DPVS remeasure wave: do a 5-minute spike with PIX (or Nsight) on the existing D3D9 client capturing the same Mos Eisley plaza scene. If the per-frame `resolveVisibility()` cost is extractable from PIX's output, proceed external-tool-only. If not, do a `git revert <D-15-cleanup-commit>` to restore Phase 10 instrumentation as the fastest path to a directly-comparable D3D11 number.

4. **FFP spike outcome** (per D-04)
   - What we know: SWG's shader template system has a `FixedFunctionPipeline` Pass type as a sibling of `VertexShader` and `PixelShader`. The D3D9 plugin has live FFP code paths gated behind `#ifdef FFP`. The asset templates in TRE archives MAY or MAY NOT exercise FFP for Tatooine outdoor + cantina interior.
   - What's unclear: Whether the spike Phase A static analysis returns empty (FFP-free target scenes → descope) or non-empty (build the generator).
   - Recommendation: Spike is the first Phase 11 plan; outcome drives whether D3D11-04 is "build runtime ps_5_0 generator" or "descope with evidence." Both branches are independently planned.

5. **`requiresVertexAndPixelShaders()` return value under D3D11**
   - What we know: D3D9 plugin returns `false` when `FFP` is defined, `true` when `VSPS` is defined (`Direct3d9.cpp:577-584`). The engine queries this via `Gl_api::requiresVertexAndPixelShaders` (and exposes via `Graphics::requiresVertexAndPixelShaders`).
   - What's unclear: What engine-side code paths key off this, and what they do differently.
   - Recommendation: Grep `requiresVertexAndPixelShaders` across the engine during the plugin-scaffold plan; expected outcome is "the engine simply selects different shader-template fallback paths." For D3D11 the plugin should return `true` (always programmable).

## Sources

### Primary (HIGH confidence — direct file inspection or cited authoritative documentation)

- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/win32/Gl_dll.def` — 120 function-pointer declarations
- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/win32/Graphics.cpp` lines 184, 209-215, 219-220, 236, 240, 245, 277 — renderer-selection plumbing
- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp` line 501 (`extern "C" __declspec(dllexport) Gl_api const * GetApi`) + lines 985-1136 (118 `ms_glApi.<slot> = ...` assignments)
- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp:351-353` — only shader-target-string site (`vs_2_0` / `vs_1_1`)
- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/src/engine/client/application/Direct3d9/src/win32/Direct3d9_StateCache.cpp:526,561` — only `SetVertexShaderConstantF` / `SetPixelShaderConstantF` sites
- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp` D3DPOOL_MANAGED count: 4 sites total (2 in StaticIndex/VertexBufferData, 2 in TextureData)
- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj` — vcxproj template (v145 toolset, x86, /MTd, post-build copy to `dev/win32/`)
- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:902-908` — current Option-α state (line numbers diverge from SPEC's "909/913")
- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.h:262, 310-358` — FFP class hierarchy
- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp:2155-2161` — shader source loaded via `TreeFile::open` from TRE archives
- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/stage/` directory listing — confirms `gl05_d.dll`, `gl05_r.dll`, `gl06_d.dll`, `gl06_r.dll`, `gl07_d.dll`, `gl07_r.dll` already present
- `[VERIFIED 2026-05-15]` `C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um/d3d11.h`, `d3dcompiler.h` — Win SDK headers present
- `[VERIFIED 2026-05-15]` `C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x86/d3d11.lib`, `d3dcompiler.lib` — Win SDK libs present (x86 = Win32 target)
- `[VERIFIED 2026-05-15]` `C:/Windows/SysWOW64/d3dcompiler_47.dll` + `C:/Windows/System32/d3dcompiler_47.dll` — runtime DLL present (32-bit + 64-bit)
- `[VERIFIED 2026-05-15]` `D:/Code/swg-client-v2/.planning/phases/10-dpvs-culling-experiment/10-SUMMARY.md` — `phase-10-instrumentation-pre-cleanup` git tag at `9f2ec3715` is the recovery anchor for SPEC R7 instrumentation restore
- `[CITED]` learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-intro — D3D11 feature levels
- `[CITED]` learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice — `D3D11CreateDevice` reference
- `[CITED]` learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model — flip-model swap chain
- `[CITED]` learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model — flip-model rationale + rebind requirement
- `[CITED]` learn.microsoft.com/en-us/windows/uwp/gaming/port-the-shader-config — D3D9 → D3D11 shader objects + cbuffer migration
- `[CITED]` learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics — `SV_POSITION` system-value semantic
- `[CITED]` learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_query_data_timestamp_disjoint — `D3D11_QUERY_TIMESTAMP_DISJOINT`
- `[CITED]` walbourn.github.io/anatomy-of-direct3d-11-create-device — D3D11 device-creation skeleton + ComPtr + feature-level fallback + Graphics Tools optional feature
- `[CITED]` walbourn.github.io/care-and-feeding-of-modern-swapchains — flip model details + MSAA-not-supported + rebind-after-present

### Secondary (MEDIUM confidence — community sources cross-referenced)

- `[CITED]` therealmjp.github.io/posts/profiling-in-dx11-with-queries — three-query timestamp pattern with disjoint guard (cross-verified against MS Learn)
- WebSearch result (2026-05-15) — D3D9 → D3D11 FFP fixed-function emulation: confirmed there is no native D3D11 FFP path; community confirms generator-per-state-tuple is the standard approach (no library does this for SWG specifically; community scope is "every engine writes its own")

### Tertiary (LOW confidence — single source, marked for validation)

- WebSearch (2026-05-15) — `swg-engine-prototype` D3D11 reference: **NOT FOUND** publicly. There is no public community D3D11 port of SWG that this researcher could discover. Phase 11 is novel work for this codebase. (`[VERIFIED-NEGATIVE]` in the sense that the absence is verified — the spec asked to look and the answer is "doesn't exist".)

## Metadata

**Confidence breakdown:**

- Standard stack: **HIGH** — every header/lib is verified present on disk; APIs are stable Win SDK surface in use for 10+ years.
- Architecture: **HIGH** — patterns are MS-cited official guidance; the D3D9 source provides a clear contract surface; the Gl_api table is read directly from `Gl_dll.def`.
- Pitfalls: **HIGH** — the eight pitfalls listed are all canonical D3D9-to-D3D11 migration gotchas confirmed by cited sources. Pitfall 7 (DLL base address) is a v2-codebase-specific quirk verified by inspection.
- DPVS remeasurement strategy: **MEDIUM** — recovery anchor is verified, but the external-tool-vs-instrumentation tradeoff depends on Phase 11's actual capture-tool experience; flagged as Open Question 3.
- FFP spike outcome: **UNKNOWN** by design — that's what the spike is for. D-04 / D-04a structure is solid.
- TAG_DX11 exact spelling: **MEDIUM** — needs Tag.h verification during plan authoring (Open Question 1).

**Research date:** 2026-05-15
**Valid until:** ~2026-08-15 (90 days; D3D11 SDK is stable, asset-side state is stable, Win SDK 10.0.26100.0 is stable)

## RESEARCH COMPLETE
