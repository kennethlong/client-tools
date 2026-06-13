---
phase: 24-dpvs-config-gate-machine-portability
plan: 01
subsystem: graphics
tags: [dpvs, occlusion-culling, config, d3d11, renderworld, bloom, vertex-buffers]

# Dependency graph
requires:
  - phase: 23-dpvs-d3d11-profiling
    provides: DPVS D3D11 verdict (outdoor occlusion keep / interior remove), the F11 ms_forceDisableOcclusionCulling A/B toggle
provides:
  - "dpvsOcclusionMode = auto | on | off tri-state config knob (HARD-01), default off (D-02)"
  - "ConfigClientGraphics::DpvsOcclusionMode enum + getDpvsOcclusionMode() getter"
  - "Config-gated OCCLUSION_CULLING bit in RenderWorld::drawScene, computed in both Debug and Release (replaces the Phase 10 Option alpha #else hardcode)"
  - "Null-guarded POB-membership auto branch (T-24-02 eliminated)"
  - "D-04 F11 force-disable override preserved (Debug only, applied after config logic)"
  - "disableMultiStreamVertexBuffers engine default flipped true->false (D-07, PORT-02 prerequisite)"
  - "D3D11 setBloomEnabled bound to a non-fatal setBloomEnabled_impl no-op (was scaffold_fatal_stub) (D-07, PORT-02 prerequisite)"
affects: [24-03 cfg-template (deletes the now-redundant disableMultiStreamVertexBuffers + ClientGame/Bloom enable keys), 26-release-ab-measurement]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Tri-state string config knob: getKeyString + _stricmp parse-at-install into a cached enum, default-safe on unrecognized/empty (T-24-01)"
    - "Gl_api slot rebind from STUB(scaffold_fatal_stub) to a real _impl no-op free function (mirrors setPointSize_impl)"

key-files:
  created: []
  modified:
    - src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.h
    - src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp
    - src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp

key-decisions:
  - "occlusionMode is a STRING read from [ClientGraphics/Dpvs] via getKeyString (NOT a KEY_BOOL in [ClientGraphics]) — matches the DpvsProfileInstrumentation precedent"
  - "DOM_off = 0 so a zero-init is the safe default; default off (D-02) keeps shipped behavior at Option alpha when the key is absent"
  - "Debug builds without an occlusionMode key now default to OFF (no longer occlusion-on-by-default); the Phase-23 posture returns via occlusionMode=auto (Debug template, plan 24-03) or F11"
  - "auto branch null-guards ms_cameraCell (T-24-02 eliminated — null cell -> bit off, no deref)"
  - "F11 force-disable applied AFTER the config/auto logic (RESEARCH Pitfall 3) so it wins regardless of mode (D-04)"
  - "setBloomEnabled has NO function body — it was a STUB-macro Gl_api slot binding to scaffold_fatal_stub; fix = add setBloomEnabled_impl no-op + rebind the slot (REVIEW correction: codex HIGH / cursor MEDIUM)"

patterns-established:
  - "Tri-state config knob parsed once at install into a cached enum, consumed per-frame"
  - "Gl_api fatal-stub -> non-fatal _impl no-op rebind for unimplemented D3D11 slots"

requirements-completed: [HARD-01, PORT-02]

# Metrics
duration: ~15 min (code tasks 1-3; Task 4 checkpoint pending human verification)
completed: 2026-06-13
---

# Phase 24 Plan 01: DPVS Config Gate + Engine-Default Flips Summary

**Tri-state `dpvsOcclusionMode = auto|on|off` knob (default off) gating the DPVS OCCLUSION_CULLING bit per-frame in `RenderWorld::drawScene` with a null-guarded POB-membership auto branch, plus the two D-07 engine-default flips (multi-stream skinned VBs default-on, D3D11 Bloom bound to a non-fatal no-op) that let the permanently-required override keys disappear from the cfg in plan 24-03.**

## Performance

- **Duration:** ~15 min (code tasks 1-3 complete; Task 4 is a blocking human-verify checkpoint)
- **Started:** 2026-06-13T01:40:00Z (approx)
- **Checkpoint reached:** 2026-06-13T01:54:00Z
- **Tasks:** 3 of 4 complete (Task 4 = blocking human-verify checkpoint, awaiting build + dual-renderer boot)
- **Files modified:** 4

## Accomplishments
- Added `DpvsOcclusionMode` enum (`DOM_off=0`/`DOM_on`/`DOM_auto`) + `getDpvsOcclusionMode()` getter on `ConfigClientGraphics`, parsing `[ClientGraphics/Dpvs] occlusionMode` once at install via `getKeyString` + `_stricmp`, defaulting to `DOM_off` (D-02; T-24-01 safe fallback).
- Replaced the Phase 10 Option alpha `#else` hardcode in `RenderWorld::drawScene` with a config-gated `occlusionBit` computed in both Debug and Release: `DOM_on` -> always set, `DOM_auto` -> null-guarded `ms_cameraCell && ms_cameraCell->isWorldCell()` (outdoor on / interior off, D-01), `DOM_off` -> never. F11 `ms_forceDisableOcclusionCulling` clears the bit *after* the config logic in `#ifdef _DEBUG` (D-04). Change-detect block + `portalRecusionDepth` untouched.
- Flipped `disableMultiStreamVertexBuffers` engine default `true -> false` so multi-stream skinned VBs are the default (D-07).
- Added `setBloomEnabled_impl(bool)` no-op (one-shot debug log) to `Direct3d11.cpp` and rebound `ms_glApi.setBloomEnabled` to it, replacing the `STUB(setBloomEnabled)` scaffold_fatal_stub binding (D-07; T-24-03 — Bloom-enabled no longer FATALs).

## Task Commits

Each code task was committed atomically:

1. **Task 1: Add DpvsOcclusionMode enum + getDpvsOcclusionMode() getter** - `4cf9926b5` (feat)
2. **Task 2: Config-gate the DPVS occlusion bit in RenderWorld::drawScene** - `9b6911b1d` (feat)
3. **Task 3: D-07 engine-default flips (multistream VB + D3D11 Bloom no-op)** - `09c5a92c6` (feat)

**Task 4 (Dual-renderer boot gate):** blocking `checkpoint:human-verify` — NOT yet complete. Requires a clientGraphics + Direct3d11 build (in the main checkout — this transient worktree has no staged client) followed by interactive dual-renderer boot verification (Mos Eisley -> cantina walk, DebugMonitor overlay, F11, rasterMajor=11/5 A/B) that only the user can perform.

## Files Created/Modified
- `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.h` - DpvsOcclusionMode enum + getDpvsOcclusionMode() declaration
- `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp` - occlusionMode parse-at-install + getter; disableMultiStreamVertexBuffers default flip true->false
- `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` - config-gated occlusionBit replacing the Option alpha #else branch; null-guarded auto; F11 override after config logic
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp` - setBloomEnabled_impl no-op free function + slot rebind replacing STUB(setBloomEnabled)

## Decisions Made
See `key-decisions` in frontmatter. Most notable, per the plan's REVIEW correction and output directive:
- **Debug-default DPVS shift (REVIEW/Cursor MEDIUM):** Debug builds without an `occlusionMode` key now default to **off** (no longer occlusion-on-by-default in Debug). Devs wanting the Phase-23 occlusion-on posture should set `occlusionMode=auto` (shipped in the Debug template, plan 24-03) or press F11. This is the intended D-02 behavior.

## Deviations from Plan

None - plan executed exactly as written. The REVIEW corrections embedded in the plan (string-read for occlusionMode, null-guarded auto branch, setBloomEnabled as a Gl_api slot binding rather than a function body) were followed as authored.

**Total deviations:** 0
**Impact on plan:** None — all automated acceptance criteria for Tasks 1-3 pass on the source as written.

## Issues Encountered
None for the code tasks. Task 4 is a blocking human-verify checkpoint by design (autonomous: false); it cannot be completed by a parallel worktree executor because:
1. The build + boot must target the main checkout's `stage/` (the worktree's `stage/` contains only `override/`, no staged client/cfg/TREs/miles redist).
2. The verification is interactive in-game (walk to cantina, DebugMonitor overlay, F11, renderer A/B) — user-only.

## Automated Verification (Tasks 1-3) — all PASS
- Task 1: `getDpvsOcclusionMode`/`DpvsOcclusionMode`/`DOM_auto` in .h AND `getKeyString("ClientGraphics/Dpvs"` in .cpp — PASS
- Task 2: `getDpvsOcclusionMode` + null-guarded `ms_cameraCell && ms_cameraCell->isWorldCell` + `ms_forceDisableOcclusionCulling` present; old Option-alpha `#else` hardcode gone; change-detect (`ms_lastCullingParameters`/`setParameters`) intact — PASS
- Task 3: `KEY_BOOL(disableMultiStreamVertexBuffers, false)` + `void setBloomEnabled_impl` + `ms_glApi.setBloomEnabled = setBloomEnabled_impl` + zero `STUB(setBloomEnabled)` matches — PASS

## Threat Surface
No new trust boundaries beyond the plan's `<threat_model>`. T-24-01 (parse tampering) mitigated via getKeyString default + _stricmp-else-DOM_off; T-24-02 (auto null deref) eliminated via null guard; T-24-03 (Bloom FATAL) mitigated via the non-fatal no-op rebind.

## Next Phase Readiness
- Code complete for HARD-01 + PORT-02 engine-side prerequisites.
- **BLOCKING:** Task 4 dual-renderer boot gate must be satisfied (build in main checkout + user in-game verification) before this plan is fully done.
- Plan 24-03 (cfg template) can delete the now-redundant `disableMultiStreamVertexBuffers=false` and `[ClientGame/Bloom] enable=0` keys once these flips land and verify.

---
*Phase: 24-dpvs-config-gate-machine-portability*
*Completed: 2026-06-13 (code tasks; checkpoint pending)*
