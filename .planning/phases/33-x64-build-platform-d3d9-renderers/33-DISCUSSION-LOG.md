# Phase 33: x64 Build Platform + D3D9 Renderers - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-17
**Phase:** 33-x64-build-platform-d3d9-renderers
**Areas discussed:** Third-party x64 sourcing, dpvs on x64, Project scope of the add, Residual D3DX blocker, Asm .vsh path handling

---

## Third-party x64 sourcing

| Option | Description | Selected |
|--------|-------------|----------|
| Hybrid: build buildable, lift binary-only | Build x64 from in-tree source (zlib/pcre/libxml2/jpeg/tinyxml); lift Restoration x64 DLLs + gen import libs for binary-only middleware (dpvs/bink/icu/discord-rpc) | ✓ |
| Build/vendor everything clean | No Restoration binaries; procure/build fresh x64 of every dep (dPVS is proprietary — likely a dead end) | |
| Lift all Restoration x64 binaries | Use Restoration's whole x64 DLL set + gen import libs for all | |

**User's choice:** Hybrid: build buildable, lift binary-only
**Notes:** Honors the "no shortcuts" value where a clean build is genuinely possible; binary-only proprietary middleware has no clean-build option.

---

## dpvs on x64

| Option | Description | Selected |
|--------|-------------|----------|
| Lift Restoration's x64 dpvs.dll | Use prebuilt x64 dpvs.dll + gen import lib + reuse headers | ✓ |
| Stub/disable dpvs on x64 | Compile-out DPVS (occlusion returns visible); loses outdoor culling | |
| You decide | Let research confirm import-lib feasibility, then pick | |

**User's choice:** "Lets start with 1, then decide if we want to keep it at all after the 64x migration. We were finding only marginal advantages even in the dxd9 use case."
**Notes:** Lift to get x64 linking/booting now; FLAGGED follow-up (deferred) to evaluate dropping DPVS entirely post-x64 given marginal benefit. Ties to the Phase-23 DPVS verdict + config gate.

---

## Project scope of the add

| Option | Description | Selected |
|--------|-------------|----------|
| Boot-path subset only | Phase 31's ~57 in-scope libs + gl05/06/07 + SwgClient; editors stay Win32 | ✓ |
| All 132 vcxprojs | Every project incl. pre-broken editors | |

**User's choice:** Boot-path subset only
**Notes:** Matches Phase 31's D-03 scope — mechanical, bounded, no editor sprawl.

---

## Residual D3DX blocker

| Option | Description | Selected |
|--------|-------------|----------|
| Absorb into Phase 33 as precondition | Finish D3DX removal (DirectXMath/own-impl + asm port) before the platform-add link | ✓ |
| Split into its own phase before 33 | Insert a dedicated D3DX-removal phase between 32 and 33 | |
| You decide | Weigh scope/risk and recommend | |

**User's choice:** Absorb into Phase 33 as precondition
**Notes:** Keeps the x64-link work in one phase. Static d3dx9 has no x64 lib, so all residual D3DX must clear before the x64 link.

---

## Asm .vsh path handling

| Option | Description | Selected |
|--------|-------------|----------|
| D3DAssemble, census-gated | Port asm .vsh → D3DAssemble; run census first, commit only if it compiles+renders clean | ✓ |
| D3DAssemble w/ redist-d3dx9 fallback | Try D3DAssemble; fall back to x64 redist d3dx9 for asm path if dialect breaks | |
| You decide | Census-driven; pick fallback only if needed | |

**User's choice:** "I think #1, but this has already been done for the d3d11 path right?"
**Notes:** Verified — the D3D11/gl11 path does NOT transfer. gl11 sidestepped asm with the Phase-19 `[P19VSFALLBACK]` generic fallback VS (D3D11 can't run vs_1_1 bytecode); D3D9 natively executes the asm, so gl05/06/07 must genuinely assemble it via D3DAssemble. Census de-risks the unproven SWG asm dialect. redist-d3dx9 fallback explicitly rejected (re-introduces the dep being removed).

---

## Claude's Discretion

- Exact per-lib sourcing disposition (build-from-source vs lift) — research enumerates against `D:\SWG Restoration\x64\` + in-tree source.
- dpvs import-lib generation mechanics + header reuse.
- Which build configurations get x64 (must cover Debug+Release + all three D3D9 plugin flavors gl05/06/07; Optimized = planner's call).
- Grow `x64-scratch/x64-compile.props` into committed platform props vs author fresh per-project.
- Build/sequencing order of the D3DX-removal precondition vs the platform-add within the phase.

## Deferred Ideas

- Evaluate dropping DPVS entirely post-x64 (marginal benefit; ties to Phase 23 verdict + config gate).
- gl11/D3D11 x64 → Phase 34. Miles 9.3b audio x64 → Phase 35.
- redist-d3dx9 fallback for asm path — contingency only, surfaced as a checkpoint if census fails.
- Door-snap (VERIFY-01) / OOM-class (VERIFY-02) / CORNERSNAP strip (VERIFY-03) → Phase 36.
