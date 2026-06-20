---
spike: 006
name: camera-hysteresis
type: standard (fix, builds on 005)
validates: "Given residual door snap after the time-scale fix (the hit/miss ray flicker at sharp angles), when we hold the wall-collision zoom clamp ~50ms after the last hit, then the flicker pop-out is suppressed and the snap is gone"
verdict: PENDING
related: [004, 005]
tags: [camera, fix, hysteresis, door-snap]
---

# Spike 006: camera-hysteresis (bug 2, increment 2)

## Why
Spike 005 (time-scale recovery) removed the FRAME-RATE amplification but the user still felt residual
snapping — the hit/miss flicker of the 4-ray diamond grazing the doorframe at sharp angles. Each
single-frame ray MISS let the (now time-scaled) recovery start popping the camera back out before the
next hit re-clamped it. Cursor's CONSULT-40 patch: add a short hysteresis HOLD.

## The fix (FreeChaseCamera.cpp + .h)
- New members `m_wallClampZoom` (< 0 = inactive), `m_wallCollisionHoldTimer`; anon-namespace
  `cs_wallCollisionHoldSeconds = 0.05f`.
- Function-scope `bool hit` (was diamond-local) so both collision branches + the hold block share it.
- On a ray hit (simple :698 and diamond :749 branches): keep the INSTANT pull-in but via
  `m_currentZoom = std::min(m_currentZoom, collisionZoom)` (wall-safe), AND arm
  `m_wallClampZoom = collisionZoom` + refresh the hold timer.
- After the collision branches: while `m_wallClampZoom >= 0`, pin `m_currentZoom = min(currentZoom,
  wallClamp)` every frame; refresh timer on hit, decrement by `elapsedTime` on miss, release the clamp
  when it expires. So a sub-50ms ray miss can't pop the camera out.
- Pull-IN stays instant (protective, wall-safe). Time-scaled recovery (spike 005) still runs and
  resumes once the hold releases.

## How to Run / Expect
Built x64 Debug → `stage-x64/SwgClient_d.exe` (gl05). Walk the cantina door, **sharp angles especially**.
- Expect: the residual flicker/snap GONE; smooth crossing at any angle and FPS.
- Watch for over-stick: camera staying pulled-in too long after clearing the doorframe (tune
  `cs_wallCollisionHoldSeconds` 0.05→0.10 if too brief, lower if it feels sticky).
- First-person should still be snap-free (collision block skipped).

## Results
PENDING — awaiting feel-test.
