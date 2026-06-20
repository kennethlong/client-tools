---
spike: 005
name: camera-zoom-timescale
type: standard (fix)
validates: "Given the camera zoom-lurch is frame-rate-amplified (recovery uses a fixed per-frame fraction, not dt-scaled), when we time-scale line 602 by elapsedTime, then the door 'snap' reduces and behaves the same across FPS"
verdict: PENDING
related: [004]
tags: [camera, framerate, fix, door-snap]
---

# Spike 005: camera-zoom-timescale (bug 2 fix)

## The fix
`FreeChaseCamera.cpp:602` — the chase-camera zoom RECOVERY used a fixed per-frame fraction
(`ms_cameraZoomSpeed`, default 0.2, ConfigClientGame.cpp:1122), NOT scaled by `elapsedTime`. At modern
high FPS it converges N× faster in wall-clock than the original ~30 FPS design (recovery τ ∝ 1/FPS),
turning the wall-avoidance pop-OUT into a sharp snap when the collision rays flicker at the doorframe.

Changed to time-scaled (mirrors the already-correct `CockpitCamera.cpp:815`):
```cpp
m_currentZoom = linearInterpolate(m_currentZoom, m_zoom, clamp(0.f, elapsedTime * ms_cameraZoomSpeed * 30.0f, 1.f));
```
`*30` preserves the tuned 30-FPS feel (at 30 FPS, weight == ms_cameraZoomSpeed as before). The
protective pull-IN (691/739) stays INSTANT — easing it would clip the near plane through walls.

## Crew (CONSULT-40, 4-AI) — framerate hypothesis CONFIRMED
- **Opus (math):** high FPS amplifies on the temporal axis — τ ∝ 1/FPS, pop-out velocity ∝ FPS
  (~4× sharper 30→120). Amplitude is FPS-independent (set by approach angle = why sharp angles worse).
  Fix = time-scale 602; keep pull-in instant.
- **Cursor (fix):** exact patch = time-scale 602 `* elapsedTime * 30` + ~50 ms hysteresis (flicker).
  Slow pull-in rejected (near-plane wall clip; near plane 0.25 m).
- **Sonnet (systemic):** it's a class — codebase half-converted to dt-scaling. `frameRateLimit` cfg cap
  = cheapest confirm/mitigation (default 0 = uncapped).
- **Codex (scan):** other non-dt-scaled offenders (future sweep): ShipTurretCamera.cpp:181 (0.1),
  FreeCamera.cpp:278+ (0.3), RemoteCreatureController.cpp:64/468 (×0.75 decay), CreatureController.cpp:400
  (slerp 0.25). NON-offenders (already correct): player movement (PlayerCreatureController 903+), most
  FreeChaseCamera (414/501/524/553/640), all CockpitCamera. → the AVATAR is FPS-safe; this is camera-only.

## How to Run / What to Expect
Built x64 Debug → `stage-x64/SwgClient_d.exe` (gl05). Walk the cantina door, sharp + straight angles.
- Expect: the door snap REDUCED and consistent regardless of FPS (the pop-out is now a steady ~150 ms
  ease at any framerate instead of a high-FPS snap).
- If residual flicker remains (rapid hit/miss at sharp angles): add the ~50 ms hysteresis (spike 006).
- Alternative empirical confirm: set `[SharedFoundation] frameRateLimit=30` in client_d.cfg (reversible,
  BOM-safe via Edit) and compare capped vs uncapped — capped should already feel like the original.

## Results
PENDING — awaiting feel-test (door snap reduced? consistent across FPS?).
