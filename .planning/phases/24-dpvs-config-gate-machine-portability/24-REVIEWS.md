---
phase: 24
reviewers: [codex, cursor]
reviewed_at: 2026-06-12
plans_reviewed: [24-01-PLAN.md, 24-02-PLAN.md, 24-03-PLAN.md]
---

# Cross-AI Plan Review — Phase 24

## Codex Review

## Summary

The plans are generally well-researched and aligned with Phase 24’s goals, but they are not execution-ready without tightening a few mechanical and validation gaps. The DPVS knob design is strong. The biggest risks are in the Bloom “no-op” task, Miles absence handling, and the cfg/template sequencing, where the plan text could lead an executor to implement something that builds poorly or still produces the warning flood the phase is meant to eliminate.

## Strengths

- Clear traceability from HARD-01 / PORT-01 / PORT-02 to concrete files and gates.
- DPVS plan uses the right existing seams: `ms_cameraCell` is assigned before the culling block, the Option α branch is at the expected site, and `ms_lastCullingParameters` already debounces parameter updates: [RenderWorld.cpp](/D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:873), [RenderWorld.cpp](/D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:908), [RenderWorld.cpp](/D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp:922).
- Config placement is sensible: `ConfigClientGraphics` already has install-time cached getters and the multi-stream default is exactly where the plan says: [ConfigClientGraphics.cpp](/D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp:104), [ConfigClientGraphics.cpp](/D:/Code/swg-client-v2/src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp:348).
- PORT-01 correctly identifies the current machine-specific Miles postbuild path in both Debug and Release: [SwgClient.vcxproj](/D:/Code/swg-client-v2/src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj:130), [SwgClient.vcxproj](/D:/Code/swg-client-v2/src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj:233).
- PORT-02 cleanup targets are real: the current cfg contains `swg_dev_bundle`, hardcoded `stage/override`, Bloom workaround, dead Vivox/G15 keys, and frame stats instrumentation.

## Concerns

- **HIGH:** Plan 24-01 misdescribes the Bloom change. There is no `setBloomEnabled` implementation at the cited line; it is a `Gl_api` slot assignment through `STUB(setBloomEnabled)`, where `STUB` assigns `scaffold_fatal_stub`: [Direct3d11.cpp](/D:/Code/swg-client-v2/src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:966), [Direct3d11.cpp](/D:/Code/swg-client-v2/src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:1134). The plan should explicitly require adding a correctly typed `setBloomEnabled_impl` function and assigning `ms_glApi.setBloomEnabled = setBloomEnabled_impl`.

- **HIGH:** Plan 24-02’s one-shot Miles warning does not actually stop the known per-sample flood. It adds a startup warning, but leaves the flood site untouched: [Audio.cpp](/D:/Code/swg-client-v2/src/engine/client/library/clientAudio/src/win32/Audio.cpp:1525). If codec files are missing and playback proceeds, the 141k-line flood can still happen. The plan needs either a single global “Miles codec redist missing” flag that suppresses that per-sample warning, or it must disable affected audio paths after the startup probe.

- **MEDIUM:** The Miles postbuild copy still uses `mssmp3.asi` as the only stage marker. If an old or partial `stage/miles` has `mssmp3.asi` but lacks `.m3d` providers, the copy will not repair it. That directly conflicts with the goal of handling broken staged clients. Check all required files or copy/update the redist unconditionally.

- **MEDIUM:** Many automated verification commands are Unix-style (`grep`, `stat`, `test -f`, `ls -la`) in a Windows/PowerShell repo. Some will fail or be unavailable in the expected environment, making acceptance unreliable.

- **MEDIUM:** Plan 24-03 has sequencing ambiguity around verify-then-remove keys. Task 1 says not to remove `runtimeDisableAsynchronousLoader` and `skipSplash=false` yet, while also authoring a cleaned template and Task 2 generating final cfgs from it. That can blur whether tests are using a temporary cfg, the template, or already-final generated files.

- **LOW:** Setup script validation should reject cfg-breaking characters in `TreRoot` such as quotes/newlines, not only `Test-Path -PathType Container`.

## Suggestions

- Amend 24-01 Task 3 to say: add `setBloomEnabled_impl(bool enabled)` or whatever signature `Gl_api::setBloomEnabled` requires, use a static one-shot log inside that function, and assign the slot directly.

- Amend 24-02 Task 3 to suppress the flood path when the startup probe fails. A minimal design: set `s_milesCodecRedistAvailable=false`, emit the one startup `WARNING`, and guard the per-sample `DEBUG_WARNING` at [Audio.cpp](/D:/Code/swg-client-v2/src/engine/client/library/clientAudio/src/win32/Audio.cpp:1525).

- Change the postbuild marker from only `mssmp3.asi` to a stronger condition: copy if any required file is missing, especially `Msseax.m3d` and `msssoft.m3d`.

- Replace plan verification snippets with PowerShell equivalents, e.g. `rg`, `Test-Path`, `(Get-Item ...).Length`, and `git check-ignore`.

- Split 24-03 into two explicit cfg artifacts during execution: a temporary generated cfg for V-key tests, then the final template update after verdicts. That removes ambiguity.

## Risk Assessment

**Overall risk: MEDIUM.** The architecture is sound and the plans cover the phase goals, but there are a few concrete execution traps that could cause build failure, leave the audio warning flood unresolved, or make acceptance checks unreliable. Fixing those plan details should bring the phase down to low-to-medium risk.

---

## Cursor Review

# Phase 24 Plan Review — DPVS Config-Gate + Machine Portability

## 1. Summary

The three plans are **well-grounded in verified source** and should achieve HARD-01, PORT-01, and PORT-02 if executed as written. Research quality is high: gate sites, byte divergences, gitignore reality, and F11 precedence are all confirmed against the live tree (e.g. `RenderWorld.cpp:903-916`, `SwgClient.vcxproj:130`, redist vs `stage/miles` sizes). Wave ordering is sound (24-01/24-02 parallel → 24-03 depends on both). The main gaps are **operational**: stale postbuild Miles staging, missing onboarding-doc updates, a **Bloom no-op implementation detail** in 24-01, and **Debug-build DPVS behavior regression** when `occlusionMode` is absent. Overall these are execution refinements, not architectural flaws.

---

## 2. Strengths

- **Accurate code anchoring** — Plans cite real locations: the Option α `#else` at `RenderWorld.cpp:913-915`, F11 flag at `:83/:909`, `KEY_BOOL(disableMultiStreamVertexBuffers, true)` at `ConfigClientGraphics.cpp:104`, machine-specific Miles copy at `SwgClient.vcxproj:130`, and the A1 redist/stage byte divergence (verified: `mssogg.asi` 41,472 B vs 99,840 B).
- **Correct precedence design** — D-04 F11 force-disable applied *after* config/auto logic matches the research pitfall and preserves Phase 23 A/B tooling.
- **A1 correction adopted** — 24-02 explicitly reconciles redist to proven `stage/miles` bytes rather than trusting the May-2025 vendored vintage; this directly addresses the highest PORT-01 audio risk.
- **Sensible wave split** — DPVS knob + engine-default flips (24-01), Miles vendoring/postbuild (24-02), and cfg templating (24-03) are loosely coupled with clear dependencies; 24-03 correctly waits for D-07 flips before omitting cfg keys.
- **Human checkpoints at the right gates** — Dual-renderer boot, DPVS overlay walk-through, audio play/absence-injection, and D-10 fresh-clone are the right validation for a phase with no unit-test harness.
- **Gitignore reality handled** — Plans correctly note cfgs are already untracked via `stage/*` and avoid pointless `git rm --cached`; template lives outside `stage/` per `.gitignore:92-93`.
- **PORT-02 audit is thorough** — Per-key K/R/V verdicts with explicit boot+play for risky keys (`runtimeDisableAsynchronousLoader`) match D-05 and reduce cfg-regression risk.
- **Threat models are proportionate** — TRE-root validation, unrecognized `occlusionMode` → `off`, and binary provenance are the right concerns for this surface.

---

## 3. Concerns

| Severity | Concern |
|----------|---------|
| **HIGH** | **Postbuild Miles copy is "create once, never refresh."** The `if not exist "...stage\miles\mssmp3.asi"` guard means existing checkouts keep stale/wrong `stage/miles` after 24-02 reconciles redist. Fresh-clone test (delete + rebuild) will pass; day-to-day devs may not. This undermines A1 reconciliation silently. |
| **MEDIUM** | **24-01 Task 3 Bloom fix is mis-specified.** There is no `setBloomEnabled` implementation body — it's `STUB(setBloomEnabled)` → `scaffold_fatal_stub` at `Direct3d11.cpp:1134`. Fix requires a `setBloomEnabled_impl(bool)` (like `setPointSize_impl` at `:309`) and `ms_glApi.setBloomEnabled = setBloomEnabled_impl`, not editing a nonexistent function body. |
| **MEDIUM** | **Debug DPVS default changes without cfg key.** Today `_DEBUG` always enables `OCCLUSION_CULLING` unless F11 (`RenderWorld.cpp:908-910`). After 24-01, absent key → `off` in Debug too (D-02). Intentional, but developers lose the Phase-23 default A/B posture until they add `occlusionMode=auto` or use F11. Task 4 tests `auto` explicitly but doesn't flag this workflow shift. |
| **MEDIUM** | **Onboarding/docs not in scope.** `docs/build.md:169-196` still instructs editing `build/bin/Debug/client.cfg` with CMake-era paths. Phase 24's portability win won't reach the next developer without a doc/README update (even a short `tools/setup/README.md`). |
| **MEDIUM** | **`loginServerAddress0` stays machine-specific.** Current cfgs use `192.168.1.200` (`stage/client_d.cfg:76`), not the templatized TRE root. 24-03 marks it "K" but doesn't parameterize it — fresh clones on other networks need manual cfg edits post-setup. |
| **LOW** | **`ms_cameraCell` null deref in `auto` mode.** Plan accepts residual risk; if `getParentCell()` ever returns null at `:873`, `isWorldCell()` crashes. A null-guard defaulting to `off` would be cheap insurance. |
| **LOW** | **Automated verify commands assume bash** (`grep`, `stat -c`, `test -f`) on a Windows/PowerShell repo. Executor may need PowerShell equivalents or Git Bash explicitly noted. |
| **LOW** | **`.gitignore` comment stale** — line 84 still says cfg template lives in `src/cmake/config/`; 24-03 only "confirms" gitignore, doesn't update the misleading comment. |
| **LOW** | **Setup script doesn't stage Miles pre-build.** D-12a warns on absence but doesn't copy from redist; first-time workflow is "setup → build → boot." Acceptable if documented in script output, but easy to misread as broken audio before first build. |
| **LOW** | **Redist/stage file-set mismatch beyond reconciled files.** Redist includes `mss32.dll`, `mssdolby.flt`, `msssrs.flt`; `stage/miles` lacks `mss32.dll` today. Plan doesn't document whether xcopy of full redist is correct vs a curated 10-file set — likely fine, but worth a one-line manifest in the setup script validation. |

---

## 4. Suggestions

- **24-02 Task 2:** Add a one-time refresh strategy — e.g. compare `mssogg.asi` size/timestamp in stage vs redist, or document "delete `stage/miles` once after pulling 24-02" in SUMMARY. Alternatively, change postbuild to always xcopy when redist is newer (still additive to Koogie pattern).
- **24-01 Task 3:** Specify the exact Bloom pattern:
  ```cpp
  void setBloomEnabled_impl(bool /*enabled*/) {}
  // ...
  ms_glApi.setBloomEnabled = setBloomEnabled_impl;  // replaces STUB(setBloomEnabled)
  ```
  Optional one-shot `DEBUG_REPORT_LOG_PRINT` on first call only if you want visibility; empty no-op matches `setPointSize_impl`.
- **24-01 Task 4 / SUMMARY:** Note explicitly that Debug builds without `occlusionMode` will no longer have occlusion on by default; recommend adding `occlusionMode=auto` to the generated template for Debug-oriented devs (even while shipped default stays `off` when key absent).
- **24-03 Task 2:** Add `@LOGIN_SERVER@` placeholder (default `192.168.1.200` or prompt) alongside `@TRE_ROOT@` for true machine portability; or document in template header that login IP is per-developer.
- **24-03 (new micro-task or Task 2 addendum):** Update `docs/build.md` Runtime asset setup section to point at `tools/setup/setup-client.ps1` and `stage/client_d.cfg`; fix `.gitignore:84` comment. Phase 5 onboarding intent is partially fulfilled here.
- **24-03 Task 2:** Print a clear "next steps" banner: `1) Build SwgClient  2) postbuild stages miles  3) launch stage/SwgClient_d.exe`.
- **24-02 Task 3:** Resolve probe paths against the process working directory (typically `stage/` when launching `SwgClient_d.exe`) — `AIL_set_redist_directory("miles")` returns a relative path; concatenation logic should be explicit in the task.
- **24-01 Task 2:** Consider `if (ms_cameraCell && ms_cameraCell->isWorldCell())` in `auto` branch — three lines, eliminates T-24-02 entirely.
- **Cross-plan:** After 24-03, add a one-line note to `AGENTS.md` workstreams table pointing new clones at `tools/setup/setup-client.ps1`.

---

## 5. Risk Assessment

**Overall: MEDIUM**

**Justification:**

- **Core engineering risk is LOW** — DPVS knob is ~30 lines against existing infrastructure; config getter pattern is proven; F11 interplay is specified correctly; Miles byte reconciliation is the right call given verified divergence.
- **Integration/ops risk is MEDIUM** — Portability success depends on human gates (fresh clone, dual renderer, audio injection) and a new setup script with no prior precedent in-tree (`tools/setup/` does not exist yet). Stale `stage/miles` on existing checkouts and undocumented onboarding create real failure modes that automated greps won't catch.
- **Regression risk is MEDIUM-LOW** — D-07 multi-stream default flip affects skinned mesh rendering on both renderers (plan boot-gates this). `runtimeDisableAsynchronousLoader` removal is correctly flagged V with play test. Bloom no-op is low risk once implemented with correct `_impl` signature.

**Phase goal coverage:**

| Requirement | Plans cover it? |
|-------------|-----------------|
| HARD-01 (config-gated DPVS) | Yes — 24-01 fully; verified via Debug overlay + F11 |
| PORT-01 (no machine paths, miles handled) | Yes — 24-02 + 24-03; weakened slightly by stale postbuild guard and non-templatized login IP |
| PORT-02 (cfg cleanup, dual boot) | Yes — 24-01 engine prereqs + 24-03 template/audit |

**Verdict:** Plans are **ready to execute** with the Bloom implementation correction and a postbuild refresh strategy added. Without those, audio regression on existing checkouts and a possible Bloom compile/assign mistake are the most likely execution failures.

---

## Consensus Summary

Both reviewers rated overall risk **MEDIUM** and agreed the architecture is sound, plans are well-grounded in verified source, and the phase goals are covered. Both flagged the same execution traps.

### Agreed Strengths
- Accurate code anchoring — all cited file:line targets verified against the live tree by both reviewers (RenderWorld.cpp gate site, ConfigClientGraphics defaults, SwgClient.vcxproj postbuild, redist byte divergence).
- A1 correction correctly adopted — reconciling redist to the proven stage/miles bytes addresses the highest PORT-01 audio risk.
- Sound wave structure — 24-01/24-02 parallel, 24-03 correctly waits for the D-07 flips before omitting cfg keys.
- Human checkpoints (dual-renderer boot, DPVS overlay walk, audio absence-injection, fresh-clone) are the right validation for a phase with no unit-test harness.

### Agreed Concerns (2+ reviewers — highest priority)
1. **Bloom task mis-specified (Codex HIGH / Cursor MEDIUM).** There is no `setBloomEnabled` function body to edit — `Direct3d11.cpp:1134` is a `Gl_api` slot assignment via `STUB(setBloomEnabled)` → `scaffold_fatal_stub`. Plan 24-01 Task 3 must require adding a correctly-typed `setBloomEnabled_impl(bool)` (pattern: `setPointSize_impl` at :309) and assigning `ms_glApi.setBloomEnabled = setBloomEnabled_impl`.
2. **Postbuild Miles guard is "create once, never refresh/repair" (Cursor HIGH / Codex MEDIUM).** The `if not exist ...mssmp3.asi` marker means (a) existing checkouts keep stale pre-reconciliation bytes after 24-02 lands, and (b) a partial stage/miles with mssmp3.asi but missing `.m3d` providers is never repaired. Fix: check the full required file set (especially `Msseax.m3d`/`msssoft.m3d`) or copy unconditionally/when-newer — still additive to Koogie's pattern.
3. **Unix-style automated verify commands on a Windows repo (Codex MEDIUM / Cursor LOW).** `grep`/`stat -c`/`test -f`/`ls -la` snippets may fail in the executor's shell; note Git Bash explicitly or provide PowerShell equivalents.

### Significant Single-Reviewer Concerns
- **Codex HIGH — one-shot warning doesn't stop the flood.** The D-12 startup warning alone leaves the per-sample flood site (`Audio.cpp:1525`) live; if codecs are missing and playback proceeds, the 141k-line flood recurs. Set a global `s_milesCodecRedistAvailable=false` on probe failure and guard the per-sample warning with it.
- **Cursor MEDIUM — Debug-build DPVS default shifts.** Today `_DEBUG` always enables occlusion unless F11; after 24-01, absent key → `off` everywhere. Intentional (D-02) but devs lose the Phase-23 posture — document it, and consider `occlusionMode=auto` in the generated Debug template.
- **Cursor MEDIUM — onboarding docs out of scope.** `docs/build.md:169-196` still gives CMake-era cfg instructions; the portability win won't reach the next developer without a doc touch (or `tools/setup/README.md`).
- **Cursor MEDIUM — `loginServerAddress0=192.168.1.200` stays machine-specific** (kept as K, not parameterized). Consider a `@LOGIN_SERVER@` placeholder or a documented per-developer note.
- **Codex MEDIUM — 24-03 V-key sequencing ambiguity.** Make explicit which cfg artifact the verify-then-remove boot tests use (temporary generated cfg vs final template) so verdicts and the final template don't blur.

### Divergent Views
- Cursor judges the plans "ready to execute" with two corrections; Codex says "not execution-ready without tightening" — same fixes, different framing. No substantive disagreement.
- Minor uniques worth a cheap line each: null-guard `ms_cameraCell` in the auto branch (Cursor LOW), validate `TreRoot` for cfg-breaking characters (Codex LOW), fix the stale `.gitignore:84` comment (Cursor LOW), setup script "next steps" banner (Cursor LOW).
