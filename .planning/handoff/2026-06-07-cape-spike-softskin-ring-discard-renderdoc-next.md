# Handoff — 2026-06-07 — D3D11 soft-skin "texture spikes": full investigation state; RenderDoc capture is the next decisive step

Branch `koogie-msvc-cpp20-base`. Main checkout `D:\Code\swg-client-v2`. Renderer plugin: in-house D3D11 (`gl11`) reimplementing legacy SWG `Gl_api`. Reference: D3D9 (`gl05`) renders the same content cleanly.

Primary memory: **`[[project_d3d11_cape_spike_skinning_regression]]`** — this handoff supersedes the running notes there; start here.

---

## 0. RESUME HERE (the one next action)
Kenny is capturing a **RenderDoc frame with BOTH a dewback AND a Jawa visibly spiking** → saving as **`stage/Capture-spike.rdc`** (capture AFTER ~30s of play so the staging probe's GPU-flush has gone dormant — see §6). When it exists, do the **raw-bytes analysis in §7** (NOT RenderDoc's clean re-simulation). That single test settles the leading hypothesis (ring-discard invalidation).

renderdoc-mcp is INSTALLED and shell/MCP-drivable (see `[[project_renderdoc_mcp_server_idea]]`). Tools are deferred — load via `ToolSearch "select:mcp__renderdoc-mcp__open_capture,..."`.

---

## 1. THE SYMPTOM (reframed 2026-06-07 — this is the key reframe)
Close, **soft-skinned** characters render with vertical "texture spikes": most of a draw's vertices collapse / fan out into thin shards. **It is NOT garment-specific** — Kenny confirmed it hits **Jawas (robes), dewbacks, and other creatures**. It is **DISTANCE-GATED**: far = clean, close = spikes. D3D9 (`gl05`) renders all of it clean. Flickers mild↔extreme frame to frame.

Distance gating == the LOD→skinning-mode switch (`SkeletalAppearance2.cpp:1737`, chosen by on-screen size):
- far/tiny → `SM_noSkinning`
- medium → `SM_hardSkinning` (GPU skin, static-ish VB) → **CLEAN**
- **close/large → `SM_softSkinning`** → CPU-skin into the shared dynamic VB ring every frame → **BROKEN**

⇒ The common factor is **the dynamic-ring soft-skin path itself**, not any mesh. THE WHOLE INVESTIGATION SHOULD BE FRAMED AROUND THE SOFT-SKIN DYNAMIC-RING PATH.

---

## 2. PROVEN GROUND TRUTH (trust these; do not re-litigate)
All via custom in-engine/in-plugin diagnostic probes (still in the tree — §5). Spikes were ON SCREEN during each.
1. **CPU skin is CLEAN.** Engine probe replicates `fillVertexBuffer`'s position blend straight from `m_sourceVectors`+bone transforms. 600 sampled skinned draws: ZERO collapse, max identical-run=3, ZERO out-of-range bone indices. Garment example vc=110, primary bone `rotate(unitX).mag = 0.9412` (healthy). → skin math is correct.
2. **The D3D11 dynamic-VB WRITE is CLEAN.** Probe in `Direct3d11_DynamicVertexBufferData::unlock()` reads the just-written slice (CPU mapped ptr, before `Unmap`). Garment writes numVerts DISTINCT verts (e.g. vc=110, pos `[0.22,0.92,0.026]`, m_offset=2670). 500 samples, ZERO collapse. → the bytes the engine writes are correct.
3. **The bound offset at the draw is CORRECT.** SBSSP binds via the VECTOR path (`setVertexBuffer(*m_vertexBufferVector)` → `ms_currentVBVectorOffsets[0]`, NOT single-stream). After a rebind-at-draw fix, the draw's bound offset matches the write offset exactly. → not a stale/clobbered bind. (The rebind fix DID NOT stop the spike → bind was never the cause.)
4. **Read-past is FALSE.** Staging probe: draw's index range `[0,numVerts)`, `maxIndex == numVerts-1` always, `readPast=0` ×31. Confirmed in code: tri-list indices are compacted to `[0,m_vertexCount)` (Cursor/Codex/Opus + `MeshConstructionHelper` `m_shaderVertexIndex` sequential). Tri-STRIP path could exceed (raw uncompacted indices) BUT **the robe has NO strip draws** (instrumented `drawPartialIndexedTriangleStrip` → no output; `addTriStripVertex` has no callers in `src/`).
5. **GOLD-STANDARD staging read-back: the VB the draw reads is CORRECT at draw time.** Probe in `drawPartialIndexedTriangleList`: `CopySubresourceRegion` the bound ring region → STAGING → `Map(READ)` → scan, right before `DrawIndexed`. Garment verts `[0,numVerts)` are CORRECT and ANIMATING (firstPos matches the write and drifts frame-to-frame); beyond numVerts the copy reads pure ZERO `[0,0,0]` (that's my fixed 256-vert window OVER-READING past the slice — an artifact, not the draw).
6. **The per-object TRANSFORM (WVP) is FINE in steady state.** Computed `clip = WVP * firstPos` (WVP = `s_slot0Shadow.objectWorldCameraProjectionMatrix`, StateCache.cpp:354) AND `MINclipW` over ALL drawn verts. Steady-state (call 2300..3750): `MINclipW > 0.9`, `nearZero=0` → all verts clip fine. Bad `clipW`/near-plane verts ONLY on `call=1..8` = the FIRST sampled frame = STARTUP TRANSIENT (view/proj not yet initialized). The "transform collapse" that looked promising was a startup artifact that fooled us twice. `worldPos=[0,0,0]` for garments is EXPECTED (SWG renders skeletal meshes OBJECT-LOCAL — camera transformed into object space, W=identity, to dodge float precision at world ~3469/-4913; a few draws show world-space `[3469.8,4.8,-4913.1]`).

**NET:** skin clean → write clean → bind correct → (for the SAMPLED draws) VB correct at draw + transform correct. Yet it spikes. ⇒ **The spike is on draws my probe does NOT sample, OR my probe's flush hides it (§3).**

---

## 3. THE LEADING HYPOTHESIS (Sonnet's, now best-fit after the reframe)
**Ring-discard invalidation between a soft-skin primitive's PREPARE (pass N) and its DEFERRED DRAW (pass M).**
- SBSSP `prepareToDraw()` locks the shared 2MB dynamic ring (`ms_d3dRingBuffer`), writes its slice at `m_offset`, unlocks, binds. The actual `DrawIndexed` is DEFERRED to a later pass (PROVEN earlier: adding a ring reset to per-pass `beginScene` shredded geometry → draws reference data written in earlier passes).
- Between N and M, `CuiLayerRenderer` locks `getNumberOfLockableDynamicVertices()` (the WHOLE remaining ring) → next lock hits `ms_used+length > ms_size` → **`WRITE_DISCARD`** → the ring is RENAMED (new physical backing), `ms_used=0`. Baseline does ~370 discards/frame from this.
- The deferred draw binds `ms_d3dRingBuffer` at byte `m_offset*stride`, but that COM buffer now points to a LATER generation → reads NOT the garment's data but whatever post-discard data is there (another skeletal mesh's verts + zeros) → collapse → spike.

**Why this fits EVERYTHING:**
- All soft-skin meshes (anything using the ring), distance-gated (close=soft-skin=ring), flickering (depends on whether a discard lands in the prepare→draw window). ✓
- VB write correct (the data WAS written correctly, then renamed away). ✓
- Bind offset correct (offset is right; the renamed buffer has wrong data there). ✓
- Frame-level `PERSIST` serialization (after Present) DIDN'T fix it — it's INTRA-frame, not a post-Present race. ✓
- **CRITICAL — why my staging probe can't see it:** the staging `Map(READ)` FLUSHES the GPU on every sampled draw → serializes the prepare→draw window for THAT draw → the sampled draw reads correct data → looks fine. Unsampled draws (no flush) still spike. The transform `clipW` is computed from `WVP*position` and is immune to the VB flush, so it (correctly) shows the transform is fine. **My probe is structurally blind to a VB-ring-discard spike.** ✗ can't test with the current probe.
- The "capture-invisible" history (early sessions: RenderDoc re-sim clean but stored RT spiked) reconciles: RenderDoc's `debug_vertex`/`export_mesh` RE-SIMULATE the VS on the generation RenderDoc attributes to the draw (clean), but the LIVE draw read the post-discard generation. The STORED RT shows the real spike.

**The ONE caveat:** the staging firstPos MATCHED the garment's expected position and animated consistently. Sonnet's counter: the post-discard data is ANOTHER nearby skeletal mesh (also animating, similar positions). Weak but not impossible. The RenderDoc raw-bytes test (§7) resolves it.

---

## 4. FALSIFIED — DO NOT RE-PROPOSE OR RE-TEST
- CPU skin wrong — proven clean (probe).
- D3D11 VB write wrong — proven clean (probe).
- Stale/clobbered vector BIND OFFSET (`ms_currentVBVectorOffsets[0]`) — rebind-at-draw made it correct, STILL SPIKED. (CONSULT-23 3-way consensus Codex+Cursor+Sonnet → WRONG.)
- READ-PAST (index ≥ m_vertexCount) — `readPast=0`; indices compacted; no strip draws. (CONSULT-24 Cursor+Codex → WRONG.)
- Per-object WVP / transform collapse — fine in steady state; bad clipW was a startup transient only. (CONSULT-24 Opus → right about startup, wrong about persistent.)
- CPU/GPU timing RACE — frame-level AND per-draw serialization both failed to fix; deterministic in stored RT.
- 5 dynamic-VB BUFFER-LIFETIME redesigns: per-generation ring, per-frame `beginFrame()` flip, uniform per-instance buffers (driver crash), fence N-buffer whole-frame, per-slice fenced advance-on-discard — ALL produced the same spikes. (Sonnet argues #5 re-bound the new offset WITHOUT re-writing the data, so it didn't actually fix the invalidation — see §8.)
- `ms_used` accounting fix (charge actual at unlock like D3D9) — made it visibly WORSE (full-screen flicker). The "fewer discards = worse" inversion: more discards → shorter ring epochs → prepare+draw more likely in the SAME epoch → fewer invalidations → fewer spikes. Reducing discards lengthens epochs → MORE invalidation. (Consistent with §3.)

---

## 5. DIAGNOSTIC PROBES CURRENTLY IN THE TREE (all temporary; remove on fix)
All are `#if`-free inline diagnostics gated by sample counters; harmless but must be removed when done. They write `.txt` files into `stage/` (cwd at runtime).
1. **`src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SoftwareBlendSkeletalShaderPrimitive.cpp`**
   - `prepareToDraw()` after `transformArray` (~@565, BEFORE the buffer branch): **CAPE-COLLAPSE PROBE v2** — replicates the skin blend, detects collapse + degenerate bones → `cape-collapse-probe.txt`. (Also a dead v1 inside the multistream branch — multistream is OFF at runtime.)
   - `draw()` (~@824): **CONSULT-23 rebind fix** — added `Graphics::setVertexBuffer(*m_vertexBufferVector); setIndexBuffer(*m_indexBuffer);` at top of draw(). **THIS FIX FAILED — revert candidate (dead weight).**
2. **`src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.{h,cpp}`**
   - `unlock()`: **CAPE-COLLAPSE D3D11-SIDE PROBE** — reads back the just-written slice → `cape-d3d11-probe.txt` (logs numVerts, offsetVerts, longestRun, discards).
   - `.h` + `.cpp`: added `static int getDiscardsEver()` accessor (returns `ms_discardsEver`).
3. **`src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp`**
   - `drawPartialIndexedTriangleList(...)`: params `minimumVertexIndex/numberOfVertices` UNCOMMENTED (were `/*...*/`). **CAPE-COLLAPSE DRAW-OFFSET PROBE** (cape-draw-probe.txt: vec/ring/boundVertOffset/discards) + **STAGING READ-BACK** (cape-stage-probe.txt: CopySubresourceRegion→staging→Map→scan; logs MINclipW over drawn range, nearZero, readPast, worldPos, firstPos, v0clipW). The staging block is the big one.
   - `drawPartialIndexedTriangleStrip(...)`: params uncommented + **STRIP READ-PAST PROBE** → `cape-strip-probe.txt` (produced NO output → no strip draws).
   - Note: `s_slot0Shadow`, `s_cachedWorld`, `ms_currentVBVector*` are in the file's anonymous namespace — accessible from these member functions.
4. Pre-existing inert: `Direct3d11_StaticShaderData.{cpp,h}` show modified in `git status` — NOT ours, leave it.

**`AGENTS.md` is also modified (pre-existing, not ours).** Many `.planning/` and `binary-backups/` are untracked.

---

## 6. BUILD / RUN CHEATSHEET
- `$env:MSBUILD` is set (settings.json). Run from PowerShell (not Git Bash). `/nodeReuse:false`.
- **Plugin only (gl11)** — fast, for StateCache/DynamicVertexBufferData probe changes:
  `& $env:MSBUILD "src\engine\client\application\Direct3d11\build\win32\Direct3d11.vcxproj" /p:Configuration=Release /p:Platform=Win32 /nodeReuse:false /m`
  Postbuild auto-copies `gl11_r.dll`+pdb into `stage/`.
- **EXE (clientSkeletalAnimation changes)** — heavier, two steps:
  `& $env:MSBUILD "src\engine\client\library\clientSkeletalAnimation\build\win32\clientSkeletalAnimation.vcxproj" ... ; if ($?) { & $env:MSBUILD "src\game\client\application\SwgClient\build\win32\SwgClient.vcxproj" ... }`
  Postbuild stages `SwgClient_r.exe`+pdb. (MSB8012 TargetName + LNK4217 `_xmlFree` warnings are PRE-EXISTING/benign; `/W4 /WX-` so unused-var warnings don't fail the build.)
- **Run:** release `SwgClient_r.exe` + `gl11_r.dll`, reads `stage/client.cfg` (`rasterMajor=11`; `=5` for the gl05 D3D9 reference). Login swg/swg → char-select → Mos Eisley → close-approach a robed Jawa / dewback. F12 in-client screenshot.
- **Probe dormancy for a clean RenderDoc capture:** the staging probe caps at `s_csLogged < 120` (first 40 calls + every 50th) and then STOPS flushing. After ~15-30s of play it's dormant → the captured frame has no flush interference. (Or rebuild a clean gl11 with the probes removed if you want zero risk.)
- Edit-matching gotcha: the StateCache probe block has DEEP nesting; multi-line Edits fail on tab counts. **Use content-only single-line anchors** (no leading whitespace) — they matched reliably.

Latest staged: `gl11_r.dll` 07:46, `SwgClient_r.exe` 07:33 (has the failed rebind fix). Current `stage/cape-stage-probe.txt` is from the per-vertex MINclipW run.

---

## 7. THE NEXT DIAGNOSTIC — analyze `stage/Capture-spike.rdc` (raw bytes, NOT re-sim)
The capture has BOTH a dewback and a Jawa spiking → two independent draws to cross-check. Procedure (renderdoc-mcp; reuses the technique from `[[project_d3d11_cape_spike_skinning_regression]]` §7):
1. `open_capture stage/Capture-spike.rdc`; `get_capture_info`; `list_draws`.
2. The scene renders to an OFFSCREEN RT (was `::323` in Capture15; re-confirm via `goto_event <mid-scene draw>` + `export_render_target 0` → read the PNG to find the spike). `pixel_history x y targetIndex=0 eventId=<late scene event>` on a **spike pixel of each creature** → the owning **draw eid** (do this for BOTH dewback and Jawa).
3. For each spike draw eid: `get_pipeline_state` + `get_bindings` → identify the bound IA vertex buffer (the 2MB ring = `Buffer 49`-class, `byteSize 2097152`) and its byte offset, the VS, and the bound slot-0 constant buffer (`SwgVertexConstants`, the WVP).
4. **THE KEY TEST — read the RAW stored VB bytes, not the re-sim.** `export_buffer` the bound ring buffer at the draw's byte offset for `numVerts*stride` bytes → inspect positions (offset 0, stride from the IA). Cross-check against what the engine wrote (the `cape-d3d11-probe.txt` value, or just: are the verts DISTINCT or collapsed?).
   - **Stored VB bytes COLLAPSED / wrong where the engine wrote correct data → RING-DISCARD INVALIDATION CONFIRMED (Sonnet).** The deferred draw read a renamed generation. → go to §8 fix.
   - **Stored VB bytes CORRECT (distinct) but the rendered RT still spiked → NOT a buffer issue at all** → re-examine: bound CB/WVP for that specific draw (degenerate?), the input LAYOUT / stride mismatch, or a draw-call parameter. (Lower probability given §2/§6 but keep open.)
5. ALSO grab the slot-0 WVP cbuffer values for the spike draw (`export_buffer` the CB resource) and transform a known vertex by it — confirm whether THIS draw's transform is degenerate (vs the sampled steady-state draws which were fine). If the cbuffer is the shared global, it should be fine; if somehow per-draw-wrong here, that reopens the transform angle.
   - IMPORTANT: trust the STORED RT (`export_render_target`, `pixel_history`) as ground truth for WHERE the spike is. Do NOT trust `debug_vertex`/`export_mesh` re-simulation alone (it re-runs the VS and can read a different generation → was "capture-invisible" before). Use `export_buffer` (raw stored bytes) + the stored RT.

---

## 8. IF RING-DISCARD IS CONFIRMED — fix candidates (none cleanly tried yet)
The data must survive from the soft-skin primitive's prepare to its deferred draw, across intra-frame discards. The 5 prior buffer fixes failed because (per Sonnet) they re-bound the new offset but did NOT re-write the data after a discard, OR weren't truly per-slice-surviving.
- **Fix A (Sonnet, preferred — discard-serial stale-detect + re-skin):** store `m_discardSerial = ms_discardsEver` at lock in `Direct3d11_DynamicVertexBufferData`. Add `getDiscardSerial()` + `getCurrentDiscardSerial()`. In `SoftwareBlendSkeletalShaderPrimitive::draw()`, if the slice's serial != current → re-run `prepareToDraw()` (re-skin to a fresh slot) before drawing. (Risk: `prepareToDraw` is const → const_cast or de-const; re-skin cost; verify it actually re-locks fresh.)
- **Fix B (the real root):** STOP the spurious ~370 discards/frame — they come from `CuiLayerRenderer` locking the WHOLE ring (`getNumberOfLockableDynamicVertices`) under the D3D11 lock-time over-charge accounting. A CORRECT low-discard accounting (charge actual at unlock like D3D9) + cap CuiLayer's lock — BUT the naive accounting fix "made it worse" before, likely because it lengthened epochs without protecting deferred slices. Needs care.
- **Fix C (architectural):** give soft-skin primitives a dedicated persistent dynamic buffer created ONCE (bounded count ~dozens, NOT per-frame — avoids the fix#3 mass-creation driver crash) that only discards on its own per-frame re-lock. The discriminator (soft-skin-only) without an ABI-risky shared-header change is the open problem.
- **Validate ANY fix** with: re-capture → stored RT clean + raw VB bytes correct at draw; AND the spike gone in-game across distances/creatures (Jawa + dewback). Watch for the pre-existing intermittent `nvwgf2um` NVIDIA driver crash on world load (retry to clear — NOT ours).

---

## 9. CONSULT TRAIL (for context; don't re-run blindly — consensus has been WRONG twice)
- `.planning/research/CONSULT-23-d3d11-cape-collapse-write-clean-draw-collapsed.md` (+ `-codex.out`, `-cursor.out`): 3-way consensus → rebind-at-draw → FAILED.
- `.planning/research/CONSULT-24-d3d11-cape-vb-correct-at-draw-still-spikes.md` (+ `-codex.out`, `-cursor.out`): Cursor/Codex → read-past (dead); fresh Opus agent → transform (startup-only); fresh Sonnet agent → ring-discard (LEADING).
- **Meta-lesson (`[[feedback_renderdoc_d3d9_vs_d3d11_is_the_diagnostic]]`):** the documented failure mode is all the AIs (incl. fresh Opus/Sonnet + Codex + Cursor) converging on the SAME wrong hypothesis. It struck TWICE this session (rebind, read-past). The lone contrarian (fresh Opus) was right about the startup transient. **Trust direct measurement of the ACTUAL spike over consensus.** That's why §7 (raw bytes of the real spike draw) is the plan, not another consult.

## 10. Evidence files
- Screenshots: `stage/screenshots/screenShot0259.jpg`, `screenShot0260.jpg` (Jawa spiking, this session).
- Probe outputs (regenerated each run): `stage/cape-collapse-probe.txt` (skin clean), `cape-d3d11-probe.txt` (write clean), `cape-draw-probe.txt` (offsets/discards), `cape-stage-probe.txt` (the staging/clipW/MINclipW data), `cape-strip-probe.txt` (none = no strips).
- `iter36d-sbssp-transform.txt` (engine appearance-world transforms, player ~3469.75/4.79/-4913.15).
- THE capture to analyze: **`stage/Capture-spike.rdc`** (Kenny producing now). Older `stage/Capture15.rdc` is baseline-but-AMBIGUOUS (debug_vertex gave sane o0 w=4.03 while export_mesh collapsed — don't trust it; use the fresh one).
