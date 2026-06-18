---
phase: 33-x64-build-platform-d3d9-renderers
plan: 06
subsystem: build
tags: [x64, boot-validation, char-select, cdb, cross-ai-crew, memory-manager, bink, text-filter, size_t-truncation, bits-02, milestone]

# Dependency graph
requires:
  - phase: 33-x64-build-platform-d3d9-renderers
    plan: 05
    provides: "The first fully-linking x64 client (SwgClient_*.exe + gl05/06/07_*.dll, machine 8664, staged to stage-x64/)"
provides:
  - "A BOOTING, PLAYABLE x64 SWG client: boots to character select (dressed), loads Tatooine at render parity, runs the Mos Eisley cantina crash-free under rasterMajor=5 (gl05). User-confirmed 2026-06-17."
  - "stage-x64/ runtime: full x64 DLL set (incl. the 3 staging-gap corrections ICU libicuuc.dll+icudt62.dll, mss64.dll, DllExport.dll); every binary dumpbin machine 8664; lifted-DLL SHA256s match the checklist"
  - "Six residual class-(A) x64 runtime bugs root-caused (cdb + cross-AI crew) and fixed -- the 'tail' Phase 31's compile-warning sweep could not catch"
  - "A1-DBGHELP-RIP resolved: DebugHelp call-stack entries widened uint32->uintptr_t (Win32 ABI byte-unchanged)"
  - "Diagnostic methodology proven for the x64 residual tail: cdb break/dump + cross-AI consultant crew (de-anchoring on runtime ground-truth)"

status: complete
result: PASSED
verified_by: user-playtest (boots/renders are truth)
completed: 2026-06-17
---

# 33-06 SUMMARY -- x64 boot validation (PHASE 33 GATE) -- PASSED

**The v3.0 x64 Port milestone deliverable is achieved: a native 64-bit SWG client boots to character
select, loads the world at parity, and plays crash-free.** This plan was the boot-validation gate;
in practice it surfaced and fixed the residual runtime-only x64 bugs (the "class-(A) tail" that
Phase 31's warnings-as-errors sweep cannot detect because they are runtime/data-dependent, not
compile-visible).

## Task 1 -- staging + A1-DBGHELP-RIP (committed 4f4267072 + staging fixes)
- `stage-x64/` populated with the full x64 runtime set; every binary `dumpbin /headers` == machine **8664**.
- **DBGHELP-RIP widen** (`4f4267072`): the call-stack entry contract `uint32 -> uintptr_t`
  (DebugHelp + CallStack/CallStackCollector/Fatal/RenderWorld/MemoryManager callers). On Win32
  `uintptr_t`==4B so the x86 ABI is byte-unchanged; both builds relink clean (0 unresolved).
- **Staging-gap corrections** (runtime DLLs the Plan-01 checklist marked N/A or omitted; staged from
  the Restoration x64 reference, SHA256-recorded):
  - `libicuuc.dll` + `icudt62.dll` -- ICU, imported by the lifted `libxml2.dll` (the X64-04 "icu N/A"
    line was wrong; the lifted libxml2 was built WITH ICU encoding support).
  - `mss64.dll` -- Miles, imported by `binkw64.dll` for video audio.
  - `DllExport.dll` (x64) -- statically imported by all 3 plugins at load (was missing from the manifest).

## Task 2 -- x64 boot to character select (the human-verify gate) -- PASSED after 6 fixes
The boot gate surfaced six runtime-only x64 bugs. Each was root-caused with **cdb** (break/dump) and
the **cross-AI consultant crew** (Codex/Cursor/fresh-Opus/fresh-Sonnet on non-overlapping angles),
then fixed:

1. **`eeeb0c30b` -- SystemAllocation x64-unsatisfiable assert (boot-blocker).** A `!=` size assert
   structurally impossible on x64 (`sizeof(SystemAllocation)`==24 vs `cms_blockSize`==32, a multiple
   of 16) fired on the FIRST allocation during static init -> Fatal -> uninitialized-mutex AV.
   Relaxed to the genuine "header fits" overflow check.
2. **`f65994f66` -- MemoryManager free-block undersize -> next-block `m_previous` stomp (boot-blocker).**
   A min-size allocation is 48B on x64 but `addToFreeList` writes a 64B `FreeBlock` node, overrunning
   8B into the next block's `m_previous` (zeroed). x86 was safe by alignment coincidence (min rounds
   to exactly 32==`cms_freeBlockSize`). Cross-AI math (Opus + Cursor) derived it independently. Fix:
   clamp `allocSize` and the split remainder to `>= cms_freeBlockSize`.
3. **`3486b9b56` -- Bink x64 procs bound by undecorated name (boot-blocker).** `BinkDLL._construct`
   requested x86 `__stdcall`-decorated names (`_BinkSetMemory@8`); the x64 `binkw64.dll` exports
   undecorated names -> every bind NULL -> `BinkSetMemory()` call to 0x0. `_bind` now strips the
   `_`/`@N` decoration on `_M_X64`.
4. **`9841ef52c` -- NULL reticle-mediator guard in `SwgCuiAllTargets::performDeactivate`.** Latent
   null-deref (lazily-created reticles deref'd unconditionally on HUD deactivate); the main-loop SEH
   recovered it but it was a first-chance AV in the boot path. Guarded.
5. **`68691b303` -- TextManager profanity-filter `size_t->int` truncation (THE big one: ONE root,
   THREE faces).** `unsigned int index = find(...)` and `int startIndex = npos` truncated the 64-bit
   `size_t`/`npos`; the no-match guards never matched npos on x64 -> `text[0xFFFFFFFF + i] = ...`
   wild store. Faces: **(a)** Debug `xstring` "string subscript out of range" assert (the bounds
   check halts the write); **(b)** Release heap corruption surfacing later as the `e06d7363` STL
   throw at a decoupled site (the "cantina crash"); **(c)** Release **naked character in char-select**
   (the corruption mangled wearable/appearance state). The "Debug dressed / Release naked" A/B was
   never an optimization bug -- it was the Debug assert masking the corruption. Fix: `index` /
   `findStartPosition` / `startIndex` are `Unicode::String::size_type`; compare `npos` directly.
   cdb-captured stack + crew (Opus's "heap corruption, throw far from the cause" verdict) cracked it.

**Boot validation result (user-confirmed 2026-06-17):** x64 client boots to character select with
the character **dressed**, loads Tatooine at render parity, and runs in/out of the Mos Eisley
cantina **crash-free** under rasterMajor=5 (gl05). Audio silent by design (Miles stubbed on x64;
real port is Phase 35).

## Task 3 -- 32-bit non-regression (SC#4)
Win32 5-target build relinked clean (0 unresolved, machine 14C); shipped `stage/` binaries untouched
by the x64 work (distinct x64 OutDirs). x64 binaries live in `stage-x64/`, isolated from `stage/`.

## Notes / open items
- **Known separate issue (NOT this bug):** windowed-D3D9 device-loss on alt-tab does not recover
  (spins in `TestCooperativeLevel`). A debugging-ergonomics nuisance, not a boot/play blocker --
  candidate for a follow-up.
- **gl06/gl07 (rasterMajor=6/7)** were not separately boot-walked this session (gl05 carried the
  validation; the renderers share the now-fixed engine/MemoryManager/text paths). Low-risk follow-up.
- Requirements X64-01 / X64-02 / X64-04 satisfied (native x64 binaries; gl05 boots char-select;
  all third-party x64 DLLs resolve, no missing-DLL at boot).

## Self-Check: PASSED
- x64 client boots to character select (user-confirmed), dressed, Tatooine parity, cantina crash-free.
- All 6 fix commits present; both x64 configs relink clean (0 unresolved, 0 error C); stage/ (Win32) untouched.
- Found via cdb + cross-AI crew; the TextManager fix resolved all three reported Release symptoms.
