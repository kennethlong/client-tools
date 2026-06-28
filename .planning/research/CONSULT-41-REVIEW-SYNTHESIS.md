# CONSULT-41 SYNTHESIS — code review of the door-snap fix (4 reviewers)

## Camera changes B & C — SETTLED (clear fixes)
- **B (pull-in rate limit): REAL BUG (Cursor).** `zoomAtFrameStart` captured BEFORE the recovery lerp
  → the clamp caps ALL third-person zoom-in to 8 m/s, not just collision pull-in. Manual scroll-zoom-in
  feels sluggish. **Fix:** capture baseline AFTER the lerp (`zoomBeforeCollision = m_currentZoom` right
  before the collision block) → only the collision drop is rate-limited.
- **C (shoulder ease): minor (Cursor/Sonnet).** The pre-existing `move_o(-m_offset.x*elapsedTime)` now
  double-applies a tiny extra pull each ease frame (was 0 with the instant zero). **Fix:** drop that
  `move_o` line (it was coupled to the instant zero). Steady-state ~4 cm offset while colliding is the
  accepted wall-peek trade-off.
- Optional: cap `maxPullIn`/alpha on elapsedTime spikes (hitch frames bypass the limit) — low priority.

## Change A (avatar) — 3-WAY SPLIT on safety
The bare gate is `if(centerWalkResult == PWR_WalkOk) continue;`.
- **Opus:** ship, preferably + shallow-graze gate. Clip-through into forbidden space is **unreachable**
  (WalkOk certifies crossable-connected walkable floor end-to-end, so the grazed uncrossable edge is
  LATERAL, not an athwart gate). Real cost = loss of legitimate lateral wall-slide (body shoulder can
  overlap a real wall ~0.5 m on glancing approach, self-correcting, cosmetic). The diff's stated reason
  ("a real wall blocks the center too") is FALSE — fix the comment.
- **Sonnet:** ship with caution. Ledge walk-off RULED OUT (center exits → not WalkOk). Portals RULED
  OUT. Residual = narrow POB doorways/pillars < 1 m (shoulder overlaps jamb). Shallow-graze gate helps.
- **Codex: NO-SHIP as-is.** center-walkable ≠ radius-walkable. Affects ALL footprinted movers (NPCs,
  pets, mounts, scaled creatures — CollisionProperty.cpp:270/290, not player-gated). Downstream relies
  on these contacts (slide history, DynamicPathGraph:281, FloorMesh::testConnectable:3333). Wants a
  SEAM-PROOF gate: also require the edge to have a matching adjacent portal/floor-seam, else keep the
  contact. NB Codex's own note: REAL rendered walls have solid extents caught separately
  (canMoveInCell:1621) — only FLOOR-ONLY clearance boundaries are at risk.

### Reconciliation
The reviewers converge: the **bare WalkOk gate is too broad** (Codex NO-SHIP; Opus/Sonnet flag
wall-slide loss / narrow doorways). The clip-through-into-forbidden-space fear is mostly unreachable
(real walls are solid-backed; ledges aren't WalkOk), but suppressing ALL WalkOk circle contacts loses
legitimate lateral blocking and overlaps narrow floor-only boundaries.

**Two tightening options:**
1. **Shallow-graze gate (Opus §3):** also require penetration `r - |contactNormal| < ~5 cm`. Flat
   floor seam (cantina, center on walkable side) = sub-cm graze → suppressed (door fixed). Narrow gap /
   wall-slide = deeper penetration → KEPT (blocked). Simple (compute contactNormal earlier + one check).
   RISK: threshold must exceed the cantina seam's actual penetration (not measured directly) — verify
   in-game that the door is still fixed at the chosen epsilon.
2. **Seam-proof gate (Codex):** verify the edge has a matching adjacent portal/floor-seam on the other
   side before suppressing. Principled, no threshold guessing, but needs a new geometric adjacency
   predicate — more code + risk.

### Empirical tiebreaker
The crux (does the body clip narrow POB doorways/pillars?) is exactly the in-game probe the user is
running now. Their result decides urgency and whether option 1's epsilon needs tuning.

## Recommended consolidated set
- B fix (post-lerp baseline) + C cleanup (drop redundant move_o) — apply.
- A: add the shallow-graze gate (option 1) + fix the misleading comment — pragmatic 80/20 that
  satisfies Codex's "deep clearance boundaries keep blocking" while keeping the door fix. Verify in-game
  the door is still smooth AND narrow gaps/pillars still block at the chosen epsilon. Fall back to
  Codex's seam-proof gate only if epsilon can't separate the two.
