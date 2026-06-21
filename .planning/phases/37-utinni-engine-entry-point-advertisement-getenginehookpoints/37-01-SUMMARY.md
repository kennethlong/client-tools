---
phase: 37-utinni-engine-entry-point-advertisement-getenginehookpoints
plan: 01
subsystem: infra
tags: [msvc, dllexport, dumpbin, pmf, bit_cast, x-macro, utinni, hookpoints, 32-bit]

# Dependency graph
requires:
  - phase: 34-utinni-dx11-hookpoint-advertisement (shipped graphics twin)
    provides: "gl11_r.dll!GetHookPoints undecorated-export pattern (Direct3d11.cpp:856-888) mirrored for the exe side"
provides:
  - "SwgClient_d.exe exports undecorated GetEngineHookPoints() returning a name->pointer table"
  - "shared/utinni_engine_hookpoints.h + .inc contract (structs + UTINNI_HOOKPOINTS_VERSION + X-macro name list), exe-local, no provider dllexport"
  - "pmfToVoid std::bit_cast PMF->void* helper under sizeof guard (37-02/37-03 inherit)"
  - "EPA-02 crash-fixer thunk: config::loadOverrideConfig -> utinni_loadOverrideConfig -> utinni_installConfigFileOverride -> installConfigFileOverride"
  - "coverage self-check: NO-sentinel count static_assert (drift smoke) + X-macro s_requiredNames[] + runtime utinni_verifyNoNullNoDup() name-set-equality/no-null/no-dup, wired into Debug boot"
  - "SwgClient.vcxproj wired Win32-only (ClCompile Condition Platform=Win32 + #if !defined(_WIN64) source guard) -- no x64 export surface"
affects: [37-02-mvp-catalog, 37-03-full-catalog, utinni-consumer-coordination]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Undecorated exe export via __declspec(dllexport) on extern \"C\" __cdecl alone (no .def/.pragma/ModuleDefinitionFile)"
    - "PMF -> void* via std::bit_cast<void*> under static_assert(sizeof(PMF)==sizeof(void*)) -- NOT a union type-pun"
    - "X-macro .inc single-source-of-truth: same file expands to table count smoke + required-name set; hand-written heterogeneous table body bridged by a runtime name-set-equality self-check"
    - "Distinctly-named external-linkage forwarding shim to expose a file-local namespaced fn without colliding under using-namespace"
    - "32-bit-only scope belt-and-suspenders: vcxproj ClCompile Condition Platform=Win32 + #if !defined(_WIN64) source guard"

key-files:
  created:
    - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h
    - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc
    - src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp
  modified:
    - src/game/client/application/SwgClient/src/win32/ClientMain.cpp
    - src/game/client/application/SwgClient/src/win32/ClientMain.h
    - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj

key-decisions:
  - "Debug-boot self-check gated on PRODUCTION == 0 (the codebase macro per Production.h), NOT the plan's literal _PRODUCTION (never defined here; would wrongly run in production) -- Rule 1 correction"
  - "sharedCommandParser/include/public was NOT on SwgClient's include path (the 37-PATTERNS 'every include\\public dir is wired' claim was inaccurate); added it to all 5 client config blocks -- Rule 3 fix"
  - "config::loadOverrideConfig maps to the installConfigFileOverride thunk (zero-arg orchestrator), NOT ConfigFile::loadFromBuffer (the inner call) -- EPA-02 RESEARCH correction honored"

patterns-established:
  - "Pattern: exe-side Utinni hookpoint advertisement mirrors the shipped gl11 graphics GetHookPoints"
  - "Pattern: coverage gate = compile-time count smoke + runtime name-set equality vs X-macro-generated required set"

requirements-completed: [EPA-02]

# Metrics
duration: 10min
completed: 2026-06-21
---

# Phase 37 Plan 01: Utinni Engine Entry-Point Advertisement Spike Summary

**SwgClient_d.exe now exports an undecorated GetEngineHookPoints() returning a 3-row name->pointer table (EPA-02 crash-fixer thunk + static fn + std::bit_cast member PMF), with an X-macro coverage self-check, proving the full mechanism end-to-end so 37-02/37-03 can scale the catalog.**

## Performance

- **Duration:** ~10 min
- **Started:** 2026-06-21T20:45:48Z
- **Completed:** 2026-06-21T20:55:57Z
- **Tasks:** 3
- **Files modified:** 6 (3 created, 3 modified)

## Accomplishments
- `GetEngineHookPoints` exported **undecorated** from `SwgClient_d.exe` (dumpbin-confirmed: `GetEngineHookPoints = _GetEngineHookPoints`, no `@0` suffix, no `?`-mangling) via `__declspec(dllexport)` on `extern "C" __cdecl` alone -- no .def, no /EXPORT pragma, matching the shipped gl11 twin.
- The EPA-02 crash-fixer row resolves: `config::loadOverrideConfig` -> `&utinni_loadOverrideConfig` (a zero-arg `int __cdecl` thunk wrapping the distinctly-named `utinni_installConfigFileOverride()` shim -> `ClientMainNamespace::installConfigFileOverride()`), the RESEARCH-corrected mapping (NOT `ConfigFile::loadFromBuffer`).
- The four hard spike parts are proven and inheritable by 37-02/37-03: (a) forced undecorated exe export, (b) PMF->void* via `std::bit_cast` under `static_assert(sizeof(PMF)==sizeof(void*))`, (c) the file-local crash-fixer made linkable via an additive distinctly-named shim, (d) the coverage self-check (NO-sentinel count `static_assert` drift smoke + X-macro `s_requiredNames[]` + runtime `utinni_verifyNoNullNoDup()` name-set-equality/no-null/no-dup, wired into Debug boot).
- 32-bit-only scope enforced two ways: vcxproj `ClCompile ... Condition="'$(Platform)'=='Win32'"` + a `#if !defined(_WIN64)` source guard wrapping the whole TU body -- zero x64 export surface.
- SwgClient Debug Win32 links clean under /FORCE (0 `unresolved external symbol`, 0 LNK1181, 0 LNK2019/2001, 0 compile errors); client boots to the login/character-select window under `rasterMajor=5` (gl05/D3D9) with no crash dump.

## Task Commits

1. **Task 1: shared contract header + .inc + crash-fixer forwarding shim** - `f50278c65` (feat)
2. **Task 2: author utinni_advertise.cpp (export + table + PMF helper + thunk + coverage self-check) + wire vcxproj Win32-only** - `0cd4139c1` (feat)
3. **Task 3: build/link-grep/dumpbin/boot — sharedCommandParser include-dir fix** - `9dac8aad6` (fix)

**Plan metadata:** _(this commit)_

## Files Created/Modified
- `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h` - `UtinniEngineHookPoint`/`UtinniEngineHookPoints` structs + `UTINNI_HOOKPOINTS_VERSION` (pinned 1); structs-only, no engine includes, NO provider dllexport (shared verbatim with D:/Code/Utinni).
- `src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.inc` - X-macro canonical name list, 3 spike rows (config/game/commandParser); the single source of truth for the advertised name set.
- `src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp` - `GetEngineHookPoints` export + `s_engineHookPoints[]` table + `pmfToVoid` helper + crash-fixer thunk + the coverage self-check; whole body under `#if !defined(_WIN64)`.
- `src/game/client/application/SwgClient/src/win32/ClientMain.cpp` - additive `utinni_installConfigFileOverride()` forwarding shim + the Debug-boot `utinni_verifyNoNullNoDup()` call (`#if PRODUCTION == 0 && !defined(_WIN64)`).
- `src/game/client/application/SwgClient/src/win32/ClientMain.h` - external-linkage decls for the shim + `utinni_verifyNoNullNoDup()`.
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` - ClCompile `utinni_advertise.cpp` (Win32-only) + ClInclude .h/.inc + `sharedCommandParser/include/public` added to the include dirs.

## Decisions Made
- **Debug-boot gate macro:** the codebase uses `PRODUCTION` (from `Production.h`: `DEBUG_LEVEL==0` => `PRODUCTION 1`), not the plan's literal `_PRODUCTION` (which is defined nowhere here and would have made the self-check run in production builds too). Used `#if PRODUCTION == 0 && !defined(_WIN64)`.
- **config::loadOverrideConfig mapping:** wrapped `installConfigFileOverride()` via the thunk, not `ConfigFile::loadFromBuffer` (the inner call). Honors the EPA-02 RESEARCH correction.
- **Shim naming:** used the distinctly-named `utinni_installConfigFileOverride()` (external linkage) rather than promoting the namespaced original or adding a same-named global, which would collide with the unqualified `installConfigFileOverride()` callers under `using namespace ClientMainNamespace;` (ClientMain.cpp:238).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Debug-boot self-check macro: `_PRODUCTION` -> `PRODUCTION == 0`**
- **Found during:** Task 2 (wiring the boot-path call)
- **Issue:** The plan specifies `#if !defined(_PRODUCTION)`. `_PRODUCTION` is never defined in this codebase (the macro is `PRODUCTION` from `sharedFoundation/.../Production.h`), so `!defined(_PRODUCTION)` is ALWAYS true -- the self-check would run in Release/Production boots, contradicting the plan's "wired into Debug boot" intent.
- **Fix:** Gated the call `#if PRODUCTION == 0 && !defined(_WIN64)` (Debug-only + 32-bit-only, matching the provider TU's `#if !defined(_WIN64)` guard).
- **Files modified:** `src/game/client/application/SwgClient/src/win32/ClientMain.cpp`
- **Verification:** Builds clean in Debug; the verify call is present (ClientMain.cpp:259) and compiled in (no x64/Release surface).
- **Committed in:** `0cd4139c1` (Task 2 commit)

**2. [Rule 3 - Blocking] `sharedCommandParser/include/public` missing from SwgClient include dirs**
- **Found during:** Task 3 (first build)
- **Issue:** `#include "sharedCommandParser/CommandParser.h"` failed `C1083: Cannot open include file`. The 37-PATTERNS / interfaces claim that "the SwgClient include path already has every engine include\public dir" was inaccurate -- `clientGame`, `sharedFoundation`, etc. are wired, but `sharedCommandParser` was not (even though `sharedCommandParser.lib` is in the link closure). A deep relative-path include also failed because the public forwarder's internal `../../src/shared/` resolves include-dir-relative.
- **Fix:** Added `..\..\..\..\..\..\engine\shared\library\sharedCommandParser\include\public` to all 5 SwgClient client config blocks (before `sharedCompression`). Reverted the TU back to the clean `"sharedCommandParser/CommandParser.h"` form (net-zero diff on the .cpp).
- **Files modified:** `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj`
- **Verification:** Build links clean (0 unresolved external symbol); harmless on x64 (the TU is Win32-conditioned + `#if !defined(_WIN64)` guarded).
- **Committed in:** `9dac8aad6` (Task 3 commit)

---

**Total deviations:** 2 auto-fixed (1 bug, 1 blocking)
**Impact on plan:** Both auto-fixes were necessary for correctness/build. No scope creep; the spike scope (3 rows + scaffolding) is unchanged.

## Issues Encountered
- Building the `.vcxproj` directly with `/t:SwgClient` errors (`MSB4057: target does not exist`) -- `SwgClient` is a solution-level target. Resolved by building against `src/build/win32/swg.sln` with `/t:SwgClient` per AGENTS.md canonical form.

## Known Stubs
None. The 3-row table is a deliberately representative spike set (the plan explicitly scopes ~5 rows BEFORE the catalog; 37-02 extends the `.inc` and table). The coverage self-check (count static_assert + name-set equality) guarantees the table and the `.inc` required set stay in lockstep, so the partial set is honest-by-construction, not a stub.

## Notes for 37-02 (carry-forward)
- **Export mechanism is proven:** `__declspec(dllexport)` on `extern "C" __cdecl` alone yields the undecorated name on the exe (dumpbin: `GetEngineHookPoints = _GetEngineHookPoints`). No .def / /EXPORT pragma needed. Do not add one.
- **PMF helper works** for single-inheritance non-virtual members (`CommandParser::addSubCommand` is single-base, non-virtual; `sizeof(PMF)==sizeof(void*)` held). Keep skipping virtual rows (vtable stub) and thunking ctors per the landmine rules.
- **Include-path trap:** any 37-02/37-03 row whose header lives under a library NOT already in SwgClient's `AdditionalIncludeDirectories` needs that `include\public` dir added (the sharedCommandParser case). Check before assuming the path is wired.
- **Coverage gate is inheritable:** add new names to `utinni_engine_hookpoints.inc` and the matching `s_engineHookPoints[]` rows; the count `static_assert` (drift smoke) + `utinni_verifyNoNullNoDup()` name-set equality enforce zero-drift automatically. Keep NO sentinel / NO `-1`.
- **Re-copy obligation:** 37-02/37-03 must re-copy `utinni_engine_hookpoints.h` + `.inc` into `D:/Code/Utinni` at wave close (version pinned at 1; the re-copy IS the anti-drift action).
- **Boot-path call site** for the self-check: `ClientMain.cpp:259`, right after `installConfigFileOverride()`.

## Next Phase Readiness
- The full P0 contract spike is landed and verified; 37-02 (MVP ~70 endpoints) is unblocked.
- Live first-detour-crash-gone confirmation is Utinni-side (out of repo, per the Validation Architecture) -- this repo proved the table advertises `config::loadOverrideConfig` non-null undecorated, which is the in-repo half of EPA-02.

## Self-Check: PASSED

- Created files verified present: utinni_engine_hookpoints.h, .inc, utinni_advertise.cpp, 37-01-SUMMARY.md.
- Task commits verified in git log: f50278c65, 0cd4139c1, 9dac8aad6.

---
*Phase: 37-utinni-engine-entry-point-advertisement-getenginehookpoints*
*Completed: 2026-06-21*
