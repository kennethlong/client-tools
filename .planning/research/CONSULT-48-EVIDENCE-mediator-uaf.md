# CONSULT-48 — CuiMediator garbageCollect use-after-free: LOCKED axioms + proposed root fix

Repo `D:/Code/swg-client-v2`. File: `src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp`.
Review the PROPOSED FIX below against the LOCKED measured axioms. Do NOT re-derive the axioms.

## LOCKED axioms (measured from real Release minidumps; treat as GROUND TRUTH)
1. Crash is `c0000005`, **`eip=00000000` — a CALL through a null target**. Faulting instruction (Win32 dump
   `stage/...20260623175853.mdmp`): `mov eax,[edi]` (eax = the object's vtable) then `call dword ptr [eax+30h]`.
   Slot `+0x30` is `CuiMediator::getCallbackObject()` (`CuiMediator.h:366`, **virtual**). `[vtable+0x30]==0`.
2. Stack: `CuiMediator::deactivate (:509) <- CuiMediator::garbageCollect (:953) <- CuiManagerManager::remove
   <- CuiManager::remove <- ExitChain::run <- SetupSharedFoundation::remove <- ClientMain:407`. So this fires
   during **ExitChain SHUTDOWN** teardown (after the main loop). No Utinni injected.
3. A healthy CuiMediator vtable's `getCallbackObject` slot is NEVER null (CuiMediator defines it, :452). A null
   slot ⇒ the object's vtable is **freed/corrupt/mid-destruction** ⇒ `garbageCollect` is calling `deactivate()`
   on an **already-freed mediator** (use-after-free).
4. `garbageCollect` (:941-959): `for(it: s_mediators){ if(refCount==0 && (force||!openNextFrame)){ deactivate();
   it=erase(it); delete mediator; } else ++it; }`.
5. `~CuiMediator` (:264-268) ALREADY contains the abandoned invariant:
   `if (std::find(s_mediators..., this)!=end) DEBUG_FATAL("mediator was still in the mediator list in dtor");`
   — i.e. a mediator MUST be erased from `s_mediators` before deletion. **DEBUG_FATAL is a no-op in Release**,
   so in Release a mediator deleted DIRECTLY (bypassing garbageCollect) while still registered leaves a
   **dangling pointer in s_mediators** that a later garbageCollect deactivates → axiom 3. Iterator
   invalidation from nested/cascade deletes during the loop compounds it.
6. Three prior crashes (38-06 `m_objectCallbackVector`, 38-08 `m_callback`, this null-vtable call) are the SAME
   freed-mediator-deactivate, three faces. Per-site member guards cannot fix it (fault is the virtual dispatch).
7. s_mediators mutations: push_back at :119 + :1047; erase ONLY at garbageCollect :953. Registration deferred
   via s_mediatorsToAdd/Next (:114-121, :242). The dtor does NOT currently erase from s_mediators.

## PROPOSED FIX (review this)
**Fix 1 — dtor self-removes from s_mediators (Release-safe the invariant; KEEP the Debug FATAL as the (a) diagnostic):**
```cpp
// ~CuiMediator, replacing the :264-268 block:
const MediatorVector::iterator it = std::find (s_mediators.begin (), s_mediators.end (), this);
if (it != s_mediators.end ())
{
    DEBUG_FATAL (true, ("mediator was still in the mediator list in dtor [%s]", m_mediatorDebugName.c_str ())); // Debug: name + catch the culprit
    WARNING (true, ("CuiMediator dtor: '%s' still in s_mediators -- self-removing (Release dangling-ptr UAF guard, CONSULT-48)", m_mediatorDebugName.c_str ()));
    IGNORE_RETURN (s_mediators.erase (it)); // Release: heal the invariant so garbageCollect never deactivates a freed mediator
}
```
**Fix 2 — garbageCollect erase-FIRST + restart-on-mutation (kills iterator-invalidation + freed-deref):**
```cpp
void CuiMediator::garbageCollect (bool force)
{
    bool removedAny = true;
    while (removedAny)
    {
        removedAny = false;
        for (MediatorVector::iterator it = s_mediators.begin (); it != s_mediators.end (); ++it)
        {
            CuiMediator * const mediator = NON_NULL (*it);
            if (mediator->getRefCount () == 0 && (force || !mediator->isOpenNextFrame ()))
            {
                REPORT_LOG_PRINT (...);
                s_mediators.erase (it);   // erase BEFORE deactivate/delete can mutate s_mediators
                mediator->deactivate ();
                delete mediator;
                removedAny = true;
                break;                    // list may have mutated (nested deletes) -> restart scan
            }
        }
    }
    if (UIManager::isUIReady()) UIManager::gUIManager().garbageCollect();
}
```

## Review questions (answer YOUR assigned angle)
- Codex (trace): find WHO deletes a CuiMediator directly (bypassing garbageCollect) while still in s_mediators
  during ExitChain shutdown — the upstream culprit (complements the Debug-FATAL runtime catch). cite file:line.
- Cursor (correctness): do Fix1+Fix2 fully close the UAF + iterator-invalidation? edge cases: erase-before-deactivate
  behavior change; re-entrant garbageCollect; double-erase (garbageCollect erases then dtor find misses); does
  `getRefCount()/isOpenNextFrame()` deref a freed object before we ever erase (i.e. is the freed pointer read at
  axiom-4 `*it`/getRefCount BEFORE my erase)? cite file:line.
- Sonnet (lateral): is fixing garbageCollect the right layer, or should the UPSTREAM direct-delete be fixed
  (refcount / ExitChain ordering / who owns the mediator)? rank alternatives; cheapest discriminating step.
- Opus (red-team): attack Fix1+Fix2. Does dtor-self-remove change IN-GAME garbageCollect (not just shutdown)?
  Could it mask a real refcount/ownership bug? Is erase-before-deactivate safe (deactivate relying on registration)?
  Does it actually eliminate eip=0 or move it? Verdict: SOLID / UNSUPPORTED parts + the single highest-confidence action.
