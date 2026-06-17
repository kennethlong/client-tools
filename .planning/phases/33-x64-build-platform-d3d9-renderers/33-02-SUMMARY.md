---
phase: 33-x64-build-platform-d3d9-renderers
plan: 02
subsystem: infra
tags: [d3d9, d3dcompiler_47, d3dassemble, d3dx-removal, x64-port, shader, gl05, gl06, gl07]

# Dependency graph
requires:
  - phase: 32-d3dx-to-d3dcompiler-47
    provides: "32-01 D3DAssemble dialect gate (byte-identical to D3DXAssembleShader on the 6-shader sample; static-local-cached GetProcAddress probe shape) + 32-02 HLSL path already migrated to D3DCompile (Fix-A wraps the HLSL call)"
  - phase: 33-x64-build-platform-d3d9-renderers
    provides: "33-01 x64 foundations (x64-platform.props, x64 import libs, tinyxml x64) — the link target this precondition unblocks"
provides:
  - "The //asm .vsh compile path now assembles through D3DAssemble (d3dcompiler_47) instead of D3DXAssembleShader — the ENTIRE D3D9 compile surface (HLSL + asm) is now off D3DX, clearing the last D3DX compile-symbol precondition for the Phase-33 x64 link"
  - "Fix-A SEH guard disposition resolved = RETAIN-with-comment (D-05 default / reviews fix #8)"
  - "x64 d3dcompiler_47.dll export of D3DAssemble CONFIRMED (machine 8664, System32) — rules out a null s_d3dAssemble → boot crash on x64"
affects: [33-03 (non-compile D3DX type removal), 33-04 (x64 platform add), 33-05 (first x64 link)]

# Tech tracking
tech-stack:
  added: [d3dcompiler_47 D3DAssemble via GetProcAddress (asm path)]
  patterns:
    - "Static-local resolve-once GetProcAddress on the undecorated export (D3DAssemble is not in the public d3dcompiler.h; the .lib symbol is C++-mangled — GetProcAddress sidesteps the mismatch). Cache HMODULE + fn pointer; never LoadLibrary/GetProcAddress per compile."
    - "reinterpret_cast at the ABI-identical call boundary (D3DXMACRO≡D3D_SHADER_MACRO, ID3DXBuffer≡ID3DBlob, ID3DXInclude≡ID3DInclude) — the same technique the 32-02 HLSL D3DCompile path already uses."
    - "Loud-fail on resolve/assemble failure (FATAL + error blob), never a silent null-VS (D-06); gl11's [P19VSFALLBACK] does NOT transfer (D3D9 natively executes assembled vs_1_1/vs_2_0)."

key-files:
  created:
    - .planning/phases/33-x64-build-platform-d3d9-renderers/33-02-SUMMARY.md
  modified:
    - src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp

key-decisions:
  - "Fix-A SEH guard RETAINED (D-05 default / reviews fix #8) — belt-and-suspenders comment only; __try/__except unchanged. The .vsh path 32-02 never exercised, so retiring is the render-risk path; the guard is cheap insurance against the 0xC0000090 FP-crash class on an unexercised path."
  - "D3DAssemble resolved via static-local-cached GetProcAddress on the undecorated export (resolve-once), not the C++-mangled .lib symbol — matches the validated 32-01 probe shape."
  - "Failure is loud (FATAL at :806 on null fn pointer, error-blob surfaced) — no silent null-VS substitution (D-06)."

patterns-established:
  - "GetProcAddress-on-undecorated-export resolve-once for d3dcompiler_47 entry points not in the public header (D3DAssemble) — mirrors the D3DCompile resolve in this same file."

requirements-completed: [X64-02, SHADER-01]

# Metrics
duration: ~45min (incl. crash recovery + human-verify wait)
completed: 2026-06-17
---

# Phase 33 Plan 02: D3DX-Removal Precondition (asm path) Summary

**The //asm .vsh compile path now assembles through D3DAssemble (d3dcompiler_47) via a static-local-cached GetProcAddress on the undecorated export — taking the LAST D3DX compile dependency off the D3D9 path (both HLSL and asm are now off D3DX), with the Fix-A SEH guard RETAINED and the asm render smoke APPROVED on the 32-bit gl05 build.**

## Performance

- **Duration:** ~45 min (spanned a transient API-529 crash + a human-verify checkpoint wait)
- **Started:** 2026-06-17 (Task 1)
- **Completed:** 2026-06-17 (Task 3 human-verify APPROVED; finalized by continuation agent)
- **Tasks:** 3 (Task 1 auto, Task 2 auto, Task 3 checkpoint:human-verify)
- **Files modified:** 1 source file (`Direct3d9_VertexShaderData.cpp`) across the two task commits

## Accomplishments
- Ported the single remaining `D3DXAssembleShader` call (the //asm .vsh path) to `D3DAssemble` (d3dcompiler_47) — the entire D3D9 compile surface (HLSL + asm) is now off D3DX, clearing the last D3DX compile-symbol precondition for the Phase-33 x64 link (D-04/D-04b/D-05).
- Resolution is a static-local-cached `GetProcAddress("D3DAssemble")` (resolve-once) at `:800`; the live assemble call is `s_d3dAssemble(...)` at `:812`, with a loud `FATAL(!s_d3dAssemble, ...)` at `:806` (D-06 — no silent null-VS, no skipped draws).
- Confirmed the x64 `d3dcompiler_47.dll` exports `D3DAssemble` (`dumpbin /exports C:\Windows\System32\d3dcompiler_47.dll` lists `D3DAssemble`, ordinal 1; System32 holds the x64 DLL on 64-bit Windows) — so `s_d3dAssemble` resolves non-null on x64, ruling out an asm-path boot crash (reviews fix #7a).
- Resolved the Fix-A SEH guard disposition = RETAIN (D-05 default / reviews fix #8); comment-only, `__try/__except` unchanged.
- The 32-bit gl05 //asm render smoke (Tatooine bump/dot3) was APPROVED by the user (2026-06-17) — no skipped/garbled draws vs the D3DX baseline.

## Task Commits

1. **Task 1: Port the asm call site D3DXAssembleShader → D3DAssemble + build gl05/06/07 clean** — `436189d8a` (feat)
2. **Task 2: Resolve the Fix-A SEH guard disposition (D-05) = RETAIN** — `e057b6d3a` (docs)
3. **Task 3: Render-correctness smoke — gl05 //asm shaders render clean** — checkpoint:human-verify, **APPROVED** by the user 2026-06-17 (no code commit; the in-engine render is the gate)

**Plan metadata:** this SUMMARY + STATE.md + ROADMAP.md (docs commit)

## Files Created/Modified
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp` — asm call site (:812) ported `D3DXAssembleShader` → `s_d3dAssemble` (D3DAssemble); `PFN_D3DAssemble` typedef (:89); static-local resolve-once at :800; loud FATAL at :806; Fix-A SEH guard retained with a belt-and-suspenders comment.

## Verified Facts (recorded at finalization)

- **Build links clean:** `build-33-02.log` (5-target Debug|Win32) and `build-33-02-fixa.log` (3-plugin) both have **0** `unresolved external symbol` (the /FORCE false-clean trap cleared — exit 0 is NOT proof).
- **D3DXAssembleShader off the live path:** 4 residual `D3DXAssembleShader` references remain in the file, **all in comments** (lines 72, 180, 565, 795 — documentation/decision-trail prose). The live assemble call is `s_d3dAssemble(...)` at :812. (Note: the plan's `grep -v '^[ \t]*//' | grep -c D3DXAssembleShader` acceptance check reports 4 because BSD/GNU basic-grep `[ \t]` matches the literal chars space/`t`, NOT a tab — the comment-stripping filter is unreliable against tab-indented `//`. Visual inspection with `cat -A` confirms all 4 lines are `^I//` comments; 0 LIVE calls.)
- **Resolve-once shape:** static-local-cached `GetProcAddress("D3DAssemble")` at :800 (HMODULE + fn pointer cached; not per-compile).
- **x64 export confirmed (reviews fix #7a):** `dumpbin /exports C:\Windows\System32\d3dcompiler_47.dll` lists `D3DAssemble` (ordinal 1); the System32 DLL is machine 8664 on 64-bit Windows → `s_d3dAssemble` is non-null on x64, no FATAL-on-every-asm-shader boot crash.
- **Fix-A:** RETAINED per D-05 + reviews fix #8 (the D3DX 0xC0000090 FP fault is believed gone with D3DCompile/D3DAssemble, but the guard is cheap insurance on the unexercised .vsh path).
- **Staged binaries (correct dependency order):** `gl05_d.dll`/`gl06_d.dll`/`gl07_d.dll` re-staged at 12:19:45–12:19:48; `SwgClient_d.exe` at 12:18:16. The plugin DLLs are NEWER than the exe — but this is the asm/Fix-A edit landing in the THREE D3D9 plugins (which share `Direct3d9_VertexShaderData.cpp`), not a shared-header ABI change to a struct the exe reads. No shared public struct was touched in this plan, so there is no stale-plugin ABI cascade; the render-smoke APPROVAL (in-engine, on these exact staged binaries) is the empirical confirmation that the exe↔plugin pairing is consistent.
- **Task 3 human-verify:** APPROVED by the user 2026-06-17 — rasterMajor=5 booted, Tatooine bump/dot3 //asm shaders rendered clean, no skipped draws, no assemble FATAL.

## Decisions Made
- **Fix-A = RETAIN (D-05 default / reviews fix #8).** Comment-only change; `__try/__except` left in place. Rationale: retiring on a .vsh path 32-02 never exercised could re-introduce the FP-crash class, and Task 1's link grep does not prove that; the guard is cheap insurance. Retaining means no extra render burden (the asm-path smoke alone sufficed for Task 3).
- **GetProcAddress on the undecorated export, resolve-once (static-local).** D3DAssemble is not in the public `d3dcompiler.h`; the .lib symbol is C++-mangled — GetProcAddress sidesteps the mismatch. Cache HMODULE + fn pointer to avoid a refcount/reload leak per compile.

## Deviations from Plan

None - plan executed exactly as written. (The asm-call swap, the Fix-A RETAIN disposition, and the x64-export confirmation all matched the plan's defaults and acceptance criteria. No Rule 1-4 deviations were required.)

## Issues Encountered
- **Executor crash mid-plan (operational, not a code defect):** the original executor completed and committed Tasks 1 and 2, then died on a transient API-529 error before writing this SUMMARY or surfacing the Task 3 checkpoint. The orchestrator verified the two commits and the build/export facts post-crash, surfaced the Task 3 human-verify (APPROVED), and spawned this continuation agent to finalize. No work was lost or redone; Tasks 1-2 were NOT re-executed.
- **Plan acceptance-grep tab/`[ \t]` quirk (documented above):** the `grep -v '^[ \t]*//'` live-code filter does not strip tab-indented `//` comments under basic grep, so it reported 4 false residuals. The actual live-code state (0 D3DXAssembleShader calls) was confirmed by `cat -A` visual inspection. Recorded here so 33-03 (which drops the d3dx9 includes) does not chase a phantom live reference.

## Next Phase Readiness
- **Plan 03 (33-03) is unblocked:** the entire D3D9 compile surface is off D3DX. Plan 03 owns the residual non-compile D3DX surface — `D3DXMATRIX`/`D3DXMatrixMultiply`/`Transpose` → DirectXMath, own-impl surface-copy/mesh/save, and finally dropping the `d3dx9*.h` includes. The 4 residual D3DXAssembleShader COMMENTS are documentation only and need no code action.
- **File-ownership boundary honored:** this plan touched ONLY the asm call site region + the Fix-A comment. The HLSL `reinterpret_cast`/`ID3DBlob` path (:178-183) and the `IncludeHandler` class vtable were NOT refactored (ABI-sensitive, shared by both paths — Plan 03's residual-typedef swap territory).
- **No blockers.** Plan 04 wires `x64-platform.props` into the boot-path vcxprojs + registers `tinyxml.vcxproj` in swg.sln (04-T3); Plan 05 swaps SwgClient's import-lib names and performs the first full x64 link.

## Self-Check: PASSED

- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp` — FOUND, contains live `s_d3dAssemble(...)` at :812 + `GetProcAddress("D3DAssemble")` at :800.
- Commit `436189d8a` (Task 1, feat) — FOUND in git log.
- Commit `e057b6d3a` (Task 2, docs/Fix-A) — FOUND in git log.
- `build-33-02.log` / `build-33-02-fixa.log` — 0 `unresolved external symbol` (verified).
- x64 `D3DAssemble` export — CONFIRMED (dumpbin, ordinal 1).
- Task 3 human-verify — APPROVED by the user 2026-06-17.

---
*Phase: 33-x64-build-platform-d3d9-renderers*
*Completed: 2026-06-17*
