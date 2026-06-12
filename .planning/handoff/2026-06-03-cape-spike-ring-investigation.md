# Handoff — 2026-06-03 (cape/garment "black spikes" — deep ring investigation)

Branch `koogie-msvc-cpp20-base`, main checkout `D:\Code\swg-client-v2`. Long session on the
D3D11-only cape/garment spike bug. **Root mechanism narrowed hard; multiple fixes tried and
REVERTED; tree is back at baseline `5fce7bb83` (robe-spikes-but-playable).**

Full detail in memory: **`[[project_d3d11_cape_spike_skinning_regression]]`**. Consult:
`.planning/research/CONSULT-20-cape-d3d11-dynamic-vb-ring-accounting.md` (+ `-codex.out`, `-cursor.out`).

## What we proved (empirical, not theory)
1. Garment skinned verts flow through the shared **dynamic VB ring** (`SoftwareBlendSkeletalShaderPrimitive` multi-stream → `m_dynamicStream`). Offset math is correct (`MISMATCH=0`). No index-span overrun.
2. **Real accounting bug:** D3D11 charges `ms_used += length` (upper-bound) in `lock()`; D3D9 charges the **actual** count in `unlock()`. `CuiLayerRenderer` locks `getNumberOfLockableDynamicVertices()` = the whole ring, so the D3D11 over-charge maxes `ms_used` → ~370 ring renames/frame. Both consults confirmed the accounting bug.
3. **The whole `beginFrame()` family has ZERO call sites in the D3D11 plugin** (`DynamicVertexBufferData`, `DynamicIndexBufferData`, `StaticShaderData`, `StateCache`, `Metrics`). D3D9 calls VB+IB beginFrame every frame in `Direct3d9.cpp:2485-2486`; D3D9's beginFrame resets `ms_used=0` per frame. So the D3D11 ring never resets per frame.

## What broke (the surprising part)
- D3D9-faithful **accounting fix** → ring stops thrashing (`WRAP=0`) but visuals get **WORSE** (full-screen flicker).
- **+ cap CuiLayer** → worse still.
- **Add VB+IB `beginFrame()` to `Direct3d11_Device::beginScene`** → **CATASTROPHIC** (player body shredded, cantina near-black). Because `beginScene` runs **per-PASS (~2×/frame)**, not per-frame — resetting the ring mid-frame tears geometry. **This proves draws are DEFERRED ACROSS PASSES** (prepared in one pass, drawn in a later one); the ring must persist the whole frame.
- Ring uses only **~14 KB/frame and never wraps** with the accounting fix → **wraps are NOT the residual-corruption cause.**

## The inversion / current best theory
Every change that REDUCES renames makes it worse; the only playable state is the original
over-charge that renames ~370×/frame. **Frequent renaming is PROTECTIVE.** The real bug is
**CPU-write-vs-GPU-read contention on the single shared ring buffer**, which heavy renaming
hides. Robe spikes are the residual where it still fails.

## Recommended next step
Implement **Codex's per-generation ring buffers** (CONSULT-20 `-codex.out`):
- Replace the single `ms_d3dRingBuffer` with a small ring of N `ID3D11Buffer`s (generations).
- On `WRITE_DISCARD`/wrap → advance to next generation, `ms_used=0`, map that buffer.
- Store the chosen `ID3D11Buffer*` on each `Direct3d11_DynamicVertexBufferData` slice at lock.
- Bind `data->getVertexBuffer()` (the slice's buffer) instead of `getSharedRingBuffer()` — at `Direct3d11_StateCache.cpp:2265` and `Direct3d11_VertexBufferVectorData.cpp:143`.
- Keep the D3D9-faithful unlock accounting. Test the **busy cantina interior FIRST** (worst case).
- Alternative: find the TRUE once-per-frame hook (Graphics::update / gl_api update slot, BEFORE the first pass — not per-pass beginScene) for a frame-level reset, but only WITH per-generation safety.

## State / housekeeping
- Tree reverted to `5fce7bb83` for the 4 touched files (`git checkout HEAD --` on Direct3d11_DynamicVertexBufferData.cpp, Direct3d11_StateCache.cpp, Direct3d11_Device.cpp, CuiLayerRenderer.cpp). `gl11_r.dll` rebuilt at baseline.
- Staged release exe backed up at `binary-backups/SwgClient_r.exe.bak-20260603-220633`.
- Stale runtime logs in `stage/`: `cape-ring-frames.log`, `cape-ring-trace.log` (harmless, gitignored).
- New durable artifacts kept: `PROGRESS.md` + `docs/progress-gallery/` (screenshot arc), CONSULT-20 docs, cape-spike-evidence.
