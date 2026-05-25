---
phase: 03-client-engine-libraries-sdk-heavy-tier
plan: 04
subsystem: build-system
tags: [cmake, client-engine, tier8, tier9, dpvs, vivox, logitechlcd, libmozilla, xpcom-stub, clientSkeletalAnimation, clientTerrain, clientUserInterface]

# Dependency graph
requires:
  - phase: 03-03
    provides: "Tier 7 HIGH RISK libs: clientGraphics.lib + clientAudio.lib (Debug+Release); clientTextureRenderer + clientParticle unblocked; 8/13 Phase 3 libs building"
  - phase: 02
    provides: "Phase 2 STATIC targets (sharedFoundation, sharedGame, sharedObject, sharedTerrain, etc.)"
provides:
  - "clientSkeletalAnimation STATIC target (.lib in Debug and Release) — DPVS + sharedXml + boost + clientTextureRenderer + include/private"
  - "clientTerrain STATIC target (.lib in Debug and Release) — DPVS + sharedImage + sharedThread + clientGame include chain"
  - "clientUserInterface STATIC target (.lib in Debug and Release) — XPCOM stub gate (D-07 PASS): zero xpcom/xul symbols"
  - "LAUNCH-02 satisfied: libMozilla stub is the ONLY Mozilla linkage in clientUserInterface"
  - "Cumulative Phase 3 .lib count: 11 of 13 (all client libs except clientGame)"
affects:
  - 03-05-PLAN

# Tech tracking
tech-stack:
  added:
    - "clientSkeletalAnimation STATIC (102 cpp/h across 9 subdirs; DPVS + sharedXml + boost + clientTextureRenderer + include/private paths)"
    - "clientTerrain STATIC (37 cpp/h across 9 subdirs; DPVS + sharedImage + sharedSynchronization + sharedThread + clientGame header chain)"
    - "clientUserInterface STATIC (152 cpp/h across 4 subdirs; Vivox + Logitech LCD + libMozilla stub; DX9 + ui include Pitfall 4)"
    - "typeinfo.h shim at build/stlport453/include/typeinfo.h — redirects old-style <typeinfo.h> to <typeinfo> for MSVC 2022 compatibility"
  patterns:
    - "include/private pattern confirmed for clientSkeletalAnimation (ShowAttachedObjectAction.h, ShowAttachedObjectActionTemplate.h)"
    - "clientGame headers pulled transitively by clientTerrain (GroundEnvironment.cpp includes clientGame/ClientRegionEffectManager.h)"
    - "boost include needed for clientSkeletalAnimation and clientTerrain transitively via clientGame/sharedGame chain"
    - "localization/include + unicode/include + localizationArchive + unicodeArchive needed by clientTerrain via clientGame headers"
    - "clientUserInterface uses both ${SWG_EXTERNALS_FIND}/ui/include AND ${SWG_EXTERNALS_FIND}/ui/src/win32 (Pitfall 4)"
    - "D-07 gate confirmed: no XPCOM symbols in clientUserInterface.lib — libMozilla stub is sole Mozilla linkage"
    - "swgSharedUtility/Attributes.def pulled by clientGame/ClientObject.h chain in clientUserInterface"
    - "libEverQuestTCG include needed by clientUserInterface (CuiManager.cpp)"

key-files:
  created:
    - src/engine/client/library/clientSkeletalAnimation/src/CMakeLists.txt
    - src/engine/client/library/clientTerrain/src/CMakeLists.txt
    - src/engine/client/library/clientUserInterface/src/CMakeLists.txt
  modified:
    - src/engine/client/library/clientSkeletalAnimation/CMakeLists.txt
    - src/engine/client/library/clientTerrain/CMakeLists.txt
    - src/engine/client/library/clientUserInterface/CMakeLists.txt

key-decisions:
  - "clientSkeletalAnimation/include/private required: ShowAttachedObjectAction.h and ShowAttachedObjectActionTemplate.h are private headers (not in include/public)"
  - "sharedXml needed by clientSkeletalAnimation (EditableAnimationStateLink.cpp includes sharedXml/XmlTreeNode.h)"
  - "boost include needed at ${SWG_EXTERNALS_FIND}/boost (clientSkeletalAnimation via AnimationStateNameIdManager.cpp; clientTerrain via sharedGame/CraftingData.h)"
  - "clientTerrain pulls clientGame headers transitively: GroundEnvironment.cpp includes clientGame/ClientRegionEffectManager.h + PlayerObject.h chain"
  - "clientTerrain needs sharedThread (ClientProceduralTerrainAppearance.h includes sharedThread/ThreadHandle.h)"
  - "clientUserInterface typeinfo.h compat: CuiMediator.cpp uses <typeinfo.h> (old-style) which STLPort 4.5.3 redirects to ../include/typeinfo.h; that file doesn't exist in MSVC 2022; shim created at build/stlport453/include/typeinfo.h"
  - "clientUserInterface needs swgSharedUtility path for Attributes.def (game/shared library, pulled via clientGame/CreatureObject.h)"
  - "clientUserInterface needs libEverQuestTCG include (CuiManager.cpp includes libEverQuestTCG/libEverQuestTCG.h)"
  - "D-07 gate PASSED: dumpbin /symbols on clientUserInterface.lib returns empty for xpcom and xul patterns"

requirements-completed: [LAUNCH-02]

# Metrics
duration: 45min
completed: 2026-05-04
---

# Phase 3 Plan 04: Tier 8+9 Client Engine Libraries Summary

**clientSkeletalAnimation.lib + clientTerrain.lib + clientUserInterface.lib produced (Debug+Release); D-07 dumpbin gate passed (zero xpcom/xul symbols) — LAUNCH-02 satisfied with libMozilla stub as the only Mozilla linkage**

## Performance

- **Duration:** ~45 min
- **Started:** 2026-05-04T18:30:00Z
- **Completed:** 2026-05-04T19:15:00Z
- **Tasks:** 2
- **Files modified/created:** 6 (3 created, 3 modified)

## Accomplishments

- Created 3 CMakeLists.txt pairs (clientSkeletalAnimation + clientTerrain + clientUserInterface) replacing placeholder stubs
- clientSkeletalAnimation.lib produced (Debug + Release): 102 source files across 9 subdirs; DPVS headers resolve; private include path confirmed
- clientTerrain.lib produced (Debug + Release): 37 source files across 9 subdirs; clientGame header chain resolved without circular build dependency
- clientUserInterface.lib produced (Debug + Release): 152 source files across 4 subdirs; Vivox + Logitech LCD + libMozilla stub linked
- **D-07 gate PASSED**: `dumpbin /symbols build/lib/Debug/clientUserInterface.lib | Select-String xpcom` returns empty — no xpcom symbols
- **D-07 gate PASSED**: `dumpbin /symbols build/lib/Debug/clientUserInterface.lib | Select-String xul` returns empty — no xul symbols
- **LAUNCH-02 SATISFIED**: libMozilla stub (not real XPCOM) is the only Mozilla linkage in the clientUserInterface compilation unit
- Cumulative Phase 3 client lib count: 11 of 13 (all except clientGame)
- typeinfo.h compat shim added to build/stlport453/include/ — resolves STLPort 4.5.3 / MSVC 2022 issue with old-style `<typeinfo.h>` include

## Task Commits

Each task was committed atomically:

1. **Task 1: Author clientSkeletalAnimation and clientTerrain CMakeLists pairs (Tier 8)** - `b1ebf77f4` (feat)
2. **Task 2: Author clientUserInterface CMakeLists pair — XPCOM stub gate (D-07, LAUNCH-02)** - `35896d488` (feat)

## Libs Produced

| Library | Debug | Release | Notes |
|---------|-------|---------|-------|
| clientSkeletalAnimation.lib | YES | YES | DPVS + sharedXml + boost + include/private |
| clientTerrain.lib | YES | YES | DPVS + clientGame transitive chain |
| clientUserInterface.lib | YES | YES | Vivox + Logitech LCD + libMozilla stub; D-07 PASS |

## D-07 / LAUNCH-02 Gate Results

```
dumpbin /symbols build/lib/Debug/clientUserInterface.lib | Select-String xpcom
→ (empty — PASS)

dumpbin /symbols build/lib/Debug/clientUserInterface.lib | Select-String xul
→ (empty — PASS)
```

No XPCOM or XUL symbols present in clientUserInterface.lib. The libMozilla stub target (compiled from `src/cmake/stubs/libMozilla_stub.cpp`) is the sole Mozilla linkage. Real XPCOM libraries (xul.lib, xpcom.lib, nspr4.lib, plc4.lib, profdirserviceprovider_s.lib) are never referenced.

## UI Include Path Confirmation (Pitfall 4)

Both required paths are confirmed in clientUserInterface/src/CMakeLists.txt:
- `${SWG_EXTERNALS_FIND}/ui/include` — public UI headers + `_precompile.h` redirect
- `${SWG_EXTERNALS_FIND}/ui/src/win32` — `_precompile.h` target (UIStlFwd.h, UiReport.h etc.)

`FirstClientUserInterface.h` includes `_precompile.h` which resolves to `ui/include/_precompile.h`, which redirects to `../src/win32/_precompile.h`. Both paths required for this chain to resolve.

## Cumulative Phase 3 .lib Count

```
(Get-ChildItem build/lib/Debug/client*.lib).Count → 11
clientAnimation.lib, clientAudio.lib, clientBugReporting.lib, clientDirectInput.lib,
clientGraphics.lib, clientObject.lib, clientParticle.lib, clientSkeletalAnimation.lib,
clientTerrain.lib, clientTextureRenderer.lib, clientUserInterface.lib
```

11 of 13 client libs (all except clientGame). Phase 3 Plan 05 (Wave 4) will produce clientGame.lib.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] clientSkeletalAnimation/include/private path missing**
- **Found during:** Task 1 (clientSkeletalAnimation build — C1083 on ShowAttachedObjectAction.h)
- **Issue:** ShowAttachedObjectAction.h and ShowAttachedObjectActionTemplate.h are private headers in `include/private/clientSkeletalAnimation/`. Plan listed `include/public` path (from PATTERN A wrapper) but the private path was not listed.
- **Fix:** Added `${SWG_ENGINE_SOURCE_DIR}/client/library/clientSkeletalAnimation/include/private` to include_directories.
- **Commit:** b1ebf77f4

**2. [Rule 1 - Bug] sharedXml include path missing from clientSkeletalAnimation**
- **Found during:** Task 1 (C1083 on sharedXml/XmlTreeNode.h via EditableAnimationStateLink.cpp)
- **Issue:** Plan's include list did not include sharedXml. EditableAnimationState* controller files include XmlTreeNode.h and XmlTreeDocument.h directly.
- **Fix:** Added `${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedXml/include/public` to include_directories.
- **Commit:** b1ebf77f4

**3. [Rule 1 - Bug] boost include path missing from clientSkeletalAnimation**
- **Found during:** Task 1 (C1083 on boost/smart_ptr.hpp via AnimationStateNameIdManager.cpp)
- **Fix:** Added `${SWG_EXTERNALS_FIND}/boost` to include_directories.
- **Commit:** b1ebf77f4

**4. [Rule 1 - Bug] clientTextureRenderer and sharedSynchronization include paths missing from clientSkeletalAnimation**
- **Found during:** Task 1 (C1083 on clientTextureRenderer/TextureRenderer.h and boost/smart_ptr.hpp)
- **Fix:** Added clientTextureRenderer/include/public and sharedSynchronization/include/public.
- **Commit:** b1ebf77f4

**5. [Rule 1 - Bug] clientTerrain missing sharedImage, sharedSynchronization, sharedThread, sharedRandom paths**
- **Found during:** Task 1 (C1083 on sharedImage/Image.h, sharedSynchronization/Gate.h, sharedThread/ThreadHandle.h)
- **Fix:** Added all three include paths to clientTerrain/src/CMakeLists.txt.
- **Commit:** b1ebf77f4

**6. [Rule 1 - Bug] clientTerrain pulls clientGame headers (GroundEnvironment.cpp)**
- **Found during:** Task 1 (C1083 on clientGame/ClientRegionEffectManager.h, then StringId.h, localizationArchive, boost, sharedGame, sharedNetwork, sharedNetworkMessages)
- **Issue:** `GroundEnvironment.cpp` directly includes `clientGame/ClientRegionEffectManager.h`, `clientGame/Game.h`, `clientGame/GuildObject.h`, `clientGame/PlayerObject.h`. These pull a deep transitive header chain including sharedGame, boost, localization libs, swgSharedUtility.
- **Fix:** Added clientGame/include/public, clientAnimation/include/public, clientSkeletalAnimation/include/public, sharedGame/include/public, sharedNetwork/include/public, sharedNetworkMessages/include/public, localization/include, localizationArchive/include/public, unicode/include, unicodeArchive/include/public, boost to clientTerrain includes.
- **Commit:** b1ebf77f4

**7. [Rule 1 - Bug] clientUserInterface typeinfo.h compatibility shim**
- **Found during:** Task 2 (C1083: stlport453/stlport/typeinfo.h cannot open ../include/typeinfo.h)
- **Issue:** CuiMediator.cpp includes `<typeinfo.h>` (old-style). STLPort 4.5.3 intercepts and tries to include `_STLP_NATIVE_CPP_RUNTIME_HEADER(typeinfo.h)` which expands to `<../include/typeinfo.h>`. MSVC 2022 has `typeinfo` (no .h) but not `typeinfo.h`. The file `build/stlport453/include/typeinfo.h` did not exist.
- **Fix:** Created `build/stlport453/include/typeinfo.h` — a one-line shim that includes `<typeinfo>`. This is a build-infrastructure file (not a C++ source edit).
- **Commit:** 35896d488

**8. [Rule 1 - Bug] Multiple missing include paths in clientUserInterface (clientAudio, clientDirectInput, clientParticle, sharedCommandParser, sharedIoWin, sharedInputMap, sharedThread, sharedMathArchive, sharedMessageDispatch, sharedSkillSystem, singleton, swgSharedUtility, libEverQuestTCG, clientBugReporting, clientTerrain)**
- **Found during:** Task 2 (multiple C1083 errors from deep clientGame transitive header chain)
- **Issue:** clientUserInterface includes many client-game headers that pull a very wide transitive dependency set.
- **Fix:** Added all missing include paths iteratively to clientUserInterface/src/CMakeLists.txt.
- **Commit:** 35896d488

---

**Total deviations:** 8 auto-fixed (all Rule 1 bugs — missing include paths)
**Impact on plan:** All auto-fixes necessary for correctness. All three libs build successfully. XPCOM gate clean. No scope creep.

## Issues Encountered

- STLPort 4.5.3 `<typeinfo.h>` redirect to `../include/typeinfo.h` fails under MSVC 2022 — required a typeinfo.h shim in build/stlport453/include/. This affects all Phase 3 libs that include `<typeinfo.h>` directly (clientUserInterface has 2 files). The shim is build-infrastructure, not a C++ source edit.

## Known Stubs

None — all planned functionality is wired. libMozilla stub is intentional (LAUNCH-02 requirement confirmed; dumpbin gate passed).

## Threat Flags

No new security-relevant surface introduced. clientSkeletalAnimation, clientTerrain, and clientUserInterface are static libraries. No network endpoints, auth paths, or file access patterns added. The D-07 dumpbin gate confirms no real XPCOM linkage escaped into clientUserInterface.

## Next Phase Readiness

- clientGame.lib (Phase 3 Plan 05, Wave 4) is now the only remaining Phase 3 target
- All include paths for clientGame are expected to mirror clientUserInterface + clientSkeletalAnimation + clientTerrain patterns (established in this plan)
- 11 of 13 client libs ready as link-time dependencies for clientGame

## Self-Check: PASSED

- `src/engine/client/library/clientSkeletalAnimation/CMakeLists.txt` — EXISTS (MODIFIED)
- `src/engine/client/library/clientSkeletalAnimation/src/CMakeLists.txt` — EXISTS (CREATED)
- `src/engine/client/library/clientTerrain/CMakeLists.txt` — EXISTS (MODIFIED)
- `src/engine/client/library/clientTerrain/src/CMakeLists.txt` — EXISTS (CREATED)
- `src/engine/client/library/clientUserInterface/CMakeLists.txt` — EXISTS (MODIFIED)
- `src/engine/client/library/clientUserInterface/src/CMakeLists.txt` — EXISTS (CREATED)
- `build/lib/Debug/clientSkeletalAnimation.lib` — EXISTS
- `build/lib/Debug/clientTerrain.lib` — EXISTS
- `build/lib/Debug/clientUserInterface.lib` — EXISTS
- `build/lib/Release/clientSkeletalAnimation.lib` — EXISTS
- `build/lib/Release/clientTerrain.lib` — EXISTS
- `build/lib/Release/clientUserInterface.lib` — EXISTS
- Commit b1ebf77f4 — EXISTS (Task 1)
- Commit 35896d488 — EXISTS (Task 2)
- D-07 xpcom gate: PASS (empty)
- D-07 xul gate: PASS (empty)
- Cumulative client .lib count: 11 of 13

---
*Phase: 03-client-engine-libraries-sdk-heavy-tier*
*Completed: 2026-05-04*
