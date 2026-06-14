---
phase: 26-instrumentation-removal-options-window-fatal
plan: 02
subsystem: ui
tags: [options-window, fatal, getCodeDataObject, swgcuioptui, codedata, d3d9, d3d11]

# Dependency graph
requires:
  - phase: 26-instrumentation-removal-options-window-fatal
    provides: the same SwgClient build used for the dual-renderer Options smoke
provides:
  - HARD-04 closed — opening the Options window no longer FATALs under either renderer
  - audit confirming the only d1b3c0eaf-introduced widget lookup is guarded optional
affects: [options-window, hud, future-ui-phases]

# Tech tracking
tech-stack:
  added: []
  patterns: [tolerant getCodeDataObject(optional=true)+null-guard for post-v3.0 widgets absent from the shipped .ui]

key-files:
  created: []
  modified: []  # NO code change required — fix already present in 5fce7bb83

key-decisions:
  - "Audit-only outcome: the fix was already complete in prior commit 5fce7bb83; no re-implementation"
  - "Verified functional reality at runtime under both renderers, not just the artifact claim"

patterns-established:
  - "When a plan says 'verify reality first', a clean audit that finds the fix already landed is a valid PASS with no diff"

requirements-completed: [HARD-04]

# Metrics
duration: ~10min (audit) + shared dual-renderer smoke with 26-01
completed: 2026-06-13
---

# Phase 26 Plan 02: Options-Window FATAL (HARD-04) Summary

**Audit confirmed the Options-window FATAL was already fixed in `5fce7bb83`; no code change needed, and the dual-renderer runtime smoke proved Options opens cleanly.**

## Performance

- **Duration:** ~10 min audit (runtime smoke shared with 26-01's boot gate)
- **Completed:** 2026-06-13
- **Tasks:** 2 (audit / dual-renderer Options-open smoke)
- **Files modified:** 0

## Accomplishments

- **Task 1 — audit:** `git show d1b3c0eaf` shows that feature commit introduced **exactly one** new `getCodeDataObject` lookup into `SwgCuiOptUi.cpp`: `checkShowToolbarCommandCooldownTimer`. `git show 5fce7bb83` shows that commit converted that exact lookup to the tolerant pattern (`getCodeDataObject(..., true)` + `if (checkbox)` guard at lines 223–226) and proactively hardened 9 more widgets (`checkRenderVariableTargetingReticle`, `checkAutoSort*`, `checkNewVendorExamine`, 5× `sliderBuffIconSize*`). Census: 63 lookups, 10 optional. **No required lookup references a widget known absent from the shipped v3.0 `.ui`.** No code change applied.
- **Task 2 — runtime smoke (human-approved):** Options window opened with **no `Unable to find CodeData object` FATAL** under both rasterMajor=11 (D3D11, in-game) and rasterMajor=5 (D3D9, char-select). HARD-04 closed against functional reality.

## Surprises

- The blocking item for this plan turned out to be a *different* bug entirely — a `stage/client.cfg` UTF-8 BOM that crashed the **Release** exe during config parse (documented in 26-01-SUMMARY). Once the BOM was stripped, Options opened cleanly under D3D9. Nothing in `SwgCuiOptUi.cpp` was at fault.

## Verification

- `SwgCuiOptUi.cpp:224` = `getCodeDataObject(TUICheckbox, checkbox, "checkShowToolbarCommandCooldownTimer", true)` with `if (checkbox)` guard at :225.
- Options opens without FATAL under both renderers (human-approved).
