# CONSULT-20 — D3D11 dynamic VB ring `ms_used` accounting (cape/garment "black spikes")

**Date:** 2026-06-03
**Repo:** `swg-client-v2`, branch `koogie-msvc-cpp20-base` (main checkout `D:\Code\swg-client-v2`)
**Renderer:** in-house D3D11 plugin (`gl11`) reimplementing the legacy SWG `Gl_api`; D3D9 plugin (`gl05`) is the working reference.

> **Reviewer instruction (READ FIRST):** Our repeated failure mode on this project is all three AIs converging on the same wrong hypothesis. Do NOT rubber-stamp the analysis below. Attack it independently. If the proposed root cause is right but the fix is wrong, say so and give the correct fix. If the root cause itself is wrong, say that. Cite specific file:line. Prefer the D3D9 reference behavior as ground truth.

---

## 1. Symptom

Under D3D11 (`rasterMajor=11`, `gl11`), robed/garment/skinned characters (robes, dresses, stormtrooper hands) show **sharp dark triangular "spikes"** — a few skinned-mesh vertices flung to garbage positions. The **same character renders perfectly clean under D3D9** (`rasterMajor=5`, `gl05`). The tearing **scales with scene complexity** (a busy cantina full of NPCs is far worse than one character) and **flickers frame-to-frame**.

**Capture-invisible:** a RenderDoc capture of a spiky frame **replays clean** — strongly implying a CPU/GPU async hazard that RenderDoc serializes away, OR data that is undefined live but zero/stable on replay.

## 2. Architecture (established by code trace, all file:line verified)

SWG **CPU-skins** skinned meshes (`SoftwareBlendSkeletalShaderPrimitive::prepareToDraw`, `clientSkeletalAnimation/.../SoftwareBlendSkeletalShaderPrimitive.cpp:483`). Multi-stream is **active** (`gl11` caps: maxStreamCount=2, supportsStreamOffsets=1). Skinned output is written into a **`DynamicVertexBuffer`** (`m_dynamicStream`) which is backed by a **single process-wide shared ring** `ID3D11Buffer` (`Direct3d11_DynamicVertexBufferData`, 2 MB, `D3D11_USAGE_DYNAMIC`).

Ring lock/unlock model (`Direct3d11_DynamicVertexBufferData.cpp`):
- `lock(n, forceDiscard)`: `roundUpUsed()`; if `ms_newFrame || forceDiscard || ms_used+length > ms_size` → `Map(WRITE_DISCARD)`, `ms_used=0`; else `Map(WRITE_NO_OVERWRITE)`. Slice starts at byte `ms_used`; `m_offset = ms_used/vertexSize` (vertex index); returns `mapped.pData + ms_used`.
- Bind (multi-stream `Direct3d11_VertexBufferVectorData.cpp:145`): dynamic stream IA byte-offset = `getOffset()*stride` = `m_offset*vertexSize`. Static stream offset 0. `BaseVertexLocation = 0` (skeletal draw routes `Graphics::drawIndexedTriangleList(0,minVtx,numVtx,startIndex,primCount)` → `Graphics.cpp:3315` → `drawPartialIndexedTriangleList` → `DrawIndexed(primCount*3, startIndex, baseIndex=0)`). Verified: write-byte == IA-read-byte for every lock (no offset divergence).

## 3. Root-cause finding (from a per-lock ring trace)

We instrumented `lock()` to log per-lock: discard/wrap flags, write-byte, IA-read-byte, per-frame lock/discard counts. In a busy cantina (cap 400 locks, all within one frame):

- **370 / 400 locks were `WRAP=1`** (a mid-frame `WRITE_DISCARD` fired because `ms_used+length > ms_size`), `discardsThisFrame` climbed to **371**. The shared ring is being **renamed dozens of times per frame**.
- Every wrap is driven by a consumer locking **`numVerts ≈ 74894`, `length ≈ 2 MB`, `vtxSize=28`** — i.e. it locks **`getNumberOfLockableDynamicVertices()` (the entire remaining ring)** but only actually uses a few verts per lock.
- `MISMATCH=0` everywhere (write-byte always equals IA-read-byte — offset math is correct).

**The "lock-all-remaining" consumer is `CuiLayerRenderer`** (UI quad batcher), `clientUserInterface/.../CuiLayerRenderer.cpp:62-68`:
```cpp
s_maxNumberOfVertices = s_vertexBuffer->getNumberOfLockableDynamicVertices(false); // remaining
if (s_maxNumberOfVertices < 128)
    s_maxNumberOfVertices = s_vertexBuffer->getNumberOfLockableDynamicVertices(true); // whole ring
s_vertexBuffer->lock(s_maxNumberOfVertices);   // locks ALL remaining; writes few quads, unlocks actual
```
It locks the whole remaining ring once, accumulates quads across many `render()` calls, then `flushRenderQueue()` draws + unlocks the **actual** count.

### The D3D11-vs-D3D9 divergence

**D3D9** (`Direct3d9_DynamicVertexBufferData.cpp`, reference, WORKS):
- `lock()` (line ~172-217): does **NOT** advance `ms_used`. Sets `m_offset = ms_used/vertexSize`. `Lock(ms_used, length, ...)`.
- `unlock(numberOfVertices)` (line 244): **`ms_used += numberOfVertices*vertexSize`** — advances by the **ACTUAL used** count.

**D3D11** (`Direct3d11_DynamicVertexBufferData.cpp`, original, BUGGY):
- `lock()`: **`ms_used += length`** — advances by the **UPPER-BOUND locked** count (the whole 74894-vert ring for `CuiLayerRenderer`).
- `unlock()`: **does nothing** to `ms_used` (an explicit comment documents the divergence and rationalizes it).

⇒ A single `CuiLayerRenderer` lock of "all remaining" maxes `ms_used` to 2 MB even though it draws a handful of quads. Every subsequent lock then wraps → the shared ring is renamed mid-frame → **deferred-draw skinned meshes read a stale rename → spikes.** Capture-invisible because RenderDoc serializes the CPU/GPU so the rename race disappears. Scales with scene complexity (more meshes deferred across more wraps).

Corroborating experiments:
- `P19_VB_ZERO_TAIL` (memset the renamed ring to 0 on `WRITE_DISCARD`): spikes **relocated to object origin (the feet/floor)** instead of random garbage, and flickered less — i.e. the bad verts read *unwritten ring bytes*, consistent with a rename/recycle hazard.
- `P19_DISCARD_ONLY` (force `WRITE_DISCARD` every lock): **largely killed the spikes but dropped whole meshes** (stormtrooper hands) — confirms (a) the append model is *required* (meshes coexist at different offsets, drawn deferred), and (b) the corruption lives in the non-zero-offset path.

## 4. Attempted fix (D3D9-faithful) — MADE IT WORSE

We mirrored D3D9 exactly:
- `lock()`: removed `ms_used += length`.
- `unlock(int numberOfVertices)`: added `ms_used = (m_offset + numberOfVertices) * vertexSize;`

Result (re-ran the trace): **`WRAP=0`, `discardsThisFrame=1`** — the ring accounting is now correct/D3D9-faithful and the ring no longer thrashes within the first 400 locks. **But the visuals got dramatically WORSE** — full-screen flickering black triangles, not just robe spikes.

Our interpretation: the old over-charge was *masking* a second problem. With correct accounting, `getNumberOfLockableDynamicVertices(false)` now returns ~74000 (was a tiny leftover), so `CuiLayerRenderer` locks a huge span via `WRITE_NO_OVERWRITE` (instead of being forced into a fresh discard-rename). Something in the deferred-draw vs `NO_OVERWRITE`/rename interaction then corrupts more geometry than before.

## 5. Questions for the reviewer

1. **Why does the D3D9-faithful `ms_used` accounting fix break D3D11**, when D3D9 uses the identical accounting and works? What is the D3D11-specific difference in `WRITE_NO_OVERWRITE` + deferred-draw + ring-rename semantics that the D3D9 path does not have? (Consider: D3D9 `DrawIndexedPrimitive` with `BaseVertexIndex=m_offset` and byteOffset=0 vs D3D11 `IASetVertexBuffers(byteOffset=m_offset*stride)` + `DrawIndexed(BaseVertexLocation=0)`; whether the engine issues draws immediately in `applyPreDrawState` or defers them; whether a mid-frame `WRITE_DISCARD` rename is honored for already-issued D3D11 draws the same way D3D9 honors `D3DLOCK_DISCARD`.)
2. **Is the real bug that the 2 MB ring is simply too small** for a full frame of dynamic data (UI "lock all remaining" + many skinned meshes), so it must wrap mid-frame, and **any** mid-frame `WRITE_DISCARD` rename corrupts already-recorded-but-not-yet-executed draws in D3D11? If so, what's the correct architecture — per-skinned-mesh dedicated dynamic buffers, a much larger ring, ring sub-allocation without `WRITE_DISCARD` rename, or forcing the GPU to consume before recycle?
3. Is `CuiLayerRenderer`'s "lock the entire remaining ring as scratch, fill incrementally, unlock actual" pattern (`CuiLayerRenderer.cpp:62-91`, `render()` at :170) compatible with our shared-ring `Direct3d11_DynamicVertexBufferData` at all, or does it need its own dynamic buffer?
4. What is the **minimal correct fix** that (a) keeps the D3D9-faithful actual-used accounting, (b) does not corrupt deferred draws on a mid-frame wrap, and (c) does not regress the UI? Cite the exact change.

## 6. Key files
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.cpp` (ring lock/unlock/accounting — the fix site)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_DynamicVertexBufferData.cpp` (reference: lock@~172, unlock@~231/244)
- `src/engine/client/library/clientGraphics/src/shared/DynamicVertexBuffer.h` (engine wrapper: lock@186, unlock@207/224, begin@241; doc@182 "multistreaming should force discard")
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexBufferVectorData.cpp:101` (multi-stream bind, dynamic offset@145)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp` (applyPreDrawState@~1190, IASetVertexBuffers@1232, drawPartialIndexedTriangleList@3221)
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SoftwareBlendSkeletalShaderPrimitive.cpp:483` (prepareToDraw; multi-stream lock/copy@566-581)
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiLayerRenderer.cpp:62` (the lock-all-remaining UI consumer)
