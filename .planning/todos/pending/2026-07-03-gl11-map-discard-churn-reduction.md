---
created: 2026-07-03
title: gl11 Map(WRITE_DISCARD) churn reduction — re-enable NV driver threading safely
area: client graphics / Direct3d11 / performance
status: backlog
priority: medium-high (the last gap between gl11 and the butter-smooth gl05 baseline)
references:
  - the conviction: 2026-07-03 Test 4 — [Direct3d11] preventDriverInternalThreading=false made
    gl11 "almost buttery" (micro-stutter = 1-2 frame hangs GONE); flag committed config-gated
    at 04c6cee6a, default true
  - memory: project_nvwgf2um_null_deref_cui_text_d3d11_map (why the flag exists: NV UMD
    worker-thread race on rapid Map(WRITE_DISCARD) churn, CONSULT-51; debug layer's serialized
    work also cured it = proof it's driver-side racing)
  - exoneration record: eviction knob ✗, pre-885 binary ✗, x64 ✗ (2026-07-03 PM handoff addendum)
---

## The trade to break

`D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS` stops the nvwgf2um zone-in crash
(driver worker-thread race servicing our Map(WRITE_DISCARD) churn) but throttles the driver's
internal threading → measured cost = the gl11 micro-stutter. The flag treats the symptom by
serializing the driver; the cure is emitting less of the churn that races it.

## Attack surface (start here)

1. **Census first:** count Map(WRITE_DISCARD)/frame and per-burst (zone-in, cantina entry,
   combat). Direct3d11_Metrics already tracks mapCount (reportFrameStats flag) — extend to split
   DISCARD vs NO_OVERWRITE and attribute by buffer (dynamic VB / dynamic IB / cbuffers).
2. **Known suspects:** the dynamic VB/IB ring ([Direct3d11] dynamicVertexBufferSize=256/
   dynamicIndexBufferSize=64; discardDynamicBuffersAtBeginningOfFrame) — classic fix is a
   properly sized ring with MAP_NO_OVERWRITE within a frame and ONE DISCARD per frame per
   buffer; cbuffer update strategy (per-draw updates → dirty-range batching).
3. **Validation loop:** with churn reduced, flip preventDriverInternalThreading=false and
   soak zone-ins (the crash was intermittent — confidence accrues; the handler now writes a
   proper mdmp on failure). Ship false only after an extended clean soak; keep the config
   fallback to true forever.

## Separate residual noted during triage (do NOT conflate)

First-visit cantina-door pause (~5-6 frames, gone on second visit) = cold-load hitch
(interior asset/shader warm-up), present regardless of the NV flag. Prior art if chased:
the zone-in optim arc (VS bytecode cache, maxInteriorCreatesPerFrame=10, "texture pre-warm
is the remaining lever"). Low priority — bounded, first-visit-only.

## Done when

gl11 runs with driver threading ENABLED (flag false), no micro-stutter, and an extended
zone-in soak shows zero nvwgf2um crashes — closing the smoothness gap with gl05.
