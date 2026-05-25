# Phase 9: STLPort → MSVC STL - Research

**Researched:** 2026-05-08
**Domain:** STL ABI swap — STLPort 4.5.3 (vc71 prebuilt + vc143 compat shim) → native MSVC STL (v143/14.44, VS 2022). Includes hash_map/hash_set sweep, Unicode unicode_char_t flip from `unsigned short` to `wchar_t`, removal of /Zc:wchar_t- and /FORCE:MULTIPLE.
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**D-01 (Three-wave structure with boot-to-character-select gate after each wave):**

| Wave | Scope | Gate |
|---|---|---|
| 1 | All `engine/shared/library/*` STLPort removal (sharedFoundation, sharedDebug, sharedFile, sharedMath, sharedThread, sharedCompression, sharedXml, sharedRegex, sharedNetwork, sharedNetworkMessages, sharedObject, sharedTerrain, sharedGame, etc. — ~30 libs). Compat header `<sharedFoundation/StlCompat.h>` introduced (D-02). Hash sweep happens here as part of the per-lib edit. | Boot to character select. |
| 2 | All `engine/client/library/*` + `game/client/library/*` + `game/shared/library/*` STLPort removal. SwgClient + executables relink without STLPort. MFC canaries (D-05) wire here as a positive-signal gate. Cleanup commit removes the `StlCompat.h` aliases inline. | Boot to character select + MFC canaries compile. |
| 3 | Flip `/Zc:wchar_t-` → `/Zc:wchar_t` globally. Migrate `Unicode::unicode_char_t` from `unsigned short` to `wchar_t`. Sweep all `unicode_char_t` casts. Strip `_USE_32BIT_TIME_T=1` if no longer needed (separate task). | Boot to character select + final verification gates (D-04). |

**D-02:** Author `src/engine/shared/library/sharedFoundation/include/public/sharedFoundation/StlCompat.h` aliasing STLPort's `std::hash_map` / `std::hash_set` to MSVC's `std::unordered_map` / `std::unordered_set`. The Wave 1/2 sweep changes `#include <hash_map>` / `#include <hash_set>` to `#include "sharedFoundation/StlCompat.h"` and leaves call-sites alone. After Wave 2 boots clean, a separate cleanup commit migrates each call-site to `std::unordered_map<...>` and deletes `StlCompat.h`.

**D-03:** STL-03 (`unicode_char_t` migration + `/Zc:wchar_t-` removal) happens in Wave 3, after Waves 1 and 2 boot clean. Two distinct test cycles isolate the STL ABI shift from the wchar_t bit-width / mangling shift.

**D-04:** All four verification categories enabled:
- Pre-migration: BEFORE-STL.txt grep snapshot, baseline runtime metrics, hash determinism baseline (Crc::calculate over ~100 strings), round-trip persist baseline.
- Per-wave: boot to character select against SWGSource VM, code/symbol grep re-run, MFC canary build (Wave 2 only).
- Final: dumpbin /imports zero stlport symbols, hash determinism re-run match, round-trip persist re-run deep equality, sort stability check, frame-time + leak-count diff within ±10%.

**D-05:** Two MFC tools wired as Wave 2 success canaries: **Turf** (small, ~5 cpps) and **Viewer** (larger, 26 win32 cpps). Their CMakeLists.txt are already authored; re-enable add_subdirectory() in tier parents.

**D-06:** Round-trip persist test target — researcher to confirm a real data class that already serializes via IFF, internally uses std::string and/or std::vector, and is small enough to verify by deep-equality check in <50 lines. Recommendation NetworkIdManager OR CrcLowerString cache. **(Researcher finding below: actual choice is `DataTable` — see "D-06 Confirmation" section.)**

**D-07:** Boost subset (`boost::shared_ptr`, `boost::scoped_ptr`, `static_assert.hpp`, `config.hpp`) auto-detects STLPort vs MSVC. Researcher to spot-check that the four header references still compile and behave equivalently after STLPort removal. **(Researcher finding below: confirmed clean — see "D-07 Confirmation" section.)**

### Claude's Discretion

- Per-library cadence within Wave 1 / Wave 2 (the boot gate fires only at wave end, not per library).
- Exact location of BEFORE-STL.txt baseline files (recommendation: `.planning/phases/09-stlport-msvc-stl/baseline/`).
- Whether to author the StlCompat.h alias as a `using` template (C++11) or a typedef (legacy). Recommendation: `using` — already authorized as the example in D-02.

### Deferred Ideas (OUT OF SCOPE)

- Boost subset upgrade to a newer boost version (post-v2).
- `/WX` warnings-as-errors re-enablement (v3+).
- STLPort directory removal from repo (separate cleanup commit after Wave 3 verifies clean).
- C++20 / C++23 upgrade (stay on `/std:c++17`).
- Re-enable remaining 13 MFC tools deferred from Phase 8 (Phase 12.x follow-up).
- Re-enable Qt-batch tools (Phase 12.1, independent).

</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| **STL-01** | STLPort include paths, prebuilt libs, FindSTLPort.cmake, stlport_vc143_compat.cpp, _STLP_* defines all gone | CMake removal surface enumerated below: 23 CMakeLists.txt files reference STLPort symbols; root CMakeLists.txt has 5 distinct removal sites (find_package, include_directories BEFORE, _STLP_DONT_FORCE_MSVC_LIB_NAME define, /Zc:wchar_t-, NODEFAULTLIB) |
| **STL-02** | hash_map/hash_set → unordered_map/unordered_set sweep | 89 files matched on raw grep; **42 files** are in SwgClient compile path (server-only excluded). 64 files have direct `#include <hash_map>` / `<hash_set>` directives. ~191 ref count is correct after server exclusion. D-02 alias header reduces sweep to one-line include changes. |
| **STL-03** | Unicode::unicode_char_t flip from `unsigned short` to `wchar_t`; remove `/Zc:wchar_t-`, restore `/Zc:wchar_t` | 414 occurrences across 83 files. Two typedef definition sites: `Unicode.h` line 27 AND `StlForwardDeclaration.h` lines 125 (must edit both for D-03). One concerning pointer-cast site found: `CuiNotepad.cpp:146`. Boost subset auto-fallbacks to MSVC (D-07). |
| **STL-04** | Remove /FORCE:MULTIPLE and /NODEFAULTLIB:stlport_vc71_static.lib | 19 CMakeLists.txt files contain `LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"`; root CMakeLists.txt has the global `/NODEFAULTLIB:stlport_vc71_static.lib`. Direct removal once STLPort is gone (no more vc71 ↔ vc143 ABI gap to bridge). |
| **STL-05** | Client compiles + boots to character select with MSVC STL; dumpbin confirms zero stlport symbols | Boot gate at every wave per D-01. Final dumpbin gate per D-04. Validation Architecture section below specifies exact commands. |

</phase_requirements>

---

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| STL container ABI | Build system (compiler) | Static-link tier | STLPort is selected by include-path order + prebuilt static lib; replacing it is a build-system change |
| Hash table replacement | C++ source (containers) | Build system (header alias) | `hash_map`/`hash_set` (SGI extension under `std::`) → `unordered_map`/`unordered_set` (C++11 standard) |
| Wide character ABI | Build system (compile flag) | C++ source (typedef) | `/Zc:wchar_t` makes wchar_t a built-in type; `/Zc:wchar_t-` made it `unsigned short`. Phase 7 CLEAN-04 removed XPCOM (the only ABI-boundary holdout); flip is now safe |
| Linker dual-symbol bridging | Build system (linker flags) | — | `/FORCE:MULTIPLE` and the compat shim exist solely to bridge prebuilt vc71 STLPort to MSVC 2022 application code. Both go away when STLPort goes. |
| Boost smart pointer subset | C++ source (boost headers) | — | 4 vendored boost headers detect STLPort via `_STLP_*` macros and auto-fall-back to MSVC paths (verified — D-07) |

---

## Summary

Phase 9 swaps the entire STL ABI from STLPort 4.5.3 (vc71 prebuilt static libs glued to MSVC 2022 application code via a custom compat shim and `/FORCE:MULTIPLE`) to the native MSVC v143/14.44 STL that ships with VS 2022. This is a foundational ABI shift, not a refactor: every container in the codebase changes underlying memory layout, every `std::string` flips from STLPort COW to MSVC SSO, every `std::hash<T>` produces different output. Three waves (D-01) isolate the change surfaces: Wave 1 swaps `engine/shared/library/*`, Wave 2 swaps client/game tiers + lights up two MFC canaries (Turf, Viewer) as positive-signal gates, Wave 3 flips `wchar_t` width and unicode_char_t typedef. Each wave gates on character-select boot.

The key risk surface is symbol determinism: SWG persists `Crc::calculateWithToLower` output in TRE asset hashes (read-only contract), and any code that persisted `std::hash<std::string>` output would break silently. The audit found **two server-only** `std::hash<std::string>` use sites (LoginServer, ConnectionServer ClientConnection.cpp) — both outside the v2 client scope, so safe. The CrcLowerString hash is a custom CRC32 (CRC32::calculateWithToLower), unaffected by the STL swap. Round-trip persist target: `DataTable` in `sharedUtility` — already loads from IFF, internally holds `std::vector<std::string>`, `std::vector<DataTableColumnTypeVector>`, `std::string m_name`, AND `stdhash_map<std::string, int> m_columnIndexMap`. It exercises every layer the migration touches.

The hash_map sweep affects **42 in-client-path files**; the rest of the 89 raw-grep hits are server-only or in deferred tools (UiBuilder, SwgGodClient, LocalizationTool, SwgDatabaseServer). The compat header pattern (D-02) reduces each Wave 1/2 file edit to a single `#include` change; call-sites continue to use `stdhash_map<...>::fwd` or `std::hash_map<...>` until the cleanup commit normalizes them. The `/Zc:wchar_t` flip surface is small and concentrated in `external/ours/library/unicode/` — only one concerning raw `unsigned short *` cast was found (`CuiNotepad.cpp:146`), and it's a buffer interpretation that just needs to switch to `wchar_t *`.

**Primary recommendation:** Execute the three waves exactly as D-01 prescribes. Wave 1 begins by deleting `find_package(STLPort REQUIRED)` and the four `WHITENGOLD_USE_STLPORT_HEADERS` blocks (root CMakeLists.txt + three tier-parent CMakeLists.txt), authoring `StlCompat.h`, then sweeping each shared library's `#include <hash_map>` to `#include "sharedFoundation/StlCompat.h"`. Wave 2 sweeps the client/game tiers, removes 19 `/FORCE:MULTIPLE` link flags, deletes `stlport_vc143_compat` target, removes `/NODEFAULTLIB:stlport_vc71_static.lib`, then re-enables Turf + Viewer in their tier parents. Wave 3 flips one line in `Unicode.h` + one in `StlForwardDeclaration.h`, removes `/Zc:wchar_t-` from root CMakeLists.txt's two `CMAKE_CXX_FLAGS_*` strings, fixes `CuiNotepad.cpp:146`, then runs all four final verification gates.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| MSVC STL (v143) | 14.44.x (VS 2022) | C++17 standard library replacing STLPort | Native to the toolchain; no compat shim needed; ships in `vcruntime140.lib` / `msvcp140.lib` (Static CRT: `libcpmt.lib` / `libcpmtd.lib` for /MT / /MTd) [VERIFIED: existing build uses `MultiThreaded$<$<CONFIG:Debug>:Debug>` per root CMakeLists.txt L35] |
| `<unordered_map>` | C++11 | Replaces `std::hash_map` (SGI/STLPort extension) | Standard since C++11; same hash table semantics; container ABI different from STLPort's hash_map |
| `<unordered_set>` | C++11 | Replaces `std::hash_set` | Same as above |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `<type_traits>` | C++11 | MSVC-native type traits | Pulled transitively by `<unordered_map>`; this is what currently collides with STLPort under `<afxwin.h>` (Phase 8 finding) |
| `_USE_32BIT_TIME_T=1` define | — | Forces `time_t` to 32-bit on MSVC | Stays during Phase 9; `time_t` width is orthogonal to the STL swap. Removal is a separate concern (D-01 notes it as deferred to a separate task within Wave 3, not strictly required for STL-05 success). |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `std::unordered_map` | `boost::unordered_map` | Boost subset is 4 headers; not vendored. Adding boost as a runtime dep contradicts the minimal-change principle. Stick with std. |
| Direct call-site rewrite | StlCompat.h alias header (D-02 — chosen) | Direct rewrite is one big commit per library; alias header decouples migration from modernization, gives clean bisect window. D-02 chose the alias path. |
| `wchar_t` as `char16_t` | — | wchar_t is 16-bit on Windows under `/Zc:wchar_t`. Not the same as char16_t (which is also 16-bit on every platform). For now stay with wchar_t — a future v4 milestone targets char16_t per memory `project_v4_linux_vulkan.md`. |

**Installation:** No new packages. MSVC STL ships with VS 2022 (already installed). Removing STLPort:

```cmake
# Remove from root src/CMakeLists.txt:
find_package(STLPort REQUIRED)
option(WHITENGOLD_USE_STLPORT_HEADERS "..." ON)
if(WHITENGOLD_USE_STLPORT_HEADERS) ... endif()

# Remove from src/external/3rd/library/CMakeLists.txt:
add_subdirectory(stlport453/src)  # if listed

# Delete:
src/cmake/win32/FindSTLPort.cmake
src/external/3rd/library/stlport453/src/stlport_vc143_compat.cpp
src/external/3rd/library/stlport453/src/CMakeLists.txt
```

**Version verification:** No package versions to verify — MSVC STL is bundled with the installed VS 2022 toolchain. Confirm via `cl.exe /?` that v143/14.4x is active at build time.

---

## Architecture Patterns

### System Architecture Diagram (Phase 9 build-graph view)

```text
┌─────────────────────────────────────────────────────────────────────┐
│                         Phase 9 START                                │
│                                                                      │
│   src/CMakeLists.txt                                                │
│     find_package(STLPort REQUIRED) ─┐                                │
│     include_directories(BEFORE      │  STAGED COPY of stlport headers│
│         ${STLPORT_INCLUDE_DIR}) ─┐  │  (build/stlport453/stlport)    │
│     _STLP_DONT_FORCE_MSVC_LIB_NAME│ │                                │
│     /Zc:wchar_t-                  │ │   ┌──────────────────────────┐ │
│     /NODEFAULTLIB:                │ │   │ stlport_vc143_compat OBJ │ │
│       stlport_vc71_static.lib   ──┴─┴──>│  (compat shim with       │ │
│                                         │   pubimbue + seekoff)    │ │
│                                         └──────────────────────────┘ │
│                                                  ▲                   │
│                                                  │                   │
│   Every CMakeLists.txt in tree:                  │                   │
│     ${STLPORT_LIBRARY} ◄──FindSTLPort.cmake──────┘                   │
│       = stlport_vc143_compat (debug)                                 │
│         + prebuilt vc71 static lib                                   │
│                                                                      │
│   /FORCE:MULTIPLE on every executable target ───┐                    │
│      (because compat OBJ + vc71 lib both        │                    │
│       define pubimbue, etc.)                    │                    │
│                                                 │                    │
│   #include <hash_map> ─────► STLPort _STL::hash_map                  │
│   std::hash_map<...>::fwd ◄─StlForwardDeclaration.h                  │
│                                                                      │
│   typedef unsigned short unicode_char_t  ◄── /Zc:wchar_t-            │
│      (Unicode.h, StlForwardDeclaration.h)                            │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼ Wave 1 (engine/shared/library/*)
┌─────────────────────────────────────────────────────────────────────┐
│  + sharedFoundation/StlCompat.h (alias)                              │
│  + per-lib: #include <hash_map> → #include "sharedFoundation/        │
│      StlCompat.h"                                                    │
│  - per-lib: STLPort include path entries                             │
│  GATE: cmake --build → SwgClient_d.exe boots to char-select          │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼ Wave 2 (client/game/*)
┌─────────────────────────────────────────────────────────────────────┐
│  + remaining libs swept                                              │
│  - 19× LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"                            │
│  - /NODEFAULTLIB:stlport_vc71_static.lib                             │
│  - $<TARGET_OBJECTS:stlport_vc143_compat> from all targets           │
│  - find_package(STLPort), WHITENGOLD_USE_STLPORT_HEADERS option      │
│  - include_directories(BEFORE STLPORT_INCLUDE_DIR) (3 sites)         │
│  - _STLP_DONT_FORCE_MSVC_LIB_NAME defines                            │
│  - add_library(stlport_vc143_compat) target removed                  │
│  + Turf + Viewer add_subdirectory uncommented in tier parents        │
│  + cleanup commit migrates StlCompat.h aliases to std::unordered_*   │
│  + StlCompat.h deleted                                               │
│  GATE: SwgClient boots + Turf/Viewer compile clean                   │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼ Wave 3 (wchar_t flip)
┌─────────────────────────────────────────────────────────────────────┐
│  - /Zc:wchar_t-  (root CMakeLists.txt L43, L44)                      │
│  + /Zc:wchar_t   (default, no flag needed)                           │
│  ~ Unicode.h L27: unsigned short → wchar_t                           │
│  ~ StlForwardDeclaration.h L125: same flip                           │
│  ~ CuiNotepad.cpp L146: unsigned short* → wchar_t*                   │
│  GATE: All four final verification categories                        │
│         (dumpbin, hash determinism, round-trip persist, frame-time)  │
└─────────────────────────────────────────────────────────────────────┘
```

### Recommended Project Structure

No new directories. Modifications target existing files:

```
src/
├── CMakeLists.txt                    # 5 STLPort removal sites + /Zc:wchar_t- removal
├── cmake/win32/
│   └── FindSTLPort.cmake             # DELETE after Wave 2
├── external/3rd/library/
│   └── stlport453/                   # entire dir DELETE after Wave 3 (separate commit)
│       └── src/CMakeLists.txt        # stlport_vc143_compat target — DELETE after Wave 2
├── engine/shared/library/
│   ├── CMakeLists.txt                # Remove `if(WHITENGOLD_USE_STLPORT_HEADERS)`
│   ├── sharedFoundation/
│   │   └── include/public/
│   │       └── sharedFoundation/
│   │           └── StlCompat.h       # CREATE in Wave 1, DELETE in Wave 2 cleanup
│   └── ...
├── engine/client/library/
│   └── CMakeLists.txt                # Remove `if(WHITENGOLD_USE_STLPORT_HEADERS)`
├── engine/shared/application/
│   └── CMakeLists.txt                # Re-enable Turf add_subdirectory (Wave 2)
├── engine/client/application/
│   └── CMakeLists.txt                # Re-enable Viewer add_subdirectory (Wave 2)
└── external/ours/library/unicode/
    └── src/shared/Unicode.h          # Wave 3: unicode_char_t typedef flip
```

### Pattern 1: StlCompat.h Alias Header (D-02)

**What:** Bridges STLPort's `std::hash_map` / `std::hash_set` (extension under std::) to MSVC STL's `std::unordered_map` / `std::unordered_set` via `using` aliases. Lets the Wave 1/2 sweep change include directives only, leaving call-sites untouched until the cleanup commit.

**When to use:** Every shared/client library that currently does `#include <hash_map>` or `#include <hash_set>`. The sweep is mechanical: find the include line, replace.

**Example:**
```cpp
// src/engine/shared/library/sharedFoundation/include/public/sharedFoundation/StlCompat.h
// Source: D-02 (verbatim)
#pragma once
#include <unordered_map>
#include <unordered_set>
namespace std {
    template <class K, class V,
              class H = std::hash<K>,
              class E = std::equal_to<K>,
              class A = std::allocator<std::pair<const K, V>>>
    using hash_map = std::unordered_map<K, V, H, E, A>;
    template <class K,
              class H = std::hash<K>,
              class E = std::equal_to<K>,
              class A = std::allocator<K>>
    using hash_set = std::unordered_set<K, H, E, A>;
}
```
[CITED: 09-CONTEXT.md D-02 verbatim]

### Pattern 2: stdhash_map<...>::fwd Forward-Declaration Wrapper

**What:** SWG's idiosyncratic forward-declaration pattern. Files include `StlForwardDeclaration.h` (which forward-declares `std::hash_map`) and use `stdhash_map<K,V>::fwd` as a type alias. This decouples header includes from full STL header pulls.

**Where:** 16 files use this pattern (`stdhash_map` or `stdhash_set` references). All are inside actual library headers (PvpUpdateObserver, NetworkIdManager, DataTable, etc.).

**Why it matters in Phase 9:** `StlForwardDeclaration.h` line 54 forward-declares `template <class K, class V, class H, class C, class A> class hash_map;` in `namespace std`. With STLPort gone, this forward-declares against MSVC `std::` (not STLPort `_STL::`). The `stdhash_map<K,V>::fwd = std::hash_map<K,V,std::hash<K>,std::equal_to<K>,std::allocator<...>>` typedef chain still resolves IF and only IF `StlCompat.h` introduced the `using hash_map = std::unordered_map<...>` alias before this template is instantiated.

**Migration:** During Wave 1, `StlForwardDeclaration.h` itself becomes a transitive consumer of `StlCompat.h`. Either include `StlCompat.h` from `StlForwardDeclaration.h` directly, OR change the file to forward-declare `std::unordered_map` and `std::unordered_set`. Recommendation: **include `StlCompat.h`** — simpler and maintains the `stdhash_map` wrapper for backward compatibility until cleanup.

### Pattern 3: time_t / _USE_32BIT_TIME_T Defensive Define

**What:** `_USE_32BIT_TIME_T=1` is set globally in root CMakeLists.txt L43, L44 (Debug + Release). Forces 32-bit `time_t` on MSVC.

**Phase 9 status:** Stays. CONTEXT.md D-01 Wave 3 mentions it as a separate concern that may be removable, but explicitly does NOT make it required for STL-05 success. Touch nothing here.

### Anti-Patterns to Avoid

- **Single-commit migration of all libraries:** Don't author a "rip out STLPort" mega-commit. Each Wave 1/2 commit must keep the build green; if a per-library STL swap breaks compilation, fix or revert before moving on.
- **Mixed Wave 2 + Wave 3 changes:** Don't flip `unicode_char_t` to `wchar_t` in the same commit as STLPort removal. The bisect-clean property of D-03's two-cycle separation depends on this discipline.
- **Skipping the StlCompat.h alias step:** Don't change call-sites from `std::hash_map` to `std::unordered_map` directly during Wave 1/2. The alias buys you a smaller per-commit blast radius and lets the cleanup commit be reverted independently.
- **Deleting STLPort headers before all CMake references are gone:** CMake configure will fail if `find_package(STLPort)` runs and FindSTLPort.cmake is missing. Sequence: remove all `find_package(STLPort)` calls FIRST, then delete FindSTLPort.cmake, then (in the post-Wave-3 cleanup commit) delete `src/external/3rd/library/stlport453/`.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| hash_map / hash_set replacement | Custom hash table | `std::unordered_map` / `std::unordered_set` | C++11 standard; nominally same algorithm; no behavioral surprises versus STLPort hash_map (only ABI differences) |
| String COW emulation | Custom `cow_string` to preserve STLPort behavior | `std::string` (MSVC SSO) | Just rebuild — every consumer should treat std::string as standard. The COW→SSO change is intended. |
| wchar_t bit-width control | Custom `unicode_char16_t` typedef chain | `wchar_t` (built-in under `/Zc:wchar_t`) | Win32 SDK and CRT all expect 16-bit wchar_t on Windows when `/Zc:wchar_t` is set. Don't introduce a third option. |
| MSVC v143 ↔ STLPort vc71 ABI bridge | The current `stlport_vc143_compat.cpp` shim | DELETE — there's no longer two STLs to bridge | The shim only existed because we kept STLPort. Phase 9 removes the reason for the shim. |
| Hash determinism baseline | Custom unit test framework | `dumpbin /imports` + a 100-line CRC test in a one-off `Md5sum`-like exe | Already-built tools (Md5sum) plus a fixed-input CRC dump cover this without new infrastructure. Document the test in `.planning/phases/09-stlport-msvc-stl/baseline/`. |

**Key insight:** The Phase 9 deliverable is the OPPOSITE of "build new infrastructure." Every line of stlport_vc143_compat.cpp + every `/FORCE:MULTIPLE` exists solely to keep STLPort working under MSVC 2022. Remove the cause, not the symptom.

---

## CMake Removal Surface

The complete enumeration of files and lines that touch STLPort. This is what Wave 1 / Wave 2 must traverse.

### A. Root CMakeLists.txt (`src/CMakeLists.txt`)

| Line(s) | Content | Wave | Action |
|---------|---------|------|--------|
| 43 | `... /D_USE_32BIT_TIME_T=1 /D_MBCS ... /Zc:wchar_t- /Ob1 /FC` (Debug) | 3 | Remove `/Zc:wchar_t-` |
| 44 | Same flags string for Release | 3 | Remove `/Zc:wchar_t-` |
| 46 | `set(CMAKE_EXE_LINKER_FLAGS "... /NODEFAULTLIB:libc.lib /NODEFAULTLIB:stlport_vc71_static.lib /SAFESEH:NO")` | 2 | Remove `/NODEFAULTLIB:stlport_vc71_static.lib` |
| 55 | `find_package(STLPort REQUIRED)` | 2 | DELETE |
| 56 | `option(WHITENGOLD_USE_STLPORT_HEADERS "..." ON)` | 2 | DELETE |
| 57–62 | `if(WHITENGOLD_USE_STLPORT_HEADERS) ... include_directories(BEFORE ${STLPORT_INCLUDE_DIR}) add_compile_definitions(_STLP_DONT_FORCE_MSVC_LIB_NAME) ... endif()` | 2 | DELETE entire block |

### B. Tier-Parent CMakeLists.txt (3 sites)

| File | Line | Content | Wave | Action |
|------|------|---------|------|--------|
| `src/engine/shared/library/CMakeLists.txt` | 1–3 | `if(WHITENGOLD_USE_STLPORT_HEADERS) include_directories(BEFORE ${STLPORT_INCLUDE_DIR}) endif()` | 1 (last commit) | DELETE |
| `src/engine/client/library/CMakeLists.txt` | 1–3 | Same pattern | 2 | DELETE |
| `src/external/ours/library/CMakeLists.txt` | unknown | Same (verify with grep) | 1 (last commit) | DELETE |
| `src/external/3rd/library/CMakeLists.txt` | unknown | Same (verify with grep) | 1 (last commit) | DELETE |

### C. stlport_vc143_compat Target (`src/external/3rd/library/stlport453/src/CMakeLists.txt`)

| Line(s) | Content | Wave | Action |
|---------|---------|------|--------|
| 9–11 | `add_library(stlport_vc143_compat STATIC ...)` | 2 | DELETE entire file (with parent's `add_subdirectory(stlport453/src)` if present) |

### D. FindSTLPort.cmake (`src/cmake/win32/FindSTLPort.cmake`)

| Line(s) | Content | Wave | Action |
|---------|---------|------|--------|
| All 145 lines | Stages STLPort headers + UCRT shim + 5 source-file patches | 2 | DELETE entire file |

### E. Per-Tool LINK_FLAGS (`/FORCE:MULTIPLE`) — 19 sites

| File | Line | Wave | Action |
|------|------|------|--------|
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | 86 | 2 | Remove `/FORCE:MULTIPLE` from `LINK_FLAGS_DEBUG` set_target_properties |
| `src/engine/shared/application/Md5sum/CMakeLists.txt` | 57 | 2 | Same |
| `src/engine/shared/application/Turf/CMakeLists.txt` | 82 | 2 | Same |
| `src/engine/shared/application/UpdateLocalizedStrings/CMakeLists.txt` | 56 | 2 | Same |
| `src/engine/shared/application/TreeFileBuilder/CMakeLists.txt` | 59 | 2 | Same |
| `src/engine/shared/application/TreeFileExtractor/CMakeLists.txt` | 59 | 2 | Same |
| `src/engine/shared/application/TreeFileRspBuilder/CMakeLists.txt` | 23 | 2 | Same |
| `src/engine/shared/application/DataLintRspBuilder/CMakeLists.txt` | 23 | 2 | Same |
| `src/engine/shared/application/StringFileTool/CMakeLists.txt` | 54 | 2 | Same |
| `src/engine/shared/application/WordCountTool/CMakeLists.txt` | 61 | 2 | Same |
| `src/engine/shared/application/LabelHashTool/CMakeLists.txt` | 56 | 2 | Same |
| `src/engine/shared/application/TemplateCompiler/CMakeLists.txt` | 76 | 2 | Same |
| `src/engine/shared/application/TemplateDefinitionCompiler/CMakeLists.txt` | 74 | 2 | Same |
| `src/engine/shared/application/P4Qt/CMakeLists.txt` | 62 | 2 | Same |
| `src/external/ours/application/LocalizationToolCon/CMakeLists.txt` | 11 | 2 | Same |
| `src/game/server/application/SwgBattlefieldTool/CMakeLists.txt` | 9 | 2 | Same |
| `src/game/server/application/SwgSchematicXmlParser/CMakeLists.txt` | 9 | 2 | Same |

(Plus any Turf and Viewer CMakeLists when re-enabled.)

### F. Per-Tool `${STLPORT_LIBRARY}` and `$<TARGET_OBJECTS:stlport_vc143_compat>` references — 23 files

These appear in:
- All 17 tools listed above (each has both `${STLPORT_LIBRARY}` in `target_link_libraries` and `$<TARGET_OBJECTS:stlport_vc143_compat>` in `add_executable`/source list)
- `src/CMakeLists.txt` (already enumerated in A)
- `src/engine/shared/library/CMakeLists.txt`, `src/engine/client/library/CMakeLists.txt`, `src/external/ours/library/CMakeLists.txt`, `src/external/3rd/library/CMakeLists.txt` — already in B
- `src/game/client/application/SwgClient/src/CMakeLists.txt` L156, L172 — `${STLPORT_LIBRARY}` and `target_link_directories(... stlport453/lib/win32)`

### G. Phase 8 Deferred Add-Subdirectory Lines to RE-ENABLE in Wave 2

| File | Lines | Action |
|------|-------|--------|
| `src/engine/shared/application/CMakeLists.txt` | 11–21 | UNCOMMENT `add_subdirectory(Turf)` (D-05 canary). Other 4 tools (TreeFileRspBuilder, DataLintRspBuilder, StringFileTool, WordCountTool) remain commented — they are NOT Phase 9 canaries; per CONTEXT.md they re-enable post-Wave-2 in a separate plan. |
| `src/engine/client/application/CMakeLists.txt` | (per Phase 8 Plan 02) | UNCOMMENT `add_subdirectory(Viewer)` only (D-05 canary). Other 9 MFC-deferred tools remain commented for Phase 12.x. |

**Verify the exact comment block in `engine/client/application/CMakeLists.txt` at plan-author time** — Plan 08-02 Summary mentions classified deferral but the file itself was not enumerated in this research. The planner should grep `^# add_subdirectory\(Viewer\)` to find the exact line to flip.

---

## Hash_map / Hash_set Call-Site Inventory

**Audit scope:** `src/` excluding `**/stlport453/**`.
**Total raw matches:** 89 files, ~284 occurrences.
**In SwgClient compile path:** 42 files (others are server-only or in deferred tools).

### In-SwgClient files with `#include <hash_map>` or `#include <hash_set>` directives

These are the **direct sweep targets** for Wave 1 / Wave 2.

| File | Lib (Wave) | hash_map refs | hash_set refs | Notes |
|------|------------|---------------|---------------|-------|
| `src/engine/shared/library/sharedObject/src/shared/object/NetworkIdManager.cpp` | sharedObject (W1) | 1 | 0 | `#include <hash_map>` L13; uses `stdhash_map<NetworkId, Object*, NetworkId::Hash>::fwd` typedef chain |
| `src/engine/shared/library/sharedObject/src/shared/object/NetworkIdManager.h` | sharedObject (W1) | 1 | 0 | Forward-decl typedef only |
| `src/engine/shared/library/sharedSkillSystem/src/shared/SkillManager.cpp` | sharedSkillSystem (W1) | 1 | 0 | |
| `src/engine/shared/library/sharedSkillSystem/src/shared/SkillManager.h` | sharedSkillSystem (W1) | 2 | 0 | |
| `src/engine/shared/library/sharedNetworkMessages/src/shared/common/GameNetworkMessage.cpp` | sharedNetworkMessages (W1) | 4 | 0 | High-density; primary message-dispatch hash table |
| `src/engine/shared/library/sharedNetwork/src/shared/Address.h` | sharedNetwork (W1) | 2 | 0 | |
| `src/engine/shared/library/sharedNetwork/src/win32/Address.cpp` | sharedNetwork (W1) | 7 | 0 | Highest density in sharedNetwork |
| `src/engine/shared/library/sharedNetwork/src/linux/Address.cpp` | (Linux — not in client compile path; document but skip in Wave 1) | 7 | 0 | Excluded from WIN32 builds |
| `src/engine/shared/library/sharedMessageDispatch/src/shared/MessageManager.cpp` | sharedMessageDispatch (W1) | 9 | 0 | High-density dispatch table |
| `src/engine/shared/library/sharedMessageDispatch/src/shared/FirstSharedMessageDispatch.h` | sharedMessageDispatch (W1) | 1 | 0 | PCH header pulls hash_map transitively |
| `src/engine/shared/library/sharedMath/src/shared/PositionVertexIndexer.h` | sharedMath (W1) | 1 | 0 | |
| `src/engine/shared/library/sharedGame/src/shared/space/ShipComponentType.cpp` | sharedGame (W1) | 2 | 0 | |
| `src/engine/shared/library/sharedGame/src/shared/space/ShipChassisSlotType.cpp` | sharedGame (W1) | 2 | 0 | |
| `src/engine/shared/library/sharedGame/src/shared/object/Waypoint.cpp` | sharedGame (W1) | 4 | 0 | |
| `src/engine/shared/library/sharedGame/src/shared/combat/CombatDataTable.cpp` | sharedGame (W1) | 3 | 0 | |
| `src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h` | sharedFoundation (W1) | 6 | 0 | **CRITICAL** — this is the forward-decl that powers `stdhash_map<...>::fwd` everywhere. After StlCompat.h is authored, include it from this header so `std::hash_map` resolves to the alias. |
| `src/engine/shared/library/sharedDebug/src/shared/LeakFinder.h` | sharedDebug (W1) | 2 | 0 | |
| `src/engine/shared/library/sharedUtility/src/shared/DataTable.h` | sharedUtility (W1) | 1 | 0 | **D-06 round-trip target** — see below |
| `src/engine/shared/library/sharedUtility/src/shared/DataTable.cpp` | sharedUtility (W1) | 1 | 0 | |
| `src/engine/shared/library/sharedUtility/src/shared/WorldSnapshotReaderWriter.h` | sharedUtility (W1) | 1 | 0 | |
| `src/engine/shared/library/sharedUtility/src/shared/WorldSnapshotReaderWriter.cpp` | sharedUtility (W1) | 2 | 0 | |
| `src/engine/client/library/clientUserInterface/src/shared/core/CuiSkillManager.cpp` | clientUserInterface (W2) | 1 | 0 | |
| `src/engine/client/library/clientObject/src/shared/appearance/ComponentAppearanceTemplate.cpp` | clientObject (W2) | 1 | 0 | |
| `src/engine/client/library/clientGame/src/shared/appearance/LightsaberCollisionManager.cpp` | clientGame (W2) | 2 | 0 | |
| `src/engine/client/library/clientGame/src/shared/core/ClientBuffManager.cpp` | clientGame (W2) | 12 | 0 | **Highest single-file density in client tier** |
| `src/external/ours/library/localization/src/shared/LocalizationManager.h` | localization (W1 — external/ours/) | 3 | 0 | |
| `src/external/3rd/library/ui/src/win32/UIBaseObject.cpp` | ui (W2) | 4 | 0 | |
| `src/external/3rd/library/ui/src/win32/UILoader.h` | ui (W2) | 2 | 0 | |
| `src/external/3rd/library/ui/src/win32/UIManager.cpp` | ui (W2) | 2 | 0 | |
| `src/external/3rd/library/ui/src/win32/UIStlFwd.h` | ui (W2) | 6 | 0 | UI library's own forward-declaration header — analogous to StlForwardDeclaration.h. Wave 2 must include `StlCompat.h` here too. |
| `src/external/3rd/library/ui/src/win32/UITextStyle.h` | ui (W2) | 1 | 0 | |
| `src/external/3rd/library/ui/src/win32/UITextStyle.cpp` | ui (W2) | 1 | 0 | |
| `src/external/3rd/library/ui/src/shared/boundary/UIWidgetBoundaries.h` | ui (W2) | 1 | 0 | |
| `src/external/3rd/library/ui/src/shared/core/UILowerString.cpp` | ui (W2) | 1 | 0 | |
| `src/external/3rd/library/ui/src/shared/core/UILowerString.h` | ui (W2) | 2 | 0 | |
| `src/external/3rd/library/ui/src/shared/core/UiMemoryBlockManager.h` | ui (W2) | 3 | 0 | |
| `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserUI.cpp` | swgClientUserInterface (W2) | 1 | 0 | |
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiSpaceRadar.h` | swgClientUserInterface (W2) | 1 | 0 | |
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiSpaceRadar.cpp` | swgClientUserInterface (W2) | 1 | 0 | |
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiExpMonitor.cpp` | swgClientUserInterface (W2) | 1 | 0 | |
| `src/external/3rd/library/boost/boost/config.hpp` | boost (header-only) | 5 | 0 | **DO NOT EDIT** — boost detects STLPort via macros; with STLPort gone, the MSVC code path wins automatically. See D-07 verification below. |

### Excluded from Wave 1/2 (server-only — not built in v2 client)

The remaining 47 files are in:
- `src/engine/server/library/serverGame/` (~17 files)
- `src/engine/server/library/serverDatabase/` (3 files)
- `src/engine/server/library/serverScript/` (2 files)
- `src/engine/server/library/serverUtility/` (1 file — Linux-only `stlhack.cpp`)
- `src/engine/server/application/{LoginServer,CentralServer,ChatServer,ConnectionServer,CustomerServiceServer,PlanetServer}/` (~10 files)
- `src/game/server/application/{SwgDatabaseServer,SwgGodClient}/` (deferred Phase 12.x)
- `src/external/ours/application/LocalizationTool/` and `src/external/3rd/application/UiBuilder/` (deferred Qt batch)
- `src/engine/client/application/UiBuilder/` (deferred Phase 12.x)

These are **not in the SwgClient compile path** but show up in raw greps. Document in BEFORE-STL.txt for completeness; do not sweep them in Phase 9.

### Hashers, Allocators, Custom Hash Functions Used

Confirmed pattern across the call-sites:

```cpp
// NetworkIdManager.h
typedef stdhash_map<NetworkId, Object *, NetworkId::Hash>::fwd NetworkIdObjectHashMap;

// SkillManager.h
typedef stdhash_map<std::string, SkillObject *>::fwd SkillMap;

// DataTable.h L86
typedef stdhash_map<std::string, int>::fwd ColumnIndexMap;
```

**Custom hash functor classes found:** `NetworkId::Hash`, `CrcLowerString::HashFunctor`, `Tag::Hash` (rare). All are simple POD-like predicates that follow the SGI hasher signature `size_t operator()(const T&) const`. Both STLPort and MSVC unordered_map accept this signature unchanged. **No adaptation needed in StlCompat.h.**

---

## Unicode / wchar_t Flip Surface (Wave 3)

### unicode_char_t typedef sites (must be edited together)

| File | Line | Content | Wave 3 Action |
|------|------|---------|---------------|
| `src/external/ours/library/unicode/src/shared/Unicode.h` | 27 | `typedef unsigned short unicode_char_t;` | Change to `typedef wchar_t unicode_char_t;` |
| `src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h` | 125 | `typedef unsigned short unicode_char_t;` (in `namespace Unicode` block) | Same flip |

**Both must change in the same commit.** If they diverge, Unicode::String becomes two different types in different translation units → ODR violation, link errors or runtime corruption.

### Compile flag changes

| File | Line | Content | Wave 3 Action |
|------|------|---------|---------------|
| `src/CMakeLists.txt` | 43 | `... /Zc:wchar_t- /Ob1 /FC` (Debug) | Remove `/Zc:wchar_t-` (default `/Zc:wchar_t` activates) |
| `src/CMakeLists.txt` | 44 | Same flags string for Release | Same |
| `src/external/3rd/library/stlport453/src/CMakeLists.txt` | 26 | `target_compile_options(stlport_vc143_compat PRIVATE ... /Zc:wchar_t-)` | DELETE — file deleted in Wave 2 |

### Pointer-cast surface

| File | Line | Risk | Action |
|------|------|------|--------|
| `src/engine/client/library/clientUserInterface/src/shared/page/CuiNotepad.cpp` | 146 | `unsigned short *unicodeBuffer = reinterpret_cast<unsigned short *>(buffer + 1);` — buffer is `char*`, cast reinterprets bytes as UTF-16 LE | After unicode_char_t flips to wchar_t, this cast still compiles BUT the local variable `unicodeBuffer` is now wider/narrower depending on platform. On Windows wchar_t is 2 bytes so binary layout matches. **Recommended action:** Change to `wchar_t *unicodeBuffer = reinterpret_cast<wchar_t *>(buffer + 1);` for type consistency. |

### unicode_char_t* parameter sites in `external/ours/library/unicode/`

10 functions in `UnicodeUtils.h` / `UnicodeUtils.cpp` take `const unicode_char_t * sepChars = whitespace` etc. parameters. After the typedef flip these become `const wchar_t *` parameters. Function signature mangling changes — clients of these functions must recompile (which they will under the global Wave 3 rebuild).

**No source change needed at the call sites** — they all pass `Unicode::whitespace` (a `unicode_char_t[]` literal) which itself flips to `wchar_t[]`.

### Other potential ABI sites

| Concern | Search Result | Status |
|---------|---------------|--------|
| Raw mbstate_t use in client | 0 hits in `src/engine/`, `src/game/`, `src/external/ours/` | SAFE |
| `wcstombs`/`mbstowcs` in client | 0 hits in `src/engine/`, `src/game/`, `src/external/ours/` (only Miff and ShaderBuilder — both deferred Phase 12.x) | SAFE |
| `_M_lock` / `_Lock` in client | 0 hits outside libMozilla (already removed Phase 7 CLEAN-04), atlmfc (MFC tool only), DirectX9 SDK headers | SAFE |
| `std::hash<std::string>` persisted | Only 2 hits, both server-only (LoginServer, ConnectionServer ClientConnection.cpp) | SAFE — server is read-only contract |

XPCOM PRUnichar boundary (the original constraint forcing `/Zc:wchar_t-`): **gone after Phase 7 CLEAN-04**. Confirmed by grepping `mozilla` and `xpcom` — only inert reference text in Phase 7 SUMMARY files. No live code consumes PRUnichar.

---

## D-06 Confirmation: Round-Trip Persist Target

### Evaluation against criteria

| Criterion | NetworkIdManager | CrcLowerString cache | **DataTable** |
|-----------|------------------|---------------------|---------------|
| Already serializes via IFF | NO — pure in-memory hash, no IFF code path | INDIRECT — used as keys but the cache itself isn't IFF-persisted | **YES — `void load(Iff &)` member function, IFF tag `D,T,I,I`** |
| Internally uses std::string | NO — uses NetworkId (uint64) keys | NO — char* internally | **YES — `std::vector<std::string> m_columns`, `std::string m_name`, `stdhash_map<std::string, int> m_columnIndexMap`** |
| Internally uses std::vector | NO | NO | **YES — `std::vector<std::string> m_columns`, `std::vector<void *> m_index`, `DataTableColumnTypeVector m_types` (= std::vector)** |
| Small enough for <50 line equality test | YES — but no IFF round-trip | YES — but doesn't exercise STL ABI | **YES — load a small known .iff datatable, dump column names + types + a few cell values** |
| Exercises COW→SSO ABI shift | NO | NO | **YES — m_name + every m_columns entry is std::string** |
| Exercises hash_map ABI shift | YES | NO | **YES — m_columnIndexMap is exactly the hash table being migrated** |

### Recommendation

**Round-trip target: `DataTable` from `sharedUtility`.**

Hits all D-06 criteria. Test outline:

```cpp
// .planning/phases/09-stlport-msvc-stl/baseline/datatable_round_trip.cpp
// Compiled standalone via the existing DataTableTool or as a one-off Md5sum-style helper.
//
// Procedure:
// 1. Pre-Phase-9: open a known .iff datatable from the SWG retail tree
//    (recommendation: misc/quest_task_data.iff — small, ~20 columns, mixed types).
// 2. Iterate every (column, row) cell, dump as text:
//    "<col-index> <col-name> <type> <value-as-string>" — one line per cell.
// 3. Save output to baseline/before-datatable-round-trip.txt.
// 4. Post-Wave-3: re-run, save to baseline/after-datatable-round-trip.txt.
// 5. Diff. Must be byte-identical.
//
// Why this works:
//   - load() reads IFF → populates m_columns (std::vector<std::string>) +
//     m_columnIndexMap (stdhash_map<std::string,int>) — every layer the migration
//     touches is traversed.
//   - getStringValue/getIntValue are pure-read; no STL allocation differences.
//   - Output is deterministic given identical input data.
```

**NetworkIdManager** is unsuitable because it doesn't persist via IFF and would require a synthesized test harness. **CrcLowerString cache** is unsuitable because the cache isn't directly IFF-persisted — it's in-memory only. DataTable is the only candidate that passes all criteria with no new test infrastructure.

---

## D-07 Confirmation: Boost Subset Spot-Check

### The four headers

| File | STLPort detection? | MSVC fallback path? | Verdict |
|------|--------------------|--------------------|---------|
| `src/external/3rd/library/boost/boost/config.hpp` | YES — checks `__STL_USE_NAMESPACES`, `__SGI_STL_PORT`, `_STLP_*` macros at lines 444, 507, 551, 576, 590 | YES — when none of those macros are defined, the `defined _MSC_VER` block at line 480 sets `BOOST_MSVC=_MSC_VER` and uses MSVC's path | **CLEAN — boost will route through MSVC after STLPort is gone** |
| `src/external/3rd/library/boost/boost/static_assert.hpp` | NO direct STLPort detection — just includes `<boost/config.hpp>` | N/A | **CLEAN — purely a `#define`-based static assert macro; behavior is identical** |
| `src/external/3rd/library/boost/boost/utility.hpp` | NO direct STLPort detection — includes config.hpp + uses `BOOST_STATIC_ASSERT` | N/A | **CLEAN — pure C++ template code; works under any C++17 standard library** |
| `src/external/3rd/library/boost/boost/smart_ptr.hpp` | NO direct STLPort detection — uses `BOOST_NO_AUTO_PTR`, `BOOST_NO_MEMBER_TEMPLATES` from config.hpp | N/A — these are FALSE under MSVC v143 | **CLEAN — `boost::shared_ptr` and `boost::scoped_ptr` template defs work as-is. Note: smart_ptr.hpp uses `std::auto_ptr` constructors (line 150, 165) — this is C++17-deprecated but **MSVC still provides `std::auto_ptr` under `/std:c++17`** as long as `_HAS_AUTO_PTR_ETC=1` is defined or default. Verify on first Wave 1 build that boost smart_ptr compiles clean. If not, define `_HAS_AUTO_PTR_ETC=1` globally — this is a one-line addition to root CMakeLists.txt CXX flags. |

### Inspection notes

config.hpp line 480-555 (the MSVC branch) sets these macros for `_MSC_VER >= 1400` (which v143 satisfies):
- `BOOST_MSVC = _MSC_VER` ✓
- `BOOST_NO_HASH` is set when `_CPPLIB_VER >= 306` (which all modern MSVC has) — meaning boost will NOT try to use boost::unordered. ✓ (Irrelevant to whitengold; only 4 headers are vendored, none use hash containers.)
- `BOOST_STD_EXTENSION_NAMESPACE` defaults to `std` (line 593). Was potentially `_STL` under STLPort detection (line 423 for Metrowerks, line 430 elsewhere). With no `_STLP_*` macros set, this is `std` — meaning any `BOOST_STD_EXTENSION_NAMESPACE::less<...>` reference resolves to `std::less` (correct).

### Risk to Phase 9: NONE

The 4 boost headers compile under STLPort (today) and will compile under MSVC STL (Phase 9). No edits required. The `_HAS_AUTO_PTR_ETC` consideration above is the only watch-out — recommend adding `add_compile_definitions(_HAS_AUTO_PTR_ETC=1)` to root CMakeLists.txt as a defensive measure during Wave 1 (cost is one CMake line; benefit is no surprise late in Wave 2).

---

## STLPort-Specific Behaviors That May Surprise

From the user memory + Phase 8 findings + this audit:

### 1. STLPort's `_STL::_M_setup_codecvt` vtable mismatch (Phase 9 RESOLVES)

**Memory note:** `project_stlport_fstream_crash.md` — basic_filebuf<wchar_t> crashes when `codecvt->encoding()` virtual call goes through a vtable that vc71 and MSVC 14.44 lay out differently.
**Phase 9 impact:** Goes away. If any production code path was relying on STLPort's specific filebuf init order, it surfaces here. Audit: 0 client-side `basic_filebuf<wchar_t>` use found; only STLPort internal uses (deleted with STLPort).
**Mitigation:** None needed — fstream I/O works with native MSVC STL.

### 2. STLPort `mbstate_t` ABI mismatch (Phase 9 RESOLVES)

**Memory note:** `project_post_v1_exploration.md` — STLPort and MSVC define `mbstate_t` with different sizes; raw memcpy of `mbstate_t` corrupts state.
**Phase 9 impact:** Goes away. MSVC `mbstate_t` is the only `mbstate_t` after Wave 2.
**Mitigation:** Audit found 0 raw `mbstate_t` use in client compile path (Miff and ShaderBuilder are deferred Phase 12.x and use `mbstowcs` not raw mbstate). SAFE.

### 3. `<afxwin.h>` ↔ STLPort 4.5.3 ↔ MSVC `<type_traits>` collision (Phase 9 RESOLVES)

**Phase 8 finding:** STLPort's `_STL::_Is_memfunptr` partial specialization references `enable_if` from MSVC `std::` (not `_STL::`) when both are on the include path. C2061 errors with no clean workaround.
**Phase 9 impact:** Goes away — only one `<type_traits>` exists after STLPort removal.
**Mitigation:** D-05 wires Turf and Viewer as Wave 2 canaries. Their successful compile is direct positive proof this collision is gone.

### 4. STLPort `std::hash<T>` differs from MSVC `std::hash<T>` (Phase 9 CHARACTERIZES, doesn't break)

**Behavior change:** `std::hash<std::string>("foo")` will produce a different size_t under MSVC than under STLPort.
**Phase 9 impact:** Affects ANY code that persists `std::hash<>` output. Audit: ZERO client-side persistence of `std::hash<>` output. Server-only sites (LoginServer, ConnectionServer) are not in v2 scope.
**Mitigation:** D-04 baseline captures `Crc::calculateWithToLower` (CUSTOM CRC32, NOT std::hash — unaffected). The hash-determinism gate covers this category by checking the persisted hash function (Crc), not the transient one (std::hash).

### 5. STLPort `std::string` is COW; MSVC `std::string` is SSO (Phase 9 SHIFTS, expected)

**Behavior change:** STLPort's basic_string had reference-counted Copy-On-Write semantics (mutating `c_str()` of a copy could trigger a deep clone). MSVC has Small String Optimization (no reference counting; copies are always deep).
**Phase 9 impact:** Code that aliased `c_str()` from a temporary, or relied on `std::string s2 = s1; s1[0] = 'X';` not affecting s2, used to be saved by COW's deep-clone-on-write. Under SSO, copies are already deep — same end behavior, slightly more memory in some cases.
**Mitigation:** D-04 frame-time gate (±10%) catches any pathological allocator change. The DataTable round-trip catches any string-corruption bug.

### 6. STLPort `auto_ptr` vs MSVC `auto_ptr` (deprecated but still present)

**Behavior:** Both ship `std::auto_ptr` and both have the same broken-by-design semantics. MSVC v143 / `/std:c++17` requires `_HAS_AUTO_PTR_ETC=1` for `auto_ptr` to be visible.
**Phase 9 impact:** Boost smart_ptr.hpp uses `std::auto_ptr`. See D-07 above — recommend defining `_HAS_AUTO_PTR_ETC=1` in root CMakeLists.txt as defensive measure.

### 7. STLPort `binder1st`, `binder2nd`, `ptr_fun`, `mem_fun` (deprecated)

**Behavior:** These C++98 binders work under both STLPort and MSVC v143 / `/std:c++17` (they're fully removed in C++17 only when `_HAS_DEPRECATED_ADAPTOR_TYPEDEFS` is 0 AND `/std:c++20` is set).
**Audit:** Found 37 hits, all in:
- LagOMatic (deferred — empty dir)
- crypto/original/* (vendored static lib, may or may not still compile)
- TextureBuilder, MayaExporter, ShaderBuilder, ClientLocalWaterManager, Miff (deferred Phase 12.x — not in client critical path)
- boost/smart_ptr.hpp (no — those are inside `namespace std` for less<> specialization, not binder use)
- 2 client-path files: `clientSkeletalAnimation/SpeedSkeletalAnimation.cpp`, `clientAnimation/PlaybackScriptTemplate.cpp`, `sharedMath/MxCifQuadTree.h`, `sharedFoundation/VoidMemberFunction.h` — each has 1-3 hits

**Recommendation:** Spot-check `VoidMemberFunction.h` (3 hits) during Wave 1. If `std::mem_fun` doesn't compile under v143/c++17, define `_HAS_DEPRECATED_ADAPTOR_TYPEDEFS=1` globally. If even that fails, surgical replacement with lambdas — but these are minor sites and not Phase 9's critical path.

---

## Code Examples

### StlCompat.h (D-02 verbatim)

```cpp
// src/engine/shared/library/sharedFoundation/include/public/sharedFoundation/StlCompat.h
// Source: 09-CONTEXT.md D-02
#pragma once
#include <unordered_map>
#include <unordered_set>

namespace std {
    template <class K, class V,
              class H = std::hash<K>,
              class E = std::equal_to<K>,
              class A = std::allocator<std::pair<const K, V>>>
    using hash_map = std::unordered_map<K, V, H, E, A>;

    template <class K,
              class H = std::hash<K>,
              class E = std::equal_to<K>,
              class A = std::allocator<K>>
    using hash_set = std::unordered_set<K, H, E, A>;
}
```
[CITED: 09-CONTEXT.md D-02]

### Wave 1 per-file edit pattern

Before:
```cpp
// NetworkIdManager.cpp L13
#include <hash_map>
```

After (Wave 1):
```cpp
#include "sharedFoundation/StlCompat.h"
```

Call-sites in NetworkIdManager.cpp (`m_objectHashMap->begin()`, `map.find()`, `map.insert()`) require **no changes** — `std::hash_map` aliases to `std::unordered_map` with same iterator and member API.

### Wave 2 cleanup commit pattern

Before (after Wave 1):
```cpp
typedef stdhash_map<NetworkId, Object *, NetworkId::Hash>::fwd NetworkIdObjectHashMap;
```

After (Wave 2 cleanup):
```cpp
typedef std::unordered_map<NetworkId, Object *, NetworkId::Hash> NetworkIdObjectHashMap;
```

(The `stdhash_map<...>::fwd` wrapper goes away; references straight to `std::unordered_map` at call-site.)

### Wave 3 unicode_char_t flip (Unicode.h L27)

Before:
```cpp
typedef unsigned short unicode_char_t;
```

After:
```cpp
typedef wchar_t unicode_char_t;
```

(Same edit in `StlForwardDeclaration.h` L125. After the flip, `Unicode::String == std::basic_string<wchar_t, ...> == std::wstring` for all practical purposes — but the typedef stays for source compatibility.)

### Wave 3 root CMakeLists.txt flag flip

Before (L43, L44):
```cmake
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ... /Zc:wchar_t- /Ob1 /FC")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ... /Zc:wchar_t- /Ob1 /FC")
```

After:
```cmake
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ... /Ob1 /FC")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ... /Ob1 /FC")
```

(`/Zc:wchar_t` is the MSVC default since VS 2005; explicitly removing the negated form activates it.)

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| STLPort 4.5.3 (vc71 prebuilt) glued to MSVC 2022 via compat shim | Native MSVC v143 STL | This phase | Eliminates 145-line FindSTLPort.cmake stage/patch script, ~80 lines of compat shim, all `/FORCE:MULTIPLE` flags |
| `std::hash_map<...>` (SGI/STLPort extension) | `std::unordered_map<...>` (C++11 standard) | C++11 (2011); SWG never migrated | Modern C++17 codebases use unordered_map exclusively |
| `unsigned short` as wchar_t (`/Zc:wchar_t-`) | Built-in wchar_t (`/Zc:wchar_t`, default) | VS 2005 (default); whitengold pinned the negation for XPCOM | Phase 7 CLEAN-04 removed XPCOM, lifting the constraint |
| `std::auto_ptr` | `std::unique_ptr` (C++11) | C++17 deprecates auto_ptr | Phase 9 keeps auto_ptr (boost subset depends on it); v3+ migration |

**Deprecated/outdated:**
- `std::hash_map` and `std::hash_set` were removed from C++11 (never standardized). STLPort's namespace-std extension is non-standard.
- `binder1st`/`binder2nd`/`ptr_fun`/`mem_fun` removed in C++17 (still ship under `_HAS_DEPRECATED_ADAPTOR_TYPEDEFS`). 4 client-path files still use these — not Phase 9's concern.
- STLPort 4.5.3 itself: last release 2008. Unmaintained for 18 years.

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `_HAS_AUTO_PTR_ETC=1` is the right macro to keep `std::auto_ptr` visible under MSVC v143 + /std:c++17 | D-07 spot-check | If wrong, Wave 1 build of boost/smart_ptr.hpp fails with `auto_ptr` not in std. Fix is well-documented (alternative macros: `_HAS_DEPRECATED_ADAPTOR_TYPEDEFS`, or per-file workaround). [ASSUMED — based on training; verify on first build] |
| A2 | `_HAS_DEPRECATED_ADAPTOR_TYPEDEFS=1` is the right macro to keep `binder1st`/`mem_fun` visible | Pitfall #7 | If wrong, ~6 client-path files break compile. Manual lambda replacement is the fallback. [ASSUMED — verify on first build] |
| A3 | `Viewer` re-enable in `engine/client/application/CMakeLists.txt` is a single-line uncomment (per Plan 08-02 inline comment pattern) | CMake removal surface §G | Plan 08-02 claims this pattern but the file itself wasn't enumerated in this audit. If Viewer's CMakeLists.txt is missing, Phase 9 must author one. [ASSUMED — verify by reading `engine/client/application/CMakeLists.txt` at plan-author time] |
| A4 | `DataTable::load()` round-trip baseline can be captured WITHOUT a new test exe — by a one-off invocation of an existing tool | D-06 confirmation | Worst case: a dedicated `DataTableRoundTripTest.exe` must be authored, ~80 lines of CMake + 50 lines of cpp. Trivial. [ASSUMED — recommendation is to use the existing DataTableTool stage, but this researcher did not verify DataTableTool builds in v2 today] |
| A5 | Hash determinism baseline can be captured by reusing existing `Md5sum.exe` or `LabelHashTool.exe` to dump CRC of a fixed string list | Validation Architecture | If both tools have their own bugs that make this hard, baseline becomes a new exe. Still trivial cost. [ASSUMED] |
| A6 | The 47 server-only files with hash_map references have NO impact on v2 client build | Hash_map sweep §"Excluded from Wave 1/2" | If any server file is transitively included by client code (unlikely — server libs are not in client target_link_libraries), Phase 9 misses a sweep target. Boot regression catches this immediately. [VERIFIED: `src/game/client/application/SwgClient/src/CMakeLists.txt` has no server libs in target_link_libraries — A6 is HIGH confidence] |
| A7 | `external/3rd/library/CMakeLists.txt` and `external/ours/library/CMakeLists.txt` have similar STLPort blocks | CMake removal surface §B | If they don't, the Wave 2 commit fails to remove STLPort fully. Cost: one extra grep at plan-author time. [ASSUMED — verify by `grep WHITENGOLD_USE_STLPORT_HEADERS src/external/3rd/library/CMakeLists.txt src/external/ours/library/CMakeLists.txt`] |

**A1, A2, A3, A4, A5, A7 should be confirmed at plan-author time** by quick grep / file-read commands. None block the research finding; all are minor verification tasks for the planner.

---

## Open Questions

1. **Should `StlCompat.h` live in `sharedFoundation/include/public/sharedFoundation/`, or in a new `compat/` subdir?**
   - What we know: D-02 specifies `sharedFoundation/include/public/sharedFoundation/StlCompat.h` — every engine library transitively pulls sharedFoundation headers, so this location reaches everything via PCH propagation.
   - What's unclear: Whether `external/3rd/library/ui/` (which has its own `UIStlFwd.h` STLPort-aware header) can find sharedFoundation/. The ui library currently includes sharedFoundation transitively via swgClientUserInterface, so YES it can.
   - Recommendation: Use D-02's specified location verbatim. No alternative needed.

2. **Should the cleanup commit (Wave 2 end) migrate ALL `stdhash_map<...>::fwd` typedefs to direct `std::unordered_map`, or just the ones in client compile path?**
   - What we know: The cleanup is described as "migrates each call-site." Server-only files have ~16 typedef sites that are out of v2 scope.
   - What's unclear: Whether to include server-only files in the cleanup for hygiene, or skip them per the read-only-server contract.
   - Recommendation: **Cleanup ONLY client-compile-path files in Wave 2.** Server files keep their `stdhash_map<...>::fwd` references — they don't compile in v2 anyway, so the form is a documentation-of-history artifact. Touching them violates the "server contract is read-only" principle in CONTEXT.md.

3. **Does removing `_USE_32BIT_TIME_T=1` belong in Wave 3 or a separate Phase 9.x follow-up?**
   - What we know: D-01 Wave 3 description says "Strip `_USE_32BIT_TIME_T=1` if no longer needed (separate task)."
   - What's unclear: Whether time_t width affects any persisted asset (it shouldn't — TRE asset format predates time_t use in any container layout).
   - Recommendation: **Defer to a separate post-Phase-9 single-line CMake change.** It's orthogonal to STLPort and adds 64-bit time_t exposure that may surface other bugs.

4. **Are there transitive STLPort references in vendored `external/3rd/library/ui/` (the SOE UI lib) beyond hash_map?**
   - What we know: `UIStlFwd.h` does forward-declare `std::hash_map`; 23 ui files have hash_map references.
   - What's unclear: Whether ui library has STLPort-specific allocator usage or COW string assumptions in non-hash_map code.
   - Recommendation: Plan-author should `grep "_STL\|_STLP_\|STLPort" src/external/3rd/library/ui/` before Wave 2 to verify no surprises. Note: `UIStlFwd.h` will need same treatment as `StlForwardDeclaration.h` — include `StlCompat.h` from it during the Wave 2 sweep.

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSVC v143 STL | All targets after Wave 2 | ✓ | 14.4x.x (VS 2022) | — (mandatory) |
| `<unordered_map>` / `<unordered_set>` | StlCompat.h | ✓ | C++11 standard | — |
| Existing CMake build (Phase 6 v1 baseline) | Wave 1 starting point | ✓ | CMake 3.27, VS 2022 | — |
| SWGSource StellaBellum VM | Boot gate at every wave | ✓ | Per memory `project_vm_server_setup.md` | — (manual setup procedure documented) |
| `dumpbin` (VS toolchain) | Final verification gate | ✓ | Bundled with VS 2022 Build Tools | — |
| Existing tools for baseline (Md5sum, LabelHashTool, DataTableTool) | Pre-migration baselines | ✓ (8 tools building post-Phase-8) | Per Phase 8 SUMMARY 04 final tally | If DataTableTool is broken, write a one-off helper exe (~80 lines) |

**Missing dependencies with no fallback:** None.

**Missing dependencies with fallback:** None — Phase 9 is pure-edit work, no new tools or SDKs needed.

---

## Validation Architecture

Config `workflow.nyquist_validation: true` — this section is required.

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Build verification + dumpbin symbol scan + manual boot test (no formal test framework in this codebase) |
| Config file | none — builds + manual VM session are the test |
| Quick run command | `cmake --build build --config Debug --target SwgClient` (per-library compile feedback) |
| Full suite command | `cmake --build build --config Debug` (full graph, including any re-enabled MFC tools in Wave 2) |

### Pre-Migration Baselines (One-Time, Before Wave 1)

The four BEFORE-STL captures from D-04. Save all to `.planning/phases/09-stlport-msvc-stl/baseline/`.

#### 1. Code/symbol grep snapshot — `BEFORE-STL.txt`

```bash
# From src/ working dir; capture every STLPort marker for diff at end of Wave 2.
{
  echo "=== _STL:: namespace use ==="
  grep -rn "_STL::" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453
  echo
  echo "=== _STLP_* macros ==="
  grep -rn "_STLP_" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453
  echo
  echo "=== hash_map / hash_set refs ==="
  grep -rn "hash_map\|hash_set" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453
  echo
  echo "=== auto_ptr ==="
  grep -rn "auto_ptr" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453
  echo
  echo "=== binders / functor adaptors ==="
  grep -rn "binder1st\|binder2nd\|ptr_fun\|mem_fun" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453
  echo
  echo "=== <hash_map> / <hash_set> includes ==="
  grep -rn '#include\s*<hash_map>\|#include\s*<hash_set>' src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453
  echo
  echo "=== <sgi/ refs ==="
  grep -rn "<sgi/" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453
} > .planning/phases/09-stlport-msvc-stl/baseline/BEFORE-STL.txt
```

After Wave 2: re-run the same command into `AFTER-STL.txt` and diff.
Acceptance: **every Wave-2-relevant line in BEFORE goes to zero in AFTER, OR is annotated as intentional in a deviation note.**

#### 2. Runtime baseline metrics — `before-runtime.txt`

```bash
# Manual capture procedure:
# 1. Build current STLPort-based SwgClient_d.exe.
# 2. Launch against SWGSource VM. Reach character select.
# 3. Note frame time at character select (look in client_console.log for FPS).
# 4. Enter Mos Eisley plaza. Note frame time on a 60-second sample.
# 5. Exit. Note exit-leak count from log (current baseline: 8,152 leaks per STATE.md v1 close notes).
# 6. Save: character_select_fps, mos_eisley_fps, exit_leak_count, exit_warning_count → before-runtime.txt
```

Acceptance: After Wave 3, character_select_fps and mos_eisley_fps within ±10%, exit_leak_count exactly the same modulo expected singletons.

#### 3. Hash determinism baseline — `before-crc.txt`

```bash
# Procedure:
# Pick ~100 known-input strings: a fixed list of TRE asset paths from the SWG retail tree
#   (e.g., the first 100 lines of publish/data_<patch>.txt).
# For each string, compute Crc::calculateWithToLower (the persistent hash).
# Save: <string>\t<hex-crc>\t<size_t-of-std::hash> per line.
# Use existing tool: LabelHashTool builds a hash for a given label using the same engine
#   path (CrcLowerString::calculateCrc → Crc32::calculateWithToLower).
# Format: each line "input_string\t0xCAFEBABE\t0xDEADBEEF" (CRC + std::hash).
```

Acceptance: After Wave 3, **every CRC column matches exactly** (CRC is custom, unchanged by STL). The std::hash column WILL DIFFER — that's expected; document the divergence as proof that no code persists std::hash output.

#### 4. Round-trip persist baseline — `before-datatable-round-trip.txt`

See "D-06 Confirmation" section above. Use a known datatable from the SWG retail TRE tree (recommendation: `misc/quest_task_data.iff` — small, ~20 columns). Dump every cell as text. Acceptance: byte-identical match after Wave 3.

### Per-Wave Validation Commands

#### Boot gate (every wave)

```cmd
:: From src/ build dir; assumes Phase 6 SwgClient.cfg + VM are already configured.
cmake --build build --config Debug --target SwgClient
build\bin\Debug\SwgClient_d.exe
:: Manual: log into SWGSource VM. Confirm character-select screen renders.
:: PASS if character-select renders + no fatal in client_console.log.
:: FAIL if compile error, link error, or crash before character select.
```

#### Code/symbol gate (Waves 1, 2, 3)

```bash
# Wave 1 endpoint: same grep set as BEFORE; expect engine/shared/library/* refs gone.
# Wave 2 endpoint: expect engine/client/library/*, game/*, external/* refs gone.
# Wave 3 endpoint: also confirm /Zc:wchar_t- is gone:
grep -n "/Zc:wchar_t-\|FORCE:MULTIPLE\|stlport_vc71_static.lib\|find_package(STLPort\|WHITENGOLD_USE_STLPORT_HEADERS\|stlport_vc143_compat\|_STLP_DONT_FORCE_MSVC_LIB_NAME" src/ -r --include="CMakeLists.txt" --include="*.cmake"
# Acceptance: ZERO matches.
```

#### MFC canary gate (Wave 2 only)

```cmd
cmake --build build --config Debug --target Turf
cmake --build build --config Debug --target Viewer
:: Acceptance: BOTH exit 0. Compile + link clean (no STLPort ↔ <afxwin.h> ↔ <type_traits> errors).
```

### Final Validation Commands (After Wave 3)

#### dumpbin /imports — zero stlport symbols

```cmd
dumpbin /imports build\bin\Debug\SwgClient_d.exe | findstr /i "stlport"
:: Acceptance: ZERO output. (No stlport_vc71_static, stlport_vc143_compat, or _STL:: symbols.)
:: Capture full dumpbin for archival:
dumpbin /imports build\bin\Debug\SwgClient_d.exe > .planning/phases/09-stlport-msvc-stl/baseline/after-imports.txt
```

#### Hash determinism re-run

```bash
# Re-run #3 baseline procedure → after-crc.txt.
diff .planning/phases/09-stlport-msvc-stl/baseline/before-crc.txt .planning/phases/09-stlport-msvc-stl/baseline/after-crc.txt
# Acceptance: every CRC column matches exactly.
# std::hash column differences are EXPECTED and acceptable; document in Phase 9 SUMMARY.
```

#### Round-trip persist re-run

```bash
# Re-run #4 baseline procedure → after-datatable-round-trip.txt.
diff before-datatable-round-trip.txt after-datatable-round-trip.txt
# Acceptance: byte-identical.
```

#### Sort stability check

```cpp
// 50-line helper: pin a known input array of CrcLowerString.
// Run std::sort(arr.begin(), arr.end(), CrcLowerString::LessAbcOrderReferenceComparator{}).
// Dump indexes after sort.
// Acceptance: pre/post Phase 9 dumps match. (sort algorithm is deterministic for unique
// inputs; this catches accidental change in the comparator behavior.)
```

#### Frame-time + leak-count diff

```bash
# Compare baseline #2 capture against post-Wave-3 capture.
# Acceptance:
#   character_select_fps:  within ±10% of baseline
#   mos_eisley_fps:        within ±10% of baseline
#   exit_leak_count:       exact match (modulo +/- expected singletons; document any drift)
#   exit_warning_count:    no NEW warnings; baseline 156k DEBUG_WARNINGs allowed (per STATE.md)
```

### Sampling Rate

- **Per task commit:** `cmake --build build --config Debug --target <library-being-edited>` (per-lib feedback)
- **Per wave merge:** `cmake --build build --config Debug` (full graph)
- **Phase gate:** All four FINAL validation commands pass before `/gsd-verify-work`

### Wave 0 Gaps

- [ ] `.planning/phases/09-stlport-msvc-stl/baseline/` — directory must be created
- [ ] `BEFORE-STL.txt` — code/symbol baseline grep capture
- [ ] `before-runtime.txt` — runtime metrics (manual VM session)
- [ ] `before-crc.txt` — hash determinism baseline (~100 strings + Crc + std::hash dump)
- [ ] `before-datatable-round-trip.txt` — DataTable load + cell-dump from a known retail datatable
- [ ] (No new test framework or library install needed — all baselines reuse existing tools)

*(Existing test infrastructure: build is the test. No pytest, no Jest. Baselines are diff-based golden files.)*

---

## Security Domain

`security_enforcement: true` in config.json. However this phase is **build-system + ABI migration** with no application code change to authentication, input handling, network endpoints, or cryptography. The migration may surface latent bugs (e.g., sort instability, hash divergence on persisted output) but does not introduce new security surfaces.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | No | No auth code changes |
| V3 Session Management | No | No session code changes |
| V4 Access Control | No | No access-control changes |
| V5 Input Validation | No | No new input handling |
| V6 Cryptography | **Indirect** — `Crc::calculateWithToLower` (custom CRC32) is in scope; std::hash is in scope | **Cryptographic determinism** is verified by the hash-determinism baseline (D-04 + Validation Architecture above). CRC32 is NOT a cryptographic hash, but its determinism IS persisted in TRE assets — any divergence is fatal |
| V11 Configuration | No | No new config keys |

### Known Threat Patterns

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Persisted hash divergence (TRE asset CRC mismatch causes asset load failures) | Tampering / DoS | D-04 hash determinism baseline — captures `Crc::calculateWithToLower` over fixed input list, gates on byte-identical match post-migration |
| Sort instability (rendering order changes after STL swap) | None (cosmetic) | D-04 sort stability gate — fixed input + deterministic comparator, dump pre/post |
| Memory leak count drift after allocator ABI shift | DoS (gradual) | D-04 leak-count baseline — exit-leak count must match modulo expected singletons |

No additional security controls beyond the four D-04 verification gates. The security model of v2 client (read TRE assets, talk UDP to server) is unchanged by Phase 9.

---

## Sources

### Primary (HIGH confidence)
- `D:/Code/swg-client/src/CMakeLists.txt` (full read) — root build flags, find_package, NODEFAULTLIB enumeration [VERIFIED]
- `D:/Code/swg-client/src/cmake/win32/FindSTLPort.cmake` (full read) — staging logic, prebuilt lib paths, patch script [VERIFIED]
- `D:/Code/swg-client/src/external/3rd/library/stlport453/src/CMakeLists.txt` (full read) — compat shim target [VERIFIED]
- `D:/Code/swg-client/src/external/3rd/library/stlport453/src/stlport_vc143_compat.cpp` (partial read, header) — virtual method bridges [VERIFIED]
- `D:/Code/swg-client/src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h` (full read) — `std::hash_map` forward-decl + unicode_char_t typedef site #2 [VERIFIED]
- `D:/Code/swg-client/src/external/ours/library/unicode/src/shared/Unicode.h` (full read) — unicode_char_t typedef site #1 [VERIFIED]
- `D:/Code/swg-client/src/engine/shared/library/sharedObject/src/shared/object/NetworkIdManager.h` (full read) — D-06 candidate [VERIFIED]
- `D:/Code/swg-client/src/engine/shared/library/sharedObject/src/shared/object/NetworkIdManager.cpp` (partial read) — D-06 candidate, no IFF persist [VERIFIED]
- `D:/Code/swg-client/src/engine/shared/library/sharedFoundation/src/shared/CrcLowerString.h` (full read) — D-06 candidate [VERIFIED]
- `D:/Code/swg-client/src/engine/shared/library/sharedFoundation/src/shared/CrcLowerString.cpp` (partial read) — confirmed CRC32-based, not std::hash [VERIFIED]
- `D:/Code/swg-client/src/engine/shared/library/sharedUtility/src/shared/DataTable.h` (full read) — **D-06 confirmed target** [VERIFIED]
- `D:/Code/swg-client/src/engine/shared/library/sharedUtility/src/shared/DataTable.cpp` (grepped — confirmed `void load(Iff &)` + `m_dataTableIffId = TAG(D,T,I,I)`) [VERIFIED]
- `D:/Code/swg-client/src/external/3rd/library/boost/boost/config.hpp` (full read) — D-07 STLPort detection blocks at lines 444, 480, 507, 551, 576, 590; MSVC fallback at 480-555 [VERIFIED]
- `D:/Code/swg-client/src/external/3rd/library/boost/boost/smart_ptr.hpp` (full read) — D-07 std::auto_ptr usage at lines 150, 165 [VERIFIED]
- `D:/Code/swg-client/src/external/3rd/library/boost/boost/static_assert.hpp` (full read) — pure macro, no STL deps [VERIFIED]
- `D:/Code/swg-client/src/external/3rd/library/boost/boost/utility.hpp` (partial read) — pure templates [VERIFIED]
- `D:/Code/swg-client/src/game/client/application/SwgClient/src/CMakeLists.txt` (full read) — `${STLPORT_LIBRARY}`, FORCE:MULTIPLE site, target_link_libraries [VERIFIED]
- 19 grep-confirmed `LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"` site list [VERIFIED]
- 64 grep-confirmed `#include <hash_map>` / `<hash_set>` site list [VERIFIED]
- 89 grep-confirmed hash_map/hash_set total file list [VERIFIED]
- 83 grep-confirmed unicode_char_t site list [VERIFIED]
- `D:/Code/swg-client/.planning/phases/09-stlport-msvc-stl/09-CONTEXT.md` (full read) — D-01 through D-07 [VERIFIED]
- `D:/Code/swg-client/.planning/phases/08-dead-code-removal-track-b/08-01-SUMMARY.md` (full read) — 5 MFC deferral list [VERIFIED]
- `D:/Code/swg-client/.planning/phases/08-dead-code-removal-track-b/08-02-SUMMARY.md` (full read) — 10 MFC deferral list + STLPort↔afxwin↔type_traits collision characterization [VERIFIED]
- `D:/Code/swg-client/CLAUDE.md` — project instructions [VERIFIED]
- User memory `project_post_v1_exploration.md`, `project_stlport_fstream_crash.md`, `feedback_source_edits_allowed.md`, `project_v2_scope.md` [VERIFIED]

### Secondary (MEDIUM confidence)
- Auto_ptr deprecation behavior in MSVC v143 / `/std:c++17` — based on training [ASSUMED — A1, see Assumptions Log]
- `_HAS_DEPRECATED_ADAPTOR_TYPEDEFS` flag for binder1st/mem_fun — based on training [ASSUMED — A2]

### Tertiary (LOW confidence)
- None.

---

## Metadata

**Confidence breakdown:**
- CMake removal surface: HIGH — every site grepped and counted directly from working tree
- Hash_map sweep inventory: HIGH — 89 raw matches deduplicated to 42 in-client-path files via direct read of SwgClient/src/CMakeLists.txt's target_link_libraries list
- D-06 round-trip target (DataTable): HIGH — three independent criteria (IFF persist, std::vector<std::string> contents, hash_map field) all verified by direct file read
- D-07 boost spot-check: HIGH — all 4 headers fully read, STLPort detection blocks identified at exact line numbers
- wchar_t flip surface: HIGH — 0 raw mbstate_t hits in client compile path, 1 cast site identified (CuiNotepad.cpp:146)
- STLPort-specific behaviors: HIGH for client-path audit; MEDIUM for assumptions about `_HAS_AUTO_PTR_ETC` macro names (A1, A2)
- Validation Architecture: HIGH — derived directly from D-04 with concrete commands

**Research date:** 2026-05-08
**Valid until:** 2026-06-08 (stable — codebase unlikely to change before Phase 9 plan authoring)
