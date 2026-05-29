---
phase: 17-psrc-census-char-select-beachhead
plan: 06b
type: execute
wave: 7
depends_on: ["17-06a"]
files_modified:
  - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp
autonomous: true
requirements: [CHAR-02, CHAR-03]
gap_closure: true
threat_flags: []
user_setup: []

must_haves:
  truths:
    - "GAP-2 mapping + write closure: Direct3d11_StaticShaderData::apply()'s candidate chains for diffuse + emissive are EXTENDED to land at the dotted reflection paths Plan 17-06a's discovery dump revealed. Implementation uses a NEW companion helper `writeVarFloat4AtOffset(layout, parentName, memberName, absoluteOffset, value)` so writes can target nested-struct or array-element offsets that the existing whole-name-string-compare `writeVarByName` lambda CANNOT reach (per Round-4 review HIGH-1: the existing lambda at Direct3d11_StaticShaderData.cpp:794 does `std::strcmp(var.Name, varName)` against TOP-LEVEL var names only — it does NOT walk nested members; writing to `userConstants.material.diffuse` requires either a full-member-walk lookup or a direct offset-based write)."
    - "Per Round-4 review HIGH-1 (CONVERGED, both reviewers): the existing `writeVarByName` cannot write sub-channels OR nested fields — it iterates `layout.Vars` (the top-level cached var list) and compares `var.Name` whole-string-equal. To write into `userConstants.<member>` per Plan 17-06a's discovery output, this plan ADDS a new helper that takes an EXPLICIT absolute byte-offset (computed from Plan 17-06a's evidence: parent.StartOffset + member.Offset) and writes XMFLOAT4 there with the SAME bounds-check discipline (offset + 16 <= layout.TotalSize). The new helper is a strict superset capability; the existing `writeVarByName` is preserved for the simple top-level cases (textureFactor, textureFactor2, materialSpecularColor)."
    - "Per Round-4 review HIGH-3 (CONVERGED, both reviewers): char-select PSes consume `textureFactor.rgb` + interpolated `COLOR0`, NOT cbuffer `materialDiffuse` / `materialEmissive`. This plan's success criterion is INSTRUMENTATION COMPLETENESS — `wroteDiffuse=1` + `wroteEmissive=1` count > 0 on the next boot — NOT char-select visual delivery. The visual driver for char-select is GAP-3 (17-07's PS input-signature rewrite) + the already-landed textureFactor path. This plan delivers the infrastructure FUTURE open-world shaders (Phase 20+) that DO consume cbuffer material colors will need."
    - "Per RESEARCH §Architectural Responsibility Map + Round-4 review: this work is PLUGIN-LOCAL — Direct3d11_StaticShaderData.cpp ONLY. NO shared `clientGraphics` edits, NO Direct3d9_*.{cpp,h} edits, NO header touches that would trigger the shared-header ABI cascade trap. Pure gl11_d.dll rebuild; SwgClient.exe untouched."
    - "Per 17-04-SUMMARY key-decisions item 5 (single-candidate-per-channel): the EXTENDED candidate chain for diffuse is `writeVarFloat4AtOffset(layout, 'userConstants', '<diffuse-member>', <offset>, passDiffuse) || writeVarByName('material[0]', passDiffuse) || writeVarByName('materialDiffuse', passDiffuse)`. Short-circuit `||`; first hit wins; no fallthrough that could stomp a sibling channel. Same shape for emissive. The new helper is the FIRST candidate (per evidence); the existing scalar fallbacks are retained for forward-compat with PSes that don't `#include` the VS rewrite header."
    - "Per 17-04 patterns-established item 3: the existing one-shot discovery dump (gated on `incomplete-write AND first-encounter`, extended by Plan 17-06a) self-suppresses when `wrote(D,S,E)=(1,1,1)`. After this plan lands, the dump should NOT fire on the next boot — its absence in stage/d3d11-debug.log is positive evidence of GAP-2 mapping closure."
    - "Per D-06: D3D9 reference path unregressed; no shared file touched; both rasterMajor=5 and rasterMajor=11 boot to char-select without crash. Plugin rebuild applies only to gl11_d.dll."
    - "Per CHAR-02 / CHAR-03 (anchor-gated, INSTRUMENTATION ATTRIBUTION): CHAR-02 + CHAR-03 are listed in `requirements` because the eye + head shaders are the ANCHOR shaders the discovery dump + the write chain are gated on (existing s_dumpedHeadVars / s_dumpedEyeVars heuristic). The actual CHAR verdicts are sealed at Plan 17-05 Task 5; this plan only supplies the runtime correctness those verdicts measure for the cbuffer-material-color subset (a subset that, per HIGH-3, does NOT drive char-select visuals — that's GAP-3's job)."
  artifacts:
    - path: "src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp"
      provides: "New `writeVarFloat4AtOffset(layout, parentName, memberName, absoluteOffset, value)` helper (companion to the existing `writeVarByName` lambda at :794); extended candidate chains for diffuse + emissive using the offsets Plan 17-06a's discovery captured; preserves `materialSpecularColor` recognition (04ef976); preserves all existing scalar fallbacks."
      contains: "writeVarFloat4AtOffset"
  key_links:
    - from: "Plan 17-06a boot evidence (Kenny's resume-signal paste — userConstants inner-field names + offsets)"
      to: "Direct3d11_StaticShaderData.cpp diffuse + emissive candidate chains (this plan's edit)"
      via: "evidence-grounded mapping commit; the offsets are derived from Plan 17-06a's dump output, not guessed"
      pattern: "writeVarFloat4AtOffset"
    - from: "writeVarFloat4AtOffset bounds-check (offset + 16 <= layout.TotalSize)"
      to: "writeVarByName existing bounds-check (:806 — var.StartOffset + sizeof(XMFLOAT4) > layout.TotalSize -> false)"
      via: "mirror the same defensive pattern; new helper has its own bounds check using EXPLICIT offset arg"
      pattern: "layout.TotalSize"
    - from: "post-17-06b boot evidence (Plan 17-05 Task 4 grep set)"
      to: "Plan 17-05 Task 5 17-VERIFICATION.md authoring"
      via: "wroteDiffuse=1 + wroteEmissive=1 counts feed the CHAR-02 + CHAR-03 INSTRUMENTATION column of the per-requirement verdict (the VISUAL column is independent — per HIGH-3)"
      pattern: "wroteDiffuse=1"
---

<objective>
Close the MAPPING half of GAP-2: extend `Direct3d11_StaticShaderData::apply()`'s writeVarByName candidate chains for diffuse + emissive to land at the dotted reflection paths Plan 17-06a's discovery dump captured. Implementation requires a NEW helper `writeVarFloat4AtOffset` because per Round-4 HIGH-1 the existing `writeVarByName` lambda at line 794 can only compare WHOLE top-level var names — it cannot reach nested members (e.g. `userConstants.material.diffuse`) or array elements (e.g. `userConstants[N]`).

Purpose: Plan 17-06a delivered the discovery dump extension and captured Kenny's boot evidence for `userConstants`'s inner-field layout. This plan consumes that evidence to commit the actual write path. Per Round-4 review HIGH-3 (CONVERGED, both reviewers), GAP-2 closure is INSTRUMENTATION COMPLETENESS — `wroteDiffuse=1` and `wroteEmissive=1` count > 0 on the next boot — NOT char-select visual delivery. Char-select PSes consume `textureFactor.rgb` + interpolated `COLOR0`, not cbuffer material colors; the visual driver is Plan 17-07's bind-rate fix.

Per Round-4 HIGH-1 (CONVERGED, both reviewers), the prior plan's `packedRegisterN.{xyzw}` channel-write text was implementable only with either:
  (a) a new helper that takes EXPLICIT byte-offsets (this plan's choice — `writeVarFloat4AtOffset`), or
  (b) a rescope to whole-var-name writes only (which fails GAP-2 because the diffuse/emissive landing site is nested INSIDE userConstants, not a top-level named var).
This plan picks (a): adds the helper, extends the candidate chains, preserves the existing scalar fallbacks. Documented in 17-06b SUMMARY.

This is an AUTONOMOUS plan — no checkpoint. The discovery checkpoint already happened in Plan 17-06a Task 2; this plan reads Plan 17-06a SUMMARY's captured evidence section to derive the offset arguments. If Plan 17-06a SUMMARY does not yet exist when this plan runs (orchestrator timing), the plan's read_first surfaces the dependency and the executor blocks gracefully — no infinite retry.

Output: a single source edit to Direct3d11_StaticShaderData.cpp adding the `writeVarFloat4AtOffset` helper + extending the wroteDiffuse + wroteEmissive candidate chains; a rebuilt gl11_d.dll Debug Win32 (0 unresolved external symbols); single atomic commit; no shared / clientGraphics / D3D9 touch.
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
@.planning/phases/17-psrc-census-char-select-beachhead/17-06a-SUMMARY.md
@.planning/phases/17-psrc-census-char-select-beachhead/17-04-SUMMARY.md

## What's true now (the evidence this plan consumes)

Plan 17-06a SUMMARY.md captures Kenny's boot evidence for the `userConstants` inner-field layout. Expected shape (the actual names + offsets come from 17-06a's resume-signal paste; the executor reads them verbatim):

```
Plan 17-06a cbuffer-vars-discovery shader='shader/sul_m_head.sht' userConstants.<diffuseMember> offset=<absOffset> size=<size> typeClass=<C> typeName='<T>'
Plan 17-06a cbuffer-vars-discovery shader='shader/sul_m_head.sht' userConstants.<emissiveMember> offset=<absOffset> size=<size> typeClass=<C> typeName='<T>'
... (other inner fields)
```

Each absolute offset = userConstants.StartOffset (=128 per Kenny's 2026-05-28 dump) + member's relative offset within the userConstants struct.

If 17-06a's evidence shows `userConstants` is reflected as a FLAT float4[17] array (rather than a named struct), the dotted paths become `userConstants[N]` and the absolute offsets are `128 + N*16`. In that case the executor cannot uniquely identify which N is diffuse and which is emissive from D3D11 reflection alone — the engine source (`Direct3d11.cpp::setVertexShaderUserConstants` or the legacy D3D9 c-register layout map) is the secondary source of truth. The executor reads that source as a fallback identification path (acceptable — `setVertexShaderUserConstants` is engine code, NOT asset data, so it's deterministic).

If 17-06a's evidence shows `userConstants` as opaque (no inner enumeration possible due to a reflection limitation — e.g. the layout is a `struct UserConstants` with no `cbuffer` ancestry that gives reflection visibility), this plan adapts to either:
  - rescope to "no mapping found; GAP-2 closes as DEFERRED" — the discovery dump itself proves the work was attempted; the verdict in Plan 17-05 Task 5 documents the deferral
  - or write to the most plausible packedRegister channel based on the legacy D3D9 c-register layout (engine source: `Direct3d11_HlslRewrite.cpp` :758-766 cites the input form — `materialDiffuseColor : register(c11)` per the conventional D3D9 fixed-function layout)

The executor decides at execution time based on what 17-06a actually delivered. The plan does NOT pretend the mapping is known in advance; the FRONTMATTER acknowledges this is evidence-driven.

## Why a new helper is required (HIGH-1)

The existing `writeVarByName` lambda at `Direct3d11_StaticShaderData.cpp:794` (read this session — verbatim):
  ```
  auto writeVarByName = [&](char const *varName, DirectX::XMFLOAT4 const &value) -> bool
  {
      for (auto const &var : layout.Vars)
      {
          if (std::strcmp(var.Name, varName) != 0) continue;
          if (var.Size < sizeof(DirectX::XMFLOAT4)) return false;
          if (var.StartOffset + sizeof(DirectX::XMFLOAT4) > layout.TotalSize) return false;
          std::memcpy(staging.data() + var.StartOffset, &value, sizeof(DirectX::XMFLOAT4));
          return true;
      }
      return false;
  };
  ```

`layout.Vars` is the TOP-LEVEL cached var list (the 9 vars per Kenny's discovery dump). The lambda has no notion of nested members. To write into `userConstants.<member>` the lookup must either:
  - walk top-level vars looking for `userConstants`, then walk its type descriptor's members (expensive; requires the cached layout to carry type info OR re-reflect per-write)
  - take an EXPLICIT absolute byte-offset that Plan 17-06a's evidence already captured, and write there directly (cheap; this plan's choice)

The new helper signature:
  ```
  auto writeVarFloat4AtOffset = [&](unsigned int absoluteOffset, DirectX::XMFLOAT4 const &value) -> bool
  {
      if (absoluteOffset + sizeof(DirectX::XMFLOAT4) > layout.TotalSize) return false;
      std::memcpy(staging.data() + absoluteOffset, &value, sizeof(DirectX::XMFLOAT4));
      return true;
  };
  ```

Same bounds-check posture as `writeVarByName`. Same `staging` byte buffer. Same return semantic. ONLY difference: no name-string lookup — caller supplies the offset.

A nicety: the caller-side strings (the offsets per anchor) become magic numbers without a name. To preserve readability + log-traceability, the call sites use a wrapper inline comment naming the source dotted path (per Plan 17-06a evidence). The dual-route log emits the path name on success for cross-reference with the discovery dump.

## Critical landmines (pulled from memory + 17-VERIFICATION.md cross-cutting)

- **D3D9 reference path NEVER regresses** (memory + D-06). This plan touches Direct3d11_StaticShaderData.cpp ONLY.
- **Shared-header ABI cascade trap** (memory project_shared_header_abi_cascade_trap). No header touched.
- **Don't re-enable per-pass blend factors early** (Iter-44C regression). N/A.
- **`isCompatibleWithVS` validator + selectFallbackPSForVS are SETTLED** (17-04). N/A for this plan.
- **Don't modify Koogie's source-level changes without strong reason** (memory feedback_dont_modify_koogie_changes). The writeVarByName candidate chain at lines 897-906 is Plan 17-04 territory; extending it is in-bounds.
- **Use `$env:MSBUILD` + `/nodeReuse:false`** (memory project_decruft_removal_build_graph_gotchas). PowerShell, not Git Bash.
- **Magenta fallback PS is a visible diagnostic — keep it**. Untouched.

<interfaces>
<!-- Live extract from Direct3d11_StaticShaderData.cpp — 17-04 + 04ef976 + 17-06a state. -->

Existing writeVarByName lambda (Direct3d11_StaticShaderData.cpp line ~794 — UNCHANGED by this plan):
  auto writeVarByName = [&](char const *varName, DirectX::XMFLOAT4 const &value) -> bool
  {
      for (auto const &var : layout.Vars)
      {
          if (std::strcmp(var.Name, varName) != 0) continue;
          if (var.Size < sizeof(DirectX::XMFLOAT4)) return false;
          if (var.StartOffset + sizeof(DirectX::XMFLOAT4) > layout.TotalSize) return false;
          std::memcpy(staging.data() + var.StartOffset, &value, sizeof(DirectX::XMFLOAT4));
          return true;
      }
      return false;
  };

New writeVarFloat4AtOffset lambda (Direct3d11_StaticShaderData.cpp — ADDED by this plan, sibling to writeVarByName):
  auto writeVarFloat4AtOffset = [&](unsigned int absoluteOffset, DirectX::XMFLOAT4 const &value) -> bool
  {
      // Per Round-4 review HIGH-1: companion helper for nested-member / array-element
      // offset writes. The existing writeVarByName lambda above iterates top-level
      // layout.Vars and cannot reach into userConstants.<member>. Plan 17-06a's
      // discovery dump captured the absolute offsets; this helper writes directly there.
      if (absoluteOffset + sizeof(DirectX::XMFLOAT4) > layout.TotalSize) return false;
      std::memcpy(staging.data() + absoluteOffset, &value, sizeof(DirectX::XMFLOAT4));
      return true;
  };

Existing post-04ef976 candidate chains (Direct3d11_StaticShaderData.cpp lines 897-906 — EXTENDED by this plan):
  bool const wroteDiffuse  =
      writeVarFloat4AtOffset(<diffuse-offset-from-17-06a>, passDiffuse)   // ← NEW: per 17-06a evidence; offset = userConstants.StartOffset + <member>.Offset
      || writeVarByName("material[0]",   passDiffuse)                     // ← 17-04 hypothesis (retained for fwd compat)
      || writeVarByName("materialDiffuse",  passDiffuse);                 // ← pre-17-04 scalar (retained for PSes not using VS rewrite header)

  bool const wroteSpecular =
      writeVarByName("materialSpecularColor", passSpecular)               // ← 04ef976 (UNCHANGED — preserved)
      || writeVarByName("material[1]",   passSpecular)
      || writeVarByName("materialSpecular", passSpecular);

  bool const wroteEmissive =
      writeVarFloat4AtOffset(<emissive-offset-from-17-06a>, passEmissive) // ← NEW: per 17-06a evidence
      || writeVarByName("material[2]",   passEmissive)                    // ← 17-04 hypothesis (retained)
      || writeVarByName("materialEmissive", passEmissive);                // ← pre-17-04 scalar (retained)

If 17-06a's evidence shows userConstants as flat-array reflection, the offsets are literal expressions:
  unsigned int const kUserConstantsBaseOffset = 128;      // from Kenny's 2026-05-28 dump (userConstants.StartOffset)
  unsigned int const kDiffuseSlotIndex  = <N from 17-06a or engine source>;
  unsigned int const kEmissiveSlotIndex = <M from 17-06a or engine source>;
  // call sites become writeVarFloat4AtOffset(kUserConstantsBaseOffset + kDiffuseSlotIndex * 16, passDiffuse) etc.

Optional success-attribution log (mirror 17-04 dual-route pattern):
  // On wroteDiffuse success via writeVarFloat4AtOffset path, emit ONCE per anchor (dedupe set):
  //   "Plan 17-06b: wroteDiffuse=1 via writeVarFloat4AtOffset(absOffset=%u) for shader='%s'"
  // Helps Plan 17-05 Task 5's per-CHAR-0x verdict attribute the mechanism.

Build invocation (memory project_decruft_removal_build_graph_gotchas):
  & $env:MSBUILD src\build\win32\swg.sln /p:Configuration=Debug /p:Platform=Win32 /t:Direct3d11:Rebuild /nodeReuse:false /m:1 > build-17-06b-gl11.msbuild.log 2>&1
  Get-Content build-17-06b-gl11.msbuild.log | Select-String -Pattern 'unresolved external symbol' -SimpleMatch | Measure-Object | % Count  # must be 0
  Get-ChildItem stage\gl11_d.dll  # must exist post-build
</interfaces>
</context>

<tasks>

<task type="auto" tdd="false">
  <name>Task 1: Add writeVarFloat4AtOffset helper; extend wroteDiffuse + wroteEmissive candidate chains with evidence-grounded offsets from Plan 17-06a; emit Plan 17-06b dual-route attribution log on success</name>
  <files>src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp</files>
  <read_first>
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp (the writeVarByName lambda at line ~794; the current candidate chains at lines 897-906; the existing one-shot discovery dump at lines ~925-985 extended by Plan 17-06a)
    - .planning/phases/17-psrc-census-char-select-beachhead/17-06a-SUMMARY.md (REQUIRED — Plan 17-06a's captured boot evidence drives this plan's offset arguments; if this SUMMARY doesn't yet exist, BLOCK with a clear status message — do NOT proceed by guessing)
    - .planning/phases/17-psrc-census-char-select-beachhead/17-04-SUMMARY.md §"key-decisions item 5" (single-candidate-per-channel discipline)
    - .planning/phases/17-psrc-census-char-select-beachhead/17-REVIEWS.md §"Round 4 Verdict" item 2 — HIGH-1 helper-required rationale; HIGH-3 instrumentation-vs-visual scope framing
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp (around lines 660-694 — `setPixelShaderUserConstants_impl` / `setVertexShaderUserConstants_impl` — SECONDARY source of truth IF Plan 17-06a evidence shows userConstants as a flat array and the executor needs engine-source confirmation of which slot index corresponds to diffuse / emissive)
  </read_first>
  <action>
    1. **VERIFY PRECONDITION:** Confirm `.planning/phases/17-psrc-census-char-select-beachhead/17-06a-SUMMARY.md` exists and contains a populated boot-evidence section with `userConstants.<member>` (or `userConstants[N]`) line(s). If it does NOT, halt this plan with status `BLOCKED — 17-06a-SUMMARY missing or empty; orchestrator should re-run Plan 17-06a Task 2 checkpoint first`. Do NOT proceed by guessing — the WHOLE POINT of the 06a/06b split is evidence-grounded mapping.

    2. **DERIVE OFFSETS** from Plan 17-06a's captured evidence. Three possible cases:

       - **Case (A) — named members.** Evidence shows lines like `userConstants.diffuse offset=140 size=16` and `userConstants.emissive offset=156 size=16`. The offsets are LITERAL (absolute, already computed by the discovery dump). Use them directly in the candidate-chain edit.

       - **Case (B) — flat float4 array.** Evidence shows lines like `userConstants[7] offset=240 size=16` and `userConstants[8] offset=256 size=16` (no member names). The executor cannot uniquely identify which N is diffuse vs emissive from D3D11 reflection alone. Fall back to the secondary source: read `Direct3d11.cpp::setVertexShaderUserConstants_impl` (around lines 660-694) for the engine-side write order — it documents which slot the engine fills with which value per the legacy D3D9 c-register layout. Cross-reference to derive N for diffuse and N for emissive.

       - **Case (C) — opaque / no inner enumeration.** Evidence shows that the discovery dump fired but produced no `userConstants.*` inner lines (e.g. the reflection treats userConstants as a single opaque chunk with no member or element info — possible if it was declared as a `cbuffer userConstants` instead of as a `struct UserConstants` field). The executor takes the "GAP-2 closes as DEFERRED" path: do NOT modify the candidate chain (the existing chain stays — no false-positive write to a wrong offset); document in 17-06b SUMMARY that GAP-2's MAPPING half could not be closed due to reflection-opacity; the build still ships (no source edit other than to add the writeVarFloat4AtOffset helper as latent infrastructure for future use).

    3. **ADD the writeVarFloat4AtOffset helper** as a lambda inside `Direct3d11_StaticShaderData::apply()` — sibling to the existing `writeVarByName` lambda at line ~794. Place it IMMEDIATELY AFTER `writeVarByName`'s closing brace. Signature + body verbatim per the interfaces block. The bounds check mirrors writeVarByName's: `absoluteOffset + sizeof(XMFLOAT4) > layout.TotalSize` → return false.

    4. **EXTEND the wroteDiffuse + wroteEmissive candidate chains** (Case A or B only — Case C skips this step). At lines 897-906 region:

       - Insert `writeVarFloat4AtOffset(<derived-diffuse-offset>, passDiffuse)` AHEAD of the existing `writeVarByName("material[0]", ...)` candidate in wroteDiffuse.
       - Insert `writeVarFloat4AtOffset(<derived-emissive-offset>, passEmissive)` AHEAD of the existing `writeVarByName("material[2]", ...)` candidate in wroteEmissive.
       - PRESERVE the existing scalar fallbacks (`material[0/2]`, `materialDiffuse/Emissive`) — single-candidate-per-channel discipline; short-circuit `||`; first hit wins.
       - DO NOT TOUCH the wroteSpecular chain — `materialSpecularColor` recognition from 04ef976 stays exactly as-is.

    5. **ADD a Plan 17-06b dual-route attribution log** on writeVarFloat4AtOffset success (one-shot per anchor, dedupe set keyed on shader-name):
       - Format: `"Plan 17-06b: wroteDiffuse=1 via writeVarFloat4AtOffset(absOffset=%u) for shader='%s' (per 17-06a evidence: %s)"`
       - The `%s` for "per 17-06a evidence" cites the dotted path from 17-06a evidence — e.g. `userConstants.diffuse` or `userConstants[7]`.
       - Dual-route: DEBUG_REPORT_LOG_PRINT + InfoQueue::AddApplicationMessage. Same dedupe pattern as 17-04's `static std::set<std::pair<...>>`.
       - Same for emissive on its writeVarFloat4AtOffset success.
       - This log gives Plan 17-05 Task 5 the mechanism-attribution it needs (per-CHAR-0x verdict cites "diffuse landed via writeVarFloat4AtOffset" vs "wrote nothing — DEFERRED").

    6. **DO NOT MODIFY:**
       - Any header file (shared-header ABI trap)
       - `Direct3d11_PixelShaderProgramData.{cpp,h}` (Plan 17-07 territory)
       - `Direct3d11_VertexShaderData.{cpp,h}`
       - `Direct3d11_StateCache.cpp`
       - `Direct3d11_HlslRewrite.cpp` (READ-ONLY)
       - Any shared/clientGraphics file
       - Any Direct3d9_*.{cpp,h} file
       - The `wroteSpecular` chain (materialSpecularColor — Koogie's 04ef976 must be preserved)
       - The `isCompatibleWithVS` validator or the `selectFallbackPSForVS` path
       - Any Koogie diagnostic patch elsewhere in src/engine/

    7. **REBUILD:**
       `& $env:MSBUILD src\build\win32\swg.sln /p:Configuration=Debug /p:Platform=Win32 /t:Direct3d11:Rebuild /nodeReuse:false /m:1 > build-17-06b-gl11.msbuild.log 2>&1`
       Verify 0 `unresolved external symbol`. Verify `stage/gl11_d.dll` is fresh. SwgClient_d.exe is NOT rebuilt.

    8. **COMMIT:**
       - Case A or B: `git add ...StaticShaderData.cpp; git commit -m "feat(17-06b): writeVarFloat4AtOffset helper + diffuse/emissive map per 17-06a evidence (closes GAP-2 mapping)"`
       - Case C: `git add ...StaticShaderData.cpp; git commit -m "feat(17-06b): writeVarFloat4AtOffset latent helper; GAP-2 mapping DEFERRED — userConstants reflection opaque"`
  </action>
  <acceptance_criteria>
    - File `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp` has been edited (`git status` Modified).
    - The new `writeVarFloat4AtOffset` helper exists: `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'writeVarFloat4AtOffset' -SimpleMatch | Measure-Object | % Count` >= 1 (Case C minimum: declaration only) or >= 3 (Case A/B: declaration + diffuse call + emissive call).
    - Case A/B: the wroteDiffuse candidate chain begins with writeVarFloat4AtOffset — `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'wroteDiffuse' -Context 0,4 | Out-String` contains `writeVarFloat4AtOffset` BEFORE any `material[0]` or `materialDiffuse` line in the same chain (executor verifies visually in diff review; the grep gate above is the count proxy).
    - Case A/B: the wroteEmissive candidate chain begins with writeVarFloat4AtOffset — same shape.
    - Case A/B: Plan 17-06b dual-route attribution log signature present — `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'Plan 17-06b' -SimpleMatch | Measure-Object | % Count` >= 1.
    - The `wroteSpecular` chain is UNCHANGED: `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'materialSpecularColor' -SimpleMatch | Measure-Object | % Count` returns exactly 1.
    - The existing writeVarByName lambda is UNCHANGED (no signature edit): `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'auto writeVarByName' -SimpleMatch | Measure-Object | % Count` returns exactly 1.
    - The Plan 17-06a discovery dump from 17-06a Task 1 is UNCHANGED in this plan (it's 17-06a's code; this plan extends the WRITE chain, not the discovery dump): `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'Plan 17-06a' -SimpleMatch | Measure-Object | % Count` >= 1 (preserved).
    - The build log shows 0 `unresolved external symbol`: `Get-Content build-17-06b-gl11.msbuild.log | Select-String -Pattern 'unresolved external symbol' -SimpleMatch | Measure-Object | % Count` returns 0.
    - `stage/gl11_d.dll` exists post-build: `Get-ChildItem stage/gl11_d.dll` returns >= 1 result.
    - `stage/SwgClient_d.exe` LastWriteTime is unchanged from the post-17-04 build artifact.
    - No file in the must-not-modify list is touched: `((git show --stat HEAD | Select-String -Pattern 'Direct3d9_' -SimpleMatch).Count) + ((git show --stat HEAD | Select-String -Pattern 'ShaderImplementation\\.' -SimpleMatch).Count) + ((git show --stat HEAD | Select-String -Pattern 'Direct3d11_PixelShaderProgramData' -SimpleMatch).Count) + ((git show --stat HEAD | Select-String -Pattern 'Direct3d11_VertexShaderData' -SimpleMatch).Count) + ((git show --stat HEAD | Select-String -Pattern 'Direct3d11_StateCache' -SimpleMatch).Count) + ((git show --stat HEAD | Select-String -Pattern 'Direct3d11_HlslRewrite' -SimpleMatch).Count) == 0`.
    - The commit subject contains `feat(17-06b)`: `git log --oneline -1` contains `feat(17-06b)`.
  </acceptance_criteria>
  <verify>
    <automated>Run: `Get-Content build-17-06b-gl11.msbuild.log | Select-String -Pattern 'unresolved external symbol' -SimpleMatch | Measure-Object | % Count` returns 0 AND `Get-ChildItem stage/gl11_d.dll` returns >= 1 result AND `Select-String -Path src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp -Pattern 'writeVarFloat4AtOffset' -SimpleMatch | Measure-Object | % Count` >= 1 AND `git log --oneline -1` contains `feat(17-06b)`.</automated>
  </verify>
  <done>writeVarFloat4AtOffset helper added; wroteDiffuse + wroteEmissive candidate chains extended per Plan 17-06a evidence (Case A/B) OR latent helper added with DEFERRED documentation (Case C); Plan 17-06b dual-route attribution log emits per anchor on success; gl11_d.dll Debug Win32 rebuilt clean; commit landed; ready for Plan 17-05 Task 4 to measure `wroteDiffuse=1` + `wroteEmissive=1` closure (or document DEFERRED status if Case C).</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| NONE | Renderer-internal cbuffer-naming / upload-side variable lookup with an explicit-offset write helper. Same trust posture as Plan 17-06a (which is the discovery half of the same work). The new helper's only difference from writeVarByName is taking an offset literal vs a name string; the bounds check enforces the same `offset + 16 <= layout.TotalSize` invariant. No new trust boundary crossed. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-17-06b-01 | Tampering / integrity | writeVarFloat4AtOffset OOB write | mitigate | Bounds check is the FIRST statement: `if (absoluteOffset + sizeof(XMFLOAT4) > layout.TotalSize) return false`. Mirrors writeVarByName's defensive posture (Direct3d11_StaticShaderData.cpp:806). The offset literals in call sites come from Plan 17-06a's CAPTURED reflection evidence (D3DReflect-emitted offsets) or from engine source for the legacy D3D9 layout (deterministic engine code), NOT from untrusted external input. Per Round-4 security_threat_model framing: input-validation guard is inline + load-bearing. |
| T-17-06b-02 | DoS / wrong-data | misidentified offset (wrong slot for diffuse vs emissive) | mitigate | Single-candidate-per-channel discipline preserves the safety net: if the new helper writes to a wrong offset, the existing scalar fallbacks (`material[N]`, `materialDiffuse/Emissive`) are still attempted — and per Round-4 HIGH-3, char-select PSes don't consume cbuffer material colors anyway, so even a wrong-slot write cannot corrupt char-select visuals. The discovery dump (17-06a's extension) self-suppresses when `wrote(D,S,E)=(1,1,1)` — so if wroteDiffuse comes back 0 despite the new helper call, the dump KEEPS firing on the NEXT boot, surfacing the failure. |
| T-17-06b-03 | (D3D9 reference path availability) | (none touched — plugin-local) | accept | Plugin-local. D-06 covered by Plan 17-05 Task 4 re-boot. |
| T-17-06b-04 | (per security_enforcement framing) | renderer-internal | accept | ASVS L1 categories do not map. Local single-player renderer. |
</threat_model>

<verification>
- `Direct3d11_StaticShaderData.cpp` contains the new `writeVarFloat4AtOffset` helper (grep returns >= 1 match for the identifier).
- Case A/B: the wroteDiffuse + wroteEmissive candidate chains start with writeVarFloat4AtOffset calls; pre-existing scalar fallbacks retained behind `||`.
- Case A/B: Plan 17-06b dual-route attribution log emits per anchor on success.
- Case C: latent writeVarFloat4AtOffset helper added; chains unchanged; SUMMARY documents DEFERRED status with the reflection-opacity rationale.
- writeVarByName lambda is UNCHANGED (no signature touch — preserves Plan 17-04 contract).
- materialSpecularColor recognition preserved (04ef976 unchanged).
- The 17-06a discovery dump remains in the file (`Plan 17-06a` token grep >= 1).
- gl11_d.dll Debug Win32 builds with 0 `unresolved external symbol`.
- `stage/SwgClient_d.exe` LastWriteTime unchanged.
- No file in the must-not-modify list is touched.
- D-06 invariant: Plan 17-05 Task 4 confirms rasterMajor=5 still boots cleanly after this plan's deploy.
</verification>

<success_criteria>
- The mapping half of GAP-2 is closed (Case A/B) or formally deferred with documented rationale (Case C).
- Round-4 HIGH-1 'writeVarByName cannot write sub-channels' is closed: a new writeVarFloat4AtOffset helper bypasses the whole-name-string constraint with an explicit-offset write primitive.
- Round-4 HIGH-3 'GAP-2 may not move CHAR visuals' is honored in the success-criterion phrasing: this plan's outcome is INSTRUMENTATION COMPLETENESS, not char-select visual delivery — the visual driver is Plan 17-07.
- Round-4 HIGH-5 'Path A is a dead end' is honored: this plan + 17-06a together constitute the Path B path, defaulted to.
- The mapping is DISCOVERED from evidence (Plan 17-06a's boot dump) or, if reflection is opaque, formally deferred — NOT guessed.
- The D3D9 reference path is unregressed (D-06).
- Plugin-local: SwgClient.exe is NOT rebuilt; gl11_d.dll Debug Win32 only.
</success_criteria>

<output>
After Task 1 completes, Plan 17-06b closes with a Plan 17-06b SUMMARY.md mirroring the shape of 17-04-SUMMARY.md (dependency-graph block; tech-tracking key-files; key-decisions including which Case (A/B/C) the evidence drove + the offsets derived; patterns-established — writeVarFloat4AtOffset as the companion to writeVarByName; metrics; deviations-from-plan; acceptance-grep table; boot-evidence placeholder pointing to Plan 17-05 Task 4 for the POST-gap capture's wroteDiffuse=1 + wroteEmissive=1 measurement).

Create `.planning/phases/17-psrc-census-char-select-beachhead/17-06b-SUMMARY.md` after the source commit lands. Single closing commit:
  `docs(17-06b): Plan 17-06b SUMMARY — diffuse/emissive mapping landed [Case A|B|C] (GAP-2 mapping closure handed to Plan 17-05 Task 4 measurement)`
</output>
</plan>
</content>
</invoke>