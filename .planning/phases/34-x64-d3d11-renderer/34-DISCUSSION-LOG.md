# Phase 34: x64 D3D11 Renderer - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-17
**Phase:** 34-x64-d3d11-renderer
**Areas discussed:** gl11 x64-correctness posture, Parity verification rigor, Debug-x64 verification path, Build-config scope

---

## gl11 x64-correctness posture

| Option | Description | Selected |
|--------|-------------|----------|
| Mechanical mirror + fix-what-breaks | Mirror gl05's x64 vcxproj blocks, link, fix only what the x64 link/boot surfaces (C4267 N2 carry-forward + runtime quirks). Justified — gl11 was already swept x64-clean in Phase 31. | ✓ |
| Proactive D3D11-specific audit first | Re-scan all Direct3d11 TUs for pointer-width + D3D11 hazards (cbuffer, shader-cache, reflection SRV remap) before linking. | |

**User's choice:** Mechanical mirror + fix-what-breaks
**Notes:** Grounded by the scout finding that Phase 31 D-03 scoped all four plugins (gl05/06/07/**11**) and `compile-all.ps1` compiled the Direct3d11 TUs x64-clean for the C4311/C4312/C4244 class. gl11 is NOT an unswept plugin like dpvs was — the proactive audit would largely re-tread completed work.

---

## Parity verification rigor

| Option | Description | Selected |
|--------|-------------|----------|
| Regression triad + RenderDoc x64-vs-Win32 A/B | Verify dressed char-select + Tatooine + cantina interior; capture gl11 Win32 vs gl11 x64 same-scene and diff (arch-only A/B). | ✓ |
| Literal SC only | Boot to character select under rasterMajor=11. Leaves v2.2 parity effects unverified on x64. | |
| Full v2.2 parity re-sweep | Re-run the entire v2.2 parity suite on x64. Maximum confidence, highest cost. | |

**User's choice:** Regression triad + RenderDoc x64-vs-Win32 A/B
**Notes:** gl11 is THE renderer where the v2.2 parity battle was fought; RenderDoc CAN capture D3D11, so a same-renderer/same-config arch-only A/B (32-bit Debug reference vs Debug|x64) cleanly isolates x64-codegen regressions and sidesteps the "RenderDoc cannot capture D3D9" limitation.

---

## Debug-x64 verification path

| Option | Description | Selected |
|--------|-------------|----------|
| Exploit Debug|x64 as primary verify path | x64 lifts the 2GB ceiling that hung 32-bit Debug under extended play; use Debug|x64 with live assert/DEBUG_FATAL oracles as the main verify surface. | ✓ |
| Stay Release-only | Verify in Release|x64 like prior phases. Loses Debug assert oracles. | |
| Both — Debug for asserts, Release for parity | Debug|x64 for asserts/extended sessions, Release|x64 for the parity bar. | |

**User's choice:** Exploit Debug|x64 as primary verify path
**Notes:** The chronic 32-bit Debug OOM (2GB address space) is gone in x64, finally making Debug|x64 viable for extended-session play. Also a first (non-binding) signal toward the Phase-36 OOM-class verdict. Combined with the parity A/B (D-02), both arches are captured in Debug.

---

## Build-config scope

| Option | Description | Selected |
|--------|-------------|----------|
| Debug|x64 only this phase | Author both x64 blocks but only link/validate Debug|x64. Mirrors Phase 33; Release|x64 exercised in a later consolidation. | ✓ |
| Debug + Release|x64 both | Link and validate both configs this phase. More complete, but Release|x64 is unproven for ALL plugins. | |

**User's choice:** Debug|x64 only this phase
**Notes:** Matches Phase 33 (authored Release|x64 blocks for gl05/06/07 but only ever exercised Debug|x64). First Release|x64 link should be one consolidation pass across every plugin + SwgClient, not a gl11-only one-off.

---

## Claude's Discretion

- Exact gl11 x64 `.vcxproj` block layout — mirror gl05's committed structure.
- Whether the DllExport x64 bridge (ported Phase 33 T2) already covers gl11 — confirm at link, re-use don't re-port.
- RenderDoc capture EIDs / which draws to diff in the A/B.
- Whether specific N2 `C4267` sites in the Direct3d11 TUs need real width fixes vs are benign — decide per actual x64 link/runtime behavior.

## Deferred Ideas

- Release|x64 link for ALL plugins + SwgClient (one consolidation pass, later in v3.0).
- Miles 9.3b audio x64 port — Phase 35.
- Door-snap / OOM-class confirmation + CORNERSNAP probe strip — Phase 36 (a clean extended Debug|x64 session here is a first signal toward VERIFY-02, not a close).
- Evaluate dropping DPVS entirely — post-x64 evaluation.

### Reviewed Todos (not folded)

- `2026-06-13-64bit-x64-port.md` — whole-milestone driver (Phases 33–36); this phase is one prerequisite, reviewed-not-folded.
