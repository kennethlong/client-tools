---
phase: 24
slug: dpvs-config-gate-machine-portability
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-12
---

# Phase 24 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None (C++ engine; validation = manual boot/play gates per established v2.x convention) |
| **Config file** | n/a |
| **Quick run command** | Build via `$env:MSBUILD ...SwgClient.vcxproj`; launch `stage/SwgClient_d.exe` |
| **Full suite command** | Dual-renderer boot gate (rasterMajor=5 then =11) + fresh-clone test (D-10) |
| **Estimated runtime** | ~5–10 min per boot gate; fresh-clone test ~30 min (clone + build + boot) |

---

## Sampling Rate

- **Per cfg-key removal (risky, D-05):** boot + brief play test, both renderers
- **Per engine-default flip (D-07):** boot + affected feature visible (skinned mesh / scene render), both renderers
- **After every plan wave:** dual-renderer boot gate
- **Before `/gsd:verify-work`:** dual-renderer boot gate + D-10 fresh-clone test green
- **Max feedback latency:** one Debug build + boot cycle (~10 min)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| *(filled by planner)* | | | HARD-01 | — | N/A | manual + DebugMonitor | `occlusionMode=auto`: walk Mos Eisley plaza → cantina; DPVS overlay shows bit set outdoors / clear indoors; F11 still wins; `on`/`off` unconditional | ✅ Phase-23 overlay + F11 | ⬜ pending |
| *(filled by planner)* | | | PORT-01 | — | N/A | fresh-clone test | Clone to new dir, run `setup-client.ps1`, build, boot → character select with audio | ❌ W0 (setup script) | ⬜ pending |
| *(filled by planner)* | | | PORT-01 | — | N/A | manual | Rename `stage/miles`, boot → exactly ONE startup warning, no flood | ❌ W0 (D-12 hook) | ⬜ pending |
| *(filled by planner)* | | | PORT-02 | — | N/A | dual boot gate | Generated `client_d.cfg`: rasterMajor=5 → char select; rasterMajor=11 → char select | ✅ established gate | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tools/setup/setup-client.ps1` — generates both cfgs from template, validates miles files (D-08/D-12a)
- [ ] `tools/setup/client*.cfg.template` — tracked source-of-truth (D-09)
- [ ] D-12b engine hook in `Audio::install` (clientAudio) — one-shot codec-absence warning
- [ ] Miles redist reconciliation: stage/miles bytes + 2 `.m3d` added to `src/external/3rd/library/miles-7.2e/redist/` (A1)

*(Boot/play gates reuse the established v2.x manual convention — no new harness.)*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Occlusion bit flips at POB boundary in `auto` | HARD-01 | Runtime renderer behavior; no unit harness | `_DEBUG` build, `occlusionMode=auto`, walk plaza ↔ cantina; verify via DPVS overlay + visible-object count (359 outdoor / 443 indoor per Phase-23 verdict) |
| Fresh clone bootability | PORT-01 | Whole-machine integration | Clone repo to NEW dir, run setup script (TRE root prompt), build, boot to character select; audio plays |
| Miles absence → one warning | PORT-01 | Failure-injection of staged files | Rename `stage/miles`, boot, confirm single startup warning + graceful degradation |
| Clean cfg dual-renderer boot | PORT-02 | Renderer/runtime behavior | Boot generated cfg under rasterMajor=5 and =11 to character select |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without a verify gate
- [ ] Wave 0 covers all MISSING references (setup script, template, D-12 hook, redist reconciliation)
- [ ] No watch-mode flags
- [ ] Feedback latency < one build+boot cycle
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
