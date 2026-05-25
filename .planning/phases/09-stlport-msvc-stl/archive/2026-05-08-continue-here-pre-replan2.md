---
phase: 09-stlport-msvc-stl
plan: 01
status: halted-architectural-test-in-progress
halted_at: 2026-05-08
reason: dll-abi-mismatch-with-pre-built-proprietary-plugins
last_commit_pre_revert: edad2dabe
last_known_good: 35b872357
build_state: source rolled back to 35b872357; build/bin/Debug/SwgClient_d.exe is the 14:47 post-Wave-2 build (MSVC STL containers) UNCHANGED — being tested against swapped 2016 SWGSource DLLs
test_in_progress: 2016-SWGSource-v3.0-DLL-swap (gl05/06/07/dpvs/DllExport replaced with 2016 community rebuilds; originals saved as build/bin/Debug/*.original-2010.bak; awaiting boot test against post-Wave-2 EXE)
---

# Phase 09-01 — Halted at Runtime (Path C round 2: rollback + replan)

User invoked `/gsd-execute-phase 9` 2026-05-08, ran the full Wave 2 sweep (Tasks 1–7
of 09-01-PLAN), build came clean. Manual VM boot test failed: SwgClient_d.exe
crashed inside `gl05_d.dll!62A1B490` with `0xC0000005: Access violation reading
location 0xB93725CC` — a wild address well above any user-mode heap, the DLL
reading garbage at a field offset that doesn't correspond to a valid pointer in
the new EXE struct layout.

User chose **rollback + replan**, paralleling the prior Path-C halt pattern.

## Critical Anti-Patterns

| pattern | severity | why |
|---------|----------|-----|
| Resuming `/gsd-execute-phase 9` against the current PLAN.md | blocking | The plan as designed cannot succeed runtime-wise. `Wave 2 boot gate` is structurally unreachable while `gl05_*.dll` (and any other proprietary plugin DLL) is pre-built against STLPort and the EXE is built against MSVC STL. Re-running this plan reproduces the same crash. |
| Treating the failure as a per-plan or per-file bug | blocking | It is not. The failure mode is the EXE/DLL ABI surface. Every class that crosses the proprietary-plugin boundary AND contains an STLPort container (vector / hash_map / map / etc.) becomes layout-incompatible the moment `StlForwardDeclaration.h` switches typedefs from STLPort aliases to MSVC `std::*`. Per-file source edits cannot fix it. |
| Trusting source-level spike validation as runtime validation | blocking | Spike 001c (validated 2026-05-08) demonstrated that ~40 client-side files compile + link clean under the big-bang sweep. Spikes 001a/001b/001c were all source-level scope checks. None of them booted the resulting binary against the VM. The runtime ABI break is invisible until the binary is launched and a proprietary-plugin DLL is dlsym'd. |
| Reverting the Phase-9 work as wasted effort | warning | The reverted source diffs (19 commits, 69 files) are a correct rendering of the migration intent. They are useful research artifacts for the replan — keep them in `git reflog` for reference. The .continue-here from the prior halt + the spike 001a/001b/001c notes already document the compile-side findings. |

## What was rolled back (single revert commit)

19 source commits (13 Wave-1 + 6 Wave-2) consolidated and rolled back via
`git checkout 35b872357 -- src/` + explicit removal of the added
`StlCompat.h` file. Reverted ranges:

- **Wave 1** (`df4bd21d3..831304d35`, 13 commits) — author StlCompat.h, remove
  STLPort include blocks at root + tier-parent CMake, sweep shared-tier
  `<hash_map>` / `<hash_set>` to StlCompat.h, wire StlCompat.h into
  StlForwardDeclaration.h, plus 8 portability fixes from `831304d35`.
- **Wave 2** (`8b45e279f..edad2dabe`, 6 commits) — remove engine/client
  tier-parent STLPort include block, sweep client-tier (5 engine/client
  files + 7 ui files + 3 UiBuilder/LocalizationTool + 3 swgClientUserInterface),
  plus 30 portability fixes from `edad2dabe` (mix of Rule-1 latent bugs unmasked
  by stricter MSVC iterators + Rule-3 idiom fixes).

Repo source state after rollback: byte-identical to `35b872357`
(`test(09-00.3): record exit_leak_count=0`). Docs/research/replan commits
between `35b872357` and HEAD are PRESERVED — only `src/` was reset.

## The architectural finding (NEW — beyond what spikes 001a/001b/001c surfaced)

`gl05_d.dll`, `gl06_d.dll`, `gl07_d.dll`, `dpvs.dll`, `dpvsd.dll` and likely the
rest of the May-5-08:54-dated DLLs in `build/bin/Debug/` are **pre-built 2010-era
SOE production binaries** (compiled MSVC 7.1 + STLPort 4.5.3). They are NOT built
from source by this CMake project. The build system at
`src/game/client/application/SwgClient/src/CMakeLists.txt:213` literally copies
them out of `exe/win32/` and renames `gl05_o.dll` → `gl05_d.dll` because "gl05_d.dll
does not exist in the repo (SOE internal only)".

The Direct3d9 plugin source DOES exist in the repo at
`src/engine/client/application/Direct3d9/` (49 tracked files), but is **explicitly
deferred to Phase 12** in `src/engine/client/application/CMakeLists.txt`:

```cmake
# add_subdirectory(Direct3d9)   # multiple: jpeglib.h missing,
                                # __pfnDliNotifyHook2 const,
                                # Pass::apply missing return
```

ABI surface example — `ShaderImplementation` (passed by `const&` across the
plugin boundary at `Graphics.cpp:1543`):

- members include `Passes *m_pass` (pointer to `stdvector<Pass*>::fwd`) and
  `std::vector<Tag> m_optionTags`.
- `stdmap`/`stdvector` are SWG-defined typedefs that resolve through
  `sharedFoundation/StlForwardDeclaration.h`.
- Pre-Wave-1: those typedefs aliased to STLPort `_STL::vector`/`_STL::hash_map`,
  matching the DLL's ABI exactly. ABI compatible.
- Post-Wave-1: those typedefs alias to MSVC `std::vector`/`std::unordered_map`.
  Outer struct member offsets may match, but the inner vector representation
  (`begin/end/capacity` pointer triple, EBCO of allocator) differs in size and
  layout between STLPort 4.5.3 and MSVC 14.x's `<vector>`. The DLL reads the
  EXE's struct as STLPort layout, dereferences `_M_start` from a wrong offset,
  and crashes on the first wild pointer.

This finding does not invalidate spikes 001a/001b/001c — they correctly
characterized the compile-time scope. It adds a SECOND axis the prior research
missed: **runtime ABI compatibility with pre-built proprietary plugin DLLs**.

## Implications for Phase 9 as scoped

The phase goal as written ("STLPort → MSVC STL, boot to char-select against
SWGSource VM") is **not achievable** while the proprietary plugin DLLs are
pre-built and STLPort-shaped. Any of the following must be true for Phase 9
to succeed runtime-wise:

1. The Direct3d9 plugin (and any other ABI-exposed proprietary plugin) is
   rebuilt from source against the new STL — i.e., resolve the three Phase 12
   blockers in scope of Phase 9. Estimated cost: days/weeks of legacy-source
   modernization across 49 tracked files. Plus dpvs (~70+ files vendored at
   `src/external/3rd/library/dpvs/`).
2. The migration is restructured to be ABI-surface-preserving — classes that
   cross any proprietary-plugin boundary keep STLPort container typedefs;
   only EXE-internal classes switch to MSVC STL. Per-class control of
   StlForwardDeclaration.h aliases. Spikes 001a/001b said per-target/coexistence
   is structurally hard; per-class is a more granular variant of the same
   approach and may be infeasible for the same reason (`#define std STLPORT`
   from STLPort's `_epilog.h` is a TU-wide rewrite).
3. Phase ordering is changed. Move Phase 11 (D3D11 renderer plugin) BEFORE
   Phase 9 — once we own the renderer plugin source, the ABI surface is no
   longer pre-built and we control both sides of every container that
   crosses it. STL migration becomes mechanical at that point. Trade-off:
   Phase 11 was estimated 2-4 months, vs Phase 9's previously-estimated
   "one session".

## Suggested research probes (next replan input)

Useful research questions for the architecture decision:

- **DLL surface inventory.** Which proprietary plugin DLLs does SwgClient
  actually load at runtime, and which exported entry points take struct types
  containing STL containers? `dumpbin /exports gl05_d.dll`, then cross-reference
  exported symbols against `Graphics.cpp` and `SetupDll.cpp`. Identify the
  full ABI-exposed class set.
- **Direct3d9 plugin rebuild cost.** What does it actually take to bring
  `src/engine/client/application/Direct3d9/` into the CMake graph? Resolve the
  three deferred blockers (jpeglib.h missing → vendor `libjpeg-turbo` or stub,
  `__pfnDliNotifyHook2 const` → modern delay-load API const-ness, `Pass::apply
  missing return` → add return statement). Time-box a spike (~2-4 hr).
- **dpvs plugin rebuild cost.** Same question for the DPVS occlusion culling
  plugin. Source vendored at `src/external/3rd/library/dpvs/{interface,implementation}`.
  Likely needs less work than D3D9 (no D3D dependency).
- **StellaBellum / swg-main reference.** Has any other modernization fork
  resolved the EXE/DLL ABI break? Check their renderer plugin status — are
  they running with the original SOE DLL, a from-scratch replacement, or a
  rebuilt-from-source D3D9?
- **Per-class StlForwardDeclaration.h.** Could the typedef wrapper resolve
  conditionally based on `#ifdef _IS_PLUGIN_BOUNDARY_CLASS` macro guards
  added per-class? Likely defeated by the same `#define std STLPORT` issue
  (textual identifier rewrite is undefeatable, per spike 001b).
- **Phase reordering: D3D11 first.** If D3D11 (currently Phase 11) is moved
  before STL migration (currently Phase 9), the renderer plugin becomes
  source-controlled and the ABI surface goes away. Estimate the trade-offs:
  D3D11 development on a STLPort EXE is harder (more legacy idioms to work
  around), but STL migration on a from-source-renderer EXE is mechanical.

## Memory facts already known

From user memory:
- `project_v2_scope`: STLPort→MSVC STL is Phase 1, DPVS experiment is Phase 2,
  D3D11 renderer is Phase 3 (sequenced as Phases 9 / 10 / 11 in the active
  roadmap).
- `feedback_source_edits_allowed`: v2 explicitly authorizes C++ source edits.
  Both rebuilding Direct3d9 from source AND rewriting StlForwardDeclaration.h
  per-class are within budget.
- `project_post_v1_exploration`: prior STLPort `mbstate_t` ABI crash series
  documents that STLPort `basic_filebuf` is broken under MSVC 2022 UCRT
  anyway — STLPort fstream I/O is a known runtime hazard, additional motivation
  to remove it.
- `project_stlport_fstream_crash`: STLPort fstream `_M_setup_codecvt` crash
  on codecvt vtable mismatch.
- Boot test target: SWGSource StellaBellum VM at 192.168.1.200, character-select
  scene.

## Recommended re-entry sequence

When resuming Phase 09:

1. **First, read this file in full + the prior `.continue-here.md` (now in
   `git reflog`) + the SUMMARY.md from spikes 001a/001b/001c.** The next
   replan must reconcile both findings (compile-time scope + runtime ABI).
2. **Finish the in-progress 2016 SWGSource DLL swap test** before doing
   anything else (see "Empirical Probes Already Run" below). If that test
   has not been completed yet (no clear PASS/FAIL recorded in this file),
   resume it FIRST — it potentially makes Strategy 3 a no-op.
3. **Run the suggested research probes above** before authoring a new PLAN.md.
   At minimum: dumpbin on the proprietary DLLs to enumerate the ABI surface
   (already done — 1 export per gl05/06/07, 140 for dpvs), plus a 2-4 hr
   spike on the Direct3d9 rebuild cost (NOT done; pending the 2016 swap test
   outcome).
4. **Decide between three architectural strategies** (numbered above):
   rebuild plugins, ABI-surface-preserving migration, or phase reordering.
   Each has very different scope; the choice drives the entire PLAN.md.
5. **Replan 09-01 (and possibly all of Phase 9)** to reflect the chosen
   strategy. The previous PLAN's wave structure (`09-00..09-04`) may not
   survive the replan.
6. **Then** execute. Do NOT re-spawn an executor under the rolled-back
   PLAN.md without addressing the architectural strategy choice.

## Empirical Probes Already Run (this session, 2026-05-08)

### gl05_d.dll exports
Exactly **1**: `GetApi`. All ABI surface concentrated in the `Gl_api` struct
returned. Struct definition lives in repo at
`src/engine/client/library/clientGraphics/src/win32/Gl_dll.def`. Counted
the function-pointer fields that take container-bearing classes: ~12
classes (Shader/StaticShader/*VertexBuffer/*IndexBuffer/Texture/Light/etc.)
+ 2 explicit `stdvector<>::fwd&` params. The ABI fence is small — see
memory `project_dll_abi_fence_classes.md` for the exact class list.
Same `GetApi` export pattern for `gl06_d.dll` and `gl07_d.dll`.

### dpvs.dll / dpvsd.dll exports
**140** mangled C++ class methods (`Commander`, `Frustum`, `Services`,
`Camera` ctors/dtors, etc.). Full class-instance ABI surface — much
bigger than D3D9 plugins, harder to fence. DPVS source is vendored at
`src/external/3rd/library/dpvs/{interface,implementation}` but is also
NOT in the CMake build; same copy-from-`exe/win32/` pattern.

### 2016 SWGSource Client v3.0 rebuild candidate (TEST IN PROGRESS)
`../SWGSource Client v3.0/` ships rebuilt versions of all four proprietary
plugin DLLs with PE timestamps **2016-06-17** (5 years after SOE shut
SWG down):
- `gl05_r.dll` md5 `f7e92af1757a5aa145cc7c92b78fe6a6` (vs leak `28a995...` `3cf660...`)
- `gl06_r.dll` md5 `f318bb60397f821e49e9b9d4054d184c`
- `gl07_r.dll` md5 `e78603544b2f680dae603f0b4ed79a92`
- `dpvs.dll`   md5 `126d37dfc9d1a545b39a5b4b908e8a90`
- `DllExport.dll` md5 `f28b8bf7911a59e22e145a5da18f873e`

Both 2010 leak and 2016 SWGSource DLLs statically link the CRT (no
`msvcr*.dll`/`msvcp*.dll`/`vcruntime*.dll` imports), so dumpbin can't tell
us which compiler the 2016 rebuild used. **The empirical test is whether
the 2016 DLLs match the post-Wave-2 EXE's MSVC STL ABI.**

### Test setup as of session pause

Working tree state when test was set up:

- `build/bin/Debug/{gl05,gl06,gl07}_{d,o,r}.dll` ← copies of 2016 SWGSource
  v3.0 `gl0X_r.dll` (renamed `_r` → `_d`/`_o`/`_r` to satisfy the build's
  load convention)
- `build/bin/Debug/{dpvs,dpvsd}.dll` ← 2016 SWGSource `dpvs.dll` (used for
  both Debug-flavor names)
- `build/bin/Debug/DllExport.dll` ← 2016 SWGSource `DllExport.dll` (this
  file was previously absent in `build/bin/Debug/` entirely; the gl05 DLL
  imports `DllExport.dll`, so this might or might not have been satisfied
  by the 2010 build)
- `build/bin/Debug/*.original-2010.bak` ← 2010 leak originals preserved
  (gl05_d, gl05_o, gl05_r, gl06_*, gl07_*, dpvs, dpvsd)
- `build/bin/Debug/SwgClient_d.exe` ← UNCHANGED, still the 14:47 post-Wave-2
  build (MSVC STL container layouts on EXE side). This is the test EXE;
  do NOT rebuild before the test or you lose the migrated-shape EXE and
  the test becomes uninformative.

### How to run / interpret the test

Launch `D:\Code\swg-client\build\bin\Debug\SwgClient_d.exe` against a running
SWGSource VM at 192.168.1.200:44453.

| Outcome | Interpretation |
|---|---|
| Boots to login / char-select | 2016 DLLs use MSVC STL ABI. Drop-in solution; Strategy 3 is done by community. Bake in the swap, replan around using these DLLs as the runtime target. |
| Same crash inside `gl05_d.dll!XXXXXXXX` (access violation, wild address) | 2016 DLLs are also STLPort-shaped. No help. Restore originals: `for f in build/bin/Debug/*.original-2010.bak; do mv "$f" "${f%.original-2010.bak}"; done`. Then proceed with Strategy 2 (ABI-surface fence) per the class set in `project_dll_abi_fence_classes.md`. |
| Different crash / earlier failure / missing dep | Some other shift. Capture VS debugger stack and characterize before deciding. |

### How to restore the 2010 leak DLLs if test fails

```bash
# Restore from .original-2010.bak files
for f in build/bin/Debug/*.original-2010.bak; do
  mv "$f" "${f%.original-2010.bak}"
done
# DllExport.dll did not exist before; remove the SWGSource one
rm -f build/bin/Debug/DllExport.dll
```
