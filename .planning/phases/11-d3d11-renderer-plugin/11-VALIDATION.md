---
phase: 11
slug: d3d11-renderer-plugin
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-05-15
---

# Phase 11 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Derived from `11-RESEARCH.md` §Validation Architecture (lines 612–660).

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | MSBuild (build-pass) + direct binary inspection (`dumpbin /exports`, `/headers`, `/imports`) + runtime smoke (boot `SwgClient_d.exe` → char-select → zone-in to Tatooine → travel to cantina). No unit-test framework in this codebase (precedent set by Phases 7, 9, 10). |
| **Config file** | None — MSBuild is the test runner; solution is `src/build/win32/swg.sln`. |
| **Quick run command** | `MSBuild D:/Code/swg-client-v2/src/build/win32/swg.sln /t:Direct3d11 /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` |
| **Full suite command** | `MSBuild D:/Code/swg-client-v2/src/build/win32/swg.sln /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` |
| **Estimated runtime** | ~30 s (plugin incremental) / ~5–10 min (full client clean build) |

---

## Sampling Rate

- **After every task commit:** `MSBuild ... /t:Direct3d11` (incremental plugin build) — fast inner loop.
- **After every plan wave:** `MSBuild ... /t:Direct3d9` (D-05 protection) + `MSBuild ... /t:Direct3d11` + `MSBuild ... /t:SwgClient_d` (full link), then boot SwgClient_d.exe under `rasterMajor=5` (D3D9 fallback green) AND under `rasterMajor=11` (D3D11 to whatever scope has landed: clear-to-color → terrain → cantina interior, growing across the phase).
- **Before `/gsd-verify-work`:** Full client build green for both renderers; smoke (Tatooine outdoor + Mos Eisley cantina, ≥5 min each, no crash) green; SPEC R6 screenshots committed; SPEC R7 DPVS verdict doc committed.
- **Max feedback latency:** ~30 s (incremental plugin compile) for per-task; ~10 min (full link + boot) for per-wave.

---

## Per-Task Verification Map

| Req ID | Behavior | Test Type | Automated Command | File Exists |
|--------|----------|-----------|-------------------|-------------|
| D3D11-01 | Plugin scaffold builds, exports `GetApi`, populates Gl_api table | build-pass + binary inspection | `MSBuild ... /t:Direct3d11` then `dumpbin /exports stage/gl11_d.dll \| findstr GetApi` | ❌ W0 (vcxproj does not yet exist) |
| D3D11-01 | Engine-side range-check at `Graphics.cpp:209-215` accepts `rasterMajor=11` | build-pass + smoke | `MSBuild ... /t:clientGraphics` (no FATAL on link), then launch with `client.cfg [ClientGraphics] rasterMajor=11` and observe `LoadLibrary` success in log | ❌ W0 |
| D3D11-01 | D3D9 plugin still builds clean (D-05 protection) | build-pass | `MSBuild ... /t:Direct3d9` after every Phase 11 commit; expect EXIT=0 | ✅ Existing |
| D3D11-02 | Zero matches for `D3DPOOL_MANAGED`, `OnLostDevice`, `OnResetDevice` in `Direct3d11/` | grep | `grep -rE "D3DPOOL_MANAGED\|OnLostDevice\|OnResetDevice" src/engine/client/application/Direct3d11/` returns 0 hits | ❌ W0 (Direct3d11/ doesn't exist) |
| D3D11-02 | Plugin survives Alt-Tab + window resize for ≥5 min | manual-only | manual smoke session | ❌ W0 (manual — no automation possible without scripted input + GPU passthrough) |
| D3D11-03 | All asset shaders compile with `vs_5_0` / `ps_5_0` | runtime smoke + log inspection | Launch under D3D11; `findstr "D3DCompile failed" stage/log-*.txt` returns no matches across a Tatooine + cantina session | ❌ W0 (compile pipeline doesn't exist) |
| D3D11-03 | Runtime device negotiates feature level ≥ 11.0 | startup log assertion | Plugin's install path emits `DEBUG_REPORT_LOG_PRINT(true, ("D3D11 device created at feature level %x", fl));`; verify in log | ❌ W0 |
| D3D11-04 | FFP spike result documented as a Phase 11 plan artifact | doc-exists check | `test -f .planning/phases/11-d3d11-renderer-plugin/11-XX-ffp-spike-finding.md` | ❌ W0 |
| D3D11-05 | Tatooine outdoor + cantina interior render under D3D11 ≥5 min each without crash | manual-only | manual smoke session | ❌ W0 (manual) |
| D3D11-05 | Visual-parity screenshots committed | doc-exists check | `test -d docs/recon/11-d3d11-screenshots/ && test -f docs/recon/11-d3d11-screenshots/comparison-notes.md` | ❌ W0 |
| SPEC R7 | DPVS verdict + decision documented | doc-exists check | `test -f docs/recon/11-dpvs-d3d11-remeasure.md` | ❌ W0 |
| SPEC R7 | If verdict diverges from Option α: source edit at `RenderWorld.cpp:902-908` | conditional grep | If verdict = "keep" or "scene-aware": `grep "OCCLUSION_CULLING" src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` returns ≥1 hit | (conditional) |

*Status legend: ✅ green · ❌ W0 (Wave 0 gap) · ⚠️ flaky · ⬜ pending*

---

## Wave 0 Requirements

- [ ] `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj` — produces `gl11_d.dll`, references all source files per RESEARCH §Recommended Project Structure
- [ ] `src/engine/client/application/Direct3d11/src/win32/Direct3d11.{h,cpp}` — plugin entry, `GetApi`, install/remove
- [ ] `src/engine/client/application/Direct3d11/src/win32/FirstDirect3d11.{h,cpp}` — precompiled header (matches `FirstDirect3d9` pattern)
- [ ] `src/build/win32/swg.sln` — add `Direct3d11.vcxproj` reference (one new `Project(...) = "Direct3d11", "..."` line + matching `EndProject` + `GlobalSection(ProjectConfigurationPlatforms)` entries for Debug|Win32 / Release|Win32 / Optimized|Win32)
- [ ] `stage/shader-cache/` directory + `.gitignore` entry — D-03 disk cache (lazy-create on first compile)
- [ ] `docs/recon/11-d3d11-screenshots/` directory — SPEC R6 reference screenshot home (lazy-create on first capture)
- [ ] `docs/recon/11-dpvs-d3d11-remeasure.md` — SPEC R7 verdict doc (created in DPVS remeasure plan, not Wave 0)

*(No test-framework gaps — this codebase has never had a unit-test framework; verification is build-pass + binary inspection + manual smoke per the precedent established in Phases 7, 9, and 10.)*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Alt-Tab + window-resize survival ≥5 min | D3D11-02 | No scripted input / GPU passthrough available | Boot under `rasterMajor=11`, reach char-select, Alt-Tab to desktop and back 5×, resize window 5×, observe no crash / no `DXGI_ERROR_DEVICE_REMOVED` |
| Tatooine outdoor render ≥5 min, no crash | D3D11-05 | Requires interactive client + zone load | Boot under `rasterMajor=11`, log in, zone into Tatooine, free-camera/walk for ≥5 min; capture screenshot when stable |
| Mos Eisley cantina interior render ≥5 min, no crash | D3D11-05 | Requires interactive client + zone traversal | After Tatooine outdoor smoke, travel to Mos Eisley cantina interior; free-camera/walk for ≥5 min; capture screenshot |
| Visual-parity screenshot pairs (D3D9 vs D3D11) for 4 reference scenes | D3D11-05 / SPEC R6 | Side-by-side visual judgment | Capture matching screenshots under both `rasterMajor=5` and `rasterMajor=11` for the 4 SPEC R6 scenes; commit pairs to `docs/recon/11-d3d11-screenshots/` + `comparison-notes.md` |
| DPVS remeasurement capture (PIX or Nsight) on D3D11 | SPEC R7 | External GPU profiler is not scriptable from MSBuild | Run PIX/Nsight capture session per `tools/dpvs-profile/test-protocol.md` against `rasterMajor=11`; extract `resolveVisibility()` per-frame cost; record verdict in `docs/recon/11-dpvs-d3d11-remeasure.md` |

---

## Validation Sign-Off

- [ ] All tasks have build-pass or binary-inspection verification (precedent: this codebase verifies via build-pass + inspection, not unit tests)
- [ ] Sampling continuity: no 3 consecutive tasks without automated build-pass verify
- [ ] Wave 0 vcxproj + sln edits complete before any plugin-source plan executes
- [ ] No watch-mode flags (MSBuild is single-shot)
- [ ] Feedback latency < 60 s for incremental plugin build
- [ ] `nyquist_compliant: true` set in frontmatter after sign-off

**Approval:** pending
