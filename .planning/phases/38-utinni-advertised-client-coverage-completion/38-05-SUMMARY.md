---
phase: 38-utinni-advertised-client-coverage-completion
plan: 05
subsystem: infra
tags: [utinni, getenginehookpoints, detour, address-correctness, msvc-abi, mi-pmf, pmf-delta, win32, gap-closure]

# Dependency graph
requires:
  - phase: 38-utinni-advertised-client-coverage-completion
    provides: "94-row GetEngineHookPoints table at VERSION 2 (38-01 groundScene forwarders + friend-decl mechanism; 38-03 chatWindow MI thunks; 38-04 byte-exact .h/.inc re-sync to D:/Code/Utinni)"
provides:
  - "4 DETOURED endpoints re-advertised by their REAL engine code entry (not a call-through forwarder thunk)"
  - "pmfRealEntry() helper with a delta==0 hard gate (DEBUG_FATAL + nullptr on non-zero -> verifyNoNullNoDup catches a silent-dead entry)"
  - "2 in-TU GroundScene.cpp real-entry accessors (friend) for the private detoured methods (update, handleInputMapEvent)"
  - "UTINNI_HOOKPOINTS_VERSION 3 (address-correctness; same 94-name set) re-synced byte-identical to D:/Code/Utinni"
affects: [utinni]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "DETOURED MI-class endpoint -> advertise the REAL engine entry: extract the MI-PMF code component (pfn @ offset 0) with a delta==0 HARD GATE; a call-through forwarder is silently dead for a detour"
    - "DETOURED private MI method -> in-TU real-entry accessor (friend) returning the verified code component; PUBLIC -> pmfRealEntry(&Class::m) inline"
    - "CALLED endpoint -> keep the call-through __fastcall forwarder (correct there); never use pmfRealEntry for a called row"
    - "address-correctness change behind an UNCHANGED name set still bumps UTINNI_HOOKPOINTS_VERSION (the 'what's behind a name moved' signal)"

key-files:
  created:
    - .planning/handoff/2026-06-23-utinni-detour-entries-fixed.md
  modified:
    - src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp
    - src/game/client/application/SwgClient/src/win32/utinni_groundScene_forward.h
    - src/game/client/application/SwgClient/src/shared/utinni_engine_hookpoints.h
    - src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp
    - src/engine/client/library/clientGame/src/shared/scene/GroundScene.h
  resynced-no-commit:
    - D:/Code/Utinni/UtinniCore/swg/utinni_engine_hookpoints.h
    - D:/Code/Utinni/UtinniCore/swg/utinni_engine_hookpoints.inc

key-decisions:
  - "The 4 DETOURED rows (groundScene::{update,handleInputMapEvent}, cuiChatWindow::{enableTextInput,chatEnterHandler}) advertise the REAL engine entry, not a forwarder — a DetourXS patch on a forwarder never fires (the engine calls the real method directly). The 6 CALLED rows keep their forwarders (correct there)."
  - "delta==0 is the safety gate: all 4 targets are OWN non-virtual most-derived methods (delta 0 -> pfn IS the entry). pmfRealEntry/accessors DEBUG_FATAL + return nullptr on non-zero delta so utinni_verifyNoNullNoDup() catches a wrong/silent-dead entry as a null row."
  - "UTINNI_HOOKPOINTS_VERSION 2->3 is an ADDRESS-CORRECTNESS bump (same 94 names) — consumer required-set unaffected; the bump-policy comment was extended to cover address-behind-name changes."
  - "4 orphaned forwarders removed (the 2 groundScene + 2 chat call-throughs that only fed the swapped rows) — verified no other referencer."

patterns-established:
  - "Pattern: DETOURED endpoint advertisement = real engine code entry (MI-PMF code component, delta==0 hard-gated), NOT a call-through forwarder. The forwarder is correct ONLY for CALLED endpoints."

requirements-completed: [EPA-05]

# Metrics
duration: ~50min
completed: 2026-06-22
---

# Phase 38 Plan 05: Detour Address-Correctness Summary

**The 4 DETOURED Utinni endpoints (groundScene::{update,handleInputMapEvent}, cuiChatWindow::{enableTextInput,chatEnterHandler}) now advertise their REAL engine code entry instead of a call-through forwarder thunk — a detour on a forwarder is silently dead (the engine calls the real method directly). delta==0-hard-gated extraction (pmfRealEntry + 2 in-TU GroundScene.cpp friend accessors); 6 CALLED rows keep their forwarders; 4 orphaned forwarders removed; UTINNI_HOOKPOINTS_VERSION 2->3 (same 94 names); .h/.inc re-synced byte-identical to D:/Code/Utinni — Debug+Release link 0 unresolved, undecorated GetEngineHookPoints on both exes, static_assert(94==94) holds.**

## Performance

- **Duration:** ~50 min
- **Completed:** 2026-06-22
- **Tasks:** 1 atomic implementation commit (build-gate autonomous; boot-smoke DEFERRED)
- **Files modified:** 5 source (+1 handoff created; 2 Utinni files re-synced, not committed)

## Accomplishments

- **`pmfRealEntry` helper** (utinni_advertise.cpp): extracts the MSVC 32-bit MI-PMF code component (`{ void* pfn; int delta; ... }`, `pfn` @ offset 0) with a **delta==0 hard gate** — on non-zero delta it `DEBUG_FATAL`s and returns `nullptr` so `utinni_verifyNoNullNoDup()` catches it as a null row and FAILS loudly. Never advertises a wrong / silent-dead entry.
- **groundScene::{update,handleInputMapEvent}** (PRIVATE) re-advertised via two in-TU real-entry accessors in GroundScene.cpp (`utinni_groundScene*RealEntry()`), friend-declared in GroundScene.h (ABI-neutral, Win32-only) — the same delta==0 extraction inlined because a private PMF can only be taken in the class's own TU.
- **cuiChatWindow::{enableTextInput->acceptTextInput, chatEnterHandler->performEnterKey}** (PUBLIC) re-advertised via `pmfRealEntry(&SwgCuiChatWindow::...)` inline in the table.
- **6 CALLED rows kept their call-through forwarders** (groundScene init/reloadTerrain/changeCamera/getCurrentCamera/handleInputMapUpdate + chat writeToAllTabs/writeToCurrentTab); `client::*` shims + `config::*` rows untouched.
- **4 orphaned forwarders removed** (the 2 groundScene + 2 chat call-throughs that only fed the swapped rows) — grep-verified no other referencer; their friend decls + header decls removed in lockstep.
- **UTINNI_HOOKPOINTS_VERSION 2 -> 3** (address-correctness; same 94-name set, `static_assert(94==94)` holds); bump-policy comment extended to cover address-behind-name changes.
- **.h/.inc re-synced byte-identical** to `D:/Code/Utinni/UtinniCore/swg/` (`.h` now VERSION 3; `.inc` unchanged — sha256 `501d88…` matches 38-04). No Utinni repo commit.
- **Response note** written: `.planning/handoff/2026-06-23-utinni-detour-entries-fixed.md`.

## Task Commits

1. **Address-correctness fix (4 detoured rows -> real entry + helper + version bump)** — `0d5726c6d` (fix)

**Plan metadata** (this SUMMARY + STATE/ROADMAP) committed separately.

## Build-Gate Evidence (AUTONOMOUS pass condition)

- **Debug** (SwgClient.vcxproj /p:Configuration=Debug /p:Platform=Win32, `/nodeReuse:false`, serial; clientGame.lib Debug rebuilt first, staged exe deleted to force relink): `unresolved external symbol` = **0**; no `error C`; no LNK1120/LNK1181. `SwgClient_d.exe` built (70 MB, fresh) + staged.
- **Release** (same flags, Configuration=Release; clientGame.lib Release rebuilt first): `unresolved external symbol` = **0**; no `error C`. `SwgClient_r.exe` built (28 MB, fresh) + staged.
- **dumpbin /exports** (VS18 BuildTools x64 dumpbin): `SwgClient_d.exe` -> `GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ord 52); `SwgClient_r.exe` -> `GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ord 51).
- **static_assert** `(sizeof s_engineHookPoints / sizeof[0]) == UTINNI_REQUIRED_COUNT`: held — both Debug and Release compiled (table 94 rows == `.inc` 94 names; name set UNCHANGED this plan).
- **Grep checks:** 94 `{ "` table rows; 94 `UTINNI_HOOKPOINT(` names; 4 `RealEntry`/`pmfRealEntry` table rows; 0 remaining source refs to the removed forwarder symbols (only a comment mention).
- **Byte-identity:** `cmp` + `sha256sum` confirm the re-synced `.h` (`4adfdb…`, VERSION 3) and `.inc` (`501d88…`, unchanged) are byte-identical across both repos.

**Boot-smoke + live-inject:** DEFERRED to the maintainer's consolidated pass — the gl05 char-select boot, the runtime `utinni_verifyNoNullNoDup()` PASS at **version 3**, the gl11 window-up non-regression, and the live inject-smoke (the four detours now fire on the real engine entries) were NOT run and are NOT claimed here. The build-gate above is the autonomous pass condition. Note: `utinni_verifyNoNullNoDup()` would FAIL loudly at boot if any of the four real-entry extractions hit a non-zero delta (the delta==0 gate turns a silent-dead detour into an observable boot failure).

## Decisions Made

- **Forwarder vs real entry is mechanism-specific.** A call-through forwarder is correct ONLY for CALLED endpoints (Utinni invokes it). For DETOURED endpoints the engine calls the real method directly, so the advertised address MUST be the real engine entry — the Utinni review finding (`2026-06-22-utinni-detour-vs-call-followup.md`), independently corroborated by Codex.
- **delta==0 is a hard safety gate, not an assumption.** All four targets are OWN non-virtual most-derived methods (primary base @ offset 0 -> delta 0 -> `pfn` is the entry). The gate verifies it at extraction; a non-zero delta (a secondary-base method whose `this` needs adjustment, not directly detour-able) yields nullptr + DEBUG_FATAL, never an advertised wrong entry.
- **Version bump for an unchanged name set.** 2->3 signals the addresses behind four names moved; the consumer required-set is name-keyed and unaffected.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed pre-existing C4099 struct/class tag mismatch on `IoEvent`**
- **Found during:** SwgClient Debug build (compiling utinni_advertise.cpp via the forward header).
- **Issue:** `utinni_groundScene_forward.h` forward-declared `class IoEvent;` but `IoEvent` is a `struct` in-tree (sharedIoWin / InputMap.h) — C4099 warning (pre-existing from 38-01, surfaced because this plan edits the same header).
- **Fix:** Changed the forward declaration to `struct IoEvent;` to match the in-tree tag.
- **Files modified:** src/game/client/application/SwgClient/src/win32/utinni_groundScene_forward.h
- **Verification:** Release build log shows no C4099/IoEvent warning.
- **Committed in:** 0d5726c6d

**Total deviations:** 1 auto-fixed (Rule 1 - cosmetic warning cleanup, in-scope because this plan edits the same header).
**Impact on plan:** None on scope — a one-token tag correction. No new rows; name set unchanged.

## Issues Encountered

- The SwgClient link runs under `/FORCE` with a large `/VERBOSE` symbol-resolution dump that overflowed the Tee'd Debug log mid-stream. Mitigated by grepping the decoded log for the link gate directly (`unresolved external symbol` = 0) and confirming the fresh exe + undecorated export via dumpbin — the exe and export are the ground truth, not the truncated log tail. (Build-log encoding is UTF-16LE under PowerShell Tee; decoded with iconv.)

## Threat Flags

None — no new security-relevant surface. The change is address-correctness on an already-advertised, read-only export: four rows re-pointed from a forwarder to the real engine entry behind the SAME contract names. The delta==0 hard gate strictly tightens correctness (a wrong entry now fails loudly at boot instead of silently). No new endpoints, no schema/network/file surface.

## Next Phase Readiness

- The 4 detoured endpoints are now real-entry advertised; Utinni can wire their bindings (detours will fire). Response note: `.planning/handoff/2026-06-23-utinni-detour-entries-fixed.md`.
- **Handback reminder for the deferred `cuiChatWindow::ctor`:** when un-deferred, it is DETOURED (`hkCtor`) -> must be the REAL ctor entry, NOT a placement-new thunk (a placement-new thunk would be detour-dead like the four fixed here). Recorded in the response note §4.
- **Maintainer pass pending:** gl05 char-select boot + `utinni_verifyNoNullNoDup()` PASS at version 3 + gl11 window-up non-regression + live inject-smoke.

## Self-Check: PASSED

- FOUND: src/game/client/application/SwgClient/src/win32/utinni_advertise.cpp (pmfRealEntry + 4 real-entry rows)
- FOUND: src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp (2 real-entry accessors)
- FOUND: .planning/handoff/2026-06-23-utinni-detour-entries-fixed.md
- FOUND: .planning/phases/38-utinni-advertised-client-coverage-completion/38-05-SUMMARY.md
- FOUND commit 0d5726c6d (address-correctness fix)
- VERIFIED: D:/Code/Utinni .h sha256 4adfdb… (VERSION 3) == provider; .inc sha256 501d88… == provider (byte-identical)

---
*Phase: 38-utinni-advertised-client-coverage-completion*
*Completed: 2026-06-22*
