# Diagnostic Remnants Audit — Cape/Creature Spike Debugging

**Branch:** koogie-msvc-cpp20-base
**Date:** 2026-06-07
**Scope:** Uncommitted working-tree diff (`git diff HEAD`) of the 8 named C++ files. All findings are NEW (uncommitted) diagnostic code added during the cape/creature-spike investigation unless explicitly marked "(committed / prior phase)".

Buckets:
- **[A] BEHAVIOR-CHANGING** — alters what is rendered/computed/written. Can CAUSE or MASK the bug.
- **[B] LOGGING-ONLY** — file writes / counters, no rendering effect.
- **[B-SYNC] LOGGING + GPU DRAIN** — adds a blocking `Map(READ)` / staging readback that drains the GPU. MASKS live-only bugs by forcing CPU/GPU serialization.
- **[C] DEAD/REVERTED** — `#if 0`, `#define ...=0`, `if(false)`, commented out. No active side effect.

---

## PRIORITY SECTION — contaminating items (fix/remove FIRST)

These are the [A] ACTIVE behavior-changers and [B-SYNC] GPU-draining readbacks that pollute measurements. Until these are removed/reverted, NO measurement of the spike is trustworthy.

### [A] ACTIVE behavior-changing remnants

| # | file:line | what it does | could it affect the spike/measurement |
|---|-----------|--------------|----------------------------------------|
| A1 | Direct3d11_DynamicVertexBufferData.cpp:79 `#define P19_TERRAIN_SPIKE_CLAMP 1` (active block at ~:604) | **ACTIVELY MUTATES VERTEX DATA before Unmap**: scans every dynamic-VB slice (32..8192 verts, vSize>=24) for "outlier" verts >`8*meanDist+50` from centroid and **rewrites their xyz to the centroid**. | **YES — TOP CONTAMINANT.** This silently *hides the very spikes being investigated* by collapsing them to the centroid on the live screen. Any "spike gone / spike present" observation while this is ON is meaningless. It also corrupts legitimate geometry (any genuinely spread mesh gets clamped). REMOVE. |
| A2 | Direct3d11_DynamicVertexBufferData.cpp:140 `getSharedRingBuffer()` → `data->getRingBuffer()` ; + whole **dedicated soft-skin ring** (install :160-175, lock routing :293-340, getNumberOfLockableDynamicVertices :629-642, beginFrameSkin, Direct3d11.cpp:288 update hook, VertexBufferVectorData.cpp:143, StateCache.cpp:2277) | Routes all skinned (vSize!=28 && !=24) dynamic VBs to a **separate 16 MB ring** with its own once-per-frame WRITE_DISCARD reset, instead of the shared 2 MB main ring. Bind sites switched to per-instance `getRingBuffer()`. | **YES.** This is a structural change to skinned-geometry upload/binding — exactly the subsystem under investigation. It changes ring offsets, discard cadence, and buffer identity for every skinned draw. It is an experimental fix left active; it changes the baseline rendering path and must be a deliberate decision, not a leftover. |
| A3 | SoftwareBlendSkeletalShaderPrimitive.cpp:876 `if (false && ShadowManager::getEnabled() ...)` | **Force-disables skeletal shadow-volume build+render** via `false &&`. Skeletal shadow volumes never built/drawn. | **YES.** Changes what is rendered (no skeletal shadows). Was a test ("are spikes a degenerate shadow-volume extrusion"). If left in, it both alters output and could MASK a shadow-volume-sourced spike. REVERT the `false &&`. |
| A4 | SoftwareBlendSkeletalShaderPrimitive.cpp:~2000-2070 (fillVertexBuffer CONSULT-30 guard) | **Replaces `VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE` with a silent clamp**: out-of-range `m_transformIndex` is clamped to `transformCount-1` instead of asserting. Changes skinning output for any OOB bone index. | **YES.** Behavior-changing: a vert that would have read OOB now mis-skins to the last bone. This *changes* the spike's appearance (and could partially mask it) rather than observing it. Note: this also removes a debug assert. REVERT to the validate macro for a clean baseline. |
| A5 | Direct3d11_DynamicVertexBufferData.cpp:~395 CONSULT-32 **sentinel memset** (`if (isSkin) memset(sliceStart, 0x7F, length)`) | **Pre-fills every skin-ring lock slice with 0x7F (~3.4e38) before the engine writes.** | **YES.** If the engine ever under-writes a slice (the read-past hypothesis), the unwritten tail now contains 0x7F garbage (huge floats) instead of the previous ring generation's data — this *changes* what a read-past draw fetches, altering the spike's on-screen value/position. It is a probe aid but it mutates buffer contents on the live path. REMOVE. |

### [B-SYNC] GPU-draining readbacks (MASK live-only bugs)

| # | file:line | what it does | why it contaminates |
|---|-----------|--------------|---------------------|
| S1 | Direct3d11_StateCache.cpp:~3300 (drawPartialIndexedTriangleList — "CAPE-COLLAPSE STAGING READ-BACK") | Before each qualifying skinned `DrawIndexed`, `CopySubresourceRegion` to a staging buffer then **`Map(D3D11_MAP_READ)`** — a blocking CPU stall until the GPU drains all prior work. ACTIVE (sampled per real-W NPC draw, up to 20000 logs). | **YES — CRITICAL.** Per the project memory, the spike is *live-only / capture-invisible / masked by any CPU↔GPU sync*. A per-draw `Map(READ)` is exactly such a sync — it can make the spike vanish while the probe runs, producing a false "clean" reading. The probe's own comment admits this ("If the spike vanishes while this runs -> CPU/GPU ordering hazard"). REMOVE. |
| S2 | Direct3d11_StateCache.cpp:~3490 (drawPartialIndexedTriangleStrip — "CAPE-COLLAPSE STRIP READ-PAST PROBE") | Same pattern for the tri-STRIP path: `CopySubresourceRegion` + `Map(READ)` before `DrawIndexed`, stride==48 strips. ACTIVE. | **YES.** Same GPU-drain masking as S1, on the strip draw path. REMOVE. |

> Also note A1 (TERRAIN_SPIKE_CLAMP) and S1/S2 are the three items that most directly corrupt the experiment: A1 erases spikes by data mutation; S1/S2 erase them by GPU serialization.

---

## FULL PER-FILE INVENTORY

### Direct3d11.cpp

| file:line | bucket | what it does | active? | affects spike? |
|-----------|--------|--------------|---------|----------------|
| Direct3d11.cpp:288 `update_impl` → `beginFrameSkin()` | A | Per-frame reset hook for the dedicated soft-skin ring (part of A2). | yes | yes — part of the soft-skin ring change (A2). |

### Direct3d11_Device.cpp

| file:line | bucket | what it does | active? | affects spike? |
|-----------|--------|--------------|---------|----------------|
| Direct3d11_Device.cpp:868-902 `#if 0` CONSULT-23 race-test full CPU↔GPU serialization | C | Full-frame GPU spin-wait (EVENT query) to test frame-level race. `#if 0`. | **no** (#if 0) | no — dead. Note: if ever re-enabled it is a [B-SYNC] full drain. Kept #if'd for the record. |

### Direct3d11_DynamicVertexBufferData.cpp / .h

| file:line | bucket | what it does | active? | affects spike? |
|-----------|--------|--------------|---------|----------------|
| .cpp:51-52 `P19_THREAD_AUDIT 0`, `P19_DISCARD_ONLY 0` | C | Thread-assert / force-discard-every-lock toggles. | no (=0) | no — dead. (DISCARD_ONLY=1 would be [A].) |
| .cpp:69 `P19_SKIN_RING_DISABLED 0` | C | When 1, routes everything to main ring (disables skin ring). Currently 0. | no (=0) | no — dead toggle. (Inverse of A2.) |
| .cpp:79 `P19_TERRAIN_SPIKE_CLAMP 1` + block ~:604 | **A** | **Clamps outlier verts to centroid before Unmap.** | **YES** | **YES — see A1.** |
| .cpp:85 `P19_FORCE_SKIN_DISCARD 0` + block ~:330 | C | The infamous `forceDiscard=true` on every skin lock. **Now OFF.** | no (=0) | no — dead NOW, but this is the toggle that "polluted measurements for weeks." Keep at 0; the [A] danger is if it flips to 1. |
| .cpp install :160-175, h:96-100 — `ms_d3dSkinRingBuffer` + skin ring state | A | Creates the second 16 MB skin ring; lock routing/offset/discard for skinned VBs. | yes | yes — A2. |
| .cpp lock :293-376 ring routing (`isSkin`, ringUsed/ringSize/ringBuffer, `m_ringBuffer`) | A | Selects main vs skin ring per lock; binds slice from skin ring. | yes | yes — A2. |
| .cpp lock :395 sentinel `memset(sliceStart,0x7F,length)` (isSkin) | **A** | **Pre-fills skin slices with 0x7F before engine write.** | **YES** | **YES — see A5.** |
| .cpp lock :112 file-static `s_lastLockIsSkin/Discard/Offset/RingUsed` | B | Capture lock context for the unlock probe. | yes | no rendering effect (probe state only). |
| .cpp unlock :461-520 "CAPE-COLLAPSE D3D11-SIDE PROBE" | B | Reads just-written slice (BEFORE Unmap, CPU ptr — no GPU stall), detects identical-position run, writes `cape-d3d11-probe.txt`. | yes | no — CPU-side readback of already-mapped memory; logging only. |
| .cpp unlock :521-600 "CONSULT-31 DYNAMIC-VB OUTLIER SCAN" (logging part) | B | Centroid/outlier/sentinel scan of the slice; writes `cape-dynvb-outlier.txt`. | yes | no — logging (the mutation half is A1). |
| .cpp :225 `getDiscardsEver()` + .h:54 decl | B | Exposes lifetime discard count for the draw probe. | yes | no — counter accessor. |
| .cpp :199-207 `beginFrameSkin()` + .h:44-52 | A | Skin ring per-frame reset (part of A2). | yes | yes — A2. |
| .h:131-156 `m_passVSSlot`/`m_passPSSlot` — N/A (this is StaticShaderData) | — | (see StaticShaderData) | — | — |
| .h:113 `m_ringBuffer` member + :131 `getRingBuffer()` inline | A | Per-instance ring pointer for bind-from-correct-ring (part of A2). | yes | yes — A2. |

### Direct3d11_StateCache.cpp

| file:line | bucket | what it does | active? | affects spike? |
|-----------|--------|--------------|---------|----------------|
| StateCache.cpp:277-281 `s_owtSeq`, `s_owtLastTx/Ty/Tz` | B | Tracks W-setter call sequence + last translation for the draw probe. | yes | no — probe state. |
| StateCache.cpp:1958-1961 `++s_owtSeq; s_owtLast* = ...` in setObjectToWorldTransformAndScale | B | Updates the above counters. | yes | no — no render effect. |
| StateCache.cpp:2277 `getSharedRingBuffer()` → `data->getRingBuffer()` (setVertexBuffer) | A | Bind from per-instance ring (part of A2). | yes | yes — A2 bind site. |
| StateCache.cpp:3233-3290 drawPartialIndexedTriangleList signature un-anonymized + "CAPE-COLLAPSE DRAW-OFFSET PROBE v2" | B | Logs bound offset/stride/readPast for stride-48 garment draws → `cape-draw-probe.txt`. No GPU readback. | yes | no — logging only (reads cached bind state). |
| StateCache.cpp:~3300 "CAPE-COLLAPSE STAGING READ-BACK" (CopySubresourceRegion + Map READ) | **B-SYNC** | **Blocking staging readback before DrawIndexed (tri-list).** Also writes `cape-stage-probe.txt`, `cape-wvp-matrix.txt`. | **YES** | **YES — see S1 (GPU drain masks live-only spike).** |
| StateCache.cpp:~3490 drawPartialIndexedTriangleStrip + "STRIP READ-PAST PROBE" (CopySubresourceRegion + Map READ) | **B-SYNC** | **Blocking staging readback before DrawIndexed (strip).** Writes `cape-strip-probe.txt`. | **YES** | **YES — see S2.** |
| StateCache.cpp:1177 `P19_SKIP_AUDIT 1`, :1512 `P19_MAXLOD0 0` | C/context | Pre-existing (committed prior-phase) toggles; NOT in this working diff. Listed for completeness. | SKIP_AUDIT=1 active, MAXLOD0=0 | not part of the spike-diagnostic batch. |

### Direct3d11_StaticShaderData.cpp / .h

| file:line | bucket | what it does | active? | affects spike? |
|-----------|--------|--------------|---------|----------------|
| StaticShaderData.cpp:335-471 + .h:131-156 — `m_passVSSlot`/`m_passPSSlot` live-deref UAF fix; apply() :769-1585 uses `liveVS`/`livePS` | A (but legitimate fix, NOT a probe) | Re-derives per-pass VS/PS graphics-data pointers live in apply() to fix the release boot-crash UAF (commit-quality, ROOT-CAUSED 2026-06-02). | yes | **Not a diagnostic remnant** — this is the resolved boot-crash fix. Behavior-changing by design and correct. Keep. Flagged only so it isn't mistaken for a probe. |
| StaticShaderData.cpp:48 `P19_LIGHTDUMP 0`, :254 `P19_MAXLOD0 0` | C/context | Pre-existing committed toggles, both 0. | no | no. |

### Direct3d11_VertexBufferVectorData.cpp

| file:line | bucket | what it does | active? | affects spike? |
|-----------|--------|--------------|---------|----------------|
| VertexBufferVectorData.cpp:143 `getSharedRingBuffer()` → `data->getRingBuffer()` | A | Bind from per-instance ring (part of A2). | yes | yes — A2 bind site. |

### SoftwareBlendSkeletalShaderPrimitive.cpp

| file:line | bucket | what it does | active? | affects spike? |
|-----------|--------|--------------|---------|----------------|
| :565-636 "CAPE-COLLAPSE PROBE v2" (branch-independent skin replay) | B | Replays position blend from source vectors, detects collapse run, writes `cape-collapse-probe.txt`. | yes | no — read-only replay + logging. |
| :668-686 "CONSULT-32 PATH LOG" big-mesh path | B | Logs render-path flags → `cape-trace-path.txt`. | yes | no. |
| :691-757 "CAPE-COLLAPSE PROBE" (multi-stream branch; ms_useMultiStreamVertexBuffers=0 so dead at runtime) | B (C-at-runtime) | systemStream collapse scan → `cape-collapse-probe.txt`. | code present, branch not taken at runtime | no. |
| :792-833 "CONSULT-32 WHOLE-PATH PROBE A" (systemStream post-skin) | B | Scans skinned systemStream for farthest vert → `cape-trace-systemstream.txt`. | yes | no — read-only + logging. |
| :842-868 "CONSULT-32 PROBE D" (lock vs copy count) | B | Logs lockN vs copyN mismatch → `cape-trace-copycount.txt`. | yes | no — logging. |
| :876 `if (false && ShadowManager::getEnabled() ...)` | **A** | **Force-disables skeletal shadow volumes.** | **YES** | **YES — see A3.** |
| :921 stray blank line in draw() | C | Cosmetic whitespace. | n/a | no. |
| :1679-1726 "CAPE-INDEX-PROBE" buildIndexBufferAndRenderCommands | B | Scans index buffer for read-past (maxIdx >= vtxCount) → `cape-index-probe.txt`. | yes | no — logging. |
| :2000-2010 + :2017-2030 "CONSULT-30 PROBE+GUARD" clamp replacing VALIDATE_RANGE | **A** | **Silent clamp of OOB bone index instead of assert.** | **YES** | **YES — see A4.** |
| :2035-2070 "CONSULT-31 SPIKE CAPTURE" (>100u from vert0) | B | Logs far-from-bulk skinned verts → `cape-skin-spike.txt`. | yes | no — logging. |

### FullGeometrySkeletalAppearanceBatchRenderer.cpp

| file:line | bucket | what it does | active? | affects spike? |
|-----------|--------|--------------|---------|----------------|
| :184-185, :240-244 `capeMaxRebasedIndex`/`capeCopiedIndexCount` tracking in index copy loop | B | Tracks max rebased index during batch index copy. | yes | no — measurement vars; the copy itself is unchanged. |
| :254-281 "CAPE-BATCH-PROBE" mismatch log | B | Logs lockedVtx/writtenVtx/maxRebasedIdx mismatches → `cape-batch-probe.txt`. | yes | no — logging. |

---

## OUT-OF-SCOPE (committed prior-phase toggles, NOT in this working diff — context only)

These `P19_`/diagnostic toggles live in files that are **already committed** (not in `git diff HEAD`). They are not part of the cape-spike uncommitted batch, but are relevant to "what diagnostic state is the build in":

- Direct3d11_ConstantBuffer.cpp:38 `P19_CBUF_ZERO_TAIL 1` — **ACTIVE behavior-changer, committed** (the rainbow-color fix, per project memory — a real fix, keep).
- Direct3d11_StateCache.cpp:1177 `P19_SKIP_AUDIT 1`, :1512 `P19_MAXLOD0 0`.
- Direct3d11_StaticShaderData.cpp:48 `P19_LIGHTDUMP 0`, :254 `P19_MAXLOD0 0`.
- Direct3d11_TextureData.cpp:496 `P19_RETAIN_TEXTURES 0`.
- Direct3d11_LightManager.cpp:38 `P19_LIGHTDUMP 0`.
- Direct3d11_DynamicIndexBufferData.cpp:28-29 `P19_THREAD_AUDIT 0`, `P19_DISCARD_ONLY 0`.

---

## COUNTS

- **[A] BEHAVIOR-CHANGING, ACTIVE:** 5 distinct remnants (A1 terrain clamp, A2 soft-skin ring [multi-site], A3 shadow disable, A4 bone clamp, A5 sentinel memset).
  - Plus the StaticShaderData UAF fix — behavior-changing but a *legitimate resolved fix*, not a remnant.
- **[B-SYNC] GPU-draining readbacks, ACTIVE:** 2 (S1 tri-list staging Map(READ), S2 strip staging Map(READ)).
- **[B] LOGGING-ONLY, ACTIVE:** ~13 probes across the files (cape-d3d11-probe, cape-dynvb-outlier[log half], cape-draw-probe, cape-collapse-probe ×2, cape-trace-path, cape-trace-systemstream, cape-trace-copycount, cape-index-probe, cape-skin-spike, cape-batch-probe, + owt/lock counters, + getDiscardsEver).
- **[C] DEAD/REVERTED:** 5 in-scope toggles (`P19_THREAD_AUDIT 0`, `P19_DISCARD_ONLY 0`, `P19_SKIN_RING_DISABLED 0`, `P19_FORCE_SKIN_DISCARD 0`, Device.cpp `#if 0` race test) + the runtime-dead multi-stream probe branch.

## CLEAN-BASELINE RECOMMENDATION (priority order)

1. **Set `P19_TERRAIN_SPIKE_CLAMP 0`** (A1) — stop mutating geometry.
2. **Remove S1 + S2** staging `Map(READ)` readbacks in StateCache draw paths — stop draining the GPU.
3. **Revert `false &&`** on shadow volumes (A3) and the **bone-index clamp → VALIDATE_RANGE** (A4) in SoftwareBlend.
4. **Remove the 0x7F sentinel memset** (A5).
5. **Decide on the soft-skin ring (A2)** as a deliberate baseline choice — it is the largest structural change and must not be an accidental leftover. Either keep-as-fix-with-justification or revert to the shared ring; do not leave it ambiguous.
6. Strip the [B] logging probes once the above are settled (they don't change rendering but they clutter and a couple write large files).
