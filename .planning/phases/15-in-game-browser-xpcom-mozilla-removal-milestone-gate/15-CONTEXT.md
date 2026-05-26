# Phase 15: In-Game Browser (XPCOM/Mozilla) Removal & Milestone Gate - Context

**Gathered:** 2026-05-26
**Status:** Ready for planning

<domain>
## Phase Boundary

Fully remove the **dormant XPCOM/Mozilla in-game web browser** from the active
MSBuild tree (`src/build/win32/swg.sln`) — the `libMozilla.vcxproj` static-lib
project, the `libMozilla.lib` link in `SwgClient`, the `libMozilla/include/public`
include-path entries, the vendored `src/external/3rd/library/libMozilla/` tree (XPCOM
SDK), the three `SwgCuiWebBrowser*` game-UI files (the only consumers of the
`libMozilla::` API), the `TUIWebBrowser` widget RTTI type in the `ui` library, and
**every live caller** (the UI-manager lifecycle, the `/browser` console command, the
HUD/IME/IoWin focus-routing hooks, and the TCG web-navigation callbacks) — de-wiring
each without breaking the surfaces those callers live in. Then run the **cross-cutting
milestone-wide gate** that closes **v2.1 Decruft**. Client stays bootable to character
select under **both** renderers. Satisfies **DECRUFT-06** (removal) + **DECRUFT-07**
(milestone gate).

**Highest-risk live-UI surgery (per roadmap), plus the milestone close.** Like Phase 14,
the browser symbols are called from surfaces that *remain* — but Phase 15 adds two new
hazards beyond Phase 14's shape: (a) `TUIWebBrowser` is a value in the `ui` library's
**positional `UITypeID` enum** (the exact shape of the Phase 14 CR-01 regression), and
(b) the browser is reached through a **live middleware callback** (`libEverQuestTCG`'s
navigate procs) that is itself out of scope and must survive. This phase clarifies HOW to
remove it; the decision to remove (not keep) is fixed by the v2.1 Decruft milestone.

**Already stubbed today (no functional loss).** STATE.md confirms `libMozilla::init()`
returns `true` but does nothing — there is no live XPCOM runtime, no staged Mozilla DLLs,
and the in-game browser never renders. Removal severs dead code; the risk is purely build,
enum-ordinal, and de-wire correctness, not feature regression.

**Scope guard — sibling "Browser" UIs are NOT in scope.** `SwgCuiCommandBrowser`,
`SwgCuiMissionBrowser`(`_TableModel`), and `SwgCuiPersistentMessageBrowser` are list/table
UIs, not web browsers — leave them untouched. `libEverQuestTCG` (the TCG card game) is
separate live middleware — only its browser *tie* is severed.

</domain>

<decisions>
## Implementation Decisions

### Removal Scope (carried forward from Phases 12–14 — LOCKED)
- **D-01:** **Remove the WHOLE browser surface, not just the loosely-named roadmap symbols.**
  The roadmap criterion #2 names `CuiWebBrowser*` / `UIWebBrowserWidget`, but the actual files
  are `SwgCuiWebBrowserManager` / `SwgCuiWebBrowserWidget` / `SwgCuiWebBrowserWindow`
  (`swgClientUserInterface`), and the widget-type is `TUIWebBrowser` in the `ui` lib's
  `UITypeID` — there is **no** file literally named `UIWebBrowserWidget`. Researcher
  **re-greps the actual symbol set repo-wide** (`SwgCuiWebBrowser`, `WebBrowserManager`,
  `libMozilla`, `TUIWebBrowser`, `xpcom`, `xul`, `[Mm]ozilla`, `nsIWebBrowser`) to enumerate
  the full file/ref list before planning (the `<code_context>` list is the scouted baseline,
  not a guarantee of completeness).

### UITypeID Enum Surgery (the CR-01 recurrence — primary risk)
- **D-02:** **Verify-then-clean-delete `TUIWebBrowser` from the `ui` `UITypeID` enum.**
  `TUIWebBrowser` sits **mid-enum** in `src/external/3rd/library/ui/src/shared/core/UITypeID.h:64`,
  inside the `TUIWidget` block with `TUIStyle` and ~30 `*Style` entries *after* it. Deleting it
  mid-sequence shifts every subsequent ordinal — the **exact shape of the Phase 14 CR-01
  regression** (memory `project_radial_menu_enum_ordinal_is_datatable_row_index`). **However,
  scouting shows this is structurally DIFFERENT from CR-01:** the `ui` library is **name-keyed**
  — widgets expose a string `TypeName`/`GetTypeName()` and `UIStandardLoader<T>` deserializes
  `.ui` data via `T::TypeName`; `UITypeID` is purely **in-memory RTTI** for `IsA()` fast-path
  checks. Across a recompile, ordinal shifts are internally consistent. So the plan is:
  **researcher confirms no `UITypeID` integer is ever persisted to TRE / `.ui` data (only the
  name string is)**, and on confirmation, **delete `TUIWebBrowser` outright** plus every
  `IsA(TUIWebBrowser)` reference and the `SwgCuiTcgControl::IsA` `TUIWebBrowser` clause.
- **D-02a:** **Hard fallback — if research finds ANY `UITypeID` ordinal serialized to disk/TRE,
  switch to an ordinal-preserving placeholder** (a `TUIReserved_WebBrowser` enumerator that
  holds the slot, mirroring the Phase 14 CR-01 fix `RESERVED_RADIAL_SLOT_103..105`). The
  link gate and char-select boot gate **cannot** catch an ordinal-shift regression — this
  verification is mandatory, not optional.

### TCG Browser Caller (libEverQuestTCG stays — sever only the browser tie)
- **D-03:** **`libEverQuestTCG` and all TCG infra (`SwgCuiTcgManager`, `SwgCuiTcgControl`,
  `SwgCuiTcgWindow`) are OUT of scope and stay.** TCG is separate live middleware (referenced by
  `CuiManager` + the 4 TCG files). The browser ties are only: (1) two `__stdcall` navigate
  callbacks in `SwgCuiTcgManager` (`navigateProc` / `navigateWithPostDataProc`) that open
  `SwgCuiWebBrowserManager`; (2) `SwgCuiTcgControl::IsA` claiming `TUIWebBrowser` identity for
  focus routing.
- **D-04:** **Gut the navigate-callback bodies AND unregister the callbacks from
  `libEverQuestTCG` entirely** (user-directed — go cleaner, leave no dead procs). Remove the
  `SwgCuiWebBrowserManager::createWebBrowserPage/setURL` calls *and* delete the
  `navigateProc`/`navigateWithPostDataProc` definitions + their `libEverQuestTCG` registration
  calls so the grep is zero and no orphan procs remain. Drop the `Type == TUIWebBrowser ||`
  clause from `SwgCuiTcgControl::IsA` (D-02). TCG card rendering (the `libEverQuestTCG::Window`
  path) is untouched; the dead web-store/loyalty flow simply no-ops.
- **D-04a:** **Research caveat — confirm `libEverQuestTCG` tolerates absent navigate callbacks.**
  Verify the lib's callback contract accepts no/null navigate handlers (it likely just won't
  request navigation). **If the lib requires the callbacks to be registered, fall back to
  registered logging-no-op procs** (gut the body, keep the registration) rather than risk a
  null-callback crash in live middleware.

### Live-Caller De-wire (carried forward — delete at call site, no stub)
- **D-05:** **De-wire each surviving caller by deleting its browser lines — no no-op shim**
  (criterion #2 grep-zero; Phase 14 D-02 model). The callers that **stay** and must be edited:
  - `swgClientUserInterface/.../SwgCuiManager.cpp:474/503/551` — remove
    `SwgCuiWebBrowserManager::install/remove/update` from the UI-manager lifecycle (+ the include).
  - `swgClientUserInterface/.../SwgCuiCommandParserUI.cpp` — delete the `browser` and
    `debugBrowserOutput` console commands: the `CommandNames` entries, the command-table rows
    (`:176`/`:214`), and the `isCommand(...browser...)` handlers (`:564-580`, `:1096`) + the include.
  - `swgClientUserInterface/.../SwgCuiHud.cpp` + `SwgCuiHudAction.cpp` — remove the
    `SwgCuiWebBrowserManager` include/hooks and the `|| focused->IsA(TUIWebBrowser)` focus-routing
    clause (`SwgCuiHud.cpp:1026`).
  - **Engine layer** `clientUserInterface`: `CuiIoWin.cpp:513,647` + `IMEManager.cpp:353` — remove
    the `|| focused->IsA(TUIWebBrowser)` clauses from the keyboard/IME focus checks. These are
    shared by BOTH renderers — both must rebuild + link clean.
- **D-06:** **Delete the stray vestigial include in `clientGame/Game.cpp:147`** —
  `#if DEBUG==0 \n #include "libMozilla/libMozilla.h" \n #endif`. There are **no actual
  `libMozilla::` calls in Game.cpp** (the engine browser driver lives entirely in
  `SwgCuiWebBrowserManager`/`SwgCuiWebBrowserWidget`); the guarded include is dead. Remove the
  include + the `libMozilla/include/public` entry from `clientGame` `includePaths.rsp:4`. (Note
  the `#if DEBUG==0` guard means only Release/Optimized configs ever saw it — Debug won't reveal
  breakage here; Release must.)

### libMozilla Project, Link & Vendored Tree
- **D-07:** **Drop `libMozilla.vcxproj` from `swg.sln`** (line 1587, GUID
  `{C6C1E14A-DEDB-48C6-B028-0C9E4EFF5DC4}`) — remove the `Project(...)` block, its
  `GlobalSection(ProjectConfigurationPlatforms)` entries, and any `ProjectDependencies` on it.
  Mirrors the Phase 12 `SwgClientSetup.vcxproj` / `lcdui.vcxproj` sln-drop.
- **D-08:** **Unlink `libMozilla.lib` from `SwgClient.vcxproj`** — it is an **inline**
  `<AdditionalDependencies>` token at lines **158 (Debug)** and **204 (Release)**; it is **NOT**
  in any `libraries*.rsp` (the `.rsp` is vestigial — recurring gotcha
  `project_decruft_removal_build_graph_gotchas`). Edit the inline `.vcxproj`. `libMozilla` is a
  **StaticLibrary** (confirmed `ConfigurationType`), so there is no runtime DLL to unstage.
- **D-09:** **Delete the vendored `src/external/3rd/library/libMozilla/` tree entirely**
  (build/, include/, src/). It holds the XPCOM SDK (`nsIWebBrowser*.h`, `.xpt` components in
  `include/private/bin/{debug,release}/components/`) whose headers contain `xpcom`/`xul`/`Mozilla`
  — leaving it on disk fails criterion #1's grep-zero across include paths. Mirrors Phase 14 D-04
  vendored-tree delete.
- **D-10:** **Stage copy list — verify, don't assume.** `SwgClient.vcxproj` PostBuildEvent only
  copies `_d.exe`/`_d.pdb` (Debug) and `_r.exe`/`_r.pdb` (Release) to `stage/` — **no Mozilla DLL
  copy was found**, consistent with the static-lib + already-stubbed reality (no `stage/mozilla/`
  dir exists). Criterion #2's "Mozilla DLLs removed from POST_BUILD/stage" is likely a **no-op
  here** — researcher confirms there is no separate Mozilla runtime staging anywhere (copy-libs
  scripts, other PostBuild steps) and records it as "none present."

### Editor Reference Purge (carried forward — Phase 14 D-07 model)
- **D-11:** **Purge the editor `.rsp` + inline `.vcxproj` Mozilla refs** to satisfy criterion #1's
  grep-zero literally. `libMozilla`/`mozilla` include + library paths appear in
  `includePaths.rsp`/`libraryPaths.rsp` (and inline `.vcxproj`) for **AnimationEditor,
  ClientEffectEditor, LightningEditor, NpcEditor, ParticleEditor, SwooshEditor, Viewer**, plus
  **SwgGodClient** (which also links `libMozilla.lib` inline). Editors are out-of-scope as *build
  targets* (pre-broken), but their stale refs must still go. Researcher re-greps
  `--include="*.rsp" --include="*.vcxproj"` for `[Mm]ozilla` to enumerate the full set. Use the
  STATE.md MSYS-sed gotcha for `.vcxproj` path purges (substring+delimiter match, not backslash
  escaping).

### Milestone Gate — DECRUFT-07 (closes v2.1)
- **D-12:** **Full milestone-wide residue sweep + per-renderer link gate + dual-renderer boot,
  incremental build** (user-selected depth). After XPCOM removal, the closing gate is:
  1. **Re-grep ALL removed subsystems repo-wide == 0** — `trackIR` (P12), `stationapi` (P12),
     `SwgClientSetup` (P12), `lcdui`/G15 (P12), `videocapture`/`AudioCapture` (P13), `[Vv]ivox`
     (P14), and `xpcom`/`xul`/`[Mm]ozilla`/`libMozilla`/`TUIWebBrowser` (P15) — **excluding the
     known KEEP-listed false positives**: `soePlatform/ChatAPI2/` community-chat `getVoice*`
     (DEF-14-02) and `soePlatform/libs/` prebuilt `VChatAPI.lib`/`Base_vchat.lib` (D-10 P14).
  2. **Debug AND Release link-grep 0 `unresolved external symbol`** (D-13) — normal **incremental**
     build (no clean-from-scratch wipe; trust MSBuild dependency tracking).
  3. **Dual-renderer boot to character select** (D-14).
  Mirrors v2.0 CLEAN-05. This is the v2.1-closing gate, not just a per-phase boot.

### Verification (carried forward — LOCKED by the v2.1 milestone invariant)
- **D-13:** **Build gate greps link output for 0 `unresolved external symbol`** — NOT just
  MSBuild exit 0. `/FORCE` downgrades unresolved externals to warnings and still emits a binary
  (Phase 12/13/14 finding). **Debug AND Release** must both link clean. Edit **inline `.vcxproj`**,
  not just the (often vestigial) `.rsp` files. Delete the target exe to force a relink.
- **D-14:** **Dual-renderer boot gate** — boots to character select under **both** `rasterMajor=5`
  (D3D9) and `=11` (D3D11). Set `rasterMajor` in **`client_d.cfg`** (the **debug** exe reads
  `client_d.cfg`, not `client.cfg` — memory `feedback_debug_exe_reads_client_d_cfg`).

### Claude's Discretion
- Exact edit order, plan/wave breakdown, and commit granularity within the removal — planner's
  call, provided every coherent step leaves the tree buildable (no mid-step un-buildable state)
  and the link gate stays green (D-13). The Phase 13/14 shape (atomic link-unit + call-site
  de-wire in Wave 1, residue/path/editor purge in Wave 1 parallel, vendored-tree delete +
  milestone gate sequenced last) is a reasonable template.
- Whether the milestone residue sweep (D-12.1) is its own plan/wave or folded into the final
  boot-gate plan.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirement & roadmap (this phase)
- `.planning/REQUIREMENTS.md` — **DECRUFT-06** (in-game browser fully removed from source +
  build: drop `libMozilla.vcxproj`; remove `libMozilla/include/public` from `includePaths.rsp`;
  delete `CuiWebBrowser*`/`UIWebBrowserWidget`/XPCOM bridge; Mozilla DLLs off any stage list) +
  **DECRUFT-07** (cross-cutting dual-renderer boot gate after the full removal set; mirrors v2.0
  CLEAN-05; verified incrementally).
- `.planning/ROADMAP.md` §"Phase 15: In-Game Browser (XPCOM/Mozilla) Removal & Milestone Gate" —
  Goal + 4 success criteria (criterion #1 sln-drop + include-path removal + grep-zero
  `xpcom`/`xul`/`Mozilla`; #2 symbol deletion + DLL stage removal; #3 Debug+Release clean build;
  #4 **milestone-wide** boot under both renderers after the full DECRUFT-01..06 set).

### Prior-phase precedent (apply the lessons directly)
- `.planning/phases/14-voice-chat-vivox-source-removal/14-CONTEXT.md` — the closest analog.
  D-01 whole-family scope, D-02 de-wire-at-callsite, D-04 vendored-tree delete, D-07 editor
  `.rsp`/`.vcxproj` purge, D-08 dual-renderer boot, D-09 `/FORCE` link-grep gate, and the
  **CR-01 correction** (positional-enum ordinal hazard — directly recurs here as D-02/D-02a).
- `.planning/phases/13-videocapture-library-unlink/13-CONTEXT.md` — inline-`.vcxproj`-vs-`.rsp`
  finding, vendored-tree delete, link-grep gate.
- `.planning/PROJECT.md` §"Current Milestone: v2.1 — Decruft" — milestone framing + the invariant
  ("every step leaves the client bootable to character select under both renderers"); §Key
  Decisions "Mozilla XPCOM stub strategy" + the `/Zc:wchar_t` interlock resolved in Phase 9.
- `.planning/STATE.md` §Accumulated Context — the Phase 12 `/FORCE` false-pass finding, the
  "dead modules link via inline `.vcxproj` deps, not just `.rsp`" finding, the MSYS-sed
  `.vcxproj` purge gotcha (Phases 13-15), the CR-01 lesson (line 104), and line 82 confirming
  libMozilla is already stubbed.

### Memory (recurring gotchas — Phases 13-15)
- Memory `project_decruft_removal_build_graph_gotchas` — `.rsp` vestigial (edit inline vcxproj);
  `/FORCE` masks unresolved-externals (grep for 0, not exit 0); msbuild not on PATH (use full
  path); `/nodeReuse:false`; run from PowerShell not Git Bash; delete exe to force relink;
  editors pre-broken on Qt.
- Memory `project_radial_menu_enum_ordinal_is_datatable_row_index` — the CR-01 lesson: deletions
  from positional enums/tables MUST use ordinal-preserving placeholders unless proven safe.
  Directly governs D-02/D-02a (UITypeID).
- Memory `feedback_debug_exe_reads_client_d_cfg` — the debug exe reads `client_d.cfg` (set
  `rasterMajor` there for the boot gate).
- Memory `project_optimized_config_safeseh_pre_existing` — the SwgClient Optimized config LNK1281
  SAFESEH failure is pre-existing (DEF-14-01), not a removal regression; validate via Debug+Release
  link-grep, not Optimized image-gen.
- Memory `feedback_dont_modify_koogie_changes` — leave Koogie's uncommitted `Direct3d9.cpp` vtable
  probe in the working tree untouched.

### Reference diff template (external — sibling repo, not in this tree)
- Original **whitengold (swg-client) Phase 07** removal commits (CLEAN-01..05; XPCOM/browser was
  bundled there), retargeted CMake → MSBuild. Reference only — the active tree is the Koogie
  MSBuild tree, so paths/build-graph differ. No in-repo path; treat as a shape guide.

</canonical_refs>

<code_context>
## Existing Code Insights

### Removal surface (scouted baseline — researcher must re-grep to confirm completeness)

**Build / project / link:**
- `src/build/win32/swg.sln:1587` → `libMozilla` project, GUID `{C6C1E14A-DEDB-48C6-B028-0C9E4EFF5DC4}` (D-07)
- `src/external/3rd/library/libMozilla/build/win32/libMozilla.vcxproj` — `ConfigurationType = StaticLibrary` (no runtime DLL)
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj:158,204` → inline `libMozilla.lib` (Debug+Release `AdditionalDependencies`) — NOT in `libraries*.rsp` (D-08)

**Vendored tree (delete — D-09):**
- `src/external/3rd/library/libMozilla/` — `include/public/libMozilla/libMozilla.h` (→ `src/win32/libMozilla.h`, the real `namespace libMozilla` API: `init/update/release/createWindow/destroyWindow/setUserAgent/enableMemoryCache/enableDiskCache` + `Window`/`ICallback`/`IBlitter`), `include/private/sdk/include/nsIWebBrowser*.h`, `include/private/bin/{debug,release}/components/*.xpt`, `src/win32/libMozilla.cpp`.

**Include paths (engine + game libs):**
- `swgClientUserInterface/build/win32/includePaths.rsp:38` → `libMozilla/include/public`
- `clientGame/build/win32/includePaths.rsp:4` → `libMozilla/include/public` (with the dead `Game.cpp:147` include, D-06)

**Game-UI source — `swgClientUserInterface` (the ONLY `libMozilla::` consumers — delete):**
- `…/src/shared/core/SwgCuiWebBrowserManager.cpp/.h` + `include/public/.../SwgCuiWebBrowserManager.h` (drives `libMozilla::init/setUserAgent/enableCache/update`)
- `…/src/shared/page/SwgCuiWebBrowserWidget.cpp/.h` + public header (`libMozilla::createWindow/destroyWindow`, `ICallback`/`IBlitter` impls; holds `libMozilla::Window*`)
- `…/src/shared/page/SwgCuiWebBrowserWindow.cpp/.h` + public header

**`ui` library widget RTTI (D-02/D-02a — the CR-01 hazard):**
- `src/external/3rd/library/ui/src/shared/core/UITypeID.h:64` → `TUIWebBrowser` (mid-enum in the `TUIWidget` block; `TUIStyle` + ~30 `*Style` entries follow). `ui` is **name-keyed** (`GetTypeName`/`TypeName`, `UIStandardLoader<T>`) — RTTI ordinal, not a serialized row index (verify per D-02).

**Live callers that STAY and must be de-wired (D-05):**
- `swgClientUserInterface/.../SwgCuiManager.cpp:474/503/551` — install/remove/update lifecycle
- `swgClientUserInterface/.../SwgCuiCommandParserUI.cpp:119,176,214,564-580,1096` — `browser` + `debugBrowserOutput` console commands
- `swgClientUserInterface/.../SwgCuiHud.cpp:98,1026` + `SwgCuiHudAction.cpp` — include + `IsA(TUIWebBrowser)` focus clause
- **Engine** `clientUserInterface/.../CuiIoWin.cpp:513,647` + `IMEManager.cpp:353` — `IsA(TUIWebBrowser)` focus/IME clauses (shared by both renderers)

**TCG ties — sever only (D-03/D-04; libEverQuestTCG STAYS):**
- `swgClientUserInterface/.../SwgCuiTcgManager.cpp:19,130-148` — `navigateProc`/`navigateWithPostDataProc` callbacks open `SwgCuiWebBrowserManager` (registered with `libEverQuestTCG`)
- `swgClientUserInterface/.../SwgCuiTcgControl.h:93` — `IsA` returns `Type == TUIWebBrowser || UIWidget::IsA(Type)` (built on `libEverQuestTCG::Window`, NOT libMozilla)

**Engine — `clientGame` (dead include — D-06):**
- `clientGame/.../Game.cpp:147` — `#if DEBUG==0` guarded `#include "libMozilla/libMozilla.h"` with no `libMozilla::` calls (vestigial)

**Editor refs to purge (D-11) — out-of-scope-as-targets, refs must still go:**
- `includePaths.rsp`/`libraryPaths.rsp` (+ inline `.vcxproj`) under AnimationEditor, ClientEffectEditor,
  LightningEditor, NpcEditor, ParticleEditor, SwooshEditor, Viewer, and SwgGodClient (the latter also
  links `libMozilla.lib` inline). Researcher re-greps `--include="*.rsp" --include="*.vcxproj"` for `[Mm]ozilla`.

### Established Patterns (from Phases 12–14 — apply directly)
- **`/FORCE` false-pass:** unresolved externals become warnings, binary still emits at exit 0. Build
  gate MUST grep link output for `unresolved external symbol` (== 0), Debug AND Release.
- **Inline `.vcxproj`, not just `.rsp`:** the real compile/link wiring is inline; `.rsp` is often vestigial.
- **Live-caller de-wire:** delete the call-site lines (criterion #2 grep-zero), no no-op stub.
- **Positional-enum ordinal hazard (CR-01):** never mid-sequence-delete from a positional enum that
  may mirror a serialized index — verify, else ordinal-preserving placeholder.
- **MSYS-sed `.vcxproj` purge:** match path segments by substring+delimiter (`[^;<>]*TOKEN[^;<>]*;`),
  NOT by escaping literal backslashes (back-reference mis-parse).

### Integration / blast-radius notes for research
- `clientUserInterface` and `clientGame` are engine libs shared by **both** the D3D9 and D3D11
  renderers — both must rebuild + link clean after the browser symbols go (D-05, D-06).
- **Symbol-resolution risk:** `SwgCuiWebBrowserManager::`/`libMozilla::` symbols resolve in the deleted
  `SwgCuiWebBrowser*.cpp`s and `libMozilla.lib` — every consumer must be de-wired in the same change set
  or the link breaks (reinforces D-05 delete-at-callsite + D-08 unlink).
- **`libEverQuestTCG` callback contract (D-04a):** confirm the lib accepts no/null navigate callbacks
  before unregistering, else fall back to registered logging-no-op procs.
- **Mid-removal build health:** with ~6 deleted files + ~7 live call sites + the enum edit + the
  project/tree drop, ordering matters; keep the tree buildable at each coherent step.

</code_context>

<specifics>
## Specific Ideas

- The in-game browser is **already stubbed and dormant** (`libMozilla::init()` returns `true`,
  no XPCOM runtime, no staged DLLs, never renders — STATE.md line 82). Removal loses no working
  feature; this de-risks the *functional* side and concentrates the risk on build + enum surgery.
- The `/Zc:wchar_t` ↔ Mozilla ABI interlock that historically blocked XPCOM removal is **already
  gone** — Phase 9 (Option D) went `char16_t` — so this is a **clean unlink** (PROJECT.md / STATE.md).
- The TCG web-navigation path (`navigateProc` opening the store/loyalty URL) is dead both ways: the
  browser is stubbed AND there is no live TCG web backend on the SWGSource VM. Severing it (D-04)
  removes no user-visible function.

</specifics>

<deferred>
## Deferred Ideas

- **Bink video codec / Miles audio** — explicitly OUT of v2.1 scope (active middleware). Do not
  conflate browser removal with the live audio/video path.
- **`libEverQuestTCG` (TCG card game) removal** — TCG is dormant middleware and a *plausible* future
  decruft target, but it is NOT in the v2.1 Decruft requirement set (only XPCOM/Mozilla is). Phase 15
  severs only the browser tie; any TCG-lib removal would be a future-milestone decision.
- **DEF-14-01 — SwgClient Optimized config LNK1281 SAFESEH defect** — pre-existing, removal-unrelated
  (validate via Debug+Release link-grep, not Optimized image-gen). Carried in deferred-items.md.
- **v2.2 Visual Parity** (asset PS pipeline, gamma LUT, minimap, particles) — the next milestone;
  read `docs/research/phase12-baseline/COMPARISON.md` when it opens.

### Reviewed Todos (not folded)
- **"Diff SWGSource vs whitengold TRE for space-scene graphics artifacts"** (score 0.6) — keyword
  match only; this is a **v2.2 Visual Parity** asset/rendering investigation, unrelated to browser
  source removal. Deferred (same disposition as Phase 14).
- **"Cantina interior corner-snap engine quirk"** (score 0.2) — rendering/collision; pre-existing
  SOE engine quirk, not in Decruft scope. Deferred.

</deferred>

---

*Phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate*
*Context gathered: 2026-05-26*
