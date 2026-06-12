# Phase 19 world-corruption — ROUND 2 consult SYNTHESIS

**Date:** 2026-05-31. Inputs: `CONSULT-19-d3d11-world-corruption-ROUND2-{codex,cursor}.out`.
**Context:** Round-1 BC-mip hypothesis was IMPLEMENTED and FALSIFIED. This round handed both AIs
the falsification + new evidence and asked them to break the new "timing" framing.

---

## Both AIs CONVERGED — on a specific, code-located, falsifiable mechanism

**The dynamic VB/IB shared ring buffer is the #1 suspect** (CODEX and Cursor independently):
`Direct3d11_DynamicVertexBufferData` / `Direct3d11_DynamicIndexBufferData` are **process-wide
shared rings** with **unsynchronized static accounting** (`ms_used`, `ms_newFrame`,
`ms_usedNumberOfIndices`, `m_offset`, `ms_d3dRingBuffer`) and `Map(WRITE_NO_OVERWRITE)` semantics.

CODEX's framing (the correction to my "context race"): the surviving mechanism is **not** naive
"D3D11 context calls race." It is **"engine-level graphics TRANSACTIONS race while D3D11
serializes only individual COM calls."** The runtime MT-lock makes each `Map`/`Unmap`/`Set*`/`Draw`
atomic, but does NOT protect:
- the static ring accounting (`ms_used += length`), or
- the logical transaction `lock → write → unlock → draw` as a unit, or
- the GPU lifetime of `NO_OVERWRITE` sub-ranges.

Fits the evidence better than the mip theory ever did: **geometry shards** = garbage verts/indices
or offsets; distance/LOD = far draws add ring pressure → more `NO_OVERWRITE` wraps → bigger hazard
surface; flicker/latch = probabilistic GPU-vs-CPU timing; RenderDoc-clean = slower replay scheduling
lets each draw finish before the next Map overwrites its slice; immune to GPU-finish = the hazard is
INTRA-frame, not inter-frame; D3D9-fine = D3D9's coarse `Lock`/managed semantics accidentally
serialized what D3D11's explicit `NO_OVERWRITE` promise does not.

## Caveats both AIs insisted on (do NOT let "Heisenbug" become the default)

- **Capture/flicker bias (branch A):** the 6 clean `.rdc` may simply be clean FRAMES. "During a
  corrupt roam" ≠ "a frame that was corrupt at the presented backbuffer." Catch a LATCHED-corrupt
  surface.
- **Display ≠ backbuffer (branch B):** rendered backbuffer correct, scanout/compositor/flip/MPO wrong.
  Note: F12 `screenShot_impl` does `CopyResource` from the SAME backbuffer the capture exports — so
  corrupt-F12 + clean-capture-Present of the *same frame* would be a contradiction → points at A or
  methodology, not B. (B only survives if the corrupt images are off-monitor, not F12.)
- Non-timing survivors still consistent with tests 1–8: **bind-before-upload on USAGE_DEFAULT
  textures** (Cursor 1c — leak test doesn't init them; RenderDoc serializes post-upload); **partial
  `Map(WRITE_DISCARD)` PS cbuffer** (Cursor 1b — TEST 1 only measured the 400B legacy b0 at
  StaticShaderData flush, NOT the 16B `setFog`/`updatePS(0,...)` path → solid terrain colors fit
  undefined PS constants); format-mismatch lock path.

## Decisive tests, ranked (cheapest falsifier first)

1. **[DONE — built] Thread-audit smoking gun (CODEX):** log every dynamic VB/IB ring lock that runs
   OFF the main thread (`!Os::isMainThread()`). Fires → off-thread graphics transaction confirmed.
   Never fires across a corrupt roam → threading hypothesis dead too. `P19_THREAD_AUDIT` in both
   `Direct3d11_Dynamic{Vertex,Index}BufferData.cpp`; logs `[P19THREAD]` to `stage/d3d11-debug.log`.
2. **DISCARD-only ring (Cursor #1):** force `D3D11_MAP_WRITE_DISCARD` on every dynamic VB/IB lock
   (disable `NO_OVERWRITE`). Corruption vanishes → intra-frame ring hazard confirmed (even if audit
   was thread-clean → it's an accounting/in-flight hazard, not threads).
3. **Pre-Present internal readback (CODEX, methodology-killer):** `CopyResource` backbuffer → staging
   right before `Present`, dump PNG/hash, WITHOUT RenderDoc attached. Doesn't perturb timing → should
   CATCH the corruption RenderDoc masks. Corrupt internal readback → it's renderer/resource/state
   (kills branch B). Clean internal readback while screen corrupt → display/compositor (branch B).
4. **MaxLOD=0 (Cursor #2):** the Round-1 "decisive test" that was NEVER actually run. 1-line sampler
   clamp. Distant texture corruption gone → mip sampling implicated for the texture part.
5. **`0xCD` sentinel-fill on CreateTexture2D DEFAULT textures:** corruption becomes solid sentinel
   blocks → bind-before-upload (1c) wins over timing.

## Interpretation matrix for the thread-audit (test 1, now staged)

| `[P19THREAD]` log | Conclusion | Next |
|---|---|---|
| Fires (off-main-thread ring locks) | **Engine-transaction race CONFIRMED** | Lock the lock→unlock→draw transaction to the render thread / serialize ring access |
| Silent across a corrupt roam | Threading dead → it's intra-frame ring hazard OR non-timing | test 2 (DISCARD-only), then test 3 (readback) |
