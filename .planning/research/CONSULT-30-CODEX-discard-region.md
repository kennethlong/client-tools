# Research task: can a soft-skin draw read an UNWRITTEN region of a freshly WRITE_DISCARD'd ring? (read-only)

## Established (treat as given)
D3D11-only skinned-character spikes are **live-only** (zero on RenderDoc replay = uninitialized-memory signature), masked by any CPU sync, and the written vertex slice reads clean. The dynamic VB is a shared ring (`Direct3d11_DynamicVertexBufferData`): `Map(WRITE_DISCARD)` on wrap/new-frame (returns a fresh, **undefined** backing allocation), `Map(WRITE_NO_OVERWRITE)` to append. `lock(n)` returns a slice; the draw binds the VB at the slice's vertex offset and `DrawIndexed`.

## Questions (trace; file:line; no fixes)
1. After a `WRITE_DISCARD`, the entire backing is undefined. The engine writes ONLY the locked slice `[ringUsed, ringUsed+length)`. Could any draw end up **bound to / fetching from** a ring byte range that has NOT been written since the last DISCARD (i.e. the undefined fresh region)? Trace `ringUsed`, `m_offset = ringUsed/vertexSize`, and the bind in `Direct3d11_StateCache` (`ms_currentVBOffset` / `IASetVertexBuffers`).
2. **Stride/offset consistency:** is the `vertexSize` used to compute `m_offset` (and the bind byte offset) the SAME as the stride the input layout uses to fetch? If `m_offset` is computed with one stride but the IA fetches with another, vertices land in unwritten bytes → garbage live / zero replay.
3. `getNumberOfLockableDynamicVertices` and the per-frame reset: is there a path where a draw's recorded `m_offset` (captured at lock time) no longer points at its written bytes by the time `draw()` runs (e.g. a later lock advanced `ringUsed` and the bind reads a stale/shared offset)? Note: `prepareToDraw()` and `draw()` are back-to-back per primitive, but verify the OFFSET binding survives that window unmodified.
4. Does `DrawIndexed` use `BaseVertexLocation`? If `BaseVertexLocation=0` but the data lives at `m_offset` via the *bind* offset, confirm the index→byte math lands inside the written slice for every index in `[0, maxIndex]`.

Deliverable: yes/no on "a soft-skin draw can fetch from unwritten (undefined-since-discard) ring bytes," with the offset/stride math and file:line. If yes, identify the exact mismatch.
