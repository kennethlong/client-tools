# CONSULT-51 — gl11 JTL space jump-to-lightspeed crash (NV driver null-deref in Map)

You are one of four independent consultants. Treat the LOCKED axioms below as measured ground truth —
**do not re-derive or dispute them**. They come from crash-dump analysis (cdb) of the exact failing
build. Answer ONLY your assigned angle (bottom). Convergence-from-divergence is the goal.

## CONTEXT (one paragraph)
Star Wars Galaxies client, SOE 2004 C++ engine, modern MSBuild fork. Two D3D renderer plugins built
from one engine: `gl05` = D3D9, `gl11` = D3D11 (our port). Selected at runtime by a cfg key. We are in
JTL "space" (player flies a ship). The player triggers the **jump-to-lightspeed (hyperspace) cutscene**.

## LOCKED AXIOMS (measured — do not dispute)
1. **gl05 (D3D9) flies into space AND survives the jump-to-lightspeed cutscene FINE, repeatedly.**
   **gl11 (D3D11) crashes at the jump cutscene every time (now very consistent).** → the defect is
   **gl11/D3D11-path specific**, NOT generic, NOT the scene data, NOT a chronic driver bug.
2. **gl11 renders GROUND scenes (cantina, Tatooine surface) crash-free.** Only the SPACE
   jump-cutscene path crashes. So the *same* D3D11 cbuffer/light code runs fine elsewhere.
3. The crash is a **null-deref in the NVIDIA D3D11 user-mode driver** `nvwgf2um.dll`
   (version **32.0.16.1062**, current):
   ```
   nvwgf2um!NVAPI_DirectMethods+0x9da4c:
     5fbc631c 8b00   mov eax,dword ptr [eax]   ds:002b:00000000=????????    ; eax=0
   stack (faulting thread — a DRIVER WORKER thread, 0 engine frames):
     nvwgf2um!NVAPI_DirectMethods+0x9da4c
     nvwgf2um!NVENCODEAPI_Thunk+0x9d7e2
     nvwgf2um!SetDependencyInfo+0x5474c
     nvwgf2um!SetDependencyInfo+0x5461f
     nvwgf2um!NVDEV_Thunk+0x1558e5
     nvwgf2um!NVDEV_Thunk+0x4e1306
     ntdll (thread start)
   ```
4. **The TRIGGER is on the MAIN render thread** (same crash event, captured in the full WER dump). The
   main thread is *inside an `ID3D11DeviceContext::Map(WRITE_DISCARD)` call* when the driver faults:
   ```
   nvwgf2um!SetDependencyInfo+0x55928        ; driver, servicing the Map
   nvwgf2um!NVENCODEAPI_Thunk+0x2179f3
   nvwgf2um!OpenAdapter10+0x7299d
   nvwgf2um!OpenAdapter10+0x74cdd
   gl11_r!Direct3d11_ConstantBuffer::updateVS+0x35   (Direct3d11_ConstantBuffer.cpp:177  <-- the Map)
   gl11_r!Direct3d11_LightManager::setLights+0x9de   (Direct3d11_LightManager.cpp:355)
   SwgClient_r!ShaderPrimitiveSorter::Phase::draw    (ShaderPrimitiveSorter.cpp:725)
   SwgClient_r!ShaderPrimitiveSorter::Phase::popCell (860) -> ::popCell (1413)
   SwgClient_r!RenderWorld::drawScene                (RenderWorld.cpp:1052)
   SwgClient_r!RenderWorldCamera::drawScene
   SwgClient_r!DebugPortalCamera::drawScene          (DebugPortalCamera.cpp:333)   <-- cutscene camera
   ```
   So: `setLights` → `updateVS` → `Map(lighting cbuffer, WRITE_DISCARD)` on the immediate context, during
   the hyperspace cutscene scene draw, drives the NV driver into a null-deref. (There are ~12 parked
   `nvwgf2um!NVDEV_Thunk → ntdll` worker threads — the driver's internal worker pool. NVIDIA control-panel
   "Threaded Optimization = Off" is set; the UMD still has this internal pool.)
5. **The 32-bit Win32 Release client** (`SwgClient_r.exe` + `gl11_r.dll`). 2 GB user address space.

## RULED OUT (do not propose these — already disproven this session)
- NOT our recent source changes — a pure warm baseline (no diagnostics, original config) still crashes.
- NOT a shader-recompile burst — warm shader cache (no recompiles) still crashes.
- NOT the new driver (it IS 32.0.16.1062), NOT control-panel Threaded-Optimization (already off),
  NOT a RenderDoc artifact, NOT off-thread D3D resource creation (GPU resources are created on the main
  thread; the async loader only warms the file cache).
- NOT bad scene data / wrong server TRE (those give wrong visuals or a load FATAL, not a driver AV).
- **NOT an OOB from our cbuffer code** (checked this session): all cbuffer slots are created with
  `ByteWidth = kMaxCBufferBytes = 1152`; the `P19_CBUF_ZERO_TAIL` `memset(mapped.pData, 0, 1152)`
  writes exactly 1152 into a 1152-byte buffer; `sizeof(Direct3d11_LightingCB)` ≈ 320 ≪ 1152. So the
  Map/memset/memcpy/Unmap is in-bounds and the buffer is correctly sized.

## THE CODE (exact, current)

`Direct3d11_ConstantBuffer.cpp` — the buffer + the faulting Map:
```cpp
// install(): all VS+PS slots created identically:
desc.ByteWidth      = kMaxCBufferBytes;          // 1152
desc.Usage          = D3D11_USAGE_DYNAMIC;
desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
CreateBuffer(&desc, nullptr, out.GetAddressOf());

void Direct3d11_ConstantBuffer::updateVS(int slot, void const *data, size_t sizeBytes) {
    // slot/size DEBUG_FATAL bounds checks, NOT_NULL(data), NOT_NULL(ms_vsBuffers[slot].Get());
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = Direct3d11_Device::getContext()->Map(
        ms_vsBuffers[slot].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);   // <-- line 177, faults here
    FATAL_DX_HR(... hr);
    std::memset(mapped.pData, 0, kMaxCBufferBytes);     // P19_CBUF_ZERO_TAIL (1152 into 1152)
    std::memcpy(mapped.pData, data, sizeBytes);
    Direct3d11_Device::getContext()->Unmap(ms_vsBuffers[slot].Get(), 0);
}
```

`Direct3d11_LightManager::setLights(lightList)` (line 355 is the call site):
- Builds a stack `Direct3d11_LightingCB cb` (~320 B): ambient, ONE directional, up to 8 point lights
  (`if (pointLightCount < 8)` guarded), a point-light count. Selection cascade over `lightList`.
- Then at line 354-357:
  ```cpp
  Direct3d11_ConstantBuffer::updateVS(kLightingCBSlot, &cb, sizeof(cb));   // kLightingCBSlot = 3
  Direct3d11_ConstantBuffer::bindVS(kLightingCBSlot);
  Direct3d11_ConstantBuffer::updatePS(kLightingCBSlot, &cb, sizeof(cb));
  Direct3d11_ConstantBuffer::bindPS(kLightingCBSlot);
  ```
- `setLights` is called by `ShaderPrimitiveSorter::Phase::draw` **per draw entry / per popCell** — i.e.
  potentially MANY times per frame (once per cell/object batch).

## OPEN QUESTION
Why does `Map(WRITE_DISCARD)` on a correctly-sized DYNAMIC constant buffer, issued from `setLights`
during the **space jump-to-lightspeed cutscene**, drive the NV D3D11 driver to null-deref — when the
identical code renders ground scenes and the gl05/D3D9 path flies the same cutscene fine?

## YOUR ASSIGNED ANGLE
<<INSERTED PER CONSULTANT BELOW>>
