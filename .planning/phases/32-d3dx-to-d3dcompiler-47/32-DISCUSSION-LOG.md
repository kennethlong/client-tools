# Phase 32: D3DX to d3dcompiler_47 - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-16
**Phase:** 32-d3dx-to-d3dcompiler-47
**Areas discussed:** Compile mechanism, HLSL rewrite depth, Asm path scope, D3DX removal boundary

---

## Compile mechanism

| Option | Description | Selected |
|--------|-------------|----------|
| D3DCompile (roadmap) | Honor SHADER-01: port D3DXCompileShader→D3DCompile. Cleanest, but Phase 27 proved it re-fights the gl11 HLSL battle. Highest effort/revert risk. | ✓ |
| Redist d3dx9 swap | Restoration's proven route: keep D3DXCompileShader, swap static d3dx9.lib → redist DLL. ~Linker swap, may fix FP fault. Requires amending SHADER-01. | |
| Spike both, then lock | Time-box: try redist first; fall back to D3DCompile if not clean. | |

**User's choice:** D3DCompile (roadmap)
**Notes:** User volunteered "Restoration took shortcuts I dont want to take" — the guiding value for the
whole phase. Wants the clean real port over the redist shortcut, eyes open on the higher effort/risk.

---

## HLSL rewrite depth

| Option | Description | Selected |
|--------|-------------|----------|
| Lift gl11's full treatment | HlslRewrite A/B/C + DECLARE_tcs-without-register + cbuffer/register + stage/override reauthored shaders + VS fallback. Memory playbook's explicit recommendation. | ✓ |
| Minimal, escalate as needed | Smallest rewrite set; add gl11 rules only on failure. Risks rediscovering Phase 27's cascade one error at a time. | |
| Census decides per-rule | Census first, map which gl11 rules each failing shader needs; lift selectively. | |

**User's choice:** Lift gl11's full treatment
**Notes:** Consistent with the no-shortcuts steer. Census still maps rule→shader, but intent is wholesale lift.

---

## Asm path scope

| Option | Description | Selected |
|--------|-------------|----------|
| Port asm → D3DAssemble now | No-shortcuts: whole compile surface off D3DX. Highest risk — unvalidated D3DAssemble handles the SWG asm dialect. Fix-A drops entirely if it works. | ✓ |
| Census-gated: try, fall back | Attempt D3DAssemble; if dialect breaks, keep asm on D3DX+Fix-A and split to own phase. | |
| Defer asm, HLSL-only now | Keep //asm on D3DXAssembleShader + Fix-A (SC#3 allows); port only HLSL. Smallest/safest. | |

**User's choice:** Port asm → D3DAssemble now
**Notes:** Captured as census-gated in execution (CONTEXT D-03) — the unvalidated-dialect risk means the
SC#1 census must confirm D3DAssemble viability first, with the fallback-split as contingency.

---

## D3DX removal boundary

| Option | Description | Selected |
|--------|-------------|----------|
| Compile-path-only; flag rest for x64 | Phase 32 = SHADER-01 scope; capture mesh/matrix/surface D3DX removal as deferred (own-impl, not redist). | (Claude's pick) |
| Expand Phase 32 to all D3DX | Rip out ALL d3dx9 this phase — bundles a non-shader refactor into a shader phase. | |
| Compile-only, decide x64 route later | Compile-only; don't pre-commit the mesh/matrix removal strategy. | |

**User's choice:** "I defer to you, but we want to get D3DX out eventually, you pick the best incremental path."
**Notes:** Claude picked compile-path-only this phase + staged own-impl removal of non-compile D3DX
(DirectXMath for matrices, own code for mesh/surface) as a follow-up before the Phase-33 x64 link.
Rationale: bundling the ~15-API non-shader refactor in is the scope-mixing that got Phase 27 reverted.

---

## Claude's Discretion

- D3DX removal boundary / incremental sequencing for non-compile D3DX (D-05) — user explicitly deferred.
- Census methodology and refresh-vs-redo of the Phase-27 framing.
- Exact gl11-rule → failing-shader mapping (research/census drives; intent is wholesale lift).

## Deferred Ideas

- Non-compile D3DX removal (mesh/matrix/surface, ~15 APIs) → own-impl / DirectXMath, NOT redist;
  a dedicated follow-up phase that must land before the Phase-33 x64 link. Flag for ROADMAP.
- Asm-port fallback split — contingency if the SC#1 census shows D3DAssemble can't handle the SWG
  asm dialect.
