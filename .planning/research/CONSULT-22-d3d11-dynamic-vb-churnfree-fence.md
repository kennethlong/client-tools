# CONSULT-22 — D3D11 dynamic-VB soft-skin tearing: a CHURN-FREE, fence-retired fix

**Date:** 2026-06-04
**Repo:** `swg-client-v2`, branch `koogie-msvc-cpp20-base` (`D:\Code\swg-client-v2`)
**Renderer:** in-house D3D11 plugin (`gl11`) reimplementing SWG's `Gl_api`. D3D9 (`gl05`) is the working reference.
**Read CONSULT-21 first** (`.planning/research/CONSULT-21-d3d11-softskin-vb-race-renderdoc.md` + `-codex.out`/`-cursor.out`) — this is a follow-up after CONSULT-21's recommended fix FAILED in a new way.

> **Reviewer instruction:** We have now failed THREE times (details in §2). Do not propose anything that (a) reduces the protective driver renaming without a real GPU-fence replacement, or (b) mass-creates D3D11 buffers. Attack the §4 proposed design specifically: will it work, what's wrong with it, what's the exact D3D11 fence/query mechanism (FL10.0+, this is `vs_4_0`/SM5 hardware), and the exact code. Cite file:line. D3D9 is ground truth.

## 1. The bug (one paragraph)
D3D11-only. CPU-skinned (close range → `SM_softSkinning`) characters tear: skinned POSITIONS in the dynamic VB read as uninitialized garbage (RenderDoc Capture14: draw 3803, VS 2216 = plain WVP transform, ~21% of verts NaN/1e38, clumpy/non-periodic). Root cause (CONSULT-21 convergent): all dynamic VBs (UI + skinned + effects) sub-allocate from ONE process-wide 2 MB shared ring (`Direct3d11_DynamicVertexBufferData`, `D3D11_USAGE_DYNAMIC`) via `WRITE_NO_OVERWRITE` appends + monotonic `ms_used`; draws DEFER across render passes; the ring reuses bytes a deferred draw hasn't consumed yet → CPU-write/GPU-read race. D3D9 (same single-ring architecture) is clean because it has correct actually-used accounting (~1 discard/frame) and older implicit `Lock` sync; the D3D11 baseline only "works" because buggy over-charge accounting forces ~370 `WRITE_DISCARD`/frame, and the driver's rename pool accidentally provides the fencing.

## 2. THREE failed fixes (do not repeat)
1. **Per-generation ring** (N=4 `ID3D11Buffer`s created once, advance generation on each `WRITE_DISCARD`, bind per-slice buffer) + D3D9-faithful accounting. → Tearing got WORSE and became RenderDoc-DETERMINISTIC. Why (CONSULT-21): 4 buffers recycled by a COUNTER, not by GPU fence; with the discard rate they were reused before the deferred draw ran. Reducing renames removed the accidental protection.
2. **Per-frame `beginFrame()` flip** wired into `update_impl` (the real once-per-frame `Gl_api::update` hook, fires before the first `beginScene`; `beginScene` is per-PASS and resetting there is catastrophic since draws defer across passes). beginFrame set `ms_newFrame=true` so the first lock advanced generation per-frame. → NO CHANGE (still tore).
3. **Uniform per-instance buffers** (removed the shared ring; EVERY `Direct3d11_DynamicVertexBufferData` instance owns its own `ID3D11Buffer`, lazy-grow, `Map(WRITE_DISCARD)` at offset 0 each lock; bind `data->getVertexBuffer()`). → **CRASH on world load**: `c0000005` INSIDE the NVIDIA UMD on a driver WORKER THREAD (`nvwgf2um!...SetDependencyInfo` null-deref), during async mesh streaming. Cause: hundreds-to-thousands of `CreateBuffer`/`Release` (one per dynamic VB: UI, effects, every streamed mesh) + grow-recreate = **D3D11 resource churn the driver can't take**. The baseline has exactly ONE buffer.

**Net constraints learned:** the fix must (A) NOT reduce the protective renaming unless it adds a REAL GPU-fence retirement; (B) NOT create buffers en masse (driver churn crash); (C) flip per-FRAME not per-pass; (D) survive draws deferred across passes (a frame's slice bytes must live until that frame's GPU work completes); (E) keep `CuiLayerRenderer`'s "lock all remaining, fill incrementally, flush" UI pattern working.

## 3. Key facts / measurements
- Baseline ring uses ~14 KB/frame in a quiet cantina but a busy CLOSE scene (many high-LOD soft-skin chars) is far higher; the close scene is where it tears (LOD: `SkeletalAppearance2.cpp:1737` picks soft-skin for large on-screen size).
- Locks are MAIN-THREAD only (a prior `Os::isMainThread()` audit logged 0 off-thread locks across ~1M draws). So `Map`/`Unmap` are single-threaded; async mesh load creates the engine VB objects but the GPU buffer + `Map` happen at render-time lock.
- Soft-skin writes ALL `m_vertexCount` verts (`SoftwareBlendSkeletalShaderPrimitive::fillVertexBuffer` :1640/:1690); draw is `DrawIndexed(count, startIndex, BaseVertexLocation=0)` with absolute indices in `[0,m_vertexCount)`; offset math verified correct. So the data is correct when written — the failure is lifetime (bytes reused before the deferred GPU read).
- `CuiLayerRenderer.cpp:62` locks `getNumberOfLockableDynamicVertices()` (whole remaining ring), fills a few quads across many `render()` calls, then `flushRenderQueue()` draws + unlocks actual count.

## 4. PROPOSED DESIGN (attack this) — "few fixed buffers, per-frame flip, fence-retired"
Synthesis of all three failures' lessons. Keep ONE shared allocator but make reuse GPU-safe WITHOUT churn:

- Allocate a SMALL FIXED set of large dynamic buffers ONCE at `install()` — e.g. **3 buffers of 4 MB** (triple-buffered; sized so one frame's total dynamic data fits in one buffer). No per-instance, no per-lock, no grow → ZERO runtime `CreateBuffer` churn (addresses constraint B / the crash).
- Per FRAME (in `update_impl`, the verified once-per-frame pre-pass hook): advance to the next buffer index `frame % 3`; **before using it, wait on that buffer's GPU fence/query** to guarantee the GPU finished the frame that last used it (addresses A/D — real fence retirement, not a counter). Reset `ms_used = 0` for the new buffer.
- WITHIN a frame: all consumers append into the current frame's buffer via `WRITE_NO_OVERWRITE` at increasing `ms_used` (D3D9-faithful actually-used accounting on `unlock`). No mid-frame discard/flip → deferred-across-passes draws in the same frame all read stable bytes (addresses C/D). Slices carry `(currentBuffer, offset)`.
- At end of frame (`present`/`endScene` of the frame, or next `update_impl`), insert the fence/query signal for the buffer just used.
- `CuiLayerRenderer` keeps working unchanged: it appends into the current frame's buffer like everyone else (addresses E). `getNumberOfLockableDynamicVertices` returns remaining-in-current-buffer.
- If a single frame ever exceeds one buffer (overflow), fall back to a mid-frame `WRITE_DISCARD` on a 4th scratch buffer OR FATAL/log + grow the fixed size at install — but with 4 MB this should not happen (baseline peaks well under 2 MB).

**Open questions for reviewers:**
1. Is this correct, and is 3×4 MB the right shape? Will the per-frame fence wait stall the CPU unacceptably (it shouldn't if N≥ GPU latency in frames)?
2. **Exact D3D11 fence mechanism** on FL10.0+ hardware: `ID3D11Query` with `D3D11_QUERY_EVENT` (insert after the frame's last draw, `GetData` spin/poll before reuse) vs `ID3D11Fence`+`ID3D11DeviceContext4::Signal/Wait` (needs 11.4 / `ID3D11Device5`). Which is safe/portable here, and the exact insert/wait placement?
3. Does per-frame flip + within-frame `WRITE_NO_OVERWRITE` actually eliminate the race given deferred-across-passes draws — or is there still an intra-frame hazard we're missing? (Per-frame flip #2 above "did nothing" with per-generation N=4 counter-recycle; does fence retirement fix what the counter could not?)
4. Is there a SIMPLER correct fix we keep missing — e.g. just keep the single baseline ring but add ONE fence so the wrap/`WRITE_DISCARD` waits for the GPU? Would that alone stop the tearing without the multi-buffer flip?
5. Anything about why D3D9 truly survives this that we still have wrong?

## 5. Key files
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.{cpp,h}` (the ring; `beginFrame()`@144 zero callers; baseline = single shared ring, over-charge accounting)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp` (`update_impl`@~289 = once-per-frame `Gl_api::update`, no-op; per-frame hook lives here)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp` (`present`/`endScene`/`beginScene` — beginScene is PER-PASS)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp:2265` + `Direct3d11_VertexBufferVectorData.cpp:143` (dynamic VB bind sites)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_DynamicVertexBufferData.cpp` (reference: lock@172, unlock@244) + `Direct3d9.cpp:2484` (beginFrame in beginScene)
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SoftwareBlendSkeletalShaderPrimitive.cpp` (soft-skin path)
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiLayerRenderer.cpp:62`
- Crash dump (per-instance attempt): `stage/SwgClient_r.exe-unknown.0-20260605042910.{mdmp,txt}`
- Repro captures: `stage/Capture13.rdc` (clean far) / `stage/Capture14.rdc` (torn close, deterministic)
