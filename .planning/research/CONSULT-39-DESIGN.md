# CONSULT-39 — Converged design: async .mgn failure/retry/recovery (4-AI synthesis)

Reviewers: Cursor (code-plan), Opus (state-machine), Sonnet (failure-policy + pressure-test), Codex
(call-graph re-drive). Strong convergence; divergences resolved below.

## Converged decisions

### A. Iteration safety (fixes A3a erase-during-for_each + the stale-pointer residual)
**Snapshot (swap + NULL the member before the loop) + fetch()/release() bookend on the snapshot.**
- swap list into a local, `delete` + NULL `m_uninitializedMeshGenerators` BEFORE iterating → a reentrant
  `~SMG → removeAsynchronouslyLoadedMeshGenerator` hits the NULL-list guard and is a no-op (no live-vector
  mutation).
- `MeshGenerator::fetch()` every snapshot entry, run `create()`, then `release()` every entry. The +1 ref
  prevents a mid-loop appearance-destroy from FREEING a still-to-be-created generator (Sonnet: plain
  snapshot leaves a use-after-free; the fetch/release bookend is the engine's own idiom and closes it).
- Decision record: REJECT "defer m_isLoaded past the loop" (Opus: breaks the shared sync-ctor path AND
  worsens the UB). REJECT plain snapshot alone (Sonnet/Opus: stale-pointer UAF).

### B. Correct readiness predicate (fixes A3d / I6 window)
**Explicit `bool m_isCreated` on SkeletalMeshGenerator**, false in ctor, set true as the LAST line of
`create()`. Route all 7 I3 guards AND `isReadyForUse()` through `m_isCreated`.
- Opus proof: `m_blendValues != NULL` is INVALID as the predicate — a zero-blend-variable template yields
  a correctly-created SMG with `m_blendValues==NULL` forever. And `isLoaded()` is true during the fixup
  window while a queued SMG is not yet created. Only an explicit per-instance flag is correct.
- Keep `DEBUG_FATAL(!m_isCreated, ...)` in Debug at the deref sites; keep the release-safe early-return.
- Also: SMG dtor self-remove (SMG:255) keys on `!m_isCreated` (not `!isLoaded()`).

### C. Heal an already-spawned NPC after load (fixes A3b, transient)
1. **`signalModified()` at the end of `create()`** — Codex confirms this reaches the registered
   `meshGeneratorModifiedCallback` (async SMGs have the handler by fixup time) and marks the appearance
   dirty; rebuild then happens on the next render (not reentrantly — safe, at most one extra build).
2. **Gate `getSkeleton()`'s rebuild on readiness** (SkeletalAppearance2.cpp:2382) so it can't clear the
   dirty bit during the not-ready window (A5). (With C1+C2, Sonnet's "dirty persists" assumption holds.)
3. **Template-side bounded retry**: on async open-failure with generators still queued and attempts < N,
   re-`AsynchronousLoader::add(...)`. Codex: `createMeshGenerator()` re-kick does NOT fire for an
   already-cached consumer, so the template MUST re-issue; on success the fixup create()s the queued
   generators → they heal. This is the real transient-recovery substrate.

### D. Permanent-failure policy (fixes A3c + the eviction livelock)
- `uint8 m_loadAttemptCount` (++ on failure), `bool m_loadPermanentlyFailed` after N (=3) attempts.
- Gate BOTH the template retry (C3) and `createMeshGenerator()` re-kick (SMGT:1959) on
  `!m_loadPermanentlyFailed` → no disk-hammer (Sonnet: re-kick is naturally bounded; NO timer/cooldown).
- On permanent failure, `isReadyForUse()` returns TRUE → unfreezes `unloadUnusedResources` (which otherwise
  resets the MRU timer forever while not-ready, Codex) so the LOD can evict. NPC stays invisible (no
  placeholder .mgn exists; invisible-but-not-frozen is the accepted permanent outcome).
- Reset `m_loadAttemptCount = 0` on successful load.

### E. Cleanups
- `getOcclusionLayer` return `-1` not `0` when unready (template default is -1; 0 mis-orders layers).
- `[MGN-PROBE]` logs: wrap `#if PRODUCTION==0` or strip before ship.

## Divergences resolved
- snapshot vs drain vs defer-isLoaded → **snapshot + fetch/release bookend** (Sonnet definitive; Opus
  rejected defer-isLoaded; both confirmed plain snapshot insufficient).
- signalModified() in create(): Sonnet said unnecessary (relies on dirty persistence) — but A5 breaks
  that; Codex confirms it's a real dirty path. → **DO add it AND gate getSkeleton** (belt + suspenders).
- predicate m_blendValues vs m_isCreated → **m_isCreated** (Opus zero-blend proof).
- permanent-fail escape "eviction re-fetches" → **false** (Codex): isReadyForUse-true only unfreezes
  eviction; transient heal comes from the template-side retry (C3), not from re-fetch.
