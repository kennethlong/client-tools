---
phase: 12
slug: orphaned-directory-project-deletes
status: validated
nyquist_compliant: true
wave_0_complete: true
created: 2026-05-25
validated: 2026-05-27
---

# Phase 12 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
>
> **This is a delete-only phase.** There is no unit-testable behavior — the validation
> surface is **compile + link + boot**. "Green" means: `swg.sln` builds clean (MSBuild
> exit 0, no missing-project / unresolved-reference / dangling-ProjectReference errors)
> AND the client boots to character select under **both** `rasterMajor=5` (D3D9) and
> `rasterMajor=11` (D3D11). All "tests" below are build assertions, grep assertions, or
> manual boot gates — there is no pytest/jest equivalent for removing dead C++ modules.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None (delete-only C++ phase) — validation is MSBuild + manual boot |
| **Config file** | `src/build/win32/swg.sln` (build); `stage/client_d.cfg` (renderer select via `rasterMajor`) |
| **Quick run command** | `msbuild src/build/win32/swg.sln /t:SwgClient /p:Configuration=Debug /p:Platform=Win32 /m` |
| **Full suite command** | `msbuild src/build/win32/swg.sln /p:Configuration=Debug /p:Platform=Win32 /m` (full-solution build — catches the 6 out-of-closure editors that also reference lcdui) |
| **Estimated runtime** | SwgClient incremental ~1–4 min; full-solution build ~10–20 min |

---

## Sampling Rate

- **After every task commit:** `/t:SwgClient` incremental build must exit 0.
- **After every module removal (per success criterion):** Build clean + **manual boot gate** to character select.
- **Before `/gsd:verify-work`:** Full-solution `swg.sln` build green AND boot gate passes under both `rasterMajor=5` and `=11`.
- **Max feedback latency:** build ~minutes; boot gate is manual (operator launches `SwgClient_d.exe`).

---

## Per-Task Verification Map

| Task ID | Module | Requirement | Secure Behavior | Verify Type | Automated Command / Assertion | Status |
|---------|--------|-------------|-----------------|-------------|-------------------------------|--------|
| stationapi | stationapi/ (pure orphan) | DECRUFT-01 | N/A | grep + build | `grep -ri stationapi src/build src/**/*.rsp src/**/*.vcxproj` → 0 live refs; `/t:SwgClient` exit 0 | ✅ green (truths 1,3 VERIFIED) |
| trackIR | trackIR/ + `ClientHeadTracking.cpp` exclude | DECRUFT-01 | N/A | grep + build | `grep -ri "NPClient\|trackIR" src` → 0 live `#include`/refs; `clientGame` + `/t:SwgClient` exit 0 | ✅ green (truths 2,4,5 VERIFIED) |
| SwgClientSetup | SwgClientSetup.vcxproj + dir | DECRUFT-02 | N/A | grep + build | `SwgClientSetup` absent from `swg.sln`; `/t:SwgClient` exit 0 | ✅ green (truths 6,7 VERIFIED) |
| lcdui | lcdui.vcxproj + EZ_LCD source edit | DECRUFT-03 | N/A | grep + build | lcdui GUID absent from all 7 sln ProjectDependency refs; `EZ_LCD.h`/`lgLcd.lib`/lcdui include-paths purged; full `swg.sln` build exit 0 | ✅ green (truths 8–15 VERIFIED; editor ProjectReference resolved in 1d6b80242) |
| boot-gate | (cross-cutting) | DECRUFT-01/02/03 | client reaches char-select | manual boot | `SwgClient_d.exe` boots to character select under `rasterMajor=5` AND `=11` (see Manual-Only) | ✅ PASS — manual, both renderers (operator-confirmed, all 3 plans) |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

No test infrastructure to scaffold — this phase removes code, it adds none. The "Wave 0"
equivalent is establishing the **pre-removal boot baseline**: confirm the client currently
boots to character select under both renderers BEFORE the first delete, so any post-delete
regression is unambiguously attributable.

- [x] Pre-removal boot baseline captured (client boots to char-select, `rasterMajor=5` + `=11`)

*Existing build infrastructure (MSBuild) covers all phase verification.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Client boots to character select (D3D9) | DECRUFT-01/02/03 | Requires live SWGSource VM + GPU + asset tree; not unit-testable | Set `stage/client_d.cfg [ClientGraphics] rasterMajor=5`; launch `SwgClient_d.exe`; confirm character-select screen renders against the SWGSource VM |
| Client boots to character select (D3D11) | DECRUFT-01/02/03 | Same — plus exercises the gl11 plugin path | Set `rasterMajor=11`; launch `SwgClient_d.exe`; confirm character-select screen renders |
| No runtime regression from lcdui/trackIR removal | DECRUFT-01/03 | Input/HUD paths only observable at runtime | After lcdui + trackIR removal, confirm HUD renders and input works at char-select (G15/head-tracking absence is expected and inert) |

*Boot verification is the central acceptance pattern for this milestone (Core Value: every change leaves the client bootable to character select). Verified incrementally after each module removal.*

---

## Validation Sign-Off

- [x] Every module-removal task gated by a clean MSBuild build (exit 0)
- [x] Sampling continuity: boot gate run after each module removal, not just at phase end
- [x] Full-solution build verified (not just `/t:SwgClient`) to catch lcdui's 6 out-of-closure editor refs
- [x] Boot gate passes under BOTH `rasterMajor=5` and `=11`
- [x] No live security-relevant or feature-critical code removed (delete surface is dead modules only)
- [x] `nyquist_compliant: true` set in frontmatter once the above hold

**Approval:** ✅ validated 2026-05-27 (retroactive audit-only flip; phase verified `passed` 2026-05-25, see 12-VERIFICATION.md 15/15).

---

## Validation Audit 2026-05-27

Retroactive Nyquist audit (State A). No code/test changes — the phase was already executed and verified `passed` (15/15 must-haves) before this audit. This pass confirms each validation-strategy gate was actually exercised green during execution and flips the doc state to match reality.

| Metric | Count |
|--------|-------|
| Requirements / task rows | 5 |
| COVERED (automated grep + build gates, verified green) | 4 |
| Manual-only (GPU dual-renderer boot — inherently un-automatable, operator-confirmed PASS) | 1 |
| MISSING (automatable, unfilled) | 0 |
| Tests generated | 0 (no unit-test harness in this C++ tree; nothing valid to generate) |
| Escalated | 0 |

**Result:** NYQUIST-COMPLIANT (PARTIAL by nature) — every automatable requirement has a repeatable grep/build verification confirmed green in 12-VERIFICATION.md; the sole manual item is the documented dual-renderer boot gate. Consistent with Phase 14/15 precedent.
