# Phase 31 — Deferred Items (out-of-scope discoveries during execution)

Logged per the executor scope-boundary rule: out-of-scope blockers discovered while
executing a plan are recorded here, NOT fixed in the current plan.

---

## DEF-31-01: Misc.h:236 `memmove(void*,const void*,int)` C2668 ambiguity (x64)

- **Discovered during:** plan 31-02 (Wave 2) per-TU x64 verification.
- **File:** `src/engine/shared/library/sharedFoundation/src/shared/Misc.h:232-236`
- **Defect:** the engine helper `inline void *memmove(void *destination, const void *source, int length)`
  calls `return memmove(destination, source, static_cast<uint>(length));`. On x64 the
  `<uint>` argument makes the call ambiguous between the engine's own
  `memmove(void*,const void*,int)` overload and the CRT `memmove(void*,const void*,size_t)`
  (`size_t` = 8 bytes on LLP64) -> `error C2668`. (The sibling `memcpy` helper at :218 has the
  same shape but resolves to the CRT `memcpy` cleanly; only the self-recursive `memmove` is
  ambiguous because the engine overload is itself a candidate.)
- **Blast radius:** this is the cross-plan dominant blocker recorded by 31-01 — it transitively
  blocks ~every in-scope TU that includes `Misc.h`. It does NOT, however, halt cl's emission of
  the C4235/C4311/C4312/C4244 worklist (cl continues past it), so it does not invalidate the
  per-TU asm/truncation verification used in this wave.
- **Why deferred from 31-02:** NOT in the 31-02 plan body / `files_modified` (which is scoped to
  FloatingPointUnit/SseMath/Transform only). The cross-plan note explicitly names 31-04 as the
  alternative owner. The 31-02 acceptance grep (`error C4235|C4311|C4312|C4244` == 0) is robust to
  the residual C2668, so 31-02's three TUs verify x64-clean regardless.
- **Owner:** plan **31-04** (the BITS-02/foundation fix wave). Recommended fix: qualify the
  recursive call to the CRT, e.g. `return ::memmove(destination, source, static_cast<size_t>(length));`
  (call the global CRT `memmove` with a `size_t` length), removing the ambiguity while keeping the
  engine wrapper's `int`-length signature for callers. Mirror the `memcpy` helper's intent.
