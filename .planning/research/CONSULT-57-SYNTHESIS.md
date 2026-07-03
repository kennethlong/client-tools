# CONSULT-57 SYNTHESIS — threading-audit fixes review (2026-07-03)

4-consultant adversarial review of the CONSULT-56 follow-up diff (TreeFile snapshot locking,
Texture/pass-object refcount serialization, destroyShader wiring). Angles: Codex = trace
completeness + lock-order cycles; Cursor = line-level defects; Sonnet = lateral blast radius;
Opus = formal protocol audit. Evidence: `CONSULT-57-EVIDENCE-threading-audit-fixes.md`; raw
outputs `CONSULT-57-threading-audit-fixes-{codex,cursor}.out` (Sonnet/Opus returned via Agent).

## Verdict grid

| Change | Codex | Cursor | Sonnet | Opus |
|---|---|---|---|---|
| 1 TreeFile snapshot | DISSENT (2 gaps) | DISSENT (capacity) | CONFIRM | SOUND |
| 2 Texture CS + pass fetches | CONFIRM (no lock cycles) | CONFIRM | CONFIRM (hold-time caveat) | SOUND |
| 3 destroyShader wiring | CONFIRM | CONFIRM | CONFIRM | **FLAWED — teardown UAF** |

## Findings ACTED ON (all applied before commit)

1. **Opus (THE catch): deterministic zone-out UAF.** `~ClientProceduralTerrainAppearance`
   deleted `m_shaderCache` (:548) BEFORE `delete m_chunkTree` (:576); `~TerrainQuadTree` deletes
   every live ClientChunk, whose new `destroyShader` calls then dereference the freed cache
   (the null-guard is useless — the member is non-null dangling). Three consultants confirmed
   past this; only the formal-teardown-order attack found it. FIX: moved `delete m_shaderCache`
   after `delete m_chunkTree`.
2. **Cursor: snapshot capacity.** 64 could truncate lowest-priority search nodes on heavy cfgs
   (maxSearchPriority buckets x unbounded per-priority keys + runtime adds). FIX: 256 +
   warn-once on the read side + a paired WARNING in addSearchNode at the moment an add crosses
   capacity. (Truncation direction verified: lowest-priority tail — the safe end.)
3. **Codex: two unconverted `ms_searchNodes` reads in `TreeFile::install`.** Boot-single-threaded
   (benign) but now locked anyway — with the original double-evaluation preserved so SearchCache
   still lands one priority ABOVE the just-added SearchAbsolute (a subtle behavior my first fix
   attempt flattened; caught in self-review).
4. **Cursor: FATAL/DEBUG_FATAL while holding locks.** `Texture::release`'s negative-count FATAL
   and `destroyShader`'s DEBUG_FATAL fired inside held CSes (crash handler would run lock-held,
   stalling other threads until abort). FIX: both hoisted outside the lock scope.
5. **Cursor: destroyShader no-match visibility.** Silent no-op on unmatched shader hides
   accounting anomalies. FIX: WARNING after the (unlocked) scope. Safe because `flushCache` —
   the only path that could make no-match legitimate — is itself dead code.
6. **Sonnet: flushCache is a dormant twin bug.** Zero callers, and `nodeList.clear()` without
   `shader->release()` — stamped with a WARNING comment so nobody "completes" it naively.

## Findings NOTED, not acted on

- **Codex: TreeFileExtractor** reads `TreeFile::ms_searchNodes` directly — standalone
  single-threaded tool exe, no race; untouched.
- **Sonnet: Texture CS hold-time tail.** release-to-zero now brackets GPU teardown
  (`~Direct3d11_TextureData` COM releases) under the global texture CS — consistent with the
  pre-existing ShaderEffect idiom but the heaviest-tailed member of that lock family; measure
  under a zone-in capture only if frame hitches appear. Related possible hot spot:
  `CuiWidgetGroundRadar::Render` re-fetches/releases a modifiable shader every frame while its
  radar shader is null (steady-state frequency unconfirmed).
- **Opus: pre-existing** `FileStreamer::read` unlocked seek+read when `ms_useThread==false`
  (threaded streamer is on in our cfgs); the `iter31b` static diagnostic logger races. Both
  orthogonal to this diff.
- **Sonnet: pre-existing** `TextureList::remove` (ExitChain) bulk-deletes named textures
  bypassing release/CS — shutdown-order question, unchanged by this round.

## Cross-checks that held

- Codex's lock partial order: `ShaderCache::m_nodeListLock -> TextureList CS`,
  `ShaderTemplateList CS -> TextureList CS`, `TextureList CS -> D3D9/D3D11 resource teardown`;
  no pair in both orders anywhere.
- Opus: ExitChain LIFO + AsynchronousLoader join + terrain-thread join all precede
  `TreeFile::remove` → snapshot pointers can never outlive nodes in the client.
- Codex+Sonnet+Opus independently: createBlendedShader/addPrimitive pairing is 1:1 with no
  early-out between them; single producer, single consumer; plugins consume the single
  EXE-resident TextureList CS via DllExport (no duplicate-lock ABI split).
