---
phase: 14-voice-chat-vivox-source-removal
plan: 03
subsystem: infra
tags: [vivox, voice-chat, decruft, vendored-sdk, soeplatform, vchatapi, grep-zero, dual-renderer-boot-gate, d3d9, d3d11]

# Dependency graph
requires:
  - phase: 14
    plan: 01
    provides: SwgClient inline link-token removal + engine-lib include-dir purge (prebuilt voice libs already unlinked at the .vcxproj link line)
  - phase: 14
    plan: 02
    provides: vestigial .rsp + all 8 editor .vcxproj/.rsp vivox refs purged (closes the editor-.vcxproj holdout for this plan's repo-wide gate)
  - phase: 13-videocapture-library-unlink
    provides: vendored-tree-delete-LAST sequencing + /FORCE link-grep gate + dual-renderer boot-gate precedent
provides:
  - 3 vendored voice trees deleted (vivox/, vivoxSharedWrapper/, soePlatform/VChatAPI/) — the last on-disk vivox/Vivox source literals removed
  - copy-libs.bat cleaned of the 4 stale VChatAPI copy lines (maint-script consistency)
  - repo-wide GATE-1 (vivox) grep-zero across src — DECRUFT-05 criterion #1 satisfied
  - DECRUFT-07 dual-renderer boot gate PASS (rasterMajor=5 D3D9 AND =11 D3D11, human-confirmed)
affects: [phase-15-xpcom-removal]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Wave-1 merge gate FIRST: repo-wide rg -i vivox before any tree delete — any match OUTSIDE the 3 doomed trees STOPS the delete (catches an incomplete Wave-1 .vcxproj/.rsp purge early, not at the milestone gate)"
    - "Vendored-tree delete sequenced LAST (Wave 2) after the include-path purge lands (14-01/14-02) so the link stays green; the prebuilt voice .lib were unlinked at the .vcxproj, so deleting the source trees does not break the link"
    - "Documented-intentional residual: prebuilt VChatAPI.lib/Base_vchat.lib stay in soePlatform/libs/ — criterion #1 targets vivox/Vivox source/include LITERALS, not .lib filenames"

key-files:
  created:
    - .planning/phases/14-voice-chat-vivox-source-removal/14-03-SUMMARY.md
  modified:
    - src/external/3rd/library/soePlatform/copy-libs.bat
    - .planning/phases/14-voice-chat-vivox-source-removal/deferred-items.md
    - stage/client_d.cfg

key-decisions:
  - "Deleted soePlatform/VChatAPI/ source tree (D-10) but PRESERVED soePlatform/libs/ (Base.lib/ChatAPI.lib + prebuilt voice VChatAPI.lib/Base_vchat.lib in Win32-Debug/Win32-Release) and the sibling ChatAPI2/ community-chat tree — the source delete does not remove the prebuilt libs and does not break the link"
  - "DEF-14-02: GATE-2 over-broad getVoice/setVoice substrings collide with 3 SOE community-chat methods (ChatRoom::getVoiceCount/getVoiceCore/getVoice) in the PRESERVED ChatAPI2/ tree (ZERO vivox literals) — benign false-positive, out of scope, documented not fixed"
  - "client_d.cfg END STATE set to rasterMajor=11 per D-08 (the file is untracked/local-only; was found at rasterMajor=5, reset to 11 to honor the plan's committed end state — reversible local config edit, no commit)"

patterns-established:
  - "Tree-delete preservation-assertion: after deleting a parent's voice subtree, assert the sibling KEEP-list targets (soePlatform/libs/*/Base.lib, ChatAPI2/) still exist on disk; the link-grep is the backstop"

requirements-completed: [DECRUFT-05]

# Metrics
duration: ~3h (incl. human dual-renderer boot gate)
completed: 2026-05-26
---

# Phase 14 Plan 03: Vendored Voice-Tree Delete + Dual-Renderer Boot Gate Summary

**Deleted the 3 vendored voice SDK trees (vivox/, vivoxSharedWrapper/, soePlatform/VChatAPI/ — 138 files, 47,201 lines, the last on-disk vivox/Vivox source literals), cleaned the stale copy-libs.bat VChatAPI lines, preserved soePlatform/libs/ + ChatAPI2/, achieved repo-wide vivox grep-zero, and confirmed the client boots to character-select under BOTH D3D9 (rasterMajor=5) and D3D11 (rasterMajor=11) — closing DECRUFT-05 criterion #1 and the DECRUFT-07 cross-cutting dual-renderer boot gate.**

## Performance

- **Duration:** ~3h (Task 1 automation + the human-verify dual-renderer boot gate spanning the user's two live launches)
- **Task 1 committed:** 2026-05-26T13:41:40-05:00
- **Tasks:** 2 (Task 1 auto + committed; Task 2 checkpoint:human-verify — APPROVED)
- **Files in Task 1 commit:** 138 (47,201 deletions)

## Accomplishments
- Ran the **Wave-1 merge gate** FIRST: repo-wide `rg -i vivox src` + explicit SwgClient/editor `.vcxproj` re-check confirmed Wave 1 (14-01 source/link + 14-02 editor `.vcxproj`/`.rsp`) landed completely before any tree was deleted — the only remaining matches were inside the 3 doomed trees.
- Deleted the 3 vendored voice trees via `git rm -r` (D-04 + D-10): `vivox/`, `vivoxSharedWrapper/` (incl. its `build/vivoxSharedWrapper.vcxproj`), `soePlatform/VChatAPI/`.
- Cleaned `soePlatform/copy-libs.bat` of the 4 stale `copy VChatAPI...` lines (reviewer LOW); kept every non-VChatAPI copy line (Base, ChatAPI, Network).
- **GATE-1 (criterion #1):** `rg -i vivox src` == 0.
- **GATE-2 (criterion #2):** the canonical voice-symbol set == 0 for the Vivox subsystem; the only residual matches are 3 SOE community-chat methods (`ChatRoom::getVoiceCount/getVoiceCore/getVoice`) in the PRESERVED ChatAPI2/ tree (zero vivox literals) — logged DEF-14-02 (benign substring false-positive).
- **Build gate (D-09):** Debug 0 unresolved externals (SwgClient_d.exe 69.9 MB), Release 0 (28.7 MB), Optimized 0 unresolved externals (link blocked only by the pre-existing LNK1281 SAFESEH config defect, DEF-14-01 — NOT a gate failure).
- **Boot gate (D-08 / DECRUFT-07):** user-confirmed PASS — client reaches character-select under BOTH rasterMajor=5 (D3D9) and rasterMajor=11 (D3D11), no crash / missing-symbol assert; options/HUD/radial surfaces load; no voice surfaces appear (D-06a orphaned-TRE no-op backstop holds).

## Task Commits

1. **Task 1: Wave-1 merge gate → delete 3 vendored voice trees + clean copy-libs.bat → repo-wide grep-zero + Debug/Optimized/Release link-grep** — `0d15c8433` (feat) — 138 files, 47,201 deletions.
2. **Task 2: Dual-renderer boot gate (checkpoint:human-verify, blocking)** — no code commit; user typed "approved" after confirming the dual-renderer boot. client_d.cfg left at rasterMajor=11 (untracked/local-only, not committed).

**Plan metadata:** _(this finalization docs commit — SUMMARY + STATE + ROADMAP + deferred-items.md)_

## Files Created/Modified
- **Deleted (Task 1, `0d15c8433`):** `src/external/3rd/library/vivox/` (include/, lib/), `src/external/3rd/library/vivoxSharedWrapper/` (Eq2Vivox.h, Vivox.cpp/.h/.inl, build/vivoxSharedWrapper.vcxproj, include/, lib/), `src/external/3rd/library/soePlatform/VChatAPI/` (VChat/VChatAPI source, VChatUnitTest, utils2.0) — 138 files total.
- `src/external/3rd/library/soePlatform/copy-libs.bat` — removed the 4 stale `copy VChatAPI...` lines; kept all non-VChatAPI copy lines.
- `.planning/phases/14-voice-chat-vivox-source-removal/deferred-items.md` — appended DEF-14-02 (the ChatAPI2 `getVoice` GATE-2 false-positive).
- `stage/client_d.cfg` — END STATE set to `rasterMajor=11` per D-08 (untracked/local file; not committed).

**Preserved (NOT deleted, D-10 KEEP list):**
- `src/external/3rd/library/soePlatform/libs/Win32-Debug/` + `Win32-Release/` — Base.lib, ChatAPI.lib, Network.lib (non-voice) AND VChatAPI.lib, Base_vchat.lib (prebuilt voice, documented intentional residual) — all present on disk after the delete.
- `src/external/3rd/library/soePlatform/ChatAPI2/` — community chat, zero vivox refs.

## Decisions Made
- **D-10 delete/keep boundary:** Deleted only the `soePlatform/VChatAPI/` subtree (the SOE VChat *source* carrying vivox literals — common.cpp VIV0X_DOT_COM, EncodeVivoxString/DecodeVivoxString, RESULT_VIVOX_*), NOT the whole soePlatform tree. The prebuilt `.lib` in `soePlatform/libs/` remain and keep the link green.
- **Documented intentional residual:** The prebuilt VChatAPI.lib/Base_vchat.lib stay in `soePlatform/libs/` by filename — criterion #1 targets vivox/Vivox source/include LITERALS, not `.lib` filenames, so these are out of criterion #1 scope (T-14-12 disposition).
- **client_d.cfg END STATE:** Found on disk at `rasterMajor=5` (the file is untracked/local-only — not in git, so there is no committed value to compare against). Reset to `rasterMajor=11` to honor the plan's D-08 committed end state. Reversible local config edit; not committed.

## Deviations from Plan

None functional. Two factual reconciliations against the handoff/plan text (neither a Rule 1–4 deviation):

1. **client_d.cfg was at rasterMajor=5, not 11.** The handoff stated "verified line 36 = rasterMajor=11; already the committed value — no cfg change needed." On disk the file is UNTRACKED (not in git at all) and line 37 read `rasterMajor=5`. Per D-08's explicit END STATE (leave at rasterMajor=11) and the "just do reversible local changes" rule, the line was set to `rasterMajor=11`. No commit (untracked local file).
2. **Preserve-target Base.lib path.** The plan's verify probed `soePlatform/libs/Base.lib` directly, but the libs live one level deeper in `Win32-Debug/` and `Win32-Release/` subdirs. Both `Base.lib` (and the prebuilt voice `VChatAPI.lib`/`Base_vchat.lib`) confirmed present — the KEEP list is intact; the verify path was an imperfect probe, not a missing file.

## Issues Encountered
- **GATE-2 ChatAPI2 collision (DEF-14-02):** The canonical GATE-2 set's broad `getVoice`/`setVoice` substrings match 3 SOE community-chat accessors (`ChatRoom::getVoiceCount`, `ChatRoomCore::getVoiceCore`, `getVoice`) in the deliberately-PRESERVED `soePlatform/ChatAPI2/` tree. `rg -i vivox` over ChatAPI2/ == 0; the files are untouched by Phase 14; GATE-2 over `src` EXCLUDING ChatAPI2/ == 0. Benign false-positive in a KEEP-listed non-voice tree; out of scope (SCOPE BOUNDARY), documented not fixed.
- **DEF-14-01 (Optimized SAFESEH, carried from 14-01):** SwgClient Optimized config fails LNK1281 SAFESEH (×32). Pre-existing, voice-unrelated config defect (Optimized `<Link>` lacks the `/SAFESEH:NO` Debug has + the `ImageHasSafeExceptionHandlers=false` Release has); 0 unresolved externals, 0 voice symbols in the error log. Deferred (deferred-items.md), NOT a Decruft regression and NOT a D-09 gate failure (the gate is 0 unresolved externals, met in all 3 configs).

## Decisions Satisfied
D-04 (vivox/ + vivoxSharedWrapper/ deleted), D-06a (orphaned compiled-TRE voice page/command/radial rows no-op gracefully — confirmed by the boot gate; no in-repo datatable to scrub), D-08 (dual-renderer boot to char-select under rasterMajor=5 AND =11; client_d.cfg left at 11), D-09 (Debug/Optimized/Release link-grep == 0 unresolved externals, not /FORCE exit 0), D-10 (soePlatform/VChatAPI/ deleted; soePlatform/libs/ + ChatAPI2/ preserved; copy-libs.bat VChatAPI lines cleaned).

## Next Phase Readiness
- DECRUFT-05 criterion #1 (repo-wide vivox grep-zero) and the DECRUFT-07 dual-renderer boot gate are both green; Phase 14 is functionally complete pending orchestrator phase verification.
- Phase 15 (XPCOM/Mozilla removal + the milestone-wide cross-cutting boot gate) is unblocked. The same vendored-tree-delete-LAST + Wave-1-merge-gate + /FORCE link-grep + dual-renderer boot-gate pattern applies.
- Carry-forward: DEF-14-01 (Optimized SAFESEH config fix) remains open — separate from Decruft, needs a `/SAFESEH:NO` / `ImageHasSafeExceptionHandlers=false` addition to the Optimized linker block before that config can ever produce an exe.

## Self-Check: PASSED

- FOUND: `.planning/phases/14-voice-chat-vivox-source-removal/14-03-SUMMARY.md`
- FOUND: commit `0d15c8433` (Task 1)
- CONFIRMED deleted on disk: vivox/, vivoxSharedWrapper/, soePlatform/VChatAPI/
- CONFIRMED preserved on disk: soePlatform/libs/Win32-Debug/Base.lib + Win32-Release/Base.lib (+ prebuilt VChatAPI.lib/Base_vchat.lib), ChatAPI2/
- CONFIRMED: copy-libs.bat has 0 VChatAPI refs
- CONFIRMED: client_d.cfg END STATE = rasterMajor=11
- Boot gate (D-08 / DECRUFT-07): user-confirmed PASS under rasterMajor=5 AND =11

---
*Phase: 14-voice-chat-vivox-source-removal*
*Completed: 2026-05-26*
