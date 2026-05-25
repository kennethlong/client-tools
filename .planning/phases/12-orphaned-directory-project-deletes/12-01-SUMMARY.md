---
phase: 12-orphaned-directory-project-deletes
plan: 01
subsystem: build
tags: [msbuild, vcxproj, rsp, stationapi, trackIR, NPClient, dead-code-removal, decruft]

requires:
  - phase: 11-d3d11-renderer
    provides: dual-renderer (D3D9 gl05 + D3D11 gl11) selectable via rasterMajor — the boot-gate baseline
provides:
  - stationapi/ orphan directory removed (SOE auth SDK; 989crypt/rdp/dbgutil libs + VS6 residue)
  - trackIR/ orphan directory removed (NaturalPoint NPClient head-tracking SDK)
  - ClientHeadTracking compiled trackIR-free as no-op stubs (head tracking reports unsupported)
  - re-established dual-renderer boot baseline for the v2.1 Decruft milestone
affects: [12-02, 12-03, decruft]

tech-stack:
  added: []
  patterns:
    - "Dead-module removal: unwire build graph -> build-verify -> delete dir -> build-verify again"
    - "Live-source de-wire via no-op stubs (keep symbols defined, compile out the dead dependency)"

key-files:
  created: []
  modified:
    - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj
    - src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj
    - src/engine/client/library/clientGame/build/win32/clientGame.vcxproj
    - src/engine/client/library/clientGame/build/win32/includePaths.rsp
    - src/engine/client/library/clientGame/src/shared/core/ClientHeadTracking.cpp

key-decisions:
  - "989crypt.lib was a live (not stale) SwgClient AdditionalDependency unique to stationapi/ — dropped it; relink clean proved it was unreferenced"
  - "Kept ClientHeadTracking.cpp in the build (stubbed) instead of dropping it — dropping orphaned its callers' symbols, masked only by /FORCE"

patterns-established:
  - "Build gate must be read for unresolved-external WARNINGS, not just exit code — /FORCE makes link errors non-fatal (exit 0)"

requirements-completed: [DECRUFT-01]

duration: ~50min
completed: 2026-05-25
---

# Phase 12 / Plan 01: stationapi + trackIR orphan removal Summary

**Deleted the stationapi and trackIR orphan directories, purged their stale build-graph references, and de-wired ClientHeadTracking from NPClient.h as no-op stubs — SwgClient builds clean (exit 0, 0 unresolved externals) and boots to character select under both D3D9 and D3D11.**

## Performance

- **Duration:** ~50 min (incl. baseline build + 4 incremental build gates)
- **Completed:** 2026-05-25
- **Tasks:** 4 (3 auto + 1 human boot gate)
- **Files modified:** 5 source/build files; 2 directories deleted

## Accomplishments
- Pre-removal baseline build confirmed green (MSBuild `/t:SwgClient` exit 0) before any change — failures now attributable to a removal step.
- `src/external/3rd/library/stationapi/` deleted; stale `AdditionalLibraryDirectories` segments purged from SwgClient.vcxproj (×3 configs) + SwgGodClient.vcxproj; dangling `989crypt.lib` dependency dropped.
- `src/external/3rd/library/trackIR/` deleted; `trackIr/include` purged from clientGame `includePaths.rsp` + inline `AdditionalIncludeDirectories` (×3); `ClientHeadTracking.cpp` de-wired from `NPClient.h` as no-op stubs.
- SwgClient links clean: **0 unresolved external symbols** (verified, not just exit-0).
- Dual-renderer boot gate PASSED — character select under `rasterMajor=5` (D3D9) and `=11` (D3D11); no regression.

## Task Commits

1. **Task 1: Pre-removal build baseline** — no commit (read-only; MSBuild exit 0 recorded). Resolved MSBuild: `D:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`.
2. **Task 2: Remove stationapi** — `adda94729` (chore)
3. **Task 3: Remove trackIR + stub ClientHeadTracking** — `7a7da726d` (chore)
4. **Task 4: Dual-renderer boot gate** — human-verified PASS (operator-run; no tree commit beyond `stage/client_d.cfg` rasterMajor toggle).

## Files Created/Modified
- `SwgClient.vcxproj` — removed stationapi lib-search-path (×3) + dangling `989crypt.lib` dependency (Debug+Release)
- `SwgGodClient.vcxproj` — removed stationapi lib-search-path (opportunistic out-of-closure cleanup)
- `clientGame.vcxproj` — removed `trackIr\include` from inline `AdditionalIncludeDirectories` (×3); ClCompile/ClInclude for ClientHeadTracking retained (file stays in build)
- `clientGame/build/win32/includePaths.rsp` — removed `trackIr/include` line
- `ClientHeadTracking.cpp` — de-wired from NPClient.h; 4 methods + install() reduced to no-op stubs

## Decisions Made
- **989crypt.lib dropped from SwgClient deps:** it physically lived only in `stationapi/` and was still listed in `AdditionalDependencies` (Debug+Release). Removing the search path orphaned it (LNK1181). Dropping the dep relinks clean → it was unreferenced dead weight. `rdp.lib`/`dbgutil.lib` were unaffected (also vendored in `soePlatform/libs`, which precedes stationapi in search order).
- **ClientHeadTracking.cpp kept in build, stubbed:** preserves the public symbols the live callers link against, while removing the trackIR dependency. Head tracking reports unsupported — the plan's own stated expected runtime behavior.

## Deviations from Plan

### Auto-fixed Issues

**1. [Blocking — plan defect] stationapi was a live link dependency, not a pure orphan**
- **Found during:** Task 2 (Remove stationapi) — build gate
- **Issue:** Plan asserted stationapi was "never linked." But `989crypt.lib`, vendored only inside `stationapi/`, was an active `AdditionalDependencies` entry. Removing the search path + deleting the dir → `LNK1181: cannot open input file '989crypt.lib'`.
- **Fix:** Removed the dangling `989crypt.lib` from SwgClient.vcxproj `AdditionalDependencies` (Debug+Release). Relink clean (exit 0, 0 unresolved), proving 989crypt was unreferenced.
- **Files modified:** SwgClient.vcxproj
- **Verification:** `MSBuild /t:SwgClient` exit 0; 0 unresolved externals
- **Committed in:** adda94729

**2. [Blocking — plan defect masked by /FORCE] dropping ClientHeadTracking.cpp from the build orphaned its callers' symbols**
- **Found during:** Task 3 (Remove trackIR) — link analysis
- **Issue:** Plan called for excluding `ClientHeadTracking.cpp` from the build and leaving its 3 callers (CockpitCamera, SetupClientGame, SwgCuiOptControls) untouched. Those callers link against the static methods whose definitions live in that `.cpp` → `LNK2019/LNK2001`. The link returned **exit 0 only because the project links under `/FORCE`** (downgrades unresolved externals to warnings, still emits a binary). A false-pass build gate hiding a latent runtime crash (`CockpitCamera::alter` runs every frame in a cockpit; the Controls options page constructs `SwgCuiOptControls`).
- **Fix:** Kept `ClientHeadTracking.cpp` in the build but compiled out its `NPClient.h` dependency — dropped the include + the NaturalPoint registry/LoadLibrary/poll logic, reduced the 4 public methods + `install()` to safe no-op stubs (`isSupported`/`getEnabled` → false). Same no-op-stub pattern the plan uses for lcdui in 12-03. `ClientHeadTracking.h` unchanged.
- **Files modified:** ClientHeadTracking.cpp; clientGame.vcxproj (ClCompile/ClInclude retained); also removed the inline `trackIr\include` AdditionalIncludeDirectories the plan missed (it only knew about includePaths.rsp).
- **Verification:** `MSBuild /t:SwgClient` exit 0; **0 unresolved external symbols** (vs the prior false-pass which had the ClientHeadTracking LNK2019s)
- **Committed in:** 7a7da726d

---

**Total deviations:** 2 (both blocking plan defects caught by the build gate + link analysis)
**Impact on plan:** Both fixes were necessary for a correct, crash-free removal and stay within the plan's intent (remove the dead orphans; client builds + boots). The ClientHeadTracking source stub deviates from the plan's "do not modify" instruction — this was reviewed and accepted by the operator at the boot gate. No scope creep beyond completing the removal correctly.

## Issues Encountered
- The build's `/FORCE` linker flag (a stlport ABI-compat artifact) makes unresolved externals non-fatal — so MSBuild exit 0 is NOT sufficient to prove a clean link. Going forward, build gates for removal steps are validated by grepping the link output for `unresolved external symbol` (must be 0), not just the exit code.

## User Setup Required
None.

## Next Phase Readiness
- Dual-renderer boot baseline re-established → Wave 2 (Plan 12-02, SwgClientSetup) is unblocked.
- swg.sln untouched by this plan — the serialized swg.sln edits (12-02 then 12-03) are clear to begin.
- Koogie's uncommitted Direct3d9.cpp + 3 .vcxproj working-tree changes left untouched.

---
*Phase: 12-orphaned-directory-project-deletes*
*Completed: 2026-05-25*
