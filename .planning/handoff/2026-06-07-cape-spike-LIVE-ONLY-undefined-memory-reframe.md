# Cape/Jawa spike — LIVE-ONLY undefined-memory reframe (2026-06-07, resume point)

## 💡 LEADING HYPOTHESIS — KENNY'S RECONCILIATION: TERRAIN GEOMETRY + CREATURE SHADER via the SHARED SKIN RING (2026-06-07) — START HERE
Kenny's insight reconciles the two facts that looked contradictory (the buffer is TERRAIN per Cursor+Codex; the texture is ROBE/dewback-green per Kenny's screenshots): **the spike GEOMETRY is stale terrain vertices, drawn with the CREATURE's shader/texture, because terrain and creatures SHARE the D3D11 skin ring.**

Mechanism (fits EVERY observation):
- The **skin ring is my D3D11-only addition**: `Direct3d11_DynamicVertexBufferData::lock` routes ALL vSize≠28 && ≠24 to `ms_d3dSkinRingBuffer`. That is BOTH terrain (ClientTerrainSorter PNCT-36) AND the robe (SBSSP PNCT-36 / dot3). They co-mingle in one ring.
- The **creature draw, on the GPU**, reads a **stale tail** from that shared ring. Stale = previous ring content = **terrain** (terrain is the dominant vSize-36 writer, world-space, ground-level). So the robe's tail verts fetch terrain geometry at terrain world-pos [-2816,0,-6400] (Y≈0).
- It's the **robe's DRAW**, so the **robe shader+texture (fabric weave + dot3 green)** is bound → terrain-positioned shards wearing the robe texture. = Kenny's screenshots exactly.

Why this explains the contradictions:
- Cursor/Codex "it's terrain" (1647/2664 = ×9) = correct, that's the terrain CONTENT on the shared ring.
- Kenny "it's robe-textured" = correct, the creature DRAW reads it.
- CPU outlier scan said the creature VB was sane = correct, the **CPU read is SYNCED** (sees the real write); the **GPU reads the stale ring UNSYNCED** = CPU/GPU coherency divergence = the original "live-only / capture-invisible / sync-masked" signature.
- D3D11-only = the shared skin ring is D3D11-only.
- Force-discard didn't fix it, only changed flicker = discard swapped the stale source (terrain-append → driver-uninitialized) but the GPU still reads an unwritten/stale tail.

**DECISIVE TEST (do FIRST next session, plugin-only build ~2min): STOP SHARING THE RING.** Isolate the creature's VB from terrain — simplest = REVERT the skin ring entirely (make `isSkin` always false so everything → main ring `ms_d3dRingBuffer`), OR give terrain its own ring. If the robe-textured terrain shards die, the shared-ring coherency/cross-contamination is CONFIRMED and the fix is ring isolation (the skin ring "never helped" per [[feedback_...]] anyway). If they persist, the GPU/CPU coherency is intrinsic to a single dynamic ring and the fix is per-skinned-lock WRITE_DISCARD + matching the draw vertex range. Current plugin state to revert when testing: force-discard-on-skin + 0x7F sentinel + sentinel-aware Probe B are still in `Direct3d11_DynamicVertexBufferData.cpp`; SBSSP has Probe A (path log + systemStream measure) + Probe D (copycount); ClientTerrainSorter has NO probe (the edit was never applied). Clean these diagnostics out when the fix lands.

## 🔄 MAJOR REORIENTATION — TERRAIN WAS A RED HERRING; SPIKE IS CREATURE + GPU-SIDE (2026-06-07 latest⁵) — superseded by Kenny's reconciliation above
**Kenny's texture clue is decisive: the spikes are CREATURE-textured — robe texture on robed NPCs, and the dewback's spikes are GREEN (dewback skin). So the spiking geometry is the CREATURE, rendered with the creature's shader — NOT terrain (which would be sand-textured).**

CONSULT-33 (find the vSize=36 world-space VB writer) results:
- **Cursor + Codex (independently): the 1647/2664 buffer = TERRAIN** (`ClientTerrainSorter::PrimitiveNode::prepareToDraw`, clientTerrain/.../ClientTerrainSorter.cpp:294; local DynamicVertexBuffer, lock(numberOfVertices), writes 9 verts/primitive world-space). `1647=183×9`, `2664=296×9`, tail `27=3×9` = three stale terrain primitives. Bulletproof.
- **Opus: the batch renderer** dangling-transform mechanism (submit stores `&appearance.getTransform_w()` pointer; NPC moves between submit and deferred draw → stale transform → tail spike). Clean live-only mechanism, BUT batch is dormant (cape-batch-probe absent) + 28-byte format + a single dewback wouldn't batch.

**THE REORIENTATION (cost ~15 builds):** my CPU-side outlier scan (Probe B) is CENTROID-based → it flags WORLD-space terrain (huge outliers) but is BLIND to the creature spike. The creature's CPU dynamic VB is SANE (faithful memcpy of the sane object-space systemStream). So **the 1647/2664 terrain buffer I chased is a SEPARATE real-but-likely-not-the-visible-bug** (terrain has its own 3-stale-primitive tail). **The VISIBLE creature spike is GPU-SIDE: a clean CPU buffer the GPU renders as spikes** = the ORIGINAL "live-only / capture-invisible / sync-masked / undefined-memory" reframe, full circle, now localized to: CREATURE, GPU-side, NOT in the CPU VB. Force-discard on the skin ring did NOT fix the creature spike → not the skin-ring append.

**NEXT (creature, GPU-side):** stop using the centroid CPU scan (it sees terrain, not the creature). Detect the spike at the CREATURE's DRAW by shader/texture, OR examine what applies a per-vertex/per-sub-mesh or GPU transform to the creature that can go stale for a tail (Opus's dangling-pointer CLASS is the right idea even if the specific batch path is dormant). Open question: does the dewback/robe go through a GPU hardware-skinning or world-bake path with a per-bone/per-sub-mesh cbuffer that has an uninitialized/stale tail entry? The creature spike being tail-only + GPU-side + live-only + D3D11-only + creature-textured is the fingerprint to chase. Terrain (1647/2664) tail bug is real but separate — fix later.
**IMAGE CONFIRMATION (screenShot, Kenny): the shards carry the ROBE FABRIC WEAVE (diagonal cross-hatch) + GREEN dot3/bump edging → definitively the creature/robe garment, and it's a DOT3 mesh.** dot3 affects only lighting, not position → the GEOMETRY spike is a bad POSITION READ on the GPU while the CPU VB is sane → GPU-CPU divergence on the skin ring. So next session: (1) the robe is a dot3 SBSSP mesh (PATH log had hasDot3=1 / skinMode=2 / vc=1906); confirm the robe's specific draw; (2) the spike is GPU-side position — chase either the GPU reading the skin-ring slice differently than the CPU wrote (undefined-memory/coherency on the skin ring; note force-discard did NOT fix it), or a hardware/GPU per-vertex transform with a stale tail entry; (3) STOP using the centroid CPU outlier scan (it sees terrain, blind to the creature). Consider: does the robe go through a path where the VB holds bind-pose + the GPU applies per-bone matrices (a bad/uninitialized tail bone = tail spike, CPU-VB-sane)? Codex's earlier hardpoint-unfilled-transform finding (transformCount includes hardpoints, calculateBindPoseModelToRootTransforms TODO at Skeleton.cpp:1546) fits a GPU-side bad-tail-bone if the robe is GPU-skinned.
**SUGGEST COMPACT NOW: context is critically heavy after a very long session; the reorientation above is the key state to resume from.**

## 🚨 SPIKE WRITER IS NOT SBSSP AND NOT THE BATCH RENDERER (2026-06-07 latest⁴) — superseded by reorientation above
Direct measurement (cape-trace-path.txt + cape-trace-systemstream.txt + cape-dynvb-outlier.txt, correlated by numVerts) BROKE the contradiction:
- **SBSSP systemStream = OBJECT space** (vert0 ~[-0.2,2.8,0.2], maxDist 3-4, `ss-tight`), meshes 1595/1906/2275. SANE.
- **Spike dynamic VB = WORLD space** (centroid ~[3307,4.9,-5002] = NPC world pos), meshes **1647 / 2664**, tail at [-2816,0,-6400].
- **The spike numVerts (1647/2664) appear in ZERO SBSSP logs** (path/systemstream/copycount all 0). All 3 SBSSP `m_dynamicStream->lock` sites (758/775/839) are inside prepareToDraw, which logs PATH for every vc>=1000 — so the spike mesh NEVER enters SBSSP::prepareToDraw.
- **Batch renderer**: VB format = pos+normal+color = **28 bytes ≠ 36**, and cape-batch-probe ABSENT (dormant). Ruled out.
- `new DynamicVertexBuffer` sites in repo: SBSSP, batch, + TextureBuilder(tool)/SpaceTargetBracketOverlay/BinkVideo/DynamicColorPolyPrimitive/StarAppearance/CuiLayerRenderer(UI). NONE obviously write world-space NPC skinned geometry.

**⇒ An UNKNOWN code path writes a WORLD-space, vSize=36 (pos+normal+color+1×2D-uv), ~1647/2664-vert dynamic VB on the D3D11 skin ring, with a stale tail. We were instrumenting the wrong file all along.** CONSULT-33 dispatched (Cursor `CONSULT-33-cursor.out`, Codex `-codex.out`, Opus `-OPUS.out`) to find the writer: any path that locks a 36-byte dynamic VB and bakes object→world on the CPU (shadow volumes / MeshAppearance / ShaderPrimitiveSet / DetailAppearance / particle-swoosh / a non-obvious VB factory). NEXT: read the 3 CONSULT-33 outputs, identify the writer, instrument IT (the world-bake loop + its lock-vs-write count), find the stale-tail mechanism (dangling per-sub-mesh transform pointer or count mismatch). The fingerprint: tail-only, NPC-centered world-space, D3D11-only, skin ring.

## 🔬 NARROWED TO THE D3D11 SKIN-RING COPY/BIND (2026-06-07 latest³) — READ FIRST
Whole-path probes ran. Hard facts now:
- **Probe A (systemStream, reads actual bytes, covers soft+hard fill): no spike** (>100-from-vert0 gate empty). Skin output appears tight/sane.
- **Probe D (copy count vs lock count): 0 MISMATCH** (copyN==lockN) — BUT only captured small meshes (<1000); the 1647/2664 spike meshes weren't in Probe D's sample.
- **Probe B (dynamic VB post-copy): spike, ALWAYS `ring=SKIN`.** firstPos=[-2816,0,-6400], centroid WORLD [3307], tail verts [firstIdx..numVerts).
- **Force-discard on skin lock: spike PERSISTS but LESS FLICKERY** (stale source changed from per-frame-append to fresh-discard-uninitialized).
- **Sentinel (0x7F fill before copy) + sentinel-aware Probe B: `UNWRITTEN(sentinel)=0`** → the tail is NOT unwritten; it's OVERWRITTEN with real world-space garbage. So NOT read-past.

**THE IRONCLAD CONTRADICTION:** systemStream sane (Probe A) + full copy (sentinel gone) + dynamic tail = real garbage [-2816]. A memcpy can't do that. ⇒ Either (a) the spike mesh takes a path I'm not instrumenting (multi-stream copy@740 vs single-stream copy@820 — Probe A/D only in single-stream), or (b) the copy's dest (`m_dynamicStream->begin()`) and the GPU-bound/Probe-B-read pointer (`s_lastLockedSliceCPUPtr`) DIVERGE on the skin ring after a WRITE_DISCARD rename (copy writes the right bytes to the wrong place; GPU reads stale). (b) is the leading hypothesis.

**THIS BUILD (running): direct measurement, not inference** — `cape-trace-path.txt` (PATH vc/multiStream/hasSystemStream/skinMode for big meshes) + Probe A rewritten to log the FARTHEST systemStream vert UNCONDITIONALLY for big meshes (`SS-HAS-SPIKE`/`ss-tight`, with vert0/lastVert/maxPos/mode). DECISIVE READS: (1) does the 1647 mesh appear in cape-trace-path.txt and is it single or multi-stream? (2) does cape-trace-systemstream show ss-tight (→ confirms copy/bind-pointer divergence, chase begin() vs s_lastLockedSliceCPUPtr on skin ring after discard) or SS-HAS-SPIKE [-2816] (→ skin source has it after all)? Plugin still has force-discard + sentinel + sentinel-aware Probe B.

## 🧭 RENDER-PATH REFERENCE + WHOLE-PATH PROBING (2026-06-07 latest-latest) — READ FIRST
**CONFIRMED via two-sided probe: the spike enters AFTER skinning, in the D3D11 dynamic-VB/draw layer.** systemStream (skin output) sane; dynamic VB (post-copy, GPU-read) spiked — same mesh, same frame (`cape-skin-spike.txt` empty, `cape-dynvb-outlier.txt`: numVerts=1692 outliers=27 firstIdx=1665 firstPos=[-2816,0,-6400] centroid=[3312,4.9,-5002] WORLD-space).

**Built a render-path reference (Kenny's "debugging tool"):** `.planning/RENDER-PATH-REFERENCE.md` (merged from `research/RENDER-PATH-TREE-{cursor,codex}.md` — full D3D9∥D3D11 method-call trees). Leaf path = SBSSP, `SM_hardSkinning` → `fillVertexBufferHard` (SSE @2305), single-stream system→dynamic copy, skin ring. Both AIs rank the **D3D11 dynamic-VB ring/offset/generation** as #1 suspect. Top D3D9-vs-D3D11 divergences: (#1) dual ring (skin ring vSize≠28/24); (#2) D3D9 resets rings in beginScene, D3D11 plain-ring beginFrame has NO caller (skin ring IS reset Direct3d11.cpp:297); (#4) D3D9 DrawIndexedPrimitive bounds vertex fetch (MinVertexIndex/NumVertices), D3D11 DrawIndexed does NOT → read-past stale. **Walk order W1-W5 in the reference.**

**WHOLE-PATH PROBES now in the build (correlate by numVerts):**
- Probe A `cape-trace-systemstream.txt` (SBSSP.cpp post-skinData ~770): reads ACTUAL systemStream position bytes — covers soft AND hard/SSE fill. Spike here = skin source; sane here = after-skin.
- Probe B `cape-dynvb-outlier.txt` (Direct3d11_DynamicVertexBufferData::unlock): dynamic VB post-copy, now tagged ring(SKIN/MAIN)/discard/lockOff.
- Probe C: lock context (isSkin/discard/offset) feeding B.
- Existing: cape-skin-spike.txt (soft blendPosition), cape-stage-probe (draw-time staging readback, collapse/nearZero — BLIND to outliers).
DECISIVE NEXT READ: does `cape-trace-systemstream.txt` have an SS-SPIKE for numVerts=1692/2691? YES→ hard fill is source (instrument fillVertexBufferHard SSE @2305). NO→ pure D3D11 ring/offset; B's ring/discard/lockOff fields localize it (then W2/W3/W4: verify bind-offset==write-pointer, read-past with real draw counts, diff D3D9 live).

## ⚠️ STATUS UPDATE (2026-06-07 latest) — OOB refuted; "enters AFTER skinning" under test
Two hypotheses below were REFINED by later runs — read this first:
- **OOB bone index: REFUTED.** Added a release clamp on `m_transformIndex` to [0,transformCount): caught ZERO out-of-range. Indices are all IN RANGE. The clamp did NOT fix the spike.
- **Skin-output-is-the-source: under question.** A spike-capture probe in `fillVertexBuffer` (logs any vert whose skinned `blendPosition` is >100u from the mesh's vertex-0) produced ZERO hits across the captured meshes → **the CPU skinning output looks SANE.** BUT the spike mesh (vSize=36, ~1656/2628 verts) is intermittent and didn't reliably render through the probe in those runs — so "sane" is not yet airtight.
- **Kenny's call: "it enters AFTER skinning"** (skinning writes sane positions; spike appears in the systemStream→dynamic copy or the D3D11 VB). This is the leading hypothesis now.
- **CODEX-31 strong candidate (if it IS in skinning):** `transformArray = skeleton.getBindPoseModelToRootTransforms()`, and `transformCount` INCLUDES HARDPOINTS (Skeleton.cpp:366-373) but `calculateBindPoseModelToRootTransforms()` does NOT fill hardpoint entries (only a TODO at Skeleton.cpp:1546-1547). A vert weighted to a hardpoint gets an in-range index whose PoseModelTransform is never written → stale → spike. Also joints skipped via `continue` (Skeleton.cpp:1404-1411) can propagate stale jointToRoot. CURSOR-31 REFUTED the transformData pointer-desync (it's per-vertex balanced).
- **ACTIVE TWO-SIDED TEST:** skin probe (systemStream, exe, `cape-skin-spike.txt`) + plugin dynamic-VB outlier scan (`cape-dynvb-outlier.txt`, post-copy). When the spike fires: outlier in BOTH → skinning is source (chase Codex hardpoint); outlier in dynamic-VB ONLY → the COPY introduces it (Kenny right, chase Direct3d11 copy/VB). Next: get the spike to render while both probes are live, compare the two logs.

## 🎯🎯 ROOT CAUSE (earlier hypothesis, NOW REFINED — see status above)
The full diagnostic chain landed. The spike is a **CPU soft-skin out-of-range bone index**, NOT anything D3D11-buffer-related.

**Proof chain (each step ran in-game, gl11_r.dll plugin probes, no exe rebuild until the last):**
1. **Outlier scan** (read engine-written positions, flag verts far from centroid): found spikes are the **contiguous TAIL** of certain skinned meshes — call1 `numVerts=1656 outliers=27 firstOutlier idx=1629` (1656-1629=27 exactly). Positions are WORLD-space (`l2p` = local-to-parent), centroid = player/NPC world pos `[3309,4.88,-5002]`, spike verts flung to `[-2816,0,-6400]` (Y=0).
2. **Sentinel under-write test** (fill slice 0x7F before copy, count untouched tail in unlock): **387 draws, 0 UNDERWRITE, all `full`** incl. big skinned `lockVerts=2079`. → the copy writes EVERY vert. **Under-write / stale-ring / ring-confound ALL DEAD.** The spikes are REAL skinned output the copy faithfully delivered.
3. **Source read** (SoftwareBlendSkeletalShaderPrimitive.cpp): skin loop `transformArray[firstTransformData.m_transformIndex]` (lines 1862-1881 dot3 / 1905-1938 non-dot3). `VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE` guards it but is a **NO-OP in release** (we run `_r`). So an OOB `m_transformIndex` reads `transformArray[]` out of bounds → garbage PoseModelTransform → spike. The zero-weight branch (line 1359) is NOT it (block is memset-0 at 1316 → extraCount=0 → vert→world-origin, but bbox has no origin verts).

**Why D3D11-ONLY (the reconciliation we were missing):** this skin code is SHARED, but the **mesh→primitive-class choice upstream differs** — D3D11 routes these robes through `SoftwareBlendSkeletalShaderPrimitive` (SM_softSkinning, CPU, runs this buggy loop); D3D9 hardware-skins them (GPU), never executing `fillVertexBuffer`. So the latent OOB-index bug only fires under D3D11.
**Why LOD-gated ("fine until I get close"):** only the HIGH-LOD mesh variant references the bad bone index; low-LOD (far) doesn't. The "distance gate" was an LOD gate.

**FIX IN FLIGHT (building now):** added a PROBE+GUARD to the non-dot3 skin loop (SoftwareBlendSkeletalShaderPrimitive.cpp ~1905): clamp `m_transformIndex` to `[0,transformCount)` (release VALIDATE is a no-op), count OOB hits per vert, log offenders to `stage/cape-skin-spike.txt` (`rawFirstIdx`, `transformCount`, `extraCount`). **This is a SHARED file → requires clientSkeletalAnimation lib rebuild + SwgClient_r.exe relink** (heavier than the plugin). One run will BOTH confirm OOB indices (log) AND likely kill the spike (clamp). NOTE: only the non-dot3 branch is guarded so far; add the dot3 branch (1848-1900) for the real fix if a dot3/vSize=60 mesh also spikes. Build via swg.sln projects: clientSkeletalAnimation.vcxproj then SwgClient.vcxproj (Release/Win32). May need to delete SwgClient_r.exe to force relink.

---

## ⭐ SESSION-END RESULT (2026-06-07 PM) — POST-WRITE-ZERO IS DECISIVE; START HERE NEXT
Ran a **post-write zero** (memset the slice in `unlock()` AFTER the engine skins, right before Unmap — Direct3d11_DynamicVertexBufferData.cpp ~line 472, gate `numberOfVertices ∈ [16,8192)`). Kenny's result: **"I see the Jawa's eyes, but nothing else on the Jawa; landspeeder renders fine"** + UI mostly gone (expected — broad gate hits HUD dynamic VBs).

**What it PROVES (hard facts now):**
1. The robe collapsed to nothing → **the robe's soft-skin geometry IS in the dynamic slice we memset.** The GPU reads exactly the buffer we control. → **SKIN-RING CONFOUND (Sonnet Rank 7) RULED OUT.** Don't revert-the-ring as a fix; that's not it.
2. Eyes survived → separate mesh/lock outside [16,8192) gate. Landspeeder static. Consistent.
3. Pre-write zero (earlier run) was a NO-OP → **engine fully writes the slice** (no position under-write; a zeroed tail would've collapsed under the pre-test too). Read-past also dead (overRead=0 AND post-zero collapsed cleanly with zero residual spikes).

**THE CORRECTION (overturns the old "VB is clean" belief):** engine fully writes the slice AND GPU faithfully reads it (both proven) → for spikes to exist, **the engine is writing spike positions INTO the slice.** Our "11,724 clean draws" cape-stage-probe MISSED them because its heuristic only checked **collapse** (identical-position runs) + **nearZero** (clipW<0.1). A spike is the OPPOSITE: verts flung FAR APART, LARGE clipW. **The probe was structurally blind to outliers.** "Clean" only ever meant "not collapsed." The spikes were almost certainly in the CPU-written data all along.

**NEXT DECISIVE TEST (queued, not yet built):** in `unlock()` for the robe-sized slice, compute the mesh **centroid** and **log any vert whose distance from centroid exceeds a threshold** (outlier = spike), with its index + position. Outliers present in the CPU-written slice → it's a **CPU soft-skin miscompute** (bad bone index / weight overflow / uninitialized transform in SoftwareBlendSkeletalShaderPrimitive::fillVertexBuffer/skinData) — hunt THERE, not in D3D11. No outliers → GPU-side/race after all (less likely now). This replaces the collapse/nearZero heuristic that fooled us.
(zero-on-RenderDoc-replay still fits: RenderDoc is known-unreliable capturing WRITE_NO_OVERWRITE dynamic sub-writes → replays zeros → "clean" replay; not proof the data was clean.)

**INSTRUMENTATION ADDED THIS SESSION (remove later):** Direct3d11_DynamicVertexBufferData.cpp lock() has BOTH a pre-skin memset (line ~347) AND a post-skin memset in unlock() (~line 472), both gated [16,8192). The post-skin one collapses all soft-skin + HUD — it's a DIAGNOSTIC, revert before any real run. Built+staged gl11_r.dll 14:30:44.

---

## THE REFRAME (the breakthrough — Kenny's observation)
When Kenny presses F12, the **live frozen frame is heavily spiked**, but the **RenderDoc thumbnail/replay is nearly clean** (only small spikes). That is the **"reads uninitialized memory that RenderDoc zero-inits on replay"** signature — and it is *exactly* the day-1 memory note we drifted away from. Consequences:
- **RenderDoc replay LIES** (zero-inits the undefined memory → shows clean) → made us doubt the VB.
- **The staging probe's blocking `Map` LIES** (forces a GPU sync that *defines* the undefined memory → reads clean) → made the VB look innocent.
- **The live screen is the only honest instrument.** This is why the probe reports 11,724 clean draws while the eyes see spikes.

## KEY DEDUCTION
Kenny saw spikes **while the per-draw blocking-Map probe was active**, and that probe reads the VB **position (offset 0) clean**. A per-draw GPU drain did NOT stop the live spike → **NOT a timing/pipeline race**. So the GPU is reading **UNINITIALIZED memory that is NOT the position-at-offset-0 the probe checks** — bytes that are zero on replay, stale/garbage live.

## ELIMINATED (with evidence — do not re-chase)
- **VB position content**: 11,724 NPC draws, every one `nearZero=0`, no flung verts, no collapse (draw-time staging read).
- **Read-past via index**: `overRead=0` all sampled prims (`maxIdx < m_vertexCount`).
- **Tri-strips**: dormant — `addTriStripVertex` has ZERO callers, skeletal IFF only loads `TAG_ITL`/`TAG_OITL`, `cape-strip-probe.txt` empty (Cursor 29-B).
- **Skeletal BATCH renderer** (`FullGeometrySkeletalAppearanceBatchRenderer`, draws via non-partial `drawIndexedTriangleList` — NOT instrumented by our probe): **dormant** — `cape-batch-probe.txt` never produced (Codex 29-A flagged it as the one uninstrumented path, but it didn't run).
- **Ring rename / WRITE_DISCARD hazard**: draws are back-to-back (`prepareToDraw`→`draw` per primitive, ShaderPrimitiveSorter.cpp:725/781); Sonnet/Cursor/Codex all confirm immediate-context submission order makes the rename impossible for back-to-back draws. 16MB dedicated skin ring did NOT help.
- **Transform/WVP for the actual NPCs**: real-W NPC draws have `clipW≈24` (normal). The `clipW≈0` draws were ALL `sul_m` = the PLAYER ("Little Bigman", a Sullustan, camera-relative identity-W) which renders FINE. The player was a red herring for ~6 messages.
- **D3D9 "clamps"**: NO. D3D9 uses HARDWARE vertex processing (config default, hw succeeds, no cfg override) → same GPU clip unit as D3D11. Codex+Cursor+Opus(CONSULT-28) unanimous: not a clipping difference. (Canonical answer recorded — stop re-asking.)

## ACTIVE EXPERIMENT (awaiting Kenny's run)
**memset-zero under-write test.** In `Direct3d11_DynamicVertexBufferData::lock()` (~line 338, after `s_lastLockedSliceCPUPtr`), for bounded soft-skin locks (`numberOfVertices >= 16 && < 8192`), `memset(sliceStart, 0, length)` BEFORE the engine skins into it. Logic: a fully-written mesh overwrites the zeros (no change); an UNDER-written soft-skin tail stays ZERO → origin verts = exactly what RenderDoc replay shows.
- **Spikes SHRINK to match the capture's small spikes** → CONFIRMED: the draw reads past the skinned verts into stale ring memory (garbage live, zero replay). Then find the count/offset source.
- **Spikes UNCHANGED** → not an under-write of position; pivot to the input-layout-reads-unwritten-component angle (Cursor 30).
- Build STAGED 14:08:30 (`stage/gl11_r.dll`), logs cleared. RUN IT.

## CONSULTANTS IN FLIGHT (CONSULT-30, outputs in .planning/research/)
- **Cursor 30** (`CONSULT-30-cursor.out`): does the D3D11 input layout for the skeletal VS read a vertex component at an offset/stride the CPU skin never writes? (Position-at-0 clean but VS reads garbage elsewhere.) Byte-map of dynamic vertex: offset|component|written?|read?.
- **Codex 30** (`CONSULT-30-codex.out`): can a soft-skin draw fetch from an unwritten (undefined-since-DISCARD) ring region? Stride/offset consistency: `vertexSize` used for `m_offset`/bind vs the input-layout fetch stride.
- **Sonnet 30** (`CONSULT-30-SONNET-lateral.out`): lateral brainstorm of ALL root causes fitting garbage-live/zero-replay/sync-masked/position-clean.

## CONSULT-30 RESULTS (Cursor + Codex in; Sonnet lateral still pending)
- **Cursor 30** (`CONSULT-30-cursor.out`): NO unwritten declared vertex component. Single-stream `dynamicFormat` = position+normal(+color+UVs+dot3); `skinData` writes position/normal/dot3 each frame, `fillConstantVertexBufferData` writes color/2D-UVs once; copy is full-vertex memcpy → every declared byte populated at draw. Input-layout offsets/stride match engine exactly. VS uses POSITION-only for SV_Position. → REFUTES "VS reads an unwritten component." Points to **index read-past (whole stale vertices)** or **"GPU sees different memory than the CPU staging read"** (coherency/ring-timing).
- **Codex 30** (`CONSULT-30-codex.out`): the ONLY viable read-past = **index value >= written vertex count** (stride, `m_offset`/bind byte math, and `BaseVertexLocation=0` all verified correct; no offset-lifetime/stale-binding issue). 
- **TENSION**: both converge on index-read-past, but `cape-index-probe` measured `overRead=0` (all indices < m_vertexCount). So EITHER read-past on an unsampled prim, OR Cursor's other branch: the GPU reads dynamic-ring memory the synced CPU read cannot see (coherency/in-flight-write timing). Both fit garbage-live/zero-replay/sync-masked/position-clean.
- **The memset-zero test decides**: spikes collapse to origin under memset → GPU reads unwritten/zeroed VB bytes (read-past or coherency), CONFIRMED. Unchanged → not the VB; pivot (cbuffer / a different resource RenderDoc auto-zeros).

## SONNET 30 (lateral) — net
Top ranks recycled already-killed ideas (strip=dormant, multi-stream=off, systemStream-uninit would be captured-not-zeroed). Two FRESH + fitting: **(Rank 5) STALE CACHED InputLayout stride mismatch** — IA stride 48 vs a cached layout expecting 32 → verts read N bytes off, position-at-0 still correct (fits "position clean"); Cursor 30 only checked the CURRENT format, not a stale cached layout — WORTH CHECKING the input-layout cache key/stride at garment draws. **(Rank 7) `getRingBuffer()` NULL-fallback to MAIN ring** if a garment binds before its first lock (m_ringBuffer==nullptr) — a risk introduced by the 16MB skin-ring split; reverting the skin ring removes it. Full: `CONSULT-30-SONNET-lateral.out`.

## LEADING HYPOTHESIS NOW
**The VS input layout reads vertex bytes the skin path never writes** (a declared-but-unwritten component — tangent/binormal/dot3 set, 2nd texcoord, or a stride/offset mismatch), so position-at-0 reads clean but the VS consumes uninitialized bytes → garbage live, zero replay. OR an under-write (skin writes M < draw's N). The memset test + Cursor 30 byte-map decide between them.

## INSTRUMENTATION STATE (all DIAGNOSTIC — remove when fixed)
- `Direct3d11_DynamicVertexBufferData.cpp` lock(): memset-zero test (above) + 16MB skin ring (`ms_d3dSkinRingBuffer`, `isSkin = vSize!=28&&!=24`) + cape-d3d11-probe in unlock().
- `Direct3d11_StateCache.cpp` drawPartialIndexedTriangleList: cape-stage-probe (player-excluded via owtTrans, every-NPC-draw, 20k cap) + cape-wvp-matrix dump + `s_owtSeq/s_owtLastT*` in setObjectToWorldTransformAndScale.
- `SoftwareBlendSkeletalShaderPrimitive.cpp`: shadow-volume force-disabled (`if (false && ...)`, ~786 — EXONERATED, revert later); cape-index-probe + cape-collapse-probe + iter36d-sbssp-transform logger.
- Captures: stage/Capture18-22.rdc (18=Jawa+alpha0, 20=Jawa charselect, 22=3rd-person 3-Jawa spike thumbnail clear). RenderDoc replay zeroes the spike — captures are LIMITED; debug_vertex/export_mesh show clean (zeroed). Live thumbnail (embedded JPEG) shows the real spike.

## CONSULT CLI (recall)
codex: `C:\Users\kenne\AppData\Roaming\npm\codex.cmd exec --skip-git-repo-check --sandbox read-only -` (prepend `C:\Program Files\nodejs` to PATH). cursor: `C:\Users\kenne\AppData\Local\cursor-agent\cursor-agent.cmd -p --mode ask --trust --output-format text`. Both stdin<file, redirect>out, auto-detect UTF-16/8. Sonnet/Opus = Agent tool (model sonnet/opus, can read repo).
