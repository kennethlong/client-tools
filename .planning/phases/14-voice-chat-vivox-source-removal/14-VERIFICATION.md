---
phase: 14-voice-chat-vivox-source-removal
verified: 2026-05-26T21:00:00Z
status: passed
score: 9/9 must-haves verified
overrides_applied: 0
---

# Phase 14: Voice Chat (Vivox) Source Removal Verification Report

**Phase Goal:** Fully remove the Vivox voice-chat subsystem from source and build — the link, the manager/UI source, and the voice preference keys — without breaking the live CuiPreferences surface.
**Verified:** 2026-05-26
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths (ROADMAP Success Criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| SC-1 | `vivoxSharedWrapper_debug.lib` unlinked; grep finds zero `vivox`/`Vivox` across `.rsp`, source, and include paths | VERIFIED | `rg -i "vivox" src/ --files-with-matches` → exit 1 (0 matches). All .rsp files clean. SwgClient.vcxproj, all 8 editor .vcxproj clean. |
| SC-2 | `CuiVoiceChatManager.cpp/.h`, `SwgCuiVoiceFlyBar.cpp/.h`, `CuiVoiceChatEventHandler` deleted; zero references to those symbols in the build | VERIFIED | All 31 voice source files absent on disk. GATE-2 over clientUserInterface/clientGame/swgClientUserInterface/sharedNetworkMessages → exit 1 (0 matches). |
| SC-3 | Voice preference keys stripped from `CuiPreferences`; no remaining caller references a removed voice symbol or key | VERIFIED | `rg "getVoice\|setVoice\|ms_voice\|REGISTER_OPTION_USER.*voice"` over CuiPreferences.cpp + .h → exit 1. GATE-2 over 4 source library trees → exit 1. |
| SC-4 | `swg.sln` builds clean (Debug and Release) with Vivox gone | VERIFIED | SUMMARY records Debug: 0 unresolved externals, SwgClient_d.exe 69.9 MB; Release: 0 unresolved externals, SwgClient_r.exe 28.7 MB. Link-grep (not MSBuild exit 0) confirmed. |
| SC-5 | Client boots to character select under both `rasterMajor=5` (D3D9) and `rasterMajor=11` (D3D11) | VERIFIED | 14-03-SUMMARY.md records user-confirmed PASS — "approved" signal received. client_d.cfg confirmed at rasterMajor=11 (`rg -n "rasterMajor" stage/client_d.cfg` → line 37: `rasterMajor=11`). |

**Score:** 5/5 ROADMAP success criteria verified.

---

### Plan Must-Haves (Merged from All Three Plans)

| # | Must-Have | Status | Evidence |
|---|-----------|--------|----------|
| D-01 | Whole voice subsystem deleted (~24 files): manager, handler, glue, pages, parser, voicechat network messages | VERIFIED | CuiVoiceChatManager.cpp/.h, CuiVoiceChatEventHandler.cpp/.h, CuiVoiceChatGlue.h, 8 swgClientUserInterface page/parser files, 3 voicechat/ network messages + all public shims absent on disk. voicechat/ directory absent. |
| D-02/D-02a | Every surviving live caller de-wired (no stubs); ONE atomic build-gated commit | VERIFIED | GATE-2 over clientUserInterface/clientGame/swgClientUserInterface/sharedNetworkMessages → 0 matches. Single commit eb9b68987 for the atomic A+B+C unit. |
| D-03/D-03a | 6 CuiPreferences voice keys stripped from canonical src/shared/core; SwgCuiHudWindowManager.cpp:318-319 de-wired | VERIFIED | Zero ms_voice/getVoice/setVoice/REGISTER_OPTION_USER(voice) in CuiPreferences.cpp + .h. Zero getVoiceShowFlybar/WS_VoiceFlyBar in SwgCuiHudWindowManager.cpp. |
| D-06 | All in-repo voice registrations removed: MAKE_SWG_CTOR_WS(VoiceFlyBar/VoiceActiveSpeakers), addCppFunction(voiceInvite/voiceKick), THREE voice radial enum values + MAKE_IDs, MAKE_MEDIATOR_TYPE, MAKE_ACTION | VERIFIED | Zero matches in SwgCuiMediatorTypes.h, SwgCuiActions.h, SwgCuiMediatorFactorySetup.cpp, CuiMenuInfoTypes.h/.cpp for all targeted tokens. CR-01 fix present: RESERVED_RADIAL_SLOT_103/104/105 placeholders at lines 174-176 preserve ITEM_EQUIP_APPEARANCE=106. |
| D-05/D-07 | Engine-lib includePaths.rsp purged of vivox+swgClientVivox; all 8 editor .rsp + 8 editor .vcxproj inline refs purged | VERIFIED | `rg -i "vivox|swgClientVivox"` over 6 .rsp files → exit 1. `rg -i "vivox|VChatAPI|Base_vchat|libsndfile"` over 16 editor .rsp + 8 editor .vcxproj → exit 1. |
| D-04/D-10 | Vendored SDK trees vivox/, vivoxSharedWrapper/, soePlatform/VChatAPI/ deleted; soePlatform/libs/ + ChatAPI2/ preserved | VERIFIED | All three tree paths absent on disk. soePlatform/libs/Win32-Debug/Base.lib present. soePlatform/ChatAPI2/ present. |
| D-09 | SwgClient.vcxproj inline vivox/VChat/libsndfile tokens removed (all 3 configs); Debug/Optimized/Release link-grep 0 unresolved externals | VERIFIED | `rg -in "vivox|VChatAPI|Base_vchat|libsndfile|swgClientVivox"` over SwgClient.vcxproj + clientUserInterface.vcxproj + clientGame.vcxproj → exit 1. SUMMARY records 0 unresolved externals in all 3 configs (Optimized SAFESEH failure is DEF-14-01 — pre-existing config defect, not voice-related, 0 voice symbols in error log). |
| D-08/DECRUFT-07 (incremental) | Dual-renderer boot to character select (rasterMajor=5 and =11); no voice surfaces appear | VERIFIED | 14-03-SUMMARY.md: user typed "approved"; no crash/missing-symbol assert; options/HUD/radial surfaces load; no voice surfaces under either renderer. rasterMajor=11 confirmed in stage/client_d.cfg line 37. |
| copy-libs.bat | Stale VChatAPI copy lines removed from soePlatform/copy-libs.bat | VERIFIED | `rg -c "VChatAPI" src/external/3rd/library/soePlatform/copy-libs.bat` → exit 1 (0 matches). |

**Score:** 9/9 plan must-haves verified.

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp` | Voice keys/registrations/getters/setters/default-disable block stripped | VERIFIED | Zero ms_voice/getVoice/setVoice/REGISTER_OPTION_USER(voice) matches |
| `src/engine/client/library/clientUserInterface/src/shared/core/CuiMenuInfoTypes.h` | 3 voice enum values removed; ordinals preserved via RESERVED_RADIAL_SLOT_103..105 | VERIFIED | VOICE_SHORTLIST_REMOVE/VOICE_INVITE/VOICE_KICK absent; RESERVED_RADIAL_SLOT_103/104/105 at lines 174-176; ITEM_EQUIP_APPEARANCE // 106 at line 178 |
| `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` | vivox/VChat/libsndfile link tokens removed across all 3 configs | VERIFIED | `rg -in "vivox|VChatAPI|Base_vchat|libsndfile"` → exit 1 (0 matches) |
| `src/engine/client/application/AnimationEditor/build/win32/AnimationEditor.vcxproj` | Editor vivoxSharedWrapper tokens removed (all 3 configs) | VERIFIED | GATE-1 over all 8 editor .vcxproj → exit 1 |
| `src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj` | vivox/VChat/libsndfile tokens removed; soePlatform\libs preserved | VERIFIED | GATE-1/GATE-2 over SwgGodClient.vcxproj → exit 1; soePlatform\libs dir preserved |
| `src/external/3rd/library/soePlatform/libs` | PRESERVED — Base.lib/ChatAPI.lib non-voice libs still present | VERIFIED | `src/external/3rd/library/soePlatform/libs/Win32-Debug/Base.lib` exists on disk |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| Surviving callers (CuiManagerManager, CuiRadialMenuManager, etc.) | CuiVoiceChatManager (deleted) | All call sites deleted | VERIFIED | GATE-2 over 4 source library trees → 0 matches |
| CuiPreferences voice keys | CuiVoiceChatManager.h include | Include deleted; 12 getters/setters removed | VERIFIED | Zero getVoice/setVoice in CuiPreferences.cpp + .h |
| SwgClient.vcxproj AdditionalDependencies | vivox/VChat libs | Inline tokens removed all 3 configs | VERIFIED | rg exit 1 on SwgClient.vcxproj |
| 8 editor .vcxproj | vivoxSharedWrapper link tokens | Inline tokens removed all 3 configs | VERIFIED | rg exit 1 on all 8 editor .vcxproj |
| vendored tree deletion | repo-wide GATE-1 grep-zero | vivox/vivoxSharedWrapper/VChatAPI trees deleted | VERIFIED | `rg -i "vivox" src/` → exit 1 (0 matches) |
| CR-01 fix | radial_menu.iff datatable alignment | RESERVED_RADIAL_SLOT_103/104/105 placeholders | VERIFIED | commit 1bfeff6b3; ITEM_EQUIP_APPEARANCE=106 confirmed in CuiMenuInfoTypes.h line 178 |

---

### Data-Flow Trace (Level 4)

Not applicable. This phase is a dead-code removal (deleting source files and build tokens), not a feature that renders dynamic data.

---

### Behavioral Spot-Checks

| Behavior | Result | Status |
|----------|--------|--------|
| GATE-1: `rg -i "vivox" src/` repo-wide | exit 1 (0 matches) | PASS |
| GATE-2: voice symbol set over 4 source library trees | exit 1 (0 matches) | PASS |
| GATE-2: voice symbol set repo-wide | 4 matches, all in soePlatform/ChatAPI2/ (DEF-14-02 — benign) | PASS (known disposition) |
| CR-01 fix: RESERVED_RADIAL_SLOT_103/104/105 present | 3 lines at CuiMenuInfoTypes.h:174-176; ITEM_EQUIP_APPEARANCE // 106 at :178 | PASS |
| Deleted files absent: CuiVoiceChatManager.cpp, SwgCuiVoiceFlyBar.cpp, VoiceChatOnGetAccount.cpp, voicechat/ dir | All absent on disk | PASS |
| SwgClient.vcxproj link tokens clean | `rg -in "vivox|VChatAPI|Base_vchat|libsndfile"` → exit 1 | PASS |
| .rsp files clean (SwgClient + includePaths.rsp) | `rg -i "vivox|swgClientVivox"` → exit 1 | PASS |
| soePlatform/libs/Base.lib preserved | File exists on disk | PASS |
| ChatAPI2/ preserved | Directory exists on disk | PASS |
| client_d.cfg end state rasterMajor=11 | Line 37 confirmed | PASS |
| Dual-renderer boot gate | User-confirmed PASS (14-03-SUMMARY.md) | PASS (human-verified) |
| Build gate (Debug + Release): 0 unresolved externals | Recorded in 14-01-SUMMARY.md and 14-03-SUMMARY.md | PASS |
| Commit trail: eb9b68987, 0b9c78f0e, 4bc512b45, 0d15c8433, 1bfeff6b3 | All commits confirmed in git log | PASS |

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| DECRUFT-05 | 14-01, 14-02, 14-03 | Voice chat (Vivox) fully removed from source and build — vivoxSharedWrapper unlinked; CuiVoiceChatManager/SwgCuiVoiceFlyBar/CuiVoiceChatEventHandler deleted; voice preference keys stripped; client builds + boots | SATISFIED | GATE-1 repo-wide grep-zero; GATE-2 over 4 source library trees 0 matches; all ~31 voice source files absent; CuiPreferences stripped; SwgClient.vcxproj and editor .vcxproj clean; dual-renderer boot confirmed |

DECRUFT-07 (cross-cutting milestone gate) is assigned to Phase 15 per REQUIREMENTS.md traceability table. Phase 14 satisfied the incremental D-08 dual-renderer boot check; the milestone-wide gate is owned by Phase 15. Not a Phase 14 gap.

---

### Anti-Patterns Found

No blockers or warnings found. Notes:

| File | Pattern | Severity | Impact |
|------|---------|----------|--------|
| `stage/client_d.cfg:91-93` | Stale `voiceChatEnabled=false` key (registration removed, key silently ignored) | Info (WR-01 from code review — accepted) | No crash; engine silently ignores unregistered keys; scheduled for stage/client_d.cfg deferred cleanup pass |
| `CuiPreferences.cpp` | Dead `ms_speakerVolume`/`ms_micVolume` pair (callers deleted with voice UI, decls remain) | Info (IN-01 from code review — accepted, possibly future audio options) | Harmless dead code; out of DECRUFT-05 scope |

Neither anti-pattern is a blocker or DECRUFT-05 gate failure.

---

### Deferred Items (Not Phase Gaps)

| Item | Status | Evidence |
|------|--------|---------|
| DEF-14-01: SwgClient Optimized-config LNK1281 SAFESEH failure | Pre-existing config defect (Optimized never built before Phase 14; 0 unresolved externals; 0 voice symbols in error log) | deferred-items.md; not a D-09 gate failure |
| DEF-14-02: GATE-2 getVoice/setVoice substring collides with soePlatform/ChatAPI2 community-chat methods | Benign false-positive in deliberately-preserved tree (zero vivox literals in ChatAPI2; untouched by Phase 14) | deferred-items.md; GATE-2 excluding ChatAPI2 == 0 |

---

### Human Verification Required

None. All automated checks passed. The dual-renderer boot gate (D-08 / DECRUFT-07 incremental) was already human-confirmed by the developer during Phase 14 Plan 03 execution (user typed "approved" after confirming character-select under both rasterMajor=5 and =11). No additional human verification is needed.

---

### Gaps Summary

No gaps. All 9 plan must-haves and all 5 ROADMAP success criteria are verified. The one code-review blocker (CR-01 — radial-menu enum ordinal shift) was identified, fixed in commit `1bfeff6b3`, and verified present in the codebase (`RESERVED_RADIAL_SLOT_103/104/105` placeholders at CuiMenuInfoTypes.h:174-176; `ITEM_EQUIP_APPEARANCE // 106` at :178). The two deferred items (DEF-14-01, DEF-14-02) are documented, non-goal-blocking, and correctly out of DECRUFT-05 scope.

---

_Verified: 2026-05-26_
_Verifier: Claude (gsd-verifier)_
