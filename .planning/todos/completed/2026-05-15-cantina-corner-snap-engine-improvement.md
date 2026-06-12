---
created: 2026-05-15
title: Cantina interior corner-snap — pre-existing SOE engine quirk, engine-improvement candidate
area: rendering / collision response / portal traversal
next_action: defer — known engine behavior; revisit when a phase touches collision pushback or portal traversal
files:
  - src/engine/shared/library/sharedCollision/  (collision response — needs site grep)
  - src/engine/shared/library/sharedObject/src/shared/object/CellProperty.cpp  (Notification::positionChanged, line ~115)
  - src/engine/shared/library/sharedObject/src/shared/object/Object.cpp  (setParentCell, line ~1387)
references:
  - .planning/debug/cantina-corner-snap.md  (closed debug session, cycles 1-5)
  - .planning/todos/pending/2026-05-14-safecast-null-dynamic-cast-world-load.md  (orthogonal; cross-linked because the v1 GOOD-reference revival attempt during Cycle 4 hit a related but unrelated SafeCast issue)
status: known_engine_behavior
priority: low (annoying but not blocking; workaround available)
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
