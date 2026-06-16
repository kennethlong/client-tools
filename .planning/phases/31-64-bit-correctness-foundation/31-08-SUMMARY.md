---
phase: 31-64-bit-correctness-foundation
plan: 08
subsystem: serialization
tags: [x64, llp64, archive, autodelta, serialization-width, bits-03, uint32, overload-resolution]

# Dependency graph
requires:
  - phase: 31-05
    provides: "the Archive std::map uint32_t wire-count pin pattern (commit f9efe220c) this plan mirrors across the AutoDelta* family"
  - phase: 31-01
    provides: "the gitignored Debug|x64 compile-only scratch harness (compile-all.ps1 + manifest) used to verify"
  - phase: 31-07
    provides: "the authoritative -Scope all sweep that exposed the ~753 AutoDelta* C2665/C2668 surface (DEF-31-07-FULLSWEEP) + the GroupObject const-size_t->unsigned-int callback fix precedent"
provides:
  - "AutoDeltaMap/PackedMap/Set/Vector x64-clean serialization (every wire count binds the 4-byte Archive overload on LLP64 x64; wire byte-identical to the 32-bit server)"
  - "the ~753 AutoDeltaMap/AutoDeltaPackedMap C2665/C2668 overload-resolution surface + its ~125-error C2065/C2059/C2143/C2238 cascade tail CLEARED"
  - "a NEW authoritative -Scope all snapshot showing 0 AutoDelta* header errors, with the residual surface re-classified"
affects: [31-06, 33, 35]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "AutoDelta* serialized COUNT (member + put/get + loop locals) pinned to uint32_t -> binds the existing 4-byte Archive overload on x86 AND LLP64 x64; wire byte-identical (the 31-05 std::map pattern, family-wide)"
    - "AutoDeltaContainer consumer callbacks that take a container index MUST be (const unsigned int), not (const size_t) -- size_t==unsigned int only on 32-bit (the 31-07 GroupObject precedent, now applied to CreatureObject)"

key-files:
  created: []
  modified:
    - src/external/ours/library/archive/src/shared/AutoDeltaMap.h
    - src/external/ours/library/archive/src/shared/AutoDeltaPackedMap.h
    - src/external/ours/library/archive/src/shared/AutoDeltaSet.h
    - src/external/ours/library/archive/src/shared/AutoDeltaVector.h
    - src/engine/client/library/clientGame/src/shared/object/CreatureObject.h
    - src/engine/client/library/clientGame/src/shared/object/CreatureObject.cpp

key-decisions:
  - "Member-type fix (not per-site casts): retype each family's baselineCommandCount/m_baselineCommandCount MEMBER + the unpackDelta count locals to uint32_t, so the skip arithmetic stays type-consistent AND modular-identical to the 32-bit build (where size_t was also 4 bytes). A template member-type change has no committed-ABI cascade (templates recompile in every consumer)."
  - "The include/Archive/AutoDelta*.h copies are thin FORWARDERS (#include \"../../src/shared/AutoDelta*.h\"), NOT divergent copies -- so fixing src/shared fixes both consumer include paths with one edit. (Plan premise of 'two copies that DIFFER' was stale; on-disk reality is a forwarder. Documented per the builds-are-truth convention.)"
  - "AutoDeltaPackedMap has no .size() count put (it uses countCharacter() which returns int -> already binds the 4-byte signed overload), so it needs no UINT32_MAX assert -- only the placeholder + get locals retyped."

patterns-established:
  - "Family-wide uint32_t count pin with assert(<=UINT32_MAX) guard before every narrowing put cast (the archive-lib house guard; DEBUG_FATAL lives above external/ours)"
  - "A gitignored scratch instantiation TU (autodelta-instantiation.cpp) that forces every pack/packDelta/unpack/unpackDelta member to COMPILE on x64 -- the exercise-it proof, since zero AutoDelta* instantiations compile under the scoped sweeps"

requirements-completed: [BITS-03]

# Metrics
duration: 2h 30m
completed: 2026-06-16
---

# Phase 31 Plan 08: AutoDelta* x64 Count-Width Gap-Closure Summary

**The ~753 pre-existing AutoDeltaMap/AutoDeltaPackedMap `C2665`/`C2668` x64 overload-resolution surface (and its ~125-error parse cascade tail) is CLEARED by pinning every AutoDelta* serialized count to `uint32_t` across all four families in both consumer include paths (wire byte-identical to the 32-bit server), exercised by a scratch instantiation TU and confirmed by an authoritative `-Scope all` sweep that now shows 0 AutoDelta header errors.**

## Performance

- **Duration:** ~2h 30m (dominated by two full 2218-TU x64 `-Scope all` sweeps)
- **Started:** 2026-06-16T02:15:00Z (approx)
- **Completed:** 2026-06-16T04:45:00Z
- **Tasks:** 3 (+ 1 Rule-3 deviation fix)
- **Files modified:** 6 (4 AutoDelta headers + CreatureObject.h/.cpp)

## Accomplishments
- **AutoDelta* count-width fix, family-wide, both copies:** every serialized count (`container.size()`/`changes.size()`/`data.size()`/`m_set.size()`/`v.size()`/`m_commands.size()`, the `baselineCommandCount`/`m_baselineCommandCount` MEMBER, and all unpack/unpackDelta count locals) pinned to `uint32_t` so it binds the existing 4-byte `Archive::put/get(unsigned int)` overload on both x86 and LLP64 x64. A raw `size_t` is 8 bytes on x64 with no Archive overload -> overload-resolution FAILURE (a compile error, not a silent wire widen).
- **Wire byte-identical:** counts were always 4-byte fields; element put/get (cmd/key/value bytes) byte-unchanged; no 8-byte count is ever written; the `unpackDelta` skip arithmetic stays modular-identical to the 32-bit build.
- **Overflow-guarded:** `assert(n <= static_cast<size_t>(UINT32_MAX))` before every narrowing `.size()` put cast (mirrors 31-05).
- **Exercised:** a gitignored scratch `autodelta-instantiation.cpp` instantiates AutoDeltaMap/PackedMap/Set/Vector<int,int>, calls the static pack/unpack round-trips, and odr-uses the non-static (de)serialization members -> compiles x64 exit 0.
- **The ~753 surface CLEARED:** authoritative `-Scope all` (2218 TUs) shows **0 AutoDeltaMap/PackedMap/Set/Vector header errors** (was 744 C2665/C2668 from AutoDeltaMap.h + 8 from AutoDeltaSet.h, ~753 total) and the **~125-error C2065/C2059/C2143/C2238 cascade tail is entirely gone**.

## Task Commits

1. **Task 1: AutoDeltaMap.h uint32_t count pin (both copies)** - `1b6a98ff4` (fix)
2. **Task 2: AutoDeltaPackedMap/Set/Vector uint32_t count pin (both copies)** - `5b5f08a2f` (fix)
3. **Rule-3 deviation: CreatureObject attributesOnSet callback const size_t -> unsigned int** - `846a2ded6` (fix)

Task 3 (the scratch instantiation TU + authoritative `-Scope all`) produced no tracked source diff — the instantiation TU, the `autodelta` scope filter, and the sweep snapshots all live under the gitignored `x64-scratch/` and `.planning/research/`. Its verification is folded into the commits above and the sweep artifacts below.

**Plan metadata:** (this commit) `docs(31-08): complete AutoDelta* gap-closure plan`

## Files Created/Modified
- `src/external/ours/library/archive/src/shared/AutoDeltaMap.h` - baselineCommandCount member + all count put/get + asserts -> uint32_t
- `src/external/ours/library/archive/src/shared/AutoDeltaPackedMap.h` - internal_pack/internal_unpack count locals + placeholder -> uint32_t
- `src/external/ours/library/archive/src/shared/AutoDeltaSet.h` - m_baselineCommandCount member + all count put/get + asserts -> uint32_t
- `src/external/ours/library/archive/src/shared/AutoDeltaVector.h` - baselineCommandCount member + all count put/get + asserts -> uint32_t (Command::index stays unsigned short by design)
- `src/engine/client/library/clientGame/src/shared/object/CreatureObject.h` - attributesOnSet param const size_t -> const unsigned int (decl)
- `src/engine/client/library/clientGame/src/shared/object/CreatureObject.cpp` - attributesOnSet param const size_t -> const unsigned int (def)

**Scratch (gitignored, not committed):** `src/build/win32/x64-scratch/autodelta-instantiation.cpp` (the exercise-it TU), the `autodelta` scope filter + archive-include special-case in `compile-all.ps1`, and the manifest append. Snapshots: `.planning/research/scope-all-31-08-final.out` (stdout) + `.planning/research/scope-all-31-08-worklist.log` (authoritative per-TU error detail).

## Before / After -Scope all (2218 TUs)

| Metric | 31-07 baseline (pre-fix) | 31-08 final (post-fix) |
|--------|--------------------------|------------------------|
| AutoDelta* `C2665`/`C2668` (header) | **~753** (744 AutoDeltaMap.h + 8 AutoDeltaSet.h + 1) | **0** |
| `C2065`/`C2059`/`C2143`/`C2238` parse cascade | ~125 (53+34+23+15) | **0** |
| `C2664` (callback / API mismatch) | (masked) | 8 (1 was CreatureObject AutoDelta -> FIXED; 7 residual non-AutoDelta) |
| `C4244` `__int64`->int (D-07/N2 count/distance) | 75 | 75 (unchanged — documented class-A) |
| `C4311`/`C4312` (Bink + WaterTest + Direct3d9) | 8 | 8 (unchanged — documented class-A) |
| Failing TUs (in-scope) | ~154 | 55 |

Per-family determination:
- **AutoDeltaMap.h** — FIXED (member + 3 size() puts + placeholder + all unpack/unpackDelta locals).
- **AutoDeltaPackedMap.h** — FIXED (internal_pack/internal_unpack locals + placeholder; no size() put -> no assert needed; derives the now-fixed base).
- **AutoDeltaSet.h** — FIXED (m_baselineCommandCount member + 3 size() puts + placeholder + all unpack/unpackDelta locals).
- **AutoDeltaVector.h** — FIXED (baselineCommandCount member + 3 size() puts + placeholder + all unpack/unpackDelta locals; Command::index stays unsigned short).

## Decisions Made
- **Retype the member, not per-site casts.** Each family's `baselineCommandCount` MEMBER and the `unpackDelta` count locals (`commandCount`, `targetBaselineCommandCount`, `skipCount`, shadowing locals) are `uint32_t`, keeping the skip arithmetic type-consistent and modular-identical to the 32-bit build. No committed-ABI cascade (templates recompile in every consumer; the 31-06 full build rebuilds them).
- **One edit fixes both include paths.** The `include/Archive/AutoDelta*.h` copies are thin `#include` forwarders to `src/shared/...`, not divergent copies — verified on disk. (The plan's "two copies that DIFFER" premise was stale; reality is a forwarder. Documented per builds/files-are-truth.)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] CreatureObject attributesOnSet callback `const size_t` -> `const unsigned int`**
- **Found during:** Task 3 (the authoritative `-Scope all` sweep, once AutoDeltaVector.h became x64-compilable)
- **Issue:** `CreatureObject.cpp:608` registers `attributesOnSet` as an `AutoDeltaVector<Attributes::Value,CreatureObject>::setOnSet` callback, but the member function took `(const size_t elem, ...)`. `size_t == unsigned int` only on 32-bit; on x64 the pointer-to-member type mismatched setOnSet's deliberately wire-stable `(const unsigned int, ...)` contract -> `C2664`. This was previously MASKED by the AutoDeltaVector header failing to compile at all; the fix unmasked it. Pre-existing (0 Phase-31 commits touched the file; the line dates to 2015).
- **Fix:** retype the `elem` param to `unsigned int` in both the header decl and the .cpp def (elem is only used in an `==` comparison; no width assumption). Mirrors the 31-07 GroupObject `members/memberShips` callback fix exactly.
- **Files modified:** CreatureObject.h, CreatureObject.cpp
- **Verification:** CreatureObject.cpp compiles x64 exit 0.
- **Committed in:** `846a2ded6`

---

**Total deviations:** 1 auto-fixed (1 blocking, same AutoDelta family / 31-07 precedent).
**Impact on plan:** The deviation is squarely inside the AutoDelta* family scope (a consumer callback against the very header this plan fixed) and follows an established 31-07 precedent — no scope creep.

## Issues Encountered
- **Harness `-Scope autodelta` + instantiation include:** added an `autodelta` scope filter and a special-case archive-include for the scratch TU (AutoDeltaPackedMap.h pulls `Archive/AutoDeltaMap.h`, which needs the lib include root). Harness-only, gitignored.
- **Two full sweeps run:** the first `-Scope all` worklist.log was clobbered by per-TU verification compiles; a second authoritative sweep was re-run post-all-fixes to produce the clean `scope-all-31-08-worklist.log` snapshot.

## STOP-and-REPORT: a NEW pre-existing class-(B) surface was UNMASKED by this fix (Rule-4)

The `-Scope all` sweep cleared the ~753 AutoDelta surface AND its cascade tail. With the cascade gone, **16 in-scope TUs now show a NON-`C4244` fatal that was previously MASKED** (each of these TUs transitively included an AutoDelta header and aborted early in the 31-07 baseline). ALL are pre-existing (0 Phase-31 commits touched them) and NONE are the AutoDelta* family or documented class-(A). Per the gap-closure Rule-4 discipline ("a NEW class-(B) escape that is NOT the AutoDelta* family and NOT documented class-(A) -> STOP and report, do not silently defer"), they are surfaced here for a USER decision (logged in deferred-items.md §DEF-31-08-UNMASKED):

| TU | Error | Root cause (pre-existing) |
|----|-------|---------------------------|
| CuiCombatManager.cpp:1898 | C2665 | `Unicode::getFirstToken(str, pos, pos, ...)` — `pos` is `unsigned int` but param 2 is `size_t&`; `unsigned int&` can't bind `size_t&` on x64 |
| Filename.cpp:327/334/336/338/344 | C2440 + 4×C2664 | `wchar_t[]` / `u"..."` literal -> `std::basic_string<char16_t>` mismatch (MSVC18 `char16_t`!=`wchar_t` strictness) |
| TemplateData.cpp:1751/1753 | 2×C2440 | same char16_t/wchar_t string-literal class |
| TpfFile.cpp:302/310/313 | 2×C2677 + C2664 | `operator+` on `Unicode::String` (char16_t) — same Unicode-type class |
| MeshConstructionHelper.cpp:556/1405/1432/1454/1475 | 5×C2672 | no matching overload (size_t arg) |
| TcpClient.cpp:544, TcpServer.cpp:123 | 2×C2664 | size_t-arg API mismatch in the network lib |
| Connection/NetworkHandler/Service/Sock/UdpSock.cpp | 5×C2371 | redefinition (winsock/platform header-order in the scratch harness; likely harness-config + pre-existing) |
| Direct3d9.cpp:226, Direct3d9_StaticShaderData.cpp | C1189, C4716 | `#error must define FFP, VSPS, or both` — scratch-harness CONFIG artifact (the real 5-target build sets the define per gl05/06/07 config); NOT a code defect |

**Recommendation (USER DECISION):** spawn a follow-up Phase-31 increment (31-09) for the genuine class-(B) members (the Unicode `char16_t`/`wchar_t` string-literal cluster in Filename/TemplateData/TpfFile/MeshConstructionHelper, CuiCombatManager getFirstToken, and the network size_t-arg C2664), exclude the Direct3d9 `#error` C1189/C4716 (harness-config artifact, not a defect) and the network C2371 (verify whether harness-config or real), THEN resume 31-06 Task 2/3. This is the same unmask-the-next-layer pattern that 31-06 -> 31-07 -> 31-08 followed.

## Next Phase Readiness
- **The plan's stated objective is fully met:** the dominant ~753-error AutoDelta* class-(B) surface is cleared x64-clean; the BITS-03 serialization-width mandate now holds for the entire AutoDelta* family.
- **31-06 Task 2/3 (full 32-bit build + dual-renderer boot smoke):** can validate the 32-bit non-regression of ALL Phase-31 changes NOW. The x64-clean certification (D-02) is NOT yet complete — the unmasked class-(B) surface above must be cleared first (recommended 31-09).
- **Documented class-(A) residue unchanged and untouched:** Miles Audio.cpp (-> P35 AUDIO-01), Bink BinkTreeFileIO/BinkVideo (-> P33 X64-04), WaterTestAppearance + the 75 D-07/N2 `C4244` `__int64`->int count/distance class (-> P33).

## Self-Check: PASSED

- All 6 modified source files exist on disk.
- All 3 task commits exist (1b6a98ff4 / 5b5f08a2f / 846a2ded6).
- Authoritative `-Scope all` worklist confirms 0 AutoDelta* header errors (the ~753 surface cleared).

---
*Phase: 31-64-bit-correctness-foundation*
*Completed: 2026-06-16*
