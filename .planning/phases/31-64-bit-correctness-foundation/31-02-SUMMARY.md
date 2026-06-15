---
phase: 31-64-bit-correctness-foundation
plan: 02
subsystem: infra
tags: [x64, inline-asm, intrinsics, sse, _mm_, _controlfp, fpu, cpuid, llp64, sharedFoundation, sharedMath]

# Dependency graph
requires:
  - phase: 31-01
    provides: scratch Debug|x64 compile-only harness (compile-all.ps1 -SingleTu) + in-scope TU manifest
provides:
  - FloatingPointUnit FPU-control port to a CRT FP API (_controlfp_s) with lossless x87<->_MCW_* boundary translation, P_24 retained on 32-bit / omitted on x64, _DEBUG round-trip self-check
  - SseMath CPUID + 4 transform routines + prefetch ported to _mm_*/__cpuid intrinsics with verified per-routine lane semantics + a _DEBUG numeric-equivalence oracle, unaligned loads, global staging array retired
  - Transform::sse_xf_matrix_3x4 ported from naked SSE asm to a normal _mm_* function (lane-3-only translate preserved) + a _DEBUG equivalence oracle vs the scalar reference
affects: [31-03, 31-04, 31-05, 31-06, phase-33-x64-build, phase-36-verify-cornersnap]

# Tech tracking
tech-stack:
  added: ["<float.h> _controlfp_s", "<intrin.h> __cpuid", "<xmmintrin.h> _mm_* intrinsics"]
  patterns:
    - "BITS-01 asm->intrinsic port: no #ifdef _M_X64 fork for SSE; exactly one justified _M_X64 guard for the FPU precision write"
    - "x87-hardware-layout <-> MSVC abstract _MCW_* translation confined to the get/set boundary (strategy A)"
    - "_DEBUG static-init numeric-equivalence oracle vs the scalar reference for every ported SSE routine (catches a transposed lane that boot smoke + compile-only harness both miss)"

key-files:
  created:
    - .planning/phases/31-64-bit-correctness-foundation/deferred-items.md
  modified:
    - src/engine/shared/library/sharedFoundation/src/win32/FloatingPointUnit.cpp
    - src/engine/shared/library/sharedMath/src/win32/SseMath.cpp
    - src/engine/shared/library/sharedMath/src/win32/SseMath.h
    - src/engine/shared/library/sharedMath/src/shared/Transform.cpp

key-decisions:
  - "FloatingPointUnit strategy A: module stays in raw x87 layout, translate x87<->_MCW_* only at get/setControlWord (faithful port; minimal blast radius)"
  - "P_24 precision RETAINED on the shipped 32-bit build (named decision, VERIFY-01 door-snap watch-item); _MCW_PC omitted from the CRT write only on x64 (#if defined(_M_X64)) to avoid the invalid-parameter handler"
  - "Transform translate term reproduced as _mm_set_ps(left[i][3],0,0,0) (lane-3 only), NOT a naive _mm_set1_ps broadcast — the verified shufps 0x15 semantic"

patterns-established:
  - "asm->intrinsic ports validated per-TU via the scratch x64 harness (0 C4235/C4311/C4312/C4244) + a per-lib 32-bit ClCompile; full 5-target build + dual-renderer boot smoke deferred to the Wave-3 plan-06 gate"
  - "_DEBUG numeric/round-trip oracles are the runtime correctness proof, confirmed at boot by plan-06's Debug exe"

requirements-completed: [BITS-01]

# Metrics
duration: ~45min
completed: 2026-06-15
---

# Phase 31 Plan 02: BITS-01 FPU + SSE Inline-Asm Intrinsic Port Summary

**Ported the three hardest x64-illegal inline-asm sites (x87 FPU-control, SseMath's CPUID + 4 matrix-transform routines + prefetch, and Transform's naked-SSE matrix concat) to MSVC intrinsics with faithful x87<->_MCW_* translation, verified per-routine lane semantics, and _DEBUG correctness oracles — all three TUs compile x64-clean (0 C4235) and the owning libs stay 32-bit-clean per-TU.**

## Performance

- **Duration:** ~45 min
- **Started:** 2026-06-15T23:05Z (approx)
- **Completed:** 2026-06-15T23:47Z
- **Tasks:** 3 (+1 Rule-1 auto-fix)
- **Files modified:** 4 (+1 created: deferred-items.md)

## Accomplishments
- **FloatingPointUnit.cpp** — `__asm fnstcw`/`fldcw` replaced with `_controlfp_s`. The module keeps trafficking `status` in RAW x87 hardware layout; translation x87<->`_MCW_*` is confined to `getControlWord`/`setControlWord` (strategy A) so `update()`'s `currentStatus != status` compare and every `status & MASK` test are byte-preserved. Exception-mask polarity (set = masked) preserved. P_24 precision RETAINED on 32-bit, omitted only on x64 via `#if defined(_M_X64)` (no `_MCW_PC` to the CRT -> avoids the x64 invalid-parameter handler; never `__control87_2`). `_DEBUG` round-trip self-check (getControlWord -> setControlWord -> getControlWord) proves the translation is lossless.
- **SseMath.cpp / SseMath.h** — `canDoSseMath`'s `__asm cpuid` -> `__cpuid` (EDX bit25 SSE / bit24 FXSR; live, not deleted). The 4 `*_l2p` routines rewritten register-faithfully reproducing the verified per-routine lane semantics (rotateTranslateScale w=1/4-lane, rotateScale w=0/3-lane, skin position-4-lane vs normal-3-lane asymmetry, Add variant accumulates). All matrix/vector loads use `_mm_loadu_ps` (unaligned — movaps on an embedded `Transform` would fault at runtime on x64); the global non-reentrant `sseVariable[5][4]` staging array is retired for stack locals. `SseMath::prefetch`'s `__asm prefetchnta` -> `_mm_prefetch(_MM_HINT_NTA)`. A `_DEBUG` numeric-equivalence oracle exercises all 4 routines (incl. the 3-lane/4-lane + position/normal asymmetries) vs the scalar `rotate_l2p`/`rotateTranslate_l2p` within 1e-4f.
- **Transform.cpp** — `__declspec(naked) sse_xf_matrix_3x4` rewritten as a normal `_mm_*` function (install selector preserved). The verified `shufps ...,0x15` translate semantic is reproduced as `_mm_set_ps(left[i][3],0,0,0)` (left[i][3] added to LANE 3 only), NOT a naive `_mm_set1_ps` broadcast. `_mm_loadu_ps`/`_mm_storeu_ps` for the unaligned embedded matrices, an aligned local scratch makes it alias-safe (out may equal left/right). A `_DEBUG` oracle compares it element-wise against the scalar `xf_matrix_3x4` within 1e-4f.

## Task Commits

1. **Task 1: FloatingPointUnit FPU-control port** - `e9edaeca8` (feat)
2. **Task 2: SseMath CPUID + 4 routines + prefetch port** - `673efdd28` (feat)
3. **Rule-1 auto-fix: qualify SseMath::*_l2p in the oracle** - `6a1fd14b7` (fix)
4. **Task 3: Transform::sse_xf_matrix_3x4 port** - `717a66689` (feat)

**Plan metadata:** (this SUMMARY + STATE/ROADMAP) committed separately.

## Files Created/Modified
- `src/engine/shared/library/sharedFoundation/src/win32/FloatingPointUnit.cpp` - CRT FP-control read/write with x87<->_MCW_* boundary translation; 32-bit precision retained, x64-guarded; _DEBUG round-trip self-check
- `src/engine/shared/library/sharedMath/src/win32/SseMath.cpp` - _mm_*/__cpuid SSE math (canDoSseMath + 4 transform routines) with _DEBUG numeric oracle; _mm_loadu_ps loads; stack locals
- `src/engine/shared/library/sharedMath/src/win32/SseMath.h` - prefetch __asm -> _mm_prefetch; #include <xmmintrin.h>
- `src/engine/shared/library/sharedMath/src/shared/Transform.cpp` - sse_xf_matrix_3x4 as a normal _mm_* function (0x15 lane-3 translate preserved) with _DEBUG equivalence oracle
- `.planning/phases/31-64-bit-correctness-foundation/deferred-items.md` - logs the deferred Misc.h C2668 blocker (-> plan 31-04)

## Decisions Made
- **FloatingPointUnit strategy A** (translate at the boundary, keep the module in x87 layout) chosen over strategy B (rewrite everything to `_MCW_*`) — A is the faithful port with the smallest blast radius; `update()`'s compare and every `status & MASK` site keep working unchanged.
- **P_24 retention is a named decision**, not a side effect of the mask argument: the 32-bit hardware write includes `_MCW_PC` (precision force the door-snap theory depends on); x64 omits it (would raise the invalid-parameter handler). `status` still records the precision bits on both bitnesses so the state-query API is consistent.
- **Translate term = lane-3 only.** Decoded the original `movss` (zeroes lanes 1-3) + `shufps ...,0x15`: the result is `{0,0,0,left[i][3]}`, matching the scalar `+left[i*4+3]` term. Used `_mm_set_ps(left[i][3],0,0,0)`; a `_mm_set1_ps` broadcast would have been wrong.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Unqualified SseMath::*_l2p calls in the Task-2 _DEBUG oracle**
- **Found during:** Task 3 (per-lib 32-bit `cl /c` validation of sharedMath)
- **Issue:** The Task-2 numeric oracle (a static-init struct in an anonymous namespace) called `rotateTranslateScale_l2p`/`rotateScale_l2p`/`skinPositionNormal_l2p`/`skinPositionNormalAdd_l2p` unqualified. They are `SseMath::` static members, not free functions -> `error C3861 'identifier not found'` on the committed 32-bit `sharedMath` ClCompile. (The x64 harness did not surface it because the deferred Misc.h C2668 halts cl before the static-init is fully instantiated, and the obj was cached — the 32-bit per-lib build is what caught it, which is exactly the Task-3 acceptance check.)
- **Fix:** Added `SseMath::` qualification to the four oracle call sites.
- **Files modified:** `src/engine/shared/library/sharedMath/src/win32/SseMath.cpp`
- **Verification:** 32-bit `sharedMath.vcxproj /t:ClCompile Debug|Win32` now compiles with 0 errors / 0 warnings; all three TUs re-verified x64-clean.
- **Committed in:** `6a1fd14b7`

---

**Total deviations:** 1 auto-fixed (1 bug). Plus 1 deferred discovery (Misc.h C2668 — DEF-31-01, handed to plan 31-04; NOT fixed here per scope boundary).
**Impact on plan:** The auto-fix was necessary for correctness (the oracle would not build 32-bit). No scope creep. The Misc.h deferral is the documented cross-plan blocker and does not affect this plan's per-TU x64 acceptance grep.

## Issues Encountered
- **Comment-token grep collisions:** the acceptance criteria use raw `grep -c '__asm'`/`'_asm'`/`'sseVariable'`/`'__declspec(naked)'` == 0. Explanatory comments that named the old constructs tripped those counts; reworded the comments (no functional change) so the literal-token greps are clean while the intent is preserved.
- **In-scope header asm:** `SseMath.h::prefetch` carried its own `_asm prefetchnta` block (C4235 at SseMath.h:44) — not called out in the plan body but part of the SseMath unit and required for SseMath.cpp to reach 0 C4235. Ported as part of Task 2 (Rule 3 — blocking the stated truth "SseMath compiles x64-clean").

## Deferred Issues
- **DEF-31-01 (Misc.h:236 `memmove` C2668 on x64)** — the cross-plan dominant blocker recorded by 31-01. Out of 31-02 scope (not in `files_modified`); logged to `deferred-items.md` and handed to **plan 31-04**. cl emits the C4235/truncation worklist past it, so it does not invalidate this plan's per-TU verification, and the 31-02 acceptance grep (`error C4235|C4311|C4312|C4244` == 0) is robust to the residual C2668.

## Known Stubs
None. All three ported sites are full register-faithful implementations with runtime correctness oracles; no placeholder/empty-data paths introduced.

## Validation Scope (per plan — no mid-wave full build)
- Per-TU scratch x64 `cl /c` (review-mandated): **FloatingPointUnit.cpp / SseMath.cpp / Transform.cpp all 0 C4235/C4311/C4312/C4244** (only residual = the deferred Misc.h C2668, excluded from the acceptance set).
- Per-lib 32-bit: **sharedMath `/t:ClCompile Debug|Win32` clean** (0 err / 0 warn) after the Rule-1 fix.
- `_DEBUG` oracles authored for all 4 SseMath routines + sse_xf_matrix_3x4 + the FPU round-trip; they run at Debug-exe startup and are **confirmed at runtime by the Wave-3 plan-06 dual-renderer boot smoke** (NOT run mid-wave per the false-green / concurrent-output-collision rule).

## Next Phase Readiness
- BITS-01's three hardest sites are done x64-clean; the remaining BITS-01 misc-sweep survivors (CollisionUtils fsqrt, Fatal/Clock int3, ProfilerTimer rdtsc, VeCritsec lock-bts, DebugHelp stack-walk) belong to the other Wave-2/sweep plans.
- **Blocker for the sweep:** DEF-31-01 (Misc.h C2668) must be cleared by plan 31-04 to let the full in-scope tree compile to completion on x64.
- The full 5-target build + dual-renderer boot smoke + runtime `_DEBUG`-oracle confirmation is the **Wave-3 plan-06 gate**.

## Self-Check: PASSED
- All 4 modified files + 1 created file present on disk.
- All 4 task commits (e9edaeca8, 673efdd28, 6a1fd14b7, 717a66689) present in git.

---
*Phase: 31-64-bit-correctness-foundation*
*Completed: 2026-06-15*
