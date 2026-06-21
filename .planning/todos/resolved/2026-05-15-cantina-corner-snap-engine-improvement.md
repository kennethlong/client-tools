---
created: 2026-05-15
updated: 2026-06-13
title: Cantina interior corner-snap — cell-ping-pong premise FALSIFIED 2026-06-13; real target is the CollisionResolve position rewind
area: rendering / collision response / portal traversal
resolves_phase: 36
next_action: re-root-cause against the CollisionResolve position rewind (setPosition_p), NOT the cell ping-pong. The cell-reversal-guard approach was built, runtime-tested (gl05), and reverted (see 2026-06-13 section). Likely /gsd-debug + consult crew.
files:
  - src/engine/shared/library/sharedObject/src/shared/portal/CellProperty.cpp  (Notification::positionChanged ~line 115; CORNERSNAP-PORTAL instrumentation at the targetCell branch)
  - src/engine/shared/library/sharedCollision/src/shared/core/CollisionResolve.cpp  (resolveCollisions rewind/replay ~line 313; CORNERSNAP-RESOLVE/CELLJUMP instrumentation)
  - src/engine/shared/library/sharedObject/src/shared/object/Object.cpp  (setParentCell, line ~1387)
references:
  - .planning/debug/cantina-corner-snap.md  (closed debug session, cycles 1-5)
  - .planning/todos/pending/2026-05-14-safecast-null-dynamic-cast-world-load.md  (orthogonal; cross-linked because the v1 GOOD-reference revival attempt during Cycle 4 hit a related but unrelated SafeCast issue)
status: known_engine_behavior
priority: low (annoying but not blocking; workaround available)
---

## 2026-06-13 — cell-ping-pong premise FALSIFIED by runtime capture; guard reverted

Phase 25 plan 25-01 implemented the re-entrancy guard the 2026-06-12 section
proposed (frame-scoped reversal suppressor in `positionChanged`, keyed on
`Object const *`, comparing `CellProperty const *` from/to pointers, with a new
APPLIED/SUPPRESSED `_DEBUG` counter). It was built (Debug+Release, 0 unresolved),
runtime-tested under gl05/D3D9, and **REVERTED** (`a6df32348`, reverting
`7820aea50`). The original CORNERSNAP instrumentation (`a9b419daf`) is untouched.

**What the gl05 capture proved (evidence:
`stage/cornersnap-capture/EVIDENCE-25-01-gl05-guard-regression-20260613.log`):**

1. The guard's premise is WRONG. The "reversed" second portal segment is the
   LEGITIMATE collision correction, not a spurious ping-pong. Raw interleaving,
   player obj 127040355, frame 4250:
     `PORTAL foyer1(1)->foyer2(2)  z -4844.290 -> -4844.121  APPLIED 1->2`   (overshoot)
     `PORTAL foyer2(2)->foyer1(1)  z -4844.121 -> -4844.280  SUPPRESSED 2->1` (correction)
   The FINAL position (z -4844.280) is on the foyer1 side, but the guard pinned
   membership in foyer2 (the overshoot). Cell/position desync.

2. That desync IS the visible breakage: the portal renderer culls from the wrong
   cell → cantina interior drops to skybox (Kenny: "looking toward the bar, saw
   sky"; "popped back after a second" = a later clean frame re-applied the
   correcting transition and re-synced membership). It also produced "snapping
   almost every time I left the cantina."

3. Without the guard, BOTH segments apply and membership tracks position — the
   cell ping-pong is SELF-CORRECTING. Suppressing any segment is harmful.

4. The actual VISIBLE SNAP is a POSITION rewind in CollisionResolve, a different
   code path the cell guard never touches: capture frame 7593, player rewind of
   **4.04m vertical** `(3468.364,1.063,-4850.307)->(3468.286,5.100,-4850.180)`
   via `setPosition_p`. CORNERSNAP-CELLJUMP was 0 — confirming net cell change is
   fine; the snap is positional.

**Corrected next-step:** re-diagnose against `CollisionResolve.cpp` rewind/replay
(`setPosition_p` at ~:337 + `moveObjectAlong` at ~:339) — why does resolve rewind
the player up to ~4m (esp. vertically) at a wall graze? The fix likely lives in
collision response, not portal cell membership. The 2026-06-12 "re-entrancy guard
in positionChanged" recommendation below is SUPERSEDED — do not re-attempt it.

## 2026-06-12 instrumented session — MECHANISM FOUND, fix deferred
(SUPERSEDED 2026-06-13 — see section above. The "same-frame portal ping-pong"
was a correlate/symptom, not the cause of the visible snap.)


Ran the instrumentation plan below in-game (Debug client under cdb, Mos Eisley
cantina, ~10 min of wall-grazing traffic by both Claude-pilot and Kenny).
Full capture: `.planning/todos/cornersnap-instrumentation-capture-2026-06-12.txt`
(2,679 events). Instrumentation is committed, `_DEBUG`-only, tagged `CORNERSNAP`.

**The original pushback hypothesis is FALSIFIED.** Zero `CORNERSNAP-CELLJUMP`
events: the `CollisionResolve::resolveCollisions` rewind/replay
(`CollisionResolve.cpp:313-323` — rewinds the object to timestep start, then
replays resolved waypoints) never produced a net parent-cell change, despite
continuous wall contact. Don't pursue the "clamp pushback" fix.

**Actual anomaly: same-frame portal ping-pong (re-entrant positionChanged).**
On 8 frames, all at portal planes (world/foyer1, foyer1/foyer2, foyer2/cantina),
the PLAYER transitioned cells 3-5 times within ONE frame, each segment the exact
reversal of the previous (cross → cross back → cross again). Frame 13019 did
this with a 3.6 m segment — visibly a snap. Mechanism: `setParentCell` from the
portal walk triggers another position notification whose old/new segment is
reversed, which walks back through the portal, recursively. The `dueToParent`
early-out does not catch this re-entry.

**Camera churn:** the chase camera (NetworkId 0) flaps cells EVERY frame for
dozens of consecutive frames whenever the camera boom straddles a doorway
portal (constant `foyer2→foyer1` walks). Each pays full cell-reparent cost.
Consistent with Kenny's 2026-06-12 observation that the snap is much less
visible under D3D11 — better frame times absorb the churn.

**Fix target when picked up:** re-entrancy/oscillation guard in
`CellPropertyNamespace::Notification::positionChanged`
(`sharedObject/src/shared/portal/CellProperty.cpp:115`) — e.g. suppress a
second transition for the same object within the same frame, or detect the
A→B→A reversal and keep the first result. Camera churn may deserve a separate
hysteresis (don't re-walk the camera's cell every frame at a portal plane).

---

## What this is

Inside Mos Eisley cantina (and almost certainly any indoor cell with internal
sub-cell portals), walking around the interior produces a position snap under
specific input conditions:

- **Trigger: collision contact with walls.** Smooth motion through the same
  geometry produces zero snap. Wall-grazing or wall-bumping fires the snap
  almost every time.
- **Mouse-steering >> keyboard-steering.** Mouse-look produces continuous
  fine-grained heading corrections that frequently graze walls during
  cell traversal. Keyboard-only motion (WASD) avoids the snap almost entirely
  unless the player deliberately walks into a wall.

This is **NOT a regression introduced by the v145 toolchain swap or any v2
work.** See "Confirmation evidence" below.

## What it is NOT

- Not a frame-rate-fragility bug. Tested at 30 FPS and 250 FPS — identical
  behavior.
- Not a `DEBUG_WARNING` / `OutputDebugString` IPC overhead bug. Tested in
  Release config (zero warning machinery) — snap persists identically.
- Not a v143→v145 codegen artifact. Reproduces in third-party SWGSource
  derivatives (e.g. Restoration server client) built with different
  toolchains.
- Not a source-change regression in v2. Git log of the original suspected
  regression window (2026-05-08 16:30 → 22:00 in `D:/Code/swg-client/`)
  shows zero `src/` modifications — only planning docs and test baselines.
- Not introduced by Phase 7 XPCOM removal, Phase 8 tool CMake work,
  Phase 9 STLPort work, or Phase 10 DPVS work. Predates Phase 7's closure
  (the first regressed v1 build dated 2026-05-08 21:51 is contemporary
  with Phase 7/8 close, but not caused by them).
- Not the orthogonal SafeCast.h:29 null dynamic_cast on world load
  (`2026-05-14-safecast-null-dynamic-cast-world-load.md`) — different
  code path, different symptom, different mechanism.

## Confirmation evidence

1. **v1 GOOD-reference test (attempted 2026-05-15):** intended to run the
   pre-regression May 8 16:44 build and reproduce wall-bump-snap on it.
   v1 binary crashed mid-zone-in before reaching the cantina (likely an
   unrelated `NOT_NULL(t)` FATAL — see the SafeCast.h:29 todo). Could not
   complete this test directly.

2. **v2 testing in current SwgClient_d.exe and SwgClient_r.exe
   (2026-05-15):** snap reproduces in both Debug and Release. Mouse-steer
   + wall-bump pattern triggers consistently; smooth keyboard motion
   produces no snap. Direction asymmetry noted earlier ("going into main
   room worse than leaving") correlates with which side of the portal
   plane the pushback rewinds you onto.

3. **Restoration server client test (2026-05-15):** "I ran the game on
   Restoration server using their client (SWGSource derivative) and it
   snaps a lot, as much as this version." Cross-client + cross-toolchain
   confirmation that this is general SOE engine behavior, not specific
   to our build.

4. **Git-log regression-window analysis (2026-05-15):** Zero `src/`
   changes in the 2026-05-08 16:30–22:00 window in v1 tree
   (`D:/Code/swg-client/`). All 11 commits in the window are planning
   docs, test baselines, and CLAUDE.md toolchain notes. Whatever produced
   the visible behavior change between the 16:44 GOOD build and the
   21:51 REGRESSED build was either the toolchain swap (v143 → v145)
   or pure observation bias from increased attention during Phase 9 test
   capture. Cross-client evidence makes observation-bias the dominant
   explanation.

## Suspected mechanism (unverified — investigation hook for future work)

When player movement produces a collision against a cell wall, the
collision resolution system applies a pushback to keep the player inside
walkable space. The hypothesis is that the pushback can briefly place
the player position on the wrong side of an internal cell-portal plane
(particularly in geometries where walls and portal planes are close to
parallel), causing one frame of cell mis-attribution before normal
portal traversal corrects it. The single-frame cell change is what the
user sees as a "snap."

Specific files to investigate when a future phase touches this code:

- `src/engine/shared/library/sharedObject/src/shared/object/CellProperty.cpp`
  — `Notification::positionChanged` near line 115. This is the
  per-frame entry point that decides which cell a moved object now
  belongs to.
- `src/engine/shared/library/sharedObject/src/shared/object/Object.cpp`
  — `setParentCell` near line 1387. Actual cell-reparenting.
- Collision pushback site in `sharedCollision` — needs grep when this
  picks up. Likely in `CollisionWorld`, `BoxExtent`, or `Floor` files.

Useful instrumented capture for a future debug session:

- Log `oldPosition`, `newPosition`, source cell ID, target cell ID, frame
  number at `CellProperty::Notification::positionChanged`.
- Log collision-hit position, pre-push position, post-push position, and
  frame number at the collision response site.
- Correlate: does every observed snap-frame have a collision event in
  the same frame? If yes, hypothesis confirmed and the fix probably
  goes in the collision-response post-push step (clamp pushback so it
  cannot cross an internal portal plane in a single step).

## Workaround for current play

- Steer with keyboard (WASD) instead of mouse-look during indoor cell
  navigation.
- Avoid grazing walls when turning interior corners.
- The snap is purely visual continuity — gameplay state is correct,
  no actual position rollback or duplicated cell traversal damage.

## Priority

**Low.** Cosmetic. Reproduces in all known SWGSource-derivative clients
including the production-grade Restoration build, none of which have
prioritized fixing it. Worth fixing eventually because it is annoying
once noticed, but not blocking for any current milestone.

## Related

- `.planning/debug/cantina-corner-snap.md` — closed debug session
  capturing the full investigation (Cycles 1-5) including the
  invalidated `OutputDebugString` theory and the
  frame-rate-fragility disproof.
- `.planning/todos/pending/2026-05-14-safecast-null-dynamic-cast-world-load.md`
  — orthogonal Koogie-tree SafeCast null cast issue. Crossed paths with
  this todo when the v1 GOOD-reference revival attempt during Cycle 4
  hit a likely v1-flavored variant of the same SafeCast call site.
