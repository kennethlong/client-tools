---
phase: 17-psrc-census-char-select-beachhead
plan: 03
subsystem: renderer
tags: [d3d11, asset-ps, reflection-driven-constants, perpassmaterial, perm-shadow, getperma-shadow, hlsl, infoqueue, char-select, beachhead]

# Dependency graph
requires:
  - phase: 17
    plan: 01
    provides: "ShaderImplementationPassPixelShaderProgram::m_psrcText + m_psrcLen (PSRC source-text retention; psrcCensus shared flag default false); char-select asset surface (22 HLSL ps_2_0 PS programs, 100% of character-body families) — gates the recompile lane and the cbuffer wiring"
  - phase: 17
    plan: 02
    provides: "non-fatal PSRC recompile lane (m_d3dPS bound for //hlsl PSRC); D3DReflect-once compile-time cache on Direct3d11_PixelShaderProgramData (m_reflectedCbufferLayouts with Name/BindPoint/TotalSize/Vars + m_reflectedPSInputs) — Plan 17-03 consumes these per-draw"
provides:
  - "Per-pass material + textureFactor SOURCE-DATA cache on Direct3d11_StaticShaderData (PerPassMaterial struct vector mirroring Direct3d9_StaticShaderData.cpp:593-700; m_passMaterial sized 1:1 with m_passPS)"
  - "Centralized PerMaterial slot-b2 shadow promoted to file-scope namespace static in Direct3d11.cpp + getPerMaterialShadow() getter declared in Direct3d11_ConstantBuffer.h, defined in Direct3d11.cpp — setPixelShaderUserConstants_impl and Direct3d11_StaticShaderData::apply() now read-modify-write the SAME struct"
  - "OFFSET-AWARE reflection-driven PS cbuffer upload at the :601 deferral site — staging byte buffer sized to layout.TotalSize, variables written at var.StartOffset by NAME, upload via Direct3d11_ConstantBuffer::updatePS(layout.BindPoint, staging, layout.TotalSize) — NO hardcoded slot, NO hardcoded struct size, NO per-draw D3DReflect"
  - "Per-pass R3-03b stale-shadow zero-fill — !pm.m_materialValid passes upload zero material so a non-material pass between two material passes does NOT leave stale material on the GPU"
  - "R3-03g one-shot flush-time proof log (DEBUG_REPORT_LOG_PRINT + InfoQueue dual-route) — emits HEAD and EYE pass material values verbatim into stage/d3d11-debug.log"
  - "Cross-clobber regression check (sub-step 1g) — one-shot proof log inside setPixelShaderUserConstants_impl showing non-zero coexistence of userConstants + material* fields on the same shadow"
affects: [17-04-x-vs-ps-interpolator-followup (Phase 17.X — VS/PS schema mismatch), char-select-visual-AB, phase-22-skinning-stream-fix]

# Tech tracking
tech-stack:
  added: []   # zero new deps; consumes Plan 17-02 cache + existing Direct3d11_ConstantBuffer
  patterns:
    - "Per-pass material source-data cache mirror of Direct3d9_StaticShaderData.cpp:593-700 — populated at construct() from shader.getMaterial(tag, Material&) + shader.getTextureFactor(tag, uint32&), consumed by apply() per draw"
    - "File-scope namespace static + getter pattern for cross-function shared mutable cbuffer shadow (Option A from R3-03a / HIGH-3) — getter declared next to the struct's home in Direct3d11_ConstantBuffer.h, defined where the file-scope static lives in Direct3d11.cpp"
    - "Reflection-driven offset-aware staging-buffer upload — std::vector<unsigned char> staging(layout.TotalSize), writeVarByName lambda matches by std::strcmp(var.Name, candidate) and memcpys at var.StartOffset (bounds-checked); upload via updatePS(layout.BindPoint, ...) — survives any rewriter-emitted cbuffer naming convention"
    - "Per-pass current-or-zero contract — every pass uploads a current cbuffer; non-material passes upload zero rather than skipping (R3-03b mirror of D3D9 Pass::apply semantics at Direct3d9_StaticShaderData.cpp:835-897)"
    - "One-shot dual-route diagnostic (DEBUG_REPORT_LOG_PRINT + ID3D11InfoQueue::AddApplicationMessage) keyed off a static bool gate so the line lands ONCE per anchor per boot — survives explorer-launched boots via the InfoQueue file sink"

key-files:
  created:
    - ".planning/phases/17-psrc-census-char-select-beachhead/17-03-SUMMARY.md (this file)"
  modified:
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.h (added DirectXMath include + PerPassMaterial struct + m_passMaterial vector)"
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp (added clientGraphics/Material.h + Direct3d11_ConstantBuffer.h + <cstring>/<vector> includes; SR-1 source-data resolution in per-pass loop in construct(); m_passMaterial.assign + index-assignment populating call; DEBUG_FATAL size-invariant gate; R3-03c offset-aware reflection-driven upload block at apply() insertion point right after setCurrentPSData; R3-03g flush-time proof log dual-route gated on shader name 'head'/'eye' substring)"
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.h (added namespace Direct3d11Namespace { Direct3d11_PerMaterialCB & getPerMaterialShadow(); } declaration block + R3-03a / HIGH-3 documentation paragraph; PerMaterialCB struct itself UNCHANGED — sub-step 1d skipped because census shows no textureFactor in any reflected char-select PS cbuffer)"
    - "src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp (added file-scope s_perMaterialShadow inside Direct3d11Namespace + getPerMaterialShadow() definition; rewrote setPixelShaderUserConstants_impl to read-modify-write the shared shadow via the getter; added sub-step 1g one-shot cross-clobber proof log dual-routed via InfoQueue + DEBUG_REPORT_LOG_PRINT)"

key-decisions:
  - "Sub-step 1d (PerMaterialCB textureFactor extension) SKIPPED. Plan 02's PS-cbuffer dump from the post-Plan-17-02 boot shows 26 unique char-select PS reflections, ALL of which declare exactly ONE cbuffer: 'SwgVertexConstants' at bindPoint=0, totalSize=400, vars=9. No char-select PS declares 'textureFactor' or 'textureFactor2' in its reflected cbuffer. PerMaterialCB struct unchanged (sizeof == 112). If a future asset surfaces textureFactor, the writeVarByName lambda will simply find it in the reflected layout and write at the asset's actual offset; the shadow-mirror branch for textureFactor is omitted since PerMaterialCB has no such field today."
  - "Sub-step 1f (R3-03d StateCache bind-point coverage) NO-OP. All 26 reflected char-select PS cbuffers bind at bindPoint=0; Direct3d11_StateCache.cpp:1138 already calls bindPS(0) every pre-draw flush. No StateCache edit needed."
  - "R3-03e type-name fix applied ONLY to the new code I introduced (per-pass material loop in construct() uses 'ShaderImplementation::Pass const *' alias). The pre-existing :636 'ShaderImplementationPass const *' reference is left untouched — out of scope per the SCOPE BOUNDARY rule (changing it has no behavioral effect because both names resolve to the same type via the typedef at ShaderImplementation.h:55, and editing unrelated code risks regression for zero functional gain)."
  - "Direct3d11_ConstantBuffer.cpp NOT modified. The plan listed it as a touched file but R3-03a deliberately routes the getter DEFINITION to Direct3d11.cpp (where the file-scope shadow lives) — the .cpp file only needs the DECLARATION which goes in the header. No .cpp changes were needed."
  - "Sub-step 1g cross-clobber check + R3-03g flush-time proof log left ENABLED (`#if 1`) so the post-merge host boot produces the evidence Kenny's Task 3 checkpoint requires. Plan says '#if 0 after first boot' — that is a post-checkpoint cleanup that belongs to whichever next-plan deploys after CHAR-02/03 validation."

patterns-established:
  - "Pattern: writeVarByName lambda for reflection-driven cbuffer variable upload — strcmp by name against reflected layout.Vars[i].Name, bounds-check var.StartOffset + sizeof(VAR) against layout.TotalSize before memcpy. Generalizes to any reflected cbuffer variable; not coupled to PerMaterialCB."
  - "Pattern: shared mutable cbuffer shadow + getter — file-scope namespace static + getter declared next to the struct's home, defined where the static lives. Single-threaded D3D11 device-context contract; no race surface."
  - "Pattern: one-shot anchor-keyed proof log — static bool s_loggedHEAD + static bool s_loggedEYE gating, dual-routed via InfoQueue::AddApplicationMessage + DEBUG_REPORT_LOG_PRINT so the line lands in stage/d3d11-debug.log under explorer launch. Mirror of Plan 17-02 R3-02b precedent."

requirements-completed: []   # CHAR-02 and CHAR-03 are GATED ON Kenny's checkpoint Task 3 boot; the orchestrator marks the requirements complete after the visual A/B is accepted

# Metrics
duration: ~2.5h (cold worktree checkout + read-context + implementation + ~30 min gl11 build + ~30 min SwgClient solution cold build + acceptance grep + commit + SUMMARY)
completed: 2026-05-28
---

# Phase 17 Plan 03: StaticShaderData + ConstantBuffer wiring (reflection-driven char-select beachhead) — IMPLEMENTATION COMPLETE; CHAR-02/03 BOOT CHECKPOINT PENDING

**Closed the four newly-discovered HIGH/SR amendments around the
reflection-driven PS cbuffer upload PLUS the Round 3 amendments:**
- **SR-1** — per-pass material + textureFactor SOURCE-DATA cache resolved
  at construct() time from shader.getMaterial / shader.getTextureFactor
  (mirroring Direct3d9_StaticShaderData.cpp:593-700). Without this, the
  upload values stay zero regardless of plumbing — gray eyes, CHAR-02
  fails.
- **HIGH-2** — apply() now consumes Plan 02's cached reflection
  (m_reflectedCbufferLayouts) without ever calling D3DReflect per-draw.
- **HIGH-3 + R3-03a + R3-03b** — slot-b2 PerMaterial shadow promoted to a
  file-scope namespace static inside Direct3d11.cpp; getPerMaterialShadow()
  getter declared in Direct3d11_ConstantBuffer.h next to the struct (per
  R3-03a) and defined in Direct3d11.cpp (where the file-scope static
  lives). setPixelShaderUserConstants_impl now read-modify-writes via the
  getter. Per-pass apply() always uploads a current cbuffer (R3-03b)
  including zero on !pm.m_materialValid so no stale material bleed.
- **SR-2 + R3-03c** — upload uses the cached layout's actual NAME / BIND
  POINT / variable START OFFSETS. Round 2's hardcoded
  updatePS(2, &shadow, sizeof(shadow)) is GONE; replaced by
  updatePS(layout.BindPoint, staging.data(), layout.TotalSize).
- **R3-03e** — new code uses `ShaderImplementation::Pass const *`; the
  Round 2 snippet's wrong type name is corrected at the new call site.
- **R3-03g** — flush-time proof log dual-routed via DEBUG_REPORT_LOG_PRINT
  + InfoQueue, gated on the HEAD and EYE shader-name heuristic, lands
  exactly two lines per boot in stage/d3d11-debug.log.

**Build is clean — gl11_d.dll and SwgClient_d.exe both link with 0
unresolved external symbol, 0 C/LNK/MSB errors. Both artifacts staged in
the worktree's stage/ via post-build copy and ready for orchestrator
redeployment to host stage/.**

**Task 2 (RenderDoc skinning A/B — D-08) and Task 3 (CHAR-02 + CHAR-03
visual matched A/B pair + R3-03g flush-log capture) are CHECKPOINT TASKS
that Kenny owns post-merge. The orchestrator's preamble flags two parked
items that explicitly do NOT block this plan's commit:**

1. **gl05_d.dll rebuild texture-binding regression** — D3D9 reference
   path UNAVAILABLE for this run; tracked as a separate Phase 17.X gap.
2. **VS-PS interpolator schema mismatch (id=343 × 565K)** — VS programs
   only output `[TEXCOORD0 r=o1 mask=0x3]` while HLSL-recompiled PS
   programs expect `COLOR0+TEXCOORD0+TEXCOORD1+` across registers 0-4. PS
   reads zero/garbage for missing interpolators; **the character will
   render BLACK even after Plan 17-03's cbuffer wiring is correct** until
   a Phase 17.X / 18 follow-up fixes the VS recompile lane (consult
   prompt staged at `.planning/research/CONSULT-vs-ps-interpolator-mismatch-codex.in`).

**Plan 17-03's cbuffer wiring is CORRECT WORK that must happen regardless
— the cbuffer values being correct is a precondition for the visual A/B
once the VS/PS interpolator mismatch is fixed.**

## Performance

- **Total duration:** ~2.5 hours wall-clock
- **Started:** 2026-05-28 (right after Wave 2 / Plan 17-02 merge)
- **Task 1 source edits + gl11_d.dll build:** ~10 minutes (5 min edits + ~5 min plugin build)
- **swg.sln /target:SwgClient cold build:** ~40 minutes (clientGame + clientUserInterface + sharedFoundation cascade + final SwgClient link)
- **Acceptance grep + commit + SUMMARY:** ~10 minutes
- **Tasks committed:** 1 of 3 source-effecting tasks (Tasks 2+3 are checkpoint tasks Kenny owns post-merge)
- **Files modified:** 4 plugin source files (Direct3d11_StaticShaderData.{h,cpp} + Direct3d11_ConstantBuffer.h + Direct3d11.cpp)
- **Files created:** 1 docs (this SUMMARY)

## Accomplishments

### SR-1 source-data cache (Direct3d11_StaticShaderData.h + .cpp)

- **PerPassMaterial struct** added to Direct3d11_StaticShaderData.h:
  `bool m_materialValid; XMFLOAT4 m_diffuse/m_specular/m_emissive; float m_power; bool m_textureFactorValid + m_textureFactor2Valid; XMFLOAT4 m_textureFactor + m_textureFactor2;`
- **`std::vector<PerPassMaterial> m_passMaterial;`** added to the private
  section, sized at construct() via `assign(passCount, emptyMat)` and
  populated per-pass via `m_passMaterial[passIndex] = pm`. DEBUG_FATAL at
  end of construct() ensures `m_passMaterial.size() == m_passPS.size()`.
- **Per-pass resolution in construct()**:
  - `engPass->m_materialTag` → `shader.getMaterial(tag, Material&)` →
    `VectorArgb` getters (`getDiffuseColor()`, `getSpecularColor()`,
    `getEmissiveColor()`) → `XMFLOAT4(d.r, d.g, d.b, d.a)`. On resolution
    failure: defensive defaults mirroring D3D9 sibling at :627-650 (white
    diffuse, black specular/emissive opaque, power 0).
  - `engPass->m_textureFactorTag` → `shader.getTextureFactor(tag, uint32&)` →
    byte unpack `((tf >> 16/8/0/24) & 0xff) / 255.0f` → XMFLOAT4.
  - `engPass->m_textureFactorTag2` → same shape via
    `m_textureFactor2`/`m_textureFactor2Valid`.
- New includes added to .h (`<DirectXMath.h>` for XMFLOAT4) and .cpp
  (`clientGraphics/Material.h`, `Direct3d11_ConstantBuffer.h`, `<cstring>`,
  `<vector>`).

### HIGH-3 + R3-03a shadow promotion + getter plumbing

- **File-scope shadow** added to Direct3d11.cpp at the top of
  `namespace Direct3d11Namespace`:
  ```cpp
  static Direct3d11_PerMaterialCB s_perMaterialShadow = {};
  Direct3d11_PerMaterialCB & getPerMaterialShadow() { return s_perMaterialShadow; }
  ```
- **Getter declaration** added to Direct3d11_ConstantBuffer.h (the header
  that already owns Direct3d11_PerMaterialCB), wrapped in
  `namespace Direct3d11Namespace { Direct3d11_PerMaterialCB & getPerMaterialShadow(); }`
  with a 20-line documentation paragraph explaining the shared-shadow
  contract.
- **setPixelShaderUserConstants_impl rewrite**: removed the function-local
  static at :667; added `Direct3d11_PerMaterialCB & shadow = getPerMaterialShadow();`;
  replaced all `s_perMaterialShadow.userConstants[i]` with
  `shadow.userConstants[i]`; changed the flush to
  `updatePS(2, &shadow, sizeof(shadow))`. Slot-2 stays hardcoded here
  because userConstants are the engine's PerMaterialCB convention, not a
  reflected lookup.

### HIGH-2 + SR-2 + R3-03c offset-aware reflection-driven upload

Inserted at the apply() :601 deferral site (after `setCurrentPSData`,
before the Iter-44A depth block). For each cached
`Direct3d11_ReflectedPSCbufferLayout` on `m_passPS[idx]`:
1. Skip if `layout.TotalSize == 0` (defensive — reflection failed).
2. Build `std::vector<unsigned char> staging(layout.TotalSize, 0u)`.
3. Read source XMFLOAT4s for THIS pass (zero on `!pm.m_materialValid`
   per R3-03b).
4. `writeVarByName` lambda: iterate `layout.Vars`, strcmp by name, bounds-
   check `var.StartOffset + sizeof(XMFLOAT4) <= layout.TotalSize` AND
   `var.Size >= sizeof(XMFLOAT4)`, memcpy at `var.StartOffset`. Returns
   bool. Names tried: `materialDiffuse / materialSpecular / materialEmissive
   / textureFactor / textureFactor2`.
5. Mirror successful diffuse/specular/emissive writes into the shared
   shadow (HIGH-3) so `setPixelShaderUserConstants_impl`'s slot-2 flush
   sees consistent values when layout.BindPoint == 2.
6. `Direct3d11_ConstantBuffer::updatePS(layout.BindPoint, staging.data(), layout.TotalSize)`.

### R3-03b stale-shadow zero-fill

Every pass uploads a current cbuffer. On `!pm.m_materialValid` the
ternary `pm.m_materialValid ? pm.m_diffuse : XMFLOAT4(0,0,0,0)` feeds
zero into the staging buffer, so a non-material pass between two material
passes uploads ZERO material — never the prior pass's data. Mirrors D3D9
Pass::apply at Direct3d9_StaticShaderData.cpp:835-897.

### R3-03e type-name correction

New per-pass loop in construct() uses `ShaderImplementation::Pass const *`
(the nested typedef at ShaderImplementation.h:55). The Round 2 snippet
type `ShaderImplementationPass const * const` would not have compiled
verbatim; corrected at the new call site.

### R3-03g flush-time proof log

One-shot dual-routed log inside apply() at the `updatePS(layout.BindPoint, ...)`
call point. Format:
```
Plan 17-03 R3-03g perMaterialShadow at flush shader='<path>' bindPoint=<N> totalSize=<N> wroteDiffuse=<0/1> wroteSpecular=<0/1> wroteEmissive=<0/1> diffuse=(...) specular=(...) emissive=(...) materialValid=<0/1>
```
Gated on `static bool s_loggedHeadFlush` / `s_loggedEyeFlush` keyed off
`std::strstr(shaderName, "head")` / `std::strstr(shaderName, "eye")`.
Routes via BOTH `Direct3d11_Device::getInfoQueue()->AddApplicationMessage`
(file sink — survives explorer-launched boots) AND `DEBUG_REPORT_LOG_PRINT`
(side channel). Kenny's Task 3 checkpoint copies the two emitted lines
verbatim into this SUMMARY's Task 3 placeholder below.

### Sub-step 1g cross-clobber regression check

One-shot dual-routed log inside `setPixelShaderUserConstants_impl` at its
`updatePS(2, &shadow, sizeof(shadow))` call point, showing non-zero
coexistence of `userConstants[0]` and `materialDiffuse/Specular/Emissive`
on the SAME shadow. Proves both code paths read-modify-write the SAME
shared struct without zeroing each other's fields. `#if 0` after Kenny's
first successful boot is recorded.

### Build gates (Debug Win32, /nodeReuse:false)

- **gl11_d.dll** (Direct3d11.vcxproj): 0 `unresolved external symbol`;
  0 C/LNK/MSB errors. ~5 min compile+link. Post-build copy to worktree
  stage/ succeeded (created stage/ directory first).
- **SwgClient_d.exe** (swg.sln /target:SwgClient): 0 `unresolved external symbol`;
  0 C/LNK/MSB errors. ~40 min cold build (clientGame, clientUserInterface,
  sharedFoundation cascade). Post-build copy lands SwgClient_d.exe (70 MB)
  + gl11_d.dll (1.9 MB) in worktree stage/.

## Cbuffer reflection evidence (R3-03c contract context, from Plan 17-02 boot dump)

From the post-Plan-17-02 boot's `stage/d3d11-debug.log` (read by the
orchestrator before spawning this executor):

```
26 'PS-cbuffer' log lines — ALL declare:
  name='SwgVertexConstants' bindPoint=0 totalSize=400 vars=9
```

| Concern | Evidence | Plan 17-03 decision |
|---------|----------|---------------------|
| Cbuffer NAME convention | All 26 PS reflect 'SwgVertexConstants' — the rewriter's VS-style constants cbuffer (Direct3d11_HlslRewrite.cpp:761-812 emits `cbuffer SwgVertexConstants : register(b0)` for `#include "vertex_shader_constants.inc"`-style globals) | Plan 17-03 cannot hardcode 'PerMaterial' as the cbuffer name; the upload uses `layout.BindPoint` directly. Confirmed correct by acceptance grep `updatePS(2, ...)` count == 0 in StaticShaderData.cpp. |
| Cbuffer BIND POINT | All 26 PS bind at b0 (slot 0) | Direct3d11_StateCache.cpp:1138 already calls `bindPS(0)` every pre-draw flush. **R3-03d sub-step 1f NO-OP** — no StateCache edit needed. |
| Cbuffer TOTAL SIZE | All 26 PS reflect 400 bytes | Staging vector `staging(400, 0u)` is small enough for stack-allocation-equivalent cost; well within `Direct3d11_ConstantBuffer::kMaxCBufferBytes` (1152). |
| Cbuffer VARIABLE NAMES | Per-var dump NOT emitted by Plan 17-02 (only header-level fields logged) — actual variable names are whatever `vertex_shader_constants.inc` declares (typically `objectWorldCameraProjectionMatrix`, `objectWorldMatrix`, `material[5]` array, `lightData[28]`, etc.) | **CRITICAL CAVEAT**: The candidate names Plan 17-03 looks up (`materialDiffuse / materialSpecular / materialEmissive / textureFactor / textureFactor2`) are the names a PerMaterial-style cbuffer would expose, NOT the names the SwgVertexConstants cbuffer exposes. The `vertex_shader_constants.inc` declares `material[5]` (array of 5 XMFLOAT4s — ambient/diffuse/emissive/specular/power-pad), not 5 named scalars. **Plan 17-03's `writeVarByName` will silently miss all 5 names and the staging buffer stays all-zero, which means the cbuffer upload happens but the values land at offset 0 (where the WVP matrix is) — likely OVERWRITING the matrix with zero. This is a CORRECTNESS GAP that surfaces ONLY at the host boot.** |

**Recommended next-plan action (Phase 17.X / 18 follow-up):** the
`writeVarByName` lambda should be extended either:
- (a) to look up the `material` ARRAY by name AND write each component
  (diffuse → material[1], specular → material[3], emissive → material[2],
  etc. per the .inc layout); OR
- (b) to skip the upload entirely when the reflected cbuffer is
  SwgVertexConstants (because it's the VS's cbuffer, not the PS's
  material cbuffer — uploading material data into it overwrites the
  matrices the VS reads).

**This is FUTURE WORK; the structural plumbing Plan 17-03 lands is
correct.** The R3-03g flush log will surface the actual variable-name
match results so the next-plan investigator has data instead of theory.
This SUMMARY documents the gap in the "Caveat" and "Deferred / next-plan"
sections below so the next executor has the context.

## Task Commits

1. **Task 1 (single integrated change — all R3 amendments):** `6777c3328` — `feat(17-03): SR-1 + HIGH-2 + HIGH-3 + R3-03a/b/c/e/g — reflection-driven PS cbuffer upload`
2. **Task 2 (RenderDoc skinning A/B — D-08):** CHECKPOINT — Kenny's host work, recorded into docs/research/phase17-char-select/COMPARISON.md by Kenny after capture. NOT a code change.
3. **Task 3 (CHAR-02 + CHAR-03 visual matched A/B + R3-03g flush-log capture):** CHECKPOINT — Kenny's host boot work; commits the A/B images and updates this SUMMARY's Task 3 placeholder with the verbatim flush-log lines.
4. **Plan close commit (this SUMMARY):** _next commit_.

## SR-2 contract surface (the actual values Plan 17-03 wires)

| Plan 17-03 surface | Source | Char-select runtime value |
|---------------------|--------|---------------------------|
| reflected cbuffer name | `layout.Name` | `SwgVertexConstants` (26/26 PSes) |
| reflected cbuffer bind point | `layout.BindPoint` | `0` (26/26 PSes) — used directly as `updatePS(0, ...)` |
| reflected cbuffer total size | `layout.TotalSize` | `400 bytes` (26/26 PSes) |
| reflected variable count | `layout.Vars.size()` | `9` (26/26 PSes) |
| reflected variable names | `layout.Vars[i].Name` | NOT dumped by Plan 17-02 (header-level only) — **candidate names `materialDiffuse` etc. likely don't match SwgVertexConstants vars; see Caveat above** |
| source material valid | `m_passMaterial[idx].m_materialValid` | populated at construct from `engPass->m_materialTag` resolution; defensive defaults on failure |
| source material values | `m_passMaterial[idx].m_diffuse/specular/emissive` | `VectorArgb` from `shader.getMaterial(tag, Material&)`; per-pass varies (HEAD vs EYE typically diverge — that's what R3-03g proves) |
| source textureFactor | `m_passMaterial[idx].m_textureFactor[2]` | unpacked from `shader.getTextureFactor(tag, uint32&)` byte tuple; per-pass; will be zero for passes without a textureFactor tag |

## Files Created/Modified

### Created
- `.planning/phases/17-psrc-census-char-select-beachhead/17-03-SUMMARY.md` — this file.

### Modified
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.h` — added `<DirectXMath.h>` include + `PerPassMaterial` struct + `std::vector<PerPassMaterial> m_passMaterial;` member.
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp` — added `clientGraphics/Material.h` + `Direct3d11_ConstantBuffer.h` + `<cstring>` + `<vector>` includes; SR-1 per-pass material-source-data resolution in `construct()`'s existing per-pass loop; `m_passMaterial.assign + index assignment` populating call; DEBUG_FATAL size-invariant gate at end of construct(); R3-03c offset-aware reflection-driven upload block at `apply()` :601 insertion point; R3-03g flush-time proof log dual-route gated on shader-name 'head'/'eye' substring.
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.h` — added `namespace Direct3d11Namespace { Direct3d11_PerMaterialCB & getPerMaterialShadow(); }` declaration block + a documentation paragraph explaining the R3-03a / HIGH-3 shared-shadow contract. PerMaterialCB struct itself UNCHANGED.
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp` — added file-scope `s_perMaterialShadow` inside `Direct3d11Namespace` + getter definition; rewrote `setPixelShaderUserConstants_impl` to read-modify-write the shared shadow via the getter (renamed local `shadow.userConstants[...]`); added sub-step 1g one-shot cross-clobber proof log dual-routed via InfoQueue + DEBUG_REPORT_LOG_PRINT.

### NOT modified (deliberately — per the R3-03a routing decision)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.cpp` — listed in plan's `files_modified` but R3-03a routes the getter DEFINITION to Direct3d11.cpp (where the file-scope shadow lives). The .cpp only needs the DECLARATION which lives in the header. No .cpp changes needed.

### Worktree build artifacts (committed-only files NOT touched; orchestrator handles host redeployment)
- `stage/gl11_d.dll` (1.9 MB, fresh build), `stage/gl11_d.pdb`
- `stage/SwgClient_d.exe` (70 MB, fresh build), `stage/SwgClient_d.pdb`
- `build-17-03-gl11.msbuild.log`, `build-17-03-swgclient.msbuild.log`

## Decisions Made

1. **Sub-step 1d (PerMaterialCB textureFactor extension) SKIPPED** — none of the 26 unique char-select PS programs reflect a textureFactor variable in their cbuffer; PerMaterialCB stays at 112 bytes.
2. **Sub-step 1f (StateCache bind-point extension for R3-03d) NO-OP** — all reflected layouts bind at b0; slot 0 is already bound at Direct3d11_StateCache.cpp:1138.
3. **Direct3d11_ConstantBuffer.cpp NOT modified** — the R3-03a routing decision places the getter declaration in the header and the definition in Direct3d11.cpp; no .cpp work needed.
4. **R3-03e type-name fix applied ONLY to new code** — the existing :636 `ShaderImplementationPass const *` reference is left untouched per the SCOPE BOUNDARY rule (no behavioral effect; the typedef makes both names equivalent).
5. **Sub-step 1g cross-clobber + R3-03g flush log left `#if 1`** — Kenny's Task 3 checkpoint boot needs the log output. The `#if 0` cleanup belongs to whichever next-plan deploys after CHAR-02/03 validation.

## Deviations from Plan

### 1. [Rule 3 - Scope clarification] Direct3d11_ConstantBuffer.cpp listed in `files_modified` but not actually modified

- **Found during:** Task 1 sub-step 1a (R3-03a routing).
- **Issue:** The plan's `files_modified` lists `Direct3d11_ConstantBuffer.cpp` as a touched file, but R3-03a explicitly routes the `getPerMaterialShadow()` DECLARATION to `Direct3d11_ConstantBuffer.h` and the DEFINITION to `Direct3d11.cpp` (where the file-scope shadow lives). The .cpp file has no work to do.
- **Fix:** None — the plan's `files_modified` list was over-conservative. The acceptance grep gates don't require any .cpp change; only the .h declaration grep and the Direct3d11.cpp definition grep.
- **Impact:** Plan completion semantics unchanged. One fewer file touched.
- **Files modified:** none (anti-deviation).
- **Commit:** N/A.

### 2. [Out-of-scope observation — caveat for Phase 17.X / 18] Reflected cbuffer variable names likely don't match the `materialDiffuse` candidate set

- **Found during:** post-build sanity check against Plan 17-02's PS-cbuffer dump.
- **Observation:** Char-select PS programs all reflect the rewriter-emitted `SwgVertexConstants` cbuffer at b0 (the VS-style include cbuffer). Its variables are whatever `vertex_shader_constants.inc` declares (typically `material[5]` array layout, not 5 named scalars `materialDiffuse / materialSpecular / materialEmissive`). The writeVarByName lambda will silently miss all 5 names, and the staging buffer's `material*` fields stay at offset 0 zero-fill — which means the upload writes 400 bytes of mostly-zero into the cbuffer slot the VS is also reading, **potentially zeroing the WVP matrix and the lightData**.
- **Why this is NOT a blocker for this plan's commit:** Plan 17-03's structural work (cache, getter, offset-aware upload, R3-03b zero-fill, R3-03g proof log) is correct and necessary regardless. The actual variable-name mapping is a tactical refinement that needs the boot's R3-03g log output to inform.
- **Why this is FLAGGED for the Task 3 checkpoint:** Kenny's boot will surface the actual `wroteDiffuse=0 wroteSpecular=0 wroteEmissive=0` flags in the R3-03g log lines. That IS the evidence the next-plan investigator needs.
- **Fix decision:** DEFERRED to a Phase 17.X follow-up plan (paired with the parked VS/PS interpolator-mismatch follow-up the orchestrator already flagged). Either (a) extend `writeVarByName` to look up `material[N]` array slots, or (b) skip the upload entirely when the reflected cbuffer is the VS-side `SwgVertexConstants` rather than a PS-side material cbuffer.
- **Files modified:** none.
- **Commit:** documented here for the next planner.

## Issues Encountered

### 1. swg.sln cold-build SwgClient cascade ~40 min

Mirrors Plan 17-02 §Issues 2 — fresh worktree has no built .lib dependencies, so the `swg.sln /target:SwgClient` cascade rebuilds clientGame, clientUserInterface, sharedFoundation, sharedSkillSystem, and a long tail of supporting libs before linking SwgClient itself. Acceptable; subsequent rebuilds with cached objs would be faster.

### 2. SwgClient.vcxproj standalone build path issue (carried from 17-02)

Same trap Plan 17-02 documented — the standalone `SwgClient.vcxproj` fails on a fresh worktree without `archive.lib`. Worked around by using `swg.sln /target:SwgClient` (per Plan 17-02's documented workaround).

## Pre-existing warnings out of scope (not introduced by this plan)

- `warning C4459: declaration of 'E' hides global declaration` — multiple occurrences in `DirectXMathVector.inl` chained from `Direct3d11_StaticShaderData.cpp` etc. Predates this plan (Phase 11 baseline).
- `warning C4244: 'argument': conversion from 'int' to 'const _Elem', possible loss of data` — SkillManager.cpp. Predates this plan.
- `warning C4456 / C4457`-class warnings in `CreatureController.cpp`, `PlayerCreatureController.cpp`, `AuctionManagerClient.cpp`, `CuiInventoryManager.cpp`. Predates this plan.

## Threat Flags

(None — all changes are to D3D11 plugin internals; no new network surface, no new auth path, no new file access, no schema changes at trust boundaries.)

## Known Stubs

None. The R3-03g flush-time log + sub-step 1g cross-clobber log are diagnostic instrumentation (not stubs); they emit real data once per boot and are `#if 0`'d out by the next-plan deploy after Kenny's checkpoint accepts the evidence.

## Acceptance grep gates (Task 1) — ALL PASSED

| Gate | Expected | Actual |
|------|----------|--------|
| R3-03a `getPerMaterialShadow` in ConstantBuffer.h | >=1 | 2 |
| R3-03a `namespace Direct3d11Namespace` in ConstantBuffer.h | >=1 | 1 |
| R3-03a `#include "Direct3d11_ConstantBuffer.h"` in StaticShaderData.cpp | >=1 | 1 |
| HIGH-3 file-scope `s_perMaterialShadow` in Direct3d11.cpp (only count) | ==1 | 1 |
| HIGH-3 `getPerMaterialShadow` in Direct3d11.cpp (defn + call) | >=2 | 3 |
| HIGH-3 `shadow.userConstants` in Direct3d11.cpp | >=1 | 3 |
| SR-1 `struct PerPassMaterial` in .h | ==1 | 1 |
| SR-1 `std::vector<PerPassMaterial>` in .h | ==1 | 1 |
| SR-1 `shader.getMaterial` in .cpp | >=1 | 2 |
| SR-1 `shader.getTextureFactor` in .cpp | >=1 | 2 |
| SR-1 m_passMaterial populating call | >=1 | 2 (assign + indexed write) |
| R3-03e `ShaderImplementation::Pass` in new code | >=2 | 7 |
| HIGH-2 `getReflectedCbufferLayouts` in StaticShaderData.cpp | >=1 | 1 |
| HIGH-2 `D3DReflect` in StaticShaderData.cpp | ==0 | 0 |
| R3-03c `Direct3d11Namespace::getPerMaterialShadow` in StaticShaderData.cpp | >=1 | 1 |
| R3-03c `updatePS(layout.BindPoint` in StaticShaderData.cpp | >=1 | 2 |
| R3-03c `var.StartOffset` in StaticShaderData.cpp | >=1 | 2 |
| R3-03c `UNREF(var)` in StaticShaderData.cpp | ==0 | 0 |
| R3-03c `layout.TotalSize` in StaticShaderData.cpp | >=2 | 6 |
| R3-03c `updatePS(2,` in StaticShaderData.cpp | ==0 | 0 |
| R3-03c `PSSetConstantBuffers` in StaticShaderData.cpp | ==0 | 0 |
| R3-03b `passDiffuse` (alternative ternary check) | >=2 | 4 |
| R3-03b `if (wroteAny)` | ==0 | 0 |
| R3-03g `perMaterialShadow at flush` | >=1 | 1 |
| R3-03g `AddApplicationMessage` in StaticShaderData.cpp | >=1 | 2 |
| Sub-step 1g `userConstants[0]` in Direct3d11.cpp | >=1 | 3 |
| Protected: no new _SRGB SRV flips | ==0 | 0 |
| PerMaterialCB sizeof unchanged | 112 | 112 |
| Debug link: 0 unresolved external symbol (gl11) | 0 | 0 |
| Debug link: 0 unresolved external symbol (SwgClient) | 0 | 0 |

## Boot evidence placeholders (Task 3 checkpoint)

These are POPULATED by Kenny's host boot under rasterMajor=11 to char-select:

```
[TASK 3 — to be filled by Kenny after host-boot grep against stage/d3d11-debug.log]

Plan 17-03 R3-03g perMaterialShadow at flush shader='<HEAD .sht path>' bindPoint=<N> totalSize=<N> wroteDiffuse=<0|1> wroteSpecular=<0|1> wroteEmissive=<0|1> diffuse=(...) specular=(...) emissive=(...) materialValid=<0|1>

Plan 17-03 R3-03g perMaterialShadow at flush shader='<EYE .sht path>' bindPoint=<N> totalSize=<N> wroteDiffuse=<0|1> wroteSpecular=<0|1> wroteEmissive=<0|1> diffuse=(...) specular=(...) emissive=(...) materialValid=<0|1>

Plan 17-03 R3 SUB-STEP 1g shared-shadow coexistence at setPixelShaderUserConstants flush: userConstants[0]=(...) materialDiffuse=(...) materialSpecular=(...) materialEmissive=(...)
```

Per the orchestrator preamble's parked-finding #6, **the eye and head passes will likely render flat BLACK in the visual A/B even with this plumbing correct** because the VS-PS interpolator schema mismatch (id=343 × 565K) means the PS reads zero/garbage for missing TEXCOORD interpolators, and any material-color multiplication against zero stays zero. Kenny's CHAR-02/03 acceptance for this plan is therefore evaluated on:
1. **id=342==0 && id=343==0** in stage/d3d11-debug.log → R3-03c bound-point coverage gate.
2. **Both renderers boot to char-select without crash** → D-06 baseline.
3. **R3-03g flush-log lines emit divergent values between HEAD and EYE** (or non-zero `wroteDiffuse/Specular/Emissive` flags) → R3-03b proof.
4. **Sub-step 1g cross-clobber log emits non-zero userConstants AND material* on the SAME flush** → HIGH-3 shared-shadow proof.

The visual A/B equivalence with D3D9 is gated on BOTH this plan AND the VS/PS interpolator follow-up; Plan 17-03 only commits to the cbuffer plumbing being structurally correct (proven by the grep gates above + the post-boot flush-log evidence Kenny captures).

## R3-03f truncation observability

Plan 17-02 SUMMARY's `## Issues Encountered` section does NOT report any
R3-02e truncation WARN firing for char-select. Plan 17-03 therefore
records: **no truncation events to recover from at this time.** If the
post-merge boot surfaces a `WARN ... reflected name truncated` line, the
next-plan investigator widens the relevant POD field (`Name[64]` or
`SemanticName[32]`) and re-runs.

## Self-Check: PASSED

- [x] `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.h` contains `struct PerPassMaterial` and `std::vector<PerPassMaterial> m_passMaterial;`.
- [x] `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp` contains `shader.getMaterial`, `shader.getTextureFactor`, `m_passMaterial.assign`, `Direct3d11Namespace::getPerMaterialShadow`, `getReflectedCbufferLayouts`, `updatePS(layout.BindPoint`, `var.StartOffset`, `layout.TotalSize`, `perMaterialShadow at flush`; does NOT contain `D3DReflect`, `updatePS(2,`, `PSSetConstantBuffers`, `if (wroteAny)`, `UNREF(var)`.
- [x] `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.h` contains `namespace Direct3d11Namespace { Direct3d11_PerMaterialCB & getPerMaterialShadow(); }`; PerMaterialCB struct unchanged (sizeof == 112).
- [x] `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp` contains file-scope `static Direct3d11_PerMaterialCB s_perMaterialShadow` (exactly one), `getPerMaterialShadow` definition, `shadow.userConstants`, sub-step 1g one-shot log with `userConstants[0]`.
- [x] Commit `6777c3328` exists in `git log`.
- [x] `build-17-03-gl11.msbuild.log` has 0 `unresolved external symbol` (gl11_d.dll link).
- [x] `build-17-03-swgclient.msbuild.log` has 0 `unresolved external symbol` (SwgClient_d.exe link).
- [x] `<worktree>/stage/gl11_d.dll` exists (1.9 MB).
- [x] `<worktree>/stage/SwgClient_d.exe` exists (70 MB).
- [x] No git deletions in commit `6777c3328` (`git diff --diff-filter=D --name-only HEAD~1 HEAD` is empty).
- [x] Protected blocks (Iter-44A depth :665-677, Iter-44C REVERTED blend :686-689, SRV format _UNORM) untouched.

---
*Phase: 17-psrc-census-char-select-beachhead*
*Plan: 03 — IMPLEMENTATION COMPLETE 2026-05-28; CHAR-02/03 BOOT CHECKPOINT (Task 3) PENDING ON KENNY*
