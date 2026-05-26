# Phase 15: In-Game Browser (XPCOM/Mozilla) Removal & Milestone Gate - Research

**Researched:** 2026-05-26
**Domain:** Live-UI source/build decruft — removing the dormant XPCOM/Mozilla in-game browser from the Koogie MSBuild tree (`src/build/win32/swg.sln`) without breaking surviving caller surfaces or the dual-renderer boot, then running the v2.1-closing milestone gate.
**Confidence:** HIGH (all 7 CONTEXT-assigned verification tasks resolved with file:line evidence via repo-wide ripgrep + source reads; the D-02 ordinal-serialization question is settled by tracing the UI type system end-to-end. No external/library lookups needed — this is an in-repo removal phase.)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Remove the WHOLE browser surface, not just the loosely-named roadmap symbols. Re-grep the actual symbol set repo-wide; the `<code_context>` list is a baseline, not a guarantee of completeness. (DONE — see Authoritative Removal Map; baseline confirmed accurate with refinements flagged.)
- **D-02:** Verify-then-clean-delete `TUIWebBrowser` from the `ui` `UITypeID` enum. The `ui` library is name-keyed; `UITypeID` is in-memory RTTI. Researcher confirms no `UITypeID` integer is persisted to TRE/`.ui` data, then deletes `TUIWebBrowser` outright + every `IsA(TUIWebBrowser)` reference + the `SwgCuiTcgControl::IsA` clause.
- **D-02a:** Hard fallback — if ANY `UITypeID` ordinal is serialized to disk/TRE, switch to an ordinal-preserving placeholder (mirroring CR-01's `RESERVED_RADIAL_SLOT_103..105`). The link gate and boot gate CANNOT catch an ordinal-shift regression — this verification is mandatory.
- **D-03:** `libEverQuestTCG` + all TCG infra (`SwgCuiTcgManager/Control/Window`) are OUT of scope and stay. Sever only the browser ties: the two navigate callbacks + the `SwgCuiTcgControl::IsA` `TUIWebBrowser` clause.
- **D-04:** Gut the navigate-callback bodies AND unregister the callbacks from `libEverQuestTCG` entirely (no dead procs). Remove the `createWebBrowserPage/setURL` calls + delete the `navigateProc`/`navigateWithPostDataProc` defns + their registration calls. Drop the `Type == TUIWebBrowser ||` clause from `SwgCuiTcgControl::IsA`.
- **D-04a:** Research caveat — confirm `libEverQuestTCG` tolerates absent navigate callbacks; if it requires them registered, fall back to registered logging-no-op procs.
- **D-05:** De-wire each surviving caller by deleting its browser lines — no no-op shim (criterion #2 grep-zero). Callers that STAY: `SwgCuiManager.cpp`, `SwgCuiCommandParserUI.cpp`, `SwgCuiHud.cpp`/`SwgCuiHudAction.cpp`, and engine `CuiIoWin.cpp` + `IMEManager.cpp` (shared by BOTH renderers).
- **D-06:** Delete the stray vestigial `#if DEBUG==0 #include "libMozilla/libMozilla.h"` in `Game.cpp:147` + the `libMozilla/include/public` entry from `clientGame includePaths.rsp:4`. No actual `libMozilla::` calls in Game.cpp.
- **D-07:** Drop `libMozilla.vcxproj` from `swg.sln` (line 1587, GUID `{C6C1E14A-DEDB-48C6-B028-0C9E4EFF5DC4}`) — the `Project(...)` block, its `GlobalSection(ProjectConfigurationPlatforms)` entries, AND any `ProjectDependencies` on it.
- **D-08:** Unlink the whole Mozilla-family link-token set from `SwgClient.vcxproj` — all INLINE `<Link>` edits (NOT in `libraries*.rsp`; the `.rsp` is vestigial). Tokens differ per config — researcher confirms exact set per D-01.
- **D-09:** Delete the vendored `src/external/3rd/library/libMozilla/` tree entirely (XPCOM SDK; headers contain `xpcom`/`xul`/`Mozilla`).
- **D-10:** Stage copy list — verify, don't assume. Confirm there is no separate Mozilla runtime staging anywhere and record it as "none present."
- **D-11:** Purge the editor `.rsp` + inline `.vcxproj` Mozilla refs (AnimationEditor, ClientEffectEditor, LightningEditor, NpcEditor, ParticleEditor, SwooshEditor, Viewer, SwgGodClient).
- **D-12:** Full milestone-wide residue sweep + per-renderer link gate + dual-renderer boot, incremental build. Re-grep ALL removed subsystems repo-wide == 0, excluding the known KEEP-listed false positives.
- **D-13:** Build gate greps link output for 0 `unresolved external symbol` — NOT just MSBuild exit 0. `/FORCE` masks unresolved externals. Debug AND Release both link clean. Edit inline `.vcxproj`.
- **D-14:** Dual-renderer boot gate — boots to character select under both `rasterMajor=5` (D3D9) and `=11` (D3D11). Set `rasterMajor` in `client_d.cfg` (debug exe reads `client_d.cfg`, not `client.cfg`).

### Claude's Discretion
- Exact edit order, plan/wave breakdown, and commit granularity within the removal — planner's call, provided every coherent step leaves the tree buildable (no mid-step un-buildable state) and the link gate stays green (D-13). The Phase 13/14 shape (atomic link-unit + call-site de-wire in Wave 1, residue/path/editor purge in Wave 1 parallel, vendored-tree delete + milestone gate sequenced last) is a reasonable template.
- Whether the milestone residue sweep (D-12.1) is its own plan/wave or folded into the final boot-gate plan.

### Deferred Ideas (OUT OF SCOPE)
- **Bink video codec / Miles audio** — active middleware, OUT of v2.1. Do not conflate.
- **`libEverQuestTCG` (TCG card game) removal** — dormant middleware, a future-milestone decision; Phase 15 severs only the browser tie.
- **DEF-14-01 — SwgClient Optimized config LNK1281 SAFESEH** — pre-existing, removal-unrelated; validate via Debug+Release link-grep, NOT Optimized image-gen.
- **v2.2 Visual Parity** (asset PS pipeline, gamma LUT, minimap, particles) — next milestone.
- **"Diff SWGSource vs whitengold TRE"** / **"Cantina corner-snap"** — deferred (v2.2 / pre-existing quirk).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| DECRUFT-06 | In-game browser (XPCOM/Mozilla) fully removed from source + build — drop `libMozilla.vcxproj`; remove `libMozilla/include/public` from `includePaths.rsp`; delete `CuiWebBrowser*`/`UIWebBrowserWidget`/XPCOM bridge; Mozilla DLLs off any stage list; client builds + boots | Authoritative Removal Map (below) enumerates all files to delete, all live call/registration sites to de-wire, all link-lib + include-path + `.rsp` + `.sln` refs to purge, with file:line evidence. D-08 per-config link tokens given verbatim. |
| DECRUFT-07 | Client compiles clean + boots to char-select under BOTH `rasterMajor=5` and `=11` after the FULL removal set (cross-cutting milestone gate, verified incrementally) | Validation Architecture + Milestone Residue Sweep (below) define the grep-zero gates with the exhaustive KEEP-list, the Debug+Release link-grep gate, and the dual-renderer boot gate. |
</phase_requirements>

## Summary

Phase 15 removes the dormant XPCOM/Mozilla in-game browser from the active MSBuild tree and then runs the v2.1-closing milestone gate. The fresh repo-wide grep **confirms the CONTEXT `<code_context>` baseline is accurate and essentially complete** — unlike Phase 14, where the scout missed several files, the Phase 15 baseline correctly identified all the consumer files, callers, and build refs. This research adds precise file:line evidence, resolves the two highest-risk caveats (D-02 ordinal serialization and D-04a TCG callback contract) with definitive verdicts, and produces the exact per-config link-token list for `SwgClient.vcxproj`.

**The primary risk — D-02 / the CR-01 recurrence — is RESOLVED as SAFE-TO-DELETE-OUTRIGHT.** The `ui` library's `UITypeID` integer ordinal is **never serialized to any on-disk format**. It is used exclusively for in-memory RTTI (`IsA(UITypeID)`), in-memory palette property-type maps, and in-memory tree navigation. The on-disk `.ui` widget format is dispatched purely by the **string** `TypeName` through `UILoader::CreateObject(name)` → a string-keyed `mConstructorMap` populated by `UIStandardLoader<T>::GetTypeName() = T::TypeName`. Critically, **there is NO `WebBrowser` loader, no `UIWebBrowser` class, and no `TUIWebBrowser` `TypeName` registered in the `ui` library at all** — the WebBrowser widget lives in the GAME layer (`SwgCuiWebBrowserWidget`, code-constructed), so the `ui` loader cannot even instantiate one from disk. Deleting `TUIWebBrowser` from the enum shifts the ordinals of the ~30 `*Style` entries that follow it, but that shift is internally consistent across a recompile and touches nothing persisted. **D-02a (ordinal-preserving placeholder) is NOT needed.** This is structurally different from CR-01, where the radial-menu enum ordinal WAS a retail-TRE row index.

**The second caveat — D-04a — is RESOLVED as SAFE to unregister entirely.** `libEverQuestTCG` stores navigate callbacks in a static-init namespace-scope `CallbackTable` (all members default to null) and only the absent `SWGTCG.dll` ever reads them. The setters are pure assignments; unregistering leaves the members at their default-null state, which is identical to never calling the setter. No fallback needed; D-04 as written is safe.

**Primary recommendation:** Mirror the Phase 13/14 shape. Wave 1 (atomic coherent change per library): delete the 3 `SwgCuiWebBrowser*` source units + their `.vcxproj` ClCompile/ClInclude entries; de-wire all live callers (`SwgCuiManager`, `SwgCuiCommandParserUI`, `SwgCuiHud`, `SwgCuiHudAction`, engine `CuiIoWin` + `IMEManager`); sever the two TCG ties; delete `TUIWebBrowser` from `UITypeID.h`; delete the `Game.cpp:147` dead include; unlink the inline Mozilla-family link tokens from `SwgClient.vcxproj` (3 configs). Wave 1 parallel: purge residue (vestigial `.rsp`, includePaths.rsp, the 7 editor `.rsp`/`.vcxproj`, SwgGodClient inline). Wave 2: drop `libMozilla.vcxproj` from `swg.sln` (project block + 10 config-platform lines + 9 ProjectDependency back-refs); delete the vendored `libMozilla/` tree (1,866 files). Wave 3 / final: the D-12 milestone residue sweep + Debug+Release link-grep + dual-renderer boot. Gate every step with the Debug+Release link-grep (0 `unresolved external symbol`).

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| XPCOM/Mozilla engine driver + browser page | Game UI — `swgClientUserInterface` | — | `SwgCuiWebBrowserManager/Widget/Window` are the ONLY `libMozilla::` consumers; CUI mediators in the game UI lib |
| `libMozilla` wrapper API (XPCOM SDK glue) | External static lib — `libMozilla.vcxproj` | — | `StaticLibrary`; wraps the vendored XPCOM SDK; linked into `SwgClient` only |
| Mozilla-family link libs (xpcom/xul/nspr/plc4/profdir) | Application link — `SwgClient` (+ editors + SwgGodClient) | — | Inline `.vcxproj` `<AdditionalDependencies>`; prebuilt `.lib`s, NOT swg.sln projects |
| `TUIWebBrowser` widget RTTI type | UI library — `ui` (`UITypeID.h`) | Game UI consumers | Pure in-memory RTTI enum value; the widget CLASS lives in the game layer |
| UI-manager browser lifecycle (install/remove/update) | Game UI — `swgClientUserInterface` (`SwgCuiManager`) | — | Renderer-agnostic CUI manager |
| `/browser`+`mozillaBrowserOutput` console commands | Game UI — `swgClientUserInterface` (`SwgCuiCommandParserUI`) | — | Console-command parser; mostly already commented out |
| HUD/IME/IoWin focus-routing `IsA(TUIWebBrowser)` clauses | Engine — `clientUserInterface` + Game UI `SwgCuiHud` | — | `CuiIoWin`/`IMEManager` are ENGINE, shared by both renderers; `SwgCuiHud` is game UI |
| TCG web-navigation callbacks (sever only) | Game UI — `swgClientUserInterface` (`SwgCuiTcgManager`) | `libEverQuestTCG` (stays) | Two `__stdcall` procs registered with the (in-scope-to-stay) TCG lib |
| Stray dead include | Engine — `clientGame` (`Game.cpp`) | — | `#if DEBUG==0` guarded; no `libMozilla::` calls |

**Tier-correctness note for the planner:** Nothing in this phase belongs in a renderer tier (`Direct3d9`/`gl11`). The only engine-tier edits are `clientUserInterface` (`CuiIoWin.cpp`, `IMEManager.cpp`) and `clientGame` (`Game.cpp` dead include) — both renderer-agnostic, which is exactly why both renderers must re-link and re-boot clean (D-05/D-14). The bulk of the surgery is in the game UI lib `swgClientUserInterface`. The `ui` library `UITypeID.h` edit is a pure RTTI-enum change (D-02).

## Authoritative Removal Map (D-01 — re-grepped, CONTEXT baseline CONFIRMED accurate)

> **D-01 verification result: the CONTEXT `<code_context>` baseline is ACCURATE and complete** for the in-scope symbols. Refinements (not misses — the baseline pointed to the right places) are flagged **[REFINED]**. Confirmed there is **NO file named `UIWebBrowserWidget`** — the roadmap criterion #2 names a symbol that does not exist as a file; the real widget is `SwgCuiWebBrowserWidget` (game layer) and the RTTI type is `TUIWebBrowser` (ui lib enum value). `[VERIFIED: repo grep]`

### A. Source files to DELETE (the ONLY `libMozilla::` consumers — D-01)

**`swgClientUserInterface` game UI — 3 units (6 src files + 3 public re-includes):**
- `src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiWebBrowserManager.cpp` + `.h` — drives `libMozilla::init/setUserAgent/enableCache/update`
- `.../src/shared/page/SwgCuiWebBrowserWidget.cpp` + `.h` — holds `libMozilla::Window*`, `ICallback`/`IBlitter` impls; extends `UIWidget`; **`IsA` returns `Type == TUIWebBrowser` at `SwgCuiWebBrowserWidget.h:47`**
- `.../src/shared/page/SwgCuiWebBrowserWindow.cpp` + `.h`
- Public re-includes: `include/public/swgClientUserInterface/{SwgCuiWebBrowserManager.h, SwgCuiWebBrowserWidget.h, SwgCuiWebBrowserWindow.h}`
- → Remove ClCompile/ClInclude entries at `swgClientUserInterface.vcxproj`: ClCompile `:228` (Manager.cpp), `:850` (Widget.cpp), `:851` (Window.cpp); ClInclude `:926` (Manager.h), `:1146` (Widget.h), `:1147` (Window.h) `[VERIFIED: repo grep]`

### B. Live call sites to DE-WIRE in place (callers STAY — D-05)

| File:line | Browser ref | Edit | Notes |
|-----------|-------------|------|-------|
| `SwgCuiManager.cpp:72,474,503,551` | include + `WebBrowserManager::install/remove/update` | delete include (72) + 3 lifecycle lines (474/503/551) | UI-manager lifecycle. **[REFINED]** include at `:72` (CONTEXT said "the include"); lifecycle 474/503/551 confirmed exact |
| `SwgCuiCommandParserUI.cpp:58,119,156,214,1094-1096` | include + `CommandNames` + table row + live handler | delete include (58), `CommandNames::browser` (119), `CommandNames::debugBrowserOutput="mozillaBrowserOutput"` (156), the `debugBrowserOutput` table row (214), the `debugBrowserOutput` handler (1094-1096) | **[REFINED — IMPORTANT]** the `isCommand(...browser...)` handler block at **563-584 is ALREADY entirely commented out (`/* */`)**; the `browser`/`url` table rows at 176-177 are also already commented. Only line 214 (table row) + 1094-1096 (handler) are LIVE. CONTEXT said handlers `:564-580` are live — they are NOT (inside a comment block). |
| `SwgCuiHud.cpp:98,1026` | include + `\|\| focused->IsA(TUIWebBrowser)` focus clause | delete include (98) + drop the `\|\| focused->IsA(TUIWebBrowser)` term from line 1026 | Confirmed exact. Leave the rest of the `IsA(TUITextbox)\|\|...` chain intact |
| `SwgCuiHudAction.cpp:109,1191-1192` | include + LIVE `createWebBrowserPage/setURL` block | delete include (109) + the 2-line `createWebBrowserPage(false)`/`setURL(finalUrl…)` block (1191-1192) | **[REFINED]** CONTEXT pointed at `SwgCuiHudAction.cpp` "include + hooks"; the live block is 1191-1192 (a CS-help URL flow). **SCOPE GUARD: lines 86/99 (`SwgCuiCommandBrowser.h`/`SwgCuiMissionBrowser.h`) and the many `commandBrowser`/`missionBrowser`/`chatRoomBrowser`/`persistentMessageBrowser` actions are SIBLING list/table UIs — NOT web browsers. DO NOT TOUCH.** |
| `CuiIoWin.cpp:513,647` (engine `clientUserInterface`) | `\|\| focused->IsA(TUIWebBrowser)` (×2) | drop the `\|\| focused->IsA(TUIWebBrowser)` term from both lines | Engine lib, shared by BOTH renderers. `:513` keyboard focus; `:647` `textIsFocused` |
| `IMEManager.cpp:353` (engine `clientUserInterface`) | `\|\| focused->IsA(TUIWebBrowser)` | drop the `\|\| focused->IsA(TUIWebBrowser)` term | Engine lib, shared by both renderers; inside a `!( … )` negation — drop only the WebBrowser disjunct |

### C. TCG ties — SEVER ONLY (D-03 / D-04; libEverQuestTCG + all TCG infra STAYS)

| File:line | Browser tie | Edit |
|-----------|-------------|------|
| `SwgCuiTcgManager.cpp:19` | `#include "swgClientUserInterface/SwgCuiWebBrowserManager.h"` | delete include |
| `SwgCuiTcgManager.cpp:33-34` | `navigateProc`/`navigateWithPostDataProc` forward decls (in `SwgCuiTcgManagerNamespace`) | delete both decls |
| `SwgCuiTcgManager.cpp:68-69` | `libEverQuestTCG::setNavigateCallback(navigateProc)` / `setNavigateWithPostDataCallback(navigateWithPostDataProc)` | delete both registration calls (D-04: unregister entirely — SAFE per D-04a verdict below) |
| `SwgCuiTcgManager.cpp:124-150` | `navigateProc` defn (124-133) + `navigateWithPostDataProc` defn (137-150) — the bodies that call `SwgCuiWebBrowserManager::createWebBrowserPage/setURL` | delete both defns |
| `SwgCuiTcgControl.h:91-94` | `IsA` returns `Type == TUIWebBrowser \|\| UIWidget::IsA(Type)` | change to `return UIWidget::IsA(Type);` (drop the `Type == TUIWebBrowser \|\|` clause) |

**Untouched TCG infra (stays):** `SwgCuiTcgManager` (install/remove/launch/update/setLoginInfo + the 6 audio/window callbacks playSound/playMusic/setSoundVolume/setMusicVolume/stopAllSounds/setWindowState), `SwgCuiTcgWindow`, the rest of `SwgCuiTcgControl`, `libEverQuestTCG` lib + its 3 compiled `.lib`s.

### D. `ui` library `UITypeID` enum (D-02 — the CR-01 hazard; verdict: SAFE-TO-DELETE-OUTRIGHT)

- `src/external/3rd/library/ui/src/shared/core/UITypeID.h:64` → `TUIWebBrowser,` — last entry in the `TUIWidget` block, immediately before `TUIStyle` (line 66). ~30 `*Style` + downstream entries follow before `TUINumTypes` (the sentinel at line 127). Delete the single line `:64`. `[VERIFIED: repo read]`
- The `include/UITypeID.h` shim (`#include "../src/shared/core/UITypeID.h"`) needs no edit.

### E. Engine `clientGame` — dead include (D-06)

- `src/engine/client/library/clientGame/src/shared/core/Game.cpp:146-148`:
  ```cpp
  #if DEBUG==0
  #include "libMozilla/libMozilla.h"
  #endif
  ```
  Delete all 3 lines. **`grep "libMozilla::" Game.cpp` == 0** — confirmed no actual calls; the include is purely vestigial. `[VERIFIED: repo grep]` (Note: `#if DEBUG==0` means only Release/Optimized configs ever preprocessed this — Debug won't reveal breakage here; Release must link clean.)
- `clientGame/build/win32/includePaths.rsp:4` → `../../../../../../external/3rd/library/libMozilla/include/public` — delete the line. `[VERIFIED]`
- `clientGame.vcxproj` — 3 inline `libMozilla\include\public` `AdditionalIncludeDirectories` entries (one per config). Remove all 3. `[VERIFIED: grep -c == 3]`
- **Note:** `clientUserInterface.vcxproj` has **0** inline libMozilla refs (`grep -c == 0`) — it reaches the WebBrowser symbols via public re-includes of `swgClientUserInterface`. No include-path edit needed there; only the `CuiIoWin.cpp`/`IMEManager.cpp` source edits (section B). `[VERIFIED]`

### F. `swg.sln` — drop `libMozilla.vcxproj` (D-07)

`src/build/win32/swg.sln`, GUID `{C6C1E14A-DEDB-48C6-B028-0C9E4EFF5DC4}`:
- **Project declaration:** `:1587-1588` — `Project("{8BC9CEB8-…}") = "libMozilla", "..\..\external\3rd\library\libMozilla\build\win32\libMozilla.vcxproj", "{C6C1E14A-…}"` + its `EndProject`. Delete the block.
- **Config-platform entries:** `:2350-2355` — 6 lines (`Debug\|Win32.ActiveCfg`/`.Build.0`, `Optimized\|Win32.ActiveCfg`/`.Build.0`, `Release\|Win32.ActiveCfg`/`.Build.0`). Delete all 6.
- **ProjectDependency back-references — 9 lines** (each a `{C6C1E14A-…} = {C6C1E14A-…}` inside another project's `ProjectSection(ProjectDependencies)`): `:25` (AnimationEditor), `:116` (ClientEffectEditor), `:251` (LightningEditor), `:386` (NpcEditor), `:452` (ParticleEditor), **`:726` (SwgClient)**, `:835` (SwgGodClient), `:938` (SwooshEditor), `:1220` (Viewer). Delete all 9. `[VERIFIED: repo grep — line 726 is the real build-order edge for the client EXE]`

### G. `SwgClient.vcxproj` — inline Mozilla-family link tokens (D-08 — exact per-config; see dedicated section below)

### H. Editor refs to PURGE (D-11) — out-of-scope-as-targets, refs must still go

All 7 editors + SwgGodClient carry both `.rsp` AND inline `.vcxproj` Mozilla tokens. `[VERIFIED: repo grep]`

| Project | `libraries.rsp` (5 libs: nspr4/plc4/profdirserviceprovider_s/xpcom/xul) | `includePaths.rsp` (libMozilla/include/public) | `libraryPaths.rsp` (libMozilla/include/private/lib/release) | inline `.vcxproj` deps (3 cfgs) | inline `.vcxproj` include/lib path |
|---------|--------|--------|--------|--------|--------|
| AnimationEditor | `:12-16` | `:9` | `:8` | `:99,141,183` | yes |
| ClientEffectEditor | `:13-17` | `:6` | `:8` | `:99,141,183` | yes |
| LightningEditor | `:17-21` | `:43` | `:9` | `:99,142,184` | yes |
| NpcEditor | `:17-21` | `:46` | `:9` | `:99,142,184` | yes |
| ParticleEditor | `:17-21` | `:43` | `:9` | `:99,143,185` | yes |
| SwooshEditor | `:17-21` | `:45` | `:9` | `:99,142,184` | yes |
| Viewer | `:10-14` | `:5` | `:7` | `:99,141,183` | yes |
| **SwgGodClient** | `:20-24` | `:44` | `:9` | **`:99` Debug** (`nspr4/plc4/profdirserviceprovider_s/xpcom/xul` + **`libMozilla.lib`**); `:143,185` (`nspr4/plc4/profdirserviceprovider_s/xpcom/xul`) | include `:76,121,163`; lib dir `:101,145,187` (`libMozilla\Debug`, `libMozilla\include\private\lib\release`) |

**SwgGodClient is the only editor that links `libMozilla.lib` inline** (Debug `:99`) AND has a libMozilla compile-output lib dir (`:101`). Use the STATE.md MSYS-sed gotcha for the `.vcxproj` path purges (substring+delimiter `[^;<>]*TOKEN[^;<>]*;`, NOT backslash-escaping).

### I. Vendored tree to DELETE (D-09)
- `src/external/3rd/library/libMozilla/` — **1,866 files** including `include/public/libMozilla/libMozilla.h` (+ `src/win32/libMozilla.h/.cpp`, the real `namespace libMozilla` API), `include/private/sdk/include/nsIWebBrowser*.h`, and `include/private/bin/{debug,release}/components/*.xpt` (XPCOM components — e.g. `alerts.xpt`, `appshell.xpt`). The `.xpt` + header contents are why D-09 grep-zero across include paths requires the whole-tree delete. `[VERIFIED: find -type f]`
- Sequence LAST (Wave 2), after all `.vcxproj`/`.rsp` include-path purges land — otherwise an unpurged `libMozilla\include` AdditionalIncludeDirectories would dangle (MSVC tolerates a missing include dir as a warning, but the goal is grep-zero correctness).

## D-02 / D-02a Verdict: SAFE-TO-DELETE-OUTRIGHT (the CR-01 recurrence is NOT present here)

**VERDICT: SAFE-TO-DELETE-OUTRIGHT (D-02). D-02a placeholder is NOT needed.** `[VERIFIED: repo read — full UI type-system trace]`

**Evidence chain proving the `UITypeID` integer ordinal is never persisted:**

1. **`UITypeID` is a plain positional enum, no explicit values.** `UITypeID.h:13-128` — `enum UITypeID { TUIObject, …, TUIWebBrowser, TUIStyle, …, TUINumTypes };`. No `= N` assignments; ordinals are implicit/positional. `TUINumTypes` is the count sentinel. `[VERIFIED: UITypeID.h]`

2. **The enum is used ONLY for in-memory operations.** A repo-wide grep of the `ui` library for `UITypeID`/`GetTypeId` usage (excluding the `IsA(const UITypeID Type)` signature itself) finds exactly:
   - `UIPalette.cpp:36` — `ui_stdmap<UITypeID, PalettePropertyMap>` (in-memory property-type map, rebuilt at runtime)
   - `UIPalette.cpp:347` / `UIPalette.h:66` — `GetPropertyNamesForType(UITypeID id, …)`
   - `UIBaseObject.h:142-143` — `GetParent(UITypeID)` (in-memory tree navigation)
   - `UIPage.cpp:1834` / `UIPage.h:124-156` — `GetCodeDataObject(UITypeID id, …)` (in-memory code-data lookup)
   - `UIManager.cpp:1601` — `CancelEffectorsFor(…, UITypeID type)` (in-memory effector cancel)
   No `Write`/`Read`/`Archive`/stream/serialize of the integer anywhere. `[VERIFIED: repo grep]`

3. **The on-disk `.ui` format is dispatched by STRING name, not the integer.** `UIBaseObject.h:78,83` declares `static const char * const TypeName;` and `virtual const char *GetTypeName() const;`. `UIStandardLoader<T>::GetTypeName() const { return T::TypeName; }` (`UIStandardLoader.h:23`). `UILoader::AddToken(const char * const name, const UILoaderExtension *Constructor)` populates a **string-keyed** `mConstructorMap[name]` (`UILoader.cpp:76-81`), and `UILoader::CreateObject(const char * const name)` does `mConstructorMap.find(name)` → `->Create()` (`UILoader.cpp:85-95`). The integer `UITypeID` is **never** a key, never written, never read from disk. `[VERIFIED: UILoader.cpp / UIStandardLoader.h / UIBaseObject.h]`

4. **There is NO `WebBrowser` loader, no `UIWebBrowser` class, and no `TUIWebBrowser` `TypeName` in the `ui` library.** The ONLY `WebBrowser` hit in the entire `ui` library is `UITypeID.h:64` (the enum value). `UILoaderSetup.cpp` registers `UIStandardLoader<T>` for ~50 widget types but **none for a WebBrowser**. The WebBrowser widget class (`SwgCuiWebBrowserWidget : public UIWidget`) lives in the GAME layer and is **code-constructed** — it has no `TypeName` and is not loadable from `.ui` data. So the `ui` loader cannot instantiate a WebBrowser from disk; the on-disk layout never references the type. `[VERIFIED: repo grep — `webbrowser` in `ui/` == only UITypeID.h:64]`

**Why this differs from CR-01:** In Phase 14, `CuiMenuInfoTypes::Type`'s ordinal WAS used directly as a `datatables/player/radial_menu.iff` ROW INDEX by `RadialMenuManager` (`s_ranges.find(menuType)`) — a serialized-index coupling. `UITypeID` has no such coupling: the `.ui` loader is name-keyed, and the only on-disk reference to a widget type is its string `TypeName`. Deleting `TUIWebBrowser` shifts the implicit ordinals of `TUIStyle`+downstream by −1, but every consumer recompiles against the new ordinals consistently, and nothing on disk encodes the old integer.

**Every `IsA(TUIWebBrowser)` / `Type == TUIWebBrowser` reference repo-wide (5 total — all deleted/edited per sections B/C/D):** `[VERIFIED: repo grep]`
- `SwgCuiWebBrowserWidget.h:47` — `IsA` returns `Type == TUIWebBrowser \|\| UIWidget::IsA(Type)` (whole file deleted, section A)
- `SwgCuiTcgControl.h:93` — `Type == TUIWebBrowser \|\| UIWidget::IsA(Type)` (edit to drop the clause, section C)
- `SwgCuiHud.cpp:1026` — `\|\| focused->IsA(TUIWebBrowser)` (drop the disjunct, section B)
- `IMEManager.cpp:353` — `\|\| focused->IsA(TUIWebBrowser)` (drop the disjunct, section B)
- `CuiIoWin.cpp:513` + `:647` — `\|\| focused->IsA(TUIWebBrowser)` (drop the disjunct ×2, section B)

## D-04a Verdict: SAFE to unregister navigate callbacks entirely (D-04 as written; NO fallback needed)

**VERDICT: SAFE — `libEverQuestTCG` tolerates absent/unregistered navigate callbacks.** `[VERIFIED: libEverQuestTCG.cpp read]`

**Evidence:**
- `libEverQuestTCG` has in-tree source (`src/external/3rd/library/libEverQuestTCG/src/win32/libEverQuestTCG.cpp`), not just a prebuilt lib.
- The callback storage is a **static namespace-scope object**: `PrivateData::callbackTable` (`libEverQuestTCG.cpp:153`, type `CallbackTable` whose `navigateProc`/`navigateWithPostDataProc` members are function pointers, lines 22-23). Static-init zero-initializes all members to null.
- `setNavigateCallback`/`setNavigateWithPostDataCallback` (`:270-278`) are pure assignments (`callbackTable.navigateProc = navigateProc;`). **Unregistering = the members stay null, which is the identical state to never calling the setter.**
- The navigate procs are **never invoked inside this `.cpp`** — `callbackTable.navigateProc` is written by the setters but read nowhere in `libEverQuestTCG.cpp`. The actual consumer is the loaded `SWGTCG.dll` (one of the DLLs in `PrivateData::aDLLsToLoad`, `:162-180`), which is **absent on the SWGSource VM** — so `launch()` never loads it and the procs are never called.
- `init()` (`:199-209`) just stores config and returns true. No callback is required at init.

**Conclusion:** Removing the two registration calls (D-04) leaves the table at its default-null state and severs the only browser tie. Since the TCG DLL never loads on the deployment target, there is zero risk of a null-callback crash. **The D-04a fallback (registered logging-no-op procs) is unnecessary.** The exact registration call sites to remove: `SwgCuiTcgManager.cpp:68-69`.

## D-08 Verdict: Exact per-config Mozilla-family link tokens in `SwgClient.vcxproj`

> **`[VERIFIED: SwgClient.vcxproj read]`** All three configs carry the wrapped XPCOM libs inline; only Optimized + Release link `libMozilla.lib`; Debug does NOT. The `.rsp` files are vestigial (mirror tokens at `libraries.rsp:10-14` but `SwgClient.vcxproj` does not `@`-reference them — confirmed by the Phase 13/14 finding). **Edit the inline `.vcxproj` as authoritative; treat `.rsp` as cosmetic grep-zero cleanup.**

> **[CORRECTION to CONTEXT D-08 baseline]:** The CONTEXT said the lib-dir entries are `libMozilla\include\private\lib\{release,Optimized}` + `libMozilla\Optimized`. The actual per-config dir entries differ — Debug carries `compile\win32\libMozilla\Release` + `include\private\lib\Release`; Optimized carries `include\private\lib\release` + `compile\win32\libMozilla\Optimized` + `include\private\lib\Optimized`; Release carries `compile\win32\libMozilla\Release` + `include\private\lib\release`. Exact tokens below.

### Debug (`'$(Configuration)\|$(Platform)'=='Debug\|Win32'`)
- **`<AdditionalDependencies>` line 103** — remove these tokens: `nspr4.lib`, `plc4.lib`, `profdirserviceprovider_s.lib`, `xpcom.lib`, `xul.lib`. **NO `libMozilla.lib` present** (confirmed — CONTEXT correct).
- **`<AdditionalLibraryDirectories>` line 105** — remove: `..\..\..\..\..\..\..\src\compile\win32\libMozilla\Release` AND `..\..\..\..\..\..\..\src\external\3rd\library\libMozilla\include\private\lib\Release`.
- SAFESEH: `/SAFESEH:NO /VERBOSE` at line 110 (Debug links clean for the gate).

### Optimized (`'$(Configuration)\|$(Platform)'=='Optimized\|Win32'`)
- **`<AdditionalDependencies>` line 158** — remove the first occurrence `nspr4.lib;plc4.lib;profdirserviceprovider_s.lib;xpcom.lib;xul.lib` AND the DUPLICATED trailing occurrence near the end: `nspr4.lib;profdirserviceprovider_s.lib;xpcom.lib;xul.lib;libMozilla.lib` (note the duplicate omits `plc4.lib` but adds `libMozilla.lib`). Remove ALL occurrences of each token. **`libMozilla.lib` IS present here.** `[VERIFIED — CONTEXT "Optimized has duplicates" confirmed]`
- **`<AdditionalLibraryDirectories>` line 160** — remove: `..\..\..\..\..\..\..\src\external\3rd\library\libMozilla\include\private\lib\release`, `..\..\..\..\..\..\..\src\compile\win32\libMozilla\Optimized`, AND `..\..\..\..\..\..\..\src\external\3rd\library\libMozilla\include\private\lib\Optimized` (THREE dir entries).
- **DEF-14-01:** Optimized has NO `/SAFESEH:NO` and NO `ImageHasSafeExceptionHandlers=false` → pre-existing LNK1281. **Gate is Debug+Release only (D-13), NOT Optimized.** `[VERIFIED — line 165 `</Link>` has neither guard]`

### Release (`'$(Configuration)\|$(Platform)'=='Release\|Win32'`)
- **`<AdditionalDependencies>` line 204** — remove: `nspr4.lib`, `plc4.lib`, `profdirserviceprovider_s.lib`, `xpcom.lib`, `xul.lib`, AND `libMozilla.lib` (near the end, before `legacy_stdio_definitions.lib`). **`libMozilla.lib` IS present here.** `[VERIFIED — CONTEXT correct]`
- **`<AdditionalLibraryDirectories>` line 206** — remove: `..\..\..\..\..\..\..\src\compile\win32\libMozilla\Release` AND `..\..\..\..\..\..\..\src\external\3rd\library\libMozilla\include\private\lib\release`.
- SAFESEH: `ImageHasSafeExceptionHandlers=false` at line 211 + `ShowProgress=LinkVerbose` at 212 (Release links clean for the gate).

**Token KEEP-warning:** Do NOT remove `legacy_stdio_definitions.lib`, `libxml2-win32-*.lib`, `libjpeg.lib`, `soePlatform\libs\Win32-*` dir, or any non-Mozilla token. The Mozilla family is exactly: `nspr4.lib`, `plc4.lib`, `profdirserviceprovider_s.lib`, `xpcom.lib`, `xul.lib`, `libMozilla.lib` + the `libMozilla\*` dir entries.

## D-10 Verdict: NO Mozilla DLL staging anywhere — "none present"

**VERDICT: none present.** `[VERIFIED: repo grep]`
- `SwgClient.vcxproj` `PostBuildEvent` copies ONLY `_d.exe`/`_d.pdb` (Debug, `:125-126`) and `_r.exe`/`_r.pdb` (Release, `:223+`) to `stage\`. No DLL copy.
- The only `copy-libs.bat` in the tree is `src/external/3rd/library/soePlatform/copy-libs.bat` (SOE/VChat-related, unrelated to Mozilla — no Mozilla refs).
- No `stage/mozilla/` directory; no Mozilla DLL under `stage/`; no Mozilla reference in any `.bat`/`.cmd`/`.ps1`.
- `libMozilla` is a `StaticLibrary` (`libMozilla.vcxproj:24/30/36` `ConfigurationType`) — there is no runtime DLL to unstage. The XPCOM `.xpt` components in the vendored tree (`include/private/bin/{debug,release}/components/`) are never copied to `stage/`.

**Criterion #2's "Mozilla DLLs removed from POST_BUILD/stage" is a NO-OP here** — record explicitly as "none present, verified."

## Milestone Residue Sweep (D-12 — exact tokens + KEEP-list)

> **IMPORTANT D-12 finding: the prior-phase tokens are NOT all literally zero in active source.** A naive `grep -ri TOKEN src` returns hits in three benign categories the sweep MUST KEEP-list. The sweep must be scoped/excluded precisely or it will false-positive on removal-provenance comments and out-of-scope editor/UI residue. `[VERIFIED: repo grep]`

### Sweep tokens (re-grep each repo-wide == 0 after applying the KEEP-list)
- **P12:** `trackIR`, `stationapi`, `SwgClientSetup`, `lcdui` / `G15`
- **P13:** `videocapture`, `AudioCapture`
- **P14:** `[Vv]ivox`
- **P15:** `xpcom`, `xul`, `[Mm]ozilla`, `libMozilla`, `TUIWebBrowser` (+ `nspr4`, `plc4`, `profdirserviceprovider`)

### KEEP-list — known false positives the sweep MUST exclude (verified still present)
1. **Vivox false positives (P14 DEF-14-02 / D-10):**
   - `soePlatform/ChatAPI2/…/ChatRoom.cpp`/`.h`/`ChatRoomCore.cpp` — `getVoiceCount`/`getVoiceCore`/`getVoice` community-chat methods (ZERO vivox literals). `[VERIFIED: 3 files still present]`
   - `soePlatform/libs/Win32-{Debug,Release}/VChatAPI.lib` + `Base_vchat.lib` — prebuilt voice libs KEPT (unlinked at the `.vcxproj` link line in P14, source tree `VChatAPI/` already deleted). `[VERIFIED: 4 lib files still present]`
2. **Removal-provenance COMMENTS (intentional, document prior removals — KEEP):**
   - `ClientHeadTracking.cpp:15-23` — "NaturalPoint TrackIR support removed in DECRUFT-01" comment block (the `trackIR`/`TrackIR` tokens here are documentation). `[VERIFIED]`
   - `SwgCuiG15Lcd.cpp:26` — "lcdui (Logitech G15 LCD) support removed in DECRUFT-03" comment. `[VERIFIED]`
   - `SwgCuiOptControls.cpp:196` — `getCodeDataObject(TUICheckbox, checkbox, "checkTrackIr")` — a UI-data label string for the (now-inert) checkbox; retained intentionally per the ClientHeadTracking comment. `[VERIFIED]`
3. **Planning/docs:** `.planning/`, `docs/`, `AGENTS.md` contain the literal tokens by design — exclude from the source sweep.

### Out-of-scope residue the sweep WILL surface but which is NOT Phase 15 scope (flag for planner, do NOT silently expand scope)
- **`lcdui` P12 gap:** `swgClientUserInterface.vcxproj:73,111,148` still list `external\3rd\library\lcdui`, `lcdui_src\src\win32`, `lcdui_src\src\win32\LCDUI` include paths, and `SwgCuiG15Lcd.cpp`/`.h` are still ClCompile/ClInclude (`:450,1004`). `SwgGodClient.vcxproj` (editor) still has `lcdui\Debug` + `lcdui\lib`. These are a leftover from DECRUFT-03 (P12) that the milestone sweep will flag. **DECISION FOR PLANNER:** D-12 says "re-grep ALL removed subsystems == 0" — if the milestone gate requires literal lcdui-zero, this P12 residue must be cleaned (opportunistically, since Phase 15 is the milestone-closing phase and the residue sweep is its responsibility). The lcdui include-path strings are inert (MSVC ignores missing AdditionalIncludeDirectories), and `SwgCuiG15Lcd.cpp:26` shows the body was already gutted to a removal comment. **Recommend: fold the lcdui-residue cleanup into the D-12 sweep wave** (it is the milestone-closing phase; leaving it fails a literal lcdui grep-zero). This is an `[ASSUMED]` scope interpretation — see Assumptions Log A1.
- **`stationapi` 989crypt.lib:** SwgGodClient `:99` links `989crypt.lib` (P12 noted stationapi's `989crypt.lib` was a LIVE dep). This is editor-only and was kept live in P12 — KEEP.

## Symbol-Resolution Analysis (drives wave/edit ordering)

**Where do the browser symbols resolve?**
- `SwgCuiWebBrowserManager::`/`SwgCuiWebBrowserWidget::`/`SwgCuiWebBrowserWindow::` → in the deleted `.cpp`s, compiled into `swgClientUserInterface.lib`.
- `libMozilla::init/createWindow/…` → in `libMozilla.lib` (the StaticLibrary built from `libMozilla.vcxproj`). Consumed ONLY by the 3 `SwgCuiWebBrowser*.cpp`s.
- The XPCOM SDK libs (`xpcom.lib`/`xul.lib`/`nspr4.lib`/`plc4.lib`/`profdirserviceprovider_s.lib`) → transitive deps that `libMozilla.lib` wraps; no direct source consumer.
- `TUIWebBrowser` → a compile-time enum value in `ui.lib` headers (no runtime symbol).

**The dangerous directions (what `/FORCE` would mask):**
1. Removing `libMozilla.lib` / the XPCOM libs from `SwgClient.vcxproj` BEFORE deleting the 3 `SwgCuiWebBrowser*.cpp`s → those `.obj`s reference `libMozilla::*` → unresolved external (masked by `/FORCE`). So delete the WebBrowser source in the SAME coherent change as removing the link libs.
2. Deleting the WebBrowser source BEFORE de-wiring its callers (`SwgCuiManager`, `SwgCuiCommandParserUI`, `SwgCuiHudAction`, `SwgCuiTcgManager`) → those `.obj`s reference `SwgCuiWebBrowserManager::*` → unresolved. So de-wire ALL callers in the same coherent change.
3. Deleting `TUIWebBrowser` from `UITypeID.h` BEFORE editing the 5 `IsA(TUIWebBrowser)` sites → compile error `TUIWebBrowser undeclared`. So edit the 5 `IsA` sites in the same change as the enum delete.

**Coherent atomic unit (Wave 1):** de-wire all callers (B) + sever TCG ties (C) + delete `TUIWebBrowser` from the enum (D) + delete the 3 WebBrowser source units + their `.vcxproj` ClCompile entries (A) + delete the `Game.cpp` dead include (E) + remove the inline Mozilla link tokens from `SwgClient.vcxproj` (G) — ALL together. The symbol graph is `callers → SwgCuiWebBrowser* → libMozilla(libs)` and `5 IsA sites → TUIWebBrowser(enum)`; both close in one change.

**Wave 1 parallel (touches no symbols):** vestigial `.rsp` purge, includePaths.rsp, the 7 editor `.rsp` + inline `.vcxproj`, SwgGodClient inline.

**Wave 2 (sequence last):** drop `libMozilla.vcxproj` from `swg.sln` (F) + delete the vendored `libMozilla/` tree (I).

**Wave 3 / final:** D-12 milestone residue sweep + Debug+Release link-grep + dual-renderer boot.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Keeping `SwgCuiWebBrowserManager` callable after lib removal | A no-op `SwgCuiWebBrowserManager` stub | Delete the class + de-wire callers (D-05) | A stub keeps the symbol → fails criterion #2 grep-zero |
| Ordinal-preserving placeholder for `TUIWebBrowser` | A `TUIReserved_WebBrowser` enumerator (the D-02a fallback) | Delete `TUIWebBrowser` outright (D-02) | The `UITypeID` ordinal is NEVER serialized — the placeholder is unnecessary complexity here (UNLIKE CR-01). See D-02 verdict. |
| Keeping the TCG navigate procs "just in case" | Registered logging-no-op procs (the D-04a fallback) | Unregister + delete entirely (D-04) | `libEverQuestTCG` tolerates null/absent callbacks (static-init null + DLL absent). See D-04a verdict. |
| Verifying a clean link | `MSBuild exit 0` | Grep link output for `unresolved external symbol == 0`, Debug AND Release (D-13) | `/FORCE` + `/VERBOSE` (`SwgClient.vcxproj:110`) downgrades LNK2019 to warnings and still emits a binary at exit 0 — proven false-pass in Phases 12/13/14 |
| Purging `.vcxproj` path lists on Windows | `sed` escaping literal backslashes | Match by substring+delimiter `[^;<>]*TOKEN[^;<>]*;` | MSYS sed mis-parses `backslash+digit` (e.g. `3rd`) as a back-reference (STATE.md Phase 13 finding) |
| Scrubbing retail TRE assets | Editing compiled `.ui`/datatable rows | Accept inert + rely on graceful no-op + boot gate | No `dsrc/`/`data/` in this repo; the WebBrowser widget is not even `.ui`-loadable (code-constructed) |

**Key insight:** This is a closed symbol chain with a single root consumer family (`SwgCuiWebBrowser*` → `libMozilla` → XPCOM libs). The atomic-delete pattern (Phase 13/14 D-01) is correct. The two scary-looking hazards (the `UITypeID` enum and the live TCG callback) BOTH resolve to "safe outright delete" once the evidence is traced — neither fallback is needed.

## Common Pitfalls

### Pitfall 1: Treating the `UITypeID` delete as a CR-01 repeat and adding an unnecessary placeholder
**What goes wrong:** Over-caution leads to a `TUIReserved_WebBrowser` placeholder that adds dead enum state for no benefit.
**Why it happens:** The CR-01 lesson ("never mid-sequence-delete a positional enum") is correct in general but the trigger condition (the ordinal IS a serialized index) does NOT hold for `UITypeID`.
**How to avoid:** Trust the D-02 evidence chain — the `.ui` loader is string-keyed; the integer is never persisted. Delete outright.
**Warning signs:** If you find ANY `IFF`/`Archive`/`Write`/`Read` of a `UITypeID` integer (there are none), STOP and switch to D-02a.

### Pitfall 2: Over-deleting in `SwgCuiHudAction.cpp` (the sibling "Browser" UIs)
**What goes wrong:** `SwgCuiCommandBrowser`, `SwgCuiMissionBrowser`, `chatRoomBrowser`, `persistentMessageBrowser` are list/table UIs, NOT web browsers. Deleting their includes/actions breaks the HUD.
**How to avoid:** Touch ONLY the `SwgCuiWebBrowserManager` include (`:109`) and the `createWebBrowserPage/setURL` block (`:1191-1192`). Leave every other `*Browser` ref intact.
**Warning signs:** Compile error `SwgCuiCommandBrowser is not a member` / missing `WS_CommandBrowser`.

### Pitfall 3: Editing only the `.rsp`, missing the inline `.vcxproj` (and vice-versa)
**What goes wrong:** Removing Mozilla from `libraries.rsp` but not `SwgClient.vcxproj:103` → the lib still links; or the source is deleted but the inline lib stays → `/FORCE`-masked unresolved-externals false-pass.
**How to avoid:** Edit the inline `.vcxproj` as authoritative (link libs + lib dirs + include dirs); treat `.rsp` as cosmetic grep-zero cleanup. The `.sln` ProjectDependency on `{C6C1E14A…}` (line 726 for SwgClient) is ALSO authoritative for build order — remove it.
**Warning signs:** Link log shows `unresolved external symbol "public: … libMozilla::"`.

### Pitfall 4: Missing the 9 `.sln` ProjectDependency back-references
**What goes wrong:** Removing only the `Project(...)` block + config-platform lines leaves 9 dangling `{C6C1E14A…} = {C6C1E14A…}` dependency refs → `swg.sln` references a non-existent project GUID.
**How to avoid:** Remove all 11 `.sln` locations: project block (1587-1588), 6 config-platform lines (2350-2355), 9 dependency back-refs (25/116/251/386/452/726/835/938/1220). **The SwgClient one (726) is the real build-order edge.**

### Pitfall 5: The milestone sweep false-positives on KEEP-list residue
**What goes wrong:** A literal `grep -ri "trackIR\|lcdui\|vivox" src == 0` fails on removal-provenance comments, `checkTrackIr` UI label, `soePlatform/ChatAPI2` `getVoice*`, and prebuilt `VChatAPI.lib`.
**How to avoid:** Apply the KEEP-list (Milestone Residue Sweep section) — exclude `soePlatform/ChatAPI2/`, `soePlatform/libs/*VChat*`, the removal-comment lines, the `checkTrackIr` label, and `.planning/`/`docs/`.
**Warning signs:** The sweep reports "holdouts" that are all in `soePlatform/ChatAPI2/` or are `// … removed in DECRUFT-0X` comments.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| XPCOM/Mozilla in-game browser (live SOE feature: TCG store, CS help, `/browser` cmd) | Stubbed dormant (`libMozilla::init` returns true, no XPCOM runtime, never renders); no TCG web backend on SWGSource VM | retail sunset / private-server era; Phase 9 `char16_t` removed the `/Zc:wchar_t` ABI blocker | Removal loses no working feature; clean unlink |
| `.rsp` response files drive the link (CMake era) | Inline `.vcxproj` `<AdditionalDependencies>` + `.sln` ProjectDependencies drive the link (Koogie MSBuild) | Phase 9 Option D base swap | `.rsp` files are vestigial; edit `.vcxproj` + `.sln` |
| `UITypeID` ordinal feared as a serialized index (CR-01 analogy) | Confirmed in-memory-only RTTI; `.ui` loader is string-keyed | this research | `TUIWebBrowser` is safe to delete outright (D-02), not D-02a |

**Deprecated/outdated:**
- The roadmap criterion #2 symbol name `UIWebBrowserWidget` — **does not exist as a file**. The real symbols are `SwgCuiWebBrowserWidget` (game layer) + `TUIWebBrowser` (ui enum value).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The D-12 milestone sweep should fold in the leftover P12 `lcdui` residue cleanup (swgClientUserInterface.vcxproj include paths + `SwgCuiG15Lcd.cpp/.h` ClCompile entries) to satisfy a literal `lcdui` grep-zero | Milestone Residue Sweep | LOW-MEDIUM — the lcdui strings are inert (missing-dir warnings only). If the planner scopes the sweep to "newly-removed (XPCOM) tokens == 0 + prior tokens == previously-established baseline," the lcdui cleanup can be deferred. Flag for plan-time decision; this is a SCOPE interpretation of D-12.1, not a build risk. |
| A2 | The 5 `IsA(TUIWebBrowser)` disjuncts can be dropped without changing focus-routing behavior for the surviving widget types | D-02 / section B | LOW — `TUIWebBrowser` widgets never instantiate (no loader, stubbed browser), so the disjunct was always false at runtime; dropping it is behavior-preserving. Backstopped by the boot gate. |
| A3 | No game-server tree in this repo consumes any browser symbol | Symbol-Resolution | LOW — confirmed the active tree is client-only (swg.sln is the client sln); the WebBrowser symbols are client-UI-only. |

## Open Questions

1. **lcdui residue scope in the D-12 milestone sweep (A1).**
   - What we know: P12 (DECRUFT-03) removed lcdui as a build/feature, but `swgClientUserInterface.vcxproj` include paths + `SwgCuiG15Lcd.cpp/.h` ClCompile entries + SwgGodClient editor lib dirs still carry `lcdui` strings (inert). The body of `SwgCuiG15Lcd.cpp` is already gutted to a removal comment.
   - What's unclear: Whether D-12's "re-grep ALL removed subsystems == 0" requires literal lcdui-zero (forcing cleanup of the P12 residue in Phase 15) or accepts the established P12-close baseline.
   - Recommendation: Since Phase 15 OWNS the milestone-closing gate, fold the lcdui-residue cleanup into the D-12 sweep wave (opportunistic, deletions-only, vestigial `.vcxproj` edits — no build risk). Confirm with the user at plan/discuss time if scope-strictness matters.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSBuild (VS 2026/v145) | Build gate (D-13) | ✓ (NOT on PATH — use full VS path per memory; `/nodeReuse:false`; run from PowerShell not Git Bash; delete target exe to force relink) | v145 toolset, stdcpp20 | — |
| `swg.sln` | Build target | ✓ | 128+ projects | — |
| SWGSource VM (192.168.1.200) | Boot gate (D-14) | ✓ (user-run) | StellaBellum/v3.0 | — |
| `ripgrep` | grep-zero gates | ✓ (via Grep tool) | — | — |
| XPCOM/Mozilla SDK | n/a — being REMOVED | ✓ (vendored `libMozilla/`, 1,866 files) | — | — |
| `SWGTCG.dll` | TCG navigate-callback consumer | ✗ (absent on SWGSource VM) | — | n/a — confirms TCG web path is dead; severing it is a no-op |

**Missing dependencies with no fallback:** None.
**Missing dependencies with fallback:** None — this is an in-repo removal; the only "dependency" being acted on is the thing being deleted.
**Build-env gotchas (apply):** debug exe reads `client_d.cfg` (set `rasterMajor` there); leave Koogie's uncommitted `Direct3d9.cpp` working-tree changes untouched; SwgClient Optimized fails LNK1281 SAFESEH pre-existing (gate Debug+Release only).

## Validation Architecture

> nyquist_validation is enabled (config.json `workflow.nyquist_validation: true`). This phase has NO unit-test framework — validation is via grep-zero gates, the Debug+Release link-grep gate, and the human-run dual-renderer boot gate (matching Phases 12/13/14). This is a removal phase: frame validation around removal-CORRECTNESS (build clean, grep zero, enum-ordinal safe, callers de-wired, boots both renderers), NOT feature behavior.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None (C++ engine removal; no unit-test harness in tree) |
| Config file | none — validation is grep + build-log + manual boot |
| Quick run command | `rg -i "SwgCuiWebBrowser\|libMozilla\|TUIWebBrowser\|\bxpcom\b\|\bxul\b\|nspr4\|plc4\|profdirserviceprovider\|nsIWebBrowser" src --glob '!*.filters'` (expect 0 after vendored-tree delete) |
| Full suite command | MSBuild Debug + Release, then grep each link log for `unresolved external symbol` (== 0) |

### Phase Requirements → Test Map (Success Criteria → observable validation signals)
| Crit | Behavior | Test Type | Automated Command / Gate | Gate kind |
|------|----------|-----------|--------------------------|-----------|
| #1a | Zero XPCOM/Mozilla refs across source + include paths + `.rsp` + `.sln` | grep-zero | `rg -i "libMozilla\|\bxpcom\b\|\bxul\b\|[Mm]ozilla\|nsIWebBrowser" src` → 0 (after vendored-tree delete) | grep |
| #1b | `libMozilla.vcxproj` dropped from `swg.sln`; GUID `{C6C1E14A…}` absent | grep-zero | `rg "C6C1E14A-DEDB-48C6-B028-0C9E4EFF5DC4\|libMozilla" src/build/win32/swg.sln` → 0 | grep |
| #2a | `SwgCuiWebBrowser*` + `TUIWebBrowser` symbol set deleted | grep-zero | `rg "SwgCuiWebBrowser\|TUIWebBrowser\|WebBrowserManager" src` → 0 | grep |
| #2b | No Mozilla DLL on any POST_BUILD/stage list | grep-zero | confirmed none present (D-10) — `rg -i mozilla` over `*.bat/*.cmd/*.ps1` + PostBuildEvent → 0 | grep |
| #2c | `UITypeID` ordinal safe — no enum-shift regression | reasoning + boot | D-02 verdict (ordinal never serialized) + radial-menu/HUD load under boot gate | analysis + human |
| #3 | swg.sln builds clean Debug AND Release with browser gone | link-grep | MSBuild Debug + Release; grep each link log: `unresolved external symbol` count == 0 (NOT exit 0 — `/FORCE` masks). Delete target exe first. | build |
| #4 | Milestone-wide residue == 0 (P12-P15 subsystems) with KEEP-list applied | grep-zero | per-token `rg` with KEEP-list exclusions (see Milestone Residue Sweep) → 0 | grep |
| #5 | Boots to char-select under rasterMajor=5 AND =11 vs SWGSource VM after FULL removal | manual boot | set `rasterMajor` in `stage/client_d.cfg`; launch `SwgClient_d.exe`; reach char-select both renderers; no crash/assert; no browser surface | human |

### Grep-zero assertion specifics (which tokens, which scopes)
- **XPCOM-family (Phase 15), scope = `src` excluding `*.filters`:** `libMozilla`, `\bxpcom\b`, `\bxul\b`, `[Mm]ozilla`, `nsIWebBrowser`, `nspr4`, `plc4`, `profdirserviceprovider`, `SwgCuiWebBrowser`, `TUIWebBrowser`, `WebBrowserManager` → all 0 after vendored-tree delete.
- **`.sln` scope:** GUID `{C6C1E14A-DEDB-48C6-B028-0C9E4EFF5DC4}` + literal `libMozilla` → 0.
- **Milestone scope (Phase 12-15), scope = `src` with KEEP-list:** see Milestone Residue Sweep — exclude `soePlatform/ChatAPI2/`, `soePlatform/libs/*VChat*`/`*vchat*`, removal-comment lines, `checkTrackIr` UI label, and (per A1 decision) optionally the lcdui residue.

### Link-grep gate (D-13)
- Build Debug + Release via MSBuild (full VS path, `/nodeReuse:false`, from PowerShell). Delete `SwgClient_d.exe`/`SwgClient_r.exe` first to force a relink.
- Grep each link log: `unresolved external symbol` count **== 0**. MSBuild exit 0 is NOT sufficient (`/FORCE`+`/VERBOSE` masks LNK2019).
- **Optimized is EXEMPT** (DEF-14-01 pre-existing SAFESEH LNK1281) — validate Debug+Release only.

### Dual-renderer boot gate (D-14)
- Set `rasterMajor=5` in `stage/client_d.cfg` → launch `SwgClient_d.exe` → reach character select (D3D9). Then `rasterMajor=11` → relaunch → reach character select (D3D11).
- Pass condition: char-select reached under both, no crash/assert, no browser surface, HUD + radial menu + IME focus still work (backstop for D-02 ordinal-shift + the 5 `IsA` de-wires).

### Sampling Rate
- **Per task commit:** quick grep-zero for the symbols touched by that task + per-library MSBuild (link-grep).
- **Per wave merge:** full XPCOM grep-zero + Debug+Release link-grep.
- **Phase gate (milestone close):** full milestone residue grep-zero (criterion #4, with KEEP-list) + Debug+Release link clean (criterion #3) + dual-renderer boot (criterion #5, human-confirmed) before `/gsd-verify-work`.

### Wave 0 Gaps
- None — no test infrastructure to create. The grep-zero, link-grep, and boot gates are the established Phase 12/13/14 validation pattern. A one-line `rg` per criterion (with the KEEP-list exclusions) suffices — no fixtures needed.

## Security Domain

> `security_enforcement: true`, `security_asvs_level: 1`. This is a code/build REMOVAL phase — it deletes a vendored third-party browser engine (XPCOM/Mozilla SDK, 1,866 files) and its bridge. No new attack surface is added; surface is strictly REDUCED.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No auth code touched (TCG session/login flow stays; only the browser tie is severed) |
| V3 Session Management | no | No session code touched |
| V4 Access Control | no | No access-control change |
| V5 Input Validation | marginal (net-positive) | Removing the WebBrowser widget eliminates an HTML/URL-rendering surface (`setURL`, `createWebBrowserPage`) and the XPCOM SDK — fewer untrusted-content parsers in the binary |
| V6 Cryptography | no | No crypto code touched (`crypto.lib`, `989crypt.lib`, `nspr4`/`plc4` are removed only as Mozilla deps; no in-house crypto changes) |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Vendored embedded browser engine (XPCOM/Gecko) with latent CVEs left on disk | Tampering / supply-chain | D-09 deletes the entire `libMozilla/` tree (1,866 files incl. XPCOM `.xpt` components) — eliminates a dormant, unpatched browser engine from the tree (strongly positive) |
| Dead URL-navigation path reachable via TCG callback | Spoofing / open-redirect (latent) | D-04 severs the `navigateProc`→`setURL` tie entirely; the browser can no longer be driven by TCG-supplied URLs (positive) |
| `/FORCE` masking an unresolved symbol that hides a partially-removed bridge | Tampering (silent broken binary) | D-13 link-grep gate (0 `unresolved external symbol`) is the control |

**Security posture:** This phase strictly reduces attack surface (deletes an embedded browser engine + its URL-navigation bridge). No security control is weakened. The only security-relevant gate is D-13 (don't ship a `/FORCE`-masked broken binary).

## Sources

### Primary (HIGH confidence — in-repo, verified this session)
- `src/external/3rd/library/ui/src/shared/core/UITypeID.h:13-128` (enum, `TUIWebBrowser:64`); `src/win32/UIBaseObject.h:78,82-83,142-143`; `src/win32/UIWidget.h:203,213`; `src/shared/loader/UIStandardLoader.h:18-24`; `src/shared/loader/UILoaderSetup.cpp:89-131`; `src/win32/UILoader.cpp:76-95` (D-02 string-keyed loader proof); `src/shared/core/UIPalette.cpp:36,347` (D-02 in-memory-only usage)
- `src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiWebBrowserManager.cpp/.h`; `src/shared/page/SwgCuiWebBrowserWidget.cpp/.h:47`; `SwgCuiWebBrowserWindow.cpp/.h`
- `swgClientUserInterface.vcxproj:228,850,851,926,1146,1147` (WebBrowser ClCompile/ClInclude); `:73,111,148` (lcdui include-path residue); `:450,1004` (SwgCuiG15Lcd)
- `SwgCuiManager.cpp:72,474,503,551`; `SwgCuiCommandParserUI.cpp:58,119,156,214,563-584(commented),1094-1096`; `SwgCuiHud.cpp:98,1026`; `SwgCuiHudAction.cpp:109,1191-1192` (+ sibling-Browser scope guard at 86/99/173-528)
- `CuiIoWin.cpp:513,647`; `IMEManager.cpp:353` (engine, both renderers)
- `SwgCuiTcgManager.cpp:19,33-34,68-69,124-150`; `SwgCuiTcgControl.h:91-94`
- `src/external/3rd/library/libEverQuestTCG/src/win32/libEverQuestTCG.cpp:20-23,144-195,199-209,270-278` (D-04a null-tolerance proof)
- `clientGame/src/shared/core/Game.cpp:146-148` (dead include, 0 `libMozilla::` calls); `clientGame/build/win32/includePaths.rsp:4`; `clientGame.vcxproj` (3 inline libMozilla); `clientUserInterface.vcxproj` (0 inline libMozilla)
- `SwgClient.vcxproj:103,105` (Debug), `:158,160` (Optimized), `:204,206` (Release), `:110,165,211` (SAFESEH); `libraries.rsp:10-14` (vestigial)
- `swg.sln:1587-1588` (project), `:2350-2355` (config-platform), `:25,116,251,386,452,726,835,938,1220` (9 ProjectDependency back-refs)
- `libMozilla.vcxproj:24,30,36` (StaticLibrary); `src/external/3rd/library/libMozilla/` (1,866 files, `.xpt` components)
- 7 editor `.rsp` + `.vcxproj` (AnimationEditor/ClientEffectEditor/LightningEditor/NpcEditor/ParticleEditor/SwooshEditor/Viewer) + SwgGodClient (`.rsp` + inline `:76,99,101,121,143,145,163,185,187`)
- D-10: `SwgClient.vcxproj` PostBuildEvent (exe/pdb only); `soePlatform/copy-libs.bat` (no Mozilla); no `stage/mozilla/`
- D-12 KEEP-list: `soePlatform/ChatAPI2/…/ChatRoom.cpp/.h`/`ChatRoomCore.cpp` (getVoice*); `soePlatform/libs/Win32-{Debug,Release}/{VChatAPI,Base_vchat}.lib`; `ClientHeadTracking.cpp:15-23`, `SwgCuiG15Lcd.cpp:26`, `SwgCuiOptControls.cpp:196` (removal comments + UI label)

### Secondary (MEDIUM)
- `.planning/phases/14-voice-chat-vivox-source-removal/14-RESEARCH.md` + `14-CONTEXT.md` — the closest analog (CR-01 correction shape, /FORCE gate, editor purge, atomic-delete)
- `.planning/phases/13-videocapture-library-unlink/13-CONTEXT.md` — inline-`.vcxproj`-vs-`.rsp`, vendored-tree delete, link-grep precedent
- `.planning/STATE.md` Accumulated Context — /FORCE false-pass, inline-`.vcxproj`, MSYS-sed gotcha, CR-01 lesson (line 104), libMozilla-stubbed (line 82)
- Memory `project_decruft_removal_build_graph_gotchas`, `project_radial_menu_enum_ordinal_is_datatable_row_index`, `feedback_debug_exe_reads_client_d_cfg`, `project_optimized_config_safeseh_pre_existing`, `feedback_dont_modify_koogie_changes`

### Tertiary (LOW)
- None — all claims verified in-repo this session.

## Metadata

**Confidence breakdown:**
- Removal Map (file/ref enumeration): HIGH — fresh repo-wide ripgrep + per-file reads; CONTEXT baseline confirmed accurate with line-precise refinements (CommandParserUI handler-already-commented, exact `.sln` dependency back-refs, per-config link-dir tokens).
- D-02 (UITypeID ordinal NOT serialized → safe outright delete): HIGH — traced the full UI type system: enum → IsA RTTI → string-keyed UILoader → no WebBrowser loader/TypeName. Definitive.
- D-04a (TCG callback null-tolerance → safe to unregister): HIGH — read the libEverQuestTCG static-init CallbackTable + setter assignments + confirmed the consumer DLL is absent.
- D-08 (per-config link tokens): HIGH — read all three `<Link>` blocks verbatim; corrected the CONTEXT lib-dir token names.
- D-10 (no Mozilla DLL staging): HIGH — PostBuildEvent + copy-libs + stage/ all checked.
- D-12 (milestone residue + KEEP-list): HIGH for the KEEP-list facts; MEDIUM for the lcdui-scope interpretation (flagged A1/Open Q1).

**Research date:** 2026-05-26
**Valid until:** Stable until the tree changes — this is an in-repo snapshot. Re-grep if other phases edit these libraries before Phase 15 executes. (Koogie's uncommitted `Direct3d9.cpp` working-tree changes are unrelated and untouched.)

## RESEARCH COMPLETE

**Phase:** 15 - In-Game Browser (XPCOM/Mozilla) Removal & Milestone Gate
**Confidence:** HIGH

### Key Findings
- **D-02 RESOLVED — SAFE-TO-DELETE-OUTRIGHT (not D-02a).** The `UITypeID` integer ordinal is never serialized; the `.ui` loader is string-keyed (`UILoader::CreateObject(name)` → string `mConstructorMap`), and there is NO WebBrowser loader/`TypeName` in the `ui` lib at all. Deleting `TUIWebBrowser` from `UITypeID.h:64` is safe. This is structurally UNLIKE CR-01 (where the enum ordinal WAS a retail-TRE row index).
- **D-04a RESOLVED — SAFE to unregister entirely (no fallback).** `libEverQuestTCG` stores navigate callbacks in a static-init null `CallbackTable`, only read by the absent `SWGTCG.dll`. Removing the registrations (`SwgCuiTcgManager.cpp:68-69`) leaves them at default-null — identical to never setting them.
- **D-08 — exact per-config link tokens delivered** (Debug: 5 XPCOM libs, NO `libMozilla.lib`; Optimized: 5+duplicated tokens + `libMozilla.lib`; Release: 5 + `libMozilla.lib`), with a correction to the CONTEXT lib-dir token names. Optimized exempt from the gate (DEF-14-01).
- **D-07 — `swg.sln` has 11 locations**, not just the project block: project (1587-1588) + 6 config-platform (2350-2355) + **9 ProjectDependency back-refs** (incl. SwgClient at 726, the real build-order edge).
- **D-12 — the milestone sweep needs a precise KEEP-list:** prior-phase tokens are NOT literally zero (removal-provenance comments, `checkTrackIr` UI label, `soePlatform/ChatAPI2` getVoice*, prebuilt VChat libs, and a leftover P12 `lcdui` residue in `swgClientUserInterface.vcxproj` — flagged A1 for plan-time scope decision).
- **D-01/D-05/D-06/D-09/D-10/D-11 — CONTEXT baseline CONFIRMED accurate** with line-precise refinements (notably: the `SwgCuiCommandParserUI` `browser` handler block at 563-584 is ALREADY commented out; the live refs are the table row 214 + handler 1094-1096). No file named `UIWebBrowserWidget` exists. No Mozilla DLL staging.

### File Created
`.planning/phases/15-in-game-browser-xpcom-mozilla-removal-milestone-gate/15-RESEARCH.md`

### Confidence Assessment
| Area | Level | Reason |
|------|-------|--------|
| Removal Map | HIGH | fresh repo-wide grep + per-file reads; baseline confirmed |
| D-02 enum-ordinal safety | HIGH | full UI type-system trace; definitive SAFE verdict |
| D-04a TCG callback | HIGH | read libEverQuestTCG source; static-null + absent DLL |
| D-08 link tokens | HIGH | all 3 `<Link>` blocks read verbatim |
| D-12 milestone sweep | HIGH (facts) / MEDIUM (lcdui scope) | KEEP-list verified; lcdui-scope flagged A1 |

### Open Questions
1. lcdui P12-residue scope in the D-12 milestone sweep (A1 / Open Q1) — recommend folding the deletions-only cleanup into the milestone wave; confirm at plan/discuss time if scope-strictness matters.

### Ready for Planning
Research complete. Every must_verify item resolved with a verdict + evidence. The planner can derive Wave 1 (atomic de-wire + delete), Wave 1-parallel (residue/editor purge), Wave 2 (sln-drop + vendored-tree delete), and Wave 3 (milestone gate) directly from the Symbol-Resolution Analysis + Authoritative Removal Map, and the VALIDATION.md from the Validation Architecture section.
