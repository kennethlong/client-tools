---
phase: 26-instrumentation-removal-options-window-fatal
plan: 01
subsystem: infra
tags: [dpvs, instrumentation, clientGraphics, decruft, d3d9, d3d11, vcxproj]

# Dependency graph
requires:
  - phase: 23-dpvs-d3d11-remeasure
    provides: the DpvsProfileInstrumentation module (restored in Phase 23) whose profiling purpose this removes
  - phase: 24-dpvs-config-gate-machine-portability
    provides: the DPVS config-gate verdict that superseded the D-15 profiling purpose
provides:
  - D-15 DPVS profiling instrumentation fully removed from all shipped code paths
  - clean tree (grep -r DpvsProfileInstrumentation src/ == 0) with zero dangling symbols
  - Debug + Release SwgClient relink clean (0 grep-confirmed unresolved externals)
affects: [27-hard-05-d3dcompile, x64-port, cornersnap-harness-removal]

# Tech tracking
tech-stack:
  added: []
  patterns: [atomic delete + de-wire in one commit, /FORCE link-grep verification gate]

key-files:
  created: []
  modified:
    - src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp
    - src/engine/client/library/clientGraphics/src/shared/RenderWorld.h
    - src/engine/client/library/clientGraphics/src/win32/SetupClientGraphics.cpp
    - src/engine/client/library/clientGraphics/build/win32/clientGraphics.vcxproj
    - src/engine/client/library/clientGame/src/shared/core/Game.cpp
    - src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp
    - src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserDefault.cpp
  deleted:
    - src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp
    - src/engine/client/library/clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h

key-decisions:
  - "Kept the F11 ms_forceDisableOcclusionCulling occlusion-flag pair (Phase 23 feature, not D-15)"
  - "Kept the CORNERSNAP _DEBUG probes (door-snap harness, deferred to x64/HARD-05) — untouched, byte-verified"
  - "Kept dpvsCpuTimer in RenderWorld.cpp per plan (removed only its record* consumers)"
  - "Verified via Debug+Release link-grep for 'unresolved external symbol', not MSBuild exit 0 (/FORCE masks)"

patterns-established:
  - "ABI-cascade check must grep the ACTUAL plugin trees (Direct3d9/Direct3d11/...), not clientGraphics/src/win32 — the header was confirmed plugin-invisible (0 refs in plugin sources)"

requirements-completed: [HARD-03]

# Metrics
duration: ~90min (incl. a non-Phase-26 client.cfg BOM crash dug out during the boot smoke)
completed: 2026-06-13
---

# Phase 26 Plan 01: D-15 DPVS Instrumentation Removal Summary

**The DpvsProfileInstrumentation module and every call site are gone in one atomic commit; both renderers boot clean and the CORNERSNAP door-snap harness is preserved intact.**

## Performance

- **Duration:** ~90 min (the build + removal were fast; most time went to diagnosing a *separate*, pre-existing `client.cfg` UTF-8-BOM crash surfaced by the Release boot smoke — see Surprises)
- **Started:** 2026-06-13T20:30Z
- **Completed:** 2026-06-13T21:10Z
- **Tasks:** 3 (delete+de-wire / build+link-grep / dual-renderer boot smoke)
- **Files modified:** 7 modified, 2 deleted

## Accomplishments

- **Task 1 — atomic removal (commit `6c95fa990`):** deleted `DpvsProfileInstrumentation.{cpp,h}` and de-wired all 6 call sites: `RenderWorld.cpp` (4 `record*` calls + the `_DEBUG` visible-object block), `Game.cpp` (`onFrameEnd`), `SetupClientGraphics.cpp` (`install`), `CuiIoWin.cpp` (DIK_F10 `toggleCapture`), `SwgCuiCommandParserDefault.cpp` (`/setrunlabel` MAKE_COMMAND + help row + handler), and the two `clientGraphics.vcxproj` entries (inline). `grep -r DpvsProfileInstrumentation src/` → 0.
- **Task 2 — build gate:** ABI-cascade check across the real plugin trees → 0 references (header plugin-invisible). Deleted the linker-output exes and rebuilt SwgClient **Debug** and **Release**; both relinked with **0 `unresolved external symbol`** (grep-confirmed; the real link.exe line consumed the rebuilt `clientGraphics.lib`).
- **Task 3 — dual-renderer boot smoke (human-approved):** Debug/**D3D11** booted all the way in-game (Mos Eisley) with Options opening cleanly (verified by Kenny + a cdb run). Release/**D3D9** (gl05_r.dll) booted to char-select with Options clean after the BOM fix.

## Preserved (NOT touched)

- F11 → `RenderWorld::toggleForceDisableOcclusionCulling()` occlusion-flag pair (Phase 23).
- CORNERSNAP `_DEBUG` probes in `CollisionResolve.cpp` / `CellProperty.cpp` — byte-unchanged (deferred to x64/HARD-05; they remain the door-snap acceptance harness).

## Surprises

- **"Release client not starting" was NOT this change.** The Release boot smoke crashed at `ConfigFile::Section::getName` (null section) inside `SetupSharedFoundation::install`. Root cause: a **UTF-8 BOM** on `stage/client.cfg` (Release reads `client.cfg`; Debug reads `client_d.cfg`, which had no BOM). The BOM defeats the `*line=='#'` comment check, so line 1 hits `processKeys` before any `[Section]`; Release crashes on the null deref where Debug's `DEBUG_FATAL` (line 443, compiled out in Release) would have caught it. Fixed by stripping the 3-byte BOM from the gitignored `stage/client.cfg`. Likely introduced by a PowerShell write (PS 5.1 BOM default) from an earlier step or the parallel session. Latent landmine for any Release boot — see new memory.

## Verification

- `grep -rn DpvsProfileInstrumentation src/` → 0; `grep -rn setrunlabel src/game/.../swgClientUserInterface` → 0.
- CORNERSNAP probe files: `git status` clean (unchanged).
- Debug + Release link logs: 0 `unresolved external symbol`.
- Both renderers reach char-select; Options opens with no FATAL under rasterMajor=5 and =11.
