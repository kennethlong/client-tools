# CONSULT-39 (Cursor / detailed code) — exact code-level redesign plan

Read `.planning/research/CONSULT-39-AXIOMS.md` first — LOCKED AXIOMS are ground truth, do not re-derive.

You are the most precise code reader. Your slice: produce the EXACT, minimal code changes (file:line,
what to add/replace) implementing the redesign, verified against the real functions. Be concrete enough
that an implementer could apply it. For each change, name the function and the surrounding invariant.

Cover:
1. **Iteration safety** in `asynchronousLoadCallback` (`SkeletalMeshGeneratorTemplate.cpp:3297`): show the
   snapshot-before-`for_each` (or set-`m_isLoaded`-after-loop) edit precisely. Note: `load()` sets
   `m_isLoaded` at `:2222` and is ALSO called on the synchronous ctor path — so if you move the isLoaded
   flip, account for BOTH paths. State the safest minimal edit.
2. **Recovery** (A3b): the exact mechanism + call to re-dirty the owning appearance after `create()`
   (or after the fixup loop) so an already-spawned NPC rebuilds once loaded. Verify SMG can reach its
   appearance (back-pointer / listener). If `getSkeleton()`'s ungated `rebuildMesh` (`SkeletalAppearance2.cpp:2382`)
   must be gated, show the exact guard.
3. **I3 predicate** (A3d): the exact replacement predicate for the 7 guards (per-instance created flag vs
   `m_blendValues`-based) and whether to add a new member; show the guard form. Keep a Debug assert.
4. **Permanent-failure** (A3c): the minimal cooldown/give-up field + where it's checked/reset.

For each proposed edit, explicitly check: does it break any existing caller, the sync load path, or the
refcount/dirty/alter idioms? Flag conflicts. Output: an ordered edit list with file:line + exact snippets.
