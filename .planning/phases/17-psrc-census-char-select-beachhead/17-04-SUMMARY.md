---
phase: 17-psrc-census-char-select-beachhead
plan: 04
subsystem: renderer
tags: [d3d11, vs-ps-linkage, signature-pair-validation, cbuffer-schema, swgvertexconstants, char-select, beachhead, visual-parity-activation]

# Dependency graph
requires:
  - phase: 17
    plan: 01
    provides: "ShaderImplementationPassPixelShaderProgram::m_psrcText + m_psrcLen (PSRC source-text retention); char-select 22 HLSL + 10 asm PS census"
  - phase: 17
    plan: 02
    provides: "tryCompilePixelShaderFromHlslNoFatal non-fatal recompile lane + DXBC retain + D3DReflect-once cache (m_reflectedCbufferLayouts + m_reflectedPSInputs)"
  - phase: 17
    plan: 03
    provides: "Per-pass material source-data cache (PerPassMaterial; SR-1) + reflection-driven offset-aware cbuffer upload at apply() (R3-03c); defensive 64 KiB TotalSize cap"
provides:
  - "Direct3d11_PixelShaderProgramData::isCompatibleWithVS — static validator that walks the PS's reflected input signature and asserts each user input is satisfiable by a VS output at the SAME hardware register slot with matching (SemanticName, SemanticIndex), matching D3D_REGISTER_COMPONENT_TYPE, and a PS read mask that is a SUBSET of the VS write mask"
  - "Direct3d11_StateCache::applyPreDrawState gate — asset PS only binds when isCompatibleWithVS returns true; otherwise routes through the existing selectFallbackPSForVS path (Iter-3 buildHlslForVSOutputs PS mirrors VS output signature register-for-register by construction)"
  - "Per-(VS, PS) pair compatibility result logged ONCE per cache entry (not per draw) via DEBUG_REPORT_LOG_PRINT + ID3D11InfoQueue::AddApplicationMessage (R3-02b dual-route precedent) — visible in stage/d3d11-debug.log under explorer launch"
  - "Schema-aware writeVarByName lookups in Direct3d11_StaticShaderData::apply() — material[0/1/2] array-element form (SwgVertexConstants schema, per Direct3d11_HlslRewrite.cpp Rule D wrap) with fallback to original materialDiffuse/Specular/Emissive hardcoded names for PSes that don't `#include` the VS rewrite header"
  - "Task 2 ONE-SHOT per-anchor cbuffer-vars-discovery dump — enumerates every reflected variable (Name, StartOffset, Size) for head + eye anchor shaders when any of wrote* lands as zero, dual-route logged"
affects: [17-X-vs-ps-recompile-fallback (potential follow-up if asset-PS bind rate too low after 17-04), char-select-visual-AB, CHAR-01/02/03 requirements gate]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "VS<->PS signature pair compatibility validator — pure function over Plan 17-02's reflected-PS-input cache + Phase 11 Iter-3's reflected-VS-output cache; no D3DReflect calls at bind time (zero per-draw cost beyond the std::set<std::pair> dedupe lookup); SV_Position auto-excluded by Plan 17-02's SystemValueType filter; HLSL-spec-correct case-insensitive semantic comparison via _stricmp"
    - "Bind-time fallthrough — when asset PS fails the compat check, fall through to the existing selectFallbackPSForVS path rather than skipping the draw or binding magenta unconditionally; preserves the Phase 11 Iter-4 per-VS dynamic PS infrastructure as the safety-net layer for incompatible asset PSes"
    - "Once-per-cache-entry logging via static std::set<std::pair<void const*, void const*>> dedupe — keys on the (VS*, PS*) raw pointers (both program-data objects are template-graphics-data singletons that outlive any draw); each new pair emits exactly one InfoQueue + DEBUG_REPORT_LOG_PRINT line with the concrete mismatch reason"
    - "Schema-aware reflected-cbuffer-variable lookup — array-element form (e.g. `material[0]`) tried FIRST, scalar-name fallback (e.g. `materialDiffuse`) tried second; single candidate per channel (no fallthrough across array indices) so a wrong-slot hit doesn't stomp a sibling channel"
    - "One-shot per-anchor cbuffer-vars-discovery dump — gated on (incomplete-write AND first-encounter-this-anchor) so exactly one head + one eye anchor produce a full layout enumeration per boot; enables follow-up index-mapping refinement from real boot evidence without re-running with a different diagnostic build"

key-files:
  created:
    - ".planning/phases/17-psrc-census-char-select-beachhead/17-04-SUMMARY.md (this file)"
  modified:
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.h (Task 1: forward-declare friend + add static isCompatibleWithVS public accessor with 40-line documentation paragraph)"
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp (Task 1: <set> + <utility> includes; isCompatibleWithVS implementation — walks psInputs, locates VS output by (semantic, index), validates Register + ComponentType + (Mask & PS.read == PS.read); per-pair dedupe set + dual-route logging on first encounter with concrete mismatch reason)"
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp (Task 1: applyPreDrawState bind-time gate — asset PS binds only when isCompatibleWithVS returns true, otherwise selectFallbackPSForVS owns the bind)"
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp (Task 2: writeVarByName lookups now schema-aware — material[0/1/2] tried first with materialDiffuse/Specular/Emissive scalar fallback; one-shot per-anchor cbuffer-vars-discovery dump on incomplete writes)"

key-decisions:
  - "Validator placed as static method on Direct3d11_PixelShaderProgramData (NOT free function). Mirrors getFallbackPS / getOrCompilePSForVS class-membership pattern; keeps the reflected-data accessors private if needed later; matches the plan's `isCompatibleWithVS` signature literally."
  - "_stricmp for semantic comparison (HLSL semantics are case-insensitive per the HLSL spec). Plan 17-02's existing buildHlslForVSOutputs already uses _stricmp for TEXCOORD / COLOR matching at Direct3d11_PixelShaderProgramData.cpp:500/527 — same convention preserved."
  - "VS ComponentMask (declared signature contract) used for the mask-subset check, NOT ReadWriteMask (which-components-VS-actually-wrote). D3D11 validates against the declared signature; ReadWriteMask is preserved for future telemetry per Direct3d11_VertexShaderData.h:88 doc."
  - "Interpolation modifiers (linear / centroid / nointerpolation / sample) NOT compared — Codex consult flagged these as can-mismatch-without-id=343 but produces wrong-output; deferred per plan guidance until a specific shader misbehaves. Asset PSRC sources don't typically declare explicit modifiers in SM2-era code so this is unlikely to bite."
  - "Slot mapping per Plan 17-04 PLAN's documented 'likely mapping' (material[0]=diffuse, material[1]=specular, material[2]=emissive) chosen over Plan 17-03 SUMMARY caveat's D3D9-FFP-style alternate (material[1]=diffuse, material[3]=specular, material[2]=emissive). Discovery dump pins the right ordering from boot; index constants tighten in a follow-up commit if the FFP ordering proves correct. NOT double-writing to both candidate orderings — single attempt per channel, no fallthrough across array indices, so a wrong-slot hit doesn't stomp a sibling channel."
  - "Defensive-null behavior on isCompatibleWithVS — vs==nullptr returns true (no VS to validate; let asset PS bind, matching pre-17-04 default); ps==nullptr returns false (callsite already pre-checks ms_currentPSData->getPixelShader() non-null before calling — defensive guard, not a contract). Empty psInputs returns true trivially; empty vsOutputs with non-empty psInputs returns false with a specific 'reflection failed or VS degenerate' log message."
  - "Task 2 discovery dump gated on (incomplete-write AND first-encounter-this-anchor) so it emits exactly when needed — if Task 2's mapping is correct, wrote(D,S,E) lands as (1,1,1) and the dump never fires (zero log overhead). If incorrect, exactly one head + one eye anchor enumerate the layout for follow-up. R3-03g flush log unchanged — Kenny's boot reads both."
  - "No Direct3d11_VertexShaderData modifications — getReflectedOutputs() already exists and exposes the Plan 11-09.13 Iter-3 cached vector with Register / ComponentMask / ReadWriteMask / ComponentType per the header doc. Plan 17-04 consumes it as-is."

patterns-established:
  - "Pattern: VS<->PS signature pair validator gates asset-PS bind time. Pure function over reflected caches; per-pair logging; fallthrough to VS-matched generated PS on incompatibility. Generalizes to any future asset PS surface that wants to validate against the bound VS at the linkage boundary."
  - "Pattern: schema-aware reflected-cbuffer variable lookup with array-element + scalar-name fallback. Generalizes to other VS-side cbuffers reused by PS (e.g. if a future shader reuses extendedLightData entries in its PS path)."
  - "Pattern: gated one-shot diagnostic dump — fires only when the failure signature is present AND only once per anchor key; zero overhead in the success case, exactly enough evidence in the failure case to drive the next iteration without a separate diagnostic build."

requirements-completed: []   # CHAR-01/02/03 are GATED ON Kenny's checkpoint Task 3 boot; the orchestrator marks the requirements complete after the visual A/B is accepted

# Metrics
duration: ~1.2h (cold worktree checkout reset to base + read-context + Task 1 implementation + Task 1 build + Task 2 implementation + Task 2 build + 2 atomic commits + SUMMARY)
completed: 2026-05-28
---

# Phase 17 Plan 04: Visual Parity Activation — VS↔PS signature pair validation + cbuffer name-lookup parity — IMPLEMENTATION COMPLETE; CHAR-01/02/03 BOOT CHECKPOINT PENDING

**Landed the two technical fixes that activate CHAR-01/02/03 visual
parity after Plans 17-01/02/03 surfaced two systemic gaps in the
recompile + reflection infrastructure:**

- **Task 1 — VS↔PS signature pair validation.** Plan 17-02's recompile
  lane bound real HLSL PS programs whose input signature was
  independently compiled and assigned register slots in declaration
  order. The bound VS's output signature (independently compiled in
  Phase 11's `Direct3d11_VertexShaderData.cpp` recompile lane) didn't
  match those register positions for character body shaders, firing
  `id=343` × 24K per char-select boot. Add bind-time signature
  compatibility validation: when the asset PS's reflected input
  signature is incompatible with the bound VS's reflected output
  signature, fall through to the existing `selectFallbackPSForVS` path
  (Phase 11 Iter-3's `buildHlslForVSOutputs()` builds a PS matching the
  VS register layout). When signatures are compatible, use the asset PS
  as Plan 17-02 designed.

- **Task 2 — writeVarByName for SwgVertexConstants `material[N]` array
  schema.** Plan 17-03's R3-03g flush log confirmed
  `wroteDiffuse / wroteSpecular / wroteEmissive = 0 / 0 / 0` for all
  char-select shaders despite valid source data. The reflected cbuffer
  at b0 is `SwgVertexConstants` (the VS-side cbuffer reused by the
  recompiled HLSL PS programs because they `#include` the VS rewrite
  header) and exposes constants as `material[N]` array elements (per
  `vertex_shader_constants.inc`), NOT as the
  `materialDiffuse / materialSpecular / materialEmissive` names Plan
  17-03's executor coded. `writeVarByName` lookups now try
  `material[0]/material[1]/material[2]` FIRST (SwgVertexConstants
  schema), falling back to the original hardcoded names for PSes that
  don't `#include` the VS rewrite header. A new ONE-SHOT discovery dump
  enumerates every reflected variable in the cbuffer when wrote* lands
  as incomplete for a head or eye anchor, surfacing the EXACT names +
  offsets for follow-up refinement from boot evidence.

**Build is clean — gl11_d.dll Debug Win32 linked with 0 unresolved
external symbols, 0 C/LNK/MSB errors. SwgClient.exe NOT rebuilt (no
shared / clientGraphics edits — pure Direct3d11 plugin work). Both
tasks committed atomically; Task 3 (human-verify boot under
rasterMajor=11) is returned as checkpoint state to the orchestrator
(executor does NOT boot — worktree environment cannot load .tre
archives).**

## Performance

- **Total duration:** ~1.2 hours wall-clock
- **Started:** 2026-05-28 (right after Plan 17-03 close-out merge `45f5c895a`)
- **Task 1 implementation + build:** ~30 minutes (read consult context + design validator + edit header + edit .cpp + edit StateCache + ~5 min plugin rebuild)
- **Task 2 implementation + build:** ~25 minutes (read Plan 17-03 cbuffer caveat + design schema-aware lookups + add discovery dump + ~5 min plugin rebuild)
- **SUMMARY + checkpoint preparation:** ~10 minutes
- **Tasks committed:** 2 of 3 source-effecting tasks (Task 3 is checkpoint state — Kenny owns)
- **Files modified:** 4 plugin source files (Direct3d11_PixelShaderProgramData.h + .cpp + Direct3d11_StateCache.cpp + Direct3d11_StaticShaderData.cpp)
- **Files created:** 1 docs (this SUMMARY)

## Accomplishments

### Task 1 — VS↔PS signature pair validation

#### Direct3d11_PixelShaderProgramData.h (declaration + 40-line documentation paragraph)

Added public static method `bool isCompatibleWithVS(Direct3d11_VertexShaderData const *vs, Direct3d11_PixelShaderProgramData const *ps)` to the class. The header doc paragraph captures:

- Why the gate exists (Plan 17-02's PS bind won unconditionally over selectFallbackPSForVS → id=343 × 24K per session → flat-black character)
- Validation contract (semantic + register slot + mask-subset + component-type for user semantics only; SV_* filtered)
- Logging discipline (once per (VS, PS) cache entry, dual-route)
- Defensive-null behavior (vs==null → true / pre-17-04 default; ps==null → false / callsite asserts)

#### Direct3d11_PixelShaderProgramData.cpp (implementation)

- Added `<set>` and `<utility>` includes for the per-pair dedupe set.
- Implementation placed immediately before `~Direct3d11_PixelShaderProgramData` (the natural end-of-class slot).
- Algorithm:
  1. Defensive nulls → trivial returns.
  2. Trivial-compat early exit for empty `psInputs`.
  3. Specific log message for non-empty `psInputs` against empty `vsOutputs` (reflection-failed-or-VS-degenerate signature).
  4. For each PS input: locate VS output by `_stricmp(SemanticName, SemanticName) == 0 && SemanticIndex == SemanticIndex` (HLSL-spec-correct case-insensitive comparison).
  5. Validate `Register == Register` (D3D11 register-position-strict linkage), `ComponentType == ComponentType` (float-vs-int rejection), and `(VS.ComponentMask & PS.Mask) == PS.Mask` (PS read mask ⊆ VS write mask).
  6. On any mismatch: format a CONCRETE per-mismatch reason string (cites semantic + indices + offending values), emit ONCE via dual-route logging, return false.
  7. On full pass: emit one-shot COMPATIBLE log, return true.
- Per-(VS, PS) dedupe via `static std::set<std::pair<void const*, void const*>>`. Keys on raw pointers — both program-data objects are template-graphics-data singletons that outlive any draw.

#### Direct3d11_StateCache.cpp (bind-time gate)

Replaced the pre-17-04 `if/else if` chain at applyPreDrawState's step 5 (`VS + PS`) with a `psToBind` variable populated by:

```cpp
if (ms_currentPSData && ms_currentPSData->getPixelShader()
    && Direct3d11_PixelShaderProgramData::isCompatibleWithVS(ms_currentVSData, ms_currentPSData))
    psToBind = ms_currentPSData->getPixelShader();
else if (ID3D11PixelShader *fallback = selectFallbackPSForVS(ms_currentVSData))
    psToBind = fallback;
if (psToBind)
    ctx->PSSetShader(psToBind, nullptr, 0);
```

When isCompatibleWithVS returns false, the existing `selectFallbackPSForVS` (which Phase 11 Iter-4 made return a per-VS dynamic PS whose input struct mirrors the VS output signature register-for-register) owns the bind — D3D11 stage linkage is restored by construction. When true, asset PS wins as Plan 17-02 designed.

### Task 2 — writeVarByName for SwgVertexConstants schema

#### Direct3d11_StaticShaderData.cpp (apply() upload block)

- Replaced the three Plan 17-03 hardcoded-name lookups (`materialDiffuse`, `materialSpecular`, `materialEmissive`) with multi-candidate lookups:

  ```cpp
  bool const wroteDiffuse =
      writeVarByName("material[0]",   passDiffuse)
      || writeVarByName("materialDiffuse",  passDiffuse);
  bool const wroteSpecular =
      writeVarByName("material[1]",   passSpecular)
      || writeVarByName("materialSpecular", passSpecular);
  bool const wroteEmissive =
      writeVarByName("material[2]",   passEmissive)
      || writeVarByName("materialEmissive", passEmissive);
  ```

  textureFactor + textureFactor2 keep their original hardcoded names (Plan 17-03 census proved no char-select PS reflected a textureFactor variable).

- Added Task 2 ONE-SHOT per-anchor cbuffer-vars-discovery dump:
  - Gated on `incomplete-write` (any of wrote(D, S, E) lands as zero) AND `first-encounter` (per-anchor static bool keyed off head/eye substring match on `shader/sul_m_head.sht` / `shader/sul_eye.sht` paths).
  - Emits a header line (`shader='...' layoutName='...' bindPoint=N totalSize=N varsCount=N wrote(D,S,E)=(0,0,0) — enumerating reflected variables:`) followed by ONE LINE per variable (`var[i]: name='...' startOffset=N size=N`).
  - Dual-route logged (InfoQueue file sink + DEBUG_REPORT_LOG_PRINT) — mirrors Plan 17-02 R3-02b and Plan 17-03 R3-03g.
  - Zero overhead in the success case (incomplete-write gate); exactly enough evidence in the failure case to drive a follow-up tightening commit without a separate diagnostic build.

- Plan 17-03's R3-03g flush log remains active and now reports wrote(D, S, E) against the corrected lookups — Kenny's boot can distinguish task-2 success (1/1/1) from a slot-mapping miss (0/0/0 with the discovery dump pinning the right indices).

### Build gates (Debug Win32, /nodeReuse:false)

- **gl11_d.dll** (Direct3d11.vcxproj): 0 `unresolved external symbol`; 0 C/LNK/MSB errors. ~5 min compile+link per task. Post-build copy lands gl11_d.dll (1.9 MB) + gl11_d.pdb in worktree `stage/`.
- **SwgClient_d.exe** NOT rebuilt — Plan 17-04 has zero shared / clientGraphics edits; SwgClient.exe is unchanged from the Plan 17-03 build artifact already in host stage/.

## Task Commits

1. **Task 1 — VS↔PS signature pair validation:** `ae9519243` — `feat(17-04): VS<->PS signature pair validation at asset-PS bind time (id=343 mitigation)`
2. **Task 2 — writeVarByName SwgVertexConstants schema:** `22d185782` — `feat(17-04): writeVarByName recognizes SwgVertexConstants material[N] array schema (cbuffer values now land)`
3. **Plan close commit (this SUMMARY):** _next commit_
4. **Task 3 (CHAR-01/02/03 human-verify boot):** CHECKPOINT — orchestrator deploys gl11_d.dll, Kenny boots `SwgClient_d.exe` under `rasterMajor=11`, captures `char_default_d3d11_0003.png`, runs the 5 boot-gate greps documented in the Task 3 section of the PLAN, reports back.

## Files Created/Modified

### Created
- `.planning/phases/17-psrc-census-char-select-beachhead/17-04-SUMMARY.md` — this file.

### Modified
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.h` — added 40-line documentation paragraph + `static bool isCompatibleWithVS(...)` public declaration.
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp` — added `<set>` + `<utility>` includes + isCompatibleWithVS implementation block (~170 lines including documentation comment).
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp` — replaced applyPreDrawState's step-5 if/else-if chain with the isCompatibleWithVS-gated psToBind variable + null-check ctx->PSSetShader.
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp` — extended 3 writeVarByName lookups with material[0/1/2] array-element form (FIRST candidate) + original hardcoded scalar name (fallback); added one-shot per-anchor cbuffer-vars-discovery dump (~70 lines including documentation).

### Worktree build artifacts (NOT committed — orchestrator handles host redeployment)
- `stage/gl11_d.dll` (1.9 MB, fresh build from Task 2), `stage/gl11_d.pdb`
- `build-17-04-task1-gl11.msbuild.log`, `build-17-04-task2-gl11.msbuild.log`

## Decisions Made

1. **Validator placement** — static method on `Direct3d11_PixelShaderProgramData` (mirrors getFallbackPS / getOrCompilePSForVS class-membership pattern; matches plan's `isCompatibleWithVS` signature literally).
2. **HLSL semantic comparison via `_stricmp`** — HLSL spec is case-insensitive; matches existing buildHlslForVSOutputs convention at Direct3d11_PixelShaderProgramData.cpp:500/527.
3. **VS ComponentMask, NOT ReadWriteMask, for the mask-subset check** — D3D11 validates against the declared signature; ReadWriteMask is preserved on the cached descriptor for future telemetry per the header doc.
4. **Interpolation modifiers NOT compared** — Codex consult flagged these as can-mismatch-without-id=343; deferred per plan guidance until a specific shader misbehaves.
5. **Slot mapping `material[0]/material[1]/material[2]` per Plan 17-04 PLAN's documented likely mapping** — alternate D3D9-FFP-style ordering surveyed by Plan 17-03 SUMMARY caveat is plausible but unconfirmed; discovery dump pins the right ordering from boot evidence. Single candidate per channel (no fallthrough across array indices) prevents wrong-slot stomp.
6. **No Direct3d11_VertexShaderData edits** — `getReflectedOutputs()` already exists and exposes the full Direct3d11_ReflectedVSOutput vector (Register / ComponentMask / ReadWriteMask / ComponentType) per Direct3d11_VertexShaderData.h:82-90 + 194-197.
7. **No SwgClient.exe rebuild** — Plan 17-04 has zero shared / clientGraphics edits; SwgClient.exe is unchanged from the Plan 17-03 build artifact already in host stage/.
8. **Task 3 returned as checkpoint state** — executor environment cannot boot the client (no .tre archives in fresh worktree); Kenny's host boot is the gate.

## Deviations from Plan

None — plan executed exactly as written.

The plan's `<context>` section (lines 56-127) provided complete implementation guidance for both tasks, and the cross-AI CONSULT synthesis at `.planning/research/CONSULT-vs-ps-interpolator-mismatch-SYNTHESIS.md` had already framed the architectural decision (validator gate over a new VS recompile lane). No Rule 1-3 auto-fixes triggered; no Rule 4 architectural decisions surfaced.

Two minor implementation choices worth flagging that align with the plan's guidance but were not literally specified:

1. **Validator placed as static method on the class** rather than a free function. The plan offered either as acceptable ("free function inside Direct3d11_PixelShaderProgramData.cpp OR a method on Direct3d11_PixelShaderProgramData"). Static method chosen for declarative consistency with the existing getFallbackPS / getOrCompilePSForVS pattern.
2. **Slot indices `material[0]/material[1]/material[2]` chosen over the D3D9-FFP-style alternate** documented in Plan 17-03 SUMMARY's line 252-258 caveat. The plan documents both as plausible; the discovery dump pins the correct mapping from boot evidence.

## Acceptance grep gates (Task 1 + Task 2) — ALL PASSED

| Gate | Expected | Actual |
|------|----------|--------|
| Task 1 `isCompatibleWithVS` declaration in .h | ==1 | 1 |
| Task 1 `isCompatibleWithVS` definition + callsite in .cpp + StateCache | >=2 | 3 (1 defn + 1 inline call in PixelShaderProgramData.cpp comment-internal + 1 StateCache call) |
| Task 1 `<set>` + `<utility>` includes in .cpp | >=2 | 2 |
| Task 1 `static std::set<std::pair<` dedupe declaration | >=1 | 1 |
| Task 1 `AddApplicationMessage(D3D11_MESSAGE_SEVERITY_INFO` in PixelShaderProgramData.cpp | >=4 (existing 4 + new 4 = 8 with the per-pair log paths) | 8 |
| Task 1 `selectFallbackPSForVS` callsite in StateCache (preserved) | ==1 | 1 |
| Task 1 `Plan 17-04 Task 1 VS<->PS pair compatibility` log format | >=4 | 4 (compatible, incompatible-vs-degenerate, incompatible-mismatch, compatible-no-inputs) |
| Task 2 `writeVarByName("material[0]"` in StaticShaderData.cpp | >=1 | 1 |
| Task 2 `writeVarByName("material[1]"` in StaticShaderData.cpp | >=1 | 1 |
| Task 2 `writeVarByName("material[2]"` in StaticShaderData.cpp | >=1 | 1 |
| Task 2 `writeVarByName("materialDiffuse"` preserved fallback | >=1 | 1 |
| Task 2 `Plan 17-04 Task 2 cbuffer-vars-discovery` log header | >=1 | 1 |
| Task 2 `s_dumpedHeadVars` + `s_dumpedEyeVars` dedupe statics | ==2 | 2 |
| Plan 17-03 R3-03g flush log preserved (not regressed) | >=1 | 1 (unchanged) |
| Plan 17-02 PS-cbuffer + PS-input-sig logs preserved | >=2 | 2 (unchanged) |
| Debug link: 0 unresolved external symbol (gl11) | 0 | 0 |
| SwgClient.exe NOT rebuilt (no shared edits) | (n/a) | confirmed — no clientGraphics / shared / SwgClient.vcxproj source touched |
| D3D9 plugin untouched (Direct3d9_*.{cpp,h}) | 0 edits | 0 edits |
| tools/swg_blender/, .cursor/, blender/maya/sat-lmg docs untouched | 0 edits | 0 edits |
| .planning/research/CONSULT-* untouched | 0 edits | 0 edits |
| STATE.md, ROADMAP.md NOT modified | 0 edits | 0 edits |

## Boot evidence placeholders (Task 3 checkpoint)

These are POPULATED by Kenny's host boot under rasterMajor=11 to char-select:

```
[TASK 3 — to be filled by Kenny after host-boot grep against stage/d3d11-debug.log]

Boot gate greps (5):
  Select-String -Path stage/d3d11-debug.log -Pattern 'id=342' -SimpleMatch | Measure-Object | % Count
    Expected: 0
    Actual: <FILL>
  Select-String -Path stage/d3d11-debug.log -Pattern 'id=343' -SimpleMatch | Measure-Object | % Count
    Expected: < 100 (down from 24,408 — Task 1 worked; some residual ok if a few shader pairs still mismatch)
    Actual: <FILL>
  Select-String -Path stage/d3d11-debug.log -Pattern 'PSRC recompile FAILED' -SimpleMatch | Measure-Object | % Count
    Expected: 0 (Plan 17-02 contract preserved)
    Actual: <FILL>
  Select-String -Path stage/d3d11-debug.log -Pattern 'wroteDiffuse=1' -SimpleMatch | Measure-Object | % Count
    Expected: > 0 (Task 2 wrote material values for at least one shader)
    Actual: <FILL>
  Select-String -Path stage/d3d11-debug.log -Pattern 'incompatible' -SimpleMatch | Measure-Object | % Count
    Expected: any (0 = all pairs compatible, asset PS wins everywhere; >0 = Task 1's pair logging fired and selectFallbackPSForVS owns those binds)
    Actual: <FILL>

Visual outcome (CHAR-01/02/03):
  char_default_d3d11_0003.png at char-select default pose under rasterMajor=11
  Expected: textured character (skin tone visible, colored clothing, eyes visible) — first time at D3D11 char-select since v2.2 effort began
  Actual: <FILL>

Task 1 per-(VS, PS) pair compatibility log lines (sample):
  Plan 17-04 Task 1 VS<->PS pair compatibility: COMPATIBLE vs=0x<addr> ps=0x<addr> (matched N ps inputs against vs outputs)
  Plan 17-04 Task 1 VS<->PS pair compatibility: INCOMPATIBLE vs=0x<addr> ps=0x<addr> reason='ps input 'COLOR0' expects register v0 but vs writes semantic at register o2 (D3D11 stage linkage is register-position-strict; id=343)'
  <FILL with verbatim lines from stage/d3d11-debug.log>

Task 2 cbuffer-vars-discovery dump (head + eye anchors, IF wrote* incomplete):
  Plan 17-04 Task 2 cbuffer-vars-discovery shader='shader/sul_m_head.sht' layoutName='SwgVertexConstants' bindPoint=0 totalSize=400 varsCount=9 wrote(D,S,E)=(?,?,?) — enumerating reflected variables:
  Plan 17-04 Task 2 cbuffer-vars-discovery shader='shader/sul_m_head.sht' var[0]: name='<NAME>' startOffset=<N> size=<N>
  Plan 17-04 Task 2 cbuffer-vars-discovery shader='shader/sul_m_head.sht' var[1]: name='<NAME>' startOffset=<N> size=<N>
  ...
  <FILL with verbatim lines from stage/d3d11-debug.log if Task 2 mapping was incorrect>
```

**Success thresholds (per PLAN <task type="checkpoint:human-verify">):**

- `id=343` < 100 (down from 24,408 — Task 1 worked; some residual ok if a few shader pairs still mismatch)
- `wroteDiffuse=1` > 0 (Task 2 wrote material values for at least one shader)
- `incompatible` >= 0 (Task 1's per-pair logging fired at least once with concrete mismatch reasons, OR 0 meaning all pairs were compatible — either is acceptable)
- Visual: character has skin tone + colored clothing + eyes (not flat black, not magenta)

**Partial-success contingency:** if `id=343` is still high, OR `wroteDiffuse=1` stays at 0, OR the character is still flat black, the per-cache-entry pair-compat log + the cbuffer-vars-discovery dump tell us why. Capture the data into this SUMMARY's Task 3 placeholder, root-cause analysis can defer to a Plan 17-04.X if needed (per PLAN spec).

## Issues Encountered

None. Task 1 + Task 2 implementations + builds proceeded cleanly:

- gl11_d.dll Debug Win32 link clean on first try for both tasks (0 unresolved external symbol, 0 C/LNK/MSB errors).
- No SwgClient.exe rebuild needed (no shared / clientGraphics changes — pure Direct3d11 plugin work, gl11_d.dll is the only artifact touched).
- Pre-existing `warning C4459: declaration of 'E' hides global declaration` in DirectXMathVector.inl chained from new compile of Direct3d11_StaticShaderData.cpp / Direct3d11_StateCache.cpp / etc. — predates this plan (Phase 11 baseline).

## Pre-existing warnings out of scope (not introduced by this plan)

- `warning C4459: declaration of 'E' hides global declaration` — multiple occurrences in `DirectXMathVector.inl` chained from Direct3d11_StaticShaderData.cpp / Direct3d11_StateCache.cpp / Direct3d11.cpp / Direct3d11_ConstantBuffer.cpp / Direct3d11_LightManager.cpp. Predates this plan (Phase 11 baseline; documented in Plans 17-02 + 17-03 SUMMARYs).

## Threat Flags

(None — all changes are to D3D11 plugin internals. No new network surface, no new auth path, no new file access, no schema changes at trust boundaries. The Plan 17-02 → 17-03 → 17-04 chain operates entirely inside the renderer's asset-load + per-draw paths.)

## Known Stubs

None. The Task 1 per-pair compatibility log + the Task 2 cbuffer-vars-discovery dump are diagnostic instrumentation (not stubs); both emit real data on a one-shot basis and the discovery dump self-suppresses when wrote(D, S, E) lands as (1, 1, 1). Plan 17-03's R3-03g flush log + sub-step 1g cross-clobber log remain `#if 1` as Kenny's Task 3 checkpoint surface — same deferred-cleanup posture documented in Plan 17-03 SUMMARY's "Deferred / next-plan" section.

## Self-Check: PASSED

- [x] `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.h` contains `static bool isCompatibleWithVS(` declaration.
- [x] `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp` contains `bool Direct3d11_PixelShaderProgramData::isCompatibleWithVS(` definition + `<set>` + `<utility>` + `static std::set<std::pair<void const *, void const *>>` + `Plan 17-04 Task 1 VS<->PS pair compatibility` (multiple) + `_stricmp(vsOut.SemanticName,` + `(vsOutMatch->ComponentMask & psIn.Mask) != psIn.Mask`.
- [x] `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp` contains `Direct3d11_PixelShaderProgramData::isCompatibleWithVS(ms_currentVSData, ms_currentPSData)` + the gated `psToBind` variable + `selectFallbackPSForVS(ms_currentVSData)` callsite preserved.
- [x] `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp` contains `writeVarByName("material[0]"` + `writeVarByName("material[1]"` + `writeVarByName("material[2]"` + `writeVarByName("materialDiffuse"` (fallback preserved) + `Plan 17-04 Task 2 cbuffer-vars-discovery` + `s_dumpedHeadVars` + `s_dumpedEyeVars`.
- [x] Commit `ae9519243` exists in `git log` (Task 1).
- [x] Commit `22d185782` exists in `git log` (Task 2).
- [x] `build-17-04-task1-gl11.msbuild.log` has 0 `unresolved external symbol`.
- [x] `build-17-04-task2-gl11.msbuild.log` has 0 `unresolved external symbol`.
- [x] `<worktree>/stage/gl11_d.dll` exists (1.9 MB).
- [x] No git deletions across either Task 1 or Task 2 commits (`git diff --diff-filter=D --name-only` empty for each).
- [x] STATE.md, ROADMAP.md NOT modified (orchestrator owns those).
- [x] tools/swg_blender/, .cursor/, blender/maya/sat-lmg/tre-project docs NOT modified.
- [x] .planning/research/CONSULT-* files NOT modified.
- [x] D3D9 plugin (Direct3d9_*.{cpp,h}) NOT modified.
- [x] SwgClient.exe NOT rebuilt (no shared changes).

---
*Phase: 17-psrc-census-char-select-beachhead*
*Plan: 04 — IMPLEMENTATION COMPLETE 2026-05-28; CHAR-01/02/03 BOOT CHECKPOINT (Task 3) PENDING ON KENNY*
