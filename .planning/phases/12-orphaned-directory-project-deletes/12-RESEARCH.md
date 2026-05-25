# Phase 12: Orphaned Directory & Project Deletes - Research

**Researched:** 2026-05-24
**Domain:** MSBuild solution surgery (swg.sln + .vcxproj + .rsp response files) — dead-module removal in the active Koogie/MSVC C++20 tree
**Confidence:** HIGH (codebase recon: every claim verified against the active tree + the whitengold Phase 07 reference diffs)

## Summary

Phase 12 removes four dormant modules from the active MSBuild tree. Two are **clean orphans** (no build references at all) and two are **wired into the build closure**. The disk-footprint and swg.sln/.rsp surfaces are fully mapped below with exact file+line citations.

The headline finding: **neither "dead" module is actually dead at the compile level.** `trackIR/include/NPClient.h` is `#include`d by LIVE engine code (`clientGame/.../ClientHeadTracking.cpp`), and `lcdui_src`'s `EZ_LCD.h` is `#include`d by LIVE UI code (`SwgCuiG15Lcd.cpp` + `SwgCuiHud.cpp`). Deleting the directories without first touching those callers will break the `SwgClient` compile. The good news: the original whitengold **Phase 07** (CMake) hit exactly these two landmines and the fixes are small, well-understood, and already partially set up in the source (`#ifdef USE_LCD` wraps all LCD usage; `ClientHeadTracking.h` is NPClient-free). This research ports those exact mechanics from CMake to MSBuild.

**Primary recommendation:** Order the removals purest-orphan-first: (1) trackIR + stationapi dirs [DECRUFT-01], (2) SwgClientSetup project [DECRUFT-02], (3) lcdui [DECRUFT-03] last because it has the deepest live-code + multi-project entanglement. Boot-gate after EACH module (not batched) under both `rasterMajor=5` and `=11`, because the lcdui step requires live UI source edits and is the only one that can plausibly regress the running client.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| trackIR head-tracking (`NPClient.h`) | Engine (clientGame) | — | `ClientHeadTracking.cpp` runtime-loads `NPClient.dll` via `LoadLibrary` (no static link); only a compile-time header dependency |
| G15 LCD output (`lcdui_src` / EZ_LCD) | Game UI (swgClientUserInterface) | 3rd-party static lib (`lcdui.vcxproj`) + Logitech SDK (`lgLcd.lib`) | UI page `SwgCuiG15Lcd` drives `CEzLcd`; built as static lib, linked into SwgClient via sln ProjectDependency |
| stationapi (SOE auth SDK) | None (orphan) | — | No project, no .rsp link, no live `#include`; pure dead disk weight + one stale lib-search-path string |
| SwgClientSetup (MFC launcher) | Standalone app (own .exe) | — | Separate `Application` target; nothing in SwgClient links it; only the sln carries its project entry |

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| DECRUFT-01 | Delete `trackIR/` + `stationapi/` orphaned dirs; no project/.rsp references them; client builds+boots both renderers | trackIR: header-only, but `clientGame/.../ClientHeadTracking.cpp` `#include`s it + `clientGame` includePaths.rsp:5 references it + `clientGame.vcxproj:359` compiles it. stationapi: truly orphaned EXCEPT a stale `AdditionalLibraryDirectories` path in SwgClient.vcxproj (×3 configs) + SwgGodClient.vcxproj. Removal mechanics in §Code Examples. |
| DECRUFT-02 | Remove SwgClientSetup: drop `.vcxproj` from swg.sln + delete dir; client builds+boots | swg.sln:779-783 project block + ProjectConfigurationPlatforms lines 1797-1802. GUID `{9080903C-CB60-41C9-BDA0-3BDE29CFF1B3}`. No other project depends on it. Cleanest removal of the four. |
| DECRUFT-03 | Remove lcdui (G15, `disableG15Lcd=true`): drop `lcdui.vcxproj` from swg.sln + purge lcdui include/lib refs from .rsp; client builds+boots | swg.sln:1553-1557 + ProjectConfigurationPlatforms 2361-2366, GUID `{20D2AEE7-...}`. **7 in-tree projects carry a ProjectDependency on this GUID** (incl. SwgClient). Live source edits required in `SwgCuiG15Lcd.cpp` + `SwgCuiHud.cpp`. Full surface in §Removal Surface Map. |

DECRUFT-05 / DECRUFT-07 lineage: every step is boot-gated under both renderers (see §Validation Architecture). DECRUFT-04 (VideoCapture), -05 (Vivox), -06 (XPCOM) are OUT of scope for Phase 12 — do not touch `VideoCapture_debug.lib`, `vivoxSharedWrapper_debug.lib`, or `libMozilla`/`xpcom.lib`/`xul.lib` here even though they appear adjacent in the same `.rsp` files.

## Removal Surface Map (the core deliverable)

### Target 1 — trackIR (DECRUFT-01)

**Disk footprint:** `src/external/3rd/library/trackIR/include/NPClient.h` (header-only, 7,695 bytes — NaturalPoint Game Client API). No source, no lib, no project file.

**swg.sln references:** NONE (not a project).

**`.rsp` references:**
- `src/engine/client/library/clientGame/build/win32/includePaths.rsp:5` → `../../../../../../external/3rd/library/trackIr/include` (note lowercase `trackIr`; resolves fine — NTFS is case-insensitive, dir is `trackIR`). **Must remove.**

**vcxproj references:**
- `src/engine/client/library/clientGame/build/win32/clientGame.vcxproj:359-361` → `<ClCompile Include="..\..\src\shared\core\ClientHeadTracking.cpp">` (3-line block with an Optimized-config child). **Must remove.**
- `clientGame.vcxproj:1244` → `<ClInclude Include="..\..\src\shared\core\ClientHeadTracking.h" />`. Optional removal (harmless to leave — `.h` is NPClient-free — but Phase 07 removed both for cleanliness).

**Live source dependency (LANDMINE):**
- `src/engine/client/library/clientGame/src/shared/core/ClientHeadTracking.cpp:20` → `#include "NPClient.h"`. This is LIVE engine code. It `LoadLibrary("NPClient.dll")`s at runtime (no static link) but compile-depends on the header. **Deleting trackIR/ breaks the clientGame compile unless this .cpp is excluded from the build.**
- `ClientHeadTracking.h` is verified NPClient-free. Its callers — `CockpitCamera.cpp`, `SetupClientGame.cpp`, `SwgCuiOptControls.cpp` (the "checkTrackIr" options checkbox at SwgCuiOptControls.cpp:196) — only touch the public `.h` interface (`setEnabled/getEnabled/isSupported`), so they keep compiling after the `.cpp` is dropped. The checkbox simply reports unsupported (`isSupported()` returns false once the install path is gone). No edits needed to callers.

**Reference (whitengold Phase 07, CMake commit `fd8f9cff8`):** removed the trackIR include path AND excluded `ClientHeadTracking.cpp/.h` from `SHARED_SOURCES`. MSBuild equivalent: delete the `.rsp` include line + the `vcxproj` ClCompile/ClInclude entries.

### Target 2 — stationapi (DECRUFT-01)

**Disk footprint:** `src/external/3rd/library/stationapi/` — SOE auth SDK: `stationapi.cpp/.h`, `stationrequest.*`, `StationAPISession.*`, `PackClass.*`, `order.*`, `extend_rdp.*`, prebuilt libs `989crypt.lib`/`dbgutil.lib`/`rdp.lib`, VS6-era `.dsp/.dsw/.mak/.opt/.scc`, plus `stationapidemo/` subdir. ~520 KB. No `.vcxproj` (never ported to MSBuild).

**swg.sln references:** NONE.

**`.rsp` references:** NONE.

**vcxproj references (stale lib-search-paths, not links):**
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` → `AdditionalLibraryDirectories` lines **105, 160, 206** (Debug/Optimized/Release) each contain `..\..\..\..\..\..\..\src\external\3rd\library\stationapi`. **Should remove** (dangling search path after dir deletion; MSVC tolerates it, but CLEAN intent says purge).
- `src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj` → same stale `AdditionalLibraryDirectories` entry. SwgGodClient is an out-of-closure tool (see §Build-Order Risk); opportunistic cleanup, not boot-critical.

**Live source dependency:** NONE. No live `#include` of any stationapi header; no stationapi `.lib` in any `libraries*.rsp`. Verified — this is the purest orphan of the four.

### Target 3 — SwgClientSetup (DECRUFT-02)

**Disk footprint:** `src/game/client/application/SwgClientSetup/` — standalone MFC `Application` (the retail launcher/config tool). `build/win32/SwgClientSetup.vcxproj`, `src/win32/*` (~40 dialog/page .cpp/.h, `SwgClientSetup.rc` 43 KB, `res/` resources, `resource.h`).

**swg.sln references (GUID `{9080903C-CB60-41C9-BDA0-3BDE29CFF1B3}`):**
- Project block: **swg.sln:779-783** (Project line + `ProjectSection(ProjectDependencies)` with one dep on `{ECBDFCFF-...}` + EndProjectSection + EndProject). **Remove the whole block.**
- ProjectConfigurationPlatforms: **swg.sln:1797-1802** (6 lines — Debug/Optimized/Release ActiveCfg+Build.0). **Remove.**
- The GUID appears NOWHERE else — no other project depends on SwgClientSetup.

**`.rsp` references:** NONE.

**vcxproj cross-references:** NONE (nothing links it; it's a separate exe).

**Live source dependency:** Self-contained. Its `ClientMachine.cpp` `#include`s `NPClient.h` (lines 20, 31) and reads TrackIR version — but that's internal to SwgClientSetup and is deleted wholesale with the directory. Note ordering: if trackIR is deleted (Target 1) before SwgClientSetup, SwgClientSetup would not compile — but SwgClientSetup is being deleted too, and it is NOT in the SwgClient boot-gate build closure, so this is moot. (Belt-and-suspenders: removing SwgClientSetup's sln entry first means it's never built regardless.)

### Target 4 — lcdui (DECRUFT-03) — the entangled one

**Disk footprint — TWO related directories (important distinction):**
- `src/external/3rd/library/lcdui_src/` — the SWG **wrapper source** built by `lcdui.vcxproj`: `EZ_LCD.cpp/.h`, `EZ_LCD_Page.*`, `LCDUI/LCD*.cpp/.h` (16 .cpp), local `includePaths.rsp`/`settings.rsp`. This is the DECRUFT-03 target.
- `src/external/3rd/library/lcdui/` — the vendored **Logitech G15 SDK**: `lglcd.h` + `lib/lgLcd.lib`. The wrapper and several projects link `lgLcd.lib`. **Scope question for the planner** (see §Open Questions): DECRUFT-03 names `lcdui.vcxproj` + ".rsp lcdui refs"; whether to also delete the `lcdui/` SDK dir is ambiguous. The whitengold Phase 07 reference removed both the wrapper AND the LogitechLCD find/link.

**`lcdui.vcxproj` (GUID `{20D2AEE7-B60A-4EC9-B187-FA76062A6C39}`):** StaticLibrary producing `lcdui.lib`; compiles all `EZ_LCD`/`LCDUI/*` sources; `AdditionalIncludeDirectories` includes `external\3rd\library\lcdui` (the SDK); has a ProjectReference to `sharedMemoryManager`.

**swg.sln references:**
- Project block: **swg.sln:1553-1557** (Project line + ProjectDependency on `{DC2CD926-...}` sharedMemoryManager + Ends). **Remove the whole block.**
- ProjectConfigurationPlatforms: **swg.sln:2361-2366** (6 lines). **Remove.**
- **ProjectDependency references in 7 OTHER projects' `ProjectSection(ProjectDependencies)` blocks** — each a line `{20D2AEE7-...} = {20D2AEE7-...}`:

| swg.sln line | Owning project | In SwgClient build closure? |
|--------------|----------------|------------------------------|
| 767 | **SwgClient** | **YES — must remove** |
| 62 | AnimationEditor | No (out-of-closure tool) |
| 154 | ClientEffectEditor | No |
| 290 | LightningEditor | No |
| 492 | ParticleEditor | No |
| 883 | SwgGodClient | No |
| 986 | SwooshEditor | No |

  All 7 lines should be removed to keep a full-solution build clean and avoid dangling-GUID references. **The SwgClient one (line 767) is mandatory** for the boot gate; the other 6 are mandatory only if a full-solution build is part of validation.

**`.rsp` references (libraryPaths + includePaths):**
- `libraryPaths.rsp` containing `external/3rd/library/lcdui/lib` (the `lgLcd.lib` search path):
  - `src/game/client/application/SwgClient/build/win32/libraryPaths.rsp:8` ← **must remove** (in closure)
  - `src/engine/client/application/AnimationEditor/.../libraryPaths.rsp:8`
  - `src/engine/client/application/ClientEffectEditor/.../libraryPaths.rsp:8`
  - `src/engine/client/application/LightningEditor/.../libraryPaths.rsp:9`
  - `src/engine/client/application/ParticleEditor/.../libraryPaths.rsp:9`
  - `src/engine/client/application/SwooshEditor/.../libraryPaths.rsp:9`
  - `src/game/client/application/SwgGodClient/build/win32/libraryPaths.rsp:9`
- `libraries.rsp` linking `lgLcd.lib`:
  - `src/game/client/application/SwgClient/build/win32/libraries.rsp:10` ← **must remove** (in closure; line 10, in the SHARED `libraries.rsp`, not `libraries_d.rsp`)
  - AnimationEditor/ClientEffectEditor/LightningEditor/ParticleEditor/SwooshEditor/SwgGodClient `libraries.rsp` each carry `lgLcd.lib` (out-of-closure)
- `includePaths.rsp` referencing lcdui/lcdui_src (in the LIVE UI static lib):
  - `src/game/client/library/swgClientUserInterface/build/win32/includePaths.rsp:54` → `lcdui`
  - `:55` → `lcdui_src/src/win32`
  - `:56` → `lcdui_src/src/win32/LCDUI`
  - **All three must remove** — swgClientUserInterface IS in the SwgClient closure.
- `src/external/3rd/library/lcdui_src/build/win32/includePaths.rsp` — internal to the deleted project; goes away with the dir.

**Live source dependencies (LANDMINE — requires source edits BEFORE the unlink compiles):**
- `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiG15Lcd.cpp:26` → `#include "EZ_LCD.h"`. ALL `CEzLcd`/`s_lcd` usage is already wrapped in `#ifdef USE_LCD` (line 29 defines it; guarded blocks at lines 35, 193, 323, 411). **Fix:** remove `#include "EZ_LCD.h"` + remove `#define USE_LCD` → every LCD block compiles out; the public methods (`initializeLcd/updateLcd/remove`) remain as no-ops. (Exact Phase 07 template — commit `700a90466`.)
- `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHud.cpp:113` → `#include "EZ_LCD.h"` (inclusion only; symbol-unused). **Fix:** remove the include line. (Its calls to `SwgCuiG15Lcd::initializeLcd/remove/updateLcd` at lines 680/688/1142 stay valid — they call the now-stubbed methods.)
- Runtime call site `src/game/client/application/SwgClient/src/win32/ClientMain.cpp:361` → `SwgCuiG15Lcd::initializeLcd()` stays valid (calls the stubbed method); no edit needed.
- `disableG15Lcd` is a **runtime** guard (`ConfigClientUserInterface.cpp:248` default `false`; getter at :854) that early-returns inside `initializeLcd()` — it does NOT remove the compile/link dependency. Config-disabled ≠ unlinked. The `.cpp`/.rsp/.sln surgery is still required.

**Reference (whitengold Phase 07, CMake commits `5a43fc4fe` + `700a90466`):** removed the LogitechLCD find_package + lcdui_src subdir + all link/include refs, then in a follow-up source-cascade commit stripped `#include "EZ_LCD.h"`/`#define USE_LCD` from `SwgCuiG15Lcd.cpp` and the bare include from `SwgCuiHud.cpp`. Phase 07 did NOT delete or restructure the `SwgCuiG15Lcd` class — it left the no-op methods in place. Recommend matching that (minimal-surgery) approach.

## Removal Order & Risk

**Recommended sequence (purest-orphan-first, lowest-risk-first):**

1. **stationapi** (DECRUFT-01) — zero live deps; only stale lib-search-path strings. Safest possible. *(Can pair with trackIR in one commit but separate boot-gate is cheap insurance.)*
2. **trackIR** (DECRUFT-01) — header-only on disk, but requires excluding `ClientHeadTracking.cpp` from `clientGame.vcxproj` + removing the includePaths.rsp line. One live engine file affected; the fix is exclusion (no source edit). Boot-gate.
3. **SwgClientSetup** (DECRUFT-02) — self-contained app; remove sln block + config block + delete dir. No closure impact, but boot-gate to confirm the sln still parses/loads.
4. **lcdui** (DECRUFT-03) — **riskiest; do last.** Touches 2 live UI source files + swgClientUserInterface includePaths + SwgClient sln dependency + SwgClient libraryPaths/libraries + (optionally) 6 out-of-closure projects. Boot-gate hardest here.

**Riskiest removal:** lcdui (Target 4) — it is the only one with (a) live UI source edits, (b) a dependency inside the SwgClient build closure, and (c) multi-project fan-out. It is also the one most likely to surface a link error if any `lcdui.lib`/`lgLcd.lib` symbol is referenced from an unexpected spot. Everything else is effectively cosmetic disk/sln cleanup.

## Boot-Gate Cadence

**Recommendation: boot-gate after EACH module, not batched.** Rationale: the project invariant is "every change leaves the client bootable to character select," and three of the four removals are near-zero-risk while lcdui involves live UI edits. Per-module gating localizes any regression to a single removal. The build is fast-targeted (`/t:SwgClient`), so the cost is low.

Each gate MUST pass under **both** renderers (DECRUFT-05/07 lineage, mirrors v2.0 CLEAN-05):
- `rasterMajor=5` → D3D9 (gl05_d.dll)
- `rasterMajor=11` → D3D11 (gl11_d.dll)
- Set `rasterMajor` in **`stage/client_d.cfg`** — the DEBUG exe (`SwgClient_d.exe`) reads `client_d.cfg`, NOT `client.cfg` (known trap; memory note `feedback_debug_exe_reads_debug_cfg`). Editing the wrong file silently does nothing.

## Build Invocation (verified from `.planning/debug/*.cmd` + memory)

- Build is **target-scoped**, not full-solution: `MSBuild src\build\win32\swg.sln /t:SwgClient /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145 /v:minimal /nologo /m`
- Renderer plugins are separate targets built independently: `/t:Direct3d9`, `/t:Direct3d9_ffp`, `/t:Direct3d9_vsps` (D3D9 family → gl05/gl06/gl07) and `/t:Direct3d11` (→ gl11). For the dual-renderer gate, ensure both the D3D9 and D3D11 plugin DLLs are present/built in `stage/`, then flip `rasterMajor` to smoke each.
- MSBuild path varies by machine — prior sessions used `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\...` and `D:\Program Files\Microsoft Visual Studio\18\Community\...`. The planner/executor should resolve the actual MSBuild.exe at run time (e.g. via `vswhere`) rather than hard-coding.
- **Build-closure consequence:** because the gate builds `/t:SwgClient` (+ renderer targets), the 6 out-of-closure projects (AnimationEditor, ClientEffectEditor, LightningEditor, ParticleEditor, SwooshEditor, SwgGodClient) are NOT compiled during the boot gate. Their dangling lcdui ProjectDependency/`.rsp` lines would only break a **full-solution** build. Decide validation scope (see §Open Questions): if "full `swg.sln` builds clean" is the bar, remove all 7 dependency references + all 7 `.rsp` lcdui lines; if only the SwgClient closure must build, the SwgClient ones (sln:767, libraryPaths.rsp:8, libraries.rsp:10, swgClientUserInterface includePaths 54-56) are mandatory and the other 6 are best-effort cleanup.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Removing a project from swg.sln | A regex that nukes any line containing the GUID | Targeted deletion of (a) the `Project(...)`…`EndProject` block, (b) the project's own `ProjectConfigurationPlatforms` lines, and (c) the GUID-as-dependency lines inside OTHER projects' `ProjectSection(ProjectDependencies)` | The GUID appears in distinct syntactic roles; a blind sweep can corrupt the `GlobalSection` config map. Verify with a `Read` of each hit. |
| "Disabling" lcdui | Relying on `disableG15Lcd=true` to drop the dependency | Source edit (`#include`/`#define` removal) + sln/.rsp unlink | The config flag only no-ops at runtime; the symbols are still compiled and linked. |
| Excluding ClientHeadTracking | Deleting `ClientHeadTracking.h` | Drop only the `.cpp` from the vcxproj (and the trackIR include path) | The `.h` is NPClient-free and 3 live callers need its public interface; deleting it cascades far beyond Phase 12 scope. |

## Common Pitfalls

### Pitfall 1: "It's dead so I can just `rm -rf` the directory"
**What goes wrong:** Deleting `trackIR/` or `lcdui_src/` first, then the `SwgClient` compile fails on a missing header (`NPClient.h` / `EZ_LCD.h`).
**Why:** Both have LIVE `#include` consumers (ClientHeadTracking.cpp; SwgCuiG15Lcd.cpp + SwgCuiHud.cpp).
**Avoid:** Do the build-graph + source edits FIRST, build-verify, THEN delete the directory. (Order within each module's task list: unwire → build → delete → build again.)

### Pitfall 2: Editing the wrong config file for the renderer smoke
**What goes wrong:** Set `rasterMajor` in `client.cfg`, debug exe silently ignores it (reads `client_d.cfg`), you "verify" the wrong renderer.
**Avoid:** Edit `stage/client_d.cfg` for the debug exe smoke. (Documented trap.)

### Pitfall 3: Removing the SwgClient sln project block instead of just the dependency
**What goes wrong:** Confusing "SwgClient depends on lcdui (line 767)" with "remove SwgClient." You remove a single dependency LINE inside SwgClient's ProjectSection, not SwgClient's own block.
**Avoid:** For lcdui, the only blocks deleted are lcdui's own (sln:1553-1557, 2361-2366). Everywhere else you delete a single `{20D2AEE7-...} = {20D2AEE7-...}` line.

### Pitfall 4: Touching the uncommitted Koogie Direct3d9 working-tree changes
**What goes wrong:** Phase 12 commits sweep up Koogie's in-progress `Direct3d9.cpp`/`.vcxproj` vtable-probe sidequest (currently `M` in `git status`).
**Avoid:** Stage only Phase-12 files explicitly. Leave `src/engine/client/application/Direct3d9/...` untouched (memory: "leave untouched"). Per CLAUDE/MEMORY: don't modify Koogie's diagnostic patches.

## Code Examples (exact edits, MSBuild-retargeted from whitengold Phase 07)

### trackIR — exclude ClientHeadTracking.cpp + drop include path
```diff
# src/engine/client/library/clientGame/build/win32/clientGame.vcxproj (~line 359)
-    <ClCompile Include="..\..\src\shared\core\ClientHeadTracking.cpp">
-      <Optimization Condition="'$(Configuration)|$(Platform)'=='Optimized|Win32'">MaxSpeed</Optimization>
-    </ClCompile>
# and (~line 1244)
-    <ClInclude Include="..\..\src\shared\core\ClientHeadTracking.h" />

# src/engine/client/library/clientGame/build/win32/includePaths.rsp (line 5)
- ../../../../../../external/3rd/library/trackIr/include
```
Then delete `src/external/3rd/library/trackIR/`. (No edit to ClientHeadTracking.cpp itself — it's removed from the build, not modified. Callers use the NPClient-free `.h`.)

### lcdui — source de-wire (the two live files)
```diff
# swgClientUserInterface/src/shared/page/SwgCuiG15Lcd.cpp (~line 26)
-#include "EZ_LCD.h"
-
-//#undef this if the LCD causes problems
-#define USE_LCD
+// lcdui_src removed from build (DECRUFT-03); LCD support is compiled out.

# swgClientUserInterface/src/shared/page/SwgCuiHud.cpp (line 113)
-#include "EZ_LCD.h"
```
All `#ifdef USE_LCD` blocks in `SwgCuiG15Lcd.cpp` then compile out; methods become no-ops; call sites in SwgCuiHud/ClientMain remain valid.

### lcdui — swg.sln (delete lcdui's own blocks)
```
# swg.sln 1553-1557  (delete entire Project..EndProject block)
Project("{8BC9CEB8-...}") = "lcdui", "..\..\external\3rd\library\lcdui_src\build\win32\lcdui.vcxproj", "{20D2AEE7-B60A-4EC9-B187-FA76062A6C39}"
	ProjectSection(ProjectDependencies) = postProject
		{DC2CD926-8EA3-4ADD-AA62-A95CCA8AC7DD} = {DC2CD926-8EA3-4ADD-AA62-A95CCA8AC7DD}
	EndProjectSection
EndProject
# swg.sln 2361-2366  (delete the 6 ProjectConfigurationPlatforms lines for {20D2AEE7-...})
# swg.sln 767 (+ 62,154,290,492,883,986)  delete the single dependency line:
		{20D2AEE7-B60A-4EC9-B187-FA76062A6C39} = {20D2AEE7-B60A-4EC9-B187-FA76062A6C39}
```

### lcdui — SwgClient .rsp purge (in-closure mandatory)
```diff
# SwgClient/build/win32/libraries.rsp (line 10)
-lgLcd.lib
# SwgClient/build/win32/libraryPaths.rsp (line 8)
-../../../../../../external/3rd/library/lcdui/lib
# swgClientUserInterface/build/win32/includePaths.rsp (lines 54-56)
-../../../../../../external/3rd/library/lcdui
-../../../../../../external/3rd/library/lcdui_src/src/win32
-../../../../../../external/3rd/library/lcdui_src/src/win32/LCDUI
```
Then delete `src/external/3rd/library/lcdui_src/` (and, pending scope decision, `src/external/3rd/library/lcdui/`).

### SwgClientSetup — swg.sln + dir
```
# swg.sln 779-783 (delete Project..EndProject block)
# swg.sln 1797-1802 (delete the 6 ProjectConfigurationPlatforms lines for {9080903C-...})
# delete src/game/client/application/SwgClientSetup/
```

### stationapi — stale lib-search-path purge + dir
```diff
# SwgClient/build/win32/SwgClient.vcxproj  AdditionalLibraryDirectories (lines 105,160,206)
-  ...;..\..\..\..\..\..\..\src\external\3rd\library\stationapi;...
# (optional, out-of-closure) SwgGodClient.vcxproj  same path
# delete src/external/3rd/library/stationapi/
```

## Validation Architecture

This is a **delete-only / unwire-only phase. There are no unit tests** and none should be authored. Per `.planning/config.json` `nyquist_validation: true`, this section anchors the validation to build/boot assertions so downstream tooling has a concrete gate (not test commands).

### "Test" Framework
| Property | Value |
|----------|-------|
| Framework | None (build + manual boot smoke) — this is correct for a removal phase |
| Config file | n/a |
| Quick run command | `MSBuild src\build\win32\swg.sln /t:SwgClient /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145 /v:minimal /nologo /m` (assert exit 0) |
| Full suite command | Same `/t:SwgClient` build + `/t:Direct3d9` + `/t:Direct3d11` builds (assert exit 0) + boot smoke to character select under `rasterMajor=5` AND `=11` |

### Phase Requirements → Verification Map
| Req ID | Behavior | Verification Type | Command / Assertion |
|--------|----------|-------------------|---------------------|
| DECRUFT-01 | trackIR + stationapi gone; no build refs; client builds | build + grep | `MSBuild /t:SwgClient` exit 0; `rg -i "trackIr\|stationapi"` returns no hits in `*.rsp`/`*.vcxproj`/`swg.sln`; dirs absent |
| DECRUFT-02 | SwgClientSetup dropped from sln + dir deleted; client builds | build + grep | sln has no `SwgClientSetup`/`{9080903C-...}`; `MSBuild /t:SwgClient` exit 0 |
| DECRUFT-03 | lcdui.vcxproj dropped + lcdui .rsp refs purged; client builds | build + grep | sln has no `lcdui`/`{20D2AEE7-...}`; SwgClient `libraries.rsp`/`libraryPaths.rsp` + swgClientUserInterface `includePaths.rsp` lcdui-free; `MSBuild /t:SwgClient` exit 0 |
| DECRUFT-07 (lineage) | Boots to character select after each removal | manual boot smoke | Launch `SwgClient_d.exe` vs SWGSource VM; reach character select under `rasterMajor=5` (client_d.cfg) AND `rasterMajor=11` |

### Sampling Rate
- **Per task / per module:** `MSBuild /t:SwgClient` exit 0 + (after the final source edit in a module) a boot smoke under one renderer.
- **Per module merge:** boot smoke under BOTH renderers.
- **Phase gate:** `/t:SwgClient` + both renderer targets build exit 0; boot to character select under both `rasterMajor` values; `swg.sln` has zero references to the four removed names.

### Wave 0 Gaps
- None — no test infrastructure to create. The "Wave 0" equivalent is confirming a clean baseline build BEFORE removals (establish that `/t:SwgClient` is green on the current tree, so any failure is attributable to a removal step). Recommend the planner add a "baseline build" pre-step.

## Security Domain

`security_enforcement: true`, `security_asvs_level: 1`. This phase **removes** code; it adds no new authentication, input-handling, crypto, or network surface. ASVS impact assessment:

| ASVS Category | Applies | Note |
|---------------|---------|------|
| V2 Authentication | no | stationapi (a dormant SOE-auth SDK) is being DELETED — net reduction of dead auth code. Not wired into the live login path. |
| V5 Input Validation | no | No new inputs introduced. |
| V6 Cryptography | no | `989crypt.lib`/`rdp.lib` (stationapi) deleted, not added. They were never linked. |
| V12/V14 Files & Config | minor | Removing `.lib`/header files from the tree — reduces attack surface. Verify no live searchPath/loader resolves the deleted dirs at runtime (none found). |

**Net security posture: improved** (less dead code, including a dormant auth SDK and prebuilt crypto libs of unknown provenance). No threat patterns introduced.

## Runtime State Inventory

Phase 12 is a source/build-graph removal. Brief inventory per the rename/refactor checklist (applies loosely since this is a delete):

| Category | Items Found | Action Required |
|----------|-------------|-----------------|
| Stored data | None — no DB/datastore keys reference these module names | None |
| Live service config | `disableG15Lcd` runtime key (default false; set true in active cfg) becomes inert after lcdui removal — harmless leftover | Optional: leave the key (CuiPreferences still reads it harmlessly) or strip from cfg. NOT a code requirement. |
| OS-registered state | trackIR reads `HKCU\Software\NaturalPoint\...\NPClient Location` at runtime via `LoadLibrary` — this is READ-only probing of the user's machine, owned by the (now-excluded) ClientHeadTracking.cpp. No registry WRITES. | None — excluding the .cpp removes the probe; nothing to unregister. |
| Secrets/env vars | None reference these modules | None |
| Build artifacts | `src/compile/win32/lcdui/` (built `lcdui.lib` output) becomes stale after vcxproj removal; SwgClientSetup compile output if ever built | Optional: clean `compile/win32/lcdui/`. Not boot-critical (nothing links it post-removal). |

**Verified:** No live runtime system caches or persists the four module names beyond the inert `disableG15Lcd` cfg key and the read-only NPClient registry probe (which is removed with the .cpp exclusion).

## Environment Availability

No NEW external dependencies introduced (this is removal). Build/runtime toolchain already validated by prior phases:

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSBuild (VS 2022/v145) | All build steps | ✓ (per prior phases) | v145 / VS "18" BuildTools | — (resolve path via vswhere if it moved) |
| SwgClient_d.exe + gl05/gl11 plugins | Boot smoke | ✓ (Phase 11 closed) | — | — |
| SWGSource VM @ 192.168.1.200 | Boot-to-char-select smoke | external (user-run) | — | Smoke is STAGED-PENDING-style: build gate is automatable; boot gate needs the VM + user run |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The boot-gate validation builds `/t:SwgClient` (+ renderer targets), NOT the full `swg.sln` — so the 6 out-of-closure projects' dangling lcdui refs don't break the gate | Build Invocation, Open Questions | If validation requires a full-solution build, all 7 dependency refs + 7 `.rsp` lines MUST be removed (not just SwgClient's). Mitigated: recommend removing all 7 regardless for cleanliness. |
| A2 | Deleting the `lcdui/` Logitech SDK dir (not just `lcdui_src/`) is in DECRUFT-03 scope | Removal Surface Map T4, Open Questions | If left, `lcdui/lglcd.h` + `lib/lgLcd.lib` remain as harmless orphans after refs are purged. Low risk either way; flag for planner decision. |

All other claims are VERIFIED against the active tree (grep/read) or CITED from the whitengold Phase 07 reference commits (`405cfe9f2`, `fd8f9cff8`, `5a43fc4fe`, `700a90466`).

## Open Questions (RESOLVED)

> All three resolved during planning and threaded into `12-03-PLAN.md` `<scope_decisions>` (A1/A2/Q3).

1. **Full-solution build vs SwgClient-target build as the validation bar?**
   - What we know: established boot-gate uses `/t:SwgClient`; the 6 editors + SwgGodClient are out of that closure but DO have `Debug|Win32.Build.0` (would compile in a full-solution build).
   - What's unclear: whether the planner wants `swg.sln` (all projects) to build clean, or just SwgClient + renderers.
   - Recommendation: **Remove all 7 lcdui ProjectDependency lines + all 7 `.rsp` lcdui entries regardless.** Cost is trivial, it keeps a full-solution build clean, and it matches the "no dangling references" CLEAN intent. Treat the SwgClient ones as mandatory and the other 6 as part of the same lcdui task.
   - **RESOLVED:** A1 — remove all 7 lcdui ProjectDependency lines + all editor `.rsp` lcdui entries; 12-03 Task 2 uses a full-solution `swg.sln` build as its verify gate.

2. **Delete the `lcdui/` (Logitech SDK) directory, or only `lcdui_src/`?**
   - What we know: DECRUFT-03 text names `lcdui.vcxproj` (which lives in `lcdui_src/`) + ".rsp lcdui refs." The `lcdui/` SDK dir (`lglcd.h` + `lib/lgLcd.lib`) is what `lgLcd.lib` resolves to.
   - What's unclear: whether the SDK dir should also be deleted. Phase 07 (CMake) removed the LogitechLCD find/link entirely.
   - Recommendation: Delete BOTH for a complete removal (no project references either after the unwire). If the planner prefers minimal blast radius, deleting only `lcdui_src/` + purging refs satisfies the literal requirement; `lcdui/` then sits as an inert orphan.
   - **RESOLVED:** A2 — delete BOTH `lcdui_src/` and `lcdui/` (12-03 Task 2 step 8) for a complete removal.

3. **Strip `disableG15Lcd` from active cfg / `CuiPreferences`?**
   - Recommendation: OUT of scope for Phase 12 (it's inert and harmless; `CuiPreferences`/`ConfigClientUserInterface` reads it without effect). Voice-key stripping precedent is Phase 14. Leave it.
   - **RESOLVED:** Q3 — OUT of scope; leave `disableG15Lcd` inert.

## Sources

### Primary (HIGH confidence — verified in active tree this session)
- `src/build/win32/swg.sln` — project blocks (779-783, 1553-1557), config blocks (1797-1802, 2361-2366), dependency refs (62/154/290/492/767/883/986)
- `src/engine/client/library/clientGame/build/win32/clientGame.vcxproj` (359-361, 1244) + `includePaths.rsp:5`
- `src/engine/client/library/clientGame/src/shared/core/ClientHeadTracking.cpp/.h`; callers `CockpitCamera.cpp`, `SetupClientGame.cpp`, `SwgCuiOptControls.cpp:196`
- `src/game/client/library/swgClientUserInterface/.../SwgCuiG15Lcd.cpp` (USE_LCD wrapping), `SwgCuiHud.cpp:113`; `includePaths.rsp:54-56`
- `src/game/client/application/SwgClient/build/win32/{libraries.rsp:10, libraryPaths.rsp:8, SwgClient.vcxproj:105/160/206}`
- `src/external/3rd/library/{trackIR, stationapi, lcdui, lcdui_src}/` disk listings; `lcdui_src/build/win32/lcdui.vcxproj`
- `.planning/debug/build-d3d9-all-v145.cmd`, `.planning/debug/resolved/c0000005-async-mgn-crash.md` (build invocation pattern)

### Secondary (HIGH confidence — reference diff template, sibling repo `D:/Code/swg-client`)
- whitengold Phase 07 commits: `405cfe9f2` (orphan dir deletes), `fd8f9cff8` (trackIR + ClientHeadTracking exclusion), `5a43fc4fe` (lcdui CMake unlink), `700a90466` (source-cascade fixes incl. SwgCuiG15Lcd/SwgCuiHud)

### Project context
- `.planning/REQUIREMENTS.md`, `STATE.md`, `PROJECT.md`; MEMORY.md (debug-cfg trap, leave-Koogie-Direct3d9-untouched)

## Metadata

**Confidence breakdown:**
- Removal surface (disk/sln/.rsp/vcxproj): HIGH — every reference grepped + read with file:line citations
- Live-dependency landmines: HIGH — confirmed both NPClient.h and EZ_LCD.h have live includers; confirmed Phase 07 hit + fixed exactly these
- Order/cadence/validation: HIGH — build invocation verified from committed `.cmd` + debug docs; closure analysis verified via sln dependency graph
- Open scope questions (A1/A2): MEDIUM — depend on planner's validation-scope choice; both have low-risk default recommendations

**Research date:** 2026-05-24
**Valid until:** ~30 days (stable — codebase recon of a frozen tree; only invalidated by intervening commits to swg.sln/.rsp/the four target dirs)
