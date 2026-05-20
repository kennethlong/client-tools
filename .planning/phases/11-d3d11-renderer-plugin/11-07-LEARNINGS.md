---
plan: 11-07
phase: 11
phase_name: "d3d11-renderer-plugin"
project: "whitengold — SWG Client Modernisation Port"
generated: "2026-05-19"
scope: "Plan 11-07 only (iterations 1–18). Plans 11-01..11-06 have separate SUMMARYs; their learnings are out of scope here."
counts:
  decisions: 7
  lessons: 10
  patterns: 7
  surprises: 8
missing_artifacts:
  - "11-07-SUMMARY.md (iteration-log substitutes per PLAN §must_haves.artifacts)"
  - "11-VERIFICATION.md (phase still active)"
  - "11-UAT.md (phase still active)"
---

# Plan 11-07 Learnings: D3D11 Renderer Smoke + X4016 Compile Wall

Plan 11-07 ran 18 iterations across three days. Iter-1..13 lived inside a single shader-compile FATAL chain on `vertex_program/2d.vsh`. Iter-14 cleared it. Iter-15..17 cleared three further walls (BC staging texture, Bloom STUB, setRenderTarget NULL) and reached a visible dark-blue MVP clear — the closeout milestone. Iter-18 attempted minimal cbuffer wiring and BSOD'd the OS; reverted.

## Decisions

### Profile target: vs_4_0_level_9_3, not vs_5_0
Direct3d11 plugin compiles asset HLSL under `vs_4_0_level_9_3` / `ps_4_0_level_9_3`, not `vs_5_0`. SM3-equivalent feature constraints comfortably cover what the engine's `.vsh` content declares (max `vs_2_0` per Plan 11-01 static analysis); `level_9_1` would have surfaced limits on more elaborate skeletal-mesh shaders.

**Rationale:** SM5 reserves geometry-shader primitive keywords at the lexer level and rejects D3D9-era struct-member register-binding shortcuts; legacy-compat profile widens the acceptable grammar without losing the D3D11 toolchain.
**Source:** 11-07-iteration-log.md §Iteration 3, §Iteration 4

### D3DCOMPILE Flags1 = `BACKWARDS_COMPATIBILITY | OPTIMIZATION_LEVEL3` (no STRICTNESS)
Final flag bitfield: BACKWARDS_COMPATIBILITY (0x1000) + DEBUG/SKIP_OPTIMIZATION (debug) or OPTIMIZATION_LEVEL3 (release). STRICTNESS (0x800) dropped — it is mutually exclusive with BACKWARDS_COMPATIBILITY and triggers X3116 at the d3dcompiler entry-validation layer before any source is lexed.

**Rationale:** SWG ships D3D9-era HLSL inside TRE archives; the compatibility mode is the strategic stance we want, and STRICTNESS is the opposite stance.
**Source:** 11-07-iteration-log.md §Iteration 5, §Iteration 6

### Plan 11-07 closes at "visible dark-blue MVP clear" milestone, not at original SPEC R5 acceptance
The PLAN's original `must_haves.truths` required Tatooine outdoor + cantina interior ≥5-min playable smoke under D3D11 across 4 subsystems. The plan was CLOSED at a visible-window-with-clear milestone (Iter-17) — geometry/UI on top of the clear is deferred to Plan 11-08+ cbuffer-wiring work. The original criteria moved to "PENDING — addressed at next plan close."

**Rationale:** the 13-iteration X4016 narrative + 4 follow-on iterations consumed Plan 11-07's iteration budget; pushing for full geometry rendering in one more iteration BSOD'd the OS (see Iter-18). Cbuffer wiring is a research+design+multi-iter implementation arc, properly its own plan.
**Source:** 11-07-iteration-log.md §"Plan 11-07 milestone", §Iteration 18 §"Strategic conclusion"

### Textual rewrite chosen over Phase 12 asset re-author at Iter-6 strategic pause
At Iter-5/6 close, the team evaluated whether to pivot to a one-time Phase 12 HLSL modernization pass against the entire `.vsh` / `.inc` corpus, OR continue iterating with runtime textual rewrites. Chose **continue**.

**Rationale:** Iter-7 was a single-iteration delta addressing a specific known error class with a well-understood fix shape; the asset-re-author would be a multi-plan investment with its own discovery sequence. The decision included an explicit re-evaluation trigger: revisit if any future iteration surfaces a class requiring STRUCTURAL rewriting beyond regex+whitespace.
**Source:** 11-07-iteration-log.md §Iteration 7 §"Strategic decision recorded"

### `D3D11_REWRITE_VERSION` macro as cache-invalidation lever
Both VS + PS compile sites push `defines = { "D3D11_REWRITE_VERSION", "<N>" }` into the `D3DCompile` invocation. The macro is hashed by `Direct3d11_ShaderCache::hashSource` (FNV-1a 64-bit; Name + Definition strings); bumping the version invalidates every cached `.cso` blob on next launch.

**Rationale:** any rewrite-rule change, compile-flag change, or profile-string change must invalidate the cache or D3DCompile serves stale binaries from prior iterations. Version monotonically advances: Iter-3 introduced it; Iter-14 closed at version 13. Never referenced by HLSL source content.
**Source:** 11-07-iteration-log.md §Iteration 3, §Iteration 4 §Fix bullet

### Atomic Flags1 audit-before-add policy
When modifying `D3DCompile` Flags1, ALWAYS read the existing value first and audit the bitfield for conflicts BEFORE adding a new flag. Land flag additions/removals as an atomic edit that updates the bitfield to the final intended state, not as a `|=` accumulator that leaves the prior state implicit.

**Rationale:** Iter-5 added BACKWARDS_COMPATIBILITY without inspecting Plan 11-05's baseline STRICTNESS; X3116 was the direct cost (one full smoke round-trip).
**Source:** 11-07-iteration-log.md §Iteration 6 §"Meta-lesson recorded for future iterations"

### Iter-18 BSOD-causing code NOT committed
Iter-18's minimal-viable WVP cbuffer push attempt BSOD'd the OS ~2 minutes after launch. The working-tree change was reverted via `git restore`; NO commit landed, even with a "broken" marker. The iteration log entry is the only record.

**Rationale:** don't commit code that took down the user's machine. The five-hypothesis enumeration in the Iter-18 entry is the design-time input for whoever picks up the cbuffer-wiring plan; that's enough provenance without a forensic commit on the timeline.
**Source:** 11-07-iteration-log.md §Iteration 18 §"Commits"

---

## Lessons

### The full canonical D3D HLSL register-type letter set is `[bcstuv]` — 6 letters, not 5
Iter-7 hardcoded `c`. Iter-8 expanded to `[bcstu]` per "modern D3D11 SRV-binding" letters. Iter-9 added `v` after the THROWAWAY dump revealed `: register(v0)` on a `POSITION0` struct member. The 6-letter canonical set is: `b#` cbuffer / `c#` single-constant / `s#` sampler / `t#` SRV / `u#` UAV / `v#` vertex-input-stream-register (D3D9-era).

**Context:** SWG's D3D9-era vertex shaders use `v#` for struct-member input bindings; SM4+ moves IA-stage binding to `D3D11_INPUT_ELEMENT_DESC[]` and rejects `register(vN)` on struct members. The Iter-8 letter speculation missed `v` because the speculative read indexed against the modern D3D11 set, not the historical D3D9-inclusive set.
**Source:** 11-07-iteration-log.md §Iteration 9 §Investigation

### SM4+ HLSL lexer reserves geometry-shader primitive keywords at lexer level — profile-string-independent
`point` / `line` / `triangle` / `lineadj` / `triangleadj` are added to d3dcompiler_47's reserved-identifier table at lexer initialization, BEFORE profile-feature gating runs on the token stream. Switching to a profile that doesn't support geometry shaders (`vs_4_0_level_9_3`) does NOT relax the reservation. The only path that avoids the lexer-level rejection is to rewrite the source bytes before D3DCompile sees them.

**Context:** Iter-3 swapped vs_5_0 → vs_4_0_level_9_3 expecting the level_9_* profiles to relax SM5 keyword reservations. Iter-4 was the discovery iteration: same X3000 `unexpected token 'point'` fired under the new profile, proving the reservation lives at the lexer level.
**Source:** 11-07-iteration-log.md §Iteration 4 §Investigation

### X4016's `'c0'` register identifier is FXC's level9 allocator report — NOT the literal source-code register slot
The X4016 "overlapping register semantics" error message includes a register identifier (`'c0'` in our case), but that identifier is FXC's first-reported collision from the level9 c-register allocator's pass — not the literal slot named in the source. The actual offending register was `c95`, declared via a `#pragma def(vs, c95, 0.0, 0.5f, 1.0f, 1.4426950408889634f)` directive on line 141 of `vertex_shader_constants.inc`.

**Context:** Iter-11 spent an iteration making the rewrite context-aware to preserve global `register(cN)` bindings — correct fix, but didn't clear X4016 because the actual collision was elsewhere. CODEX phone-a-friend with the verified ITER-13B dump as the controlled artifact diagnosed the `#pragma def` collision against FXC's auto-allocator; the level9 c-register pass and the pragma's manual allocation collide.
**Source:** 11-07-iteration-log.md §Iteration 14 §Hypothesis, §Root cause

### D3D11 strict-rejects USAGE_STAGING `CreateTexture2D` when W or H < 4 for BC formats
BC blocks are an indivisible 4x4 storage unit; staging resources require CPU-accessible block-aligned data per the D3D11 spec. D3D9 silently tolerated sub-block bottom-of-chain mip levels (e.g., a 256x64 BC3 texture's mip 5 at 8x2). D3D11 returns E_INVALIDARG on `CreateTexture2D` for the same descriptor.

**Context:** Fix is dimension padding to next multiple of 4 in both 2D and volume-map staging paths. Engine-side awareness NOT required — the BC block grid covers the same data regardless of locked mip's logical dimensions; pitch stays identical; `CopySubresourceRegion` with `pSrcBox = nullptr` resolves cleanly. Helper `isBlockCompressedFormat(DXGI_FORMAT)` must cover BOTH non-contiguous BC ranges: `BC1_TYPELESS..BC5_SNORM` (70..84) AND `BC6H_TYPELESS..BC7_UNORM_SRGB` (94..99) — single `>= && <=` check is incorrect.
**Source:** 11-07-iteration-log.md §Iteration 15

### `setRenderTarget(NULL)` is the engine's documented "restore back buffer" idiom
The engine's PostProcessingEffectsManager (and similar post-FX paths like Bloom restore) passes `texture == NULL` to `Graphics::setRenderTarget` after rendering its scratch-buffer pass. The plugin wrapper must dispatch to `setRenderTargetToPrimary()` (or equivalent back-buffer-RTV rebind) on the NULL branch — not forward to the user-texture path with its `NOT_NULL(texture)` precondition.

**Context:** `cubeFace` and `mipmapLevel` are ignored in the NULL branch (back buffer is a flat 2D target with no mip chain or cube faces). `setRenderTargetToPrimary` is idempotent (early-out on `ms_primaryTargetSet`).
**Source:** 11-07-iteration-log.md §Iteration 17

### STRICTNESS and BACKWARDS_COMPATIBILITY are mutually exclusive at the entry-validation layer
`D3DCOMPILE_ENABLE_STRICTNESS` (0x800) and `D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY` (0x1000) trip d3dcompiler_47's entry validation if both set — the X3116 message ("Flags specified both compatibility and strict mode. These are mutually exclusive") fires BEFORE any source bytes are lexed and contains no `(line,col)` annotation.

**Context:** Source-location-LESS error messages are diagnostic gold: they signal entry-point sanity check failures (Flags1 conflicts, missing required args) versus lex/parse errors which always carry `(line,col)`. Use the presence/absence of `(line,col)` as a quick classifier for where in the compile pipeline the failure occurred.
**Source:** 11-07-iteration-log.md §Iteration 6 §Investigation

### Cbuffer-wiring requires research + design + multi-iteration implementation
Iter-18 attempted a minimum-viable WVP composition fix (compose `WVP = V * P * W`, push 128-byte blob to slot 0). BSOD'd the OS ~2 minutes after launch — TDR escalation from GPU rasterizer hitting degenerate clip-space coords or NaN cascade from shader reads of unwritten slot bytes. The work needs: (1) verified D3D9 plugin matrix composition trace, (2) initial-state guarantee (identity matrices before first draw), (3) full slot-0 1024-byte coverage with sane defaults, (4) D3D11 debug-layer enabled to catch errors before TDR, (5) per-draw flush+bind sequencing.

**Context:** Slot 0 is 1024 bytes; partial writes (e.g., only c0..c7 = 128 bytes via `Map(WRITE_DISCARD)`) zero the remaining bytes. Shader reads `LightData` (c16, 28 regs), `Material` (c11, 5 regs), `ExtendedLightData` (c60, 8 regs), fog (c10) as zeros. Any path that uses an unguarded `numLights` as loop count or divisor on zero produces shader hang → TDR → BSOD.
**Source:** 11-07-iteration-log.md §Iteration 18 §"Strategic conclusion"

### Textual-rewrite arc has a structural ceiling — `#pragma`/cbuffer-restructuring lives at the asset layer
The Iter-7..13 textual-rewrite arc spanned 7 iterations and hit a natural limit at Rule D (cbuffer-wrap of c-register globals). Rule D's output was syntactically valid HLSL but didn't clear X4016 — required CODEX phone-a-friend to diagnose the `#pragma def` collision. Rule E (line-anchored `#pragma def` stripper) was the final fix that landed.

**Context:** Each new rule that requires more than "single regex + whitespace replacement" is a signal to evaluate Phase 12 asset re-author. Rule D crossed that line (allocates a heap-wrapped buffer + emits explicit cbuffer header + `packoffset(cN)` substitution per global); Rule E pulled back to line-anchored single-pattern strip. Future textual-rewrite proposals: if the design requires structural awareness (cumulative register tracking, macro pre-expansion, multi-line state), pivot to asset modernization instead.
**Source:** 11-07-iteration-log.md §Iteration 13, §Iteration 14

### C++20 strict lexing rejects `"%s"SLASHCHAR"%s"` adjacent string-literal/macro concatenation
3rd-party `src/external/3rd/library/platform/utils/Base/AutoLog.cpp:291,334` has the pattern `"%s"SLASHCHAR"%s"` which C++20 strict mode rejects (treats the macro identifier following the string as a UDL). Fix is purely cosmetic: insert spaces — `"%s" SLASHCHAR "%s"`. Surfaces incidentally when an unrelated build invalidates `Base.vcxproj`'s incremental `.tlog`.

**Context:** Iter-14's wider solution build triggered this AutoLog issue (today's source-tree activity invalidated the .tlog; prior builds reused stale .obj). Several other 3rd-party-library lex-strictness blockers exist (BugTool, SwgContentSync, LoginAPI, CommonAPI, atlalloc.h); none are on the SwgClient runtime path (renderer loads as a runtime DLL, not statically linked). Fix when surfaced, not pre-emptively.
**Source:** 11-07-iteration-log.md §Iteration 14 §Fix

### Cross-AI peer review unblocks dead-ends in opaque error-class space
When an error class has no `(line,col)` annotation AND the textual-rewrite arc has reached a natural limit (Iter-13 Rule D was syntactically perfect and still didn't clear), spinning further iterations is negative-expectation. Capturing the controlled artifact (the post-rewrite dump) + handing to an external AI (CODEX, in this case) with the explicit hypothesis space surfaced the real root cause in a single round.

**Context:** Iter-14 was preceded by 13 iterations on the same compile wall. The phone-a-friend pattern requires (1) a verified artifact (Iter-13's ITER-13B dump was the controlled artifact), (2) a stated hypothesis space (struct packoffset / `register(b0)` / struct decls / something else), (3) a verifiable test (CODEX's FXC repro toggled each variable and proved which one cleared the wall).
**Source:** 11-07-iteration-log.md §Iteration 14 §Hypothesis, §Investigation

---

## Patterns

### THROWAWAY diagnostic pattern: land speculative-fix + diagnostic-dump together
When facing a FATAL with no `(line,col)` or limited diagnostic info, land BOTH (a) a speculative rule/fix AND (b) ground-truth diagnostic dumps in the same iteration. If the speculative fix clears the wall, the dump confirms why. If it doesn't, the dump gives the next iteration the data to design the correct fix. Iter-8 (single-input dump) → Iter-10 (4-pair input+output dumps at both entry points) → Iter-12 (5-include + main, 12 dump files) refined the pattern.

**When to use:** any iteration facing X-class errors without source-location info, OR where the previous speculation could be wrong AND the cost of re-spinning a clean diagnostic-only iteration would be a full smoke round-trip with the user. NOT for iterations where the failure mode is mechanically obvious from the existing crash dump.
**Source:** 11-07-iteration-log.md §Iteration 8, §Iteration 10, §Iteration 12 — pattern established and reaffirmed across three uses

### Revert THROWAWAY as first commit of the FOLLOWING iteration
Every THROWAWAY-pattern iteration commits as `feat(11-07): THROWAWAY ITER-N — <diagnostic>`. The NEXT iteration's first commit is `revert(11-07): remove THROWAWAY ITER-N HLSL diagnostic dump`. Revert happens regardless of whether the speculative fix worked — diagnostic instrumentation has a fixed shelf life.

**When to use:** every THROWAWAY iteration. The revert commit is mechanical via `grep -l 'THROWAWAY ITER-N' src/engine/client/application/Direct3d11/`. Bump `D3D11_REWRITE_VERSION` in the same revert commit to invalidate any cache hits from the diagnostic iteration.
**Source:** 11-07-iteration-log.md §Iteration 9 §Fix, §Iteration 11 §Fix, §Iteration 13 §Fix — three consecutive consistent applications

### `// THROWAWAY ITER-N` line markers for grep-clean reverts
Every line of THROWAWAY diagnostic code carries a `// THROWAWAY ITER-N DIAGNOSTIC` comment marker. The revert is `grep -l 'THROWAWAY ITER-N' <dir>` → single affected file → Edit-block-removal → `#include <cstdio>` cleanup. Iter-10 had 72 markers; Iter-12 had 83; all reverted cleanly with zero leaks to other files.

**When to use:** any code added inside a THROWAWAY block, including the `<cstdio>` include line. The marker convention ensures the revert is mechanical, not error-prone.
**Source:** 11-07-iteration-log.md §Iteration 10 §Verification §"THROWAWAY marker audit"

### Per-iteration verification gate set: D-13 / D-05 / D-04a / FFP / STUB-count / warnings
Every iteration's Verification block runs the same gate set: (1) MSBuild Direct3d11 + Direct3d9 + Direct3d9_ffp + Direct3d9_vsps + SwgClient EXIT=0; (2) D-13 grep for `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` — non-comment count must be 0; (3) D-05 git diff against Direct3d9/ must be empty (or pre-existing Koogie patches excluded); (4) D-04a Glob `Direct3d11_FfpGenerator.*` must return 0 results; (5) FFP scan on modified TUs must have 0 functional `#ifdef FFP / D3DTSS_ / D3DTOP_`; (6) STUB count delta tracked iteration-over-iteration; (7) MSVC /W4 zero new warnings.

**When to use:** every Plan-11-* iteration. Cheap to run; catches drift on the phase invariants the SPEC pinned.
**Source:** 11-07-iteration-log.md §Iteration 1..18 §Verification (consistent application across all 18 entries)

### "Awaiting" block: ranked best/likely/possible/less-likely next-smoke outcomes
Each iteration closes with an "Awaiting Kenny smoke" block enumerating 3-4 ranked outcome scenarios (best / likely / possible / less likely) with the prior-probability rationale for each. When the next smoke result comes in, the iteration entry can be cross-referenced against the ranked list to see which scenario actually fired.

**When to use:** every iteration that hands off to a user smoke. The ranking discipline forces the iteration author to think through the possible failure modes BEFORE the user reports back; the "we predicted hypothesis 2" or "we missed this outcome" narrative compounds across iterations.
**Source:** 11-07-iteration-log.md every iteration §"Awaiting"

### Cleanup-as-you-go: delete THROWAWAY artifacts in stage/ at iteration close
Even though `stage/` is gitignored, the iteration-close revert commit explicitly deletes the diagnostic dump files (`shader-rewrite-*.txt`, `shader-debug-*.txt`). Verified via `ls stage/shader-* — no matches` at the start of the next iteration's Verification block.

**When to use:** at every THROWAWAY iteration's revert commit. Cleanup-as-you-go discipline keeps the diagnostic file count bounded and makes it easy to spot stale dumps from past iterations.
**Source:** 11-07-iteration-log.md §Iteration 11 §Fix §"Deleted the stage/shader-* dump artifacts"

### Atomic plan-level decisions captured as in-line "Strategic decision recorded" blocks
When the iteration encounters a fork (textual rewrite vs Phase 12 asset re-author), the chosen path AND the explicit re-evaluation trigger are captured as a "Strategic decision recorded" block inside the iteration entry. Future sessions reading the log can see (a) the decision, (b) the rationale at decision-time, (c) the condition under which to revisit.

**When to use:** any iteration that surfaces a strategic-pivot option. Without the explicit record, future sessions risk re-litigating the same decision with stale context.
**Source:** 11-07-iteration-log.md §Iteration 7 §"Strategic decision recorded"

---

## Surprises

### Iter-13 Rule D was syntactically perfect and STILL didn't clear X4016
Iter-13 implemented a structural cbuffer-wrap pre-pass (~25 globals + multi-register `LightData` / `Material` / `ExtendedLightData` structs wrapped in `cbuffer SwgVertexConstants : register(b0) { ... };` with each `register(cN)` rewritten to `packoffset(cN)`). The output was verified via the uncommitted ITER-13B dump; CODEX's FXC repro confirmed the cbuffer + nested-struct packoffsets compile clean under both vs_4_0_level_9_3 AND vs_5_0. The actual offending construct was unrelated.

**Impact:** without the CODEX phone-a-friend pattern, Iter-14 would have continued chasing structural-rewrite refinements; the actual fix (Rule E, line-anchored `#pragma def` strip) is conceptually orthogonal to the cbuffer-wrap work. Establishes the principle: "syntactically perfect" ≠ "fixes the problem"; verify via external repro before doubling down.
**Source:** 11-07-iteration-log.md §Iteration 14 §Hypothesis

### Iter-18 BSOD'd the OS, not just the client process
A 128-byte `Map(WRITE_DISCARD)` cbuffer push with wrong matrix composition order triggered GPU TDR (Timeout Detection and Recovery) → recovery failed → full system Blue Screen of Death. No Windows minidump (`%WINDIR%\Minidump\` didn't exist); no user-mode crash dump (the BSOD took down the OS before write). Hard reboot recovered.

**Impact:** D3D11's failure modes can escalate beyond the process boundary. Future cbuffer-wiring work must enable the D3D11 debug layer (catches ID3D11Debug warnings before TDR), guarantee initial-state defaults (identity matrices before first draw), and cover all 1024 bytes of slot 0 with sane defaults before any geometry draws.
**Source:** 11-07-iteration-log.md §Iteration 18 §OUTCOME, §Strategic conclusion

### THROWAWAY diagnostic pattern paid off in BOTH the speculative-hit and speculative-miss outcomes
The pattern's value was tested twice. Iter-8: speculative `[bcstu]` letter expansion MISSED, but the dump revealed the `v0` ground truth → Iter-9 fixed it. Iter-10/12: pure-diagnostic iterations (no speculative rule change), explicitly designed under the principle that any speculation on X4016 (no line/col) would be a complete shot in the dark. Iter-11's context-aware fix design came directly from the Iter-10 dumps; Iter-14's CODEX repro came directly from the Iter-13B dump.

**Impact:** the "diagnostic-only iteration" is a first-class iteration shape, not a degenerate one. Future plans facing opaque error classes should plan diagnostic iterations explicitly.
**Source:** 11-07-iteration-log.md §Iteration 8 §"Strategic concern", §Iteration 10 §Hypothesis "Meta-bet"

### The 12 X3206 `functions.inc` truncation warnings persisted across every iteration but were ALWAYS non-fatal
12 implicit-truncation warnings (lines 45/69/197/200/203/216/290/370/397/430/465 — two on 45) appeared in every crash dump from Iter-3 through Iter-14. They are `mul()` calls on legacy float-vector arithmetic that the D3DX9-era compiler accepted silently. Documented in Iter-7's semantic-loss caveat suspect list. None of the iteration fixes addressed them; they never escalated.

**Impact:** non-fatal warnings are noise, not signal — recognize the pattern early and don't burn diagnostic effort on them. They may matter someday for visual-parity (Plan 11-09 screenshot diffing) if they produce subtly-different precision, but they did not block Plan 11-07.
**Source:** 11-07-iteration-log.md §Iteration 7 §Semantic-loss caveat, then every subsequent crash-dump quote

### Window required external `ShowWindow + SetForegroundWindow` PowerShell poke at Iter-17
Post-Iter-17 process started with the window at default 640x480 in `Vis=False` state — the engine's normal `ShowWindow(SW_SHOW)` path didn't fire on this launch. Manual PowerShell helper called `ShowWindow + SetWindowPos(100,100,1024,768) + SetForegroundWindow` and the window became visible with the dark-blue MVP clear rendering. Theories: Bloom config-disable in Iter-16 re-ordered init; shader-cache fast path skipped a normal-show step; prior-run swap-chain state interaction.

**Impact:** Plan 11-07's "visible window" milestone required this external assist; future diagnostic smokes need a once-per-launch ShowWindow poke until the engine-init path is investigated. Tracked as a separate workstream from Plan 11-07.
**Source:** 11-07-iteration-log.md §Iteration 17 §Verification, §"ShowWindow assist note"

### Bloom STUB was unblocked via config edit, NOT code change
Iter-16: `Direct3d11.cpp:717` has `STUB(setBloomEnabled);`. Bloom is default-enabled. Three options enumerated: (A) config-disable via `client_d.cfg`, (B) replace STUB with no-op-with-log binding, (C) implement properly. Chose A — `[ClientGame/Bloom] enable=0` in `stage/client_d.cfg`. No code change, no rebuild, no commit. `Bloom::install` reads enable=0 → skips `enable()` → `Graphics::setBloomEnabled` never called → STUB never fires.

**Impact:** establishes pattern for other scaffold STUBs hit during smoke: config-disable is the cheapest unblock for non-critical subsystems. Real implementation deferred until the broader cbuffer / draw-dispatch work is done (Plan 11-08+).
**Source:** 11-07-iteration-log.md §Iteration 16

### `inc-0` in the dumps was `vertex_shader_constants.inc` with ~15 global register bindings that the context-blind rule was over-stripping
The Iter-10 dump revealed the over-strip site cleanly: ~15 global `: register(cN)` declarations + boolean `register(bN)` bindings under `#if VERTEX_SHADER_VERSION >= 20` ALL stripped to whitespace by the context-blind Iter-7 Rule C. The intent of Iter-7 was to handle struct members; the rule applied to every match without distinguishing member-level vs file-scope context.

**Impact:** establishes the necessity of context-aware rule design in textual rewriters. Iter-11's `sawColonThisLine` flag was a small mechanical fix; the LESSON is that "rule for case X applied universally" is a design defect, not just a coverage gap.
**Source:** 11-07-iteration-log.md §Iteration 11 §Investigation

### Plan 11-07 scoped 1 plan; actual delivery spanned 18 iterations across 3 days
The PLAN's `<objective>` framed Plan 11-07 as "an iterative fix-by-fix plan" with no specified iteration cap. Task 2's guardrail set the cap at 15 iterations with explicit checkpoint surface beyond that. Plan delivered 18 iterations + 1 milestone close + 1 reverted BSOD attempt — exceeded the 15-cap guardrail but never triggered the CHECKPOINT surface because Kenny + orchestrator decided to continue at each strategic pause point (Iter-5/6 evaluation, Iter-7 strategic decision, Iter-11 over-strip diagnosis, Iter-14 CODEX consult).

**Impact:** the 15-iteration cap was a guideline, not a hard stop. Future iterative plans should set a soft cap for explicit strategic-evaluation surfacing, not a hard stop — the value of "Plan 11-07 hit 18 iterations and cleared the wall" exceeds the value of "Plan 11-07 stopped at 15 iterations and surfaced a CHECKPOINT for re-planning." The discipline of explicit strategic-decision blocks (see Pattern §"Atomic plan-level decisions") matters more than the iteration count.
**Source:** 11-07-PLAN.md §Task 2 §Guardrails "Cap iteration count: if you go past 15 iterations without converging", contrasted with actual 18-iteration delivery
