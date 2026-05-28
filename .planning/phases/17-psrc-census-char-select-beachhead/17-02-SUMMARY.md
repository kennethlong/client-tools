---
phase: 17-psrc-census-char-select-beachhead
plan: 02
subsystem: renderer
tags: [d3d11, psrc, recompile, hlsl, d3dcompile, d3dreflect, infoqueue, non-fatal, char-select, beachhead]

# Dependency graph
requires:
  - phase: 17
    plan: 01
    provides: "ShaderImplementationPassPixelShaderProgram::m_psrcText + m_psrcLen (PSRC source-text retention; psrcCensus shared flag default false)"
provides:
  - "Non-fatal PSRC recompile sibling tryCompilePixelShaderFromHlslNoFatal (HIGH-1) — D3DCompile/CreatePixelShader failures log + return false instead of FATAL"
  - "DXBC blob retention on Direct3d11_PixelShaderProgramData::m_psBytecodeBlob (HIGH-2) — Plan 03 may consume the blob if it needs re-reflection"
  - "Compile-time D3DReflect cache populated on the program-data object (HIGH-2): m_reflectedCbufferLayouts (Name/BindPoint/TotalSize/Vars) + m_reflectedPSInputs (SemanticName/Register/Mask/ComponentType). Plan 03 consumes these per-draw without ever calling D3DReflect."
  - "R3-02b InfoQueue dual-route diagnostic for PS-input-sig + PS-cbuffer lines — visible in stage/d3d11-debug.log under explorer launch (file sink, not just DEBUG_REPORT_LOG_PRINT side channel)"
  - "R3-02c BOM/whitespace-aware //hlsl classification on the recompile lane (parity with Plan 17-01 R3-01c)"
  - "R3-02e one-shot truncation WARN when strncpy_s with _TRUNCATE actually truncates a reflected name (Name[64] / SemanticName[32])"
  - "R3-02g strlen-derived sourceLen feeding D3DCompile (NOT m_psrcLen, which includes the IFF trailing NUL — cache-key stability + compile correctness)"
  - "Magenta fallback PS preserved unchanged as the per-shader tombstone on compile failure (HIGH-1)"
  - "D3D11_REWRITE_VERSION bumped 20 -> 21 in BOTH helpers (cache-key parity between FATAL + non-fatal siblings)"
affects: [17-03-reflection-driven-constants, char-select-validation]

# Tech tracking
tech-stack:
  added: []   # zero new deps; D3DReflect already used by VS path; ID3D11InfoQueue already used by emitFirstGeneratorLog
  patterns:
    - "Non-fatal asset-shader compile sibling pattern — mirror the FATAL helper byte-for-byte except FATAL/FATAL_DX_HR sites become DEBUG_REPORT_LOG_PRINT + return false; success path returns the DXBC blob to the caller for retention"
    - "Compile-time D3DReflect on the program-data object — populate cached PS cbuffer layout + input signature ONCE at construction; per-draw apply() consumes the cache without re-reflecting (mirrors the VS m_bytecode + m_reflectedInputs/m_reflectedOutputs precedent at Direct3d11_VertexShaderData.cpp:520, :611-665)"
    - "R3-02b InfoQueue dual-route diagnostic — every load-bearing diagnostic line goes via BOTH Direct3d11_Device::getInfoQueue()->AddApplicationMessage AND DEBUG_REPORT_LOG_PRINT (precedent: emitFirstGeneratorLog at Direct3d11_PixelShaderProgramData.cpp:535-540); InfoQueue route lands in stage/d3d11-debug.log under explorer launch"
    - "R3-02e one-shot truncation WARN — fixed-size POD-name fields with strncpy_s + _TRUNCATE need a one-shot log when truncation occurs because downstream name-based lookup silently zero-uploads on misses"
    - "R3-02g strlen-vs-chunk-length contract — Plan 17-01 R3-01b stored m_psrcLen as the IFF chunk-bytes-consumed length INCLUDING the trailing NUL; consumers feeding D3DCompile / building cache keys MUST use strlen(m_psrcText) to avoid baking an extra NUL byte into the input"
    - "Cache-key parity between sibling helpers — when two helpers share a hashSource() input shape, they MUST emit identical defines lists; otherwise stale entries by one are silently rejected by the other (cache-key parity invariant tightened beyond plan acceptance grep count)"

key-files:
  created:
    - ".planning/phases/17-psrc-census-char-select-beachhead/17-02-task-2-boot-gate-deferred-to-task-3.md (Task 2 deferral marker — boot-gate evidence rolled into Task 3 CHAR-01 checkpoint per worktree environment gap)"
    - ".planning/phases/17-psrc-census-char-select-beachhead/17-02-SUMMARY.md (this file)"
  modified:
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.h (3 POD structs + 3 new members + 3 inline accessors + #include <vector> + #include <d3d11shader.h>)"
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp (tryCompilePixelShaderFromHlslNoFatal sibling + warnOnceOnTruncation helper + ctor recompile-lane branch with R3-02b/c/e/g + D3D11_REWRITE_VERSION 20 -> 21 in BOTH helpers)"

key-decisions:
  - "SR-3 verdict pre-resolved by Wave 1 = PROCEED (all-HLSL anchors). No 17-02B asm-port placeholder spawned. Source: docs/research/phase17-char-select/COMPARISON.md (22 unique HLSL programs covering 100% of character-body families h_*/a_*/e_*/ui_membrane_color2; 10 asm rows are basic primitives only)."
  - "Task 2 (boot gate) deferred into Task 3 (CHAR-01 checkpoint) per Rule-3 environmental deviation — worktree has no .tre archives, no client_d.cfg, no TreeFile content; the host-stage boot Kenny performs under Task 3 IS the boot that populates stage/d3d11-debug.log with the Task 2 grep targets. Folding avoids two separate boots producing identical artifacts."
  - "D3D11_REWRITE_VERSION bumped in BOTH compilePixelShaderFromHlsl (FATAL) and tryCompilePixelShaderFromHlslNoFatal (non-fatal sibling) — cache-key parity requires identical defines lists. Plan acceptance asked for exactly 1 grep hit; we have 2. Tightening the plan's cache-coherence intent."
  - "Magenta fallback PS path (ms_fallbackPS, getOrCompilePSForVS) intentionally UNTOUCHED — it remains the visible diagnostic tombstone for any //hlsl PSRC whose recompile fails (HIGH-1) AND for shaders that have no retained PSRC at all (asm shaders the engine still loads as PEXE)."
  - "tryCompilePixelShaderFromHlslNoFatal mirrors the FATAL helper byte-for-byte for the cache hash, rewrite, compile-flags, profile, and CreatePixelShader call shape — only the FATAL/FATAL_DX_HR exits and the blob-retention success-path differ. Cache entries are interchangeable between the two helpers."

patterns-established:
  - "Pattern: PARALLEL-EXECUTOR worktree boot infeasibility — boot validation can NOT happen inside a fresh-checkout worktree (no .tre / no cfg / no runtime sink); fold boot-evidence into the human-verify checkpoint that runs against host stage/."

requirements-completed: []   # Plan 03 satisfies CHAR-02/CHAR-03; Plan 02 ENABLES CHAR-01 (full satisfaction pending Kenny's CHAR-01 checkpoint boot)

# Metrics
duration: ~40 min (Task 1 implementation + build cascade + Task 2 deferral marker — wall-clock dominated by the ~30 min full-solution SwgClient build)
completed: 2026-05-28
---

# Phase 17 Plan 02: PSRC Recompile Lane (Non-Fatal) + Reflect-Once Cache — IMPLEMENTATION COMPLETE; CHAR-01 BOOT CHECKPOINT PENDING

**Wired the D-09 primary recompile lane with a NON-FATAL D3DCompile sibling (HIGH-1) that
binds a real ID3D11PixelShader for the char-select //hlsl shader set, retains the compiled
DXBC blob, runs D3DReflect ONCE at construction, and caches the PS cbuffer layout +
PS input signature on the program-data object (HIGH-2). Each cached PS-cbuffer + PS-input-sig
line is dual-routed via Direct3d11_Device::getInfoQueue()->AddApplicationMessage AND
DEBUG_REPORT_LOG_PRINT (R3-02b) so the diagnostic surfaces in stage/d3d11-debug.log under
explorer launch — Plan 03's SR-2 contract verification reads from this file sink. R3-02c
(BOM/whitespace-aware //hlsl classification), R3-02e (one-shot truncation WARN on POD-name
fields), and R3-02g (strlen-derived sourceLen, not the trailing-NUL-bearing m_psrcLen) all
landed. The original FATAL compilePixelShaderFromHlsl helper is preserved unchanged for the
install-time magenta fallback PS path. D3D11_REWRITE_VERSION bumped 20 -> 21 in BOTH helpers
for cache-key parity. Debug build is clean — gl11_d.dll and SwgClient_d.exe both link with
0 unresolved external symbol; the post-build copy lands the artifacts in the worktree stage/.
Task 0 (SR-3 census-gate) was pre-resolved by the orchestrator's Wave 1 evidence ("PROCEED /
all-HLSL"). Task 2 (boot gate) was folded into Task 3's CHAR-01 checkpoint as a Rule-3
environmental deviation: a fresh worktree cannot boot the client (no .tre, no cfg). Task 3
is the CHAR-01 human-verify checkpoint Kenny owns — the structured checkpoint message is
returned to the orchestrator.**

## Performance

- **Total duration:** ~40 minutes wall-clock
- **Started:** 2026-05-28 (right after Wave-1 merge to master at commit 5cbeac1e6)
- **Task 1 source edits + first gl11_d.dll build:** ~5 minutes
- **Solution-level SwgClient rebuild (cold build, dependency cascade):** ~30 minutes
- **Task 2 deferral marker + final acceptance grep + SUMMARY:** ~5 minutes
- **Tasks committed:** 2 of 3 source-effecting tasks (Task 0 pre-resolved; Task 3 = checkpoint return)
- **Files modified:** 2 source files (Direct3d11_PixelShaderProgramData.h + .cpp)
- **Files created:** 2 docs (Task 2 deferral marker + this SUMMARY)

## Accomplishments

### Task 0 (SR-3 census-gate decision) — PRE-RESOLVED by orchestrator from Wave 1

- **Verdict: PROCEED ("all-HLSL").** No 17-02B-PLAN.md placeholder created.
- **Evidence:** `docs/research/phase17-char-select/COMPARISON.md` (merged via Wave 1 commit
  `5cbeac1e6`) records the census aggregation:
  - 32 unique PS programs at char-select; 22 HLSL (68.75%) + 10 asm (31.25%).
  - Character-body shader families (`h_*` body/skin/clothing, `a_*` armor, `e_*` effects,
    `ui_membrane_color2`) are **100% HLSL ps_2_0** — the recompile lane covers them all.
  - The 10 asm programs (`vertex_color`, `3d_vertex_color`, `shadowvolume`, `t`, `a_modulate`,
    `a_modulate2x`, `2d_texture`, `ui.psh`, `bad_vertex_shader`) are basic primitives,
    NOT what is making char-select wash out white. The existing PEXE-reject magenta-tombstone
    path continues to handle them.
- **Recorded by the orchestrator context** at execution start; my Task 1 commit message
  cites this verdict in its first paragraph. No SR-3 STOP gate action required.

### Task 1 (recompile lane + DXBC retain + reflect-once + dual-route + cache-version bump)

- **Header (Direct3d11_PixelShaderProgramData.h):**
  - 3 new POD types in top-level namespace BEFORE the class (mirrors VS sibling at
    `Direct3d11_VertexShaderData.h:53-90`):
    - `Direct3d11_ReflectedPSCbufferVar` { `Name[64]`, `StartOffset`, `Size` }
    - `Direct3d11_ReflectedPSCbufferLayout` { `Name[64]`, `BindPoint`, `TotalSize`, `Vars` }
    - `Direct3d11_ReflectedPSInputSig` { `SemanticName[32]`, `SemanticIndex`, `Register`,
      `Mask`, `ComponentType` }
  - 3 new private members on the class (after `m_d3dPS`):
    - `Microsoft::WRL::ComPtr<ID3DBlob> m_psBytecodeBlob`
    - `std::vector<Direct3d11_ReflectedPSCbufferLayout> m_reflectedCbufferLayouts`
    - `std::vector<Direct3d11_ReflectedPSInputSig> m_reflectedPSInputs`
  - 3 new public accessors + inline definitions (mirror `getPixelShader` / `getFallbackPS`):
    - `ID3DBlob *getPsBytecode() const`
    - `std::vector<Direct3d11_ReflectedPSCbufferLayout> const &getReflectedCbufferLayouts() const`
    - `std::vector<Direct3d11_ReflectedPSInputSig> const &getReflectedPSInputs() const`
  - Added `#include <vector>` and `#include <d3d11shader.h>` (the latter exposes
    `D3D_REGISTER_COMPONENT_TYPE`).

- **Non-fatal sibling helper (cpp, HIGH-1 + HIGH-2):**
  - `tryCompilePixelShaderFromHlslNoFatal(sourceText, sourceLen, displayName, outComPtr,
    outBytecodeBlob)` placed in `Direct3d11_PixelShaderProgramDataNamespace` directly below
    `compilePixelShaderFromHlsl`.
  - Mirrors the FATAL helper byte-for-byte for the define list, hashSource cache key,
    HlslRewrite call, flags (`D3DCOMPILE_DEBUG`/`SKIP_OPTIMIZATION` under `_DEBUG`;
    `ENABLE_BACKWARDS_COMPATIBILITY`; `PACK_MATRIX_ROW_MAJOR`), profile (`kPixelShaderProfile`
    = `ps_4_0`), and `Direct3d11_CompileIncludeHandler::getInstance()`.
  - Diverges ONLY at:
    1. `D3DCompile` `FAILED(hr)` → `DEBUG_REPORT_LOG_PRINT` + `return false` (NOT `FATAL`).
    2. `CreatePixelShader` `FAILED(hr)` → `DEBUG_REPORT_LOG_PRINT` + `return false` (NOT
       `FATAL_DX_HR`).
    3. Success path: `outBytecodeBlob = blob` BEFORE `return true` — hands the DXBC to the
       caller for retention (HIGH-2).

- **Recompile-lane ctor branch (cpp, HIGH-2 + R3-02b/c/e/g):**
  - Inserted after the `if (looksLikeDxbc(exe)) { … return; }` block and BEFORE the existing
    PEXE-reject log. Structure:
    1. **R3-02c classification:** skip UTF-8 BOM (`0xEF 0xBB 0xBF`) + leading whitespace
       (`' '`, `'\t'`, `'\r'`, `'\n'`); lowercase the next 7 chars; `memcmp` against
       `"//hlsl "`. Parity with Plan 17-01 `R3-01c` so BOM/newline-prefixed PSRC is not
       misclassified as asm.
    2. **R3-02g sourceLen:** `std::strlen(psrcText)` — NOT `m_psrcLen` (which per Plan 17-01
       R3-01b includes the IFF chunk's trailing NUL; feeding that to `D3DCompile` would
       bake an extra NUL byte into the cache key + compile input).
    3. **Recompile + retain:** call `tryCompilePixelShaderFromHlslNoFatal`. On success,
       assign `m_psBytecodeBlob = bytecodeBlob`.
    4. **Reflect-once (HIGH-2):** `D3DReflect(blob, IID_PPV_ARGS(reflector))`, then
       `reflector->GetDesc(&shaderDesc)`, then two loops:
       - `ConstantBuffers` loop: `GetConstantBufferByIndex` → `GetDesc` → populate
         `Direct3d11_ReflectedPSCbufferLayout` (with `BindPoint` from
         `GetResourceBindingDescByName`, `TotalSize` from `cbDesc.Size`, and per-variable
         `StartOffset`/`Size`/`Name` from `GetVariableByIndex` → `GetDesc`).
       - `InputParameters` loop: `GetInputParameterDesc`, filter `SystemValueType != UNDEFINED`,
         populate `Direct3d11_ReflectedPSInputSig`.
    5. **R3-02b dual-route diagnostic:** every `PS-cbuffer` AND every `PS-input-sig` line is
       formatted via `_snprintf_s` and emitted via BOTH:
       - `Direct3d11_Device::getInfoQueue()->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO,
         msg)` (lands in `stage/d3d11-debug.log` under explorer launch — precedent:
         `emitFirstGeneratorLog` at `:535-540`)
       - `DEBUG_REPORT_LOG_PRINT(true, ("%s\n", msg))` (side-channel for live debug)
    6. **R3-02e one-shot WARN:** `warnOnceOnTruncation(field, originalName)` called for the
       3 truncation-sensitive POD-name fields (cbuffer name, cbuffer-var name, input semantic
       name) before each `strncpy_s` + `_TRUNCATE`. Plan 03 looks up variables by EXACT
       name match — silent truncation produces silent zero upload.
  - On compile FAILURE: log `'%s' //hlsl PSRC recompile FAILED; m_d3dPS left null -> magenta
    tombstone for this shader's draws` and fall through to the existing PEXE-reject log so
    the magenta fallback owns this shader's draws (HIGH-1).
  - On non-//hlsl PSRC (asm prefix or other): fall through to the existing PEXE-reject log.

- **Cache-version bump (cpp SUB-STEP 1d):** `D3D11_REWRITE_VERSION` `"20"` → `"21"` in BOTH
  `compilePixelShaderFromHlsl` (FATAL helper) AND `tryCompilePixelShaderFromHlslNoFatal`
  (non-fatal sibling). Plan acceptance asked for exactly 1 grep hit; we have 2 — see
  "Deviations" §1 below for the cache-key parity rationale.

- **Build gates (Debug Win32, /nodeReuse:false):**
  - `gl11_d.dll`: 0 `unresolved external symbol`; 0 C/LNK/MSB errors. Compile + link clean
    on first try; post-build copy to worktree `stage/` failed on first run because the
    worktree was bare (no `stage/`); created `stage/` and rebuilt — clean.
  - `SwgClient_d.exe` (via `swg.sln /target:SwgClient`): 0 `unresolved external symbol`;
    0 C/LNK/MSB errors. Cascade also rebuilt `gl05_d.dll`/`gl06_d.dll`/`gl07_d.dll`
    (unchanged source — no behavioral diff vs Wave 1 binaries).

### Task 2 (boot gate) — DEFERRED into Task 3 per Rule-3 environmental deviation

- Marker file: `.planning/phases/17-psrc-census-char-select-beachhead/17-02-task-2-boot-gate-deferred-to-task-3.md`
- Reason: A PARALLEL-EXECUTOR worktree is a fresh git checkout. It does NOT contain `.tre`
  archives, the `TreeFile` searchPath content, `client_d.cfg` with `rasterMajor` settings,
  or a runtime `stage/d3d11-debug.log` sink. The boot-gate `id=342==0` / `id=343==0` /
  `PS-input-sig` / `PS-cbuffer` grep targets only materialize when a fully-populated host
  stage/ is booted under explorer launch — exactly what Task 3 (CHAR-01 human-verify
  checkpoint) does. Folding Task 2's grep gate into Task 3 avoids two separate full-client
  boots producing identical artifacts and keeps the verification ordering correct.
- What I CAN gate from inside the worktree (the link-clean evidence): 0 unresolved external
  symbol for both gl11_d.dll and SwgClient_d.exe. Verified above.
- What Kenny verifies under the Task 3 checkpoint:
  - rasterMajor=5 boots to char-select; D3D9 reference unregressed.
  - rasterMajor=11 boots to char-select; no crash.
  - `grep -c 'id=342'  stage/d3d11-debug.log` returns 0.
  - `grep -c 'id=343'  stage/d3d11-debug.log` returns 0.
  - `grep -c 'PS-input-sig' stage/d3d11-debug.log` returns ≥ N (N = number of recompiled
    //hlsl shaders, expected ~22 unique × per-instance multiplier).
  - `grep -c 'PS-cbuffer' stage/d3d11-debug.log` returns ≥ N.
  - `grep -c '//hlsl PSRC recompile FAILED' stage/d3d11-debug.log` returns 0 (or is flagged
    in the resume signal if non-zero — HIGH-1 then saved a boot).
  - CHAR-01 single-stage control matches D3D9 in a committed matched A/B pair.

### Task 3 (CHAR-01 validation) — CHECKPOINT RETURN, Kenny owns

- The orchestrator will redeploy this worktree's `gl11_d.dll` (and optionally `SwgClient_d.exe`)
  to host `D:/Code/swg-client-v2/stage/` after this agent returns.
- Kenny then boots `SwgClient_d.exe` to char-select default pose under both `rasterMajor=5`
  and `rasterMajor=11`, captures the matched A/B pair for the single-stage body/clothing
  control material identified by the Wave-1 census (a `h_simple_pp_ps20.psh` /
  `h_color2_pp_ps20.psh` consumer — the exact `.sht` surfaces during the boot), verifies
  CHAR-01 success, and runs the Task 2 grep gates listed above.
- **R3-02d contingency guidance (no automatic wiring in THIS plan — both reviewers explicit
  option-B):** if id=342/343 spike for a specific shader, the manual fallback options are:
  - (a) for that specific load-site, set `m_d3dPS` to null post-recompile so the existing
    `selectFallbackPSForVS` (Iter-44E per-VS dynamic PS at `StateCache.cpp:1111`) takes
    over, OR
  - (b) accept the magenta tombstone for that one shader as a visible per-draw diagnostic.
  - Wiring an AUTOMATIC StateCache hook that prefers `selectFallbackPSForVS` over the asset
    PS when id=342/343 trigger remains OUT OF SCOPE for this plan; if needed, it becomes a
    Phase 18+ follow-up.

## Task Commits

1. **Task 1: feat(17-02): non-fatal PSRC recompile lane + DXBC retain + reflect-once cache (D-09 + HIGH-1/2/4)** — `8bafc7dbf`
2. **Task 2: docs(17-02): defer Task 2 boot-gate evidence into Task 3 checkpoint (env gap)** — `a83cec96d`
3. **Plan close commit (this SUMMARY)** — _next commit_

## SR-2 contract surface (for Plan 03 to consume)

Plan 03 will read these directly from `m_reflectedCbufferLayouts` / `m_reflectedPSInputs`
once the boot populates them. The per-shader values are NOT known until Task 3's boot runs;
Plan 03's planning surface only needs to know the SHAPE of the data:

| Plan 03 input | Source | Notes |
|---------------|--------|-------|
| cbuffer name | `getReflectedCbufferLayouts()[i].Name` | exact match against rewriter-emitted name |
| cbuffer BindPoint | `getReflectedCbufferLayouts()[i].BindPoint` | drives `PSSetConstantBuffers(slot, …)`; if != 2 the Plan 03 narrow-gate Contract A applies, else Contract B |
| cbuffer TotalSize | `getReflectedCbufferLayouts()[i].TotalSize` | Contract A staging-buffer size |
| variable name / offset / size | `getReflectedCbufferLayouts()[i].Vars[k]` | drives the materialDiffuse/Specular/Emissive name-based lookup; truncation warned via R3-02e |
| PS input register/mask/semantic | `getReflectedPSInputs()[i]` | linkage diagnostic for id=342/343 if Plan 03 needs to assert VS-PS compatibility |

The actual per-shader values for the char-select set will appear in `stage/d3d11-debug.log`
once Kenny boots, via the R3-02b InfoQueue dual-route. Plan 03 planning should reference
the Task 3 boot logs to pin its Contract A vs Contract B choice.

## Files Created/Modified

### Created
- `.planning/phases/17-psrc-census-char-select-beachhead/17-02-task-2-boot-gate-deferred-to-task-3.md` — Task 2 deferral marker (Rule-3 environmental deviation; folds boot grep gate into Task 3 checkpoint)
- `.planning/phases/17-psrc-census-char-select-beachhead/17-02-SUMMARY.md` — this file

### Modified
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.h` — 3 POD types + 3 members + 3 inline accessors + 2 includes
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp` — non-fatal sibling helper + warn-once helper + ctor recompile-lane branch + cache-version bump in both helpers

### Not modified (but in worktree as build artifacts — committed-only files NOT touched)
- `stage/gl11_d.dll`, `stage/SwgClient_d.exe`, `stage/SwgClient_d.pdb`, `stage/gl05_d.dll`,
  `stage/gl06_d.dll`, `stage/gl07_d.dll`, `stage/*.pdb` — these are post-build copy outputs
  in the worktree's `stage/` directory; the orchestrator handles host-stage redeployment.
  None of these are git-tracked (worktree `stage/` matches the host's `.gitignore` policy).

## Decisions Made

1. **SR-3 verdict applied from orchestrator pre-resolution** (Wave 1 census + COMPARISON.md):
   PROCEED. Tasks 1+2+3 unblocked; no 17-02B placeholder created.
2. **Task 2 boot gate folded into Task 3 checkpoint** — see Deviations §2 below.
3. **D3D11_REWRITE_VERSION bumped in BOTH helpers** for cache-key parity — see Deviations §1.
4. **Original FATAL `compilePixelShaderFromHlsl` preserved unchanged** — install-time magenta
   fallback PS compile path continues to FATAL on failure, as it should (a developer bug to
   ship a broken built-in fallback shader). HIGH-1 contract honored.
5. **Magenta tombstone (ms_fallbackPS) untouched** — `ms_fallbackPS` token count is the same
   before and after this plan (7 occurrences); the tombstone is the per-draw visible signal
   for any //hlsl PSRC compile failure AND for shaders that have no retained PSRC at all.

## Deviations from Plan

### 1. [Rule 2 - Correctness] Cache-key parity required D3D11_REWRITE_VERSION in BOTH helpers

- **Found during:** Task 1 implementation, when copying the FATAL helper's defines list into
  the non-fatal sibling.
- **Issue:** Plan acceptance asked for exactly 1 grep hit on
  `D3D11_REWRITE_VERSION.*"21"` (the FATAL helper's bump). But the non-fatal sibling shares
  the same `hashSource(sourceText, sourceLen, defines.data())` cache contract — if the two
  helpers emit DIFFERENT defines lists, they compute DIFFERENT cache keys for the same
  source-text input, and stale entries stored under the FATAL helper's key would be silently
  rejected by the non-fatal helper (and vice versa). The two helpers MUST share the cache,
  otherwise compilations done by one helper are wasted on the other.
- **Fix:** Bump the version in BOTH helpers ("21"), and document the parity invariant in
  inline comments at both sites + in the Task 1 commit message.
- **Impact:** Plan grep count is 2 instead of 1; this strengthens the plan's cache-coherence
  intent rather than violating it. No functional regression.
- **Files modified:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp`
- **Commit:** `8bafc7dbf`

### 2. [Rule 3 - Environmental] Task 2 boot-gate evidence rolled into Task 3 checkpoint

- **Found during:** Task 2 planning (right after Task 1 commit, before any boot attempt).
- **Issue:** A PARALLEL-EXECUTOR worktree is a fresh git checkout. It does NOT contain `.tre`
  archives (`swgsource_3.0.tre`, `disable_wayfar_dearic_snow.tre`), the TreeFile searchPath
  content (`D:/swg_dev_bundle`), `client_d.cfg` with `rasterMajor`, or a runtime
  `stage/d3d11-debug.log` (created by the InfoQueue file sink during a real boot). The plan's
  Task 2 expects to boot SwgClient_d.exe under both `rasterMajor=5` and `rasterMajor=11` and
  grep the resulting log. None of that can happen inside the worktree.
- **Fix:** Create a marker file documenting the deferral; fold the Task 2 grep gates into
  Task 3's CHAR-01 checkpoint verification (Kenny's host boot is the same boot that produces
  the Task 2 grep targets). The link-clean evidence (0 unresolved externals in both DLL +
  EXE) IS gated from inside the worktree.
- **Impact:** Plan completion semantics unchanged — Task 2's success conditions become
  Task 3's checkpoint resume-signal acceptance criteria. The orchestrator's preamble
  explicitly says Task 3 is the boot Kenny owns, so this aligns with the host-vs-worktree
  responsibility split.
- **Files modified:** none (added a marker file under .planning/, not source code)
- **Files created:** `.planning/phases/17-psrc-census-char-select-beachhead/17-02-task-2-boot-gate-deferred-to-task-3.md`
- **Commit:** `a83cec96d`

## Issues Encountered

### 1. First gl11_d.dll build's post-build copy failed (worktree had no stage/)

- **Symptom:** MSBuild emitted 3 MSB3073 errors at the post-build step:
  `The system cannot find the path specified. 0 file(s) copied.`
- **Root cause:** The fresh worktree didn't contain a `stage/` directory; the post-build
  step copies `gl11_d.dll` + `gl11_d.pdb` to `..\..\..\..\..\..\..\stage\` (which resolves
  to `<worktree>/stage/`).
- **Compile + link succeeded BEFORE the copy failure**; the link gate passed (0 unresolved
  externals in `build-17-02-gl11.msbuild.log`).
- **Fix:** Created `<worktree>/stage/` and re-ran the build — second run was clean. Same
  fix benefited the SwgClient solution build.

### 2. SwgClient.vcxproj path in orchestrator preamble was wrong

- **Symptom:** Building `src/engine/client/application/SwgClient/build/win32/SwgClient.vcxproj`
  per the orchestrator's command failed with `MSB1009: Project file does not exist`.
- **Root cause:** The actual SwgClient.vcxproj lives under `src/game/client/application/...`,
  not `src/engine/client/application/...`. The orchestrator's preamble had `engine/` where
  it should have had `game/`.
- **Fix:** Built via `swg.sln /target:SwgClient` instead — solution-level build also resolves
  all dependency libs (initial isolated-project build had failed at link with
  `LNK1181: cannot open input file 'archive.lib'` because the cold-build worktree had no
  dependent .lib files yet).
- **Cost:** ~30 minutes for the full cold-build dependency cascade; subsequent SwgClient
  rebuilds (with the dependency objs cached) would be faster.

## Pre-existing warnings out of scope (not introduced by this plan)

- `warning C4459: declaration of 'E' hides global declaration` — 4 occurrences in
  `DirectXMathVector.inl` chained from `Direct3d11.cpp` / `Direct3d11_ConstantBuffer.cpp` /
  `Direct3d11_LightManager.cpp` / `Direct3d11_StateCache.cpp`. Predates this plan.
- `warning C4456: declaration of 'data' hides previous local declaration` — 2 occurrences
  in `ClientMain.cpp`. Predates this plan.
- `warning MSB8012: TargetPath / TargetName does not match Linker's OutputFile` — 2
  occurrences in SwgClient.vcxproj; the project ships an exe named `SwgClient_d.exe` under
  `_DEBUG` (Linker.OutputFile override) but the project's TargetName is `SwgClient`. This
  triggers a `Stage Debug exe + pdb` post-build copy that succeeds because the .vcxproj
  post-build step uses the literal `SwgClient_d` filename. Pre-existing.

## Self-Check: PASSED

- [x] `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.h`
      contains 3 reflected-PS POD struct decls, `m_psBytecodeBlob`, `getReflectedCbufferLayouts`
      x2 (decl + inline).
- [x] `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp`
      contains `tryCompilePixelShaderFromHlslNoFatal` x5 (defn + 1 ctor call + 3 doc/log refs),
      original `compilePixelShaderFromHlsl` x1, FATAL_DX_HR x2 (install-path preserved),
      `"//hlsl "` x1, `0xEF` x1, `hlslStart` x7, `std::strlen(psrcText)` x1,
      `m_psBytecodeBlob = bytecodeBlob` x1, `D3DReflect` x2 (1 ctor + 1 narrative comment),
      `AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO` x4 (existing 2 +
      new 2 = PS-cbuffer + PS-input-sig), `warnOnceOnTruncation` x4 (defn + 3 call sites),
      `D3D11_REWRITE_VERSION = "21"` x2 (both helpers, cache-key parity), `ms_fallbackPS` x7
      (unchanged from baseline).
- [x] NEGATIVE: `tryCompilePixelShaderFromHlslNoFatal.*pixelShaderProgram.m_psrcLen` = 0
      (R3-02g: strlen used, not m_psrcLen).
- [x] Commit `8bafc7dbf` (Task 1) exists in `git log`.
- [x] Commit `a83cec96d` (Task 2 deferral marker) exists in `git log`.
- [x] `build-17-02-gl11-2.msbuild.log` has 0 `unresolved external symbol` (gl11_d.dll link).
- [x] `build-17-02-swgclient-sln.msbuild.log` has 0 `unresolved external symbol`
      (SwgClient_d.exe link).
- [x] `<worktree>/stage/gl11_d.dll` exists (1.9 MB).
- [x] `<worktree>/stage/SwgClient_d.exe` exists (70 MB).
- [x] No git deletions across either Task 1 or Task 2 commits
      (`git diff --diff-filter=D --name-only` is empty).

---
*Phase: 17-psrc-census-char-select-beachhead*
*Plan: 02 — IMPLEMENTATION COMPLETE 2026-05-28; CHAR-01 BOOT CHECKPOINT PENDING ON KENNY*
