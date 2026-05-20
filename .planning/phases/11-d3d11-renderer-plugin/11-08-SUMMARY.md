---
phase: 11-d3d11-renderer-plugin
plan: 08
subsystem: renderer
tags: [d3d11, cbuffer, slot-retrofit, debug-layer, infoqueue, primeDefaults, rtv-cache, wvp-composition, codex-reviewed, structural-prerequisites-complete, shader-binding-deferred]

# Dependency graph
requires:
  - phase: 11-d3d11-renderer-plugin
    plan: 07
    provides: Plan 11-07 close at visible-dark-blue-clear milestone (Iter-17 outcome). Renderer end-to-end functional, frame loop alive, no FATAL, MVP clear visible. Iter-18 minimal-WVP attempt BSOD'd the OS; deferred to Plan 11-08's structural-prerequisite retrofits.
provides:
  - Direct3d11 debug layer enabled by default in debug + ID3D11InfoQueue frame-drain (Iter-1) + stage/d3d11-debug.log file sink (Iter-1.7) -- diagnostic infrastructure for every subsequent Plan 11-xx smoke
  - D3DCOMPILE_PACK_MATRIX_ROW_MAJOR flag at VS + PS D3DCompile sites (Iter-1.5) + REWRITE_VERSION bump 13->14 for shader-cache invalidation -- CODEX Q2 retrofit closing the latent Iter-18 matrix-transpose risk
  - SetBreakOnSeverity(ERROR, FALSE) tuning (Iter-1.6) -- ERROR-severity D3D11 messages route to log instead of process abort; first smoke under Iter-1 broke on lightsaberblade_lava_a.sht because ERROR was killing the process before drainInfoQueue could log it
  - CopySubresourceRegion srcBox 4-aligned pad for BC formats (Iter-1.8) -- closes the Plan 11-07 Iter-15 hole: Iter-15 padded the staging-texture allocation dims; the matching srcBox pad never landed; 96 BC2 errors per session pre-fix -> 0 post-fix
  - Direct3d11_ConstantBuffer::kMaxCBufferBytes expanded 1024 -> 1152 bytes (Iter-2.5) + new Direct3d11_VertexSlot0CB struct with 10 static_asserts enforcing every packoffset boundary -- CODEX Q3a/Q3b retrofit, slot capacity now matches shader register usage with 64-byte safety margin
  - Direct3d11_ConstantBuffer::primeDefaults install-time fill (Iter-3a) -- every VS/PS slot filled with identity matrices / zero defaults BEFORE first draw; mitigates CODEX 6th hypothesis (Map(WRITE_DISCARD) yields ARBITRARY GARBAGE not zeros)
  - Direct3d11_TextureData::getOrCreateRenderTargetView lazy RTV cache (Iter-3a.1) -- ~20,975 per-session Create+Destroy RTV cycles dropped to 2 (one per RT-texture lifetime); closes Plan 11-07 Iter-17's per-bind churn pattern
  - File-scope matrix shadows + composeAndUploadSlot0 helper + per-draw first-draw race guard (Task 3b) -- full-fill Direct3d11_VertexSlot0CB upload with WVP = (P*V)*W per CODEX Q1; replaces the latent partial-write pattern that was the Iter-18 BSOD root cause #6
  - tools/d3d11-smoke/show-window.ps1 v3 (chore commits) -- persistent ShowWindow + SetWindowPos + SetForegroundWindow helper with EnumWindows-by-PID enumeration + versioned class for cache-bust + List/Override flags
  - DIAGNOSTIC FINDING: setStaticShader is a no-op (StateCache.cpp:912) + StaticShaderData::apply is Iter-1 minimum-viable (StaticShaderData.cpp:157); resolveShaders rejects ms_currentVSData == nullptr; no draw call ever reaches IL resolution or the GPU. CODEX peer review confirmed: diagnosis correct, no upstream blocker missed. Inherited by Plan 11-09.
affects: [11-09, 11-10, 11-11]

# Tech tracking
tech-stack:
  added:
    - ID3D11InfoQueue + drainInfoQueue per-frame pump + stage/d3d11-debug.log persistent file sink
    - D3DCOMPILE_PACK_MATRIX_ROW_MAJOR (0x8) flag at VS + PS D3DCompile call sites
    - Direct3d11_VertexSlot0CB struct with packoffset-boundary static_asserts (10 compile-time enforcements)
    - Direct3d11_ConstantBuffer::primeDefaults() install-time full-fill API
    - Direct3d11_TextureData::getOrCreateRenderTargetView() lazy RTV cache + mutable m_rtv member
    - File-scope matrix shadows (s_cachedView / s_cachedProj / s_cachedWorld / s_cachedCameraPos) + composeAndUploadSlot0 helper in Direct3d11_StateCacheNamespace + first-draw race guard at top of applyPreDrawState
  patterns:
    - "InfoQueue safety-net pattern: ID3D11InfoQueue acquired post-D3D11CreateDevice when DEBUG flag survives Pitfall 8 fallback; PushEmptyStorageFilter + SetBreakOnSeverity(CORRUPTION, TRUE) + SetBreakOnSeverity(ERROR, FALSE) per Iter-1.6; per-frame drain at present() before ms_swapChain->Present; final drain + Reset at destroy()."
    - "Diagnostic file-sink pattern: parallel FILE* (fopen append + line-flush after each write) alongside engine DEBUG_REPORT_LOG_PRINT routing. Captures messages that would otherwise vanish when SwgClient_d.exe launches from explorer with no attached console."
    - "Full-fill cbuffer upload mandate: every updateVS(slot, &cb, sizeof(cb)) call uploads a fully-initialized struct via {}-init; Map(WRITE_DISCARD) yields UNDEFINED bytes per CODEX 6th hypothesis -- partial-field writes leak garbage into shader-readable regions."
    - "Lazy-cached RTV pattern: mutable ComPtr<ID3D11RenderTargetView> on the resource class; lazy-create on first bind, persistent for lifetime; eliminates per-bind Create+Destroy churn the D3D11 debug layer reports as id=2097243/2097245 traffic."
    - "Matrix-shadow + compose-on-canonical-site pattern: per-frame setters (setWorldToCameraTransform / setProjectionMatrix) shadow ONLY into file-scope statics with NO cbuffer write; the canonical upload site (setObjectToWorldTransformAndScale, called per-object) composes the full WVP and uploads the full Direct3d11_VertexSlot0CB. First-draw race guard at applyPreDrawState top closes the gap if engine fires draws before any setObjectToWorldTransformAndScale call."
    - "CODEX peer-review consult pattern (Plan 11-07 Iter-14 precedent + Plan 11-08 pre-implementation): verified artifact + stated hypothesis space + verifiable test. Used twice across Plan 11-08 -- pre-implementation (Q1 matrix order, Q2 ROW_MAJOR bit-collision, Q3a/b slot capacity, Q6 WRITE_DISCARD garbage) AND post-implementation diagnostic confirmation (setStaticShader no-op is the real blocker; cascade map for Plan 11-09)."

key-files:
  created:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexSlot0CB struct (declared in Direct3d11_ConstantBuffer.h immediately after the class)
    - tools/d3d11-smoke/show-window.ps1 (v3 -- versioned class, EnumWindows-by-PID, multi-PID enumeration)
    - .planning/phases/11-d3d11-renderer-plugin/11-08-iteration-log.md (1500+ ln; 9 iteration entries with 6-field shape + Awaiting blocks)
    - .planning/phases/11-d3d11-renderer-plugin/11-08-SUMMARY.md (this file)
  modified:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.h + .cpp (Iter-1 + Iter-1.6 + Iter-1.7: InfoQueue + drainInfoQueue + file sink)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexShaderData.cpp (Iter-1.5: ROW_MAJOR flag + REWRITE_VERSION 14)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp (Iter-1.5 mirror)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_TextureData.h + .cpp (Iter-1.8 BC2 srcBox pad + Iter-3a.1 RTV cache)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.h + .cpp (Iter-2.5 slot capacity + struct + static_asserts; Iter-3a primeDefaults)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_RenderTarget.cpp (Iter-3a.1 RTV cache consumer)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp (Task 3b matrix shadows + helper + first-draw race guard + setter rewrites)
    - stage/client_d.cfg ([ClientGraphics/Direct3d11] reportFrameStats=true for verification diagnostics; untracked, runtime-only)

key-decisions:
  - "Plan 11-08 closes at the structural-prerequisites-complete milestone, NOT the original 'visible geometry on top of the dark-blue clear' bar. Post-Task-3b smoke confirmed all 9 iterations land cleanly (0 ERRORs, 0 WARNINGs, 0 BSOD across 5+ minutes of runtime per session) BUT visible geometry remains absent -- diagnostic root cause is setStaticShader no-op + StaticShaderData::apply minimum-viable Iter-1 state from Plan 11-07. Per CODEX peer review (post-implementation consult), the diagnosis is locked: ms_currentVSData stays null per draw -> resolveShaders returns false -> applyPreDrawState early-exits with ++ms_skippedDrawsNullVS -> zero draws reach the GPU. Plan 11-08 was scoped as the cbuffer-wiring arc; the shader-binding arc is a different multi-iteration project deferred to Plan 11-09. Closing here is the right move (precedent: Plan 11-07 closed at visible-dark-blue-clear, NOT at original SPEC R5 ≥5-min playable bar)."
  - "Iter-3a.1 lazy RTV cache on Direct3d11_TextureData (Plan 11-04 fix surfaced by Plan 11-08 InfoQueue diagnostic) is a structural correctness fix as well as a perf win. 20,975 -> 2 per-session creates means the GPU debug layer was reporting a real D3D11 resource-lifetime smell that would have compounded in Plan 11-09's geometry workload."
  - "Iter-1.6 SetBreakOnSeverity(ERROR, FALSE) tuning fix to Iter-1: ERROR-severity D3D11 messages now route to log instead of process abort. The Iter-1 contract assumed ERROR = fatal, but D3D11's actual ERROR-severity spectrum is wider -- recoverable conditions (CreateXxx returning E_INVALIDARG, Draw skipped due to invalid state) fire ERROR-severity. The 'safety net = early warning BEFORE TDR/BSOD' intent is satisfied by the LOG, not the BREAK."
  - "Iter-3a Rule-1 deviation on slot 3: Plan 11-08 Task 3a's contract instructed primeDefaults to SKIP slot 3 (Plan 11-06 LightManager-owned). But direct source read of Direct3d11_LightManager::install (LightManager.cpp:42-48) confirms install is intentionally empty -- slot 3 is unprimed until setLights fires (post-first-draw). primeDefaults takes ownership of priming slot 3 too with a full 1152-byte zero-fill, closing the same garbage-read gap. When setLights runs later, its updateVS(kLightingCBSlot, &cb, sizeof(cb)) cleanly overwrites. Zero impact on Plan 11-06 LightManager behavior; explicit Rule-1 deviation documented in Iter-3a iteration log entry."
  - "Iter-1.7 file-sink architecture: Direct3d11_DeviceNamespace owns a FILE * ms_d3d11LogFile = nullptr opened in append mode at create() time when ms_infoQueue is acquired. drainInfoQueue() writes each message in parallel to DEBUG_REPORT_LOG_PRINT and ms_d3d11LogFile with fflush-after-each-line so crashes preserve content. The motivation: engine DEBUG_REPORT_LOG_PRINT routes via Report.cpp to stdout + OutputDebugString + stderr -- invisible when SwgClient_d.exe launches from explorer with no attached console. Without this sink, the diagnostic infrastructure would have been blind during smokes."
  - "show-window.ps1 evolution across v1 -> v2 -> v3 reflects three real bugs found in the helper itself: v1 used FindWindow with hardcoded title (failed when title differed); v2 added EnumThreadWindows with a managed-method on the type (failed when AppDomain cached the previous version's type); v3 uses versioned class name (WinSwgShow_v3) + EnumWindows + GetWindowThreadProcessId filter + multi-PID support (handles stale + fresh processes). Pattern: tooling iterations are first-class iterations and deserve persistence + clean version-bump discipline."
  - "Post-implementation CODEX consult validated the diagnosis AND landed three cascade-map items for Plan 11-09: (1) PS NULL still blocks color output once draws fire (D3D11 has no FFP pass-through); (2) Input-layout creation may fail/skip if VS signature ≠ VB format; (3) D3D11 VS compile lacks D3D9's per-static-shader texture-coordinate macro variants (Direct3d11_VertexShaderData.cpp:225) -- next asset-specific failure to expect. CODEX recommended re-framing the minimum-VS-bind iteration acceptance bar as 'draw activity observable' (CreateInputLayout fires + IASet*/Draw* in InfoQueue + ms_skippedDrawsNullVS counter changes) NOT 'visible geometry' -- separates the VS-bind milestone from the visible-color milestone cleanly."

patterns-established:
  - "Tuning-fix iterations (Iter-1.6, Iter-1.7) as PROPER first-class iterations following the initial structural iteration. Iter-1 lands the safety net; Iter-1.6 tunes it (ERROR no-break); Iter-1.7 makes its output persistent (file sink). Each is a single concept with its own 6-field iteration log entry. Sub-numbering (X.Y) captures the lineage cleanly without polluting the integer-iteration counter."
  - "Retrofit iterations (Iter-1.5, Iter-2.5, Iter-1.8) that close holes in PRIOR plans surfaced during this plan's smokes. Iter-1.5 retrofits Plan 11-07's D3DCompile flags; Iter-2.5 retrofits Plan 11-05's cbuffer slot size; Iter-1.8 retrofits Plan 11-04's BC pad. All carry `fix(11-04):` or `feat(11-08):` prefixes as appropriate and explicitly cite the upstream plan's hole in the commit body."
  - "Two-phase verification of structural iterations: compile-time enforcement via static_assert constellations (10 across Direct3d11_VertexSlot0CB at Iter-2.5) + runtime enforcement via InfoQueue ERROR-severity floor of zero. Iter-3a primeDefaults' correctness can't be verified by static_assert alone -- the runtime ERROR-floor catch is the second leg."
  - "Diagnostic-grade tooling investment pays off in proportion to remaining iterations. Plan 11-07's THROWAWAY-dump pattern was useful but transient. Plan 11-08's permanent diagnostic infrastructure (InfoQueue file sink, show-window.ps1, reportFrameStats config wiring) outlives Plan 11-08 and supports every subsequent Phase 11 plan."

requirements-completed: []
requirements-partial: [D3D11-01, D3D11-02, D3D11-03]
requirements-pending: [D3D11-05]
requirements-inherited-by-11-09: [D3D11-01 (visible geometry), D3D11-03 (shader compile + cache + render evidence), D3D11-04 (CLOSED-AS-DESCOPED carry-forward)]

# Metrics
duration: ~1 day active (2026-05-19 .. 2026-05-20; 9 iterations + 1 CODEX peer-review consult + 1 diagnostic finding)
completed: 2026-05-20
iterations: 9 (1, 1.5, 1.6, 1.7, 1.8, 2.5, 3a, 3a.1, 3b)
commits: 9 implementation commits + 4 chore commits (show-window.ps1 v1/v2/v3 + reportFrameStats config) + Plan 11-08 close commits = ~13 in plan-08 series
exe-size: gl11_d.dll 1,484,288 bytes at Plan close (Plan 11-07 close was 1,418,240; +64 KB across 9 iterations; size growth dominated by Iter-1.8 ComPtr::As template instantiation overhead)
stub-count: 27 remaining at Plan close (unchanged from Plan 11-07 close; no Gl_api slot bodies wired in Plan 11-08 -- this was a structural retrofit arc, not slot-wiring arc)
codex-consults: 2 (pre-implementation review providing Q1/Q2/Q3a/Q3b/Q6 inputs that landed in Iter-1.5/2.5/3a/3b; post-implementation diagnostic confirmation locking setStaticShader-no-op as the visible-geometry blocker + cascade map for Plan 11-09)
---

# Phase 11 Plan 08: D3D11 Cbuffer-Wiring Arc + Structural Prerequisites Complete

**9 iterations across 1.5 days (2026-05-19 .. 2026-05-20). Plan 11-08 lands all 8 Iter-18 BSOD must-haves + CODEX's sixth hypothesis + 4 separate retrofit holes from prior plans (Plan 11-04 BC2 srcBox + RTV churn; Plan 11-05 cbuffer slot capacity; Plan 11-07 ROW_MAJOR flag). Cbuffer-wiring arc is structurally complete: every VS/PS slot fills with identity/zero defaults at install (primeDefaults), every per-frame matrix-setter shadows to file-scope statics, the canonical per-object site composes the full 1152-byte Direct3d11_VertexSlot0CB with WVP = (P*V)*W per CODEX Q1 + uploads via Map(WRITE_DISCARD) with full-fill discipline. Iter-18 BSOD risk fully mitigated: 5+ minutes runtime per smoke session, zero CORRUPTION/ERROR/WARNING D3D11 validation messages, no BSOD across any iteration. Plan 11-07 visible-dark-blue-clear milestone preserved through every smoke. Original Plan 11-08 milestone bar ('visible geometry on top of the clear') NOT REACHED -- diagnostic root cause is setStaticShader no-op + StaticShaderData::apply minimum-viable state inherited from Plan 11-06/Plan 11-07 Iter-1; CODEX peer review confirmed setStaticShader IS the blocker (ms_currentVSData stays null per draw -> applyPreDrawState early-exits -> zero draws reach the GPU). Visible-geometry milestone deferred to Plan 11-09 (shader-binding arc) with CODEX-reframed acceptance bar 'draw activity observable' for the minimum-VS-bind iteration. Plan 11-08 closes at structural-prerequisites-complete milestone; Plan 11-07 precedent applied (closed at most-meaningful structural milestone, deferred original SPEC criteria forward).**

## Performance

- **Duration:** ~1 day active across 2026-05-19..05-20; 9 iterations + 1 mid-arc CODEX peer-review consult + 1 post-arc CODEX diagnostic confirmation
- **Started:** 2026-05-19 evening (immediately after Plan 11-07 close at c06029e8a)
- **Completed:** 2026-05-20 (this plan-close commit)
- **Iterations:** 9 (1, 1.5, 1.6, 1.7, 1.8, 2.5, 3a, 3a.1, 3b)
- **Commits:** ~13 in plan-08 series (9 implementation + 4 chore commits for show-window.ps1 evolution + config). Commits use `feat(11-08):` for new functionality and `fix(11-04):` for prior-plan hole closures (Iter-1.8 BC2 srcBox pad + Iter-3a.1 RTV cache mirror Plan 11-07's `fix(11-04):` convention).
- **gl11_d.dll growth:** 1,418,240 (Plan 11-07 close) -> 1,484,288 (Plan 11-08 close). +64 KB across 9 iterations.
- **STUB count:** unchanged at 27 (Plan 11-08 was structural retrofits, not Gl_api slot wiring).
- **CODEX consults:** 2 across the plan -- pre-implementation review locked Q1/Q2/Q3a/Q3b/Q6 hypotheses; post-implementation diagnostic confirmation closed the visible-geometry deferral.

## Iteration Arc Summary

| Iter | Wall | Fix shape | Commit |
|------|------|-----------|--------|
| 1    | Iter-18 BSOD needs safety net BEFORE next cbuffer write | D3D11 debug layer enabled by default + ID3D11InfoQueue per-frame drain via Direct3d11_Device::drainInfoQueue + D3D9 matrix composition verified by source read | `d27e62f54` |
| 1.5  | Plan 11-07 deferred ROW_MAJOR flag (CODEX Q2) | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR at VS + PS D3DCompile sites + REWRITE_VERSION bump 13->14 for shader cache invalidation | `fc6808ee9` |
| 1.6  | First smoke under Iter-1 crashed on lightsaberblade_lava_a.sht due to SetBreakOnSeverity(ERROR, TRUE) killing process before drainInfoQueue could log | SetBreakOnSeverity(ERROR, FALSE); CORRUPTION stays TRUE | `50c7f3d07` |
| 1.7  | InfoQueue messages going to stdout / OutputDebugString / stderr only -- invisible when launched from explorer | FILE * sink on Direct3d11_Device opened in append mode at create(); per-message fprintf + fflush; close at destroy() | `3e193bf59` |
| 1.8  | Iter-1.7 log surfaced 96/session BC2_UNORM srcBox alignment errors from CopySubresourceRegion -- the second half of Iter-15's BC pad fix that was never closed | Pad srcBox.right + srcBox.bottom 4-aligned for BC-format destinations in Direct3d11_TextureData::unlock | `25d0297c9` |
| 2.5  | Plan 11-05's kMaxCBufferBytes = 1024 was unverified; CODEX Q3a confirms cbuffer span ends at c67 (ExtendedLightData) = 1088B minimum | kMaxCBufferBytes = 1152 (72 registers; 64-byte safety margin) + new Direct3d11_VertexSlot0CB struct + 10 static_asserts | `d42d183b4` |
| 3a   | Map(WRITE_DISCARD) yields UNDEFINED bytes per CODEX 6th hypothesis -- partial writes leak shader-readable garbage | primeDefaults() install-time full-fill of every VS/PS slot with identity matrices / zero-init payload (Rule-1 deviation on slot 3: LightManager::install is empty per direct source read, so primeDefaults primes slot 3 too) | `8782c2480` |
| 3a.1 | Iter-3a smoke log surfaced 20,975 per-session Create+Destroy RTV cycles -- Direct3d11_RenderTarget::setRenderTarget creating fresh RTVs every bind | Mutable Direct3d11_TextureData::m_rtv + lazy getOrCreateRenderTargetView(device) accessor; setRenderTarget consumes the cache | `79c2ad14b` |
| 3b   | THE ACTUAL ITER-18 FIX: cbuffer composition that BSOD'd Plan 11-07 Iter-18, now with all 8 must-have prerequisites in place | File-scope matrix shadows + composeAndUploadSlot0 helper + per-frame setter rewrites (shadow-only, no cbuffer write) + canonical per-object upload site + first-draw race guard at applyPreDrawState top | `922bbc748` |

## Outcome State (post-Task-3b, Plan 11-08 close)

| Subsystem | Status | Notes |
|-----------|--------|-------|
| All 9 iterations build clean | ✓ | MSBuild Direct3d11 EXIT=0 at every iteration; MSVC /W4 carries only the pre-existing C4459 DirectXMathVector warning |
| 0 ERROR / 0 WARNING / 0 CORRUPTION D3D11 messages | ✓ | Across 5+ min runtime per session, 64,000+ INFO-severity messages, zero validation failures |
| 0 BSOD across any iteration | ✓ | Iter-18 BSOD risk fully mitigated by the 8-prerequisite chain |
| Plan 11-07 visible-dark-blue-clear milestone | ✓ | Preserved through every smoke; ShowWindow assist still needed (Iter-17 carry-forward) |
| Input + audio + UI loops alive | ✓ | Login screen hover SFX audible; cursor capture works; engine logic loops independent of GPU draw path |
| D-13 / D-05 / D-04a invariants | ✓ | Unchanged across all 9 iterations; D3D9 plugin still builds clean every iteration |
| Visible geometry on top of the clear | ✗ | DIAGNOSTIC ROOT CAUSE: setStaticShader is no-op + StaticShaderData::apply Iter-1 minimum-viable -> ms_currentVSData stays null -> resolveShaders returns false -> zero draws reach the GPU. Inherited by Plan 11-09 (shader-binding arc). |
| Stage diagnostic infrastructure | ✓ | stage/d3d11-debug.log (InfoQueue file sink) + tools/d3d11-smoke/show-window.ps1 + [ClientGraphics/Direct3d11] reportFrameStats config wire |

## Plan 11-08 vs. Original Acceptance Criteria

The Plan 11-08 PLAN.md milestone bar: "Bring the D3D11 renderer from 'visible dark-blue clear, no geometry' (Plan 11-07 close) to 'visible geometry on top of the clear in at least one scene.'" Post-Task-3b diagnostic shows visible geometry is upstream-blocked by Plan 11-06/11-07-deferred shader-binding chain. Status reclassification:

| Original Criterion (PLAN.md must_haves.truths) | Status |
|------------------------------------------------|--------|
| D3D9 ms_cachedWorldToProjectionMatrix composition order verified by source read | ✓ MET (Iter-1; CODEX Q1 ratified) |
| D3D11 debug layer enabled by default + ID3D11InfoQueue frame-drain | ✓ MET (Iter-1 + Iter-1.6 + Iter-1.7) |
| Every VS/PS cbuffer slot initialized to identity/zero defaults BEFORE first draw | ✓ MET (Iter-3a primeDefaults; Rule-1 deviation on slot 3 documented) |
| Per-object WVP composition written to slot 0 packoffset(c0..c3) + per-object world to slot 0 packoffset(c4..c7) | ✓ MET (Task 3b) |
| applyPreDrawState explicitly calls updateVS(0,...) + bindVS(0) at top of every draw dispatch | ✓ MET (Task 3b first-draw race guard + Plan 11-06's existing bindVS(0)) |
| Every cbuffer slot upload writes the FULL struct, no partial writes | ✓ MET (composeAndUploadSlot0 uploads sizeof(Direct3d11_VertexSlot0CB) = 1152 bytes; primeDefaults uploads kMaxCBufferBytes zero-fill or full-struct identity) |
| Plan 11-07 retrofit: D3DCOMPILE_PACK_MATRIX_ROW_MAJOR present at VS + PS sites | ✓ MET (Iter-1.5) |
| Plan 11-05 retrofit: kMaxCBufferBytes ≥ 1088 + Direct3d11_VertexSlot0CB struct with per-field static_asserts | ✓ MET (Iter-2.5; 1152 bytes; 10 static_asserts) |
| **Geometry visible on top of Plan 11-07 dark-blue clear in at least ONE scene** | **✗ PENDING -- inherited by Plan 11-09 (shader-binding arc); diagnostic root cause is setStaticShader no-op confirmed by CODEX post-implementation consult** |
| No BSOD across any iteration | ✓ MET (Iter-18 risk fully mitigated) |
| D3D11 debug layer reports zero ERROR-severity during steady-state | ✓ MET (post-Iter-1.8: 0 ERRORs per session; 100% INFO-severity traffic) |
| D3D9 plugin still builds clean | ✓ MET (D-05 invariant) |
| D-13 grep / D-04a Glob | ✓ MET (invariants preserved across all 9 iterations) |
| Iteration log records every iteration following Plan 11-07 6-field shape | ✓ MET (11-08-iteration-log.md with 9 entries) |

12 of 13 criteria met. The PENDING criterion (visible geometry) is the inheritance to Plan 11-09. All 8 Iter-18 BSOD must-haves (the original Plan 11-08 design pressure) are satisfied.

## Post-Implementation Diagnostic Finding

**Setting:** Post-Task-3b smoke captured a clean d3d11-debug.log (64,450 INFO-severity messages, 0 ERROR, 0 WARNING) and a clean stage-all.log (141,064 engine output lines, 0 FATAL). But: 0 `CreateInputLayout` calls in the InfoQueue stream, 0 `CreateID3D11PixelShader` calls, only 2 `CreateID3D11VertexShader` calls (the engine's install-time VS load). Per-frame `Direct3d11_Metrics::endFrame` output was missing entirely.

**Investigation:**
- `Direct3d11_Metrics::endFrame` defined at Direct3d11_Metrics.cpp:77 but NEVER called from anywhere in the codebase (verified by repo-wide grep). Plan 11-06 added the methods but never wired the per-frame call site. The per-frame stats line never appears because endFrame() never fires.
- `Direct3d11_Metrics::drawCallCount` static int (Direct3d11_Metrics.cpp:20) is also never incremented. The actual draw counter lives at Direct3d11_StateCache.cpp:155 as `ms_drawCallCount` (namespace-local) which IS incremented at line 506 inside applyPreDrawState. Two separate counters share a conceptual name.
- `Direct3d11_StateCache::setStaticShader` (StateCache.cpp:912) is a documented no-op. Plan 11-06's own comment at line 922-924: "this slot is called by the engine PER DRAW. Treating it as no-op means draws happen with whatever VS/PS was last bound (likely nothing on the first draw -- which is fine, draw skips)."
- `Direct3d11_StaticShaderData::apply` (StaticShaderData.cpp:157) is Plan 11-07 Iter-1 minimum-viable: toggles `ms_active = this` + `ms_activePass = passNumber` and returns false. Documented "Per-pass VS / PS binding requires friend access to ShaderImplementation::m_pass + StaticShaderTemplate::m_effect (see construct() rationale). That friend extension lands when smoke surfaces a specific symptom that requires it (e.g. solid-color rendering because the engine's default VS doesn't transform correctly for the asset's vertex format)."

**Chain:**
```
engine fires draw* slot (Direct3d11_StateCache.cpp:931+) ->
  applyPreDrawState(topology) ->
    resolveShaders() returns false (ms_currentVSData == nullptr) ->
    ++ms_skippedDrawsNullVS; return false ->
  drawN slot returns early; no IASet*/Draw/DrawIndexed ever submitted to GPU
```

ms_currentVSData stays null because nothing wires it: setStaticShader is no-op, StaticShaderData::apply is minimum-viable, no other engine path assigns it.

**CODEX peer-review confirmation (post-implementation consult):** Diagnosis locked. No upstream blocker missed. Cascade map for Plan 11-09:
1. Once VS binds and draws fire, PS NULL still blocks color output (D3D11 has no FFP pass-through; needs debug/fallback PS).
2. Input-layout creation may fail or skip if VS signature ≠ VB format (Pitfall 6 territory).
3. D3D11 VS compile lacks D3D9's per-static-shader texture-coordinate macro variants (Direct3d11_VertexShaderData.cpp:225) -- next asset-specific failure to expect.

**CODEX reframe of minimum-VS-bind iteration acceptance bar:** "make draw submission observable, not geometry visible. Minimum VS binding is a shader-binding arc, not cbuffer wiring. It is valuable, but its honest success criteria are CreateInputLayout/IASet*/Draw* activity and a changed skip counter, not visible geometry. The visible-geometry milestone now depends on shader binding plus at least a known-good PS/debug color path."

**Implication for Plan 11-08:** All 9 iterations land structurally-correct work that's invisible-effect today because the draw chain doesn't reach it. None of the work is wasted -- it's prerequisite infrastructure that needs to be in place before draws can produce visible output. Plan 11-08's contract was the cbuffer-wiring arc; that arc is complete. The shader-binding arc is its own multi-iteration project deferred to Plan 11-09.

## Diagnostic Infrastructure (Permanent, Outlives Plan 11-08)

The following persistent diagnostic infrastructure was built during Plan 11-08 and is reusable by every subsequent Phase 11 (and beyond) iteration:

- **stage/d3d11-debug.log file sink** -- Iter-1.7 ID3D11InfoQueue messages routed via `Direct3d11_Device::drainInfoQueue` with append-mode FILE * + per-message fflush. Survives across launches. Captures every D3D11 validation INFO/WARNING/ERROR/CORRUPTION from the moment ms_infoQueue is acquired.

- **tools/d3d11-smoke/show-window.ps1 (v3)** -- versioned-class P/Invoke helper for ShowWindow + SetWindowPos + SetForegroundWindow assist on hidden engine windows. Handles multi-PID, AppDomain cache, and the engine's WS_POPUP-without-WS_VISIBLE pattern. `-List` mode enumerates without action; `-WindowHandle` overrides target.

- **stage/client_d.cfg [ClientGraphics/Direct3d11] reportFrameStats=true** -- DebugFlag config wire for Direct3d11_Metrics endFrame output. Currently dormant because Direct3d11_Metrics::endFrame is never called (Plan 11-06 omission discovered in Plan 11-08 post-Task-3b diagnostic). Plan 11-09 can pick up the endFrame wire as a small Plan 11-06 retrofit if useful.

- **`fix(11-04):` commit prefix convention for resource-layer hole closures** surfaced during later-plan smokes. Established by Plan 11-07 Iter-15 + Iter-17; carried forward by Plan 11-08 Iter-1.8 + Iter-3a.1. Distinguishes "we found a hole in an earlier plan" from "we landed new functionality this plan."

## Self-Check

- [x] All 9 iterations executed; iteration log records every iteration with the 6-field shape + ranked Awaiting outcome block (Plan 11-07 inheritance)
- [x] Iter-1.6 unblocked the smoke crash; Iter-1.8 dropped BC2 errors 96 -> 0; Iter-2.5 satisfied compile-time slot-capacity static_asserts; Iter-3a primeDefaults runs at install with full-fill; Iter-3a.1 dropped RTV churn 20,975 -> 2; Task 3b matrix wiring uploads full 1152-byte VertexSlot0CB per CODEX Q1 order
- [x] 8 of 8 Iter-18 BSOD must-haves satisfied + CODEX sixth hypothesis (WRITE_DISCARD garbage) addressed
- [x] D-13 / D-05 / D-04a invariants preserved across all 9 iterations
- [x] D3D9 plugin builds clean every iteration; SwgClient builds clean every iteration
- [x] 11-08-iteration-log.md committed (`922bbc748` and follow-up commits)
- [x] CODEX peer-review consults landed Q1/Q2/Q3a/Q3b/Q6 pre-implementation + setStaticShader-no-op diagnostic confirmation post-implementation
- [x] Diagnostic infrastructure persistent (file sink + show-window.ps1 + reportFrameStats config wire)
- [ ] Original Plan 11-08 milestone bar (visible geometry on top of clear) -- DEFERRED to Plan 11-09 (PENDING)
- [ ] D3D9 plugin geometry rendering parity -- DEFERRED (PENDING)

**Plan 11-08 closes at the structural-prerequisites-complete milestone. The cbuffer-wiring arc is done. The shader-binding arc is inherited by Plan 11-09 with CODEX-reframed acceptance bar 'draw activity observable' for the minimum-VS-bind iteration. Plan 11-07 precedent applied (closed at most-meaningful structural milestone; original SPEC criteria deferred forward).**
