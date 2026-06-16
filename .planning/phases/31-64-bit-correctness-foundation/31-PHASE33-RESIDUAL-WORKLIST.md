# Phase 31 → Phase 33 Residual Worklist (D-02 hand-off)

**Created:** 2026-06-16 (plan 31-06, the phase gate)
**Consumer:** **Phase 33** (X64-01/04/02 — x64 Build Platform + D3D9 renderers)
**Purpose:** The explicit D-02 hand-off of every item that is NOT forced x64-clean inside Phase
31, each CLASSIFIED (review #4) as **(A) legitimate residue** (third-party / link-only / runtime-only —
deferrable to Phase 33) vs **(B) phase escape** (in-scope boot-path SOURCE that was merely unlisted —
which MUST be fixed in Phase 31, never deferred).

> **Phase 31 acceptance bar (D-02):** all in-scope build-path engine libs COMPILE x64-clean in the
> scratch `Debug|x64` compile-only harness (0 `C4235`/`C4311`/`C4312`/`C4244`), EXCEPT items classified
> (A) below. The committed 32-bit canonical 5-target build links clean (0 `unresolved external symbol`)
> and boots both renderers to character select (the D-08 regression proof). The scratch harness is a
> **signal generator, not a deliverable** — gitignored, thrown away after this phase.

---

## Integrity result (the (A)/(B) classification gate, review #4)

> **CRITICAL FINDING (2026-06-16, plan 31-06 full sweep):** The FIRST full `-Scope all` sweep over all
> 2217 in-scope TUs surfaced a LARGER class-(B) survivor set than the wave-2 scoped (substring-filtered)
> sweeps ever compiled. The wave-2 BITS-01/02/03 sweeps used narrow `ScopeFilters` substrings and
> therefore NEVER compiled ~12 in-scope TUs that carry genuine x64 defects. Per review #4 these are
> class (B) phase escapes that MUST be fixed in Phase 31, and per the plan's own Task 1 acceptance
> ("If any (B) item exists and cannot be fixed within this plan's wave, STOP and flag it for a
> gap-closure plan rather than writing it to the deferral list"), they are flagged below for
> **GAP CLOSURE (a new Phase-31 plan)** — NOT deferred to Phase 33.

- The **8 enumerated** wave-2 escapes (RESIDUAL-31-04: 6 must-fix; RESIDUAL-31-05: ByteStream.cpp:347)
  were **fixed in-place in plan 31-06** (commit `ba66d6657`), enumerated in `31-06-SUMMARY.md`.
- The full sweep additionally surfaced **NEW class-(B) escapes** (see the GAP-CLOSURE section below):
  13 `C4235` `__asm` sites + ~8 genuine `C4311`/`C4312` pointer-truncations + 2-3 `time_t` width-movers.
  These are NOT deferred — they require a Phase-31 gap-closure plan.
- The 80 `C4244` are dominated by D-07-EXCLUDED container-count narrowings (`__int64`→`int` from
  `.size()`/`distance` — same semantic class as the excluded C4267 count paths, codex-validated) plus
  STL-header instantiation noise from the global `/we4244` promotion. These are class (A) (N2 carry-forward).
- The only EXPLICIT-cast site (`/we4311`-blind) remaining after the escape-valve fixes is the
  **(A)-deferrable** `WaterTestAppearance.cpp:97` test-harness item.

---

## (A) LEGITIMATE RESIDUE — deferrable to Phase 33

These are genuinely third-party / link-only / runtime-only. They CANNOT be forced clean on the
still-32-bit, compile-only tree (no x64 platform/link infra, or the defect is a RUNTIME behavior the
compile-only harness cannot observe).

### A1. RUNTIME residuals (compile-clean in Phase 31, need RUNTIME x64 validation in Phase 33)

| ID | Site | Residual | Phase-33 action |
|----|------|----------|-----------------|
| A1-DBGHELP-WALK | `sharedDebug/.../win32/DebugHelp.cpp` (`#if defined(_M_X64)` `RtlCaptureContext` fork, plan 31-03 Task 3) | The x64 backtrace **WALK** (`StackWalk64` over the AMD64 context) is **compile-clean only** — it was never executed on a real x64 build. The 64-bit `Rip/Rsp/Rbp` `.Offset` fields are full-width (no `DWORD` truncation, review #6), but the walk itself is unvalidated. | RUNTIME-validate the x64 stack walk produces correct frames once the x64 platform exists. |
| A1-DBGHELP-RIP | `sharedDebug/.../win32/DebugHelp.cpp` (`uint32*` callStack output) | The callStack **output buffer is `uint32*`**, so even with a full-width capture the stored Rip is **narrowed to 32 bits** when written to the output array (review #6). Compile-clean (the narrowing is into an existing 32-bit-wide API contract), runtime-lossy on x64. | Widen the callStack output contract to `uintptr_t`/`DWORD64` (or document the truncation as accepted) at Phase-33 once x64 addresses are live. |
| A1-SSE-ALIGN | `sharedMath/.../win32/SseMath.cpp` + `sharedMath/.../Transform.cpp` (plan 31-02) | The SSE intrinsic rewrite uses `_mm_loadu_ps` for the unaligned matrix/`Transform` loads (an aligned `_mm_load_ps` on an unaligned `Transform` faults at RUNTIME on x64 — invisible to the compile-only harness, review #2). This is believed correct, but the **16-byte-alignment fault class must be RE-CONFIRMED on a real x64 run**. The plan-02 `_DEBUG` numeric-equivalence self-tests cover lane correctness; they do NOT cover an alignment fault that only fires on the x64 movaps path. | RUNTIME-confirm no alignment fault on x64 across the live matrix/skin paths; the `_DEBUG` oracles + boot smoke are the trip-wire. |
| A1-FPU-PC | `sharedFoundation/.../win32/FloatingPointUnit.cpp` (`_MCW_PC` omitted on x64, plan 31-02) | The x87 24-bit precision force (`P_24`) is **RETAINED on 32-bit** (`#if !defined(_M_X64)`, review #1b) and **omitted on x64** (the `_MCW_PC` write raises the invalid-parameter handler on x64, it is not a silent no-op). This is the **VERIFY-01 door-snap watch-item** (D-04): x64's deterministic SSE float is the working theory for why the cantina door-snap disappears, and the dropped x87 24-bit precision force is exactly that domain (CONSULT-43 / `project_cantina_corner_snap_engine_quirk`). | Phase 36 (VERIFY-01) confirms door-snap clean on x64 vs the kept CORNERSNAP `_DEBUG` probes + the Restoration x64 D3D9 reference. Not a defect — an expected x64 behavior change to verify. |

### A2. Third-party / link-only / build-platform residue (no x64 header/lib until Phase 33 link)

| ID | Site | Residual | Phase-33 action |
|----|------|----------|-----------------|
| A2-TIME-T | `_USE_32BIT_TIME_T=1` (committed 32-bit build flag) vs x64 | On x64 the UCRT **hard-errors** if `_USE_32BIT_TIME_T` is defined under `_WIN64`, so the scratch harness DROPS it → `time_t` is 8 bytes on x64 (recorded by plan 31-01). This is a serialization-width lever (D-07) that only materializes once the x64 platform is added; any wire/blob struct that embeds a raw `time_t` must be audited at link time. | At Phase-33 platform-add, decide the x64 `time_t` width policy and re-audit any `time_t`-bearing serialized struct against the 32-bit wire format. |
| A2-3RDPARTY-LIBS | dpvs / bink / pcre / libxml2 / icu / jpeg / zlib / discord-rpc | These are **link-only** at Phase 33 (X64-04) — the scratch harness is compile-only (`cl /c`, no link), so no x64 third-party lib is needed and none was resolved here. Restoration's `x64/` is the availability map. | Resolve all third-party x64 libs at Phase-33 (X64-04) as a build/link exercise. |
| A2-WATERTEST | `clientGame/.../test/WaterTestAppearance.cpp:97` — `reinterpret_cast<int>(&vertexBuffer)` | **(A)-deferrable test code, NOT on the boot path** (RESIDUAL-31-04 class (b)). It is the SOLE explicit-cast site remaining after the plan-06 escape-valve fixes (confirmed by `-CastAudit` post-fix). In-tree test harness, not vendored. Same hash-to-int sort-key shape if revived. | Fix opportunistically at Phase 33 (or whenever the water test harness is revived); harmless on the boot path. |

---

## (B-GAP) NEW PHASE ESCAPES — require a Phase-31 GAP-CLOSURE plan (NOT deferred to Phase 33)

These were surfaced by the FIRST full `-Scope all` sweep (plan 31-06). They are in-scope boot-path
source (all confirmed present in committed engine-lib `.vcxproj`s) and are genuine x64 defects, NOT
third-party/runtime/link-only residue. Per review #4 + the plan-06 Task 1 acceptance, they CANNOT be
deferred to Phase 33 — they need a new Phase-31 gap-closure plan. They exceed the plan-06 gate's
enumerated escape-valve (Rule 4: the asm rewrites + the int-mark pointer-store + the shared-header
C4312 are structural). Listed here as the gap-closure worklist, NOT as Phase-33 deferrals.

### B-GAP-1. BITS-01 — x64-illegal `__asm` survivors (13 sites, 6 TUs)
The wave-2 BITS-01 sweep (`-Scope bits01`) only filtered for FloatingPointUnit/SseMath/Transform/
CollisionUtils/Fatal/Clock/ProfilerTimer/DebugHelp/VeCritsec — it never compiled these:

| TU | Sites | Asm | Fix shape |
|----|-------|-----|-----------|
| `sharedFoundation/.../win32/ByteOrder.cpp` | 18,28,38,49 | `__declspec(naked)` `bswap` ntohl/htonl/ntohs/htons | `_byteswap_ulong`/`_byteswap_ushort` intrinsics (drop naked) |
| `sharedRegex/.../win32/RegexServices.cpp` | 21 | `__declspec(naked)` MemoryManager::allocate trampoline | normal function calling `MemoryManager::allocate` (return-addr owner via `_ReturnAddress()`) |
| `Direct3d11/.../shared/MemoryManagerHook.cpp` | 34,60 | `__asm` (alloc hook) | intrinsic/normal-fn port |
| `Direct3d9/.../shared/MemoryManagerHook.cpp` | 28,54 | `__asm` (alloc hook) | intrinsic/normal-fn port |
| `clientGraphics/.../shared/RenderWorldServices.cpp` | 48 | `__asm` | intrinsic/normal-fn port |
| `clientSkeletalAnimation/.../appearance/SoftwareBlendSkeletalShaderPrimitive.cpp` | 1739,2044 | `__asm` (SSE skinning blocks) | `_mm_*` intrinsics (PEER of the SseMath/Transform work in plan 31-02; needs the same `_DEBUG` numeric oracle) |

### B-GAP-2. BITS-02 — genuine pointer-truncation `C4311`/`C4312` (in-scope, non-third-party)
The wave-2 BITS-02 sweep filtered for StaticShader/MemoryManager/Os/Direct3d*VertexBufferData/
Direct3d9*ShaderData/RenderWorld/ShaderEffect — it never compiled these:

| Site | Cast | Severity | Fix shape |
|------|------|----------|-----------|
| `sharedPathfinding/.../PathSearch.cpp:189/196/215/500` | `(int)((void*)PathSearchNode*)` stored in an `int` node MARK field, read back as `(PathSearchNode*)((void*)mark)` | **FUNCTIONAL on x64** (a real pointer round-tripped through a 32-bit int field → corruption + use-after-free, NOT just a log) | widen the mark storage to `intptr_t`/`void*` (touches `PathNode::getMark/setMark` — structural) |
| `sharedStatusWindow/.../win32/StatusWindow.cpp:144` | `reinterpret_cast<LONG>(this)` for `SetWindowLong(GWL_USERDATA)` | **FUNCTIONAL on x64** (window user-data pointer truncated) | `SetWindowLongPtr` + `GWLP_USERDATA` + `LONG_PTR`/`reinterpret_cast<LONG_PTR>` |
| `sharedFoundation/.../VoidBindSecond.h:48` | `C4312` `BindArgumentType`→pointer `SecondArgumentType` of greater size in a SHARED template header | shared-header template | inspect the instantiation (likely a pointer bound via `VoidBindSecond`); fix the cast width — **shared-header touch = ABI-cascade caution** |
| `sharedGame/.../core/LfgDataTable.cpp:108` | `reinterpret_cast<unsigned long>(void*)` level compare (`unsigned long`=32-bit LLP64) | logic (pointer-as-int comparison) | widen to `uintptr_t` (verify the table semantics — these are sentinel level bounds packed as void*) |
| `sharedObject/.../object/AlterScheduler.cpp:316` | `reinterpret_cast<unsigned int const>(object)` for a `%x` WARNING | diagnostic only | `%p` + `static_cast<void const*>` (same shape as the 31-06 escape-valve fixes) |

### B-GAP-3. BITS-03 — `time_t` width-movers (audit required, codex-validated as in-scope)
On x64 `time_t` is 8 bytes (the harness drops `_USE_32BIT_TIME_T`; see A2-TIME-T). These narrow it:

| Site | Cast | Disposition |
|------|------|-------------|
| `clientGame/.../object/GroupObject.cpp:413/424` | `return` `time_t`→`unsigned int` | **AUDIT** — if this value is stored/serialized/compared as object state it is a real BITS-03 defect; if pure display, benign. Triage at gap closure. |
| `swgClientUserInterface/.../page/SwgCuiGroup.cpp:269/272` | `time_t`→`long`/`unsigned int` | **AUDIT** — likely UI display/duration (probably benign) but must verify the use, not blanket-exclude (codex). |

---

## (B) PHASE ESCAPES — fixed in Phase 31 (NOT deferred; the 8 enumerated wave-2 escapes)

Per review #4 these MUST NOT appear on a deferral list. They were fixed in-place in plan 31-06 (the
escape valve, commit `ba66d6657`) and are enumerated in `31-06-SUMMARY.md`. Recorded here only to prove
the (A)/(B) gate was applied and these 8 enumerated escapes did not escape.

| Site | Defect | Fix (plan 31-06) |
|------|--------|------------------|
| `clientGraphics/.../RenderWorld.cpp:1127` | `reinterpret_cast<int>(DPVS::Object*)` for `%08x` leaked-object DEBUG_REPORT_LOG | `%p` + `static_cast<void const*>` |
| `Direct3d9/.../Direct3d9.cpp:137/183/185/203` | `(DWORD)(uintptr_t)` base/device/vtable/entry for `%08X` (the double-cast EVADES `/we4311`, review #4) | `%p` + `static_cast<void const*>` (RVA pointer-diffs at :187/:202 kept — they fit `DWORD` within the image) |
| `clientSkeletalAnimation/.../EditableAnimationState.cpp:436/489` | `reinterpret_cast<int>(child/link)` for `%08x` WARNING | `%p` + `static_cast<void const*>` |
| `clientUserInterface/.../CuiMediator.cpp:1009` | `reinterpret_cast<int>(mediator)` for `%08x` profiler-name dump | `%p` + `static_cast<void const*>` |
| `sharedDebug/.../LeakFinder.cpp:137` | `reinterpret_cast<int>(...->object)` for `%08X` leak REPORT | `%p` + `static_cast<void const*>` |
| `SwgClient/.../win32/WinMain.cpp:88/90` | `reinterpret_cast<int>(HINSTANCE)` ShellExecute return | `INT_PTR result` + `<= 32` + `%Id` (mirrors the Os.cpp launchBrowser fix from 31-04) |
| `archive/.../ByteStream.cpp:347` | `reinterpret_cast<unsigned int>(Data*)` C4311 in the `0xefefefef` freed-memory-poison sentinel assert (RESIDUAL-31-05) | full-width `reinterpret_cast<uintptr_t>` vs a `uintptr_t` `0xefefefefefefefef` sentinel (width-correct, not cast-to-silence; D-06) |

---

## STANDING-COVERAGE notes for Phase 33

### N1. No standing `/we4311 /we4312` guardrail survives the harness deletion (review #8)
The scratch `Debug|x64` harness (`src/build/win32/x64-scratch/`) is **gitignored and thrown away** at
the end of Phase 31 — it is a signal generator, not a deliverable (D-01 / specifics). Therefore **NO
committed `/we4311 /we4312 /we4244`-as-error guardrail survives the Phase 31 → 33 gap.** Once the
committed `x64` platform exists, **Phase 33 must RE-ESTABLISH this enforcement** on the committed x64
`.vcxproj`s so pointer-truncation cannot silently regress. This is a known coverage hole, recorded here
deliberately so it is not lost.

### N2. C4267 (`size_t` → smaller) was deliberately NOT promoted this phase (review #7 / D-06c)
`C4267` (`size_t` → smaller narrowing) is **a Phase-33 carry-forward.** It was intentionally NOT promoted
to an error in Phase 31 because it would flood on the `signed int length = source.size()` count paths
that D-07 explicitly EXCLUDES (the `std::vector`/`set`/`deque` Archive count paths — pre-existing, NOT
defects). Phase 33 should decide the `C4267` policy once the x64 platform + the wider serialization audit
exist. **Do NOT confuse the D-07-excluded vector C4244 at `Archive.h:348/360/370` with a survivor** — it
is the documented exclusion (review #5), not a defect, and must NOT be "fixed".

---

## Summary

| Bucket | Count | Disposition |
|--------|-------|-------------|
| (A) runtime residuals | 4 (A1-DBGHELP-WALK, A1-DBGHELP-RIP, A1-SSE-ALIGN, A1-FPU-PC) | RUNTIME-validate on x64 (Phases 33/36) |
| (A) third-party / link-only / test residue | 3 (A2-TIME-T, A2-3RDPARTY-LIBS, A2-WATERTEST) | Resolve at Phase-33 link / opportunistic |
| (A) C4244 container-count + STL-header noise | ~77 of the 80 C4244 | D-07-EXCLUDED (N2 / C4267 carry-forward); NOT defects |
| **(B-GAP) NEW phase escapes** | **13 `__asm` + ~8 `C4311`/`C4312` + 2-3 `time_t`** | **Phase-31 GAP-CLOSURE PLAN (NOT deferred)** |
| (B) enumerated wave-2 escapes | 8 source sites FIXED in 31-06 (`ba66d6657`) | none escape — see 31-06-SUMMARY.md |
| Standing-coverage holes for Phase 33 | 2 (N1 guardrail re-establish, N2 C4267 carry-forward) | Phase 33 to-establish |

**BLOCKING:** The (B-GAP) escapes mean Phase 31's D-02 acceptance bar ("all in-scope build-path engine
libs COMPILE x64-clean") is NOT YET MET. They require a new Phase-31 gap-closure plan before plan-06
Task 2 (the full 32-bit build) + Task 3 (the boot smoke) can certify the phase. They are NOT eligible
for Phase-33 deferral (review #4).

**Phase 33 owns:** the x64 build platform add, the third-party x64 relink (X64-04), re-establishing the
`/we4311 /we4312` guardrail (N1), the C4267 policy (N2), the `time_t` width policy (A2-TIME-T), and the
RUNTIME validation of A1-DBGHELP-WALK / A1-DBGHELP-RIP / A1-SSE-ALIGN. **Phase 36 owns** the
VERIFY-01 door-snap confirmation (A1-FPU-PC).
