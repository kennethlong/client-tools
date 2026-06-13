---
created: 2026-06-13
title: Test Jump To Lightspeed (JTL / space) rendering now that the D3D11 renderer + asset-PS pipeline are mature
area: testing / space rendering / D3D11 parity
next_action: defer — run a dedicated JTL render-test pass; many ground-side renderer fixes (asset PS, FFP combiner, lighting, shader compile) likely improved space as a side effect. Pair with the TRE-diff tool to settle data-vs-code.
files:
  - D:/Code/swg-client-v2/  (codebase)
  - src/engine/client/library/clientGame/src/shared/space/  (NebulaManagerClient, particle/swoosh trails, etc.)
references:
  - .planning/todos/pending/2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md  (the data-vs-code space investigation; this is its validation counterpart)
  - .planning/phases/11-d3d11-renderer-plugin/11-SPEC.md  (Phase 11 explicitly EXCLUDED space)
status: research
priority: low (not blocking; informational — gauges whether JTL is revivable)
---

## What this is

Kenny (2026-06-13): we should add a todo to **test JTL space rendering** now. Back at the
Phase 11 era (D3D11 port start), space "worked barely — most of the graphics artifacts
didn't load properly." Since then the v2.0–v2.2 effort rebuilt the renderer (D3D11 asset-PS
pipeline, FFP combiner cascade, lighting parity, shader compilation, texture binding). Kenny's
intuition: **we may have fixed most of those early space issues as a side effect** of the
ground/world/interior parity work, because space shares that asset-PS / shader / lighting
infrastructure.

## My read (worth testing, not assuming)

Likely a MIX:
- **Probably improved:** the *renderer-side* space artifacts — anything that was broken
  because the D3D11 asset-PS / shader-compile / lighting / FFP path was incomplete. That
  infrastructure is now mature and is shared by space objects (ship hulls, station interiors,
  HUD). So a real chunk of the early "artifacts didn't load" symptoms plausibly cleared up.
- **Probably still broken:** the originally-hypothesized *TRE-data* mismatches (SWGSource v3.0
  assets vs the 2010 client's expected format) — nebula definitions, space `.iff`, particle
  defs. A renderer fix doesn't fix missing/wrong data.
- **Untested:** space-specific paths (NebulaManagerClient, cockpit HUD, swoosh/particle
  trails, ship LODs) were never exercised during the ground-focused v2.2 work — could hold
  latent regressions OR latent fixes.

## Suggested test pass

1. Boot (D3D11, rasterMajor=11 — the default/clean renderer), get into space (JTL launch from
   a starport, or `/space` if available on the SWGSource VM).
2. Capture: nebula sectors, ship hull self + nearby ships, asteroid fields, station interior,
   cockpit HUD / reticle / weapon-group overlays. Note what renders clean vs missing/corrupt.
3. A/B against the D3D9 path (gl05/gl07) and against the **Restoration client** (it runs D3D9
   in x64 and is a known-good reference) to separate "our bug" from "SWGSource-data bug".
4. For anything still broken, use the **v2.3 TRE compare tool (Phases 28-30)** to diff the
   space assets vs Restoration/SWGEmu (see the TRE-diff todo) — settles data-vs-code.

## Priority

Low / informational. Not blocking v2.3. Worth doing because it gauges whether JTL is feasible
to revive and converts a long-standing "space barely works" unknown into a concrete bug list.
