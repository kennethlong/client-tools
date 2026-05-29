---
phase: 17-psrc-census-char-select-beachhead
plan: 06a
type: execute
wave: 6
depends_on: ["17-04", "17-05-task3"]
files_modified:
  - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp
autonomous: false
requirements: [CHAR-02, CHAR-03]
gap_closure: true
threat_flags: []
user_setup: []

must_haves:
  truths:
    - "GAP-2 discovery: the one-shot cbuffer-vars-discovery dump in `Direct3d11_StaticShaderData::apply()` is EXTENDED to walk `userConstants`'s inner reflection (D3D11 `ID3D11ShaderReflectionType::GetMemberTypeByIndex` against the var-7 / var-8 type descriptor) and emit one log line per inner field — name + StartOffset (relative to userConstants) + Size — so Plan 17-06b has unambiguous mapping evidence for where diffuse + emissive material colors actually live in the SwgVertexConstants layout."
    - "Per Round-4 review HIGH-5 (CONVERGED, both reviewers): Rule D in `Direct3d11_HlslRewrite.cpp:747-767` only wraps `: register(cN)` -> `packoffset(cN)` while preserving original declaration names — it contains NO `c[N] -> packedRegister[K].channel` translation table. Plan 17-06's prior 'Path A first, Path B fallback' framing was a dead end on the Path A side. This plan defaults to PATH B (discovery dump extension) as the primary path and rescopes Path A to a passing reference for completeness."
    - "Per Round-4 review HIGH-3 (CONVERGED, both reviewers): char-select PSes (`h_simple_pp_ps20.psh`, `h_color2_specmap_cbmp_ps20.psh` per evidence/plan-17-04x-psrc-source-dump.txt) consume `textureFactor.rgb` + interpolated `COLOR0` (vertexDiffuse), NOT cbuffer `materialDiffuse` / `materialEmissive`. So GAP-2 closure (in plans 17-06a + 17-06b together) is INSTRUMENTATION COMPLETENESS — the discovered mapping enables future open-world shaders that DO consume cbuffer material colors, but the CHAR-01/02/03 visual delivery is NOT materially driven by this plan. CHAR-02/03 are listed in requirements because the eye + head materials are the ANCHOR shaders the discovery dump is gated on (existing s_dumpedHeadVars / s_dumpedEyeVars heuristic per 17-04 SUMMARY line 259)."
    - "Per RESEARCH §Architectural Responsibility Map: this work is PLUGIN-LOCAL (Direct3d11_StaticShaderData.cpp ONLY) — NO shared `clientGraphics` edits, NO Direct3d9_*.{cpp,h} edits, NO header touches that would trigger the shared-header ABI cascade trap (memory project_shared_header_abi_cascade_trap). Pure gl11_d.dll rebuild; SwgClient.exe untouched."
    - "Per 17-04 patterns-established item 3: the existing gated one-shot diagnostic dump (gated on `incomplete-write AND first-encounter-this-anchor`, controlled by `s_dumpedHeadVars` / `s_dumpedEyeVars` per 17-04 SUMMARY line 259) is the structural template. This plan EXTENDS the existing dump rather than introducing a new one — same gate, same anchors, same output channel; new content (inner-field enumeration of userConstants) on top."
    - "Per D-06: D3D9 reference path unregressed; no shared file is touched; both rasterMajor=5 and rasterMajor=11 boot to char-select without crash. The plugin rebuild applies only to gl11_d.dll."
    - "Per CHAR-02 + CHAR-03 (anchor-gated): the discovery dump fires when the post-04ef976 baseline (`wrote(D,S,E)=(0,1,0)`) gates trigger — which it does for sul_eye and sul_*_head anchors per Kenny's 2026-05-28 boot evidence. So the discovery output naturally captures the layout for the anchors CHAR-02 + CHAR-03 measure against."
  artifacts:
    - path: "src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp"
      provides: "Extended cbuffer-vars-discovery one-shot dump that walks userConstants's inner reflection — emits one line per inner float4 (and any nested struct fields) so Plan 17-06b has unambiguous mapping evidence."
      contains: "userConstants inner-field discovery dump"
  key_links:
    - from: "Direct3d11_StaticShaderData::apply() existing 17-04 Task 2 discovery dump (gated on `incomplete && first-encounter`)"
      to: "extended inner-walk of userConstants type-descriptor via ID3D11ShaderReflectionType::GetMemberTypeByIndex"
      via: "self-extension — same gate, same anchors, new content"
      pattern: "userConstants"
    - from: "post-17-06a boot evidence (Kenny's host capture)"
      to: "Plan 17-06b mapping decision (which dotted reflection path = diffuse, which = emissive)"
      via: "evidence-grounded mapping — no guessing"
      pattern: "Plan 17-06a cbuffer-vars-discovery"
---

<objective>
Close the DISCOVERY half of GAP-2: extend the existing 17-04 Task-2 one-shot cbuffer-vars-discovery dump in `Direct3d11_StaticShaderData::apply()` so it walks the inner reflection of `userConstants` (the 272-byte / 17-float4 opaque region at offset 128) and emits one log line per inner field (or nested struct field), giving Plan 17-06b unambiguous boot evidence for which dotted reflection path corresponds to diffuse + emissive material colors.

Purpose: the prior monolithic 17-06 plan offered "Path A (Rule D source-read) FIRST, Path B (discovery dump extension) as fallback." Round-4 cross-AI review (Codex + Cursor, BOTH HIGH-5) confirmed Path A is a dead end — Rule D at `Direct3d11_HlslRewrite.cpp:747-767` only wraps `register(cN)` → `packoffset(cN)` preserving original declaration names, it does NOT contain a `c[N] → packedRegister[K].channel` mapping table. The `packedRegister0..4` names in the reflection mean the rewriter INPUT (`vertex_shader_constants.inc`) declares them that way — the actual diffuse + emissive material values live INSIDE `userConstants`'s 17 float4 slots (currently opaque). Path B is the ONLY viable path.

This plan splits Path B out as its own atomic unit so:
  - Kenny boots ONCE with the discovery extension (this plan's Task 2 checkpoint)
  - The mapping decision becomes evidence-grounded (Plan 17-06b reads the inner-field names from Kenny's log paste)
  - The two halves can be reviewed / committed independently — no conditional Task 2 that bifurcates plan structure

Per Round-4 HIGH-3 (CONVERGED), GAP-2 closure (across 17-06a + 17-06b) is INSTRUMENTATION COMPLETENESS — char-select PSes consume `textureFactor.rgb` + interpolated `COLOR0`, NOT cbuffer material colors. This plan + 17-06b deliver the discovery + mapping infrastructure for FUTURE open-world shaders that DO consume cbuffer material colors. The CHAR-02/03 requirement attribution is via the anchor-gating heuristic, not direct visual delivery.

Output: a single source edit to Direct3d11_StaticShaderData.cpp extending the existing one-shot dump's inner walk; a rebuilt gl11_d.dll Debug Win32 (0 unresolved external symbols); single atomic commit; one checkpoint:human-verify task for Kenny's boot evidence; no shared / clientGraphics / D3D9 touch.
</objective>

<execution_context>
@D:/Code/swg-client-v2/.claude/get-shit-done/workflows/execute-plan.md
@D:/Code/swg-client-v2/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@.planning/ROADMAP.md
@.planning/phases/17-psrc-census-char-select-beachhead/17-CONTEXT.md
@.planning/phases/17-psrc-census-char-select-beachhead/17-VERIFICATION.md
@.planning/phases/17-psrc-census-char-select-beachhead/17-REVIEWS.md
@.planning/phases/17-psrc-census-char-select-beachhead/17-04-SUMMARY.md

## What's true now (the cbuffer discovery dump baseline this plan extends)

Kenny's 2026-05-28 22:20 host boot under rasterMajor=11 captured the top-level SwgVertexConstants reflected layout at b0 (per 17-VERIFICATION.md §Per-anchor cbuffer discovery dump):

```
totalSize=400  varsCount=9  layoutName='SwgVertexConstants'  bindPoint=0
  var[0]:  packedRegister0        offset=  0   size=16
  var[1]:  packedRegister1        offset= 16   size=16
  var[2]:  packedRegister2        offset= 32   size=16
  var[3]:  packedRegister3        offset= 48   size=16
  var[4]:  packedRegister4        offset= 64   size=16
  var[5]:  textureFactor          offset= 80   size=16
  var[6]:  textureFactor2         offset= 96   size=16
  var[7]:  materialSpecularColor  offset=112   size=16
  var[8]:  userConstants          offset=128   size=272   (17 × float4 — OPAQUE)
```

The discovery dump (gated on `incomplete-write AND first-encounter` per 17-04 SUMMARY line 259, controlled by `s_dumpedHeadVars` / `s_dumpedEyeVars`) currently enumerates ONLY the 9 top-level vars. The 272-byte userConstants region is opaque — could be 17 float4 slots, or a nested struct (e.g. `struct UserConstants { float4 lighting[4]; struct Material { float4 diffuse; float4 emissive; float4 _pad; } material; ... }`). Without inner-walk evidence, Plan 17-06b cannot map diffuse + emissive to a specific dotted reflection path without guessing.

## Why Path A is a dead end (HIGH-5)

Rule D at `Direct3d11_HlslRewrite.cpp:747-767` (verified read this session) only wraps the input declaration `: register(cN)` to `: packoffset(cN)` while preserving the ORIGINAL declaration name. If the input `vertex_shader_constants.inc` declares:
  `float4 packedRegister0 : register(c0);`
Rule D emits:
  `float4 packedRegister0 : packoffset(c0);`
inside `cbuffer SwgVertexConstants : register(b0)`. There is NO c[N] → packedRegister[K].channel translation; the `packedRegister0..4` names ARE the input declaration names. Diffuse + emissive therefore live in either:
  (i) packed channels INSIDE packedRegister0..4 (which the engine writes by the legacy D3D9 c-register layout, but the engine-side material upload site decides the packing), OR
  (ii) inside `userConstants`'s 272-byte opaque region (set by the engine's `setVertexShaderUserConstants` path — Direct3d11.cpp).

The discovery dump extension this plan adds will reveal which. If userConstants reflection exposes named inner fields (e.g. `userConstants.material.diffuse`), case (ii) is confirmed. If userConstants is reflected as a flat float4[17] array, case (ii) is still confirmed but the dotted path becomes `userConstants[N]` and the mapping requires cross-referencing engine source for which N is which.

## What the extension emits

For each top-level var that has a non-trivial type descriptor (specifically `userConstants` and `materialSpecularColor` — already partly known), the extension calls:
  - `ID3D11ShaderReflectionType *innerType = layout.GetVariableByIndex(i)->GetType();`
  - `D3D11_SHADER_TYPE_DESC innerDesc; innerType->GetDesc(&innerDesc);`
  - if `innerDesc.Members > 0` (it's a struct), iterate `GetMemberTypeByIndex(j)` + `GetMemberTypeName(j)` and emit one line per member with: parent-name + member-name + relative offset + size.

Expected lines (illustrative — the actual member names come from Kenny's boot):
  `Plan 17-06a cbuffer-vars-discovery shader='shader/sul_m_head.sht' userConstants.<memberName0> offset=<N> size=<M>`
  `Plan 17-06a cbuffer-vars-discovery shader='shader/sul_m_head.sht' userConstants.<memberName1> offset=<N> size=<M>`
  ... (up to 17 lines if it's a flat float4[17], more if nested)

If userConstants is reflected as `D3D_SVT_FLOAT` with `Elements > 0` (an array, not a struct), the extension instead emits one line per element with `userConstants[i]` synthetic notation + offset = `i * 16`.

## Critical landmines (pulled from memory + 17-VERIFICATION.md cross-cutting)

- **D3D9 reference path NEVER regresses** (memory + D-06). This plan touches Direct3d11_StaticShaderData.cpp ONLY.
- **Shared-header ABI cascade trap** (memory project_shared_header_abi_cascade_trap). No header touched in this plan.
- **Don't re-enable per-pass blend factors early** (Iter-44C regression). N/A.
- **`isCompatibleWithVS` validator + selectFallbackPSForVS are SETTLED** (17-04). N/A for this plan.
- **Don't modify Koogie's source-level changes without strong reason** (memory feedback_dont_modify_koogie_changes). The discovery dump site at line ~925+ is Plan 17-04 territory — extending it is in-bounds.
- **Use `$env:MSBUILD` + `/nodeReuse:false`** (memory project_decruft_removal_build_graph_gotchas). PowerShell, not Git Bash. Delete `stage/gl11_d.dll` to force relink if needed.
- **Magenta fallback PS is a visible diagnostic — keep it** (CONTEXT.md). Untouched.

<interfaces>
<!-- Live extract from Direct3d11_StaticShaderData.cpp — 17-04 + 04ef976 state. -->
<!-- Executor uses these directly — no exploration needed. -->

Existing discovery-dump block (Direct3d11_StaticShaderData.cpp ~ lines 925-985):
  {
      static bool s_dumpedHeadVars = false;
      static bool s_dumpedEyeVars  = false;
      char const *shaderNameDbg = "?";
      if (m_shader) {
          shaderNameDbg = m_shader->getStaticShaderTemplate().getName().getString();
      }
      bool const incomplete = !(wroteDiffuse && wroteSpecular && wroteEmissive);
      bool isHeadAnchor = (strstr(shaderNameDbg, "_head") != nullptr);
      bool isEyeAnchor  = (strstr(shaderNameDbg, "eye") != nullptr);
      bool const fireDump =
          (incomplete && isHeadAnchor && !s_dumpedHeadVars) ||
          (incomplete && isEyeAnchor && !s_dumpedEyeVars);
      if (fireDump) {
          // Currently emits one DEBUG_REPORT_LOG_PRINT + InfoQueue line PER TOP-LEVEL VAR:
          //   "Plan 17-04 cbuffer-vars-discovery shader='%s' var[%u]: %s offset=%u size=%u
          //    totalSize=%u varsCount=%u layoutName='%s' bindPoint=%u"
          // The extension below adds INNER-WALK lines for each non-trivial type-descriptor.
          if (isHeadAnchor) s_dumpedHeadVars = true;
          if (isEyeAnchor)  s_dumpedEyeVars  = true;
      }
  }

D3D11 reflection API the extension uses (read-only reference — well-documented Microsoft API):
  ID3D11ShaderReflectionConstantBuffer *cbuf = ...;    // the reflected SwgVertexConstants
  ID3D11ShaderReflectionVariable *var = cbuf->GetVariableByIndex(i);   // top-level var (e.g. userConstants)
  ID3D11ShaderReflectionType     *type = var->GetType();
  D3D11_SHADER_TYPE_DESC td; type->GetDesc(&td);
  // td.Class    = D3D_SVC_STRUCT | D3D_SVC_VECTOR | D3D_SVC_SCALAR | ...
  // td.Type     = D3D_SVT_FLOAT | D3D_SVT_INT | ...
  // td.Rows, td.Columns, td.Elements (array length, 0 if not array), td.Members (struct member count, 0 if not struct)
  // For struct (Members > 0):
  ID3D11ShaderReflectionType *member = type->GetMemberTypeByIndex(j);
  LPCSTR memberName = type->GetMemberTypeName(j);       // "diffuse", "lighting", ...
  D3D11_SHADER_TYPE_DESC mt; member->GetDesc(&mt);
  // mt.Offset is RELATIVE to the parent struct's start

Build invocation (memory project_decruft_removal_build_graph_gotchas):
  & $env:MSBUILD src\build\win32\swg.sln /p:Configuration=Debug /p:Platform=Win32 /t:Direct3d11:Rebuild /nodeReuse:false /m:1 > build-17-06a-gl11.msbuild.log 2>&1
  Get-Content build-17-06a-gl11.msbuild.log | Select-String -Pattern 'unresolved external symbol' -SimpleMatch | Measure-Object | % Count  # must be 0
  Get-ChildItem stage\gl11_d.dll  # must exist post-build
</interfaces>
</context>

<tasks>

<task type="auto" tdd="false">
  <name>Task 1: Extend the 17-04 Task 2 one-shot discovery dump in Direct3d11_StaticShaderData.cpp to walk userConstants's inner reflection (and any other non-trivial top-level type descriptors); emit Plan 17-06a labelled lines</name>
  <files>src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp</files>
  <read_first>
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp (the discovery-dump block at lines ~925-985; the cached reflection layout structure passed in via m_reflectedCbufferLayouts; the existing per-var emit loop the extension wraps)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp (lines 1040-1090 for the existing D3DReflect / ID3D11ShaderReflectionConstantBuffer call pattern this plan mirrors for the inner walk — the layout cache stores already-reflected metadata; the extension may need direct ID3D11ShaderReflection lookups if the cached struct doesn't carry type descriptors)
    - .planning/phases/17-psrc-census-char-select-beachhead/17-04-SUMMARY.md §"Task 2 cbuffer-vars-discovery dump" + §"patterns-established item 3" (the gated-one-shot-dump pattern)
    - .planning/phases/17-psrc-census-char-select-beachhead/17-VERIFICATION.md §"Per-anchor cbuffer discovery dump" (the falsifying boot evidence — the 9-var top-level layout)
    - .planning/phases/17-psrc-census-char-select-beachhead/17-REVIEWS.md §"Round 4 Verdict" item 2 — Path A dead-end rationale
  </read_first>
  <action>
    1. **Locate the existing discovery-dump block** in `Direct3d11_StaticShaderData.cpp` at the gated `s_dumpedHeadVars` / `s_dumpedEyeVars` site (around lines 925-985 per 17-04 SUMMARY). Do NOT change the gate (`incomplete-write AND first-encounter`); do NOT change the anchor heuristics; do NOT change the top-level per-var emit. Just ADD the inner-walk after the existing per-var emit.

    2. **Determine the reflection-access strategy.** Two options depending on what the cached layout struct (`m_reflectedCbufferLayouts`) carries:

       - **Option (a) — cached layout already has type info.** If the cached `Direct3d11_ReflectedCbufferVar` (or whatever the layout-var struct is named in the live code — read the cache type in 17-02's PSData) carries type descriptors (Class / Type / Members / Elements), the inner walk reads those directly from cached state. NO new D3DReflect call.

       - **Option (b) — cached layout is offset+size+name only.** If the cache stores only `(Name, StartOffset, Size)` and NOT type info, the dump extension must re-run `D3DReflect` on the bound PS bytecode at dump-time to get the type-descriptor info. This is a ONE-SHOT cost (gated by the same first-encounter flag), not per-frame.

       The executor reads the live cached-struct definition (Plan 17-02 territory in PixelShaderProgramData.cpp) and picks Option (a) if type info is cached, Option (b) otherwise. Document the choice in 17-06a SUMMARY.

    3. **Implement the inner walk.** For each top-level reflected var, if its type descriptor is a struct (`td.Members > 0`) OR an array (`td.Elements > 0`), iterate the members/elements and emit one log line each. Output format (matches the existing 17-04 dump style):

       For struct members:
         `Plan 17-06a cbuffer-vars-discovery shader='%s' %s.%s offset=%u size=%u typeClass=%d typeName='%s'`
         where %s.%s is parent-name + member-name (e.g. `userConstants.material`); offset is `parentStartOffset + memberOffset`; size is the member's reflected size.

       For array elements (no member names available):
         `Plan 17-06a cbuffer-vars-discovery shader='%s' %s[%u] offset=%u size=%u typeClass=%d`
         where `[%u]` is the element index; offset is `parentStartOffset + i * elementStride`.

       For NESTED structs (e.g. `userConstants.material.diffuse`), recurse one more level (cap recursion at depth 2 to avoid log-flood from any malformed reflection). Skip any leaf that is a scalar of the same size as its parent (avoids redundant emission).

       Use the same dual-route pattern as the existing dump (`DEBUG_REPORT_LOG_PRINT` + `InfoQueue::AddApplicationMessage`).

    4. **Trim log volume.** The discovery extension fires AT MOST ONCE PER ANCHOR per boot (gated by `s_dumpedHeadVars` / `s_dumpedEyeVars` — UNCHANGED). With 17 inner float4 slots in userConstants worst case, the dump adds ~17 lines per anchor, ~34 lines total per boot. Acceptable budget.

    5. **DO NOT** modify the candidate-chain writes (`wroteDiffuse` / `wroteEmissive` / `wroteSpecular`) in this plan — that's Plan 17-06b's job. This plan is DISCOVERY ONLY. The existing chains continue to miss diffuse + emissive; the dump fires; Kenny captures evidence; 17-06b lands the mapping.

    6. **DO NOT** modify `Direct3d11_PixelShaderProgramData.{cpp,h}`, `Direct3d11_VertexShaderData.{cpp,h}`, `Direct3d11_StateCache.cpp`, `Direct3d11_HlslRewrite.cpp` (READ-ONLY), or any shared/clientGraphics file. **DO NOT** touch the `isCompatibleWithVS` validator or the `selectFallbackPSForVS` path.

    7. **REBUILD:**
       `& $env:MSBUILD src\build\win32\swg.sln /p:Configuration=Debug /p:Platform=Win32 /t:Direct3d11:Rebuild /nodeReuse:false /m:1 > build-17-06a-gl11.msbuild.log 2>&1`
       Verify 0 `unresolved external symbol`. Verify `stage/gl11_d.dll` is fresh. SwgClient_d.exe is NOT rebuilt.

    8. **COMMIT:**
       `git add src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp`
       `git commit -m "diag(17-06a): extend cbuffer-vars-discovery to walk userConstants inner reflection (GAP-2 discovery)"`
  </action>
  <acceptance_criteria>
    - File `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp` has been edited (`git status` shows Modified).
    - The new Plan 17-06a log signature is present: `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'Plan 17-06a' -SimpleMatch | Measure-Object | % Count` >= 1.
    - The inner-walk recursion is present: `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'GetMemberTypeByIndex' -SimpleMatch | Measure-Object | % Count` >= 1 OR `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'D3D11_SHADER_TYPE_DESC' -SimpleMatch | Measure-Object | % Count` >= 1 (whichever API path the executor chose between cached-type-info vs re-reflect).
    - The existing 17-04 Task 2 dump structure is UNCHANGED: `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 's_dumpedHeadVars' -SimpleMatch | Measure-Object | % Count` >= 1 (the existing gate flag is preserved); `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 's_dumpedEyeVars' -SimpleMatch | Measure-Object | % Count` >= 1.
    - The `wroteDiffuse` candidate chain is UNCHANGED in this plan (17-06b lands the mapping): `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'writeVarByName.*material' -SimpleMatch | Measure-Object | % Count` returns the same count as pre-edit baseline (the executor confirms by `git diff` showing the wroteDiffuse / wroteEmissive lines unchanged).
    - The `materialSpecularColor` recognition is preserved: `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'materialSpecularColor' -SimpleMatch | Measure-Object | % Count` returns exactly 1.
    - The build log shows 0 `unresolved external symbol`: `Get-Content build-17-06a-gl11.msbuild.log | Select-String -Pattern 'unresolved external symbol' -SimpleMatch | Measure-Object | % Count` returns 0.
    - `stage/gl11_d.dll` exists post-build: `Get-ChildItem stage/gl11_d.dll` returns >= 1 result.
    - `stage/SwgClient_d.exe` LastWriteTime is unchanged from the post-17-04 build artifact.
    - No file in the must-not-modify list (17-01/02/03/04 PLAN/SUMMARY, 17-CONTEXT/RESEARCH/PATTERNS/VALIDATION/REVIEWS, 17-03-CLOSEOUT, Direct3d9_*.{cpp,h}, ShaderImplementation.{cpp,h}, Direct3d11_PixelShaderProgramData.*, Direct3d11_VertexShaderData.*, Direct3d11_StateCache.cpp, Direct3d11_HlslRewrite.cpp, any Koogie diagnostic patches) is touched: sum of `-SimpleMatch` counts for each is 0: `((git show --stat HEAD | Select-String -Pattern 'Direct3d9_' -SimpleMatch).Count) + ((git show --stat HEAD | Select-String -Pattern 'ShaderImplementation\\.' -SimpleMatch).Count) + ((git show --stat HEAD | Select-String -Pattern 'Direct3d11_PixelShaderProgramData' -SimpleMatch).Count) + ((git show --stat HEAD | Select-String -Pattern 'Direct3d11_VertexShaderData' -SimpleMatch).Count) + ((git show --stat HEAD | Select-String -Pattern 'Direct3d11_StateCache' -SimpleMatch).Count) + ((git show --stat HEAD | Select-String -Pattern 'Direct3d11_HlslRewrite' -SimpleMatch).Count) == 0`.
    - The commit subject contains `diag(17-06a)`: `git log --oneline -1` contains `diag(17-06a)`.
  </acceptance_criteria>
  <verify>
    <automated>Run: `Get-Content build-17-06a-gl11.msbuild.log | Select-String -Pattern 'unresolved external symbol' -SimpleMatch | Measure-Object | % Count` returns 0 AND `Get-ChildItem stage/gl11_d.dll` returns >= 1 result AND `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'Plan 17-06a' -SimpleMatch | Measure-Object | % Count` >= 1 AND `git log --oneline -1` contains `diag(17-06a)`.</automated>
  </verify>
  <done>The 17-04 Task 2 discovery dump is extended to walk userConstants's inner reflection (Path B per Round-4 HIGH-5); gl11_d.dll Debug Win32 rebuilds clean; ready for Kenny's discovery boot (Task 2 checkpoint) to capture the inner-field evidence Plan 17-06b consumes.</done>
</task>

<task type="checkpoint:human-verify" gate="blocking">
  <name>Task 2: Kenny boots the discovery-dump extension; reports userConstants inner-field enumeration to the executor</name>
  <read_first>
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp (the extended discovery dump landed by Task 1)
    - .planning/phases/17-psrc-census-char-select-beachhead/17-04-SUMMARY.md §"Task 2 cbuffer-vars-discovery dump" (the existing dump shape Task 1 extended)
    - .planning/phases/17-psrc-census-char-select-beachhead/17-VERIFICATION.md §"Per-anchor cbuffer discovery dump" (the predecessor evidence from Kenny's 2026-05-28 boot — what the inner walk supplements)
  </read_first>
  <what-built>
    Task 1 extended the existing 17-04 Task 2 one-shot discovery dump to ALSO walk `userConstants`'s inner reflection — emitting one log line per inner float4 field (name + offset + size, or `userConstants[N]` synthetic notation if the reflection is a flat array, plus type-class info).

    Kenny will, on his host:
    1. Deploy the new gl11_d.dll (Task 1 build) into `stage/`.
    2. Confirm `stage/client_d.cfg` is `rasterMajor=11`.
    3. Delete `stage/d3d11-debug.log` (clean slate).
    4. Launch `stage/SwgClient_d.exe`, reach char-select default pose, exit.
    5. Capture stage/d3d11-debug.log and grep for `Plan 17-06a cbuffer-vars-discovery` (the new line prefix Task 1 uses) — paste each unique inner-field line into the resume signal.

    Expected output shape (Kenny pastes verbatim):
      `Plan 17-06a cbuffer-vars-discovery shader='shader/sul_m_head.sht' userConstants.<fieldName0> offset=<N> size=<M> typeClass=<C> typeName='<T>'`
      `Plan 17-06a cbuffer-vars-discovery shader='shader/sul_m_head.sht' userConstants.<fieldName1> offset=<N> size=<M> typeClass=<C> typeName='<T>'`
      ... (max ~17 lines per anchor if flat array; possibly nested if struct)

    From this evidence, Plan 17-06b derives which dotted reflection path corresponds to diffuse and which to emissive.

    6. Confirm D3D9 reference path remains unregressed: re-boot rasterMajor=5 to char-select; no crash. (Standard D-06 gate; no need to screenshot — Plan 17-05 Task 2 captured the D3D9 baseline once.)
  </what-built>
  <how-to-verify>
    1. Confirm `stage/d3d11-debug.log` contains lines matching `Plan 17-06a cbuffer-vars-discovery` (>= 1 if dump fired) — `Select-String -Path stage/d3d11-debug.log -Pattern 'Plan 17-06a cbuffer-vars-discovery' -SimpleMatch | Measure-Object | % Count` >= 1.
    2. Kenny pastes the unique inner-field discovery lines so the executor has the exact dotted-path names (e.g. `userConstants.material.diffuse`, `userConstants.lighting.emissive`, or `userConstants[N]` if flat).
    3. Confirm no boot crash, no D3D9 regression (Kenny re-boots rasterMajor=5 to sanity-check).
  </how-to-verify>
  <acceptance_criteria>
    - Kenny reports at least 1 line matching `Plan 17-06a cbuffer-vars-discovery .* userConstants` from stage/d3d11-debug.log in the resume signal.
    - The pasted evidence includes EITHER named inner fields (`userConstants.<name>`) OR `userConstants[N]` synthetic-index lines — both are acceptable; 17-06b's mapping strategy adapts to whichever the reflection emits.
    - Boot completed cleanly under both rasterMajor=5 (D-06 reference path) and rasterMajor=11 (the test boot).
  </acceptance_criteria>
  <resume-signal>Type "approved" once you've pasted the userConstants inner-field discovery lines (or "blocked — dump did not fire" with the boot log excerpt). After approval, the orchestrator routes to Plan 17-06b to land the mapping commit. Plan 17-05 Task 4 still waits on 17-06b + 17-07 to land before re-capture.</resume-signal>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| NONE | Renderer-internal diagnostic logging. The dump operates on reflected cbuffer metadata produced by D3DCompile/D3DReflect (already-vetted on Plan 17-02's lane), then emits to the existing safe log channel. No new network surface, no auth surface, no file-trust boundary crossed, no IPC. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-17-06a-01 | DoS / log flood | inner-walk recursion | mitigate | One-shot per anchor (s_dumpedHeadVars / s_dumpedEyeVars gate — UNCHANGED from 17-04); recursion capped at depth 2; per-anchor budget ~17 lines worst case for userConstants. No per-frame emission. |
| T-17-06a-02 | OOB read | recursing into a corrupted reflection type descriptor | mitigate | D3D11 reflection API returns null pointers on miss (`GetMemberTypeByIndex` returns nullptr on out-of-range index; `GetMemberTypeName` likewise); the extension null-checks each call before emission. Same defensive pattern as the existing 17-04 dump (which already handles null shader names per the live code's `?` placeholder). |
| T-17-06a-03 | Integrity (input validation per Round-4 security_threat_model framing) | reflected metadata input | mitigate | `GetDesc(&td)` failure is checked via HRESULT; on failure, the recursion bails on that subtree. The Type+Class+Members+Elements fields are bounded by D3D11 reflection's documented invariants (e.g. Members count is bounded by struct size / minimum-field-size). No untrusted external input is parsed — this is engine-produced metadata from D3DCompile. |
| T-17-06a-04 | (D3D9 reference path availability) | (none touched — plugin-local) | accept | No shared/clientGraphics file touched; no Direct3d9_*.{cpp,h} touched; no header touched. D-06 covered by Plan 17-05 Task 4 re-boot. |
| T-17-06a-05 | (per security_enforcement framing) | renderer-internal | accept | ASVS L1 categories do not map. Local single-player renderer; no external attack surface. |
</threat_model>

<verification>
- `Direct3d11_StaticShaderData.cpp` contains the new `Plan 17-06a` log signature (grep returns >= 1).
- The inner-walk implementation calls `GetMemberTypeByIndex` OR `D3D11_SHADER_TYPE_DESC` (grep returns >= 1 in one of those tokens).
- The existing 17-04 gate (s_dumpedHeadVars / s_dumpedEyeVars) is UNCHANGED.
- The existing wroteDiffuse / wroteEmissive / wroteSpecular candidate chains are UNCHANGED in this plan.
- gl11_d.dll Debug Win32 builds with 0 `unresolved external symbol`.
- `stage/SwgClient_d.exe` LastWriteTime is unchanged from the post-17-04 build artifact.
- No file in the must-not-modify list is touched.
- The Plan 17-06a discovery dump fires on Kenny's next boot and produces the inner-field evidence Plan 17-06b consumes.
- D-06 invariant: rasterMajor=5 boots clean (covered by Plan 17-05 Task 4 re-boot + Kenny's checkpoint sanity-check).
</verification>

<success_criteria>
- The discovery half of GAP-2 is closed: Kenny's boot evidence captures `userConstants`'s inner-field layout (dotted reflection paths or synthetic-index lines).
- Plan 17-06b has unambiguous mapping evidence to commit against — no guessing on dotted paths.
- Round-4 HIGH-5 'Path A is a dead end' is honored: this plan IS Path B, defaulted to (not gated behind a Path A attempt).
- The D3D9 reference path is unregressed (D-06).
- Plugin-local: SwgClient.exe is NOT rebuilt; gl11_d.dll Debug Win32 only.
- Shared-header ABI cascade trap avoided: no .h touched.
</success_criteria>

<output>
After Task 2 completes, Plan 17-06a closes with a Plan 17-06a SUMMARY.md mirroring the shape of 17-04-SUMMARY.md (dependency-graph block; tech-tracking key-files; key-decisions including Option (a) vs Option (b) reflection-access strategy; patterns-established; metrics; deviations-from-plan; acceptance-grep table; boot-evidence section populated from Kenny's resume-signal paste — this is the discovery evidence Plan 17-06b reads at its first task).

Create `.planning/phases/17-psrc-census-char-select-beachhead/17-06a-SUMMARY.md` after the discovery boot evidence is captured. Single closing commit:
  `docs(17-06a): Plan 17-06a SUMMARY — userConstants inner-field discovery captured (mapping handed to 17-06b)`
</output>
</plan>
</content>
</invoke>