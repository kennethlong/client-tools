FIRST: read `.planning/research/CONSULT-32-EVIDENCE-round2.md` — those are LOCKED runtime-measured axioms that FALSIFIED the earlier "it's not 32-02" round. Do not re-derive or contradict them. The leak in Phase-32's D3DCompile path is PROVEN (+144 MB/2min in an A/B isolation); your job is the mechanism, not whether it exists.

YOUR ANGLE (repo-tracer — Question A: bounded vs recompile-per-transition): The +144 MB linear accumulation requires EITHER repeated recompiles OR per-compile retention. Settle whether shaders RECOMPILE on every cantina cell transition.

Trace precisely:
- The lifetime of `Direct3d9_VertexShaderData` and its owner `ShaderImplementationPassVertexShader` across `GroundScene` / `ClientWorld` CELL load and unload (interior cantina ↔ exterior). Are these compiled-VS-holding objects DESTROYED on cell unload and RE-CREATED (=> recompiled) on the next cell load? Or are they cached per `.sht` (`ShaderTemplateList`/`StaticShaderTemplate`) and reused?
- The compile gate: `createVertexShader` (Direct3d9_VertexShaderData.cpp ~433-650), `getVertexShader` single-entry cache (`m_lastRequestedKey`/`m_lastReturnedValue` ~654-686), `m_container`, `m_nonPatchedVertexShader`. What invalidates/evicts them? Does cell transition, LOD change, or shader-template release tear the cache down so the next use recompiles?
- Count the realistic distinct (shader × textureCoordinateSetKey) set hit during a cantina in/out, and whether the SAME shaders recompile each cycle.

Deliver: a definitive verdict — do shaders recompile per cantina transition (recompile-thrash → fix = persist a bytecode cache / stop recompiling), or are compiles bounded (=> the leak is per-compile d3dcompiler-internal retention, someone else's angle)? Cite file:line for the lifetime/eviction. Repo root: D:/Code/swg-client-v2.
