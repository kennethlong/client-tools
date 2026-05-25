# Phase 9: STLPort → MSVC STL — Pattern Map

**Mapped:** 2026-05-08
**Files analyzed:** ~45 (modify + delete + author surface across CMake / source / typedef / flag tiers)
**Analogs found:** 42 / 45 — every modify-target has a concrete reflog or HEAD analog; only 3 author-from-scratch entries (StlCompat.h re-author, baseline manifests, plan documents) lack a same-shape analog.

## Critical Constraint Reminder for the Planner (D-08, binding)

**All boot-test verification mentioned anywhere in this map runs against `D:/Code/swg-client/build-v145/bin/Debug/SwgClient_d.exe`** — NOT `D:/Code/swg-client/build/bin/Debug/SwgClient_d.exe`.

The archived plans (09-01..09-04 in `archive/`) and the BEFORE-STL.txt baseline scripts in 09-RESEARCH.md repeatedly invoke `cmake --build build --config Debug` / `build/bin/Debug/SwgClient_d.exe`. **The planner MUST translate every `build/` reference to `build-v145/`** when the equivalent step lands in the new wave structure, and reconfigure with the v145 generator flags from CLAUDE.md ("Build toolchain paths"):

```
cmake.exe path: "D:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
generator:      -G "Visual Studio 18 2026" -A Win32 -T v145,host=x64
policy minimum: -DCMAKE_POLICY_VERSION_MINIMUM=3.5
```

The 2016 SWGSource v3.0 community-rebuilt DLL set is staged in `build-v145/bin/Debug/` per Wave-1 of the new plan (D-10). The boot gate is **zone-into-Tatooine**, not just character-select.

---

## File Classification

Files cluster into five roles. Data flow is uniform across this phase (build-system + source-edit, no runtime data flow), so it's collapsed into a single column.

| New / Modified File | Role | Closest Analog | Match Quality |
|---|---|---|---|
| **Wave-1 anchor wave (D-10) — author baseline & manifests; no source edits** | | | |
| `.planning/phases/09-stlport-msvc-stl/baseline/before-imports-replan2.txt` | baseline-capture | spike-001a `before-imports.txt` (against `build/`, stale) | role-match |
| `.planning/phases/09-stlport-msvc-stl/baseline/dll-baseline-2016.txt` | manifest-author (new) | `.continue-here.md` MD5 list (lines 213–220) | role-match |
| **Wave-2 (sweep) — source-include rewrite** | | | |
| `src/engine/shared/library/sharedFoundation/include/public/sharedFoundation/StlCompat.h` | C++ header author (re-author after rollback) | reflog `df4bd21d3` (StlCompat.h authored) + `e8b59f704` (hash_multimap augmentation) | exact (cherry-pick candidate per D-12) |
| `src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h` | C++ typedef-flip + include-rewrite | reflog `d9ca1269a` (group 2.1) — exact same edit | exact |
| `src/engine/client/library/CMakeLists.txt` | CMake tier-parent block removal | reflog `8b45e279f` (Wave-2 unblock) + Wave-1 sibling `df4bd21d3` | exact |
| `src/engine/shared/library/sharedDebug/src/shared/LeakFinder.h` | C++-include-rewrite | reflog `9acbc7f05` — itself the canonical analog | exact (same file, cherry-pick candidate) |
| `src/engine/shared/library/sharedSkillSystem/src/shared/SkillManager.{cpp,h}` | C++-include-rewrite | reflog `a915b3f76` (group 2.9) | exact |
| `src/engine/shared/library/sharedNetworkMessages/src/shared/common/GameNetworkMessage.cpp` | C++-include-rewrite | reflog `ddaaf753f` (group 2.6) | exact |
| `src/engine/shared/library/sharedNetwork/src/{shared,win32}/Address.{cpp,h}` | C++-include-rewrite | reflog `9acbc7f05` (sweep pattern) | exact |
| `src/engine/shared/library/sharedMessageDispatch/src/shared/MessageManager.cpp` + `FirstSharedMessageDispatch.h` | C++-include-rewrite | reflog `1fd6ace00` (group 2.7) | exact |
| `src/engine/shared/library/sharedMath/src/shared/PositionVertexIndexer.h` | C++-include-rewrite | reflog `8086b2b83` (group 2.3) | exact |
| `src/engine/shared/library/sharedGame/src/shared/space/{ShipComponentType,ShipChassisSlotType}.cpp` | C++-include-rewrite | reflog `2a1c1c854` (group 2.10) | exact |
| `src/engine/shared/library/sharedGame/src/shared/object/Waypoint.cpp` | C++-include-rewrite | reflog `2a1c1c854` (group 2.10) | exact |
| `src/engine/shared/library/sharedGame/src/shared/combat/CombatDataTable.cpp` | C++-include-rewrite | reflog `2a1c1c854` (group 2.10) | exact |
| `src/engine/shared/library/sharedObject/src/shared/object/NetworkIdManager.{cpp,h}` | C++-include-rewrite | reflog `7fdbd2a46` (group 2.8) | exact |
| `src/engine/shared/library/sharedUtility/src/shared/{DataTable,WorldSnapshotReaderWriter}.{cpp,h}` | C++-include-rewrite | reflog `5d3ee4c01` (group 2.4) | exact |
| `src/external/ours/library/localization/src/shared/LocalizationManager.h` + `src/CMakeLists.txt` | C++-include-rewrite + CMake include-path add | reflog `e8b59f704` (group 2.11) — both files shown together | exact |
| `src/engine/client/library/clientGame/src/shared/appearance/LightsaberCollisionManager.cpp` | C++-include-rewrite | reflog `5a1e03e59` (group 2.12) | exact |
| `src/engine/client/library/clientGame/src/shared/core/ClientBuffManager.cpp` | C++-include-rewrite (high density: 12 hash_map refs) | reflog `5a1e03e59` (sweep pattern; not in original group but identical edit) | role-match |
| `src/engine/client/library/clientUserInterface/src/shared/core/CuiSkillManager.cpp` | C++-include-rewrite | reflog `5a1e03e59` (group 2.12) | exact |
| `src/engine/client/library/clientUserInterface/src/shared/core/CuiMediator.cpp` | C++-include-rewrite (`<typeinfo.h>` → `<typeinfo>`, NOT StlCompat.h) | reflog `5a1e03e59` (group 2.12 — handled the same special case) | exact |
| `src/engine/client/library/clientObject/src/shared/appearance/ComponentAppearanceTemplate.cpp` | C++-include-rewrite | reflog `5a1e03e59` (group 2.12) | exact |
| `src/external/3rd/library/ui/src/shared/core/UILowerString.{cpp,h}` | C++-include-rewrite | reflog `24e203e82` (group 2.13) | exact |
| `src/external/3rd/library/ui/src/shared/core/UiMemoryBlockManager.h` | C++-include-rewrite (BOTH hash_map AND hash_set — collapse to single StlCompat.h) | reflog `24e203e82` (group 2.13 — same collapse pattern) | exact |
| `src/external/3rd/library/ui/src/win32/UIBaseObject.cpp` | C++-include-rewrite (BOTH hash_map AND hash_set — collapse) | reflog `24e203e82` (group 2.13) | exact |
| `src/external/3rd/library/ui/src/win32/UIManager.cpp` | C++-include-rewrite | reflog `24e203e82` (group 2.13) | exact |
| `src/external/3rd/library/ui/src/win32/UIStlFwd.h` | C++-include-rewrite (delete `stl/_config.h`, add StlCompat.h — analogue of StlForwardDeclaration.h treatment) | reflog `edad2dabe` (Wave 2 portability — UIStlFwd.h handled the same way) | exact |
| `src/external/3rd/library/ui/src/win32/UITextStyle.{cpp,h}` | C++-include-rewrite | reflog `24e203e82` (group 2.13) | exact |
| `src/external/3rd/library/ui/src/win32/UILoader.h` | C++-include-rewrite | reflog `24e203e82` (group 2.13) | exact |
| `src/external/3rd/library/ui/src/shared/boundary/UIWidgetBoundaries.h` | C++-include-rewrite | reflog `24e203e82` (group 2.13 sweep pattern) | role-match |
| `src/external/3rd/application/UiBuilder/{DefaultObjectPropertiesManager,SimpleSoundCanvas}.h` (and engine/client/application/UiBuilder/ duplicates) | C++-include-rewrite | reflog `898f853a2` (group 2.14) | exact |
| `src/external/ours/application/LocalizationTool/src/shared/LocalizationData.h` | C++-include-rewrite | reflog `898f853a2` (group 2.14) | exact |
| `src/game/client/library/swgClientUserInterface/src/shared/page/{SwgCuiSpaceRadar,SwgCuiExpMonitor}.{cpp,h}` | C++-include-rewrite | reflog `248127e60` (group 2.15) | exact |
| `src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserUI.cpp` | C++-include-rewrite | reflog `248127e60` (group 2.15) | exact |
| **Wave-3 (link-cleanup) — CMake link-ref + flag removal** | | | |
| `src/CMakeLists.txt` | CMake compile-flag + linker-flag + find_package removal | reflog `2bdfa8325` — root STLPort include-path removal (closest precedent edit on the same file) | role-match (different stanza, same file/pattern) |
| `src/game/client/application/SwgClient/src/CMakeLists.txt` | per-target STLPort link-ref removal (`${STLPORT_LIBRARY}`, `/FORCE:MULTIPLE`, `target_link_directories(stlport453)`, `$<TARGET_OBJECTS:stlport_vc143_compat>`) | archived `09-02-PLAN` Task 4 + current Turf/CMakeLists.txt as the "before" template | role-match (no prior commit landed; archived plan is the template) |
| `src/engine/shared/application/Turf/CMakeLists.txt` | per-target STLPort link-ref removal + Phase-9 canary | archived `09-02-PLAN` Task 5 (Turf-specific section) — references the live file at HEAD as the "before" | role-match |
| `src/engine/client/application/Viewer/CMakeLists.txt` | author-from-scratch CMake (Phase-9 canary) | existing `src/engine/shared/application/Turf/CMakeLists.txt` (live HEAD) — explicit-source-list MFC pattern | role-match (closest live MFC tool template) |
| `src/engine/shared/application/{Md5sum,UpdateLocalizedStrings,TreeFileBuilder,TreeFileExtractor,TreeFileRspBuilder,DataLintRspBuilder,StringFileTool,WordCountTool,LabelHashTool,TemplateCompiler,TemplateDefinitionCompiler,P4Qt}/CMakeLists.txt` (12 tools) | per-target STLPort link-ref removal | archived `09-02-PLAN` Task 4 + current Turf/CMakeLists.txt | role-match |
| `src/external/ours/application/LocalizationToolCon/CMakeLists.txt` | per-target STLPort link-ref removal | archived `09-02-PLAN` Task 4 | role-match |
| `src/game/server/application/CMakeLists.txt` | server-tier defer (comment out `add_subdirectory(SwgBattlefieldTool/SwgSchematicXmlParser)` — Phase-8-style deferral pattern) | live `src/engine/shared/application/CMakeLists.txt` lines 11–21 (Phase-8 deferred-tool comment block) | exact (same pattern, same file shape) |
| `src/engine/shared/application/CMakeLists.txt` | uncomment `add_subdirectory(Turf)` | reverse of Phase-8 commit that introduced the comment (Phase 08-01 SUMMARY) | role-match |
| `src/engine/client/application/CMakeLists.txt` | uncomment `add_subdirectory(Viewer)` | reverse of Phase-8 commit (Phase 08-02 SUMMARY) | role-match |
| `src/cmake/win32/FindSTLPort.cmake` | DELETE (entire file) | no prior delete-an-entire-cmake-module precedent in this repo | no analog (mechanical `git rm`) |
| `src/external/3rd/library/stlport453/` (entire directory subtree) | DELETE (subtree) | no prior subtree-delete precedent | no analog (mechanical `git rm -r`) |
| **Wave-4 (wchar_t flip)** | | | |
| `src/external/ours/library/unicode/src/shared/Unicode.h` | C++ typedef flip (`unsigned short` → `wchar_t`) | no prior flip in this repo (one-time event); archived `09-03-PLAN` Task 2 documents the exact edit | no analog (one-time edit; archived plan is the template) |
| `src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h` | C++ typedef flip (must be in SAME commit as Unicode.h per D-03) | same as above | no analog |
| `src/CMakeLists.txt` (compile-flag flip — remove `/Zc:wchar_t-`) | CMake compile-flag mutation | reflog `df4bd21d3` (added `_HAS_AUTO_PTR_ETC=1` — same flag-mutation pattern on the same file) | role-match |
| `src/engine/client/library/clientUserInterface/src/shared/page/CuiNotepad.cpp` | source-cast site fix (`unsigned short *` → `wchar_t *`) | reflog `edad2dabe` (Wave 2 portability — same kind of per-line cast-fix pattern with a comment marker) | role-match |
| **Wave-5 (idiom-portability spillover, incremental)** | | | |
| Per-file mechanical edits as the v145 build surfaces them (`push_back()` → `emplace_back()`, `<typeinfo.h>` → `<typeinfo>`, missing `<iterator>`, iterator default-init, `make_pair` template-arg drops, etc.) | C++ idiom-portability fix | reflog `831304d35` (Wave-1, 8 fixes) AND reflog `edad2dabe` (Wave-2, ~30 fixes — much richer pattern catalog) | exact |
| **Cleanup commit (post-Wave-5)** | | | |
| Bulk `std::hash_map<` → `std::unordered_map<`, `std::hash_set<` → `std::unordered_set<`, `std::hash_multimap<` → `std::unordered_multimap<` across all client-tier files; remove StlCompat.h includes; add direct `<unordered_map>`/`<unordered_set>` includes; DELETE StlCompat.h | C++ bulk find-replace + header delete | no prior bulk-rename commit in this repo | role-match (archived `09-04-PLAN` Task 4 documents the exact mechanics) |

---

## Pattern Assignments

### Pattern A: C++ include-rewrite (single-file, mechanical)

**Applies to:** every file in the "C++-include-rewrite" rows above (~25 files in client tier; the engine/shared rows already executed once in Wave 1 of the rolled-back attempt and may be cherry-picked or re-executed per D-12).

**Canonical analog:** reflog commit `9acbc7f05` (`feat(09-01): sharedDebug — hash_map → StlCompat.h (group 2.2)`). Diff is one line in one file:

```diff
--- a/src/engine/shared/library/sharedDebug/src/shared/LeakFinder.h
+++ b/src/engine/shared/library/sharedDebug/src/shared/LeakFinder.h
@@ -17,7 +17,7 @@

 #include <vector>
 #include <list>
-#include <hash_map>
+#include "sharedFoundation/StlCompat.h"

 // ======================================================================
```

**Mechanical rule:** find the line `#include <hash_map>` (or `<hash_set>`); replace with `#include "sharedFoundation/StlCompat.h"`. Do NOT touch any other line. Call sites (`std::hash_map<K,V> foo;`) stay verbatim — they resolve through the alias.

**Special cases observed in the reflog:**

1. **Both `<hash_map>` AND `<hash_set>` in the same file** (`UiMemoryBlockManager.h`, `UIBaseObject.cpp`): collapse both lines to a single `#include "sharedFoundation/StlCompat.h"`. Pattern from reflog `24e203e82` (group 2.13 commit body explicitly notes "9 raw include instances collapsed into 7 StlCompat.h includes").

2. **`<typeinfo.h>` (legacy STLPort compatibility header)** → `<typeinfo>` (standard MSVC header). NOT routed through StlCompat.h. Pattern from reflog `5a1e03e59` (group 2.12, CuiMediator.cpp).

3. **STLPort-specific `<stl/_config.h>` includes** (found in some ui/* and sharedFoundation/StlForwardDeclaration.h): DELETE the line outright; the StlCompat.h include subsumes the forward-decl needs. Pattern from reflog `2bdfa8325` (root removal commit body) and `edad2dabe` (UIStlFwd.h treatment).

4. **External-tier file needs sharedFoundation include path** (only `external/ours/library/localization/`): also add to `localization/src/CMakeLists.txt`:
   ```cmake
   include_directories(
       ...existing...
       # Phase 9 — reach sharedFoundation/StlCompat.h for the alias bridge.
       # Header-only dependency; does not introduce a link-time edge.
       ${SWG_ENGINE_SOURCE_DIR}/shared/library/sharedFoundation/include/public
   )
   ```
   Pattern from reflog `e8b59f704` (group 2.11) — Rule-3 portability fix.

---

### Pattern B: StlCompat.h re-author

**Applies to:** `src/engine/shared/library/sharedFoundation/include/public/sharedFoundation/StlCompat.h` (this file is NOT at HEAD — confirmed by `ls` of that directory; it was authored in reflog `df4bd21d3` and rolled back by `f9512e663`).

**Canonical analog:** reflog `df4bd21d3` (initial author) + reflog `e8b59f704` (hash_multimap + `<string>` augmentation). Verbatim content per CONTEXT D-02:

```cpp
// src/engine/shared/library/sharedFoundation/include/public/sharedFoundation/StlCompat.h
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

    // hash_multimap alias added in reflog `e8b59f704`; keep it.
}
```

**Cherry-pick option per D-12:** `git show df4bd21d3:src/engine/shared/library/sharedFoundation/include/public/sharedFoundation/StlCompat.h > <dest>` then layer in the `e8b59f704` augmentation, OR `git cherry-pick df4bd21d3 e8b59f704` if the planner verifies they apply cleanly to `35b872357` HEAD.

**HARD CONSTRAINT (D-02-clarification):** Do NOT add `#ifdef _STLP_VERSION` coexistence guards. Spike 001b INVALIDATED that approach. The header is safe ONLY when STLPort headers are absent from every TU (which the Wave-2 sweep guarantees).

---

### Pattern C: StlForwardDeclaration.h wire-in

**Applies to:** `src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h`.

**Canonical analog:** reflog `d9ca1269a` (group 2.1). The diff against current HEAD:

```diff
--- a/src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h
+++ b/src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h
@@ -32,9 +32,12 @@

 // Phase 9 Wave 1 — `stl/_config.h` was the STLPort header that defined the
 // _STL macros. Once the root STLPort include path is removed (Wave 1 Option A),
-// this header is no longer reachable. The forward-declaration block below
-// uses only `class hash_map`/`class hash_set` which are now provided by
-// sharedFoundation/StlCompat.h (added in Task 2.1).
+// this header is no longer reachable. The hash_map / hash_set names are now
+// provided by sharedFoundation/StlCompat.h as `using` aliases for
+// std::unordered_map / std::unordered_set; the forward-decl lines for
+// hash_map and hash_set have been deleted because the alias subsumes them.
+
+#include "sharedFoundation/StlCompat.h"

 namespace std
 {
@@ -54,10 +57,8 @@ namespace std
 	template <class _Tp, class _Sequence>                                               class  stack;
 	template <class _Tp, class _Container, class _Compare>                              class  priority_queue;
 	template <class _Key, class _Tp, class _Compare, class _Alloc>                      class  map;
-	template <class _Key, class _Tp, class _HashFcn, class _Compare, class _Alloc>      class  hash_map;
 	template <class _Key, class _Tp, class _Compare, class _Alloc>                      class  multimap;
 	template <class _Key, class _Compare, class _Alloc>                                 class  set;
-	template <class _Key, class _HashFcn, class _Compare, class _Alloc>                 class  hash_set;
 	template <class _Key, class _Compare, class _Alloc>                                 class  multiset;
```

**Verification at HEAD:** lines 54 and 57 of `StlForwardDeclaration.h` currently contain those `class hash_map` and `class hash_set` forward-decls (confirmed via grep at HEAD); the file IS in the rolled-back state.

---

### Pattern D: CMake tier-parent block removal

**Applies to:** `src/engine/client/library/CMakeLists.txt` (currently at HEAD with the block present — confirmed by Read; lines 1-3 are the if/include_directories/endif block).

**Canonical analog:** reflog `df4bd21d3` (Wave-1 sibling — same edit applied to engine/shared/library, external/ours/library, external/3rd/library tier parents). Verbatim diff pattern:

```diff
--- a/src/engine/shared/library/CMakeLists.txt
+++ b/src/engine/shared/library/CMakeLists.txt
@@ -1,7 +1,3 @@
-if(WHITENGOLD_USE_STLPORT_HEADERS)
-	include_directories(BEFORE ${STLPORT_INCLUDE_DIR})
-endif()
-
 add_subdirectory(sharedFoundationTypes)
```

**Mechanical rule:** delete lines 1-3 (the 3-line `if/include_directories/endif` block) PLUS the blank line following. First non-blank line of the file becomes `# Tier 6 (basic — no HIGH RISK SDK; clientDirectInput first per D-11)`.

**Pre-existing instance in the rolled-back commit set:** reflog `8b45e279f` already removed exactly this block (Wave-2 first commit). Cherry-pick candidate per D-12.

---

### Pattern E: Per-target STLPort link-reference removal

**Applies to:** ~18 CMakeLists.txt files (SwgClient + 12 engine/shared tools + LocalizationToolCon + Turf canary + 2 server-tier deferrals).

**Best live "before" template:** `src/engine/shared/application/Turf/CMakeLists.txt` at HEAD. The exact stanzas to remove are:

```cmake
add_executable(Turf
    ${SHARED_SOURCES}
    ${WIN32_SOURCES}
    $<TARGET_OBJECTS:stlport_vc143_compat>     # ← REMOVE
)

target_link_libraries(Turf PRIVATE
    ...
    ${STLPORT_LIBRARY}                          # ← REMOVE
    legacy_stdio_definitions
    ...
)

set_target_properties(Turf PROPERTIES
    LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"          # ← REMOVE entire property OR remove the LINK_FLAGS_DEBUG line if it's the only one
)
```

**SwgClient also has** (lines 171-173 of `src/game/client/application/SwgClient/src/CMakeLists.txt`):

```cmake
target_link_directories(SwgClient PRIVATE
    ${SWG_EXTERNALS_FIND}/stlport453/lib/win32   # ← REMOVE entire target_link_directories block
)
```

**Closest plan analog:** archived `09-02-PLAN.md` Tasks 4-6. No commit landed for this in the reflog (Wave 3 was never executed under the prior plan), so the archived plan is the template. The planner should treat the archived 09-02 as a per-task script and update each `cmake --build build` reference to `cmake --build build-v145`.

---

### Pattern F: Root CMakeLists.txt mutation (compile-flag + find_package)

**Applies to:** `src/CMakeLists.txt` — multiple distinct stanzas across Waves 3 and 4.

**Wave-3 stanzas to remove** (current state at HEAD):

| Line | Current | Action |
|---|---|---|
| 46 | `set(CMAKE_EXE_LINKER_FLAGS "... /NODEFAULTLIB:libc.lib /NODEFAULTLIB:stlport_vc71_static.lib /SAFESEH:NO")` | Remove `/NODEFAULTLIB:stlport_vc71_static.lib` token |
| 55 | `find_package(STLPort REQUIRED)` | DELETE entire line |
| 56 | `option(WHITENGOLD_USE_STLPORT_HEADERS "..." ON)` | DELETE entire line |
| 57-62 | `if(WHITENGOLD_USE_STLPORT_HEADERS) include_directories(BEFORE ${STLPORT_INCLUDE_DIR}) add_compile_definitions(_STLP_DONT_FORCE_MSVC_LIB_NAME) set(...) set(...) endif()` | DELETE entire if-block |

**Wave-4 stanza to mutate** (lines 43-44):

```cmake
set(CMAKE_CXX_FLAGS_DEBUG "...  /Zc:wchar_t- /Ob1 /FC")     # ← strip /Zc:wchar_t-
set(CMAKE_CXX_FLAGS_RELEASE "... /Zc:wchar_t- /Ob1 /FC")    # ← strip /Zc:wchar_t-
```

**Canonical analog (root file mutation precedent):** reflog `df4bd21d3` (added `_HAS_AUTO_PTR_ETC=1` to root CMakeLists.txt CXX flags) + reflog `2bdfa8325` (removed the WHITENGOLD_USE_STLPORT_HEADERS option's payload — leaves a comment marker explaining why). The `2bdfa8325` diff is the template comment-style:

```diff
-option(WHITENGOLD_USE_STLPORT_HEADERS "Place vendored STLPort headers before MSVC headers" ON)
-if(WHITENGOLD_USE_STLPORT_HEADERS)
-	include_directories(BEFORE ${STLPORT_INCLUDE_DIR})
-	add_compile_definitions(_STLP_DONT_FORCE_MSVC_LIB_NAME)
-	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /I\"${STLPORT_INCLUDE_DIR}\" /D_STLP_DONT_FORCE_MSVC_LIB_NAME")
-	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /I\"${STLPORT_INCLUDE_DIR}\" /D_STLP_DONT_FORCE_MSVC_LIB_NAME")
-endif()
+# Phase 9 Wave 1 (Option A) — Root STLPort include-path block removed because
+# STLPort 4.5.3's stl/_epilog.h does `#define std STLPORT`, which renders
+# MSVC's <unordered_map> unparseable in any TU that already saw an STLPort header.
```

The Wave-3 pass under the new plan removes ALL of `find_package(STLPort)`, the option declaration, and the if-block (since by Wave 3 there's no per-target reference to `${STLPORT_LIBRARY}` left to keep alive).

---

### Pattern G: wchar_t typedef flip (one-time, atomic)

**Applies to:** `src/external/ours/library/unicode/src/shared/Unicode.h` line 27 AND `src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h` line 125 — **must be in the SAME commit per D-03**. Confirmed at HEAD: both files have `typedef unsigned short unicode_char_t;` (verified via Read).

**No prior repo analog** (one-time ABI flip). Closest pattern is the archived `09-03-PLAN.md` Task 2 — it documents both edits and the BLOCKING ABI-event commit shape. Verbatim before/after:

```cpp
// src/external/ours/library/unicode/src/shared/Unicode.h:27
//
// Before:
    typedef unsigned short unicode_char_t;
//
// After:
    // Phase 9 Wave N: flipped from `unsigned short` to `wchar_t`. Now matches
    // MSVC's built-in wchar_t (2 bytes on Win32) — restores default behavior
    // after Phase 7 CLEAN-04 removed the XPCOM PRUnichar ABI boundary.
    typedef wchar_t unicode_char_t;
```

```cpp
// src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h:125
//
// Same edit, with comment cross-referencing Unicode.h to make the duplication explicit.
```

**Why two sites:** `Unicode.h` is the public typedef seen by `external/ours/library/unicode/` clients; `StlForwardDeclaration.h` re-defines it inside `namespace Unicode` for files that pull only sharedFoundation headers. If only one flips, `Unicode::String` is two distinct types in different TUs → ODR violation.

---

### Pattern H: Idiom-portability fixes (incremental Wave-5)

**Applies to:** TBD per-file as the v145 build surfaces compile errors.

**Canonical analog A:** reflog `831304d35` (Wave-1 fixes, 8 mechanical edits). Patterns it covers:

| Issue | Example file | Fix |
|---|---|---|
| `<typeinfo.h>` → `<typeinfo>` | `sharedCollision/extent/CollisionDetect.cpp` | Standard header swap |
| Mismatched container `iterFind` comparison (latent bug, masked by STLPort raw-pointer iterators) | `sharedGame/travel/TravelManager.cpp:378` | Compare iterator to the correct container's `.end()` |
| Iterator vs raw-pointer parameter mismatch | `sharedCollision/core/CollisionUtils.cpp:916` | `&sortedVerts[0]` (with empty guard) instead of iterator |
| `push_back()` (no args) requires `emplace_back()` under MSVC | `sharedNetwork/NetworkHandler.cpp:258` | Switch to `emplace_back()` |
| Implicit pair-element conversion | `sharedGame/core/AiDebugString.cpp:547` | Explicit `CachedNetworkId(target)` ctor |

**Canonical analog B (richer catalog):** reflog `edad2dabe` (Wave-2 fixes, ~30 edits across client tier). Adds patterns:

| Issue | Files | Fix |
|---|---|---|
| `find()` argument-order reversed + `!find(...)` instead of `== end()` | `clientTerrain/WeatherManager.cpp:42` | Correct arg order; compare to `.end()` |
| UIStlFwd.h needs same treatment as StlForwardDeclaration.h | `external/3rd/library/ui/src/win32/UIStlFwd.h` | Drop `stl/_config.h` + forward-decls; add StlCompat.h |
| `vec[idx]` for iterator parameter | `ui/UIPie.cpp:516,688` | `&vec[0]` with empty guard |
| `basic_string` ctor from iterator | `ui/UIText.cpp:382,456` | `&*it` for the const-pointer overload |
| Iterator-as-bool no longer works | `clientSkeletalAnimation/EditableAnimationState.cpp:486` | `if (it != container.end())` |
| Direct `_STL::string` reference | `clientGame/ClientEffectManager.cpp:1227,1290` | Use `std::string` (alias gone) |
| Explicit `make_pair<K,V>(...)` template args | `clientGame/CreatureObject.cpp:6637`, `ZoneMapObject.cpp:98-108`, `swgClientUserInterface/SwgCuiGalacticCivilWar.cpp` (6×), `SwgCuiHyperspaceMap.cpp` (8×), `SwgCuiQuestBuilder.cpp` (12+) | Drop template args; let C++17 deduce |

**Mechanical rule:** every Wave-5 fix is per-file, single-issue, marked with a `// Phase 9 Wave N: <issue>` comment on the changed line. R-04 cadence: each fix is its own atomic commit (NOT bundled into a single fix-up commit like Wave-1's `831304d35`) — bisect-friendly.

---

### Pattern I: Bulk cleanup commit (post-Wave-5)

**Applies to:** every file containing `std::hash_map<` / `std::hash_set<` / `std::hash_multimap<` after Waves 2-5 settle.

**No prior repo analog** (this is the deliberate end-of-migration commit). Closest template: archived `09-04-PLAN.md` Task 4. Mechanical rule:

```text
1. Bulk find-and-replace across src/ (excluding stlport453/, which is gone):
     std::hash_map<      →  std::unordered_map<
     std::hash_set<      →  std::unordered_set<
     std::hash_multimap< →  std::unordered_multimap<

2. For every file that previously included "sharedFoundation/StlCompat.h":
     - Remove the StlCompat.h include
     - Add `#include <unordered_map>` and/or `#include <unordered_set>` as needed

3. git rm src/engine/shared/library/sharedFoundation/include/public/sharedFoundation/StlCompat.h

4. cmake --build build-v145 --config Debug --target SwgClient
5. Boot gate: zone into Tatooine on the SWGSource VM.
```

**Boundary on scope:** cleanup touches CLIENT-TIER call-sites only. Server-tier `stdhash_map<...>::fwd` typedefs are out of scope (CLAUDE.md "server is read-only contract").

---

### Pattern J: Server-tier deferral comment block

**Applies to:** `src/game/server/application/CMakeLists.txt` — comment out `add_subdirectory(SwgBattlefieldTool)` and `add_subdirectory(SwgSchematicXmlParser)` BEFORE Wave-3's `find_package(STLPort)` removal (or those tools' CMakeLists.txt fail at configure when `${STLPORT_LIBRARY}` is undefined).

**Canonical analog:** Phase-8 deferral comment block in `src/engine/shared/application/CMakeLists.txt` lines 11-21, AND `src/engine/client/application/CMakeLists.txt` lines 31-42. The "DEFERRED to Phase 12.x" comment shape is established Phase-8 idiom — preserve verbatim style.

**Sample comment block** (from archived `09-02-PLAN.md` Task 1, mirrors Phase-8 style):

```cmake
# DEFERRED to Phase 12.x — server-tier STLPort sweep (out of Phase-9 scope):
# These two console tools' CMakeLists.txt reference ${STLPORT_LIBRARY},
# $<TARGET_OBJECTS:stlport_vc143_compat>, and LINK_FLAGS_DEBUG "/FORCE:MULTIPLE".
# Per CLAUDE.md ("server is read-only contract") and CONTEXT D-01, Phase 9
# only sweeps client-tier STLPort references.
# add_subdirectory(SwgBattlefieldTool)        # console tool — blocked on server-tier STLPort sweep
# add_subdirectory(SwgSchematicXmlParser)     # console tool — blocked on server-tier STLPort sweep
```

---

### Pattern K: MFC canary tier-parent uncomment (Phase-8 reverse)

**Applies to:**
- `src/engine/shared/application/CMakeLists.txt` — uncomment `# add_subdirectory(Turf)` (~line 21 per archived 09-02-PLAN; verify at plan-author time)
- `src/engine/client/application/CMakeLists.txt` — uncomment `# add_subdirectory(Viewer)` (~line 41; verify)

**No reflog analog** (Phase-8 introduced the comment; this is the reverse). Phase-8 SUMMARY 01 + 02 document the comment placement. **One-line uncomment per file** — the simplest possible CMake mutation.

**Coupled file:** `src/engine/client/application/Viewer/CMakeLists.txt` does NOT exist on main today (per archived 09-02-PLAN.md Task 6). The planner authors a fresh CMakeLists for it using **`src/engine/shared/application/Turf/CMakeLists.txt` at HEAD as the explicit-source-list MFC pattern template** (reproduced fully above under Pattern E). The Viewer source list is 26 .cpp + 26 .h enumerated by name — no `file(GLOB)`.

---

## Shared Patterns

### Per-group atomic commit shape (R-04, applies to ALL Wave-2/3/4 commits)

**Source:** Wave-1's already-landed shape (reflog `df4bd21d3`, `9acbc7f05`, `8086b2b83`, `5d3ee4c01`, `ddaaf753f`, `1fd6ace00`, `7fdbd2a46`, `a915b3f76`, `2a1c1c854`, `e8b59f704`).

**Apply to:** every commit in Waves 2-4. Mechanical rule:

- One library / source-tree per commit (group 2.X numbering)
- Subject prefix: `feat(09-XX): <library> — <change> (group N.M)`
- Body lists each file touched + the include flip applied
- Tree may be non-buildable intra-wave; only the wave-end boot gate is binding
- Each commit ends with `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>` per CLAUDE.md gsd-commit hooks

### Boot gate (D-04, applies at end of every wave)

**Source:** Wave-0 capture procedure in 09-RESEARCH.md §"Pre-Migration Baselines" + `.continue-here.md` empirical Tatooine zone-in test.

**Apply to:** every wave's gate. Mechanical rule (UPDATED for v145 + 2016 DLLs per D-08/D-10/D-11):

```powershell
# Reconfigure with v145 generator (use VS 2026's bundled CMake, not VS 2022's):
& "D:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" `
    -S src -B build-v145 `
    -G "Visual Studio 18 2026" -A Win32 -T v145,host=x64 `
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 `
    -DWHITENGOLD_BUILD_VALIDATION_HELPERS=OFF

# Build:
& "D:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" `
    --build D:/Code/swg-client/build-v145 --config Debug --target SwgClient

# Verify the 2016 SWGSource DLL set is staged in build-v145/bin/Debug/
# (per D-10 baseline manifest dll-baseline-2016.txt). MD5s must match the
# captured manifest — if any DLL has drifted, halt the wave.

# Launch & boot test:
D:\Code\swg-client\build-v145\bin\Debug\SwgClient_d.exe
# Manual: log into SWGSource VM at 192.168.1.200:44453.
# PASS: zone into Tatooine cleanly (necessary; character-select alone is NOT sufficient per D-11)
# FAIL: any crash, any new fatal in client_console.log, or failure to reach Tatooine zone
```

**STLPort symbol gate (Final, post-cleanup):** `dumpbin /imports build-v145/bin/Debug/SwgClient_d.exe` MUST show zero `stlport_*` symbols. Compare to baseline `before-imports-replan2.txt` captured in Wave-1 of the new plan (D-10).

### Verification helper rebuild (post-Wave-5 final gate)

**Source:** Wave-0 helpers committed in `35b872357` (current HEAD). They live OUTSIDE `src/` at `.planning/phases/09-stlport-msvc-stl/baseline/` and are gated behind `-DWHITENGOLD_BUILD_VALIDATION_HELPERS=ON`.

**Apply to:** Final D-04 verification (after the cleanup commit). Run all four helpers (CRC determinism, DataTable round-trip, sort stability, std::hash dump) against the same fixed inputs as before-* baselines. Acceptance criteria per CONTEXT D-04 and 09-RESEARCH.md §"Validation Architecture".

**IMPORTANT:** The helpers were configured against `build/`. The planner must reconfigure them under `build-v145/` (same `-DWHITENGOLD_BUILD_VALIDATION_HELPERS=ON` toggle, but pointed at the v145 build dir).

### `build/` → `build-v145/` translation (binding, applies everywhere)

**Source:** D-08 + CLAUDE.md "Build toolchain paths" section.

**Apply to:** every command in 09-RESEARCH.md, archived `09-01..04-PLAN.md` files, and Wave-0 helper invocations. Mechanical rule:

| Old (stale) | New (binding for Phase 09) |
|---|---|
| `cmake -S src -B build` | `cmake -S src -B build-v145 -G "Visual Studio 18 2026" -A Win32 -T v145,host=x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5` |
| `cmake --build build --config Debug` | `cmake --build build-v145 --config Debug` (using VS 2026's CMake) |
| `build/bin/Debug/SwgClient_d.exe` | `build-v145/bin/Debug/SwgClient_d.exe` |
| `dumpbin /imports build/bin/Debug/SwgClient_d.exe` | `dumpbin /imports build-v145/bin/Debug/SwgClient_d.exe` |
| baseline file `before-imports.txt` (stale, from spike 001a) | new baseline `before-imports-replan2.txt` captured in Wave-1 of new plan |
| Boot test target: "character-select" | Boot test target: "zone into Tatooine" (necessary AND sufficient) |

The planner MUST flag any references to `build/` in plan-action excerpts and translate them. The archived plans were authored against `build/`; the new plans must NOT be.

---

## No Analog Found

Files with no close analog in the codebase or reflog (planner should reference RESEARCH.md / CONTEXT.md directly):

| File | Role | Reason |
|---|---|---|
| `.planning/phases/09-stlport-msvc-stl/baseline/dll-baseline-2016.txt` | manifest-author | NEW file documenting the 2016 SWGSource v3.0 DLL drop (MD5 + source path + renamed copy). Closest precedent is the MD5 list in `.continue-here.md` lines 213-220 — copy that table into the new manifest, expand to include the rename mapping. |
| `src/cmake/win32/FindSTLPort.cmake` (DELETE) | CMake-module-delete | No prior delete-an-entire-cmake-module precedent. Mechanical: `git rm src/cmake/win32/FindSTLPort.cmake` in its own commit (last in Wave 3, or paired with stlport453/ subtree delete). Confirmed file size: 144 lines. |
| `src/external/3rd/library/stlport453/` (DELETE entire directory) | source-subtree-delete | No prior subtree-delete precedent. Mechanical: `git rm -r src/external/3rd/library/stlport453/` as its OWN final Wave-3 commit so any "did the link still pull a header from there?" hypothesis can be tested by `git revert HEAD`. |

---

## Metadata

**Analog search scope:**
- `git reflog` for hash range `df4bd21d3..edad2dabe` (19 commits — the rolled-back Wave-1 + Wave-2 work; D-12 diff-template authority)
- HEAD source tree at commit `35b872357` (current state per D-09)
- `.planning/phases/08-dead-code-removal-track-b/08-0{1,2,4}-SUMMARY.md` (deferred-tool comment style)
- `.planning/phases/09-stlport-msvc-stl/archive/09-0{1,2,3,4}-PLAN.md` (archived diff-templates)
- `.planning/spikes/001c-big-bang-audit/FILE-LIST-at-HEAD.md` (canonical client-tier file inventory)
- `.planning/phases/09-stlport-msvc-stl/.continue-here.md` (replan-2 source-of-truth)
- `D:/Code/swg-client/CLAUDE.md` "Build toolchain paths" (v145 environment)

**Files scanned:**
- 25 archive + research + context files (full-read or grep)
- 7 reflog commits diffed (`df4bd21d3`, `2bdfa8325`, `9acbc7f05`, `d9ca1269a`, `e8b59f704`, `831304d35`, `edad2dabe`)
- 6 HEAD CMake/source files (`src/CMakeLists.txt`, `src/engine/client/library/CMakeLists.txt`, `src/engine/shared/application/Turf/CMakeLists.txt`, `src/game/client/application/SwgClient/src/CMakeLists.txt`, `src/external/ours/library/unicode/src/shared/Unicode.h`, `src/engine/shared/library/sharedFoundation/src/shared/StlForwardDeclaration.h`)

**Pattern extraction date:** 2026-05-08
