# Phase 31: 64-bit Correctness Foundation - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-15
**Phase:** 31-64-bit-correctness-foundation
**Areas discussed:** x64-clean validation method, Sweep & fix scope, Warnings-as-errors blast radius, BITS-03 audit depth + regression proof

---

## x64-clean validation method

| Option | Description | Selected |
|--------|-------------|----------|
| Scratch x64 compile harness (compiler-driven) | Throwaway Debug\|x64 compile-only config; cl.exe emits real C4311/4312/4244 + asm errors as the worklist | ✓ |
| Pure manual audit on 32-bit tree | Grep + inspection only; let Phase 33's first x64 compile surface residue | |
| Hybrid: scratch-compile high-signal libs only | Scratch-compile sharedFoundation/Math/Collision/Network/File/Memory; manual rest | |

**User's choice:** Scratch x64 compile harness (compiler-driven)
**Notes:** Highest signal; also the groundwork Phase 33 needs.

### Harness relationship to Phase 33 + acceptance bar

| Option | Description | Selected |
|--------|-------------|----------|
| Throwaway harness, hand a residual list to 33 | x64 config stays local/uncommitted; done = in-scope build-path libs compile clean; residual worklist → Phase 33 | ✓ |
| Commit a documented scratch x64 config | Commit the WIP x64 config so 33 inherits a baseline | |
| Harness must compile-clean 100% of build path | No residual deferral; every build-path lib clean before phase ends | |

**User's choice:** Throwaway harness, hand a residual list to 33

### FPU port semantics (BITS-01 correctness)

| Option | Description | Selected |
|--------|-------------|----------|
| Port faithfully, document P_24 as x64 no-op | _controlfp for rounding+exceptions; precision-control becomes a documented no-op + VERIFY-01 watch-item | ✓ |
| Preserve precision intent where it maps | Additionally audit code that depends on 24-bit single precision for determinism | |
| Defer the FPU semantics call to research | Lock only the asm→_controlfp swap; leave precision question to researcher | |

**User's choice:** Port faithfully, document P_24 as x64 no-op
**Notes:** Ties to the door-snap (VERIFY-01) — x64 deterministic SSE is the theory for why door-snap clears; lost x87 precision-force is part of that domain.

---

## Sweep & fix scope

| Option | Description | Selected |
|--------|-------------|----------|
| SwgClient boot path + 4 renderer plugins only | SwgClient + gl05/06/07/11 + their engine libs; editors explicitly OUT (MSB8066 pre-broken) | ✓ |
| Whole tree including pre-broken editors | Sweep entire solution, editors included | |
| Boot path now + cheap mechanical swaps tree-wide | Boot path fully + __asm int 3 → __debugbreak() everywhere | |

**User's choice:** SwgClient boot path + 4 renderer plugins only
**Notes:** DllExport.cpp (91 of 121 asm sites, all `__asm int 3`) is editor-side → out of scope.

### SseMath.cpp inline-asm port (the hard BITS-01 chunk)

| Option | Description | Selected |
|--------|-------------|----------|
| Faithful _mm_* intrinsic translation | Rewrite 13 asm SSE blocks as intrinsics; compiles both x86+x64, no #ifdef fork; verify usage first | ✓ |
| Audit usage first, then decide per-routine | Check which of the 13 are still live; rewrite live, delete dead | |
| Replace with portable C++ scalar fallback | Drop SSE; let MSVC auto-vectorize; needs perf sanity check | |

**User's choice:** Faithful _mm_* intrinsic translation (with per-routine usage audit folded in)

---

## Warnings-as-errors blast radius

| Option | Description | Selected |
|--------|-------------|----------|
| Enforce in the scratch x64 config; leave 32-bit flags untouched | /we4311/4312/4244 in the harness only; fixes land in shared source; committed 32-bit flags + global /wd4244 untouched | ✓ |
| Split: 4311/4312 errors committed tree-wide, 4244 scratch-only | Permanent committed guard on pointer-truncation; 4244 scratch-only | |
| All three as committed errors, lift the global /wd4244 | Strongest permanent enforcement; large 32-bit triage blast radius now | |

**User's choice:** Enforce in the scratch x64 config; leave 32-bit flags untouched
**Notes:** C4311/4312 barely fire on 32-bit (pointer==int); they only bite under the x64 compile — so the harness is their natural enforcement home. Keeps the shipped 32-bit build pristine.

---

## BITS-03 audit depth + regression proof

### Audit depth

| Option | Description | Selected |
|--------|-------------|----------|
| Width-changing types in serialized/packed structs only | Target pointer/size_t/ptrdiff_t/time_t in #pragma pack/sizeof/blob/Archive sites; skip explicit fixed-width | ✓ |
| Full serialization sweep | Inventory every IFF/network serializer regardless of type | |
| Width-changing types + enumerate sizeof/pack sites | Targeted fixes + full sizeof/#pragma pack inventory artifact | |

**User's choice:** Width-changing types in serialized/packed structs only
**Notes:** LLP64 — `long` stays 32-bit; movers are pointer/size_t/ptrdiff_t/time_t. `_USE_32BIT_TIME_T=1` is a known width lever.

### Regression proof

| Option | Description | Selected |
|--------|-------------|----------|
| Boot smoke + static_assert size/offset guards | Dual-renderer boot (proves live load/wire paths) + compile-time static_assert(sizeof/offsetof) on touched structs, baselined to 32-bit | ✓ |
| Boot smoke only | Rely on dual-renderer boot-to-char-select as sufficient evidence | |
| Boot smoke + golden round-trip fixtures | Add IFF/save/network round-trip byte-compare 32-bit before/after | |

**User's choice:** Boot smoke + static_assert size/offset guards

---

## Claude's Discretion

- Per-phase task sequencing (asm → truncation → layout, or interleaved).
- Exact structure of the residual-worklist artifact handed to Phase 33.
- Mechanical choice of width-correct replacement type per truncation site (`uintptr_t`/`intptr_t`/`DWORD_PTR`/`size_t`) — convention: fix with the correct width-preserving type, never cast-to-silence.

## Deferred Ideas

- Committed `/we4311 /we4312` as a permanent tree-wide guardrail — deferred; revisit once the x64 platform exists (Phase 33+).
- Full serialization sweep / golden round-trip fixtures — deferred behind the targeted audit + static_assert guards; heavier follow-up only if a layout regression surfaces.
- Editor-app x64 cleanup — out of scope; future concern only if the editors are revived.
- Committed x64 platform add, third-party x64 relink, D3DX removal — Phases 32–33 (already roadmapped).
