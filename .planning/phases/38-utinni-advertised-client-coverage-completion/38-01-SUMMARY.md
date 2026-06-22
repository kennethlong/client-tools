---
phase: 38-utinni-advertised-client-coverage-completion
plan: 01
subsystem: infra
tags: [utinni, getenginehookpoints, msvc-abi, thunk, fastcall, multiple-inheritance, groundscene, advertisement, win32]

# Dependency graph
requires:
  - phase: 37-utinni-engine-entry-point-advertisement-getenginehookpoints
    provides: "GetEngineHookPoints export + 78-row table + X-macro .inc + count static_assert + runtime utinni_verifyNoNullNoDup() self-check + the __fastcall ctor-thunk and in-TU forwarder precedents"
provides:
  - "8 advertised groundScene::* rows (3 public MI __fastcall thunks + 1 placement-new ctor thunk + 4 PRIVATE in-TU GroundScene.cpp forwarders)"
  - "exe-local utinni_groundScene_forward.h declaring the 4 private-method forwarders"
  - "friend-declaration mechanism in GroundScene.h granting the in-TU forwarders private access (ABI-neutral)"
affects: [38-02, 38-03, 38-04, utinni]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "MI-class private method -> __fastcall(GroundScene*, int /*edx*/, args) forwarder DEFINED in the class's own .cpp + friend-declared in the class header (same-TU alone is NOT enough; C2248 requires friendship)"
    - "MI-class public method/ctor -> free __fastcall thunk in utinni_advertise.cpp (never pmfToVoid on an MI class)"

key-files:
  created:
    - src/game/client/application/SwgClient/src/win32/utinni_groundScene_forward.h
  modified:
    - src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp
    - src/engine/client/library/clientGame/src/shared/scene/GroundScene.h
    - src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp
    - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc

key-decisions:
  - "groundScene private methods reached via in-TU forwarders that REQUIRE friend declarations in GroundScene.h (RESEARCH/PATTERNS claimed same-TU suffices; C++ access rules say otherwise — C2248). Friend decls are ABI-neutral (no data member/vtable change), so no plugin cascade."
  - "groundScene::draw SKIPPED (virtual, vtable-resolved); groundScene::g_instance OMITTED (Game::getScene inline, ms_scene private) — flagged for the EPA-08 handback."
  - "UTINNI_HOOKPOINTS_VERSION left at 1 (38-03 owns the single 1->2 bump per the plan)."

patterns-established:
  - "Pattern: MI-class PRIVATE method advertisement = __fastcall forwarder in the defining .cpp + a Win32-guarded friend declaration in the class header (the in-TU forwarder is BOTH member-accessible AND ABI-correct for the MI class in one function)."

requirements-completed: [EPA-05]

# Metrics
duration: 10min
completed: 2026-06-22
---

# Phase 38 Plan 01: groundScene Coverage Summary

**8 source-confirmed groundScene::* rows added to GetEngineHookPoints — 3 public MI __fastcall thunks + 1 placement-new ctor thunk in utinni_advertise.cpp, plus 4 PRIVATE init/update/handleInputMap* __fastcall forwarders compiled in GroundScene.cpp (friend-granted private access), draw SKIP / g_instance OMIT — Debug+Release link 0 unresolved, undecorated export on both exes, count static_assert holds.**

## Performance

- **Duration:** ~10 min
- **Started:** 2026-06-22T20:53:54Z
- **Completed:** 2026-06-22T21:05:00Z
- **Tasks:** 2 auto + 1 checkpoint (boot-smoke DEFERRED per execution-pacing decision)
- **Files modified:** 5 (1 created, 4 modified)

## Accomplishments

- 4 PRIVATE GroundScene methods (`init`, `update`, `handleInputMapUpdate`, `handleInputMapEvent`) advertised via `__fastcall(GroundScene*, int /*edx*/, args)` forwarders defined inside GroundScene.cpp (member access) and declared in a new exe-local header.
- 3 PUBLIC GroundScene methods (`reloadTerrain`, `changeCamera`->`setView`, `getCurrentCamera`) advertised via free `__fastcall` MI thunks in utinni_advertise.cpp (no `pmfToVoid` on the MI class).
- `groundScene::ctor` advertised via a placement-new `__fastcall` thunk over the `(const char*, const char*, CreatureObject*)` overload [GroundScene.h:199].
- `groundScene::draw` SKIPPED (virtual) and `groundScene::g_instance` OMITTED (inline getter / private member) — both kept out of the `.inc` required set; g_instance flagged for the EPA-08 handback.
- 8 matching `UTINNI_HOOKPOINT(groundScene, *)` names added to the `.inc` in lockstep; count `static_assert` holds (table +8 == `.inc` +8).

## Task Commits

1. **Task 1: groundScene private-method forwarders + exe-local header** - `abc2eed5d` (feat)
2. **Task 2: 8 groundScene rows (MI thunks + ctor + forwarders) + .inc** - `b99515648` (feat)
3. **Task 3: boot-smoke checkpoint** - DEFERRED (see Build-Gate Evidence + Deviations)

**Plan metadata:** (this SUMMARY + STATE/ROADMAP) committed separately.

## Files Created/Modified

- `src/game/client/application/SwgClient/src/win32/utinni_groundScene_forward.h` (NEW) - extern declarations of the 4 `__fastcall` groundScene private-method forwarders; forward-decls only, no heavy engine headers; included ONLY by utinni_advertise.cpp (exe-local, no plugin cascade).
- `src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp` - 4 in-TU `__fastcall` private-method forwarders under `#if !defined(_WIN64)`.
- `src/engine/client/library/clientGame/src/shared/scene/GroundScene.h` - 4 Win32-guarded `friend` declarations granting the in-TU forwarders private access (ABI-neutral).
- `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` - 3 public MI thunks + 1 ctor thunk; 8 table rows; `#include` of GroundScene.h + utinni_groundScene_forward.h; SKIP/OMIT comments.
- `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc` - 8 new `UTINNI_HOOKPOINT(groundScene, *)` names.

## Build-Gate Evidence (AUTONOMOUS pass condition for this wave)

- **Debug** (SwgClient.vcxproj /p:Configuration=Debug /p:Platform=Win32, `/nodeReuse:false`, serial, staged exe deleted to force relink): `unresolved external symbol` = **0**, LNK1181 = 0, LNK1120 = 0, `error C` = 0. Build succeeded; staged to `stage/SwgClient_d.exe`.
- **Release** (same flags, Configuration=Release; clientGame.lib Release rebuilt first — see Deviation #2): `unresolved external symbol` = **0**, LNK1181 = 0, LNK1120 = 0, `error C` = 0. Build succeeded; staged to `stage/SwgClient_r.exe`.
- **dumpbin /exports** (VS18 BuildTools x64 dumpbin): `SwgClient_d.exe` -> `GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ordinal 52); `SwgClient_r.exe` -> `GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ordinal 51).
- **static_assert** `(sizeof s_engineHookPoints / sizeof[0]) == UTINNI_REQUIRED_COUNT`: held — the Debug AND Release builds compiled (table gained exactly 8 rows, `.inc` gained exactly 8 names).
- **Grep checks:** exactly 8 `{ "groundScene::` rows in advertise.cpp; 0 `pmfToVoid(&GroundScene::` table rows (the 2 grep hits are comment warnings); exactly 8 `UTINNI_HOOKPOINT(groundScene` lines in the `.inc`; 0 `draw`/`g_instance` groundScene lines in the `.inc`; the forwarder header is `#include`d only by advertise.cpp.

**Boot-smoke (Task 3 checkpoint:human-verify):** DEFERRED to the final consolidated maintainer pass (per execution-pacing decision for this run). The gl05 char-select boot, the gl11 window-up non-regression, and the runtime `utinni_verifyNoNullNoDup()` PASS were NOT run and are NOT claimed here. The build-gate above is the autonomous pass condition.

## Decisions Made

- **Private-method access requires `friend`, not just same-TU.** See Deviation #1 — the plan/RESEARCH/PATTERNS asserted that a free function compiled in GroundScene.cpp would have member access; C++ access rules grant private access only to members/friends, so the build hit C2248. Resolved with ABI-neutral `friend` declarations.
- **`groundScene::g_instance` OMITTED, flagged for EPA-08.** No dedicated GroundScene singleton; reached via inline `Game::getScene()` (no ODR address) cast to GroundScene, and `Game::ms_scene` is private. Recorded in the .cpp OMIT comment for the Utinni handback — if the groundScene editor strictly needs the raw singleton pointer, a non-inline `Game` accessor is the cheap follow-up.
- **Version bump deferred.** `UTINNI_HOOKPOINTS_VERSION` left at 1; 38-03 owns the single 1->2 bump (per the plan and prompt).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added `friend` declarations to GroundScene.h for the private-method forwarders**
- **Found during:** Task 1 (GroundScene.cpp private-method forwarders)
- **Issue:** The plan, 38-RESEARCH, and 38-PATTERNS all assert that a free function "compiled in the same TU as the class definition" has access to the class's private members. This is incorrect for C++: same-TU grants no access; only members and friends may name private members. The clientGame.lib Debug build failed with 4x `C2248: cannot access private member` at the four forwarder definitions.
- **Fix:** Added 4 `friend void __fastcall utinni_groundScene*(...)` declarations to GroundScene.h, guarded by `#if !defined(_WIN64)`. Friend declarations add no data member and no virtual, so the GroundScene struct/vtable ABI is byte-identical — no shared-header plugin cascade (verified: only the forwarder access changed; clientGame.lib rebuilt clean, SwgClient relinked clean in both Debug and Release).
- **Files modified:** src/engine/client/library/clientGame/src/shared/scene/GroundScene.h
- **Verification:** clientGame.lib Debug + Release rebuilt with 0 C2248/C3865/C2065; SwgClient Debug + Release linked 0 unresolved.
- **Committed in:** abc2eed5d (Task 1 commit)

**2. [Rule 3 - Blocking] Rebuilt clientGame.lib Release before relinking SwgClient Release**
- **Found during:** Task 2 / Task 3 build-gate (Release link)
- **Issue:** The first SwgClient Release link failed `LNK1120: 4 unresolved externals` (the 4 `utinni_groundScene*` forwarders) because the Release `clientGame.lib` had not been rebuilt — it still lacked the new forwarder symbols. (The Debug link had succeeded only because clientGame Debug was rebuilt during Task 1.)
- **Fix:** Built `clientGame.vcxproj` Release|Win32 to emit the forwarders into the Release lib, then deleted the staged Release exe and relinked SwgClient Release.
- **Files modified:** none (build sequencing only)
- **Verification:** Release link 0 unresolved; dumpbin undecorated export confirmed on `SwgClient_r.exe`.
- **Committed in:** n/a (no source change — build artifact only; the forwarder source is in abc2eed5d)

---

**Total deviations:** 2 auto-fixed (both Rule 3 - blocking)
**Impact on plan:** Deviation #1 corrects a factual error carried through the plan/RESEARCH/PATTERNS about C++ private-member access and is essential for the feature to compile; the friend-declaration approach is ABI-neutral so it introduces no plugin-cascade risk. Deviation #2 is a build-ordering correction (engine lib must be rebuilt before the exe relink for a multi-config build). No scope creep; no new rows beyond the planned 8.

## Issues Encountered

- Build-log encoding is UTF-16LE (BOM `fffe`) under PowerShell `Tee-Object`; plain bash `grep` silently missed the LNK lines. Decoded with `iconv -f UTF-16LE` to read the link-gate results. (Operational note for future executors auditing build logs on this machine.)

## Threat Flags

None — no new security-relevant surface beyond the read-only, additive export already covered by the plan's threat model (T-38-01..03). All 8 rows are source-confirmed (GroundScene.h file:line in each row comment), MI methods use thunks (never pmfToVoid), private methods use in-TU forwarders, draw is a virtual SKIP, g_instance an OMIT — the T-38-01 `mitigate` disposition is satisfied.

## Next Phase Readiness

- groundScene coverage complete; ready for 38-02 (client/config external-linkage shims + WR-05 commit).
- **Cross-cutting note for 38-02/38-03:** the same C2248 friend-access correction (Deviation #1) applies to ANY future private-member forwarder claimed to work via "same-TU" — author the forwarder in the defining .cpp AND friend-declare it (or confirm the target is genuinely file-local/namespace-scope, which is the actual mechanism the ClientMain.cpp precedent uses, NOT private-member access).
- **Handback item for EPA-08:** `groundScene::g_instance` OMITTED — recorded in utinni_advertise.cpp. If Utinni's groundScene editor needs the singleton pointer, add a non-inline `Game` accessor.
- **38-03 owns** the `UTINNI_HOOKPOINTS_VERSION` 1->2 bump and the byte-exact `.inc`/`.h` re-sync to `D:/Code/Utinni`.

## Self-Check: PASSED

- FOUND: src/game/client/application/SwgClient/src/win32/utinni_groundScene_forward.h
- FOUND: .planning/phases/38-utinni-advertised-client-coverage-completion/38-01-SUMMARY.md
- FOUND commit abc2eed5d (Task 1)
- FOUND commit b99515648 (Task 2)

---
*Phase: 38-utinni-advertised-client-coverage-completion*
*Completed: 2026-06-22*
