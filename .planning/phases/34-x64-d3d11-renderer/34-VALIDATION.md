---
phase: 34
slug: x64-d3d11-renderer
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-06-17
revised: 2026-06-18
revision_note: "Gate commands updated for the --reviews replan (GetApi export gate, BaseAddress/LanguageStandard guards, DLL-bitness sweep, semantic-match A/B + numeric tolerance, clean-log boot oracle)."
---

# Phase 34 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Renderer validation has always been **build-gate + dumpbin + boot + RenderDoc**, not a unit suite
> (consistent with Phases 17–33). There is no renderer unit-test framework; the automated surface is
> the MSBuild link-grep + `dumpbin` machine/exports check, and the observed surface is
> boot-to-char-select (clean log) + the RenderDoc arch-only A/B (semantic-matched, within tolerance).

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None (no renderer unit-test framework) — validation = MSBuild link-gate + `dumpbin` (machine + exports) + boot (clean log) + RenderDoc CLI semantic-matched diff |
| **Config file** | `src/build/win32/swg.sln` (build); `stage-x64/client_d.cfg` (runtime `rasterMajor=11` select, BOM-safe) |
| **Quick run command** | `Select-String -Path build-34-gl11-x64.log -Pattern "unresolved external symbol"` (must be empty) + `dumpbin /headers stage-x64\gl11_d.dll` → `machine (x64)` + `dumpbin /exports stage-x64\gl11_d.dll` → `GetApi` |
| **Full suite command** | `$env:MSBUILD swg.sln /t:Direct3d11 /p:Configuration=Debug /p:Platform=x64 /nodeReuse:false /flp:logfile=build-34-gl11-x64.log;verbosity=normal` + dumpbin machine-x64 + GetApi export + all-stage-x64-DLL 8664 sweep + boot `stage-x64\SwgClient_d.exe` rasterMajor=11 (clean log) + RenderDoc triad A/B (semantic-matched) |
| **Estimated runtime** | ~build 2–5 min · boot+triad capture ~10 min (first x64 boot is a COLD shader-compile — longer; not a hang) |

---

## Sampling Rate

- **After every task commit (vcxproj/sln edit):** Run targeted `/t:Direct3d11 /p:Platform=x64` build → link-grep `unresolved external symbol` == 0 + `dumpbin /headers` machine-x64 + `dumpbin /exports` GetApi + the BaseAddress/LanguageStandard/PCH guard greps.
- **After every plan wave:** Boot `stage-x64\SwgClient_d.exe` (rasterMajor=11) to char-select — Debug|x64 surface with assert / `DEBUG_FATAL` oracles live (D-03); scan the client log (0 DEBUG_FATAL/assert/shader-compile-failure).
- **Before `/gsd-verify-work`:** RenderDoc arch-only A/B clean (semantic-matched, within tolerance) on the full triad + the all-stage-x64-DLL 8664 sweep + both non-regression boots (32-bit gl11 + x64 gl05) + the human mini-map/gamma sign-off.
- **Max feedback latency:** ~5 min (targeted x64 link + dumpbin machine/exports).

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated/Observed Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 34-01-T1 | 01 | 1 | X64-03 (SC#1) | — | N/A | vcxproj grep-gate | `grep -c "Debug\|x64"` ≥5; `grep -c "x64-platform.props"`==2; `grep -c "MachineX64"`==2; `grep -c "Optimized\|x64"`==0; **`grep -c "BaseAddress"`==3** (Win32-only); **`grep -c "LanguageStandard"`==3** (Win32-only — falsified-finding guard); **`grep -c "PrecompiledHeader Condition=.*x64.*Create"`==2** | ✅ | ⬜ pending |
| 34-01-T2 | 01 | 1 | X64-03 (SC#1) | — | N/A | sln grep-gate | `grep "{DC3F2C16-…}" swg.sln \| grep -c x64`==4; `head -c 3 swg.sln \| xxd` no BOM | ✅ | ⬜ pending |
| 34-01-T3 | 01 | 1 | X64-03 (SC#1) | — | N/A | build-gate + exports | `Select-String build-34-gl11-x64.log "unresolved external symbol"` == empty; `LNK1181`==0; `dumpbin /headers stage-x64\gl11_d.dll` → `8664`; **`dumpbin /exports stage-x64\gl11_d.dll` → `GetApi`** (loadable, not just linkable); `C4267\|C4244` count recorded (watch-list, non-blocking); 32-bit `stage/gl11_d.dll` byte-unchanged | ✅ build infra | ⬜ pending |
| 34-02-T1 | 02 | 2 | X64-03 (SC#2) | — | N/A | boot + bitness sweep + capture | **`dumpbin /headers` 8664 for stage-x64\{gl11_d.dll, DllExport.dll, d3dcompiler_47.dll, SwgClient_d.exe}**; boot `stage-x64\SwgClient_d.exe` rasterMajor=11 → dressed char-select with **clean log (0 DEBUG_FATAL/assert/shader-compile-failure)**; capture triad with per-draw SEMANTIC signature (shader-name+bindings+RT-format+topology) — NOT bare EID | ✅ RenderDoc CLI present | ⬜ pending |
| 34-02-T2 | 02 | 2 | X64-03 (SC#2 + SC#3 + cross) | — | N/A | semantic A/B + non-regression | RenderDoc gl11 32-bit Debug vs gl11 Debug\|x64, draws **matched by SEMANTIC signature not EID**, within tolerance (`shader` byte-identical; `debug pixel --trace` ≤1 ULP; `export-rt` visually identical) on dot3/bump/terrain/FFP; 32-bit `stage\SwgClient_d.exe` rasterMajor=11 → char-select + `stage\gl11_d.dll`==14C; `stage-x64\SwgClient_d.exe` rasterMajor=5 → char-select (gl05 shared x64); cfg restored to 11 | ✅ | ⬜ pending |
| 34-02-T3 | 02 | 2 | X64-03 (SC#2 full bar) | — | N/A | human sign-off | Human "approved" on the v2.2 bar across triad **PLUS mini-map (e4f94a0f6 round-not-square) + gamma (493287510 curve)** — the SC#2 subset/human-bar split — and four green boots | manual | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- None — existing build / boot / RenderDoc infrastructure covers all X64-03 criteria. No new test files, no framework install.

*Existing infrastructure (MSBuild link-gate + dumpbin machine/exports + boot + RenderDoc CLI at `D:\Code\renderdoc-mcp\v0.3.0\...\bin\`) covers all phase requirements.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| v2.2 visual-parity bar on x64 — triad (SC#2) | X64-03 | No automated pixel oracle for the full v2.2 parity battle; RenderDoc arch-only A/B (semantic-matched, within tolerance) + human triad inspection is the diagnostic | Capture gl11 32-bit Debug (v2.2 reference) and gl11 Debug\|x64 on dressed char-select + Tatooine + cantina interior; re-find each representative draw by SEMANTIC signature (shader-name/bindings/RT-format — NOT EID), diff within tolerance (`shader` byte-identical, `debug pixel --trace` ≤1 ULP, `export-rt` visually identical) |
| Mini-map + gamma on x64 (SC#2 full bar) | X64-03 | NOT in the automated triad A/B — covered by human sign-off (the SC#2 subset/human-bar split) | Confirm the mini-map renders round-not-square (e4f94a0f6 `ps.1.1` alpha-mask) and gamma response matches the 32-bit reference (493287510 pre-Present curve, not washed/crushed) |
| Boot-to-char-select under rasterMajor=11 with clean log (SC#2/SC#3) | X64-03 | Boot reachability + log cleanliness is observed at runtime, not a unit test | Launch the exe with the correct cfg (`stage-x64\client_d.cfg` rasterMajor=11 for x64; `stage\client_d.cfg` for 32-bit); confirm dressed char-select renders AND scan the client log for 0 DEBUG_FATAL/assert/shader-compile-failure (expect a COLD shader-compile on the first x64 boot — longer startup, not a hang) |

---

## Validation Sign-Off

- [x] All tasks have an automated build/grep-gate or observed boot/RenderDoc verify (no Wave 0 deps needed)
- [x] Sampling continuity: every task has a link-grep + dumpbin (machine/exports) or boot check; no 3 consecutive tasks without a verify
- [x] Wave 0 covers all MISSING references (none — existing infra)
- [x] No watch-mode flags
- [x] Feedback latency < 300s (targeted x64 link + dumpbin)
- [x] `nyquist_compliant: true` set in frontmatter
- [x] Review gates wired: GetApi export, BaseAddress/LanguageStandard/PCH guards, DLL-bitness sweep, semantic-match A/B + numeric tolerance, clean-log boot oracle

**Approval:** pending
