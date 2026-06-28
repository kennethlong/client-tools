# CONSULT-48 — Synthesis: CuiMediator::garbageCollect shutdown use-after-free

4 consultants (Codex/Cursor/Sonnet/Opus) reviewed the proposed Fix1/Fix2 against the locked axioms
(`CONSULT-48-EVIDENCE-mediator-uaf.md`). Then a **Debug-build run produced the measured root**, which
re-aimed the fix. Forensics: `crash-20260623-nvwgf2um-FORENSICS.md` (the texture crash, separate).

## Crew verdict on the originally-proposed Fix1/Fix2
- **Fix2-as-stated (erase-before-deactivate) does NOT eliminate eip=0** (Opus + Cursor, confirmed):
  `getRefCount()`/`isOpenNextFrame()` are non-virtual inline member reads (`CuiMediator.h:565/:614`) — on a
  freed mediator they read garbage but don't crash; the gate often passes; `deactivate()` then runs the
  virtual `getCallbackObject()` (`:509`) on the freed object → null vtable slot → eip=0. Erasing the list
  entry doesn't un-free the captured pointer.
- **No external direct-delete exists** (Sonnet + Codex, exhaustive trace): the ONLY `delete mediator` is
  `garbageCollect:954`; every teardown path (`SwgCuiAllTargets`, `CuiMediatorFactory::remove`, `CuiWorkspace`)
  is `release()`-only. So the "dangling pointer from a rogue delete" premise (the basis of Fix1) is FALSE.
- **Fix1 alone is dangerous** (Sonnet): a dtor self-erase can invalidate the outer GC loop's live iterator.
- **The mechanism is re-entrant cascade GC** (Sonnet/Codex), not a stray dangling pointer. Codex's prime
  suspect: `SwgCuiAllTargets::~`/`removeUnusedStatusPages` dumping zero-ref status mediators into a later GC.
- Codex/Cursor extra: if Fix1 were kept it must also scrub `s_mediatorsToAdd`/`s_mediatorsToAddNext`.

## MEASURED ROOT (Debug build, the (a) run — ground truth that supersedes the hypotheses)
Debug `FATAL` (`stage/SwgClient_d.exe-...181837`): **`CuiWorkspace.cpp:335: Can't removeMediator
[SwgCuiGroundRadar] during m_iteratingEnabledStates`**. Stack (the re-entrant cascade):
```
CuiManagerManager::remove -> CuiMediator::garbageCollect (OUTER, :952 deactivate)
  -> SwgCuiHud...::handlePerformDeactivate -> CuiWorkspace::setEnabled
    -> CuiWorkspace::updateMediatorEnabledStates  [m_iteratingEnabledStates = true; snapshot+fetch iterate]
      -> CuiMediator::deactivate -> SwgCuiAllTargets::performDeactivate
        -> SwgCuiAllTargets::removeUnusedStatusPages (:838)
          -> CuiMediator::garbageCollect (NESTED/re-entrant, :954 delete)
            -> ~SwgCuiHudWindowManagerGround -> CuiWorkspace::removeMediator (:335)  <-- forbidden mid-iteration
```
`updateMediatorEnabledStates` snapshots with `fetch()` refs so `deactivate()` may free set nodes safely, but
its invariant FORBIDS add/removeMediator mid-iteration. The nested GC's `delete` triggers `removeMediator`,
violating it. Debug FATALs; **Release does the forbidden removal → corrupts the live iteration → eip=0 UAF.**

## FINAL FIX (38-09) — surgical, honors the engine's own invariant
**Re-entrancy guard in `CuiMediator::garbageCollect`** (static `s_collecting`; a nested call returns early and
defers to the outer/next pass — refcount-0 mediators are reaped by the continuing force=true loop / next
frame). This kills the nested GC, so no `removeMediator` fires during `m_iteratingEnabledStates`; the
snapshot+fetch iteration completes as designed. **Plus Fix2's erase-first + restart-on-mutation** kept as cheap
belt-and-suspenders loop robustness. **Fix1 (dtor self-remove) DROPPED** — it guarded a non-existent external
delete and is dangerous alone. Behavior-preserving for normal (non-re-entrant) collection.

`garbageCollect`-only change; no header touch (no plugin ABI cascade); Utinni contract untouched (v3).
Supersedes the per-site symptom guards 38-06 (`m_objectCallbackVector`) + 38-08 (`m_callback`), which remain
as harmless defense-in-depth.

## Why this is the de-anchoring win
The crew (reasoning from generic axioms) split Fix1-vs-Fix2 and converged on "re-entrancy, ship both." The
**measured Debug FATAL** then pinpointed the exact re-entrant caller + the exact invariant violated, replacing
both Fix1 and the as-stated Fix2 with a single re-entrancy guard that the crew's own mechanism analysis
supports. Consensus framing (Fix1+Fix2) was refined by ground truth into something smaller and more correct.
