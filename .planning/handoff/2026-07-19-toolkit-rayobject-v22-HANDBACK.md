# 2026-07-19 — v22 HANDBACK: clientWorld::collideScreenRayObject (borrowed-Object* pick)

**Status:** DONE 2026-07-19 night, build-gated + 45s boot smoke, exe restaged.
**Contract v21 → v22, 143 → 144 names.**
**Request:** SWG-Toolkit `2026-07-19-CHANGE-REQUEST-collideScreenRayObject.md` — the fallback
row pre-described in the hybrid-incell ANSWERS addendum, requested after the CONSULT-69
layer probe MEASURED that pure .ilf decorations never reach the hud pick
(`cuiHud::getTarget` = null for an id-less table), refuting the watcher-path prediction.
The initial experiment "PASS" is corrected in `CONSULT-69-SYNTHESIS.md` (the moved chair
was a server-streamed networked tangible, id 1127094080).

## 1. The row

`clientWorld::collideScreenRayObject` →
`extern "C" void* __cdecl utinni_collideScreenRayObject(int screenX, int screenY, int objectsOnly)`

- **Same ray as `collideScreenRay`** (shared internal core, refactored — identical flags,
  camera-cell start, `cms_default`, player excluded, getTargetingRange length,
  `objectsOnly=1` drops terrain/terrainFlora/interiorGeometry).
- Returns the **RAW nearest-hit `Object*`** — NO ancestor walk, NO id resolution. Null =
  miss / no camera. The hit may be: an .ilf decoration, a networked tangible, a child part,
  the TerrainObject (objectsOnly=0 outdoors), or the BUILDING itself when the ray strikes
  cell geometry.
- **BORROWED, game-thread-only.** Lifetime: an .ilf object lives until its owning building
  leaves world (single delete site, TangibleObject.cpp:502 — no LOD/visibility/throttle
  invalidation). Clear on cell/zone change; never cache across a zone; your SEH guard +
  per-frame re-pick discipline stands.

## 2. ⚠️ Two truths to design the consumer UX around

1. **The floor-hit ambiguity — your `id=0, point≈table` probe result is NOT yet proof the
   ray hit the TABLE.** A hit on the floor beneath the cursor reads almost identically
   (id 0 was measured, but a server-building's geometry hit should have resolved the
   BUILDING's id through the ancestor walk — that it didn't is genuinely odd). Decoration
   meshes may not implement collision at all (`implementsCollide()` gates the appearance
   test, ClientWorld.cpp:1180) — which would explain BOTH measurements at once (ray "hits"
   near the table, hud pick finds nothing). **This row settles it instantly:** the returned
   `Object*`'s template (your existing read) names exactly what the ray touched. If it
   comes back floor/building rather than the table, the next ask is an EXTENT-based pick
   sibling (`CF_extentsOnly` pass, the hud's own findObjectsByExtent shape) — flag it and
   we'll spec it; do not burn a round discovering this the slow way.
2. **Gizmo-ing server-streamed hits desyncs from the server** (your floating-NPC chair).
   Layer-triage before manipulating: `collideScreenRay` id != 0 + `wsGetNodeInfo(id)` miss
   → server object → warn/refuse in the editor.

## 3. Gates (all green, 2026-07-19 ~20:50 local)

- Release/Win32 `/t:SwgClient` forced relink: exit 0, **0 unresolved**; exe auto-staged
  20:49:19.
- `GetEngineHookPoints` ordinal 82, undecorated. 144 == 144 static_assert holds.
- 45s boot smoke: alive, no new dumps. x64 untouched by construction.

## 4. Contract re-sync (maintainer)

```
be7338381d41a1df1fb1fb8f141717a3f077df0f0f9e22e3ad3be389b141ed79  engine_hookpoints.h
45791354539873cd636f423d229f97e2091b42cd95c2ea463635280973b6f284  engine_hookpoints.inc
```

Version-assert 22, count 144. Append-only over v21/v20 — one re-sync covers all pending
binds (getSceneId included).

## 5. Smoke steps (consumer)

1. Hover the SAME pure-.ilf table from your probe → `collideScreenRayObject` non-null →
   read its template name → confirm it IS the table (settles §2.1).
2. Latch the pointer on the hover/click frame → gizmo drives the transform rows → table
   moves live. (If step 1 returned floor/building instead: report back, request the
   extent-pick sibling.)
3. Contrast check: same call on the sittable chair → same Object* your getTarget path
   already delivered.
4. Zone/cell change with a latched pointer → your clear-on-change path fires, no touch of
   the stale pointer.
