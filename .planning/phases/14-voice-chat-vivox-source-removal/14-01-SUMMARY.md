---
phase: 14-voice-chat-vivox-source-removal
plan: 01
subsystem: infra
tags: [vivox, voice-chat, decruft, msbuild, vcxproj, cuipreferences, sharednetworkmessages, link-gate]

# Dependency graph
requires:
  - phase: 13-videocapture-library-unlink
    provides: atomic closed-chain delete pattern + /FORCE link-grep gate + MSYS-sed vcxproj-path gotcha
provides:
  - Vivox voice-chat subsystem fully removed from source and build (~24 files deleted)
  - All live voice callers + in-repo registrations de-wired (no stubs)
  - 6 CuiPreferences voice keys stripped (canonical src/shared/core, not shims)
  - vivox/VChatAPI/Base_vchat/libsndfile link tokens removed from SwgClient.vcxproj (all 3 configs)
  - inline vivox include-dirs removed from clientUserInterface + clientGame vcxproj
affects: [14-02, 14-03, phase-15-xpcom-removal]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Atomic closed-chain delete: de-wire callers + delete source + remove ClCompile + unlink libs as ONE non-buildable-intermediate unit, gated only at the end (mirrors Phase 13 D-01)"
    - "Canonical-header rule: edit src/shared/*.h (real declarations); the include/public/*.h 1-line shims auto-follow"
    - "Link gate = grep link log for 'unresolved external symbol' == 0, NOT MSBuild exit 0 (/FORCE masks)"

key-files:
  created:
    - .planning/phases/14-voice-chat-vivox-source-removal/deferred-items.md
  modified:
    - src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp
    - src/engine/client/library/clientUserInterface/src/shared/core/CuiMenuInfoTypes.h
    - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj

key-decisions:
  - "Optimized-config SAFESEH link failure (LNK1281) is a pre-existing config defect unrelated to voice removal — deferred, not fixed in this plan"
  - "Removed commented-out voice dispatch blocks (SwgCuiHudAction 1596-1604, SwgCuiOpt, SwgCuiCommandParserDefault) to satisfy grep-zero, beyond the literal de-wire action"
  - "Removed orphaned hash_report static + its empty else-if branch in ClientCommandQueue (existed only for the voice CS-report path)"

patterns-established:
  - "3-config link gate: build Debug + Optimized + Release; gate on per-log unresolved-external count, not exit code"

requirements-completed: [DECRUFT-05]

# Metrics
duration: 163min
completed: 2026-05-26
---

# Phase 14 Plan 01: Voice Chat (Vivox) Source Removal Summary

**Atomically deleted the entire Vivox voice-chat subsystem (~24 source files + 3 voicechat network messages), de-wired all 10 live callers and 5 in-repo registration sites, stripped the 6 CuiPreferences voice keys, and unlinked vivox/VChatAPI/Base_vchat/libsndfile from SwgClient — Debug + Release link clean with 0 unresolved externals.**

## Performance

- **Duration:** ~163 min (dominated by serial Optimized + Release optimized-config builds)
- **Started:** 2026-05-26T15:34:07Z
- **Completed:** 2026-05-26T18:17:39Z
- **Tasks:** 3 (Sub-steps A, B, C+gate — committed as ONE atomic unit per D-02a)
- **Files modified:** 23 modified + 31 deleted = 54 plan files

## Accomplishments
- Deleted all voice source: CuiVoiceChatManager/EventHandler/Glue (clientUserInterface), SwgCuiVoiceFlyBar(+MessageQueue)/SwgCuiVoiceActiveSpeakers(+_TableModel)/SwgCuiOptVoice/SwgCuiCommandParserVoice (swgClientUserInterface), VoiceChatChannelInfo/MiscMessages/OnGetAccount (sharedNetworkMessages), plus all public re-include shims.
- De-wired every surviving caller by deleting the call site (no stub): CuiManagerManager (install/remove/update), CuiRadialMenuManager (VOICE_INVITE radial), CommandCppFuncs (voiceInvite/voiceKick funcs + addCppFunction bindings), ClientCommandQueue + CustomerServiceManager (CS-report append), SwgCuiHudAction (toggleVoiceFlyBar action + dispatch), SwgCuiStatusGround (updateVoiceChatIcon), SwgCuiHudWindowManager (getVoiceShowFlybar/WS_VoiceFlyBar — D-03a baseline miss), SwgCuiOpt + SwgCuiCommandParserDefault (includes).
- Stripped 6 CuiPreferences voice keys (bools, REGISTER_OPTION_USER, 12 getters/setters, default-disable block, include) from the canonical src/shared/core file; verified the canonical header (not the shim) was emptied.
- Removed in-repo registrations: MAKE_SWG_CTOR_WS(VoiceFlyBar/VoiceActiveSpeakers), 3 radial enum values (VOICE_SHORTLIST_REMOVE/VOICE_INVITE/VOICE_KICK) + their MAKE_IDs, MAKE_MEDIATOR_TYPE(WS_VoiceFlyBar/WS_VoiceActiveSpeakers), MAKE_ACTION(toggleVoiceFlyBar/toggleVoiceActiveSpeakers).
- Removed voice ClCompile/ClInclude entries from 3 vcxproj, inline vivox include-dirs from clientUserInterface+clientGame vcxproj, and vivox/VChatAPI/Base_vchat/libsndfile link tokens from SwgClient.vcxproj across all 3 configs (kept soePlatform\libs dirs).

## Task Commits

Per D-02a, Sub-steps A (de-wire + prefs) and B (delete files + ClCompile) are NON-BUILDABLE intermediate states — NOT individually committed. The whole atomic unit (A+B+C) is ONE commit after the build gate:

1. **Atomic unit (Sub-steps A+B+C + build gate)** - `eb9b68987` (feat)

**Plan metadata:** _(pending final docs commit)_

## Files Created/Modified

**Deleted (31 voice files):** CuiVoiceChat{Manager,EventHandler}.cpp/.h, CuiVoiceChatGlue.h + public shim; SwgCuiVoiceFlyBar(+MessageQueue)/SwgCuiVoiceActiveSpeakers(+_TableModel)/SwgCuiOptVoice.cpp/.h + parser + 5 public shims; 3 voicechat/*.cpp/.h + 3 public shims.

**Modified — callers/registrations/prefs (18):**
- `clientUserInterface/.../CuiManagerManager.cpp` - removed install/remove/update + include
- `clientUserInterface/.../CuiRadialMenuManager.cpp` - removed VOICE_INVITE radial block + include
- `clientUserInterface/.../CuiPreferences.cpp` + `.h` - stripped 6 voice keys (canonical)
- `clientUserInterface/.../CuiMenuInfoTypes.cpp` + `.h` - removed 3 voice enum values + MAKE_IDs
- `clientGame/.../CommandCppFuncs.cpp` - removed voice funcs + addCppFunction bindings + include
- `clientGame/.../ClientCommandQueue.cpp` - removed CS-report voice append + orphaned hash_report + include
- `clientGame/.../CustomerServiceManager.cpp` - removed CS-report voice append + include
- `swgClientUserInterface/.../SwgCuiHudAction.cpp` - removed action + dispatch + include
- `swgClientUserInterface/.../SwgCuiStatusGround.cpp` + `.h` - removed updateVoiceChatIcon + call + include
- `swgClientUserInterface/.../SwgCuiHudWindowManager.cpp` - removed getVoiceShowFlybar/WS_VoiceFlyBar (D-03a)
- `swgClientUserInterface/.../SwgCuiOpt.cpp` - removed OptVoice include + commented refs
- `swgClientUserInterface/.../SwgCuiCommandParserDefault.cpp` - removed include + commented ref
- `swgClientUserInterface/.../SwgCuiMediatorFactorySetup.cpp` - removed 2 ctor registrations + includes
- `swgClientUserInterface/.../SwgCuiMediatorTypes.h` - removed WS_VoiceFlyBar/WS_VoiceActiveSpeakers
- `swgClientUserInterface/.../SwgCuiActions.h` - removed toggleVoiceFlyBar/toggleVoiceActiveSpeakers

**Modified — build (5 vcxproj):** clientUserInterface, clientGame, swgClientUserInterface, sharedNetworkMessages (ClCompile/ClInclude + inline include-dirs); SwgClient (link tokens, 3 configs).

## Decisions Made
- **Optimized SAFESEH defer:** The Optimized config fails to link with LNK1281/LNK2026 (SAFESEH). This is a pre-existing config gap (Optimized `<Link>` lacks the `/SAFESEH:NO` Debug has and the `ImageHasSafeExceptionHandlers=false` Release has) and is fully independent of voice removal — 0 unresolved externals, 0 voice symbols in the error log. Logged to deferred-items.md (DEF-14-01); not fixed here.
- **Grep-zero over literal-action:** The plan's per-file action text said "delete the include only" for SwgCuiOpt / SwgCuiCommandParserDefault and "delete line 286" for SwgCuiHudAction, but the plan's own acceptance grep + criterion #2 require zero voice symbols. Removed the commented-out voice dispatch/refs too (SwgCuiHudAction 1596-1604, SwgCuiOpt OT_voice/pageVoice, SwgCuiCommandParserDefault 442).
- **Orphaned hash_report:** ClientCommandQueue's `hash_report` static + its `else if` branch existed only to feed the voice CS-report append; removed the whole branch (de-wire intent, no dangling voice-purpose code).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug / grep-zero correctness] Removed commented-out voice dispatch + refs beyond the literal de-wire action**
- **Found during:** Task 1 (Sub-step A)
- **Issue:** Plan action text was minimal ("delete include only" / "delete line 286"), but the plan's acceptance grep and phase criterion #2 require ZERO voice symbols (incl. `toggleVoiceFlyBar`, `WS_VoiceFlyBar`, `CommandParserVoice`). Commented blocks at SwgCuiHudAction.cpp:1596-1604, SwgCuiOpt.cpp (OT_voice/pageVoice), SwgCuiCommandParserDefault.cpp:442 would have failed grep-zero.
- **Fix:** Deleted the commented-out voice dispatch/enum/refs in those 3 files.
- **Files modified:** SwgCuiHudAction.cpp, SwgCuiOpt.cpp, SwgCuiCommandParserDefault.cpp
- **Verification:** GATE-2 grep over all 4 libraries returns 0 matches.
- **Committed in:** eb9b68987

**2. [Rule 1 - Bug] Removed orphaned hash_report static + empty else-if branch in ClientCommandQueue**
- **Found during:** Task 1 (Sub-step A)
- **Issue:** Deleting the voice CS-report append left `else if (command.m_commandHash == hash_report) { }` empty and `hash_report` (commented "for Vivox integration phase 1") orphaned.
- **Fix:** Removed the whole else-if branch and the hash_report declaration.
- **Files modified:** ClientCommandQueue.cpp
- **Verification:** clientGame Debug + Release compile clean (0 unresolved).
- **Committed in:** eb9b68987

---

**Total deviations:** 2 auto-fixed (both Rule 1 — grep-zero/dead-code correctness). No scope creep — all within the voice-removal envelope.
**Impact on plan:** Both necessary to satisfy the plan's own grep-zero acceptance and leave no dangling voice-purpose code. Plan executed as written otherwise.

## Issues Encountered
- **Optimized-config SAFESEH link failure (LNK1281):** Discovered during the build gate. Optimized produced 0 unresolved externals (voice graph clean) but failed SAFESEH image generation due to non-SAFESEH prebuilt 3rd-party libs (zlib, dinput8, libpcre) and a missing `/SAFESEH:NO` in the Optimized linker block. Pre-existing, unrelated to voice; Optimized had never been built in this repo before (no output dir existed). Logged DEF-14-01; the D-09 gate (0 unresolved externals) is met across all 3 configs. Debug + Release — the repo's validated configs — both link clean and produce exes.
- **Background-build task fragility:** The orchestration shell killed long background builds twice (Optimized + Release interrupted mid-compile). Re-ran each config to completion with fresh exe deletion + relink; final Debug, Optimized (compile), and Release all reached their link step.

## Build Gate Results (D-09)

| Config | unresolved external symbol | Exe produced | Verdict |
|--------|---------------------------|--------------|---------|
| Debug | 0 | SwgClient_d.exe (69.9 MB, 11:07) | PASS |
| Optimized | 0 | none (LNK1281 SAFESEH — pre-existing, DEF-14-01) | symbol-clean; link-config defect deferred |
| Release | 0 | SwgClient_r.exe (28.7 MB, 13:11) | PASS |

D-09 core criterion (0 `unresolved external symbol`, grep-confirmed not /FORCE exit 0) satisfied in ALL THREE configs. soePlatform\libs dirs preserved (Base.lib present).

## Decisions Satisfied
D-01 (whole subsystem), D-02 (delete call sites, no stub), D-02a (one atomic unit, no per-sub-step commit), D-03 (canonical CuiPreferences strip), D-03a (SwgCuiHudWindowManager de-wired), D-06 (all in-repo registrations incl. VOICE_SHORTLIST_REMOVE/VOICE_INVITE/VOICE_KICK), D-09 (link-grep gate, 0 unresolved Debug+Optimized+Release).

## Next Phase Readiness
- 14-02 (residue purge: vestigial .rsp libs/paths, includePaths.rsp, dangling swgClientVivox, 8 editor .rsp) is unblocked — the source/link unit is removed and grep-clean across the 4 libraries.
- 14-03 (D-10: delete vendored vivox/ + vivoxSharedWrapper/ + soePlatform/VChatAPI/ trees) is unblocked — no .vcxproj include-dir references the vivox trees anymore.
- Carry-forward concern: the Optimized SwgClient config (DEF-14-01) needs a `/SAFESEH:NO` / `ImageHasSafeExceptionHandlers=false` fix before it can ever produce an exe — separate from Decruft.

## Self-Check: PASSED

- FOUND: .planning/phases/14-voice-chat-vivox-source-removal/14-01-SUMMARY.md
- FOUND: .planning/phases/14-voice-chat-vivox-source-removal/deferred-items.md
- FOUND: commit eb9b68987
- CONFIRMED deleted: CuiVoiceChatManager.cpp (sentinel of 31 voice files)
- GATE-2 voice-symbol grep == 0 across clientUserInterface, clientGame, swgClientUserInterface, sharedNetworkMessages
- Debug + Release link logs: 0 unresolved external symbols

---
*Phase: 14-voice-chat-vivox-source-removal*
*Completed: 2026-05-26*
