# CONSULT-38 SYNTHESIS — door-snap face B: the fix (round-2 crew, post spike-001 capture)

Date 2026-06-20. Evidence: CONSULT-38-doorsnap-resolution-EVIDENCE.md (+ ADDENDUM). Crew: Codex
(resolution trace), Cursor (slide quality), Opus (detection-vs-resolution math), Sonnet (step
subdivision). Spike-001 capture = `.planning/spikes/001-mechanism-probe/capture-0620.log`.

## AGREED MECHANISM (all four + runtime data)
Walking, the avatar footprint **center walks cleanly (`PWR_WalkOk`)** but the offset **circle (r=0.5)
grazes an uncrossable mesh-boundary edge** (`portalId=-1`, `neighbor=-1`, `FET_Uncrossable`). The
graze is recorded as a HARD contact on the unguarded FWD branch (FloorMesh.cpp:2911-2916). The move is
truncated at `circleHitTime`; the tangential remainder `step*(1-min)` is **lost** → per-frame backward
clip vs the intended endpoint = the visible stutter. PERVASIVE (136/161 not at a doorway). Magnitude
scales with `step` → worse at low FPS (Debug/gl05), better at high FPS (Release/gl11), absent in retail
(small steps, same assets). `SingleSlide` math itself is CORRECT (Codex) — the slide is either not
reached or discarded. `centerWalkResult` is available at detection but COLLAPSED to `PWR_HitEdge`
before resolution (Codex) — so only a detection-site fix can condition on "center cleared".

## THE THREE-WAY SPLIT ON FIX LOCUS
| # | Locus | Who | Mechanism of fix | Blast / risk |
|---|-------|-----|------------------|--------------|
| **F1 Detection gate** | `FloorMesh.cpp:2911-2916` (FWD record), gate on `centerWalk==PWR_WalkOk` | Cursor, Codex | don't emit a hard block for a center-OK circle-only graze; slide/skip instead | precise (centerWalk available), narrow. RISK: naive *suppress* ≠ *slide* → shallow wall-clip if avatar angles INTO a wall (center not yet hit). Must produce a tangential slide, not just delete the contact. |
| **F2 Resolution slide-drop** | `CollisionResolve.cpp:448-454` dot-test early-out | Opus | stop discarding the tangential slide (`slidDelta·localStartDelta<=0` bails before pushing the slid waypoint) | keeps blocking guaranteed (no clip). RISK: cannot condition on center-OK (info gone) → changes slide response for ALL contacts/extents/obstacles/multi-contact. Global. |
| **F3 Sub-step the delta** | `findContactWithFloor` / before `floor->canMove()` — cap per-call delta to ~0.1-0.2 m, subdivide | Sonnet | make large low-FPS steps behave like retail's small steps (the engine ALREADY sub-steps >2 m at CollisionWorld.cpp:639 — copy that pattern finer) | root-cause + retail-parity, reuses a PROVEN pattern, same semantics finer granularity → low semantic blast. RISK: perf (more canMove iterations/frame), and must re-scale `circleHitTime` from sub-step [0,1] to full-frame [0,1]. Mitigates magnitude; may not fully eliminate grazes (still slides along walls — which is correct). |

## CONVERGED DISCRIMINATING PROBE (Opus+Cursor+Codex all proposed it) — settles F1 vs F2
Instrument `CollisionResolve::resolveCollisions` (~:446-463), for each `RR_Resolved` from a floor
circle-contact: log `contactCount`, `contact.m_time`, `slidDelta`, **`slidDelta.dot(localStartDelta)`**,
**whether the `:452` early-out fired**, and **final pushed waypoint vs contactPoint**.
- `:452` fires + only contactPoint pushed → the slide IS being discarded (Opus right) → resolution is
  implicated; a detection gate alone may not suffice.
- `:452` doesn't fire (slide runs) but the contact shouldn't exist → pure detection (Cursor/Codex).

## FALSIFIED THIS ROUND
- Sonnet Reframe-2 (min<0.005 "already-touching" = distinct large-step bug): NO — same step
  distribution as grazing (0.131 vs 0.151). One continuous population.
- (Still BANNED from CONSULT-37/38: portalId fixes, doorway-locality, CONSULT-36 reverted trio.)

## RECOMMENDATION (for spike 002)
Two viable paths, not mutually exclusive:
- **Lowest-risk root fix = F3 sub-step.** Most retail-parity, reuses the proven >2m subdivision,
  doesn't touch detection/resolution semantics. Strong candidate to PROTOTYPE-and-measure directly.
- **If F3 only mitigates (doesn't eliminate), add F1 detection gate** (convert center-OK graze to a
  slide). F2 (global resolution change) is the fallback of last resort given its blast radius.
- The discriminating probe is cheap and de-risks F1-vs-F2 if F3 proves insufficient.
