---
phase: 37
slug: utinni-engine-entry-point-advertisement-getenginehookpoints
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-21
---

# Phase 37 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Derived from RESEARCH.md `## Validation Architecture` (§7 acceptance split).

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | MSBuild link gate + `dumpbin /exports` + build-time `static_assert` + runtime self-check |
| **Config file** | none — verification is build/link-driven (no unit-test harness in this repo) |
| **Quick run command** | `dumpbin /exports stage/SwgClient_r.exe \| grep GetEngineHookPoints` |
| **Full suite command** | Canonical 5-target Win32 build + link-log grep for `unresolved external symbol` (must be 0) + dumpbin undecorated-export check |
| **Estimated runtime** | ~build time (single SwgClient relink ~minutes) |

---

## Sampling Rate

- **After every task commit:** Build the touched config; grep link log for `unresolved external symbol` (0 expected, /FORCE masks them).
- **After every plan wave:** Relink `SwgClient_r.exe` (delete exe first to force relink) + `dumpbin /exports` undecorated check.
- **Before `/gsd-verify-work`:** Build links clean, export present+undecorated, coverage self-check green, client still boots to char-select (boot gate).
- **Max feedback latency:** one build cycle.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 37-01-* | 01 | 1 | EPA-02 | — | read-only getter, inert when un-injected | link+dumpbin | `dumpbin /exports stage/SwgClient_r.exe` shows undecorated `GetEngineHookPoints` | ❌ W0 | ⬜ pending |
| 37-02-* | 02 | 2 | EPA-03/EPA-04 | — | zero-missing on MVP required set | static_assert+selfcheck | coverage self-check exits 0 for MVP set | ❌ W0 | ⬜ pending |
| 37-03-* | 03 | 3 | EPA-04 | — | graceful degrade on unresolved rows | static_assert+selfcheck | coverage self-check exits 0 for full required set | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Coverage self-check mechanism (build-time `static_assert` on row count/no-duplicate + optional runtime null-scan) — established in 37-01 spike, extended per-tier in 37-02/37-03.

*No unit-test framework exists in this repo; verification is link/dumpbin/static_assert driven by construction.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| First-detour `0xC0000005 target=0x00401000` crash gone | EPA-02 | Requires live Utinni injection (out of this repo) | Inject UtinniCore into `SwgClient_r.exe`; confirm `config::loadOverrideConfig` resolves via table, no first-detour AV |
| DX11 overlay installs+renders off the contract | EPA-03 | Requires Utinni overlay + live client | Inject with graphics `GetHookPoints` + `GetEngineHookPoints`; confirm overlay renders, kickoff not gated on hardcoded `graphics::install` RVA |
| No SWGEmu regression | EPA-02 | Requires SWGEmu Pre-CU client + Utinni | Run existing SWGEmu D3D9 live-smoke; hardcoded RVA path unchanged, still passes |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify (link/dumpbin/static_assert) or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers the coverage self-check mechanism
- [ ] Boot gate honored (client boots to char-select after relink)
- [ ] Manual-only Utinni-injection items explicitly flagged (out-of-repo)
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
