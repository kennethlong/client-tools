# CONSULT-39 (Codex / call-graph) — verify the RECOVERY re-drive path

Read `.planning/research/CONSULT-39-AXIOMS.md` first — the LOCKED AXIOMS are ground truth, do not re-derive.

You are the repo tracer. Your slice of the redesign: PROVE which recovery mechanism actually re-drives a
rebuild after an async load, so the chosen design (goal 2 + 3) is grounded in the real call graph.

Trace and answer with exact file:line:
1. The per-frame heal path: `alter()` -> `rebuildIfDirtyAndAvailable()` / `rebuildOrAdjustDisplayLodIndex`
   -> `areAllMeshGeneratorsReadyForDetailLevel` -> rebuild. Confirm it re-runs `addShaderPrimitives` /
   `applySkeletonModifications` once readiness flips true, IF the dirty flag is still set.
2. If `SkeletalMeshGenerator::create()` (or `asynchronousLoadCallback` after the fixup loop) called
   `signalModified()` / marked the owning appearance dirty, trace whether that reliably reaches a rebuild
   — AND whether it can RE-ENTER (create()->signalModified->rebuild->addShaderPrimitives) dangerously.
   Does SMG even have a back-pointer to its SkeletalAppearance2 to signal? If not, what is the wiring
   (modification listener registered at fetchOwnedMeshGeneratorsForDetailLevel ~3517)?
3. The eviction/re-fetch path: `unloadUnusedResources` keep-LOD-while-not-ready guard (~4379). Trace
   whether anything ever re-fetches a failed template for an already-spawned consumer, or if it is a true
   dead-end (confirms A3b). What is the minimal nudge that would let a failed LOD be re-fetched?
4. Bound the re-kick: who can call `createMeshGenerator()` for the same template, and how often per frame,
   on the failure path — to size the disk-hammer (A3c).

Output: concrete call chains (file:line) + a verdict on which recovery mechanism is REAL/safe vs illusory.
Do not propose the full design — give the call-graph facts the design must satisfy.
