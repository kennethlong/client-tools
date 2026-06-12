# Research task: enumerate EVERY draw a skeletal character issues + its draw function/primitive type (read-only)

## Established (treat as given)
For a skeletal character (e.g. `appearance/jawa_m.sat`), the `SoftwareBlendSkeletalShaderPrimitive` body draws — issued via `Graphics::drawPartialIndexedTriangleList` — are **verified 100% clean at the GPU** (11,724 draws sampled, zero degenerate vertices, correct transforms). Yet the character **visibly renders radiating vertex "spikes"**. So the shards are produced by **a draw call the instrumentation does NOT cover** — only `drawPartialIndexedTriangleList` is instrumented.

## Task (map it, file:line; no fixes)
Enumerate **every** draw call issued when rendering ONE skeletal character per frame, and for EACH give the exact `Graphics::drawXXX(...)` function it ultimately calls and the D3D primitive type (triangle-list / triangle-strip / fan / quad / line):

1. The soft-skin body shader-primitives (baseline — `drawPartialIndexedTriangleList`).
2. **Wearables / clothing / armor** — are these separate shader prims? Same draw function or different?
3. **Attachments at hardpoints** — weapons, held items, backpacks — what appearance type and draw path?
4. **Shadows** — blob/simple shadow, shadow volume (note: volumetric is currently force-disabled) — draw path?
5. **Hair / facial / eye** prims — draw path?
6. Any **effect / glow / cloak / extent / debug** geometry attached to the character.

For each, trace the dispatch from `SkeletalAppearance2::render()` / `ShaderPrimitiveSorter` down to the concrete `Graphics::draw*` call. **Explicitly flag every draw that does NOT go through `drawPartialIndexedTriangleList`** — especially anything using `drawPartialIndexedTriangleStrip`, `drawTriangleStrip`, `drawTriangleFan`, `drawQuadList`, or a non-indexed `draw*`.

Deliverable: a table (geometry | owning class | Graphics::draw function | primitive type | file:line), with the non-`drawPartialIndexedTriangleList` draws highlighted. That set is where the spike must live.
