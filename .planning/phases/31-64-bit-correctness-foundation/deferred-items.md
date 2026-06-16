# Phase 31 — Deferred Items (out-of-scope discoveries during execution)

Logged per the executor scope-boundary rule: out-of-scope blockers discovered while
executing a plan are recorded here, NOT fixed in the current plan.

---

## DEF-31-01: Misc.h:236 `memmove(void*,const void*,int)` C2668 ambiguity (x64) — RESOLVED (plan 31-04, commit 359214d2b)

- **Status:** RESOLVED 2026-06-16 by plan 31-04 (the named owner). Fix applied exactly as recommended:
  `return ::memmove(destination, source, static_cast<size_t>(length));` — the `::`-qualification +
  `size_t` binds the CRT overload unambiguously while the engine wrapper's `int`-length signature is
  unchanged for callers. Verified: full clientGraphics TUs (e.g. `StaticShader.obj`, 676KB) now
  compile to exit 0 in the scratch x64 harness, where the C2668 previously gated them.
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

---

## RESIDUAL-31-04: BITS-02 truncation in NON-owned TUs (handed to plan 06 phase gate)

Discovered during plan 31-04 Task 3 (`-Scope bits02` sweep + reconcile against
`x64-scratch/explicit-cast-audit.log`). These sites are NOT in plan 31-04's `files_modified`,
so they were recorded (not fixed) per the executor scope boundary. Plan 06 (the phase-gate
sweep) owns them. Each is classified (review #4): **(a) in-scope boot-path source that MUST be
fixed in Phase 31** vs **(b) third-party / link-only residue deferrable to Phase 33**.

All 11 owned-file sites the audit listed are RESOLVED in 31-04 (live grep == 0); the audit.log
is a stale plan-01 snapshot.

| Site | Cast | Class | Disposition |
|------|------|-------|-------------|
| `clientGraphics/.../RenderWorld.cpp:1127` | `reinterpret_cast<int>(DPVS::Object*)` for `%08x` DEBUG_REPORT_LOG | (a) in-scope boot-path | **MUST-FIX (Phase 31, plan 06).** Fires implicit C4311 too. Diagnostic-log only (leaked-object trace), but it is live in-scope clientGraphics. Fix: `%p` + `static_cast<void const*>(object)`. |
| `Direct3d9.cpp:137/183/185/203` | `(DWORD)(uintptr_t)hD3d9/pDevice/vtbl/entry` for `%08X` | (a) in-scope boot-path | **MUST-FIX (Phase 31, plan 06).** The `(DWORD)(uintptr_t)` double-cast EVADES /we4311 (review #4 named these). D3D9 base-address diagnostic dump. Fix: print the full `uintptr_t` via `%p`/`%Ix`. (Direct3d9.cpp is NOT in 31-04's files_modified.) |
| `clientSkeletalAnimation/.../EditableAnimationState.cpp:436/489` | `reinterpret_cast<int>(child/link)` for `%08x` WARNING | (a) in-scope boot-path | **MUST-FIX (Phase 31, plan 06).** Diagnostic WARNING text only; clientSkeletalAnimation is in the boot path. Fix: `%p`. |
| `clientUserInterface/.../CuiMediator.cpp:1009` | `reinterpret_cast<int>(mediator)` for `%08x` | (a) in-scope boot-path | **MUST-FIX (Phase 31, plan 06).** Mediator debug-name dump. Fix: `%p`. |
| `sharedDebug/.../LeakFinder.cpp:137` | `reinterpret_cast<int>(...->object)` for `%08X` REPORT | (a) in-scope boot-path | **MUST-FIX (Phase 31, plan 06).** sharedDebug links into the boot path. Leak-report log. Fix: `%p`. |
| `SwgClient/.../WinMain.cpp:88/90` | `reinterpret_cast<int>(HINSTANCE result)` (ShellExecute return) | (a) in-scope boot-path | **MUST-FIX (Phase 31, plan 06).** Same defect class as the Os.cpp:1579 launchBrowser fix landed in 31-04 — mirror it: `INT_PTR result` + `>32`/`%Id`. WinMain.cpp is the SwgClient app entry, not in 31-04's files_modified. |
| `clientGame/.../test/WaterTestAppearance.cpp:97` | `reinterpret_cast<int>(&vertexBuffer)` | (b) deferrable | **DEFERRABLE.** `test/` harness code, not on the boot path; can be fixed opportunistically in plan 06 or carried to Phase 33. Same hash-to-int sort-key shape if it is ever revived. |

**Note:** No genuine third-party (vendored SDK) truncation surfaced in the bits02 scope — every (a)
site is our own engine/app source and must be fixed in Phase 31 (no in-scope boot-path source escapes
via the residual valve, review #3/#4). The only (b) item is in-tree test code, not vendored.

---

## RESIDUAL-31-05: BITS-02 pointer truncation in ByteStream.cpp (NON-owned by 31-05; handed to plan 06)

Discovered during plan 31-05 Task 3 (`-Scope bits03` backstop sweep, 61 TUs). This is the ONLY
residual the bits03 sweep surfaced. It is a **BITS-02 pointer-truncation** defect, NOT a
serialization-width hazard — the bits03 *serialization* mandate (C2664/C2665 overload-resolution
survivors) came back **0** in this plan's `files_modified`, and the Archive `std::map` map-count fix
compiles x64-clean and is exercised by `archive-map-instantiation.cpp` (review #5). ByteStream.cpp is
in the archive lib but is **NOT** in 31-05's `files_modified` (which is Archive.h only), so per the
executor scope boundary it is recorded here, not fixed cross-plan.

| Site | Cast | Class | Disposition |
|------|------|-------|-------------|
| `archive/.../ByteStream.cpp:347` | `reinterpret_cast<unsigned int>(oldData)` (a `Data*` pointer) for an `assert(... != 0xefefefefu)` freed-memory-poison sentinel check | (a) in-scope (archive lib links into the boot path) | **MUST-FIX (Phase 31, plan 06).** Fires C4311 (pointer→`unsigned int` truncation) on x64; the `0xefefefef` debug-poison sentinel only meaningfully compares the low 32 bits, so on x64 a 64-bit poisoned pointer `0x????????efefefef` would still match but `0xefefefef????????` would not. Fix: compare the full-width `reinterpret_cast<uintptr_t>(oldData)` against a `uintptr_t` sentinel (e.g. mirror the engine's existing freed-memory poison-pattern width), or drop the low-32 assumption. Width-correct, not cast-to-silence (D-06). |

**Note (D-07 exclusion):** the pre-existing vector/set/deque `signed int length = source.size()` C4244
(Archive.h:348/360/370) is the documented D-07 exclusion — it is NOT a defect and must NOT be "fixed".
It did NOT surface in this bits03 sweep (the only serialization instantiation exercised was
`std::map<int,int>` via the instantiation TU; the vector signed-int narrowing only fires where a
`std::vector<...>` Archive template is actually instantiated with the size mismatch). Recorded here so
the plan-06 gate executor does not mistake it for a survivor (review #5). The SAFE-by-design fixed-width
paths (IFF, vector/set/deque counts, `int32`/`uint32` = `long` = 32-bit) are unchanged by 31-05.

---

## DEF-31-07: `std::distance` → `int index` C4244 (D-07-EXCLUDED count/distance class, N2 carry-forward)

- **Status:** NOT FIXED — out of (B-GAP) scope. Documented class-(A) residue (N2 / C4267 carry-forward).
- **Discovered during:** plan 31-07 Task 3 verification (compiling EditableAnimationState.cpp to
  exercise the VoidBindSecond.h C4312 fix; the C4312 IS fixed — these are unrelated pre-existing C4244).
- **Sites:**
  - `clientSkeletalAnimation/.../controller/EditableAnimationState.cpp:275` —
    `index = std::distance(m_childStates.begin(), result.first);` (`__int64`→`int`)
  - `clientSkeletalAnimation/.../controller/EditableAnimationStateHierarchyTemplate.cpp:257` —
    `index = std::distance(m_logicalAnimationNames->begin(), result.first);` (`__int64`→`int`)
- **Why deferred:** these are `std::distance` iterator-difference (`ptrdiff_t`/`__int64`) → `int`
  narrowings — the SAME semantic class as the D-07-EXCLUDED `.size()`/distance container-count paths
  (review #5) and the N2 `C4267` Phase-33 carry-forward. They are NOT pointer-truncation defects, NOT
  in this plan's files_modified, and per the plan scope boundary + D-07 must NOT be "fixed" here.
  Phase 33 decides the C4267/count-narrowing policy once the x64 platform + serialization audit exist.

---

## DEF-31-07-AUTODELTAMAP: NEW class-(B) escape — `AutoDeltaMap.h` Archive put/get(size_t) C2668/C2665 (STOP-and-report, NOT fixed)

- **Status:** NOT FIXED — flagged per the plan 31-07 gap-closure Rule-4 discipline
  ("If a NEW class-(B) escape appears that is NOT in the worklist and exceeds this plan's scope,
  STOP and report it -- do not silently defer"). Surfaced while verifying Task 4 (GroupObject's
  -SingleTu substring also pulls the Group*ObjectTemplate TUs).
- **Pre-existing:** YES — confirmed by stashing all 31-07 edits and recompiling on the clean tree;
  the AutoDeltaMap.h errors are present without any 31-07 change. NOT introduced by this plan.
- **Site:** `src/external/ours/library/archive/src/shared/AutoDeltaMap.h`
  - put: `:353 put(target, container.size())`, `:354/:412 put(target, baselineCommandCount)`,
    `:411 put(target, changes.size())` (and `:381 put(target, static_cast<size_t>(0))`)
  - get: `:526/:562/:587 get(source, commandCount)`, `:527/:563/:588 get(source, baselineCommandCount)`
- **Defect class:** the SAME std::map/size_t-in-Archive width defect 31-05 fixed for plain
  `Archive::get/put(std::map<...>)` (review #5 / D-07: on x64 `size_t` = `unsigned __int64`, and
  there is no 8-byte `Archive::get/put` overload, so the call FAILS overload resolution = a
  COMPILE error the harness catches, NOT a silent wire widen). But this is a DIFFERENT
  serialization path: `AutoDeltaMap`'s pack/packDelta/unpack/unpackDelta, plus its `size_t
  baselineCommandCount` MEMBER (`:91`) which is itself serialized as a count.
- **Why NOT fixed here (exceeds 31-07 scope):** (a) it is NOT in the 31-PHASE33-RESIDUAL-WORKLIST
  §(B-GAP) (the authoritative worklist for this gap-closure plan); (b) the correct fix is the
  31-05 uint32_t wire-stable pin applied across ~8 put/get sites AND a width change to the
  `baselineCommandCount` member -- a multi-site serialization-width change in a broadly-consumed
  shared template header under `external/ours/`, with the same exercise-it caveat 31-05 hit (zero
  AutoDeltaMap instantiations may compile under the scoped sweeps; needs an instantiation TU to
  prove the round-trip). That is a coherent unit of work, not a one-liner, and exceeds 31-07's
  4-task (asm / enumerated-ptr-trunc / time_t) scope.
- **Affected in-scope TUs (manifest):** `sharedTemplate/.../ServerGroupObjectTemplate.cpp`,
  `sharedTemplate/.../SharedGroupObjectTemplate.cpp` (both C2668/C2665). The CLIENT GroupObject.cpp
  + ClientGroupObjectTemplate.cpp + sharedGame/SharedGroupObjectTemplate.cpp compile exit 0.
- **Recommended disposition:** a follow-up gap-closure increment (31-08) OR fold into the resumed
  plan 31-06, applying the 31-05 pattern: pin every AutoDeltaMap count put/get to `uint32_t`
  (binds the existing 4-byte overload, wire byte-identical to the 32-bit server), widen/pin
  `baselineCommandCount` consistently, overflow-guard the puts, and exercise via a scratch
  AutoDeltaMap-instantiation TU. This is class (B) — NOT eligible for Phase-33 deferral (review #4).

---

## DEF-31-07-FULLSWEEP: post-31-07 `-Scope all` reveals a LARGE pre-existing class-(B) surface the 31-06 worklist undercounted (STOP-and-report)

- **Status:** REPORTED, NOT FIXED. The 11 enumerated (B-GAP) survivors ARE cleared (this plan's job).
  But the authoritative post-31-07 `-Scope all` sweep (snapshot:
  `.planning/research/scope-all-31-07-final.out` + `...-worklist.log`) shows the 31-06 "(B-GAP)
  survivor set" was INCOMPLETE: ~154 in-scope TUs still fail x64 compile. ALL confirmed pre-existing
  (independent of every 31-07 edit; none of the 154 are this plan's 11 target TUs).
- **The (B-GAP) survivor set IS cleared:** 0 C4235 anywhere; the only C4311/C4312 left are
  `WaterTestAppearance.cpp:97` (documented A2-WATERTEST, class-A test code) + `Bink/BinkTreeFileIO.cpp`
  (7, see below); the 75 C4244 are the D-07-EXCLUDED `__int64`→int count/distance + STL-header noise (N2).
- **The undercounted surface (~154 failing TUs):**
  - **~753 C2665/C2668 (DOMINANT) — `AutoDeltaMap`/`AutoDeltaPackedMap` + the Tpf/Template object-template
    family.** `Archive::put(target, container.size())` / `get(source, size_t)` fail overload resolution
    on x64 (`size_t`=8 bytes, no 8-byte Archive overload) — the SAME defect class 31-05 fixed for plain
    `Archive::get/put(std::map)`, but across the AutoDelta* templates + their `size_t baselineCommandCount`
    member. Fix = the 31-05 uint32_t wire-stable pin applied across the AutoDelta* count put/get sites +
    member, exercised by an instantiation TU. A coherent unit of work; class (B), NOT Phase-33 deferrable.
    The C2065/C2059/C2143/C2238 parse-error tail is largely CASCADE from these template failures.
  - **Miles audio callbacks (`clientAudio/Audio.cpp` C2664):** `AIL_set_file_callbacks` signature
    mismatch — this is **Phase 35 (AUDIO-01)** Miles 7.2e→9.x scope, explicitly OUT of Phase 31.
  - **Bink third-party glue (`Bink/BinkTreeFileIO.cpp` C4311/C4312, `BinkVideo.cpp` C4244):** OUR shim
    round-trips `AbstractFile*` through Bink's 32-bit `U32` file-handle callback API (radopen/radread/…).
    FUNCTIONAL on x64 (handle truncation) BUT the fix is constrained by the x64 Bink SDK callback
    signature (`U32` vs a wider handle) — **Phase 33 (X64-04) third-party** territory (A2-3RDPARTY-adjacent).
  - Remaining D-07/N2 C4244 (`BitStream.cpp`, `BoxTree.cpp`, …): the excluded count/distance class.
- **Why this is STOP-and-report (not fixed here):** it is NOT in `31-PHASE33-RESIDUAL-WORKLIST.md`
  §(B-GAP) (the authoritative worklist for THIS gap-closure plan), and it MASSIVELY exceeds 31-07's
  4-task scope (asm / enumerated-ptr-trunc / time_t). Per the plan's own Rule-4 discipline ("a NEW
  class-(B) escape NOT in the worklist that exceeds this plan's scope -> STOP and report, do not
  silently defer"), it is surfaced here for a USER decision, exactly as the 31-06 finding spawned 31-07.
- **Recommended disposition (user decision):**
  1. A new Phase-31 gap-closure increment (31-08) for the AutoDelta* C2665/C2668 family (the 31-05
     uint32_t pattern; the big, in-Phase-31 class-(B) item) — then resume 31-06 Task 2/3.
  2. Miles `Audio.cpp` → Phase 35 (AUDIO-01) — already scoped there.
  3. Bink `BinkTreeFileIO`/`BinkVideo` → Phase 33 (X64-04) third-party (U32 handle is Bink-API-bound).
  4. `WaterTestAppearance` + D-07/N2 C4244 → unchanged class-(A) residue (Phase 33).
- **Impact on the phase gate:** plan 31-06's D-02 acceptance bar ("all in-scope build-path engine libs
  COMPILE x64-clean") is STILL NOT MET after 31-07 — the AutoDelta* surface remains. 31-06 Task 2/3
  (full 32-bit build + dual-renderer boot smoke) can validate the 32-bit non-regression of the 31-07
  fixes NOW, but the x64-clean certification needs the AutoDelta* increment first.
