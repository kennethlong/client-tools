---
phase: 37
slug: utinni-engine-entry-point-advertisement-getenginehookpoints
status: approved
nyquist_compliant: true
wave_0_complete: true
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
| 37-01-03 | 01 | 1 | EPA-02 | T-37-02 | read-only getter, inert when un-injected | link+dumpbin | `grep -c "unresolved external symbol" build-37-01.log` == 0 ; `dumpbin /exports stage/SwgClient_d.exe \| grep GetEngineHookPoints` shows undecorated name ; `grep -c loadFromBuffer utinni_advertise.cpp` == 0 (EPA-02 thunk correction) | ✅ by-construction | ⬜ pending |
| 37-02-03 | 02 | 2 | EPA-03, EPA-04 | T-37-05 | name-set-equality on MVP required set | static_assert(smoke)+name-set-selfcheck | 5-target build succeeds (compile-time count `static_assert` row==`.inc` count, NO sentinel — a drift SMOKE) ; `grep -c "unresolved external symbol"` == 0 ; `utinni_verifyNoNullNoDup()` true = no-null + no-dup + table-names==`s_requiredNames[]` (the actual zero-missing gate) | ✅ by-construction | ⬜ pending |
| 37-03-03 | 03 | 3 | EPA-04 | T-37-07 | name-set-equality on trimmed required set | static_assert(smoke)+name-set-selfcheck | full-set count `static_assert` compiles in all 3 flavors (drift smoke) ; `grep -c "unresolved external symbol"` == 0 (×3) ; 3-flavor `dumpbin` undecorated ; `utinni_verifyNoNullNoDup()` true = no-null + no-dup + table-names==`s_requiredNames[]` on the trimmed (skip-virtuals-excluded) required set | ✅ by-construction | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*
*Automated commands are link-log/dumpbin/static_assert driven — no unit-test framework exists in this repo; verification is correct-by-construction at build/link time (the `<acceptance_criteria>` of each plan's Task 3 carry these exact commands).*

---

## Wave 0 Requirements

- [x] No separate test-infrastructure wave is required. Verification for this phase is **correct-by-construction** at build/link time: MSBuild link gate (`/FORCE` → grep `unresolved external symbol` == 0), `dumpbin /exports` undecorated-name check, and the compile-time count coverage `static_assert` (a drift smoke) plus the runtime name-set-equality self-check. These tools all exist; nothing to install.
- [x] The coverage self-check mechanism (build-time count `static_assert` row-count==`.inc`-count as a drift SMOKE + runtime `utinni_verifyNoNullNoDup()` = no-null + no-dup + name-set-equality vs the X-macro-generated `s_requiredNames[]`, the actual zero-missing gate) is **established as the first task of execution** — 37-01 Task 2 authors it, 37-02/37-03 extend it per tier. It is the Wave 0 harness for this phase and is part of the spike that gates everything downstream.

*`wave_0_complete: true` reflects that no test-framework install is needed and the validation contract (every Task 3 carries runnable link/dumpbin/static_assert commands) is fully specified.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| First-detour `0xC0000005 target=0x00401000` crash gone | EPA-02 | Requires live Utinni injection (out of this repo) | Inject UtinniCore into `SwgClient_r.exe`; confirm `config::loadOverrideConfig` resolves via table, no first-detour AV |
| DX11 overlay installs+renders off the contract | EPA-03 | Requires Utinni overlay + live client | Inject with graphics `GetHookPoints` + `GetEngineHookPoints`; confirm overlay renders, kickoff not gated on hardcoded `graphics::install` RVA |
| No SWGEmu regression | EPA-02 | Requires SWGEmu Pre-CU client + Utinni | Run existing SWGEmu D3D9 live-smoke; hardcoded RVA path unchanged, still passes |

---

## Validation Sign-Off

- [x] All tasks have automated verify (link/dumpbin/static_assert in each plan's Task 3 `<acceptance_criteria>`) or Wave 0 dependencies
- [x] Sampling continuity: each plan's build/link/dumpbin gate runs at its Task 3; no 3 consecutive tasks without automated verify (the spike + per-tier coverage gates sample every wave)
- [x] Wave 0 covers the coverage self-check mechanism (seeded 37-01 Task 2, extended 37-02/37-03)
- [x] Boot gate honored (each Task 3 boots to char-select after force-relink; 37-03 dual-renderer rasterMajor=5 & =11)
- [x] Manual-only Utinni-injection items explicitly flagged (out-of-repo — §7 #2 first-detour-crash-gone, #4 SWGEmu no-regression, #5 DX11 overlay-off-contract)
- [x] `nyquist_compliant: true` set in frontmatter

> **EPA-04 "done" scope (down-scoped per cross-AI review 2026-06-21):** EPA-04 green = count-parity (drift smoke) + no-null + no-dup + **name-set-equality** vs the X-macro `s_requiredNames[]` proven on the **trimmed** required set (skip-virtuals intentionally excluded — Utinni resolves those off the live vtable). It does NOT claim **correct-`&`** for every row — that is by-construction/human-reviewed in-repo and **live-verified Utinni-side** (out of repo). The trimmed-required-set vs Utinni's full ~230-name hook list is an explicit out-of-repo coordination item. The `.inc`+`.h` are re-copied into `D:/Code/Utinni` per catalog wave (the lockstep sync); `UTINNI_HOOKPOINTS_VERSION` stays 1 (a per-wave bump is cosmetic under shared-header lockstep).

**Approval:** approved 2026-06-21 (plan-phase checker iteration 2); revised 2026-06-21 (plan-phase --reviews: name-set-equality gate + EPA-04 down-scope + symbol/terminator/Win32 fixes)
