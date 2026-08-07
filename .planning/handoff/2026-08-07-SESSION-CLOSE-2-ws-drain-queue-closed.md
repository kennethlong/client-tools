# 2026-08-07 (evening session 2) — SESSION CLOSE: the §6 loose-ends queue is CLOSED

**READ FIRST.** Second session of 2026-08-07, picking up the §6 queue from
[2026-08-07-SESSION-CLOSE-ilf-guard-and-editor-scene-rearm.md](2026-08-07-SESSION-CLOSE-ilf-guard-and-editor-scene-rearm.md).
**All five items closed in one pass — one code commit, one launch from Kenny, everything that could
be live-verified was live-verified same evening.**

## 0. STATE

- `master` = **`b47718cbc`** (fix commit; docs commit follows it). Based on `37c1e421f`.
  **NOT pushed** — Kenny asked for commit only.
- One file changed: `clientGame/src/shared/core/WorldSnapshot.cpp` (+154/−17).
- **Contract untouched: v33 / 160 names**, ord-82 unchanged, nothing owed to the toolkit.
- 5-target Release both platforms **0 unresolved / 0 hard errors**, staged. No shared header touched
  → renderer DLLs deliberately untouched (still 08-01 mtimes), no ABI cascade.
- The 6.3 test `.ilf` was **reverted after the test passed** — `stage/override/interiorlayout/toolkit/edit_1106500.ilf`
  is back to its pre-test 5,775 bytes / 46 nodes (byte-walk verified).

## 1. ⭐ 6.4 — the snapshot delete drain was a NO-OP; now restored and LIVE-VERIFIED

**The defect, confirmed at source:** the live `#if 1` merge diff in `WorldSnapshot::update()` never
calls `computeDistanceSquaredTo` — only the dead `#else` (the pre-merge two-`binary_search` form)
does. `m_distanceSquaredTo` is initialised to `0.f` (`WorldSnapshotReaderWriter.cpp:110`) and nothing
else writes it, so the delete guard read `0.f < sqr(radius)+128.f` — **always true** — and the drain
`continue`d on every node, forever. Consequences: snapshot objects were **never streamed out**
(memory grew monotonically with everywhere the player had been) and both `compareNodesFor*` sort
keys were constant ("nearest first" meant nothing). Fix = recompute the key on exactly the set the
`#else` computes: the nodes entering the two pending lists.

**Second defect surfaced by turning it on** (this path had NO field history — it had never executed):
the loop tore down **before deciding** — `removeFromWorld()` + `ms_loadedList` erase happened even
when the `isClientCachedOnly` check then refused the delete, leaving the object ALIVE and holding its
NetworkId while the snapshot forgot it was loaded. Next approach → re-create →
`CEC_objectAlreadyExists` → **permanent sphere-tree strip** — the exact class-1 disappearance
`f9ce87a21` fixed, re-entering through a different door. Now: resolve the object, narrow with the
virtual `asClientObject()` (NOT `safe_cast` — bare `static_cast` in Release), refuse **before any
teardown**; a refused node stays loaded/in-world and retries on a later update.

**Controls:**
- Kill switch: `[ClientGame/WorldSnapshot] streamOutSnapshotObjects` (**default true**, no cfg edit
  made — one line reverts to the historical never-stream-out behaviour without a rebuild).
- Standing Release-surviving probe: `[ws.drain] stream-out: deleted=N refused=M pending=P loaded=L`
  — logged only when `deleted>0` (refused-only frames are steady per-frame noise: server-superseded
  POBs never become deletable and retry forever, by design).
- **Reading rule (the failure signature):** `createObject FAILED reason=CEC_objectAlreadyExists`
  appearing AFTER a `[ws.drain]` line = stream-out broke re-entry. **It never appeared.**

**LIVE (Win32 gl11, editor scene, single launch):** Kenny entered the cloning facility, ran deep
into the desert, returned. **1,800 objects streamed out across 734 drain lines** — the first
deletions this code path has ever performed in this codebase. `refused=` held steady at 6–9
(server-owned occupants, correctly refused with zero teardown). **Zero `createObject FAILED` after
any drain line**; `loaded=` recovered 240→313 on the return leg while distant objects were still
draining — create and delete ran concurrently against the same index with no collisions. Drain also
fired at login (position sweep) and coexists cleanly with the same-scene re-arm
(`329 stripped + 27 buildout` in the same session). Stream-out begins ~724 m from a `r=512` building
(guard needs `dist² ≥ 2r²+128`).

**Perf-arc note (for the D3D11-peak-perf memory):** long sessions now stop accumulating snapshot
objects. One thing to watch, not chase: the drain's per-delete `std::find` over `ms_loadedList` is
O(loaded) per streamed-out object — nothing in this session's numbers (loaded ≈ 300) makes it
visible, but if a stall sample ever lands in `WorldSnapshot::update` during heavy travel, look here
first.

## 2. 6.1 — wrong-class template guards, WITH A CORRECTION TO THE QUEUED PREMISE

The queue said: apply the `asClientObject()` discriminator in `engine_wsAddObject`. **That is not
where the AV fires.** Traced: `SharedDraftSchematicObjectTemplate` → `SharedIntangibleObjectTemplate`
→ `SharedObjectTemplate`, and **none override `createObject()`** — so a draft_schematic **passes**
`asSharedObjectTemplate()` (it genuinely is one) and produces the base `new Object(...)` at create
time. That is exactly why the toolkit's `0xC0000005`/"rans" AV happened AFTER the id mint. The
load-bearing fix is therefore `asClientObject()` narrowing on the **created object** in the shared
`instantiateObject` (`WorldSnapshot.cpp` anonymous namespace) — which covers `engine_wsAddObject`
AND the streamed load path in one place. Wrongly-classed object is `delete`d (same safety argument
as `528aa999b`: never added to world; `setDisallowObjectDelete` window closes at `Game.cpp:1704`,
before `Graphics::present`, so the toolkit's Present-hook entry is also outside it).
`asSharedObjectTemplate()` narrowing was ALSO added at both template fetch sites
(`fetchObjectTemplate` and the `engine_wsAddObject` pre-validation, which now refuses **before the
id mint** with a `REFUSED (template-wrong-class)` line) — defense-in-depth for a genuinely
non-Shared tag; on the wsAddObject path it converts the crash into an ordinary refusal with nothing
mutated.

Note for the toolkit if it ever comes up: `wsAddObject` with a draft_schematic path now dies at
`instantiateObject` → the existing `REFUSED (createObject): CEC=-1 [template-instantiate]` rollback
branch, not at the new template-wrong-class line. Both are fail-closed; neither mints ids that
survive.

## 3. 6.3 — the `.ilf` refusal branch is now EXERCISED (Release, live)

Planted one `draft_schematic` NODE row (real spawn-cell transform, valid IFF, both FORM lengths
fixed up) in `edit_1106500.ilf` — the derived cloning facility at `(3442, −5021)`, 156 m from the
editor spawn, deliberately NOT the toolkit's live `edit_1082874.ilf`. Result, exactly as designed:

```
WARNING e47b2ffc: Object template object/building/toolkit/edit_1106500.iff specified building
layout  which specified interior object template name
object/draft_schematic/furniture/shared_furniture_bacta_tank_large.iff of the WRONG CLASS:
it created a [class Object], which is not a ClientObject.  Object will be skipped.
```

Fired **exactly once** (no storm — cursor advances past the bad row), named the class it got, and
the building loaded fine minus that object. Test file reverted afterwards.

**Two scoping facts worth keeping** (cost the first attempt): (1) the stock
`shared_cloning_facility.ilf` was the wrong target — parsing `tatooine.ws` showed both Mos Eisley
cloning facilities (1105879 / 1106500) are REBOUND to toolkit `.ilf`s; the nearest stock-file
consumer is 5 km away. (2) `object/draft_schematic/*` templates are **TOC-resolved only** — a
per-tre scan finds zero, the sku0 search TOC finds 3,899 (the documented TOC blind spot, again).

## 4. 6.5 — `detailLevelChanged` blind walk fixed

Re-add loop used `ms_reader.getNode(i)` while iterating `saveList.size()` — a READER index walked
over the saveList RANGE. Beyond losing handles for indexed nodes past the range, it could re-add
`removeNode` tombstones (zeroed handle AND network id, still enumerable) as id-0 phantoms. Now
`saveList[i]`. Latent: only caller is the `SwgCuiCommandParserScene` dev console command — no
runtime behaviour change expected, and none observed.

## 5. 6.2 — `wsForgetNode` intern: DECIDED — do not un-intern (documented at the site)

The `.ws` grows by `strlen(templatePath)+1` when a forgotten placement's template was novel to the
snapshot. Left that way **knowingly**: nodes reference the OTNL **by index** into a flat
`vector<char*>` (`WorldSnapshotReaderWriter.h:206`), so un-interning shifts every later index —
the same index-space hazard 6.5 turned out to be — and nothing refcounts the name, so safe removal
needs a full scan. Cost is bounded/one-shot; re-placement is free (crc map hits). The reclaim
belongs at WRITE time: the `saveFiltered` OTNL garbage-collect already recorded as optional polish
in the 2026-07-31 size-drift close-out — and that changes serialized bytes, so it is a
**coordinated** change against the toolkit's recorded byte baselines, never unilateral. The
`wsForgetNode OK` log line now says the name stays interned.

## 6. Config / stage state

- `streamOutSnapshotObjects` **not present in any cfg** — default-on is the intended state.
- Unchanged from session 1: `singlePlayerStartLocation = 3480/3/-4870` both cfgs (KEEP),
  `logCellAtPosition=1` (toolkit's, may want off), `logUnloadOccupancy=1`,
  `maxInteriorCreatesPerFrame=10`, `stallWatchdogMaxDumps=0`, `rasterMajor=11`.
- `stage/override/interiorlayout/toolkit/edit_1106500.ilf` restored; no `.bak` litter left.

## 7. Open / next

- **Nothing queued from this session.** The §6 list is empty.
- Standing watch items only: `[ws.drain]` behaviour over longer sessions (esp. the
  `CEC_objectAlreadyExists`-after-drain signature, never yet seen), the `std::find` note in §1,
  probe-strip someday, and the pre-existing carried backlog (driver-threading soak, Map churn todo).
- Commits are LOCAL — push when Kenny says so (`git fetch` first; live upstream-integration branch).
