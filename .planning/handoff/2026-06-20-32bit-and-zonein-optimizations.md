# 32-bit fixes + cantina zone-in load optimizations — 2026-06-20

**Status:** DONE + pushed to `origin/master`. Started as "work the 32-bit memory leak"; the leak was
already fixed (Phase-32 bytecode cache, re-measured flat) — the real issues were three others, all now
fixed, plus a clean set of zone-in load-smoothness optimizations.

## Landed (all on origin/master)
| Commit | What |
| --- | --- |
| `db040db29` | **D3DCREATE_FPU_PRESERVE** — gl05/D3D9 32-bit cantina see-through-wall (x87 single-precision degraded the shared portal-visibility math). Localized by a renderer×bitness matrix at the spot (gl05-32bit only). Memory: `project_d3d9_32bit_fpu_preserve_cantina_seethrough`. |
| `b4749ea9a` | **/LARGEADDRESSAWARE** (SwgClient Win32, all 3 configs) — 32-bit Debug client was locking up on world load at the 2 GB address-space wall (fragmentation; it only peaks ~1.5 GB). Now runnable. Memory: `project_32bit_largeaddressaware_debug_runnable`. |
| `16f4e58ec` | **#1 disk VS bytecode cache** (`Direct3d9_ShaderCache`, port of gl11) — kills the first-load runtime shader-compile stall (the big cliff). |
| `272906c2b` | **#2 interior-layout throttle** — spreads the ~84-object cantina create burst; safe-by-construction (resume count on `CellProperty`, reset in `removeFromWorld`). |
| `b4860e0bc` | **config template** — persists #4 (`[SharedFoundation] minFrameRate=10`, the elapsedTime clamp) + #2 (`maxInteriorCreatesPerFrame=10`). |

\#2 built into **all 4 configs** (Win32/x64 × Debug/Release) — `CellProperty.h` is a shared header, so
all 4 plugins (gl05/06/07/11) + SwgClient were rebuilt per platform (force-delete plugin DLLs to relink
against the recompiled `clientGraphics.lib`).

**Result (user-validated):** cantina zone-in shader-compile gone; the load stutter is now "a few
slow-motion frames" not a snap; the prop burst is spread; props correct (no dup/miss), fast in-out clean.

## Method
Two cross-AI consults: **CONSULT-45** (causes of the jerk) + **CONSULT-46** (the throttle's
risk/value/design). Synthesis docs: `.planning/research/CONSULT-45-SYNTHESIS.md`,
`CONSULT-46-SYNTHESIS.md` (+ per-consultant `-{codex,cursor,sonnet,opus}.out`). CONSULT-46 had a real
Sonnet-vs-Cursor divergence (do/don't), resolved by a direct code check (texture loads in its ctor,
on the creation path → throttling creation spreads the GPU cost). Full detail in memory
`project_zonein_load_jerk_optimizations`. Tool/probe: `tools/setup/mem-sampler.ps1`.

## OPEN follow-ups (none blocking)
1. **Back-room chase-camera clip** — separate, pre-existing (NOT from this work: the throttle never
   touches the camera and the clamp is inactive at normal framerate). In tight/tall interiors the chase
   camera gets behind near geometry / near-plane clips (screenshots `stage/screenshots/screenShot0392,
   0394,0395.jpg`). Likely the door-snap fix's rate-limited pull-in (`cs_cameraPullInSpeed`, `3549c7104`
   — its comment documents "may show the near plane through a wall while easing in"). Candidate fix:
   tune `cs_cameraPullInSpeed` or add a "snap in fast when deeply clipped into geometry" exception.
2. **`_ITERATOR_DEBUG_LEVEL=2`→1** — the real remaining Debug-only perf lever, but a cross-cutting ABI
   change across ~50 projects (must match everywhere); deferred as its own decision.
3. **Texture pre-warm during zone transition** — the residual first-entry cost is device-affine texture
   upload (can't move off the render thread); pre-warm is the lever if a fully-flat first entry is wanted.

## Tuning knobs (local stage/*.cfg now have them; template carries the defaults)
- `[SharedFoundation] minFrameRate=10` — higher = tighter/smoother clamp but more time-dilation on stalls.
- `[ClientGame/ClientInteriorLayoutManager] maxInteriorCreatesPerFrame=10` — 0=off (original); higher =
  less pop-in, more per-frame burst.
