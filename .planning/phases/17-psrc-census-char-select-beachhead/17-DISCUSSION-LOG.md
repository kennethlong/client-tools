# Phase 17: PSRC Census + Char-Select Beachhead - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-27
**Phase:** 17-PSRC Census + Char-Select Beachhead
**Areas discussed:** Census mechanism + asset source, Pass::apply constant scope, Diagnostic bridge PS, Validation evidence bar

---

## Census mechanism

| Option | Description | Selected |
|--------|-------------|----------|
| Engine-instrumented boot log | Log per-shader lang/profile/includes/samplers/constants at the PSRC-discard site during a real char-select boot; reuses proven asset resolution, captures exact working set | ✓ |
| Offline Python (tre_reader) | Extend tre_reader.py to crack swgsource_3.0.tre offline; no boot but must replicate asset resolution + guess the working set | |
| Standalone C++ harness | Link engine IFF + ShaderImplementation loader offline; most faithful + reusable but most up-front wiring | |

**User's choice:** Engine-instrumented boot log
**Notes:** Captures exactly the char-select working set deterministically; reuses the proven `.tre`/`.toc` resolution; instrumentation point is `ShaderImplementation.cpp:2895-2908`.

## Census source

| Option | Description | Selected |
|--------|-------------|----------|
| SWGSource 3.0 as-is + keep instrumentation | Census what the client already loads; keep the logging behind a flag for Phase 20 reuse; defer the retail-TRE diff | ✓ |
| SWGSource 3.0 as-is, throwaway instrumentation | Same source, rip logging out after Phase 17 (Phase 20 re-adds) | |
| Reconcile against retail TRE first | Resolve the SWGSource-vs-retail asset-diff before censusing — more rigorous, but pulls a deferred item into the gating path | |

**User's choice:** SWGSource 3.0 as-is + keep instrumentation
**Notes:** Asset source = `swgsource_3.0.tre` + `swg_dev_bundle` per `client_d.cfg:23-25`. Retail-diff todo stays deferred.

## Pass::apply constant scope

| Option | Description | Selected |
|--------|-------------|----------|
| Reflect-generic path, feed char-select subset | Build D3DReflect-driven upload generically; wire only material + textureFactor now; defer scroll/fog/stencil | ✓ |
| Full Pass::apply parity now | Wire all five constant feeds mirroring D3D9 this phase — fewer future touches but lands unverifiable scroll/fog/stencil + pulls in stencil state work | |
| Census-driven — wire exactly what it shows | Defer the scope decision until the census reveals the referenced constant set | |

**User's choice:** Reflect-generic path, feed char-select subset
**Notes:** Minimal shared `Direct3d11_StaticShaderData` surgery, stays bootable, reflection plumbing reusable for later phases. Reflection-driven, NOT copied D3D9 register indices.

## Diagnostic bridge PS

| Option | Description | Selected |
|--------|-------------|----------|
| Recompile-first, bridge as fallback tool | Go straight to PSRC recompile; build the multi-sampler bridge only if attribution gets murky | |
| Bridge-first as deliberate isolation | Build the multi-sampler dynamic PS first to prove plumbing before recompile is a variable | |
| You decide during execution | Executor picks based on how the recompile lands | ✓ |

**User's choice:** You decide during execution
**Notes:** Captured as Claude/executor discretion (D-09) — start recompile, pivot to the bridge only if the first textured frame is ambiguous.

## Validation evidence bar

| Option | Description | Selected |
|--------|-------------|----------|
| Manual A/B pairs, committed + log gate | Eyeball A/B at default pose, commit matched pairs, id=342/343==0 gate | |
| Scripted pixel-diff harness | Build a per-pixel delta + threshold tool now — objective but extra work + thresholding care | |
| Hybrid — manual gate now, structured for diff later | Manual A/B is the Phase 17 gate, but save matched pairs in a structured COMPARISON dir + naming convention so a diff harness bolts on later without recapturing | ✓ |

**User's choice:** Hybrid — manual gate now, structured for diff later
**Notes:** Mirrors `docs/research/phase12-baseline/COMPARISON.md`. CHAR-02 + minimap-style claims require a committed matched pair (Iter-44B lesson). Single-stream-vs-multistream = manual RenderDoc A/B.

---

## Claude's Discretion
- **Bridge PS sequencing (D-09):** recompile-first, build the diagnostic bridge only if attribution gets murky.
- Single-stage body/clothing control material choice — census-derived.
- Census-flag gating mechanism (cfg key vs compile-time define) — executor's choice, must leave D3D9 path untouched when off.

## Deferred Ideas
- SWGSource-vs-retail TRE faithfulness reconciliation (revisit only if census surfaces modified shaders).
- Single-stream-skinning fix-vs-flip decision → Phase 22.
- Scripted pixel-diff harness build → Phase 20+.
- scroll / fog / stencil constant feeds → Phase 19 / Phase 21.
- Reviewed-not-folded todos: `2026-05-15-swgsource-vs-whitengold-tre-asset-diff` (informational, space-scene), `2026-05-15-cantina-corner-snap-engine-improvement` (collision quirk, not PS work).
