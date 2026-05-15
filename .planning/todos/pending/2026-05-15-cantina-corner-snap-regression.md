---
created: 2026-05-15
title: Cantina interior corner-snap regression introduced May 8 2026
area: rendering / portal traversal / toolchain
next_action: /gsd-debug
files:
  - D:/Code/swg-client/build/bin/Debug/SwgClient_d.exe (GOOD reference — May 8 16:44, no corner snap)
  - D:/Code/swg-client/build-v145/bin/Debug/SwgClient_d.exe (REGRESSED — May 8 21:51, has corner snap)
  - D:/Code/swg-client-v2/stage/SwgClient_d.exe (current v2; inherits regression from build-v145 lineage)
  - D:/Code/swg-client/exe/win32/SwgClient_r.exe (original 2010 SOE binary; doesn't run against SWGSource VM, can't use as reference)
---

## Symptom

Inside the Mos Eisley cantina, walking around the interior and turning a corner
to a different sub-area of the cantina causes the camera/character to **snap to
a new position** in a way that breaks visual continuity. Distinct from the
expected snap at the main cantina door (outdoor→indoor portal transition).

- **GOOD reference (no corner snap):** `D:/Code/swg-client/build/bin/Debug/SwgClient_d.exe`
  dated 2026-05-08 16:44. Snaps only at the main entrance door (expected
  outdoor→indoor portal transition).
- **First regressed build:** `D:/Code/swg-client/build-v145/bin/Debug/SwgClient_d.exe`
  dated 2026-05-08 21:51. Snaps at the main door AND at interior corners
  (between sub-cells / connected interior rooms).
- **Current v2:** Inherits the regression from the build-v145 lineage. Confirmed
  reproducing in `D:/Code/swg-client-v2/stage/SwgClient_d.exe` post-Phase-10
  D-13/D-14 edits (commit `18bc4fdc5`). NOT caused by my Phase 10 edits —
  predates them by a week.

## Regression window

**~5 hours on 2026-05-08, between 16:44 and 21:51.**

Possible introduction mechanisms:

1. **Toolchain swap to v145 MSVC.** The directory naming convention
   (`build-v145/`) strongly suggests this build was an explicit v145-toolset
   rebuild. If the GOOD build used an earlier MSVC toolset (vXXX, likely v143
   or vXXY), the regression could be a **toolchain artifact** rather than a
   source change. Candidate mechanism: STLPort 4.5.3 compat-shim
   (`stlport_vc143_compat.cpp`) interacts differently with v145 codegen for
   portal-traversal / cell-rendering inline functions.

2. **Source changes in Phase 7 / Phase 8 work that landed that evening.**
   Phase 7 (dead-code removal Track A) closed 2026-05-07. Phase 8 (Track B —
   tool CMakeLists) was active during May 8. Phase 8 plans authored CMake
   files for editor tools — should not have touched client engine code, BUT
   git log of that window may reveal incidental client-tier edits.

3. **A specific commit on May 8 evening between 16:44 and 21:51** that's
   independently identifiable via `git log --oneline --since='2026-05-08 16:44'
   --until='2026-05-08 21:51'` in the swg-client tree (whitengold lineage,
   not v2).

## Bisection approach for /gsd-debug

1. **Confirm the toolchain hypothesis first** (cheapest test):
   - Rebuild `D:/Code/swg-client/build/bin/Debug/SwgClient_d.exe` clean with
     v145 toolset explicitly. If the new build snaps → toolchain regression
     confirmed. If still clean → source-change regression, narrow to commits
     in the 5-hour window.

2. **If toolchain-confirmed**, suspects are:
   - STLPort 4.5.3 compat-shim's interaction with v145 inline / template
     instantiation
   - MSVC v145 default optimization changes (LTCG, /Ob, inlining heuristics)
     applied to portal-traversal or cell-visibility code paths
   - Different default behavior of `/Zc:wchar_t-` between toolset versions
     (relevant if portal code passes wchar_t-typed data through XPCOM-stub
     boundaries that were still present in this window — XPCOM source removal
     was Phase 7 step 3, which closed 2026-05-07; XPCOM headers may have
     lingered into May 8 builds via cached object files)

3. **If source-change-confirmed**, candidate files based on Phase 7 closure
   notes:
   - `engine/client/library/clientGraphics/src/shared/` — anything that
     touched cell/portal/rendering during the window
   - `engine/client/library/clientObject/src/shared/cellProperty/` — cell
     property updates (portal traversal)
   - Files touching `CellProperty::cellChange` or similar callbacks

## Suspect candidates (orthogonal)

- **Phase 7 XPCOM removal could have left an inline stub** that was
  exercised during cell transitions, and v145 toolset evaluates it
  differently. Low confidence — but XPCOM removal IS exactly the kind of
  scoped surgery that could leave a hidden dependency.
- **DPVS cell-portal traversal** is independent of the `OCCLUSION_CULLING`
  bit we just modified — `portalRecusionDepth` is unchanged in this Phase 10
  work. So the corner-snap is genuinely orthogonal to Phase 10.

## Not blocking

- Phase 10 closure: boot smoke acceptance was "client renders Mos Eisley
  plaza without crash" — met indoor AND outdoor. The corner-snap is a
  visual continuity bug, not a crash or rendering failure.
- Phase 11 (D3D11 renderer plugin): renderer-side work that may incidentally
  fix this regression OR may inherit it. Worth a parallel test once D3D11
  baseline renders.
- The SafeCast.h:29 todo (also at this directory): orthogonal again.

## Reproduction

1. Launch any binary in the build-v145 / Koogie / v2 lineage against the
   SWGSource VM
2. Login as Tatooine character; walk to Mos Eisley cantina
3. Enter cantina (snap at main door is EXPECTED — that's the outdoor→indoor
   portal transition)
4. Walk inside and try to turn a corner toward a different sub-area of the
   interior
5. Observe: camera/character snaps to new position

Compare against `D:/Code/swg-client/build/bin/Debug/SwgClient_d.exe`
launched from `D:/Code/swg-client/build/bin/Debug/` (CWD matters per
v1 milestone client.cfg setup) for the GOOD reference behavior — main-door
snap only, no interior corner snap.

## Cross-references

- `.planning/STATE.md` — v1 milestone close (Phase 6, 2026-05-05) → Phase 7
  close (2026-05-07) → Phase 8 close-as-scoped (2026-05-08)
- `project_v2_fork_location` memory in `D:/Code/swg-client/` — v1 working
  directory and build path history
- This todo is orthogonal to:
  - `2026-05-14-safecast-null-dynamic-cast-world-load.md` (Koogie SafeCast issue)
  - Phase 11 DPVS remeasurement (ROADMAP criterion #6)
