# CONSULT-55 (fresh Sonnet 5) — The "frequent after rebuild" timing signal + alternative causes

Read `.planning/research/CONSULT-55-EVIDENCE-BRIEF.md` first (facts are GIVEN). Repo: `D:\Code\swg-client-v2`.
You have read access to the tree.

## Your angle (lateral):
The strongest empirical clue is **"frequently manifests after a rebuild."** Explain WHY a rebuild raises
the crash rate, and let that discriminate among causes:
- Cold OS file cache + cold shader-bytecode disk cache after rebuild → slower/differently-timed async
  loads → wider race window. Does the codebase have a shader bytecode disk cache (Phase 32 zonein optims)
  that a rebuild invalidates? Where, and does its cold-start change async-load timing?
- Could a rebuild change WHICH files land in the async preload set for a zone-in, or the order, exposing a
  latent dependency-ordering bug (a dependent shader not preloaded → opened at a moment the cache is being
  cleared)?
Then pressure-test the leading hypothesis with alternatives:
- TreeFile cache EVICTION mid-parse (clearCachedFiles pitching a file an in-progress parse still reads).
- zlib decompression into a shared/recycled scratch buffer.
- `Iff::open(name, true)` semantics (what does the `true` do — cached read? optional?).
- Whether the mesh's dependent named shaders (the :506 redirect target) are reliably preloaded vs raced.
- Is there a `suspendAsynchronousLoaderThread` / disable-async-preload path that would confirm-by-elimination?

Deliverable: a mechanistic explanation of the rebuild correlation that points at one root cause, plus the
cheapest confirming experiment (config flag flip / cache wipe / forced cold-start) the user can run.

Write your final answer to `.planning/research/CONSULT-55-sonnet.out` and return it.
