# CODEX Consult Round 4 — Engine Pushes NEITHER Matrices NOR User-Constants

**Date:** 2026-05-21 (continuing same Iter-2 diagnostic arc)
**Predecessors:** Round 1 (magenta-missing), Round 2 (pipeline-stats narrowing), Round 3 (dead user-constants → Fix C prescription)

---

## Round 3 fix landed cleanly

Iter-2.7a implements Fix C exactly as you prescribed:

| Component | Status |
|---|---|
| Namespace-scope `Direct3d11_VertexSlot0CB s_slot0Shadow` + `s_slot0Dirty` flag in `StateCache.cpp` | ✓ |
| `composeAndUploadSlot0` renamed `composeSlot0Shadow`: writes to shadow + marks dirty (no GPU upload) | ✓ |
| `flushSlot0IfDirty()`: lazy upload-once-per-draw via `updateVS(0, &s_slot0Shadow, sizeof(s_slot0Shadow))` | ✓ |
| `applyPreDrawState` top → `flushSlot0IfDirty()` (replaces Plan 11-08's first-draw race guard) | ✓ |
| Plan 11-08 matrix setters (setWorldToCameraTransform, setProjectionMatrix, setObjectToWorldTransformAndScale) all call `composeSlot0Shadow` after their cached writes | ✓ |
| `Direct3d11_StateCache::setVSConstants(register, data, count)` public method (thunks to namespace `setVSConstantsRaw`) | ✓ |
| `setVertexShaderUserConstants_impl` in `Direct3d11.cpp`: routes to `setVSConstants(VCSR_userConstant0 + index, data, 1)` where `VCSR_userConstant0 = 52` (verified against `Direct3d9_VertexShaderConstantRegisters.h:37` and `Direct3d9.cpp:3497`) | ✓ |
| Shadow initialized to identity at `StateCache::install` to match `primeDefaults` GPU state | ✓ |
| Iter-2.X probe scaffolding kept in place per Kenny's call (strip only after magenta confirmed) | ✓ |
| Build: MSBuild Direct3d11 EXIT=0 first attempt | ✓ |

## Iter-2.7a smoke result (unchanged from Iter-2.6)

Re-ran the smoke. Probe output:

```
[Iter-2.1 probe] install entered + compile=true + ms_fallbackPS valid
[Iter-2.5 probe] composeAndUploadSlot0 call #1 (s_anyMatrixWritten=0 before this call)
  View / Proj / World / WVP = IDENTITY (only the install-time compose ran)
  CameraPos = (0,0,0,0)
[Iter-2.1 probe] applyPreDrawState first FALLBACK PS bind: fallback=<valid>, drawCallCount=0
  RS/DSS/BS = canonical, viewport=1024x768, scissor disabled, IL valid, VB stride=24 vertexCount=4
[Iter-2.2 probe] PIPELINE_STATISTICS results (10 draws bracketed):
  IAVertices: 40, IAPrimitives: 10, VSInvocations: 30
  CInvocations: 10, CPrimitives: 10, PSInvocations: 0
  Skip counters: NullVS=0, NullLayout=0, totalDrawCallCount=11
```

**NO `[Iter-2.7 probe]` lines appeared.** That means:
- `setVertexShaderUserConstants` is NEVER called by the engine
- `flushSlot0IfDirty` never has anything to flush (no setter is touching the shadow)
- The shadow stays at install-time identity (matching the GPU's primeDefaults upload)

## Net new finding

In Round 3 we proved Plan 11-08's matrix setters were never called. We hypothesized the engine would use `setVertexShaderUserConstants` instead (D3D9 `SetVertexShaderConstantF` analog at register 52). **It doesn't.** The engine is pushing constants through some OTHER mechanism — or not at all.

Yet 11 draws still happen, VS runs, primitives reach the rasterizer, survive clip + cull, and produce zero fragments.

## What we've eliminated

| Suspect | Status | Evidence |
|---|---|---|
| Fallback PS not bound | RULED OUT | Probe shows fallback bound on first draw |
| Cull mode | RULED OUT | CPrimitives = CInvocations |
| Clip rejection | RULED OUT | CPrimitives = CInvocations |
| Depth test (early-Z) | RULED OUT | DepthEnable=FALSE diagnostic, same PSInvocations=0 |
| Viewport / scissor | RULED OUT | State probe: full window, scissor disabled |
| IL resolution | RULED OUT | State probe: ms_currentInputLayout non-null |
| Topology mismatch | PARTIALLY RULED OUT | 4-verts-1-prim signature is consistent with `drawTriangleList(4 verts)` — D3D9 would produce the same |
| WVP-via-matrix-setters | RULED OUT | Setters never called |
| WVP-via-user-constants at c52..c59 | RULED OUT | setVertexShaderUserConstants never called |
| Plan 11-05 dead-static user-constants bug | FIXED | Fix C routes to GPU; no behavior change in smoke |

## Verified context from the codebase

- `Camera::applyState` (`Camera.cpp:637-638`) is the canonical call site for `Graphics::setWorldToCameraTransform` + `setProjectionMatrix`. **Login screen UI rendering apparently doesn't go through Camera::applyState.**
- Other call sites of these setters: `BinkVideo.cpp`, `DebugPrimitive.cpp`, `IndexedTriangleListShaderPrimitive.cpp`, `ShaderPrimitiveSet.cpp`, `ShaderPrimitiveSorter.cpp`, `SimplePolyPrimitive.cpp`. None evidently invoked in this scene.
- `Graphics::ms_api` IS our `Direct3d11_StateCache::*` function pointers — verified via wiring at `Direct3d11.cpp:654-657`.
- The 10 draws are non-indexed `drawTriangleList(4 verts)` → exactly the signature of UI quads being submitted as TRIANGLELIST with the 4th vertex dropped. This is the same behavior the D3D9 plugin would exhibit (`SOE` does this as a UI rendering convention — quads drawn as 1 triangle of "test/marker" geometry, or there's a path that submits 6 verts that we're not seeing).

## Hypotheses

**Hypothesis A — Engine UI subsystem uses a pre-projected coordinate convention.** UI vertices are submitted in clip space `[-1,+1]` directly. With our IDENTITY WVP, VS produces `mul(in_pos, identity) = in_pos` → output IS in clip space → CPrimitives counts them → but the triangle is degenerate or sub-pixel for some other reason. Pretty unlikely given the 4-verts-1-prim batch pattern; a clip-space quad would still produce a visible triangle.

**Hypothesis B — Engine UI VS reads constants from cbuffer slot we haven't populated, distinct from slot 0.** Plan 11-08 + Plan 11-05's wiring focused on slots 0, 1, 2. There may be a slot 3+ for UI-specific data. The engine's VS bytecode reads from there → finds primeDefaults zeros → output is degenerate.

**Hypothesis C — The HlslRewrite produces broken VS bytecode that runs but produces nonsense.** Plan 11-07's Rule D wraps c-registers into `cbuffer ... : register(b0)` with `packoffset(cN)`. If the rewrite is incorrect (e.g., missing some register declaration), the VS reads garbage. CPrimitives could still count primitives that have at-least-one-vertex-in-clip while the rest are NaN.

**Hypothesis D — Engine pushes constants through a Gl_api slot we haven't grepped for.** Maybe there's `setVertexShaderConstants` (without "User") that we don't bind in `Direct3d11.cpp`. Or some other slot like `pushShaderConstants`. The default-stub binding from scaffold_fatal_stub would no-op silently in release / DEBUG_REPORT_LOG_PRINT in debug — invisible without instrumentation.

**Hypothesis E — A different `Direct3d11_StateCache::*` slot IS being called and ITS implementation is broken** (similar to the user-constants dead-static bug). E.g., `setAlphaFadeOpacity_impl`, `setFog`, `setTextureTransform`, or something else writes to a function-local-static that's never read.

## Questions for CODEX

**Q1.** What's the typical SOE engine UI/HUD rendering matrix path? Does the login-screen UI go through `Camera::applyState`, `ShaderPrimitiveSorter`, `SimplePolyPrimitive`, a 2D UI subsystem (`UIPage`/`UICanvas`/`UI*`), or something else entirely? Where in the engine code should I grep to find the actual matrix-push call site for these 10 quad draws?

**Q2.** Beyond `setVertexShaderUserConstants`, are there other Gl_api slots used to push raw shader constants? Specifically: is there a `setVertexShaderConstants(register, data, count)` slot (no "User") that we should have bound?

**Q3.** If you had to guess what the engine VS HLSL reads c0..c71 from given (a) we've verified Rule D wraps everything into `cbuffer SwgVertexConstants : register(b0)` + `packoffset(cN)`, and (b) we've now populated slot 0 with primeDefaults identity + Fix C dirty-flush wiring — what's the most likely mechanism for clip-space output to come out degenerate?

**Q4.** Given the evidence that ALL of Plan 11-08's matrix wiring + Plan 11-05's user-constants wiring is unused by the engine in the login-screen scene, AND geometry still mysteriously renders degenerate — is there a strong hypothesis you'd offer beyond A/B/C/D/E above?

**Q5.** Best next diagnostic? Options I'm considering:
- (a) Dump VB bytes for the first draw (capture vertex positions verbatim)
- (b) Inject known-good fullscreen clip-space triangle via separate VS+VB+IL (CODEX Round 1's "cheaper than stream-out" suggestion)
- (c) Look at the compiled VS bytecode via D3DReflect or by capturing the .cso file the cache wrote
- (d) Add probes to ALL other Gl_api slot implementations to find the one that IS being called for these UI draws
- (e) Trace the engine call stack at the first applyPreDrawState (would need debugger or stack-walk instrumentation)

## What's NOT broken

- D3D11 device, swap chain, RTV, DSV — working (Plan 11-07 dark-blue clear is visible)
- Shader install / VS compile / IL resolution — working (probes confirm)
- Fallback PS install + bind — working (Iter-2 + Iter-2.1 confirmed)
- Primitive submission to GPU — working (CPrimitives=10)

## Branch/commit state

- HEAD: `71a9869f0` (last committed: archive of morning's `.continue-here.md`)
- Iter-2 work uncommitted in working tree: ~6 sub-iterations of probes + Iter-2.7a Fix C
- Will commit only the final working fix per your Q7 from Round 3 (with probes stripped)

---
