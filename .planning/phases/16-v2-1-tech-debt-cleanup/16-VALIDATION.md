---
phase: 16
slug: v2-1-tech-debt-cleanup
status: validated
nyquist_compliant: true
wave_0_complete: true
created: 2026-05-26
validated: 2026-05-27
---

# Phase 16 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> **No-behavior-change cleanup.** Validation is framed around **removal-correctness +
> absence-of-residue + absence-of-regression** — the established Phase 12–15 discipline.
> There is **no C++ unit-test harness** in this tree (confirmed: no `test/`/`tests/`
> dirs under `src`). The validation primitives are `rg` grep-zero, MSBuild link-log
> grep, and one manual boot.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None (C++ engine cleanup; no unit-test harness in tree) |
| **Config file** | none — validation is `rg` grep-zero + MSBuild link-log grep + one manual boot |
| **Quick run command** | per-token `rg` grep-zero for the symbol the task removed (see Per-Task map) — seconds |
| **Full suite command** | MSBuild **SwgClient Debug + Release** (full VS path, `/nodeReuse:false`, from PowerShell; delete `SwgClient_d.exe`/`SwgClient_r.exe` first), then grep each link log for `unresolved external symbol` (== 0). **Optimized EXEMPT** (DEF-14-01 SAFESEH LNK1281 pre-existing). |
| **Estimated runtime** | grep ~seconds; Debug+Release relink ~minutes; boot smoke ~1 min |

---

## Sampling Rate

- **After every task commit:** per-token `rg` == 0 for the symbol/token that task removed (seconds).
- **After every plan wave:** SwgClient Debug+Release link-grep `unresolved external symbol` == 0 (minutes).
- **Before phase sign-off:** all grep-zero gates green + Debug+Release link clean + ONE `rasterMajor=11` (D3D11) boot to character-select, human-confirmed (D-08). **NOT** the full dual-renderer matrix — this is a no-behavior-change phase, so one boot confirms no link/init regression.
- **Max feedback latency:** grep gates seconds; link gate minutes.

---

## Per-Task Verification Map

> Task IDs are assigned by the planner/execution; rows below are keyed to the
> research Targets (T1/T2/T3a/T3b) so each task inherits its grep-zero gate.

| Target | Plan/Task | Wave | Decision | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|--------|-----------|------|----------|------------|-----------------|-----------|-------------------|-------------|--------|
| T1 — 989crypt dead-token sweep (SwgGodClient.vcxproj, 3 configs) | TBD | — | D-01/D-02/D-03 | — | No new attack surface; removes dead dep token | grep-zero | `rg -i "989crypt" src` → 0; KEEP-list still present: `rg "Base.lib\|ChatAPI.lib\|TcpLibrary.lib" SwgGodClient.vcxproj` ≠ 0 | ✅ (rg) | ✅ green (VERIFIED) |
| T2 — lcdui editor LIBPATH (verify-only no-op) | TBD | — | D-04 | — | N/A | grep-zero | per-editor `rg -i lcdui <vcxproj>` → 0 (already 0; confirm, no edit) | ✅ (rg) | ✅ green (VERIFIED) |
| T3a — dead `finalUrl` block + now-dead ConfigClientGame.h/shellapi.h includes (SwgCuiHudAction.cpp ~:1170-1189, :24, :11) | TBD | — | D-06 | — | No behavior change (consumer `//ShellExecute` already commented out) | grep-zero | `rg "finalUrl\|ConfigClientGame\|shellapi\|ShellExecute\|SW_SHOW" src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp` → 0 | ✅ (rg) | ✅ green (VERIFIED) |
| T3b — voice-volume API (CuiPreferences .cpp/.h: 2 statics + 2 REGISTER_OPTION lines + 4 accessors + 4 decls) | TBD | — | D-06/D-07 | — | Zero callers; the 2 `REGISTER_OPTION(speakerVolume/micVolume)` persistence lines (:841-842) ARE removed in the same edit — **CORRECTED** from the earlier "no save/load hook" claim (they register the statics into LocalMachineOptionManager; benign persistence-surface reduction, Vivox UI gone) | grep-zero | `rg "speakerVolume\|micVolume\|SpeakerVolume\|MicVolume" src` → 0 (repo-wide, incl. CuiPreferences — catches statics + accessors + decls + the 2 REGISTER_OPTION keys) | ✅ (rg) | ✅ green (VERIFIED) |
| T3a+T3b — SwgClient links clean | TBD | — | D-08 | — | No unresolved externals introduced | link-grep | MSBuild Debug+Release; each link log `unresolved external symbol` == 0 (Optimized EXEMPT) | ✅ (msbuild) | ✅ green (VERIFIED) |
| All — no init regression | TBD | — | D-08 | — | Client still boots | manual boot | `stage/client_d.cfg` `rasterMajor=11` (already set, :37); launch `SwgClient_d.exe`; reach character-select | ✅ (manual) | ✅ PASS — manual, D3D11 (operator-confirmed; char-select + ~11 min in-world, 16-03-SUMMARY) |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

*None — no test infrastructure to scaffold.* The grep-zero + link-grep + boot gates are the
project's established validation primitives (Phase 12–15 precedent in 15-VALIDATION.md).
Framework install: N/A.

---

## Manual-Only Verifications

| Behavior | Decision | Why Manual | Test Instructions |
|----------|----------|------------|-------------------|
| Client boots to character-select under D3D11 after cleanup | D-08 | No automated runtime harness; renderer + game init are interactive | Confirm `stage/client_d.cfg` `rasterMajor=11`; delete + relink `SwgClient_d.exe`; launch it; verify it reaches the character-select screen without crash/init regression. ONE boot only (not the full `=5`/`=11` matrix). |

---

## Validation Sign-Off

- [x] T1/T2/T3a/T3b grep-zero gates all green (and T1 KEEP-list still present)
- [x] SwgClient Debug + Release link-grep `unresolved external symbol` == 0 (Optimized EXEMPT)
- [x] One `rasterMajor=11` boot to character-select human-confirmed
- [x] Sampling continuity: every removal task has a grep-zero gate
- [x] No watch-mode flags
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** ✅ validated 2026-05-27 (retroactive audit-only flip; phase verified `passed` 2026-05-27, see 16-VERIFICATION.md 8/8).

---

## Validation Audit 2026-05-27

Retroactive Nyquist audit (State A). No code/test changes — the phase was executed and verified `passed` (8/8 must-haves, D-01 through D-08) before this audit. This pass confirms the grep-zero gates (incl. the T1 KEEP-list survival check) and the Debug+Release link-grep gate were exercised green during execution, and flips the doc state to match reality.

| Metric | Count |
|--------|-------|
| Target rows | 6 |
| COVERED (automated `rg` grep-zero + Debug/Release link-grep gates, verified green) | 5 |
| Manual-only (single D3D11 boot — inherently un-automatable, operator-confirmed PASS) | 1 |
| MISSING (automatable, unfilled) | 0 |
| Tests generated | 0 (no unit-test harness in this C++ tree) |
| Escalated | 0 |

**Result:** NYQUIST-COMPLIANT (PARTIAL by nature) — all grep-zero gates green (and the T1 KEEP-list still present), Debug+Release link-grep `unresolved external symbol` == 0 (Optimized EXEMPT per DEF-14-01), per 16-VERIFICATION.md; the sole manual item is the documented single D3D11 boot to character-select. The pre-existing Options-window crash (SwgCuiOptUi.cpp:219) is confirmed unrelated to this phase's diff.
