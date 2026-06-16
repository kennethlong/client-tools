---
phase: 31-64-bit-correctness-foundation
plan: 04
subsystem: clientGraphics / sharedMemoryManager / sharedFoundation (x64 correctness — BITS-02)
tags: [x64, bits-02, pointer-truncation, sort-key, hash-to-int, c4311, c4312, c4244, def-31-01]
requirements-completed: [BITS-02]
dependency-graph:
  requires:
    - "31-01 scratch x64 Debug|x64 compile-only harness (compile-all.ps1, in-scope-tus.txt, explicit-cast-audit.log)"
  provides:
    - "Stable hash-to-int sort-key contract applied uniformly to all 7 pointer sort keys (int return preserved, no ABI cascade)"
    - "Width-correct MemoryManager pointer math + %p logging; Os Win32-boundary types (INT_PTR/UINT_PTR/ULONG_PTR)"
    - "DEF-31-01 (Misc.h:236 memmove C2668) RESOLVED — unblocks ~every in-scope TU on x64"
    - "RESIDUAL-31-04 classified BITS-02 worklist handed to plan 06 phase gate"
  affects:
    - "plan 06 (phase gate sweep) — owns the 7 classified non-owned residual truncation sites"
    - "32-bit shipped build inherits the shared-source fixes (re-validated by the plan-06 dual-renderer boot smoke)"
tech-stack:
  added: []
  patterns:
    - "Pointer sort key -> static_cast<int>(std::hash<uintptr_t>{}(reinterpret_cast<uintptr_t>(ptr))) — int return kept, full pointer entropy on x64 (D-06 review #3)"
    - "Pointer diff -> byte* subtraction yielding ptrdiff_t; pointer logging -> %p of a void const* (never %08x of a truncated int, never %zx)"
    - "Win32 API boundary -> INT_PTR (ShellExecute HINSTANCE return), UINT_PTR (InsertMenu/AppendMenu uIDNewItem), const ULONG_PTR* (RaiseException lpArguments)"
key-files:
  created: []
  modified:
    - "src/engine/shared/library/sharedFoundation/src/shared/Misc.h (DEF-31-01)"
    - "src/engine/client/library/clientGraphics/src/shared/StaticShader.cpp"
    - "src/engine/client/library/clientGraphics/src/shared/ShaderEffect.h"
    - "src/engine/client/application/Direct3d9/src/win32/Direct3d9_StaticVertexBufferData.cpp"
    - "src/engine/client/application/Direct3d9/src/win32/Direct3d9_DynamicVertexBufferData.cpp"
    - "src/engine/client/application/Direct3d9/src/win32/Direct3d9_StaticShaderData.cpp"
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticVertexBufferData.cpp"
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11_DynamicVertexBufferData.cpp"
    - "src/engine/shared/library/sharedMemoryManager/src/shared/MemoryManager.cpp"
    - "src/engine/shared/library/sharedFoundation/src/win32/Os.cpp"
    - ".planning/phases/31-64-bit-correctness-foundation/deferred-items.md"
  unchanged-confirmed:
    - "src/engine/client/library/clientGraphics/src/shared/ShaderPrimitiveSorter.cpp (Entry.shaderTemplateSortKey stays int — no widening, no ABI cascade)"
decisions:
  - "D-06 review #3 hash-to-int over return-type widening: keeps int return so the virtual interface, Entry struct, comparators and 4 plugin impls are all untouched (no shared-header ABI event)"
  - "MemoryManager::Block::getSize keeps its int API contract (used by ~15 callers); the bounded ptrdiff is cast to int explicitly (value < 2GB by allocator design) rather than cascading the return type"
  - "Os.cpp SetThreadName RaiseException C2664 fixed inline (Rule 3) — it was an x64 data-width blocker preventing a clean owned-TU compile, even though it is not a C4311/C4312/C4244"
metrics:
  duration: "~40 min"
  tasks: 3
  files-modified: 11
  completed: 2026-06-16
---

# Phase 31 Plan 04: BITS-02 Pointer/Integer Truncation Fixes Summary

Resolved every BITS-02 pointer/integer truncation defect in this plan's 10 owned TUs so they
compile x64-clean under `/we4311 /we4312 /we4244` (0 implicit errors, explicit-cast audit clean),
plus cleared DEF-31-01 — the Misc.h `memmove` C2668 that was the dominant cross-plan x64 blocker
gating ~every in-scope TU.

## What Was Built

**DEF-31-01 (Misc.h:236 `memmove` C2668) — RESOLVED.** The engine `inline void *memmove(...,int)`
wrapper's recursive call was ambiguous on x64 between itself and the CRT `memmove(...,size_t)`.
Fixed with `return ::memmove(destination, source, static_cast<size_t>(length));` — the
`::`-qualification + `size_t` binds the CRT overload, the engine wrapper's `int`-length signature is
unchanged for callers. This was the unblock: full clientGraphics TUs (e.g. `StaticShader.obj`,
676KB) now compile to exit 0 in the scratch harness where the C2668 previously gated them.

**Task 1 — uniform hash-to-int sort-key contract (7 sites).** All pointer-derived sort keys now use
`static_cast<int>(std::hash<uintptr_t>{}(reinterpret_cast<uintptr_t>(ptr)))`:
`StaticShader::getShaderTemplateSortKey`, `ShaderEffect::getShaderImplementationSortKey` (inline,
shared header), the 3 live D3D9 boot-path sites
(`Direct3d9_StaticVertexBufferData::getSortKey`, `Direct3d9_DynamicVertexBufferData::getSortKey`,
`Direct3d9_StaticShaderData::Stage::getTextureSortKey`), and the 2 D3D11 re-truncation sites
(`Direct3d11_StaticVertexBufferData`/`Direct3d11_DynamicVertexBufferData::getSortKey`). **The `int`
return is preserved on every site**, so `ShaderPrimitiveSorter::Entry.shaderTemplateSortKey` (an
`int` field), its comparators, the `virtual int getSortKey()` interface, and all 4 plugin impls do
NOT widen — no shared-header ABI cascade (D-06 review #3). `ShaderPrimitiveSorter.cpp` is byte-unchanged.

**Task 2 — MemoryManager pointer math/logging + Os Win32 boundaries.**
- `MemoryManager::Block::setNext` pointer diff via `byte*` subtraction (`ptrdiff_t`), not int operands.
- `MM::alloc`/`MM::free`/`report()` logging prints the full address via `%p` of a `void const*`
  (never `%08x` of a truncated int, never `%zx`).
- `Block::getSize` casts the bounded ptrdiff to `int` explicitly (int API kept; value < 2GB by design).
- `Os::launchBrowser` ShellExecute HINSTANCE return -> `INT_PTR` for the `>32` success threshold.
- `InsertMenu`/`AppendMenu` `uIDNewItem` HMENU -> `reinterpret_cast<UINT_PTR>` (3 sites).
- WM_CHAR keycode -> `(lParam >> 16) & 0xFF` (x64-safe; old `(lParam<<8)>>24` assumed 32-bit LPARAM).

**Task 3 — sweep + residual classification.** The `-Scope bits02` scratch sweep reports **0
C4311/C4312/C4244 across all 10 owned files**; the explicit-cast-audit reconcile confirms all 11
owned-file audit sites are resolved in live source. Non-owned residuals recorded + classified for
plan 06 (see Deviations / RESIDUAL-31-04).

## Verification Results

Per-TU scratch x64 `cl /c /we4311 /we4312 /we4244` (via `compile-all.ps1 -SingleTu` / `-Scope bits02`):

| TU | C4311/C4312/C4244 | obj produced | exit |
|----|-------------------|--------------|------|
| StaticShader.cpp | 0 | yes (676KB) | 0 |
| Direct3d9_StaticVertexBufferData.cpp | 0 | yes | 0 |
| Direct3d9_DynamicVertexBufferData.cpp | 0 | yes | 0 |
| Direct3d9_StaticShaderData.cpp | 0 | yes | 0 |
| Direct3d11_StaticVertexBufferData.cpp | 0 | yes | 0 |
| Direct3d11_DynamicVertexBufferData.cpp | 0 | yes | 0 |
| MemoryManager.cpp | 0 | yes | 0 |
| Os.cpp | 0 | yes (369KB) | 0 |

Grep gates (all pass):
- `reinterpret_cast<int>` in the 5 D3D9/shared sort files + MemoryManager + Os: **0**
- `static_cast<int>(reinterpret_cast<uintptr_t>` in the 2 D3D11 files: **0**
- `std::hash<uintptr_t>` across the 7 sort files: **7** (>= 7 required)
- `%zx` in MemoryManager: **0**
- `INT_PTR`/`UINT_PTR`/`ULONG_PTR` in Os: present (8 hits)
- `#pragma warning(disable)` introduced in any of the 10 owned files: **0**
- `ShaderPrimitiveSorter.cpp` byte-unchanged: confirmed (git status empty)

(32-bit link + dual-renderer render smoke is plan-06's phase-gate responsibility after the wave-2 merge, per the plan's verification note.)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Additional C4311/C4244 sites in owned TUs beyond the named lines (Task 2)**
- **Found during:** Task 2 harness compile of MemoryManager.cpp / Os.cpp.
- **Issue:** Beyond the plan's named lines, the harness surfaced `MemoryManager.cpp:429`
  (`Block::getSize` ptrdiff->int C4244), `Os.cpp:851/868/884` (`reinterpret_cast<UINT>(HMENU)`
  C4311), and `Os.cpp:1071` (`(lParam<<8)>>24` LPARAM->int C4244). The plan's Task 2 action
  explicitly requires fixing every C4311/C4312/C4244 in these two TUs, not only the named lines
  (review #4) — so these are in scope.
- **Fix:** `getSize` explicit bounded `static_cast<int>`; menu handles `reinterpret_cast<UINT_PTR>`;
  keycode `(lParam>>16)&0xFF`.
- **Files modified:** MemoryManager.cpp, Os.cpp
- **Commit:** 0fd4a74a8

**2. [Rule 3 - Blocking] Os.cpp:1403 SetThreadName RaiseException C2664 (x64)**
- **Found during:** Task 2 — Os.cpp would not produce an obj (exit 2) because of a pre-existing
  `RaiseException(..., DWORD*, ...)` vs `const ULONG_PTR*` mismatch on x64. Not a C4311/C4312/C4244,
  but a 64-bit data-width defect blocking a clean compile of an owned TU.
- **Fix:** `sizeof(info)/sizeof(ULONG_PTR)` + `reinterpret_cast<const ULONG_PTR*>(&info)` — the
  canonical x64-safe SetThreadName idiom. Os.cpp now compiles exit 0 with obj produced.
- **Files modified:** Os.cpp
- **Commit:** 0fd4a74a8

### Out-of-scope (recorded, NOT fixed) — RESIDUAL-31-04 -> plan 06

The `-Scope bits02` sweep + explicit-cast-audit reconcile surfaced 7 truncation sites in TUs NOT in
this plan's `files_modified`. Recorded + classified in
`deferred-items.md#RESIDUAL-31-04` per Task 3 (review #4: in-scope-boot-path-must-fix vs
third-party/link-only-deferrable):
- **Must-fix in Phase 31 (plan 06):** `RenderWorld.cpp:1127`, `Direct3d9.cpp:137/183/185/203`
  (`(DWORD)(uintptr_t)` log casts that EVADE /we4311), `EditableAnimationState.cpp:436/489`,
  `CuiMediator.cpp:1009`, `LeakFinder.cpp:137`, `WinMain.cpp:88/90` (ShellExecute return — mirror
  the Os.cpp launchBrowser fix landed here).
- **Deferrable:** `WaterTestAppearance.cpp:97` (test-harness code, not boot path).
- No vendored third-party truncation surfaced — every must-fix site is our own engine/app source,
  so no in-scope boot-path source escapes via the residual valve.

## Known Stubs

None. All fixes are functional width-correct replacements; no placeholder values, no TODO stubs.

## Self-Check: PASSED
(see appended verification below)
