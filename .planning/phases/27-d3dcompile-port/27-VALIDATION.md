---
phase: 27
slug: d3dcompile-port
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-06-14
---

# Phase 27 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> **This is a renderer/visual port — validation is runtime dual-renderer boot smoke + targeted
> visual spots (D-05), NOT an automated unit-test suite.** No code-level test framework applies to
> shader-compilation output; every prior D3D phase validated via runtime A/B per project convention
> (`27-RESEARCH.md` § Validation Architecture).

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | none — runtime visual A/B (no automated shader-compile test framework) |
| **Config file** | `stage/client_d.cfg` (Debug exe) / `stage/client.cfg` (Release exe); Claude owns `rasterMajor` |
| **Quick run command** | rebuild gl05+gl07; grep link output for `unresolved external symbol` (must be 0) |
| **Full suite command** | boot SwgClient to char-select / in-game under rasterMajor=5, then =11 |
| **Estimated runtime** | ~build + ~2× boot smoke (manual observation) |

---

## Sampling Rate

- **After every task commit:** rebuild the touched plugin(s); confirm 0 unresolved externals (`/FORCE` false-pass guard)
- **After every plan wave:** boot smoke the affected renderer to char-select
- **Before `/gsd:verify-work`:** dual-renderer (rasterMajor=5 AND =11) boots clean to char-select/in-game; the 96 `//asm` consumers (saber/decal/detail) render; Tatooine Fix-A bump/dot3 spot renders without `0xC0000090`
- **Max feedback latency:** one build + one boot per renderer

---

## Per-Task Verification Map

> Filled by the planner against the final task IDs. Validation type is `build` (link grep) or
> `runtime-visual` (boot smoke / targeted spot), not `unit`.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Verify Method | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|---------------|-------------|--------|
| TBD | TBD | TBD | HARD-05 | — | N/A | build / runtime-visual | link grep 0 unresolved + dual-renderer boot smoke | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] Stage `d3dcompiler_47.dll` (tracked source + postbuild copy to `stage/`) — required before any boot test (currently absent from `stage/`)
- [ ] Capture a pre-port A/B baseline screenshot set (gl05 + gl11) for the parity diff

*No test files — this is runtime visual validation per project convention.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| HLSL VS compiles via `D3DCompile` (no D3DX on HLSL path) | HARD-05 | renderer output; no compile-unit harness | rebuild gl05+gl07; boot rasterMajor=5 to char-select/in-game; world renders |
| asm path not dropped (96 `//asm` consumers render) | HARD-05 | visual presence of saber/decal/detail geometry | in-game: lightsaber blades, decals, detail-mapped surfaces present |
| asm path compiles via `D3DAssemble` (D-02-R) | HARD-05 | renderer output; dialect behaviour only observable at runtime | confirm the `//asm` consumers render identically; if `D3DAssemble` rejects the SWG dialect, fall back to `D3DXAssembleShader`+SEH |
| no shader-compile regression (A/B parity) | HARD-05 | visual comparison | dual-renderer boot (rasterMajor=5 AND =11); compare to pre-port baseline |
| Fix-A Tatooine spot clean | D-05 | targeted crash-site observation | the exact Tatooine bump/dot3 VS spot from `db83b0f5c` renders without `0xC0000090` |
| SEH-guard removability | D-03/D-05 | depends on the Tatooine spot result | drop `compileVertexShaderFpGuarded` only after the spot renders clean under the new path |

---

## Validation Sign-Off

- [ ] All tasks map to a `build` (link grep) or `runtime-visual` (boot/spot) verification
- [ ] Sampling continuity: no plugin change committed without a 0-unresolved-externals link grep
- [ ] Wave 0 covers the `d3dcompiler_47.dll` staging + baseline capture
- [ ] Boot invariant honoured: Debug exe→`client_d.cfg`, Release exe→`client.cfg`; no `.cfg` written via PowerShell (UTF-8 BOM → Release boot crash)
- [ ] Dual-renderer (rasterMajor=5 AND =11) clean before `/gsd:verify-work`
- [ ] `nyquist_compliant: true` set once the map above is filled against final task IDs

**Approval:** pending
