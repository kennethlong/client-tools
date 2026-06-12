# CONSULT-21 — D3D11 soft-skin dynamic-VB corruption (cape/garment "black spikes"), now RenderDoc-deterministic

**Date:** 2026-06-04
**Repo:** `swg-client-v2`, branch `koogie-msvc-cpp20-base` (main checkout `D:\Code\swg-client-v2`)
**Renderer:** in-house D3D11 plugin (`gl11`) reimplementing the legacy SWG `Gl_api`. D3D9 plugin (`gl05`) is the working reference and renders the identical content CLEAN.

> **Reviewer instruction (READ FIRST):** Our repeated failure mode on this project is all three AIs converging on the SAME wrong hypothesis. Do NOT rubber-stamp. Attack independently. Prefer the D3D9 reference as ground truth. Cite file:line. **In particular: a previous round already concluded "per-generation ring buffers" was the fix; we implemented it and it made things WORSE and changed the bug's character (see §5). So the prior consensus is suspect.** If the real bug is something other than the dynamic ring, say so.

---

## 1. Symptom

Under D3D11 (`rasterMajor=11`), robed/skinned characters (Jawas, robes) show **sharp dark triangular "spikes"** — vertices flung to garbage positions. **Distance/LOD-dependent (NEW, decisive):** the SAME Jawa renders CLEAN at a distance and TEARS up close. SWG picks skinning mode by on-screen size (`SkeletalAppearance2.cpp:1737`):
- far/tiny → `SM_noSkinning`
- medium → `SM_hardSkinning`
- **close/large → `SM_softSkinning`** ← the torn case

So the tearing path is **soft skinning**: the CPU skins vertices every frame into a **`DynamicVertexBuffer`** (`SoftwareBlendSkeletalShaderPrimitive::prepareToDraw`, `clientSkeletalAnimation/.../SoftwareBlendSkeletalShaderPrimitive.cpp:566-620`). The same character under D3D9 (`gl05`) is clean at all distances.

## 2. Architecture (verified file:line)

- Soft-skin writes skinned POSITIONS into a process-wide shared dynamic ring `ID3D11Buffer` (`Direct3d11_DynamicVertexBufferData`, 2 MB, `D3D11_USAGE_DYNAMIC`). Multi-stream: stream0 = static (UV/normal/color), stream1 = dynamic (skinned positions).
- Ring lock/unlock (`Direct3d11_DynamicVertexBufferData.cpp`): `lock(n)` → if `ms_newFrame||forceDiscard||ms_used+len>ms_size` then `Map(WRITE_DISCARD), ms_used=0` else `Map(WRITE_NO_OVERWRITE)`; slice at byte `ms_used`; `m_offset=ms_used/vertexSize`.
- Bind: dynamic stream IA byte-offset = `m_offset*stride`; draw is `DrawIndexed(count, startIndex, BaseVertexLocation=0)` with ABSOLUTE indices in `[0,m_vertexCount)` (`SoftwareBlendSkeletalShaderPrimitive::RenderCommand::render` :314 passes baseVertex=0). Verified earlier: write-byte == IA-read-byte (offset math correct, `MISMATCH=0`).
- **The engine writes ALL `m_vertexCount` verts** (soft-skin loop `fillVertexBuffer` :1640/:1690 `for i<m_vertexCount`; system & dynamic streams both sized `m_vertexCount`). So per-mesh the slice is fully written and self-consistent.
- `beginFrame()` exists but has **ZERO call sites** — the ring is NEVER reset per-frame; `ms_used` grows monotonically and only wraps (`WRITE_DISCARD`) on overflow.

## 3. D3D9 reference (WORKS, identical architecture)

`Direct3d9_DynamicVertexBufferData` is ALSO a single shared ring (one `IDirect3DVertexBuffer9`), `D3DLOCK_NOOVERWRITE`/`D3DLOCK_DISCARD`, same wrap logic. Difference in accounting: D3D9 advances `ms_used` by the **ACTUAL used count in unlock()** (`Direct3d9_DynamicVertexBufferData.cpp:244`); D3D11 originally advanced by the **UPPER-BOUND locked length in lock()** (over-charge). D3D9 calls `beginFrame()` once per frame inside `beginScene()` (`Direct3d9.cpp:2484-2486`), and D3D9 wraps the whole frame in one BeginScene/EndScene.

## 4. The two hard, contradictory empirical facts

**(A) The original bug is capture-INVISIBLE.** A RenderDoc capture of a spiky frame REPLAYS CLEAN. Classic async CPU-write/GPU-read race signature (RenderDoc serializes CPU/GPU so the race vanishes; undefined-live data zero/stable on replay).

**(B) Reducing renames makes it WORSE; frequent renaming is PROTECTIVE.** (Established a prior session, re-confirmed this one.) The buggy over-charge accounting causes `CuiLayerRenderer` (a UI batcher that locks `getNumberOfLockableDynamicVertices()` = the whole remaining ring, `CuiLayerRenderer.cpp:62`) to max `ms_used` every UI lock → **~370 `WRITE_DISCARD`/frame**. That thrash is the LEAST-bad, playable state (mild localized spikes). Every change that REDUCES discards (D3D9-faithful accounting → ~1 discard/frame; per-frame reset) makes the tearing DRAMATICALLY WORSE (full-screen black triangles).

These two together are the puzzle: it behaves like a CPU/GPU ring-reuse race, yet **D3D9 with the SAME ring + SAME (actually-used) accounting + only ~1 discard/frame is perfectly clean**, while D3D11 NEEDS ~370 discards/frame to be merely playable.

## 5. What we tried THIS session (and how it backfired)

Implemented the prior round's recommendation: **per-generation ring buffers** (N=4 `ID3D11Buffer`s; advance generation on each `WRITE_DISCARD`; store the chosen buffer on each slice; bind `data->getVertexBuffer()` instead of the shared ring) + **D3D9-faithful actual-used accounting** + later a **true once-per-frame `beginFrame()` flip** wired into `update_impl` (the `Gl_api::update` slot, fired every frame BEFORE the first `beginScene`; `beginScene` itself runs per-PASS and resetting there is catastrophic because draws DEFER across passes).

Result: **WORSE, and the bug changed character** — it became **DETERMINISTIC / RenderDoc-CAPTURABLE** (the per-frame flip build still tore identically). This is the key new datum: per-generation + reduced renames converted a capture-invisible RACE into a capture-DETERMINISTIC corruption. Reverted to baseline.

## 6. RenderDoc localization (on the per-generation build, which captures deterministically)

Captures: `stage/Capture13.rdc` (clean, far) + `stage/Capture14.rdc` (torn, close), both D3D11.
- Scene renders to offscreen RT `::326`, then a fullscreen quad composites to the swapchain.
- pixel_history on `::326` at the big black wedge → **eid 3803** owns it (dozens of overlapping triangles collapsed to one depth, output pure black). eid 3803 = indexed, 2112 indices / 704 tris, VS=`ResourceId::2216` (used by 31 draws = the skeletal VS), PS=`7704`.
- **VS 2216 input signature = `POSITION, NORMAL, TEXCOORD` (no blend weights/indices → NOT hardware skinning; it's a plain WVP transform).** Therefore garbage `SV_Position` can ONLY come from garbage POSITION INPUT (the dynamic stream).
- **export_mesh vs-out of eid 3803 = GARBAGE:** of 1895 verts, 112 are NaN/null and ~400 are flung to ±1e33..1e38 (uninitialized-float pattern); the rest sit in a sane clip range. Garbage is **clumpy and NON-periodic** (bad-vertex fraction ≈ 21% and uniform across index mod 2/3/4/6/8 → NOT a stride/format mismatch). Pattern: a contiguous early bad run, then interleaved good/bad chunks.

Interpretation: the dynamic-VB region this draw reads contains uninitialized/garbage POSITIONS for ~21% of its verts, in clumps — consistent with the buffer memory being partially clobbered/recycled relative to when this (deferred) draw executes.

## 7. Questions for the reviewer

1. **Resolve the core contradiction:** if this is a CPU-write/GPU-read race on `D3D11_USAGE_DYNAMIC` + `WRITE_NO_OVERWRITE`, why is D3D9 (same single ring, same actually-used accounting, ~1 discard/frame) CLEAN while D3D11 needs ~370 `WRITE_DISCARD`/frame to be merely playable? What does D3D9's `Lock(offset,length,DISCARD/NOOVERWRITE)` + `DrawIndexedPrimitive` do that D3D11's `Map(WHOLE-buffer, WRITE_NO_OVERWRITE)` + `IASetVertexBuffers(byteOffset)` + `DrawIndexed` does NOT? (Consider: D3D9 runtime implicit lock synchronization vs D3D11's true no-sync `WRITE_NO_OVERWRITE` contract that the engine may be VIOLATING by reusing ring memory across deferred draws without a per-frame discard.)

2. **Is the "capture-invisible → capture-deterministic" transition the key clue?** A genuine async race should replay CLEAN in RenderDoc (serialized). The per-generation build replays DIRTY. Does that prove the per-generation code introduced a deterministic binding/lifetime bug (e.g., a slice bound to a generation buffer that was legitimately recycled before its deferred draw), rather than fixing the race? Or can a deterministic replay still reflect a real race that RenderDoc happens to serialize differently post-change?

3. **Why does frequent `WRITE_DISCARD` PROTECT?** Mechanistically, what is the difference between (a) ~370 discards/frame on ONE buffer (playable) and (b) ~1 discard/frame on one buffer or per-generation buffers (broken)? If frequent discard is protective because each rename hands the driver fresh memory that no in-flight draw references, why doesn't per-generation (which also hands out distinct buffers) achieve the same — and instead make it worse?

4. **Deferred draws across passes.** We have PROOF draws defer across render passes (resetting the ring in per-pass `beginScene` shreds geometry). Given the engine prepares a soft-skin slice in one pass and submits its `DrawIndexed` in a LATER pass, what is the correct D3D11 dynamic-buffer discipline so a slice's bytes survive until its deferred draw executes, WITHOUT relying on the accidental 370-discards/frame thrash?

5. **What is the minimal correct fix** that (a) makes the soft-skin POSITION data survive to the deferred draw, (b) keeps UI/`CuiLayerRenderer` working, and (c) matches D3D9 visual output? Candidates we're weighing: dedicated per-skinned-mesh dynamic buffers (isolate skinned data from the shared ring + the UI thrash entirely); a much larger ring; proper N-buffered per-frame flip with N sized to GPU latency AND mid-frame overflow; or something we're missing about how D3D9's lock semantics differ. Cite the exact change.

## 8. Key files
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.cpp` (ring lock/unlock/accounting; `beginFrame()` @144 has zero callers)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp` (`update_impl` @289 = the once-per-frame `Gl_api::update` hook, currently no-op)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexBufferVectorData.cpp` (multi-stream dynamic bind @143)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp` (single-stream dynamic bind @2265; draw @3223; per-pass beginScene reset is catastrophic)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_DynamicVertexBufferData.cpp` (reference: lock@172, unlock@244)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp:2484` (D3D9 once-per-frame beginFrame inside beginScene)
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SoftwareBlendSkeletalShaderPrimitive.cpp` (soft-skin prepareToDraw@483/566, fillVertexBuffer@1617, RenderCommand::render@314)
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SkeletalAppearance2.cpp:1737` (skinning-mode-by-screen-size selection)
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiLayerRenderer.cpp:62` (lock-all-remaining UI consumer that drives the 370 discards/frame)
- Evidence: `stage/Capture13.rdc` (clean), `stage/Capture14.rdc` (torn, deterministic), `stage/renderdoc-mcp-export/mesh_3803_vsout.json` (garbage mesh), `rt_6356_0.png` (torn scene RT).
