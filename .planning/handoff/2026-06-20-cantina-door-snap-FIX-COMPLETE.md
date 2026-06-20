# Cantina door-snap — FIX COMPLETE (2026-06-20)

**Status:** root-caused, fixed, code-reviewed (4-AI), built clean in all 4 configs, user-verified.
**Supersedes:** `2026-06-20-cantina-door-snap-spike-START.md` (the START handoff is now historical).
**Commit:** `3549c7104` (master, not pushed) — the 4 source files only. Planning docs committed
separately. 4 staged source files: FloorMesh.cpp, FreeChaseCamera.cpp, CollisionResolve.cpp,
CellProperty.cpp.

---

## TL;DR
The "cantina door-snap" was **TWO stacked bugs in two different subsystems**, both root-caused by
runtime probes (not analysis), then fixed:

1. **Avatar floor collision (the ~75% rubberband).** The footprint **circle** (r=0.5) grazed an
   uncrossable floor-mesh boundary edge while the circle **center** walked the triangle cleanly
   (`PWR_WalkOk`), generating a spurious blocking contact → `CollisionResolve` rewind → cross-axis
   pin/rubberband. The resolver's slide was verified CORRECT (`:452` dot-test bail 0/160; tangential
   motion preserved exactly) — the **contact itself was spurious.**
2. **Chase camera (the residual).** Several **instant assignments** in `FreeChaseCamera::alter`
   snapped: a frame-rate-dependent zoom recovery, the wall-avoidance **zoom pull-in** (measured −2.1 m
   in ONE frame), and the shoulder-clip **`m_offset.x` zeroing** (0.4 m sideways jump). View-only;
   first-person was always clean (the collision rays are skipped there).

## The landed fix (3 changes, 4 source files, ~50 net lines — no probes)
- **A — `FloorMesh.cpp` `pathWalkCircleGetIds`:** suppress the circle contact when
  `centerWalk == PWR_WalkOk` **AND** the graze is shallow (`penetration < cs_seamGrazeEpsilon`, 5 cm).
  Measured: 1041 cantina seam grazes all ~0 m penetration → suppressed; real walls / narrow gaps /
  pillars (deeper) stay **blocked** (this gate satisfied the code-review NO-SHIP concern).
- **B — `FreeChaseCamera.cpp`:** rate-limit the inward zoom pull-in (`cs_cameraPullInSpeed`, 8 m/s),
  baseline captured AFTER the recovery lerp so manual scroll-zoom-in is unaffected.
- **C — `FreeChaseCamera.cpp`:** ease the shoulder offset to 0 (same rate) instead of an instant zero;
  dropped the now-redundant per-frame `move_o`.
- **Cleanup:** removed ALL `CORNERSNAP-*` `_DEBUG` probes (this session's + the prior acceptance
  harness in `CollisionResolve.cpp` / `CellProperty.cpp`) and their dead `Os.h` includes.
  `git grep CORNERSNAP` is now empty.

## Why the reference clients are clean (settled)
Restoration **and** SWGEmu both run **uncapped** (no `[SharedFoundation] frameRateLimit`) yet don't
snap — so FPS-capping is NOT their trick. They run the **retail binary**, whose camera already eases
these; our leaked-source copy had the instant snaps. The framerate angle was real for **bug 1**
(avatar collision scaled with per-frame step), a partial detour for the camera (the real camera fix
was rate-limiting the *instant*, FPS-independent pull-in). The narrow exhaust-pipe run-through is
**reference-correct** (SWGEmu does it too — it's solid/terrain collision, not our floor-mesh change),
and our camera there is now BETTER than SWGEmu (no jump).

## Verification
- Built clean (0 unresolved) in **all 4 configs**: Win32/x64 × Debug/Release, all gl05, BOM-safe.
- User-walked the cantina door (rubberband gone, camera smooth), scroll-zoom (normal), narrow features
  (still blocked), across configs.

## Tunables introduced (compile-time constants, NOT .cfg)
- `cs_seamGrazeEpsilon` = 0.05 (`FloorMesh.cpp:73`) — Change A shallow-graze depth.
- `cs_cameraPullInSpeed` = 8.0 m/s (`FreeChaseCamera.cpp:107`) — Changes B+C inward ease rate.
- Full reference (what each does, how to tune, interacting pre-existing values like
  `freeChaseCameraZoomSpeed` / `wallEpsilon` / footprint radius / `frameRateLimit`):
  **`.planning/spikes/door-snap-fix-tunables.md`**.

## Remaining (not blocking)
- **Commit** the fix (4 files) when ready — user controls commits.
- **Systemic non-dt-scaled-constant sweep** (CONSULT-40 / Codex): `ShipTurretCamera.cpp:181`,
  `FreeCamera.cpp:278+`, `RemoteCreatureController.cpp:64/468`, `CreatureController.cpp:400`. Player
  movement is already time-scaled (FPS-safe). A clean future backlog item.
- The straight-on cantina residual matches SWGEmu (reference-correct) — not a bug.
- **Docs migration (at final wrap-up of all changes):** promote the few stable, repo-worthy reference
  docs out of `.planning/` into a top-level `docs/` folder and link them from the public `README.md`
  then (candidate: `door-snap-fix-tunables.md`). The investigation/consult/spike docs stay in
  `.planning/`. Deliberately NOT touching the upstream-tracked root README until then.

## Docs created this investigation (door-snap)
- `.planning/spikes/door-snap-fix-tunables.md` — the tunable config-value reference.
- `.planning/spikes/MANIFEST.md` — spike index + the two-bug outcome.
- `.planning/spikes/001..006/README.md` — the spike chain (mechanism probe → resolution probe →
  faceB-fix → camera-zoom-lurch → time-scale → hysteresis).
- `.planning/research/CONSULT-37/38/39 *-SYNTHESIS.md` — root-cause + fix-selection consult rounds.
- `.planning/research/CONSULT-40-camera-lurch-framerate-EVIDENCE.md` (+ the systemic scan) — framerate.
- `.planning/research/CONSULT-41-REVIEW-SYNTHESIS.md` + `CONSULT-41-doorsnap-fix.diff` — the 4-AI code
  review of the final fix.
- `.planning/spikes/_diff-backup/` — backup patches of the trimmed 005/006 (recoverable if needed).
