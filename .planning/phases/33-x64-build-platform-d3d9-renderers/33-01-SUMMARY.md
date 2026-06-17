---
phase: 33-x64-build-platform-d3d9-renderers
plan: 01
subsystem: infra
tags: [x64, msbuild, vcxproj, props, import-lib, dumpbin, tinyxml, libxml2, pcre, jpeg, supply-chain]

# Dependency graph
requires:
  - phase: 31-64-bit-correctness-foundation
    provides: "Phase-31 scratch x64 props (x64-compile.props) + the N1/N2 carry-forward residual worklist (re-establish /we4311 /we4312, decide C4267) promoted here"
  - phase: 32-d3dx-to-d3dcompiler-47
    provides: "D3DX-free shader path (d3dcompiler_47) so the x64 link is not blocked by x64-hostile D3DX"
provides:
  - "Committed x64-platform.props (config-split Debug|x64 / Release|x64, N1 guardrail, no _USE_32BIT_TIME_T)"
  - "Three x64 import libs (libxml2-x64.lib / pcre-x64.lib / jpeg62-x64.lib) generated from Restoration x64 DLL exports, dumpbin-verified machine x64"
  - "tinyxml.vcxproj x64 from-source StaticLibrary producing tinyxmld_STL.lib + tinyxmld.lib (TIXML_USE_STL)"
  - "33-x64-DLL-CHECKLIST.md: V14 supply-chain provenance (size+SHA256+dumpbin x64) for every staged x64 DLL"
  - "X64-04 icu/discord-rpc recorded satisfied-by-N/A with exact-token evidence"
affects: [33-02, 33-03, 33-04, 33-05, x64-platform-add, swg.sln-x64-registration]

# Tech tracking
tech-stack:
  added: [x64-platform.props, libxml2-x64.lib, pcre-x64.lib, jpeg62-x64.lib, tinyxml.vcxproj]
  patterns: ["config-split x64 ItemDefinitionGroups (shared + Debug|x64 + Release|x64)", "import-lib-from-DLL-exports (dumpbin /exports -> .def -> lib /machine:x64)", "x64 OutDir isolation under compile/win32/<proj>/x64/<cfg>/", "dual-name static-lib output via post-build copy alias"]

key-files:
  created:
    - src/build/win32/x64-platform.props
    - src/external/3rd/library/libxml2-2.6.7.win32/lib/libxml2-x64.lib
    - src/external/3rd/library/pcre/4.1/win32/lib/pcre-x64.lib
    - src/external/3rd/library/libjpeg/lib/jpeg62-x64.lib
    - src/external/3rd/library/tinyxml/build/win32/tinyxml.vcxproj
    - .planning/phases/33-x64-build-platform-d3d9-renderers/33-x64-DLL-CHECKLIST.md
    - .planning/research/33-import-libs/ (provenance: .exports.txt, .def, README.md)
  modified: []

key-decisions:
  - "tinyxml STL macro is TIXML_USE_STL, NOT the plan's TINYXML_USE_STL (corrected from tinyxml.h:46) -- the wrong token leaves the std::string overloads out"
  - "Single tinyxml.vcxproj emits BOTH tinyxmld_STL.lib (Lib OutputFile) and tinyxmld.lib (post-build copy alias) -- SwgClient.vcxproj:103/:216 link both names; collapses the legacy two-.dsp split to one swg.sln project"
  - "tinyxml OutDir depth = 7 levels (..\\ x7), filesystem-verified from build/win32/ to repo root, NOT the assumed 5"
  - "X64-04 icu/discord-rpc satisfied-by-N/A: they ship in Restoration's x64/ because Restoration added those features; this client has 0 include/vendor/link references"

patterns-established:
  - "x64 import-lib procurement: dumpbin /exports <Restoration-dll> -> parse NAME column -> EXPORTS .def -> lib /def /machine:x64; always dumpbin /headers | findstr machine -> 8664"
  - "Run dumpbin/lib/MSBuild via PowerShell, never Git Bash (MSYS mangles /flag args into paths -> LNK1181)"
  - "Force-add vendored third-party x64 libs past the broad lib/ .gitignore (matches the tracked x86 siblings)"

requirements-completed: [X64-04, X64-01]

# Metrics
duration: 12min
completed: 2026-06-17
---

# Phase 33 Plan 01: x64 Build Foundations Summary

**Committed x64-platform.props (config-split Debug|x64/Release|x64 with the N1 /we4311/4312 guardrail), three Restoration-lifted x64 import libs (libxml2/pcre/jpeg, all dumpbin-verified machine 8664), a from-source tinyxml x64 StaticLibrary emitting tinyxmld_STL.lib + tinyxmld.lib, and a V14 supply-chain DLL provenance checklist — the Wave-0 procurement blockers for the x64 platform-add, with the 32-bit build provably untouched.**

## Performance

- **Duration:** ~12 min
- **Started:** 2026-06-17T16:58:40Z
- **Completed:** 2026-06-17T17:10:55Z
- **Tasks:** 3
- **Files modified:** 13 (6 deliverables + 7 provenance)

## Accomplishments
- **x64-platform.props** committed as the x64 compile single-source-of-truth — three `ItemDefinitionGroup`s SPLIT by configuration (shared `'$(Platform)'=='x64'` + Debug|x64 + Release|x64). Release|x64 carries `NDEBUG` (not `_DEBUG`) — the reviews-fix-#2 silent-Release-miscompile guard. N1 guardrail (`/we 4311;4312`) re-established; `_USE_32BIT_TIME_T` dropped (A2-TIME-T); `_LIB`/ConfigurationType defines NOT baked in (stay per-project).
- **Three x64 import libs** generated from the Restoration x64 DLL exports (libxml2 1675 exports, pcre 30, jpeg62 107), each dumpbin-verified machine `8664 (x64)`. The **pcre export-name spot-check PASSED** (reviews fix #7b): Restoration's `pcre.dll` exports `pcre_compile`/`pcre_exec`/`pcre_free`/`pcre_malloc` UNDECORATED, matching the engine call sites — the MinGW-`.a`-vs-MSVC-DLL decoration trap does not bite, so `pcre-x64.lib` links clean for Plan 05.
- **tinyxml x64 from-source build** — `tinyxml.vcxproj` (StaticLibrary, the one dep with no Restoration DLL fallback) builds clean x64, producing `tinyxmld_STL.lib` (machine 8664) at the peer-verified OutDir `src/compile/win32/tinyxml/x64/Debug/`, plus a `tinyxmld.lib` alias so both names SwgClient links resolve.
- **33-x64-DLL-CHECKLIST.md** records V14 supply-chain provenance (size + SHA256 + dumpbin-x64 command) for every staged x64 DLL, with Miles `mss64.dll` deferred to Phase 35 and X64-04's icu/discord-rpc recorded satisfied-by-N/A.

## Task Commits

Each task was committed atomically:

1. **Task 1: Promote x64-compile.props into committed x64-platform.props** — `971f08e49` (feat)
2. **Task 2: Generate x64 import-libs (libxml2/pcre/jpeg) from Restoration x64 DLL exports** — `66b896195` (feat)
3. **Task 3: Author tinyxml x64 build + write the staged-DLL provenance checklist** — `0121fcbba` (feat)

**Plan metadata:** (this SUMMARY + STATE/ROADMAP) — final docs commit

## Files Created/Modified
- `src/build/win32/x64-platform.props` — committed x64 ClCompile single-source-of-truth (3 config-split ItemDefinitionGroups, N1 guardrail)
- `src/external/3rd/library/libxml2-2.6.7.win32/lib/libxml2-x64.lib` — x64 import lib (1675 exports, machine 8664)
- `src/external/3rd/library/pcre/4.1/win32/lib/pcre-x64.lib` — x64 import lib (30 exports, undecorated pcre_* verified)
- `src/external/3rd/library/libjpeg/lib/jpeg62-x64.lib` — x64 import lib (107 exports, machine 8664)
- `src/external/3rd/library/tinyxml/build/win32/tinyxml.vcxproj` — x64 from-source StaticLibrary (TIXML_USE_STL; emits tinyxmld_STL.lib + tinyxmld.lib)
- `.planning/phases/33-x64-build-platform-d3d9-renderers/33-x64-DLL-CHECKLIST.md` — V14 staged-DLL provenance manifest (79 lines)
- `.planning/research/33-import-libs/{libxml2,pcre,jpeg62}.{exports.txt,def}` + `README.md` — import-lib provenance + pcre spot-check record

## Decisions Made
- **tinyxml macro corrected to `TIXML_USE_STL`** (the plan said `TINYXML_USE_STL`). Verified against `tinyxml.h:46` — the real switch is `TIXML_USE_STL`; the plan's token would silently omit the std::string overloads. Treated as a Rule-3 blocking correction (see Deviations).
- **One vcxproj emits both lib names.** SwgClient.vcxproj links `tinyxmld.lib;tinyxmld_STL.lib` (confirmed at :103 Debug and :216 Release — debug-suffixed even in Release, the existing tree's quirk). A single StaticLibrary project produces one OutputFile, so `tinyxmld_STL.lib` is the Lib output and a post-build `copy` aliases it to `tinyxmld.lib`. This keeps swg.sln to one project (Plan 04's registration contract) while satisfying both link-input names. No SwgClient-closure source actually `#include`s tinyxml's STL API, so a single build under both names is correct.
- **OutDir depth = 7 levels**, filesystem-verified (`cd build/win32 && cd ../../../../../../.. == repo root`), not the assumed 5. Mirrors SwgClient.vcxproj's own 7-deep `..\` paths.
- **Force-added the x64 import libs** past the broad `.gitignore:31 lib/` rule, matching how the existing tracked x86 siblings (`libjpeg.lib`, `libpcre.a`, `libxml2-win32-*.lib`) are kept.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] tinyxml STL macro is TIXML_USE_STL, not the plan's TINYXML_USE_STL**
- **Found during:** Task 3 (authoring tinyxml.vcxproj)
- **Issue:** The plan repeatedly specifies `TINYXML_USE_STL` as the define. tinyxml's actual STL switch is `TIXML_USE_STL` (`tinyxml.h:46`, and the legacy `tinyxmlSTL.dsp:44` confirms `TIXML_USE_STL`). Defining the plan's token would compile a non-STL lib mislabeled `_STL`.
- **Fix:** Defined `TIXML_USE_STL` in both x64 ClCompile blocks. The STL flavor compiled correctly (the `std::string` overloads are present).
- **Files modified:** src/external/3rd/library/tinyxml/build/win32/tinyxml.vcxproj
- **Verification:** Standalone x64 Debug build exit 0; `tinyxmld_STL.lib` produced at the expected OutDir, dumpbin machine 8664.
- **Committed in:** 0121fcbba (Task 3 commit)

**2. [Rule 3 - Blocking] dumpbin/lib invoked via PowerShell (Git Bash mangled the /flag args)**
- **Found during:** Task 2 (generating import libs)
- **Issue:** The first `dumpbin /exports` run under Git Bash mangled `/exports` into a Windows path (`C:\Program Files\Git\exports`) → `LNK1181: cannot open input file`. This is the documented MSYS path-mangling (AGENTS.md: builds must run from PowerShell).
- **Fix:** Re-ran the entire dumpbin/lib import-lib generation (and the tinyxml MSBuild + dumpbin verification) from a PowerShell script invoked via `powershell.exe -File`.
- **Files modified:** (none committed — tooling-invocation fix)
- **Verification:** All three import libs generated, dumpbin machine 8664; tinyxml built exit 0.
- **Committed in:** 66b896195 (Task 2) / 0121fcbba (Task 3)

**3. [Rule 3 - Blocking] Force-add the x64 import libs past the lib/ .gitignore**
- **Found during:** Task 2 (committing import libs)
- **Issue:** `.gitignore:31` has a broad `lib/` rule that ignores the new `*-x64.lib` files; the plan requires them committed. The existing tracked x86 siblings prove these vendored libs are intentionally force-tracked.
- **Fix:** `git add -f` the three import libs (consistent with the tracked x86 siblings).
- **Files modified:** (gitignore not changed — used -f)
- **Verification:** `git status` shows all three staged as new; committed.
- **Committed in:** 66b896195 (Task 2 commit)

---

**Total deviations:** 3 auto-fixed (1 bug-correction, 2 blocking — both tooling/VCS, no source-logic change)
**Impact on plan:** All three were necessary to produce the plan's stated artifacts correctly. The macro correction is the only one affecting a committed file's content; no scope creep.

## Issues Encountered
- The `grep -rIn` over the entire `src/` tree for the X64-04 include-directive check was slow enough that the bash harness backgrounded it; resolved by also running the equivalent via the Grep tool (both returned 0). All three X64-04 N/A checks (include / vendor / link) confirmed 0.

## Notes for Downstream Plans

- **Plan 04-T3 MUST register `tinyxml.vcxproj` in `swg.sln`** (Project entries + its boot-path x64 configuration mapping). This task authored the vcxproj only.
- **Plan 04** wires `x64-platform.props` into the boot-path .vcxproj x64 ItemDefinitionGroups (this file imports into nothing yet). A Release|x64 preprocessor-define proof is deferred to Plan 04 acceptance (first project to compile Release|x64).
- **Plan 05** owns the SwgClient x64 `AdditionalDependencies` name swap: `libxml2-win32-*.lib` → `libxml2-x64.lib`, `libpcre.a` → `pcre-x64.lib`, `libjpeg.lib` → `jpeg62-x64.lib`; tinyxml names (`tinyxmld.lib;tinyxmld_STL.lib`) are unchanged but now resolved from the x64 OutDir.
- **`d3dcompiler_47.dll` (x64)** staging: copy from `C:\Windows\Sysnative\` (or a native-x64 shell) to dodge the WoW64 redirector — see the checklist note.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All Wave-0 procurement/foundation blockers cleared: the committed props, three x64 import libs, the one no-fallback from-source dep (tinyxml), and the staged-DLL provenance manifest are in place.
- 32-bit build provably untouched: no shipped Win32 `.vcxproj` was edited, no project imports the new props yet, and the import-lib filenames are distinct from the x86 inputs.
- Plan 04 (platform-add) is unblocked.

## Self-Check: PASSED

All 7 created files verified present on disk; all 3 task commits verified in git history (971f08e49, 66b896195, 0121fcbba).

---
*Phase: 33-x64-build-platform-d3d9-renderers*
*Completed: 2026-06-17*
