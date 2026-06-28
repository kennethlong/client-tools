# LOCKED EVIDENCE — Tatooine spawn-load memory leak (treat as GIVEN; do not re-derive)

You are one of four independent consultants. Reason from the measured facts below. Do NOT assume the
person briefing you knows the answer — they don't. Your job is to find the dominant leak and/or what
recent change introduced it, from your assigned angle.

## Measured facts (LOCKED)

1. **Build:** 32-bit (x86) Debug SWG client (MSVC v145 / VS18). Custom engine allocator
   (`sharedMemoryManager`) with a fixed 2048 MB pool cap (set in 2016, unchanged).

2. **Crash:** `MemoryManager.cpp:1243 FATAL b0780503: failed allocation attempt` — the pool/process
   address space is exhausted (~2 GB). Pure OUT-OF-MEMORY, not corruption.

3. **When:** ~41–51 s after beginning the **world-load into the Tatooine cantina spawn point**
   (Player 3464.42, 4.00, -4876.41). It OOMs DURING the spawn load, frequently before the first frame
   renders. `WorldSnapshot [tatooine]: 1173 object templates, 11239 root objects, 18478 total objects`.

4. **The OOM victim allocation VARIES run-to-run** (each is just "whoever asked when the pool was
   full" — NOT necessarily the leak source). Observed victim call stacks:
   - (a) `SwgCuiAllTargets → SwgCuiStatusGround → ClientObject::getLocalizedName → std::string`
   - (b) `GroundScene(2536) → TangibleObject → SkeletalAppearance2 → SkeletalAppearanceTemplate(327/426) → BasicSkeletonTemplate(518) → Skeleton(791/1893)`
   - (c) `MeshGeneratorTemplateList → ZlibFile` (skeletal mesh load)
   - (d) `Graphics CreateTexture ... E_OUTOFMEMORY`
   Common theme: instantiating **objects / appearances / skeletons / meshes for entities at spawn**.

5. **Live-allocation census at 150 MB-requested:** 1,250,387 live blocks (≈2.5–3M at OOM). Size
   histogram dominated by SMALL blocks: 8B≈493k, 48B≈131k, 32B≈116k, 24B≈101k, 44B≈84k, 64B≈81k,
   20B≈75k, 16B≈29k... (a broad small-object spray, ~70 allocations per loaded object).

6. **Owner attribution is UNAVAILABLE.** `DO_TRACK` was re-enabled but `DebugHelp::getCallStack`
   returns 0 for 99.97% of blocks on this Windows/MSVC build (the owner-capture path is broken —
   SOE gated `DO_TRACK` to `PLATFORM_LINUX`). So the leaking call site CANNOT be named from runtime data.

7. **GROUND TRUTH (user, high confidence):** this exact login → spawn-into-world-at-the-cantina was
   performed MANY times in recent prior work (the cantina corner-snap investigation, ~2026-06-12/13)
   WITHOUT this OOM. => The spawn-load leak is a **RECENT REGRESSION**, not a long-standing condition.

8. **Ruled out:**
   - `MemoryManager.cpp` Phase-31 change (commit `0fd4a74a8`): diff is logging-format + x64-only casts,
     **inert on the 32-bit build**.
   - 2048 MB cap: unchanged since 2016.
   - `SwgCuiAllTargets.cpp` has a real `new SwgCuiStatusGround` without free in `removeUnusedStatusPages`
     (from the INITIAL commit), but it is **minor** (hundreds of objects, not millions of blocks) and
     a fix (`CuiMediator::garbageCollect(false)` in both cleanup branches) did NOT stop the OOM.

9. **Recent change window** (between the last known-good corner-snap exercise and now) = Phase 31
   (64-bit-correctness foundation, BITS-01/02/03, ~Jun 14-15) + Phase 32 (D3DCompile vertex-shader
   compile port — touches ONLY the .vsh compile path, not object/appearance loading).
   Phase-31 commits that touched FUNCTIONAL code in the object/skeletal/appearance path:
   - `d9ee92088` — port SSE skinning `__asm` → `_mm_*` intrinsics (`MeshConstructionHelper.cpp`,
     `SoftwareBlendSkeletalShaderPrimitive.cpp`) — NOTE: replacing x86 `__asm` necessarily changes the
     32-bit codegen.
   - `8ae95abc6` — "B-GAP-2 pointer-truncations (2 FUNCTIONAL x64 fixes + shared header)".
   - `846a2ded6` — `CreatureObject` attributesOnSet callback `const size_t → unsigned int`.
   - `feaddc08e` — width-reconcile class-(B) members.
   - also: `AlterScheduler.cpp`, `EditableAnimationState.cpp`.
   (This list is DATA, not a conclusion. Some Phase-31 changes are x64-only and inert on x86 — verify.)

## THE QUESTION
What dominant allocation leak exhausts the pool during the Tatooine spawn load, and what RECENT change
introduced it (fact #7 says it's recent)? Repo root: `D:/Code/swg-client-v2`. Cite file:line and commit.
