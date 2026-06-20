---
spike: 002
name: resolution-probe
type: standard
validates: "Given a cantina door cross, when we log the :452 slide-discard decision and the NET intended-vs-actual displacement in CollisionResolve, then we learn whether the tangential slide is discarded (→ resolution fix at :452/:320) or runs but still loses motion (→ detection guard) — settling the face-B fix locus"
verdict: VALIDATED
related: [001]
tags: [collision, instrument, resolution, door-snap]
---

# Spike 002: resolution-probe

## What This Validates
Spike 001 proved face B = footprint circle grazing a non-portal uncrossable boundary edge while the
center walks OK, causing a RESOLVE rewind. CONSULT-38/39 (4-AI x2) split on the FIX: detection guard
(F1/Variant A) vs resolution slide-fix (F2, `:452`/`:320`) — and DEMOTED F3 (sub-step) as a
slice-not-eliminate knob. This probe captures the **resolution behavior** to decide:
- Does the `:452` dot-test (`slidDelta·localStartDelta <= 0`) **DISCARD** the tangential slide for
  these center-OK floor grazes? (Opus's resolution hypothesis.)
- What is the **NET** displacement — intended frame endpoint vs actual final position after the full
  rewind + slide replay? (The FELT stutter; NOT the rewind magnitude, which is ~full step by design.)

## The probe (in-tree `_DEBUG`, throwaway)
`CollisionResolve.cpp`:
- **`CORNERSNAP-SLIDE`** at the `:452` contactCount==1 slide path — logs `isFloor` (m_surface!=NULL),
  `contact.m_time`, `|slidDelta|`, `|remaining delta|`, `|startDelta|`, the `dot`, and **`bail`**
  (1 = slide discarded, move ends at truncated contact point).
- **`CORNERSNAP-NET`** after `moveObjectAlong` — logs `intended` (`moveSeg.getEnd(m_cellB)`),
  `final` (actual post-replay position), and **`errXZ`** = the net horizontal error (the stutter).
- Existing `CORNERSNAP-CIRCLE/RESOLVE/PORTAL` probes remain.

## How to Run
Build (done): `sharedCollision.vcxproj` then `SwgClient.vcxproj`, x64 Debug → `stage-x64/SwgClient_d.exe`.
1. Start monitor: `powershell -File tools/setup/dbwin-cornersnap-capture.ps1`
2. Launch `stage-x64/SwgClient_d.exe` (rasterMajor=5), cantina, **walk the main door back and forth**
   (slow + fast), trigger the stutter several times.
3. Logs → `stage-x64/cornersnap-x64-capture.log`.

## What to Expect / decode
- `CORNERSNAP-SLIDE ... isFloor 1 ... bail 1` on stutter frames → slide IS discarded → **resolution
  fix** (relax `:452` / fix `:320` rewind). `bail 0` with large `errXZ` → slide runs but motion still
  lost → deeper look / detection guard.
- `CORNERSNAP-NET errXZ`: if ≈ the into-wall component only → correct collision, "stutter" is the
  rewind-replay visual (`:320`). If ≈ full step → slide failed to recover tangent.

## Investigation Trail
- 2026-06-20: built after CONSULT-39 demoted F3 and own analysis found Variant A has a fast-approach
  clip hole — so probe the resolution path directly before committing to a fix. Capture pending (user pilots).

## Results — VALIDATED (capture: 160 SLIDE, 160 NET, 190 CIRCLE)

### Resolution is mechanically CORRECT — slide-discard FALSIFIED
- **`:452` dot-test bail = 0 / 160.** The tangential slide is NEVER discarded. Opus's resolution
  hypothesis (F2-at-:452) is FALSIFIED.
- **Slide recovers 80% of remaining motion** (mean slidMag/remMag = 0.800) — removes only the ~20%
  into-wall normal component (geometrically what a slide must do).
- **Forward (tangential) motion preserved EXACTLY** — dX = 0.0000 on every pinned frame (4180-4187).

### The bug is the CONTACT itself (cross-axis seam pin)
- Net error is purely cross-axis: the player is pinned to a contact line (e.g. Z=2.551) while X
  advances freely. errXZ: 40 imperceptible (<0.01), 69 small, 46 felt (0.05-0.15), 5 hard (>0.15).
- **The "rubberbanded hard" (user-reported) = the errXZ 0.15-0.30 frames.** Frame 4185: intended
  Z=2.252, yanked to Z=2.551 = a **0.30 m snap to the seam line**. Same across multiple seams/objs.
- Mechanism: footprint CIRCLE catches an uncrossable floor-mesh boundary edge **while the CENTER walks
  cleanly across it** (`PWR_WalkOk`, from spike 001). The slide pins the cross-axis to that seam; when
  input crosses the seam, the player is yanked back = rubberband.

### Adjudicates the CONSULT-38/39 crew split
- **Resolution (Opus, F2/:452): DEAD** (bail=0).
- **Detection gate (Cursor/Codex, F1/Variant A): CONFIRMED + now well-justified.** The center walking
  across the edge PROVES it's walkable → the circle contact is spurious. A real wall blocks the center
  too (`centerWalk != WalkOk`), so gating on `WalkOk` removes the seam-pin without weakening real walls.
- Residual risk to test in spike 003: a thin wall/narrow gap the center's point threads but the 0.5 m
  body shouldn't — there suppression would clip. The fix-pilot must verify thin walls still block.

### Fix (spike 003): in the circle sweep, when `centerWalkResult == PWR_WalkOk`, do not record a hard
blocking contact from a circle graze of an uncrossable boundary edge.
