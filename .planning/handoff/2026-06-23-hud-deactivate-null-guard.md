# HUD deactivate-teardown m_callback null-guard (2026-06-23, 38-08)

**This is a SECOND, distinct deactivate-teardown null-deref** -- the in-game / ExitChain-shutdown
version of the Utinni gap-doc **Finding-1** `CuiMediator`-deactivate crash. Read the distinction first
so this is not conflated with the two other 2026-06-23 crash-guards:

- **NOT** the texture crash (38-07): that is the intermittent Tatooine cold-zone-in **NVIDIA driver**
  null-deref (`nvwgf2um` `NtGdiDdDDICreateAllocation`) in the async `MeshAppearanceTemplate` ->
  `StaticShaderTemplate` -> gl11 `CreateTexture2D` path. Guarded in `Texture.cpp` +
  `Direct3d11_TextureData.cpp`. Different subsystem, different dump
  (`...120326.mdmp`/`crash-20260623-nvwgf2um-FORENSICS.md`).
- **NOT** the 38-06 `m_objectCallbackVector` null: that guarded `CuiMediator::deactivate()`'s unguarded
  `m_objectCallbackVector->begin()` cleanup loop. **This** crash is a *different member* (`m_callback`)
  in a *different file* (`SwgCuiHudWindowManagerGround.cpp`).

## What crashed

Real crash captured 2026-06-23: `stage/SwgClient_r.exe-unknown.0-20260623152839.mdmp`. Null `m_callback`
deref in `SwgCuiHudWindowManagerGround::handlePerformDeactivate` during ExitChain shutdown teardown
(`garbageCollect -> deactivate` on a torn-down manager). **No Utinni** -- pure in-game shutdown.

## Root

- `m_callback` is owned by the BASE `SwgCuiHudWindowManager`: `new`'d in ctor
  (`SwgCuiHudWindowManager.cpp:155`), `delete m_callback; m_callback = 0;` in dtor (`:324-325`).
- Base `SwgCuiHudWindowManager::handlePerformDeactivate` (`:487`) is SAFE: early-returns on
  `!m_WindowManagerActive` (`:489`); `m_callback` is only nulled in the dtor (by which time
  `m_WindowManagerActive` is already false).
- The Ground override (`SwgCuiHudWindowManagerGround.cpp:659`) calls the base THEN runs its OWN 6
  `m_callback->disconnect(...)` (`:663-669`) with **no guard**. On a torn-down manager (`m_callback==0`)
  the base bails but Ground null-derefs at the first disconnect. **The Ground override never mirrored
  the base's guard.**

## Fix (landed)

`SwgCuiHudWindowManagerGround::handlePerformDeactivate` -- wrapped the 6 `m_callback->disconnect(...)`
calls in `if (m_callback) { ...verbatim... } else { WARNING(...) }`; the leading
`SwgCuiHudWindowManager::handlePerformDeactivate();` stays before the guard (the base is self-guarded).
Behavior-preserving when `m_callback` is valid. Release-visible `WARNING(true, (...))` names the
function + that `m_callback` was already torn down, so a future occurrence self-logs.

Scope is ONLY `SwgCuiHudWindowManagerGround`. The Space variant does not override
`handlePerformDeactivate` (uses the guarded base); the other mediators own separate `m_callback`
members and are not implicated.

Build-gate (autonomous): SwgClient Debug+Release on **Win32 + x64**, 0 `unresolved external symbol`,
fresh exes in `stage/` + `stage-x64/`. No shared-header change (no plugin ABI cascade);
`UTINNI_HOOKPOINTS_VERSION` untouched (still 3).

## Deeper root -- follow-up (defer for a joint decision)

`CuiMediator::garbageCollect` deactivates a (partly) destructed mediator. The per-site guards (38-06
`m_objectCallbackVector`, 38-08 `m_callback`) handle the **symptom**; a `CuiMediator`-teardown-ordering
fix would be the real **cure** (stop deactivating destructed mediators in the first place). Deferred for
a joint decision -- it is a wider-blast-radius change than a per-site null-check.

## Maintainer control

Re-run the same exit / shutdown that produced `...152839.mdmp`:

- If the named WARNING fires (`SwgCuiHudWindowManagerGround::handlePerformDeactivate: m_callback already
  torn down ...`) -> the torn-down path is caught; the null deref is gone.
- If it crashes elsewhere in `deactivate` -> paste the new stack (a different member/site, likely the
  next Finding-1 family member, or the teardown-ordering root above).

Boot-smoke + live-inject are DEFERRED to maintainer (execution mode: do NOT run the client).
