---
phase: 32
slug: d3dx-to-d3dcompiler-47
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-16
---

# Phase 32 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> **This phase has NO automated shader-test framework (D-07 locks this).** Validation is
> runtime build + boot + render smoke. "Builds/boots/renders are the truth" (AGENTS.md).
> The "automated command" column below is the *build/link gate*; behavioral validation is manual.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None automated (D-07: builds/boots/renders are the truth) |
| **Config file** | `stage/client.cfg` (Release) / `stage/client_d.cfg` (Debug) — `rasterMajor` selects renderer (5=gl05 D3D9, 11=gl11 D3D11) |
| **Quick run command** | Build gate: `$env:MSBUILD` 5-target build, grep log for `unresolved external symbol` (must be 0) |
| **Full suite command** | Dual-renderer boot: `stage/SwgClient_d.exe` under `rasterMajor=5` AND `=11`, both reach character select + render the Tatooine bump/dot3 spot |
| **Estimated runtime** | ~3–5 min build + ~2 min/boot smoke |

---

## Sampling Rate

- **After every task commit:** 5-target MSBuild, grep build log for `unresolved external symbol` = 0 (`/FORCE` masks LNK2019/2001 — exit 0 ≠ clean link)
- **After every plan wave:** Boot smoke for the renderer(s) touched by that wave
- **Before phase verify:** Dual-renderer boot smoke (rasterMajor=5 AND =11) + Tatooine bump/dot3 render, both clean
- **Max feedback latency:** ~5 min (build) / ~2 min (boot)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 32-W0-01 | census | 0 | SHADER-01 | — | N/A | manual probe | `D3DAssemble` accepts representative `//asm` sample → valid bytecode → renders (the D-06 GATE) | ✅ corpus at `.planning/research/vsh-extract/` | ⬜ pending |
| 32-HLSL-* | hlsl | 1 | SHADER-01 | — | N/A | build gate + manual | 5-target build, `unresolved external symbol`=0; gl05 boot to char-select + render | ❌ manual smoke | ⬜ pending |
| 32-ASM-* | asm | 2 | SHADER-01 | — | N/A | build gate + manual | `D3DAssemble` path compiles; dual-renderer render clean | ❌ manual smoke | ⬜ pending |
| 32-RM-* | removal | 3 | SHADER-01 | — | N/A | grep/dumpbin | `dumpbin`/grep shows no `D3DXCompileShader`/`D3DXAssembleShader`/`D3DXInclude` in compile path | ❌ grep gate | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] **D3DAssemble dialect probe (the D-06 census GATE)** — assemble a representative `//asm` sample (`.planning/research/vsh-extract/` has 11 `//asm` `.vsh` with canonical `TARGET`/`m4x4`/`dcl_*`/`oPos` syntax) via `D3DAssemble` (declared manually / `GetProcAddress` — not in public `d3dcompiler.h`), `CreateVertexShader` the bytecode, and render. **PASS** = assembles + renders clean → asm port proceeds. **FAIL** = dialect rejected → take the sanctioned fallback (keep `//asm` on `D3DXAssembleShader` + Fix-A, split asm port to a follow-up phase).
- [ ] **HLSL census refresh** — confirm the `//hlsl` vs `//asm` split (Phase-27 framing: 190 `//hlsl` + 96 `//asm` of 286 `.vsh`); no VS may be silently nulled (no skipped draws).

*Wave 0 IS the de-risk gate — it must complete before the asm port commits.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| HLSL `.vsh` compile correct | SHADER-01 | No shader-test framework; correctness = visual render | Boot `SwgClient_d.exe` `rasterMajor=5`, reach character select, render Tatooine bump/dot3 spot; diff against Fix-A reference (db83b0f5c) — no missing/garbled geometry |
| Asm `.vsh` compile correct | SHADER-01 | Same — runtime render is the proof | Same scene under the `D3DAssemble` path; no skipped draws vs the D3DX baseline |
| Dual-renderer parity | SHADER-01 | gl11 must remain untouched/clean | Boot under `rasterMajor=11`, confirm no regression (gl11 already on D3DCompile since Phase 11) |
| D3DX absent from compile path | SHADER-01 | Link-graph fact, not behavior | `dumpbin`/grep gl05_d.dll + gl07 for `D3DXCompileShader`/`D3DXAssembleShader` symbols = absent; `d3dx9` only in mesh/matrix/surface (D-05, stays) |

---

## Validation Sign-Off

- [ ] All tasks have a build/link gate, a grep/dumpbin check, or a documented manual smoke
- [ ] Sampling continuity: every wave ends with a build gate; behavioral waves end with a boot smoke
- [ ] Wave 0 census GATE (D3DAssemble probe) runs FIRST and gates the asm port
- [ ] No automated shader framework invented (D-07) — manual render smoke is the contract
- [ ] Dual-renderer (rasterMajor 5 + 11) parity confirmed before phase verify
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
