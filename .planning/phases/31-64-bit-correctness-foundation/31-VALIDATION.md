---
phase: 31
slug: 64-bit-correctness-foundation
status: validated
nyquist_compliant: true
wave_0_complete: true
created: 2026-06-15
validated: 2026-06-16
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

- **After every task commit:** scratch x64 `cl /c` of the touched TU(s) → 0 C4235 (asm) / C4311 / C4312 / C4244; in plans 02 + 05 the `_DEBUG` numeric/round-trip oracle compiles. The 32-bit build of the owning lib stays clean (per-lib `cl /c`, NOT the full 5-target build — that gate is Wave 3 only, see note below).
- **After Wave 2 (all source edits merged):** the full canonical 5-target 32-bit build + dual-renderer boot smoke runs ONCE, in Wave 3 / plan 06 (`/nodeReuse:false`), link-grep `unresolved external symbol` = 0 (exit 0 ≠ clean under `/FORCE`). The full build is DELIBERATELY NOT run mid-wave by individual wave-2 plans — concurrent wave-2 plans share `stage/` + `src/compile/win32/` output dirs and a wave-2 plan's boot would false-green against only its own subset of edits.
- **Before `/gsd:verify-work`:** all in-scope libs compile x64-clean in the scratch harness (residual deferrals classified + documented for Phase 33) + dual-renderer boot smoke green + all new `static_assert`s pass + plan-02 `_DEBUG` self-tests pass at startup.
- **Max feedback latency:** per-TU scratch compile (seconds)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Secure Behavior | Test Type | Automated Command | Committed? | Status |
|---------|------|------|-------------|-----------------|-----------|-------------------|------------|--------|
| BITS-03 packed-struct guards | 05 | 2 | BITS-03 | on-disk TGA/customization layout unchanged | `static_assert(sizeof)` | runs on every 32-bit build | ✅ committed (`TargaFormat.cpp`, `AssetCustomizationManager.cpp`) | ✅ green |
| BITS-03 Archive uint32_t pin | 05/08 | 2 | BITS-03 | wire count byte-identical to 32-bit server | `static_assert` + `assert(<=UINT32_MAX)` | runs on every build (Archive.h / AutoDelta*.h) | ✅ committed | ✅ green |
| BITS-01 FPU/SSE numeric oracle | 02/07 | 2 | BITS-01 | FPU/SSE port numerically equivalent to scalar | `_DEBUG` round-trip + numeric oracle | fires at Debug startup (no DEBUG_FATAL) | ✅ committed (`#if _DEBUG` self-tests) | ✅ green |
| SC#4 link-clean | 06 | 3 | SC#4 | 32-bit links with 0 unresolved | log grep | `grep -c "unresolved external symbol" build-32bit.log` == 0 | ✅ committed (build log) | ✅ green |
| BITS-01/02/03 boot smoke | 06 | 3 | BITS-01/02/03, SC#4 | dual-renderer boot to char-select, no regression | manual smoke | `rasterMajor=5` then `=11`, boot `SwgClient_d.exe` | ⚠️ manual-only (no game-boot harness) | ✅ green (human-approved 2026-06-16) |
| BITS-02 x64 compile guardrail | 01 | 0 | BITS-02 | no x64 truncation/asm survivors | `cl /c /we4311 /we4312 /we4244` | scratch x64 harness (per-TU) | ❌ gitignored throwaway (N1) | ⏸ deferred → Phase 33 |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky · ⏸ deferred*

### Phase Requirements → Test Map (from RESEARCH.md)

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| BITS-01 | No x64-illegal `__asm` survivors in build path | compile (x64 scratch) | scratch x64 `cl /c` of each in-scope lib → grep log for C4235 (must be 0) | ✅ harness = Wave 0 (plan 01) |
| BITS-01 | FPU/SSE port is NUMERICALLY equivalent (not just compile-clean) | compile-time + runtime assert | plan-02 `_DEBUG` FPU control-word round-trip + SseMath/Transform numeric oracle vs scalar ref (1e-4f) | ✅ self-tests added in-phase (plan 02) |
| BITS-01 | FPU/SSE/asm replacements preserve 32-bit behavior | smoke | 32-bit build + dual-renderer boot to char-select (Wave 3 / plan 06) | ✅ existing |
| BITS-02 | No pointer/int truncation survivors (implicit) | compile (x64 scratch) | scratch x64 `cl /c /we4311 /we4312 /we4244` → grep log for C4311/C4312/C4244 (must be 0) | ✅ harness = Wave 0 (plan 01) |
| BITS-02 | No EXPLICIT truncating-cast survivors (/we4311 is blind to these) | grep audit | plan-01 `-CastAudit` → explicit-cast-audit.log; plan 04 resolves owned-file entries | ✅ cast audit = Wave 0 (plan 01) |
| BITS-03 | Packed/serialized struct sizes unchanged | compile-time assert | 32-bit build compiles with new `static_assert(sizeof==N)` green | ✅ asserts added in-phase (plan 05) |
| BITS-03 | Archive std::map uint32_t pin is EXERCISED | compile (x64 scratch) | plan-05 `archive-map-instantiation.cpp` instantiates get/put(std::map<int,int>) → x64-clean | ✅ instantiation TU added (plan 05) |
| BITS-03 | Assets load + saved data + network parse | smoke | boot to char-select (exercises TRE/IFF load + login handshake) under both renderers | ✅ existing |
| SC#4 | 32-bit boots both renderers after edits | smoke | `rasterMajor=5` then `=11` in `client_d.cfg`, Debug exe, boot to char-select (Wave 3 / plan 06) | ✅ existing |

---

## Wave 0 Requirements

- [x] `src/build/win32/x64-scratch/x64-compile.props` — the D-01 scratch x64 compile-only config (uncommitted, gitignored). Sets x64 target, `/we4311 /we4312 /we4244`, `PLATFORM_WIN32` + `_USE_32BIT_TIME_T=1` preprocessor parity, in-scope include dirs. **Designed in plan 01 Task 2.**
- [x] `src/build/win32/x64-scratch/compile-all.ps1` — iterate the in-scope lib TU list, `cl /c` each, collect C4235/C4311/C4312/C4244 into the worklist; supports `-SingleTu`, `-Scope`, `-CastAudit` modes (the per-task verify interface plans 02-06 depend on). **Designed in plan 01 Task 3.**
- [x] EXHAUSTIVE in-scope lib/TU manifest — the `.cpp` list to feed the harness, derived from the in-scope libs' `<ClCompile Include=.../>` lists (RESEARCH.md §Build-Graph Scope), not a hand-picked first cut. **Designed in plan 01 Task 1.**
- [x] No xUnit framework needed/added — the compiler + boot smoke + `static_assert` + the plan-02 `_DEBUG` numeric oracles are the validation surface for C++ source-correctness work.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Dual-renderer boot to character select | BITS-01/02/03, SC#4 | No automated game-boot harness; rendering/asset-load correctness is visual | Set `rasterMajor=5` in `stage/client_d.cfg`, launch `SwgClient_d.exe`, confirm boot to character select (no startup DEBUG_FATAL from the plan-02 _DEBUG self-tests); repeat with `rasterMajor=11`. (Wave 3 / plan 06 Task 3.) |
| Door-snap watch-item (VERIFY-01, D-04) | BITS-01 | x87 precision-control no-op is an x64-build observation, not validatable in 32-bit | Note only — informational watch-item for Phase 33+ x64 build; no Phase 31 gate. (P_24 is RETAINED on 32-bit per D-04 review refinement #1b.) |
| x64 compile-cleanness guardrail (`/we4311 /we4312 /we4244`) + archive-map instantiation TU | BITS-02/03 | Validated only via the **gitignored** `x64-scratch/` throwaway harness — structurally un-committable as a standing test until an x64 `.vcxproj` platform exists (you cannot x64-compile-check with no x64 target) | **Deferred to Phase 33 (N1 coverage hole).** Re-establish `/we4311 /we4312` on the committed x64 `.vcxproj`s and re-add the instantiation TU once the x64 platform lands. Tracked in `31-PHASE33-RESIDUAL-WORKLIST.md` N1. |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify (scratch-x64 compile / 32-bit build / static_assert / `_DEBUG` oracle) or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references (scratch harness props + compile-all script w/ `-SingleTu`/`-Scope`/`-CastAudit` + exhaustive TU manifest) — all designed in plan 01 (Wave 1, the enabler wave; the harness IS Wave 0 for the fix waves)
- [x] No watch-mode flags
- [x] Feedback latency < per-TU scratch compile (seconds)
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** signed off 2026-06-15 — the Wave 0 scratch harness is fully designed in plan 01 (props + driver w/ all three verify-modes + exhaustive ClCompile-derived manifest); every code-producing task carries an `<automated>` command; the plan-02 `_DEBUG` numeric/round-trip oracles + plan-05 instantiation TU close the semantic-correctness gaps the reviews flagged. Sampling continuity holds across all six plans.

---

## Validation Audit 2026-06-16

Post-execution State-A audit (phase complete: 9 plans, 31-VERIFICATION.md 4/4 must-haves PASSED).

| Metric | Count |
|--------|-------|
| Requirements audited | 4 (BITS-01, BITS-02, BITS-03, SC#4) |
| Covered by committed automated tests | 3 (BITS-01 `_DEBUG` oracles, BITS-03 `static_assert`s, SC#4 link-grep) |
| Manual-only | 1 (dual-renderer boot smoke — human-approved 2026-06-16) |
| Deferred (un-fillable pre-x64) | 1 (BITS-02 x64 compile guardrail → Phase 33 N1) |
| New test files generated | 0 |

**Findings:** The committed automated surface is verified present and green — `static_assert(sizeof)` packed-struct guards (`TargaFormat.cpp`, `AssetCustomizationManager.cpp`) and Archive `uint32_t` count guards run on every 32-bit build; `_DEBUG` FPU/SSE numeric oracles fire at Debug startup; 32-bit link is clean (0 unresolved). No xUnit framework exists in this C++ tree (by design — the compiler + `static_assert` + `_DEBUG` oracles + boot smoke ARE the validation surface, per RESEARCH.md §Validation Architecture). The single non-committed validation — the x64 `/we4311` compile guardrail + archive-map instantiation TU — lived in the gitignored `x64-scratch/` harness and is **structurally un-committable until Phase 33 adds the x64 `.vcxproj` platform**; documented as deferred (N1). **Verdict: nyquist-compliant for achievable scope; no fillable gaps.**
