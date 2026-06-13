---
created: 2026-06-12T17:51:06.531Z
title: Config-gate DPVS occlusion per Phase 23 verdict (outdoor on, indoor off)
area: rendering / DPVS culling / clientGraphics
resolves_phase: 24
files:
  - src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp (Option α #else culling-parameter branch, ~line 914; _DEBUG re-gate at the cullingParameters block)
  - docs/recon/23-dpvs-d3d11-profiling.md (the verdict + per-scene data)
  - docs/recon/10-dpvs-profiling.md (Phase 10 D3D9 baseline this revises)
---

## Problem

Phase 23's live D3D11 A/B re-measure flipped BOTH Phase 10 verdicts (docs/recon/23-dpvs-d3d11-profiling.md, 2026-06-12):

- **Outdoor (Mos Eisley plaza): `verdict = keep`** — the DPVS occlusion query culls ~140 objects (median 359 visible vs ~500 without) and wins total_frame_ms median by 0.76 ms / p95 by 1.06 ms. Phase 10 D3D9 said `remove`.
- **Indoor (cantina): `verdict = remove`** — POB portals already bound the visible set (443 identical in both conditions); the occlusion query is pure CPU overhead, OFF wins median by 0.70 ms. Phase 10 said `keep`.

But the shipped Release code still implements Option α (occlusion bit unconditionally removed in the `#else` branch of RenderWorld's culling-parameter selection) — i.e., shipped behavior is now backwards relative to the measured optimum on BOTH sides. The only existing toggle is `ms_forceDisableOcclusionCulling`, a `_DEBUG`-only DebugFlag (F11) that does not exist in Release builds. There is no config knob.

Caveats recorded in the verdict doc: deltas were measured on a Debug build (the harness is `_DEBUG`-only); in Release the absolute deltas likely compress to sub-millisecond — the direction (driven by visible-object counts, not timer precision) should hold, but stakes are small. That's why this is a config knob, not surgery.

## Solution

Make the `DPVS::Camera::OCCLUSION_CULLING` bit config-gated instead of hardcoded:

1. Read a `[ClientGraphics]` cfg key once at install (e.g. `dpvsOcclusionMode = auto | on | off`, default `auto`).
2. `auto` = the measured optimum: OR the OCCLUSION_CULLING bit into the culling parameters only when the camera is NOT inside a POB cell (outdoor → occlusion on; interior → portals only). `on`/`off` = unconditional override for testing.
3. This replaces the hardcoded Option α `#else` branch in RenderWorld.cpp; keep the `_DEBUG` F11 force-disable flag working on top (force-disable wins over config).
4. Optional validation: a Release-side A/B is currently impossible (harness is `_DEBUG`-only) — if Release numbers are wanted before defaulting `auto`, the CSV writer would need a Release-safe variant first.

Note: the Phase 23 instrumentation itself is THROWAWAY (D-15 cleanup target) — don't couple this knob to it. 23-REVIEW.md's 4 warnings live in that throwaway code and are moot once D-15 removes it.
