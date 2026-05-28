---
phase: 17
reviewers: [codex, cursor]
reviewed_at: 2026-05-28T02:16:30Z
plans_reviewed: [17-01-PLAN.md, 17-02-PLAN.md, 17-03-PLAN.md]
---

# Cross-AI Plan Review — Phase 17 (PSRC Census + Char-Select Beachhead)

Two independent reviewers, both reading the **live tree** at `D:/Code/swg-client-v2`: Codex (prompt + read-only repo access) and Cursor (`--mode ask`, repo-access edge). The internal gsd-plan-checker PASSED; these external reviewers verified plan claims against actual source and surfaced execution-blocking gaps the checker did not.

## Codex Review

**Overall Summary**

The wave ordering is directionally sound: Plan 01 measures and retains PSRC, Plan 02 consumes retained HLSL, Plan 03 handles constants and validation. The plans honor most locked decisions and landmines. The main issue is that Plan 02/03 assume APIs and runtime state that the live tree does not currently expose: `compilePixelShaderFromHlsl()` is fatal on compile/create failure, and Plan 03 cannot reflect an `ID3D11PixelShader` without retaining the compiled blob. Those are fixable, but they should be made explicit before execution.

I verified the key source claims against `D:\Code\swg-client-v2`.

**Live-Tree Verification**

Accurate claims:
- PSRC discard site exists at [ShaderImplementation.cpp](D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp:2895): `enterChunk(TAG_PSRC); exitChunk(TAG_PSRC, true);`
- PS program currently has only `m_exe` at [ShaderImplementation.h](D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.h:680)
- Destructor/reload ownership sites are at [ShaderImplementation.cpp](D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp:2783) and [ShaderImplementation.cpp](D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp:2832)
- `ConfigClientGraphics` uses `[ClientGraphics]` `KEY_BOOL` at [ConfigClientGraphics.cpp](D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp:66)
- `compilePixelShaderFromHlsl()` exists at [Direct3d11_PixelShaderProgramData.cpp](D:/Code/swg-client-v2/src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp:86)
- `D3D11_REWRITE_VERSION` is currently `"20"` at [Direct3d11_PixelShaderProgramData.cpp](D:/Code/swg-client-v2/src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp:151)
- D3D11 PEXE reject path exists at [Direct3d11_PixelShaderProgramData.cpp](D:/Code/swg-client-v2/src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp:748)
- StaticShaderData deferral and SRV slot binding claims match [Direct3d11_StaticShaderData.cpp](D:/Code/swg-client-v2/src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp:601) and [Direct3d11_StaticShaderData.cpp](D:/Code/swg-client-v2/src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp:711)
- `stage/client_d.cfg` asset paths are accurate at [client_d.cfg](D:/Code/swg-client-v2/stage/client_d.cfg:23)

Important contradiction:
- `compilePixelShaderFromHlsl()` is not non-fatal today. It calls `FATAL` on `D3DCompile` failure and `FATAL_DX_HR` on `CreatePixelShader` failure at [Direct3d11_PixelShaderProgramData.cpp](D:/Code/swg-client-v2/src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp:238). Plan 02’s “compile failure falls through to magenta tombstone” is false unless the helper is changed or wrapped.

## 17-01-PLAN.md

**Summary**

Plan 01 is mostly well-scoped and correctly places the only shared `clientGraphics` edit before D3D11-specific work. It captures the right gating deliverable and includes the right dual-renderer boot guard. The main risks are around raw PSRC string reading, ownership initialization, and making the census output reliable enough to become the Plan 02/03 artifact.

**Strengths**

- Correctly keeps the census engine-instrumented rather than offline.
- Correctly uses shared `[ClientGraphics]` config, not `ConfigDirect3d11`.
- Correctly identifies all ownership sites for the new PSRC buffer.
- Good dual-renderer boot gate for the shared-code edit.
- COMPARISON scaffold is useful and matches the locked validation strategy.

**Concerns**

- **HIGH:** The plan says to read PSRC by chunk byte length and raw bytes, but `TAG_PSRC` is written by `insertChunkString()` and existing readers use `Iff::read_string()` / `IffReadString()`. Raw reading may work, but the plan should specify exact API use and string terminator handling.
- **HIGH:** The constructor must initialize `m_psrcText=nullptr` and `m_psrcLen=0` before any load/capability early path. The plan mentions this in Task 2 but it should be an acceptance criterion.
- **MEDIUM:** No sanity cap on PSRC chunk length. A corrupt asset could allocate an unbounded buffer.
- **MEDIUM:** `DEBUG_REPORT_LOG_PRINT` may not produce a clean machine-readable census artifact. A dedicated `stage/psrc-census.tsv` or `.csv` is more reliable.
- **MEDIUM:** `stage/client_d.cfg` is changed during the task but not listed under `files_modified`. If the plan expects temporary local edits only, say that explicitly.
- **LOW:** `requirements: [CHAR-01, CHAR-02, CHAR-03]` overstates this plan. It enables those requirements but does not satisfy them.
- **LOW:** `execution_context` paths point at `D:/Code/swg-client/...`, not `D:/Code/swg-client-v2/...`.

**Suggestions**

- Prefer `Iff::read_string(std::string&)` or `char *tmp = iff.read_string()` if ownership is clear; otherwise document the exact `read_uint8(len, ...)` raw read.
- Add acceptance criteria: initialized in ctor, reset after reload, no double-delete, no stale pointer after reload.
- Add a PSRC max length guard, for example warning+skip above a conservative threshold.
- Emit census to a dedicated file with one row per shader and aggregate summary.
- Separate “Plan 01 enables CHAR” from “Plan 01 satisfies CHAR.”

**Risk Assessment: MEDIUM**

The design is good, but this is shared asset-loading code. A small ownership or string-read mistake can break both renderers. The boot gate mitigates this.

## 17-02-PLAN.md

**Summary**

Plan 02 targets the right seam, but it currently depends on a false premise: the existing HLSL compile helper is not safe to call on arbitrary asset PSRC because failures are fatal. It also needs to retain the compiled blob or reflection metadata for Plan 03. Without those adjustments, this plan can crash at first bad shader and leave Plan 03 without the data it needs.

**Strengths**

- Correctly avoids passing `m_exe`/PEXE to `CreatePixelShader`.
- Correctly preserves the magenta fallback as the diagnostic tombstone in concept.
- Correctly bumps `D3D11_REWRITE_VERSION`.
- Correctly gates CHAR-01 separately from eyes/head multi-stage validation.
- Correctly uses Plan 01’s census as a dependency.

**Concerns**

- **HIGH:** `compilePixelShaderFromHlsl()` currently fatal-crashes on compile or `CreatePixelShader` failure. This directly contradicts Plan 02’s fallback behavior.
- **HIGH:** The asm lane clause is too broad: “if census showed asm among named anchors, port asm->HLSL by hand” could explode Plan 02 scope. That should become a separate plan or explicit stop condition.
- **HIGH:** Plan 03 needs reflection, but Plan 02 only stores `ID3D11PixelShader`. D3DReflect needs DXBC bytecode. No blob is retained or exposed.
- **MEDIUM:** Recompiled asset PS input signatures may not match current VS outputs. The dynamic fallback path had custom VS-output mirroring specifically to avoid id=342/343; Plan 02 does not describe how authored PS inputs are made linkage-safe.
- **MEDIUM:** `files_modified` omits `Direct3d11_PixelShaderProgramData.h`, but exposing bytecode/reflection data likely requires header changes.
- **LOW:** The helper comments still mention old phase/profile text in places. Not blocking, but stale comments may confuse execution.

**Suggestions**

- First refactor or add a `tryCompilePixelShaderFromHlsl()` variant that returns false on D3DCompile/CreatePixelShader failure and logs errors.
- Store compiled DXBC blob or extracted reflection metadata in `Direct3d11_PixelShaderProgramData`; expose accessors for Plan 03.
- If asm appears in char-select anchors, stop after recording it and create `17-02B` for asm-port scope.
- Add a specific linkage validation step: dump/reflection-check PS input semantics against bound VS output semantics before binding or after first boot.
- Update `files_modified` to include `Direct3d11_PixelShaderProgramData.h` if blob/reflection accessors are added.

**Risk Assessment: HIGH**

The target is correct, but the current helper’s fatal behavior is a boot-blocking mismatch. The missing bytecode retention also blocks the next wave.

## 17-03-PLAN.md

**Summary**

Plan 03 has the right validation goals and correctly insists on reflection-driven constants, but its implementation shape is under-specified and partly incompatible with the live tree. `D3DReflect` cannot run on `m_d3dPS`; it needs the compiled blob from Plan 02. There is also a serious cbuffer ownership risk: `Direct3d11.cpp::setPixelShaderUserConstants_impl()` already owns a static `PerMaterialCB` shadow and uploads slot `b2`, so StaticShaderData updates can clobber or be clobbered unless the shadow is centralized.

**Strengths**

- Correctly rejects D3D9 register folklore.
- Correctly keeps blend factors deferred and avoids `_SRGB` changes.
- Correctly treats Iter-44A depth as already done and only revalidates it.
- Correctly separates CHAR-02 and CHAR-03 validation with matched A/B pairs.
- Correctly records D-08 skinning confirmation before attributing mesh artifacts to PS.

**Concerns**

- **HIGH:** `D3DReflect(m_d3dPS)` is not possible. Reflection requires the compiled shader bytecode blob, which Plan 02 does not retain.
- **HIGH:** `PerMaterialCB` slot `b2` is already updated by `setPixelShaderUserConstants_impl()` in [Direct3d11.cpp](D:/Code/swg-client-v2/src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:660). Plan 03 does not modify that file, so material uploads and user constants can overwrite each other.
- **HIGH:** The plan assumes a reflected cbuffer named `PerMaterial`. That only works if `HlslRewrite` actually rewrites asset PS constants into that named cbuffer/binding. The live code comments around [Direct3d11_HlslRewrite.cpp](D:/Code/swg-client-v2/src/engine/client/application/Direct3d11/src/win32/Direct3d11_HlslRewrite.cpp:797) suggest existing rewrite cbuffer layout may not match `PerMaterialCB`.
- **MEDIUM:** `textureFactor` has no explicit field in `Direct3d11_PerMaterialCB`. Reusing `userConstants[]` by “reflected layout” is ambiguous and may collide with real PS user constants.
- **MEDIUM:** The plan says reflection failure leaves constants zero. That is safe, but it will likely fail CHAR-02/03; it should log a clear one-shot diagnostic with shader name and missing variable names.
- **MEDIUM:** `Direct3d11_ConstantBuffer.cpp` is listed, but the more likely required file is `Direct3d11.cpp` or a new shared material-cbuffer shadow helper.

**Suggestions**

- Move reflection metadata creation into `Direct3d11_PixelShaderProgramData` immediately after compile, store cbuffer binding/offsets by name, and expose accessors.
- Centralize `PerMaterialCB` shadow state so material diffuse/specular/emissive, texture factors, and `userConstants[]` are merged into one full-struct upload.
- Add `Direct3d11.cpp` to `files_modified` if slot `b2` shadow ownership changes.
- Confirm and document the actual HLSL rewrite contract: cbuffer name, binding slot, variable names, and offsets.
- Add explicit `textureFactor[2]` fields or a named mapping with no collision against `userConstants[]`.

**Risk Assessment: HIGH**

The validation goals are right, but the implementation path needs redesign around bytecode retention and cbuffer ownership. Without that, Plan 03 is likely to compile poorly or produce visually misleading results.

**Final Recommendation**

Keep the three-wave structure, but revise before execution:

- Plan 01: tighten PSRC read/ownership and census artifact output.
- Plan 02: make PS compile non-fatal and retain DXBC/reflection data.
- Plan 03: consume stored reflection metadata and centralize `PerMaterialCB` slot `b2` updates.

With those corrections, the phase becomes a credible path to CHAR-01/02/03 while honoring D-01..D-09.

---

## Cursor Review

# Phase 17 Plan Review — PSRC Census + Char-Select Beachhead

## Overall Summary

The three-plan wave structure (census → recompile → reflection/constants → validation) is sound and aligns well with locked decisions D-01–D-09. Research claims match the live tree at the cited sites: PSRC is discarded at `ShaderImplementation.cpp:2900-2901`, `compilePixelShaderFromHlsl` exists at `Direct3d11_PixelShaderProgramData.cpp:86-171`, the PEXE-reject ctor is at `:716-756`, the deferral comment is at `Direct3d11_StaticShaderData.cpp:601-605`, and VSPS pass selection is at `ShaderImplementation.cpp:1592-1602`. The plans honor most named landmines (no asm re-assembly, keep magenta tombstone, dual-renderer boot gate, no early blend factors, no `_SRGB` flip).

However, verification against the live tree surfaces **three execution-critical gaps** Plan 02/03 do not fully specify: (1) `compilePixelShaderFromHlsl` **FATALs on compile failure**, contradicting the “magenta tombstone on failure” contract; (2) D3D11 `StaticShaderData` has **no D3D9-equivalent per-pass material/textureFactor cache**, so Plan 03’s reflection upload has nothing to read; (3) `PixelShaderProgramData` stores only `m_d3dPS`, **not the DXBC blob**, so `D3DReflect` at the `:601` apply site has no obvious input unless Plan 02/03 extends the compile path. These are fixable with scoped plan amendments but are blockers as written.

**Overall risk: MEDIUM-HIGH** — architecture and sequencing are strong; the gaps above are likely to cause boot crashes, zero constants despite “wired” reflection, or CHAR-02/03 failures until addressed.

---

## Plan 17-01 — PSRC Retain + Census

### Summary

Well-scoped shared-code touch: add `m_psrcText`/`m_psrcLen`, flag-gated census, COMPARISON scaffold, human gate for HLSL:asm ratio. Correctly treats census as the gating artifact and keeps instrumentation for Phase 20 (D-03). Line references verified (`ShaderImplementation.h:680` has only `m_exe`; VS precedent at `:427-428`; discard site at `ShaderImplementation.cpp:2900-2901`).

### Strengths

- **Correct integration point** — PSRC is only visible at the shared load site; `ConfigClientGraphics` is the right flag home (not `ConfigDirect3d11`).
- **Ownership plan mirrors VS** — destructor `:2783` and reload cleanup `:2832` both need `delete [] m_psrcText`; plan calls both out.
- **Dual-renderer boot gate** — D-06 explicitly required for the only shared edit; appropriate.
- **Human checkpoint (Task 3)** — correctly blocks Plans 02/03 until HLSL:asm ratio and anchors exist.
- **Threat model (T-17-01)** — bounds + null-terminate on PSRC read matches `insertChunkString` semantics (`Iff.cpp:893-896` writes `strlen+1` including embedded null).

### Concerns

| Severity | Issue |
|----------|-------|
| **MEDIUM** | **Census log sink** — Task 2 defaults to `DEBUG_REPORT_LOG_PRINT`, which research notes is invisible under explorer launch. Task 3 assumes census output is capturable; without a file sink (e.g. `stage/psrc-census.log`) or explicit DebugView/CLI boot, the gating artifact may be lost. |
| **MEDIUM** | **Constructor init missing from Task 1** — `ShaderImplementationPassPixelShaderProgram` ctor (`:2751-2756`) initializes `m_exe(NULL)` but not new members. Task 2 mentions init; Task 1 should require `m_psrcText(nullptr)` / `m_psrcLen(0)` in the ctor init list to avoid undefined reads before first load. |
| **MEDIUM** | **Reload path incomplete** — reload sets `m_exe = 0` after delete (`:2835`) but plan should also null `m_psrcText`/`m_psrcLen` after delete to avoid dangling pointers if load fails mid-reload. |
| **LOW** | **Empty/missing PSRC chunk** — plan mentions bounds but not explicit behavior for `len == 0` (leave `m_psrcText = nullptr`, skip census record vs. logging empty). |
| **LOW** | **Metadata noise** — frontmatter lists `requirements: [CHAR-01, CHAR-02, CHAR-03]` though this plan delivers census infrastructure only; harmless for traceability but may confuse executors. |
| **LOW** | **`getVersionMajor()`** — reads `m_exe[0]` without null check (`:2857`); pre-existing, but first load with retained PSRC-only failure modes worth a smoke note. |

### Suggestions

- Add a **dedicated census file sink** (e.g. append to `stage/psrc-census.log` when `psrcCensus=true`) in addition to debug print; document aggregation step for HLSL:asm ratio.
- Task 1 acceptance: ctor init list includes `m_psrcText(nullptr), m_psrcLen(0)`.
- Task 2: on reload cleanup, set `m_psrcText = nullptr; m_psrcLen = 0` after delete.
- Consider reading PSRC via chunk length + `iff.read` bytes (as planned) — verified compatible with `insertChunkString` format; no need for `read_uint32`.

### Risk Assessment

**LOW–MEDIUM** — Smallest blast radius; main risks are observability of census output and ownership edge cases on reload.

---

## Plan 17-02 — Recompile Lane + CHAR-01

### Summary

Correct primary lane: feed retained `//hlsl` PSRC into existing `compilePixelShaderFromHlsl`, bind `m_d3dPS`, bump `D3D11_REWRITE_VERSION` (`"20"` → `"21"` at `:151`). StateCache already prefers asset PS when non-null (`Direct3d11_StateCache.cpp:1107-1109`) over per-VS dynamic fallback — exactly the behavior Phase 17 needs. CHAR-01 before multi-sampler/constants (Plan 03) matches Pitfall 5.

### Strengths

- **Recompile-first (D-09)** — uses proven compile/cache/include stack; no new dependencies.
- **Cache invalidation** — `D3D11_REWRITE_VERSION` bump is required and correctly specified.
- **Plugin-only edit** — no shared-code risk beyond Plan 01.
- **CHAR-01 isolation** — single-stage control before eyes/head; checkpoint requires D3D9 match, not “no magenta.”
- **Asm lane conditional on census** — secondary port only if Plan 01 finds asm; never re-assemble — landmine honored.

### Concerns

| Severity | Issue |
|----------|-------|
| **HIGH** | **`compilePixelShaderFromHlsl` FATALs on failure** — at `:241-242` and `:259-260` it calls `FATAL`/`FATAL_DX_HR`, it does **not** return false on compile/create failure (only empty input returns false at `:92-93`). Plan 02 says “on failure fall through, leave `m_d3dPS` null” — **as written this will crash on the first failing char-select shader**, not tombstone. Requires a non-fatal wrapper or a `compilePixelShaderFromHlsl(..., bool fatalOnError)` parameter before ctor-time asset compiles. |
| **HIGH** | **VS–PS linkage unvalidated** — when `m_d3dPS` is non-null, StateCache binds it **without** per-VS signature matching (`:1107-1109`). Asset PSRC recompiled to `ps_4_0` may not match skeletal VS output register layout; history shows 483k `id=342` / 65k `id=343` errors from linkage mismatch. Plan gates on `id=342==0 && id=343==0` but doesn’t specify a mitigation if asset PS fails linkage (validate reflectively against bound VS? diagnostic bridge PS per D-09?). |
| **MEDIUM** | **PSRC preprocessing unspecified** — asset text starts with `//hlsl ps_2_0` (ShaderBuilder pattern at `PixelShaderProgramView.cpp:304-313`). Plan should confirm: pass full text to rewriter (comment line is fine), entry point remains `"main"`, profile is always `kPixelShaderProfile` (`"ps_4_0"` at `:73`) not asset `ps_2_0`. Worth an explicit acceptance note. |
| **MEDIUM** | **Compile timing** — recompile runs in `Direct3d11_PixelShaderProgramData` ctor during shader load; many failures × FATAL = boot failure. Even with non-fatal fix, first boot may be slow (cache warms on disk). |
| **LOW** | **Stale comment in StateCache** — `:1114-1117` still references “Variant T”; cosmetic only. |

### Suggestions

- **Before Task 1:** add `compilePixelShaderFromHlslNoFatal` or a `allowFailure` flag; asset ctor uses non-fatal path; install-time magenta fallback keeps FATAL (current behavior at `:622-623`).
- Add acceptance: **boot survives** even if some char-select PSRC shaders fail compile (magenta on those draws only).
- If `id=342/343` spike after recompile lands, document D-09 fallback: per-draw diagnostic bridge or temporary bind of per-VS dynamic PS when asset PS linkage fails (executor discretion — but plan should name the decision tree).
- Optionally log compile success/failure counts to `d3d11-debug.log` via InfoQueue (visible under explorer) for census cross-check.

### Risk Assessment

**HIGH** — Core fix path is right, but FATAL compile behavior and VS–PS linkage are the two most likely failure modes in execution.

---

## Plan 17-03 — Reflection + CHAR-02/03 + D-08

### Summary

Correctly targets the `:601` deferral site, mandates reflection-by-name (not D3D9 register folklore), preserves Iter-44A depth and Iter-44C-reverted blend blocks, and splits CHAR-02/03 validation with committed A/B pairs (D-07). D-08 RenderDoc confirmation is appropriately non-blocking for CHAR fixes.

### Strengths

- **D-04 scope discipline** — material + textureFactor only; census-driven expansion for scroll/fog/stencil.
- **Uses existing upload path** — `updatePS(2, ...)` / slot b2; no direct `PSSet*` from StaticShaderData (CODEX Q1).
- **Protected blocks explicit** — `:665-677` depth, `:686-689` reverted blend called DO-NOT-TOUCH.
- **D-08 ordering** — skinning confirmation before misattributing head weirdness to PS.
- **Separate A/B gates** — CHAR-02 and CHAR-03 not collapsed into one screenshot.

### Concerns

| Severity | Issue |
|----------|-------|
| **HIGH** | **No material/textureFactor source data in D3D11** — `Direct3d9_StaticShaderData` has `Pass::construct` resolving `pass.m_materialTag` → `shader.getMaterial()` and textureFactor tags (`Direct3d9_StaticShaderData.cpp:593-694`, struct at `.h:100-119`). **`Direct3d11_StaticShaderData` has no equivalent** — only `m_passVS`, `m_passPS`, `m_passStages` (`.h:128-129`). Plan 03 says “source semantics from D3D9 reference” at apply time but **does not task porting construct-time material/textureFactor resolution**. Reflection plumbing without source data still uploads zeros → gray eyes (CHAR-02 fail). |
| **HIGH** | **No DXBC blob for `D3DReflect` at apply site** — VS reflects at compile time from stored blob (`Direct3d11_VertexShaderData.cpp:611-615`, `m_bytecode` at `:520`). `Direct3d11_PixelShaderProgramData` stores only `m_d3dPS` (`.h:129`); no blob/reflection cache. Plan 03 modifies only `StaticShaderData`/`ConstantBuffer` — **nowhere tasks storing reflection layout at compile time** (Plan 02) or reloading from `.cso` cache by hash. |
| **MEDIUM** | **`PerMaterialCB` / `userConstants` stomp risk** — `Direct3d11.cpp:667-693` keeps a **function-local static** `s_perMaterialShadow` and flushes full struct on `setPixelShaderUserConstants`. Plan 03 uploading a separate local `PerMaterialCB` via `updatePS(2, ...)` can **overwrite `userConstants[]`** unless reads merge into one shared shadow (move shadow to `Direct3d11_ConstantBuffer` or read-modify-write). |
| **MEDIUM** | **`textureFactor` field placement unclear** — `Direct3d11_PerMaterialCB` has diffuse/specular/emissive + `userConstants[4]` (`Direct3d11_ConstantBuffer.h:58-64`); no dedicated `textureFactor`. D3D9 uses `PSCR_textureFactor` + `PSCR_textureFactor2` (two float4s). Plan says “add field or reuse userConstants per reflected layout” — needs explicit mapping from census constant names to struct fields. |
| **MEDIUM** | **Per-draw reflection cost** — reflecting every `apply()` is expensive; acceptable for beachhead but Plan should cache layout per PS hash/program pointer at compile time (in Plan 02). |
| **LOW** | **`ConstantBuffer.cpp` in files_modified** — task may not need edits if only calling `updatePS`; fine if reflection helper lives there. |
| **LOW** | **RenderDoc not installed** — research verified; fallback path mentioned but not specified (engine vertex diagnostics from `Direct3d11_StateCache.cpp:2338+` reflection logs?). |

### Suggestions

- **Add explicit sub-task (or Plan 02 extension):** at compile success in `Direct3d11_PixelShaderProgramData`, run `D3DReflect` on the compile blob and cache `PerMaterial` variable offsets/names on the program data object (mirror VS `m_reflectedInputs`/`m_reflectedOutputs` pattern).
- **Add explicit sub-task:** port minimal D3D9 `Pass::construct` material + textureFactor resolution into D3D11 — either per-pass cache in `Direct3d11_StaticShaderData` (mirror D3D9 `Pass` struct) or inline resolve in `apply()` from `m_shader` + `engPass->m_materialTag` / `m_textureFactorTag`. Without this, CHAR-02 cannot pass.
- **Unify slot-2 shadow:** centralize `Direct3d11_PerMaterialCB` shadow in `Direct3d11_ConstantBuffer` (or shared namespace); both `setPixelShaderUserConstants_impl` and `StaticShaderData::apply` merge into it before flush.
- Plan 03 Task 1 acceptance: add **“material upload non-zero when char-select eye pass has valid material tag”** debug log or RenderDoc cbuffer inspection — proves source data + reflection, not just code presence.
- For D-08 fallback without RenderDoc: document using existing VS reflection + `disableMultiStreamVertexBuffers=true` default (`ConfigClientGraphics.cpp:98`) as context in COMPARISON.md.

### Risk Assessment

**HIGH** — Reflection approach is architecturally correct, but plans omit the two prerequisites that make it functional (material source data + reflection input/cache).

---

## Cross-Cutting Analysis

### Wave dependency (census → recompile → reflection)

**Sound.** Census gates lane mix and anchor selection; CHAR-01 in Plan 02 can succeed with texture-only PS before material constants; CHAR-02/03 correctly land in Plan 03. No ordering inversion.

### Locked decisions compliance

| Decision | Status |
|----------|--------|
| D-01 engine census | Plan 01 |
| D-02 real asset tree | Plan 01 Task 3 |
| D-03 kept instrumentation | Plan 01 |
| D-04 reflection-driven, minimal feeds | Plan 03 — **missing source data port** |
| D-05 COMPARISON pairs | All plans |
| D-06 dual boot + id=342/343 | All plans |
| D-07 committed A/B | Plans 02/03 checkpoints |
| D-08 skinning confirm only | Plan 03 Task 2 |
| D-09 recompile-first | Plan 02 |
| Never re-assemble asm | Plan 02 conditional |
| Magenta tombstone | Plan 02 — **contradicted by FATAL in helper** |
| D3D9 byte-for-byte except store | Plan 01 |
| No early blend factors | Plan 03 explicit |
| No `_SRGB` flip | Plan 03 explicit |

### Live-tree line reference audit

| Claim | Verified |
|-------|----------|
| PSRC discard `2900-2901` | Yes |
| PS struct only `m_exe` `:680` | Yes |
| VS `m_text` `:427-428` | Yes |
| `compilePixelShaderFromHlsl` `:86-171` | Yes |
| PEXE reject ctor `:716-756` | Yes |
| Deferral `:601-605` | Yes (comment says “Phase 12”) |
| Iter-44A depth `:665-677` | Yes |
| Iter-44C reverted `:686-701` | Yes |
| SRV 0..7 `:711-737` | Yes |
| `PerMaterialCB` zero fields `:58-64` | Yes |
| VSPS program path `:1592-1602` | Yes |
| `kPixelShaderProfile = "ps_4_0"` | Yes (`:73`) |
| `D3D11_REWRITE_VERSION "20"` | Yes (`:151`) |

---

## Recommendations (priority order)

1. **Fix compile failure handling in Plan 02** — non-fatal asset compile path is mandatory; current helper will FATAL.
2. **Add D3D9-style material/textureFactor resolution to Plan 03** (or split 03-00 task) — reflection alone is insufficient.
3. **Cache PS reflection at compile time in Plan 02** — extend `Direct3d11_PixelShaderProgramData` with blob/reflection layout; Plan 03 consumes it.
4. **Unify `PerMaterialCB` shadow** — avoid slot-2 clobber between `Direct3d11.cpp` and `StaticShaderData`.
5. **Census file sink in Plan 01** — don’t rely on `DEBUG_REPORT_LOG_PRINT` alone.
6. **Document VS–PS linkage contingency** — if `id=342/343` return after asset bind, trigger D-09 diagnostic bridge or linkage validation step.

With those amendments, the plans are well-aligned with Phase 17 goals and the verified codebase. Without them, expect boot crashes at shader load, or CHAR-01 partial success with CHAR-02/03 blocked on zero constants despite “completed” reflection code.

---

## Consensus Summary

Both reviewers rate the **wave structure and decision compliance as sound** (census → recompile → reflection; D-01..D-09 honored; all cited file:line landmarks verified accurate against the live tree). Both rate **overall risk MEDIUM-HIGH** and agree the plans need targeted amendments **before execution** — not a re-architecture.

### Agreed Strengths (both reviewers)
- Wave dependency ordering is correct: census gates lane-mix/anchors; CHAR-01 (texture-only) before CHAR-02/03 (constants); no ordering inversion.
- Engine-instrumented census on shared `ConfigClientGraphics` (not `ConfigDirect3d11`) is the right integration point.
- Plan 01 ownership plan (dtor `:2783` + reload `:2832`) and dual-renderer boot gate (D-06) are appropriate for the only shared-code edit.
- Recompile-first (D-09), `D3D11_REWRITE_VERSION` bump, asm-never-reassembled, magenta-tombstone-in-concept, reflection-by-name (not D3D9 register folklore), blend-deferred, no `_SRGB` flip — all honored.
- Live-tree line-reference audit: every cited landmark verified accurate (PSRC discard `:2895-2901`, PS struct `m_exe`-only `:680`, `compilePixelShaderFromHlsl :86`, PEXE reject `:748`, deferral `:601`, SRV binding `:711`, `PerMaterialCB` zero fields `:58-64`, rewrite version `"20"` `:151`).

### Agreed Concerns (raised by BOTH — highest priority, treat as HIGH)
1. **`compilePixelShaderFromHlsl()` is FATAL on failure, not non-fatal.** It calls `FATAL`/`FATAL_DX_HR` on `D3DCompile`/`CreatePixelShader` failure (Codex `:238`; Cursor `:241-242`,`:259-260`); only empty input returns false. Plan 02's "compile failure falls through to magenta tombstone" is FALSE as written — **first failing char-select shader crashes the boot.** FIX: add a non-fatal variant (`tryCompilePixelShaderFromHlsl` / `allowFailure` flag) for asset compiles; install-time path keeps FATAL.
2. **No DXBC blob retained → `D3DReflect` has no input.** `Direct3d11_PixelShaderProgramData` stores only `m_d3dPS`, not the compiled bytecode; `D3DReflect(m_d3dPS)` is not possible. Plan 03 reflects at the `:601` apply site but nothing in Plan 02 retains/exposes the blob or caches the reflection layout at compile time. FIX: Plan 02 stores the DXBC blob / extracted reflection metadata on the program-data object (mirror the VS `m_reflectedInputs/outputs` pattern) and exposes accessors; Plan 03 consumes that. Likely needs `Direct3d11_PixelShaderProgramData.h` in `files_modified`.
3. **`PerMaterialCB` slot `b2` is already owned by `setPixelShaderUserConstants_impl()` (`Direct3d11.cpp:660-693`, function-local static shadow, full-struct flush).** Plan 03 uploads a separate `PerMaterialCB` via `updatePS(2,...)` but does NOT modify `Direct3d11.cpp` → material upload and `userConstants[]` clobber each other. FIX: centralize the slot-2 shadow (read-modify-write into one shared struct); add `Direct3d11.cpp` to `files_modified`.
4. **VS↔PS linkage / input-signature mismatch.** StateCache binds `m_d3dPS` without per-VS signature matching (`Direct3d11_StateCache.cpp:1107-1109`); recompiled `ps_4_0` may not match skeletal VS output layout — the historical id=342/343 flood. Plan gates on `id=342==0 && id=343==0` but specifies no mitigation/decision-tree if linkage fails. FIX: add an explicit linkage-validation step (reflect PS input semantics vs bound VS output) and name the D-09 diagnostic-bridge fallback as the contingency.
5. **Census output observability.** `DEBUG_REPORT_LOG_PRINT` may be invisible under explorer launch / not machine-readable; the gating HLSL:asm artifact could be lost. FIX: add a dedicated file sink (`stage/psrc-census.tsv`/`.csv`, one row per shader + aggregate).

### Divergent / Single-Reviewer Views (worth investigating)
- **Cursor (HIGH), Codex implicit:** No material/textureFactor **source data** in D3D11 — `Direct3d11_StaticShaderData` has no equivalent of D3D9 `Pass::construct` resolving `m_materialTag`/textureFactor tags (`Direct3d9_StaticShaderData.cpp:593-694`). Even with blob+reflection wired, the upload is zeros → **gray eyes, CHAR-02 fails.** Plan 03 must task porting the construct-time tag resolution, not just the reflection plumbing. *(This is the most consequential single-reviewer finding — it means fixing #2/#3 alone is still insufficient for CHAR-02/03.)*
- **Codex (HIGH), Cursor not raised:** HLSL-rewrite cbuffer contract uncertainty — the assumption of a reflected cbuffer named `PerMaterial` only holds if `Direct3d11_HlslRewrite.cpp` (~`:797`) actually rewrites asset-PS constants into that named cbuffer/binding; existing rewrite layout may not match `PerMaterialCB`. Confirm + document the rewrite contract (cbuffer name, slot, var names, offsets) before wiring.
- **Codex (HIGH), Cursor (noted as conditional):** The asm secondary-lane clause is too broad — "port asm→HLSL by hand if census finds asm" could explode Plan 02 scope. Codex recommends making it an explicit stop-condition + separate `17-02B` plan rather than in-scope work.
- **Divergence on PSRC read API:** Cursor says reading by chunk-length + raw bytes is "verified compatible" with `insertChunkString` (which writes `strlen+1` incl. null); Codex says don't assume raw — specify the exact API (`Iff::read_string`) and terminator handling. → Plan 01 should pin the exact read path explicitly.
- **Concrete LOW bugs (Codex):** `execution_context` paths point at `D:/Code/swg-client/...` not `swg-client-v2/...`; `requirements: [CHAR-01/02/03]` on Plan 01 overstates (it *enables*, doesn't *satisfy*); add a PSRC max-length sanity cap (corrupt asset → unbounded alloc).

### Verdict
**~5 convergent HIGH concerns + 3 high-value single-reviewer findings.** The plans are directionally correct and decision-compliant, but several tasks assume APIs/runtime state the live tree does not expose (fatal compile, missing blob retention, contended slot-b2 shadow, absent material source data). Recommend a **`--reviews` replan pass** to fold these in before execution — the cost of fixing now is trivial vs. discovering a boot crash or zero-constant gray-eyes result mid-execution.
