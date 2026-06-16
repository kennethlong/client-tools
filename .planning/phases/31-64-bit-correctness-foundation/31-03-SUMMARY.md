---
phase: 31-64-bit-correctness-foundation
plan: 03
subsystem: infra
tags: [x64, bits-01, inline-asm, intrinsics, __rdtsc, __debugbreak, sqrtf, _interlockedbittestandset, RtlCaptureContext, stack-walk]

# Dependency graph
requires:
  - phase: 31-64-bit-correctness-foundation (plan 01)
    provides: scratch Debug|x64 compile-only harness (compile-all.ps1 -SingleTu) — the per-TU x64-clean signal generator
provides:
  - "BITS-01 misc __asm sweep complete: CollisionUtils (sqrtf), Fatal/Clock (__debugbreak), ProfilerTimer (__rdtsc, __int64 kept), VeCritsec MSVC spinlock (_interlockedbittestandset), DebugHelp (RtlCaptureContext x64 fork)"
  - "The phase's ONE justified #if defined(_M_X64) bitness fork (DebugHelp register capture)"
  - "Two PHASE-33 RUNTIME RESIDUALs handed to plan 06's worklist (DebugHelp x64 unwind walk; uint32* callStack Rip narrowing)"
affects: [31-04, 31-05, 31-06, phase-33-x64-build-platform]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "1:1 intrinsic replacement for x64-illegal __asm with NO #ifdef fork (sqrtf/__debugbreak/__rdtsc/_interlockedbittestandset compile both bitnesses)"
    - "Return-type-preserving static_cast<__int64>(__rdtsc()) keeps caller signatures intact (no C4244)"
    - "Single justified #if defined(_M_X64) fork ONLY where the register set is inherently bitness-specific (DebugHelp)"
    - "Compile-clean != function-clean: x64 runtime-walk correctness explicitly flagged as a Phase-33 residual, not silently 'done'"

key-files:
  created:
    - .planning/phases/31-64-bit-correctness-foundation/31-03-SUMMARY.md
  modified:
    - src/engine/shared/library/sharedCollision/src/shared/core/CollisionUtils.cpp
    - src/engine/shared/library/sharedFoundation/src/shared/Fatal.cpp
    - src/engine/shared/library/sharedFoundation/src/shared/Clock.cpp
    - src/engine/shared/library/sharedDebug/src/win32/ProfilerTimer.cpp
    - src/engine/client/library/clientGame/src/shared/HTTPpost/VeCritsec.hpp
    - src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp

key-decisions:
  - "ProfilerTimer keeps its __int64 return via static_cast<__int64>(__rdtsc()) so no caller signature breaks and no C4244 fires (review)"
  - "VeCritsec uses a C-style (long*) cast (not reinterpret_cast) because the intrinsic takes long* and the cast must strip m_iLock's volatile; the intrinsic is a full barrier so atomicity is preserved"
  - "DebugHelp x64 .Offset fields take the FULL 64-bit context.Rip/Rsp/Rbp with NO DWORD truncation (review #6)"
  - "DebugHelp x64 backtrace WALK + the uint32* output Rip narrowing are recorded as Phase-33 RUNTIME residuals, not fixed here (Phase-31 bar = compile-clean, Open-Q3)"

patterns-established:
  - "Pattern: x64-illegal __asm -> direct intrinsic, no bitness fork unless the register set itself differs"
  - "Pattern: flag any compile-clean-but-runtime-wrong x64 site as an explicit Phase-33 residual in code AND in the plan-06 worklist"

requirements-completed: [BITS-01]

# Metrics
duration: ~25min
completed: 2026-06-15
---

# Phase 31 Plan 03: BITS-01 Misc __asm Sweep Summary

**Replaced the six remaining in-scope x64-illegal `__asm` survivors with intrinsics — `sqrtf` (CollisionUtils), `__debugbreak` (Fatal/Clock), `__rdtsc` keeping the `__int64` return (ProfilerTimer), `_interlockedbittestandset` (VeCritsec MSVC spinlock), and the phase's one justified `#if defined(_M_X64)` `RtlCaptureContext` fork (DebugHelp) — all compiling x64-clean (0 C4235).**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-06-15T23:33Z (approx)
- **Completed:** 2026-06-15T23:59Z
- **Tasks:** 3
- **Files modified:** 6

## Accomplishments
- Four trivial instruction sites swept to intrinsics (CollisionUtils `sqrtf`, Fatal/Clock `__debugbreak`, ProfilerTimer `__rdtsc`); ProfilerTimer's `__int64` signature preserved via `static_cast<__int64>(__rdtsc())` so all callers and the `getTime`/`getCalibratedTime`/install path are unchanged and no C4244 narrowing fires.
- VeCritsec MSVC `_WIN32` spinlock ported to `_interlockedbittestandset((long*)&m_iLock, 0)` with the equivalent acquire/fail control flow (prev bit 1 -> fail -> false; 0 -> acquired -> fall through); the `#else` GCC `__asm__` branch left byte-unchanged.
- DebugHelp stack-walk given the phase's ONE justified `#if defined(_M_X64)` fork: x64 uses `RtlCaptureContext` + full 64-bit `Rip/Rsp/Rbp` `.Offset` (no DWORD truncation) + `IMAGE_FILE_MACHINE_AMD64`; the 32-bit `__asm GetEIP` grab + `IMAGE_FILE_MACHINE_I386` path is untouched.
- All six files verified x64-clean in the scratch harness (0 C4235/C4311/C4312/C4244 per TU); the only residual error across the touched TUs is the pre-existing DEF-31-01 `Misc.h:236` memmove C2668 owned by plan 31-04.

## Task Commits

Each task was committed atomically:

1. **Task 1: Sweep trivial instruction sites (fsqrt, int 3, rdtsc)** - `a4f711419` (fix)
2. **Task 2: Port VeCritsec spinlock to _interlockedbittestandset** - `379920283` (fix)
3. **Task 3: x64 DebugHelp stack-walk via RtlCaptureContext + Phase-33 residual flag** - `5a8924b8c` (feat)

**Plan metadata:** committed separately with this SUMMARY + STATE/ROADMAP updates.

## Files Created/Modified
- `src/engine/shared/library/sharedCollision/src/shared/core/CollisionUtils.cpp` - x87 fld/fsqrt/fstp -> `sqrtf` (added `#include <cmath>`)
- `src/engine/shared/library/sharedFoundation/src/shared/Fatal.cpp` - inline-asm int-3 trap -> `__debugbreak()` (added `#include <intrin.h>`)
- `src/engine/shared/library/sharedFoundation/src/shared/Clock.cpp` - inline-asm int-3 trap -> `__debugbreak()` (added `#include <intrin.h>`; the site is inside a `#if 0` debug block but still swept to keep the file asm-free)
- `src/engine/shared/library/sharedDebug/src/win32/ProfilerTimer.cpp` - naked `__stdcall` rdtsc wrapper -> normal fn returning `static_cast<__int64>(__rdtsc())` (added `#include <intrin.h>`)
- `src/engine/client/library/clientGame/src/shared/HTTPpost/VeCritsec.hpp` - MSVC `lock bts` spinlock -> `_interlockedbittestandset` (added `#include <intrin.h>`); GCC branch untouched
- `src/engine/shared/library/sharedDebug/src/win32/DebugHelp.cpp` - `#if defined(_M_X64)` RtlCaptureContext + AMD64 fields branch; 32-bit asm path unchanged; two Phase-33 runtime residuals flagged

## Decisions Made
- **C-style `(long*)` cast in VeCritsec** rather than `reinterpret_cast`: the intrinsic prototype is `_interlockedbittestandset(long*, long)`, and `m_iLock` is `volatile unsigned int`. `reinterpret_cast<long*>` cannot drop `volatile` (it produced a real C2440 on first attempt — see Issues). The C-style cast legally strips `volatile`; the intrinsic is a full memory barrier so atomic-acquire semantics are unaffected. This matches the plan's literal `(long*)&m_iLock` spec.
- **DebugHelp `.Offset` full-width**: assigned `context.Rip/Rsp/Rbp` directly to the `DWORD64` STACKFRAME64 fields with no `DWORD(...)` wrapper (review #6) so the address is not truncated before the walk.
- **Comments scrubbed of literal `__asm`/`naked`/`lock bts`/`__declspec(naked)` tokens** so the plan's grep acceptance counts (`__asm`==0, `lock bts`==0, `__declspec(naked)`==0) are literally clean and future asm-survivor sweeps don't false-positive on documentation. The compiler's 0-C4235 result is the authoritative no-asm proof.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] VeCritsec reinterpret_cast could not strip `volatile` (C2440)**
- **Found during:** Task 2 (VeCritsec spinlock port)
- **Issue:** The first implementation used `reinterpret_cast<long*>(&m_iLock)`. Because `m_iLock` is `volatile unsigned int`, this produced `error C2440: 'reinterpret_cast': cannot convert from 'volatile unsigned int *' to 'long *'` in the x64 harness — `reinterpret_cast` cannot drop a cv-qualifier.
- **Fix:** Switched to the plan-specified C-style cast `(long*)&m_iLock`, which legally strips `volatile`. The intrinsic is a full barrier, so dropping the qualifier does not weaken the atomic acquire.
- **Files modified:** `src/engine/client/library/clientGame/src/shared/HTTPpost/VeCritsec.hpp`
- **Verification:** Re-compiled in the scratch harness — 0 VeCritsec-cited errors; the only remaining error is the pre-existing DEF-31-01 Misc.h C2668.
- **Committed in:** `379920283` (Task 2 commit — caught and fixed before the task commit)

---

**Total deviations:** 1 auto-fixed (1 bug).
**Impact on plan:** The fix aligns the implementation with the plan's literal `(long*)` spec; no scope creep. All other tasks executed exactly as written.

## Issues Encountered
- **DEF-31-01 (Misc.h:236 memmove C2668)** appears as the single residual error when compiling VeCritsec.cpp and DebugHelp.cpp in the harness. This is the known pre-existing cross-plan blocker assigned to **plan 31-04** (logged in `deferred-items.md`), not a regression from this plan. Per the wave guidance, the per-TU acceptance check (0 C4235/C4311/C4312/C4244 cited at the touched file) is robust to it — cl emits the full worklist past the C2668, and no error cites CollisionUtils/Fatal/Clock/ProfilerTimer/VeCritsec.hpp/DebugHelp.cpp themselves.
- The plan's `grep -cE "error C4235..."` verify pattern false-positives on the harness's own `===== WORKLIST (error C4235/...) =====` banner line. Resolved by filtering the banner and reading the actual TU-cited error lines from `worklist.log`; every touched TU is clean.

## Residuals Handed to Plan 06's Gate (PHASE-33 RUNTIME RESIDUALs)

Recorded here for plan 06's residual worklist (review #6):

1. **DebugHelp x64 backtrace WALK is compile-clean only.** `RtlCaptureContext` + `IMAGE_FILE_MACHINE_AMD64` compiles, but the x64 unwind mechanism (`.pdata`/RUNTIME_FUNCTION tables) differs from the 32-bit frame-pointer walk and is not runtime-validated here (Phase-31 bar = compile-clean, Open-Q3). Runtime-validate in Phase 33/36.
2. **`callStack` output array still narrows the 64-bit return address.** `getCallStack` writes `uint32 *callStack`; `*callStack = DWORD(Offset)` truncates the x64 `Rip` to 32 bits. Widening the output array (and its callers + symbol lookup) to 64-bit is the Phase-33 runtime fix; left as-is now to avoid a shared-signature cascade. (Note: the STACKFRAME64 `.Offset` fields themselves are NOT narrowed — only the final output store is.)

Both are documented inline in `DebugHelp.cpp` under `// PHASE-33 RUNTIME RESIDUAL` comments.

## Next Phase Readiness
- BITS-01 misc-asm sweep done. Combined with 31-02 (FPU/SSE/Transform), the in-scope build-path `__asm` survivors are now intrinsic-ported.
- **Blocking for full in-harness clean:** DEF-31-01 (Misc.h memmove C2668) must land in **plan 31-04** before the in-scope tree compiles past it cleanly; it does not block this plan's per-TU verification.
- 32-bit boot regression is covered by plan 06's phase-gate dual-renderer smoke after the wave-2 fixes merge (not run here — no 32-bit build performed in this plan).

## Threat Flags
None — no new security-relevant surface introduced. The two trust boundaries from the plan's threat model (VeCritsec spinlock atomicity, DebugHelp diagnostic integrity) are addressed: the spinlock translation is a faithful single-bit intrinsic with the jnc-equivalent branch (T-31-07 mitigate), and the DebugHelp x64 walk correctness is the accepted-and-deferred T-31-08 runtime residual.

## Self-Check: PASSED

- All 6 modified files exist and contain their expected intrinsic (sqrtf, __debugbreak x2, __rdtsc, _interlockedbittestandset, RtlCaptureContext).
- SUMMARY.md exists.
- All 3 task commits found in git (a4f711419, 379920283, 5a8924b8c).

---
*Phase: 31-64-bit-correctness-foundation*
*Completed: 2026-06-15*
