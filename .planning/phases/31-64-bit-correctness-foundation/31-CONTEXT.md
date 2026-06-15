# Phase 31: 64-bit Correctness Foundation - Context

**Gathered:** 2026-06-15
**Status:** Ready for planning
**Updated:** 2026-06-15 — locked the cross-AI review refinements to D-04/D-05 (see `**Review refinement**` notes; the decisions themselves are unchanged, the *implementation spec* is sharpened per 31-REVIEWS.md).

<domain>
## Phase Boundary

Make the **whole SwgClient build path compile x64-clean while the tree is still building 32-bit** — three correctness fronts:
- **BITS-01** — replace x64-illegal x87/SSE inline asm (`__asm`) with intrinsics; tree-wide `__asm` sweep over the build path.
- **BITS-02** — fix pointer/integer truncation defects (C4311/C4312/C4244 class).
- **BITS-03** — audit + correct IFF/TRE and network-message data-layout / serialization-width assumptions.

**No `x64` platform is added in this phase** — that is Phase 33. Phase 31 is source surgery on the still-32-bit committed tree, validated against the existing dual-renderer boot-to-character-select smoke (`rasterMajor=5` and `=11`). The committed shipped 32-bit build must not regress.

**Explicitly out of scope:** adding the `x64` platform to `swg.sln`/`.vcxproj`s (Phase 33), the D3DX→`d3dcompiler_47` port (Phase 32 / SHADER-01), any third-party x64 relink work (Phase 33 / X64-04), and fixing the pre-broken editor apps.

</domain>

<decisions>
## Implementation Decisions

### Validation method (how we generate the worklist + define "x64-clean")
- **D-01:** Stand up a **throwaway, uncommitted scratch `Debug|x64` *compile-only* config** as the compiler-driven worklist generator. It COMPILES the in-scope engine libs (no link — so no x64 third-party libs are needed) and lets `cl.exe` emit the real C4311/C4312/C4244 + inline-asm errors. This converts the audit from grep+eyeballs into a compiler-generated worklist. **It must NOT touch committed `swg.sln`/`.vcxproj`** — the committed x64 platform-add stays Phase 33 scope.
- **D-02:** **Phase-31 acceptance bar** = all in-scope build-path engine libs COMPILE x64-clean in the scratch config. Anything that genuinely cannot be validated without the platform/link infra (e.g. third-party-lib-dependent code) is documented as a **residual worklist handed forward to Phase 33** — not forced clean here.

### Sweep & fix scope
- **D-03:** Scope = **SwgClient boot path + the 4 renderer plugins (gl05/gl06/gl07/gl11) + every engine lib they link** (sharedFoundation, sharedMath, sharedCollision, sharedMemoryManager, sharedDebug, clientGame, sharedNetwork, sharedFile/IFF, etc.). **Editor apps are explicitly OUT** (ShaderBuilder, Headless, DllExport, AnimationEditor/ParticleEditor/SwgGodClient, …) — they are already pre-broken on MSB8066, are not part of the boot invariant, and are not the x64 ship target. Matches the established "validation bar = SwgClient clean + dual-renderer boot, not a green full-solution build" convention.
  - Consequence: `DllExport.cpp` (91 of the 121 `__asm` sites, all `__asm int 3` debug-break traps) is an editor-side helper → **out of scope** under D-03.

### BITS-01 — inline asm port specifics
- **D-04:** **`FloatingPointUnit.cpp`** — port faithfully to `_controlfp`/`_control87`. Rounding-mode and exception-masking bits map cleanly to SSE/MXCSR. **Precision control (P_24/P_53/P_64) has NO SSE equivalent** — `_controlfp` ignores `_MCW_PC` on x64, so `setPrecision()` becomes a **documented no-op on x64**. Document this explicitly as expected x64 behavior and a **VERIFY-01 (door-snap) watch-item** — x64's deterministic SSE float is the working theory for why the door-snap disappears, and the x87 24-bit-precision force is exactly that domain (see CONSULT-43 / `project_cantina_corner_snap_engine_quirk`).
  - **Review refinement (#1b, LOCKED 2026-06-15):** P_24 precision is **RETAINED on the shipped 32-bit build** via a `#if !defined(_M_X64)` guard — this is a **named decision, not a side effect of the mask argument**. On x64 the `_MCW_PC` write is **omitted entirely** (NOT passed to `_controlfp`/`_controlfp_s`/`_control87`) because it raises the invalid-parameter handler on x64 (it is NOT a silent no-op). The "documented no-op on x64" wording above means *the x64 hardware write is omitted*, NOT that precision is dropped on 32-bit. A naive reading of the original D-04 (drop P_24 on both bitnesses) is WRONG and was the top finding of all four reviewers.
  - **Review refinement (#1a, LOCKED 2026-06-15):** the module's `status` WORD is in **raw x87 hardware-control-word layout** (`ROUND_MASK=0x0C00`, `PRECISION_MASK=0x0300`, `EXCEPTION_ALL=0x003F`), which is **NOT** the MSVC abstract `_MCW_*`/`_RC_*`/`_PC_*`/`_EM_*` encoding the CRT FP API expects (different bit positions). The port MUST translate x87-layout ↔ `_MCW_*` **at the `get/setControlWord` boundary only** (or rewrite the module to traffic exclusively in `_MCW_*`) so `update()`'s `currentStatus != status` comparison keeps working. A grep-count acceptance is insufficient — a `_DEBUG` control-word round-trip self-check is required (see plan 31-02 Task 1).
- **D-05:** **`SseMath.cpp`** (13 real `__asm { }` SSE blocks) — **faithful `_mm_*` intrinsic translation**, preserving register-level semantics. Intrinsics compile on BOTH x86 and x64, so there is **no `#ifdef` fork** — the committed 32-bit build gets the same intrinsic code. **Usage-audit each of the 13 routines first**: rewrite the live ones; delete/stub any found dead under modern auto-vectorization.
  - **Review refinement (#2, LOCKED 2026-06-15):** "register-faithful" is sharpened to require (a) reproducing each routine's verified per-routine lane semantics (rotateScale 3-lane/w=0 vs rotateTranslateScale 4-lane/w=1; skin position-4-lane vs normal-3-lane asymmetry; Transform `sse_xf_matrix_3x4`'s `0x15` shuffle = broadcast lane 1 + zero lane 3); (b) `_mm_loadu_ps` for loads from the (unaligned) matrix/`Transform` pointer, reserving `_mm_load_ps` for `__declspec(align(16))` locals only (an aligned load on an unaligned `Transform` faults at RUNTIME on x64 — invisible to the compile-only harness); (c) retiring the global non-reentrant `sseVariable` staging array in favor of locals; (d) a `_DEBUG` numeric-equivalence self-test vs the scalar reference (boot smoke cannot detect a transposed lane). See plan 31-02 Tasks 2-3. `Transform.cpp`'s `__declspec(naked) sse_xf_matrix_3x4` is a PEER of SseMath (the live primary matrix-concat path), not a trivial sweep item.

### BITS-02 — warnings-as-errors enforcement
- **D-06:** Enforce **`/we4311 /we4312 /we4244` in the scratch x64 harness ONLY**. That is where pointer truncation actually fires (x64 pointers are 64-bit; on 32-bit pointer==int so C4311/C4312 barely fire) and where "x64-clean" is defined. The **truncation FIXES land in shared source** (so the committed 32-bit build gets them too), but the **committed 32-bit `.vcxproj` flags and the global `/wd4244` suppression stay exactly as-is** — no warning flood, no regression risk to the shipped build. (Pointer-truncation fixes use proper width-correct types — `uintptr_t`/`intptr_t`/`DWORD_PTR`/`size_t` — not cast-to-silence.)
  - **Review refinement (#3/#4, LOCKED 2026-06-15):** (a) pointer-derived **sort keys** use a uniform stable **hash-to-int** contract (`static_cast<int>(std::hash<uintptr_t>{}(reinterpret_cast<uintptr_t>(ptr)))`) that KEEPS the `int` return — so the `virtual int getSortKey()` interface, `ShaderPrimitiveSorter::Entry.shaderTemplateSortKey` (an `int` field), the comparators, and all 4 plugin impls do NOT widen (no shared-header ABI cascade). Widening the return type was the rejected alternative. (b) `/we4311` is BLIND to **explicit** casts (`reinterpret_cast<int>`, `(DWORD)(uintptr_t)`) — those need a separate grep audit (the harness emits `explicit-cast-audit.log`). (c) C4267 (`size_t`→smaller) is deliberately NOT promoted this phase (it would flood on the `signed int length` count paths excluded by D-07) — a Phase-33 carry-forward.

### BITS-03 — data-layout / serialization audit + regression proof
- **D-07:** **Targeted audit depth.** Audit only the types that actually change width on Windows **LLP64** x64: **pointer-derived fields, `size_t`, `ptrdiff_t`, `time_t`** (note the existing `_USE_32BIT_TIME_T=1` flag — a serialization-width lever) — wherever they appear in `#pragma pack` structs, hardcoded `sizeof()`-driven reads, `memcpy`/blob serialization, or `Archive` put/get of width-sensitive types. **`long` stays 32-bit on LLP64**, so it is NOT a mover. **Skip** the already-explicit fixed-width serialization (`int32`/`float`/explicit `get<T>`) that is the bulk of the IFF/network paths.
  - **Review refinement (#5, LOCKED 2026-06-15):** the `Archive` `std::map` raw `size_t numKeys` hazard does NOT silently widen the wire format — on x64 (`size_t` = `unsigned __int64`, and there is no 8-byte `get`/`put` overload) it **FAILS overload resolution = a compile error the harness catches**, not a silent runtime corruption. The `uint32_t` pin is still the correct wire-stable fix (binds the existing 4-byte overload). Because **zero** `Archive::get/put(std::map<...>)` instantiations exist under `src/engine`, the fix needs an explicit scratch instantiation TU to EXERCISE it (boot smoke won't). The `signed int length = source.size()` vector/set/deque count paths fire C4244 in the harness and are **pre-existing / EXCLUDED per D-07** (do NOT "fix"). Guard `static_cast<uint32_t>(source.size())` against `>UINT32_MAX`. See plan 31-05.
- **D-08:** **Regression proof = dual-renderer boot smoke + compile-time guards.** The existing boot-to-character-select smoke under both renderers already exercises TRE/IFF asset load + the login network handshake (proves the live layouts still parse). **PLUS add `static_assert(sizeof(X)==N)` / `offsetof` guards on each serialized/packed struct we touch**, baselined to the current 32-bit sizes — cheap, permanent, catches accidental layout drift forever, and the same asserts later document the intended x64 size.

### Claude's Discretion
- Per-phase task sequencing (asm → truncation → layout, or interleaved), exact structure of the residual-worklist artifact handed to Phase 33, and the mechanical choice of width-correct replacement type at each truncation site (`uintptr_t` vs `DWORD_PTR` vs `size_t`) are left to the planner/executor — the convention is "fix with the correct width-preserving type, never cast-to-silence."

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase requirements & milestone plan
- `.planning/ROADMAP.md` (Phase 31 block + v3.0 dependency-chain rationale) — BITS-01/02/03 success criteria; why 31 front-loads before the Phase 33 x64 link.
- `.planning/REQUIREMENTS.md` §"64-bit Correctness" — BITS-01/02/03 exact wording.
- `.planning/STATE.md` §"v3.0 x64 Port — the plan" + §Decisions — the locked milestone sequencing and core-invariant extension.
- `.planning/phases/31-64-bit-correctness-foundation/31-REVIEWS.md` — the cross-AI review feedback whose findings are locked into D-04/D-05/D-06/D-07 above (read before implementing the FPU/SSE/sort-key/Archive work).

### BITS-01 primary sites
- `src/engine/shared/library/sharedFoundation/src/win32/FloatingPointUnit.cpp` — the named x87 `__asm fnstcw/fldcw` site (D-04); precision-control no-op analysis; the raw-x87↔`_MCW_*` translation (review #1a).
- `src/engine/shared/library/sharedMath/src/win32/SseMath.cpp` — 13 inline-asm SSE blocks → `_mm_*` (D-05).
- Build-path `__asm` survivors to sweep (D-03): `sharedCollision/.../CollisionUtils.cpp`, `sharedMath/.../Transform.cpp`, `sharedFoundation/.../Clock.cpp`, `sharedFoundation/.../Fatal.cpp`, `sharedMemoryManager/.../MemoryManager.cpp`, `sharedDebug/.../DebugHelp.cpp`, `sharedDebug/.../ProfilerTimer.cpp`, `clientGame/.../HTTPpost/VeCritsec.hpp`. (Editor-app sites incl. `DllExport.cpp` are OUT per D-03.)

### x64 reference (behavior/availability, NOT a source diff)
- `D:\SWG Restoration\x64\` — stable x64 D3D9 client; **binaries only** (per `reference_restoration_binaries_intel`). Use as a door-snap/OOM behavior reference, not a source-patch to lift.
- Door-snap root-cause context (VERIFY-01 link for D-04): `.planning/research/CONSULT-43-*.out`; memory `project_cantina_corner_snap_engine_quirk`.

### Build/convention constraints
- `AGENTS.md` §Build — `$env:MSBUILD` (VS18/v145), `/nodeReuse:false`, run from PowerShell, `/FORCE` link-grep (0 `unresolved external symbol`), shared-header ABI cascade trap, editor-apps-pre-broken validation bar.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **Dual-renderer boot smoke** is the in-place regression gate (D-08) — no new harness needed for the functional proof; `rasterMajor` in `client_d.cfg` selects the renderer (Debug exe → `client_d.cfg`).
- **`_USE_32BIT_TIME_T=1`** compile flag already in the build — a known serialization-width lever to factor into the BITS-03 `time_t` audit (D-07).

### Established Patterns
- **`/FORCE` link masks unresolved externals** → every link gate greps the build log for `unresolved external symbol` (must be 0); MSBuild exit 0 is NOT proof.
- **Shared-header ABI cascade trap** — touching a public struct in a shared header (e.g. adding `static_assert` is safe, but changing layout is not) silently breaks stale plugin DLLs → boot crash. If a serialized struct in a shared header is altered, rebuild ALL plugin `.vcxproj`s (gl05/06/07/11), not just SwgClient.
- **Intrinsics compile both bitnesses** (D-05) — no `#ifdef _M_X64` fork is needed for the SSE-asm→intrinsic rewrite; this keeps the committed 32-bit build on the same code path. (The FPU path has exactly ONE justified `#if defined(_M_X64)` guard — the precision write — per review #1b.)

### Integration Points
- Scratch x64 config (D-01) compiles the same `.cpp`/`.h` source the committed 32-bit projects use — fixes are shared-source edits, so the 32-bit build inherits them automatically and is re-validated by the boot smoke (D-08).
- The residual worklist (D-02) is the explicit hand-off artifact into Phase 33 (x64 platform add).

</code_context>

<specifics>
## Specific Ideas

- The scratch x64 harness is a **signal generator, not a deliverable** — keep it local/uncommitted; its value is the compiler worklist, not a checked-in artifact. (Consequence noted in review #8: no standing `/we4311` guardrail survives the harness's deletion across the Phase 31→33 gap — recorded in the residual worklist for Phase 33 to re-establish.)
- The FPU precision no-op (D-04) is deliberately framed as a **VERIFY-01 hypothesis confirmation**, not a defect: if door-snap clears on x64 as theorized, the lost x87 24-bit precision force is part of *why*. (On 32-bit the precision force is RETAINED — review #1b.)

</specifics>

<deferred>
## Deferred Ideas

- **Committed `/we4311 /we4312` as a permanent tree-wide guardrail** — considered, deferred. Enforcement lives in the scratch x64 config this phase (D-06); a permanent committed guard could be revisited once the x64 platform exists (Phase 33+).
- **Full serialization sweep / golden round-trip fixtures** — considered for BITS-03 proof, deferred in favor of the targeted audit (D-07) + static_assert guards (D-08). Full inventory or round-trip test infra is a heavier follow-up if a layout regression ever surfaces.
- **Editor-app x64 cleanup** (ShaderBuilder/Headless/DllExport/etc.) — out of scope (D-03); a future concern only if/when the editors are revived (they are MSB8066-pre-broken independent of x64).
- Adding the committed `x64` platform, third-party x64 relink, and D3DX removal — Phases 32–33 (already roadmapped).

### Reviewed Todos (not folded)
- `todos/pending/2026-06-13-64bit-x64-port.md` — this is the whole-milestone driver, not a Phase-31-specific todo; it is satisfied across Phases 31–36, not folded into a single phase.

</deferred>

---

*Phase: 31-64-bit-correctness-foundation*
*Context gathered: 2026-06-15*
