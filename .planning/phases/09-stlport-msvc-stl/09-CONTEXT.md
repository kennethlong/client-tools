# Phase 9: STLPort → MSVC STL - Context

**Gathered:** 2026-05-08
**Replanned:** 2026-05-08 (post spike 001a/001b/001c findings)
**Replanned-2:** 2026-05-08 (post Wave-2 runtime-ABI revert + 2016 SWGSource DLL swap proven against v145 / Tatooine zone-in) — **REVOKED by replan-3 below**
**Replanned-3:** 2026-05-10 (post Option D empirical validation 2026-05-09: v145 EXE + 2016 DLLs FATAL at graphics init; Koogie tree boots to char-select; D-13..D-19 below are binding)
**Status:** Replan-3 binding. Replan-2 D-08..D-12 and the original Wave-1..Wave-5 structure under D-01 are archaeology. Phase 9 STL migration is satisfied by ADOPTING Koogie's tree as the v2 base (Option D), not by sweeping STLPort out of this repo. Remaining Phase 9 work: forward-port whitengold's SWGSource compat guards onto the merged tree, then Tatooine zone-in gate.

<domain>
## Phase Boundary

### Replan-3 (2026-05-10) — Option D framing

Under Option D, the STL migration is satisfied by **adopting Koogie's `MSVC-CPP20-Upgrade` branch as the v2 base** (merge commit `479d35df3` on `swg-client-v2/koogie-msvc-cpp20-base`). Koogie's tree already has: STLPort removed, MSVC STL adopted, v145 toolchain, C++20, plugins built from source. So STL-01 through STL-04 are mechanically satisfied by the merge itself — no per-file STLPort sweep is performed in `swg-client/src/`. STL-05 (Tatooine zone-in clean) requires forward-porting whitengold's SWGSource compat guards onto the merged tree, since Koogie's standalone build FATALs at world-load on a character-loadout IFF chunk-size mismatch that whitengold v1 guards with `exitChunk(true)`. See D-14..D-19 below.

The original phase-boundary description (preserved below for historical context) was authored against the assumption that Phase 9 sweeps STLPort out of this repo file-by-file. That assumption is retired by Option D.

### Original framing (replan-2; archaeology — DO NOT execute)

Remove **STLPort 4.5.3** from the v2 build entirely and replace with the **native MSVC STL** that ships with the Visual Studio 2022 toolchain. This includes:

- Removing the STLPort include path (`build/stlport453/stlport/`), the prebuilt vc71 static libs (`stlport_vc71_static.lib`, `stlport_vc71_stldebug_static.lib`), and the v1-era `stlport_vc143_compat.cpp` shim from every CMake target.
- Sweeping all `std::hash_map` / `std::hash_set` references in **client-relevant** source (~40 files across engine/client, game/client, external/3rd/library/ui, UiBuilder, LocalizationTool) to use `std::unordered_map` / `std::unordered_set` (via `sharedFoundation/StlCompat.h` — see D-02).
- Flipping `Unicode::unicode_char_t` from `unsigned short` to `wchar_t`; removing `/Zc:wchar_t-` from compile flags; restoring `/Zc:wchar_t` (the modern default).
- Removing `/FORCE:MULTIPLE` and `/NODEFAULTLIB:stlport_vc71_static.lib` linker flags.
- Verifying SwgClient_d.exe boots to character select against the SWGSource VM at every wave gate.
- Verifying Phase 8's deferred MFC tools (Turf, Viewer, TerrainEditor, etc.) compile cleanly — strong proof STLPort is actually gone.

**Server tier is OUT OF SCOPE per CLAUDE.md** ("server is read-only contract"). The 33 server-side hash_map/hash_set sites in `engine/server/` and `game/server/` are NOT touched. This is a 60% reduction in scope versus the original plan that conflated client and server sweeps.

Out of scope: any change unrelated to STL/CRT/wchar_t (renderer, gameplay, networking, asset pipeline). Any STL-conformance bugs surfaced by the migration that aren't trivially fixed (rule-of-five, allocator interface) are documented as deviations and tracked, not silently rewritten.

</domain>

<decisions>
## Implementation Decisions

### Replan-3 (2026-05-10) — Option D Adopted: Koogie tree as v2 base

The decisions in this section are **binding** and supersede the replan-2 D-08..D-12 series, the original D-01 wave structure, and the original Wave-1..Wave-5 contents below. The replan-2 runtime contract (v145 + 2016 SWGSource v3.0 DLLs) was empirically invalidated on 2026-05-09 (see `.continue-here-replan3.md`): the v145 EXE + 2016 DLL pair FATALs during graphics setup with "format arg out of range" (callstack alternating EXE / DLL-at-`~0x6BE90000`). Memory `project_2016_dll_runtime_invalidated.md` already records this; D-08 was authored despite that prior finding, which replan-3 corrects.

A four-corner test 2026-05-09 then validated **Option D**: clone the SWG-Source/client-tools fork, `--no-ff` merge Koogie's `MSVC-CPP20-Upgrade` branch (preserves attribution per upstreaming protocol), and forward-port whitengold's SWGSource compat guards. The merged tree builds clean under VS 2026/v145 and boots to character select — matching Koogie-standalone behavior. The remaining gap is the world-load IFF FATAL that whitengold v1 guards.

- **D-13 (REVOKE):** D-08, D-09, D-10, D-11, D-12 are revoked. They were authored against the unverified premise that v145 + 2016 SWGSource v3.0 DLLs form a valid runtime contract; probes E + G (2026-05-09) showed this premise is false. Baseline artifacts under `.planning/phases/09-stlport-msvc-stl/baseline/` (`dll-baseline-2016.txt`, `before-imports-replan2.txt`, `warnings-wave-1.txt`, etc.) remain on disk as research artifacts of an invalidated approach — **DO NOT use them as boot gates or comparators going forward.** The 9 commits `898bc9a89..68967ee42` are preserved as the empirical investigation arc that arrived at Option D; do not revert. Replan-2's R-01..R-04 are also revoked — there is no Wave 1..6 to land.

- **D-14 (NEW; binding) — Option D phase scope:** Phase 9 STL migration is satisfied by ADOPTING Koogie's tree as the v2 base, NOT by sweeping STLPort out of this repo. Koogie's tree has STLPort removed, MSVC STL adopted, v145 toolchain, C++20, plugins built from source — **STL-01 through STL-04 are mechanically satisfied by the merge commit `479d35df3` itself**, no per-file work needed. **STL-05 (Tatooine zone-in clean) requires forward-porting whitengold's IFF chunk-size guard** because Koogie's standalone tree FATALs at world-load on `creation/default_pc_equipment.iff/LOEQ/0000/PTMP/ITEM: exiting chunk but not at the end of it` (the SWGSource content has extra ITEM fields beyond what the 2010 client's IFF parser was frozen against; whitengold's `exitChunk(true)` graceful-exit handles this; Koogie + SWG-Source/master do not). Whitengold's `src/` becomes read-only research history going forward.

- **D-15 (NEW; binding) — Working tree topology:**
  - **Active repo:** `D:/Code/swg-client-v2/` (fork at `github.com/kennethlong/client-tools`)
  - **Branch:** `koogie-msvc-cpp20-base`
  - **Merge anchor:** commit `479d35df3` (`merge: adopt Koogie's MSVC-CPP20-Upgrade as v2 base`) — `--no-ff` merge of `koogie/MSVC-CPP20-Upgrade` preserves attribution per memory `project_swg_source_upstreaming.md`.
  - **Remotes:** `origin` → `kennethlong/client-tools`; `koogie` → `KoogieKepler/client-tools`; both already configured per `git branch -a` check 2026-05-10.
  - **Toolchain:** VS 2026 / MSVC v14.51 / v145 / x64 host / Win32 target; Koogie's `swg.sln` is the build entry point (NOT CMake — Koogie did not port to CMake; whitengold's CMake authoring is research history under Option D).
  - **whitengold (`D:/Code/swg-client/`):** stays as research history. No `src/` edits going forward. `.planning/` directory STAYS here through Phase 9 replan-3 closeout (see D-16).

- **D-16 (NEW; binding) — `.planning/` location plan:** Phase 9 replan-3 plans 09-01 and 09-02 are authored and committed in whitengold's `.planning/` directory as today. After 09-02 SUMMARY.md is written and Tatooine PASS is recorded, the closeout step copies (not git-mv) **only** these top-level files into `swg-client-v2/.planning/`: `PROJECT.md`, `ROADMAP.md`, `REQUIREMENTS.md`, `STATE.md`. Per-phase directories (`phases/01-*` through `phases/09-*`) STAY in whitengold as v1+v2-research-history; they are NOT migrated. Phase 10 (DPVS) and onward originate cold in `swg-client-v2/.planning/phases/`. Memory `MEMORY.md` is project-keyed to the whitengold directory under `~/.claude/projects/D--Code-swg-client/`; whether to re-key memory to swg-client-v2 is a post-Phase-9 concern, not a replan-3 decision.

- **D-17 (NEW; binding) — Plan structure (two plans):**
  - **09-01-PLAN.md — Merge anchor + char-select boot gate:** Tasks:
    1. Verify `D:/Code/swg-client-v2/` builds clean from `479d35df3` under VS 2026/v145 via Koogie's `src/build/win32/swg.sln` MSBuild.
    2. Stage runtime DLLs/configs at `swg-client-v2/stage/` mirroring whitengold's working `build/bin/Debug/` runtime layout: copy non-plugin runtime DLLs (binkw32, Mss32, mozilla XPCOM stubs, Qt, Vivox stubs as needed), overlay Koogie-built `SwgClient_d.exe` + `gl05/06/07_d.dll` + `dpvs.dll` + `DllExport.dll`, rename `client.cfg` → `client_d.cfg` (Koogie's renamed config-lookup convention per `.continue-here-replan3.md` 2026-05-09).
    3. Launch against SWGSource VM at 192.168.1.200:44453 (per memory `project_vm_server_setup.md` startup sequence).
    4. Confirm reaches character-select scene cleanly. Boot gate: **character-select**, not Tatooine (Tatooine is 09-02's gate).
    5. Capture a Koogie-built EXE imports dump (`dumpbin /imports`) as the new symbol baseline replacing `before-imports-replan2.txt`.
    
    Zero source edits in 09-01. Mirrors the 2026-05-09 Koogie-standalone empirical baseline.
  - **09-02-PLAN.md — Compat-guard port + Tatooine gate + closeout:** Tasks:
    1. Port the character-loadout IFF chunk-size guard (`exitChunk(true)`) to v2. Canonical source: whitengold `src/engine/client/library/clientUserInterface/src/shared/core/CuiCharacterLoadoutManager.cpp:162` with the comment "tolerate extra fields added by SWGSource after this client was frozen". Original whitengold commit: `dd78832c4 fix(06): tolerate extra ITEM chunk fields in character loadout IFF`. Land as a single thematic commit on `swg-client-v2/koogie-msvc-cpp20-base`.
    2. Rebuild v2 + re-stage + Tatooine zone-in boot test.
    3. **Bisect-first** (D-18): if Tatooine surfaces additional FATALs, port additional guards on-demand from the whitengold candidate set — NOT preemptively. Candidate set, in expected-failure order: ContrailData cast guard (whitengold commit `89bce3a43`), NebulaManagerClient null lightning guard (whitengold commit `fcce67cf1`), POB / runtime-config fragments from `fac49a4c3` (extract only the POB-relevant fragments; the STLPort + voice-chat portions of that commit are obsolete under Option D).
    4. Tatooine zone-in clean confirmation = STL-05 satisfied.
    5. Write 09-02-SUMMARY.md + update STATE.md (close Phase 9; mark STL-01..04 "satisfied via Option D merge `479d35df3`"; mark STL-05 satisfied via the ported guard set).
    6. Copy top-level `.planning/` files (`PROJECT.md`, `ROADMAP.md`, `REQUIREMENTS.md`, `STATE.md`) into `swg-client-v2/.planning/` per D-16.

- **D-18 (NEW; binding) — Compat-guard scope (bisect-first):** Mandatory port in 09-02 is the character-loadout IFF chunk-size guard ONLY. ContrailData / NebulaManagerClient / POB guards are NOT pre-ported; they are added only if the Tatooine boot test surfaces them as FATALs. Rationale: (1) Koogie's tree may have already absorbed equivalent fixes from SWG-Source/master via independent contributions, in which case pre-porting causes duplicate-fix conflicts; (2) bisect-first keeps the upstream PR set scoped to what's actually needed for the empirical SWGSource VM configuration the user runs; (3) the IFF guard is the single known-failing case from the 2026-05-09 Koogie-standalone test — anything else is speculative until reproduced.

- **D-19 (NEW; binding) — Upstream PR cadence:** Each compat guard is a thematic commit on `swg-client-v2/koogie-msvc-cpp20-base`. PRs against `SWG-Source/master` open ONLY AFTER Tatooine zone-in PASS in 09-02 confirms the full guard set works together end-to-end. Per-guard PRs land separately so each PR references its empirical Tatooine evidence inline. **Pre-Tatooine-PASS upstream PRs are explicitly NOT in Phase 9 scope.** The 30-day outreach protocol per memory `project_swg_source_upstreaming.md` (initial PR + maintainer notification + 30-day window before considering alternate distribution) applies after PRs are open — that is post-Phase-9 followthrough.

---

### Replan-2 (2026-05-08, post-Tatooine zone-in) — Architectural Update — **REVOKED by replan-3 above**

The decisions below were authored against the unverified premise that v145 + 2016 SWGSource v3.0 DLLs form a valid runtime contract. That premise was empirically invalidated 2026-05-09 — see D-13. Kept in-file for archaeological value; **do not execute D-08..D-12 or the original D-01 wave structure as planning targets**.

The decisions in this section are **binding**. They supersede the wave structure in D-01 below and the commit-fate decision in R-01. The post-spike-001c plan was authored against an unstated assumption — that the EXE/DLL ABI surface was already MSVC-STL-shaped — that the Wave-2 execution disproved. Strategy 3 from `.continue-here.md` ("phase reordering / community already solved it") was empirically validated by an out-of-band test the user ran:

- Toolchain: **v145** (VS 2026 / MSVC v14.5, host=x64, target Win32) using `D:/Program Files/Microsoft Visual Studio/18/Community/.../CMake/bin/cmake.exe`
- Build dir: `build-v145/` (per CLAUDE.md "Build toolchain paths" — the v143 `build/` dir is the v1 fallback, do not target it for Phase 9)
- DLL set: `gl05_d.dll`, `gl06_d.dll`, `gl07_d.dll`, `dpvs.dll`, `dpvsd.dll`, `DllExport.dll` swapped from `D:/Code/SWGSource Client v3.0/` (PE timestamp 2016-06-17, statically-linked CRT, MSVC-STL-shaped)
- Result: **SwgClient_d.exe boots clean and zones into Tatooine** against the SWGSource VM at 192.168.1.200:44453

This empirically forecloses the runtime-ABI debate: the 2016 community-rebuilt plugin DLLs are MSVC-STL-shaped and form a viable runtime contract. Phase 09 no longer needs to rebuild Direct3d9 or DPVS from source (Strategy 1 deferred to Phase 11), and no longer needs an ABI-surface-preserving fence (Strategy 2 retired). The STL migration becomes a build-system + source-sweep exercise against an already-MSVC-STL-shaped DLL boundary.

- **D-08 (NEW; binding):** Runtime baseline for Phase 09 is **v145 toolchain + 2016 SWGSource v3.0 community-rebuilt proprietary plugin DLLs** staged in `build-v145/bin/Debug/`. Every wave's boot gate runs against this baseline. The 2010 SOE leak DLLs are research artifacts; do NOT swap them back during Phase 09 except for one-shot regression isolation. The DLL set:
  - `gl05_d.dll`, `gl05_o.dll`, `gl05_r.dll` (renamed copies of `gl05_r.dll` from SWGSource v3.0)
  - `gl06_d.dll`, `gl06_o.dll`, `gl06_r.dll` (same pattern)
  - `gl07_d.dll`, `gl07_o.dll`, `gl07_r.dll` (same pattern)
  - `dpvs.dll`, `dpvsd.dll` (both names point at the SWGSource `dpvs.dll`)
  - `DllExport.dll` (SWGSource `DllExport.dll`; absent from leak baseline)
  - 2010 originals preserved as `*.original-2010.bak` if regression isolation is later needed.

- **D-09 (NEW; binding):** Source state at the start of replan-2 is commit **`35b872357`** (`test(09-00.3): record exit_leak_count=0`). The 19 STL-migration commits from the prior attempt (`df4bd21d3..edad2dabe`, Wave 1 + Wave 2) were rolled back by `f9512e663` because they were authored against the pre-swap (2010 STLPort-shaped) DLL baseline. Those commits remain in `git reflog` as research artifacts and a correct rendering of migration intent — the planner SHOULD reference them by hash when the equivalent edits land in the new wave structure (see D-10), but MUST NOT depend on them being present at HEAD. **R-01 ("keep all 13 landed commits at HEAD") is REVOKED — those commits are no longer at HEAD.**

- **D-10 (NEW; binding):** Wave-1 of the replan-2 plan is a **runtime-baseline-anchor wave**. It is the first wave the planner authors and contains no source edits — only:
  1. Verify the v145 build dir exists at `build-v145/` and produces a clean SwgClient_d.exe from current `35b872357` source. (Pre-Phase-09-baseline EXE.)
  2. Verify the 2016 DLL set is staged in `build-v145/bin/Debug/` per D-08 (or stage it from `D:/Code/SWGSource Client v3.0/`).
  3. Run a **clean-baseline boot test** against the SWGSource VM and confirm zone-into-Tatooine still works with the unmodified `35b872357` EXE + the 2016 DLLs. This anchors the contract every subsequent wave's boot gate must continue to satisfy.
  4. Capture `dumpbin /imports build-v145/bin/Debug/SwgClient_d.exe` to a `baseline/before-imports-replan2.txt` so the post-Wave-N gate has an STLPort-symbol delta to assert against (the `before-imports.txt` capture from spike 001a is stale — it was against `build/` not `build-v145/`).
  5. Check in a manifest (`baseline/dll-baseline-2016.txt`) listing each 2016 DLL with its MD5, source path under `D:/Code/SWGSource Client v3.0/`, and the renamed copy in `build-v145/bin/Debug/`. This is the contract — if any of these MD5s drift mid-phase, halt.

- **D-11 (NEW; binding):** STL-migration waves (formerly D-01 Waves 1–5) re-execute under the v145 + 2016 DLL baseline. The wave content is **conceptually similar** to the post-spike-001c plan (StlCompat.h authoring, tier-parent include block removal, per-library hash_map sweep, wchar_t flip, idiom fixes, cleanup) — but:
  - Each wave's boot gate runs against `build-v145/` not `build/`.
  - Each wave's boot gate must zone into Tatooine, not just hit the character-select scene. (Tatooine is the empirically-validated baseline; character-select is necessary-but-not-sufficient.)
  - The planner MAY restructure wave count (e.g., consolidate Wave 1+2 of the prior plan into a single sweep wave now that the runtime ABI block is gone), but each wave MUST end with a boot-to-Tatooine gate.
  - Per-group atomic commits within a wave is preserved (R-04 still holds). Tree may not compile intra-wave.

- **D-12 (NEW; binding):** The reverted 19-commit graph from `df4bd21d3..edad2dabe` is a **diff-template, not a rebase target**. Concretely: when Wave 2 of the new plan sweeps `<hash_map>` to `StlCompat.h` in (e.g.) `sharedDebug`, the planner SHOULD reference `git show 9acbc7f05` for the exact mechanical pattern, but MUST author a fresh commit (the rolled-back hash is now in reflog only). Cherry-picking the rolled-back commit is acceptable iff the planner verifies it still applies cleanly to `35b872357` and the resulting binary still boots to Tatooine under the new DLL baseline. If cherry-pick is the chosen mechanism, document it explicitly in the task `<action>`.

### Architecture — Big-Bang Removal, NOT Per-Tier or Coexistence

- **D-01 (REPLACES original D-01):** Phase 9 uses **big-bang full STLPort removal** in 5 sequenced waves. The original 3-wave hybrid (engine/shared then client+game then wchar_t) is **architecturally invalid** and was retired by spike findings.

  **Why it changed:** Spikes 001a and 001b proved that no incremental migration is viable. STLPort's `stl/_epilog.h` does `#define std STLPORT` per translation unit. That rewrite is text-level and TU-global — it is undefeatable at the per-target or per-file level (001a INVALIDATED), and it produces silent miscompiles when bridged via coexistence guards (001b INVALIDATED, classified `dangerous: silent miscompile`). Spike 001c VALIDATED that big-bang scope is bounded at ~40 client-relevant files and one-session-doable.

  **Wave structure:**

  | Wave | Scope | Status | Boot Gate |
  |---|---|---|---|
  | **1** | Author `sharedFoundation/StlCompat.h`, remove root + tier-parent STLPort include blocks, sweep engine/shared `<hash_map>`/`<hash_set>` includes (11 group commits + 8 portability fixes) | **DONE — 13 commits already on main** (`df4bd21d3..831304d35`) | (was non-bootable due to client tier still poisoning TUs — re-tested at end of Wave 2) |
  | **2** | Remove engine/client tier-parent STLPort include block; sweep all client-relevant `<hash_map>`/`<hash_set>` includes (~19 source files: engine/client 5, game/client 4, external/3rd/library/ui 9, external/3rd/application/UiBuilder 2, external/ours/application/LocalizationTool 1) | TODO | Boot to character select |
  | **3** | Strip per-target STLPort link refs (~18 CMake files): `${STLPORT_LIBRARY}` removal, `/FORCE:MULTIPLE` removal, `/NODEFAULTLIB:stlport_vc71_static.lib` removal, `find_package(STLPort)` removal, delete `src/cmake/win32/FindSTLPort.cmake`, delete `src/external/3rd/library/stlport453/` subtree. **MFC canaries (D-05) wire here.** | TODO | Boot to character select + MFC canaries (Turf, Viewer) compile clean |
  | **4** | Unicode flip (D-03 unchanged from original): flip `Unicode::unicode_char_t` from `unsigned short` to `wchar_t`; drop `/Zc:wchar_t-`; restore `/Zc:wchar_t`; sweep cast sites; strip `_USE_32BIT_TIME_T=1` if no longer needed (separate task) | TODO | Boot to character select + final D-04 verification gates |
  | **5** | Idiom-portability fixes as they surface during full-graph build (estimated 5–15 small mechanical edits). Pattern is already established in commit `831304d35` (8 fixes from Wave 1). Each fix lands as a single atomic commit. | TODO (incremental) | Final boot test once all surfaced issues are resolved + StlCompat.h cleanup commit (see D-02) |

### hash_map / hash_set Sweep — StlCompat.h Transitional Alias

- **D-02 (UNCHANGED from original CONTEXT, with spike-derived clarification):** `src/engine/shared/library/sharedFoundation/include/public/sharedFoundation/StlCompat.h` (already authored in commit `df4bd21d3`) aliases STLPort's `std::hash_map` / `std::hash_set` to MSVC's `std::unordered_map` / `std::unordered_set`:
  ```cpp
  // sharedFoundation/StlCompat.h (already on main)
  #pragma once
  #include <unordered_map>
  #include <unordered_set>
  namespace std {
      template <class K, class V, class H = std::hash<K>, class E = std::equal_to<K>, class A = std::allocator<std::pair<const K, V>>>
      using hash_map = std::unordered_map<K, V, H, E, A>;
      template <class K, class H = std::hash<K>, class E = std::equal_to<K>, class A = std::allocator<K>>
      using hash_set = std::unordered_set<K, H, E, A>;
      // Augmented in 09-01 with hash_multimap, <string>, etc. — keep current additions.
  }
  ```
- The Wave 2 sweep changes `#include <hash_map>` / `#include <hash_set>` to `#include "sharedFoundation/StlCompat.h"` and leaves call-sites alone.
- **After Wave 5 boots clean**, a separate **cleanup commit** migrates each call-site from `std::hash_map<...>` → `std::unordered_map<...>` (and `std::hash_set` → `std::unordered_set`) and **deletes StlCompat.h**. This decouples the migration from the modernization, keeps each commit small, and gives a clean bisect window between "did the alias bridge break it?" and "did the call-site migration break it?"

- **D-02-clarification (from spike 001b):** StlCompat.h MUST NOT be made coexistence-aware. Any `#ifdef _STLP_VERSION` branch in StlCompat.h that tries to bridge between STLPort and MSVC parsers in the same TU produces silent miscompiles because `#define std _STL` is a text-level identifier rewrite, not a namespace move. The header is safe ONLY when STLPort headers are guaranteed absent from every TU (i.e., post-Wave-3 link-cleanup, but practically already enforced by Wave 1's tier-parent include-block removals). DO NOT re-introduce coexistence guards.

### wchar_t Flip Timing — Wave 4 (Separate from STLPort Removal)

- **D-03 (UNCHANGED, renumbered Wave 3 → Wave 4):** STL-03 (`unicode_char_t` migration + `/Zc:wchar_t-` removal) happens in Wave 4, **after Waves 1–3 boot clean**. Two distinct test cycles:
  - Waves 1–3 isolate the STL ABI shift (size_t hashing, allocator interface, string layout).
  - Wave 4 isolates the wchar_t bit-width / mangling shift (function name decoration, `wchar_t*` vs `unsigned short*` parameter types).
- If a regression surfaces, the bisect points at exactly one of the two changes — not a tangled mess of both.
- This is feasible because Phase 7 CLEAN-04 already removed XPCOM, which was the original constraint forcing `/Zc:wchar_t-`.

### Verification Gates — All Four Categories Across the New Wave Boundaries

- **D-04 (UNCHANGED scope, REMAPPED to new wave structure):** All four verification categories enabled. Per-wave + final gates baked into the plan.

  **Pre-migration baselines (already captured 2026-05-08, in `.planning/phases/09-stlport-msvc-stl/baseline/`):**
  - `BEFORE-STL.txt` — full grep snapshot of every STLPort marker
  - `before-runtime.txt` — log frame times + exit-leak counts on character-select + Mos Eisley plaza
  - `before-crc.txt` — `Crc::calculate` over ~100 fixed-input strings
  - `before-datatable-round-trip.txt` — RESOLVED: `datatables/player/radial_menu.iff` (322 rows × 4 cols), captured per 09-00-SUMMARY.md (commit `3bbbc4e34`). Same path locked for `after-datatable-round-trip.txt` in plan 09-04.

  **Per-wave gates (boot test plus category-specific gate):**
  - **Boot to character select against SWGSource VM** at end of every wave (Wave 2, 3, 4, 5). Non-negotiable.
  - **Code/symbol gate:** Re-run BEFORE-STL.txt grep set. After Wave 3, every line should be gone or annotated as intentional. After Wave 4, also confirm `/Zc:wchar_t-` and `/FORCE:MULTIPLE` are absent from any CMakeLists.txt.
  - **Canary gate (Wave 3 only — re-targeted from original Wave 2):** MFC tools (D-05) compile clean. Re-enable their `add_subdirectory()` lines in `engine/shared/application/CMakeLists.txt` and `engine/client/application/CMakeLists.txt` (currently commented out as Phase-9-deferred).

  **Final (one-time, after Wave 5 + cleanup commit):**
  - `dumpbin /imports build/bin/Debug/SwgClient_d.exe` — must show zero `stlport_*` mangled symbols.
  - Hash determinism re-run — match `before-crc.txt` exactly OR every divergence documented as intentional.
  - Round-trip persist re-run — pre and post outputs must deep-compare equal on the chosen data class.
  - Sort stability check — pin a known input array; pre/post `std::sort` outputs must match.
  - Frame-time + leak-count diff — within ±10% on frame time on character-select; exact match on exit-leak count modulo expected singletons.

  All four gate categories are mandatory. The user explicitly requested diligence after weighing the "invisible bugs" risk.

### MFC Canary Selection — Re-targeted to Wave 3

- **D-05 (UNCHANGED tools, RE-TARGETED Wave 2 → Wave 3):** Wire **two** MFC tools deferred from Plan 08-02 / Plan 08-01 as canaries at end of Wave 3 (when STLPort link refs are gone, not when source sweep is done):
  - **Turf** (small, ~5 cpps, MFC console — fast signal that `<afxwin.h>` + MSVC `<type_traits>` no longer collides)
  - **Viewer** (larger, 26 win32 cpps, MFC GUI — exercises a wider engine slice; if it builds, the rest of the deferred MFC tools are very likely to follow)
- These tools' CMakeLists.txt are already authored. Re-enabling them in Wave 3 is a one-line uncomment per tier parent.
- After Wave 3 passes the canary gate, the planner may schedule a follow-up plan to re-enable the remaining 13 MFC tools — no longer Phase-9 critical-path.
- **Why Wave 3 not Wave 2:** Wave 2 only removes the include block; STLPort static libs still link-present. The MFC `<afxwin.h>` collision is fundamentally about `<type_traits>` clashing with STLPort *library* symbols, not just headers. The canary needs link-cleanup-done to be a meaningful signal.

### Persist Round-Trip Target (RESOLVED)

- **D-06 (RESOLVED — 2026-05-08, per 09-00-SUMMARY.md commit `3bbbc4e34`):** Round-trip persist test target locked to `datatables/player/radial_menu.iff` (322 rows × 4 cols, mixed Int/Float/String column types). Source: `D:/Code/SWGSource Client v3.0/datatables/player/radial_menu.iff` (user's retail install).
  - Already serializes via DataTable's existing IFF accessors (loaded with `SetupSharedFile::install(false)` + `DataTableManager::install()` per `DataTableTool/src/shared/DataTableTool.cpp:117-120` pattern) — no new wiring needed.
  - Internally exercises `std::string` (DT_String columns) and `std::vector` (per-row container), so the COW→SSO ABI shift is exercised.
  - Output is 1288 lines (322 rows × 4 cols), small enough for deep-equality `diff` against the post-migration capture.
- Original recommendation (`NetworkIdManager` table snapshot OR `CrcLowerString`-keyed std::map dump) was superseded — the plan's originally-recommended `misc/quest_task_data.iff` ships only inside .tre archives in this user's retail install, so radial_menu.iff was substituted. Lock-in target is unchanged from Wave 0; same path MUST be used in plan 09-04 Task 5 Gate F-3 for after-baseline.

### Boost Subset Compatibility

- **D-07 (UNCHANGED):** Boost subset (`boost::shared_ptr`, `boost::scoped_ptr`, `static_assert.hpp`, `config.hpp`) auto-detects STLPort vs MSVC and changes its internal namespace selection. After STLPort removal, boost will route through MSVC paths. Researcher to spot-check that the four header references still compile and behave equivalently — no concern expected, but worth documenting.

### Replan-Specific Decisions (R-series)

- **R-01 (Commit fate):** **Keep all 13 landed commits** at HEAD (`df4bd21d3..831304d35`). They constitute genuine Wave-1 progress per spike 001c §Implications. Replan starts at Wave 2. Do not revert; do not squash. Per-group bisect granularity (group 2.1 sharedFoundation, 2.2 sharedDebug, …, 2.11 localization, plus portability fixes) is preserved.

- **R-02 (Wave count):** **5 distinct waves**, with idiom-portability fixes carved out as their own Wave 5 rather than folded inline. Each idiom fix lands as a single atomic commit, matching the 09-01 `831304d35` pattern. Bisect-friendly: a regression points at "the sweep wave" or "the fix wave," never both.

- **R-03 (StlCompat.h lifecycle):** D-02 unchanged — StlCompat.h is the transitional alias. The cleanup commit fires AFTER Wave 5 boots green and migrates call-sites + deletes the header.

- **R-04 (Cadence):** **Per-group atomic commits, boot gate at wave end only.** Tree may not compile intra-wave; this matches the existing 13 landed commits' shape. Five boot tests total (one per wave end). Per-commit boot tests are explicitly NOT required — `compile-green-every-commit` is impossible without intermediate alias hacks because the std-namespace hijack is all-or-nothing within a TU's #include graph.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Replan-3 Source-of-Truth (READ FIRST — replan-3 supersedes replan-2)
- `.planning/phases/09-stlport-msvc-stl/.continue-here-replan3.md` — **Latest checkpoint (replan-3, post Option D adoption).** Documents probes E + G that invalidated D-08, the four architectural options (A v143+2010 fallback / B incremental cherry-pick / C pull Phase 11 forward / D Koogie-base + whitengold-compat-port), the 2026-05-09 Koogie empirical sweep (builds clean, boots to char-select, FATALs at world-load IFF), and the verdict adopting Option D. Section "Verdict — Option D adopted" lists the 8-step replan-3 strategy that 09-01 and 09-02 implement.
- `D:/Code/swg-client-v2/` — **Active v2 repo.** Branch `koogie-msvc-cpp20-base` at merge anchor `479d35df3`. Phase 9 replan-3 code work happens here, not in whitengold. Remotes: `origin` → `kennethlong/client-tools`; `koogie` → `KoogieKepler/client-tools`.
- **Memory entries** (auto-memory under `~/.claude/projects/D--Code-swg-client/`; persistent across conversations):
  - `project_koogie_empirical_validation_2026_05_09.md` — Option D empirical evidence (the truth-test that adopted it)
  - `project_2016_dll_runtime_invalidated.md` — D-08 invalidation rationale
  - `project_v2_fork_location.md` — v2 working-tree topology
  - `project_koogie_branches.md` — Koogie's `MSVC-CPP20-Upgrade` and `x64bit-Upgrade` branches; what's in each
  - `project_swgsource_iff_mismatch.md` — IFF guard rationale (the original gap discovery in v1)
  - `project_swg_source_upstreaming.md` — PR-to-master model, attribution mechanics, 30-day outreach protocol
  - `feedback_empirical_validation_first.md` — recurring lesson: prove premise before adopting strategy
  - `project_v2_scope.md` — v2 milestone scope (Phase 9 STLPort, Phase 10 DPVS, Phase 11 D3D11)
  - `project_v2_fork_location.md` — v2 fork location and merge state
  - `project_vm_server_setup.md` — VM + SWGSource server startup sequence (relevant for 09-01 boot test)

### Whitengold Compat-Guard Source Patterns (for 09-02 port-forward)
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiCharacterLoadoutManager.cpp:162` — IFF `exitChunk(true)` graceful guard. Comment: "tolerate extra fields added by SWGSource after this client was frozen". Authored in whitengold commit `dd78832c4 fix(06): tolerate extra ITEM chunk fields in character loadout IFF`. **MANDATORY port in 09-02 (D-18).**
- whitengold commit `89bce3a43 fix(07): guard ContrailData swoosh cast against SWGSource TRE type mismatch` — ContrailData cast guard at `src/engine/client/library/clientGame/src/shared/objectTemplate/ClientDataFile_ContrailData.cpp`. **Bisect-first: port only if 09-02 Tatooine surfaces this FATAL.**
- whitengold commit `fcce67cf1 fix(07): continue past null lightning appearance in NebulaManagerClient` — Nebula null-guard at `src/engine/client/library/clientGame/src/shared/space/NebulaManagerClient.cpp`. **Bisect-first.**
- whitengold commit `fac49a4c3 fix(07): in-world crash series — STLPort ABI, voice chat, runtime guards` — compound commit. **The STLPort + voice-chat portions are obsolete under Option D** (STLPort gone in Koogie tree; voice chat handled separately). Extract only POB-related and runtime-config fragments as needed. **Bisect-first.**

### Halt-and-Replan Source-of-Truth (READ FIRST — replan-2 supersedes replan-1)
- `.planning/phases/09-stlport-msvc-stl/.continue-here.md` — **Latest checkpoint (replan-2, post Tatooine zone-in).** Documents the 19-commit revert at `f9512e663`, the 2016 SWGSource v3.0 DLL swap that proved the runtime ABI break is solvable, and the empirical setup (DLL md5s, restore procedure). **READ THIS BEFORE THE PLANNER FIRES.** The "test in progress" section in this file has since been resolved: v145 + 2016 DLLs + Tatooine zone-in PASSED. D-08..D-12 above codify the locked decisions that follow from that result.
- `.planning/phases/09-stlport-msvc-stl/archive/` — **Archived stale plans** (09-01 through 09-04 from the post-spike-001c replan). They were authored against the pre-DLL-swap assumption and are no longer current. Useful as diff-template per D-12; do not execute.
- `.planning/spikes/MANIFEST.md` — Spike findings table. 001a INVALIDATED, 001b INVALIDATED, 001c VALIDATED.
- `.planning/spikes/001a-per-target-include-scope/` — Per-target CMake include scope is incompatible with SwgClient's public-header API surface (LNK2019 unresolved externals at link time).
- `.planning/spikes/001b-coexistence-guards/` — Coexistence guards in StlCompat.h silently miscompile in mixed parse orders. **`#define std _STL` is a text-level identifier rewrite, not a namespace move.**
- `.planning/spikes/001c-big-bang-audit/README.md` — Big-bang full removal scope: ~40 client-relevant files. **§Implications has the recommended 5-wave structure that this CONTEXT.md adopts.**
- `.planning/spikes/001c-big-bang-audit/INVENTORY-at-HEAD.md` — Count snapshot at current HEAD (after 13 partial 09-01 commits).
- `.planning/spikes/001c-big-bang-audit/FILE-LIST-at-HEAD.md` — Concrete file paths at HEAD, sorted by category.

### Requirements
- `.planning/REQUIREMENTS.md` §STL — Phase 9 requirement IDs STL-01 through STL-05
- `.planning/ROADMAP.md` §Phase 9 — Goal statement, success criteria, depends-on Phase 7

### Project & State
- `.planning/PROJECT.md` — Core value: every change must leave the client bootable
- `.planning/STATE.md` — Phase 7/8 decisions carried forward (see "Decisions" section)

### Phase 7 / Phase 8 Outputs (relevant to STL migration)
- `.planning/phases/08-dead-code-removal-track-b/08-01-SUMMARY.md` — 5 MFC tools deferred to Phase 9 (Turf, WordCountTool, StringFileTool, DataLintRspBuilder, TreeFileRspBuilder)
- `.planning/phases/08-dead-code-removal-track-b/08-02-SUMMARY.md` — 10 additional MFC tools deferred (TerrainEditor, Viewer, ShaderBuilder, etc.) and the explicit characterization of the STLPort ↔ `<afxwin.h>` ↔ modern MSVC `<type_traits>` conflict
- `.planning/phases/08-dead-code-removal-track-b/08-04-SUMMARY.md` — Phase 8 final tally + per-category deferral roadmap referencing Phase 9 as the unblocker

### Phase 9 Existing Artifacts (planner: read RESEARCH + 09-00, replace 09-01..04)
- `.planning/phases/09-stlport-msvc-stl/09-RESEARCH.md` — Original research. Still authoritative on the hash_map call-site inventory, CMake removal surface, wchar_t flip surface, and STLPort idiom hazards. The Validation Architecture section (line 799+) is reused as-is. **Pre-dates the v145 + 2016 DLL finding** — runtime-ABI conclusions in §State of the Art and §Architecture Patterns must be filtered through D-08.
- `.planning/phases/09-stlport-msvc-stl/09-VALIDATION.md` — Nyquist validation strategy. Reused as-is.
- `.planning/phases/09-stlport-msvc-stl/09-00-PLAN.md` — Wave 0 baseline capture. **DONE** (commit `35b872357`); has SUMMARY.md. **Preserve**, do not replace.
- `.planning/phases/09-stlport-msvc-stl/09-00-SUMMARY.md` — Plan 09-00 outcome record. **Preserve.**
- `.planning/phases/09-stlport-msvc-stl/archive/09-01-PLAN.md..09-04-PLAN.md` — **ARCHIVED** post-spike-001c plans. They were executed up through Wave 2 of the prior attempt (`df4bd21d3..edad2dabe`), then rolled back by `f9512e663` because the boot gate against the 2010 STLPort-shaped DLLs failed. Per D-12, the planner MAY reference these as diff-templates for the equivalent edits in the new wave structure. Do NOT re-execute them as-is.

### Active CMake Modules to Modify or Delete (per 001c §Iteration 4 + INVENTORY-at-HEAD.md)

**Already modified by Wave 1 (do not re-touch):**
- `src/CMakeLists.txt` — `_HAS_AUTO_PTR_ETC=1` added; STLPort include block removed; `WHITENGOLD_USE_STLPORT_HEADERS` option re-declared without global directive (commit `2bdfa8325`)
- `src/engine/shared/library/CMakeLists.txt` — STLPort tier-parent block removed (`df4bd21d3`)
- `src/external/ours/library/CMakeLists.txt` — STLPort tier-parent block removed (`df4bd21d3`)
- `src/external/3rd/library/CMakeLists.txt` — STLPort tier-parent block removed (`df4bd21d3`)
- `src/external/ours/library/localization/src/CMakeLists.txt` — adds sharedFoundation public include path (`e8b59f704`)

**Wave 2 target:**
- `src/engine/client/library/CMakeLists.txt` — still has `if(WHITENGOLD_USE_STLPORT_HEADERS) include_directories(BEFORE ${STLPORT_INCLUDE_DIR}) endif()` block. **Remove this in Wave 2 BEFORE the source sweep, or the sweep is invalid.**

**Wave 3 targets (per-target link refs + cleanup):**
- Every CMakeLists.txt that references `${STLPORT_LIBRARY}` or `$<TARGET_OBJECTS:stlport_vc143_compat>` (per audit, ~18 files including 12 Phase 8 tool CMakeLists.txt)
- `src/game/client/application/SwgClient/src/CMakeLists.txt` — `LINK_FLAGS_DEBUG "/FORCE:MULTIPLE"`, `/NODEFAULTLIB:stlport_vc71_static.lib`
- `src/cmake/win32/FindSTLPort.cmake` — DELETE entirely
- `src/external/3rd/library/stlport453/src/stlport_vc143_compat.cpp` + `src/external/3rd/library/stlport453/src/CMakeLists.txt` — DELETE
- `src/external/3rd/library/stlport453/` (entire directory) — DELETE in cleanup commit at end of Wave 3

### Source Files Affected by hash_map Sweep — Wave 2 Targets

Per spike 001c §Iteration 4 + FILE-LIST-at-HEAD.md, the ~19 client-relevant files are:

**engine/client (5 files):**
- `src/engine/client/library/clientGame/.../LightsaberCollisionManager.cpp` (`<hash_map>`)
- `src/engine/client/library/clientUserInterface/.../CuiSkillManager.cpp` (`<hash_map>`)
- `src/engine/client/library/clientUserInterface/.../CuiMediator.cpp` (`<typeinfo.h>` → `<typeinfo>`)
- `src/engine/client/library/clientObject/.../ComponentAppearanceTemplate.cpp` (`<hash_set>`)
- (1 more — researcher to enumerate from FILE-LIST-at-HEAD.md)

**game/client + swgClient (4 files):**
- `src/game/client/library/swgClientUserInterface/.../SwgCui*.{cpp,h}` (multiple — researcher to enumerate)

**external/3rd/library/ui (9 files):**
- `src/external/3rd/library/ui/src/{shared/core,win32}/UI*.{cpp,h}` (multiple `<hash_map>`/`<hash_set>`/`stl/_config.h`)

**external/3rd/application/UiBuilder (2 files):** tool-only, but on critical path because it links against ui/

**external/ours/application/LocalizationTool (1 file):** tool-only

**Definitive list:** `.planning/spikes/001c-big-bang-audit/FILE-LIST-at-HEAD.md` is the canonical source. Researcher should re-run `bash .planning/spikes/001c-big-bang-audit/list-files.sh` if HEAD has moved.

### CRT/wchar_t-affected Code (Wave 4)
- `src/engine/shared/library/sharedFoundation/include/public/sharedFoundation/Unicode.h` — `Unicode::unicode_char_t` typedef (currently `unsigned short`)
- `src/external/ours/library/unicode/` — pervasive use of `unicode_char_t`
- `src/engine/client/library/clientUserInterface/src/shared/page/CuiNotepad.cpp` (Wave 4)
- All call sites that cast `wchar_t* ↔ unsigned short*` — researcher to enumerate

### Compat Shim Already Created (Wave 1 — preserve as-is)
- `src/engine/shared/library/sharedFoundation/include/public/sharedFoundation/StlCompat.h` — authored in `df4bd21d3`, augmented with `hash_multimap` and `<string>` in `e8b59f704`. **DO NOT add coexistence guards** (D-02-clarification).

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- The Wave 1 13-commit graph IS reusable history. Each per-library group (`group 2.1` through `group 2.11`) shows the exact mechanical pattern Wave 2 should follow on its ~19 client-tier files: `#include <hash_map>` → `#include "sharedFoundation/StlCompat.h"`, no call-site changes. Diff `git show 9acbc7f05` (sharedDebug group 2.2) for the canonical example.
- The 8 portability fixes in `831304d35` show exactly what Wave-5 idiom fixes look like (push_back signature differences, missing `<iterator>`, etc.). Pattern is small + mechanical + per-file-localized.
- Phase 8 established: Inline-comment-deferral pattern in tier parent CMakeLists.txt — Wave 3 will REVERSE this for the 2 MFC canaries (re-enable their `add_subdirectory()` lines).

### Established Patterns
- Engine libraries use `target_precompile_headers` against `First<Lib>.h`. After STLPort removal, MSVC STL takes over with no PCH changes needed (MSVC STL is on the system include path; it was being shadowed by STLPort).
- `Unicode::unicode_char_t` is used as a typedef everywhere — flipping it from `unsigned short` to `wchar_t` is a one-line change in `Unicode.h`, but every interop site needs review.
- `CrcLowerString::calculate` is the engine's primary string hash — its output is persisted in TRE asset Crc fields. ANY change to its hashing implementation is fatal at runtime. The hash bisect baseline (D-04) covers this.

### Known Pitfalls (from Memory + Phase 8 Findings + Spike Findings)
- **STLPort `#define std _STL` is undefeatable per-TU.** This is the central architectural finding from 001a + 001b. Any approach that tries to keep STLPort headers active in *some* TUs while MSVC STL is active in *others* fails — either at compile time (LNK2019, 001a) or silently at runtime (type identity drift, 001b). Big-bang is structurally required.
- STLPort `basic_filebuf<wchar_t>::_M_setup_codecvt` crashes with `codecvt->encoding()` vtable mismatch between vc71 and MSVC 2022 — this disappears with this phase, but if any code relied on STLPort-specific `wchar_t` codecvt behavior, it surfaces here.
- STLPort `mbstate_t` has different ABI than MSVC's — any code that sized or copied `mbstate_t` raw is broken.
- `<afxwin.h>` (MFC) cannot coexist with STLPort 4.5.3 on the same translation unit under MSVC 14.44 (Phase 8 finding) — Phase 9 fixes this entire class of issue.
- `_STL::*` direct references (13 in audit) are mostly inside `stlport_vc143_compat.cpp` — that whole file gets deleted with the vendored tree at end of Wave 3, so they don't need individual sweeping.
- `std::hash_multimap` (4 references) — already added to StlCompat.h augmentations in Wave 1.
- No `<slist>` or `<rope>` use anywhere — the codebase doesn't depend on SGI-specific extensions beyond hash_map/hash_set/hash_multimap. Smaller idiom-fix risk than originally feared.

### Integration Points
- `engine/shared/library/sharedFoundation/include/public/sharedFoundation/Unicode.h` — `unicode_char_t` typedef site (Wave 4)
- `external/ours/library/unicode/` — Unicode library implementation (Wave 4 testing)
- `engine/shared/library/sharedFile/src/shared/Iff.cpp` — IFF persist code; relevant to round-trip test (D-06)
- `engine/shared/library/sharedNetwork*` — network message ABI; relevant if wire format embeds STL containers (researcher audit)

</code_context>

<specifics>
## Specific Ideas

- The `StlCompat.h` header is authored in `sharedFoundation/include/public/` (already on main, commit `df4bd21d3`). Every engine lib that pulls `sharedFoundation` headers transitively picks it up with no extra include directories — this is why the engine/shared sweep in Wave 1 was minimally invasive.
- For the canary build gate (D-05), wire **Turf first** because it's the smallest MFC compile (~5 cpps with `<afxwin.h>`). If Turf compiles after Wave 3's STLPort link cleanup, that alone is a strong positive signal. Then Viewer for the wider engine-slice exercise.
- For the round-trip persist target (D-06), researcher should evaluate whether `NetworkIdManager` is suitable — it has a known IFF-roundtrip test path and uses both `std::string` keys and `std::vector` value containers internally.
- For the hash determinism check, capture both `Crc::calculate` (lowercase string CRC, persistent) AND `std::hash<std::string>` (transient, used by unordered_map). `Crc::calculate` MUST match exactly. `std::hash` may legitimately differ — STLPort's hash and MSVC's hash are different — but that change must be characterized: any code that PERSISTS std::hash output is a latent bug.
- Each commit during Waves 2–4 keeps the per-group atomic shape — one library / target / typedef site per commit. Tree may not compile intra-wave. Boot test fires only at wave end.
- Wave 5's idiom fixes follow the 09-01 `831304d35` pattern: each fix is per-file, mechanical, with a `// Phase 9 Wave 5: <issue>` comment marker so they're greppable for post-mortem.
- The cleanup commit after Wave 5 has a specific shape: (a) bulk find-and-replace `std::hash_map` → `std::unordered_map` and `std::hash_set` → `std::unordered_set` across the codebase, (b) delete `sharedFoundation/StlCompat.h`, (c) re-run boot gate. This is one commit, not part of any wave.

</specifics>

<deferred>
## Deferred Ideas

- **Boost subset upgrade to a newer boost version** — D-07 keeps the existing 4-header subset and lets it auto-detect MSVC. Future modernization (post-v2) could vendor a newer boost or move to vcpkg.
- **`/WX` warnings-as-errors re-enablement** — listed as v3+ in REQUIREMENTS.md. Phase 9 may surface new warnings (deprecated allocator API, removed binders) that should be cleaned up before re-enabling `/WX`. Track separately.
- **Server-tier STLPort removal** — server tier (33 hash_map/hash_set sites in `engine/server/`, `game/server/`) is OUT OF SCOPE per CLAUDE.md ("server is read-only contract"). The community fork (StellaBellum / swg-main) handles server-side STL choice independently. If a future client-port milestone needs to re-include server-side, treat as its own phase.
- **C++20 / C++23 upgrade** — explicitly out of scope for Phase 9. Stay on `/std:c++17`. Two changes at once = ambiguous bisects (matches the "wchar_t separate wave" reasoning).
- **Re-enable remaining 13 MFC tools deferred from Phase 8** — after Wave 3 verifies the 2 canaries (Turf, Viewer) build, schedule a Phase 12.x follow-up plan to uncomment `add_subdirectory()` lines for the rest. Not part of Phase 9's critical path.
- **Re-enable Qt-batch tools (Phase 12.1)** — independent of STLPort migration; remains its own follow-up phase.
- **STLPort directory removal as a separate commit** — within Wave 3, the deletion of `src/external/3rd/library/stlport453/` happens as its own commit (last in Wave 3) so the migration history is bisectable. If any TU in Waves 1–3 still pulled an STLPort header, that commit's build break makes it obvious.

</deferred>

---

*Phase: 9 — STLPort → MSVC STL*
*Context originally gathered: 2026-05-08*
*Replanned (post spikes 001a/001b/001c): 2026-05-08*
*Replanned-2 (post v145+2016-DLL Tatooine zone-in test): 2026-05-08 — REVOKED 2026-05-09 by Probe E/G FATAL*
*Replanned-3 (Option D adopted: Koogie tree as v2 base + whitengold compat-guard port): 2026-05-10*
