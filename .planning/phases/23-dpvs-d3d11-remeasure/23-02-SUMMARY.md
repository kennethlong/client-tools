---
phase: 23-dpvs-d3d11-remeasure
plan: 02
subsystem: infra
tags: [dpvs, occlusion-culling, instrumentation, csv, profiling, d3d11, keybinds, console-command, harness-wiring]

# Dependency graph
requires:
  - phase: 23-dpvs-d3d11-remeasure
    plan: 01
    provides: "DpvsProfileInstrumentation.{h,cpp} (install/onFrameEnd/toggleCapture/setRunLabel) + RenderWorld::toggleForceDisableOcclusionCulling() accessor (the F11 target)"
provides:
  - "DpvsProfileInstrumentation::install() wired at startup (SetupClientGraphics, after RenderWorld::install) -- CSV writer + overlay DebugFlag + ExitChain teardown now register"
  - "Per-frame DpvsProfileInstrumentation::onFrameEnd(elapsedTime*1000) driver in Game::runGameLoopOnce after the Graphics::present() block -- one CSV row per frame while capture is active"
  - "F10 (toggleCapture) + F11 (toggleForceDisableOcclusionCulling, rewired) _DEBUG keybind intercept in CuiIoWin IOET_KeyDown, before InputMap"
  - "/setrunlabel console command in SwgCuiCommandParserDefault (MAKE_COMMAND + cmds[] + performParsing branch) -> setRunLabel sanitizer"
  - "Validated analysis.py schema contract: the writer's 10-column header is accepted with no header-mismatch, -on/-off pooling works, verdict line emitted"
affects: [23-03, dpvs-capture]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Restore-from-recovery-tag harness wiring with one surgical adaptation (F11 re-pointed at the surviving toggleForceDisableOcclusionCulling accessor)"
    - "_DEBUG-gated keybind intercept (F10/F11) so the shipped Release build has no capture/occlusion-toggle surface; /setrunlabel stays on, guarded by its sanitizer"
    - "Multi-library rebuild discipline: a single top-level SwgClient.vcxproj build does NOT recompile changed .cpp in sibling library projects -- each changed lib must be built explicitly before the relink picks up the new objs"

key-files:
  created: []
  modified:
    - src/engine/client/library/clientGraphics/src/win32/SetupClientGraphics.cpp
    - src/engine/client/library/clientGame/src/shared/core/Game.cpp
    - src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp
    - src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp

key-decisions:
  - "F11 rewired (the only non-verbatim restore): the pre-cleanup F11 called the deleted RenderWorld::setDisableOcclusionCulling(...); re-pointed at Plan 01's RenderWorld::toggleForceDisableOcclusionCulling() which flips the surviving ms_forceDisableOcclusionCulling DebugFlag"
  - "F10/F11 block kept entirely inside #ifdef _DEBUG (Elevation mitigation T-23-02); /setrunlabel + its sanitizer stay always-on (the sanitizer is the control, T-23-01)"
  - "install() added without a matching ::remove() -- install() registers its own ExitChain::add teardown"
  - "onFrameEnd fed elapsedTime*1000 (the previous frame's Clock::frameTime() seconds -> ms for total_frame_ms); renderer-agnostic, fires identically under gl11"
  - "Schema contract validated against a hand-written fixture (one -on + one -off row using the writer's exact header) in a throwaway temp dir, NOT in stage/dpvs-profile/ -- the real capture dir already holds Phase 10 measurement CSVs that must not be touched"

requirements-completed: [DPVS-01]

# Metrics
duration: 50min
completed: 2026-06-12
---

# Phase 23 Plan 02: DPVS Harness Wiring Restore + Activation Summary

**Restored the Phase 10 DPVS harness wiring from the recovery tag and connected it to the Plan 01 instrumentation core -- the install() hook (SetupClientGraphics), the per-frame onFrameEnd driver (Game.cpp), the F10 capture + F11 occlusion-toggle _DEBUG keybinds (CuiIoWin, F11 rewired to the surviving toggleForceDisableOcclusionCulling accessor), and the /setrunlabel console command (SwgCuiCommandParserDefault) -- so a Debug client can now capture CSVs and visibly toggle DPVS state.**

## Performance

- **Duration:** ~50 min
- **Completed:** 2026-06-12
- **Tasks:** 3
- **Files modified:** 4 (0 created, 4 modified)

## Accomplishments
- **Install hook (SetupClientGraphics.cpp):** added `#include "clientGraphics/DpvsProfileInstrumentation.h"` and `DpvsProfileInstrumentation::install();` immediately after `RenderWorld::install();`. No matching `::remove()` -- `install()` registers its own `ExitChain::add` teardown. Verbatim restore from the tag (comment updated to reference the new `getForceDisableOcclusionCulling()` source).
- **Per-frame driver (Game.cpp):** added the header include and `DpvsProfileInstrumentation::onFrameEnd(elapsedTime * 1000.0f);` immediately after the `Graphics::present()` block in `runGameLoopOnce`. `elapsedTime` (the loop-top `Clock::frameTime()` local) is the previous frame's duration in seconds, converted to ms for the `total_frame_ms` column. Renderer-agnostic.
- **F10/F11 intercept (CuiIoWin.cpp):** restored the `#ifdef _DEBUG` block inside `IOET_KeyDown`, before InputMap. F10 calls `DpvsProfileInstrumentation::toggleCapture()` (verbatim). **F11 rewired** -- instead of the deleted `RenderWorld::setDisableOcclusionCulling(...)`, it calls `RenderWorld::toggleForceDisableOcclusionCulling()` (the Plan 01 accessor on the surviving `ms_forceDisableOcclusionCulling` flag). Restored the two `_DEBUG` includes (`DpvsProfileInstrumentation.h` + `RenderWorld.h`).
- **/setrunlabel command (SwgCuiCommandParserDefault.cpp):** restored the three coordinated edits verbatim -- `MAKE_COMMAND (setrunlabel);`, the `cmds[]` table entry, and the `performParsing` else-if branch that joins `argv[1..]` with `_` (via `Unicode::wideToNarrow`) and calls `DpvsProfileInstrumentation::setRunLabel(label)` (the sanitizer runs inside `setRunLabel`). Re-added the header include. Command + sanitizer are always-on (not `_DEBUG`-gated).
- **Build:** both Debug and Release link clean -- 0 `unresolved external symbol`, 0 LNK2001/LNK2019, 0 `error C`/`error LNK` (verified despite `/FORCE`). All four changed `.cpp` recompiled (see the multi-lib rebuild deviation).
- **Boot:** Debug `SwgClient_d.exe` (rasterMajor=11) booted through full D3D11 device/swapchain creation into a steady present loop with no FATAL/crash for 30s. Release link gate passed.
- **Schema contract:** `analysis.py` accepts the writer's exact 10-column header with NO header-mismatch, pools `-on`/`-off` conditions, and emits `verdict = keep` on a hand-written fixture.

## Task Commits

1. **Task 1: Restore install hook (SetupClientGraphics) + per-frame onFrameEnd (Game.cpp)** - `33fe2eff3` (feat)
2. **Task 2: Restore F10/F11 intercept (CuiIoWin, F11 rewired) + /setrunlabel command** - `f9ac31d81` (feat)
3. **Task 3: Rebuild, boot-gate, smoke the toggle + analysis.py schema** - no source/cfg edits of its own (the cfg `rasterMajor=11` + `[ClientGraphics/Dpvs] reportInstrumentation=true` already lingered from Phase 10); surfaced the multi-lib rebuild deviation, ran the build + boot + schema verification.

**Plan metadata:** final docs commit (this SUMMARY + STATE + ROADMAP + REQUIREMENTS).

## Files Modified
- `clientGraphics/src/win32/SetupClientGraphics.cpp` - `DpvsProfileInstrumentation.h` include + `install()` call after `RenderWorld::install()`.
- `clientGame/src/shared/core/Game.cpp` - header include + `onFrameEnd(elapsedTime*1000)` after the present block in `runGameLoopOnce`.
- `clientUserInterface/src/shared/core/CuiIoWin.cpp` - `_DEBUG` F10/F11 intercept in `IOET_KeyDown` (F11 rewired) + the two `_DEBUG` includes.
- `swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp` - `/setrunlabel` 3-edit restore + header include.

## Decisions Made
- **F11 rewire** is the single non-verbatim restore (Shared Pattern A): the deleted `get/set` occlusion accessor pair is replaced by Plan 01's `toggleForceDisableOcclusionCulling()`. The acceptance assertion `setDisableOcclusionCulling` == 0 hits in CuiIoWin passes (an initial explanatory comment containing the literal symbol string was reworded so the source-assertion grep stays clean).
- **`_DEBUG` gating** kept on the F10/F11 block (T-23-02 Elevation mitigation); `/setrunlabel` + sanitizer always-on (T-23-01 Tampering mitigation -- the sanitizer is the control).
- **Schema smoke isolation:** validated `analysis.py` against a throwaway fixture in a temp dir rather than `stage/dpvs-profile/`, because that capture dir already contains 23 Phase 10 measurement CSVs that must be preserved for reference. No smoke CSV leaked into the real dir (before/after diff confirmed empty).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Single-project SwgClient build left 3 of 4 changed libraries' objects stale**
- **Found during:** Task 3 (Debug build verification)
- **Issue:** Building only `SwgClient.vcxproj` (Debug) relinked the exe but recompiled only `clientGraphics` (a direct, changed reference). The Debug objects for `Game.cpp` (clientGame), `CuiIoWin.cpp` (clientUserInterface), and `SwgCuiCommandParserDefault.cpp` (swgClientUserInterface) stayed at their May-29 timestamps -- meaning the first relinked exe did NOT contain Task 1's `onFrameEnd` hook or Task 2's keybinds/command. MSBuild's project-reference dependency tracking did not propagate the source-file changes through the sibling library projects in this invocation.
- **Fix:** Explicitly built `clientGame.vcxproj`, `clientUserInterface.vcxproj`, and `swgClientUserInterface.vcxproj` (Debug) -- each recompiled its changed `.cpp` -- then relinked `SwgClient.vcxproj`. Confirmed via obj timestamps (all four at 10:33-10:50 today) before claiming the build pass. Same explicit-per-lib build was applied for Release.
- **Files modified:** none (build-process correction only).
- **Verification:** Debug + Release relink clean (0 unresolved externals); Debug boot to D3D11 present loop, no crash.
- **Committed in:** n/a (no source change). Recorded here as the controlling build invariant for Plan 03.

---

**Total deviations:** 1 (Rule 3 - blocking build-process issue, no source change). **Impact:** none on plan intent; it is the reason the build was rebuilt per-library before the boot/schema gate -- without it the harness would have appeared linked but been absent from the exe. **This is the load-bearing build note for Plan 03: rebuild each changed library project explicitly, do not trust a single top-level SwgClient build to recompile sibling-lib source.**

## Issues Encountered

- **SWGSource VM login server (192.168.1.200:44453) closed this session** (Test-NetConnection = False), exactly as Plan 01 documented. Full character-select (login -> cluster -> character list) is unreachable, so the in-engine F10/F11 overlay-flip + `/setrunlabel` overlay-update + on-screen REC interaction could not be exercised at a loaded scene.
  - **What was verified instead:** (1) Debug rasterMajor=11 booted through D3D11 device/swapchain creation into a steady present loop (frames presenting, "initial clear-to-color present complete", per-frame WVP uploads) with no FATAL/crash for 30s -- proving the harness (install hook, onFrameEnd driver, _DEBUG keybinds, parser command) is linked into `SwgClient_d.exe` and runs under gl11. (2) The `analysis.py` schema contract -- the deterministic, server-independent half of the smoke -- was validated against a hand-written fixture using the writer's exact 10-column header (one `-on` + one `-off` row): no header-mismatch, conditions pooled, `verdict = keep` emitted (exit 0).
  - **Plan 03 (the real >=3-pass/condition capture) must be run with the SWGSource VM up.** The build + renderer-init + boot-to-present-loop milestone and the schema acceptance are proven here; the live overlay interaction is deferred to the Plan 03 capture session (which requires the VM regardless).

## Boot Gate Record (Task 3)
- **Link-grep (Debug):** 0 `unresolved external symbol`, 0 LNK2001/2019, 0 `error C/LNK`. Exe staged `stage/SwgClient_d.exe` @ 10:50.
- **Link-grep (Release):** 0 `unresolved external symbol`, 0 LNK2001/2019, 0 `error C/LNK`. Exe staged `stage/SwgClient_r.exe` @ 11:05.
- **Debug rasterMajor=11 boot:** PASS to D3D11 present loop (no crash, 30s). Full character-select gated on server availability (login port closed).
- **analysis.py schema smoke:** PASS -- writer's exact header accepted, `-on`/`-off` pooled, `verdict = keep` line emitted (exit 0).
- **Capture dir hygiene:** `stage/dpvs-profile/` unchanged (23 Phase 10 CSVs preserved; before/after diff empty). No smoke CSV leaked.

## Known Stubs
- None introduced by this plan. (Inherited from Plan 01: `gpu_us` always 0 and `visible_object_count` 0-in-Release -- both intentional, documented in 23-01-SUMMARY.md.)

## Next Phase Readiness
- **Plan 03 (capture session) is unblocked on the harness side.** F10/F11/`/setrunlabel`/per-frame-onFrameEnd are all wired and linked into `SwgClient_d.exe` (rasterMajor=11). The remaining prerequisite is the **SWGSource VM login server running** so the client can reach a loaded scene to capture against.
- **Controlling build note for Plan 03:** rebuild each changed library project explicitly before relinking SwgClient -- a single top-level build does not recompile sibling-lib source (see the deviation).
- **Do not delete the existing 23 Phase 10 CSVs in `stage/dpvs-profile/`** when starting Plan 03 unless intentionally archiving them; this plan left them untouched.
- The shipped Release path is unaffected -- the F10/F11/occlusion surface is `_DEBUG`-only; `/setrunlabel` is harmless without the `_DEBUG` capture path.

---
*Phase: 23-dpvs-d3d11-remeasure*
*Completed: 2026-06-12*

## Self-Check: PASSED
- Modified files verified on disk: SetupClientGraphics.cpp, Game.cpp, CuiIoWin.cpp, SwgCuiCommandParserDefault.cpp (all FOUND).
- Commits verified in git log: 33fe2eff3 (FOUND), f9ac31d81 (FOUND).
- Automated verifies passed: install/onFrameEnd grep hits, toggleForceDisableOcclusionCulling/toggleCapture hits, setrunlabel/setRunLabel hits, setDisableOcclusionCulling == 0 hits in CuiIoWin.
- Both renderers link clean (0 unresolved); Debug boots to D3D11 present loop; analysis.py emits `verdict = keep` on the writer's exact header.
