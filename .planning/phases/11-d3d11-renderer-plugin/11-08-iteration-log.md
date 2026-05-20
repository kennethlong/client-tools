# Phase 11 — Plan 08 Iteration Log

**Started:** 2026-05-19
**Status:** In progress — Iter-1 + Iter-1.5 landed (debug-layer safety net + ROW_MAJOR retrofit); awaiting Kenny smoke #1 (Task 2 checkpoint) to confirm debug-layer wiring is live + ROW_MAJOR shader recompile is clean + Plan 11-07 visible-dark-blue-clear milestone is preserved before any cbuffer-write changes (Task 2.5 onward).

**Plan reference:** `11-08-PLAN.md` (cbuffer-wiring arc; 7 tasks across the 4 safety-prerequisite waves + the actual Iter-18-fix wave + the iterative-smoke fix-by-fix loop)

**Iteration-shape contract** (6 fields + Awaiting block, inherited from Plan 11-07):

```
- Date
- Symptom (or Predicted symptom)
- Hypothesis
- Investigation
- Root cause
- Fix
- Verification (11-gate set: MSBuild D3D11/D3D9/_ffp/_vsps/SwgClient EXIT=0; D-13/D-05/D-04a invariants; FFP scan; STUB count delta; MSVC /W4 zero new warnings; ERROR-severity debug-layer message count; ROW_MAJOR flag present; slot capacity ≥1088B)
- Commits
- Awaiting Kenny smoke (ranked outcomes: best / likely / possible / less likely)
```

---

## Iteration 1: D3D9 matrix-composition trace + D3D11 debug layer enabled-by-default + ID3D11InfoQueue frame-drain (the safety net)

- **Date:** 2026-05-19
- **Predicted symptom:** none — pre-implementation safety-net iteration. The Plan 11-07 Iter-18 BSOD established that the next cbuffer-wiring bug MUST surface as a logged validation warning BEFORE TDR escalates to BSOD. Without the InfoQueue drain in place, Plan 11-08's first cbuffer-write iteration could repeat the Iter-18 BSOD pattern with no diagnostic trace recoverable from disk.
- **Hypothesis:** Three changes land atomically:
  1. **D3D9 matrix composition order verified by direct source read** of `src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp` lines 3271-3365 (setWorldToCameraTransform + setProjectionMatrix VSPS branches) AND lines 4030-4040 (draw-time WVP composition). Planner+CODEX pre-locked `wtp = XMMatrixMultiply(P, V); wvp = XMMatrixMultiply(wtp, W)` per CODEX Q1 ratification. Executor verifies against the live file.
  2. **D3D11 debug layer already defaults to enabled in `_DEBUG` builds** per `ConfigDirect3d11.cpp:58-62` (KEY_BOOL enableDebugLayer default `true` in `#ifdef _DEBUG` branch, `false` otherwise). No code change required to satisfy the Task 1 step 3 contract — only verification.
  3. **ID3D11InfoQueue wiring** — add a namespace-private `ComPtr<ID3D11InfoQueue> ms_infoQueue` to `Direct3d11_DeviceNamespace`, query it from `ms_device` after D3D11CreateDevice succeeds AND the debug flag is live; configure with `PushEmptyStorageFilter` + `SetBreakOnSeverity(CORRUPTION, TRUE)` + `SetBreakOnSeverity(ERROR, TRUE)`; expose `Direct3d11_Device::drainInfoQueue()` static method; hook into `present()` BEFORE the `ms_swapChain->Present(...)` call; release in `destroy()` BEFORE `ms_device.Reset()`.
- **Investigation:**
  - **D3D9 composition read (line 3271-3365 + 4030-4040):**
    - `Direct3d9.cpp:3291` (setWorldToCameraTransform, VSPS branch): `D3DXMatrixMultiply(&ms_cachedWorldToProjectionMatrix, &ms_cachedProjectionMatrix, &ms_cachedWorldToCameraMatrix);` → output = ms_cachedProjectionMatrix * ms_cachedWorldToCameraMatrix = `P * V`. D3DXMatrixMultiply signature is `out = pM1 * pM2` per the official Direct3D 9 API contract.
    - `Direct3d9.cpp:3359` (setProjectionMatrix, VSPS-only branch — `#elif defined(VSPS)`): identical multiplication order `D3DXMatrixMultiply(&ms_cachedWorldToProjectionMatrix, &ms_cachedProjectionMatrix, &ms_cachedWorldToCameraMatrix);` → `P * V`. Computed when projection matrix changes; mirrors the setWorldToCameraTransform site.
    - `Direct3d9.cpp:4034` (draw-time, `#else` branch = VSPS-only, NOT `#ifdef FFP`): `D3DXMatrixMultiply(matrices+0, &ms_cachedWorldToProjectionMatrix, &ms_cachedObjectToWorldMatrix);` → matrices[0] = ms_cachedWorldToProjectionMatrix * ms_cachedObjectToWorldMatrix = `(P * V) * W` = `P * V * W`. This is the WVP that ships to slot 0 packoffset c0.
    - `Direct3d9.cpp:4035`: `matrices[1] = ms_cachedObjectToWorldMatrix;` — direct assignment, NO transpose. This is the objectWorldMatrix that ships to slot 0 packoffset c4. The VSPS branch uses row-major-no-transpose; the FFP branch at 4031-4032 uses `D3DXMatrixMultiplyTranspose` + `D3DXMatrixTranspose` because FFP expects column-major for `SetTransform` API contract. VSPS does NOT transpose because vertex shaders read row-major matrices natively when compiled with `D3DCOMPILE_PACK_MATRIX_ROW_MAJOR` (Task 1.5 retrofit) and use row-vector multiplication (`mul(vector, matrix)` = `vector * matrix`).
  - **Verdict vs planner+CODEX pre-lock:** **CONFIRMED.** Executor's reading matches the planner's record verbatim. `wtp = XMMatrixMultiply(P, V); wvp = XMMatrixMultiply(wtp, W)` is the correct port-forward to XMMath. No CHECKPOINT surfaced; CODEX Q1 ratification holds. Proceed with Task 1 implementation.
  - **ConfigDirect3d11 debug-layer default audit:** `ConfigDirect3d11.cpp:58-62` already has the correct shape — `#ifdef _DEBUG KEY_BOOL(enableDebugLayer, true) #else KEY_BOOL(enableDebugLayer, false) #endif`. No edit required. `getEnableDebugLayer()` at line 75 returns the registered value; in debug builds without an explicit `stage/client_d.cfg [Direct3d11] enableDebugLayer=` override, the function returns `true`. Iter-1 contract step 3 satisfied by verification, not by edit.
  - **Direct3d11_Device.cpp existing wiring (lines 124-170):** `D3D11_CREATE_DEVICE_DEBUG` is conditionally OR'd into `createDeviceFlags` under `#ifdef _DEBUG && ConfigDirect3d11::getEnableDebugLayer()` (line 128-131); Pitfall 8 fallback path strips the flag and retries without DEBUG if `DXGI_ERROR_SDK_COMPONENT_MISSING` fires (lines 148-159). Existing structure already supports the InfoQueue addition — after the second `D3D11CreateDevice` attempt resolves, if `createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG` is still set, query the InfoQueue from `ms_device`. The Pitfall 8 fallback path leaves the DEBUG bit cleared, so the InfoQueue query never fires when the SDK component is missing — no need for separate guarding.
  - **`present()` body (lines 349-364):** clean hook point just before `ms_swapChain->Present(1, 0)` at line 354. Insert `drainInfoQueue();` between the `if (!ms_swapChain) return false;` guard and the `Present` call. Single message ID format suffices.
- **Root cause:** none — this is a structural pre-implementation iteration. Task 1's contract is "the safety net exists in code AND is enabled by default in debug builds AND drains every frame," not "fix bug X."
- **Fix:** Three file edits + one new artifact:
  1. **`.planning/phases/11-d3d11-renderer-plugin/11-08-iteration-log.md`** (this file) — created.
  2. **`src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.h`** — extended with `static void drainInfoQueue();` static method declaration in `Direct3d11_Device` class body.
  3. **`src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp`** — extended:
     - `#include <d3d11sdklayers.h>` (for ID3D11InfoQueue + D3D11_MESSAGE_*) — included by the SDK alongside `<d3d11.h>` on Windows SDK 10+, but explicit include is correct hygiene.
     - `ComPtr<ID3D11InfoQueue> ms_infoQueue` added to `Direct3d11_DeviceNamespace` state block.
     - After the second `D3D11CreateDevice` resolves (line 159 region), conditionally query `ms_infoQueue` from `ms_device` when `createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG` is still set. Configure:
       - `PushEmptyStorageFilter()` (start with no filtering — capture everything; later iterations can tighten)
       - `SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE)` (corruption is unrecoverable — trap into debugger)
       - `SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE)` (errors in debug — same)
     - `Direct3d11_Device::drainInfoQueue()` implementation: loop `GetNumStoredMessages` → `GetMessage` (size probe via NULL → allocate → fill → format severity-category-ID-description → `DEBUG_REPORT_LOG_PRINT(true, ...)`) → `ClearStoredMessages`. Use a stack-bounded buffer (or heap if message is large) to avoid per-frame allocation in the steady-state-clean case.
     - `present()` hook — call `drainInfoQueue()` immediately before `ms_swapChain->Present(1, 0)`.
     - `destroy()` — `ms_infoQueue.Reset()` BEFORE `ms_device.Reset()` (proper COM release order; even with ComPtr, explicit pre-device reset eliminates a debug-layer late-destruction warning).
- **Verification (11-gate set):**
  - Gate 1: `MSBuild D:/Code/swg-client-v2/src/build/win32/swg.sln /t:Direct3d11 /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` — EXPECTED EXIT=0.
  - Gate 2: `MSBuild ... /t:Direct3d9` — EXPECTED EXIT=0 (D-05 — should not have touched any D3D9 file).
  - Gate 3: `MSBuild ... /t:Direct3d9_ffp` — EXPECTED EXIT=0.
  - Gate 4: `MSBuild ... /t:Direct3d9_vsps` — EXPECTED EXIT=0.
  - Gate 5: `MSBuild ... /t:SwgClient` — EXPECTED EXIT=0.
  - Gate 6: D-13 grep — non-comment hits for `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` in `src/engine/client/application/Direct3d11/` must be 0.
  - Gate 7: D-05 `git diff` (committed range) against `src/engine/client/application/Direct3d9/` — must be empty (excluding pre-existing Koogie working-tree dirt per `feedback_dont_modify_koogie_changes.md`).
  - Gate 8: D-04a Glob `Direct3d11_FfpGenerator.*` — must return 0 results.
  - Gate 9: FFP scan on modified TUs — must be 0 functional `#ifdef FFP / D3DTSS_ / D3DTOP_`.
  - Gate 10: MSVC `/W4` — zero new warnings on the touched TUs.
  - Gate 11: ERROR-severity debug-layer message count — N/A pre-debug-layer-confirmation. First measurement at Task 2 (Kenny smoke #1) checkpoint after debug-layer wiring is live and the dark-blue-clear frame loop runs.
  - Gate 12 (ROW_MAJOR flag present): N/A this iteration — Task 1.5 lands the flag.
  - Gate 13 (slot capacity ≥1088B): N/A this iteration — Task 2.5 lands the slot-size retrofit.
  - STUB count: unchanged at 27 (no slot wiring changed in this iteration).
- **Commits:** one commit `feat(11-08): enable D3D11 debug layer by default in debug + ID3D11InfoQueue frame-drain; record D3D9 matrix composition order verified vs CODEX Q1 pre-ratification`.
- **Awaiting Kenny smoke (after Iter-1.5 also lands):** see Iter-1.5 below + Task 2 checkpoint contract — both iterations bundle into a single smoke event because Task 1.5 invalidates the shader cache and Task 2's checkpoint specifically asks for "post-Iter-1 + post-Iter-1.5" combined verification.

---

## Iteration 1.5: Plan 11-07 retrofit — D3DCOMPILE_PACK_MATRIX_ROW_MAJOR (0x8) added to VS + PS D3DCompile flags; D3D11_REWRITE_VERSION 13 → 14

- **Date:** 2026-05-19
- **Predicted symptom:** none — pre-implementation retrofit iteration. Without `D3DCOMPILE_PACK_MATRIX_ROW_MAJOR`, HLSL `float4x4` defaults to COLUMN-MAJOR storage; XMFLOAT4X4 (row-major bytes) uploaded to slot 0 would be read TRANSPOSED by the shader (Iter-18 BSOD Hypothesis 1 redux, structurally guaranteed by the layout mismatch). CODEX Q2 mandate.
- **Hypothesis:** Adding `D3DCOMPILE_PACK_MATRIX_ROW_MAJOR (1 << 3 = 0x8)` to the `flags` bitfield at BOTH D3DCompile call sites — `Direct3d11_VertexShaderData.cpp` (VS site) and `Direct3d11_PixelShaderProgramData.cpp` (PS site, the `[[maybe_unused]]` ps_5_0 SPEC R3 compile-time proof helper) — forces HLSL `float4x4` to compile as row-major. Combined with bumping `D3D11_REWRITE_VERSION 13 → 14` at both sites (so the FNV-1a hash in `Direct3d11_ShaderCache::hashSource` invalidates every cached `.cso` blob on next launch), this delivers a clean first-launch shader recompile under the new flag.
- **Investigation:**
  - **Bit-collision audit:** `D3DCOMPILE_PACK_MATRIX_ROW_MAJOR` is `(1 << 3) = 0x8`. The previously-dropped `D3DCOMPILE_ENABLE_STRICTNESS` is `(1 << 11) = 0x800`. Different bits entirely — adding ROW_MAJOR does NOT reintroduce the X3116 STRICTNESS-vs-BACKWARDS_COMPATIBILITY conflict that Iter-5/6 resolved. CODEX Q2 ratification confirms this.
  - **Existing VS flag bitfield read** (`Direct3d11_VertexShaderData.cpp:280-490`): the bitfield assembles `UINT flags = 0;` → `flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION` (in debug) or `D3DCOMPILE_OPTIMIZATION_LEVEL3` (in release) → `flags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY` → fed into `D3DCompile`. Insertion point: immediately after the `BACKWARDS_COMPATIBILITY` line, with an explanatory comment block referencing CODEX Q2.
  - **Existing PS flag bitfield read** (`Direct3d11_PixelShaderProgramData.cpp:80-220`): same shape as VS site — mirrors the bitfield assembly. Same insertion point.
  - **`D3D11_REWRITE_VERSION` macro location:** both VS + PS sites push `defines` vector entries `{ "D3D11_REWRITE_VERSION", "13" }` into the D3DCompile shader-define list. Bump to `"14"` at BOTH sites for cache-key symmetry; either site bumping alone would leave the other site's cached blobs stale.
- **Root cause:** none — this is a structural retrofit iteration. CODEX peer review identified the gap during Plan 11-08 design; Plan 11-07 closed before the gap was identified, so the retrofit lands here as Plan 11-07 follow-on work properly scoped to Plan 11-08's safety-prerequisite wave.
- **Fix:** two file edits:
  1. **`src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexShaderData.cpp`** — added `flags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;` immediately after the `BACKWARDS_COMPATIBILITY` line, with a CODEX Q2 reference comment block. Bumped `D3D11_REWRITE_VERSION` define from `"13"` to `"14"` with a one-line `// Iter-1.5: ROW_MAJOR flag retrofit per CODEX Q2.` comment on the bump line.
  2. **`src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp`** — mirror of (1); same flag + same comment block + same REWRITE_VERSION bump.
- **Verification (11-gate set):**
  - Gates 1-10: same expected outcomes as Iter-1 (build EXIT=0 across all 5 targets, D-13/D-05/D-04a invariants, MSVC /W4 clean).
  - Gate 11: still N/A pre-debug-layer-smoke.
  - Gate 12 (ROW_MAJOR flag present): `grep -n D3DCOMPILE_PACK_MATRIX_ROW_MAJOR src/engine/client/application/Direct3d11/src/win32/Direct3d11_*Shader*.cpp` MUST return one hit per file (2 total).
  - Gate 13 (slot capacity ≥1088B): still N/A — Task 2.5 lands the retrofit.
  - STUB count: unchanged at 27.
- **Commits:** one commit `feat(11-08): D3DCOMPILE_PACK_MATRIX_ROW_MAJOR retrofit at VS + PS sites; bump REWRITE_VERSION 13->14 for shader-cache invalidation (Iter-1.5; CODEX Q2 mandate)`.
- **Awaiting Kenny smoke #1 (Task 2 checkpoint — covers BOTH Iter-1 + Iter-1.5):**
  - **Best outcome (~50% prior):** process launches under `rasterMajor=11`; shader cache hits a wave of cache-misses on first launch (REWRITE_VERSION 13→14 invalidated everything); shaders recompile clean under ROW_MAJOR (zero X-class compile errors); D3D11 debug-layer InfoQueue produces 2-3 `STATE_CREATION` info-severity messages from device creation routed via `DEBUG_REPORT_LOG_PRINT`; zero ERROR-severity messages (no cbuffer-write changes yet, no broken state); dark-blue MVP clear visible (Plan 11-07 milestone preserved); process responsive for ≥30 sec.
  - **Likely outcome (~25% prior):** same as best PLUS the `ShowWindow` assist is still needed (Iter-17 Surprise carries forward; Iter-16's Bloom config + Iter-14's shader cache state interaction may persist).
  - **Possible outcome (~15% prior):** shader recompile under ROW_MAJOR surfaces 1-2 warnings on the 12 `functions.inc` truncation X3206s that persisted through Plan 11-07 (these are non-fatal arithmetic warnings); should NOT block proceed-to-Task-2.5. Verdict GO if X-class errors are zero.
  - **Less likely outcome (~10% prior):** ROW_MAJOR flag surfaces an X-class compile error on some `.vsh` source we haven't seen before (Plan 11-07 covered `vertex_program/2d.vsh`; first launch under Plan 11-08 will compile the full asset set). Verdict REVIEW with the specific error text; iterate within Task 1.5 to handle the new case (likely another HlslRewriteUtil rule). Iter-18 BSOD must NOT recur because no cbuffer writes have changed yet.
  - **BSOD outcome (~0% prior):** zero hypotheses for a BSOD in Iter-1 + Iter-1.5 alone — no cbuffer writes have been modified, no draw-call dispatch sequence has changed. If a BSOD surfaces here, the root cause is something completely unexpected (driver crash, hardware fault, OS state corruption from a previous Iter-18 binary residue); revert both commits and re-investigate from a clean tree.

---

## Iteration 1.6: Tuning fix to Iter-1 — SetBreakOnSeverity(ERROR, FALSE) so ERROR-severity D3D11 messages log instead of killing the process

- **Date:** 2026-05-20
- **Symptom:** Kenny smoke #1 launched the Iter-1 + Iter-1.5 binary; engine progressed past shader-cache invalidation + ROW_MAJOR recompile (3 new `.cso` blobs written to `stage/stage/shader-cache/` at mtime 07:06 today, confirming REWRITE_VERSION=14 cache key took effect) into shader-template-iff loading; crashed during `shader/lightsaberblade_lava_a.sht` load with `Exception 0000087a(2170)=code 77454984=addr`. Cursor remained constrained (window class was created), no window was visible, no FATAL dialog surfaced. Crash dump at `stage/SwgClient_d.exe-unknown.0-20260520120641.{txt,mdmp}`.
- **Hypothesis:** Iter-1 configured `SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE)` per the Plan 11-08 Task 1 contract's stated "abort is preferable to silent NaN cascade." The D3D11 debug layer calls `DebugBreak()` synchronously when a message of the configured severity is ADDED to the queue (not when the queue is drained). With no debugger attached, `DebugBreak()` raises an unhandled exception → process terminates. The 0x0000087a exception code is in the kernel32/ntdll user-mode raise path consistent with this signature. `drainInfoQueue()` never got the chance to log the actual D3D11 message because the break fired before the next `present()` call. The "early warning BEFORE TDR/BSOD" intent of the safety net is satisfied by the LOG, not by the BREAK — ERROR-severity should route through `drainInfoQueue → DEBUG_REPORT_LOG_PRINT` without killing the process.
- **Investigation:**
  - Crash dump `.txt` contents (3 lines after the header): `GameFeatureBits: 1111` / `SubscriptionFeatureBits: 1` / `ShaderTemplate_Iff: shader/lightsaberblade_lava_a.sht` — the asset-load marker confirms engine progressed deep into asset loading before the abort. Plan 11-07's Iter-15 fixed BC-format staging-texture dim pad for `lightsaberblade` family textures, but that fix was for `lock()`-time E_INVALIDARG, not shader-template-iff loading.
  - Shader cache directory at `stage/stage/shader-cache/` (the relative-path double-`stage` artifact from Plan 11-05) contains 22 `.cso` blobs total, 3 of which carry today's mtime. The pre-Iter-1.5 hash space (REWRITE_VERSION=13) and the post-Iter-1.5 hash space (REWRITE_VERSION=14) co-exist; the 3 May 20 entries are the post-1.5 newly-compiled blobs — confirms shader cache invalidation worked AND ROW_MAJOR shader recompile didn't surface X-class errors (or the engine would have FATAL'd before reaching shader-template-iff load with a `D3DCompile ... failed:` message).
  - **No runtime log file written.** The engine's `DEBUG_REPORT_LOG_PRINT` calls during this launch went to the debugger output / console only; nothing under `stage/*.log` or `stage/log.txt` was updated. This is consistent with the engine's logging config + the abort-before-log-flush pattern. The InfoQueue drain in `present()` would have routed the D3D11 messages to `DEBUG_REPORT_LOG_PRINT` BUT the break happened before any `present()` call could fire.
  - Plan 11-07 ran cleanly through the same shader-template loading without the InfoQueue installed. The difference is solely the Iter-1 break-on-severity config.
- **Root cause:** Iter-1's `SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE)` is too aggressive. D3D11's debug layer fires ERROR-severity messages for many recoverable conditions (CreateXxx with bad arg → returns E_INVALIDARG instead of valid resource; Draw with invalid state → skips draw; etc.). Plan 11-07's plugin almost certainly hits one of these during `lightsaberblade_lava_a.sht` shader load (PEXE bytecode mismatch per Plan 11-05 caveat, or shader-implementation-data per-pass apply that hasn't been wired yet, or similar). With ERROR=TRUE the first such message kills the process before drainInfoQueue can route the message text to the log. The Plan 11-08 Task 1 contract anticipated ERROR-severity = "fatal" but the actual D3D11 ERROR-severity spectrum is wider than that.
- **Fix:** Direct3d11_Device.cpp single-line change:
  ```cpp
  // OLD (Iter-1):
  ms_infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
  // NEW (Iter-1.6):
  ms_infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, FALSE);
  ```
  CORRUPTION stays TRUE (genuinely unrecoverable; trap is appropriate). The "Direct3d11: ID3D11InfoQueue acquired..." log line text also updated to reflect the new policy. Iter-1's intent ("safety net for the next cbuffer-wiring BSOD") is satisfied by the per-frame log drain alone — when a future iteration causes a near-TDR bug, drainInfoQueue() emits the D3D11 messages to the log BEFORE the GPU hangs, and the user sees them in the log without losing the smoke process. The break-on-CORRUPTION still fires for true heap-corruption-class events; ERROR-severity now produces a logged line per message.
- **Verification (11-gate set):**
  - Gates 1-10: MSBuild D3D11/D3D9/_ffp/_vsps/SwgClient EXIT=0; D-13/D-05/D-04a invariants; FFP scan; STUB count unchanged; MSVC /W4 clean.
  - Gate 11 (ERROR-severity debug-layer message count during steady-state): FIRST MEASUREMENT pending Kenny smoke #1 retry. Expected: non-zero (the Iter-1.6 smoke is the first time we'll actually SEE what D3D11 is complaining about during `lightsaberblade_lava_a.sht` load).
  - Gates 12 + 13: ROW_MAJOR flag still present (Iter-1.5 carry-forward); slot capacity still N/A (Task 2.5 pending).
  - STUB count unchanged at 27.
- **Commits:** one commit `fix(11-08): SetBreakOnSeverity(ERROR, FALSE) so ERROR-severity D3D11 messages log instead of killing the process (Iter-1.6 tuning fix to Iter-1)`.
- **Awaiting Kenny smoke #1 retry (Iter-1 + Iter-1.5 + Iter-1.6 combined):**
  - **Best outcome (~40% prior):** process launches; dark-blue clear visible (Plan 11-07 milestone preserved); drainInfoQueue logs N non-zero ERROR-severity D3D11 messages including the one that fired during `lightsaberblade_lava_a.sht` load; process runs ≥30 sec. The actual ERROR message text becomes the design input for whether Task 2.5 onward can proceed OR a new diagnostic iteration is needed.
  - **Likely outcome (~35% prior):** same as best PLUS the ShowWindow assist is needed (Iter-17 carry-forward; Iter-16 Bloom config + shader-cache fast-path may still misfire ShowWindow). Kenny's existing PowerShell helper covers this.
  - **Possible outcome (~15% prior):** process crashes again at a DIFFERENT asset (not lightsaberblade) — would indicate that ERROR-severity messages are frequent enough that a different asset hits CORRUPTION-class severity. Less likely because corruption is rare; if it surfaces, the iteration would need to investigate the specific corruption message.
  - **Less likely outcome (~10% prior):** process aborts again at the same `lightsaberblade_lava_a.sht` site — would indicate the abort is NOT from SetBreakOnSeverity, and the real bug is in the asset-load path itself. If this happens, the log will have D3D11 message context just before the abort.

---

## Iteration 1.7: FILE* sink in drainInfoQueue → stage/d3d11-debug.log so InfoQueue messages persist beyond the OutputDebugString stream

- **Date:** 2026-05-20
- **Symptom:** Kenny smoke #1 retry under the Iter-1.6 binary CONFIRMED Plan 11-07 visible-dark-blue-clear milestone preserved (window pops via tools/d3d11-smoke/show-window.ps1; dark-blue clear visible; back/next button hover SFX audible; no crash). Window-enumeration confirmed engine's render window is class+title `SwgClient (swg-client-v2.0)` 640x480 Visible=False post-CreateWindowEx, same pattern as Iter-17. **Task 2 verdict = GO.** But: no D3D11 InfoQueue messages captured to any file. `Direct3d11_Device::drainInfoQueue` routes every message through `DEBUG_REPORT_LOG_PRINT` which `sharedDebug/src/shared/Report.cpp:127-149` writes to `stdout + OutputDebugString + stderr` only — invisible when `SwgClient_d.exe` is launched from explorer with no attached console. Task 3a/3b (the actual cbuffer matrix wiring) needs to see the D3D11 ERROR messages that fired during shader-template-iff load to know what's wrong; without persistent log capture we'd be debugging blind.
- **Hypothesis:** Add a parallel file sink directly in `Direct3d11_Device.cpp` — open `d3d11-debug.log` (CWD-relative; SwgClient_d.exe runs with CWD=stage/ so the file lands at `stage/d3d11-debug.log`) in append mode at create() time when ms_infoQueue is acquired; mirror every drained message to it; flush after each write so a crash mid-frame doesn't strand messages in stdio's internal buffer; close at destroy() before ComPtr release. Engine-side `DEBUG_REPORT_LOG_PRINT` continues in parallel for live DebugView observation. fopen failure is non-fatal — the engine route still runs.
- **Investigation:**
  - Verified `Report.cpp` output sinks via grep: line 127 `fputs(buffer, stdout)`, line 145 `OutputDebugString(buffer)`, line 147+149 `fputs(buffer, stderr)`. No FILE* sink in the engine's Report.cpp — file logging is not a built-in capability of the engine's DEBUG_REPORT subsystem.
  - Checked `D:\SWGEmu-Client\SWGEmu\logs\` (Kenny's actual client install) — contains only `SWGEmu.exe-stage.*` files from the OFFICIAL SWGEmu launcher, not our SwgClient_d.exe. No log file from our build.
  - Checked `D:\Code\swg-client-v2\stage\` — `ui.log` exists (UI subsystem load-time profile, written by sharedClientUi/ TreeView+font load tracking; mtime 08:25:24 reflects the just-killed PID 51424 session) but it's a UI-specific format, not a general engine log.
  - Conclusion: no existing log file destination captures `DEBUG_REPORT_LOG_PRINT` output. A parallel file sink in `drainInfoQueue` is the minimal-surface fix.
- **Root cause:** none — this is a structural diagnostic-infrastructure iteration. Iter-1's `drainInfoQueue` implementation correctly satisfied the Plan 11-08 Task 1 contract ("route via DEBUG_REPORT_LOG_PRINT"); the contract just didn't account for SwgClient_d.exe's launch-from-explorer profile having no captured stdout/stderr destination.
- **Fix:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_Device.cpp` extended:
  - `#include <ctime>` for time_t / localtime_s / strftime (session header timestamp formatting).
  - `FILE * ms_d3d11LogFile = nullptr;` added to `Direct3d11_DeviceNamespace` state.
  - In `create()` immediately after the InfoQueue is acquired + configured: `std::fopen("d3d11-debug.log", "ab")`; on success write a 5-line session header (`========...` separator + `=== D3D11 InfoQueue session begin` + ISO-8601 timestamp + feature-level + debug-flag state + separator) and `fflush`; on fopen failure log a fallback message via `DEBUG_REPORT_LOG_PRINT` and continue.
  - In `drainInfoQueue()` inside the per-message loop (after the existing `DEBUG_REPORT_LOG_PRINT` call): `fprintf` the same `D3D11 [SEV] category=N id=N: description` line to `ms_d3d11LogFile` + `fflush` after each message.
  - In `destroy()` immediately after the final drain + before `ms_infoQueue.Reset()`: write `=== D3D11 InfoQueue session end ===` + `fclose` + nullptr the FILE pointer (idempotent if create() never opened it).
- **Verification (11-gate set):**
  - Gates 1-10: MSBuild Direct3d11 EXIT=0 confirmed; gl11_d.dll auto-restages. D3D9/_ffp/_vsps/SwgClient builds untouched (no shared headers changed). D-13/D-04a/D-05 unchanged. MSVC /W4 clean (pre-existing C4459 DirectXMathVector carry-forward only).
  - Gate 11 (ERROR-severity message count during steady-state): now MEASURABLE. First measurement at Kenny's next smoke will land in `stage/d3d11-debug.log`.
  - Gates 12 + 13: unchanged from Iter-1.5 / pending Task 2.5.
  - STUB count unchanged at 27.
- **Commits:** one commit `feat(11-08): add file sink stage/d3d11-debug.log to drainInfoQueue so D3D11 messages persist across launches (Iter-1.7)`.
- **Awaiting Kenny smoke (next launch):** the next time SwgClient_d.exe launches under the Iter-1.7 binary, `stage/d3d11-debug.log` will be created (or appended to) with a session header + every D3D11 InfoQueue message that fires during the launch. Best-case outcome: log captures the actual ERROR-severity message that triggered the Iter-1 abort on `lightsaberblade_lava_a.sht` (now demoted to log-only by Iter-1.6) AND any WARNING/INFO messages from device creation + shader compile + first-draw state. Likely-case outcome: log is non-trivial (dozens to hundreds of messages) but mostly WARNING/INFO; the ERROR set is bounded and points at the specific cbuffer / state / asset issues Task 3a/3b needs to address. Less-likely-case outcome: fopen fails (CWD different from `stage/` per launch context, or filesystem permission) — falls back to engine-only logging with a clear log message indicating the fallback fired.

---

## Iteration 1.8: close Plan 11-07 Iter-15's second-half hole — pad CopySubresourceRegion srcBox to 4-aligned for BC formats

- **Date:** 2026-05-20
- **Symptom:** Iter-1.7's `stage/d3d11-debug.log` capture revealed the actual ERROR-severity message pattern firing during the smoke. Frequency rollup: 19,222 INFO Create/Destroy RTV per-frame + 19,220 OMSetRenderTargets unbind-from-FLIP_SEQUENTIAL informational + **96 ERROR-severity hits, all category=8 id=280**. First-occurrence text: `ID3D11DeviceContext::CopySubresourceRegion: Cannot invoke CopySubresourceRegion with a SrcBox that contains coordinates that are unaligned to a block or byte addressable boundary. *pSrcBox = { {left:0, top:0, front:0}, {right:1, bottom:1, back:1} }. BC2_UNORM requires alignment of coodinates to multiples of {4, 4, 1}.` Identical message text with `right:1,bottom:1` and `right:2,bottom:2` variants. ~5 minutes of smoke runtime (19,220 frames at ~60fps) with 96 BC2 errors firing during texture-asset load and not recurring per-frame (steady state has zero ERRORs).
- **Hypothesis:** This is the second half of the Plan 11-07 Iter-15 BC-format pad fix that wasn't closed. Iter-15 (commit `e4bdab17b`, `fix(11-04):` prefix) padded the staging-texture ALLOCATION dimensions to a 4-pixel block boundary so D3D11's `CreateTexture2D` no longer rejects sub-4 BC mips. But the matching `CopySubresourceRegion` srcBox passed by `Direct3d11_TextureData::unlock` was never padded — it still uses the LOGICAL `lockData.getWidth()` / `getHeight()` which are 1 or 2 at the bottom-of-chain BC mips of textures like `lightsaberblade_lava_a.tga`. D3D11 strict-rejects: `BC2_UNORM requires alignment of coordinates to multiples of {4, 4, 1}`. The staging texture itself was already padded to 4-aligned in `lock()` so its BC block grid covers the same data; padding `srcBox` here makes the source coordinates block-aligned end-to-end, mirroring Iter-15's allocation pad pattern.
- **Investigation:**
  - Read `Direct3d11_TextureData.cpp` lines 525-571 (lock site): confirmed Iter-15's allocation pad at line 540-544 applies `(N + 3u) & ~3u` to `stagingWidth` + `stagingHeight` when `isBlockCompressedFormat(native)` is true. Comment at line 533 explicitly states "unlock CopySubresourceRegion (with pSrcBox = nullptr)" — the author's INTENT was for unlock to pass nullptr, but the code at line 605-621 unconditionally passes `&srcBox` with the unpadded logical dims. The Iter-15 commit history shows the lock-site pad was added but no corresponding unlock-site edit.
  - Read `Direct3d11_TextureData.cpp` lines 595-621 (unlock site): srcBox dims are populated from `lockData.getWidth() / getHeight() / getDepth()` directly. No padding. This is the gap.
  - Looked at `isBlockCompressedFormat()` declared at line 99: covers both BC1..BC5 (70..84) and BC6H..BC7 (94..99) ranges per Iter-15's existing helper. Reusable here.
  - `m_texture` is declared as `ComPtr<ID3D11Resource>` at TextureData.h:90 — to inspect the destination format from unlock, query interface to `ID3D11Texture2D` via `ComPtr::As`. Volume textures hit a separate 3D code path and are not BC-typical; safe to short-circuit the pad to the 2D path.
- **Root cause:** missing srcBox dim pad in `Direct3d11_TextureData::unlock`. Iter-15's allocation pad was the first half of the fix; the unlock-site srcBox pad is the second half. Without the unlock pad, every sub-4 BC mip lock-unlock cycle fires id=280 from the D3D11 debug layer. The errors did not surface in Plan 11-07 smokes because Iter-15 closed the FATAL path (`E_INVALIDARG` from `CreateTexture2D`); the surviving validation messages went to OutputDebugString only. Iter-1.7's file-sink finally surfaced them.
- **Fix:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_TextureData.cpp` lines 605-621 (unlock site) extended:
  ```cpp
  // Existing srcBox population stays as-is...
  // NEW: after srcBox is populated, before CopySubresourceRegion:
  Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTex2d;
  if (SUCCEEDED(m_texture.As(&dstTex2d)) && dstTex2d)
  {
      D3D11_TEXTURE2D_DESC dstDesc = {};
      dstTex2d->GetDesc(&dstDesc);
      if (isBlockCompressedFormat(dstDesc.Format))
      {
          srcBox.right  = (srcBox.right  + 3u) & ~3u;
          srcBox.bottom = (srcBox.bottom + 3u) & ~3u;
          // srcBox.back stays as-is; BC block grid is {4,4,1}
      }
  }
  ```
  Volume-texture path (3D) is separate code and BC-typical for 3D is rare; left unchanged.
- **Verification (11-gate set):**
  - Gates 1-10: MSBuild Direct3d11 EXIT=0 confirmed; gl11_d.dll grew from 1,420,288 → 1,484,288 bytes (+64 KB; the QueryInterface pattern + extra braces compile into a small block). D-13/D-04a unchanged. D-05 unchanged. MSVC /W4 clean.
  - Gate 11 (ERROR-severity message count during steady-state): EXPECTED to drop from 96 → 0 for the BC2-pSrcBox class on next smoke. First measurement at Kenny's next smoke.
  - Gates 12 + 13: unchanged from Iter-1.5 / pending Task 2.5.
  - STUB count unchanged at 27.
- **Commits:** one commit `fix(11-04): pad CopySubresourceRegion srcBox to 4-aligned for BC formats (Plan 11-08 Iter-1.8; closes Iter-15 hole)`. The `fix(11-04):` prefix mirrors Iter-15 (`e4bdab17b`) and Iter-17 (`8d4dcc934`) — these are all Plan 11-04 resource-layer fixes surfaced during later-plan smokes.
- **Awaiting Kenny smoke (next launch):** expected outcome is `stage/d3d11-debug.log` carries ZERO `D3D11 [ERROR] category=8 id=280` lines this session. Other invariants unchanged (window still requires ShowWindow assist; per-frame RTV churn still firing — separate issue tracked but not addressed here; Plan 11-07 dark-blue-clear milestone still preserved). When the 96 BC2 errors are confirmed gone, log noise drops by ~5% allowing future Task 3a/3b iterations to spot new ERROR messages more easily.

---

## Iteration 2.5: Plan 11-05 retrofit — expand cbuffer slot 1024 → 1152 bytes + declare Direct3d11_VertexSlot0CB with per-field static_asserts (CODEX Q3a+Q3b)

- **Date:** 2026-05-20
- **Predicted symptom:** none — pre-implementation retrofit. Plan 11-05's `Direct3d11_ConstantBuffer::kMaxCBufferBytes = 1024` was an unverified "comfortable headroom" assumption that under-provisioned `c60..c67` (ExtendedLightData) by 64 bytes. Combined with `Map(WRITE_DISCARD)` returning UNDEFINED bytes (CODEX sixth hypothesis), Iter-18 BSOD's downstream cbuffer-wiring iterations would have left arbitrary garbage in the unwritten 1024..1088 region — exactly the symptom that took the OS down.
- **Hypothesis:** CODEX Q3a verdict: ExtendedLightData is 8 registers (2 × HemisphericLightData; HemisphericLightData = 4 float4 per HLSL struct-row-alignment rules); occupies c60..c67. Cbuffer total span = (67+1) × 16 = 1088 bytes. CODEX Q3b verdict: EXPAND THE SLOT (splitting / dropping both untenable). Plan 11-08 chooses 1152 bytes (72 registers) — 64 bytes of safety margin above the 1088 requirement, room for the trailing pad block in `Direct3d11_VertexSlot0CB` and any future ±1-register drift. Static_assert constellation enforces every packoffset boundary at compile time so any future drift (C++ side OR HLSL side) surfaces as a build error, never a runtime NaN cascade / BSOD.
- **Investigation:**
  - Read `Direct3d11_ConstantBuffer.h` lines 70-101 — confirmed Plan 11-05's `kMaxCBufferBytes = 1024` is a single point-of-truth used by `createOneSlot` for `D3D11_BUFFER_DESC::ByteWidth` (cpp:43), the install log line (cpp:73), and the bounds checks in `updateVS/updatePS` (cpp:93/94/114/115). No hardcoded 1024 elsewhere — bumping the constant propagates everywhere cleanly.
  - Confirmed `Direct3d11_PerFrameCB` / `Direct3d11_PerObjectCB` / `Direct3d11_PerMaterialCB` are still consumed by Plan 11-06 LightManager + other call sites — declarations must be PRESERVED (Plan 11-08's struct is ADDITIVE, not replacement).
  - Placed the new struct + static_asserts AFTER the `Direct3d11_ConstantBuffer` class closing brace so the `static_assert(sizeof(Direct3d11_VertexSlot0CB) <= Direct3d11_ConstantBuffer::kMaxCBufferBytes)` reference resolves cleanly.
- **Root cause:** none — this is a structural retrofit iteration. Plan 11-05's slot capacity decision was made before the HLSL register-usage audit landed; CODEX's Q3a analysis surfaced the gap during Plan 11-08 peer review.
- **Fix:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.h` extended:
  1. `kMaxCBufferBytes` constant bumped 1024 → 1152 with a 12-line comment block documenting CODEX Q3a/Q3b verdict + Iter-18 BSOD redux risk + 64-byte safety margin rationale.
  2. After class closing brace, added the `Direct3d11_VertexSlot0CB` struct with 8 fields covering c0..c71 (WVP / World / fog region / material / LightData / gap / ExtendedLightData / trailing pad).
  3. 10 static_asserts: sizeof == 1152, sizeof % 16 == 0, sizeof <= kMaxCBufferBytes, and 7 offsetof checks (c0 / c4 / c8 / c11 / c16 / c44 / c60 boundaries).
  4. `Direct3d11_ConstantBuffer.cpp` UNCHANGED — all uses of `kMaxCBufferBytes` flow through the single constant; no hardcoded 1024 anywhere in the cpp.
- **Verification (11-gate set):**
  - Gate 1: MSBuild Direct3d11 EXIT=0 confirmed — ALL 10 static_asserts passed at compile time. The struct layout matches the locked design.
  - Gates 2-5: MSBuild Direct3d9 / _ffp / _vsps / SwgClient — pending (will batch with Iter-1.8 + 2.5 build sweep). Not expected to regress; no shared headers touched.
  - Gates 6-10: D-13/D-04a/D-05 unchanged; MSVC /W4 clean (pre-existing C4459 DirectXMathVector carry-forward only).
  - Gate 11 (ERROR-severity message count): unchanged from Iter-1.8 baseline expectation (zero BC2 errors next smoke); cbuffer-write changes don't land until Task 3a/3b.
  - Gate 12 (ROW_MAJOR flag present): unchanged from Iter-1.5.
  - **Gate 13 (slot capacity ≥1088B): SATISFIED.** kMaxCBufferBytes = 1152, 64 bytes above the 1088 minimum. `sizeof(Direct3d11_VertexSlot0CB) == 1152` confirmed at compile time. 7 packoffset boundary offsetof checks pass at compile time.
  - STUB count unchanged at 27.
- **Commits:** one commit `feat(11-08): Plan-11-05 retrofit -- expand cbuffer slot 1024->1152 bytes; declare Direct3d11_VertexSlot0CB with per-field static_asserts (CODEX Q3a+Q3b)`.
- **Awaiting:** no smoke required — compile-only change. The static_assert constellation is the at-compile-time enforcement. Task 3a is the next iteration (initial-state guarantee + primeDefaults install-time landing). Task 3a's smoke checkpoint is where the new slot capacity gets exercised at runtime.

---

## Iteration 3a: primeDefaults install-time landing — every VS/PS cbuffer slot filled with identity/zero defaults BEFORE first draw

- **Date:** 2026-05-20
- **Predicted symptom:** none — pre-implementation safety-net iteration. The Iter-18 BSOD root-cause-set item #6 (CODEX sixth hypothesis): `Map(D3D11_MAP_WRITE_DISCARD)` does NOT zero unwritten bytes per Microsoft documentation; unwritten bytes contain ARBITRARY GARBAGE from prior frames. Without this prime, the first draw call's shader would read uninitialized cbuffer memory in slot 0's c8..c71 region + every byte in slots 1..3 + every byte in PS slots 0..3 — exactly the symptom that triggered the Iter-18 OS BSOD via NaN cascade -> degenerate rasterization -> GPU TDR escalation.
- **Hypothesis:** Add a new `Direct3d11_ConstantBuffer::primeDefaults()` static method called from `install()` after the `createOneSlot` loop. Strategy:
  - VS slot 0: full `Direct3d11_VertexSlot0CB` (just landed in Iter-2.5) with identity matrices for WVP + World; all other packoffset regions zeroed by `{}-init`. `lightData[0].x = 0` is implicit (numLights = 0 guard against shader-side `for (i=0; i<numLights; ++i)` bombs reading uninitialized counters).
  - VS slots 1, 2: full `kMaxCBufferBytes` (1152B) zero-fill. Belt-and-suspenders against any engine path that binds slots 1/2 between Iter-3a landing and Task 3b's setter rewrite that consolidates VS state into slot 0.
  - VS slot 3: full zero-fill (Plan 11-06's `Direct3d11_LightingCB` lives here at kLightingCBSlot = 3, but `LightManager::install()` is empty — see Investigation; Iter-3a primes slot 3 to close the same garbage-read gap rather than leaving LightingCB undefined until the first `setLights()` call).
  - PS slots 0..3: full zero-fill each. PS-side reads of zero produce defined-dark visuals (not NaN); covers Iter-18 must-have #3.
  - Smoke gate: process launches + dark-blue clear visible + debug-layer ERROR count = 0 + ≥30 sec without crash. NO behavioral regression expected vs Iter-1 baseline — Iter-3a only adds defensive initial-state writes, doesn't change any per-frame logic.
- **Investigation:**
  - Read `Direct3d11_ConstantBuffer.cpp` lines 36-74 (current install body): per-slot `createOneSlot` loop populates `D3D11_BUFFER_DESC::ByteWidth = kMaxCBufferBytes` via `CreateBuffer`, then logs. Buffer contents are UNDEFINED at this point per D3D11 docs (no pInitialData provided). Confirmed device + context are both alive at this call site (`NOT_NULL(Direct3d11_Device::getDevice())` already asserts the device; context is initialized in the same `Direct3d11_Device::create()` call BEFORE the per-subsystem install loop in Direct3d11.cpp runs).
  - Read `Direct3d11_ConstantBuffer.cpp` lines 89-127 (updateVS/updatePS implementations): use Map(WRITE_DISCARD) → memcpy → Unmap exactly per RESEARCH Pattern 2. The CODEX-sixth-hypothesis bug applies: any byte not covered by the memcpy is undefined. primeDefaults' FULL-FILL discipline (memcpy a kMaxCBufferBytes-sized payload via `updateVS(slot, &data, kMaxCBufferBytes)`) makes the entire slot defined.
  - Read `Direct3d11_LightManager.cpp` lines 42-48 — confirmed `install()` is intentionally empty (comment: "No process-wide state to initialize. setLights populates the cbuffer on demand."). Per the Plan 11-08 Task 3a contract: "if LightManager primes slot 3, skip" — but the LightManager's `install` does NOT prime. So the plan's deferral branch doesn't apply; primeDefaults takes ownership of priming slot 3 too. Recorded as a Rule-1 deviation from the plan; the original plan author assumed LightManager primed at install.
  - Confirmed `kLightingCBSlot = 3` in LightManager.cpp:36 (constexpr int).
  - Verified `Direct3d11_LightingCB` is declared in LightManager.h with `static_assert(sizeof == 320, ...)` so its layout is stable. primeDefaults uses zero-fill on slot 3 rather than a typed LightingCB struct — zero-fill covers the FULL 1152-byte slot (vs LightingCB's 320 bytes) which is the correct width for Map(WRITE_DISCARD) semantics.
- **Root cause:** none — structural safety-net iteration. The bug being prevented is the Iter-18 BSOD's hypothesis #6 (Map(WRITE_DISCARD) = arbitrary garbage in unwritten bytes); primeDefaults makes every slot defined before any draw can read it.
- **Fix:** Two file edits:
  1. **`Direct3d11_ConstantBuffer.h`** — added `static void primeDefaults();` declaration with a 10-line header comment explaining the FULL-FILL discipline rationale + CODEX sixth-hypothesis cross-reference.
  2. **`Direct3d11_ConstantBuffer.cpp`** — added `#include "Direct3d11_LightManager.h"` (for Direct3d11_LightingCB struct identity reference in the slot 3 comment; the actual write uses zero-fill not LightingCB struct), implemented `primeDefaults()` body with per-slot fills: stack-allocated `unsigned char zero[kMaxCBufferBytes]` reused across slots 1/2/3/PS-all; `Direct3d11_VertexSlot0CB slot0 = {}` + two `XMStoreFloat4x4(&slot0.*, XMMatrixIdentity())` for slot 0; ends with a `DEBUG_REPORT_LOG_PRINT` confirming the prime fired. Wired `primeDefaults()` call into `install()` immediately after the `createOneSlot` loop + before the existing install log line. Total: ~45 lines added.
- **Deviation from PLAN:** Rule-1 deviation on the slot 3 handling. Plan 11-08 Task 3a contract: "VS slot 3 is Direct3d11_LightManager's; DO NOT write here." This assumed `LightManager::install` primed slot 3. Verification via source read confirms `LightManager::install` is intentionally empty — slot 3 is unprimed until `setLights()` runs, which happens AFTER the first draw. Iter-3a takes ownership of slot 3 priming to close the same garbage-read gap covered for slots 0/1/2 and all PS slots. When `setLights()` fires later, its `updateVS(kLightingCBSlot, &cb, sizeof(cb))` cleanly overwrites the zero-fill with real LightingCB data. Zero impact on Plan 11-06 LightManager behavior.
- **Verification (11-gate set):**
  - Gate 1: MSBuild Direct3d11 EXIT=0 confirmed. gl11_d.dll auto-restages clean.
  - Gates 2-5: MSBuild Direct3d9 / _ffp / _vsps / SwgClient EXIT=0 — pending; no shared headers touched so no regression expected. Will batch-verify if needed.
  - Gate 6: D-13 grep — unchanged from Iter-1 (0 non-comment hits).
  - Gate 7: D-05 git diff — unchanged (no D3D9 source touched).
  - Gate 8: D-04a Glob — unchanged (no FfpGenerator).
  - Gate 9: FFP scan on Direct3d11_ConstantBuffer.cpp — 0 functional `#ifdef FFP / D3DTSS_ / D3DTOP_` (verified by inspection; primeDefaults uses only XMMath + memcpy).
  - Gate 10: MSVC /W4 — clean (pre-existing C4459 carry-forward only).
  - Gate 11 (ERROR-severity message count): pending Kenny smoke; expected = 0 (combined with Iter-1.8's BC2 fix, the 96 prior errors should drop to 0).
  - Gate 12 (ROW_MAJOR flag present): unchanged from Iter-1.5.
  - Gate 13 (slot capacity ≥1088B + Direct3d11_VertexSlot0CB defined): unchanged from Iter-2.5 (1152B + 10 static_asserts passing).
  - STUB count unchanged at 27.
- **Commits:** one commit `feat(11-08): primeDefaults install-time landing -- every VS/PS slot fills with identity/zero defaults before first draw (Iter-18 must-haves #2 + #3 + #5; Task 3a)`.
- **Awaiting Kenny smoke (Iter-3a checkpoint; combined effect of Iter-1.8 BC2 fix + Iter-2.5 slot expansion + Iter-3a primeDefaults):**
  - **Best outcome (~70% prior):** process launches; dark-blue MVP clear visible (Plan 11-07 milestone preserved); `stage/d3d11-debug.log` carries the new "primeDefaults: VS slot 0 = identity-matrix Direct3d11_VertexSlot0CB (1152B); ..." line near the top; ZERO ERROR-severity D3D11 messages this session (BC2 errors from Iter-1.8 expected to drop to 0; no new cbuffer-write errors because primeDefaults uses the same valid Map/Unmap pattern as updateVS); process responsive ≥30 sec; ShowWindow assist still needed (Iter-17 carry-forward); button hover SFX still audible. Iter-3a baseline matches Iter-1 baseline behaviorally — primeDefaults is purely defensive, doesn't change what the renderer draws.
  - **Possible outcome (~15% prior):** Iter-1.8 BC2 fix doesn't fully cover, e.g. additional ERROR id=280 messages fire on textures we didn't see in the previous smoke (different mip sizes, different BC variants). Verdict REVIEW; iterate within Iter-1.8 to widen the pad.
  - **Less likely outcome (~10% prior):** primeDefaults' install-time call exposes a context-readiness timing bug — e.g. `Direct3d11_Device::getContext()` returns null when ConstantBuffer::install runs. Surface via FATAL or HRESULT failure. Verdict REVIEW; defer primeDefaults call to end of `Direct3d11::install` in Direct3d11.cpp (alternate site documented in plan §Action step 4).
  - **Less-likely-still outcome (~5% prior):** something else regresses (debug-layer fires a new class of warning about cbuffer state, etc.). Verdict REVIEW with the new D3D11 message text.

---

## Iteration 3a.1: cache RTV on Direct3d11_TextureData — eliminate per-frame RTV churn from Direct3d11_RenderTarget::setRenderTarget

- **Date:** 2026-05-20
- **Symptom:** Iter-3a + Iter-1.8 smoke produced a clean 64,450-message d3d11-debug.log over ~5 minutes (20,975 frames at ~60fps); ZERO ERROR-severity messages. But the INFO-severity frequency rollup showed 20,975 `Create ID3D11RenderTargetView` (id=2097243) + 20,975 `Destroy ID3D11RenderTargetView` (id=2097245) + 20,974 `OMSetRenderTargets ... FLIP_SEQUENTIAL unbind` (id=49). That's roughly 1.2 RT-bind cycles per frame burning a fresh `CreateRenderTargetView` + matching `Destroy` every cycle. Wasteful but not crash-class; not a Plan 11-08 critical-path issue but worth closing before Task 3b's cbuffer-wiring work touches the same render-loop code.
- **Hypothesis:** `Direct3d11_RenderTarget::setRenderTarget` at lines 197-221 (the `texture->isRenderTarget()` branch) calls `ms_userRTV.Reset()` followed by `device->CreateRenderTargetView(texData->getTexture(), nullptr, &ms_userRTV)` on EVERY bind, then `setRenderTargetToPrimary` at line 150 calls `ms_userRTV.Reset()` again on every restore. Single-slot `ms_userRTV` cache cannot persist across different RT-texture bindings. The fix: cache the RTV on the `Direct3d11_TextureData` itself (mirrors the D3D9 plugin pattern where each render-target texture owns its `IDirect3DSurface9`). Lazy-create on first `setRenderTarget` call, reuse on subsequent calls; lifetime tied to the engine texture lifetime via the existing `Direct3d11_TextureData` destructor.
- **Investigation:**
  - Read `Direct3d11_RenderTarget.cpp` lines 136-221 — confirmed two RTV churn sites: (a) line 150 `ms_userRTV.Reset()` in `setRenderTargetToPrimary`, (b) lines 210-211 `Reset() + CreateRenderTargetView` in `setRenderTarget`'s `isRenderTarget()` branch.
  - Read `Direct3d11_RenderTarget.cpp` lines 192-195 (the `!isRenderTarget()` branch) — uses the persistent `ms_renderTargetView` (the baked 512x512 RTV created once in `install()` at line 104-105). This branch does NOT churn; only the isRenderTarget branch does.
  - Read `Direct3d11_Device.cpp` `beginScene()` lines 285-298 — uses cached `ms_backBufferRTV` from install. No per-frame creates here. Confirmed the churn source is NOT the back-buffer rebind path.
  - Read `Direct3d11_TextureData.h` lines 31-118 — confirmed there's no existing RTV slot; `m_srv` is the only paired view. Need to add `m_rtv` slot + a `getOrCreateRenderTargetView(ID3D11Device *device) const` lazy accessor. The `mutable` keyword keeps the accessor `const` per the engine's "logical const = no observable state change" model.
  - Verified that `setRenderTarget`'s `cubeFace` + `mipmapLevel` parameters are NOT used in the existing `CreateRenderTargetView` call (line 211 passes `nullptr` for the desc — whole-texture binding). The cache is therefore SINGLE-RTV-per-texture; cube/mip differentiation would require a (cubeFace, mip)-keyed map but is out of scope here (preserves existing engine behavior).
- **Root cause:** missing cache. The author of Plan 11-04's `Direct3d11_RenderTarget::setRenderTarget` followed the simplest correct pattern (Reset + CreateRenderTargetView every bind) which is functionally right but performance-wasteful. The fix is a textbook D3D11 cache-on-resource pattern.
- **Fix:** Three file edits:
  1. **`Direct3d11_TextureData.h`** — added `mutable Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;` private member + public `ID3D11RenderTargetView * getOrCreateRenderTargetView(ID3D11Device *device) const;` declaration with header comment.
  2. **`Direct3d11_TextureData.cpp`** — implemented `getOrCreateRenderTargetView` at the bottom of the file. Returns `nullptr` if `m_engineTexture.isRenderTarget()` is false; returns cached `m_rtv.Get()` if already created; otherwise calls `device->CreateRenderTargetView(m_texture.Get(), nullptr, m_rtv.GetAddressOf())` and returns the new pointer. `FAILED(hr)` path emits a `DEBUG_WARNING` and returns nullptr so callers can fall back gracefully (existing setRenderTarget call site asserts non-null via `FATAL(!cachedRTV, ...)` — the FATAL is intentional, an RT texture that cannot create an RTV is a malformed asset/format combination worth surfacing loudly).
  3. **`Direct3d11_RenderTarget.cpp`** lines 197-221 (the `isRenderTarget()` branch in `setRenderTarget`) — replaced `ms_userRTV.Reset() + CreateRenderTargetView(...)` with `ID3D11RenderTargetView * cachedRTV = texData->getOrCreateRenderTargetView(device); FATAL(!cachedRTV, ...); ms_userRTV = cachedRTV;`. `ms_userRTV` is still a ComPtr — the AddRef on assignment is cheap and `setRenderTargetToPrimary`'s `Reset()` (line 150) releases the per-binding ref without destroying the master cache on the TextureData. Removed the unused `D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {}` + `UNREF(rtvDesc)` debt that was preserved from Plan 11-04's stub.
- **Verification (11-gate set):**
  - Gate 1: MSBuild Direct3d11 EXIT=0 confirmed; gl11_d.dll restages clean. Touched 3 TUs (TextureData.h + .cpp + RenderTarget.cpp); incremental build only recompiles those.
  - Gates 2-5: MSBuild Direct3d9 / _ffp / _vsps / SwgClient — pending; no D3D9 source touched.
  - Gate 6: D-13 grep — unchanged from Iter-1 baseline (0 non-comment hits).
  - Gate 7: D-05 git diff — unchanged.
  - Gate 8: D-04a Glob — unchanged.
  - Gate 9: FFP scan on TextureData.h/cpp + RenderTarget.cpp — 0 functional `#ifdef FFP / D3DTSS_ / D3DTOP_`.
  - Gate 10: MSVC /W4 — clean (pre-existing C4459 carry-forward only).
  - Gate 11 (ERROR-severity message count): expected unchanged at 0 from Iter-3a; the cache change is functionally equivalent to the pre-fix behavior plus persistence.
  - Gate 12 (ROW_MAJOR): unchanged.
  - Gate 13 (slot capacity): unchanged.
  - STUB count unchanged at 27.
- **Commits:** one commit `fix(11-04): cache RTV on Direct3d11_TextureData -- eliminate per-frame RTV churn in Direct3d11_RenderTarget::setRenderTarget (Plan 11-08 Iter-3a.1)`. `fix(11-04):` prefix mirrors Iter-15, Iter-17, Iter-1.8 — Plan 11-04 resource-layer fixes surfaced during later-plan smokes.
- **Awaiting Kenny smoke (Iter-3a.1 verification):**
  - **Best outcome (~80% prior):** smoke produces a fresh `stage/d3d11-debug.log` showing dramatically reduced RTV traffic — ideally a HANDFUL of `Create ID3D11RenderTargetView id=2097243` lines total (one per distinct RT texture bound during the session) instead of ~20,975. `Destroy ID3D11RenderTargetView` count similarly drops to roughly match the number of distinct RT textures actually freed during the session (likely 0 if no RT texture was destroyed mid-session). Plan 11-07 milestone still preserved. Process responsive, no crash.
  - **Likely outcome (~15% prior):** small lingering RTV traffic if some engine code path destroys + recreates RT textures within a frame (less likely, but possible for transient FBO use). If observed, log the rate to compare against the pre-fix 70/sec baseline.
  - **Less likely outcome (~5% prior):** unexpected behavioral regression — e.g. an RT texture whose RTV was cached from a stale device/format. Surface as REVIEW with the new D3D11 message text.

---

## Future iterations (placeholders)

Filled as Task 3b+ land. Each entry follows the 6-field shape (Date / Predicted symptom or Symptom / Hypothesis / Investigation / Root cause / Fix / Verification + Commit hash + Awaiting block).

## Final state (filled at plan close)

- Visible geometry on top of the Plan 11-07 dark-blue MVP clear (char-select planet OR Tatooine outdoor OR cantina interior): PENDING
- No BSOD across any iteration: PENDING (Iter-1 + Iter-1.5 don't change any cbuffer write — first BSOD-risk iteration is Task 3b's matrix-shadow rewrite)
- D3D11 debug-layer ERROR-severity message count during steady-state: PENDING (first measurement at Task 2 checkpoint)
- D3D9 plugin builds clean (D-05 invariant): PENDING (per-iteration verification)
- Iteration log records every iteration with 6-field shape + Awaiting block: IN PROGRESS
