# Phase 14: Voice Chat (Vivox) Source Removal - Research

**Researched:** 2026-05-26
**Domain:** Live-UI source/build decruft — removing the Vivox voice-chat subsystem from the Koogie MSBuild tree (`src/build/win32/swg.sln`) without breaking surviving caller surfaces or the dual-renderer boot.
**Confidence:** HIGH (all 7 CONTEXT-assigned verification tasks confirmed with file:line evidence via repo-wide ripgrep + source reads; no external/library lookups needed — this is an in-repo removal phase)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Remove the WHOLE voice subsystem, not just the 3 DECRUFT-05-named files. Researcher re-greps to enumerate the full file list (done — see Authoritative Removal Map; the baseline MISSED several files, flagged below).
- **D-02:** De-wire by deleting each call site — no stub. Edit each surviving caller to remove its voice lines; do NOT leave a no-op `CuiVoiceChatManager` shim (fails criterion #2's grep-zero). Callers that STAY and must be edited in place: `CuiManagerManager.cpp`, `CuiRadialMenuManager.cpp`, `CommandCppFuncs.cpp` (funcs + table bindings), `ClientCommandQueue.cpp` + `CustomerServiceManager.cpp` (CS-report append), `SwgCuiHudAction.cpp` / `SwgCuiStatusGround.cpp` (voice hooks).
- **D-02a:** Removal must keep the tree buildable at each coherent step (no mid-step un-buildable state). Exact edit order is the planner's call, provided the link gate stays green per D-09.
- **D-03:** Full strip of all 6 voice preference keys from `CuiPreferences` — the bools, their `REGISTER_OPTION_USER(...)` registrations, all getters/setters, the `CuiVoiceChatManager::setVoice*(false)` default-disable block, and the `CuiVoiceChatManager.h` include.
- **D-03a:** Safe — verified during discussion that no non-voice callers of the voice getters/setters exist (NOTE: research found one the discussion missed — `SwgCuiHudWindowManager.cpp:318`; see below). Researcher confirms `CurrentUserOptionManager` unregistered-key tolerance (CONFIRMED — inert).
- **D-04:** Delete the vendored SDK trees `src/external/3rd/library/vivox/` AND `src/external/3rd/library/vivoxSharedWrapper/` entirely.
- **D-05:** Purge the dangling `swgClientVivox` references (dir does not exist on disk — confirmed). Refs in `clientUserInterface` includePaths.rsp, `clientGame` includePaths.rsp, and matching inline `.vcxproj` AdditionalIncludeDirectories.
- **D-06:** Chase ALL in-repo source registrations of voice symbols: `SwgCuiMediatorFactorySetup.cpp` ctor registrations + includes; `CommandCppFuncs.cpp` command-table bindings; `CuiMenuInfoTypes.cpp`/`.h` `VOICE_INVITE` enum + `MAKE_ID(voice_invite)`.
- **D-06a:** Compiled TRE assets are out-of-reach and accepted as inert. Researcher verifies graceful no-op; the boot gate is the backstop. "Chase the data" resolves to D-06's source-level registrations — there is no in-repo datatable file to scrub.
- **D-07:** Purge the 8 editor `.rsp` vivox references (AnimationEditor, ClientEffectEditor, LightningEditor, NpcEditor, ParticleEditor, SwooshEditor, Viewer, SwgGodClient). Editors are out-of-scope as build targets but their stale refs must still go.
- **D-08:** Dual-renderer boot gate after removal — boots to character select under BOTH `rasterMajor=5` (D3D9) and `=11` (D3D11). Set `rasterMajor` in `client_d.cfg` (the debug exe reads `client_d.cfg`, not `client.cfg`).
- **D-09:** Build gate greps link output for 0 `unresolved external symbol` — NOT just MSBuild exit 0. `/FORCE` downgrades unresolved externals to warnings and still emits a binary. Debug AND Release must both link clean. Edit inline `.vcxproj` compile/link lists, not just the (often vestigial) `.rsp` files.

### Claude's Discretion
- Exact edit order, plan/wave breakdown, and commit granularity within the removal — planner's call, provided every coherent step leaves the tree buildable (D-02a) and the link gate green (D-09). The Phase 13 shape (atomic link-unit removal in Wave 1, residue/path purge in Wave 1 parallel, vendored-tree delete sequenced last in Wave 2) is a reasonable template.

### Deferred Ideas (OUT OF SCOPE)
- **In-Game Browser (XPCOM/Mozilla) removal** — that's Phase 15 (DECRUFT-06).
- **Bink video codec / Miles audio playback** — explicitly OUT of v2.1 scope (active middleware). Do not conflate voice removal with the live audio path; Miles `AIL_*` playback is untouched.
- **"Diff SWGSource vs whitengold TRE"** / **"Cantina corner-snap"** — deferred (v2.2 / pre-existing quirk).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| DECRUFT-05 | Voice chat (Vivox) fully removed from source and build — `vivoxSharedWrapper_debug.lib` unlinked; `CuiVoiceChatManager.cpp/.h`, `SwgCuiVoiceFlyBar.cpp/.h`, `CuiVoiceChatEventHandler` deleted; voice preference keys stripped from `CuiPreferences`; client builds + boots | Authoritative Removal Map (below) lists all ~24 files to delete, all 9 live call/registration sites to de-wire, all link-lib + include-path + .rsp refs to purge, with file:line evidence. Symbol-resolution analysis (below) gives the minimal coherent change-set ordering. |
| DECRUFT-07 | Client compiles clean + boots to char-select under BOTH `rasterMajor=5` and `=11` after removal (cross-cutting milestone gate, verified incrementally) | Validation Architecture (below) defines the grep-zero gates, the Debug+Release link-grep gate, and the dual-renderer boot gate. |
</phase_requirements>

## Summary

Phase 14 removes the Vivox voice-chat subsystem from the active MSBuild tree. The fresh repo-wide grep confirms the subsystem is **~3× larger than the 3 DECRUFT-05-named files** and **larger than the CONTEXT `<code_context>` scouted baseline** — the baseline missed several real references that this research has now enumerated authoritatively. The removal touches three engine/game libraries (`clientUserInterface`, `clientGame`, `swgClientUserInterface`), one shared library (`sharedNetworkMessages`, which compiles three voice-only network-message files), the `SwgClient` application link, eight editor `.rsp` files, and two vendored SDK trees. No working user feature is lost: voice is off-by-default with no Vivox backend on the SWGSource VM.

The single most important build finding (confirming the recurring Phase 12/13 lesson): **the authoritative link wiring is inline in `SwgClient.vcxproj`'s `<AdditionalDependencies>` and `<AdditionalLibraryDirectories>`, NOT in the `.rsp` files**. `SwgClient.vcxproj` does not reference its `.rsp` files at all — they are fully vestigial. The vivox libs (`vivoxSharedWrapper_debug.lib`, `vivoxsdk.lib`, `vivoxplatform.lib`, `VChatAPI.lib`, `Base_vchat.lib`, `libsndfile-1.lib`) appear inline in all three SwgClient configs (Debug/Optimized/Release) AND in the vestigial `.rsp` files. Criterion #1's grep-zero forces purging both.

The symbol-resolution chain is clean and well-bounded: `CuiVoiceChatManager.cpp` is the **only** consumer of the `SwgVivox` singleton (from `vivoxSharedWrapper`) AND the only consumer of the three `voicechat/` network messages AND the only thing that pulls in `VChatAPI.lib`/`Base_vchat.lib` transitively. When `CuiVoiceChatManager.cpp` is deleted and its callers de-wired, every vivox/VChat lib becomes orphaned and can be unlinked from `SwgClient.vcxproj` in the same coherent change. There is no game server in this repo, and nothing server-side consumes the voice symbols.

**Primary recommendation:** Mirror the Phase 13 shape exactly. Wave 1: atomically delete the voice link-unit (the ~24 source files + their `.vcxproj` ClCompile/ClInclude entries) AND de-wire all 9 live call/registration sites AND strip the `CuiPreferences` keys AND remove the inline `.vcxproj` link libs/paths — as ONE coherent buildable change per library, because the symbols cross-reference. Wave 1 parallel: purge all residue refs (vestigial `.rsp` libs/paths, includePaths.rsp, dangling `swgClientVivox`, 8 editor `.rsp`). Wave 2: delete the two vendored trees (`vivox/`, `vivoxSharedWrapper/`) last. Gate every step with the Debug+Release link-grep (0 `unresolved external symbol`) and close with the dual-renderer boot gate.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Voice manager lifecycle + Vivox SDK glue | Engine — `clientUserInterface` | — | `CuiVoiceChatManager`/`CuiVoiceChatEventHandler`/`CuiVoiceChatGlue` own the `SwgVivox` singleton and the message processors; shared by both renderers |
| Voice UI pages (fly bar, active speakers, options tab, parser) | Game UI — `swgClientUserInterface` | — | `SwgCuiVoice*`, `SwgCuiOptVoice`, `SwgCuiCommandParserVoice` are CUI mediators/parsers |
| Voice command funcs + CS-report glue | Engine — `clientGame` | — | `CommandCppFuncs`, `ClientCommandQueue`, `CustomerServiceManager` call into the manager |
| Voice network messages (channel info / account / misc) | Shared — `sharedNetworkMessages` | — | `voicechat/*.cpp` compiled into `sharedNetworkMessages.lib`; consumed ONLY by `CuiVoiceChatManager` |
| Voice link libs (Vivox SDK, VChat backend) | Application link — `SwgClient` (+ `SwgGodClient` editor) | — | Inline `.vcxproj` `<AdditionalDependencies>` + vestigial `.rsp`; prebuilt `.lib`s, NOT swg.sln projects |
| Radial-menu type / mediator-factory / action registration | Engine + Game UI | — | `CuiMenuInfoTypes` (`VOICE_INVITE`), `SwgCuiMediatorFactorySetup`, `SwgCuiMediatorTypes.h`, `SwgCuiActions.h` |
| Voice preference keys | Engine — `clientUserInterface` (`CuiPreferences`) | — | 6 bools + `REGISTER_OPTION_USER` + getters/setters; persisted via `CurrentUserOptionManager`→`OptionManager` |

**Tier-correctness note for the planner:** Nothing in this phase belongs in a renderer tier (`Direct3d9`/`gl11`). The blast radius is `clientUserInterface` + `clientGame` + `swgClientUserInterface` + `sharedNetworkMessages` + the `SwgClient` link — all renderer-agnostic, which is exactly why both renderers must re-link and re-boot clean.

## Authoritative Removal Map (D-01 — re-grepped, supersedes the CONTEXT baseline)

> **D-01 verification result: the CONTEXT `<code_context>` baseline is INCOMPLETE.** The fresh grep (`[Vv]ivox` + `CuiVoiceChat|SwgCuiVoice|SwgCuiOptVoice|CommandParserVoice|VOICE_INVITE|voiceInvite|voiceKick|voice_invite|swgClientVivox|ms_voice|getVoice|setVoice|VoiceChat|VoiceFlyBar|VoiceActiveSpeakers|VChat`) surfaced files/refs the baseline did not list. New items are flagged **[BASELINE MISS]**.

### A. Source files to DELETE (full delete — D-01)

**`clientUserInterface` engine (manager + handler + glue):**
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiVoiceChatManager.cpp` + `.h`
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiVoiceChatEventHandler.cpp` + `.h`
- `src/engine/client/library/clientUserInterface/src/shared/core/CuiVoiceChatGlue.h`
- `src/engine/client/library/clientUserInterface/include/public/clientUserInterface/CuiVoiceChatManager.h` (public re-include)
- → Remove ClCompile/ClInclude entries at `clientUserInterface.vcxproj:292,293,484,485,486`

**`swgClientUserInterface` game UI (pages + parser):**
- `.../src/shared/page/SwgCuiVoiceFlyBar.cpp` + `.h`
- `.../src/shared/page/SwgCuiVoiceFlyBarMessageQueue.cpp` + `.h`
- `.../src/shared/page/SwgCuiVoiceActiveSpeakers.cpp` + `.h`
- `.../src/shared/page/SwgCuiVoiceActiveSpeakers_TableModel.cpp` + `.h`
- `.../src/shared/page/SwgCuiOptVoice.cpp` + `.h`
- `.../src/shared/parser/SwgCuiCommandParserVoice.cpp` + `.h`
- Public re-includes: `include/public/swgClientUserInterface/{SwgCuiVoiceFlyBar.h, SwgCuiVoiceActiveSpeakers.h, SwgCuiVoiceActiveSpeakers_TableModel.h, SwgCuiOptVoice.h, SwgCuiCommandParserVoice.h}`
- → Remove ClCompile/ClInclude entries at `swgClientUserInterface.vcxproj:650,842,843,844,845,905,1082,1150,1151,1152,1153,1175`

**`sharedNetworkMessages` shared lib (voice-only network messages) — [BASELINE MISS]:**
- `src/engine/shared/library/sharedNetworkMessages/src/shared/voicechat/VoiceChatChannelInfo.cpp` + `.h`
- `.../voicechat/VoiceChatMiscMessages.cpp` + `.h`
- `.../voicechat/VoiceChatOnGetAccount.cpp` + `.h`
- Public re-includes: `include/public/sharedNetworkMessages/{VoiceChatChannelInfo.h, VoiceChatMiscMessages.h, VoiceChatOnGetAccount.h}` (confirmed re-include shims — `#include "../../src/shared/voicechat/..."`)
- → Remove ClCompile/ClInclude entries at `sharedNetworkMessages.vcxproj:1178,1179,1180,1545,1546,1547`
- **Consumer = ONLY `CuiVoiceChatManager.cpp` (lines 27-29, 190-191, 218-219, 245-246, 274-356)** — confirmed by grep across `src` excluding the voice files: zero other consumers. No server tree consumes them (server tree is editor tools only, no game server). **Recommendation: in-scope to delete** (criterion #1 forbids any residual `VoiceChat` symbol; these are voice-only and orphaned by the manager's deletion).

### B. Live call sites to DE-WIRE in place (callers STAY — D-02 / D-06)

| File:line | Voice ref | Edit | Notes |
|-----------|-----------|------|-------|
| `CuiManagerManager.cpp:57,105,156,210` | include + `install/remove/update` | delete 4 lines | UI-manager lifecycle |
| `CuiRadialMenuManager.cpp:60,1182,1184` | include + `isLoggedIn()`-guarded `addRootMenu(VOICE_INVITE,…)` | delete include + the guarded block | entry never appears today (isLoggedIn always false) |
| `CommandCppFuncs.cpp:57,267,268,2422-2446,2448-2466,2659,2660` | include + `commandFuncVoiceInvite/Kick` decl+defn + 2 `addCppFunction` bindings | delete all | D-02 + D-06 |
| `ClientCommandQueue.cpp:44,696` | include + `getCsReportString()` append | delete 2 lines | CS-report |
| `CustomerServiceManager.cpp:22,865-866` | include + `getCsReportString()` append | delete include + the 2-line append | CS-report |
| `SwgCuiHudAction.cpp:110,286` | include `SwgCuiVoiceFlyBar.h` + LIVE `addAction(toggleVoiceFlyBar,…)` | delete include + line 286 | dispatch at 1598-1605 already commented; line 286 is LIVE |
| `SwgCuiStatusGround.cpp:36,1196,2040-2051` | include + LIVE `updateVoiceChatIcon()` method + its call site | delete include, the call at :1196, and the method defn :2040-2051 | also remove decl in `SwgCuiStatusGround.h` |
| `SwgCuiHudWindowManager.cpp:318-319` **[BASELINE MISS]** | LIVE `if (CuiPreferences::getVoiceShowFlybar()) CuiMediatorFactory::activateInWorkspace(WS_VoiceFlyBar)` | delete both lines | **non-voice surface calling a voice getter AND a voice mediator type — both vanish under D-03/D-06; this caller MUST be de-wired or it won't compile** |
| `SwgCuiCommandParserDefault.cpp:77,442` | include + already-commented `//addSubCommand(new SwgCuiCommandParserVoice)` | delete include (line 442 already commented) | include is the only live ref |
| `SwgCuiOpt.cpp:32,54,112-114` | include `SwgCuiOptVoice.h` (lines 54,113,114 already commented) | delete include only | the OptVoice page tab is already disabled in source |

### C. In-repo registration sites (D-06)

| File:line | Registration | Edit |
|-----------|-------------|------|
| `SwgCuiMediatorFactorySetup.cpp:132,133,258,259` | 2 voice includes + `MAKE_SWG_CTOR_WS(VoiceFlyBar,"/Voice.VoiceFlyBar")` / `(VoiceActiveSpeakers,"/Voice.VoiceActiveSpeakers")` | delete 2 includes + 2 ctor lines |
| `CuiMenuInfoTypes.cpp:308` | `MAKE_ID(VOICE_INVITE, none, voice_invite, 0)` | delete line |
| `CuiMenuInfoTypes.h:165` | `VOICE_INVITE,` enum value | delete line |
| `SwgCuiMediatorTypes.h:104,105` **[BASELINE MISS]** | `MAKE_MEDIATOR_TYPE(WS_VoiceFlyBar)` / `(WS_VoiceActiveSpeakers)` | delete 2 lines (referenced by SwgCuiHudWindowManager + the factory + SwgCuiHudAction) |
| `SwgCuiActions.h:69,70` **[BASELINE MISS]** | `MAKE_ACTION(toggleVoiceFlyBar)` / `(toggleVoiceActiveSpeakers)` | delete 2 lines (referenced only by SwgCuiHudAction.cpp:286 + commented lines) — header-only string consts, no .cpp defn |

### D. CuiPreferences voice keys (D-03)

`src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp`:
- Include: `:22` `#include "clientUserInterface/CuiVoiceChatManager.h"`
- Bools: `:391-396` (6 bools — note `ms_voiceUsePushToTalk` is `:392`, baseline said keys at 390-391; full set is **391-396**)
- Registrations: `:842-847` (6× `REGISTER_OPTION_USER(...)`)
- Default-disable block: `:869-873` — **4 calls** `CuiVoiceChatManager::setVoiceChatEnabled/setUsePushToTalkForceUpdate/setShowFlybar/setUseAdvancedChannelSelection(false)` (these call the DELETED class's setters — distinct from CuiPreferences' own setters; baseline said `:869-873` ✓ but described it as "setVoice* block" — confirm all 4 go)
- Getters/setters: `:3412-3491` (12 functions — 6 getters + 6 setters)

`CuiPreferences.h:538-554`: 12 declarations (6 get/6 set).

### E. Build / link refs to PURGE

**`SwgClient` (the only client EXE target) — AUTHORITATIVE = inline `.vcxproj`:**
- `SwgClient.vcxproj` `<AdditionalDependencies>` Debug (line 103), Optimized (158), Release (204): remove `Base_vchat.lib;...;VChatAPI.lib;...;vivoxplatform.lib;vivoxsdk.lib;vivoxSharedWrapper_Debug.lib;vivoxSharedWrapper_debug.lib` (and release variants `vivoxSharedWrapper_Release.lib;vivoxSharedWrapper_release.lib`) and `libsndfile-1.lib`
- `SwgClient.vcxproj` `<AdditionalLibraryDirectories>` Debug (105), Optimized (160), Release (206): remove `...\external\3rd\library\vivox\lib` and `...\vivoxSharedWrapper\lib` (and `soePlatform\libs\Win32-Debug`/`Win32-Release`/`Win32-Optimized` **only if** no surviving non-voice lib uses it — see Open Questions Q1)
- **Vestigial `.rsp` (purge for criterion #1 grep-zero only — NOT consumed by the build):** `libraries_d.rsp:2`, `libraries_r.rsp:2`, `libraries_o.rsp:2` (`vivoxSharedWrapper_*.lib`); `libraryPaths.rsp:9` (`vivoxSharedWrapper/lib`)

**`clientUserInterface.vcxproj` inline include dirs** (lines 73, 111, 148 — Debug/Optimized/Release): remove `...\external\3rd\library\vivox\include`, `...\vivoxSharedWrapper\include`, and `...\game\client\library\swgClientVivox\include\public`
**`clientUserInterface/includePaths.rsp`** (vestigial): remove vivox/vivoxSharedWrapper/swgClientVivox lines (`:51` swgClientVivox + the `:43-44`-equivalent vivox lines)

**`clientGame.vcxproj` inline include dirs** (lines 73, 111, 148): remove `...\external\3rd\library\vivox\include` + `...\swgClientVivox\include\public`
**`clientGame/includePaths.rsp`** (vestigial): remove vivox + `:55` swgClientVivox

### F. Dangling `swgClientVivox` (D-05) — dir CONFIRMED ABSENT on disk
Refs: `clientUserInterface/includePaths.rsp:51`; `clientGame/includePaths.rsp:55`; inline `clientUserInterface.vcxproj:73,111,148`; inline `clientGame.vcxproj:73,111,148`. Pure vestigial — purge all (also covered by E above).

### G. Editor `.rsp` (D-07) — 8 editors CONFIRMED, 16 line refs total

| Editor | libraries.rsp (lib) | libraryPaths.rsp (path) |
|--------|---------------------|--------------------------|
| AnimationEditor | `:17` vivoxSharedWrapper_release.lib | `:9` vivoxSharedWrapper\lib |
| ClientEffectEditor | `:18` | `:9` |
| LightningEditor | `:22` | `:10` |
| ParticleEditor | `:22` | `:10` |
| SwooshEditor | `:22` | `:10` |
| NpcEditor | `:22` | `:10` |
| Viewer | `:15` | `:8` (forward-slash style `../../../../../../external/3rd/library/vivoxSharedWrapper/lib`) |
| SwgGodClient | `:25` | `:10` |

**[BASELINE MISS for SwgGodClient inline]:** `SwgGodClient.vcxproj` also carries vivox/VChat INLINE in `<AdditionalDependencies>` (line 99 Debug: `Base_vchat.lib`, `VChatAPI.lib`, `vivoxsdk.lib`, `vivoxplatform.lib`, `vivoxSharedWrapper_Debug.lib`, `vivoxSharedWrapper_debug.lib`; lines 143/185: `vivoxSharedWrapper_release.lib`). Purge these inline refs too for criterion #1.

### H. Vendored SDK trees to DELETE (D-04)
- `src/external/3rd/library/vivox/` (subdirs: `include`, `lib`)
- `src/external/3rd/library/vivoxSharedWrapper/` (`Eq2Vivox.h`, `Vivox.cpp/.h/.inl`, `build`, `include`, `lib`)
- Confirmed: NO non-voice client source includes the vivox/vivoxSharedWrapper headers (grep across `src/engine`+`src/game` excluding voice files = 0).

## Symbol-Resolution Analysis (Verification Task 4 — drives wave/edit ordering)

**Where do `CuiVoiceChatManager::` symbols resolve?** In the deleted `.cpp`s (`CuiVoiceChatManager.cpp`/`CuiVoiceChatEventHandler.cpp` compiled into `clientUserInterface.lib`). They are NOT in `vivoxSharedWrapper_debug.lib`.

**Where does `vivoxSharedWrapper_debug.lib` resolve?** It defines the `Vivox<...>` template / `SwgVivox` singleton (`SwgVivox` = `typedef Vivox<std::string, SwgVivoxGlue>` per `CuiVoiceChatGlue.h:89`). `CuiVoiceChatManager.cpp:1881` calls `SwgVivox::getInstance().BeginConnect(...)` — so `CuiVoiceChatManager.cpp` is the **sole** consumer of the vivox SDK libs. `VChatAPI.lib`/`Base_vchat.lib`/`vivoxsdk.lib`/`vivoxplatform.lib`/`libsndfile-1.lib` are transitive deps of `vivoxSharedWrapper` with **no direct source consumer** (grep for `VChatAPI` outside soePlatform/build-files = 0).

**Minimal coherent change-set (what keeps the link clean at each step):**
1. The **dangerous direction**: removing the vivox link libs from `SwgClient.vcxproj` BEFORE deleting `CuiVoiceChatManager.cpp` → `CuiVoiceChatManager.obj` references `SwgVivox::*` → `unresolved external symbol` (masked by `/FORCE`). So **delete the manager source first (or in the same change) as removing the libs.**
2. The **other dangerous direction**: deleting `CuiVoiceChatManager.cpp` BEFORE de-wiring its callers (`CuiManagerManager`, `CuiRadialMenuManager`, `CommandCppFuncs`, `ClientCommandQueue`, `CustomerServiceManager`, `SwgCuiStatusGround`, `SwgCuiHudWindowManager`) → those `.obj`s reference `CuiVoiceChatManager::*` → unresolved. So **de-wire all callers in the same coherent change as deleting the manager.**
3. **Coherent unit:** Because the symbol graph is `callers → CuiVoiceChatManager → SwgVivox(libs)` and `CuiVoiceChatManager → voicechat-messages`, the single safe atomic change is: de-wire all callers + delete all voice source (manager, handler, glue, pages, parser, voicechat messages) + remove their `.vcxproj` ClCompile entries + remove the inline link libs from `SwgClient.vcxproj` — **all together**, per library. The `CuiPreferences` strip and registration-site edits belong in the same unit (they reference deleted symbols). Mirrors Phase 13 D-01 "atomic closed-chain delete."
4. **Residue purge (vestigial `.rsp`, includePaths, swgClientVivox, editor `.rsp`)** can run in parallel — it touches no symbols.
5. **Vendored-tree delete (D-04)** sequences LAST — once no `.vcxproj` includes `vivox\include`, the trees are safe to remove (deleting earlier would break compile if any include-dir purge was missed).

## D-03a Verification: CurrentUserOptionManager Unregistered-Key Tolerance (CONFIRMED INERT)

**Load path:** `CuiPreferences` `REGISTER_OPTION_USER(a)` → `CurrentUserOptionManager::registerOption(ms_a, KeyName, #a)` (`CuiPreferences.cpp:423`) → `OptionManager::registerOption` (`OptionManager.cpp:254-321`) which pushes to `m_registeredOptionList`. On load, `OptionManager::load` → `loadVersion` (`OptionManager.cpp:581-726`) reads EVERY persisted chunk into `m_savedOptionList` (the `default` case at `:713-717` even gracefully skips unknown TAG types via `iff.enterChunk()/exitChunk(true)`), then `copyOptionListIntersection(m_savedOptionList, m_registeredOptionList)` (`:735-781`) copies values **only for keys present in BOTH lists** (matched by name+section+type+version).

**Conclusion:** A persisted-but-unregistered key (e.g. a stale `voiceChatEnabled` in an existing per-user `options.cfg` after the strip) lands in `m_savedOptionList`, is never matched in the intersection, is **silently ignored** (no `DEBUG_FATAL`, no parse error, no crash), and is dropped on the next `save()` (which only writes `m_registeredOptionList`). **D-03a CONFIRMED — stripping the registrations is safe; orphaned persisted keys are fully inert.** Evidence: `OptionManager.cpp:447-487` (load), `:581-726` (loadVersion), `:735-781` (copyOptionListIntersection), `:491-577` (save writes registered list only).

## D-06a Verification: Orphaned TRE-Asset Graceful No-Op (CONFIRMED)

The compiled `.ui` page layouts (`/Voice.VoiceFlyBar`, `/Voice.VoiceActiveSpeakers`), command-name rows, and radial-menu datatable rows live in the user's retail TRE install, NOT in this source tree (no `dsrc/`/`data/` directory — confirmed). After removal:
- **Page lookups:** `CuiMediatorFactory::activateInWorkspace(WS_VoiceFlyBar)` is the only activator and is being deleted (`SwgCuiHudWindowManager.cpp:318-319`). With the mediator type unregistered, no code path requests the page. The factory's `MAKE_SWG_CTOR_WS` registration (`SwgCuiMediatorFactorySetup.cpp:258-259`) is also removed, so the engine never tries to construct a mediator for the orphaned page path. No lookup occurs → no-op.
- **Command names (`voiceInvite`/`voiceKick`):** removed from `CommandTable::addCppFunction` (`CommandCppFuncs.cpp:2659-2660`). The engine command-dispatch tolerates a command-name with no registered C++ func (it's a server-driven table; an unbound client name is a no-op). The retail TRE command-table rows referencing them simply find no client handler — inert.
- **Radial-menu entry (`VOICE_INVITE`):** the `addRootMenu(VOICE_INVITE,…)` call is removed (`CuiRadialMenuManager.cpp:1184`) and was already gated on `isLoggedIn()` (always false — no Vivox backend), so it never appeared. The enum value is removed (`CuiMenuInfoTypes.h:165`); orphaned radial rows in retail TRE referencing the old type id are ignored by the menu builder.
- **Backstop:** the dual-renderer boot gate (D-08) empirically confirms the options surface, HUD, and radial menu still load. **D-06a CONFIRMED — graceful no-op on all three orphaned-asset classes; boot gate is the backstop.**

## Inline `.vcxproj` vs `.rsp` (Verification Task 7 — AUTHORITATIVE WIRING)

**`SwgClient.vcxproj` does NOT reference any `.rsp` file** (no `@response`, no `.rsp` mention — grep = 0). The `.rsp` files (`libraries_d.rsp`, `libraryPaths.rsp`, etc.) are **fully vestigial** carryovers from the CMake era. The real link wiring is **inline** in `<AdditionalDependencies>`/`<AdditionalLibraryDirectories>` (3 configs).

| Project | vivox link libs | vivox/vivoxSharedWrapper include path | swgClientVivox include path |
|---------|-----------------|----------------------------------------|------------------------------|
| `SwgClient` | **INLINE `.vcxproj`** (lines 103/158/204) + vestigial `.rsp` | — (SwgClient doesn't include vivox headers) | — |
| `clientUserInterface` | n/a (engine lib, no link step) | **INLINE `.vcxproj`** (73/111/148) + vestigial `includePaths.rsp` | **INLINE `.vcxproj`** (73/111/148) + `includePaths.rsp:51` |
| `clientGame` | n/a | **INLINE `.vcxproj`** (73/111/148: vivox\include) + vestigial `includePaths.rsp` | **INLINE `.vcxproj`** (73/111/148) + `includePaths.rsp:55` |
| `swgClientUserInterface` | n/a | — (uses public re-includes of clientUserInterface) | — |
| `SwgGodClient` (editor) | **INLINE `.vcxproj`** (99/143/185) + `.rsp` | — | — |

**Planner directive:** Edit the INLINE `.vcxproj` for all link-lib and include-path removal. The `.rsp` edits are purely to satisfy criterion #1's literal grep-zero and have ZERO effect on the build.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Keeping `CuiVoiceChatManager` callable after lib removal | A no-op `CuiVoiceChatManager` stub | Delete the class + de-wire callers (D-02) | A stub keeps the symbol → fails criterion #2 grep-zero; Phase 12 lcdui used a stub only because callers stayed AND no grep-zero requirement existed |
| Verifying a clean link | `MSBuild exit 0` | Grep link output for `unresolved external symbol == 0`, Debug AND Release (D-09) | `/FORCE` + `/VERBOSE` (SwgClient.vcxproj:110) downgrades LNK2019 to warnings and still emits a binary at exit 0 — proven false-pass in Phases 12/13 |
| Purging `.vcxproj` path lists on Windows | `sed` escaping literal backslashes | Match by substring+delimiter `[^;<>]*TOKEN[^;<>]*;` | MSYS sed mis-parses `backslash+digit` (e.g. `3rd`) as a back-reference (STATE.md Phase 13 finding) |
| Scrubbing retail TRE assets | Editing compiled `.ui`/datatable rows | Accept inert + rely on graceful no-op + boot gate (D-06a) | No `dsrc/`/`data/` in this repo; TRE assets are out-of-reach and not redistributed |

**Key insight:** This is a closed symbol chain with a single root consumer (`CuiVoiceChatManager`). The atomic-delete pattern (Phase 13 D-01) is correct; the only added risk vs. Phase 13 is the larger live-caller count (10 call sites + 5 registration sites), so the "coherent unit" is bigger but the principle is identical.

## Common Pitfalls

### Pitfall 1: Editing only the `.rsp`, missing the inline `.vcxproj`
**What goes wrong:** Removing vivox from `libraries_d.rsp` but not `SwgClient.vcxproj:103` → the lib still links, criterion #1 still fails on `.vcxproj`, and (worse) if the source is deleted but the inline lib stays, you get an unresolved-externals false-pass under `/FORCE`.
**How to avoid:** Edit the inline `.vcxproj` as authoritative; treat `.rsp` as cosmetic grep-zero cleanup.
**Warning signs:** Build exit 0 but link log shows `unresolved external symbol "public: ... SwgVivox"`.

### Pitfall 2: Missing the [BASELINE MISS] callers/registrations
**What goes wrong:** `SwgCuiHudWindowManager.cpp:318` calls `CuiPreferences::getVoiceShowFlybar()` + `WS_VoiceFlyBar`; if the prefs key + mediator type are stripped (D-03/D-06) but this caller isn't de-wired, `swgClientUserInterface` won't compile. Same for `SwgCuiMediatorTypes.h:104-105` and `SwgCuiActions.h:69-70`.
**How to avoid:** Use the Authoritative Removal Map (sections B/C), not the CONTEXT baseline.
**Warning signs:** Compile error `getVoiceShowFlybar is not a member` / `WS_VoiceFlyBar undeclared`.

### Pitfall 3: Deleting the vendored tree before purging include paths
**What goes wrong:** `rm -rf vivox/` while `clientUserInterface.vcxproj` still lists `vivox\include` → compile fails (missing include dir, though MSVC tolerates missing AdditionalIncludeDirectories as a warning) and the `Vxc.h` includes in any not-yet-deleted voice file break.
**How to avoid:** Sequence the tree delete LAST (Wave 2), after all source + include-path edits land and build green.

### Pitfall 4: Over-removing the shared `soePlatform\libs` path
**What goes wrong:** `Base_vchat.lib`/`VChatAPI.lib` live in `soePlatform/libs/Win32-Debug` alongside `Base.lib`, `ChatAPI.lib`, `Network.lib`, `TcpLibrary.lib`, `CommodityAPI.lib` etc. which ARE used by non-voice code (community chat, commodities). Removing the **library directory** would orphan those.
**How to avoid:** Remove only the specific voice `.lib` tokens (`Base_vchat.lib`, `VChatAPI.lib`) from `<AdditionalDependencies>`; KEEP the `soePlatform\libs\Win32-Debug` directory in `<AdditionalLibraryDirectories>` (see Open Questions Q1). Do NOT delete the soePlatform tree (out of D-04 scope — D-04 names only `vivox/` and `vivoxSharedWrapper/`).

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Vivox/VChat voice chat (live SOE feature) | Disabled by default; no Vivox backend on SWGSource VM | retail sunset / private-server era | Removal loses no working feature |
| `.rsp` response files drive the link (CMake era) | Inline `.vcxproj` `<AdditionalDependencies>` drives the link (Koogie MSBuild) | Phase 9 Option D base swap | `.rsp` files are vestigial; edit `.vcxproj` |

**Deprecated/outdated:**
- The CONTEXT `<code_context>` baseline file list — superseded by the Authoritative Removal Map above (baseline missed `voicechat/` messages, `SwgCuiHudWindowManager`, `SwgCuiMediatorTypes.h`, `SwgCuiActions.h`, `SwgGodClient.vcxproj` inline refs, and the inline `SwgClient.vcxproj` VChat/libsndfile libs).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The 3 `voicechat/` network-message files are in-scope to delete (voice-only, no other consumer) | Removal Map §A | LOW — confirmed zero non-voice consumers + no game server; if kept, criterion #1 fails on `VoiceChat` symbol. Recommend delete. |
| A2 | `libsndfile-1.lib` is a Vivox-only dependency (safe to unlink with vivox) | Removal Map §E | LOW-MEDIUM — `libsndfile` appears adjacent to vivox libs in the link line; if some non-voice code uses it the link breaks (caught by D-09 gate). Verify at plan time with a grep for `sndfile`/`sf_open` consumers. |
| A3 | Removing `Base_vchat.lib`+`VChatAPI.lib` tokens but KEEPING the `soePlatform\libs` dir is correct | Pitfall 4 / Q1 | LOW — `Base.lib`/`ChatAPI.lib`/etc. are non-voice and stay; the dir must stay for them. If the dir is wrongly removed, non-voice link breaks (caught by D-09). |

## Open Questions

1. **Should the `soePlatform/VChatAPI` + `soePlatform/ChatAPI2/...VChat...` SOURCE trees be touched?**
   - What we know: D-04 names ONLY `vivox/` and `vivoxSharedWrapper/`. The `soePlatform` tree contains `VChatAPI` (voice) AND non-voice libs (`ChatAPI`, `Base`, `Network`, `TcpLibrary`, `CommodityAPI`) used by community chat/commodities. The prebuilt `VChatAPI.lib`/`Base_vchat.lib` are consumed only transitively by vivoxSharedWrapper.
   - What's unclear: Whether criterion #1's "grep finds zero `vivox`/`Vivox`" extends to `VChat` strings inside the `soePlatform/VChatAPI` source tree. Criterion #1 literally says `vivox`/`Vivox`, NOT `VChat` — and a grep of `soePlatform/VChatAPI` for `vivox` should be checked.
   - Recommendation: **Out of scope to delete the soePlatform source tree** (not named in D-04, contains live non-voice libs, and the editor-target VChat refs are D-07 `.rsp`/inline cleanup). Remove only the `Base_vchat.lib`/`VChatAPI.lib` LINK tokens from `SwgClient`/`SwgGodClient`. Confirm at plan time that `soePlatform/VChatAPI` source contains no literal `vivox`/`Vivox` (the earlier grep showed `vivox` matches only under `external/3rd/library/vivox*`, NOT under soePlatform — so criterion #1 is already satisfied for soePlatform). **Leave soePlatform alone.**

2. **Does `swgClientUserInterface.vcxproj` carry any inline vivox include path?**
   - What we know: The grep for vivox in `.vcxproj` matched `SwgClient`, `clientGame`, `clientUserInterface`, `SwgGodClient`, and the editor vcxprojs — NOT `swgClientUserInterface.vcxproj` for vivox paths (it matched only for the `SwgCuiVoice*` ClCompile entries).
   - Recommendation: `swgClientUserInterface` reaches voice symbols via public re-includes of `clientUserInterface`; no inline vivox path to purge there. Just remove the `SwgCuiVoice*` ClCompile/ClInclude entries (Removal Map §A).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSBuild (VS 2026/v145) | Build gate (D-09) | ✓ (not on PATH — use full path per memory) | v145 toolset | — |
| `swg.sln` | Build target | ✓ | — | — |
| SWGSource VM (192.168.1.200) | Boot gate (D-08) | ✓ (user-run) | StellaBellum/v3.0 | — |
| `ripgrep` | grep-zero gates | ✓ (via Grep tool) | — | — |
| Vivox/VChat SDK | n/a — being REMOVED | ✓ (prebuilt .libs in `soePlatform/libs`, `vivox*/lib`) | — | — |

**Missing dependencies with no fallback:** None.
**Missing dependencies with fallback:** None — this is an in-repo removal; the only "dependency" being acted on is the thing being deleted.

## Validation Architecture

> nyquist_validation is enabled (config.json `workflow.nyquist_validation: true`). This phase has NO unit-test framework — validation is via grep-zero gates, the link-grep gate, and the human-run dual-renderer boot gate (matching Phases 12/13).

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None (C++ engine removal; no unit-test harness in tree) |
| Config file | none — validation is grep + build-log + manual boot |
| Quick run command | `rg -c "[Vv]ivox\|CuiVoiceChat\|SwgCuiVoice\|VOICE_INVITE\|voiceInvite\|voiceKick\|voice_invite\|swgClientVivox\|VoiceChat\|VoiceFlyBar\|VoiceActiveSpeakers" src --glob '!*.vcproj' --glob '!*.filters'` (expect 0 outside soePlatform/non-voice ChatAPI) |
| Full suite command | MSBuild Debug + Release, then grep link logs for `unresolved external symbol` |

### Phase Requirements → Test Map (Success Criteria)
| Crit | Behavior | Test Type | Automated Command | Gate |
|------|----------|-----------|-------------------|------|
| #1 | Zero vivox/Vivox refs across `.rsp`, source, include paths | grep-zero | `rg -i "vivox" src` → 0 (after vendored-tree delete; soePlatform contains no `vivox` literal — pre-verified) | grep |
| #2 | `CuiVoiceChatManager`/`SwgCuiVoiceFlyBar`/`CuiVoiceChatEventHandler` + full symbol set deleted | grep-zero | `rg "CuiVoiceChat\|SwgCuiVoice\|VOICE_INVITE\|voiceInvite\|voiceKick" src` → 0 | grep |
| #3 | Voice prefs stripped, no remaining caller references a removed key/symbol | grep-zero + compile | `rg "ms_voice\|getVoice\|setVoice\|REGISTER_OPTION_USER(voice" src` → 0; clean compile | grep + build |
| #4 | swg.sln builds clean Debug AND Release with Vivox gone | link-grep | MSBuild Debug + Release; grep each link log: `unresolved external symbol` count == 0 (NOT exit 0 — `/FORCE` masks) | build |
| #5 | Boots to char-select under rasterMajor=5 AND =11 vs SWGSource VM | manual boot | set `rasterMajor` in `stage/client_d.cfg`; launch `SwgClient_d.exe`; reach char-select both renderers | human |

### Sampling Rate
- **Per task commit:** quick grep-zero for the symbols touched by that task + per-library MSBuild (link-grep).
- **Per wave merge:** full repo grep-zero + Debug+Release link-grep.
- **Phase gate:** full grep-zero (criteria #1-3) + Debug+Release link clean (criterion #4) + dual-renderer boot (criterion #5, human-confirmed) before `/gsd-verify-work`.

### Wave 0 Gaps
- None — no test infrastructure to create. The grep-zero, link-grep, and boot gates are the established Phase 12/13 validation pattern. (If the planner wants a scripted grep gate, a one-line `rg` invocation per criterion suffices — no fixtures needed.)

## Security Domain

> `security_enforcement: true`, `security_asvs_level: 1`. This is a code/build REMOVAL phase — it deletes a third-party voice SDK (Vivox/VChat) and its network-message handlers. No new attack surface is added; surface is REDUCED.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | Voice auth (`VoiceChatOnGetAccount` GAR_SUCCESS) is being removed, not added |
| V3 Session Management | no | No session code touched |
| V4 Access Control | no | No access-control change |
| V5 Input Validation | marginal | Removing `VoiceChatChannelInfo`/`OnGetAccount`/`MiscMessages` message parsers REDUCES the network-message attack surface (fewer `Archive::ReadIterator` deserializers exposed). Net-positive for security. |
| V6 Cryptography | no | No crypto code touched (`crypto.lib` stays — unrelated) |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Removing a network-message handler still referenced by an inbound message | Denial of Service (orphaned dispatch) | The dual-renderer boot gate + the fact that `connectToMessage(VoiceChatChannelInfo::cms_name)` is removed with the manager means no handler is registered → inbound voice messages (if any) are simply unhandled (engine default = ignore), not a crash |
| Vendored third-party SDK with latent CVEs left on disk | Tampering / supply-chain | D-04 deletes the entire `vivox`/`vivoxSharedWrapper` trees — eliminates the dormant SDK from the tree entirely (positive) |
| `/FORCE` masking an unresolved symbol that hides a partially-removed handler | Tampering (silent broken binary) | D-09 link-grep gate (0 `unresolved external symbol`) is the control |

**Security posture:** This phase strictly reduces attack surface (deletes a third-party SDK + 3 network deserializers). No security control is weakened. The only security-relevant gate is D-09 (don't ship a `/FORCE`-masked broken binary).

## Sources

### Primary (HIGH confidence — in-repo, verified this session)
- `SwgClient.vcxproj` lines 103/105/158/160/204/206 — inline link libs + lib dirs (Debug/Optimized/Release)
- `SwgClient/build/win32/{libraries_d,libraries_r,libraries_o,libraryPaths}.rsp` — vestigial, confirmed not referenced by .vcxproj
- `clientUserInterface.vcxproj:73,111,148,292,293,484,485,486`; `clientUserInterface/includePaths.rsp:51`
- `clientGame.vcxproj:73,111,148`; `clientGame/includePaths.rsp:55`
- `swgClientUserInterface.vcxproj:650,842-845,905,1082,1150-1153,1175`
- `sharedNetworkMessages.vcxproj:1178-1180,1545-1547`
- `OptionManager.cpp:167-205,447-487,581-726,735-781` (D-03a unregistered-key tolerance)
- `CuiPreferences.cpp:22,391-396,842-847,869-873,3412-3491`; `CuiPreferences.h:538-554`
- `CuiVoiceChatManager.cpp:27-29,190-356,1881`; `CuiVoiceChatGlue.h:89` (SwgVivox typedef)
- `CuiManagerManager.cpp:57,105,156,210`; `CuiRadialMenuManager.cpp:60,1182-1184`; `CommandCppFuncs.cpp:57,267-268,2422-2466,2659-2660`; `ClientCommandQueue.cpp:44,696`; `CustomerServiceManager.cpp:22,865-866`
- `SwgCuiMediatorFactorySetup.cpp:132-133,258-259`; `CuiMenuInfoTypes.cpp:308`/`.h:165`; `SwgCuiMediatorTypes.h:104-105`; `SwgCuiActions.h:69-70`
- `SwgCuiHudAction.cpp:110,286,1598-1605`; `SwgCuiStatusGround.cpp:36,1196,2040-2051`; `SwgCuiHudWindowManager.cpp:318-319`; `SwgCuiOpt.cpp:32,54,112-114`; `SwgCuiCommandParserDefault.cpp:77,442`
- 8 editor `.rsp` (D-07) + `SwgGodClient.vcxproj:99,143,185` inline
- `swg.sln` — 128 projects, no vivox/VChat/server-game project (prebuilt .libs only)
- `src/external/3rd/library/{vivox,vivoxSharedWrapper}/` directory listings; `src/external/3rd/library/soePlatform/libs/Win32-{Debug,Release}/` listings

### Secondary (MEDIUM)
- `.planning/phases/13-videocapture-library-unlink/13-CONTEXT.md` — precedent (atomic delete, /FORCE gate, editor .rsp purge)
- `.planning/STATE.md` Blockers/Concerns — /FORCE false-pass + inline-.vcxproj findings; MSYS sed gotcha
- Memory `project_decruft_removal_build_graph_gotchas` — build-graph + invocation gotchas

### Tertiary (LOW)
- None — all claims verified in-repo this session.

## Metadata

**Confidence breakdown:**
- Removal Map (file/ref enumeration): HIGH — fresh repo-wide ripgrep + per-file reads; baseline gaps explicitly identified.
- Symbol-resolution / ordering: HIGH — traced the `callers → manager → SwgVivox/libs` + `manager → voicechat-messages` graph directly from source includes and call sites.
- D-03a (option-key tolerance): HIGH — read the full `OptionManager` load/save/intersection logic.
- D-06a (TRE no-op): MEDIUM-HIGH — confirmed the activation/dispatch/menu-build paths are all removed; backstopped by the boot gate (compiled TRE assets are genuinely out-of-reach).
- soePlatform/VChat scope boundary: MEDIUM — recommendation is "leave alone" based on D-04's literal scope + non-voice lib usage; flagged as Open Question Q1 for plan-time confirmation.

**Research date:** 2026-05-26
**Valid until:** Stable until the tree changes — this is an in-repo snapshot; re-grep if other phases edit these libraries before Phase 14 executes.
