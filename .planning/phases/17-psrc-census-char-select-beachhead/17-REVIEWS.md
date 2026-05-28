---
phase: 17
review_round: 3
reviewers: [codex, cursor]
reviewed_at: 2026-05-28T16:22:00Z
plans_reviewed: [17-01-PLAN.md, 17-02-PLAN.md, 17-03-PLAN.md]
prior_rounds:
  - "Round 1 review at commit 797e9db57 (`docs: cross-AI review for phase 17`)"
  - "Round 2 replan at commit 6e0eae4e6 (`docs(17): incorporate cross-AI review feedback (codex+cursor)`)"
  - "Round 2 review at commit 85ff0b90b (`docs(17): cross-AI review round 2 (codex+cursor) — convergence NOT achieved`)"
  - "Round 3 replan at commit 6cfe19a7f (`docs(17): round 3 replan — convergence amendments (SR-2 reflect, HIGH-3 shadow, HIGH-4 routing, SR-3 split, plumbing fixes)`)"
verdict: CONVERGENCE ACHIEVED — execution-ready with 1 LOW-severity frontmatter-cleanup item
---

# Cross-AI Plan Review — Phase 17 (PSRC Census + Char-Select Beachhead) — ROUND 3

Same two reviewers as Rounds 1 and 2 (Codex via `codex exec --sandbox read-only` and Cursor via `cursor-agent -p --mode ask --trust`), reading the live tree at `D:/Code/swg-client-v2` and the Round-3-amended plans authored by commit `6cfe19a7f`. Round 2 verdict was **CONVERGENCE NOT ACHIEVED** — three blocking items (SR-2 hardcoded `updatePS(2,...)`, HIGH-3 stale-shadow-across-draws, HIGH-4 invisible `DEBUG_REPORT_LOG_PRINT`) plus five MEDIUM amendments and several LOWs. This Round 3 review independently verifies whether each of those items is now closed with grep-targeted, live-tree-aligned amendments — without regressing the Round 1/2 closures.

## Consensus

Both reviewers **converge on closure of all three Round 2 blockers and all five MEDIUM amendments**. Both verdicts read as "ready for execution under normal checkpoint discipline" — exactly the Round 2 forecast: "After that, plans would be MEDIUM risk — suitable for execution with normal checkpoint discipline."

| Item | Codex Round 3 | Cursor Round 3 | Consensus |
|------|---------------|----------------|-----------|
| HIGH-1 (non-fatal compile) | CLOSED | CLOSED | ✓ Closed |
| HIGH-2 (DXBC + compile-time reflect) | CLOSED | CLOSED | ✓ Closed |
| HIGH-3 (stale shadow across draws) | CLOSED | CLOSED | ✓ Closed |
| HIGH-4 log routing (`InfoQueue` dual-route) | CLOSED | CLOSED | ✓ Closed |
| HIGH-4 contingency (Option B downgrade) | CLOSED | CLOSED | ✓ Closed |
| HIGH-5 (census file sink) | CLOSED | CLOSED | ✓ Closed |
| SR-1 (D3D9 source-data port) | CLOSED | CLOSED | ✓ Closed |
| **SR-2 (offset-aware reflection upload)** | **CLOSED** | **CLOSED** | **✓ Closed (Round 2 convergent HIGH resolved)** |
| SR-3 (asm STOP-CONDITION, Task 0 split) | CLOSED | CLOSED | ✓ Closed |
| LOWs (8 items combined into Round 3) | CLOSED with minor residue | CLOSED | ✓ Closed |

**Codex verdict:** `CONVERGENCE PARTIAL — minor items, executable with executor discretion.` (One LOW-severity NEW concern — Plan 03 artifact-metadata residue at `17-03-PLAN.md:40-42`.)

**Cursor verdict:** `CONVERGENCE ACHIEVED — ready for execution under normal checkpoint discipline.` (Three MEDIUM NEW concerns — `must_haves.truths` wording drift vs R3-03c, empty-reflection-cache skip path, conditional shadow-mirror vs unconditional staging — all classified as checkpoint-foldable, not Round 4 blockers.)

**Convergent NEW finding (both reviewers):** Plan 03's `must_haves.truths` / artifact metadata still carry text from the pre-R3-03c contract (mentions of `updatePS(2,...)` and `getPerMaterialShadow` attributed to `Direct3d11_StaticShaderData.cpp`). Both reviewers note the **action text and acceptance greps are correct** — only the frontmatter wording is stale. Codex calls it LOW; Cursor calls one variant MEDIUM (the `updatePS(2,...)` wording in truths line 22-24) because an executor scanning `must_haves` first could be misled. Neither reviewer treats it as Round-4-blocking; both recommend a one-line frontmatter cleanup OR an explicit Task 1 start-up note "acceptance block `:561-576` is authoritative."

Plus a few execution edge cases (Cursor MEDIUMs): empty `layouts` from a reflection failure leaves prior GPU state on slot b2 (mitigated by per-shader skip logging in SUMMARY); shadow mirror is conditional on `writeVarByName` success but staging zero-fills unconditionally (mitigated by R3-02e truncation WARN + R3-03g cross-clobber log).

Both reviewers explicitly recommend proceeding to `/gsd-execute-phase 17`, gated on Plan 02 Task 0's `checkpoint:human-verify` census-gate verdict (all-HLSL anchors → continue; asm in anchors → spawn `17-02B-PLAN.md` per SR-3 contract).

---

## Codex Review

### Summary

Round 3 closes the Round 2 blockers in substance: SR-2 is now offset/bind-point driven, HIGH-3 has an unconditional per-pass upload/zero-fill contract, HIGH-4 logging is dual-routed through InfoQueue, and SR-3 is split into a real Task 0 gate. I found no new execution-blocking gaps. One minor metadata residue remains in Plan 03's artifact block: it still lists `getPerMaterialShadow` under `Direct3d11_StaticShaderData.cpp` even though Round 3 says the getter's home is `Direct3d11.cpp`.

### Round 2 Disposition Verification

| ID | Round 3 Verdict | Evidence (file:line) |
|---|---|---|
| HIGH-1 | CLOSED | Non-fatal asset path specified in Plan 02 while fatal install helper remains preserved: `17-02-PLAN.md:16-18`, `17-02-PLAN.md:641-644`; live fatal helper at `Direct3d11_PixelShaderProgramData.cpp:86`, `:241`, `:259`, install path at `:616-623`. |
| HIGH-2 | CLOSED | DXBC blob + cached reflection/accessors specified: `17-02-PLAN.md:18`, `:171-202`; Plan 03 consumes cache and forbids per-draw reflect: `17-03-PLAN.md:21`, `:562-563`; live VS precedent at `Direct3d11_VertexShaderData.cpp:520`, `:612-673`. |
| HIGH-3 | CLOSED | Shared shadow getter plumbing + stale-shadow fix specified: `17-03-PLAN.md:22-24`, `:85-87`, `:573-590`; live current function-local shadow risk at `Direct3d11.cpp:667`, D3D9 per-pass reference at `Direct3d9_StaticShaderData.cpp:835-896`. |
| HIGH-4 log + contingency | CLOSED | InfoQueue dual-route specified and grep-gated: `17-02-PLAN.md:19`, `:63`, `:653-656`, `:695-696`; live precedent at `Direct3d11_PixelShaderProgramData.cpp:523-540`. Contingency correctly downgraded to checkpoint guidance: `17-02-PLAN.md:23`, `:725-759`. |
| HIGH-5 | CLOSED | File sink retained: `17-01-PLAN.md:26-28`, `:195-208`, `:391-403`, `:455-464`. |
| SR-1 | CLOSED | D3D11 source-data cache uses `getMaterial` / `getTextureFactor`: `17-03-PLAN.md:20`, `:557-558`; live APIs at `StaticShader.cpp:510`, `:582`; D3D9 precedent at `Direct3d9_StaticShaderData.cpp:593-702`. |
| SR-2 | CLOSED | Contract A now uses `layout.TotalSize`, `var.StartOffset`, and `updatePS(layout.BindPoint, ...)`: `17-03-PLAN.md:25-27`, `:78`, `:401-461`, `:565-570`; hardcoded `updatePS(2,...)` in `StaticShaderData.cpp` is explicitly forbidden. |
| SR-3 | CLOSED | Task 0 checkpoint owns census gate and STOP/spawn behavior: `17-02-PLAN.md:20-22`, `:62`, `:294-365`, `:808-809`. |
| LOWs | CLOSED with minor residue | TSV length, IFF terminator, BOM/whitespace classification, truncation WARN, source `strlen`, Plan-01 HEAD sha are present: `17-01-PLAN.md:75-78`, `:324-356`, `:401-403`, `:460-464`; `17-02-PLAN.md:64-68`, `:147-164`, `:638-656`. Minor artifact residue at `17-03-PLAN.md:40-48`. |

### Strengths

- Round 3 replaced the SR-2 "presence-only" reflection idea with an actual byte-staging contract keyed by reflected offsets and bind points.
- HIGH-4 is no longer pretending an automatic fallback exists; the plan either logs enough to diagnose linkage or explicitly treats fallback as checkpoint guidance.
- SR-3 is now procedurally enforceable. The executor cannot proceed into CHAR-01 if named anchors include asm without passing through the Task 0 gate.
- The `//hlsl` classifier fix is present in both Plan 01 and Plan 02, including BOM and leading whitespace handling.

### Concerns

- **LOW NEW:** Plan 03's artifact metadata still says `Direct3d11_StaticShaderData.cpp` `contains: "getPerMaterialShadow"` at `17-03-PLAN.md:40-42`, while R3-03h says the getter is defined in `Direct3d11.cpp` and StaticShaderData only calls it at `17-03-PLAN.md:92`. The implementation instructions and verification are clear at `17-03-PLAN.md:85` and `:695`, so this is a cleanup item, not a blocker.

### Suggestions

- Before execution, adjust the Plan 03 artifact block so `Direct3d11_StaticShaderData.cpp` says it calls `getPerMaterialShadow`, while the definition target remains only `Direct3d11.cpp`.
- Keep the existing Plan 03 grep gates exactly as written; they are the important protection against reintroducing Round 2's `updatePS(2,...)` / `UNREF(var)` failure mode.

### Risk Assessment

**MEDIUM** overall. The original blockers are closed, but renderer validation still depends on runtime census data, D3D11 debug logs, and matched A/B screenshots rather than automated tests.

### Verdict on Convergence

**CONVERGENCE PARTIAL — minor items, executable with executor discretion.**

---

## Cursor Review

### Summary

Round 3 closes all three Round 2 blockers with grep-targeted, live-tree-aligned amendments: SR-2 Contract A (offset-aware staging + `layout.BindPoint`), HIGH-3 unconditional per-pass upload with zero-fill on `!pm.m_materialValid`, HIGH-4 InfoQueue dual-route logging, SR-3 Task 0 census gate, getter plumbing, type-name fix, and BOM/whitespace `//hlsl` parity. Round 1/2 closures (non-fatal compile, DXBC+reflect cache, census file sink, SR-1, wave order) are preserved. No Phase 18–22 scope creep detected. Residual gaps are documentation drift in Plan 03 `must_haves.truths` (still mentions `updatePS(2,...)` for the apply path) and a few execution edge cases foldable into checkpoints — not Round 4 architecture blockers.

### Round 2 Disposition Verification

| ID | Round 3 Verdict | Evidence (file:line) |
|----|-----------------|----------------------|
| **HIGH-1** | **CLOSED** | `17-02-PLAN.md:402-432` — `tryCompilePixelShaderFromHlslNoFatal` with FATAL→log-and-return; `:643-644` acceptance preserves `compilePixelShaderFromHlsl` FATAL sites. Live FATAL at `Direct3d11_PixelShaderProgramData.cpp:238-243`, `:259`. |
| **HIGH-2** | **CLOSED** | `17-02-PLAN.md:171-202`, `:507-606` — `m_psBytecodeBlob`, `m_reflectedCbufferLayouts` (+ `TotalSize`), `m_reflectedPSInputs`, ctor-time `D3DReflect`. Plan 03 consumes via `getReflectedCbufferLayouts`; `:563` greps `D3DReflect == 0` in StaticShaderData. VS precedent `Direct3d11_VertexShaderData.cpp:520`, `:611-665`. |
| **HIGH-3** | **CLOSED** | `17-03-PLAN.md:267-282` — file-scope shadow + getter in `Direct3d11.cpp`; `:259-264` — declaration in `Direct3d11_ConstantBuffer.h`. `:392-396` — zero source on `!pm.m_materialValid`; `:461` — unconditional `updatePS(layout.BindPoint, ...)` per layout; `:575` forbids `if (wroteAny)`. Live clobber site `Direct3d11.cpp:667`. **Doc drift:** `must_haves.truths` `:22-24` still say apply flushes via `updatePS(2,...)` — contradicts R3-03c action/acceptance. |
| **HIGH-4 log** | **CLOSED** | `17-02-PLAN.md:557-600` — dual-route `AddApplicationMessage` + `DEBUG_REPORT_LOG_PRINT` for PS-cbuffer and PS-input-sig. `:653-655` acceptance greps InfoQueue route. Precedent `Direct3d11_PixelShaderProgramData.cpp:535-540`. |
| **HIGH-4 contingency** | **CLOSED (Option B)** | `17-02-PLAN.md:23`, `:725-738` — automatic fallback removed from `must_haves.truths`; checkpoint guidance in Task 3. Live: asset PS preferred at `Direct3d11_StateCache.cpp:1107-1109`; fallback only when null `:1111-1122`. |
| **HIGH-5** | **CLOSED** | `17-01-PLAN.md:385-412` — per-record `fopen_s`/`fwrite`/`fclose` to `stage/psrc-census.tsv`; `:403` — `sizeof(header) - 1` (R3-01a). |
| **SR-1** | **CLOSED** | `17-03-PLAN.md:287-351` — construct-time `shader.getMaterial` / `getTextureFactor` → `m_passMaterial`. D3D9 shape `Direct3d9_StaticShaderData.cpp:593-700`; apply reference `:835-897`. |
| **SR-2** | **CLOSED** | `17-03-PLAN.md:418-461` — `writeVarByName` uses `var.StartOffset`, staging sized to `layout.TotalSize`, `updatePS(layout.BindPoint, staging, layout.TotalSize)`. `:565-570` — replaces Round 2 `updatePS(2,` grep with `updatePS(layout.BindPoint` + `var.StartOffset`; requires `updatePS(2,` == 0 in StaticShaderData. Round 2 `UNREF(var)` grep forbidden `:568`. |
| **SR-3** | **CLOSED** | `17-02-PLAN.md:294-366` — Task 0 `checkpoint:human-verify` census gate; asm → spawn `17-02B-PLAN.md` + STOP; Tasks 1–3 downstream. Mid-task stop primitive gone. |
| **LOWs** | **CLOSED** | R3-01a/b/c/d in `17-01-PLAN.md`; R3-02e/f/g in `17-02-PLAN.md`; R3-03e/h in `17-03-PLAN.md`. Wave `01→02→03` unchanged (`17-01:6-7`, `17-02:6-7`, `17-03:6-7`). |

### Strengths

- **Convergent HIGH fully addressed:** SR-2 Contract A is spelled out in sub-step 1e with staging buffer, `StartOffset`, `BindPoint`, and acceptance greps that explicitly reverse Round 2's enshrined `updatePS(2,` bug (`17-03-PLAN.md:565-570`).
- **HIGH-3 semantics match D3D9:** Per-pass zero source values + unconditional upload per reflected layout; `if (wroteAny)` explicitly grep-forbidden (`17-03-PLAN.md:575`).
- **HIGH-4 grepability fixed at the right layer:** InfoQueue dual-route mirrors verified precedent (`Direct3d11_PixelShaderProgramData.cpp:535-540`); boot acceptance targets `stage/d3d11-debug.log` (`17-02-PLAN.md:695-696`).
- **Procedural fragility removed:** SR-3 is a discrete Task 0 checkpoint, not embedded control flow (`17-02-PLAN.md:294-366`).
- **Getter home specified:** `getPerMaterialShadow()` declared in `Direct3d11_ConstantBuffer.h`, defined in `Direct3d11.cpp` (R3-03a `:259-264`, `:267-272`).
- **Classification parity:** BOM + whitespace skip in both Plan 01 census (`17-01-PLAN.md:327-342`) and Plan 02 recompile lane (`17-02-PLAN.md:465-491`).
- **Honest contingency scope:** D-09 downgrade to checkpoint guidance avoids grep-verifying unimplemented StateCache hook (`17-02-PLAN.md:23`, `:725-738`).
- **Landmines preserved:** Iter-44A/44C protected (`17-03-PLAN.md:541-542`, `:579-581`); asm re-assemble blocked; magenta tombstone kept.

### Concerns

- **MEDIUM — NEW: `must_haves.truths` stale vs R3-03c (`17-03-PLAN.md:22-24, :30`).** Frontmatter still says apply() flushes via `updatePS(2,...)`, while sub-step 1e and acceptance require `updatePS(layout.BindPoint, ...)`. Action/acceptance win, but an executor scanning `must_haves` first could mis-implement. Fold into Task 1 start: treat acceptance block `:561-576` as authoritative.

- **MEDIUM — NEW: empty reflection cache skips upload (`17-03-PLAN.md:506-510`).** If Plan 02 leaves `layouts` empty (reflection failure), apply makes no `updatePS` call — prior GPU cbuffer state may persist. Acceptable as non-fatal VS-stance defense, but record per-shader skips in `17-03-SUMMARY.md`; if char-select anchors hit this, STOP before CHAR-02/03.

- **MEDIUM — NEW: shadow mirror is conditional, staging is not (`17-03-PLAN.md:451-456`).** `shadow.material*` updates only when `writeVarByName` succeeds; failed name match leaves stale shadow while staging may be zeroed. Mitigated by R3-02e truncation WARN + Plan 02 PS-cbuffer dump cross-check; sub-step 1g cross-clobber log catches slot-2 incoherence at runtime.

- **LOW — NEW: `warnOnceOnTruncation` is process-global one-shot (`17-02-PLAN.md:436-445`).** Only the first truncated symbol is logged; subsequent truncations are silent. Acceptable for char-select; note in SUMMARY if multiple symbols truncate.

- **LOW — NEW: R3-03g HEAD/EYE detection via `strstr(shaderName, "head"/"eye")` (`17-03-PLAN.md:481-485`).** Fragile if anchor filenames differ; executor should key off Plan 02 SUMMARY anchor names, not rely solely on substring heuristic.

- **LOW — D-04 truth still partially aspirational in frontmatter (`17-03-PLAN.md:22`).** Implementation is generic (Contract A), but `must_haves` line 22 wording implies both paths hard-flush slot 2 — cosmetic only given acceptance greps.

### Suggestions

1. **At Plan 03 Task 1 start:** Patch-read `must_haves.truths` lines 22–24 mentally — implement sub-step 1e + acceptance `:561-576`, not the stale `updatePS(2,...)` apply wording. One-line frontmatter fix would eliminate ambiguity but is not execution-blocking.

2. **Plan 02 Task 2 boot gate:** Capture PS-cbuffer dump lines verbatim into `17-02-SUMMARY.md` with `BindPoint`, `TotalSize`, and `materialDiffuse`/`materialSpecular`/`materialEmissive` offsets — Plan 03 Contract A and R3-03d bind-loop decision depend on this.

3. **Plan 03 empty-layouts path:** If any char-select anchor has empty `layouts`, document and treat as compile/reflection failure — do not claim CHAR-02/03 from magenta/zero-constant draws.

4. **R3-03g proof:** If HEAD/EYE substring heuristic misses, fall back to logging the two passes with `pm.m_materialValid==1` and divergent `passDiffuse` from construct-time SR-1 data.

5. **HIGH-4 contingency:** Keep R3-02d as manual checkpoint only; if id=342/343 spike post-recompile, exercise Task 3 guidance (null `m_d3dPS` for affected shader → `selectFallbackPSForVS` at `StateCache.cpp:1111`) before opening a Phase 18 wiring plan.

### Risk Assessment

**MEDIUM** — Round 2's silent-failure modes (hardcoded slot 2, stale shadow skip, invisible reflection dump) are now specified with grep gates and live-tree precedents. Residual risk is execution-time (actual PSRC layout vs assumed variable names, linkage id=342/343, visual A/B) — appropriate for a beachhead phase with checkpoint discipline, not plan-convergence blockers.

### Verdict on Convergence

**CONVERGENCE ACHIEVED — ready for execution under normal checkpoint discipline.**

All eight Round 3 checklist items are present, grep-targeted, and aligned with the live tree. The three Round 2 consensus blockers (SR-2, HIGH-3 stale shadow, HIGH-4 log + contingency) are closed in plan action text and acceptance criteria. Remaining concerns are frontmatter wording drift and edge-case observability — address at executor checkpoints, not via Round 4 replan. Do **not** run `/gsd-execute-phase 17` until Task 0 census verdict (`17-02-PLAN.md:294-366`) confirms all-HLSL anchors or accepts the 17-02B spawn path.

---

## Consensus Summary

### Agreed Strengths

- **SR-2 Contract A** is implemented properly in `17-03-PLAN.md` sub-step 1e — staging buffer sized to `layout.TotalSize`, `writeVarByName` keyed off `var.StartOffset`, upload via `updatePS(layout.BindPoint, ...)`. Both reviewers explicitly note the Round 2 acceptance bug (`grep "updatePS(2,"` enshrined as passing) is now inverted: acceptance forbids `updatePS(2,` in StaticShaderData.cpp.
- **HIGH-3 unconditional per-pass upload** correctly mirrors D3D9 `Pass::apply` per-pass semantics; the Round 2 `if (wroteAny) updatePS(...)` skip pattern is explicitly grep-forbidden.
- **HIGH-4 dual-route via `ID3D11InfoQueue::AddApplicationMessage`** mirrors the live `emitFirstGeneratorLog` precedent at `Direct3d11_PixelShaderProgramData.cpp:535-540`, making reflection dumps visible under explorer-launched boots.
- **SR-3 task split** removes the procedural fragility — Task 0 owns the census-gate decision as a discrete `checkpoint:human-verify`, not embedded "stop after Task 1" control flow.
- **`getPerMaterialShadow()` plumbing** is now explicitly homed in `Direct3d11_ConstantBuffer.h` (declaration) + `Direct3d11.cpp` (definition), with `Direct3d11_StaticShaderData.cpp` including the header to consume.
- **D-09 contingency Option B downgrade** is honest about what's wired vs what's checkpoint guidance; `must_haves.truths` does not enshrine an unimplemented StateCache hook as a passing acceptance.
- **`//hlsl` classifier parity** between Plan 01 census (`17-01-PLAN.md:327-342`) and Plan 02 recompile lane (`17-02-PLAN.md:465-491`) — both skip leading whitespace + UTF-8 BOM before the 7-byte lowercase compare.
- **Landmines preserved:** Iter-44A depth wiring (`Direct3d11_StaticShaderData.cpp:658-677`) untouched; Iter-44C blend revert preserved; asm re-assemble blocked; magenta tombstone kept; D3D9 register folklore banned.

### Agreed Concerns (LOW — checkpoint-foldable, not Round 4 blockers)

- **`17-03-PLAN.md` frontmatter wording drift.** Both reviewers note that `must_haves.truths` lines 22-24 and the artifact block at `:40-48` still carry text from the pre-R3-03c contract — mentioning `updatePS(2,...)` for the apply path and attributing `getPerMaterialShadow` to `Direct3d11_StaticShaderData.cpp` (the call site) rather than `Direct3d11.cpp` (the definition site). The action text (sub-step 1e) and acceptance greps (`:565-570`) are correct; only the frontmatter prose is stale. Codex grades this LOW; Cursor flags the `updatePS(2,...)` wording in truths as MEDIUM because an executor scanning `must_haves` first could be misled. Both recommend the same fix: one-line frontmatter cleanup, OR explicit Task 1 start-up note "acceptance block `:561-576` is authoritative." Neither treats this as Round-4-blocking.

### Cursor-Only NEW Concerns (MEDIUM — execution-time observability)

- **Empty reflection cache skips upload.** If Plan 02's reflection-cache fill fails for a shader, Plan 03's apply() makes no `updatePS` call for that pass — prior GPU cbuffer state persists on slot b2. Codex didn't flag this; Cursor says it's an acceptable non-fatal stance but anchors the safety net at runtime: log per-shader skips in `17-03-SUMMARY.md`; if char-select anchors hit this, STOP before CHAR-02/03 verification.

- **Shadow mirror conditional, staging unconditional.** The file-scope `s_perMaterialShadow.material*` fields update only when `writeVarByName` succeeds; failed name matches leave stale shadow values while the staging buffer is zeroed for the upload. Cursor classifies this as MEDIUM because it's a subtle coherence question — mitigated by R3-02e truncation WARN + R3-03g cross-clobber flush-time log catching slot-2 incoherence at runtime.

### Divergent Views (where the reviewers split)

- **Final-verdict label:** Codex `CONVERGENCE PARTIAL`; Cursor `CONVERGENCE ACHIEVED`. The substantive conclusion is identical — both judge the plans executable, both call risk MEDIUM, both recommend `/gsd-execute-phase 17` after the Task 0 census gate. The split is about whether a single LOW frontmatter cleanup item is enough to label the overall verdict "partial" or "achieved." Operationally, treat as **CONVERGENCE ACHIEVED — one frontmatter touch-up recommended before execution.**

- **`must_haves.truths` `updatePS(2,...)` wording severity:** Cursor MEDIUM (executor-mislead risk); Codex implicit LOW (artifact metadata, not truths body). The wording lives in two places — Cursor flagged the truths block at `:22-24`; codex flagged the artifact block at `:40-48`. Both should be cleaned up; both reviewers explicitly note this does not gate execution.

### Recommended Next Action

1. **One-line frontmatter cleanup in `17-03-PLAN.md`** (both reviewers concur): adjust `must_haves.truths:22-24` to say `updatePS(layout.BindPoint, staging.data(), layout.TotalSize)` (matching sub-step 1e); adjust artifact-block at `:40-48` so `Direct3d11_StaticShaderData.cpp` says it CALLS `getPerMaterialShadow` while the definition target remains `Direct3d11.cpp`. **This is a docs-only touch — no plan logic change.**

2. **`/gsd-execute-phase 17`** — the Task 0 census-gate `checkpoint:human-verify` (Plan 02 line 294-366) is the first executor interaction point after Plan 01 ships. The asm-in-anchors verdict gates whether `17-02B-PLAN.md` spawns or whether Plan 02 proceeds straight to the HLSL recompile lane.

3. **At Plan 03 execution start** (per Cursor suggestion 1): treat acceptance block `17-03-PLAN.md:561-576` as authoritative over any stale `must_haves.truths` wording.

4. **At Plan 02 Task 2 boot gate** (per Cursor suggestion 2): capture PS-cbuffer dump lines verbatim into `17-02-SUMMARY.md` with `BindPoint`, `TotalSize`, and `materialDiffuse`/`Specular`/`Emissive` offsets — Plan 03's Contract A and the R3-03d StateCache bind-loop decision depend on this real data.
