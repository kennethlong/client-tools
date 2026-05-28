---
phase: 17
review_round: 2
reviewers: [codex, cursor]
reviewed_at: 2026-05-27T21:54:00Z
plans_reviewed: [17-01-PLAN.md, 17-02-PLAN.md, 17-03-PLAN.md]
prior_round: "Round 1 review at commit 797e9db57 (`docs: cross-AI review for phase 17`); Round 2 replan at commit 6e0eae4e6 (`docs(17): incorporate cross-AI review feedback (codex+cursor)`)"
verdict: CONVERGENCE NOT ACHIEVED — Round 3 amendments needed
---

# Cross-AI Plan Review — Phase 17 (PSRC Census + Char-Select Beachhead) — ROUND 2

Same two reviewers as Round 1 (Codex via `codex exec` and Cursor via `cursor-agent -p --mode ask --trust`), both reading the live tree at `D:/Code/swg-client-v2` and the revised plans authored by the Round 2 `--reviews` replan. The Round 1 internal gsd-plan-checker re-verified the revised plans and PASSED; these external reviewers found that ~85% of Round 1 issues are textually closed but **~15% of Round 1 partials + 2-3 new HIGH gaps introduced by the Round 2 expansion** are sufficient to fail the char-select beachhead silently. Both rate execution as not-ready.

## Consensus

Both reviewers **converge sharply** on one new HIGH finding (raised independently with the same file:line evidence), confirm closure on five Round 1 items, and split on the severity of two Round 1 items:

| Item | Codex | Cursor | Consensus |
|------|-------|--------|-----------|
| HIGH-1 (non-fatal compile) | CLOSED | CLOSED | ✓ Closed |
| HIGH-2 (DXBC + compile-time reflect) | CLOSED | CLOSED | ✓ Closed |
| HIGH-3 (centralized slot-b2 shadow) | CLOSED (directionally) + medium getter-plumbing gap | CLOSED (plan text) + NEW HIGH stale-shadow-across-draws | **Partial** — promotion shape right, but invalidation contract missing |
| HIGH-4 (linkage diag + D-09 contingency) | "mostly closed" — `getOrCompilePSForVS` exists, StateCache already falls back at `:1111-1122` (when `m_d3dPS` is null) | "PARTIALLY NOT CLOSED" — log routing (DEBUG_REPORT_LOG_PRINT invisible under explorer launch); contingency dispatch is documentation-only, fallback NOT wired when `m_d3dPS` is non-null | **Partial** — diagnostic surface exists, but log routing + contingency dispatch when asset PS is bound are open |
| HIGH-5 (census file sink) | CLOSED | CLOSED | ✓ Closed |
| SR-1 (D3D9 source-data port) | CLOSED | CLOSED | ✓ Closed |
| **SR-2 (reflection-by-name)** | **PARTIALLY NOT CLOSED** — `writeVarIfPresent` ignores `StartOffset`; `updatePS(2,...)` hardcodes slot 2 ignoring reflected `BindPoint` | **PARTIALLY NOT CLOSED** — same finding, same files cited | **🔴 CONVERGENT NEW HIGH** — Round 2's headline gap |
| SR-3 (asm STOP-CONDITION) | CLOSED conceptually + medium task-shape gap | CLOSED + medium task-shape gap | **Partial** — landmines preserved, but stop-condition embedded in a single auto task needs Task 0 separation |
| LOWs (paths, requirements, ctor init, etc.) | CLOSED | CLOSED | ✓ Closed |

**Verdict:** Round 2 does NOT achieve convergence. Three blocking items remain:

1. **🔴 SR-2 implementation gap (Plan 03) — CONVERGENT HIGH.** Both reviewers independently flag the same code in Plan 03 sub-step 1d: the `writeVarIfPresent` lambda only checks variable name PRESENCE in the cached reflection layout, then writes to `Direct3d11_PerMaterialCB` fields by C++ struct field NAME — explicitly marking `var.StartOffset` as `UNREF`. The upload then calls `Direct3d11_ConstantBuffer::updatePS(2, &shadow, sizeof(shadow))` regardless of the reflected `layout.BindPoint`. If the asset PSRC declares constants in `$Globals` (the D3DCompile default for loose `: register(cN)` declarations), or in a cbuffer bound to anything other than slot 2, or with field offsets that don't match `Direct3d11_PerMaterialCB`'s diffuse/specular/emissive/userConstants layout, **the upload passes every grep gate while feeding the GPU wrong bytes**. This contradicts the SR-2 must-have ("use the actual cbuffer name + bind point + variable layout"). Acceptance greps already include `grep -n "updatePS(2" ... returns >=1` — the hardcoding is enshrined as a passing acceptance criterion.

2. **🔴 HIGH-3 stale shadow across draws (Cursor HIGH — Codex implicit).** The Round 2 amendment promoted `s_perMaterialShadow` to file-scope shared state and added a read-modify-write contract — but the upload runs only when `wroteAny == true`. A pass with `!pm.m_materialValid` or empty cached reflection **skips `updatePS` entirely**, leaving the prior draw's material values on slot b2. Multi-pass char-select (body → head → eyes) is exactly where this bites — eyes inherit head material, head inherits body material. D3D9's `Pass::apply` uploads per-pass (`Direct3d9_StaticShaderData.cpp:835-897`); D3D11 must either upload every pass or zero-fill the relevant fields when source data is absent.

3. **🟡 HIGH-4 log routing + contingency dispatch (Cursor HIGH — Codex partial agreement).** Plan 02 dumps `PS-input-sig` / `PS-cbuffer` via `DEBUG_REPORT_LOG_PRINT`, but acceptance greps `stage/d3d11-debug.log`. The codebase explicitly documents that `DEBUG_REPORT_LOG_PRINT` is invisible under explorer launch (`Direct3d11_PixelShaderProgramData.cpp:516-519`, `Direct3d11_StateCache.cpp:2783-2786`); the established fix is dual-routing through `ID3D11InfoQueue::AddApplicationMessage` (precedent: `emitFirstGeneratorLog` at `:535-538`). On contingency: `getOrCompilePSForVS` exists (`.h:103`, `StateCache.cpp:986`) and StateCache already prefers it when `m_d3dPS` is null (`:1107-1122`), but **once Plan 02 binds a recompiled asset PS, linkage failures produce id=342/343 floods with NO automatic fallback** — StateCache always prefers asset PS at `:1107-1109`. The D-09 fallback is documented as checkpoint guidance, not as a code path.

Plus several MEDIUM/LOW items where the two reviewers either agree (SR-3 task-shape, getter-plumbing, POD truncation) or each found something the other missed (Codex: wrong type name `ShaderImplementationPass` vs live `ShaderImplementation::Pass`; Cursor: `//hlsl` classification uses byte-offset 0..6 instead of first-line scan, BOM/newline causes misclassification).

Both reviewers recommend a **targeted Round 3 amendment pass** (NOT re-architecture). Cursor: "After that, plans would be MEDIUM risk — suitable for execution with normal checkpoint discipline." Codex: "Once bind point and offset handling are made real, I'd downgrade the phase to MEDIUM risk."

---

## Codex Review

**Summary**

The revised plans are much closer to execution-ready, and most Round 1 HIGH/SR amendments are genuinely present and grep-verifiable. I would not call convergence complete yet: I found one new HIGH concern in Plan 03 around the reflection contract. The plan says it uses actual reflected cbuffer name/bind point/layout, but the proposed implementation still hardcodes `updatePS(2)` and ignores reflected `StartOffset`, which can silently fail for `$Globals`, non-b2 cbuffers, or different packing. That means SR-2 is only partially closed.

**Strengths**

- HIGH-1 looks closed: the live helper is fatal at `Direct3d11_PixelShaderProgramData.cpp:241` and `:259`, and install-time fallback uses it at `:616-623`; the plan adds a sibling non-fatal path only for asset PSRC.
- HIGH-2 looks substantially closed: Plan 02 adds `m_psBytecodeBlob` plus cached cbuffer/input reflection, matching the VS precedent at `Direct3d11_VertexShaderData.cpp:520` and `:612-673`.
- HIGH-3 is directionally sound: the live clobber risk exists as a function-local `s_perMaterialShadow` at `Direct3d11.cpp:667`; promoting it and sharing read-modify-write is the right model.
- HIGH-4 is mostly closed: `getOrCompilePSForVS` exists at `Direct3d11_PixelShaderProgramData.h:103` / `.cpp:647`, and StateCache already falls back through it at `Direct3d11_StateCache.cpp:1111-1122`.
- HIGH-5 is closed in intent: Plan 01 adds a real `stage/psrc-census.tsv` sink, not just debug printing.
- SR-1 is valid against live code: D3D9 resolves material/textureFactor at `Direct3d9_StaticShaderData.cpp:593-700`, and `StaticShader` exposes `getMaterial` / `getTextureFactor` at `StaticShader.h:79` and `:84`.
- SR-3 is explicit enough conceptually: named-anchor asm triggers `17-02B-PLAN.md`, and the "never re-assemble asm" landmine is preserved.

**Concerns**

- **HIGH: Plan 03 does not actually honor reflected cbuffer bind point or variable offsets.** The plan claims SR-2 closure by using the actual cbuffer name + bind point, but the proposed upload still calls `Direct3d11_ConstantBuffer::updatePS(2, &shadow, sizeof(shadow))`. Live StateCache only binds PS cbuffers 0 and 2 during normal pre-draw flush (`Direct3d11_StateCache.cpp:1138-1139`) plus PS slot 1 for alpha-test elsewhere (`:1857`). If reflection reports `$Globals` at b0, or any non-b2 cbuffer, the upload is present but invisible. The proposed `writeVarIfPresent` also ignores `StartOffset`; it only checks presence, then writes C++ fields by assumed `Direct3d11_PerMaterialCB` layout. Live `Direct3d11_PerMaterialCB` is fixed as diffuse/specular/emissive/userConstants at `Direct3d11_ConstantBuffer.h:58-66`, so any different HLSL packing or `$Globals` layout will read wrong bytes. This is a new Round 2 issue, not a Round 1 re-raise.
- **MEDIUM: Plan 03's getter plumbing is underspecified.** `Direct3d11.cpp` has an internal namespace beginning at `Direct3d11.cpp:74`, followed by `using namespace Direct3d11Namespace` at `:719`. There is no shared plugin header for `Direct3d11Namespace::getPerMaterialShadow()`. A forward declaration in `Direct3d11_StaticShaderData.cpp` can work, but the plan should prescribe it explicitly and include `Direct3d11_ConstantBuffer.h` before declaring the return type.
- **MEDIUM: Plan 03 action text uses a likely non-existent type name.** The live code uses `ShaderImplementation::Pass const *` (`Direct3d11_StaticShaderData.cpp:367`); the plan snippet says `ShaderImplementationPass const * const engPass`. If copied literally, this will not compile.
- **MEDIUM: SR-3 stop condition is embedded inside one auto task.** The plan asks the executor to make a task-start decision, complete sub-steps, boot, then stop before Task 2 if asm anchors exist. That is procedurally fragile. It should be a distinct Task 0 decision/checkpoint so the executor cannot accidentally proceed to CHAR-01 validation when an anchor is asm.
- **LOW: Plan 01 hardcodes an incorrect TSV header byte count.** The literal header string length should be derived with `strlen` or `sizeof(header) - 1`; the plan's `fwrite(..., /*len*/ 61, fp)` risks writing the terminating NUL into the TSV.
- **LOW: Plan 01's `m_psrcLen = psrcLen` likely includes the IFF string terminator.** `Iff::read_string(void)` copies through the NUL and advances by `sourceLength` including it (`Iff.cpp:1594-1606`). D3DCompile usually tolerates explicit lengths, but for cleaner cache keys and compile inputs, store `strlen(m_psrcText)` as compile length and separately use chunk length for cap/skip.
- **LOW: Fixed `Name[64]` and `SemanticName[32]` are probably fine for SWG-era assets, but truncation should be observable.** Add a one-time log when `_TRUNCATE` actually truncates, especially for cbuffer/variable names because Plan 03 depends on exact name matching.

**Suggestions**

- Fix Plan 03 by making the cbuffer upload truly reflection-driven:
  - Allocate a temporary byte buffer sized to the reflected cbuffer size.
  - Write values at `var.StartOffset` with bounds checks against reflected `Size`.
  - Upload to `layout.BindPoint`, not always slot 2.
  - Ensure `Direct3d11_ConstantBuffer::bindPS(layout.BindPoint)` is called in pre-draw, or generalize StateCache's PS cbuffer bind loop beyond hardcoded slots 0/2.
- If the intended contract is "all Phase 17 asset PSRC must declare the engine PerMaterial layout at b2," make that a hard acceptance gate:
  - fail/stop if `BindPoint != 2`;
  - fail/stop if `materialDiffuse/specular/emissive` offsets do not equal `offsetof(Direct3d11_PerMaterialCB, ...)`;
  - record the reflected offsets in `17-02-SUMMARY.md`.
- Split Plan 02 SR-3 into explicit tasks:
  - Task 0: read census and decide lane.
  - Task 1: implement HLSL lane.
  - Task 2: boot/log gate.
  - Task 3: only validate CHAR-01 if named anchors are all HLSL.
- In Plan 01, replace hardcoded TSV header length with `strlen(header)` and consider `stage/psrc-census.tsv.tmp` + rename for the aggregate artifact if crash robustness matters.
- Add grep gates for Plan 03:
  - `rg "updatePS\\(layout\\.BindPoint|updatePS\\(bindPoint" Direct3d11_StaticShaderData.cpp`
  - `rg "StartOffset" Direct3d11_StaticShaderData.cpp`
  - `rg "bindPS\\(" Direct3d11_StateCache.cpp` confirming reflected bind points are actually bound.

**Risk Assessment**

**MEDIUM-HIGH** overall until Plan 03 is corrected. Plans 01 and 02 are execution-ready with small cleanup. Plan 03 has the right data sources and lifetime model, but the current proposed upload can pass grep gates while still not feeding the shader the reflected constants. Once bind point and offset handling are made real, I'd downgrade the phase to MEDIUM risk, mostly due to visual validation and unknown asset PSRC shape.

---

## Cursor Review

### Summary

The Round 1 amendments are **substantially present** in the revised plans: non-fatal compile sibling, DXBC + compile-time reflection cache, shared slot-b2 shadow, census file sink, SR-1 source-data port, SR-3 asm stop condition, and threat-model extensions are all spelled out with grep-verifiable acceptance criteria. Wave order **01 → 02 → 03** still makes architectural sense.

However, **convergence is not achieved**. Three Round 1 items are only **partially** closed (HIGH-4 log grepability, SR-2 offset-aware upload, HIGH-4 runtime contingency wiring), and the Plan 03 expansion introduces **new HIGH-severity gaps** around global shadow stale state and a reflection consumer that checks variable names but ignores cached byte offsets/bind points. Plans are **not execution-ready** without targeted fixes; overall risk is **HIGH**.

### Round 1 Disposition Verification

| ID | Verdict | Evidence |
|----|---------|----------|
| **HIGH-1** | **CLOSED (plan text)** | Plan 02 adds `tryCompilePixelShaderFromHlslNoFatal`; preserves FATAL `compilePixelShaderFromHlsl` at install path (`Direct3d11_PixelShaderProgramData.cpp:616-622`). Live tree confirms FATAL sites at `:238-243` and `:259`. |
| **HIGH-2** | **CLOSED (plan text)** | Plan 02 adds `m_psBytecodeBlob`, `m_reflectedCbufferLayouts`, `m_reflectedPSInputs`, accessors; compile-time `D3DReflect` in ctor; Plan 03 greps `D3DReflect` == 0 in `StaticShaderData.cpp`. ComPtr lifetime matches VS `m_bytecode` precedent (`Direct3d11_VertexShaderData.cpp:520`). T-17-10 is sound. |
| **HIGH-3** | **CLOSED (plan text), NEW GAP** | Plan 03 promotes `s_perMaterialShadow` from `Direct3d11.cpp:667` to file-scope + `getPerMaterialShadow()`. **But** global shadow without per-draw invalidation introduces stale-material bleed (see NEW-3 below). T-17-11 mitigation is incomplete. |
| **HIGH-4** | **PARTIALLY NOT CLOSED** | PS-input-sig dump specified via `DEBUG_REPORT_LOG_PRINT`, but acceptance greps `stage/d3d11-debug.log`. Codebase explicitly documents that path is **invisible under explorer launch** (`Direct3d11_PixelShaderProgramData.cpp:516-519`, `Direct3d11_StateCache.cpp:2783-2786`); established fix is **dual-route via `ID3D11InfoQueue::AddApplicationMessage`** (`emitFirstGeneratorLog` at `:535-538`). `getOrCompilePSForVS` exists (`.h:103`, `StateCache.cpp:986`) but is **not wired** when `m_d3dPS` is non-null (`StateCache.cpp:1107-1109` always prefers asset PS). |
| **HIGH-5** | **CLOSED** | Plan 01 adds `stage/psrc-census.tsv` with per-record `fopen_s`/`fwrite`/`fclose`, mirroring `Direct3d11_VertexShaderData.cpp:546-575`. |
| **SR-1** | **CLOSED (plan text)** | `StaticShader.h:79,84` exposes `getMaterial` / `getTextureFactor`. D3D9 precedent at `Direct3d9_StaticShaderData.cpp:593-700`. D3D11 `construct()` already has `shader` + per-pass loop (`Direct3d11_StaticShaderData.cpp:331-393`). Fields exist on `ShaderImplementation.h:288-306`. |
| **SR-2** | **PARTIALLY NOT CLOSED** | HlslRewrite contract correctly cited (`Direct3d11_HlslRewrite.cpp:761-812` — VS-only `SwgVertexConstants:b0`). **But** Plan 03 `writeVarIfPresent` explicitly ignores `StartOffset` and always writes `PerMaterialCB` field slots; still hardcodes `updatePS(2, ...)`. This does not handle `$Globals` / loose `: register(cN)` PSRC or bind-point mismatches. |
| **SR-3** | **CLOSED** | Explicit STOP after Task 1 if asm in named anchors; spawn `17-02B-PLAN.md`; never re-assemble. |
| **LOW-1..4, DIV-1** | **CLOSED** | v2 paths, `enables: [CHAR-01..03]`, ctor init + reload null, empty PSRC skip, `Iff::read_string(void)` at `Iff.h:244` / `Iff.cpp:1581-1609`. Over-cap path is safe: `exitChunk(true)` advances by full chunk length (`Iff.cpp:1296-1306`). `getChunkLengthLeft(sizeof(char))` matches `Iff.h:1345` precedent. |

### Strengths

- **Amendments are grep-verifiable.** Each Round 1 fix has explicit acceptance patterns (`tryCompilePixelShaderFromHlslNoFatal`, `m_psBytecodeBlob`, `getPerMaterialShadow`, `psrc-census.tsv`, `getReflectedCbufferLayouts`, etc.).
- **Wave gating is correct.** Census → recompile → reflection-driven constants preserves dependency order; Plan 01 `enables` vs Plan 02/03 `requirements` traceability is clean.
- **Landmines preserved.** Magenta tombstone kept; Iter-44A depth / Iter-44C blend revert explicitly protected; D3D9 register folklore banned; asm re-assemble blocked.
- **SR-1 is the right fix.** Without construct-time `getMaterial`/`getTextureFactor`, Plan 03 would upload zeros regardless of reflection plumbing — correctly identified as CHAR-02 blocker.
- **Threat model extended appropriately.** T-17-10 (DXBC lifetime) and T-17-11 (shadow clobbering) are present; existing T-17-01..09 not weakened.

### Concerns

#### Round 1 residuals (partially closed)

- **HIGH — HIGH-4 log sink mismatch (Round 1 NOT fully closed):** Plan 02 Task 1 dumps `PS-input-sig` / `PS-cbuffer` via `DEBUG_REPORT_LOG_PRINT` only, but acceptance requires `grep -c "PS-input-sig" stage/d3d11-debug.log`. The plugin's own precedent (`emitFirstGeneratorLog`, `Direct3d11_PixelShaderProgramData.cpp:515-540`) dual-routes through **`ID3D11InfoQueue::AddApplicationMessage`** for explorer-launched smokes. Without that, HIGH-4 acceptance can false-fail even when reflection succeeded.

- **HIGH — SR-2 reflection consumer is presence-only (Round 1 NOT fully closed):** Plan 03 sub-step 1d finds variables in cached layout, then writes `shadow.materialDiffuse = pm.m_diffuse` using **engine struct field order**, explicitly marking `StartOffset` as `UNREF`. For D3D9-style PSRC with loose `float4 materialDiffuse : register(c11)` (no named `PerMaterial` cbuffer), D3DCompile typically places constants in a **`$Globals` cbuffer at non-zero offsets**. Presence check passes; GPU reads wrong bytes. The plan's "beachhead assumes PerMaterialCB layout matches asset PSRC" contradicts the must-have claim that upload uses cached **Name + BindPoint**, not hardcoded b2.

- **MEDIUM — HIGH-4 contingency is diagnostic-only, not dispatch:** `getOrCompilePSForVS` exists and works as fallback when `m_d3dPS` is null (`StateCache.cpp:1107-1123`). Once Plan 02 binds a recompiled asset PS, **linkage failures produce id=342/343 floods with no automatic fallback**. D-09 contingency is documented in checkpoints, not in code paths. Acceptable as executor discretion, but must_haves overstate automated contingency.

#### New gaps from amendments

- **HIGH — Global `s_perMaterialShadow` stale state across draws (NEW):** D3D9 stores material per pass (`Direct3d9_StaticShaderData` per-pass struct, uploaded every `Pass::apply()` at `:835-897`). Plan 03 uses one file-scope shadow; upload runs only when `wroteAny == true`. Passes with `!pm.m_materialValid` or missing reflection **skip `updatePS`**, leaving prior draw's material on GPU slot b2. Multi-pass char-select (body → head → eyes) is exactly where this bites CHAR-02/03.

- **HIGH — Plan 03 bind point not actually consumed (NEW):** Sub-step 1d logs bind-point mismatches but always calls `Direct3d11_ConstantBuffer::updatePS(2, ...)`. If reflected `BindPoint != 2`, constants never reach the shader. Acceptance greps `updatePS(2` but not bind-point correctness. Conflicts with SR-2 must-have.

- **MEDIUM — `getPerMaterialShadow()` declaration plumbing unspecified (NEW):** `Direct3d11_StaticShaderData.cpp` does not include `Direct3d11_ConstantBuffer.h` today; `Direct3d11.h` only exports `hresultString`. Plan says "pick smallest-diff accessor surface" but gives no header home. Risk: TU linkage error or duplicate static if executor guesses wrong.

- **MEDIUM — Plan 02 SR-3 stop is embedded in a single auto task (NEW):** "Stop plan after Task 1, spawn 17-02B, do not proceed to Task 2" is control-flow inside one `<task type="auto">`. GSD executor has no mid-task stop primitive — needs **Task 0: census gate decision** as separate checkpoint before CHAR-01 work.

- **MEDIUM — `//hlsl` classification uses first 7 bytes, not first line (NEW):** ShaderBuilder classifies the **first line** (`PixelShaderProgramView.cpp:301-308`). Plans 01/02 lowercase `m_psrcText[0..6]` without skipping leading whitespace/blank lines. Leading BOM/newline → misclassified as asm → magenta for HLSL anchors.

- **MEDIUM — Cross-clobber check timing (NEW):** HIGH-3 acceptance requires one-shot logs proving userConstants + material* coexist. Plan does not specify **call order** relative to `applyPreDrawState` / `setPixelShaderUserConstants_impl`. Coexistence in shadow ≠ correct values reaching GPU for the draw if last writer is wrong path.

- **LOW — POD name truncation:** `Direct3d11_ReflectedPSCbufferVar::Name[64]` is probably fine for char-select; HLSL reflection can emit mangled names longer than 64 in edge cases. Truncation → failed name match → silent zero upload.

- **LOW — Plan 03 artifact typo:** `must_haves.artifacts` for `Direct3d11_StaticShaderData.cpp` lists `contains: "getPerMaterialShadow"` but getter lives in `Direct3d11.cpp`.

### Suggestions

1. **Fix HIGH-4 grep path (Plan 02):** Mirror `emitFirstGeneratorLog` — for each `PS-input-sig` / `PS-cbuffer` line, call `Direct3d11_Device::getInfoQueue()->AddApplicationMessage(...)` **and** `DEBUG_REPORT_LOG_PRINT`. Alternatively append to `stage/psrc-reflection-dump.tsv` (same rationale as HIGH-5).

   ```text
   grep -c "PS-input-sig" stage/d3d11-debug.log
   grep -c "AddApplicationMessage.*PS-input-sig"  # if file-sink added instead
   ```

2. **Make SR-2 upload offset-aware (Plan 03):** Replace `writeVarIfPresent` → field-name mapping with:
   - Find layout containing variable by name.
   - `memcpy` into a **byte staging buffer** sized to `max(layout.Size, sizeof(PerMaterialCB))` at `var.StartOffset`, **or** extend `Direct3d11_ConstantBuffer::updatePS` to accept `(slot, bindPoint, rawBytes, size)` from reflection.
   - Use `layout.BindPoint` for upload slot; grep acceptance: `updatePS(.*layout.BindPoint` or `bindPS(reflectedBindPoint)`.

3. **Fix stale shadow (Plan 03, HIGH-3 extension):** At start of each `apply()` pass handling, when `!pm.m_materialValid`, **zero `materialDiffuse/Specular/Emissive`** (and textureFactor if extended) in shadow and call `updatePS`. Or store per-pass material in `PerPassMaterial` and upload full struct every pass (mirrors D3D9 per-pass upload semantics).

4. **Split Plan 02 Task 1 / SR-3 gate:**
   - **Task 0 (checkpoint):** Read `17-01-SUMMARY.md` + TSV; record asm-in-anchors verdict; spawn or skip `17-02B`.
   - **Task 1:** Recompile lane only.
   - **Task 2:** Boot gate.
   - **Task 3:** CHAR-01 checkpoint.

5. **Classification parity (Plans 01/02):** Skip leading `\r\n\t ` before `//hlsl ` check; or scan first non-empty line like ShaderBuilder. Grep: `skipLeadingWhitespace.*//hlsl`.

6. **`getPerMaterialShadow` declaration:** Add to `Direct3d11_ConstantBuffer.h` (already owns `Direct3d11_PerMaterialCB`):

   ```cpp
   namespace Direct3d11Namespace { Direct3d11_PerMaterialCB & getPerMaterialShadow(); }
   ```

   Include that header from `Direct3d11_StaticShaderData.cpp`.

7. **HIGH-4 contingency (optional, Plan 02 or 03):** If post-recompile boot shows id=342/343 > 0 for a shader, document executor step: temporarily null `m_d3dPS` for that program OR add StateCache hook: when debug flag set and linkage errors detected, prefer `selectFallbackPSForVS` over asset PS. At minimum, move contingency from must_have truth to checkpoint guidance so it is not grep-verified as implemented.

8. **Census boot ordering (Plan 01 Task 3):** PS program ctors (Plan 02) run **after** census boot. Confirm Task 3 runs with Plan 01-only binary so TSV reflects census, not recompile side effects.

### Wave Structure

| Wave | Plan | Depends on | Still valid? |
|------|------|------------|----------------|
| 1 | 17-01 | — | Yes — gating artifact |
| 2 | 17-02 | 17-01 | Yes — needs retained PSRC + census anchors |
| 3 | 17-03 | 17-02 | Yes — needs cached reflection + bound `m_d3dPS` |

No renumbering needed. Plan 03 expansion does not break dependencies; it **tightens** the contract Plan 02 must satisfy.

### Locked Decisions Check (D-01..D-09)

| Decision | Honored? | Notes |
|----------|----------|-------|
| D-01 engine census | Yes | Shared `ConfigClientGraphics` flag + load-site instrumentation |
| D-02 asset tree as-is | Yes | No retail-TRE reconciliation in gating path |
| D-03 census kept | Yes | Flag-gated, not throwaway |
| D-04 generic reflect, minimal feeds | **Partial** | Scope correct; SR-2 implementation not truly generic |
| D-05 hybrid A/B | Yes | COMPARISON dir + naming |
| D-06 dual boot + id=342/343 | Yes | Hard gates throughout |
| D-07 committed pairs | Yes | Checkpoints for CHAR-02/03 |
| D-08 skinning confirm only | Yes | RenderDoc checkpoint, deferred fix |
| D-09 recompile-first | Yes | Bridge PS as manual contingency only |

No scope creep into Phase 18–22 feature work detected.

### Risk Assessment

**Overall: HIGH**

**Justification:** Round 1 closed the *architecture* (non-fatal compile, reflection cache, shared shadow, census file sink, source-data port). Remaining issues are in **implementation contracts** that directly affect CHAR-02/03: (1) reflection dump may not land in `d3d11-debug.log`, (2) upload ignores reflected offsets/bind points, (3) global shadow can serve stale material across passes. Any one of these can produce "textures work, eyes gray/wrong" false partial wins — the exact failure mode Phase 17 exists to eliminate.

**Path to execution-ready:** Address the four HIGH items above (log routing, offset-aware upload, shadow invalidation, bind-point consumption). After that, plans would be **MEDIUM** risk — suitable for execution with normal checkpoint discipline.

### Verdict on Convergence

**Round 2 does NOT achieve convergence.** Do not execute yet without amending Plan 02 (HIGH-4 log routing) and Plan 03 (SR-2 offset-aware upload, HIGH-3 stale-shadow fix, bind-point consumption, getter declaration). Round 1 dispositions are **~85% closed in plan text**; the remaining ~15% plus new Plan 03 gaps are sufficient to fail the char-select beachhead silently rather than loudly.

---

## Recommendations (priority order, for Round 3 replan)

1. **🔴 SR-2 implementation fix (Plan 03 sub-step 1d) — CONVERGENT HIGH.** Replace the `writeVarIfPresent` lambda + hardcoded `updatePS(2, &shadow, sizeof(shadow))` with a truly reflection-driven upload:
   - For each variable in the cached layout, look up by NAME, copy the source-data value into a **staging byte buffer at `var.StartOffset`** (bounds-checked against `var.Size`); the staging buffer is sized to the cbuffer's reflected total size.
   - Upload via `Direct3d11_ConstantBuffer::updatePS(layout.BindPoint, &stagingBuffer, layout.totalSize)` — NOT slot 2 hardcoded.
   - Ensure StateCache's PS cbuffer bind loop covers `layout.BindPoint` (today only slots 0/2 are bound in normal pre-draw flush per `Direct3d11_StateCache.cpp:1138-1139`; slot 1 only for alpha-test at `:1857`).
   - Acceptance greps: `rg "updatePS\\(layout\\.BindPoint|var\\.StartOffset" Direct3d11_StaticShaderData.cpp` → both present.
   - **Alternative if Phase 17 wants a narrower contract:** explicitly require all char-select asset PSRC to declare PerMaterial at b2 with the engine struct layout. Then the hardcoded slot 2 is correct but must be a HARD ACCEPTANCE GATE (fail/stop if `BindPoint != 2` or if `materialDiffuse` offset != `offsetof(Direct3d11_PerMaterialCB, materialDiffuse)`). Record reflected offsets in `17-02-SUMMARY.md` so a violation is visible.

2. **🔴 HIGH-3 stale-shadow fix (Plan 03) — Cursor HIGH.** When `!pm.m_materialValid` for a pass, either zero `materialDiffuse/Specular/Emissive` (+ textureFactor if extended) in the shadow and call `updatePS`, OR upload the full per-pass struct EVERY pass (mirrors D3D9 per-pass upload). Add a regression test in acceptance: "consecutive passes with different material data produce different cbuffer contents at slot b2 (verified via RenderDoc OR a one-shot debug log of the shadow at flush time for the head and eye passes)."

3. **🟡 HIGH-4 log routing fix (Plan 02) — Cursor HIGH.** Dual-route `PS-input-sig` / `PS-cbuffer` lines through `ID3D11InfoQueue::AddApplicationMessage` (mirror `emitFirstGeneratorLog` at `:535-538`) so explorer-launched smokes can grep `stage/d3d11-debug.log`. Alternatively append to a dedicated `stage/psrc-reflection-dump.tsv` (same rationale as HIGH-5).

4. **🟡 SR-3 task-shape fix (Plan 02) — both reviewers MEDIUM.** Split Task 1 into Task 0 (census gate decision: read 17-01-SUMMARY.md + TSV; spawn 17-02B if asm anchors found; stop or proceed) → Task 1 (HLSL recompile lane) → Task 2 (boot gate) → Task 3 (CHAR-01 checkpoint, only runs if all named anchors were HLSL). The GSD executor has no mid-task stop primitive; this is procedurally fragile as written.

5. **🟡 `getPerMaterialShadow()` declaration plumbing (Plan 03) — both reviewers MEDIUM.** Add the namespace forward declaration to `Direct3d11_ConstantBuffer.h` (which already owns `Direct3d11_PerMaterialCB`): `namespace Direct3d11Namespace { Direct3d11_PerMaterialCB & getPerMaterialShadow(); }`. Include that header from `Direct3d11_StaticShaderData.cpp`. Specify the home; do not leave it as "pick smallest-diff option."

6. **🟡 Type-name correction (Plan 03 sub-step 1b) — Codex MEDIUM.** The plan snippet uses `ShaderImplementationPass const * const engPass`; live code uses `ShaderImplementation::Pass const *` (`Direct3d11_StaticShaderData.cpp:367`). Fix the snippet text so it compiles when copied verbatim.

7. **🟡 `//hlsl` classification fix (Plans 01/02) — Cursor MEDIUM.** Skip leading `\r\n\t ` (and a possible UTF-8 BOM `EF BB BF`) before the lowercase-first-7 == "//hlsl " check, OR scan the first non-empty line like ShaderBuilder does (`PixelShaderProgramView.cpp:301-308`). A BOM/newline-prefixed HLSL PSRC will misclassify as asm today and land in the magenta tombstone.

8. **🟢 LOW items (combine into Round 3):**
   - Plan 01: `fwrite(..., /*len*/ 61, fp)` → replace with `strlen(header)` or `sizeof(header) - 1` (Codex; the literal byte count is a NUL-write risk).
   - Plan 01: `m_psrcLen = psrcLen` includes the IFF string terminator; for cleaner compile/cache inputs, store `strlen(m_psrcText)` as the compile length separately from the chunk length used for cap/skip (Codex).
   - Plan 03: artifact-block typo — `must_haves.artifacts` for `Direct3d11_StaticShaderData.cpp` says `contains: "getPerMaterialShadow"` but the getter lives in `Direct3d11.cpp` (Cursor).
   - Plan 02/03 POD types: log a one-time WARNING when `_TRUNCATE` actually truncates `Name[64]` / `SemanticName[32]`, especially for cbuffer/variable names since Plan 03 depends on exact name matching (both reviewers).
   - Plan 02 cross-clobber check: specify call order relative to `applyPreDrawState` / `setPixelShaderUserConstants_impl` (Cursor).
   - Plan 01 Task 3: confirm the census boot uses a Plan-01-only binary (Plan 02's recompile ctor hasn't shipped yet at that point — verify ordering isn't accidentally muddied) (Cursor).

With these amendments folded in, both reviewers expect the phase to drop to MEDIUM risk and be ready for execution. **Do not run `/gsd-execute-phase 17` yet.**
