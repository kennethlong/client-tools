---
phase: 34
reviewers: [codex, cursor, sonnet, opus]
reviewed_at: 2026-06-17
plans_reviewed: [34-01-PLAN.md, 34-02-PLAN.md]
self_skipped: claude (running as Claude Code — skipped its own CLI for independence)
unavailable: [gemini, qwen, opencode]
---

# Cross-AI Plan Review — Phase 34 (x64 D3D11 Renderer)

Four independent reviewers on non-overlapping angles: **Codex** (repo tracer), **Cursor** (detailed
code reader, live-tree grounded), **fresh Sonnet** (lateral), **fresh Opus** (gate-sufficiency /
spec). All four returned **overall risk = MEDIUM** with Plan 01 (build/link) low-risk and Plan 02
(boot/parity verify) carrying the residual risk. Two contested findings were resolved against the
live tree (see **Divergent Views**).

---

## Codex Review (repo tracer) — Risk: MEDIUM

**Summary:** The "mirror gl05, fix-what-breaks" strategy is sound (gl11 was P31-swept, no D3DX
burden), and the plan correctly separates plugin-link validation from the SwgClient `/FORCE`
false-clean behavior. Highest concern: the declared validation scope (Debug|x64 only) conflicts with
SC#1's literal wording (`gl11_d.dll` / `gl11_r.dll`).

**Strengths:** mirror strategy justified by P31 sweep; plugin-link vs `/FORCE` separation (still greps
unresolved); shared-header ABI STOP condition; 32-bit gl11 non-regression boot included; RenderDoc
gl11-Win32-vs-gl11-x64 is the right A/B shape; stage-x64 isolation + BOM-safe cfg.

**Concerns:**
- **HIGH — SC#1 not fully proven.** ROADMAP SC#1 names both `gl11_d.dll` AND `gl11_r.dll`, but the
  plan only links Debug|x64. Authoring the Release|x64 block is not evidence it builds/stages. Either
  link Release|x64 for gl11 once, or formally amend SC#1 to "Debug|x64 only, Release deferred."
- **HIGH — `d3dcompiler_47.dll` staging may be brittle.** Copying `%SystemRoot%\Sysnative\…` only
  works from a 32-bit process; if VS18 MSBuild is 64-bit, `Sysnative` may not resolve. Verify the
  *staged* DLL machine type is 8664, not just that the file exists.
- **MEDIUM — representative draws can miss real D3D11 x64 regressions.** dot3/bump/terrain/FFP are
  necessary but not sufficient for the v2.2 bar; gamma, mini-map, UI/load-screen, face/skin/clothing,
  blend factors, particle/ribbon, interior lighting are all in the parity history and absent from the
  draw list.
- **MEDIUM — D3D11 x64 risks deferred but not forced into coverage.** cbuffer packing/alignment,
  reflection offset assumptions, shader-cache key stability, compile-cache cold-start. Deliberately
  exercise cold vs warm cache.
- **MEDIUM — under-specified log/runtime oracles.** Capture client logs, DEBUG_FATAL/assert output,
  shader-compile failures, device-creation warnings — a visually-plausible scene with silent shader
  fallback or cache-miss spam should not pass unnoticed.
- **MEDIUM — 32-bit non-regression is boot-only** while the plan also treats it as the v2.2 reference;
  if it's the A/B baseline it should render the same triad, not just boot.
- **LOW — rollback/cfg hygiene** (record original rasterMajor for both cfgs); **LOW — mechanical
  mirror may copy gl05 assumptions** (postbuild conventions, delay-load, import-lib order, PCH names).

---

## Cursor Review (detailed code reader, live-tree grounded) — Risk: MEDIUM (Plan 01 LOW / Plan 02 MED–HIGH)

**Summary:** Well-scoped Phase-33-repeat on a simpler plugin; the mechanical mirror is grounded in the
live tree (gl05 template lines verified, gl11 has zero x64 configs today). The main weakness is
verification oracles: SC#2's "v2.2 parity bar" is only partially automated and one acceptance
criterion (matching EIDs) is technically wrong and could produce a false clean bill of health.

**Strengths:** correct template choice + gl11-specialization; link-gate discipline right for a
non-`/FORCE` plugin; isolation preserved; ABI cascade guardrail; arch-only A/B architecturally sound;
D-04 scope discipline; D-01 justified in code (gl11 already has hash-to-`uintptr_t` keys in
`Direct3d11_StaticVertexBufferData.cpp:167-173`, cbuffer `static_assert`s in
`Direct3d11_ConstantBuffer.h`, no inline asm).

**Concerns:**
- **HIGH — RenderDoc A/B can false-pass if EIDs are matched naively.** Plan 02 Task 2 says "matching
  EIDs to the Task-1 x64 captures." **EIDs are capture-local event indices — they do NOT correspond
  across two separate `.rdc` files** even for the same scene. An executor following the letter could
  diff unrelated draws and record PASS. *Fix:* match by **semantic identity** (shader entry name,
  texture bindings, `draws --filter` on dot3/bump/terrain), then compare pixels at a fixed coord.
  Document that EID numbers are not portable across captures.
- **HIGH — SC#2 "v2.2 parity bar" under-specified; thin triad sampling can miss real regressions.**
  Three draws + human checkpoint is reasonable for a *phase gate* but not sufficient to prove the full
  v2.2 bar. Mini-map (`ps.1.1` alpha-mask, fixed e4f94a0f6), gamma (493287510), and other shader paths
  are absent from the diff list. *Fix:* add a mini-map frame + a gamma-sensitive scene, or downgrade
  SC#2 language to "triad parity at v2.2 bar (subset)."
- **MEDIUM — shader-cache path is cwd-sensitive; first x64 boot underspecified.**
  `ConfigDirect3d11.cpp:57` hardcodes `ms_shaderCacheDir = "stage/shader-cache/"`. Launching from
  `stage-x64/` resolves it to `stage-x64/stage/shader-cache/` — NOT the warm 32-bit cache. So the
  first x64 boot is a **cold D3DCompile**, longer startup, and the boot gate may look like a hang/TDR
  rather than a link failure. *Fix:* add a boot pre-flight note (confirm cwd, expect cold compile,
  optionally seed the cache for a fair A/B — bytecode is arch-neutral).
- **MEDIUM — DllExport ProjectReference x64 assumed transitive.** Confirm `stage-x64/DllExport.dll` is
  machine 8664 before the Plan 02 boot; the Phase-33 DLL checklist does not yet list `gl11_d.dll`.
- **MEDIUM — D-01 "don't pre-audit" defers the only plausible D3D11 x64 runtime surprises:** cold
  shader compile + HlslRewrite under x64 codegen, and debug-layer/validation timing differences that
  could read as false RenderDoc divergence. Acceptable *if* the A/B methodology is fixed (see HIGH).
- **MEDIUM — non-regression coverage narrower than "four green boots" implies:** missing gl06/gl07 x64
  in the shared client and 32-bit gl11 Release/Optimized. *Fix:* `dumpbin`-sweep that ALL
  `stage-x64/*.dll` remain 8664 after the gl11 postbuild (Phase-33 Pitfall-2 pattern).
- **MEDIUM — no explicit rollback** beyond "STOP and flag"; no escalation task for the shared-header
  ABI wall.
- **LOW — Release|x64 authored-but-unbuilt can bitrot;** **LOW — Plan 01 Task 1 grep verifies
  presence, not correctness** (Task 3 link gate covers most of it).

---

## Sonnet Review (lateral) — Risk: MEDIUM

**Summary:** On its narrow scope the plans are well-engineered; every gl11-vs-gl05 asymmetry that
matters (no D3DX, no FFP/VSPS, different lib list, same dir depth) is correctly identified and the
acceptance grids are dense. One structural divergence is not gated: gl11's Win32 link blocks carry a
`BaseAddress` that the x64 blocks must drop, with no grep-check to confirm absence. (Sonnet read
`x64-platform.props` and confirmed `LanguageStandard>stdcpp20` and `_DEBUG`/`DEBUG_LEVEL` come from
the props — so the per-project x64 block correctly omits them.)

**Strengths:** lib-list delta nailed (keeps d3d11/d3dcompiler/dxgi, no d3dx9); props-path correctness
by design; `LanguageStandard` + `_DEBUG`/`DEBUG_LEVEL` correctly delegated to the props; Sysnative
d3dcompiler_47 right; `/SAFESEH:NO` exclusion correct + checked; blocking human checkpoint on a visual
phase; fix-what-breaks STOP discipline.

**Concerns:**
- **HIGH — no acceptance grep for `BaseAddress` absent from x64 blocks.** gl11 Win32 carries
  `<BaseAddress>0x60A00000</BaseAddress>` (lines 126/186/243); gl05 x64 correctly omits it. A
  copy/paste from gl11's own Win32 block would silently embed a 32-bit preferred base in the x64 DLL —
  invisible to every current check (`dumpbin` still reads 8664, link succeeds, DLL loads). *Fix:*
  `grep -c "BaseAddress" …Direct3d11.vcxproj` == 3. *(Synthesis note: linker silently ignores a
  32-bit base in an x64 image, so impact is cosmetic — re-rated MEDIUM — but the detection gap is
  real and the guard is one line.)*
- **HIGH — D-01 reflection-path x64 truncation risk the A/B triad cannot catch.**
  `ID3D11ShaderReflection` calls return pointer-/`UINT`-sized values that drive cbuffer slot
  assignment. A leftover `int` local receiving a `UINT`/`SIZE_T` reflection return would misbind a
  resource only past a size/high-bit threshold — passing the triad cleanly. *Fix:* a scoped 15-minute
  grep of `Direct3d11_ShaderImplementationData.cpp`, `Direct3d11_ShaderCache.cpp`,
  `Direct3d11_ConstantBuffer.cpp` for `int`/`UINT` locals taking reflection returns — validates D-01
  by evidence rather than assumption.
- **MEDIUM — mini-map, UI, gamma unverified** in the triad EIDs (same as Cursor/Codex). Add mini-map +
  gamma bullets to the Task 3 human-verify checklist.
- **MEDIUM — Config PropertyGroup insertion-point phrasing** ("after Debug|Win32 at line 39") is
  slightly ambiguous given Optimized/Release/Debug ordering; verify post-edit with a
  `Label="Configuration"` ordering grep.
- **MEDIUM — `InlineFunctionExpansion` omission** from the x64 Debug ClCompile (gl11 Win32 Debug sets
  `OnlyExplicitInline`) could give the x64 side a different inline level, confounding A/B
  interpretation ("arch difference" vs "inline difference"). Add it for codegen-parity interpretability.
- **MEDIUM — `TargetName` vs `OutputFile` redundancy** (low impact, slight inconsistency with the gl05
  x64 template).
- **LOW — `2>&1 | Tee-Object` on MSBuild** (PS 5.1 wraps native stderr as NativeCommandError, can mask
  linker diagnostics — AGENTS.md warns against it). Prefer `/flp:logfile=…;verbosity=normal` + grep the
  file. **LOW — Plan 02 Task 2 automated verify lists nonexistent `.rdc` files** (captures aren't
  committed — false sense of gate coverage). **LOW — `stage-x64/client_d.cfg` may be missing** with no
  fallback (`if not exist … copy stage\client_d.cfg`).

---

## Opus Review (gate-sufficiency / spec) — Risk: MEDIUM

**Summary:** Unusually well-specified for a build-platform mirror — line-accurate insertion points (all
verified live), a verbatim gl05 template, correct grep gates for the six regions, and the right parity
oracle. Two real correctness gaps let a defective build pass the gates: a (later-falsified)
`LanguageStandard` concern, and — the durable one — **Plan 01's gate never links the EXE or
runtime-loads the plugin**, so the DllExport/DelayLoad bridge and the whole export surface are deferred
to Plan 02's boot with no fallback.

**Strengths:** every insertion point + gl05 template ref verified accurate against the live tree (rare;
materially reduces execution risk); the grep-count gates are self-consistent and correct
(`Debug|x64`≥5, `x64-platform.props`==2, `MachineX64`==2, `Optimized|x64`==0 — no off-by-one);
`/FORCE` discipline correctly internalized; arch-only oracle is a clean controlled experiment;
byte-unchanged 32-bit guard is concrete; D-04 author-both-link-Debug is consistent with the swg.sln
`.Build.0` registration.

**Concerns:**
- **HIGH — ~~`LanguageStandard` (`stdcpp20`) silently dropped~~ → FALSIFIED.** Opus reasoned the x64
  ClCompile mirrors gl05's template (which omits `LanguageStandard`) → C++14 default. **Resolved
  against ground truth: `x64-platform.props:50` sets `<LanguageStandard>stdcpp20</LanguageStandard>`
  in its shared ClCompile group, which every x64 vcxproj imports.** The omission is correct
  (inherited). NON-ISSUE — do not "fix" by adding it to the per-project block.
- **HIGH — Plan 01's gate proves "the DLL links," NOT "the DLL is loadable / exports resolve."**
  `DelayLoadDLLs DllExport.dll` by construction does **not** force symbol resolution at link — the
  loader resolves on first call at runtime. So `unresolved external symbol == 0` cannot establish the
  bridge works on x64, and the plugin's own host-loaded `DIRECT3D11_EXPORTS` entrypoints aren't
  validated by the plugin link either. Both collapse into Plan 02's un-isolated boot. *Fix:* add a
  `dumpbin /exports stage-x64\gl11_d.dll` gate in Plan 01 asserting the host-facing export(s) are
  present + x64-decorated — catches an export-surface break at build time, not at boot.
- **MEDIUM — SC#2 parity is a 3-draw sample asserted PASS-by-default, no divergence tolerance.**
  `pixel`/`export-rt` will differ in the LSB on legit SSE-vs-x87 rounding (RESEARCH A3 itself notes
  this). Without a stated tolerance the executor either false-stops on a 1-LSB delta or hand-waves
  "looks the same." *Fix:* state tolerance numerically (byte-identical bytecode on `shader`, ≤1 ULP on
  `debug pixel --trace`, visually-identical `export-rt`), and widen the sample or downgrade the A/B's
  claimed proof role.
- **MEDIUM — the "benign C4267 size_t→int" posture (D-01) is unverified for the x64 compile.** This is
  the exact class that bit Phase 33 (TextManager size_t→int, one root / three faces). On x64 `size_t`
  is 64-bit; a count/index truncation may not manifest on the sampled draws. *Fix (cheap, non-blocking):*
  grep the x64 build log for `C4267`/`C4244` and record the count in the SUMMARY — make it visible
  rather than suppressed by assertion.
- **LOW — D-04 arms an untested config.** `{DC3F2C16}.Release|x64.Build.0 = Release|x64` means a full
  `/p:Platform=x64` solution build will try to link gl11 Release|x64 (declared unproven) with no gate.
  *Fix:* note in the SUMMARY/handoff that Release|x64 is registered-but-never-linked; don't run a full
  x64 Release solution build until the consolidation phase.
- **LOW — PCH-Create gate under-specified:** the `Debug|x64`≥5 gate passes with or without the two
  x64 `FirstDirect3d11.cpp` PCH-Create conditions; a missing one yields a confusing C1853. *Fix:*
  `grep -c "PrecompiledHeader Condition=.*x64.*Create"` == 2.

---

## Consensus Summary

All four reviewers independently rated the phase **MEDIUM**: Plan 01 (build/link) is genuinely
low-risk (gl11 is the simplest x64 plugin port — no D3DX, SDK-only link, P31-swept source, a committed
gl05 template verified live), and **all the residual risk lives in Plan 02's verification oracles.**

### Agreed Strengths (2+ reviewers)
- Correct template choice + gl11-specialization (lib list, no D3DX/FFP/VSPS, same props-path depth).
- Link-gate discipline is right for a non-`/FORCE` plugin (grep unresolved + LNK1181, force-relink,
  kill stale nodes, dumpbin 8664).
- `stage-x64/` isolation + BOM-safe cfg + byte-unchanged 32-bit `stage/gl11_d.dll` guard.
- Shared-header ABI cascade STOP guardrail.
- The same-renderer, same-config, arch-only RenderDoc A/B is the architecturally correct diagnostic.
- D-04 scope discipline (author both x64 blocks, link only Debug|x64).

### Agreed Concerns (ranked — fix before execution)
1. **[HIGH] SC#2 parity verification is too thin** *(all 4)* — 3 hand-picked draws + subjective human
   sign-off cannot prove the full v2.2 bar; mini-map, gamma, UI, face/blend/particles are named in the
   parity history but absent from the diff list, and no divergence tolerance is defined. → Add mini-map
   + gamma to the Task 3 checklist (and/or capture set), state a numeric A/B tolerance, OR downgrade
   SC#2's claimed scope to "triad subset, full bar via human sign-off."
2. **[HIGH] RenderDoc A/B methodology — EID matching is wrong** *(Cursor; Opus/Sonnet adjacent on
   tolerance)* — EIDs are capture-local and won't correspond across `.rdc` files; an executor could
   diff unrelated draws and sign off a false PASS. → Match by **semantic draw identity** (shader name /
   bindings / `--filter`), compare at a fixed coord, record draw signatures not EID numbers. **Single
   most actionable execution fix.**
3. **[HIGH] Build-time gate doesn't prove the plugin is loadable** *(Opus HIGH + Cursor MEDIUM)* —
   DelayLoadDLLs + host-loaded exports are asserted, not tested, until the un-isolated boot. → Add
   `dumpbin /exports stage-x64\gl11_d.dll` (host entrypoint present + x64) and confirm
   `stage-x64\DllExport.dll` is 8664 *before* the Plan 02 boot.
4. **[HIGH→spec gap] SC#1 names `gl11_r.dll` but Release|x64 is never linked** *(Codex HIGH; Opus/Cursor
   note the bitrot/armed-config angle)* — either link gl11 Release|x64 once, or formally narrow SC#1 to
   "Debug|x64 this phase, Release deferred (D-04)" so the phase can honestly close. Also note the armed
   `.Build.0` trap for any future full x64 Release solution build.
5. **[MEDIUM] D-01 "don't pre-audit" leaves the only realistic x64 *runtime* risk uncovered** *(Cursor +
   Opus + Sonnet converge)* — shader-reflection / cbuffer / cache `size_t→int` truncation that a thin
   triad won't trip (Phase-33 TextManager precedent). Cheap mitigations: grep the x64 log for
   `C4267`/`C4244` counts (record in SUMMARY), + a scoped ~15-min reflection-caller audit of
   `Direct3d11_ShaderImplementationData.cpp` / `Direct3d11_ShaderCache.cpp` / `Direct3d11_ConstantBuffer.cpp`.
6. **[MEDIUM] `d3dcompiler_47.dll` staging robustness** *(Codex)* + **all `stage-x64/*.dll` stay 8664
   sweep** *(Cursor)* — verify the *staged* DLLs' machine type, don't trust the Sysnative copy blindly.
7. **[MEDIUM] Shader-cache cwd cold-compile on first x64 boot** *(Cursor, code-grounded)* —
   `ConfigDirect3d11.cpp:57` hardcodes `stage/shader-cache/`; launching from `stage-x64/` cold-compiles
   and can look like a hang. Add a boot pre-flight note (and optionally seed the arch-neutral cache for
   a fair A/B).
8. **[MEDIUM] BaseAddress not gated absent from the x64 blocks** *(Sonnet, verified)* — gl11 Win32 has
   `0x60A00000` ×3; the x64 blocks must drop it. Add `grep -c "BaseAddress" == 3`. (Low real impact —
   linker ignores a 32-bit base in an x64 image — but the guard is one line.)
9. **[MEDIUM] Rollback / escalation under-specified** *(Codex, Cursor)* — document the reversible path
   (restore rasterMajor, remove `gl11_d.dll` from stage-x64) and an explicit shared-header ABI
   escalation task instead of an indefinite STOP. Capture runtime log oracles (DEBUG_FATAL, shader
   compile failures) on the boot.
10. **[LOW] Tooling hygiene** — replace `2>&1 | Tee-Object` on the MSBuild line with
    `/flp:logfile=…;verbosity=normal` (PS 5.1 NativeCommandError wrapping, per AGENTS.md); fix the Plan
    02 Task 2 automated verify that lists nonexistent `.rdc` files; add a `PrecompiledHeader …x64…Create`
    == 2 gate and a config-PropertyGroup ordering check.

### Divergent Views (resolved against the live tree)
- **`LanguageStandard`/`stdcpp20`: Opus rated HIGH (dropped → C++14 default); Sonnet said safe.**
  **RESOLVED — Sonnet correct, NON-ISSUE.** `x64-platform.props:50` sets `stdcpp20` in its shared
  ClCompile group; every x64 vcxproj imports it, so the per-project omission inherits it. Opus verified
  gl05-x64 + the plan omit it but didn't read the props' ClCompile contents. **Planner: do NOT add
  `LanguageStandard` to the per-project x64 block.** (Same logic clears the `_DEBUG`/`DEBUG_LEVEL`
  delegation — props-supplied, correctly omitted per-project.)
- **BaseAddress: Sonnet rated HIGH.** Verified real (gl11 Win32 ×3, gl05 x64 omits) but **re-rated
  MEDIUM** — a 32-bit preferred base in an x64 DLL is silently ignored by the linker, so it's a
  detection-gap/tidiness issue, not a malformed-binary risk. Worth the one-line grep guard.
- **Mechanical-mirror safety:** Codex/Cursor judged it mostly safe; Sonnet flags the real failure mode
  precisely — it's safe *only if the executor copies from the gl05 x64 blocks, not gl11's own Win32
  blocks* (the BaseAddress/InlineFunctionExpansion deltas are exactly where a Win32 copy/paste bites).
  Convergent: keep gl05:304-377 as the single copy source; gates #3 (exports), #8 (BaseAddress) catch a
  Win32-source slip.

---

## To incorporate into planning

```
/gsd-plan-phase 34 --reviews
```

Highest-leverage edits the replan should make: (2) semantic-draw matching in Plan 02 Task 2 [fixes a
signable false-negative], (3) `dumpbin /exports` + DllExport-8664 gate in Plan 01, (1) broaden the SC#2
oracle / state tolerance, (4) resolve the SC#1-vs-Release|x64 wording, (5) the cheap C4267-count +
3-file reflection-grep for D-01, (7) the shader-cache cwd pre-flight, plus the (8)/(10) one-line gates.
**Do not** act on the falsified `stdcpp20` finding.
