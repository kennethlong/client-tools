# Client Performance Pressure Evaluation Plan

> Plan for evaluating whether the current client is constrained by memory,
> CPU, GPU, asset streaming, or renderer architecture before committing to a
> larger 64-bit port.

## Executive summary

Do not start with a 64-bit port. Start by measuring where the current client is
actually hurting.

Higher-resolution textures and more complex geometry can stress several
different systems:

- 32-bit process address space and heap fragmentation.
- CPU-side asset loading, decompression, scene traversal, animation, culling,
  and draw submission.
- GPU VRAM, texture upload bandwidth, shader cost, overdraw, and draw call
  count.
- Asset format limits, streaming policy, mip generation, and cache behavior.

64-bit only directly helps the first category: process memory headroom. It may
also make tooling and future dependency replacement easier, but it will not
automatically fix GPU pressure, bad batching, too many draw calls, poor LODs,
oversized textures, or inefficient streaming.

Recommended approach:

1. Establish repeatable baseline scenes.
2. Add lightweight instrumentation for memory, frame time, renderer counters,
   asset loading, and GPU timing.
3. Test current assets, moderately upgraded assets, and intentionally heavy
   stress assets.
4. Classify the bottleneck before choosing a major engineering direction.

The desired outcome is a decision matrix:

| Finding | Likely response |
| --- | --- |
| Near 32-bit address-space ceiling or fragmentation failures | Investigate x64 client branch. |
| CPU frame time dominated by traversal/culling/draw submission | Optimize scene/render submission before x64. |
| GPU frame time dominated by shader/texture/fill cost | Optimize renderer/materials/assets before x64. |
| VRAM exhausted but process memory healthy | Texture budgets, streaming, compression, mip policy. |
| Load hitches from asset IO/decompression/upload | Streaming and async upload work. |
| No current pressure | Defer x64 and keep it as a long-term modernization track. |

## Goals

This evaluation should answer five questions:

1. How close is the 32-bit client to its practical memory ceiling during normal
   and heavy scenes?
2. Are frame-time spikes caused by CPU work, GPU work, IO, asset upload, or
   synchronization?
3. What happens when texture resolution and geometry complexity increase?
4. Which subsystems fail first under stress?
5. Is a 64-bit port justified in the near term, or should effort go elsewhere?

## Non-goals

This plan does not require:

- Starting the x64 port.
- Replacing middleware.
- Rewriting the renderer.
- Building a full telemetry platform.
- Perfect profiler coverage on the first pass.

The first version should be cheap, repeatable, and useful enough to guide
priorities.

## Baseline scenarios

Pick a small fixed set of scenes and reuse them for every measurement pass.

### Scene A: Login and character select

Purpose:

- Measures baseline memory footprint before entering the world.
- Catches browser/UI/startup regressions.
- Gives a stable lower-bound reference.

Measurements:

- Process private bytes.
- Virtual size.
- Working set.
- Startup time.
- UI frame time.
- Loaded DLL count and staged middleware.

### Scene B: Interior hub

Example: cantina or other populated interior.

Purpose:

- Stresses character rendering, skeletal animation, UI overlays, audio, and
  localized object density.
- Useful for testing CPU animation cost and draw submission.

Measurements:

- CPU frame time.
- GPU frame time.
- Draw calls.
- Skinned mesh count.
- Texture memory.
- Animation update time.
- Object count.

### Scene C: Outdoor city

Example: Mos Eisley street or similar high-object area.

Purpose:

- Stresses terrain, buildings, foliage, culling, LODs, texture variety, and
  streaming.

Measurements:

- CPU traversal/culling time.
- Render queue size.
- Draw calls by material/shader family.
- Visible object count.
- Terrain chunk count.
- Texture upload spikes.
- VRAM use.

### Scene D: Fast traversal / streaming path

Purpose:

- Finds hitches caused by asset IO, TRE lookup, decompression, texture creation,
  shader creation, and geometry upload.

Measurements:

- Frame-time spikes.
- Asset load counts per second.
- Bytes read per second.
- Texture creates/uploads per second.
- Main-thread stalls.
- GPU synchronization stalls.

### Scene E: Synthetic asset stress

Purpose:

- Tests the future content direction deliberately: higher-resolution textures,
  denser meshes, or both.

Variants:

- Current assets.
- 2x texture resolution subset.
- 4x texture resolution subset.
- Higher-poly static mesh subset.
- Higher-poly skinned mesh subset.
- Worst-case material variety.

The stress scene should be obviously unrealistic at the high end. The point is
to find the failure curve, not to define final art budgets.

## Measurement categories

## 1. Process memory and 32-bit pressure

Track:

- Private bytes.
- Virtual size.
- Working set.
- Peak commit.
- Largest free virtual allocation block, if feasible.
- Allocation failure count.
- Custom memory manager target/current usage.
- Heap fragmentation indicators.
- Texture/system-memory duplicate counts.
- Asset cache sizes.

Why this matters:

The 32-bit client can fail before total physical RAM or GPU VRAM is exhausted.
Address-space fragmentation can make large allocations fail even when aggregate
free memory looks acceptable.

Tools:

- Windows Performance Monitor.
- VMMap.
- Process Explorer.
- Visual Studio Diagnostic Tools.
- Existing memory manager logs.
- Optional in-client overlay/debug counters.

Useful thresholds:

| Signal | Interpretation |
| --- | --- |
| Private bytes regularly above ~1.5 GB | Start watching closely. |
| Virtual size regularly above ~2 GB | 32-bit ceiling is becoming relevant. |
| Allocation failures despite available system RAM | Strong x64 signal or allocator/fragmentation issue. |
| Large spikes during zoning or texture upload | Streaming/upload strategy may matter more than x64 alone. |

If the client is Large Address Aware, document the effective ceiling separately
for 32-bit Windows and 64-bit Windows. If it is not, note whether enabling LAA
is a smaller interim option than native x64.

## 2. CPU pressure

Track frame time by subsystem:

- Main loop total.
- Scene update.
- Object alter/update.
- Skeletal animation.
- Collision/raycasting.
- Terrain update.
- Culling/visibility.
- Render queue build.
- Draw submission.
- UI update/render.
- Audio update.
- Network receive/process.
- Asset loading and decompression.

Also track:

- Thread count.
- CPU utilization per thread.
- Lock contention.
- Main-thread stalls.
- Job queue depth, if any.
- Time spent waiting on GPU or IO.

Tools:

- Visual Studio CPU profiler.
- Windows Performance Recorder / Analyzer.
- Tracy or Optick, if lightweight integration is acceptable.
- Existing debug timers, if already present.

Interpretation:

| Finding | Likely response |
| --- | --- |
| One main thread saturated | Optimize hot path or parallelize; x64 will not fix it. |
| Draw submission dominates | Batching/material sorting/instancing work. |
| Animation dominates | LOD animation, skeleton update throttling, SIMD, caching. |
| Culling dominates | DPVS/frustum/portal optimization or simpler culling path. |
| Asset decompression dominates hitches | Async loading, prefetching, cache changes. |

## 3. GPU pressure

Track:

- GPU frame time.
- Draw calls.
- Primitive count.
- Vertex count.
- Pixel cost/overdraw.
- Render target switches.
- Texture binds.
- Shader switches.
- Constant buffer/state updates.
- Texture upload bytes.
- VRAM usage.
- Resource creation count during gameplay.
- Present interval and missed frames.

Tools:

- PIX.
- RenderDoc, where compatible.
- GPUView.
- Vendor tools: NVIDIA Nsight / AMD Radeon GPU Profiler if practical.
- D3D11 debug layer for development builds.

Interpretation:

| Finding | Likely response |
| --- | --- |
| VRAM exhausted | Texture budgets, compression, mips, streaming. |
| GPU frame high but CPU low | Renderer/shader/fill/texture optimization. |
| Draw calls very high | Batching, instancing, material sorting, static batching. |
| Upload spikes | Async texture upload and streaming policy. |
| Resource creation during frame | Preload/cache resources off the hot path. |

64-bit is not the primary fix for these unless CPU memory pressure is forcing
poor streaming decisions.

## 4. Asset pipeline pressure

Track:

- Texture dimensions and formats.
- Mip presence.
- Compressed vs uncompressed texture ratio.
- Mesh vertex/index counts.
- Material count per object.
- Unique shader/template count per scene.
- TRE lookup/load frequency.
- Duplicate asset loads.
- CPU decoded size vs GPU uploaded size.
- Cache eviction/reload churn.

Questions:

- Are high-resolution textures compressed on disk and in VRAM?
- Are full mip chains present?
- Are UI/loading textures being kept around longer than needed?
- Are objects split into too many materials?
- Are higher-poly assets increasing draw calls or just vertex count?
- Does the asset format impose 16-bit/32-bit count limits?

Output:

- A per-scene asset budget table.
- A list of top memory consumers.
- A list of top draw-call contributors.
- A list of top load-hitch contributors.

## 5. Stability and crash pressure

Track:

- Crash dumps.
- Fatal/assert logs.
- Allocation failure logs.
- Device removed/reset errors.
- Out-of-memory errors.
- SafeCast/assert frequency.
- Load failures.
- Long-frame events over threshold.

Recommended thresholds:

- Log any frame over 50 ms.
- Log any frame over 100 ms with subsystem timings.
- Log asset load operations over 25 ms on the main thread.
- Log texture/resource creation during active gameplay.
- Log memory snapshots at scene enter, scene settled, and scene exit.

## Instrumentation plan

### Minimal first pass

Add or use the smallest possible set of counters:

- Process private bytes / virtual size sampled once per second.
- Frame time min/avg/p95/p99/max.
- CPU frame section timers around major update/render phases.
- Draw call count.
- Triangle/vertex count if available.
- Texture memory estimate.
- Asset load count and bytes loaded.
- Long-frame log entries.

Output format:

- CSV or JSONL file per run.
- Include build hash, config, scene name, renderer, resolution, and asset set.

Example fields:

```text
timestamp_ms,scene,frame_ms,cpu_update_ms,cpu_render_ms,gpu_ms,draw_calls,
triangles,private_mb,virtual_mb,working_set_mb,texture_mb,asset_loads,
asset_bytes,resource_creates,long_frame_reason
```

### Better second pass

Add:

- GPU timestamp queries.
- Renderer state-change counters.
- Per-subsystem memory tags.
- Asset cache inventory dumps.
- Texture upload byte counters.
- Load queue timing.
- Optional in-game debug overlay.

### Deep-dive pass

Use external profilers:

- PIX/RenderDoc for GPU frame captures.
- WPR/WPA for CPU scheduling, IO, and waits.
- VMMap snapshots for address-space layout.
- Visual Studio profiler for CPU hotspots.

Deep-dive only after the minimal pass shows where to look.

## Test matrix

Run each baseline scene across a small controlled matrix.

| Variable | Values |
| --- | --- |
| Renderer | D3D9 baseline, D3D11 current |
| Resolution | 1280x720, 1920x1080, target high resolution |
| Texture set | Current, 2x subset, 4x subset |
| Geometry set | Current, higher static mesh, higher skinned mesh |
| Client config | Debug/dev, optimized/release if available |
| Duration | 2 min settled scene, 5 min traversal, zoning loop |

Keep the first matrix small. Expand only after the first results show a pattern.

## Run protocol

For each run:

1. Reboot or start from a clean client process.
2. Record build hash, config, renderer, resolution, and asset set.
3. Launch to login/character select.
4. Capture memory snapshot.
5. Enter target scene.
6. Wait for streaming to settle.
7. Capture settled 2-minute sample.
8. Run the scripted movement/traversal path if applicable.
9. Capture post-run memory snapshot.
10. Save logs, screenshots, profiler captures, and crash dumps.

Use the same camera anchors and movement paths each time. A rough but repeatable
test is more valuable than an impressive one-off capture.

## Analysis deliverables

Each evaluation pass should produce:

- Summary table by scene.
- Top bottleneck classification.
- Memory trend chart.
- Frame-time percentile table.
- Draw call/triangle/resource table.
- Asset memory table.
- Long-frame event list.
- Crash/failure summary.
- Recommendation: defer x64, continue measuring, optimize subsystem, or start
  x64 feasibility branch.

Suggested summary table:

| Scene | Private MB | Virtual MB | GPU MB | Avg ms | P99 ms | Draws | Bottleneck | Recommendation |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |

## Decision gates

### Gate 1: Is memory actually the limit?

Start x64 feasibility work only if one or more are true:

- Normal or target-upgraded scenes approach the practical 32-bit memory ceiling.
- Allocation failures occur under target assets.
- Address-space fragmentation blocks large allocations.
- The client needs to keep substantially more asset data resident to avoid
  unacceptable streaming hitches.

If none are true, x64 can stay a long-term modernization item.

### Gate 2: Is GPU the limit?

Prioritize renderer/assets instead of x64 if:

- GPU frame time is high while CPU and process memory are healthy.
- VRAM pressure appears before process memory pressure.
- Draw calls, shader cost, overdraw, or texture upload dominate.

### Gate 3: Is CPU the limit?

Prioritize engine optimization instead of x64 if:

- One CPU thread is saturated.
- Frame time is dominated by animation, culling, object update, or draw
  submission.
- Streaming hitches are caused by synchronous IO/decompression/upload rather
  than total memory capacity.

### Gate 4: Is content policy the limit?

Prioritize asset budgets if:

- Larger textures lack mips or compression.
- Higher-poly assets multiply materials/draw calls.
- Asset duplication dominates memory.
- Poor LODs cause avoidable CPU/GPU work.

## Suggested short-term recommendation

Because there may be no short-term need for native x64, keep this work as a
measurement track:

1. Add minimal counters when touching nearby systems.
2. Capture baseline D3D9/D3D11 numbers after major renderer milestones.
3. Create one synthetic high-texture/high-geometry test scene.
4. Revisit the x64 question only when measurements show memory headroom is the
   limiting factor.

This avoids spending months on an x64 port before proving that address space is
the real constraint.

