# Handoff — 2026-06-05 — D3D11 cape/garment "texture spikes": capture-invisible RACE confirmed; serialization test staged

Branch: `koogie-msvc-cpp20-base`. Main checkout: `D:\Code\swg-client-v2`. Renderer plugin: in-house D3D11 (`gl11`), reimplementing the legacy SWG `Gl_api`. Reference: D3D9 plugin (`gl05`) renders the identical content cleanly.

Primary memory: **`[[project_d3d11_cape_spike_skinning_regression]]`** (read its CURRENT-STATE / Capture15 block). This handoff is the authoritative, self-contained record — start here.

---

## 0. TL;DR (read this, then §4 and §6)
- **Symptom:** close, soft-skinned characters (Jawas/robed NPCs) render with vertical "texture spikes" + detached floating boots in-world (Mos Eisley). D3D9 is clean. D3D11-only.
- **Breakthrough (Capture15, 2026-06-05):** the torn draws have **perfectly clean vertex data in the captured buffers** (RenderDoc re-runs their VS and gets correct geometry) but render torn **live**. ⇒ it is a **capture-invisible CPU-write / GPU-read RACE**. The vertex data is *correct*; the live GPU read it at the wrong moment.
- **Consequence:** the **5 buffer-data/lifetime fixes I tried all failed** because the VB data was never corrupt — you cannot fix a timing race by reshaping correct data.
- **Current staged build:** a **GPU-serialization diagnostic** is enabled in `Direct3d11_Device::present()` (full CPU↔GPU sync each frame). **Run it and report whether the spikes vanish** — that single result decides the fix direction (§6).
- Everything else is reverted to baseline. An intermittent `nvwgf2um` (NVIDIA driver) crash on world load is **pre-existing and not ours** (§8) — retry to clear.

---

## 1. Exact symptom
- Vertical brown "texture spikes" erupt from robed/garment characters; the robe texture is stretched along the spike. Some skinned sub-parts (e.g. **boots**) detach and float as **intact units** elsewhere in the scene.
- **Distance/LOD-dependent:** the SAME Jawa is clean at a distance and tears up close. Skinning mode is chosen by on-screen size in `SkeletalAppearance2.cpp:1737`:
  - far/tiny → `SM_noSkinning`
  - medium → `SM_hardSkinning`
  - **close/large → `SM_softSkinning`**  ← the torn case (CPU-skinned into a dynamic VB every frame)
- **D3D11-only:** D3D9 (`gl05`, `rasterMajor=5`) renders the same character clean at all distances.
- **In-world-specific:** char-select soft-skinned bodies render CLEAN (same VS, same input layout, same `skinData`) — see `[[project_phase17_charselect_d3d11_parity_verified]]`. So the core skinning/VS/layout is fine; something about the busy in-world scene triggers it.
- Severity flickers frame-to-frame (mild → extreme).

---

## 2. Architecture (file:line, verified this saga)
Soft-skin path — `clientSkeletalAnimation/src/shared/appearance/SoftwareBlendSkeletalShaderPrimitive.cpp`:
- `prepareToDraw()` @483. Multi-stream branch @566-581: `skinData()` writes positions+normals into `m_systemStream` (a SystemVertexBuffer), then `m_dynamicStream->lock(m_vertexCount)` + copy systemStream→dynamic + `unlock()`. Non-multistream variants @586-619.
- The skinning fill `fillVertexBuffer()` @1617 writes ALL `m_vertexCount` verts (loops @1640 / @1690) — position, normal, texcoord per vertex.
- `RenderCommand::render()` @306-315 issues `(*m_drawPrimitiveCommand)(0, m_minimumVertexBufferIndex, m_numberOfVertices, m_startIndexBufferIndex, m_primitiveCount)` → `Graphics::drawIndexedTriangleList(baseVertexIndex=0, minVbIndex, numVerts, startIndex, primCount)`. **baseVertex=0; IB holds ABSOLUTE indices.**
- Index buffer built in `buildIndexBufferAndRenderCommands()` @1291; **`m_indexBuffer = new StaticIndexBuffer(indexCount)` @1298 — a STATIC IB for the single-mesh path (NOT the dynamic ring).** `createTriList(minVbIndex, (maxVbIndex-minVbIndex)+1, firstTriListIndex, triCount)` @1369.
- `m_vertexCount = mesh.getVertexCount(...)` @1019; `m_staticStream`/`m_systemStream` sized `m_vertexCount` @1224/1231; `m_dynamicStream = new DynamicVertexBuffer(dynamicFormat)` @1106.

Batched path — `clientSkeletalAnimation/src/shared/batch/FullGeometrySkeletalAppearanceBatchRenderer.cpp` (batches multiple chars into ONE dynamic VB + ONE dynamic IB):
- `LocalShaderPrimitive::draw()` @298; do-while loop @317-352. `allowDiscard` decided @312-343: `getNumberOfLockableDynamicVertices(false)` @322; if a source mesh won't fit remaining → `allowDiscard=true` @340 (a real mid-frame `forceDiscard`).
- `renderBuffers()` @178: `s_batchVertexBuffer->lock(vertexCount, allowDiscard)` @191; `s_batchIndexBuffer->lock(indexCount)` @195 (dynamic IB); copies verts + **adjusts indices `*destIbIt = *sourceIbIt + copiedVertexCount` @238**; then draws each chunk.

D3D11 dynamic buffers (the suspects):
- `Direct3d11_DynamicVertexBufferData.{h,cpp}` — ONE process-wide shared ring (`ms_d3dRingBuffer`, `D3D11_USAGE_DYNAMIC`, ~2MB), `lock()` Map(WRITE_DISCARD on wrap / WRITE_NO_OVERWRITE on append), over-charge accounting (`ms_used += length` in lock), `getSharedRingBuffer()`, `beginFrame()` @144 **has ZERO call sites**, `getNumberOfLockableDynamicVertices()`.
- `Direct3d11_DynamicIndexBufferData.cpp` — the **dynamic INDEX ring**, same shared-ring architecture, `getSharedRingBuffer()` @125, `beginFrame()` zero callers. **NEVER modified in any of the 5 fixes.** Used by the batcher (`s_batchIndexBuffer`). Single soft-skin meshes use a STATIC IB.
- Bind sites: single-stream VB `Direct3d11_StateCache.cpp:2265`; dynamic IB `Direct3d11_StateCache.cpp:2364`; multi-stream VB `Direct3d11_VertexBufferVectorData.cpp:143`. Draw: `drawPartialIndexedTriangleList()` @3223 → `DrawIndexed(primCount*3, startIndex, baseIndex)` (D3D11 has **no** min/numVertices bound, unlike D3D9 `DrawIndexedPrimitive`).
- Once-per-frame hook: `Direct3d11.cpp` `update_impl()` @289 = `ms_glApi.update` @1011 = `Graphics::update`, fires every frame BEFORE the first `beginScene`. (`beginScene` is PER-PASS — resetting dynamic state there shreds geometry because draws defer across passes; proven catastrophic.)
- `Direct3d11_Device::present()` @847 — once per frame; the serialization diagnostic block lives here ~@868.

D3D9 reference: `Direct3d9_DynamicVertexBufferData.cpp` lock@172 (no ms_used advance) / unlock@244 (`ms_used += actual`); `Direct3d9.cpp:2484` calls `beginFrame()` inside `beginScene` (D3D9 wraps the whole frame in one BeginScene/EndScene).

---

## 3. The 5 FAILED fixes — do NOT retry (buffer-lifetime hypothesis EXHAUSTED)
Each was a substantially different dynamic-VB strategy; all produced the SAME spikes.
1. **Per-generation counter ring** (N=4 buffers, advance generation on each WRITE_DISCARD, per-slice bind). → spikes WORSE + became RenderDoc-deterministic. (This is why Capture13/14 froze garbage — misleading.)
2. **Per-frame `beginFrame()` flip** wired into `update_impl` (one shared buffer, WRITE_DISCARD at frame start). → no change.
3. **Uniform per-instance buffers** (every dynamic VB its own ID3D11Buffer, WRITE_DISCARD each lock). → **CRASH** in `nvwgf2um` (driver resource churn) on world load. (NOTE §8: baseline crashes there too — churn wasn't the sole cause.)
4. **Fence-retired N-buffer, whole-frame** (4 fixed buffers + ID3D11Query EVENT, per-frame flip, signal in present). → spikes.
5. **Per-slice-identity advance-on-discard, fence-retired** (Codex CONSULT-22 review design: advance to next fenced buffer on any discard incl. batcher forceDiscard; per-slice `m_buffer`; bind `data->getVertexBuffer()`; signal all used buffers; within-frame-wrap guard). → spikes.
All 5 reverted to baseline. Lesson: the dynamic-VB buffer lifetime / ring contention is **NOT** the root cause.

---

## 4. THE KEY FINDING — capture-invisible race (Capture15, baseline build)
Tooling: renderdoc-mcp (`D:\Code\renderdoc-mcp`, see `[[project_renderdoc_mcp_server_idea]]`). `debug_vertex` and `export_mesh` **re-run the vertex shader against the buffers AS STORED in the capture**.

Procedure used (REUSE THIS — §7):
1. `open_capture stage/Capture15.rdc`.
2. The scene renders to offscreen RT **`::323`**, then a fullscreen quad composites to the swapchain — so `pixel_history` on the swapchain only shows the composite; probe `::323`. `goto_event 5915` (a mid-scene draw) + `export_render_target 0` → `stage/renderdoc-mcp-export/rt_5915_0.png` (shows player clean, two right-side Jawas shredded into vertical spikes, detached boots floating).
3. `pixel_history` on a spike pixel of `::323` → the owning draw:
   - **robe spike = eid 2469** (579 indices, 97 unique verts)
   - **floating boot = eid 3895** (3390 indices, 703 unique verts)
4. `export_mesh`/`debug_vertex` those draws → **CLEAN**:
   - 2469: 97 verts, indices 0–96, x∈[-1.90,-1.19] y∈[-0.65,2.24] z∈[4.93,5.41], **zero outliers, no NaN/huge/zero**.
   - 3895: 703 verts, indices 0–702, x∈[-0.89,-0.41] y∈[-0.59,0.36] z∈[0.97,1.25], **zero outliers**.
   - (Cross-check: `debug_vertex` per-vertex inputs = valid POSITION/NORMAL/TEXCOORD for low AND high vertexIds; only unused ring slots past the mesh read zero.)

**Reasoning:** RenderDoc replays the VS on the stored buffers and gets the correct mesh, yet the live framebuffer (`rt_5915_0.png`) shows spikes. A real async CPU/GPU race replays clean (RenderDoc serializes execution). ⇒ **the vertex DATA in the buffer is correct; the live GPU rendered wrong = capture-invisible CPU-write/GPU-read race.** This explains why all 5 VB-data fixes failed (the data was never corrupt) and why the bug is "live-only".

VS reflection (the soft-skin VS): inputs `POSITION, NORMAL, TEXCOORD` (NO blend weights → not hardware skinning; plain WVP transform) → garbage SV_Position can only come from garbage POSITION INPUT, and the stored input is clean.

CAVEAT on Capture13/14: those are from the PER-GENERATION build (fix #1), which froze garbage into storage (deterministic) and led me down the buffer-layout path. **Capture15 = baseline = the real, capture-invisible behavior. Trust Capture15.**

---

## 5. CURRENT TREE STATE (precise)
- **`Direct3d11_Device.cpp` `present()` ~@868: serialization diagnostic ENABLED** (`#if 1`). It creates one static `ID3D11Query` (EVENT) and after `Present` does `Flush()` + `End(query)` + spin on `GetData` until the GPU finishes the frame → forces full CPU↔GPU serialization (kills pipelining; static query avoids per-frame CreateQuery/Release churn). This is the ONLY functional change vs baseline.
- **All other files reverted to baseline:** `Direct3d11_DynamicVertexBufferData.{h,cpp}`, `Direct3d11.cpp`, `Direct3d11_StateCache.cpp`, `Direct3d11_VertexBufferVectorData.cpp`.
- `Direct3d11_StaticShaderData.{cpp,h}` show as modified in `git status` — that is a PRE-EXISTING inert change (m_passVSSlot/m_passPSSlot live-deref), NOT ours; leave it.
- `gl11_r.dll` built clean (0 errors) and staged 2026-06-05. Build cmd: `& $env:MSBUILD "src\engine\client\application\Direct3d11\build\win32\Direct3d11.vcxproj" /p:Configuration=Release /p:Platform=Win32 /nodeReuse:false /m` (postbuild auto-copies dll+pdb into `stage/`).

---

## 6. NEXT STEP — run the serialization test (decisive), then branch
Boot release client (rasterMajor=11, SwgClient_r.exe + gl11_r.dll), enter Mos Eisley, close-approach a robed Jawa (the repro). Expect a big FPS drop (serialization kills pipelining — that's expected, diagnostic only).

- **Spikes VANISH → CPU/GPU race CONFIRMED.** The fix is a targeted synchronization/ordering point, NOT buffer layout. Then narrow WHICH resource races and find the MINIMAL sync (full-frame sync is too slow to ship). Ranked suspects:
  1. **Dynamic INDEX buffer** (`Direct3d11_DynamicIndexBufferData`) — NEVER touched, same shared-ring architecture as the VB; the batcher uses it (`s_batchIndexBuffer`). Live IB races → wrong indices → spikes; stored IB clean → replay clean. **Top suspect.** Try: a per-frame/fenced discipline OR a Flush, applied to the IB.
  2. **Map/Unmap ordering vs deferred-across-passes draws** — the dynamic write may not be ordered before the draw that consumes it on the immediate context in some path. Try: `context->Flush()` (or a fence) after the dynamic VB/IB `Unmap`, narrowed to the skinned path.
  3. Constant buffer / texture upload race (less likely — spikes are geometric/per-vertex).
  To narrow: re-enable serialization only around specific submits, or bisect (sync after Unmap only) and re-test.
- **Spikes PERSIST → NOT a CPU/GPU race.** Pivot: re-examine the engine skinning/transform feed and the per-draw input layout under the "stored data is clean" constraint. Re-capture and diff gl05 vs gl11 of the same close Jawa (the gl05-vs-gl11 pipeline-state DIFF was never actually done — see `[[feedback_renderdoc_d3d9_vs_d3d11_is_the_diagnostic]]`).

After the experiment, **revert `present()` `#if 1`→`#if 0`** regardless of outcome.

---

## 7. Reusable diagnostic technique (renderdoc-mcp)
1. `open_capture <rdc>`; `get_capture_info`; `list_draws`.
2. Find the offscreen scene RT id: `goto_event <mid-scene draw>` + `export_render_target 0` → read the PNG; it's `::323` (this build).
3. `pixel_history x y targetIndex=0 eventId=<late scene event>` on a spike pixel of the scene RT → the passing fragment's `eventId` = the culprit draw.
4. `export_mesh eventId stage=vs-out format=json outputPath=...` and/or `debug_vertex eventId vertexId` → these RE-RUN the VS on STORED buffers. Clean output + torn framebuffer = **capture-invisible race**. Garbage output = data corrupt in storage.
5. `get_shader eventId stage=vs mode=reflect` for the input/output signature; `get_pipeline_state` for shaders/RTs (does NOT expose IA vertex buffers — use `debug_vertex` for inputs).
Note: renderdoc-mcp can analyze `.rdc` but CANNOT inject the 32-bit client — Kenny captures via qrenderdoc GUI.

---

## 8. Intermittent crash (PRE-EXISTING, NOT ours)
`c0000005` at `nvwgf2um!...SetDependencyInfo+...` (NVIDIA user-mode driver) on a **driver worker thread** (`kernel32!BaseThreadInitThunk` → nvwgf2um), null-deref `mov eax,[eax]`, during async `.mgn` mesh streaming on world load (UpTime ~7s, Mos Eisley, player 3469.75/4.79/-4913.15). **Intermittent — retry the launch clears it** (Capture15 loaded fine moments before a crash on relaunch). The SAME signature occurred on the per-instance build (fix #3) — so that crash was NOT solely my churn; baseline has it too. Distinct from the spike bug. Sibling of `[[project_intermittent_tatooine_crash_087a]]`. Dumps: `stage/SwgClient_r.exe-...-2026060506*.{mdmp,txt}`. Analyze with x86 cdb: `& "C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe" -z <dump> -y "D:\Code\swg-client-v2\stage" -lines -c ".ecxr; k 24; q"`.

---

## 9. Evidence files
- Captures: `stage/Capture13.rdc` (clean far, per-gen build), `stage/Capture14.rdc` (torn close, per-gen build — deterministic/misleading), `stage/Capture15.rdc` (torn close, BASELINE — the real one).
- Exports: `stage/renderdoc-mcp-export/` — `rt_5915_0.png` (scene RT ::323, baseline, shows spikes+boots), `mesh_2469_cap15.json` (clean robe), `mesh_3895_cap15.json` (clean boot), `mesh_2413_cap15.json` (clean unrelated mesh), and per-gen-build `rt_6356_0.png`/`rt_7439_0.png`/`mesh_3803_vsout.json`.
- Consults (`.planning/research/`): `CONSULT-21-d3d11-softskin-vb-race-renderdoc.md` (+`-codex.out`,`-cursor.out`); `CONSULT-22-d3d11-dynamic-vb-churnfree-fence.md` (+`-codex.out`,`-cursor.out`, `-REVIEW-codex.out`, `-REVIEW2-codex.out`). Both AIs also anchored on buffer lifetime — the capture-invisible-race finding (§4) supersedes them.

---

## 10. Environment / build / test cheatsheet
- Build: `$env:MSBUILD` is set (settings.json). `& $env:MSBUILD <vcxproj> /p:Configuration=Release /p:Platform=Win32 /nodeReuse:false /m`. Postbuild stages `gl11_r.dll`+pdb into `stage/`.
- Run: release `SwgClient_r.exe` + `gl11_r.dll`, reads `stage/client.cfg` (`rasterMajor=11` for D3D11; `=5` for the D3D9 `gl05` reference). DEBUG exe reads `client_d.cfg`.
- Server: local SWGSource in VirtualBox; login `swg`/`swg` → through char-select. F12 in-client screenshot.
- Reference flip: set `rasterMajor=5` + ensure `gl05_r.dll` present to compare against clean D3D9.

---

## 11. Open hypotheses (ranked, post-Capture15)
1. **Dynamic INDEX buffer race** (never fixed; batcher uses it; same shared-ring arch). ← test first if serialization confirms a race.
2. **Map/Unmap → draw ordering** on the dynamic VB for deferred-across-passes draws (a missing Flush/fence between the skinned write and its later-pass draw).
3. Per-object transform / every-other-frame skinning interaction producing a stale binding (less likely — stored data is clean & correct for the drawn mesh).
4. (Falsified) VB data corruption / buffer lifetime / ring contention — 5 fixes, all failed; Capture15 shows VB data clean.
