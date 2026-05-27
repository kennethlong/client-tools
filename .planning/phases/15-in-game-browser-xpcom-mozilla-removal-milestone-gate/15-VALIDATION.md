---
phase: 15
slug: in-game-browser-xpcom-mozilla-removal-milestone-gate
status: validated
nyquist_compliant: true
wave_0_complete: true
created: 2026-05-26
validated: 2026-05-26
---

# Phase 15 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> **Removal phase** — validation is framed around removal-CORRECTNESS (build clean, grep zero,
> enum-ordinal safe, callers de-wired, boots both renderers), NOT feature behavior. There is no
> unit-test harness in the C++ engine tree; the gates are grep-zero, the Debug+Release link-grep
> gate, and the human-run dual-renderer boot gate (the established Phase 12/13/14 pattern).

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None (C++ engine removal; no unit-test harness in tree) |
| **Config file** | none — validation is grep + build-log + manual boot |
| **Quick run command** | `rg -i "SwgCuiWebBrowser\|libMozilla\|TUIWebBrowser\|\bxpcom\b\|\bxul\b\|nspr4\|plc4\|profdirserviceprovider\|nsIWebBrowser" src --glob '!*.filters'` (expect 0 after vendored-tree delete) |
| **Full suite command** | MSBuild Debug + Release (full VS path, `/nodeReuse:false`, from PowerShell; delete target exe first), then grep each link log for `unresolved external symbol` (== 0) |
| **Estimated runtime** | grep ~seconds; full Debug+Release relink ~minutes; manual dual-renderer boot ~5 min |

---

## Sampling Rate

- **After every task commit:** quick grep-zero for the symbols touched by that task + per-library MSBuild (link-grep).
- **After every plan wave:** full XPCOM grep-zero + Debug+Release link-grep (`unresolved external symbol` == 0).
- **Before `/gsd:verify-work` (milestone close):** full milestone residue grep-zero (criterion #4, with KEEP-list) + Debug+Release link clean (criterion #3) + dual-renderer boot (criterion #5, human-confirmed).
- **Max feedback latency:** grep seconds; link gate minutes.

---

## Per-Task Verification Map

> Filled per task by the planner/executor. Each removal task maps to a grep-zero and/or link-grep
> gate; the boot gate is the milestone-close backstop. Threat refs map to the RESEARCH Security Domain
> (T-15-01 vendored browser engine on disk; T-15-02 dead TCG URL-nav path; T-15-03 `/FORCE`-masked broken binary).

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 15-01-T1 (de-wire callers + sever TCG + delete TUIWebBrowser enum + Game.cpp include) | 15-01 | 1 | DECRUFT-06 | T-15-02 / T-15-04 / T-15-05 | dead TCG URL-nav severed; enum-ordinal safe; siblings untouched | grep-zero (gated via 15-01-T3) | `rg -i "TUIWebBrowser\|WebBrowserManager" src` → 0; sibling Browser UIs intact | ✅ (gates pre-exist) | ✅ green |
| 15-01-T2 (delete 3 SwgCuiWebBrowser* units) | 15-01 | 1 | DECRUFT-06 | T-15-01 | browser consumer source removed | grep-zero (gated via 15-01-T3) | `rg -i "SwgCuiWebBrowser\|UIWebBrowserWidget" src` → 0 | ✅ | ✅ green |
| 15-01-T3 (inline include purge + SwgClient link tokens + Debug/Release build gate) | 15-01 | 1 | DECRUFT-06 | T-15-01 / T-15-03 | attack surface reduced; no masked unresolved symbol | grep-zero + link-grep | `rg -i "<XPCOM set>" src` → 0; Debug+Release link log `unresolved external symbol` == 0 | ✅ | ✅ green |
| 15-02-T1 (.rsp purge: SwgClient / swgClientUserInterface / clientGame) | 15-02 | 1 | DECRUFT-06 | T-15-06 / T-15-07 | config grep-zero; no live-dep over-deletion | grep-zero | `rg -i "Mozilla-family" *.rsp` → 0; `989crypt.lib` survives | ✅ | ✅ green |
| 15-02-T2 (7 editor .rsp + inline .vcxproj purge) | 15-02 | 1 | DECRUFT-06 | T-15-07 | config grep-zero; no backslash mis-parse | grep-zero | per-editor `rg -i Mozilla` → 0 (no mangled tokens) | ✅ | ✅ green |
| 15-02-T3 (SwgGodClient .rsp + .vcxproj purge) | 15-02 | 1 | DECRUFT-06 | T-15-06 | only editor linking libMozilla.lib inline; 989crypt KEEP | grep-zero | `rg -i Mozilla SwgGodClient.vcxproj` → 0; `989crypt.lib` present | ✅ | ✅ green |
| 15-03-T1 (Wave-1 merge gate → drop sln GUID → delete vendored tree → grep-zero + link-grep) | 15-03 | 2 | DECRUFT-06 | T-15-01 / T-15-08 / T-15-09 | vendored engine gone; no dangling GUID; no premature-delete dangle | grep-zero + tree-delete + link-grep | `rg "C6C1E14A…\|libMozilla" src/build/win32/swg.sln` → 0; `test -d …/libMozilla` → absent; Debug+Release link == 0 | ✅ | ✅ green |
| 15-04-T1 (A1 lcdui cleanup + full P12–P15 milestone residue sweep + link-grep) | 15-04 | 3 | DECRUFT-06 | T-15-10 / T-15-11 | milestone residue == 0 w/ KEEP-list; no over-deletion | grep-zero + link-grep | milestone per-token `rg` (KEEP-list) → 0; `SwgCuiG15Lcd.cpp` survives; Debug+Release link == 0 | ✅ | ✅ green |
| 15-04-T2 (dual-renderer milestone boot gate — closes v2.1) | 15-04 | 3 | DECRUFT-07 | T-15-04 | boots both renderers, no browser surface; HUD/radial/IME focus intact | manual boot (human-verify, blocking) | set `rasterMajor` in `stage/client_d.cfg`; launch `SwgClient_d.exe`; reach char-select under 5 & 11 | ✅ | ✅ human-confirmed (commit 16fd3ac4c) |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

> **Gates re-run this audit (2026-05-26):** consolidated XPCOM/Mozilla grep over `src` → 0 (rg exit 1); `swg.sln` GUID `{C6C1E14A…}` + literal `libMozilla` → 0 (rg exit 1, correct path `src/build/win32/swg.sln`); SwgClient project still in sln (well-formed); vendored `libMozilla/` tree → DELETED. Debug+Release link-grep (`unresolved external symbol` == 0) is recorded green across 15-01/15-03/15-04 SUMMARYs (build logs ephemeral/gitignored — not re-run this audit). DECRUFT-07 boot gate human-confirmed PASS this milestone, not re-run (needs live SWGSource VM).

### Success Criteria → observable validation signals (from RESEARCH Validation Architecture)

| Crit | Behavior | Gate kind | Command / Gate |
|------|----------|-----------|----------------|
| #1a | Zero XPCOM/Mozilla refs across source + include paths + `.rsp` + `.sln` | grep | `rg -i "libMozilla\|\bxpcom\b\|\bxul\b\|[Mm]ozilla\|nsIWebBrowser" src` → 0 (after vendored-tree delete) |
| #1b | `libMozilla.vcxproj` dropped from `swg.sln`; GUID `{C6C1E14A…}` + 9 ProjectDependency back-refs absent | grep | `rg "C6C1E14A-DEDB-48C6-B028-0C9E4EFF5DC4\|libMozilla" src/build/win32/swg.sln` → 0 |
| #2a | `SwgCuiWebBrowser*` + `TUIWebBrowser` + `WebBrowserManager` symbol set deleted | grep | `rg "SwgCuiWebBrowser\|TUIWebBrowser\|WebBrowserManager" src` → 0 |
| #2b | No Mozilla DLL on any POST_BUILD/stage list | grep | confirmed none present (D-10); `rg -i mozilla` over `*.bat/*.cmd/*.ps1` + PostBuildEvent → 0 |
| #2c | `UITypeID` ordinal safe — no enum-shift regression | analysis + boot | D-02 verdict (ordinal never serialized; string-keyed loader) + radial-menu/HUD/IME focus exercised under boot gate |
| #3 | swg.sln builds clean Debug AND Release with browser gone | link-grep | MSBuild Debug + Release; each link log `unresolved external symbol` == 0 (NOT exit 0 — `/FORCE` masks). Delete target exe first. **Optimized EXEMPT** (DEF-14-01). |
| #4 | Milestone-wide residue == 0 (P12–P15 subsystems) with KEEP-list applied | grep | per-token `rg` with KEEP-list exclusions (see RESEARCH Milestone Residue Sweep) → 0 |
| #5 | Boots to char-select under rasterMajor=5 AND =11 vs SWGSource VM after FULL removal | manual boot | set `rasterMajor` in `stage/client_d.cfg`; launch `SwgClient_d.exe`; reach char-select both renderers; no crash/assert; no browser surface |

---

## Wave 0 Requirements

*Existing infrastructure covers all phase validation — no test framework to create.* The grep-zero,
link-grep, and boot gates are the established Phase 12/13/14 validation pattern. A one-line `rg` per
criterion (with the KEEP-list exclusions from the RESEARCH Milestone Residue Sweep) suffices — no
fixtures needed. `wave_0_complete: true`.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Boots to character select under D3D9 (rasterMajor=5) | DECRUFT-07 | No headless harness; requires live SWGSource VM + GPU + visual confirmation | Set `rasterMajor=5` in `stage/client_d.cfg`; launch `SwgClient_d.exe`; confirm char-select reached, no crash/assert, no browser surface, HUD + radial menu + IME focus work |
| Boots to character select under D3D11 (rasterMajor=11) | DECRUFT-07 | Same | Set `rasterMajor=11` in `stage/client_d.cfg`; relaunch; confirm char-select reached, same backstops |
| `UITypeID` ordinal-shift backstop (HUD/radial/IME focus intact) | DECRUFT-06 | Ordinal-shift regression cannot be caught by link or grep — only by running the focus-routing paths | Under both renderers, exercise radial menu open, HUD focus, IME focus toggle; confirm no misrouted focus / wrong-widget RTTI |

> The link gate and grep gate **cannot** catch an enum-ordinal-shift regression (D-02a rationale).
> D-02's verdict is SAFE-TO-DELETE-OUTRIGHT (ordinal never serialized), but the boot gate's focus-path
> exercise remains the mandatory backstop.

---

## Validation Sign-Off

- [x] All tasks have automated verify (grep-zero / link-grep) or are listed as manual-only (boot gate)
- [x] Sampling continuity: every removal task has a grep-zero or link-grep gate
- [x] Wave 0 covers all MISSING references (none — gates pre-exist)
- [x] No watch-mode flags
- [x] Feedback latency: grep seconds, link minutes
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** validated 2026-05-26

---

## Validation Audit 2026-05-26

State A audit (existing VALIDATION.md). Placeholder Per-Task rows replaced with the real 9-task map (15-01-T1..T3, 15-02-T1..T3, 15-03-T1, 15-04-T1..T2). All automated gates re-run green; the lone manual-only item (DECRUFT-07 dual-renderer boot) is human-confirmed PASS.

| Metric | Count |
|--------|-------|
| Gaps found | 0 |
| Resolved | 0 |
| Escalated | 0 |
| Automated (grep-zero / link-grep) | 8 tasks → DECRUFT-06 |
| Manual-only (boot gate, human-confirmed) | 1 task → DECRUFT-07 |

No fillable automated gaps: the C++ engine tree has no unit-test harness, and removal-correctness is validated by grep-zero + Debug/Release link-grep + the human dual-renderer boot gate (the established P12–P14 pattern). No `gsd-nyquist-auditor` spawned — nothing to generate. `nyquist_compliant: true` upheld; DECRUFT-07 is manual-only **by nature** (no headless harness; needs live VM + GPU), already executed and approved this milestone (commit 16fd3ac4c).
