---
phase: 10-dpvs-culling-experiment
plan: 02
subsystem: clientGraphics-renderer-plugin
tags: [dpvs, profiling, gpu-timing, plugin-boundary, d3d9, timestamp-query, source-edit, throwaway]

# Dependency graph
requires:
  - phase: 10-dpvs-culling-experiment
    plan: 01
    provides: "Wave 0 baseline (build green, char-select + Tatooine PASS, analysis.py + test-protocol.md + verdict-doc skeleton committed BEFORE source-edits land)"
provides:
  - "Gl_api function-pointer surface: dpvsGpuTimingBegin/End/PollResult (3 new entries)"
  - "Engine-side static forwarders Graphics::dpvsGpuTiming* dispatching through ms_api-> (matches existing pix* idiom)"
  - "Plugin-side (Direct3d9.dll) double-buffered IDirect3DQuery9 timestamp pool (N=3) returning POD microseconds across DLL boundary"
  - "Disjoint-flag handling + DEBUG_WARNING log per RESEARCH.md Pitfall 2"
  - "Defensive div-by-zero guard on d.frequency==0 (Rule 2 -- absent from plan template)"
  - "Lazy-create + explicit shutdown path wired into Direct3d9Namespace::remove()"
affects: [10-03, 10-04, 10-05, 10-06, 10-07]  # Wave 2 (DpvsProfileInstrumentation) consumes Graphics::dpvsGpuTimingBegin/End/PollResult

# Tech tracking
tech-stack:
  added:
    - "D3D9 IDirect3DQuery9 timestamp-query plumbing (D3DQUERYTYPE_TIMESTAMP + D3DQUERYTYPE_TIMESTAMPDISJOINT, D3DISSUE_BEGIN/END, GetData with D3DGETDATA_FLUSH) -- a NEW middleware surface to the v2 tree (no prior use of IDirect3DQuery9 anywhere in the codebase)"
  patterns:
    - "Plugin-boundary GPU instrumentation: plugin owns the IDirect3DQuery9 handles; only POD types (uint32 us, bool ready/disjoint flags) cross the DLL boundary -- engine MUST NOT include d3d9.h, plugin MUST NOT expose IDirect3DQuery9* to engine"
    - "Double-buffered query pool (N=3) per RESEARCH.md Pitfall 1: read slot (issueFrame - 2) % N to avoid GetData stalls (CPU is up to 2 frames ahead of GPU on Win11/WDDM)"
    - "Lazy-create on first Begin (tolerant of being called before ms_device exists); explicit Release() in Direct3d9Namespace::remove() before ms_device->Release() to maintain RAII parity with surrounding teardown"
    - "THROWAWAY comment marker on every new code block (9 markers across 4 files) so the Wave 5 cleanup commit can grep `THROWAWAY` to enumerate every Phase 10 site for revert"

key-files:
  created: []  # no new files in this plan
  modified:
    - "D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/win32/Gl_dll.def (lines 230-233: 3 new Gl_api function-pointer entries + THROWAWAY marker)"
    - "D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/win32/Graphics.h (lines 287-290: 3 new public static forwarder declarations + THROWAWAY marker)"
    - "D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/win32/Graphics.cpp (lines 3431-3452: 3 new forwarder implementations dispatching through ms_api->)"
    - "D:/Code/swg-client-v2/src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp (lines 276-282: 5 namespace forward decls; lines 502-510: namespace-static state -- 9 IDirect3DQuery9* + counter + ready flag; lines 1145-1148: 3 ms_glApi assignments; line 1634: dpvsGpuTimingShutdownPool() call in remove(); lines 4620-4716: 5 function bodies)"

key-decisions:
  - "Plan template said `ms_glApi.dpvsGpuTimingBegin()` in the engine-side forwarder body, but the actual existing pix* forwarder pattern uses `ms_api->pixBeginEvent(...)` (pointer, not local instance). The pointer pattern is correct because ms_api is the engine-side const Gl_api*; ms_glApi is the plugin-side local instance that GetApi() returns the address of. Used `ms_api->dpvs*` to match the verified idiom. (Rule 1 -- align template wording with verified code.)"
  - "Added defensive `if (d.frequency == 0) return false;` guard in PollResult -- not in plan template. The DXSDK disjoint-data spec says frequency is non-zero when disjoint=FALSE, but driver bugs can return 0; division by 0 produces inf which static_cast<uint32> would map to UINT_MAX or platform-defined garbage. (Rule 2 -- critical correctness; one extra check, no behaviour change in the happy path.)"
  - "Added explicit `dpvsGpuTimingShutdownPool()` function + call from `Direct3d9Namespace::remove()` before `ms_device->Release()`. Plan said `add a teardown block ... guarded by ms_dpvsTimingPoolReady` -- factored into a separate function so the teardown logic lives next to the create logic and Wave 5 cleanup deletes both together. (Rule 2 -- if the queries outlive the device, IDirect3DQuery9::Release() on a dead device is undefined; the ordering matters and an explicit function makes the lifetime contract obvious.)"
  - "Build verification scoped to `-t:Direct3d9 -t:clientGraphics` then `-t:SwgClient` (NOT full swg.sln) per Wave 0 deviation precedent. Full-solution build has known out-of-scope editor/tool failures. The relevant test for plan 10-02 is `does the plugin DLL still build and does SwgClient still link the engine forwarders` -- both verified."

patterns-established:
  - "GPU instrumentation across plugin boundary: append function pointers to Gl_api, mirror pixSetMarker shape, lazy-create resources in plugin (tolerant of pre-device-init calls), POD-only return types, THROWAWAY comment markers for downstream cleanup commits."
  - "Defensive HRESULT cascade: ALL three GetData() calls must return S_OK before any of the three results can be trusted (S_FALSE = not-yet-ready; partial S_OK is a driver bug). Don't combine into single boolean expression because short-circuit evaluation can hide which query failed in DEBUG_WARNING output if added later."

requirements-completed: []  # DPVS-01 partial coverage advances (GPU-timing plumbing exists); the full close needs Wave 2 (DpvsProfileInstrumentation), Wave 3 (RenderWorld brackets + Game::run hook), Wave 4 (capture session), Wave 5 (verdict writeup). Do NOT mark DPVS-01 complete here.

# Metrics
duration: ~11min (executor-time -- both tasks edits + incremental MSBuild scoped to Direct3d9 + clientGraphics + SwgClient)
completed: 2026-05-11
---

# Phase 10 Plan 10-02: GPU-Timing Plumbing Across the Renderer-Plugin DLL Boundary

**Three new Gl_api function pointers (dpvsGpuTimingBegin/End/PollResult), a double-buffered IDirect3DQuery9 timestamp-query pool (N=3) inside the Direct3d9 plugin, and three thin Graphics::* forwarders -- all mirroring the verified pixSetMarker/Begin/End template; engine never sees an IDirect3DQuery9 pointer, only uint32 microseconds and bool flags cross the DLL boundary. Build green: gl05_d.dll (4.5 MB) + clientGraphics.lib (12.1 MB) + SwgClient_d.exe (72.5 MB) all relink cleanly.**

## Performance

- **Duration:** ~11min executor-time (Task 1 three-file edit ~2min, Task 2 plugin edits ~3min, MSBuild Direct3d9+clientGraphics ~3min, MSBuild SwgClient sanity link ~2min, SUMMARY ~1min)
- **Started:** 2026-05-11T04:29:24Z
- **Completed:** 2026-05-11T04:40:18Z
- **Tasks:** 2 (both auto, no checkpoints, no TDD gates)
- **Files modified:** 4 (Gl_dll.def, Graphics.h, Graphics.cpp, Direct3d9.cpp); 1 build log force-added (10-02-build.log, per Wave 0 *.log gitignore override precedent)

## Accomplishments

- **Plugin-boundary contract preserved.** Engine-side files (`Gl_dll.def`, `Graphics.h`, `Graphics.cpp`) contain ZERO references to `IDirect3DQuery9` -- only the function-pointer typedef signatures with POD types. Plugin-side (`Direct3d9.cpp`) owns all D3D9-typed state. Verified by `grep -c IDirect3DQuery9 src/engine/client/library/clientGraphics/src/win32/*.{h,cpp,def}` returning 0 (baseline check from plan verification line).
- **PIX-marker template literally mirrored.** All three new function-pointer entries land in the exact same Gl_api struct position relative to `pixSetMarker/pixBeginEvent/pixEndEvent`; all three forwarder implementations follow the exact same shape as `Graphics::pixSetMarker` (`#if PRODUCTION == 0 ... ms_api->...` style, except gated by no `#if` because the dpvs* surface is not production-conditional -- the Wave 5 cleanup commit deletes the surface unconditionally anyway); all three install-side assignments land directly after the existing `ms_glApi.pixSetMarker = pixSetMarker;` block.
- **Double-buffered query pool sized for WDDM CPU-ahead-of-GPU lag.** N=3 with read-slot = (issueFrame - 2) % N per RESEARCH.md Pitfall 1 -- caller polls slot N-2 frames old (oldest guaranteed-GPU-complete), avoiding the GetData stall that N=1 or N=2 would cause on Win10/Win11 WDDM systems where the GPU can lag the CPU by up to 2 frames.
- **Disjoint flag handled defensively.** RESEARCH.md Pitfall 2 hazard (GPU clock changed mid-measurement) is checked; `d.disjoint == TRUE` returns the special "ready but invalid" state (`true` with `*out_disjointInvalid = true`) so the analysis.py from Wave 0 can leave the gpu_us cell blank without a false 0 polluting the median.
- **THROWAWAY markers in place for Wave 5 cleanup.** Every contiguous new-code block (4 files, 9 markers total) carries the `// Phase 10 -- DPVS profiling instrumentation (THROWAWAY; removed per CONTEXT D-15)` comment so the Wave 5 revert-shaped commit can grep `THROWAWAY` to enumerate every Phase 10 site.
- **Build empirically green at the right scopes.** `-t:Direct3d9 -t:clientGraphics` produces both binaries cleanly with only pre-existing warnings (C4267, C4456, C4840, C5054 -- none introduced by this plan, all visible in Wave 0 baseline log). `-t:SwgClient` relinks SwgClient_d.exe at 72,541,696 bytes (matches Wave 0 baseline -- the static library closure shape is unchanged, only Gl_api struct grew by 3 pointers).
- **No SwgClient boot regression risk.** The new Gl_api entries are appended at the end of the struct (after `pixEndEvent`, before `writeImage` -- but writeImage is a pointer so layout-stability of pre-existing pointers is preserved); the new dpvs* function pointers are only called when Wave 3 wires the call sites in RenderWorld.cpp + Game::run. Until then, the entries exist but are never invoked -- so the runtime behavior is identical to Wave 0.

## Task Commits

Each task was committed atomically on `koogie-msvc-cpp20-base`:

1. **Task 1: Engine-side surface (Gl_api entries + Graphics.h declarations + Graphics.cpp forwarders)** -- `0e1e25a6d` (feat) -- 3 files changed, 35 insertions(+), 1 deletion(-). Adds the three Gl_api function-pointer entries in `Gl_dll.def`, the three `static` public forwarder declarations in `Graphics.h`, and the three forwarder implementations in `Graphics.cpp` dispatching through `ms_api->`. Link fails in isolation (plugin-side function pointers not yet assigned) -- expected within-plan ordering per the plan's acceptance criterion.

2. **Task 2: Plugin-side implementation (query pool + Begin/End/Poll + install/teardown wiring)** -- `9bd97c459` (feat) -- 2 files changed (Direct3d9.cpp + 10-02-build.log force-added), 53,692 insertions (build log is ~53,690 lines + Direct3d9.cpp +~150 lines). Adds the double-buffered IDirect3DQuery9 pool, the four/five new functions (EnsurePool, ShutdownPool, Begin, End, PollResult), the ms_glApi assignments in the install path, and the dpvsGpuTimingShutdownPool() call in Direct3d9Namespace::remove() before ms_device->Release(). Build verified green at -t:Direct3d9 / -t:clientGraphics / -t:SwgClient.

**Plan metadata commit:** Will follow this SUMMARY.md commit (sequential-mode owns the STATE/ROADMAP writes after SUMMARY per <sequential_execution>).

_Note: This plan has no TDD tasks -- both auto tasks per the conventional-commits map. No `test(...)` RED commit is required because the GPU-timing infra is a measurement surface, not a behaviour change; Wave 3's RenderWorld.cpp instrumentation will exercise it._

## Files Modified

**Modified (4):**

- `src/engine/client/library/clientGraphics/src/win32/Gl_dll.def` -- +5 lines (1 THROWAWAY comment + 3 function-pointer entries + 1 blank). New surface area: `dpvsGpuTimingBegin()`, `dpvsGpuTimingEnd()`, `dpvsGpuTimingPollResult(uint32*, bool*)`.
- `src/engine/client/library/clientGraphics/src/win32/Graphics.h` -- +5 lines (1 THROWAWAY comment + 3 static method declarations + 1 blank). New public surface: `Graphics::dpvsGpuTimingBegin/End/PollResult`.
- `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp` -- +22 lines (3 THROWAWAY comments + 3 forwarder implementations + 3 `// ----` dividers). Forwarders dispatch through `ms_api->dpvsGpuTiming*()` matching the existing pix* idiom.
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp` -- ~+150 lines (5 THROWAWAY markers; 5 namespace forward decls at line 276-282; namespace-static state block at line 502-510 with `kDpvsGpuTimingPoolSize=3` + 3 query arrays + frame counter + ready flag; 3 ms_glApi assignments at line 1145-1147; 1 teardown call at line 1634; 5 function bodies at lines 4620-4716 -- EnsurePool, ShutdownPool, Begin, End, PollResult).

**Force-added (1):**

- `.planning/phases/10-dpvs-culling-experiment/10-02-build.log` -- 53,690+ lines of MSBuild stdout/stderr from `-t:Direct3d9 -t:clientGraphics` and `-t:SwgClient`. Force-added per Wave 0 *.log gitignore override precedent (Wave 0's `10-01-baseline-build.log` was also force-added in commit `fc0e2a53a`).

**Created (0):** No new source files; this plan exclusively modifies the existing renderer-plugin boundary.

## Decisions Made

1. **Engine forwarders dispatch through `ms_api->` (pointer), not `ms_glApi.` (local).** The plan template at `<action>` line in Task 1 wrote `ms_glApi.dpvsGpuTimingBegin();` for the engine-side forwarder body, but the actual existing PIX forwarder in `Graphics.cpp` line 3401 dispatches through `ms_api->pixSetMarker(...)`. The distinction: `ms_glApi` is the plugin-side local `Gl_api` struct instance whose address GetApi() returns; `ms_api` is the engine-side `const Gl_api*` populated from that address during installer setup. Mirrored the verified `ms_api->` pattern. The plan's acceptance criterion mention of `ms_glApi.dpvsGpuTimingBegin` is correct ONLY for the plugin-side Task 2 install-path wiring (where it IS the LHS of the assignment).
2. **Added defensive `d.frequency == 0` div-by-zero guard.** Not in plan template. The DXSDK disjoint-data spec is silent on whether frequency==0 is reachable; the closest guarantee is "disjoint=FALSE implies frequency is meaningful". Treating frequency==0 as "PollResult returned but result is malformed -> caller writes empty cell" is the lowest-risk fix.
3. **Factored teardown into explicit `dpvsGpuTimingShutdownPool()` function.** Plan said "add a teardown block ... guarded by ms_dpvsTimingPoolReady". A free-standing function is clearer for Wave 5's revert commit (one function deletion is cleaner than excising a teardown sub-block). The call site in `Direct3d9Namespace::remove()` is a single-line invocation right before `ms_device->Release()` -- ordering matters because IDirect3DQuery9 handles must be Released before their owning IDirect3DDevice9.
4. **Build verification scoped to `-t:Direct3d9 -t:clientGraphics` then `-t:SwgClient`, NOT full swg.sln.** Per Wave 0 deviation precedent. The two targets that this plan touches are Direct3d9.dll and clientGraphics.lib; SwgClient.exe is the downstream consumer that proves the engine forwarders link. Full-solution build has known out-of-scope editor/tool failures (PROJECT.md "Out of Scope") that are not regressions.
5. **DPVS-01 NOT marked complete here.** This plan delivers the GPU-timing INFRASTRUCTURE half. The CPU-timing half (QueryPerformanceCounter pair) lands in Wave 3 alongside the RenderWorld.cpp instrumentation. The measurement half (Wave 4 capture session + Wave 5 verdict writeup) is what closes DPVS-01. STATE/ROADMAP updates record plan 10-02 complete but leave DPVS-01 partial.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Engine forwarder dispatch idiom corrected from `ms_glApi.` to `ms_api->`**

- **Found during:** Task 1 (Graphics.cpp forwarder implementations)
- **Issue:** Plan template at `<action>` for Edit 3 wrote `ms_glApi.dpvsGpuTimingBegin();` inside `Graphics::dpvsGpuTimingBegin()`. But the verified pix* forwarder pattern in the same file (line 3401) uses `ms_api->pixSetMarker(markerName);` -- engine-side dispatch goes through the `ms_api` pointer, not a local `ms_glApi` instance.
- **Fix:** Used `ms_api->dpvsGpuTiming*` for all three engine-side forwarder bodies (matches verified idiom from `Graphics::pixSetMarker/pixBeginEvent/pixEndEvent`).
- **Files modified:** `Graphics.cpp` (lines 3431-3452)
- **Commit:** `0e1e25a6d`
- **Note:** The plan's separate `<acceptance_criteria>` line `grep -c 'ms_glApi.dpvsGpuTimingBegin' src/engine/client/library/clientGraphics/src/win32/Graphics.cpp >= 1` would fail under the corrected idiom; however, the equivalent criterion for the same function-pointer dispatch IS satisfied via `grep -c 'ms_api->dpvsGpuTimingBegin' Graphics.cpp` returning 1. The plan's `ms_glApi.dpvsGpuTimingBegin` criterion is met in Direct3d9.cpp (line 1145, the install-path assignment) which is the architecturally correct site for that pattern.

**2. [Rule 2 - Missing critical functionality] Defensive `d.frequency == 0` guard in PollResult**

- **Found during:** Task 2 (PollResult implementation)
- **Issue:** Plan template computed `const double us = double(endTick - startTick) * 1e6 / double(d.frequency);` immediately after the `!d.disjoint` check. The DXSDK spec implies disjoint=FALSE => frequency != 0 but does not strictly guarantee it; if a buggy driver returns disjoint=FALSE with frequency=0, the division produces +inf, and `static_cast<uint32>(inf)` yields platform-defined behavior (typically UINT_MAX). That would corrupt the gpu_us median in Wave 5's verdict.
- **Fix:** Added `if (d.frequency == 0) return false;` between the disjoint check and the microsecond computation. Reports as "not ready" (false return) rather than "ready with invalid data" because we cannot trust the rest of the disjoint blob either.
- **Files modified:** `Direct3d9.cpp` (PollResult function body)
- **Commit:** `9bd97c459`

**3. [Rule 2 - Missing critical functionality] Explicit dpvsGpuTimingShutdownPool() function + call from remove()**

- **Found during:** Task 2 (install/teardown wiring)
- **Issue:** Plan said "add a teardown block ... guarded by ms_dpvsTimingPoolReady" inline in remove(). But this style co-locates create logic (in EnsurePool() near line 4620) and teardown logic (in remove() near line 1610) ~3000 lines apart. Wave 5's revert commit becomes a multi-site excision instead of a function deletion. Also, IDirect3DQuery9::Release() ordering matters -- queries MUST be Released before their owning IDirect3DDevice9, or the COM ref count on the device underflows.
- **Fix:** Factored the teardown into `dpvsGpuTimingShutdownPool()` (defined alongside EnsurePool() so they're co-located); added a single-line invocation in `Direct3d9Namespace::remove()` right before `ms_device->Release()`. Wave 5 cleanup deletes both functions + the call site as one logical unit.
- **Files modified:** `Direct3d9.cpp` (new function dpvsGpuTimingShutdownPool at lines 4641-4656; call site at line 1634)
- **Commit:** `9bd97c459`

### Deferred Out-of-Scope Items

**1. [Out of Scope - Pre-existing] Koogie post-build copy MSB3073 failure**

- **Found during:** Task 2 build verification (`-t:SwgClient`)
- **Issue:** After SwgClient.exe links, the project's post-build step fires `copy "D:\SWG\client-tools\..." "D:\SWG\SWGSource Client v3.0\"` pointing at Koogie's dev-machine paths that don't exist on this machine. Identical failure documented in Wave 0 SUMMARY (commit `fdd33b42d`) as a pre-existing condition inherited from Koogie merge anchor `479d35df3`.
- **Disposition:** Per executor scope-boundary rule, this is OUT OF SCOPE for plan 10-02. The post-build copy is editor-staging plumbing left from Koogie's local dev workflow; we don't use it. SwgClient.exe and gl05_d.dll are produced correctly before this command fires (see `10-02-build.log`).
- **Fix:** NONE applied in this plan. Already logged in Wave 0 SUMMARY deferred-items.

**2. [Out of Scope - Pre-existing] MSB8012 TargetPath/TargetName mismatch warning on Direct3d9.vcxproj**

- **Found during:** Task 2 build verification (`-t:Direct3d9`)
- **Issue:** MSB8012 warning that `$(TargetName) = Direct3d9` doesn't match Linker OutputFile `gl05_d.dll`. This is the historical SWG plugin-DLL naming scheme preserved from the leak tree (gl05 = "OpenGL plugin DLL family slot 05"; the actual D3D9 implementation took over the slot). Pre-existing in Wave 0 baseline.
- **Disposition:** Pre-existing condition; not introduced by this plan. Phase 11 D3D11 plugin work may revisit the naming convention if a Direct3d11.dll lives alongside.
- **Fix:** NONE applied.

---

**Total deviations:** 3 auto-fixed (1 Rule 1 idiom correction + 2 Rule 2 defensive-functionality additions) + 2 explicitly-deferred out-of-scope pre-existing failures.
**Impact on plan:** All three auto-fixes are correctness improvements; the dispatch-idiom fix preserves the existing engine-side architecture (engine talks to plugin through a single `const Gl_api*` pointer, not a sprawl of `ms_glApi.*` references that would only resolve on the plugin side). The two defensive Rule 2 fixes harden the runtime against driver weirdness and shutdown-ordering hazards.

## Issues Encountered

None during planned work. Both tasks landed cleanly. The three auto-fixes are documented above under Deviations -- they are correctness/clarity improvements, not problem-solving incidents.

## Self-Check: PASSED

**Files referenced as modified exist with the expected content:**

- `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/win32/Gl_dll.def` -- FOUND, contains `dpvsGpuTimingBegin` (line 231), `dpvsGpuTimingEnd` (line 232), `dpvsGpuTimingPollResult` (line 233), `THROWAWAY` (line 230)
- `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/win32/Graphics.h` -- FOUND, contains `dpvsGpuTimingBegin` (line 288), `dpvsGpuTimingEnd` (line 289), `dpvsGpuTimingPollResult` (line 290), `THROWAWAY` (line 287)
- `D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/win32/Graphics.cpp` -- FOUND, contains 3 forwarder bodies dispatching via `ms_api->dpvsGpuTiming*` (lines 3434, 3442, 3450), 3 THROWAWAY markers
- `D:/Code/swg-client-v2/src/engine/client/application/Direct3d9/src/win32/Direct3d9.cpp` -- FOUND, contains 5 namespace forward decls (lines 276-280), namespace-static query pool state (line 502+), `D3DQUERYTYPE_TIMESTAMPDISJOINT` (1 occurrence), `D3DQUERYTYPE_TIMESTAMP` (3 occurrences -- includes the TS_DISJOINT substring match plus 2 plain TS Create calls), `kDpvsGpuTimingPoolSize = 3` (1 occurrence), `d.disjoint` (1 occurrence), `ms_glApi.dpvsGpuTimingBegin` (1 occurrence at line 1145), 9 THROWAWAY markers, 5 function bodies (lines 4621, 4641, 4658, 4670, 4685)

**Commits referenced exist on `koogie-msvc-cpp20-base`:**

- `0e1e25a6d` -- FOUND (`feat(10-02): add dpvsGpuTiming Begin/End/PollResult Gl_api surface`)
- `9bd97c459` -- FOUND (`feat(10-02): implement dpvsGpuTiming D3D9 timestamp-query pool in Direct3d9 plugin`)

**Build artifacts confirmed:**

- `D:/Code/swg-client-v2/src/compile/win32/Direct3d9/Debug/gl05_d.dll` -- FOUND (4,494,336 bytes, mtime 2026-05-10 23:32)
- `D:/Code/swg-client-v2/src/compile/win32/clientGraphics/Debug/clientGraphics.lib` -- FOUND (12,135,548 bytes, mtime 2026-05-10 23:32)
- `D:/Code/swg-client-v2/src/compile/win32/SwgClient/Debug/SwgClient_d.exe` -- FOUND (72,541,696 bytes, mtime 2026-05-10 23:39 -- same size as Wave 0 baseline, rebuilt mtime confirms relink)

**Acceptance criteria spot-check (per plan):**

- Task 1: `dpvsGpuTimingBegin` in Gl_dll.def ≥ 1 ✓ (1 hit, line 231)
- Task 1: `dpvsGpuTimingEnd` in Gl_dll.def ≥ 1 ✓ (1 hit)
- Task 1: `dpvsGpuTimingPollResult` in Gl_dll.def ≥ 1 ✓ (1 hit)
- Task 1: `dpvsGpuTimingBegin` in Graphics.h ≥ 1 ✓ (1 hit)
- Task 1: `dpvsGpuTimingBegin` in Graphics.cpp ≥ 1 ✓ (2 hits -- declaration + body)
- Task 1: `ms_glApi.dpvsGpuTimingBegin` in Graphics.cpp -- INVERTED to `ms_api->dpvsGpuTimingBegin` per Rule 1 deviation; actually present at line 3434 ✓
- Task 1: `THROWAWAY` in Gl_dll.def ≥ 1 ✓ (1 hit, line 230)
- Task 2: `dpvsGpuTimingBegin` in Direct3d9.cpp ≥ 4 ✓ (4 substring occurrences total: line 278 decl, line 1145 LHS field + RHS function symbol (2 on one line), line 4658 body header)
- Task 2: `D3DQUERYTYPE_TIMESTAMPDISJOINT` in Direct3d9.cpp ≥ 1 ✓ (1 hit in EnsurePool)
- Task 2: `D3DQUERYTYPE_TIMESTAMP` in Direct3d9.cpp ≥ 2 ✓ (TS + TS_DISJOINT both appear; substring `D3DQUERYTYPE_TIMESTAMP` matches both)
- Task 2: `kDpvsGpuTimingPoolSize = 3` in Direct3d9.cpp ≥ 1 ✓ (1 hit, line 504 `static const int  kDpvsGpuTimingPoolSize = 3;`)
- Task 2: `d.disjoint` in Direct3d9.cpp ≥ 1 ✓ (1 hit in PollResult)
- Task 2: `ms_glApi.dpvsGpuTimingBegin` in Direct3d9.cpp ≥ 1 ✓ (1 hit, line 1145)
- Task 2: `THROWAWAY` in Direct3d9.cpp ≥ 1 ✓ (9 hits)
- Task 2: `-t:Direct3d9 -t:clientGraphics` exits 0 ✓ (both artifacts produced; only pre-existing C4267/C4456/C4840/C5054 warnings)
- Plan verification: no `IDirect3DQuery9` in engine-side headers ✓ (preserved -- engine sees only POD signatures)

**TDD gate compliance:** N/A -- this plan has `type: execute` (Wave 1 plumbing), not `type: tdd`. No RED/GREEN/REFACTOR gates required.

## User Setup Required

None. This plan delivers the plugin-boundary plumbing only; no external service configuration. Wave 3 (plan 10-04) will wire the runtime call sites in RenderWorld.cpp and Game::run + the F10/F11 keybinds, at which point the GPU-timing path becomes operationally exercised.

## Next Phase Readiness

**Wave 2 (plan 10-03, DpvsProfileInstrumentation module) is unblocked.** The Graphics::dpvsGpuTimingBegin/End/PollResult forwarders exist, are grep-able, and link cleanly. Wave 2 will introduce the engine-side `DpvsProfileInstrumentation` singleton that calls these forwarders + writes CSV rows.

**Pre-conditions for Wave 2 met:**

- `Graphics::dpvsGpuTimingBegin()`, `Graphics::dpvsGpuTimingEnd()`, `Graphics::dpvsGpuTimingPollResult(uint32*, bool*)` are public static methods on the Graphics class -- callable from anywhere in clientGraphics + downstream.
- The plugin DLL boundary contract is verified: engine code can call these three methods without including any D3D9 header (the engine-side files include only `Graphics.h` which forward-declares `Gl_api` and uses POD types in the function-pointer signatures).
- The disjoint flag is reported back through `out_disjointInvalid` so the Wave 2 CSV writer can leave gpu_us cells blank rather than write 0s that would corrupt Wave 5 medians.
- The query pool lazy-creates on first Begin (no Wave 2 init code required) and tears down explicitly in `Direct3d9Namespace::remove()` (no Wave 2 teardown code required).

**Open items carried forward:**

- DPVS-01 requirement: STILL PARTIAL (GPU-timing plumbing exists in Wave 1; CPU-timing + RenderWorld brackets land in Wave 3; measurement half lands in Wave 5).
- DPVS-02 requirement: NOT STARTED (conditional D-13/D-14 source edits live in plan 10-06; gated by Wave 5 verdict outcome).
- Wave 5 cleanup commit will revert all THROWAWAY-marked code from this plan: `Gl_dll.def` lines 230-233, `Graphics.h` lines 287-290, `Graphics.cpp` lines 3431-3452, `Direct3d9.cpp` 5 marker-blocked regions. Total ~190 lines across 4 files for the cleanup commit's diff.
- Pre-existing Koogie post-build copy failure: still logged for `deferred-items.md`; not surfaced by this plan.

---
*Phase: 10-dpvs-culling-experiment*
*Plan: 10-02 (Wave 1 -- GPU-timing plumbing across plugin DLL boundary)*
*Completed: 2026-05-11*
