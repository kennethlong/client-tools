---
phase: 12-orphaned-directory-project-deletes
reviewed: 2026-05-25T00:00:00Z
depth: standard
files_reviewed: 3
files_reviewed_list:
  - src/engine/client/library/clientGame/src/shared/core/ClientHeadTracking.cpp
  - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiG15Lcd.cpp
  - src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHud.cpp
findings:
  critical: 0
  warning: 0
  info: 3
  total: 3
status: clean
---

# Phase 12: Code Review Report

**Reviewed:** 2026-05-25T00:00:00Z
**Depth:** standard
**Files Reviewed:** 3
**Status:** clean

## Summary

This phase is a dead-code removal (v2.1 Decruft). The three source files under review
contain DELETIONS and de-wiring, not new logic. I reviewed each change against the
phase intent (remove NaturalPoint TrackIR and Logitech G15 LCD support without breaking
callers) and cross-checked the call graph and public headers.

The de-wiring is correct and complete:

1. **ClientHeadTracking.cpp** — All NPClient/TrackIR implementation removed; the 5 public
   methods reduced to no-op stubs. I verified the stub signatures match the public header
   (`src/shared/core/ClientHeadTracking.h`) exactly: `install()`, `isSupported()`,
   `getEnabled()`, `setEnabled(bool)`, `getYawAndPitch(float&, float&)`. All three callers
   degrade correctly:
   - `CockpitCamera.cpp:826` gates on `getEnabled()` (now always false) and falls through to
     the normal mouse path — no regression, head tracking simply never engages.
   - `SwgCuiOptControls.cpp:198` gates on `isSupported()` (now always false) and disables the
     "checkTrackIr" checkbox — the intended "reports unsupported" behavior.
   - `SetupClientGame.cpp:738` calls the now-no-op `install()` — harmless.
   The namespace-local `remove()` and its `ExitChain::add(remove, ...)` registration were
   deleted together; `remove()` had no external callers (grep-confirmed), so no dangling
   ExitChain entry or orphaned reference results.

2. **SwgCuiG15Lcd.cpp** — `#include "EZ_LCD.h"` and `#define USE_LCD` removed. All
   `CEzLcd`/`s_lcd` usage stays inside `#ifdef USE_LCD` blocks, so it compiles out; the three
   public methods (`initializeLcd`/`updateLcd`/`remove`) remain as empty no-op bodies. Header
   signatures match. Callers in SwgCuiHud (`:678`, `:686`, `:1140`) and ClientMain invoke the
   no-ops harmlessly.

3. **SwgCuiHud.cpp** — Only the bare `#include "EZ_LCD.h"` line was removed. That symbol was
   unused in this translation unit (no `CEzLcd`/`s_lcd`/`LG_*` references anywhere in the file —
   grep-confirmed). The `SwgCuiG15Lcd::initializeLcd/remove/updateLcd` call sites are unchanged
   and now call no-op methods. No behavioral change.

No orphaned references to deleted symbols remain in any source file outside the planning/docs
trees (grep across `**/*.cpp` for `NPClient|TRACKIRDATA|NPRESULT|EZ_LCD.h|CEzLcd` returns only
the inert guarded code in SwgCuiG15Lcd.cpp and the descriptive comments). No BLOCKER or WARNING
findings. The Info items below concern the inert `#ifdef USE_LCD` code that remains in the file —
latent issues that would only surface if someone re-enables `USE_LCD`. They are pre-existing and
out of scope for the deletion, recorded for awareness only.

## Info

### IN-01: Dead guarded block in SwgCuiG15Lcd.cpp retains a latent buffer-overflow risk if re-enabled

**File:** `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiG15Lcd.cpp:260,274-310`
**Issue:** The inert `#ifdef USE_LCD` block in `updateLcd()` uses `char tmp[512]` with a long
sequence of unbounded `sprintf(tmp, ...)` calls. This is dead code today (USE_LCD is undefined),
so it cannot execute. It is recorded only because the phase comment (lines 26-29) explicitly
states the guarded code "remains valid... so callers keep linking" and implies it could be
re-enabled. If `USE_LCD` is ever re-defined, these `sprintf` calls (e.g. `"%5d"` of an `int`)
are bounded in practice but the pattern is fragile. This is pre-existing and NOT introduced by
this phase.
**Fix:** If the LCD feature is permanently removed, prefer deleting the guarded `#ifdef USE_LCD`
bodies entirely in a follow-up rather than leaving inert code. If retained for possible re-enable,
migrate `sprintf` to `snprintf(tmp, sizeof(tmp), ...)` when/if revived. No action required for this
deletion phase.

### IN-02: Stub `setEnabled(bool)` drops its parameter name — intentional but worth a one-word marker

**File:** `src/engine/client/library/clientGame/src/shared/core/ClientHeadTracking.cpp:47`
**Issue:** `void ClientHeadTracking::setEnabled(bool)` omits the parameter name to suppress the
unused-parameter warning, which matches the no-op intent and the header declaration
(`setEnabled(bool enabled)`). This is correct and consistent with the codebase. Noting only that
`getYawAndPitch(float & yaw, float & pitch)` keeps its named params (because it assigns them),
creating minor stylistic asymmetry between the two stubs. No correctness impact.
**Fix:** None required. Optionally add a brief `// no-op: TrackIR removed` inline comment on the
stub bodies for future readers, though the file header comment already documents this.

### IN-03: Inert guarded code still references engine state, masking future drift

**File:** `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiG15Lcd.cpp:35-184,193-315,323-404,411-416`
**Issue:** The `#ifdef USE_LCD` blocks reference a wide set of live engine APIs (PlayerObject,
PvpData, CuiChatManager, ContainerInterface, etc.). Because the block is compiled out, these
references are never type-checked by the compiler. If any of those APIs change signature during
later decruft phases, the LCD code will silently rot and only break on a future `USE_LCD` re-enable.
This is the standard trade-off of leaving large guarded blocks behind; recorded for awareness, not a
defect in this change.
**Fix:** Consider a follow-up to fully delete the LCD implementation (the includes at lines 12-24
and the entire guarded namespace/methods) once the feature is confirmed permanently gone, leaving
only the empty public method bodies. Not required for this phase.

---

_Reviewed: 2026-05-25T00:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
