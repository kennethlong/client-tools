# CONSULT-38 — CODE REVIEW (line-level correctness): defensive hardening of async .mgn load

You are the most detailed code reader. Review an UNCOMMITTED diff in a C++ game client (SWG, MSBuild,
x64). Your job is to FIND PROBLEMS — correctness bugs, regressions, unsafe returns. Do not rubber-stamp.

## Context (facts, treat as given — do NOT re-derive)
- Subsystem: `clientSkeletalAnimation` skeletal mesh generator (.mgn) async load.
- A `SkeletalMeshGeneratorTemplate` (SMGT) can be created UNLOADED (iff==NULL stub) when the
  AsynchronousLoader is enabled; the real `load()` runs LATER on the MAIN thread inside
  `asynchronousLoadCallback()`. Consumers (`SkeletalMeshGenerator`, "SMG") can exist before the template
  is loaded. `SMGT::isLoaded()` is the readiness predicate.
- The diff hardens an intermittent load-time crash. Three change classes:
  - **I4**: in `asynchronousLoadCallback()`, clear `m_asynchronousLoadInProgress` on BOTH success and
    failure (was success-only). On failure, leave `m_uninitializedMeshGenerators` queued, return.
  - **RANK-1**: `removeAsynchronouslyLoadedMeshGenerator()` now early-returns if
    `m_uninitializedMeshGenerators` is NULL (was `NOT_NULL`, compiled out in Release -> NULL deref).
  - **I3**: 7 `SkeletalMeshGenerator` methods that were `DEBUG_FATAL(!isLoaded)`-then-deref now
    early-return a default when `!isLoaded()` (release-safe guard). Two are void render-path methods
    (`applySkeletonModifications`, `addShaderPrimitives`); the rest return int/ptr/ref/void.

## The diff to review
Read: `.planning/research/CONSULT-38-DIFF.txt` (full unified diff, 3 files).
Also read surrounding context in the actual files as needed:
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SkeletalMeshGeneratorTemplate.cpp`
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/SkeletalMeshGenerator.cpp`
- `src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/MeshGeneratorTemplateList.cpp`

## Answer precisely (with file:line)
1. For EACH I3 guard: is the returned default SAFE for every caller? Specifically
   `getReferencedSkeletonTemplateName` returns a `static CrcLowerString("")` by reference — any caller
   that stores/compares that reference incorrectly? Is returning 0 from `getOcclusionLayer` /
   `createAppearance` / `getReferencedSkeletonTemplateCount` safe at all call sites?
2. The two void render methods now SKIP work when unloaded (contribute no shader primitives / no skeleton
   mods). Is there a path that RE-RUNS them after the template finishes loading (so the skip is temporary,
   not a permanently-invisible/incorrect appearance)? Trace it or state it's missing.
3. I4: is moving `m_asynchronousLoadInProgress = false` to before the failure-return correct w.r.t. the
   brace/scope it now sits in? Any double-clear or ordering hazard vs the success path's fixup block?
4. RANK-1 NULL guard: with I4 now KEEPING the uninitialized list on failure, can the list still be NULL
   here in a legitimate path? Is the early-return the right resolution, or does it drop a needed cleanup?
5. Any NEW null-deref, leak, or use-after-free introduced by the diff? Any compile/type issue?

Output: concrete file:line findings. If a change is correct, say so explicitly and why. Find the holes.
