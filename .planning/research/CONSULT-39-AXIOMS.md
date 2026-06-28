# CONSULT-39 — async .mgn load FAILURE/RETRY/RECOVERY redesign (convergence round)

GOAL: converge on ONE minimal, invariant-correct redesign. This is a DESIGN round, not a re-review.

## LOCKED AXIOMS — treat as GROUND TRUTH, do NOT re-derive, re-litigate, or "verify" these
(They were established by a prior 4-AI review with cross-confirmation. Re-deriving them wastes the round.)

- **A1.** Async `.mgn` loads on the MAIN thread. `SkeletalMeshGeneratorTemplate::asynchronousLoadCallback()`
  (`SkeletalMeshGeneratorTemplate.cpp:3250`) runs `load()` — which sets `m_isLoaded=true` at `:2222` —
  THEN a `std::for_each(... SkeletalMeshGenerator::create)` fixup over `m_uninitializedMeshGenerators`
  (`:3297`), then frees the list. So `m_isLoaded` is TRUE during the fixup loop ("I6 window").
- **A2.** Current (under-review) change: `asynchronousLoadCallback` clears `m_asynchronousLoadInProgress`
  on BOTH success and failure; on failure it leaves `m_uninitializedMeshGenerators` queued and returns.
  `createMeshGenerator()` (`:1953`) re-kicks a load when `(!m_isLoaded && !m_asynchronousLoadInProgress)`
  and appends the new generator to the SAME list.
- **A3 — the PROVEN problems to FIX (do not re-argue whether they're real; design the fix):**
  - (a) **erase-during-`for_each` UB**: re-kick builds a multi-generation list; reentrant
    `create()→handleCustomizationModification` (`:300`) can destroy a sibling generator →
    `~SkeletalMeshGenerator`→`removeAsynchronouslyLoadedMeshGenerator` mutates the vector mid-iteration.
    (Also latently possible on a multi-generator SUCCESS load.)
  - (b) **silent invisible NPC**: on failure an ALREADY-SPAWNED consumer stays not-ready forever;
    `SkeletalAppearance2::unloadUnusedResources` keeps the LOD (won't evict while not-ready) → no
    re-fetch ever heals it. (Recovery via re-kick only helps a NEW consumer.)
  - (c) **no backoff**: re-kick on every touch disk-hammers a permanently-missing/corrupt asset.
  - (d) **wrong I3 predicate**: 7 `SkeletalMeshGenerator` guards key on `isLoaded()`, but per A1
    `isLoaded()==true` while a queued generator's `m_blendValues==NULL` during fixup → guard can pass and
    still deref NULL. Correct predicate is per-instance readiness (created / m_blendValues populated).
- **A4.** `SkeletalMeshGenerator` holds a refcount on its template (`m_referenceCount`, fetch/release) →
  the template CANNOT be freed while a live SMG references it. So RANK-1's value is the NULL-list
  (load-completed) case, not a freed-template case.
- **A5.** `SkeletalMeshGenerator::create()` does NOT currently `signalModified()`/re-dirty. Success-path
  healing relies on the appearance's dirty flag staying true until ready, then `rebuildIfDirtyAndAvailable`
  on a later `alter()` frame. `getSkeleton()` (`SkeletalAppearance2.cpp:2382`) calls `rebuildMesh`
  WITHOUT a readiness gate (clears dirty even on a partial/skipped build).

## THE OPEN DESIGN QUESTION (this is what to converge on)
Specify the MINIMAL set of changes so the async `.mgn` failure/retry/recovery is correct:
1. **Iteration safety** for the fixup loop (snapshot-before-iterate vs set-`m_isLoaded`-after-loop vs other).
2. **Recovery** so an already-spawned NPC heals after a TRANSIENT failure (re-dirty on create? eviction
   nudge? explicit re-kick of queued generators?), without the silent-invisible end-state.
3. **Permanent-failure policy**: bounded retry / cooldown so a truly-missing asset neither hammers disk
   nor wedges invisibly forever (fallback appearance? give-up-after-N + visible placeholder?).
4. **Correct I3 guard predicate** (per-instance readiness vs isLoaded), and whether to keep a Debug assert.
5. Keep the diff MINIMAL and consistent with this 2003-era codebase's idioms (refcounting, dirty/alter,
   no new threads). Call out anything that conflicts across goals 1–4.
