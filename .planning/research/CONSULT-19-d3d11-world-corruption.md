# Consult: D3D11 world-view wholesale texture/geometry corruption

## The bug
A D3D11 renderer port (`gl11`, `rasterMajor=11`) of the Star Wars Galaxies client shows **wholesale wrong-texture + geometry corruption** on the open world (Mos Eisley). The legacy **D3D9 path (`gl05`) renders the identical scene/server/assets correctly** — it is a fully-working reference. We are porting D3D9 → D3D11; both plugins share the engine (clientGraphics) and load the same assets.

## Symptoms (from screenshots)
- Entire surfaces render with the **WRONG texture**: a recognizable lava/fire/circuit texture appears on stone buildings; large areas of terrain/ground render as a **solid saturated color** (blue, then magenta, then green, then yellow — changes frame to frame); white blocks.
- **Floating disconnected geometry fragments** in some frames (wrong vertex positions), and one frame nearly all-black with colored shards.
- **Near/foreground geometry renders FINE** (the player character, nearest building). Corruption is on **mid/far buildings** — strongly **distance / LOD correlated**.
- It **flickers** (corrupt ↔ normal) fast, and some surfaces **latch** into a bad state and stay (e.g., a wall stuck blue even up close).
- Worse on **occluded** geometry (a building behind a wall).

## Empirical eliminations (hard test results, not hypotheses)
1. **PS constant buffer b0 is NOT the cause.** Instrumented all 25 `float4` registers of the asset-PS `SwgVertexConstants` cbuffer @ b0, read from the final staging just before upload, across **282,404 draws** over a roam. Light direction (c0), light diffuse/specular (c1/c2), hemispheric terms (c3/c4), material (c5-c7), user constants (c8+) are all **sane and stable**; `d3.valid==1` on every draw (lights always written, never inherited). No saturated/garbage register.
2. **SRV slot bindings are consistent.** Terrain draws bind **exactly 4 textures** (slots 0-3), no missing/null/swapped slots, across the whole roam.
3. **Per-frame full GPU serialization does NOT fix it.** Added a per-frame `Flush()` + `D3D11_QUERY_EVENT` spin-wait after `Present` (CPU blocks until GPU finishes the frame). Corruption **unchanged** at 5 FPS. → not a render-thread vs GPU pipeline race.
4. **Disabling the async loader does NOT fix it.** Set `SharedFile.runtimeDisableAsynchronousLoader` (loads synchronously on the main thread). Corruption **unchanged**. → not a loader-thread vs render-thread CPU race on resource uploads.
5. **RenderDoc captures of the corrupt frames REPLAY CLEAN.** The live frame is visibly corrupt; pressing capture freezes on the corrupt image; but the captured command stream, replayed deterministically, produces a **correct** image. → the corruption is **not in the recorded D3D11 commands / pipeline state / bindings**. (RenderDoc presents un-tracked/uninitialized resources as zero on replay.)

## Current leading hypothesis (not yet pinned to a defect)
**Uninitialized / stale D3D11 resource CONTENT** (textures primarily, some vertex buffers). The recognizable lava texture on a building = **aliased GPU memory** — a resource created but not fully written, showing prior contents. Consistent with: replay-clean (RenderDoc zeros uninitialized resources), distance/LOD-only (LOD resources created/recreated constantly), latching (garbage persists until overwritten), and immunity to both GPU-serialization and loader-disable.

## Relevant D3D11 code facts
- Textures: BC/DXT block-compressed (BC1/BC2/BC3). Created `CreateTexture2D(desc, nullptr=no initial data, &tex)`, content uploaded later via the engine lock/unlock → `context->Map`/`UpdateSubresource` with `RowPitch` handling, per mip level. There is special handling for sub-4 BC mips (D3D11 rejects USAGE_STAGING < 4 for BC).
- Static VB/IB: `USAGE_DEFAULT`, `CreateBuffer(desc, nullptr, &buf)`; `lock(write)` returns `operator new[](byteWidth)` (**uninitialized**); engine writes; `unlock` does `UpdateSubresource(buf, 0, nullptr, lockBuffer, 0, 0)` of the full width.
- All uploads use the **immediate context**; no mutex anywhere (but threading is ruled out by test #4).
- Device created without `SINGLETHREADED`; `createDeviceFlags=0` (+DEBUG in debug builds).

## The question
Given the eliminations — especially **replay-clean + immune to GPU-serialization + immune to loader-disable** — what **D3D11-specific mechanism** produces wholesale WRONG-texture (recognizable other-asset content) and wrong-geometry on distant/LOD surfaces, that the D3D9 path does not exhibit?
1. What is the **most likely specific code defect** (texture creation/upload init gap? BC mip pitch? a resource pool / memory-block manager reuse? UpdateSubresource on USAGE_DEFAULT semantics? mip-chain partial upload? texture used before first upload?).
2. What is the **single decisive next test** to pin it?
3. Are we wrong that it's uninitialized content — is there a mechanism we've eliminated too hastily?

Be skeptical; we are explicitly trying to avoid all three AIs converging on the same wrong hypothesis. Challenge the framing if warranted.
