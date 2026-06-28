You are a repo tracer / call-graph analyst on the codebase at D:\Code\swg-client-v2 (a modern MSVC
v145 / C++20 rebuild of 2003 SWG client source). READ-ONLY. Be concrete: cite file:line.

## Context (treat as GIVEN — measured this session, do NOT re-derive or re-litigate)
- Zoning into the Mos Eisley cantina interior produces visible per-frame hitches ("jerk") during the
  load window in our client. Reproduces on ALL renderers (gl05/06/07 D3D9, gl11 D3D11) and BOTH
  bitness, Debug+Release. It is a per-frame TIME stall, not OOM (memory plateaus ~1.5 GB).
- The retail-derived SWGEmu client zoning into the same cantina is MUCH smoother. Retail ships
  precompiled shader bytecode.
- CONFIRMED: our //hlsl vertex shaders compile at RUNTIME via D3DCompile on the main thread during
  zone-in (Direct3d9_VertexShaderData.cpp::createVertexShader ~line 684). Pixel shaders are precompiled.
- CONFIRMED: an in-memory VS bytecode cache exists (eliminates re-entry recompiles, not first load).
- Player movement is dt-scaled and run speed is server-driven; NOT in scope.

## Your angle (stay on THIS; other consultants cover shader-cache design, allocator-swap, and fix
## architecture — do not duplicate them)
Trace what executes **synchronously on the main / render thread** during a single cantina cell
zone-in. Specifically:
1. The zone-in / cell-load call path: from the network "you have entered" / cell creation down through
   object instantiation → Appearance/Mesh/Skeleton/Texture/Shader creation. Cite the driving loop and
   the per-object creation call sites.
2. Is asset **I/O + decode + GPU-resource creation** done on the **main thread**, or on a worker /
   background loader thread? Does a background/streaming loader exist in this codebase, and is it
   actually used for zone-in object/appearance/texture loads, or only for some asset classes? Name the
   thread(s) and the queue/dispatch mechanism if any.
3. The top BLOCKING call sites on the main thread during zone-in, ranked by likely cost (shader
   compile, texture create/upload, mesh/skeleton load, file decompression, allocation).
4. Is SOE's MemoryManager (sharedMemoryManager/MemoryManager.cpp free-list-tree allocator) in the
   per-object zone-in hot path? Is it a real pooling allocator or effectively a malloc wrapper?

Deliver: a concise call-graph (file:line) of the synchronous zone-in path, a clear verdict on
sync-vs-async asset loading, and the ranked blocking call sites. Flag anything that looks like it was
synchronous-on-main-thread in our build but would have been backgrounded in retail.
