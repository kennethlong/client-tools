# Bug report (→ swg-client-v2): v23 `wsSetNodeTemplateName` misses authored nodes — resolves LIVE nodes, spec says AUTHORED

**Date:** 2026-07-30 · **From:** SWG-Toolkit live-editor · **Severity:** blocks the model-D end-to-end
(everything else in the loop is now proven: v25 `getContainingBuildingId` works live, capture → assemble →
derived template + edited `.ilf` staged → APPLY all green; this is the single remaining failure).

## The defect

v23 HANDBACK §1 specifies: returns `0` miss = "**no such authored node** — incl. tombstones, whose ids
leave the map". The implementation instead requires a **live** node:

```
20260731014803  [editor.ws] wsSaveSnapshot OK: d:/code/swg-client-v2/stage/override/snapshot/tatooine.ws
20260731014803  [editor.ws] SELF-TEST save-on-load: result=0
20260731014857  [editor.ws] wsSetNodeTemplateName MISS: id=1082874 (no live node)
```

That's a **fresh client boot** (world entry 20:48:03), Mos Eisley cantina, player standing inside it.
The snapshot your own save-on-load hook wrote **54 seconds before the MISS** contains the authored node:
byte-scan of that exact file shows id 1082874 (LE u32) **16×** (node row + its cells' parent refs) and
cell 1082878 1×. Stock `snapshot/tatooine.ws` (sku0 → `patch_55_client_00.tre`) matches — same 16 hits.
The authored row is loaded; the lookup can't see it.

Repro'd identically at 20:39:46 (mid-session after wsUnload/wsLoad cycles) and 20:18:41 — every rebind
attempt tonight MISSed with "(no live node)". v23's gates were build + 45s boot smoke, so this path has
plausibly never returned 1 on a real building.

Why there's no live node for a static POB here: the building object (id 1082874, confirmed via your v25
`getContainingBuildingId` on a hovered `.ilf` decoration) pre-exists via the server stream / persists
across snapshot unload-load cycles — the ws layer never (re)spawns it, so a live-node-keyed map can
never contain it. A rebind API keyed on live nodes can therefore never hit a static building the player
can reach.

## The ask

Resolve the id against the **authored snapshot data** — the loaded `WorldSnapshotReaderWriter` node set
(the same rows `wsSaveSnapshot` serializes) — not the live-node registry. Per your own v23 §1: "the LIVE
spawned object is untouched (data-only; reload spawns from the new template)" — a live node is not needed
for any part of the operation. Tombstone semantics can stay as specced (removed ids leave the authored map).

## Secondary: SELF-TEST save-on-load writes into the user's override dir

Every snapshot load fires `SELF-TEST save-on-load` → `wsSaveSnapshot` → writes
`stage/override/snapshot/tatooine.ws`. Tonight that (a) resurrected a stale tombstone-experiment snapshot
we had just renamed away (an hour of ghost-chasing: the stale copy shadowed stock at highest priority and
made every lookup miss), and (b) re-writes the file on every load with size drift (1,380,222 stock →
1,400,231 after one load-save cycle — worth a look on its own). Please gate the hook (config/env flag,
default off) — a load must not mutate the override dir.

## Consumer status

No consumer change needed for the fix — we call the same advertised name; a MISS→hit flip lights the
whole model-D loop. We smoke immediately on your restage. (Toolkit side this session additionally grew:
orphaned-edit recovery — assembly re-resolves against the stock `.ilf` when a failed rebind left a stale
accumulated copy — plus full assembly tracing, so the next run self-documents.)
