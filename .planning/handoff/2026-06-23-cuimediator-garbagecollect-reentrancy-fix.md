# 2026-06-23 — CuiMediator::garbageCollect re-entrancy fix (ROOT of the intermittent shutdown UAF)

**Phase 38-09 gap-closure.** Build-gate autonomous (client NOT run). Single source change:
`src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp`. No shared-header
change, no plugin ABI cascade, `UTINNI_HOOKPOINTS_VERSION` still **3**.

## The MEASURED root (CONSULT-48)

The intermittent shutdown crash was `c0000005` with **`eip=00000000`** — a CALL through a null vtable
slot (`CuiMediator::getCallbackObject`, virtual, `+0x30`), i.e. `garbageCollect` was calling
`deactivate()` on an **already-freed mediator** (use-after-free). Stack: ExitChain SHUTDOWN teardown
(`CuiManager::remove <- ExitChain::run`), no Utinni injected.

The Debug build's own FATAL named the mechanism:

```
CuiWorkspace.cpp:335  Can't removeMediator [SwgCuiGroundRadar] during m_iteratingEnabledStates
```

**The re-entrant cascade** (the actual root):

```
CuiMediator::garbageCollect                     (outer pass)
  -> mediator->deactivate()
     -> CuiWorkspace::updateMediatorEnabledStates   (sets m_iteratingEnabledStates = true)
        -> SwgCuiAllTargets::removeUnusedStatusPages
           -> CuiMediator::garbageCollect           (RE-ENTRANT)
              -> delete mediator
                 -> CuiWorkspace::removeMediator     (erase + release WHILE iterating)
                    => invariant violation: DEBUG_FATAL in Debug,
                       use-after-free (dangling s_mediators ptr later deactivated) in Release
```

The nested `delete -> removeMediator` mutates the container mid-iteration; the dangling pointer left
in `s_mediators` is deactivated by a later pass → the eip=0 null-vtable call.

## The fix (garbageCollect-only)

Rewrote `CuiMediator::garbageCollect(bool force)`:

1. **Re-entrancy guard** — `static bool s_collecting`; a nested call returns early. Refcount-0 mediators
   are reaped by the continuing outer loop (`force=true` at shutdown) or the next frame's collect.
   Normal (non-re-entrant) collection is unchanged.
2. **Erase-BEFORE-deactivate/delete + restart-on-mutation** — erase the entry first (cascade can't
   invalidate a live iterator), then `deactivate()`, then `delete`; `break` and restart the scan after
   each removal. O(n²) worst-case but n is small (teardown / a few collects per frame).
3. `UIManager::garbageCollect()` kept OUTSIDE the guard (after `s_collecting=false`), matching original.

**NOT changed:** the dtor (the crew-considered "Fix1" dtor self-remove is DROPPED — Codex+Sonnet proved
no external direct-delete; the re-entrancy guard covers the real mechanism), `SwgCuiAllTargets`,
`CuiWorkspace`.

Correctness checks (verified, not blindly trusted): `deactivate()` does not depend on `this` being in
`s_mediators`; the dtor's `find(this)` at ~:265 now misses (already erased) → no double-erase; the
re-entrancy guard already blocks the nested GC so erase-before-deactivate is cascade-safe.

## Supersession

This **SUPERSEDES** the per-site member guards:
- 38-06 — `m_objectCallbackVector` null-guard in `CuiMediator::deactivate`
- 38-08 — `m_callback` null-guard in `SwgCuiHudWindowManagerGround::handlePerformDeactivate`

Those were three faces of the SAME freed-mediator-deactivate. They **remain in place as harmless
defense-in-depth** — no need to revert them.

## Build gate (PASS)

`$env:MSBUILD` (VS18 v145), PowerShell, `/nodeReuse:false`, serial; rebuilt `clientUserInterface`
(Debug AND Release) before relinking each `SwgClient` config; deleted staged exes first.

| Config | Platform | unresolved external symbol | staged exe |
|--------|----------|----------------------------|------------|
| Debug   | Win32 | 0 | `stage/SwgClient_d.exe` |
| Release | Win32 | 0 | `stage/SwgClient_r.exe` |
| Debug   | x64   | 0 | `stage-x64/SwgClient_d.exe` |
| Release | x64   | 0 | `stage-x64/SwgClient_r.exe` |

Logs: `.planning/research/build-38-09-{dbg,rel}-{win32,x64}.log` (0 `unresolved external symbol`,
0 `LNK1181` each). Repro is Win32, milestone is x64; `clientUserInterface` links into `SwgClient` on
both — built both.

## Maintainer controls (boot-smoke + live-inject DEFERRED)

Re-run the SAME exit/shutdown that produced the 18xx dumps:
- **Expect NO crash.**
- **Debug build should NO LONGER hit `CuiWorkspace.cpp:335` FATAL** — the re-entrant GC is gone.
- **If the Debug FATAL STILL fires**, the re-entrant path has another caller — paste the new FATAL
  caller-chain (the re-entrancy guard returns early, so a surviving FATAL means a path that mutates
  `s_mediators`/`m_mediators` WITHOUT going through `garbageCollect`).
