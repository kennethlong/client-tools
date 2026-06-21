# CONSULT-45 SYNTHESIS — cantina zone-in "jerk": our rebuild vs retail/SWGEmu

4 consultants, non-overlapping angles, neutral evidence. Convergence-from-divergence below.

## Root causes (ranked by cross-consultant agreement)

### #1 — Runtime vertex-shader compile on the main thread (ALL FOUR converge)
Our `//hlsl` VS compile via `D3DCompile` at zone-in (`Direct3d9_VertexShaderData.cpp:684`), ~44 first
cantina load, synchronous. Retail/SWGEmu ship precompiled bytecode → no stall. The in-memory cache
(`ff02a367e`) fixes re-entry only, not first-ever load.
- **Fix (Cursor + Opus converge on mechanism):** port gl11's already-shipping `Direct3d11_ShaderCache`
  to a `Direct3d9_ShaderCache` — disk `.cso` blobs in `stage/shader-cache-d3d9/`, hooked in
  `createVertexShader`.
- **Divergence resolved (Cursor vs Opus on the key):** the DISK key must be a **content hash**
  (`hashSource` over source bytes + full macro list + a `REWRITE_VERSION` + d3dcompiler fingerprint),
  NOT the in-memory path-key `(filename, texcoordKey, target)` — else a `.vsh`/HlslRewrite edit
  silently loads stale bytecode. Porting gl11's `hashSource` faithfully GIVES the content hash, so the
  two reconcile: "port gl11, don't copy the path-key."

### #2 — Texture create + upload on the main thread, WDDM-amplified (Cursor #2 + Sonnet #3 + Codex #2)
`D3DPOOL_MANAGED` textures do a synchronous first-use staging-buffer upload; `CreateTexture` asserts
main-thread (`Direct3d9_TextureData.cpp:392`). Under WDDM (Win10/11) this stalls in a way it did NOT
under WinXP's XPDM — so it's worse for us than retail-era. Cantina pulls many unique `.dds` on first
zone-in. `.dds` bytes can prefetch async (`TextureList.cpp:148`), but the GPU create/upload is
device-affine → main thread.

### #3 — Interior-layout BURST creation is UNTHROTTLED (Codex-unique discovery)
`ClientInteriorLayoutManager::update()` instantiates ALL visible-cell layout objects **in one frame**
(`ClientInteriorLayoutManager.cpp:62,109`), unlike `WorldSnapshot` which throttles to
`ms_maximumNumberOfCreatesPerFrame=1000` (`WorldSnapshot.cpp:96,914`). A newly visible cantina cell
materializes many objects at once. **Renderer-independent → explains why the jerk is on ALL renderers
and BOTH bitness.** Fix = add a per-frame create budget mirroring WorldSnapshot's existing pattern.

### Debug-only amplifiers (Sonnet) — why Debug feels MUCH worse than Release
- `DEBUG_REPORT_LOG_PRINT(true, ...)` per rewrite at `Direct3d9_HlslRewrite.cpp:844,888` → an
  `OutputDebugString` per rewrite × 44 compiles; 1–5 ms each under a debugger. Gate/remove it.
- `_ITERATOR_DEBUG_LEVEL=2` in the Debug vcxproj → every container op 5–20× slower; zone-in is
  container-heavy. Drop to 1.

### Secondary / deferred — Allocator (Opus + Sonnet + Codex agree it's NOT primary)
SOE `MemoryManager` is active (best-fit tree + critical section, `OsNewDel.cpp:32`), but key zone-in
classes already use `MemoryBlockManager` fixed-size pools (Texture/StaticShader/StaticShaderTemplate).
Background drag, below the top costs. (Kenny's SmartHeap instinct = real but secondary.) Measure via
`DISABLE_MEMORY_MANAGER 1` A/B before investing; consider mimalloc only if data justifies.

## Architecture constraint (Opus + Codex)
The `AsynchronousLoader` worker EXISTS but zone-in is NOT a streaming pipeline — it's selective
prefetch; a missing async-map entry falls back to immediate synchronous load, and GPU creation +
callbacks run on the main thread regardless. **D3D9 device affinity:** `D3DCompile`→bytecode is pure
CPU / thread-safe (movable to a worker); `CreateVertexShader` / `CreateTexture` are device-affine
(must stay on the render thread). So async *compile* is feasible later; async GPU *create* is not.

## Recommended sequence (value / risk / effort)
1. **Disk VS bytecode cache** — port gl11 `Direct3d11_ShaderCache`, content-hash key. Kills #1 after
   one warm run. HIGH value, LOW risk (mirrors shipping code), MED effort. **FIRST.**
2. **Throttle `ClientInteriorLayoutManager`** create burst (per-frame budget like WorldSnapshot).
   Smooths the cantina-cell materialization across frames; renderer-independent. LOW-MED effort, LOW
   risk. **Strong cheap second.**
3. **Debug cheap wins** (drop IDL to 1; gate the per-rewrite ODS log). Tiny effort; directly fixes the
   Debug-only amplification. **Do alongside #1** (Kenny tests Debug).
4. **`elapsedTime` clamp** — bounds the visible lurch on any residual hitch + the literal first-ever
   cold load before the cache warms. Cheap complement.
5. **Texture upload** — device-affine + WDDM; defer, measure with RenderDoc Present-gap; the cache +
   throttle may make it acceptable. Bigger lift.
6. **Allocator swap** — deferred; measure (DISABLE_MEMORY_MANAGER A/B) before investing.

## Validation note
Re-entry is already smoother than first load (Kenny) → consistent with shader-compile + texture-upload
being the one-time first-load costs. The disk cache extends the shader benefit to first-ever load; the
layout throttle + texture handling are what remain for first-load parity with re-entry. Build #1+#2,
then re-measure first-load feel (the cache/throttle are their own probes).
