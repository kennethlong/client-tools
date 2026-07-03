# 2026-07-02 — gl11 camera see-through + intermittent async-load crash — BOTH FIXED

**Status: DONE, committed + pushed, verified. Both platforms (Win32 + x64) rebuilt & staged.**
Two independent bugs root-caused via crew consults (CONSULT-54, CONSULT-55) and fixed. Working tree
clean; `master` = `b9c890992`.

---

## 1. gl11 chase-camera clipping through interior walls — FIXED (`a976f81e2`)

**Symptom:** on gl11 (D3D11), standing adjacent to an interior POB wall and moving the camera let it
pass through the wall and show the exterior. gl05 (D3D9) was correct (camera zoomed in). We had been
masking it with an interior zoom-cap band-aid (`freeChaseCameraInteriorMaximumZoom=3.0`).

**Root cause (NOT the camera code):** `FreeChaseCamera` + `ClientWorld::collide` + `CollisionWorld` +
the whole collision-data build path are **byte-identical to the SWG-Source reference** (`D:\Code\client-tools`).
The bug is gl11-specific and mechanical: `Direct3d11_StaticVertexBufferData::lock(readOnly)` **memset the
returned buffer to ZERO** (DEFAULT D3D11 buffers can't be GPU-read-back). At load time
`ShaderPrimitiveSetTemplate::collisionSplit()` builds each mesh's CPU collision triangles from a
read-only VB+IB lock; the INDEX buffer had a CPU shadow (the cape-spike fix) but the VERTEX buffer never
did, so every wall's collision geometry collapsed to the origin → `collide()` never hit interior walls →
camera passed through. gl05 returns real vertices, so it worked.

**Fix:** mirror the index-buffer CPU shadow onto `Direct3d11_StaticVertexBufferData` (persist uploaded
bytes on write-unlock; seed read-only locks from the shadow instead of zeroing). Restores ALL gl11
mesh-collision-from-VB, not just the camera. Then removed the interior zoom-cap band-aid from
`FreeChaseCamera.cpp` + all four staged cfgs. Door-snap fixes (`cs_seamGrazeEpsilon`,
`cs_cameraPullInSpeed`) untouched. **Live-verified:** camera now zooms in at the wall on gl11 like gl05.

**Residual (accepted, pre-existing, renderer-agnostic):** fast camera rotation against a wall still
peeks outside for a fraction of a second = our `cs_cameraPullInSpeed` rate-limit throttling the inward
pull-in. Kenny accepted it. If we tighten it later: Opus's penetration-bound (`min(zoom, collisionZoom+~0.25m)`)
or a persistence-gate (sustained wall snaps / transient doorframe eases) — must NOT regress the door-snap.
See `.planning/research/CONSULT-54-*` and memory `project_d3d11_static_vb_readback_zero_collision`.

## 2. Intermittent "Unknown shader template tag" async-load crash — FIXED (`9c03f53c5`)

**Symptom:** intermittent FATAL in `ShaderTemplateList::create` — `Unknown shader template tag ??]' for
shader/...` (garbage `iff.getCurrentName()`), **"frequent after a rebuild."** The `.sht` is valid on
disk → runtime buffer corruption. (This is the parked "SSHT flake" / MeshAppearanceTemplate async
heap-corruption sibling.)

**Root cause:** `TreeFile::SearchCache::m_cachedFileMap` (a `std::map`) is read by the AsynchronousLoader
BACKGROUND thread (`TreeFile::open`→`SearchCache::open`/`exists`) with **NO lock**, while the MAIN thread
inserts across many zone-in frames (`CachedFileManager::preloadSomeAssets`→`SearchCache::addCachedFile`).
Concurrent `std::map` find/insert = UB (torn red-black rebalance) → a fetched `.sht` lands on the
wrong/torn node → wrong bytes → garbage tag. The search-node traversal in `TreeFile::open` runs OUTSIDE
`TreeFile::ms_criticalSection`; SearchCache took zero locking. **Amplifier:** `ZlibFile::decompress`
discarded `ZlibCompressor::expand()`'s return, so a corrupt/partial inflate silently returned an
uninitialized buffer as valid IFF data → the garbage surfaced far from the fault. **"After rebuild":**
cold caches slow preloading → the main-thread insert stream stretches across many more frames while the
loader hammers `TreeFile::open` → race window widens. Crashing asset was a spaceport shader (space
preload pump runs concurrently with async loads). (The async `cachedFilesMap` preload path is a RED
HERRING — safe; `Iff::open` takes a private copy via `readEntireFileAndClose`.)

**Fix:** per-instance leaf `Mutex m_cachedFileMapMutex` on `SearchCache` serializing ONLY the map
find/insert (I/O + `createAbstractFile` + decompress stay OUTSIDE the lock; entries never evicted so the
captured `CachedFile*` stays valid; strict leaf → no deadlock with `ms_criticalSection`/async `ms_mutex`).
Plus `ZlibFile::decompress` now FATALs on `expand() != expected length` (fail-loud, self-diagnosing).
sharedFile lives in the EXE, so `SwgClient_r.exe` rebuild covers it; gl0X dlls unaffected.
**Verified:** Kenny ran several post-rebuild launches, no recurrence. Likely also fixes the older
scattered `c0000005` async-load crashes (same underlying corruption, varying symptom).
See `.planning/research/CONSULT-55-*` and memory `project_treefile_searchcache_unlocked_map_async_crash`.

## 3. Cleanup (`b9c890992`)
Removed redundant local override band-aid assets (`stage/override/ksk_all_spaceterminal.dds`,
`decd_dath_gosaanree.apt` — added in `62547317d`). Removed the temporary `validateIff=true` diagnostic
from `stage/client.cfg` after the async fix was confirmed.

---

## Crew process note
Both fixes came from 4-consultant phone-a-friend rounds (Codex/Cursor/Sonnet-5/Opus) on non-overlapping
angles — convergence-from-divergence. CONSULT-55 was textbook: Codex cleared the async-cache red herring,
Cursor found the concurrent reader, Sonnet the zlib amplifier + rebuild mechanism, Opus pinned the
unlocked SearchCache map + the deadlock-free fix. The crew Sonnet is spawned via the Agent tool
`model: sonnet` alias → auto-resolves to the current default (Sonnet 5); the Agent tool only accepts
family aliases, not pinned IDs.

## State for next session
- `master` = `b9c890992`, clean tree, pushed. Both Win32 `stage/` + x64 `stage-x64/` carry all fixes.
- Boots to char-select; gl05 + gl11 render; JTL space works; camera collision holds on gl11.
- Nothing pending on our side.
