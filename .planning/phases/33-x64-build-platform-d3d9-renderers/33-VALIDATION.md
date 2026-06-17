---
phase: 33
slug: x64-build-platform-d3d9-renderers
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-17
---

# Phase 33 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> **No automated unit/render-test framework exists** — per AGENTS.md, *builds/boots/renders are the truth*. Validation here = MSBuild build-gate + `dumpbin` + `/FORCE` link-grep + manual dual-bitness boot smoke. Source: 33-RESEARCH.md §Validation Architecture.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None (C++ MSBuild; no unit-test harness). Validation = MSBuild + `dumpbin` + manual dual-bitness boot smoke |
| **Config file** | `src/build/win32/swg.sln` (+ per-project `.vcxproj`) |
| **Quick run command** | x64 lib/plugin build: `& $env:MSBUILD src/build/win32/swg.sln /t:<proj> /p:Configuration=Debug /p:Platform=x64 /nodeReuse:false` |
| **Full suite command** | Canonical x64 build `/t:Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient /p:Platform=x64` **plus** the same targets `/p:Platform=Win32` (non-regression) |
| **Estimated runtime** | ~minutes per target (full tree relink); boot smoke ~1–2 min per renderer |

---

## Sampling Rate

- **After every task commit:** the affected x64 project/lib builds clean (`/p:Platform=x64`); for any **linking** project, the link log greps `unresolved external symbol` == 0 (the `/FORCE` trap — exit 0 ≠ clean link).
- **After every plan wave:** canonical 5-target x64 build + Win32 non-regression build; **both** 0 unresolved.
- **Before `/gsd-verify-work`:** dumpbin confirms `machine (x64)` on the exe + all 3 plugins; x64 boots to char-select under rasterMajor 5/6/7; Win32 boots under rasterMajor 5 + 11 unchanged; every staged DLL in `stage-x64/` is dumpbin-x64.
- **Max feedback latency:** one build target (minutes) for compile/link gates; one boot for runtime gates.

---

## Per-Task Verification Map

> Populated per-plan by the planner. Each task's `<acceptance_criteria>` must map to one of the requirement verifications below. Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky

| Task ID | Plan | Wave | Requirement | Verify Type | Automated/CLI Command | Status |
|---------|------|------|-------------|-------------|-----------------------|--------|
| (planner fills) | | | X64-01/02/04 / SC#4 | build · dumpbin · link-grep · boot | see Requirements → Test Map | ⬜ pending |

---

## Requirements → Test Map (from 33-RESEARCH.md)

| Req ID | Behavior | Verify Type | Command / Procedure | Auto? |
|--------|----------|-------------|---------------------|-------|
| X64-01 | `SwgClient_*.exe` is x64 | build + dumpbin | `dumpbin /headers stage-x64/SwgClient_d.exe \| grep -i machine` → `8664 (x64)` | ✅ CLI |
| X64-01 | x64 platform in sln + boot-path vcxprojs | grep | `grep "Debug\|x64" src/build/win32/swg.sln` + per-project `<ProjectConfiguration Include="Debug\|x64">` check | ✅ CLI |
| X64-04 | all third-party x64 libs link; no unresolved | link-grep | x64 link log `grep "unresolved external symbol"` == 0 | ✅ CLI |
| X64-04 | no missing-DLL at boot | runtime boot | launch x64 exe; no missing-DLL popup; reaches char-select. Wave-0 pre-check: every staged DLL dumpbin-x64 | ❌ manual smoke |
| X64-04 | icu / discord-rpc | N/A | satisfied-by-N/A — not dependencies of this client (filesystem-verified zero refs; user-confirmed 2026-06-17) | ✅ documented |
| X64-02 | gl05/06/07 are x64 | dumpbin | `dumpbin /headers stage-x64/gl0{5,6,7}_d.dll \| grep machine` → x64 | ✅ CLI |
| X64-02 | x64 boots char-select under rasterMajor 5/6/7 | runtime boot | set `rasterMajor=5/6/7` in `client_d.cfg`; launch x64 `SwgClient_d.exe`; reach char-select (3 boots) | ❌ manual smoke |
| SC#4 | 32-bit non-regression | build + boot | full Win32 5-target build links clean (0 unresolved) + dual-renderer boot smoke (rasterMajor 5 + 11) unchanged | ✅ build / ❌ manual boot |

---

## Wave 0 Requirements

- [ ] `stage-x64/` (or a platform-conditioned postbuild target) — so x64 DLLs never clobber the shipped Win32 `stage/` (protects the SC#4 non-regression gate)
- [ ] x64 import-lib generation for libxml2 / pcre / jpeg (from the Restoration x64 DLL exports: `dumpbin /exports` → `.def` → `lib /machine:x64 /def:`) — needed before the x64 link
- [ ] tinyxml x64 build path (author `.vcxproj` or fold the 3 `.cpp` into sharedXml) — the one dep with no DLL fallback
- [ ] A boot-time DLL-presence checklist (dumpbin-x64 on every staged DLL) before the first x64 boot attempt
- [ ] Re-establish committed `/we4311 /we4312` on the x64 platform (N1) — fold into `x64-platform.props`

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| x64 client boots to character select under rasterMajor 5/6/7 | X64-02 | No headless render harness; boot-to-char-select is a live-window observation | Set `rasterMajor` in `stage-x64/client_d.cfg`, launch `SwgClient_d.exe`, confirm char-select renders. Repeat for 5, 6, 7. |
| x64 client launches with no missing-DLL popup | X64-04 | DLL-load failures only surface at process launch | Launch x64 exe from `stage-x64/`; confirm no "missing DLL" / "not a valid Win32 application" dialog |
| 32-bit dual-renderer boot unchanged | SC#4 | Regression is observed against the shipped baseline, not asserted | Boot Win32 `SwgClient_d.exe` under rasterMajor 5 and 11; confirm char-select reached as before |

---

## Validation Sign-Off

- [ ] Every task's `<acceptance_criteria>` maps to a build / dumpbin / link-grep / boot verification above
- [ ] Sampling continuity: no 3 consecutive tasks without an automated (CLI) verify
- [ ] Wave 0 covers all MISSING references (stage-x64, import-libs, tinyxml, DLL checklist, /we4311 guardrail)
- [ ] No watch-mode flags
- [ ] `/FORCE` link-grep (0 unresolved) is an explicit acceptance criterion on every linking task
- [ ] `nyquist_compliant: true` set in frontmatter once the above hold

**Approval:** pending
