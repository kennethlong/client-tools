# 36-01 SUMMARY — VERIFY-01 / VERIFY-02 outcome (2026-06-19/20)

Status: **Verification done. VERIFY-02 PASS · VERIFY-01 FAIL (door-snap real) → root-caused → PARKED
as a pre-existing engine quirk by user decision. Milestone call made.**

## VERIFY-02 — extended x64 session, no MemoryManager OOM → **PASS**
Live x64 Debug session (`stage-x64/SwgClient_d.exe`, rasterMajor=5). Memory sampled via
`Get-Process PrivateMemorySize64`: baseline 1819.4 MB → after door cycling 1857.9 MB (+38.5 MB),
handles 844 → 847 (+3, flat), working set +30.8 MB. Decelerating floor, no 2 GB ceiling (x64), no
`b0780503`-class OOM FATAL. Matches the documented x64 no-leak profile. **Confirmed.**

## VERIFY-01 — cantina door-snap resolved in x64 → **FAIL** (then root-caused + parked)
User in-world observation (ground truth, AGENTS.md): door-snap **still occurs in the x64 build under
rasterMajor=5 (gl05/D3D9)** — ~40% snapping + rubberbanding, later ~75% of crossings as a subtle
horizontal "shift left/right like a frame was skipped." **This FALSIFIES the milestone hypothesis that
x64 (deterministic SSE) or D3D11 would fix it** — the snap is bitness- AND renderer-independent.

### Root cause (fully diagnosed — live DBWIN capture + 4-AI crew + 2 instrumentation probes)
The door snapping is the **floor-collision walk reacting to the cantina doorway's FLOOR-MESH SEAM**,
NOT to any wall (user confirmed never touching the door frame/walls). The footprint *center* threads
the portal cleanly, but the footprint *circle* (~0.5 m radius) clips an **uncrossable** seam edge →
the walk returns `PWR_HitEdge` instead of `PWR_HitPortalEdge` → `RR_Resolved` → rewind to
`moveSeg.getBegin(m_cellB)` (the stale prior position). Two faces of one cause:
- **A — rare ~4 m VERTICAL pop:** cross-cell, the stale foyer floor (world-Y 5.10 vs terrain 1.06).
  Caught red-handed at frame 2850: `floorCell 'foyer1' objCell 'world' walk 6 (PWR_HitEdge) contact 1`,
  `RESOLVE rewind Y 1.063→5.100`.
- **B — frequent ~0.07–0.14 m HORIZONTAL stutter (the ~75% complaint):** same-cell, the footprint
  circle grazing seam edges across 6–10 consecutive frames in the tight foyer.

Intermittency = whether the seam edge returns `PWR_HitEdge` (6, snaps) vs `PWR_HitPortalEdge` (7, clean)
for the footprint circle that frame. Full analysis: `.planning/research/CONSULT-36-doorsnap-SYNTHESIS.md`
(+ `-EVIDENCE-doorsnap.md`, crew `CONSULT-36-{codex,cursor}.out`); method/decisions in memory
`project_cantina_corner_snap_engine_quirk`.

### Fixes considered (no clean single fix — PARKED by user decision 2026-06-20)
- Gate band-aid (skip floor contacts whose `getCell()` != current parentCell) — kills only the rare
  vertical pop A; does NOT touch the common horizontal stutter B (same-cell). Not applied.
- Proper fix (treat portal-adjacent uncrossable seam edges as passable for the footprint circle when
  the center crosses) — high-risk core floor-collision surgery affecting ALL movement; this bug has
  already eaten 3 reverted fixes. Not attempted.
- **Decision: PARK.** Investigation probes reverted (`git checkout` of CollisionResolve.cpp +
  CollisionWorld.cpp); original CORNERSNAP-PORTAL/RESOLVE/CELLJUMP harness retained.

## Milestone call
The door-snap is a **pre-existing engine quirk, orthogonal to the v3.0 x64 milestone** (bitness/
renderer/x64-independent; not a regression; not caused by the x64 work). The x64 milestone's real
deliverables — x64 build, dual renderers (gl05+gl11), Miles 9.3v audio, no 32-bit OOM — all stand.
- **VERIFY-02:** PASS.
- **VERIFY-01:** not met, **reclassified out-of-scope-for-x64** and parked as a documented known issue
  (door-snap fix = future dedicated effort, guided by this SUMMARY + the SYNTHESIS doc).
- **VERIFY-03 / plan 36-02 (strip CORNERSNAP probes):** **DEFERRED** — the probes remain the acceptance
  harness for the future fix; do NOT strip them while the snap is unfixed.
