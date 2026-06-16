---
phase: 31-64-bit-correctness-foundation
plan: 06
subsystem: infra
tags: [x64, llp64, bits-01, bits-02, bits-03, phase-gate, residual-classification, link-grep, dual-renderer-boot, d-02, d-08]

# Dependency graph
requires:
  - phase: 31-01
    provides: the scratch Debug|x64 compile-only harness (compile-all.ps1 / x64-compile.props / in-scope-tus.txt — the D-01 worklist generator)
  - phase: 31-02
    provides: BITS-01 FPU/SSE intrinsic rewrite + the _DEBUG numeric/round-trip self-tests the boot smoke runs at startup
  - phase: 31-03
    provides: BITS-01 misc __asm sweep + the DebugHelp _M_X64 runtime residual handed to this gate
  - phase: 31-04
    provides: BITS-02 truncation fixes + RESIDUAL-31-04 (the 6 NON-owned wave-2 escapes handed to this gate)
  - phase: 31-05
    provides: BITS-03 Archive uint32_t pin + RESIDUAL-31-05 (ByteStream.cpp:347 handed to this gate)
  - phase: 31-07
    provides: gap-closure 1 (the 11 enumerated NEW class-(B) survivors the first full sweep surfaced)
  - phase: 31-08
    provides: gap-closure 2 (the ~753 AutoDelta* C2665/C2668 cascade; failing TUs 154->55)
  - phase: 31-09
    provides: gap-closure 3 (CAPPED convergence; 4 width members + reclassification; 0 NEW class-(B) -> source COMPLETE)
provides:
  - "the SINGLE full canonical 5-target 32-bit Debug build of the phase, link-grep clean (0 unresolved external symbol under /FORCE)"
  - "dual-renderer boot smoke APPROVED (D-08/SC#4): both rasterMajor=5 (gl05/D3D9) and =11 (gl11/D3D11) booted AND zoned into Tatooine, no regression"
  - "the CLASSIFIED Phase 33 residual worklist (D-02): (A) legitimate residue vs (B) phase escape, named runtime residuals, harness-deletion coverage hole, C4267 carry-forward"
  - "BITS-01/02/03 certified complete (contributed across 31-02..31-09)"
  - "the phase-escape (A)/(B) integrity gate applied: every class-(B) in-scope boot-path defect fixed in Phase 31, none deferred to Phase 33"
affects: [32-d3dx-d3dcompiler, 33-x64-build-platform, 35-miles-audio, 36-verify-cornersnap]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Single full build of a multi-plan phase runs ONCE at the gate (wave plans validate per-TU only) — avoids mid-wave false-green + concurrent-output-collision"
    - "/FORCE link-grep (unresolved external symbol == 0) is the real link gate; exit 0 is not trusted"
    - "Residual (A)/(B) classification as the phase-integrity gate: in-scope boot-path source defects MUST be fixed in-phase, never deferred via the residual valve (review #4)"
    - "Gap-closure increment chain: each full -Scope all sweep unmasks the next previously-cl-aborted class-(B) layer; spawn a user-authorized increment, re-sweep, until the cap holds with 0 NEW class-(B)"

key-files:
  created: []
  modified:
    - .planning/phases/31-64-bit-correctness-foundation/31-PHASE33-RESIDUAL-WORKLIST.md
    - .planning/phases/31-64-bit-correctness-foundation/deferred-items.md
    # Task 1 escape-valve source edits (commit ba66d6657) — the 8 enumerated wave-2 escapes:
    - src/engine/client/library/clientGraphics/src/shared/scene/RenderWorld.cpp
    - src/engine/client/renderer/gl0d/Direct3d9/src/win32/Direct3d9.cpp
    - src/engine/client/library/clientSkeletalAnimation/src/shared/controller/EditableAnimationState.cpp
    - src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp
    - src/engine/shared/library/sharedDebug/src/shared/LeakFinder.cpp
    - src/game/client/application/SwgClient/src/win32/WinMain.cpp
    - src/external/ours/library/archive/src/shared/ByteStream.cpp

key-decisions:
  - "Task 1 full -Scope all sweep surfaced a LARGER class-(B) surface than the wave-2 substring-scoped sweeps ever compiled — per review #4 these are phase escapes, flagged for GAP CLOSURE (31-07/08/09), NOT written to the Phase-33 deferral list"
  - "The 8 enumerated wave-2 escapes (RESIDUAL-31-04 x6 + RESIDUAL-31-05 ByteStream) fixed IN-PLACE via the escape valve (ba66d6657): %p + static_cast<void const*> for the %08x/%08X log casts, INT_PTR for the ShellExecute return, full-width uintptr_t for the ByteStream freed-poison sentinel"
  - "Task 2: the SINGLE full canonical 5-target 32-bit Debug build links clean (0 unresolved external) with ABI-consistent plugin DLLs; NO source touch-ups were needed to link, so Task 2 produced 0 committable source changes"
  - "Task 3: dual-renderer boot smoke APPROVED by the user — exceeds the char-select bar (both renderers zoned in-world into Tatooine, no regression, no _DEBUG self-test DEBUG_FATAL)"
  - "D-02 acceptance: all in-scope build-path class-(B) SOURCE compiles x64-clean in the scratch harness; the remaining residue is CLASSIFIED class-(A) (Miles->P35, Bink->P33, WaterTest + D-07/N2 C4244->P33) + confirmed harness artifacts + tool-only Unicode reclassification"

patterns-established:
  - "Phase gate = full sweep + classified residual worklist + single full link-grep build + dual-renderer boot smoke, all run ONCE after all wave + gap-closure source edits merge"
  - "Residual worklist is an explicit D-02 deliverable (not a silent omission): names runtime residuals, the harness-deletion coverage hole (N1), and the C4267 carry-forward (N2) as Phase-33 to-establish items"

requirements-completed: [BITS-01, BITS-02, BITS-03]

# Metrics
duration: 240min
completed: 2026-06-16
---

# Phase 31 Plan 06: 64-bit Correctness Foundation — Phase Gate Summary

**The Phase-31 gate certified: the full in-scope build path compiles x64-clean for all class-(B) source (residue CLASSIFIED legitimate per D-02), the SINGLE full canonical 5-target 32-bit build links clean (0 unresolved external symbol), and BOTH renderers (rasterMajor=5/gl05 and =11/gl11) booted AND zoned into Tatooine with no regression — closing BITS-01/02/03 and handing the third-party/runtime residue forward to Phase 33.**

## Performance

- **Duration:** ~240 min (spanning the gate's full sweep → 3 gap-closure increments → resume → build → boot-smoke approval)
- **Started:** 2026-06-16 (Task 1 full sweep)
- **Completed:** 2026-06-16 (Task 3 human-verify APPROVED)
- **Tasks:** 3 (Task 1 auto, Task 2 auto, Task 3 checkpoint:human-verify — approved)
- **Files modified:** 9 (2 planning artifacts + 7 escape-valve source files via ba66d6657)

## Accomplishments

- **Task 1 — full scratch-x64 sweep + the CLASSIFIED Phase 33 residual worklist (D-02 / reviews #4/#6/#2/#8/#7).** Ran `compile-all.ps1 -Scope all` over the full in-scope manifest and cross-referenced `explicit-cast-audit.log`. Fixed the **8 enumerated wave-2 escapes** in-place via the escape valve (commit `ba66d6657`): the `%08x`/`%08X` pointer-log truncations (RenderWorld, Direct3d9 `(DWORD)(uintptr_t)` double-cast that evades `/we4311`, EditableAnimationState, CuiMediator, LeakFinder) rewritten to `%p` + `static_cast<void const*>`; WinMain's `ShellExecute` `HINSTANCE` return widened to `INT_PTR`; ByteStream's freed-memory-poison sentinel compared as full-width `reinterpret_cast<uintptr_t>` vs a `uintptr_t` `0xefefefefefefefef`. Wrote `31-PHASE33-RESIDUAL-WORKLIST.md` (the D-02 hand-off) with the full (A)/(B) classification, the named runtime residuals, and the standing-coverage notes.
- **The (A)/(B) integrity gate held — but the first full sweep undercounted.** The full sweep surfaced a LARGER class-(B) survivor set than the wave-2 substring-scoped sweeps ever compiled (the narrow `ScopeFilters` substrings never compiled ~12 then ~154 in-scope TUs that carry genuine x64 defects). Per review #4 these are phase escapes that MUST be fixed in Phase 31 — NOT deferred. This spawned **3 user-authorized gap-closure plans (31-07/08/09)** rather than letting an in-scope boot-path defect escape via the residual valve.
- **Task 2 — the SINGLE full canonical 5-target 32-bit Debug build links clean.** Deleted `SwgClient_d.exe` to force a real relink, killed stale MSBuild/cl/link nodes, built `Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient` `/p:Configuration=Debug /p:Platform=Win32 /nodeReuse:false` from PowerShell, teed to `build-32bit.log` (gitignored). **Result: Build succeeded, 0 Error(s); link-grep `unresolved external symbol` = 0** (the `/FORCE` false-clean trap cleared), LNK1181=0, LNK2019/2001=0. All 5 targets linked + auto-staged this run: gl05/06/07/11_d.dll + SwgClient_d.exe (exe newest = correct link-last dependency order, NOT a stale-plugin cascade; every plugin recompiled against the current shared headers from 31-07/31-08 in the same build → no ABI mismatch). NO source touch-ups were needed to link → 0 committable source changes (commit `7b2be2b0d` is the STATE marker).
- **Task 3 — dual-renderer boot smoke APPROVED (D-08 / SC#4).** The user ran the smoke and reported verbatim: **"approved, both zoned into Tatooine, no regression"**. Both `rasterMajor=5` (D3D9/gl05) AND `rasterMajor=11` (D3D11/gl11) booted to character select AND successfully zoned in-world to Tatooine, with NO startup `DEBUG_FATAL` (the plan-02 `_DEBUG` FPU control-word + SseMath/Transform numeric-equivalence self-tests passed) and NO render/asset/skinning regression vs the shipped v2.3 baseline. This **exceeds** the char-select bar — full in-world load was verified.
- **BITS-01/02/03 closed.** The 64-bit-correctness source work is x64-clean for all in-scope class-(B) source and proven non-regressive on the shipped 32-bit tree.

## Task Commits

This plan's source surgery landed across the gate's Task 1 escape valve + the 3 gap-closure increments; the gate tasks themselves committed atomically:

1. **Task 1: full sweep + escape-valve fixes** — `ba66d6657` (fix: the 8 enumerated wave-2 BITS-02 truncation escapes, RESIDUAL-31-04/05) + `f7e3fed44` (docs: full x64 sweep + classified residual worklist; flagged the NEW class-(B) gap that spawned 31-07/08/09)
2. **Task 2: full 32-bit build link-grep gate** — `7b2be2b0d` (chore: canonical 5-target 32-bit build links clean, 0 unresolved external; 0 committable source changes — the clean-linking stage/ binaries + the gitignored build-32bit.log are the evidence)
3. **Task 3: dual-renderer boot smoke** — checkpoint:human-verify, APPROVED by the user (no commit — verification gate)

**Contributing gap-closure increments (the class-(B) surface the full sweep surfaced):**
- 31-07: `8479fb6ba` / `d9ee92088` / `8ae95abc6` / `4f7b1a99a` (+ docs `932c5507a`)
- 31-08: `1b6a98ff4` / `5b5f08a2f` / `846a2ded6`
- 31-09: `feaddc08e` / `81b19c345` / `79f5bc84a`

**Plan metadata:** this commit (docs: complete the 31-06 phase-gate plan)

## Files Created/Modified

- `.planning/phases/31-64-bit-correctness-foundation/31-PHASE33-RESIDUAL-WORKLIST.md` — the D-02 Phase-33 hand-off: (A)/(B) classification, named runtime residuals, standing-coverage notes
- `.planning/phases/31-64-bit-correctness-foundation/deferred-items.md` — the cross-plan deferral/escape ledger (DEF-31-01..09)
- `src/engine/client/library/clientGraphics/src/shared/scene/RenderWorld.cpp` — `:1127` `%08x` DPVS-object log cast → `%p`
- `src/engine/client/renderer/gl0d/Direct3d9/src/win32/Direct3d9.cpp` — `:137/183/185/203` `(DWORD)(uintptr_t)` base/device/vtbl/entry log casts → `%p` (RVA pointer-diffs kept)
- `src/engine/client/library/clientSkeletalAnimation/src/shared/controller/EditableAnimationState.cpp` — `:436/489` child/link `%08x` → `%p`
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp` — `:1009` mediator `%08x` profiler dump → `%p`
- `src/engine/shared/library/sharedDebug/src/shared/LeakFinder.cpp` — `:137` leak-report `%08X` → `%p`
- `src/game/client/application/SwgClient/src/win32/WinMain.cpp` — `:88/90` `ShellExecute` `HINSTANCE` return → `INT_PTR` + `%Id` (mirrors the Os.cpp launchBrowser fix)
- `src/external/ours/library/archive/src/shared/ByteStream.cpp` — `:347` freed-poison sentinel → full-width `uintptr_t` vs `0xefefefefefefefef` (width-correct, not cast-to-silence; D-06)

## Decisions Made

- **The full sweep is an integrity gate, not a rubber stamp.** When `-Scope all` revealed a class-(B) surface larger than any wave-2 scoped sweep ever compiled, the correct response (review #4) was to fix it in Phase 31 via gap-closure plans — NOT to widen the Phase-33 deferral list. An in-scope boot-path defect cannot escape via the residual valve.
- **Gap-closure chain over a single mega-plan.** Each full sweep unmasked the next previously-cl-aborted class-(B) layer (31-06 → 11 enumerated → 31-07 → ~154 AutoDelta* → 31-08 → 16 unmasked → 31-09). Each was a discrete user-authorized increment with its own re-sweep, capped when 0 NEW class-(B) remained (31-09 HARD CAP held with margin; NO 31-10).
- **Task 3 was approved at a higher bar than required.** The plan asked for boot-to-character-select; the user verified full in-world zone-in to Tatooine under both renderers. Recorded as the strongest possible D-08 regression proof.

## Deviations from Plan

The 3 gap-closure increments (31-07/08/09) are NOT deviations within this plan — they were authored as separate user-authorized plans per the Task 1 acceptance discipline ("If any (B) item exists and cannot be fixed within this plan's wave, STOP and flag it for a gap-closure plan rather than writing it to the deferral list"). They are the structurally-correct handling of the class-(B) surface this gate surfaced.

### Auto-fixed Issues (Task 1 escape valve)

**1. [Rule 1 - Bug] Fixed 8 enumerated wave-2 pointer-truncation escapes in-place**
- **Found during:** Task 1 (full scratch-x64 sweep + RESIDUAL-31-04/05 reconcile)
- **Issue:** 8 in-scope boot-path source sites (NON-owned by the wave-2 plans that surfaced them) carried `C4311`/`C4312` pointer truncations — the `%08x`/`%08X` diagnostic casts (incl. the `(DWORD)(uintptr_t)` double-cast that evades `/we4311`), the `ShellExecute` `HINSTANCE` return, and the ByteStream freed-poison sentinel that only compared the low 32 bits.
- **Fix:** `%p` + `static_cast<void const*>` for the log casts; `INT_PTR` + `%Id` for the ShellExecute return; full-width `uintptr_t` sentinel for ByteStream.
- **Files modified:** the 7 source files above (RenderWorld, Direct3d9, EditableAnimationState, CuiMediator, LeakFinder, WinMain, ByteStream)
- **Verification:** all sites 0 C4311/C4312 in the scratch x64 harness; the full 32-bit build links clean and both renderers boot + zone in.
- **Committed in:** `ba66d6657` (Task 1 escape valve)

---

**Total deviations:** 1 in-plan auto-fix (the escape valve, by design — the frontmatter declares this escape-valve scope) + 3 user-authorized gap-closure plans (31-07/08/09).
**Impact on plan:** All escape-valve fixes were correctness-required pointer-truncation defects. The gap-closure plans were the integrity-gate-mandated response to an undercounted class-(B) surface. No scope creep — every fix is an in-scope x64-correctness defect.

## Issues Encountered

- **The wave-2 substring-scoped sweeps undercounted the class-(B) surface.** The narrow `ScopeFilters` substrings (`-Scope bits01/bits02/bits03`) never compiled ~12 then ~154 in-scope TUs (the `__asm` survivors, the AutoDelta* C2665/C2668 cascade, the 16 TUs the AutoDelta clearance unmasked). The first full `-Scope all` sweep at this gate is what exposed them. Resolved by the 3 gap-closure increments + a CAPPED convergence test (31-09) that exhaustively classified every remaining failing TU and confirmed 0 NEW class-(B).
- **Cascade unmasking.** Clearing each class-(B) layer unmasked the next (cl had aborted early on the prior cascade). This is the documented 31-06 → 07 → 08 → 09 dynamic; the HARD CAP (user-set) bounded it and held with margin.

## Residual Hand-off to Phase 33 (D-02)

The consumer artifact is **`31-PHASE33-RESIDUAL-WORKLIST.md`**. The residue NOT forced clean in Phase 31, all CLASSIFIED class-(A) legitimate residue:

- **(A1) Runtime residuals** (compile-clean, need x64 RUNTIME validation): DebugHelp x64 backtrace WALK + the `uint32*` callStack Rip narrowing (review #6); the SSE `_mm_loadu_ps` 16-byte-alignment fault class to re-confirm on x64 (review #2); the FPU `_MCW_PC`-omitted-on-x64 / door-snap VERIFY-01 watch-item (D-04 → Phase 36).
- **(A2) Third-party / link-only / test residue:** the `_USE_32BIT_TIME_T` → 8-byte `time_t` width policy; all third-party x64 libs (dpvs/bink/pcre/libxml2/icu/jpeg/zlib/discord-rpc → X64-04); `WaterTestAppearance.cpp:97` (in-tree test code, off the boot path).
- **(A) C4244 long tail:** ~77 of the 80 C4244 are the D-07-EXCLUDED `__int64`→int container-count/`std::distance` paths + STL-header instantiation noise (the N2 / C4267 carry-forward) — NOT defects, must NOT be "fixed".
- **Confirmed harness artifacts (EXCLUDED, not defects):** Direct3d9 `#error must define FFP/VSPS` (real per-config vcxprojs define them); 5× winsock `C2371` SOCKET redefinition (WindowsWrapper.h `WIN32_LEAN_AND_MEAN` header-order artifact of isolated single-TU compile).
- **Reclassified tool-only:** the sharedTemplateDefinition char16_t/wchar_t Unicode cluster (Filename/TemplateData/TpfFile) — `sharedTemplateDefinition.lib` links ONLY into the pre-broken ShipComponentEditor/TemplateCompiler/TemplateDefinitionCompiler, never SwgClient (link-closure evidence) → out of v3.0 scope.
- **Standing-coverage notes:** **N1** — the gitignored scratch harness is thrown away, so NO committed `/we4311 /we4312` guardrail survives the Phase 31→33 gap; Phase 33 must re-establish enforcement on the committed x64 `.vcxproj`s. **N2** — `C4267` (`size_t`→smaller) was deliberately NOT promoted this phase (D-07); it is a Phase-33 carry-forward.

## Requirements Satisfied

- **BITS-01** (x87 inline asm → intrinsics; tree-wide `__asm` sweep) — 31-02 (FPU/SSE), 31-03 (misc __asm + DebugHelp), 31-07 (ByteOrder/RegexServices/MemoryManagerHook/RenderWorldServices/SSE-skinning). Certified at this gate: 0 `C4235` anywhere in the full sweep.
- **BITS-02** (pointer/int truncation as errors) — 31-04 (sort keys/MemoryManager/Os), 31-06 escape valve (the 8 enumerated escapes), 31-07 (PathSearch UAF + StatusWindow functional fixes + VoidBindSecond/LfgDataTable/AlterScheduler), 31-09 (TcpClient/TcpServer/CuiCombatManager/MeshConstructionHelper width members). Certified: 0 unaddressed in-scope-boot-path `C4311`/`C4312`.
- **BITS-03** (struct packing / serialization width) — 31-05 (Archive map uint32_t pin + TargaFormat/AssetCustomization static_asserts), 31-08 (AutoDelta* family uint32_t count pin), 31-07/09 (time_t display audits). Certified: 0 serialization-width overload-resolution failures in-scope; wire byte-identical to the 32-bit server.

## Next Phase Readiness

- **Phase 32 (SHADER-01 / D3DX → d3dcompiler_47)** is unblocked: the source tree is x64-clean for all in-scope class-(B) source and the shipped 32-bit build is proven non-regressive under both renderers.
- **Phase 33 (x64 Build Platform + D3D9)** owns the consumer of this gate's hand-off: `31-PHASE33-RESIDUAL-WORKLIST.md` (the third-party x64 relink X64-04, the `/we4311 /we4312` guardrail re-establishment N1, the C4267 policy N2, the `time_t` width policy, the runtime validation of the A1 residuals).
- **Phase 36 (VERIFY-01)** owns the door-snap confirmation against the kept CORNERSNAP probes (A1-FPU-PC).
- No blockers remain in Phase 31. The 3 prior Phase-31 open blockers (RESIDUAL-31-06 / DEF-31-07-FULLSWEEP / DEF-31-08-UNMASKED) are all RESOLVED by the gap-closure increments.

## User Setup Required

None — no external service configuration required.

## Self-Check: PASSED

- `31-06-SUMMARY.md` created — FOUND
- `31-PHASE33-RESIDUAL-WORKLIST.md` (D-02 hand-off) — FOUND
- Task commits verified present: `ba66d6657` (escape valve), `f7e3fed44` (sweep + worklist), `7b2be2b0d` (Task 2 link-grep), `8479fb6ba`/`1b6a98ff4`/`feaddc08e` (gap-closure 31-07/08/09) — all FOUND

---
*Phase: 31-64-bit-correctness-foundation*
*Completed: 2026-06-16*
