---
phase: 31
slug: 64-bit-correctness-foundation
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-15
---

# Phase 31 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> This phase is C++ source surgery with **no unit-test framework in the tree** — validation is
> **compiler-driven + boot-smoke + compile-time `static_assert`**, not a pytest/jest suite.
> The "test framework" is the MSVC toolchain itself (per RESEARCH.md §Validation Architecture).

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | MSVC compiler (`cl.exe`, v145) as the assertion engine — no xUnit harness exists in this repo |
| **Config file** | scratch `src/build/win32/x64-scratch/x64-compile.props` (uncommitted, gitignored) for the x64 worklist; committed `.vcxproj`s for the 32-bit rebuild |
| **Quick run command** | `cl /c /we4311 /we4312 /we4244 <TU>` (scratch x64) per touched translation unit → log greps to 0 C4235/C4311/C4312/C4244 |
| **Full suite command** | `$env:MSBUILD swg.sln /t:Direct3d11;Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient /p:Configuration=Debug /p:Platform=Win32 /nodeReuse:false` then dual-renderer boot smoke (`rasterMajor=5` and `=11`) |
| **Estimated runtime** | scratch per-TU compile ~seconds; full 5-target 32-bit build + dual boot ~minutes |

---

## Sampling Rate

- **After every task commit:** scratch x64 `cl /c` of the touched TU(s) → 0 C4235 (asm) / C4311 / C4312 / C4244; 32-bit build of the owning lib stays clean.
- **After every plan wave:** full canonical 5-target 32-bit build (`/nodeReuse:false`), link-grep `unresolved external symbol` = 0 (exit 0 ≠ clean under `/FORCE`).
- **Before `/gsd:verify-work`:** all in-scope libs compile x64-clean in the scratch harness (residual deferrals documented for Phase 33) + dual-renderer boot smoke green + all new `static_assert`s pass.
- **Max feedback latency:** per-TU scratch compile (seconds)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| (populated during planning/execution) | — | — | BITS-01/02/03 | — | N/A (source-correctness) | compile / smoke / static_assert | see Phase Requirements → Test Map below | ⬜ pending | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

### Phase Requirements → Test Map (from RESEARCH.md)

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BITS-01 | No x64-illegal `__asm` survivors in build path | compile (x64 scratch) | scratch x64 `cl /c` of each in-scope lib → grep log for C4235 (must be 0) | ✅ harness = Wave 0 |
| BITS-01 | FPU/SSE/asm replacements preserve 32-bit behavior | smoke | 32-bit build + dual-renderer boot to char-select | ✅ existing |
| BITS-02 | No pointer/int truncation survivors | compile (x64 scratch) | scratch x64 `cl /c /we4311 /we4312 /we4244` → grep log for C4311/C4312/C4244 (must be 0) | ✅ harness = Wave 0 |
| BITS-03 | Packed/serialized struct sizes unchanged | compile-time assert | 32-bit build compiles with new `static_assert(sizeof==N)` green | ✅ asserts added in-phase |
| BITS-03 | Assets load + saved data + network parse | smoke | boot to char-select (exercises TRE/IFF load + login handshake) under both renderers | ✅ existing |
| SC#4 | 32-bit boots both renderers after edits | smoke | `rasterMajor=5` then `=11` in `client_d.cfg`, Debug exe, boot to char-select | ✅ existing |

---

## Wave 0 Requirements

- [ ] `src/build/win32/x64-scratch/x64-compile.props` — the D-01 scratch x64 compile-only config (uncommitted, gitignored). Sets x64 target, `/we4311 /we4312 /we4244`, `PLATFORM_WIN32` + `_USE_32BIT_TIME_T=1` preprocessor parity, in-scope include dirs.
- [ ] `src/build/win32/x64-scratch/compile-all.ps1` — iterate the in-scope lib TU list, `cl /c` each, collect C4235/C4311/C4312/C4244 into the worklist.
- [ ] In-scope lib/TU manifest — the explicit list of `.cpp` to feed the harness (RESEARCH.md §Build-Graph Scope).
- [ ] No xUnit framework needed/added — the compiler + boot smoke + `static_assert` are the validation surface for C++ source-correctness work.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Dual-renderer boot to character select | BITS-01/02/03, SC#4 | No automated game-boot harness; rendering/asset-load correctness is visual | Set `rasterMajor=5` in `stage/client_d.cfg`, launch `SwgClient_d.exe`, confirm boot to character select; repeat with `rasterMajor=11`. |
| Door-snap watch-item (VERIFY-01, D-04) | BITS-01 | x87 precision-control no-op is an x64-build observation, not validatable in 32-bit | Note only — informational watch-item for Phase 33+ x64 build; no Phase 31 gate. |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify (scratch-x64 compile / 32-bit build / static_assert) or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references (scratch harness props + compile-all script + TU manifest)
- [ ] No watch-mode flags
- [ ] Feedback latency < per-TU scratch compile (seconds)
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
