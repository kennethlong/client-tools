---
phase: 11-d3d11-renderer-plugin
plan: 07
subsystem: renderer
tags: [d3d11, shader-compile, hlsl-rewrite, iteration, smoke, milestone-close, bsod-revert, cbuffer-deferred]

# Dependency graph
requires:
  - phase: 11-d3d11-renderer-plugin
    plan: 06
    provides: Fully-wired D3D11 plugin scaffold -- state cache, draw dispatch, input-layout cache, light manager, metrics. Plan 11-06 ended at 29 STUB() bindings; Plan 11-07 is the first plan to run the plugin end-to-end against engine HLSL shader assets and surface the next FATAL wall iteratively.
provides:
  - Direct3d11_ShaderImplementationData -- minimal `ShaderImplementationGraphicsData` subclass; constructs successfully so engine `ShaderTemplate::install` advances past `createShaderImplementationGraphicsData` factory slot (Iter-1 unblock)
  - Direct3d11_StaticShaderData -- minimal `StaticShaderGraphicsData` subclass with `update(StaticShader const &)` + `getTextureSortKey()` virtuals satisfied; recorded-no-op `apply` (per-pass state-apply work deferred to a later plan once geometry surfaces visual symptoms)
  - Direct3d11_CompileIncludeHandler -- stateless singleton implementing `ID3DInclude::Open / Close` against the engine's `TreeFile` abstraction; replaces D3D_COMPILE_STANDARD_FILE_INCLUDE at both VS + PS D3DCompile call sites so `.inc` headers archived in TRE bundles resolve correctly
  - Direct3d11_HlslRewriteUtil -- text-pass rewriter for SWG-era HLSL targeting d3dcompiler_47; final ruleset (Rules A..E plus context-aware register-binding strip) clears the X4016 register-collision wall on `vertex_shader_constants.inc` under `vs_4_0_level_9_3` / `ps_4_0_level_9_3` profiles
  - D3D11 visible-dark-blue MVP clear -- renderer end-to-end functional through swap-chain `Present`, frame loop iterating, process responsive, no FATAL (Plan 11-07 close milestone; original SPEC R5 ≥5-min playable acceptance deferred to Plan 11-08+ cbuffer-wiring arc)
  - Iteration-log discipline -- 18-iteration log with 6-field entries (Date / Symptom / Hypothesis / Investigation / Root cause / Fix / Verification + Commit hash) + ranked "Awaiting Kenny smoke" outcome blocks; pattern carried forward to Plan 11-08
  - LEARNINGS extract -- 7 decisions + 10 lessons + 7 patterns + 8 surprises distilled from the iteration arc (`11-07-LEARNINGS.md`)
  - Cross-AI peer review pattern (CODEX) -- diagnosed `#pragma def(vs, c95, ...)` collision against FXC's level9 c-register allocator (Iter-14) after 13 in-house iterations failed to clear the X4016 wall; pre-ratified Plan 11-08 hypothesis space
  - STUB count delta: Plan 11-06 ended at 29; Plan 11-07 ends at 27 (Iter-1 wired `createShaderImplementationGraphicsData` + `createStaticShaderGraphicsData` factory slots)
affects: [11-08, 11-09]

# Tech tracking
tech-stack:
  added:
    - Direct3d11_CompileIncludeHandler (ID3DInclude over TreeFile)
    - Direct3d11_HlslRewriteUtil text-pass (Rules A..E, context-aware Rules B/C with `sawColonThisLine` flag)
    - D3DCompile profile pinned at `vs_4_0_level_9_3` / `ps_4_0_level_9_3` with Flags1 = BACKWARDS_COMPATIBILITY | (DEBUG/SKIP_OPTIMIZATION in debug, OPTIMIZATION_LEVEL3 in release); STRICTNESS dropped (mutually exclusive with BACKWARDS_COMPATIBILITY at d3dcompiler entry validation)
    - `D3D11_REWRITE_VERSION` macro pushed into D3DCompile defines (FNV-1a hash participant) so any rewrite-rule change invalidates cached `.cso` on next launch; monotonic counter ended at version 13 at Plan close
    - THROWAWAY diagnostic pattern (speculative-fix + diagnostic-dump in same iteration; `// THROWAWAY ITER-N` line markers for grep-clean reverts; explicit revert as first commit of following iteration)
  patterns:
    - "Cross-AI peer review for opaque error classes (no line/col + textual-rewrite arc at natural limit): capture verified artifact (post-rewrite dump), state hypothesis space, hand to external AI (CODEX) with the hypothesis enumerated. CODEX's FXC repro toggled each variable and proved `#pragma def(vs, c95, ...)` was the X4016 trigger -- a single round closed a 13-iteration wall."
    - "Atomic Flags1 audit-before-add: when modifying D3DCompile Flags1, ALWAYS read existing value first and audit the bitfield for conflicts BEFORE adding a new flag. Land flag changes as an atomic edit to the final intended state, not as `|=` accumulator. Iter-5 added BACKWARDS_COMPATIBILITY without inspecting Plan 11-05's baseline STRICTNESS; X3116 was the cost (one full smoke round-trip)."
    - "Per-iteration verification gate set (every iteration runs): MSBuild Direct3d11 + Direct3d9 + Direct3d9_ffp + Direct3d9_vsps + SwgClient EXIT=0; D-13 grep non-comment count == 0; D-05 git diff empty against Direct3d9/; D-04a Glob Direct3d11_FfpGenerator.* == 0; FFP scan on modified TUs == 0; STUB count tracked delta; MSVC /W4 zero new warnings."
    - "Revert THROWAWAY as first commit of FOLLOWING iteration: every THROWAWAY iteration's revert is mechanical via `grep -l 'THROWAWAY ITER-N' src/engine/client/application/Direct3d11/`; bump REWRITE_VERSION in the same revert commit to invalidate cache hits from the diagnostic iteration."
    - "Ranked 'Awaiting Kenny smoke' outcome block at end of every iteration entry: 3-4 ranked outcome scenarios (best / likely / possible / less likely) + prior-probability rationale. Cross-references after smoke result returns let the iteration author see which scenario actually fired; 'we predicted hypothesis 2' or 'we missed this outcome' narrative compounds across iterations."
    - "Atomic plan-level decisions captured as in-line 'Strategic decision recorded' blocks (textual rewrite vs Phase 12 asset re-author at Iter-7): chosen path + rationale-at-decision-time + explicit re-evaluation trigger. Without the explicit record, future sessions risk re-litigating the same decision with stale context."
    - "Don't commit code that BSOD'd the user's machine. Iter-18's minimum-viable WVP composition attempt BSOD'd ~2 min after launch; working-tree change reverted via `git restore` and never committed. The five-hypothesis enumeration in the Iter-18 entry is sufficient provenance for Plan 11-08 design without a forensic commit on the timeline."

key-files:
  created:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_ShaderImplementationData.h (~90 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_ShaderImplementationData.cpp (~115 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.h (~100 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp (~165 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_CompileIncludeHandler.h (~30 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_CompileIncludeHandler.cpp (~170 ln)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_HlslRewriteUtil.h
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_HlslRewriteUtil.cpp (Rules A..E, context-aware register-binding strip; final state at REWRITE_VERSION 13)
    - .planning/phases/11-d3d11-renderer-plugin/11-07-iteration-log.md (1050 ln; 18 iteration entries + Plan 11-07 milestone block + Iter-18 BSOD entry)
    - .planning/phases/11-d3d11-renderer-plugin/11-07-LEARNINGS.md (7 decisions + 10 lessons + 7 patterns + 8 surprises)
    - .planning/phases/11-d3d11-renderer-plugin/11-07-SUMMARY.md (this file)
  modified:
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp (Iter-1 install/remove ordering + 2 STUB() lines replaced with factory bindings)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexShaderData.cpp (Iter-2 ID3DInclude wired; Iter-3 profile string; Iter-5/6 Flags1 changes; Iter-7..14 HLSL rewrite invocation + REWRITE_VERSION pushdown)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp (mirror of VS site for ID3DInclude + profile + Flags1 + rewrite pipeline; `[[maybe_unused]]` ps_5_0 SPEC R3 compile-time proof contract maintained)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_TextureData.cpp (Iter-15 BC-format staging-texture dim pad to 4-pixel block boundary; commit `e4bdab17b` carries `fix(11-04):` prefix since it's a fix to Plan 11-04's resource layer surfaced by Plan 11-07 smoke)
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_RenderTarget.cpp (Iter-17 `setRenderTarget(NULL)` -> `setRenderTargetToPrimary` route; commit `8d4dcc934` carries `fix(11-04):` prefix for the same reason)
    - src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj + .filters (4 new ClCompile + 4 new ClInclude entries across Iter-1 + Iter-2 + Iter-7)
    - stage/client_d.cfg (Iter-16 `[ClientGame/Bloom] enable=0`; config-only Bloom STUB unblock)
    - src/external/3rd/library/platform/utils/Base/AutoLog.cpp (commit `ec0a42cee`, incidental during Iter-14 wider solution build; C++20 strict lexing required spaces around SLASHCHAR macro -- `"%s"SLASHCHAR"%s"` -> `"%s" SLASHCHAR "%s"`)

key-decisions:
  - "Plan 11-07 closes at the 'visible-dark-blue MVP clear' milestone (Iter-17 outcome), NOT at the original SPEC R5 ≥5-min Tatooine/cantina playable acceptance bar. The 13-iteration X4016 narrative + 4 follow-on iterations consumed the iteration budget; pushing for full geometry rendering in one more iteration BSOD'd the OS (Iter-18). Cbuffer wiring is research + design + multi-iteration implementation, properly Plan 11-08+ scope. Original SPEC R5 criteria moved to PENDING."
  - "Profile target chosen: `vs_4_0_level_9_3` / `ps_4_0_level_9_3` (not vs_5_0). SM5 reserves geometry-shader primitive keywords (`point` / `line` / `triangle` / `lineadj` / `triangleadj`) at the lexer level + rejects D3D9-era struct-member register-binding shortcuts; level_9_3 widens the acceptable grammar without losing the D3D11 toolchain. level_9_1 considered too restrictive for elaborate skeletal-mesh shaders."
  - "D3DCompile Flags1 final = BACKWARDS_COMPATIBILITY + (DEBUG/SKIP_OPTIMIZATION in debug, OPTIMIZATION_LEVEL3 in release). STRICTNESS dropped -- it's mutually exclusive with BACKWARDS_COMPATIBILITY at d3dcompiler entry validation (X3116). SWG ships D3D9-era HLSL inside TRE archives; compatibility mode is the strategic stance we want, STRICTNESS is the opposite."
  - "Textual rewrite chosen over Phase 12 asset re-author at Iter-7 strategic pause. Re-evaluation trigger captured explicitly: revisit if any future iteration surfaces a class requiring STRUCTURAL rewriting beyond regex + whitespace. Iter-13's Rule D (cbuffer-wrap of c-register globals) approached that line but pulled back to Rule E (line-anchored `#pragma def` strip) at Iter-14 -- the structural ceiling was identified but not crossed."
  - "Iter-18 BSOD-causing code NOT committed. Working-tree change reverted via `git restore`; no commit landed, even with a 'broken' marker. The iteration log entry is the only record. The five-hypothesis enumeration in that entry is the design-time input for Plan 11-08."
  - "Bloom unblocked via config-only change (Iter-16): `stage/client_d.cfg [ClientGame/Bloom] enable=0`. Bloom::install reads enable=0 -> skips enable() -> Graphics::setBloomEnabled never called -> STUB never fires. No code change, no rebuild, no commit. Establishes pattern for other non-critical scaffold STUBs hit during smoke."
  - "ShowWindow assist is a known runtime requirement post-Iter-17: the engine's normal `ShowWindow(SW_SHOW)` path didn't fire on the Iter-17 run; PowerShell helper `ShowWindow + SetWindowPos(100,100,1024,768) + SetForegroundWindow` made the window visible. Theories enumerated (Bloom config re-ordered init / shader-cache fast-path skip / prior-run swap-chain state); investigation deferred to a separate workstream from Plan 11-07's compile/load goal."

patterns-established:
  - "THROWAWAY diagnostic pattern paid off in BOTH speculative-hit and speculative-miss outcomes. Iter-8 (single-input dump): speculative `[bcstu]` letter expansion MISSED but the dump revealed the `v0` ground truth -> Iter-9 fixed it. Iter-10/12 (pure-diagnostic iterations, no speculative rule change): explicitly designed under the principle that any speculation on X4016 (no line/col) would be a complete shot in the dark; Iter-11's context-aware fix came directly from the Iter-10 dumps; Iter-14's CODEX repro came directly from the Iter-13B dump. The 'diagnostic-only iteration' is a first-class iteration shape, not a degenerate one."
  - "X-class error messages WITHOUT (line,col) annotation are entry-validation failures (Flags1 conflicts, missing required args); X-class messages WITH (line,col) are lex/parse errors. Use presence/absence of (line,col) as a quick classifier for where in the compile pipeline the failure occurred. Iter-6's X3116 (no line,col) signaled entry-validation conflict between STRICTNESS and BACKWARDS_COMPATIBILITY -- not a source issue."
  - "X4016's `'c0'` register identifier is FXC's level9 allocator report -- NOT the literal source-code register slot. The actual offending register was `c95`, declared via `#pragma def(vs, c95, 0.0, 0.5f, 1.0f, 1.4426950408889634f)` on line 141 of `vertex_shader_constants.inc`. Iter-11 spent an iteration making the rewrite context-aware to preserve global `register(cN)` bindings -- correct fix, didn't clear X4016 because the actual collision was elsewhere."
  - "The full canonical D3D HLSL register-type letter set is `[bcstuv]` -- 6 letters, not 5. `b#` cbuffer / `c#` single-constant / `s#` sampler / `t#` SRV / `u#` UAV / `v#` vertex-input-stream-register (D3D9-era). Iter-9's expansion from `[bcstu]` to `[bcstuv]` discovered the historical D3D9-inclusive set via the THROWAWAY dump showing `: register(v0)` on a `POSITION0` struct member."
  - "SM4+ HLSL lexer reserves geometry-shader primitive keywords at the lexer level -- profile-string-independent. Switching to a profile that doesn't support geometry shaders (`vs_4_0_level_9_3`) does NOT relax the reservation (Iter-3 -> Iter-4 discovery). The only path that avoids the lexer-level rejection is to rewrite the source bytes before D3DCompile sees them."
  - "D3D11 strict-rejects USAGE_STAGING `CreateTexture2D` when W or H < 4 for BC formats. D3D9 silently tolerated sub-block bottom-of-chain mip levels (e.g., 8x2 mip of a 256x64 BC3 texture); D3D11 returns E_INVALIDARG. Fix: dimension pad to next multiple of 4 in both 2D and volume-map staging paths. Helper `isBlockCompressedFormat(DXGI_FORMAT)` must cover BOTH non-contiguous BC ranges (BC1..BC5 = 70..84 AND BC6H..BC7 = 94..99)."
  - "`setRenderTarget(NULL)` is the engine's documented 'restore back buffer' idiom -- PostProcessingEffectsManager + similar post-FX paths use it after rendering a scratch-buffer pass. Plugin wrapper must dispatch to `setRenderTargetToPrimary()` (or equivalent back-buffer-RTV rebind) on the NULL branch, NOT forward to the user-texture path with its `NOT_NULL(texture)` precondition. `cubeFace` + `mipmapLevel` are ignored in the NULL branch."

requirements-completed: []
requirements-partial: [D3D11-01, D3D11-02, D3D11-03]
requirements-pending: [D3D11-05]

# Metrics
duration: ~3 days active (2026-05-17 .. 2026-05-19; 18 iterations across 3 sessions)
completed: 2026-05-19
iterations: 18
commits: ~40 (Iter-1..14 fix + docs + revert commits; Iter-15..17 fixes carry `fix(11-04):` prefixes since they patched Plan 11-04 resource layer; Iter-18 attempt UNCOMMITTED per "don't commit BSOD code" decision)
exe-size: gl11_d.dll 1,418,240 bytes at Plan close (Plan 11-06 baseline 1,392,128 bytes; +26 KB across 18 iterations)
stub-count: 27 remaining (Plan 11-06 baseline 29; -2 = Iter-1 wired `createShaderImplementationGraphicsData` + `createStaticShaderGraphicsData`)
---

# Phase 11 Plan 07: D3D11 Smoke + Shader-Compile Wall + Visible-Dark-Blue MVP Clear Summary

**18 iterations across 3 days (2026-05-17 .. 2026-05-19). Iter-1 wired the `createShaderImplementationGraphicsData` + `createStaticShaderGraphicsData` factory slots. Iter-2..13 lived inside a single shader-compile FATAL chain on `vertex_program/2d.vsh` (X1507 missing include resolver -> X3000 SM4+ keyword reservation -> X3202 struct-member binding -> X3116 STRICTNESS vs BACKWARDS_COMPATIBILITY conflict -> X3202 redux -> X4016 register collision). Iter-14 cleared X4016 via CODEX-diagnosed Rule E `#pragma def` stripper. Iter-15 (BC staging dim pad), Iter-16 (Bloom config-disable), Iter-17 (setRenderTarget NULL -> setRenderTargetToPrimary) cleared subsequent walls. Renderer end-to-end functional, MVP dark-blue clear visible, frame loop iterating. Iter-18 minimal-WVP cbuffer push attempt BSOD'd the OS via GPU TDR escalation ~2 min after launch; reverted, NOT committed. Cbuffer-wiring multi-iteration arc deferred to Plan 11-08+. Plan 11-07 CLOSED at the visible-dark-blue-clear milestone; original SPEC R5 ≥5-min playable acceptance criteria moved to PENDING.**

## Performance

- **Duration:** ~3 days active (2026-05-17 .. 2026-05-19); 18 iterations across 3 sessions; 1 strategic pause (Iter-7) + 1 CODEX phone-a-friend (Iter-14)
- **Started:** 2026-05-17 (Iter-1, after Plan 11-06 close at `358fe1a7e`)
- **Completed:** 2026-05-19 (post-Iter-17 milestone; Iter-18 BSOD attempt reverted)
- **Iterations:** 18 (Plan PLAN.md Task 2 guardrail set soft cap at 15; cap exceeded but never triggered CHECKPOINT surface -- Kenny + orchestrator chose to continue at each strategic pause point)
- **Commits:** ~40 across the plan (fix + docs + THROWAWAY-add + revert; chronological sequence in `git log --grep="11-07" --reverse`). Iter-15..17 fixes carry `fix(11-04):` prefixes since they patched Plan 11-04 resource layer. Iter-18 UNCOMMITTED.
- **gl11_d.dll growth:** 1,392,128 bytes (Plan 11-06 baseline) -> 1,418,240 bytes (Plan 11-07 close); +26 KB across 18 iterations
- **STUB count:** Plan 11-06 ended at 29; Plan 11-07 ends at 27 (Iter-1 wired 2 factory slots)

## Iteration Arc Summary

| Iter | Wall | Fix shape | Commit |
|------|------|-----------|--------|
| 1  | createShaderImplementationGraphicsData STUB | Author ShaderImplementationData + StaticShaderData wrappers (minimum-viable) | `972b02427` + `ff73b7e11` |
| 2  | D3DCompile X1507 missing include `vertex_program/include/vertex_shader_constants.inc` | Author CompileIncludeHandler routing ID3DInclude through TreeFile | `d191a9ac1` + `d7e79d955` |
| 3  | D3DCompile vs_5_0 X3000 `unexpected token 'point'` | Swap profile to vs_4_0_level_9_3 / ps_4_0_level_9_3 | `9aaa0efdc` |
| 4  | D3DCompile vs_4_0_level_9_3 X3000 `point` redux (lexer-level reservation) | Author HlslRewriteUtil; Rule A rewrites reserved keywords | `182317b27` |
| 5  | D3DCompile X3202 `location semantics cannot be specified on members` | Enable D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY | `db0ed346c` |
| 6  | D3DCompile X3116 `Flags specified both compatibility and strict mode` | Drop D3DCOMPILE_ENABLE_STRICTNESS (mutually exclusive with BACKWARDS_COMPATIBILITY at entry validation) | `79e364709` |
| 7  | X3202 redux (main shader source) | Extract HLSL rewrite utility; add Rules B/C for register-binding shortcuts; strategic-decision block (textual rewrite vs Phase 12 asset re-author -> chose textual) | `34dd0fffb` + `6d0b39d92` |
| 8  | X3202 redux (THROWAWAY iteration) | Expand Rules B/C letters from `c` to `[bcstu]` + add diagnostic dump | `4920f3edd` + `3b66af98b` |
| 9  | X3202 (post-Iter-8 dump revealed `v0` ground truth) | Revert Iter-8 THROWAWAY; expand Rules B/C to `[bcstuv]` (full 6-letter canonical) | `e436f1204` + `2df70f772` |
| 10 | X4016 register collision (no line/col; PURE DIAGNOSTIC iteration) | THROWAWAY I/O dumps at both entry points; REWRITE_VERSION 6->7 | `8518a73b9` |
| 11 | X4016 (post-Iter-10 dumps revealed Rules B/C OVER-STRIPPING globals) | Revert Iter-10/11 THROWAWAY; context-aware Rules B/C via `sawColonThisLine` flag; REWRITE_VERSION 8->9 | `2dd25c20b` + `594234a4b` |
| 12 | X4016 persistent post-Iter-11 (PURE DIAGNOSTIC iteration) | Re-add THROWAWAY dumps to verify Iter-11 fix output; REWRITE_VERSION 9->10 | `37497cc5f` + `fa2ca6634` |
| 13 | X4016 (Iter-12 dumps refute state-machine hypothesis) | Revert Iter-12 THROWAWAY; Rule D structural cbuffer-wrap of c-register globals; REWRITE_VERSION 11->12 | `a493000fe` + `5976a3530` |
| 14 | X4016 ROOT CAUSE: `#pragma def(vs, c95, ...)` collision with FXC level9 allocator (CODEX FXC repro) | Rule E line-anchored `#pragma def` strip for SM4+ compile; REWRITE_VERSION 12->13 (incidental: AutoLog.cpp C++20 SLASHCHAR fix `ec0a42cee`) | `96311b480` |
| 15 | `lightsaberblade` D3D11_USAGE_STAGING BC3 mip 5 (8x2) E_INVALIDARG | Pad BC-format staging dims to 4-pixel block boundary; helper covers BOTH BC1..BC5 + BC6H..BC7 ranges | `e4bdab17b` (`fix(11-04):` prefix) |
| 16 | Bloom scaffold STUB hit `setBloomEnabled` | Config-disable via `stage/client_d.cfg [ClientGame/Bloom] enable=0`; no code change | (config-only, no commit) |
| 17 | `setRenderTarget(NULL)` from PostProcessingEffectsManager not dispatched | Route NULL branch to existing `setRenderTargetToPrimary` in plugin wrapper | `8d4dcc934` (`fix(11-04):` prefix) |
| 18 | (attempt) minimal-viable WVP composition push to slot 0 | BSOD ~2 min after launch via GPU TDR escalation; reverted via `git restore` | **NONE** -- not committed |

## Outcome State (post-Iter-17, Plan 11-07 close)

| Subsystem | Status | Notes |
|-----------|--------|-------|
| Plugin DLL loads under rasterMajor=11 | ✓ | gl11_d.dll 1,418,240 bytes |
| Engine reaches char-select without FATAL | ✓ | Iter-14 cleared shader compile; Iter-15/17 cleared resource-layer + RT-NULL walls |
| Swap chain creation + Present cycle | ✓ | Plan 11-03 device MVP confirmed end-to-end |
| Back-buffer clear (dark-blue MVP placeholder) visible | ✓ | Plan 11-03 clearViewport unchanged; visible on window |
| Frame loop iterating, no FATAL, no crash | ✓ | Process responsive; iterates indefinitely against clear color |
| Window visible (after `ShowWindow` assist) | ✓ | External PowerShell `ShowWindow + SetForegroundWindow` required; root cause deferred |
| Geometry / UI rendering on top of clear | ✗ | Iter-18 enumerated 5 must-have prerequisites; cbuffer-wiring deferred to Plan 11-08 |
| D-13 invariant (no D3DPOOL_MANAGED / OnLostDevice / OnResetDevice non-comment) | ✓ | 0 non-comment hits across all 18 iterations |
| D-05 invariant (D3D9 plugin source untouched) | ✓ | `git diff 358fe1a7e..HEAD -- src/engine/client/application/Direct3d9/` empty |
| D-04a invariant (no FfpGenerator) | ✓ | Glob Direct3d11_FfpGenerator.* returns 0 results |
| D3D9 plugin still builds clean | ✓ | All 4 variants (gl05/06/07/09_d.dll) link clean each iteration |

## Plan 11-07 vs. Original SPEC R5 Acceptance Criteria

Plan 11-07's original `must_haves.truths` required full SPEC R5 acceptance: Tatooine outdoor + cantina interior ≥5-min playable smoke under D3D11 with all 4 target subsystems (terrain, skeletal, particles, HUD) visible. **None of those criteria are met at Plan close** -- the visible-dark-blue-clear milestone is upstream of "geometry on top of clear" by the cbuffer-wiring arc that BSOD'd Iter-18.

Status reclassification at close:

| Original Criterion | Status |
|--------------------|--------|
| Boots under `rasterMajor=11` to char-select without crash | ✓ MET |
| Tatooine outdoor renders (terrain + player + NPCs + HUD) | PENDING -- depends on cbuffer wiring + per-pass state-apply on StaticShaderData |
| Cantina interior renders (cell + NPCs + HUD, no portal z-fight) | PENDING |
| ≥5 min Tatooine outdoor without renderer-specific crash | PENDING -- predicated on geometry rendering first |
| ≥5 min cantina interior without renderer-specific crash | PENDING -- predicated on geometry rendering first |
| All 4 target subsystems visible | PENDING -- predicated on geometry rendering first |
| No all-black / all-white frames; no glaring corruption | PARTIAL -- current state is "all-dark-blue clear"; not corrupt, but not yet rendering content |
| D3D9 plugin still builds clean | ✓ MET (D-05 invariant) |
| Iteration log records every bug + fix | ✓ MET (18 entries + milestone + Iter-18 BSOD) |

The PENDING criteria are inherited by Plan 11-08 (cbuffer wiring) + later plans (per-pass state apply, point-sprite setters, window/cursor utilities).

## Iter-18 BSOD -- Five Hypotheses (Plan 11-08 Design Input)

Iter-18's minimum-viable WVP composition attempt pushed a 128-byte blob (`{WVP, World}`) to slot 0 via `Map(WRITE_DISCARD)`. The OS BSOD'd ~2 minutes after launch via GPU Timeout Detection and Recovery (TDR) escalation. No minidump (`%WINDIR%\Minidump\` empty); no user-mode crash dump (BSOD took down OS before write).

Plan 11-08 must address all five hypotheses + CODEX's sixth + two retrofits before any cbuffer-write touches the geometry pipeline:

1. **Matrix-composition order wrong.** D3D9's `ms_cachedWorldToProjectionMatrix` order must be verified by direct source read at `Direct3d9.cpp:3291 + 3359` BEFORE porting to XMMath.
2. **Shader reads beyond 128-byte write into uninitialized bytes.** Slot 0 is 1024 bytes (Plan 11-05 sizing); shader reads `LightData` at c16 (28 regs), `Material` at c11 (5 regs), `ExtendedLightData` at c60 (8 regs). Plan 11-05 retrofit needed: slot capacity ≥1088 bytes (chosen 1152 for safety = 72 registers).
3. **`objectWorldMatrix` at c4 malformed** at C++/XMMath row-major boundary. Plan 11-07 retrofit: D3DCompile must use `D3DCOMPILE_PACK_MATRIX_ROW_MAJOR` (0x8) -- HLSL defaults to column-major storage; XMFLOAT4X4 row-major byte upload is read TRANSPOSED without this flag.
4. **Per-frame setter race.** First-draw before `setWorldToCameraTransform` / `setProjectionMatrix` ever fired -> identity view+proj -> object-space coords in clip space -> degenerate rasterization. Initial-state guarantee must upload identity matrices in `install()`.
5. **2-minute delayed BSOD.** Periodic draw (particle emit, deferred asset, animation tick) eventually exercises bad matrix data. D3D11 debug layer must be enabled by default in debug builds; `ID3D11InfoQueue` drains validation messages once per frame via `drainInfoQueue` helper so the next BSOD-class bug surfaces as a logged warning instead of a system crash.

**CODEX sixth hypothesis** (added during 11-08 PLAN peer review): `Map(D3D11_MAP_WRITE_DISCARD)` does NOT guarantee zeroed memory per Microsoft documentation -- unwritten bytes contain ARBITRARY GARBAGE from previous draws, not "safe zero defaults." Every cbuffer slot upload must write the FULL struct, no partial writes; per-frame setters must memcpy or struct-assign the full `Direct3d11_VertexSlot0CB` before every `updateVS(0, ...)` call.

## Accomplishments

### Direct3d11_ShaderImplementationData + Direct3d11_StaticShaderData wrappers (Iter-1)

Two new GraphicsData subclasses landed minimum-viable (construct + minimal accessors + no-op apply). Per-pass BS/DSS overrides + per-pass VS/PS lookup + per-instance state resolution (vertex shader / material / texture-factor / alpha-test / stencil / fog / per-stage texture) are explicitly deferred to a later plan once visible geometry surfaces visual symptoms that justify the work. Iter-1's contract was "advance the FATAL boundary past `createShaderImplementationGraphicsData`" -- met.

### Direct3d11_CompileIncludeHandler (Iter-2)

Stateless singleton implementing `ID3DInclude::Open / Close` against the engine's `TreeFile` abstraction. SWG ships `.inc` headers inside TRE archives; the on-disk `stage/` directory has no `vertex_program/include/` subtree. Default `D3D_COMPILE_STANDARD_FILE_INCLUDE` (= sentinel `(ID3DInclude*)1` selecting the Win32 FS handler) cannot see TRE content. Handler resolves both `#include "..."` (D3D_INCLUDE_LOCAL) and `#include <...>` (D3D_INCLUDE_SYSTEM) identically; missing files return `STG_E_FILENOTFOUND` so D3DCompile emits a clean X1507 message rather than triggering an engine FATAL.

Wired into BOTH D3DCompile call sites: Direct3d11_VertexShaderData::compileOrLoad (hot path) AND Direct3d11_PixelShaderProgramData::compilePixelShaderFromHlsl (forward-compat; `[[maybe_unused]]` ps_5_0 SPEC R3 contract maintained).

### Direct3d11_HlslRewriteUtil (Iter-4..14)

Text-pass rewriter targeting d3dcompiler_47 for SWG-era HLSL under `vs_4_0_level_9_3` / `ps_4_0_level_9_3` profiles. Final ruleset at REWRITE_VERSION 13:

- **Rule A:** SM4+ reserved-keyword rewrites for `point` / `line` / `triangle` / `lineadj` / `triangleadj` (lexer-level reservation, profile-string-independent)
- **Rule B/C:** strip `: register(cN)` / `: c<N>` shortcuts on struct members (X3202 trigger); context-aware via `sawColonThisLine` line-scoped flag so global file-scope register bindings are preserved (Iter-11 fix)
- **Rule D:** structural cbuffer-wrap of c-register globals with `cbuffer SwgVertexConstants : register(b0) { ... };` + `packoffset(cN)` substitutions; SYNTACTICALLY perfect but didn't clear X4016 (Iter-13 dead-end, kept in place as defense-in-depth)
- **Rule E:** line-anchored `#pragma def(vs|ps, cN, ...)` stripper -- the actual X4016 fix from CODEX FXC repro (Iter-14). The `#pragma def` allocates manually at c95; FXC's level9 c-register allocator's auto-allocation pass collides because the pragma's claim isn't visible to the auto-allocator's first pass

Both VS + PS sites push `D3D11_REWRITE_VERSION` as a D3DCompile define so the FNV-1a 64-bit hash in `Direct3d11_ShaderCache::hashSource` (hashes Name + Definition strings) invalidates every cached `.cso` blob on next launch when the rewriter changes.

### BC-format staging-texture dim pad (Iter-15)

`Direct3d11_TextureData::lock` BC-format mip 5+ staging textures hit E_INVALIDARG on `CreateTexture2D` because D3D11 strict-rejects W/H < 4 for block-compressed formats. Fix: dimension pad to next multiple of 4 in both 2D and volume-map staging paths. Helper `isBlockCompressedFormat(DXGI_FORMAT)` covers BOTH non-contiguous BC ranges (BC1_TYPELESS..BC5_SNORM = 70..84 AND BC6H_TYPELESS..BC7_UNORM_SRGB = 94..99). Pitch stays identical; `CopySubresourceRegion` with `pSrcBox = nullptr` resolves cleanly.

Commit carries `fix(11-04):` prefix since it patches Plan 11-04's resource layer; surfaced during Plan 11-07 smoke.

### Bloom config-disable (Iter-16)

`Direct3d11.cpp:717` has `STUB(setBloomEnabled);`. Bloom is default-enabled. Three options enumerated: (A) config-disable, (B) replace STUB with no-op-with-log, (C) implement properly. Chose A -- `stage/client_d.cfg [ClientGame/Bloom] enable=0`. Bloom::install reads enable=0 -> skips enable() -> Graphics::setBloomEnabled never called -> STUB never fires. No code change, no rebuild, no commit. Establishes pattern for other non-critical scaffold STUBs hit during smoke.

### setRenderTarget(NULL) routing (Iter-17)

`setRenderTarget(NULL)` is the engine's documented "restore back buffer" idiom from PostProcessingEffectsManager (and similar post-FX paths). Plugin wrapper was forwarding the NULL pointer to the user-texture path which trips `NOT_NULL(texture)` precondition. Fix: route the NULL branch to existing `setRenderTargetToPrimary()` (idempotent via `ms_primaryTargetSet` early-out). `cubeFace` + `mipmapLevel` are ignored in the NULL branch.

Commit carries `fix(11-04):` prefix since it patches Plan 11-04's render-target wrapper; surfaced during Plan 11-07 smoke.

## STUB Wiring Status (cumulative through Plan 11-07)

**Replaced (~69 of ~119 Gl_api slots):**

Plan 11-02..11-06 baseline: 90 of 119 slots wired (Plan 11-06 ended at 29 STUBs remaining).

Plan 11-07 (Iter-1 shader-impl factory slots): 2 slots wired:
- `createShaderImplementationGraphicsData` -- Direct3d11_ShaderImplementationData factory
- `createStaticShaderGraphicsData` -- Direct3d11_StaticShaderData factory

**Remaining 27 STUBs cluster into (unchanged from Plan 11-06 minus the 2 Iter-1 wires):**

- 6 point-sprite setters: `setPointSize` + `setPointSizeMax` + `setPointSizeMin` + `setPointScaleEnable` + `setPointScaleFactor` + `setPointSpriteEnable` (HLSL SV_PointSize substitution; particle subsystem -- surfaces once geometry renders)
- 4 windowing utilities: `resize` + `setWindowedMode` + `lockBackBuffer` + `unlockBackBuffer` (Wave 7+ scope; not blocking visible-clear milestone)
- 1 windowed-launcher slot: `presentToWindow` (Wave 7+ scope)
- 2 cursor: `setMouseCursor` + `showMouseCursor` (Wave 7+ scope)
- 14 misc setters / per-pass state callbacks deferred until Plan 11-08+ cbuffer wiring + per-pass state apply surfaces them

## Cross-AI Peer Review (CODEX)

Iter-14 broke a 13-iteration X4016 stall via CODEX (the same external AI that diagnosed the Plan 11-07 Iter-14 pattern in its first FXC repro round). The peer-review pattern requires:

1. **A verified artifact.** Iter-13's ITER-13B dump (uncommitted, captured to disk via the Iter-12 THROWAWAY rewrite) is the controlled artifact -- the exact post-rewrite HLSL bytes that D3DCompile saw.
2. **A stated hypothesis space.** Plan 11-07 author enumerated: struct packoffset / `register(b0)` cbuffer-wrap / struct declarations / something else. CODEX's FXC repro toggled each variable.
3. **A verifiable test.** CODEX's repro compiled the verified bytes under both `vs_4_0_level_9_3` AND `vs_5_0`, then progressively removed each construct from the source until X4016 cleared. The `#pragma def(vs, c95, ...)` strip was the single change that cleared the wall.

Plan 11-08's design space was pre-ratified by CODEX peer review (CODEX added the sixth `Map(WRITE_DISCARD)` hypothesis + two retrofits -- ROW_MAJOR flag + 1088B slot capacity).

## Lessons Learned (Distilled From LEARNINGS.md)

1. **`syntactically perfect` ≠ `fixes the problem`** -- Iter-13's Rule D cbuffer-wrap was structurally clean and still didn't clear X4016. Verify via external repro before doubling down on a textual-rewrite hypothesis.
2. **D3D11's failure modes can escalate beyond the process boundary** -- a 128-byte `Map(WRITE_DISCARD)` cbuffer push with wrong matrix composition triggered GPU TDR -> recovery failed -> full system BSOD with NO minidump. Future cbuffer work MUST enable the D3D11 debug layer + guarantee initial-state defaults + cover all bytes of cbuffer slots before any draw.
3. **The "diagnostic-only iteration" is a first-class iteration shape** -- pattern paid off in BOTH speculative-hit (Iter-8 missed but dump revealed v0 ground truth) and speculative-miss (Iter-10/12 pure-diagnostic) outcomes.
4. **Cross-AI peer review unblocks dead-ends in opaque error-class space** -- when an error class has no `(line,col)` AND textual-rewrite arc has hit its natural limit, capture the artifact + hand to external AI with explicit hypothesis space.
5. **Textual-rewrite arc has a structural ceiling** -- Iter-7..13 spanned 7 iterations and hit a natural limit at Rule D (cbuffer-wrap). Each rule requiring more than "single regex + whitespace replacement" is a signal to evaluate Phase 12 asset re-author. Rule D crossed that line conceptually; Rule E pulled back to line-anchored single-pattern strip.

Full distillation in `11-07-LEARNINGS.md` (7 decisions + 10 lessons + 7 patterns + 8 surprises).

## Self-Check

- [x] All 18 iterations executed; iteration log records every iteration with the 6-field shape + ranked "Awaiting" outcome block
- [x] Iter-14 cleared the X4016 wall (CODEX-diagnosed Rule E `#pragma def` stripper)
- [x] Iter-17 reached the visible-dark-blue MVP clear milestone (Plan 11-07 close criterion)
- [x] Iter-18 BSOD reverted via `git restore`; no BSOD-causing code committed
- [x] D-13 / D-05 / D-04a invariants maintained across all 18 iterations
- [x] D3D9 plugin still builds clean (all 4 variants)
- [x] SwgClient still builds clean
- [x] `11-07-iteration-log.md` complete with milestone + Iter-18 BSOD entry
- [x] `11-07-LEARNINGS.md` distilled and committed (`a594ed5fd`)
- [ ] Original SPEC R5 ≥5-min Tatooine/cantina playable acceptance -- DEFERRED to Plan 11-08+ cbuffer wiring arc (PENDING)
- [ ] Geometry / UI rendering on top of clear -- DEFERRED (PENDING)

**Plan 11-07 closes at the visible-dark-blue-clear milestone (Iter-17 outcome). The original SPEC R5 criteria moved to PENDING and are inherited by Plan 11-08 (cbuffer wiring) and later plans.**
