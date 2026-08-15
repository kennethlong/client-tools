# CONSULT-74 / Codex — D3D11 renderer architecture, side by side

READ FIRST (locked facts, do not re-derive):
  D:\Code\swg-client-v2\.planning\research\CONSULT-74-EVIDENCE-two-fork-comparison.md

Two independent D3D11 renderer plugins for the same 2004 engine, both at
src/engine/client/application/Direct3d11/, both selected by rasterMajor=11.

  Repo A (57 new files): D:\Code\swg-client-v2
  Repo B (88 new files): D:\Code\Galaxies-Reborn\client-tools  (branch origin/x64-dx11-vanilla)

YOUR JOB — structural tracing, not opinion. Produce a factual architecture comparison:

1. DEVICE/CONTEXT MODEL. How each creates and owns the device, swap chain, immediate context,
   render targets, depth-stencil. Deferred contexts used at all? Device-loss/resize handling.
2. STATE MANAGEMENT. How each maps the engine's legacy state calls onto D3D11 state objects
   (blend/raster/depth-stencil/sampler). Caching/dedup strategy, hash keys, lifetime.
3. SHADER SYSTEM. How each gets HLSL/asm shaders to bytecode: compile path (D3DCompile?),
   the fixed-function-emulation strategy, whether shaders are generated/permuted, on-disk
   caching, and how the engine's shader-template system is bridged.
4. CONSTANT BUFFERS. Layout, update discipline (Map DISCARD vs NO_OVERWRITE vs UpdateSubresource),
   register mapping from the engine's constant-register model, matrix transpose handling.
5. VERTEX/INDEX BUFFERS. Static vs dynamic, CPU shadow copies (if any), locking discipline,
   the input-layout/vertex-declaration bridge.
6. TEXTURES. Format mapping and conversions, mip/DDS handling, SRV binding and stage->slot mapping.

For each of the six: state what A does, what B does, and name the DIVERGENCES explicitly.
Where one has a subsystem the other lacks entirely, say so.

Cite file:line. End with a table: subsystem | A approach | B approach | divergent? | which is more
complete on evidence (or "insufficient evidence").
