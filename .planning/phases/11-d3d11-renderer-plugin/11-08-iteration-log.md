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

## Future iterations (placeholders)

Filled as Task 2 smoke result returns + subsequent tasks land. Each entry follows the 6-field shape (Date / Predicted symptom or Symptom / Hypothesis / Investigation / Root cause / Fix / Verification + Commit hash + Awaiting block).

## Final state (filled at plan close)

- Visible geometry on top of the Plan 11-07 dark-blue MVP clear (char-select planet OR Tatooine outdoor OR cantina interior): PENDING
- No BSOD across any iteration: PENDING (Iter-1 + Iter-1.5 don't change any cbuffer write — first BSOD-risk iteration is Task 3b's matrix-shadow rewrite)
- D3D11 debug-layer ERROR-severity message count during steady-state: PENDING (first measurement at Task 2 checkpoint)
- D3D9 plugin builds clean (D-05 invariant): PENDING (per-iteration verification)
- Iteration log records every iteration with 6-field shape + Awaiting block: IN PROGRESS
