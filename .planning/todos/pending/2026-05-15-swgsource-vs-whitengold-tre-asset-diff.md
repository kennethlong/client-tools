---
created: 2026-05-15
title: Diff SWGSource (Restoration / community) vs whitengold codebase + TRE files for space-scene graphics artifacts
area: research / data-content / space rendering
next_action: defer — research item; revisit when JTL/space scenes become a priority (likely post-Phase-11)
files:
  - D:/Code/swg-client-v2/  (codebase)
  - D:/Code/SWGSource Client v3.0/  (TRE/TOC tree this client points at)
  - Restoration server client (if accessible)
references:
  - .planning/phases/11-d3d11-renderer-plugin/11-SPEC.md  (Phase 11 explicitly excludes space; this todo is its diagnostic counterpart)
status: research
priority: low (not blocking; informational for post-Phase-11)
---

## What this is

During the Phase 11 SPEC interview on 2026-05-15, the user noted:

> "Space worked barely - most of the graphics artifacts didnt load
> properly - due to differences in tre files I think. We should add
> a comparison between SWGSource and our code and tre files maybe."

This is a research item to investigate WHY space scenes (JTL nebulae,
ship hulls, asteroid fields, station interiors) render with missing
or corrupt graphics under whitengold v2 against the SWGSource VM. The
working hypothesis is that this is **data-driven** (TRE asset mismatch
between SWGSource v3.0 and the 2010 SOE client's expected format) and
**not renderer-driven** — but neither has been verified.

## Scope of the comparison

1. **Codebase diff**: whitengold v2 (`D:/Code/swg-client-v2/`) vs
   the Restoration server client tree (if accessible) — what's the
   delta in space-rendering code paths? Specifically:
   - `clientGame/src/shared/space/` files (NebulaManagerClient,
     ParticleSpaceSwooshTrail, etc.)
   - `sharedGame/src/shared/objectTemplate/SharedShipObjectTemplate`
   - HUD overlays for cockpit view, target reticle, weapon groups
2. **TRE asset diff**: SWGSource v3.0 client TRE contents vs the
   Restoration distribution (if accessible) — what space-related
   assets differ?
   - `space/*.iff` files (nebula definitions, ship templates,
     hyperroute graphs)
   - `appearance/*.sat` / `*.mgn` / `*.lod` files for ship hulls
   - `shader/*` files referenced by space objects
   - Particle definitions for nebula effects
3. **Behavior diff under same VM**: launch Restoration client AND v2
   client against the same SWGSource VM, fly to a nebula sector, and
   document what each renders. If Restoration renders the nebula
   correctly and v2 doesn't, that isolates the bug to either code
   delta or TRE-staging difference.

## Why this is NOT a Phase 11 requirement

Phase 11 explicitly excludes space scenes per the SPEC.md "Out of scope"
list:

> Space scenes (JTL nebulae, ship combat) — different rendering
> pipeline; v1 had pre-existing TRE-data artifacts unrelated to
> renderer; defer to follow-up phase

The space artifacts are believed to be a **data-content problem**, not
a **renderer problem**. Putting it inside Phase 11 would conflate two
unrelated investigations (D3D9→D3D11 port AND space data integrity)
and inflate Phase 11's scope by a hard-to-bound amount.

This todo lets the diagnostic work proceed in parallel or follow-up
without contaminating Phase 11's scope.

## Suggested investigation order (whoever picks this up)

1. **Cheapest first** — launch Restoration client against its own
   server and against the SWGSource VM (if Restoration can connect),
   record whether nebula renders correctly in each case. If broken
   only when pointing at SWGSource VM, the bug is TRE/server data,
   not the Restoration client.
2. **TRE inventory comparison** — diff the `space/*.iff` listing
   between SWGSource v3.0 and whatever Restoration ships. Look for
   missing files, version differences, byte-count anomalies.
3. **Network/asset trace** — launch v2 client, fly to a nebula
   sector, monitor what asset references the server requests vs
   what TRE provides. Missing-asset warnings in the client log are
   likely concentrated around the nebula entry moment.
4. **Code delta** — only investigate code paths if (1)–(3) point
   to a code issue rather than data.

## Cross-references

- `.planning/phases/11-d3d11-renderer-plugin/11-SPEC.md` — explicitly
  excludes space scenes from Phase 11; cross-links here for the
  parallel diagnostic.
- `project_swg_source_upstreaming` memory — if the bug turns out to
  be a missing whitengold guard or compat fix, it's an upstream PR
  candidate.
- `2026-05-15-cantina-corner-snap-engine-improvement.md` —
  unrelated, but a similar "investigate later" engine-quirk pattern.

## Priority

**Low.** Not blocking any active milestone. Worth doing because:
- It informs whether JTL space is ever feasible to revive under
  whitengold against SWGSource VM data.
- The TRE-diff methodology is reusable for any future data-content
  question (combat, professions, housing, etc.).
- It removes a known unknown from the project's surface area.
