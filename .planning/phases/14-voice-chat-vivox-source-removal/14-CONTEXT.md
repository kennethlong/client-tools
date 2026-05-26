# Phase 14: Voice Chat (Vivox) Source Removal - Context

**Gathered:** 2026-05-26
**Status:** Ready for planning

<domain>
## Phase Boundary

Fully remove the **Vivox voice-chat subsystem** from the active MSBuild tree
(`src/build/win32/swg.sln`) — the `vivoxSharedWrapper` link, the engine manager +
event handler, the game-side UI (fly bar, active speakers, voice options tab, voice
command parser), the `CuiPreferences` voice keys, and **every in-repo registration of
voice symbols** (command-table bindings, radial-menu type, mediator-factory ctors) —
de-wiring every live caller without breaking the surfaces those callers live in
(`CuiManagerManager`, `CuiRadialMenuManager`, `CuiPreferences`, the command tables,
the CS report). Client stays bootable to character select under **both** renderers.
Satisfies **DECRUFT-05**.

**Higher-risk live-UI surgery (per roadmap), not a `.rsp` edit.** Unlike Phase 13's
closed VideoCapture chain, voice symbols are called from surfaces that *remain* — the
core UI manager lifecycle, the radial menu, the options/preferences surface, and the
client command tables. This is the Phase-12 lcdui "live de-wire" shape, but criterion #2
("grep finds zero references to those symbols") forces **deletion at every call site**,
not the no-op stub Phase 12 used for lcdui. This phase clarifies HOW to remove it; the
decision to remove (not keep) is fixed by the v2.1 Decruft milestone.

**The surface is ~3× the DECRUFT-05 named files.** DECRUFT-05 names only
`CuiVoiceChatManager`, `SwgCuiVoiceFlyBar`, `CuiVoiceChatEventHandler` — but a fresh grep
finds the whole subsystem (see `<code_context>`). All of it is in scope.

</domain>

<decisions>
## Implementation Decisions

### Removal Scope
- **D-01:** **Remove the WHOLE voice subsystem, not just the 3 DECRUFT-05-named files.**
  Mirrors Phase 13 D-02 ("whole capture family"). The named-3 alone will not compile —
  `SwgCuiVoiceActiveSpeakers(+_TableModel)`, `SwgCuiVoiceFlyBarMessageQueue`,
  `SwgCuiOptVoice` (the Voice options tab), `SwgCuiCommandParserVoice`, `CuiVoiceChatGlue.h`,
  and the public re-include headers all reference the deleted symbols. Researcher
  **re-greps `[Vv]ivox` + the voice-symbol set repo-wide to enumerate the full file list**
  before planning (the list in `<code_context>` is the scouted baseline, not a guarantee
  of completeness).

### Live-Caller Model
- **D-02:** **De-wire by deleting each call site — no stub.** Edit each surviving caller to
  remove its voice lines; do NOT leave a no-op `CuiVoiceChatManager` shim (that keeps the
  symbol and fails criterion #2's grep-zero). Concretely, the callers that **stay** and must
  be edited in place:
  - `CuiManagerManager.cpp` — remove `CuiVoiceChatManager::install/remove/update` from the
    UI-manager lifecycle (+ the include).
  - `CuiRadialMenuManager.cpp:1182-1184` — remove the `isLoggedIn()`-guarded `VOICE_INVITE`
    `addRootMenu` block (+ the include).
  - `CommandCppFuncs.cpp` — delete `commandFuncVoiceInvite/Kick` (decl + defn) **and** their
    `CommandTable::addCppFunction("voiceInvite"/"voiceKick", …)` registrations (`:2659-2660`).
  - `ClientCommandQueue.cpp:696` + `CustomerServiceManager.cpp:866` — drop the
    `CuiVoiceChatManager::getCsReportString()` append from the CS-report string.
  - `SwgCuiHudAction.cpp` / `SwgCuiStatusGround.cpp` — remove their voice hooks (call sites,
    same model).
- **D-02a:** **Removal must keep the tree buildable at each coherent step** (no mid-step
  un-buildable state). Exact edit order / commit granularity is the planner's call (see
  Claude's Discretion), provided the link gate stays green per D-07.

### CuiPreferences Voice Keys
- **D-03:** **Full strip of all 6 voice preference keys from `CuiPreferences`** — the
  `ms_voiceChatEnabled / ms_voiceUsePushToTalk / ms_voiceShowFlybar /
  ms_voiceUseAdvancedChannelSelection / ms_voiceAutoDeclineInvites / ms_voiceAutoJoinChannels`
  bools, their `REGISTER_OPTION_USER(...)` registrations (`:842-847`), all getters/setters
  (`:3412-3482+`), the `CuiVoiceChatManager::setVoice*(false)` default-disable block
  (`:869-873`), and the `CuiVoiceChatManager.h` include (`:22`).
- **D-03a:** **Safe — verified during discussion:** a repo-wide grep found **zero non-voice
  callers** of the voice getters/setters (only the voice subsystem + `CuiPreferences` itself
  reference them). `REGISTER_OPTION_USER` registers with `CurrentUserOptionManager`
  (`#define` at `:423`), and unregistered persisted keys are simply not read on load — so a
  stale `voiceChatEnabled=…` line in an existing per-user `options.cfg` is **inert**, not a
  parse error. Researcher confirms the `CurrentUserOptionManager` unregistered-key tolerance;
  the dual-renderer boot gate (D-06) is the backstop that the options surface still loads.

### Trees, Dangling Refs & In-Repo Registrations
- **D-04:** **Delete the vendored SDK trees** `src/external/3rd/library/vivox/` **and**
  `src/external/3rd/library/vivoxSharedWrapper/` entirely (mirrors Phase 13 D-03 videocapture
  delete). Their headers contain `vivox`, so leaving them on disk would fail criterion #1's
  "grep finds zero `vivox`/`Vivox` references across … include paths."
- **D-05:** **Purge the dangling `swgClientVivox` references.** The directory
  `src/game/client/library/swgClientVivox/` **does not exist on disk** (confirmed) — yet
  `swgClientVivox/include/public` is still listed in `clientUserInterface` includePaths.rsp
  (`:51`), `clientGame` includePaths.rsp (`:55`), and the matching `.vcxproj` ItemDefinition
  AdditionalIncludeDirectories. Pure vestigial cleanup; remove all of them.
- **D-06:** **Chase ALL in-repo source registrations of voice symbols** (user-directed
  refinement — "chase the data," reconciled to source because this repo has **no `dsrc/`/`data/`
  datatable tree**):
  - `SwgCuiMediatorFactorySetup.cpp:258-259` — the `MAKE_SWG_CTOR_WS(VoiceFlyBar, …)` /
    `(VoiceActiveSpeakers, …)` ctor registration lines + the two voice `#include`s.
  - `CommandCppFuncs.cpp:2659-2660` — `CommandTable::addCppFunction("voiceInvite"/"voiceKick", …)`
    (also under D-02).
  - `CuiMenuInfoTypes.cpp:308` (`MAKE_ID(VOICE_INVITE, none, voice_invite, 0)`) +
    `CuiMenuInfoTypes.h:165` (the `VOICE_INVITE` enum value) — the radial menu type definition.
- **D-06a:** **Compiled TRE assets are out-of-reach and accepted as inert.** The actual
  `.ui` page layouts (`/Voice.VoiceFlyBar`, `/Voice.VoiceActiveSpeakers`) and any command/radial
  **datatable rows** live in the user's retail SWG TRE install, NOT in this source tree (there
  is no `dsrc/`/`data/` directory here). They cannot be edited and are not redistributed; the
  engine no-ops on an orphaned page-path / command-name / radial entry whose handler is gone.
  Researcher verifies graceful no-op; the boot gate is the backstop. "Chase the data" resolves
  to D-06's source-level registrations — there is no in-repo datatable file to scrub.
- **D-07:** **Purge the 8 editor `.rsp` vivox references** (D-04 carry-forward from Phase 13).
  `vivoxSharedWrapper_release.lib` / `vivoxSharedWrapper/lib` paths appear in
  AnimationEditor, ClientEffectEditor, LightningEditor, NpcEditor, ParticleEditor, SwooshEditor,
  Viewer, and SwgGodClient `.rsp` files. Required to satisfy criterion #1's grep-zero literally.
  Editors are out-of-scope as *build targets* (pre-broken), but their stale refs must still go.

### Verification (carried forward — LOCKED by the v2.1 milestone invariant)
- **D-08:** **Dual-renderer boot gate** after removal — boots to character select under
  **both** `rasterMajor=5` (D3D9) and `=11` (D3D11). Set `rasterMajor` in `client_d.cfg`
  (the **debug** exe reads `client_d.cfg`, not `client.cfg`).
- **D-09:** **Build gate greps link output for 0 `unresolved external symbol`** — NOT just
  MSBuild exit 0. `/FORCE` downgrades unresolved externals to warnings and still emits a binary
  (Phase 12/13 finding). **Debug AND Release** must both link clean. Edit **inline `.vcxproj`**
  compile/link lists, not just the (often vestigial) `.rsp` files.

### Claude's Discretion
- Exact edit order, plan/wave breakdown, and commit granularity within the removal — planner's
  call, provided every coherent step leaves the tree buildable (D-02a) and the link gate green
  (D-09). The Phase 13 shape (atomic link-unit removal in Wave 1, residue/path purge in Wave 1
  parallel, vendored-tree delete sequenced last in Wave 2) is a reasonable template.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirement & roadmap (this phase)
- `.planning/REQUIREMENTS.md` — **DECRUFT-05** (the requirement: unlink `vivoxSharedWrapper_debug.lib`;
  delete the 3 named files; strip `CuiPreferences` voice keys) + **DECRUFT-07** (cross-cutting
  dual-renderer boot gate, verified after every removal).
- `.planning/ROADMAP.md` §"Phase 14: Voice Chat (Vivox) Source Removal" — Goal + 5 success criteria
  (criterion #1 grep-zero across `.rsp`/source/include; #2 named-file + symbol deletion; #3 prefs
  strip without breaking `CuiPreferences`; #4 Debug+Release clean build; #5 dual-renderer boot).

### Prior-phase precedent (apply the lessons directly)
- `.planning/phases/13-videocapture-library-unlink/13-CONTEXT.md` — the closest analog. D-02
  "whole family" scope, D-03 vendored-tree delete, D-04 editor `.rsp` purge, D-05 dual-renderer
  boot gate, D-06 `/FORCE` link-grep gate. Phase 14 extends D-02 to *live* callers.
- `.planning/PROJECT.md` §"Current Milestone: v2.1 — Decruft" — milestone framing + the invariant
  ("every step leaves the client bootable to character select under both renderers").
- `.planning/STATE.md` §Blockers/Concerns — the Phase 12 `/FORCE` false-pass finding and the
  "dead modules link via inline `.vcxproj` deps, not just `.rsp`" finding.

### Memory (recurring gotchas — Phases 13-15)
- Memory `project_decruft_removal_build_graph_gotchas` — `.rsp` vestigial (edit inline vcxproj);
  `/FORCE` masks unresolved-externals (grep for 0, not exit 0); msbuild not on PATH (use full
  path); `/nodeReuse:false`; run from PowerShell not Git Bash; delete exe to force relink;
  editors pre-broken on Qt.
- Memory `feedback_debug_exe_reads_client_d_cfg` — the debug exe reads `client_d.cfg` (set
  `rasterMajor` there for the boot gate).

### Reference diff template (external — sibling repo, not in this tree)
- Original **whitengold (swg-client) Phase 07** removal commits (CLEAN-01..05; voice was bundled
  there), retargeted CMake → MSBuild. Reference only — the active tree is the Koogie MSBuild tree,
  so paths/build-graph differ. No in-repo path; treat as a shape guide.

</canonical_refs>

<code_context>
## Existing Code Insights

### Removal surface (scouted baseline — researcher must re-grep to confirm completeness)

**Build / link (SwgClient):**
- `src/game/client/application/SwgClient/build/win32/libraries_d.rsp:2` → `vivoxSharedWrapper_debug.lib`
- `.../SwgClient/build/win32/libraries_r.rsp:2` → `vivoxSharedWrapper_release.lib`; `libraries_o.rsp:2` → debug
- `.../SwgClient/build/win32/libraryPaths.rsp:9` → `…/vivoxSharedWrapper/lib`
- `.../SwgClient/build/win32/SwgClient.vcxproj` — check inline lib/path refs too

**Include paths (engine libs):**
- `clientUserInterface/build/win32/includePaths.rsp:43-44,51` → `vivox/include`, `vivoxSharedWrapper/include`, `swgClientVivox/include/public` (last is dangling — dir absent)
- `clientGame/build/win32/includePaths.rsp:54-55` → `vivox/include`, `swgClientVivox/include/public` (dangling)
- `clientGame.vcxproj` + `clientUserInterface.vcxproj` — inline `swgClientVivox` AdditionalIncludeDirectories (dangling)

**Engine source — `clientUserInterface` (manager + handler + glue):**
- `…/src/shared/core/CuiVoiceChatManager.cpp/.h` (named) + `include/public/clientUserInterface/CuiVoiceChatManager.h`
- `…/src/shared/core/CuiVoiceChatEventHandler.cpp/.h` (named)
- `…/src/shared/core/CuiVoiceChatGlue.h`
- `…/src/shared/core/CuiManagerManager.cpp` — install/remove/update lifecycle (LIVE caller, de-wire)
- `…/src/shared/core/CuiRadialMenuManager.cpp:1182-1184` — VOICE_INVITE entry (LIVE caller, de-wire)
- `…/src/shared/core/CuiPreferences.cpp` — 6 voice keys (D-03)
- `…/src/shared/core/CuiMenuInfoTypes.cpp:308` + `.h:165` — `VOICE_INVITE` enum + `MAKE_ID(voice_invite)` (D-06)

**Engine source — `clientGame` (command + CS-report callers):**
- `…/src/shared/command/CommandCppFuncs.cpp:267-268,2422,2448,2659-2660` — voice command funcs + table bindings (D-02/D-06)
- `…/src/shared/command/ClientCommandQueue.cpp:696` — CS-report append (LIVE caller, de-wire)
- `…/src/shared/core/CustomerServiceManager.cpp:866` — CS-report append (LIVE caller, de-wire)

**Game UI — `swgClientUserInterface` (pages + parser + factory):**
- `…/src/shared/page/SwgCuiVoiceFlyBar.cpp/.h` (named) + `SwgCuiVoiceFlyBarMessageQueue.cpp/.h`
- `…/include/public/swgClientUserInterface/SwgCuiVoiceFlyBar.h`
- `…/src/shared/page/SwgCuiVoiceActiveSpeakers.cpp/.h` + `_TableModel.cpp/.h`
- `…/src/shared/page/SwgCuiOptVoice.cpp/.h` — the Voice **options tab** (reads the CuiPreferences keys)
- `…/src/shared/parser/SwgCuiCommandParserVoice.cpp` — voice console-command parser
- `…/src/shared/page/SwgCuiHudAction.cpp` + `SwgCuiStatusGround.cpp` — voice hooks (LIVE callers, de-wire)
- `…/src/shared/core/SwgCuiMediatorFactorySetup.cpp:132-133,258-259` — voice page includes + ctor registrations (D-06)

**Vendored SDK (delete the trees — D-04):**
- `src/external/3rd/library/vivox/` and `src/external/3rd/library/vivoxSharedWrapper/`

**Out-of-scope-as-targets but refs must be purged (D-07):**
- `.rsp` files under AnimationEditor, ClientEffectEditor, LightningEditor, NpcEditor, ParticleEditor,
  SwooshEditor, Viewer, SwgGodClient (researcher: re-grep `--include="*.rsp"` for `vivox` to enumerate).

### Established Patterns (from Phases 12-13 — apply directly)
- **`/FORCE` false-pass:** unresolved externals become warnings, binary still emits at exit 0. Build
  gate MUST grep link output for `unresolved external symbol` (== 0), Debug AND Release.
- **Inline `.vcxproj`, not just `.rsp`:** the `.rsp` files can be vestigial; the real compile/link
  wiring is often inline in the `.vcxproj`. Check both.
- **Live-caller de-wire (Phase 12 lcdui):** when callers stay, the call site is edited — but here
  delete the lines (criterion #2 grep-zero), do NOT leave a no-op stub (the Phase-12 stub kept symbols).

### Integration / blast-radius notes for research
- `clientUserInterface` and `clientGame` are engine libs shared by **both** the D3D9 and D3D11
  renderers — both must rebuild + link clean after the voice symbols go.
- **Symbol-resolution risk to confirm:** where do `CuiVoiceChatManager::` symbols resolve — the
  deleted `.cpp`s vs `vivoxSharedWrapper_debug.lib`? Every consumer must be de-wired in the same
  change set or the link breaks (reinforces D-02 delete-at-callsite).
- **Mid-removal build health:** with ~15 files + 6+ live call sites + 3 registration sites, ordering
  matters more than Phase 13. Keep the tree buildable per D-02a.

</code_context>

<specifics>
## Specific Ideas

- The voice options are **disabled by default** (`CuiPreferences.cpp:390-391` "all voice options
  disabled by default" + the explicit `setVoice*(false)` block) — confirming voice is a dormant,
  off-by-default feature with no functional Vivox backend on the SWGSource VM. No working user
  feature is lost by removal.
- The radial `VOICE_INVITE` entry is gated on `CuiVoiceChatManager::isLoggedIn()`, which (voice
  being disabled + no Vivox server) is always false — the menu entry never appears today. Safe to
  delete the enum + the add-call together.

</specifics>

<deferred>
## Deferred Ideas

- **In-Game Browser (XPCOM/Mozilla) removal** — that's Phase 15 (DECRUFT-06), the highest-risk
  surgery + milestone gate. Out of Phase 14 scope.
- **Bink video codec / Miles audio playback** — explicitly OUT of v2.1 scope (active middleware).
  Do not conflate voice removal with the live audio path; Miles `AIL_*` playback is untouched.

### Reviewed Todos (not folded)
- **"Diff SWGSource vs whitengold TRE for space-scene graphics artifacts"** (score 0.6) — keyword
  match only (`swgsource`/`client`); this is a **v2.2 Visual Parity** asset/rendering investigation,
  unrelated to voice-chat source removal. Deferred.
- **"Cantina interior corner-snap engine quirk"** (score 0.2) — rendering/collision; pre-existing
  SOE engine quirk, not in Decruft scope. Deferred.

</deferred>

---

*Phase: 14-voice-chat-vivox-source-removal*
*Context gathered: 2026-05-26*
