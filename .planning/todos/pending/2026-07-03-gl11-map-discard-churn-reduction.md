---
created: 2026-07-03
title: gl11 Map(WRITE_DISCARD) churn reduction — re-enable NV driver threading safely
area: client graphics / Direct3d11 / performance
status: MOSTLY DELIVERED 2026-07-03 evening (CONSULT-58) — the churn root cause was the VB ring
  advancing its cursor by the locked UPPER BOUND instead of the actual count at unlock (D3D9
  parity fix, d09a62198): 19x hitch-rate collapse, gl11 median 9.6ms with driver threading
  re-enabled, no crash in 9.5 min. REMAINING before closing: (1) multi-session flag-off soak with
  zone-ins → then flip the code default of preventDriverInternalThreading to false; (2) convict
  the residual cold-load pauses via the creation-census columns (735cea241) from organic play;
  (3) optional margin: vsB0 NO_OVERWRITE cbuffer ring (D3D11.1 feature probe needed).
soak-log:
  - session 1 (2026-07-03 ~9:45pm, 9.5 min): median 9.6ms, 0.6% >20ms, no crash (committed evidence).
  - session 2 (2026-07-03 ~10:07pm, 3.3 min): median 12.1ms steady-state, 8.7% >20ms but nearly all
    20-25ms missed-vsync variance in a heavier scene (38-54 draws) — no hang-class stutter; INCLUDED
    a zone-in (2.6s load frame ~3896) with NO nvwgf2um crash. Kenny verdict: feels better than gl05.
  - COLD-LOAD CONVICTION (item 2 = DONE, verdict negative): the >100ms stalls carry ZERO in-frame
    creates (688ms mid-play frame: 37 draws same as neighbors, 0 creates; creates trail the 2.6s
    zone stall as arriving assets, 3 shaderCreates ≠ 258ms). NOT D3D resource creation → pre-warm/
    off-thread-creation is the wrong lever. Stall is upstream main-thread load work (TreeFile/disk/
    decompress/world-build) — converges with the audio-pops todo's TreeFile-contention suspect.
    Next tool: >100ms frame watchdog that samples the main-thread stack, not more D3D census.
  - STALL WATCHDOG BUILT (2026-07-03 late, uncommitted): file-local namespace in clientGame
    Game.cpp + heartbeat at top of runGameLoopOnce. [ClientGame] stallWatchdogMs=100 (0=off) /
    stallWatchdogMaxDumps=6 -> whole-process MiniDumpNormal (all threads, incl. audio) as
    stage/stall-loop<N>-s<K>.mdmp + stall-watchdog.log (per-stall total-duration lines even
    past the dump budget; unfocused stalls log-only). Second sample at 5x threshold. Loads
    SYSTEM dbghelp.dll so DebugHelp's single-use OOM crash reserve stays armed. Verified
    end-to-end at threshold=1: 6 dumps, budget cap, MDMP magic, cdb+our-PDBs symbolize
    (caught Clock::limitFrameRate/Sleep as expected). cfg armed at 100ms in stage/client.cfg.
    Next: organic play with zone-ins + cantina cold-entries -> symbolize the real stall dumps.
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
