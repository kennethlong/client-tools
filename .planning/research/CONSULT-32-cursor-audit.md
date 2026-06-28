FIRST: read `.planning/research/CONSULT-32-EVIDENCE-spawnleak.md` in this repo — LOCKED measured facts. Do not re-derive.

YOUR ANGLE (detailed line-level code reader — free-balance audit): Audit the allocation/free balance, line by line, in the subsystems named by the OOM victim stacks. For each, find any `new`/`alloc`/`fetch`/`DuplicateObject`/`Attach`/callback-connect that lacks a matching `delete`/`free`/`release`/`Detach`/disconnect on the object-load or per-frame path:

- `clientSkeletalAnimation`: `Skeleton.cpp` (791,1893), `BasicSkeletonTemplate.cpp` (518), `SkeletalAppearanceTemplate.cpp` (327,426), `SkeletalAppearance2.cpp`, `MeshConstructionHelper.cpp`, `SoftwareBlendSkeletalShaderPrimitive.cpp` (482)
- `sharedObject`: `AppearanceTemplateList.cpp` (400) — is the template cache reference-counted correctly?
- `clientGame`: `TangibleObject.cpp` (319,604), `ClientTangibleObjectTemplate.cpp` (150), `CreatureObject.cpp`, `GroundScene.cpp` (2536) — object create/destroy lifetime.

Cite exact file:line for any imbalance and explain why it leaks during a load of 18,478 objects.

SECONDARY (validate a proposed fix): In `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiAllTargets.cpp`, `removeUnusedStatusPages` was leaking `new SwgCuiStatusGround` objects (raw pointers cleared/erased from `m_statii` without freeing). The proposed fix adds `CuiMediator::garbageCollect(false)` in BOTH cleanup branches (`~CuiMediator` is protected so direct `delete` is illegal; `garbageCollect` reaps refcount-0 not-open-next-frame mediators). Is that fix correct and double-free-safe? Any edge case (e.g. it's a global sweep, not scoped)? Repo root: D:/Code/swg-client-v2.
