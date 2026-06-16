---
phase: 31-64-bit-correctness-foundation
plan: 07
subsystem: infra
tags: [x64, llp64, intrinsics, sse, pointer-truncation, time_t, use-after-free, byteswap, msbuild, win32-api]

# Dependency graph
requires:
  - phase: 31-64-bit-correctness-foundation (plans 01-06)
    provides: "scratch Debug|x64 compile-only harness (31-01); the established intrinsic-port (31-02/03), pointer-width-fix (31-04), and serialization-width (31-05) patterns; the 31-06 full-sweep (B-GAP) worklist + escape-valve commit ba66d6657"
provides:
  - "B-GAP-1: 13 x64-illegal __asm survivors across 6 TUs ported to intrinsics/normal fns (ByteOrder _byteswap_*, 4 allocate-hook trampolines via _ReturnAddress(), RenderWorldServices, and the SSE skinning as lane-faithful _mm_* with a _DEBUG numeric oracle)"
  - "B-GAP-2: the two FUNCTIONAL x64 memory-safety fixes (PathSearch pointer-in-int MARK UAF widened to intptr_t end-to-end; StatusWindow this-pointer via SetWindowLongPtr/GWLP_USERDATA/LONG_PTR) + VoidBindSecond shared-header C4312 + LfgDataTable + AlterScheduler width-correct"
  - "B-GAP-3: per-site STATE-vs-DISPLAY time_t verdicts for GroupObject + SwgCuiGroup (all DISPLAY; epoch state kept full-width)"
  - "Authoritative post-31-07 -Scope all sweep: the 11 enumerated (B-GAP) survivors are CLEARED; surfaced a LARGE undercounted pre-existing AutoDelta* C2665/C2668 surface (DEF-31-07-FULLSWEEP) flagged STOP-and-report"
affects: [31-06 (resume Task 2/3), 31-08 (proposed AutoDelta* gap-closure), 33 (X64 platform / Bink / WaterTest / C4267-N2), 35 (Miles AUDIO-01)]

# Tech tracking
tech-stack:
  added: ["<intrin.h> _byteswap_ulong/_byteswap_ushort/_ReturnAddress", "<xmmintrin.h> _mm_* SSE skinning + _mm_getcsr/_mm_setcsr MXCSR bracket", "if-constexpr (std::is_pointer_v/is_integral_v) trait-routed cast", "Win32 SetWindowLongPtr/GetWindowLongPtr + GWLP_USERDATA + LONG_PTR"]
  patterns: ["naked allocate-hook trampoline -> normal fn capturing the leak owner via _ReturnAddress() (preserves the [ebp+4] caller-addr owner)", "pointer-in-int field -> intptr_t end-to-end (field + accessors + every cast)", "the reference IS the spec: resolve a confusing asm layout by finding the real type (PoseModelTransform != Transform) before porting", "consult-the-crew to break a false layout premise (CONSULT-44)"]

key-files:
  created: []
  modified:
    - "src/engine/shared/library/sharedFoundation/src/win32/ByteOrder.cpp"
    - "src/engine/shared/library/sharedRegex/src/win32/RegexServices.cpp"
    - "src/engine/client/application/Direct3d11/src/shared/MemoryManagerHook.cpp"
    - "src/engine/client/application/Direct3d9/src/shared/MemoryManagerHook.cpp"
    - "src/engine/client/library/clientGraphics/src/shared/RenderWorldServices.cpp"
    - "src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SoftwareBlendSkeletalShaderPrimitive.cpp"
    - "src/engine/shared/library/sharedPathfinding/src/shared/PathSearch.cpp"
    - "src/engine/shared/library/sharedPathfinding/src/shared/PathNode.h"
    - "src/engine/shared/library/sharedStatusWindow/src/win32/StatusWindow.cpp"
    - "src/engine/shared/library/sharedFoundation/src/shared/VoidBindSecond.h"
    - "src/engine/shared/library/sharedGame/src/shared/core/LfgDataTable.cpp"
    - "src/engine/shared/library/sharedObject/src/shared/object/AlterScheduler.cpp"
    - "src/engine/client/library/clientGame/src/shared/object/GroupObject.cpp"
    - "src/engine/client/library/clientGame/src/shared/object/GroupObject.h"
    - "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiGroup.cpp"

key-decisions:
  - "SSE skinning transformArray is PoseModelTransform (a separate align(16) 64-byte COLUMN-MAJOR class, NOT Transform); the asm <<6 stride + col-dot == PoseModelTransform::rotate{Translate}_l2p (CONSULT-44, codex+cursor concur). The _DEBUG oracle references that scalar peer, not raw Transform row-dot."
  - "VoidBindSecond.h is a shared template header: fixed the C4312 via an internal if-constexpr integral->pointer-through-uintptr_t cast (no struct layout change); plan 31-06 Task 2's full 5-target build recompiles all consumers (ABI-cascade noted)."
  - "GroupObject/SwgCuiGroup time_t sites are ALL DISPLAY (seconds-duration UI countdowns); the std::pair<time_t,time_t> epoch MEMBER stays full-width (live state, never serialized raw)."
  - "Post-sweep finding: the 31-06 worklist UNDERCOUNTED the class-(B) surface (~154 TUs still fail, dominated by AutoDelta* C2665/C2668). Flagged STOP-and-report (DEF-31-07-FULLSWEEP), not silently fixed/deferred."

patterns-established:
  - "Pattern: x86 naked-allocate-hook -> normal fn with _ReturnAddress() to preserve the MemoryManager leak owner (the call-site address the asm read off [ebp+4])."
  - "Pattern: pointer-in-int field widening is END-TO-END (field type + get/set accessor signatures + every call-site cast), using intptr_t + reinterpret_cast, never a per-site (int) cast-to-silence."

requirements-completed: [BITS-01, BITS-02, BITS-03]

# Metrics
duration: ~6h 30m
completed: 2026-06-16
---

# Phase 31 Plan 07: 64-bit Correctness Gap Closure Summary

**Cleared the 11 NEW class-(B) x64 escapes the 31-06 full sweep surfaced (13 __asm survivors incl. lane-faithful _mm_* SSE skinning with a _DEBUG oracle; two FUNCTIONAL memory-safety fixes — PathSearch pointer-in-int UAF and StatusWindow this-truncation; a shared-header C4312; time_t audits) — and the authoritative -Scope all sweep then exposed a much larger pre-existing AutoDelta* C2665/C2668 surface the worklist undercounted, flagged STOP-and-report.**

## Performance

- **Duration:** ~6h 30m (incl. two full 2217-TU scratch x64 sweeps + a CONSULT-44 crew round)
- **Started:** 2026-06-16T01:00Z (approx)
- **Completed:** 2026-06-16T03:10Z
- **Tasks:** 4
- **Files modified:** 15

## Accomplishments

- **B-GAP-1 (BITS-01) — 13 __asm survivors / 6 TUs ported, 0 C4235:**
  - `ByteOrder.cpp`: 4 `__declspec(naked)` `bswap` ntohl/htonl/ntohs/htons → `_byteswap_ulong`/`_byteswap_ushort`.
  - `RegexServices.cpp` + `Direct3d9`/`Direct3d11` `MemoryManagerHook.cpp` + `RenderWorldServices.cpp`: naked allocate-hook trampolines → normal fns forwarding to `MemoryManager::allocate`, capturing the leak `owner` via `_ReturnAddress()` (preserves the `[ebp+4]` caller-addr owner the asm read).
  - `SoftwareBlendSkeletalShaderPrimitive.cpp`: the two SSE skinning `__asm` blocks → lane-faithful `_mm_*` intrinsics with `_mm_loadu_ps` column loads, an `_mm_getcsr`/`_mm_setcsr` MXCSR flush-to-zero/exception-mask bracket, and a `_DEBUG` numeric-equivalence oracle vs the scalar `PoseModelTransform` reference.
- **B-GAP-2 (BITS-02) — pointer-truncations, 0 C4311/C4312:** the two FUNCTIONAL x64 corruptions are structurally fixed (PathSearch UAF + StatusWindow `this`), plus the VoidBindSecond shared-header, LfgDataTable, and AlterScheduler.
- **B-GAP-3 (BITS-03) — time_t audit:** each of the 4 narrowing sites carries a STATE-vs-DISPLAY verdict; all DISPLAY (the epoch pair member stays full-width).
- **Authoritative `-Scope all` sweep (2217 TUs):** the 11 enumerated (B-GAP) survivors are CLEARED; surfaced + flagged a large undercounted surface (see Deviations / DEF-31-07-FULLSWEEP).

## Task Commits

1. **Task 1: B-GAP-1 trivial __asm ports** — `8479fb6ba` (fix)
2. **Task 2: B-GAP-1 SSE skinning → _mm_* + _DEBUG oracle** — `d9ee92088` (fix)
3. **Task 3: B-GAP-2 pointer-truncations (2 functional + shared header)** — `8ae95abc6` (fix)
4. **Task 4: B-GAP-3 time_t audit + Rule-3 callback fix** — `4f7b1a99a` (fix)

## Files Created/Modified

(15 source files — see frontmatter key-files.) Highlights:
- `PathNode.h` / `PathSearch.cpp` — `m_marks[4]` int → `intptr_t` end-to-end (the UAF fix).
- `StatusWindow.cpp` — `SetWindowLongPtr`/`GetWindowLongPtr` + `GWLP_USERDATA` + `LONG_PTR` (the `this` fix).
- `SoftwareBlendSkeletalShaderPrimitive.cpp` — 687-line net reduction (asm + dead reference blocks → compact `_mm_*` + oracle).
- `VoidBindSecond.h` — shared template header; `convertBindArgument<>` if-constexpr helper.
- `GroupObject.h`/`.cpp` — time_t DISPLAY casts + the Rule-3 AutoDeltaVector callback `size_t`→`unsigned int` fix.

## Per-site time_t STATE-vs-DISPLAY verdicts (B-GAP-3)

| Site | Use | Verdict | Fix |
|------|-----|---------|-----|
| `GroupObject.cpp:413` (`getSecondsLeftOnGroupPickup`) | seconds-remaining for a UI countdown | **DISPLAY** | `static_cast<unsigned int>` (epoch pair member stays `time_t`) |
| `GroupObject.cpp:424` (`getGroupPickupDurationSeconds`) | total-duration seconds for the same UI | **DISPLAY** | `static_cast<unsigned int>` |
| `SwgCuiGroup.cpp:269` (`width = GetWidth()*timerEnd/timerTotal`) | timer-bar pixel width | **DISPLAY** | retype `timerEnd`/`timerTotal` `time_t`→`unsigned int` (matches the getter contract) |
| `SwgCuiGroup.cpp:272` (`convertSecondsToMS(timerEnd)`) | seconds→M:S label string | **DISPLAY** | same retype (`convertSecondsToMS` takes `unsigned int`) |

No site is STATE/serialized — the `std::pair<time_t,time_t> m_groupPickupTimerClientEpoch` MEMBER is live client-epoch state kept full-width and never serialized raw (the wire value is converted on receipt), so no wire-width pin was needed (D-07).

## VoidBindSecond.h shared-header ABI-cascade note

`sharedFoundation/.../VoidBindSecond.h` is a SHARED TEMPLATE HEADER consumed by many libs and all 4 renderer plugins (gl05/06/07/11). The C4312 fix is a template-INTERNAL conversion change (the new `convertBindArgument<>` if-constexpr helper) with **no struct layout change**, so it carries no stale-plugin ABI risk by itself. Nonetheless, per AGENTS.md's shared-header rule, plan 31-06 Task 2's full canonical 5-target build (Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient) must recompile ALL consumers so no consumer links against a stale template instantiation.

## Decisions Made

- **SSE skinning layout (CONSULT-44).** The asm appeared to contradict the scalar peer (col-dot ×64 stride vs row-dot ×48). Resolved by finding the real type: `transformArray` is `PoseModelTransform` — a SEPARATE `__declspec(align(16))` `float[4][4]` (64-byte) COLUMN-MAJOR class, NOT `Transform`. The asm faithfully computes `PoseModelTransform::rotate{Translate}_l2p`, which is the scalar reference. Codex + Cursor independently concurred (`.planning/research/CONSULT-44-*.out`). The SSE path is LIVE (`s_useSSE = canDoSseMath()` on WIN32, true on modern CPUs), so the oracle is load-bearing.
- **PathSearch mark widened to `intptr_t` (not per-site cast).** Mark slot 3 round-trips a live `PathSearchNode*`; the field + `getMark`/`setMark` + every cast were widened so the pointer never truncates. Marks are runtime-only ("not persisted") so no serialization concern.
- **VoidBindSecond fixed in the header via trait-routed cast,** not at each call site — keeps the shared abstraction correct for the integral-literal→pointer instantiation (`VoidBindSecond(..., 0)`).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] GroupObject AutoDeltaVector callback signatures size_t → unsigned int**
- **Found during:** Task 4 (compiling GroupObject.cpp to verify the time_t fix).
- **Issue:** `membersOnInsert/Erase` + `memberShipsOnInsert/Erase` were declared `(const size_t, ...)`, but `Archive::AutoDeltaVector`'s `setOnInsert/setOnErase` callback contract is `(const unsigned int, ...)`. `size_t == unsigned int` only on 32-bit; on x64 the pointer-to-member type mismatched → C2664, blocking the TU from compiling x64-clean.
- **Fix:** changed the four callback signatures (header + cpp) to `const unsigned int` to match the wire-stable AutoDeltaVector index contract. These fns are used ONLY as AutoDeltaVector callbacks.
- **Files modified:** GroupObject.h, GroupObject.cpp.
- **Verification:** GroupObject.cpp now compiles x64 exit 0.
- **Committed in:** `4f7b1a99a` (Task 4 commit).

### Out-of-scope discoveries (logged to deferred-items.md, NOT fixed — scope boundary / Rule-4)

- **DEF-31-07:** 2 `std::distance`→`int` C4244 (EditableAnimationState.cpp:275, EditableAnimationStateHierarchyTemplate.cpp:257) — D-07-EXCLUDED count/distance class (N2). Surfaced while exercising VoidBindSecond's C4312 fix (which IS fixed). Not pointer-truncation, not in scope.
- **DEF-31-07-AUTODELTAMAP → subsumed by DEF-31-07-FULLSWEEP.**
- **DEF-31-07-FULLSWEEP (the headline finding):** the authoritative post-31-07 `-Scope all` sweep shows the 31-06 "(B-GAP) survivor set" was INCOMPLETE — ~154 in-scope TUs still fail x64 compile (ALL pre-existing; none are this plan's 11 targets). Dominated by **~753 C2665/C2668** from the `AutoDeltaMap`/`AutoDeltaPackedMap` + Tpf/object-template family (`Archive::put(container.size())` / `get(size_t)` overload-resolution failure on x64 — the SAME class 31-05 fixed for plain `Archive::get/put(std::map)`, plus the `size_t baselineCommandCount` member; the C2065/C2059 tail is largely cascade). Long tail: Miles `Audio.cpp` C2664 (→ Phase 35 AUDIO-01), Bink `BinkTreeFileIO`/`BinkVideo` (→ Phase 33 X64-04, U32-handle is Bink-API-bound), and D-07/N2 C4244. Flagged STOP-and-report per Rule-4 (NOT in the worklist, exceeds this plan's 4-task scope); recommended a follow-up Phase-31 increment (31-08) for the AutoDelta* family before 31-06 Task 2/3 can certify x64-clean.

---

**Total deviations:** 1 auto-fixed (Rule 3 blocking) + 3 logged out-of-scope (1 N2 C4244, 1 subsumed, 1 large STOP-and-report finding).
**Impact on plan:** the Rule-3 fix was necessary to compile the time_t TU. No scope creep — the AutoDelta* surface was reported, not fixed. The plan's own 4-task scope (the 11 enumerated B-GAP survivors) is complete and verified clean.

## Issues Encountered

- **worklist.log clobber trap:** the harness rewrites `worklist.log` every run, so interleaving a `-SingleTu` test during the long `-Scope all` analysis destroyed the sweep's per-TU error record (the filtered WORKLIST stdout only carries C4235/C4311/C4312/C4244, NOT the C2665/C2668 that dominate). Resolved by re-running a CLEAN `-Scope all` with NO interleaved single-TU compiles and snapshotting `worklist.log` to `.planning/research/scope-all-31-07-worklist.log`.
- **SSE asm layout false-premise:** initially mis-assumed `PoseModelTransform == Transform`; broke it with a CONSULT-44 crew round (neutral evidence, codex+cursor converged on the separate 64-byte column-major type).

## Next Phase Readiness

- The 11 enumerated (B-GAP) survivors are CLEARED — the two FUNCTIONAL memory-safety fixes (PathSearch UAF, StatusWindow `this`) are in, the SSE skinning has a runtime oracle, the shared-header is width-correct.
- **BLOCKER for the phase gate:** plan 31-06's D-02 acceptance ("all in-scope build-path engine libs COMPILE x64-clean") is STILL NOT MET — the AutoDelta* C2665/C2668 surface (~154 TUs) remains. A user decision is required (recommended: a 31-08 increment for the AutoDelta* family using the 31-05 uint32_t pattern; Miles → Phase 35; Bink → Phase 33). 31-06 Task 2/3 (full 32-bit build + dual-renderer boot smoke) can validate the 32-bit non-regression of THIS plan's 15-file change set now.
- The RESIDUAL-31-06 BLOCKING entry's specific gap-closure (the 11 enumerated survivors) is done; STATE.md updated accordingly, with DEF-31-07-FULLSWEEP recorded as the new open item.

---
*Phase: 31-64-bit-correctness-foundation*
*Completed: 2026-06-16*

## Self-Check: PASSED

- 31-07-SUMMARY.md present.
- All 4 task commits present in git: 8479fb6ba, d9ee92088, 8ae95abc6, 4f7b1a99a.
- All 11 (B-GAP) target TUs verified x64-clean (per-TU + full -Scope all sweep); none appear in the 154 failing TUs.
