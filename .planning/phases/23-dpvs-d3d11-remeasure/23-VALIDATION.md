---
phase: 23
slug: dpvs-d3d11-remeasure
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-12
---

# Phase 23 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Measurement phase: "tests" are the capture protocol + the aggregator's deterministic verdict logic, not unit tests of game code.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None for game code (measurement phase). Aggregator `tools/dpvs-profile/analysis.py` is Python 3.12 stdlib. |
| **Config file** | none |
| **Quick run command** | `python tools/dpvs-profile/analysis.py --csv-dir <dir>` (emits table + `verdict = ...`) |
| **Full suite command** | Capture protocol in `tools/dpvs-profile/test-protocol.md` (manual, live-server) |
| **Estimated runtime** | quick: seconds; full capture session: live-server, manual |

---

## Sampling Rate

- **Per restored-source commit:** client builds + boots to character select (boot gate, AGENTS.md); F11 visibly toggles DPVS state in the overlay
- **Per capture session:** ≥3 passes/condition/scene, alternating ON-OFF; `analysis.py` runs clean (no header-mismatch warnings)
- **Before `/gsd:verify-work`:** verdict line emitted + recorded in the recon doc; client still boots with the shipped Option α `#else` branch untouched
- **Max feedback latency:** one build+boot cycle per restored-source commit

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| TBD (planner fills) | — | — | DPVS-01 | T-23-01 (CSV injection via `/setrunlabel`) | run-label sanitizer restored with writer | manual (live capture) | `test-protocol.md` session, then `analysis.py` | ✅ protocol + aggregator present | ⬜ pending |
| TBD (planner fills) | — | — | DPVS-01 | T-23-02 (debug surface in release) | F10/F11 + `/setrunlabel` + occlusion re-gate `_DEBUG`-only | unit-ish (deterministic) | `python tools/dpvs-profile/analysis.py --csv-dir stage/dpvs-profile/` → final verdict line | ✅ analysis.py present | ⬜ pending |
| TBD (planner fills) | — | — | DPVS-01 | — | N/A | doc artifact | manual write of `docs/recon/23-dpvs-d3d11-profiling.md` | ❌ Wave N (new doc) | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Restore `DpvsProfileInstrumentation.{h,cpp}` (CPU path) from tag `phase-10-instrumentation-pre-cleanup` — covers DPVS-01 capture
- [ ] Re-add `clientGraphics.vcxproj` ClCompile/ClInclude entries — build wiring
- [ ] Re-introduce `OCCLUSION_CULLING` bit gated on `ms_forceDisableOcclusionCulling` + QPC pair — covers the A/B toggle
- [ ] Restore F10/F11 intercept (`CuiIoWin.cpp`) + `/setrunlabel` (`SwgCuiCommandParserDefault.cpp`) + `onFrameEnd` hook (`Game.cpp`)
- [ ] Framework install: none required; `analysis.py` already exists

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Occlusion-vs-no-occlusion frame-time A/B under D3D11 | DPVS-01 | Live-server capture; NPC density drives the DPVS delta; engine timers require a real play session | `tools/dpvs-profile/test-protocol.md` — ≥3 passes/condition/scene, alternating ON-OFF, then `analysis.py` |
| Keep/remove verdict recorded confirming/revising Option α | DPVS-01 | Human-recorded doc artifact | Write `docs/recon/23-dpvs-d3d11-profiling.md` with verdict + raw tables |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < one build+boot cycle
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
