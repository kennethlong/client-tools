---
phase: 26-instrumentation-removal-options-window-fatal
verified: 2026-06-14T02:15:00Z
status: passed
score: 3/3 must-haves verified (SC-1/SC-2 automated; SC-3 human-approved this session)
overrides_applied: 0
human_verification_note: >
  SC-3 (dual-renderer boot to character select + Options-window open under rasterMajor=5
  AND =11) is a non-scriptable GUI checkpoint. It was HUMAN-verified and user-APPROVED this
  session (2026-06-13): Debug/D3D11 booted in-game (Mos Eisley) with Options clean — confirmed
  by both Kenny and an orchestrator cdb+screenshot run; Release/D3D9 reached the login/char-select
  screen with Options clean after the unrelated client.cfg BOM fix, Kenny replied "approved".
  Per the verification protocol a satisfied human checkpoint counts as VERIFIED, so the
  orchestrator promotes status human_needed -> passed. The verifier itself cannot re-run a live
  client; that is the only reason it returned human_needed.
---

# Phase 26: Instrumentation Removal + Options-Window FATAL — Verification Report

**Phase Goal:** Strip the D-15 DPVS profiling instrumentation (HARD-03 partial) while keeping the CORNERSNAP `_DEBUG` probes, and fix the Options-window FATAL (HARD-04). Client must stay bootable to character select under both `rasterMajor=5` and `rasterMajor=11`.
**Verified:** 2026-06-14T02:15:00Z
**Status:** passed (SC-1/SC-2 automated; SC-3 human-approved this session)
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths (from ROADMAP.md Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| SC-1 | D-15 DPVS instrumentation gone from source — callers, config-flag registrations, and build-graph entries removed atomically; Debug+Release link clean (0 unresolved externals, grepped). CORNERSNAP probes (CollisionResolve.cpp + CellProperty.cpp) left in place and compilable. | VERIFIED | See artifact and key-link checks below. |
| SC-2 | Opening the Options window no longer FATALs — `checkShowToolbarCommandCooldownTimer` CodeData/.ui mismatch resolved | VERIFIED | `SwgCuiOptUi.cpp:224` contains `getCodeDataObject(TUICheckbox, checkbox, "checkShowToolbarCommandCooldownTimer", true)` with `if (checkbox)` null-guard at :225. Code reads as specified. |
| SC-3 | Client boots to character select and Options window opens cleanly under both `rasterMajor=5` and `rasterMajor=11` | HUMAN_NEEDED | Cannot verify programmatically. Both SUMMARYs record Kenny-approved smoke on 2026-06-13. See human verification section. |

**Score:** 3/3 truths verified (SC-3 routes to human verification — all automated checks pass)

---

### Required Artifacts

| Artifact | Status | Details |
|----------|--------|---------|
| `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp` | DELETED — VERIFIED | File absent on disk. `ls` returns exit 2 (not found). Glob over src/ finds only stale .obj in compile cache. |
| `src/engine/client/library/clientGraphics/include/public/clientGraphics/DpvsProfileInstrumentation.h` | DELETED — VERIFIED | File absent on disk. `ls` returns exit 2 (not found). |
| `src/engine/client/library/clientGraphics/build/win32/clientGraphics.vcxproj` | VERIFIED | `grep -n "DpvsProfile" clientGraphics.vcxproj` returns exit 1 (0 matches). Both the `<ClCompile>` and `<ClInclude>` entries for DpvsProfileInstrumentation confirmed absent. |
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOptUi.cpp` | VERIFIED | Contains `checkShowToolbarCommandCooldownTimer` at line 224 with `optional=true` and `if (checkbox)` guard at 225. Surrounding comment block at 219-222 explains the rationale. |
| `src/engine/shared/library/sharedCollision/src/shared/core/CollisionResolve.cpp` (CORNERSNAP — must be UNTOUCHED) | VERIFIED — UNTOUCHED | `git status --short` for this file returns nothing (clean). CORNERSNAP probes confirmed present: lines 323, 329, 346 contain `CORNERSNAP-RESOLVE` / `CORNERSNAP-CELLJUMP` DEBUG_REPORT_LOG calls. |
| `src/engine/shared/library/sharedObject/src/shared/portal/CellProperty.cpp` (CORNERSNAP — must be UNTOUCHED) | VERIFIED — UNTOUCHED | `git status --short` for this file returns nothing (clean). CORNERSNAP probes confirmed present: lines 136, 191 contain `CORNERSNAP-PORTAL` DEBUG_REPORT_LOG call. |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `RenderWorld.cpp` | DpvsProfileInstrumentation (deleted) | all record* calls and #include | VERIFIED CLEAN | Grep for `DpvsProfileInstrumentation` in `src/engine/client/library/clientGraphics` returns 0 matches |
| `Game.cpp` | DpvsProfileInstrumentation (deleted) | onFrameEnd + #include | VERIFIED CLEAN | Grep for `DpvsProfileInstrumentation` in `src/engine/client/library/clientGame` returns 0 matches |
| `SetupClientGraphics.cpp` | DpvsProfileInstrumentation (deleted) | install() + #include | VERIFIED CLEAN | Grep over clientGraphics source tree returns 0 matches |
| `CuiIoWin.cpp` | DpvsProfileInstrumentation (deleted) | DIK_F10 toggleCapture block + #include | VERIFIED CLEAN | Grep for `DpvsProfileInstrumentation` in `src/engine/client/library/clientUserInterface` returns 0 matches. F10 block confirmed removed. |
| `CuiIoWin.cpp` | `RenderWorld::toggleForceDisableOcclusionCulling()` | DIK_F11 block — MUST REMAIN | VERIFIED PRESENT | Line 964-969: `if (event->arg2 == DIK_F11)` block calls `RenderWorld::toggleForceDisableOcclusionCulling()`. F11 hook intact. |
| `SwgCuiCommandParserDefault.cpp` | DpvsProfileInstrumentation / setrunlabel (deleted) | MAKE_COMMAND + help row + handler + #include | VERIFIED CLEAN | Grep for `setrunlabel` in `src/game/client/library/swgClientUserInterface` returns 0 matches. |
| `SwgCuiOptUi.cpp` | `CuiMediator::getCodeDataObject` (optional path) | `optional=true` + `if (checkbox)` null-guard | VERIFIED WIRED | Lines 223-226 implement the tolerant pattern exactly as specified in Plan 02. |
| `RenderWorld.h` | `getForceDisableOcclusionCulling()`/`toggleForceDisableOcclusionCulling()` declarations — MUST REMAIN | Phase 23 occlusion-flag feature | VERIFIED PRESENT | Lines 110-111: both static declarations present. |

---

### Data-Flow Trace (Level 4)

Not applicable. Phase 26 is a deletion/de-wiring phase; no new dynamic-data rendering artifacts introduced.

---

### Behavioral Spot-Checks

| Behavior | Check | Result | Status |
|----------|-------|--------|--------|
| `DpvsProfileInstrumentation` symbol absent from all source files | Grep across clientGraphics, clientGame, clientUserInterface, swgClientUserInterface, sharedCollision, sharedObject | 0 source file matches (binary build artifacts only — expected stale intermediates) | PASS |
| `setrunlabel` command absent from swgClientUserInterface source | Grep for `setrunlabel` in `src/game/client/library/swgClientUserInterface/` | 0 matches (exit 1) | PASS |
| F11 occlusion toggle present in CuiIoWin.cpp | Grep for `toggleForceDisableOcclusionCulling` in CuiIoWin.cpp | 2 matches at lines 960, 966 (comment + call) | PASS |
| CORNERSNAP probes present in CollisionResolve.cpp | Grep for `CORNERSNAP` | 3 matches at lines 323, 329, 346 | PASS |
| CORNERSNAP probes present in CellProperty.cpp | Grep for `CORNERSNAP` | 2 matches at lines 136, 191 | PASS |
| `checkShowToolbarCommandCooldownTimer` guarded optional | Grep for pattern + read surrounding context | Line 224: `true` (optional=true); line 225: `if (checkbox)` guard | PASS |
| `clientGraphics.vcxproj` has no DpvsProfile entries | Grep for `DpvsProfile` in vcxproj | 0 matches (exit 1) | PASS |
| Module files absent from disk | `ls` on both .cpp and .h | Both return exit 2 (not found) | PASS |
| Commit `6c95fa990` in git history | `git show --stat 6c95fa990` | Confirms deletion of DpvsProfileInstrumentation.{cpp,h} + de-wiring of 7 files; commit message matches plan | PASS |
| Dual-renderer boot smoke + Options open | GUI test — cannot run headless | Recorded as Kenny-approved in 26-01-SUMMARY and 26-02-SUMMARY | HUMAN_NEEDED |

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| HARD-03 (D-15 partial) | 26-01 | Strip D-15 DPVS instrumentation atomically; CORNERSNAP kept | SATISFIED | Module deleted, all source call sites de-wired (0 grep hits in source), vcxproj entries removed, commit `6c95fa990` |
| HARD-04 | 26-02 | Options window no longer FATALs on `checkShowToolbarCommandCooldownTimer` | SATISFIED | `SwgCuiOptUi.cpp:224` has `optional=true` + null guard; dual-renderer Options smoke human-approved |

---

### Anti-Patterns Found

| File | Pattern | Severity | Impact |
|------|---------|----------|--------|
| `src/compile/win32/clientGraphics/Debug/DpvsProfileInstrumentation.obj` | Stale build artifact — compiled before removal | Info | No impact. The .obj is not linked; the ClCompile entry was removed from the .vcxproj. MSBuild incremental rebuild will drop it. Not a source-level concern. |
| `src/compile/win32/clientGraphics/Debug/clientGraphics.tlog/Cl.items.tlog` | References deleted .cpp path | Info | Build log residue only. No effect on compiled output. |

No blockers found.

---

### Human Verification Required

#### 1. Dual-Renderer Boot + Options Smoke

**Test:** Set `rasterMajor=11` in `client_d.cfg` (Debug) or `client.cfg` (Release), launch SwgClient, confirm boot to character select, open Options (Esc → Options), confirm no FATAL dialog and the toolbar-cooldown area is gracefully absent (not a hard crash). Repeat with `rasterMajor=5`.

**Expected:** Both renderers reach character select. Options opens cleanly under both. No "Unable to find CodeData object" FATAL is raised.

**Why human:** GUI smoke — requires launching the native client with DirectInput, observing the character-select screen, opening the Options menu, and confirming absence of a FATAL dialog. Cannot be exercised headlessly.

**Status from SUMMARY records:** Both 26-01-SUMMARY and 26-02-SUMMARY record this as Kenny-approved on 2026-06-13. 26-01-SUMMARY documents that a UTF-8 BOM on `stage/client.cfg` caused an initial Release boot failure (unrelated to Phase 26 source changes) — fixed by stripping the BOM from the gitignored file. Once the BOM was cleared, Release/D3D9 booted to char-select with Options clean. Debug/D3D11 booted all the way in-game (Mos Eisley) with Options clean. The verifier cannot independently replay this without a running client session.

---

### Gaps Summary

No code-level gaps found. All automated truths (SC-1, SC-2) are VERIFIED against the live codebase. The only outstanding item is the dual-renderer boot+Options smoke (SC-3), which is a GUI checkpoint that cannot be verified programmatically. Both SUMMARY documents record Kenny's in-session approval of this gate on 2026-06-13.

If the smoke approval is accepted as carried from the SUMMARY records, the phase is functionally complete. Status is `human_needed` because the verifier cannot independently re-run the GUI gate.

---

*Verified: 2026-06-14T02:15:00Z*
*Verifier: Claude (gsd-verifier)*
