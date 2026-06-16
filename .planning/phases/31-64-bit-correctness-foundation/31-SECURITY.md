---
phase: 31
slug: 64-bit-correctness-foundation
status: verified
threats_open: 0
asvs_level: n/a
created: 2026-06-16
---

# SECURITY.md — Phase 31: 64-bit Correctness Foundation

**Audit Date:** 2026-06-16
**Phase Status:** SECURED
**Threats Closed:** 38/38
**ASVS Level:** N/A (native C++ desktop client — STRIDE register is build/data-integrity focused)

---

## Audit Scope

Phase 31 made the in-scope source tree x64-clean (inline asm to intrinsics, pointer/integer truncation
fixes, serialization-width audit) without shipping an x64 binary and without regressing the 32-bit
client. The threat register was authored at plan-time across 9 PLAN.md files. This audit verifies each
declared mitigation exists in the implemented code.

---

## Threat Verification

### Plan 31-01 (Scratch Harness)

| Threat ID | Category | Disposition | Evidence |
|-----------|----------|-------------|----------|
| T-31-01 | Tampering / committed swg.sln/.vcxproj | mitigate | `grep -c x64 src/build/win32/swg.sln` returns 0 (confirmed). `.gitignore` contains `x64-scratch` line (confirmed, 1 match). `git check-ignore src/build/win32/x64-scratch/probe.txt` exits 0 (IGNORED). |
| T-31-02 | Tampering / time_t serialization width | mitigate | `src/build/win32/x64-scratch/x64-compile.props` contains `_USE_32BIT_TIME_T=1` (6 occurrences confirmed). |
| T-31-03 | Info disclosure / local build tooling | accept | Accepted per plan: compile-only harness produces only local .obj + log; no external surface. Rationale documented in 31-01-PLAN.md threat model. |
| T-31-20 | Tampering / false-clean gate | mitigate | `compile-all.ps1` confirmed present with `-SingleTu`/`-Scope`/`-CastAudit` modes (11 occurrences of parameter names). Exhaustive TU manifest derived from ClCompile graph (gitignored, confirmed). Positive proof: harness verified a known-defect TU errors and a clean TU compiles per 31-01-SUMMARY. |
| T-31-25 | Repudiation / driver parameter contract | mitigate | `compile-all.ps1` implements and documents `-SingleTu`, `-Scope`, and `-CastAudit` modes (11 pattern occurrences in file). Each mode invoked by downstream plans 02-06 as designed. |

### Plan 31-02 (FPU / SSE Intrinsics)

| Threat ID | Category | Disposition | Evidence |
|-----------|----------|-------------|----------|
| T-31-04 | Tampering / FP control-word encoding mismatch | mitigate | `FloatingPointUnit.cpp`: 0 `__asm` in code (none found); `_MCW_RC`/`_controlfp`/`_MCW_PC`/`_MCW_EM` present (12 occurrences). `#if defined(_M_X64)` precision guard present at lines 247 and 270. `_DEBUG` round-trip self-check pattern confirmed per 31-VERIFICATION.md. |
| T-31-05 | DoS/Tampering / skinning/matrix correctness | mitigate | `SseMath.cpp`: 0 `__asm` in code; `_mm_loadu_ps` present (4 occurrences); `__cpuid` present (2 occurrences). `Transform.cpp`: confirmed via commits 717a66689 + 6a1fd14b7 (31-VERIFICATION.md SC-1). `_DEBUG` numeric-equivalence oracle confirmed per 31-VERIFICATION.md behavioral spot-checks. |
| T-31-06 | Tampering / SseMath dead-code deletion | mitigate | `SseMath.cpp`: all 4 `*_l2p` routines + `canDoSseMath` confirmed present per 31-VERIFICATION.md SC-1. `_mm_` occurrences confirm live SSE path. No 4 routines deleted. |
| T-31-21 | EoP / unaligned SSE load runtime fault | mitigate | `SseMath.cpp`: `_mm_loadu_ps` for matrix/vector pointer loads (4 occurrences). `SoftwareBlendSkeletalShaderPrimitive.cpp`: `_mm_loadu_ps` present in `_mm_` count (41 occurrences). Phase-33 RUNTIME re-confirm documented as A1-SSE-ALIGN in `31-PHASE33-RESIDUAL-WORKLIST.md`. |
| T-31-23 | Repudiation / mid-wave false-green | mitigate | Full 5-target build deferred to Wave-3 plan 06 (per plan design). Build log `build-32bit.log` exists (58268 lines), 0 errors, 0 `unresolved external symbol` (grep confirmed). |

### Plan 31-03 (Misc __asm Sweep)

| Threat ID | Category | Disposition | Evidence |
|-----------|----------|-------------|----------|
| T-31-07 | Tampering / VeCritsec lock acquire/fail | mitigate | `VeCritsec.hpp`: `_interlockedbittestandset` present (4 occurrences). `lock bts` / `__asm` in MSVC branch: 0 code occurrences (only comments). GCC `__asm__` branch untouched per 31-03-SUMMARY.md. |
| T-31-08 | Repudiation / DebugHelp x64 backtrace | accept | Accepted: Phase-31 bar is compile-clean only (Open-Q3). `DebugHelp.cpp`: `RtlCaptureContext` at line 518, `IMAGE_FILE_MACHINE_AMD64` at line 525, `PHASE-33 RUNTIME RESIDUAL` comments at lines 527 and 563. A1-DBGHELP-WALK and A1-DBGHELP-RIP documented in `31-PHASE33-RESIDUAL-WORKLIST.md`. |
| T-31-09 | Tampering / __debugbreak/__rdtsc/sqrtf equivalence | mitigate | `CollisionUtils.cpp`: `sqrtf` present (2 occurrences), 0 `__asm`. `Fatal.cpp`: `__debugbreak` present (2 occurrences), 0 `__asm`. `Clock.cpp`: `__debugbreak` present (1 occurrence). `ProfilerTimer.cpp`: `static_cast<__int64>(__rdtsc())` at line 39, 0 `__asm`/`__declspec(naked)`. |

### Plan 31-04 (Pointer Truncation Fixes)

| Threat ID | Category | Disposition | Evidence |
|-----------|----------|-------------|----------|
| T-31-10 | EoP / pointer truncation | mitigate | `MemoryManager.cpp`: `uintptr_t`/`ptrdiff_t` present (2 occurrences); 0 `reinterpret_cast<int>` of pointers. `Os.cpp`: `INT_PTR`/`DWORD_PTR` present (8 occurrences). Escape-valve fixes (ba66d6657) cover 8 log-cast sites (31-PHASE33-RESIDUAL-WORKLIST.md §B). |
| T-31-11 | Tampering / sort-key uniqueness | mitigate | `StaticShader.cpp`: `static_cast<int>(std::hash<uintptr_t>{}(...))` at line 298. `ShaderEffect.h`: same hash-to-int at line 100. `Direct3d9_StaticVertexBufferData.cpp`: hash-to-int at line 137. `ShaderPrimitiveSorter` `Entry.shaderTemplateSortKey` stays `int` (no widening). |
| T-31-12 | Spoofing / ShellExecute success check | mitigate | `Os.cpp`: `INT_PTR`/`DWORD_PTR` present (8 occurrences). `WinMain.cpp` escape-valve fix (ba66d6657): `INT_PTR result` + `<= 32` per 31-PHASE33-RESIDUAL-WORKLIST.md §B. |
| T-31-22 | Tampering / in-scope boot-path truncation escaping | mitigate | Task 3 of plan 31-04 classifies every out-of-plan survivor. The 3 live D3D9 boot-path sites fixed directly in plan 31-04. All class-(B) escapes fixed in 31-06 (ba66d6657) and gap-closure plans 31-07/08/09. 31-PHASE33-RESIDUAL-WORKLIST.md §B lists all 8 wave-2 escapes fixed in-phase. |

### Plan 31-05 (Serialization Width)

| Threat ID | Category | Disposition | Evidence |
|-----------|----------|-------------|----------|
| T-31-13 | Tampering / Archive std::map wire count | mitigate | `Archive.h`: `uint32_t numKeys` at lines 201 and 398 (both get and put). `assert(source.size() <= static_cast<size_t>(UINT32_MAX))` at line 397. No 8-byte overload introduced. Wire byte-identical per 31-VERIFICATION.md SC-3. |
| T-31-14 | Tampering / packed-struct on-disk layout | mitigate | `TargaFormat.cpp`: `static_assert(sizeof(TargaHeader) == 18, ...)` at line 100; `static_assert(sizeof(TargaFooter) == 26, ...)` at line 101. `AssetCustomizationManager.cpp`: 5 `static_assert(sizeof)` occurrences confirmed. Baselined to live 32-bit sizeof per 31-VERIFICATION.md SC-3. |
| T-31-15 | Tampering / Archive.h shared-header ABI | mitigate | Edit changes only a local variable type + adds asserts (no struct-layout change). Full 5-target build (plan 31-06) confirmed 0 `unresolved external symbol` in `build-32bit.log`. |

### Plan 31-06 (Phase Gate)

| Threat ID | Category | Disposition | Evidence |
|-----------|----------|-------------|----------|
| T-31-16 | Tampering / shipped 32-bit correctness | mitigate | Dual-renderer boot smoke human-approved (user verbatim "approved, both zoned into Tatooine, no regression" per 31-VERIFICATION.md SC-4). `_DEBUG` FPU/SSE oracles passed (no startup DEBUG_FATAL). `build-32bit.log`: 0 errors, 0 `unresolved external symbol`. |
| T-31-17 | Tampering / stale plugin DLL ABI | mitigate | Full 5-target Debug build (`build-32bit.log` confirms fresh build). Sort keys kept `int` (hash-to-int), `Archive.h` local-var-only change — no struct layout change. Per 31-VERIFICATION.md: no stale-plugin cascade. |
| T-31-18 | Repudiation / /FORCE false-clean link | mitigate | `grep -c "unresolved external symbol" build-32bit.log` returns 0. Exe deleted before build to force relink (per plan 31-06 Task 2 action). |
| T-31-19 | Tampering / residual silently dropped | mitigate | `31-PHASE33-RESIDUAL-WORKLIST.md` exists with full (A)/(B) classification. Named runtime residuals A1-DBGHELP-WALK/RIP, A1-SSE-ALIGN, A1-FPU-PC all present. N1 coverage-hole note + N2 C4267 carry-forward both present. Phase 33 cited 24 times as consumer. |
| T-31-24 | Repudiation / escape-valve edits untracked | mitigate | `files_modified` in 31-06-PLAN.md declares the escape-valve scope. `31-06-SUMMARY.md` enumerates all escape-valve source edits (commit ba66d6657). 31-PHASE33-RESIDUAL-WORKLIST.md §B lists all 8 enumerated wave-2 escapes. |

### Plan 31-07 (Gap Closure 1 — B-GAP)

| Threat ID | Category | Disposition | Evidence |
|-----------|----------|-------------|----------|
| T-31-17 (plan 07) | Tampering / PathSearch node mark pointer | mitigate | `PathNode.h`: `intptr_t m_marks[4]` at line 107; `getMark`/`setMark` accessors return/take `intptr_t` (lines 78/79/225/230). `PathSearch.cpp`: `intptr_t mark` locals at lines 188/199/214/499. No `(int)` cast of a node pointer remains. |
| T-31-18 (plan 07) | Tampering / StatusWindow this-pointer | mitigate | `StatusWindow.cpp`: `SetWindowLongPtr` at lines 148/151; `GetWindowLongPtr` at lines 97/100; `GWLP_USERDATA` at lines 97/100/148/151; `LONG_PTR` at line 151. |
| T-31-19 (plan 07) | Tampering / SSE skinning intrinsic equivalence | mitigate | `SoftwareBlendSkeletalShaderPrimitive.cpp`: 0 `__asm` in code (1 match is a comment at line 1741); `_mm_` occurrences = 41; `_mm_loadu_ps` for unaligned loads; `_DEBUG` numeric oracle pattern confirmed per 31-VERIFICATION.md SC-1. |
| T-31-20 (plan 07) | Tampering / VoidBindSecond shared-header ABI | mitigate | `VoidBindSecond.h` (src/shared copy): `if constexpr (std::is_pointer_v<Target> && std::is_integral_v<Source>)` at line 33; `uintptr_t` round-trip for integral->pointer case (line 37). Full 5-target build rebuilds all consumers (confirmed via build-32bit.log). |
| T-31-21 (plan 07) | Tampering / time_t serialization width parity | mitigate | `GroupObject.cpp`/`SwgCuiGroup.cpp`: per-site STATE-vs-DISPLAY audit confirmed in 31-VERIFICATION.md SC-2 ("GroupObject/SwgCuiGroup time_t sites all DISPLAY (not serialized)"). Sites handled with explicit cast + documentation per 31-07-PLAN.md Task 4. |

### Plan 31-08 (Gap Closure 2 — AutoDelta)

| Threat ID | Category | Disposition | Evidence |
|-----------|----------|-------------|----------|
| T-31-22 (plan 08) | Tampering / AutoDelta* serialized count wire width | mitigate | `AutoDeltaMap.h` (src/shared): `uint32_t baselineCommandCount` at line 99; `static_cast<uint32_t>` count puts at lines 391/422/423; `assert(...UINT32_MAX)` overflow guard present (3 occurrences). `AutoDeltaVector.h`: `uint32_t baselineCommandCount` at line 102; `static_cast<uint32_t>` puts at lines 380/397/398/414. Wire byte-identical confirmed. |
| T-31-23 (plan 08) | Tampering / divergent header-copy consumer parity | mitigate | `include/Archive/AutoDeltaMap.h`, `AutoDeltaVector.h`, `AutoDeltaPackedMap.h`, `AutoDeltaSet.h` are all 1-line forwarders to their `src/shared/` counterpart (verified by direct file read). No divergent copy exists — the forwarder pattern ensures single-source-of-truth. |
| T-31-24 (plan 08) | DoS / count overflow narrowing put | mitigate | `Archive.h`: `assert(source.size() <= static_cast<size_t>(UINT32_MAX))` at line 397. `AutoDeltaMap.h`: 3 `UINT32_MAX` overflow assert occurrences. Same guard pattern in AutoDeltaPackedMap/Set/Vector per 31-08-PLAN.md and 31-VERIFICATION.md SC-3. |
| T-31-25 (plan 08) | Tampering / scope creep third-party | accept | Accepted: Miles/Bink/WaterTest/D-07-N2 explicitly OUT of scope. Documented in `31-PHASE33-RESIDUAL-WORKLIST.md` as class-(A) residue: Miles -> P35, Bink -> P33, WaterTest -> P33. Rationale: third-party/tool-only, not in-scope boot-path. |

### Plan 31-09 (Gap Closure 3 — Convergence)

| Threat ID | Category | Disposition | Evidence |
|-----------|----------|-------------|----------|
| T-31-26 | Tampering / width-arg reconciliation | mitigate | `CuiCombatManager.cpp`: `size_t pos = 0` at line 1891 (parse cursor; not serialized). `TcpClient.cpp`: `ULONG_PTR completionKey = 0` at line 523 (local IOCP key; not serialized). `MeshConstructionHelper.cpp`: `static_cast<size_t>(0)` confirmed per 31-VERIFICATION.md SC-2. No on-the-wire field widths changed. |
| T-31-27 | Tampering / Unicode literal/type fix | mitigate | `sharedTemplateDefinition` reclassified as tool-only (NOT in SwgClient link closure): `SwgClient.vcxproj` has 0 ProjectReferences to it; `clientGame.vcxproj` has 0 references. Evidence per 31-VERIFICATION.md Key Link Verification (confirmed via grep of .vcxproj files). Files Filename.cpp/TemplateData.cpp/TpfFile.cpp unedited (correct — tool-only reclassification). |
| T-31-28 | Repudiation / harness-artifact mis-classification | mitigate | Direct3d9 `#error` confirmed scratch-config artifact (real .vcxproj per-config defines FFP/VSPS). UdpSock C2371 confirmed harness header-order artifact (winsock/platform header isolation in scratch; sharedNetwork confirmed bootable in 32-bit client). Real 5-target build (31-06 Task 2) proves both in `build-32bit.log` (0 errors). |
| T-31-29 | DoS / unbounded increment recursion | mitigate | User hard cap applied: final `-Scope all` (scope-all-31-09-final.out) shows 0 NEW class-(B); failing-TU residual = 41 class-(A) + 7 harness artifacts + 3 reclassified tool-only. SUMMARY declares Phase-31 class-(B) COMPLETE. No 31-10 was authored. |

---

## Unregistered Flags

One `## Threat Flags` block appears in 31-03-SUMMARY.md (the only SUMMARY.md with this section). It
states: "None — no new security-relevant surface introduced." The trust boundaries described (VeCritsec
spinlock atomicity, DebugHelp diagnostic integrity) both map to registered threats T-31-07 and T-31-08
respectively — no unregistered surface.

No other SUMMARY.md files contain `## Threat Flags` sections. All implementation-surfaced concerns map
to existing threat IDs.

---

## Accepted Risks Log

| ID | Threat | Rationale | Documented In |
|----|--------|-----------|---------------|
| T-31-03 | Info disclosure / local build tooling | Compile-only harness: no external surface, no PII, no network, only local .obj + log | 31-01-PLAN.md threat model |
| T-31-08 | DebugHelp x64 backtrace walk correctness | Phase-31 bar is compile-clean only (Open-Q3); RUNTIME validation requires x64 platform (Phase 33). 32-bit diagnostic path unchanged. | 31-PHASE33-RESIDUAL-WORKLIST.md A1-DBGHELP-WALK/RIP; `DebugHelp.cpp` lines 527/563 PHASE-33 RUNTIME RESIDUAL comments |
| T-31-25 (plan 08) | Scope creep / third-party Miles/Bink/WaterTest | Third-party API-bound (Miles/Bink) or test-harness-only (WaterTest) — not in-scope boot-path source. Cannot force-clean without x64 third-party libs. | 31-PHASE33-RESIDUAL-WORKLIST.md §A2; deferred-items.md |

---

## Coverage Honesty Notes

1. **ByteOrder.cpp `__asm` match:** The grep for `__asm` in ByteOrder.cpp returns 1 match — but this
   is a comment (`// Phase 31...`), not code. All 4 `_byteswap_ulong`/`_byteswap_ushort` intrinsic
   replacements are confirmed (5 occurrences).

2. **RegexServices.cpp / RenderWorldServices.cpp / MemoryManagerHook.cpp `__asm`:** Each returns 1
   match — all confirmed to be explanatory comments describing the removed code, not surviving asm.
   Zero C4235-producing code remains.

3. **`include/Archive/AutoDelta*.h` forwarders:** The plan required "both copies" to be fixed. The
   `include/Archive/` copies are verified to be 1-line `#include` forwarders to `src/shared/` — they
   cannot diverge. T-31-23 CLOSED.

4. **SoftwareBlendSkeletalShaderPrimitive.cpp:** 1 `__asm` hit is at line 1741 in a comment. Zero
   C4235-producing code survives. `_mm_` intrinsic count = 41.

5. **All class-(B) escapes were fixed in Phase 31:** The gap-closure chain (31-06 escape-valve commit
   ba66d6657, then plans 31-07, 31-08, 31-09) demonstrates the opposite of a soft-audit pattern —
   every time the full sweep exposed new class-(B) surface, it was fixed in-phase rather than deferred.

---

_Auditor: Claude (gsd-security-auditor)_
_Audit completed: 2026-06-16_

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-06-16 | 38 | 38 | 0 | gsd-security-auditor (Claude/sonnet) |

Register origin: `register_authored_at_plan_time: true` — all 9 PLAN.md files carried a `<threat_model>` STRIDE block. Auditor ran in **verify-mitigations** mode (register complete; no new-threat scan). 35 mitigate + 3 accept; every mitigation confirmed in source (file:line evidence above); all 3 accepted risks have documented rationale.

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log (T-31-03, T-31-08, T-31-25/plan-08)
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-06-16
