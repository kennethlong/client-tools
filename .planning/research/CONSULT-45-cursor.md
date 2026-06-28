You are the most detailed code reader on the crew. Codebase: D:\Code\swg-client-v2 (modern MSVC v145 /
C++20 rebuild of 2003 SWG client source). READ-ONLY. Cite exact file:line.

## Context (treat as GIVEN — measured this session, do NOT re-derive)
- Zoning into the Mos Eisley cantina produces per-frame hitches ("jerk") on first load in our client;
  the retail-derived SWGEmu client is much smoother. It is a main-thread TIME stall, not OOM.
- CONFIRMED: our //hlsl vertex shaders compile at RUNTIME via D3DCompile on the main thread during
  zone-in: src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp,
  createVertexShader (~line 684; ~44 compiles first cantina load). Pixel shaders are precompiled
  (Direct3d9_PixelShaderProgramData uses m_exe bytecode).
- CONFIRMED: a process-global IN-MEMORY VS bytecode cache exists (createVertexShader: it builds a key
  from (.vsh filename, textureCoordinateSetKey, target) and stores the compiled blob bytes). It fixes
  RE-ENTRY recompiles but NOT first-ever load, and is NOT persisted to disk.

## Your angle (stay on THIS; others cover the sync-vs-async load trace, allocator swap, and fix
## architecture — do not duplicate)
Two detailed deliverables:

A) **Per-object GPU-resource creation during zone-in, file:line.** Map what gets created per object on
   first load: vertex-shader create (the runtime compile), pixel-shader create, texture create+upload,
   vertex/index buffer create. For each, state whether it is lazy-on-first-use vs preloaded, and which
   are the heaviest. Look at Direct3d9_VertexShaderData, Direct3d9_TextureData, the static VB/IB data
   classes, and ShaderImplementation. Quantify (counts/sizes) where the code makes it possible.

B) **Concrete design for a DISK-PERSISTED vertex-shader bytecode cache** that extends the existing
   in-memory cache so the FIRST-ever cantina load is also a cache hit (what retail/SWGEmu effectively
   ship). Specify: where exactly to hook read/store in createVertexShader; the on-disk file/layout
   (one .cso per key vs a single pack); EXACTLY what the cache key must encode to be correct given the
   textureCoordinateSetKey permutation system (so a stale/incorrect bytecode is never loaded); cache
   invalidation when a .vsh source or the compiler version changes; and where the file should live
   (writable dir; note stage/ layout). Call out correctness traps (the texcoordKey permutations, the
   vs_2_0 target promotion, the HlslRewrite/define macro inputs that affect emitted bytecode).

Deliver tight, file:line-grounded findings and a concrete, implementable cache design.
