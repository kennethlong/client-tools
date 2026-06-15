---
phase: 31
reviewers: [codex, cursor, opus, sonnet]
reviewed_at: 2026-06-15
plans_reviewed: [31-01-PLAN.md, 31-02-PLAN.md, 31-03-PLAN.md, 31-04-PLAN.md, 31-05-PLAN.md, 31-06-PLAN.md]
note: "claude CLI skipped (running inside Claude Code, independence); gemini/qwen/opencode/coderabbit not installed. Codex (gpt-5.5) + Cursor are the external CLIs; fresh Sonnet + fresh Opus added via the phone-a-friend crew for math/spec + lateral angles."
---

# Cross-AI Plan Review — Phase 31 (64-bit Correctness Foundation)

Four independent reviewers on non-overlapping angles: **Codex** (gpt-5.5, repo tracer),
**Cursor** (detailed code reader, source-grounded), fresh **Opus** (FPU/SSE spec + serialization
math, source-verified), fresh **Sonnet** (lateral blind-spot). All four read the same prompt
(PROJECT context + ROADMAP + CONTEXT + VALIDATION + all 6 plans). Raw outputs are in
`.planning/research/gsd-review-31-{codex,cursor,opus,sonnet}.out`.

---

## Codex Review (gpt-5.5)

**Summary** — The phase is thoughtfully decomposed and mostly sound: it separates source-level x64 correctness from the later platform/link work, uses compiler diagnostics as the primary signal, and preserves the 32-bit boot gate. The biggest risks are not in the phase shape but in false confidence: the scratch harness may not faithfully reproduce MSBuild compilation, the SSE rewrites are under-specified for numerical equivalence, `_control87` may change more FP state than the original x87-only `fldcw`, and BITS-03 is too narrow for a real serialization/layout audit. Approve the direction, not the plans as written without tightening harness fidelity, adding math equivalence checks, and explicitly expanding the residual/layout inventory.

**Strengths**
- Clear phase boundary: no committed x64 platform, no x64 third-party relink, no D3DX work — prevents Phase 31 becoming the whole port.
- Compiler-driven worklist is the right primary tool for `__asm`, C4311/C4312, many C4244.
- Keeps fixes in shared source and re-validates the Win32 client afterward.
- Correctly treats `/FORCE` link success as untrusted; keeps the unresolved-symbol grep gate.
- Editor/tool projects correctly out of scope.
- Recognizes `size_t` serialization danger under LLP64 and that `long` stays 32-bit on x64.
- Avoids cast-to-silence and warning suppression.
- Final residual worklist guards against pretending third-party/runtime-only x64 behavior was proven.

**Concerns**
- **HIGH — Scratch harness fidelity is likely weaker than the plans imply.** A raw `cl /c` driver scraping include dirs from `.vcxproj` can miss imported property sheets, forced includes, per-file compiler options, PCH assumptions, generated headers, project references, aggregator defines, config-specific macros → false positives AND false negatives. Calling it "the authoritative enumerator" is too strong unless MSBuild-derived or proven equivalent.
- **HIGH — The harness is gitignored but plans 02–06 depend on it.** Operationally brittle across agents, clean worktrees, context switches, CI. Commit a generated manifest/driver checksum or deterministic reproduction instructions.
- **HIGH — `_control87` may not faithfully replicate old x87-only behavior on Win32.** `fnstcw/fldcw` touched the x87 CW only; `_control87` can affect CRT FP state and possibly MXCSR. The 32-bit shipped build still runs SSE math, so this could change 32-bit behavior. Decide explicitly whether Win32 keeps the exact old x87 path or uses a CRT API and proves MXCSR is unchanged.
- **HIGH — SSE intrinsic rewrites need numerical tests, not only boot smoke.** Register/lane mapping, row/column order, matrix stride, w-lane, alignment, accumulation order can be subtly wrong while still booting. Add deterministic equivalence tests vs the old asm on Win32.
- **HIGH — Alignment assumptions not called out.** Existing asm may `movaps`/assume 16-byte alignment; `_mm_load_ps` faults if not aligned. Audit each source pointer; choose `_mm_load_ps` vs `_mm_loadu_ps` intentionally.
- **HIGH — BITS-03 scope too optimistic.** Pinning `Archive<std::map>` counts + Targa/customization asserts is useful but doesn't prove IFF/TRE/network layout. Width-sensitive types hide in `std::vector<size_t>`, pointer-derived IDs, `time_t`, enum-size assumptions, `sizeof(class)` blob writes, `memcpy` of non-POD structs, custom `Archive::put` overloads.
- **MEDIUM** — static_asserts only protect touched structs (spot-hardening, not an audit); sort-key widening may change public/virtual/ABI contracts; C4244-as-error may swamp real pointer-width work with benign narrowings; DebugHelp x64 branch compiles but may not walk (Rbp not guaranteed frame pointer); VeCritsec `_interlockedbittestandset((long*)...)` needs size/volatile review; `sqrtf` may not be bit-identical to x87 `fsqrt`; "full sweep or residual" weakens acceptance unless residual eligibility is strict.
- **LOW** — `.gitignore` change for an intentionally-uncommitted tool is odd; grep-based acceptance (`_MCW_PC` only inside x64 guard, `grep -c x64 swg.sln == 0`) is fragile — prefer targeted diffs.

**Risk: MEDIUM-HIGH.** Well-scoped, sensible ordering, but implementation risk is high in math + serialization. The most dangerous blind spots: silent SSE math drift, FP-control behavior change in the still-shipped 32-bit build, incomplete layout/serialization coverage.

---

## Cursor Review (source-grounded)

**Summary** — Thoughtfully structured: scratch harness as worklist generator, wave parallelism, explicit Phase 33 residual hand-off, strong repo-convention alignment (`/FORCE` link-grep, dual-renderer boot, ABI cascade). Correctly treats `Transform.cpp`'s naked SSE as first-class. But against the live tree there are **material correctness gaps** — FPU control-word masking that can break 32-bit `P_24`, an incomplete BITS-02 surface (D3D9 sort keys, header-inline truncations), a scratch harness that gives false confidence if the TU manifest/warning set is incomplete, and an under-proven Archive map fix.

**Strengths** — compiler-as-oracle is right (grep found ~169 `__asm` across 29 files); crisp phase boundary; research corrections baked in (`_MCW_PC` on x64, Transform as peer to SseMath, no phantom `/wd4244`); residual worklist explicit; intrinsics-on-both-bitness avoids `#ifdef` forks; validation layering appropriate for a no-unit-test tree; threat models on sort-key collisions / Archive wire format / `/FORCE` false-clean are real.

**Concerns**
- **HIGH #1 — `FloatingPointUnit::setControlWord` may stop applying precision on 32-bit (regression).** Plan masks to `_MCW_RC | _MCW_EM` (no `_MCW_PC`), but `install()` builds a full `status` incl. `PRECISION_24` and calls `setControlWord(status)`. Masking out PC means `install()`'s `PRECISION_24` never reaches the FPU on **32-bit either** → directly undermines the cantina door-snap theory (x87 24-bit precision) on the still-shipped 32-bit build.
- **HIGH #2 — BITS-02 scope leaves in-scope boot-path truncations to "gate sweep" roulette.** Verified harness will also hit `Direct3d9_StaticVertexBufferData.cpp:131`, `Direct3d9_DynamicVertexBufferData.cpp:262`, `Direct3d9_StaticShaderData.cpp:388`, `ShaderEffect.h:93` — none in plan 04's `files_modified`. Sort-key truncation in D3D9 is live boot-path code, not link-time residue.
- **HIGH #3 — Sort-key widening has underestimated ABI blast radius.** `getSortKey()` is `virtual int` in shared headers consumed by all four plugins (`StaticVertexBuffer.h:27`). Widening requires changing the virtual interface + every impl + inline wrappers — a cross-plugin ABI event. Mandate a repo-wide contract decision (hash-to-int vs widen) upfront.
- **HIGH #4 — Scratch harness completeness = phase acceptance integrity.** A "first cut" manifest can produce a green gate with large latent defect populations in unlisted TUs. The harness is only as good as `in-scope-tus.txt` + per-lib include parity.
- **MEDIUM** — FPU vs SSE float environment divergence on x64 (`_control87` changes x87 CW but transforms/skinning run on SSE/MXCSR; door-snap fix may come from SSE determinism, not `_control87` parity); `canDoSseMath` try/catch → `__cpuid` becomes dead code; SSE alignment + horizontal-sum + manual stack-alignment-trick risks; DebugHelp `DWORD(Offset)` already truncates Rip — belongs in the Phase 33 residual; Archive map fix likely unexercised (zero `Archive::get/put(std::map)` instantiations under `src/engine` → boot smoke won't prove it; needs an explicit instantiation TU); warning set narrower than "x64-clean" (missing C4267 `size_t`→`int`, C5026/C5027); wave-2 plans make behavioral changes without mid-wave boot validation; `PLATFORM_WIN32` parity must come through `First*.h` chain not just `-D`.
- **LOW** — plan 05 path typo (`src/shared/Archive.h` vs `src/external/ours/library/archive/...`); committed `.gitignore` line is a permanent footprint; `VeCritsec` type-punning; TargaFormat `pack(push)` uses `PLATFORM_WIN32` but `pack(pop)` uses `_WIN32` (pre-existing); plan 02 full 5-target build only in Task 3.

**Risk: MEDIUM-HIGH.** Architecture/boundaries strong (LOW as a doc alone); execution risk elevated because the FPU mask spec can regress the shipped 32-bit client, BITS-02 isn't closed across the declared in-scope boot path, SSE/FPU lack numeric golden tests, and compile-only is weak for header-only templates + uninstantiated serializers. Highest-value pre-execution fixes: correct the FPU mask spec; expand BITS-02 ownership to all in-scope truncation sites; make the TU manifest exhaustive with explicit Archive map instantiation.

---

## Opus Review (math/spec, source-verified)

**Summary** — Well-scoped, unusually disciplined. Harness-as-worklist is the right architecture; landing fixes in shared source is sound. The serialization conclusion is correct and **verified against source**: on LLP64 the Archive `std::map` hazard does not silently widen — it **fails overload resolution** (no 8-byte `get`/`put` overload), so the harness catches it as a hard error, and pinning to `uint32_t` is the correct wire-stable fix. The serious weaknesses are under-specification of the two hardest items: the `_control87` port subtly changes which bits `setControlWord` writes on the **32-bit** build, and the naked-SSE → `_mm_*` conversions contain non-obvious lane semantics a faithful rewrite must preserve.

**Concerns**
- **HIGH — `_control87` port silently changes 32-bit precision-control behavior.** Every mutator funnels through `setControlWord(status)` where `status` includes the precision bits (`PRECISION_24` set in `install()`:56, `setPrecision`:164). Masking to `_MCW_RC|_MCW_EM` drops P_24 on **both** bitnesses — a real change to the shipped 32-bit client, on the door-snap causal path (D-04/VERIFY-01). Make it a *named decision*, not an accident of the mask argument.
- **HIGH — `_control87` vs `_controlfp` bit-layout mismatch is glossed.** Existing constants (`ROUND_MASK=0x0C00`, `PRECISION_MASK=0x0300`, `EXCEPTION_*=0x003F`) are **raw x87 hardware bits**; `_control87` takes MSVC abstract `_MCW_*` constants with **different bit positions**. Handing the hardware-layout `status` WORD to `_control87(status, _MCW_RC|_MCW_EM)` sets the wrong state. Must translate hardware↔`_RC_*`/`_EM_*`, or rewrite to traffic exclusively in `_MCW_*`. The acceptance criteria (`grep -c _control87 >= 2`, 0 C4235) would pass a *semantically wrong* port. **The single biggest correctness gap.**
- **HIGH — naked-SSE lane/w-lane semantics under-specified and not regression-detectable.** Verified traps: `skinPositionNormal_*` loads normal w=1.0 but sums only 3 lanes for the normal vs 4 for position (asymmetry must be reproduced or it folds the translate column into normals); `Transform::sse_xf_matrix_3x4` uses `shufps ...,0x15` = broadcast **lane 1** (not lane 0) + zero lane 3 (a naive `_mm_set1_ps(left[3])` is wrong); manual 16-byte-aligned stack scratch + `movups` to `out`. None caught by boot smoke — no numeric oracle.
- **HIGH — `movaps` vs `movups` alignment hazard.** `movaps` against `transform` rows requires `Transform` 16-byte aligned; on x64 default stack/`new` alignment differs and `Transform` is an embedded plain member. `_mm_load_ps` on unaligned `Transform` **faults at runtime** — invisible to the compile-only harness and possibly to 32-bit boot. Decide `_mm_loadu_ps` vs `_mm_load_ps` + assert; flag as a Phase-33 *runtime* validation item.
- **MEDIUM** — global non-reentrant `sseVariable[5][4]` staging array should be retired (use locals), not faithfully preserved; StaticShader sort-key widening propagates into `ShaderPrimitiveSorter.cpp:496`'s `entry` field + comparator (different file, not in plan 04) — pre-decide hash-to-int; the Archive map threat narrative is factually wrong (compile error, not silent widen) though the fix is right; `Direct3d9.cpp:137-203` `(DWORD)(uintptr_t)` casts are latent x64 truncations the harness is **blind to** because explicit casts suppress C4311.
- **LOW** — `_control87` per-thread scope unchanged (note it); `__rdtsc` ordering differs (profiling-only); no standing guardrail once the throwaway harness is deleted (Phase 31→33 coverage hole).

**Risk: MEDIUM-HIGH.** Strategy is low-risk and verified; the two hardest items (FPU control-word port, naked-SSE conversions) are exactly the ones under-specified and exactly the ones the validation surface (compile-clean + character-select) is weakest at catching. Adding a numeric SSE/FPU self-test oracle and resolving the `_control87` constant-translation + precision-mask question would drop this to LOW.

---

## Sonnet Review (lateral/blind-spot)

**Summary** — Well-structured, methodical. Honest about its own scope limits. Three concerns rise to HIGH: `_control87` semantics incompatible with the raw x87 bit-pattern constants; SseMath `movaps` needs 16-byte alignment from the Transform object (unverified) and the intrinsic translation must resolve it explicitly; and the `shaderTemplateSortKey` fix is under-scoped because the `Entry` struct field is `int` — widening the return without widening the field just moves the truncation. Remaining concerns MEDIUM/LOW.

**Concerns**
- **HIGH — `_control87(0,0)` incompatible with raw x87 constants.** `_control87` returns MSVC `_MCW_*` encoding (`_MCW_RC=0x0300`, `_MCW_PC=0x0003`, `_MCW_EM=0x003F`); masking against x87-layout `ROUND_MASK=0x0C00` etc. corrupts `status`. Switch fully to `_MCW_*`, or keep x87 on 32-bit + bit-remap layer on x64. Guarding `_MCW_PC` out on x64 is insufficient — the `ROUND_MASK` vs `_MCW_RC` position mismatch remains. (Independent confirmation of Opus HIGH.)
- **HIGH — `movaps`/`_mm_load_ps` alignment on the Transform pointer.** Transform has no `alignas(16)`; 32-bit MSVC heap = 8-byte. Use `_mm_loadu_ps` for the Transform rows; `_mm_load_ps` only for the aligned `sseVariable`. (Independent confirmation of Opus HIGH.)
- **HIGH — Sort-key widening under-scoped: `ShaderPrimitiveSorter::Entry.shaderTemplateSortKey` is `int`.** Widening the return type without the struct field moves the truncation to the struct assignment; `ShaderPrimitiveSorter.cpp` is not in plan 04's `files_modified`. (Independent confirmation of Opus + Cursor.)
- **MEDIUM** — Archive map `static_cast<uint32_t>(source.size())` truncates above UINT32_MAX (unguarded) and the vector `signed int length` path fires C4244 in the harness (acknowledge as excluded per D-07 or it confuses the executor); `update()` compares MXCSR-via-`_control87` against an x87-built `status` → fails on x64; ProfilerTimer `__rdtsc()` returns `unsigned __int64` while signature is `__int64` — keep `__int64` + `static_cast`; `canDoSseMath` try/catch is dead on x64; MemoryManager `%zx` has inconsistent old-CRT support (prefer `%p`/`%Ix`).
- **LOW** — `compile-all.ps1` relative-include extraction is fragile (unresolved headers → TU skips → false-clean; acceptance only tests the failure case); `TRY_FOR_SSE`=`WIN32` is active on x64 too (executor must rewrite `sse_xf_matrix_3x4` unconditionally); plan 06 `files_modified` only lists the residual .md but the escape-valve may edit source; DebugHelp 32-bit `__asm` path won't compile under x64 harness (fine).

**Suggestions** — two-path FPU model (`#if !defined(_M_X64)` keeps exact x87 path, x64 uses `_MCW_*` exclusively); `_mm_loadu_ps` for Transform rows; add `ShaderPrimitiveSorter.cpp` to plan 04 OR commit to pointer-hash (no struct cascade); note vector `signed int` C4244 exclusion; add a "one non-defect TU compiles" acceptance to plan 01 (verifies include-path extraction); ProfilerTimer keep `__int64`; ensure `sseVariable` stores aren't reordered vs C++ reads (use a local stack buffer); note plan 06 may edit source as escape-valve.

**Risk: MEDIUM.** Strategy sound; the three HIGH concerns survive the acceptance criteria as written and none break the 32-bit gate, so they slip through Phase 31 undetected (FPU mismatch → broken `update()` on x64 not caught until Phase 33; alignment → possible SIGSEGV on x64; sort-key → residual C4244). MEDIUM not HIGH because the 32-bit gate is robust and the residual mechanism exists.

---

## Consensus Summary

Four independent reviewers — two external CLIs (Codex gpt-5.5, Cursor) and two fresh in-harness
models (Opus, Sonnet) — **converge sharply**. All four endorse the phase *strategy* (compile-only
scratch harness as worklist generator, shared-source fixes, clean wave DAG, honest residual
hand-off) and all four locate the risk in *execution depth on the two hardest items*, not in scope.
Risk ratings cluster tightly: **MEDIUM-HIGH ×3 (Codex, Cursor, Opus), MEDIUM ×1 (Sonnet)**.

### Agreed Strengths (2+ reviewers)
- Compiler-driven worklist / scratch harness is the right primary tool (all four).
- Crisp phase boundary — no x64 platform-add, no D3DX, editors out (all four).
- Fixes land in shared source so the 32-bit build inherits them; `/FORCE` link-grep kept; `_USE_32BIT_TIME_T=1` + "`long` stays 32-bit on LLP64" prevent false BITS-03 positives (Codex, Cursor, Opus, Sonnet).
- `static_assert(sizeof==N)` baselined from the *live compiler*, not RESEARCH literals (Opus, Sonnet, Cursor).
- DebugHelp as the single justified `#if _M_X64` fork; clean dependency ordering (Opus, Sonnet, Cursor).

### Agreed Concerns (2+ reviewers — priority order)
1. **[HIGH — all four] The `_control87` FPU port is under-specified and likely incorrect as written.** Two distinct defects: **(a) bit-layout mismatch** — the existing constants are raw x87 hardware bits, `_control87` uses MSVC `_MCW_*` encoding with different positions → masking/comparison corrupts `status` on x64 and breaks `update()` (Opus, Sonnet, Codex); **(b) precision-bit drop on 32-bit** — masking out `_MCW_PC` removes the `PRECISION_24` force on the *shipped 32-bit build too*, directly on the door-snap causal theory (Cursor, Opus). The current acceptance criteria (`grep -c _control87 >= 2`, 0 C4235) pass a semantically wrong port. **This is the top finding.**
2. **[HIGH — all four] naked-SSE → `_mm_*` needs a numeric oracle, not just boot smoke**, AND **`movaps`/`_mm_load_ps` 16-byte alignment on `Transform` is a runtime x64 fault the compile-only harness is structurally blind to** (Codex, Opus, Sonnet, Cursor). Specific verified lane traps: `0x15` shuffle broadcasts lane 1; normal sums 3 lanes vs position 4; manual stack alignment + `movups` to `out`. Use `_mm_loadu_ps` for Transform rows; add a `_DEBUG` equivalence self-test vs the scalar reference.
3. **[HIGH — all four] Sort-key fix under-scoped / ABI blast radius.** `getSortKey()`/`getShaderTemplateSortKey()` returns `int`; widening cascades into `ShaderPrimitiveSorter::Entry.shaderTemplateSortKey` (an `int` field + comparator, *not* in plan 04's `files_modified`) and, for `getSortKey()`, a `virtual int` in shared headers consumed by all four plugins. **Pre-decide** the contract: stable hash-to-`int` (lower blast radius, keeps the change in owned files) vs widen-everywhere. Cursor additionally flags 3 live D3D9 sort-key/truncation sites missing from plan 04.
4. **[HIGH/MEDIUM — all four] Scratch-harness false confidence.** Compile-only `cl /c` scraping `.vcxproj` include dirs can miss property sheets/forced-includes/PCH/generated-headers (Codex); a "first cut" manifest can green-gate latent defects in unlisted TUs (Cursor); relative include paths may silently fail → TU skips → false-clean (Sonnet); and **explicit C-style casts suppress C4311**, so known `(DWORD)(uintptr_t)` sites (`Direct3d9.cpp:137-203`) are invisible to the harness (Opus). The harness is authoritative only for *implicit* truncations.
5. **[MEDIUM — all four] BITS-03 / Archive map.** Codex + Cursor: BITS-03 is too narrow and the map fix is unexercised (zero `std::map` instantiations under `src/engine` → boot smoke won't prove it; add an explicit instantiation TU). Opus + Sonnet *refine/correct* this: the map hazard is a **compile error** (overload-resolution failure on x64), not a silent wire-format widen — good news, but the threat-model narrative should be corrected so Phase 33 doesn't mis-prioritize.
6. **[MEDIUM] DebugHelp x64 is compile-clean ≠ function-clean** (Rbp/Rip stack-walk + `DWORD(Offset)` truncation) — document as a Phase-33 runtime item (Codex, Cursor, Opus).
7. **[MEDIUM/LOW] Warning set narrower than "x64-clean"** (C4267 `size_t`→`int` missing; C4244 noise vs real pointer-width) (Cursor, Codex).
8. **[LOW] gitignored throwaway harness** vs later plans depending on it, and **no standing guardrail** once it's deleted (Phase 31→33 coverage hole) (Codex, Opus, Cursor).

### Divergent Views
- **Archive map severity/framing:** Codex + Cursor read it as a HIGH/under-proven serialization gap; Opus + Sonnet *verified against source* that it can't silently ship (it fails to compile on x64) and downgraded it — the fix is right, the narrative is wrong. The source-verified view should win.
- **Overall risk:** Sonnet alone says MEDIUM (vs MEDIUM-HIGH) — but on the same three HIGH findings; the difference is weighting the robust 32-bit gate, not a disagreement on substance.
- **`_control87` vs `_controlfp` choice:** Codex suggests possibly keeping the exact x87 asm path on `_M_IX86` and a CRT API only on `_M_X64`; Opus/Sonnet suggest a full `_MCW_*` rewrite with a 32-bit guard. Both resolve the same root issue — pick one explicitly.

### Recommended pre-execution actions (highest leverage first)
1. **Rewrite the FPU port spec** around `_MCW_*` constants (or an explicit x87↔`_MCW_*` translation), and make the 32-bit precision-bit retention a *named decision* — not a side effect of the mask. Add a 32-bit before/after control-word round-trip check.
2. **Decide `_mm_loadu_ps` vs `_mm_load_ps`** per pointer + add a `_DEBUG` numeric equivalence self-test for SseMath + `Transform::sse_xf_matrix_3x4`; flag the alignment item as a Phase-33 runtime check.
3. **Pre-decide the sort-key contract** (hash-to-int recommended) and either add `ShaderPrimitiveSorter.cpp` + the 3 D3D9 sites to plan 04's `files_modified` or commit to hashing.
4. **Make the TU manifest exhaustive** (derive from the MSBuild dependency graph), add a "one non-defect TU compiles" acceptance, and add an explicit `Archive::get/put(std::map<int,int>)` instantiation TU. Note that explicit casts evade `/we4311` and audit those sites by grep.
5. **Correct the Archive map threat narrative** (compile error, not silent widen) and add C4267 to the harness (or document its deliberate omission as a Phase-33 carry-forward).

---

To incorporate this feedback into planning:
```
/gsd:plan-phase 31 --reviews
```
