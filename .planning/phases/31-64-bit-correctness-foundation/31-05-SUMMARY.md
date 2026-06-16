---
phase: 31-64-bit-correctness-foundation
plan: 05
subsystem: serialization
tags: [bits-03, llp64, archive, wire-format, static_assert, packed-structs, x64, uint32_t, targa, customization]

# Dependency graph
requires:
  - phase: 31-01
    provides: "scratch Debug|x64 compile-only harness (compile-all.ps1 -SingleTu/-Scope) + in-scope-tus.txt manifest"
  - phase: 31-04
    provides: "DEF-31-01 Misc.h memmove C2668 resolved (359214d2b) so in-scope TUs compile past it"
provides:
  - "Archive std::map get/put wire count pinned to uint32_t (binds the existing 4-byte overload on both bitnesses; wire format byte-identical to the shipped 32-bit server)"
  - "overflow-guarded put (assert source.size() <= UINT32_MAX before the narrowing cast)"
  - "scratch-only archive-map-instantiation.cpp EXERCISING the std::map<int,int> (de)serialization x64-clean (zero such instantiations exist under src/engine)"
  - "static_assert(sizeof==N) + offsetof layout guards on TargaHeader/TargaFooter and the 5 AssetCustomizationManager packed structs, baselined to the live 32-bit sizeof"
  - "RESIDUAL-31-05 (ByteStream.cpp:347 C4311 ptr-truncation) classified + handed to plan 06"
affects: [31-06, phase-33, BITS-03, serialization, wire-format]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "BITS-03 serialization width pin: wire counts bind the existing 4-byte unsigned-int Archive overload via a uint32_t local, never a new 8-byte overload (a raw size_t FAILS overload resolution on x64 = compile error, not a silent widen)"
    - "BITS-03 layout guard: static_assert(sizeof(X)==N)+offsetof baselined to the LIVE 32-bit compiler (confirmed via a standalone cl probe), per the Direct3d11_ConstantBuffer.h house convention"
    - "Exercising-TU pattern: a gitignored scratch instantiation TU forces an otherwise-uninstantiated template path to compile so the harness proves the fix"

key-files:
  created:
    - "src/build/win32/x64-scratch/archive-map-instantiation.cpp (gitignored scratch instantiation TU)"
  modified:
    - "src/external/ours/library/archive/src/shared/Archive.h (uint32_t map count + overflow guard)"
    - "src/engine/shared/library/sharedImage/src/shared/TargaFormat.cpp (static_assert layout guards)"
    - "src/engine/shared/library/sharedGame/src/shared/core/AssetCustomizationManager.cpp (static_assert layout guards)"
    - "src/build/win32/x64-scratch/in-scope-tus.txt (gitignored; appended the instantiation TU)"
    - ".planning/phases/31-64-bit-correctness-foundation/deferred-items.md (RESIDUAL-31-05)"

key-decisions:
  - "Used assert (archive lib house guard) for the >UINT32_MAX overflow guard, NOT DEBUG_FATAL — DEBUG_FATAL lives in sharedFoundation which sits ABOVE the external/ours archive layer; using it would be a layering violation and would break the standalone instantiation TU"
  - "Placed the instantiation TU in x64-scratch/ and #include Archive.h by relative path — Archive.h depends only on ByteStream.h (+ <vector>) and stdlib, so the TU needs zero per-lib include dirs (Get-LibIncludes returning empty is correct)"
  - "Baselined the static_assert N from a standalone 32-bit cl probe (exit 0), NOT the TGA-spec 18/26 literals from RESEARCH"

patterns-established:
  - "Pattern 5 (RESEARCH): LLP64 serialization width guard — pin to fixed width, bind the existing overload"
  - "Pattern 6 (RESEARCH): compile-time layout guard — static_assert(sizeof==N)+offsetof baselined to the compiler"

requirements-completed: [BITS-03]

# Metrics
duration: 35min
completed: 2026-06-16
---

# Phase 31 Plan 05: BITS-03 Serialization-Width + Layout-Guard Summary

**Archive `std::map` wire count pinned to `uint32_t` (binds the existing 4-byte overload, wire byte-identical to the shipped 32-bit server, overflow-guarded), exercised by a scratch `std::map<int,int>` instantiation TU, plus permanent `static_assert(sizeof==N)`/`offsetof` layout guards on the TGA + customization packed structs baselined to the live 32-bit sizeof.**

## Performance

- **Duration:** ~35 min
- **Started:** 2026-06-16T00:17:00Z (approx)
- **Completed:** 2026-06-16
- **Tasks:** 3
- **Files modified:** 3 tracked (Archive.h, TargaFormat.cpp, AssetCustomizationManager.cpp) + 1 doc (deferred-items.md) + 2 gitignored scratch (instantiation TU, manifest)

## Accomplishments
- The ONE real BITS-03 hazard — `Archive::get/put(std::map)` raw `size_t numKeys` — is pinned to `uint32_t`, binding the existing 4-byte `unsigned int` overload on both x86 and LLP64 x64. The wire format is byte-identical to the shipped 32-bit server (no 8-byte overload added).
- The corrected threat narrative (review #5) is realized: a raw `size_t` here FAILS overload resolution on x64 (a compile error the harness catches), it does NOT silently widen the wire.
- `put(std::map)` is overflow-guarded: `assert(source.size() <= UINT32_MAX)` before the narrowing cast.
- A scratch-only `archive-map-instantiation.cpp` round-trips a `std::map<int,int>` through a `ByteStream`, EXERCISING both template paths so the fix compiles x64-clean (zero such instantiations exist under `src/engine`, so boot smoke alone would not prove it).
- Permanent `static_assert(sizeof==N)` + `offsetof` guards added to `TargaHeader`(18)/`TargaFooter`(26) and the 5 `AssetCustomizationManager` packed structs (`IntRange`=8/`VariableUsage`=6/`UsageIndexEntry`=5/`LinkIndexEntry`=5/`CrcLookupEntry`=6), baselined to the live 32-bit `sizeof`. The 32-bit SwgClient Debug build is green (0 `static_assert failed`/C2338; `SwgClient_d.exe` produced).
- The bits03 backstop sweep (61 TUs) reports 0 serialization overload-resolution (C2664/C2665) survivors in this plan's files; the one residual (ByteStream.cpp:347 C4311 ptr-truncation, a BITS-02 defect not owned here) is classified + handed to plan 06.

## Task Commits

Each task was committed atomically:

1. **Task 1: Pin Archive std::map count to uint32_t + overflow guard + exercising TU** - `f9efe220c` (fix)
2. **Task 2: static_assert layout guards on the packed structs (baselined to live 32-bit sizeof)** - `650848aff` (feat)
3. **Task 3: bits03 backstop sweep + record residual for plan 06** - `f9fefbc21` (docs)

**Plan metadata:** (this commit) (docs: complete plan)

## Files Created/Modified
- `src/external/ours/library/archive/src/shared/Archive.h` - `get/put(std::map)` use `uint32_t numKeys` (binds the 4-byte overload), `put` asserts `<= UINT32_MAX`; added `<cassert>`/`<cstdint>`. SAFE paths (vector/set/deque/IFF/int32) byte-unchanged.
- `src/engine/shared/library/sharedImage/src/shared/TargaFormat.cpp` - `static_assert(sizeof(TargaHeader)==18)`, `sizeof(TargaFooter)==26`, `offsetof(TargaFooter,m_signature)==8`; added `<stddef.h>`.
- `src/engine/shared/library/sharedGame/src/shared/core/AssetCustomizationManager.cpp` - 5 `static_assert(sizeof)` guards + `offsetof(CrcLookupEntry,assetId)==4`; added `<stddef.h>`.
- `src/build/win32/x64-scratch/archive-map-instantiation.cpp` *(gitignored, created)* - exercising TU for `Archive::get/put(std::map<int,int>)`.
- `src/build/win32/x64-scratch/in-scope-tus.txt` *(gitignored)* - appended the instantiation TU so the harness compiles it.
- `.planning/phases/31-64-bit-correctness-foundation/deferred-items.md` - RESIDUAL-31-05 (ByteStream.cpp C4311) for plan 06.

## Decisions Made
- **`assert` over `DEBUG_FATAL` for the overflow guard.** `DEBUG_FATAL` lives in `sharedFoundation`, which sits ABOVE the `external/ours/archive` layer; pulling it in here would be a layering violation and would break the standalone instantiation TU (no Fatal.h). The archive lib's house style is `<cassert>`/`assert` (AutoByteStream.h, AutoDeltaByteStream.h), so `assert` is the correct, layering-clean choice. The plan explicitly permitted assert OR a documenting comment.
- **Instantiation TU includes Archive.h by relative path.** Archive.h transitively needs only ByteStream.h (+ `<vector>`) and stdlib headers, so the TU compiles with zero per-lib include dirs — `Get-LibIncludes` returning empty for the x64-scratch path is harmless. This keeps the TU at the plan-specified `x64-scratch/` location (D-01 gitignored) without a harness change.
- **Static_assert N baselined to the live compiler.** A standalone 32-bit `cl /c` probe with all 7 asserts compiled exit 0, confirming TargaHeader=18/TargaFooter=26/IntRange=8/VariableUsage=6/UsageIndexEntry=5/LinkIndexEntry=5/CrcLookupEntry=6 against the live compiler (not the RESEARCH literals). The full SwgClient 32-bit build then re-confirmed them in-tree (0 C2338).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Layering-correct overflow guard (assert, not DEBUG_FATAL)**
- **Found during:** Task 1
- **Issue:** The plan suggested `DEBUG_FATAL`/assert for the `>UINT32_MAX` narrowing guard. `DEBUG_FATAL` is a `sharedFoundation` macro and is NOT reachable in the `external/ours/archive` layer (verified: zero `DEBUG_FATAL`/`Fatal.h` usage in the archive lib) — using it would create an upward layering dependency and break the standalone instantiation TU.
- **Fix:** Used `assert(source.size() <= static_cast<size_t>(UINT32_MAX))` (the archive lib's existing house guard) + `#include <cassert>`/`<cstdint>`.
- **Files modified:** `src/external/ours/library/archive/src/shared/Archive.h`
- **Verification:** instantiation TU compiles x64-clean; assert + UINT32_MAX greps == 2.
- **Committed in:** `f9efe220c` (Task 1 commit)

**2. [Rule 3 - Blocking] Added `<stddef.h>` for `offsetof` in both packed-struct TUs**
- **Found during:** Task 2
- **Issue:** The `offsetof` layout guards require `<stddef.h>`/`<cstddef>`; neither TargaFormat.cpp nor AssetCustomizationManager.cpp included it explicitly (relied on transitive availability).
- **Fix:** Added `#include <stddef.h>` to both files.
- **Files modified:** TargaFormat.cpp, AssetCustomizationManager.cpp
- **Verification:** 32-bit SwgClient Debug build green (0 C2338, exe produced).
- **Committed in:** `650848aff` (Task 2 commit)

---

**Total deviations:** 2 auto-fixed (1 missing-critical/layering, 1 blocking include)
**Impact on plan:** Both auto-fixes are correctness/layering requirements with no scope creep. The wire format, struct layouts, and SAFE-by-design paths are all unchanged.

## Issues Encountered
- A fast 32-bit preflight `cl /c` of the two struct TUs failed on `windows.h` (the ad-hoc vcvars32 subshell did not fully populate the Windows SDK INCLUDE). Resolved by relying on (a) the standalone size-only probe (exit 0, confirms the sizeof baselines) and (b) the authoritative full SwgClient 32-bit Debug build (the plan's actual Task 2 verify), which compiled both TUs with 0 C2338 and produced `SwgClient_d.exe`.

## Deferred / Residual Items
- **RESIDUAL-31-05** (deferred-items.md): `archive/.../ByteStream.cpp:347` `reinterpret_cast<unsigned int>(Data*)` C4311 — a BITS-02 pointer-truncation in the freed-memory-poison sentinel assert, in the archive lib but NOT in 31-05's `files_modified`. Classified (a) in-scope MUST-FIX, handed to plan 06. Fix: compare full-width `uintptr_t` against a `uintptr_t` sentinel.
- The pre-existing vector/set/deque `signed int length = source.size()` C4244 is documented as the D-07 exclusion (NOT a defect, did not surface in the bits03 sweep) so the plan-06 gate executor does not mistake it for a survivor.

## Known Stubs
None — no stubs introduced. The instantiation TU is a deliberate scratch-only exerciser (gitignored), not a stub.

## Self-Check: PASSED
- All created/modified files exist on disk (Archive.h, TargaFormat.cpp, AssetCustomizationManager.cpp, archive-map-instantiation.cpp, deferred-items.md).
- All task commits exist in git history: `f9efe220c`, `650848aff`, `f9fefbc21`.

## Next Phase Readiness
- BITS-03 complete: the one real serialization hazard is fixed wire-stable + exercised; the packed structs carry permanent layout guards; the SAFE-by-design paths are untouched; the 32-bit build is green.
- Plan 06 (phase gate) inherits RESIDUAL-31-05 (ByteStream.cpp C4311) plus the RESIDUAL-31-04 set, and must run the dual-renderer boot smoke (which exercises TRE/IFF asset load + the login handshake) as the functional regression proof (D-08).

---
*Phase: 31-64-bit-correctness-foundation*
*Completed: 2026-06-16*
