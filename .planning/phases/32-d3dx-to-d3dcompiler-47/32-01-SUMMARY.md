---
phase: 32-d3dx-to-d3dcompiler-47
plan: 01
subsystem: infra
tags: [d3dcompiler_47, d3dassemble, d3dxassembleshader, vertex-shader, asm, census, d3d9, x64-port]

# Dependency graph
requires:
  - phase: 31-64bit-correctness-foundation
    provides: x64-clean source tree; the still-32-bit build that this gate's probe links against
provides:
  - "GATE VERDICT: PASS — D3DAssemble (d3dcompiler_47) is byte-for-byte equivalent to D3DXAssembleShader on the SWG .vsh asm dialect; Wave 1 = 32-03 (asm port), 32-04 skipped"
  - "Authoritative corpus refresh: 286 .vsh = 190 //hlsl + 96 //asm, 0 unclassified (Phase-27 framing holds)"
  - "Reusable standalone D3DAssemble dialect probe (GetProcAddress on undecorated export + size/hash diff + D3DDisassemble token fallback) — the shape 32-03 lifts into the engine"
  - "Raw dumpbin evidence: D3DAssemble ordinal 1 in the x86 staged d3dcompiler_47.dll"
affects: [32-03, 32-05, 33-x64-build-platform]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "D3DAssemble resolved via static-local resolve-once GetProcAddress on the undecorated DLL export (sidesteps the C++-mangled d3dcompiler.lib import)"
    - "Cross-assembler equivalence proven empirically (bytecode size+FNV-hash diff, D3DDisassemble token-compare fallback) rather than asserted"

key-files:
  created:
    - .planning/phases/32-d3dx-to-d3dcompiler-47/32-CENSUS.md
    - .planning/research/vsh-probe/d3dassemble_probe.cpp
    - .planning/research/vsh-probe/probe-output.txt
  modified: []

key-decisions:
  - "GATE VERDICT: PASS (clean — zero byte-variance, not PASS-WITH-NOTE). D3DAssemble output is bit-identical to D3DXAssembleShader on all 6 probed shaders incl. the lit opcode and the if-b# static boolean branch; CreateVertexShader S_OK on every blob."
  - "Probe resolves D3DAssemble + D3DDisassemble via GetProcAddress on undecorated exports (no d3dcompiler.lib import) — the only link-faithful path given the mangled .lib symbol."
  - "Corpus count reconciled to ground truth: 12 extracted .vsh on disk (prior docs said 16); 286 full / 3 baseline-probed."
  - "F3 (if b# boolean branch) classified TOOL-path-only: the runtime asm path never defines VERTEX_SHADER_VERSION, so the branches are #if-stripped at runtime; probed as a bonus (still byte-identical), recorded as non-binding."

patterns-established:
  - "Pass-on-sample gate with an explicit recovery path: a non-probed shader failing render in 32-03 → roll back, re-route to 32-04, append to 32-ASM-FOLLOWUP.md"

requirements-completed: [SHADER-01]

# Metrics
duration: ~95min
completed: 2026-06-16
---

# Phase 32-01: D3DAssemble Census + Dialect-Probe GATE Summary

**GATE = PASS: `D3DAssemble` (d3dcompiler_47) assembles the SWG `.vsh` asm dialect to bytecode byte-for-byte identical to the `D3DXAssembleShader` baseline (6/6 shaders, incl. `lit` + `if b#`), `CreateVertexShader` S_OK — so Wave 1 ports the asm path (32-03), 32-04 skipped.**

## Performance

- **Duration:** ~95 min (interactive inline)
- **Completed:** 2026-06-16
- **Tasks:** 3 (Task 1 census, Task 2 probe, Task 3 GATE checkpoint — user ratified PASS)
- **Files created:** 3 (+ a `.gitignore` for build artifacts)

## Accomplishments
- **Empirically settled the phase's central open question.** There was no existence-proof that `D3DAssemble` accepts the SWG asm dialect (Restoration precompiles bytecode; the API is absent from public `d3dcompiler.h`). The probe proves it does — and produces **bit-identical** bytecode to `D3DXAssembleShader` on every probed shader (sizes 588/1384/1456/1540/1636 B, FNV hashes match exactly), with `CreateVertexShader` S_OK throughout.
- **Authoritative corpus refresh:** enumerated all 209 searchPath TREs → **286 unique `.vsh` = 190 `//hlsl` + 96 `//asm`, 0 unclassified**. The Phase-27 190/96-of-286 framing holds unchanged.
- **Construct coverage (binding flag-list):** baseline (c_simple/tfcl/cloudlayer) + **F1 `lit` opcode** (tfcsl) + **F2 4-texcoord set** (tfcl_4uv) + **F3 bonus `if b#` branch** (tfcsl + VERTEX_SHADER_VERSION=20) — all byte-identical.
- **Raw, reproducible dumpbin evidence** persisted (D3DAssemble ordinal 1, undecorated, x86 staged DLL; Windows-Kit x86 import-lib path).

## Task Commits

Executed interactive-inline; the plan's three tasks (two `auto` + the checkpoint verdict) committed as one atomic plan commit (user reviewed at the gate before commit):

1. **Tasks 1–3 (census + probe + ratified GATE verdict)** — see plan commit below.

## Files Created/Modified
- `.planning/phases/32-d3dx-to-d3dcompiler-47/32-CENSUS.md` — corpus classification, dumpbin evidence, construct flag-list, and the `## GATE VERDICT: PASS` section.
- `.planning/research/vsh-probe/d3dassemble_probe.cpp` — standalone x86 probe: `GetProcAddress` D3DAssemble/D3DDisassemble, size+hash diff vs `D3DXAssembleShader`, token-compare fallback, `CreateVertexShader` on a real HAL device.
- `.planning/research/vsh-probe/probe-output.txt` — captured per-shader run (the PASS evidence).
- `.planning/research/vsh-probe/.gitignore` — excludes `*.exe/*.obj/*.pdb` build artifacts.

## Decisions Made
- **Clean PASS, not PASS-WITH-NOTE:** zero byte-variance was observed, so the disasm token-fallback never had to adjudicate anything.
- **F3 (`if b#`) is non-binding:** the runtime asm path doesn't define `VERTEX_SHADER_VERSION`, so the boolean-branch lines are `#if`-stripped at runtime; probed as a bonus only (and still byte-identical).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 — harness completeness] Probe supplied 3 missing `cExtLtData_*` aliases**
- **Found during:** Task 2 (first probe run)
- **Issue:** The Phase-27 extracted `diffuse.inc` (an ILM-pack copy from `ILM_visuals.tre`) references `cExtLtData_parallelSpec_0_*` tangent-light aliases that the extracted `registers.inc` (from `data_other_00.tre`) does not define — a **corpus include version-skew**. Both `D3DAssemble` AND `D3DXAssembleShader` rejected the `diffuse.inc`-using shaders **identically** (`X2005`).
- **Fix:** The probe supplies the 3 aliases (`→ c52/c53/c54`) to **both** assemblers identically, keeping the equivalence comparison valid and letting c_simple/tfcl/tfcl_4uv (F2) assemble. The probe's classifier also gained an `EXCLUDED` class so "both assemblers fail identically" can never be misread as a D3DAssemble divergence.
- **Verification:** After the fix all 6 cases assemble byte-identically; `pass=6 fail=0 excluded=0`.
- **Committed in:** plan commit.

**2. [doc reconcile] Corpus count "16 extracted" corrected to ground-truth "12 `.vsh`"**
- Prior phase docs said "16 extracted"; the on-disk reality is 12 `.vsh` (the dir also holds 1 `.psh` + 11 `.inc`). Recorded the truth and the reconciliation in 32-CENSUS.md §1 (AGENTS.md: files are truth, docs lag).

---

**Total deviations:** 2 (1 harness-completeness, 1 doc-reconcile). **Impact:** none on the gate conclusion — both are about making the sample self-consistent and the count honest; the equivalence result is unchanged.

## Issues Encountered
- `dumpbin /exports` mangled by Git-Bash (`/exports` → `C:\Program Files\Git\exports`) — re-run via the PowerShell tool (AGENTS.md MSYS-mangling note). Resolved.
- Probe link needed `legacy_stdio_definitions.lib` (legacy `d3dx9.lib` references old `__vsnprintf`/`_sprintf`) + `user32/gdi32/advapi32` — the same libs the engine `Direct3d9.vcxproj` links. Resolved.

## Next Phase Readiness
- **Wave 1 = 32-03** (port `Direct3d9_VertexShaderData.cpp:567` `D3DXAssembleShader` → `D3DAssemble`). **32-04 is skipped.** 32-03's Task-1 double-execution guard will assert the `PASS` token + `test ! -f 32-04-SUMMARY.md`.
- Because the gate is PASS (not FAIL-WITH-FOLLOWUP), 32-05 can fully remove the Fix-A SEH guard (both compile paths off D3DX).
- **Next to execute: 32-02** (the HLSL `//hlsl` `:477` path → `D3DCompile`, the big native port) — independent of this gate, runs before Wave-1's 32-03.

---
*Phase: 32-d3dx-to-d3dcompiler-47*
*Completed: 2026-06-16*
