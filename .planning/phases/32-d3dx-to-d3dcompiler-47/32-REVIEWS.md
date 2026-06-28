---
phase: 32
round: 2
reviewers: [cursor, sonnet, codex, opus]
reviewed_at: 2026-06-16
plans_reviewed: [32-01-PLAN.md, 32-02-PLAN.md, 32-03-PLAN.md, 32-04-PLAN.md, 32-05-PLAN.md]
review_method: cross-AI consult crew (Cursor=HLSL render-correctness / Sonnet=lateral / Codex=repo-trace / Opus=gate-logic), non-overlapping angles
note: Round 2 — reviews the REVISED plan draft (529 ins / 337 del after round-1's 32-REVIEWS, backed up as research/32-REVIEWS-round1.md.bak). All four reviewers completed. Orchestrator independently verified the load-bearing factual claims against the repo before writing Consensus (see § Adjudication).
---

# Cross-AI Plan Review — Phase 32 (D3DX → d3dcompiler_47), Round 2

Four independent reviewers, each handed neutral evidence on a **different** angle so they cross-check
rather than echo. Convergence-from-divergence is the signal. The orchestrator independently grepped the
repo to verify the load-bearing claims before writing the Consensus.

**Verdict at a glance:** all four converge on **MEDIUM** overall risk (Cursor rates render-correctness
specifically **HIGH**). The revised plans correctly fix the round-1 landmines (Rule D disabled for D3D9,
override macro leak, honest partial-SHADER-01 framing, tri-state gate). Residual risk concentrates in
**validation/proof adequacy**, not architecture: the gate + smoke certify a *sample*, not the corpus;
the strongest single finding (Cursor, repo-verified) is that **no probe checks the `v#` vertex-input
register map** — only `c#` constants.

---

## Cursor Review — HLSL render-correctness (detailed code reader)

**Risk: HIGH (render-correctness) / MEDIUM (plan-process).**

### Summary
Round-2 plans are materially stronger on the D3D9-specific rewrite deltas (Rule D off, Rule E
conditional, override macro fix, no `PACK_MATRIX_ROW_MAJOR`, partial SHADER-01 honesty). The
**constant-register (`c#`) story is mostly sound** if Rule D stays disabled and the Task-2 layout probe
runs. The **`v#` input-register story is still under-specified**: verbatim Rules B/C strip the gapped
struct bindings SWG authored to match `Direct3d9_VertexShaderVertexRegisters.h`, while validation assumes
semantic-only binding suffices without disassembly proof. The WIP branch stalled exactly here.

### Strengths
- **Rule D disable** (`32-02-PLAN.md:69-76,161`) — correctly identifies WIP root cause; matches code at `Direct3d11_HlslRewrite.cpp:746-1003`.
- **Override macro fix** (`32-02-PLAN.md:217-223`) — addresses the real post-preprocessor leak (`tfcl.vsh:22-23`); gl11 already omits these registers (`Direct3d11_VertexShaderData.cpp:786`).
- **`PACK_MATRIX_ROW_MAJOR` exclusion** (`32-02-PLAN.md:205-208`) — gl11 sets it; D3D9 uses explicit transposed uploads — correct divergence.
- **Wave-0 gate tri-state** (`32-01-PLAN.md:55-59`) + **honest partial SHADER-01** (`32-05-PLAN.md:42-49`).
- **Fix-A / asm-unguarded correction** (`32-04-PLAN.md:37-44`) — matches code: SEH wraps only HLSL at `:477`; asm at `:567` is bare `D3DXAssembleShader`.

### Concerns
| ID | Sev | Issue | Location |
|---|---|---|---|
| C1 | **HIGH** | No bytecode/`v#` input-register parity check vs D3DX; Task-2 layout probe covers `c#` source/disasm only. Rules B/C strip struct-member `: register(v0/v3/v5/v7)`; if `D3DCompile` packs sequentially instead of honoring the gapped VSVR layout, bytecode disagrees with the `D3DVERTEXELEMENT9` decl while still linking — classic "compiles, renders garbage." | `32-02-PLAN.md:230-237`; `tfcl.vsh:98-100` |
| C2 | **HIGH** | D3D9 `IncludeHandler` caches **raw** bytes; a 2nd `#include` of the same `.inc` in one compile can return **unrewritten** text unless the plan specifies rewrite-on-cache-insert. gl11 rewrites on every `Open`; D3D9 does not. | `Direct3d9_VertexShaderData.cpp:153-170`; `32-02-PLAN.md:209-211` |
| C3 | **HIGH** | Tatooine bump/dot3 smoke insufficient for the 190 //hlsl TRE shaders + multi-UV (`tfcl_4uv` v7–v10) + env (`envmask_specmap_c`) families; scene is still mostly //asm-on-D3DX during 32-02. | `32-VALIDATION.md:66`; `32-02-PLAN.md:262-295` |
| C4 | MEDIUM | Struct-member `: register(v0/v3/v5)` in overrides still stripped by B/C; plan text implies POSITION/NORMAL bindings "may remain" — they won't in rewritten source. | `32-02-PLAN.md:252`; `tfcl.vsh:98-100` |
| C5 | MEDIUM | `tfcl_4uv` needs 4× `register(vN)` removed from macro; acceptance greps only `tfcl.vsh`. | `tfcl_4uv.vsh:16-20` |
| C6 | MEDIUM | Rule E conditional probe lacks an explicit pass/fail → enable/disable decision table. | `32-02-PLAN.md:166-168` |
| C7 | MEDIUM | `const bool : register(b0-b7)` compile-acceptance at `vs_2_0` not probed. | `vertex_shader_constants.inc:104-111` |
| C8 | LOW | 32-02 `wave:0` runs parallel with the asm gate — fine for asm, but an HLSL render failure after 32-02 could be misattributed to the Wave-1 asm port. | `32-02-PLAN.md:5` |

### Suggestions
1. **Extend the layout probe**: add `ID3DXShaderReflection`/disasm compare of the **input signature (`v#` slots)** and **constant register usage** vs the Fix-A/D3DX baseline for `tfcl` + one non-override //hlsl `.vsh`.
2. **Explicit include-cache contract**: "store rewritten bytes in `Include::m_data` before cache insert; OR bypass the cache for the HLSL compile path."
3. **Expand override acceptance to all 9 files**, explicitly `tfcl_4uv.vsh`/`tfcl_2uv.vsh` (per-file `grep -c register(v` in the `DECLARE_textureCoordinateSets` block = 0).
4. **Minimal shader checklist** in `32-VALIDATION.md`: `envmask_specmap_c`, `tfcl_4uv`, one TRE //hlsl, one bool-light shader, char-select UI shader if distinct.
5. **Rule E decision**: probe compile with Rule E **off** first; enable strip only if `#pragma def` fails AND runtime c95 upload confirmed in disasm.
6. **Document the contested `v#` hypothesis** in the 32-02 SUMMARY: if Tatooine fails after Rule D off + flag audit, the next lever is a D3D9-specific Rule C narrowing that preserves first-colon `register(vN)` on struct members — not verbatim gl11 strip.

---

## Sonnet Review — lateral / out-of-the-box

**Risk: MEDIUM.**

### Summary
Well-researched, defensively-structured set that has clearly been through a prior cycle. The HLSL half
(32-02) and the asm gate (32-01) are strong. Weakness is at the boundary between what it *claims* to
prove and what it *actually* proves, in ambiguity inside "PASS," in a class of shaders that could bypass
the full rewrite, and in whether Phase 32 unambiguously closes the gap Phase 33 must open on.

### Strengths
- Three-corpus reconciliation (286 full / 16 extracted / 3 baseline) closes an execution-wide ambiguity.
- Rule D disabled — the single most important correctness call, made explicitly with a code-comment mandate.
- Bytecode size+hash diff (32-01 Task 2) catches the false-PASS where D3DAssemble accepts the dialect but emits divergent bytecode.
- `PACK_MATRIX_ROW_MAJOR` exclusion + the asm-unguarded-vs-Fix-A-HLSL-only correction propagated correctly.
- `conditional:` frontmatter note is operationally honest; PARTIAL SHADER-01 framing ("does NOT unblock Phase 33") stated three times.

### Concerns
- **H1 [HIGH] — bytecode hash hard-gate is a false-FAIL trap.** 32-01 Task 3 requires size+hash MATCH vs `D3DXAssembleShader`. Two different-era assemblers can emit semantically-identical but byte-distinct bytecode (padding, instruction order, header). The gate would force `FAIL-WITH-FOLLOWUP` on a *functionally correct* shader, needlessly parking the asm port on the conservative branch. RESEARCH Pitfall 2 only addresses the false-PASS direction. **Fix:** if hash diverges but `CreateVertexShader` is S_OK, run `D3DDisassemble` on both blobs and accept PASS if token-identical (or PASS-WITH-NOTE).
- **H2 [HIGH] — XOR branch is intent-only, no enforcement.** Both 32-03/32-04 carry `depends_on:["32-01","32-02"]`, `wave:1`, and `conditional:` is non-functional. A scheduler seeing two wave-1 plans with satisfied deps could launch both; they edit the same file (`:567`) in opposite directions. **Fix:** relabel one branch or move selection into 32-01 Task-3's resume signal (type PASS→invoke 32-03 / FAIL→32-04 as a direct plan invocation).
- **M1 [MEDIUM] — the non-compile D3DX removal follow-up (D-05) has no phase number.** Applies on BOTH branches; 32-05 mentions it in prose only. No artifact like `32-NONCOMPILE-DX-FOLLOWUP.md`. This is the exact deferred-debt pattern that produced Phase 27's revert: Phase 33 opens believing prerequisites are met, the link fails on still-linked D3DX symbols. **Fix:** require a concrete planning artifact / ROADMAP entry as a Phase-32 deliverable.
- **M2 [MEDIUM] — override files un-audited for OTHER post-expansion register macros** beyond `DECLARE_textureCoordinateSets`. **Fix:** one-time grep of all 9 overrides for `: register(v` outside that macro body.
- **M3 [MEDIUM] — vs_1_1→vs_2_0 promotion underspecified.** 32-02 offers FATAL-or-promote without guidance; vs_1_1 vs vs_2_0 differ in codegen, and no test hardware exercises the <2.0 path. The census hasn't characterized which //hlsl shaders use vs_1_1. **Fix:** census flags vs_1_1-divergent constructs; SUMMARY records the chosen resolution.
- **M4 [MEDIUM] — WIP branch disposition unspecified** ("cherry-pick or re-derive"). On a LIVE upstream-integration `master`, leaving `d3d9-fixb-d3dcompile-wip` diverged creates zombie tech-debt. **Fix:** one sentence in 32-02 output recording retire-or-keep.
- **M5 [MEDIUM] — 32-01 census reads the 9 overrides while 32-02 (parallel, `wave:0`) may edit them** → transient mid-edit census artifact. **Fix:** clarify the overrides are enumerated for coverage, not classified-by-content.
- **L1 [LOW]** — searchPath-active check is prose, not an acceptance criterion; a missing override searchPath entry silently tests the wrong corpus. Elevate to `grep "override" stage/client_d.cfg`.
- **L2 [LOW]** — `D3DAssemble` `GetProcAddress`/`LoadLibraryA` caching policy unspecified → per-shader reload/refcount leak. Specify static-local cache.
- **L3 [LOW]** — gl11 not build-clean-checked until 32-05; a shared-header touch could ABI-cascade. Add an explicit `Direct3d11`-target build+grep to 32-02.

### Risk: MEDIUM
Most round-1 landmines addressed with explicit acceptance criteria. Primary residual is operational (H2 conflicting-edit hazard; H1 false-FAIL). Technical risk of the HLSL port itself is LOW-MEDIUM given the WIP scaffold + Rule D/E guards.

---

## Codex Review — repo-trace / call-graph (claim verification)

**Risk: MEDIUM.**

### Claim trace (against the live repo)
| Claim | Verdict |
|---|---|
| D3D9 depends on flat `cN` registers → Rule D wrap dangerous (`VSCR_*`=0/16/60; `SetVertexShaderConstantF` at `StateCache.cpp:526`; matrices at `Direct3d9.cpp:4038`; light data at `LightManager.cpp:723`) | **VERIFIED** |
| gl11 Rule D wraps `.inc` globals into `cbuffer ... register(b0)`/`packoffset` (`HlslRewrite.cpp:747,762-766`; `applyToIncludeBuffer:1147`) | **VERIFIED** |
| Revised 32-02 protects D3D9 from Rule D (`:27,69-76,161-165,232-236`) | **VERIFIED** |
| Rewrite runs before macro preprocessing → macro `: register(vN)` bypasses B/C; 32-02 fixes all 9 overrides (`:217`) | **VERIFIED** |
| WIP branch "compiling but render-incomplete" — diff adds `Direct3d9_HlslRewrite`, swaps HLSL→`D3DCompile`, omits generated texcoord `register(vN)`, leaves asm on `D3DXAssembleShader`; **no concrete render-incomplete diagnosis in the code** | **PARTLY VERIFIED / cause UNVERIFIABLE** |
| `D3DAssemble` not in public `d3dcompiler.h` (only `D3DCompile` at `:210`) | **VERIFIED** |
| `D3DAssemble` export/import-lib symbol details (32-RESEARCH `dumpbin` at `:606`) | **UNVERIFIABLE IN SESSION** (dumpbin invocation blocked by sandbox policy) |

### Strengths
- Wave-0 gate is sound: bytecode size/hash comparison, not just `CreateVertexShader` S_OK.
- 32-02 directly addresses the top D3D9 hazard (Rule D off + flat `register(cN)` audit).
- Preprocessor ordering handled by editing both the generated macro path and the 9 overrides.
- Branching clear; 32-05 join correctly documented.
- SHADER-01 no longer overstated (32-bit-only, partial for x64).

### Concerns
- **HIGH** — `D3DAssemble` export/signature proof is in **prose, not a reproducible artifact**. Wave 0 should save raw `dumpbin /exports` + import-lib evidence (the reviewer could not re-run dumpbin in-sandbox).
- **MEDIUM** — WIP-branch evidence is **over-weighted**: it proves the HLSL path was ported and asm stayed on D3DX, not what "render-incomplete" concretely means. Plan should avoid implying WIP diagnosed it.
- **MEDIUM** — the constant-layout audit could miss shader families outside `c_simple`/`tfcl` (uploads touch matrices, light structs, material, texture factors, c95, dot3 offsets).
- **LOW** — manual visual smoke is acceptable here, but the audit artifact should include shader names + disasm snippets, not just "render clean."

### Suggestions
- 32-01: require `32-CENSUS.md` to embed raw `dumpbin` snippets for `D3DAssemble` + exact DLL/import-lib paths.
- 32-02 Task 2(F): broaden the layout probe to ≥1 shader using `c0`, `c16`, `c44/47`, `c60`, `c95`.
- Treat WIP as a source patch only ("ported mechanically; render status not root-caused").
- Add one acceptance line: no `cbuffer SwgVertexConstants`, no `packoffset`, no `D3DCOMPILE_PACK_MATRIX_ROW_MAJOR` anywhere in the D3D9 compile path.

### Risk: MEDIUM
Right architecture, catches the main call-graph hazards. Residual: shader correctness is data-dependent, the asm assembler is undocumented, and validation is 32-bit while SHADER-01's final bar is x64.

---

## Opus Review — gate-logic & specification soundness

**Risk: MEDIUM.**

### Summary
Structurally strong and unusually self-aware. The conditional Wave-1 XOR (32-03 PASS / 32-04
FAIL-WITH-FOLLOWUP) is well-defined, the gate verdict is single-sourced (`32-CENSUS.md ## GATE VERDICT`,
echoed in `32-01-SUMMARY.md`), and Fix-A retirement is correctly gated per-branch against a
source-verified fact. Three gate-logic defects remain: a false-positive risk in the PASS gate's
**coverage basis**, branch selection that depends on a **manual step the framework doesn't enforce**, and
a **wave-numbering inconsistency**.

### Strengths
- Gate output single-sourced and deterministically read (exact `## GATE VERDICT` heading, fixed token grammar, both branches self-abort on mismatch). Textbook fail-safe consumption.
- Branch is genuine XOR; 32-05 `depends_on:["32-02"]` only — not both wave-1 plans — with a `join_note` explaining why an AND-join on a skipped plan would deadlock. Avoids the classic conditional-wave stall.
- Gate is conjunctive (non-null bytecode AND size+hash match AND `CreateVertexShader` S_OK); the bytecode-diff requirement closes the most dangerous false-positive.
- **Fix-A retirement is correct and source-grounded** — verified against `Direct3d9_VertexShaderData.cpp`: `compileVertexShaderFpGuarded` (`:93-104`, `__try/__except`) wraps `D3DXCompileShader` only; `:567` `D3DXAssembleShader` is unguarded. No path removes a guard that prevents the FP crash.
- No overclaiming — Phase 32 explicitly satisfies SHADER-01 *partially* (compile-path, Win32-only) and states it does not unblock the Phase-33 x64 link.

### Concerns
- **[HIGH] PASS gate's coverage basis cannot prove "all //asm shaders assemble" — only "the probed subset does."** 32-01 probes 3 baseline + a Task-1 flag-list enumerated over the **16 extracted** `.vsh` (11 //asm), not the **286-shader/96-//asm full corpus**, and the flag-list is generated by the same agent eyeballing "uncovered constructs." A //asm shader in the unextracted 85 using a rejected construct would PASS the gate and only fail at 32-03's render-smoke — **and there is no defined behavior for "gate said PASS, 32-03 render-smoke then fails."** The binary gate has no representation for partial-PASS.
- **[HIGH] Branch selection relies on a manual action the framework does not enforce.** `conditional:` is documented as non-functional. Two plans declare `files_modified` on the SAME file with opposite intent; if both run, the second clobbers the first silently. Gate *logic* is sound; *enforcement* is convention.
- **[MEDIUM] Wave-numbering inconsistent.** Prompt/prose say "Wave 0/1/2/3" (4 waves); frontmatter is `wave:0,0,1,1,2` (3 waves) and ROADMAP says 3. The dependency graph is correct (no ordering bug) but the human-facing labels contradict.
- **[MEDIUM] 32-02 commits engine changes before the gate verdict is known** (both `wave:0`, parallel with the probe). Sound because 32-02 never touches the asm path — but 32-02 and 32-03 both edit `Direct3d9_VertexShaderData.cpp`, so the PASS branch has a serialized cross-wave edit chain to one file (32-03 must rebase onto 32-02's version).
- **[LOW] "No VS silently nulled" (SC#1/D-06) is asserted structurally but only spot-checked** by one Tatooine scene; a shader bound only in an off-path scene could compile-FATAL/null undetected. Add a "validated on reference scene; corpus-wide out of scope" caveat.
- **[LOW] The size+hash gate may over-trigger** (benign cross-assembler encoder variance → false-negative FAIL → conservative 32-04). Safe direction, but treat hash-mismatch as "must render-verify," not definitive semantic drift.

### Suggestions
- Close the partial-PASS hole: probe one representative per distinct construct-signature across all 96 //asm (the plan already has the `tre_reader.py` path), OR rename PASS→**PASS-ON-SAMPLE** and define the recovery path: "32-03 render-smoke failure on a non-probed shader → roll back 32-03, re-route to 32-04, append to 32-ASM-FOLLOWUP."
- Add a double-execution guard: 32-03/32-04 each assert their own branch token AND `test ! -f <other>-SUMMARY.md` as their first action.
- Reconcile wave labels to one scheme (0/1/2) everywhere; fix the "Wave 3" prose.
- Soften hash-diff from hard FAIL to flag-for-render-check; keep `CreateVertexShader` + minimal render as the authoritative semantic gate.
- Add the corpus-coverage caveat to 32-05's SHADER-01 partial-satisfaction statement.

### Risk: MEDIUM
Load-bearing properties for a conditional-wave plan hold (objective gate, true XOR, single-sourced verdict, source-verified Fix-A logic with no guard-removal path). Risk concentrates in the gate's **sample-not-corpus coverage basis** (no fallback from PASS-branch render failure) and **convention-not-control branch selection** against two same-file opposite-intent plans. The explicit refusal to overclaim SHADER-01 materially lowers spec-soundness risk.

---

## Consensus Summary

### Adjudication (orchestrator repo-verification of load-bearing claims)
Verified directly against the working tree before synthesizing:
- **Cursor C1 (`v#` gap) — CONFIRMED REAL.** `Direct3d9_VertexShaderVertexRegisters.h` defines the gapped layout (`VSVR_position=0, normal=3, color0=5, textureCoordinateSet0=7`) with a triple-WARNING that these values are *encoded into the shader data files*. `tfcl.vsh:98-100` carries struct-member `: register(v0/v3/v5)` + `:23 register(v7)`; Rules B/C strip them; 32-02's layout probe (`:181`) proves only flat `register(cN)` — **no `v#` input-signature check exists.** This is the standout under-covered risk.
- **Hash hard-gate (Sonnet H1 / Opus) — CONFIRMED.** 32-01 `:16,193-194` mandate "size+hash mismatch is a FAIL even if both succeed." Two reviewers independently flag the false-FAIL direction — high-signal convergence.
- **`conditional:` not enforced (Opus / Sonnet H2) — CONFIRMED & self-documented.** 32-03 `:7` itself states `conditional:` is unsupported and relies on manual orchestrator action; no double-exec guard exists.
- **Rule D disabled — CONFIRMED correct** (32-02 `:161,181`, acceptance grep for the disable + no live `cbuffer` emission).
- **Fix-A wraps HLSL only, asm unguarded — CONFIRMED** (`:477` guarded / `:567` bare) — independently verified by Opus and the orchestrator.
- **Wave-numbering — CONFIRMED inconsistent** (frontmatter `0,0,1,1,2` vs prose "Wave 0–3"). Cosmetic; dependency graph is correct.

### Agreed Strengths (2+ reviewers)
- **Rule D disabled for D3D9** — all four; the single most important correctness call.
- **Wave-0 gate beyond `CreateVertexShader`** (bytecode size+hash diff) — Cursor, Sonnet, Codex, Opus.
- **Honest partial-SHADER-01 framing, no x64 overclaim** — all four.
- **Preprocessor-ordering / override-macro leak addressed** — Cursor, Codex, Sonnet.
- **XOR branch + 32-05 join correctly modeled; Fix-A correction factually accurate (`:477`/`:567`)** — Opus, Cursor, Codex, Sonnet.

### Agreed Concerns (2+ reviewers — highest priority)
1. **[HIGH] Validation/proof certifies a SAMPLE, not the corpus.** Opus (partial-PASS hole + no PASS-branch render-failure fallback), Cursor C3 (Tatooine can't cover 190 //hlsl + multi-UV/env), Codex (layout audit misses families), Sonnet M3 (vs_1_1 uncharacterized). **Action:** probe one representative per distinct construct-signature across the full corpus (use `tre_reader.py`); rename PASS→PASS-ON-SAMPLE; **define the recovery path when a non-probed shader fails render after the branch is chosen.**
2. **[HIGH] Bytecode hash hard-gate → false-FAIL.** Sonnet H1 + Opus (+ Cursor implicit). **Action:** on hash-mismatch with `CreateVertexShader` S_OK, fall back to `D3DDisassemble` token-compare; reserve auto-FAIL for assemble-error HRESULT or `CreateVertexShader` rejection.
3. **[HIGH] XOR branch enforced by convention, not control.** Opus + Sonnet H2 — two plans edit the same file with opposite intent; nothing prevents both running. **Action:** add a first-action double-execution guard (`assert own branch token` + `test ! -f <other>-SUMMARY.md`), or move selection into 32-01 Task-3's resume signal.
4. **[MEDIUM] Wave-numbering inconsistency** (Opus, Sonnet). **Action:** reconcile prose to frontmatter (3 waves: 0/1/2).

### Divergent / unique high-value findings (single reviewer — verify, don't discount)
- **Cursor C1 — no `v#` input-register parity probe** (orchestrator-CONFIRMED). The highest-value unique catch; add a disasm input-signature compare vs the D3DX baseline.
- **Cursor C2 — D3D9 include-cache returns unrewritten `.inc` on 2nd include.** Concrete impl trap; specify rewrite-on-cache-insert.
- **Codex — `D3DAssemble` export proof is prose, not a reproducible artifact.** Wave 0 should persist raw `dumpbin /exports` + import-lib paths.
- **Sonnet M1 — non-compile D3DX removal (D-05) has no phase number.** Deferred-debt pattern that caused Phase 27's revert; require a tracked artifact.
- **Codex/Sonnet — WIP branch over-weighted as root-cause evidence;** disposition unspecified. Treat as scaffold only; record retire-or-keep.

### Recommended pre-execution edits (ranked)
1. Add a **`v#` input-signature disasm probe** to 32-02's layout check (Cursor C1) — repo-verified gap, matches where WIP stalled.
2. **Soften the hash gate** to a disasm-token fallback (Consensus #2).
3. **Define the PASS-branch render-failure recovery path** + scope the gate as PASS-ON-SAMPLE (Consensus #1).
4. Add a **double-execution guard** to 32-03/32-04 (Consensus #3).
5. Specify the **include-cache rewrite contract** (Cursor C2) and persist **raw dumpbin** D3DAssemble evidence in 32-CENSUS (Codex).
6. Create a **tracked follow-up artifact** for non-compile D3DX removal (Sonnet M1); reconcile **wave labels** (Consensus #4).

**No reviewer recommends blocking the phase.** All four rate it MEDIUM (Cursor: render-correctness HIGH). The architecture is sound; the edits above harden proof/validation and the branch-selection mechanism. To fold this back in:

```
/gsd:plan-phase 32 --reviews
```
