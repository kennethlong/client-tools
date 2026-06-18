---
phase: 34
slug: x64-d3d11-renderer
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-06-17
---

# Phase 34 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Renderer validation has always been **build-gate + dumpbin + boot + RenderDoc**, not a unit suite
> (consistent with Phases 17–33). There is no renderer unit-test framework; the automated surface is
> the MSBuild link-grep + `dumpbin` machine check, and the observed surface is boot-to-char-select +
> the RenderDoc arch-only A/B.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None (no renderer unit-test framework) — validation = MSBuild link-gate + `dumpbin` + boot + RenderDoc CLI diff |
| **Config file** | `src/build/win32/swg.sln` (build); `stage-x64/client_d.cfg` (runtime `rasterMajor=11` select, BOM-safe) |
| **Quick run command** | `Select-String -Path build-34-gl11-x64.log -Pattern "unresolved external symbol"` (must be empty) + `dumpbin /headers stage-x64\gl11_d.dll` → `machine (x64)` |
| **Full suite command** | `$env:MSBUILD swg.sln /t:Direct3d11 /p:Configuration=Debug /p:Platform=x64 /nodeReuse:false` + dumpbin machine-x64 + boot `stage-x64\SwgClient_d.exe` rasterMajor=11 + RenderDoc triad A/B |
| **Estimated runtime** | ~build 2–5 min · boot+triad capture ~10 min |

---

## Sampling Rate

- **After every task commit (vcxproj/sln edit):** Run targeted `/t:Direct3d11 /p:Platform=x64` build → link-grep `unresolved external symbol` == 0 + `dumpbin /headers` machine-x64.
- **After every plan wave:** Boot `stage-x64\SwgClient_d.exe` (rasterMajor=11) to char-select — Debug|x64 surface with assert / `DEBUG_FATAL` oracles live (D-03).
- **Before `/gsd-verify-work`:** RenderDoc arch-only A/B clean on the full triad + both non-regression boots (32-bit gl11 + x64 gl05).
- **Max feedback latency:** ~5 min (targeted x64 link + dumpbin).

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated/Observed Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 34-01-* | 01 | 1 | X64-03 (SC#1) | — | N/A | build-gate | `Select-String build-34-gl11-x64.log "unresolved external symbol"` == empty; `dumpbin /headers stage-x64\gl11_d.dll` → `8664 machine (x64)`; `LNK1181` == 0 | ✅ build infra | ⬜ pending |
| 34-02-* | 02 | 2 | X64-03 (SC#2) | — | N/A | boot + RenderDoc A/B | Boot `stage-x64\SwgClient_d.exe` rasterMajor=11 → char-select; `renderdoc-cli.exe` capture gl11 Debug\|x64 vs gl11 32-bit Debug on triad → no arch-only divergence on dot3/bump/terrain draws | ✅ RenderDoc CLI present | ⬜ pending |
| 34-02-* | 02 | 2 | X64-03 (SC#3 + cross) | — | N/A | boot non-regression | Boot 32-bit `stage\SwgClient_d.exe` rasterMajor=11 → char-select; boot `stage-x64\SwgClient_d.exe` rasterMajor=5 → char-select (gl05 shared x64 client) | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- None — existing build / boot / RenderDoc infrastructure covers all X64-03 criteria. No new test files, no framework install.

*Existing infrastructure (MSBuild link-gate + dumpbin + boot + RenderDoc CLI at `D:\Code\renderdoc-mcp\v0.3.0\...\bin\`) covers all phase requirements.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| v2.2 visual-parity bar on x64 (SC#2) | X64-03 | No automated pixel oracle for the full v2.2 parity battle; RenderDoc arch-only A/B + human triad inspection is the diagnostic | Capture gl11 32-bit Debug (v2.2 reference) and gl11 Debug\|x64 on dressed char-select + Tatooine + cantina interior; diff representative dot3/bump/terrain draws — a clean A/B = no arch-only divergence |
| Boot-to-char-select under rasterMajor=11 (SC#2/SC#3) | X64-03 | Boot reachability is observed at runtime, not a unit test | Launch the exe with the correct cfg (`stage-x64\client_d.cfg` rasterMajor=11 for x64; `stage\client_d.cfg` for 32-bit) and confirm dressed character-select renders |

---

## Validation Sign-Off

- [x] All tasks have an automated build-gate or observed boot/RenderDoc verify (no Wave 0 deps needed)
- [x] Sampling continuity: every task has a link-grep + dumpbin or boot check; no 3 consecutive tasks without a verify
- [x] Wave 0 covers all MISSING references (none — existing infra)
- [x] No watch-mode flags
- [x] Feedback latency < 300s (targeted x64 link + dumpbin)
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
