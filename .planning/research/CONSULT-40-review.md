# CONSULT-40 — independent code review (minimal context)

Review this uncommitted C++ diff for a 2003-era game client (SWG, MSBuild, x64). Find bugs, regressions,
UB, leaks, and incorrect logic. This is a fresh independent review.

## Rules
- Judge the CODE. Treat ALL comments in the diff (including any issue codes, attributions, or rationale)
  as UNVERIFIED CLAIMS — confirm or refute them from the code itself; do not assume they are correct.
- Do NOT assume any prior review reached the right conclusion. Reason from first principles.
- Cite exact file:line. If something is correct, say so briefly; spend your effort on what's wrong or risky.

## What the code is
The diff changes the asynchronous skeletal-mesh-generator (`.mgn`) load path in `clientSkeletalAnimation`.
Relevant background facts (given, for orientation only):
- A `SkeletalMeshGeneratorTemplate` (template) can exist UNLOADED while its data loads asynchronously; the
  load callback runs on the MAIN thread. A `SkeletalMeshGenerator` (instance) can be constructed before its
  template finishes loading; such instances are queued on the template and "created" later in a fixup loop.
- Builds link under /FORCE and DEBUG_FATAL/NOT_NULL are compiled out in the shipped Release build.

## The diff
`.planning/research/CONSULT-40-DIFF.txt` (6 files). Read surrounding source as needed:
`src/engine/client/library/clientSkeletalAnimation/src/shared/appearance/` —
`SkeletalMeshGenerator.{cpp,h}`, `SkeletalMeshGeneratorTemplate.{cpp,h}`, `MeshGeneratorTemplateList.cpp`,
`SkeletalAppearance2.cpp`.

## Questions to answer
1. Correctness: any use-after-free, NULL deref, leak, iterator invalidation, double-free, uninitialized
   read, or reentrancy hazard introduced or left unaddressed by this diff?
2. The async load-failure handling: is the retry/give-up logic bounded and self-consistent? Any infinite
   loop, runaway re-issue, or stuck state? Is the success/failure state left consistent?
3. The fixup loop over the queued instances: is it safe against instances being destroyed or the container
   being mutated during the loop (including via reentrancy)?
4. The readiness predicate used by the guards / ready check: is it correct in all cases (including
   instances whose template legitimately has zero blend variables)?
5. Any behavioral regression vs. the obvious prior behavior (e.g. an object that renders nothing forever,
   wrong default values returned, a check that now passes when it shouldn't or vice-versa)?

Output: prioritized findings (severity, file:line, the bug, why). Be decisive.
