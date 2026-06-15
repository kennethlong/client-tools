---
phase: 31-64-bit-correctness-foundation
plan: 01
subsystem: infra
tags: [x64, msbuild, cl.exe, compile-only, warnings-as-errors, C4235, C4311, C4312, C4244, llp64, time_t, powershell]

# Dependency graph
requires: []
provides:
  - "Uncommitted, gitignored scratch Debug|x64 compile-only harness (D-01) at src/build/win32/x64-scratch/"
  - "EXHAUSTIVE in-scope TU manifest (in-scope-tus.txt, 2216 TUs, 57 libs) derived mechanically from the ClCompile graph"
  - "x64-compile.props: x64 compile parity (PLATFORM_WIN32, stdcpp20, /we4311 /we4312 /we4244, /c, /Y-)"
  - "compile-all.ps1 driver with the -SingleTu / -Scope / -CastAudit / -PositiveProof + default-sweep interface every later plan invokes"
  - "explicit-cast-audit.log generator (the BITS-02 companion worklist /we4311 is blind to)"
  - "DOMINANT x64 worklist finding: Misc.h:236 C2668 (memmove int/size_t ambiguity) blocks ~the entire in-scope tree on x64"
  - "BITS-03 finding: _USE_32BIT_TIME_T is ILLEGAL under _WIN64 (UCRT hard #error) -> time_t IS 8 bytes on x64, must be pinned in serialization, not by a flag"
affects: [31-02, 31-03, 31-04, 31-05, 31-06, phase-33-x64-platform]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Compiler-driven worklist: cl /c per in-scope TU with /we4311 /we4312 /we4244 -> the deterministic truncation/asm worklist"
    - "PowerShell driver derives the x64 cl.exe + x64 env from $env:MSBUILD (vcvars64) and reads per-lib include dirs out of each committed .vcxproj (with $(SolutionDir)/$(ProjectDir) macro expansion)"
    - "Props is the single source of truth for defines+flags; the driver reads them so props/driver cannot drift"

key-files:
  created:
    - ".gitignore (one line: src/build/win32/x64-scratch/)"
    - "src/build/win32/x64-scratch/x64-compile.props (UNCOMMITTED/gitignored)"
    - "src/build/win32/x64-scratch/compile-all.ps1 (UNCOMMITTED/gitignored)"
    - "src/build/win32/x64-scratch/in-scope-tus.txt (UNCOMMITTED/gitignored)"
    - "src/build/win32/x64-scratch/gen-manifest.sh (UNCOMMITTED/gitignored)"
    - "src/build/win32/x64-scratch/include-resolution-probe.cpp (UNCOMMITTED/gitignored)"
  modified:
    - ".gitignore"

key-decisions:
  - "Manifest derived mechanically from every in-scope lib's ClCompile list (2216 TUs, 57 libs) -- exhaustive, not a hand-picked first cut (review #4)"
  - "Driver targets the VS18/v145 Hostx64/x64 cl.exe, compile-only (/c), no link -> no x64 third-party libs needed"
  - "_USE_32BIT_TIME_T DROPPED from the x64 scratch defines (UCRT hard-errors on it under _WIN64) -- recorded as a BITS-03 residual"
  - "Positive include-resolution proof targets sharedFoundationTypes (Misc.h-free) because Misc.h's x64 C2668 blocks every foundation-dependent TU"

patterns-established:
  - "Per-task verify modes (-SingleTu/-Scope) downstream plans call; -CastAudit for the explicit-cast worklist; -PositiveProof for the include-resolution proof"
  - "$(SolutionDir)/$(ProjectDir) MSBuild-macro expansion when reading committed include lists for standalone cl invocation"

requirements-completed: [BITS-01, BITS-02, BITS-03]

# Metrics
duration: 21min
completed: 2026-06-15
---

# Phase 31 Plan 01: Scratch x64 Compile-Only Harness Summary

**Stood up the uncommitted Debug|x64 `cl /c` worklist generator (props + PowerShell driver + 2216-TU exhaustive manifest) that surfaces C4235 asm survivors, C4311/C4312/C4244 truncation, and an explicit-cast audit -- and on first run it already exposed the two highest-value x64 blockers: the tree-wide Misc.h memmove C2668 and the UCRT _USE_32BIT_TIME_T/_WIN64 hard-error.**

## Performance

- **Duration:** ~21 min
- **Started:** 2026-06-15T23:10:12Z
- **Completed:** 2026-06-15T23:30:55Z
- **Tasks:** 3
- **Files modified (tracked):** 1 (`.gitignore`); the harness itself is gitignored by design (D-01)

## Accomplishments
- **Gitignored scratch harness + exhaustive manifest** (Task 1): `.gitignore` line added; `in-scope-tus.txt` mechanically expanded from every in-scope lib's `<ClCompile>` list -> **2216 TUs across 57 libs**, with all 18 known-defect TUs present as a SUBSET and editor/tool/3rd-party/server libs excluded (D-03). A `gen-manifest.sh` regenerator + a documented header give the completeness audit trail.
- **x64 parity props** (Task 2): `x64-compile.props` mirrors the committed 32-bit Debug ClCompile block (PLATFORM_WIN32, stdcpp20, RTTI, native wchar_t), adds the scratch-only deltas (x64 target, `/c`, `/Y-`, `/we4311 /we4312 /we4244`), and documents the deliberate C4267 Phase-33 carry-forward (review #7).
- **Driver with the full verify interface** (Task 3): `compile-all.ps1` exposes the documented `-SingleTu` / `-Scope <bits01|bits02|bits03|all>` / `-CastAudit` (+ a `-PositiveProof` + default-sweep) interface that plans 02-06 invoke; derives the Hostx64/x64 cl.exe + x64 env from `$env:MSBUILD`; reads per-lib includes from each committed `.vcxproj`. Proven end-to-end:
  - **NEGATIVE:** `-SingleTu SseMath.cpp` -> 5x `error C4235` (`__asm` not supported on x64).
  - **TRUNCATION worklist:** `-Scope bits02` surfaced **19x `error C4311`** (ShaderEffect.h:93, Direct3d9 buffer/shader data, RenderWorld.cpp:1127, StaticShader.cpp:289) + `C4244` -- the /we worklist works.
  - **POSITIVE:** `-PositiveProof` compiles a probe including a real engine header to a `.obj`, exit 0 (include extraction resolves; no false-clean skip).
  - **CAST AUDIT:** `-CastAudit` -> `explicit-cast-audit.log` with 23 sites incl. the required StaticShader `reinterpret_cast<int>` + Direct3d11 `static_cast<int>(reinterpret_cast<uintptr_t>` + Direct3d9 `(DWORD)(uintptr_t)`.
- **Committed tree untouched:** `grep -c x64 swg.sln == 0`; no in-scope `.vcxproj` modified; every scratch artifact `git check-ignore`-confirmed ignored.

## Task Commits

Task 1 produced the only tracked change (the `.gitignore` line); Tasks 2-3 produced ONLY gitignored scratch artifacts (D-01 "signal generator, not a deliverable") so they have no git commit by design.

1. **Task 1: gitignore + exhaustive manifest** - `bd453e696` (chore)
2. **Task 2: x64-compile.props parity** - (no commit; gitignored artifact by design)
3. **Task 3: compile-all.ps1 driver + proofs** - (no commit; gitignored artifacts by design)

## Files Created/Modified
- `.gitignore` - added `src/build/win32/x64-scratch/` (committed, `bd453e696`)
- `src/build/win32/x64-scratch/in-scope-tus.txt` - 2216-TU exhaustive manifest (gitignored)
- `src/build/win32/x64-scratch/gen-manifest.sh` - mechanical manifest regenerator (gitignored)
- `src/build/win32/x64-scratch/x64-compile.props` - x64 compile parity + scratch deltas (gitignored)
- `src/build/win32/x64-scratch/compile-all.ps1` - the driver with all verify modes (gitignored)
- `src/build/win32/x64-scratch/include-resolution-probe.cpp` - positive include-resolution proof TU (gitignored)
- runtime outputs (gitignored): `worklist.log`, `explicit-cast-audit.log`, `obj/`

## Decisions Made
- **Manifest is ClCompile-graph-derived, not hand-picked** (review #4): 2216 TUs / 57 libs, mechanically expanded by `gen-manifest.sh`; known-defect TUs are a verified subset.
- **`sharedDatabaseInterface` + all server/database libs excluded** as server-side, not in the client boot path (D-03 scope = SwgClient boot path + gl05/06/07/11 + the engine/game-shared/external-ours libs they link).
- **`ui` and `libEverQuestTCG` excluded** as `external/3rd/*` (third-party, Phase 33).
- **Positive proof targets `sharedFoundationTypes`** (the one Misc.h-free in-scope public header) because Misc.h's x64 defect blocks every other candidate.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1/3 - Toolchain-impossible plan premise] `_USE_32BIT_TIME_T=1` cannot be carried on x64**
- **Found during:** Task 3 (first SseMath compile)
- **Issue:** The plan's must_haves truth + threat T-31-02 require the harness to carry `_USE_32BIT_TIME_T=1` "so time_t stays 32-bit on x64." The modern UCRT (`corecrt.h`) has an UNCONDITIONAL `#if defined _USE_32BIT_TIME_T && defined _WIN64 -> #error You cannot use 32-bit time_t (_USE_32BIT_TIME_T) with _WIN64`. With the flag set, EVERY x64 TU fails with fatal `C1189` -- the harness cannot compile anything.
- **Fix:** Dropped `_USE_32BIT_TIME_T` from the x64 scratch define set (the only way the harness compiles). Documented the constraint + its consequence prominently in `x64-compile.props`.
- **Consequence (a real BITS-03 finding, surfaced as designed):** on x64 `time_t` IS 8 bytes and CANNOT be pinned by this flag. Any `time_t` serialized/packed as 4 bytes on the 32-bit wire/disk is a genuine x64 width-mover the BITS-03 plan (31-05) must pin explicitly (serialize as int32/uint32, not raw time_t). This **corrects** the CONTEXT/RESEARCH premise that the flag preserves 32-bit time_t on x64. Recorded as a residual for 31-05 + Phase 33.
- **Verification:** SseMath compile got past corecrt.h (C1189 gone) and produced the expected C4235.

**2. [Rule 1 - Bug] Driver dropped `$(SolutionDir)`/`$(ProjectDir)` include roots**
- **Found during:** Task 3 (positive proof / ByteStream attempt)
- **Issue:** Several committed include lists (e.g. archive.vcxproj) use `$(SolutionDir)..\..\engine\...` macros. cl doesn't expand them, and the driver's `Resolve-Path`/`Join-Path` silently dropped those roots -> TUs failed with `C1083 cannot open include file` (exactly the false-clean trap review #4 warns about).
- **Fix:** Expand `$(SolutionDir)` (= `src/build/win32/`) and `$(ProjectDir)` (= the vcxproj dir) before resolving; skip `Join-Path` for already-rooted paths (Join-Path mangles an absolute second arg).
- **Verification:** archive includes now fully resolve; the positive proof reaches a clean `.obj`, and bits02 TUs reach real compilation surfacing the C4311 worklist.

---

**Total deviations:** 2 auto-fixed (1 toolchain-impossible premise corrected, 1 driver bug)
**Impact on plan:** Both essential for a functioning harness. No scope creep. Deviation #1 is also a substantive BITS-03 finding that strengthens the phase; the must_haves "carries _USE_32BIT_TIME_T=1" truth and T-31-02 mitigation are superseded by toolchain reality (documented + handed forward).

## Issues Encountered

- **DOMINANT x64 blocker -- `Misc.h:236` C2668 (highest-value harness finding):** `sharedFoundation/src/shared/Misc.h` defines `inline void* memmove(void*, const void*, int)` whose body calls `memmove(dest, src, static_cast<uint>(length))`. On x64 (size_t = 8 bytes) that call is AMBIGUOUS between this `int` overload and the CRT's `size_t` overload (`error C2668`). Misc.h is transitively included by ~the entire in-scope tree, so it **blocks essentially every in-scope TU from a clean x64 compile today**. This is a genuine BITS-02/03 defect for an early fix-wave plan (31-02/04) -- fixing it is what unblocks the full x64 sweep. It is the single biggest worklist item the harness produced. (It also forced the positive proof to target the Misc.h-free `sharedFoundationTypes` header rather than a plain math/util TU.)
- **Harmless `C4005 'PLATFORM_WIN32' macro redefinition`:** PLATFORM_WIN32 is defined both on the command line (props parity requirement) and in FoundationTypesWin32.h. Not in the /we set, not an error -- left as-is to honor the plan's PLATFORM_WIN32 acceptance.

## Residual Worklist (handed to 31-02..06 + Phase 33)

- **Misc.h:236 C2668** -- fix the `memmove(void*,const void*,int)` overload's body (pin the count width) so the in-scope tree compiles x64-clean; it gates the full sweep (BITS-02/03; earliest fix-wave plan).
- **time_t is 8 bytes on x64** -- `_USE_32BIT_TIME_T` is unusable under _WIN64; any 4-byte-`time_t` serialization/packing must be pinned to int32/uint32 explicitly (BITS-03 / 31-05).
- **C4267 (`size_t`->smaller) not promoted to /we this phase** -- Phase-33 carry-forward (review #7); plus re-establishing committed `/we4311 /we4312` once the x64 platform exists (the scratch harness does not survive its own deletion across the 31->33 gap).
- **Explicit-cast sites (23)** in `explicit-cast-audit.log` -- `/we4311` is blind to most of these (e.g. `static_cast<int>(reinterpret_cast<uintptr_t>...)`); they need the manual grep audit for plan 04. (Note: a bare `reinterpret_cast<int>(ptr)` DOES still trip C4311.)

## User Setup Required
None - the harness derives its toolchain from `$env:MSBUILD` (already set per AGENTS.md). No external service configuration.

## Next Phase Readiness
- The Wave-1 enabler is in place: plans 31-02..06 can sample against `compile-all.ps1` via `-SingleTu`/`-Scope`, and the phase gate (31-06) can run the full sweep.
- **Recommended first fix-wave target:** `Misc.h:236` C2668 -- it currently gates almost every in-scope TU on x64, so the per-task `-SingleTu` verifies in 31-02/04 will keep tripping it until it is fixed.
- No regression risk to the committed 32-bit build: the only tracked change is one `.gitignore` line; swg.sln and all in-scope `.vcxproj` are untouched.

## Self-Check: PASSED

- SUMMARY.md present at `.planning/phases/31-64-bit-correctness-foundation/31-01-SUMMARY.md`
- Task 1 commit `bd453e696` present in git log
- All scratch artifacts present on disk (x64-compile.props, compile-all.ps1, in-scope-tus.txt, gen-manifest.sh, include-resolution-probe.cpp) and `git check-ignore`-confirmed gitignored (Tasks 2-3 are uncommitted by design per D-01)
- Committed tree integrity: `grep -c x64 swg.sln == 0`, no in-scope `.vcxproj` modified

---
*Phase: 31-64-bit-correctness-foundation*
*Completed: 2026-06-15*
