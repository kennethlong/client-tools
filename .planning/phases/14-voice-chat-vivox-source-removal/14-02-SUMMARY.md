---
phase: 14-voice-chat-vivox-source-removal
plan: 02
subsystem: infra
tags: [vivox, voice-chat, decruft, msbuild, vcxproj, rsp, editors, swggodclient, grep-zero, no-build]

# Dependency graph
requires:
  - phase: 14
    plan: 01
    provides: SwgClient + source-library inline vivox removal (zero file overlap; ran parallel in Wave 1)
  - phase: 13-videocapture-library-unlink
    provides: editor-.rsp-purge precedent (D-04) + MSYS-sed vcxproj-path-token gotcha
provides:
  - SwgClient vestigial .rsp purged of vivoxSharedWrapper_*.lib + path (cosmetic grep-zero, D-09)
  - clientUserInterface + clientGame includePaths.rsp purged of vivox + dangling swgClientVivox (D-05)
  - 16 editor .rsp (8 editors) purged of vivoxSharedWrapper_release.lib + vivoxSharedWrapper/lib (D-07)
  - 7 editor .vcxproj inline vivox link tokens + lib dirs removed across all 3 configs (the reviewer-flagged gap)
  - SwgGodClient.vcxproj inline vivox/VChat/Base_vchat/libsndfile tokens removed; soePlatform\libs preserved
affects: [14-03]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Token-substring vcxproj purge: match ;TOKEN;%(AdditionalDependencies) / ;PATH;%(AdditionalLibraryDirectories) sentinel-anchored, replace_all across the 3 configs (avoids MSYS-sed backslash-as-backreference mis-parse)"
    - "Variable .vcxproj path-prefix depth: editors mix 6-level (..\\..\\..\\..\\..\\..) and 4-level (..\\..\\..\\..) external prefixes — match the prefix exactly per file, not a fixed depth"
    - "Word-diff token audit: git diff --word-diff-regex='[^;<>]+' proves ONLY voice tokens removed, ZERO tokens added — the over-removal (T-14-05) guard"

key-files:
  created:
    - .planning/phases/14-voice-chat-vivox-source-removal/14-02-SUMMARY.md
  modified:
    - src/game/client/application/SwgClient/build/win32/libraries_d.rsp
    - src/game/client/application/SwgClient/build/win32/libraries_r.rsp
    - src/game/client/application/SwgClient/build/win32/libraries_o.rsp
    - src/game/client/application/SwgClient/build/win32/libraryPaths.rsp
    - src/engine/client/library/clientUserInterface/build/win32/includePaths.rsp
    - src/engine/client/library/clientGame/build/win32/includePaths.rsp
    - src/engine/client/application/AnimationEditor/build/win32/AnimationEditor.vcxproj
    - src/engine/client/application/ClientEffectEditor/build/win32/ClientEffectEditor.vcxproj
    - src/engine/client/application/LightningEditor/build/win32/LightningEditor.vcxproj
    - src/engine/client/application/NpcEditor/build/win32/NpcEditor.vcxproj
    - src/engine/client/application/ParticleEditor/build/win32/ParticleEditor.vcxproj
    - src/engine/client/application/SwooshEditor/build/win32/SwooshEditor.vcxproj
    - src/engine/client/application/Viewer/build/win32/Viewer.vcxproj
    - src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj
    - "(+ 16 editor libraries.rsp / libraryPaths.rsp — 2 per editor)"

key-decisions:
  - "LightningEditor.vcxproj + SwooshEditor.vcxproj use 4-level external prefixes (..\\..\\..\\..) vs the 6-level (..\\..\\..\\..\\..\\..) the plan's interfaces block assumed for all 7 — matched per-file; not a deviation, just prefix-depth variance"
  - "Kept soePlatform\\libs\\Win32-Debug dir in SwgGodClient.vcxproj (Pitfall 4) — holds non-voice Base.lib/ChatAPI.lib/Network.lib; removed only the voice .lib tokens"
  - "No build performed: SwgClient.vcxproj does not consume any .rsp (vestigial), and the 8 editors are pre-broken non-build-targets — refs purged for criterion #1 grep-zero only"

patterns-established:
  - "Preservation-assertion: after a shared-element token purge, grep the element for a known non-voice neighbour (xpcom.lib == 3, one per config) to prove the rest of the AdditionalDependencies survived"

requirements-completed: [DECRUFT-05]

# Metrics
duration: 6min
completed: 2026-05-26
---

# Phase 14 Plan 02: Vestigial + Editor Vivox Residue Purge Summary

**Purged every non-load-bearing vivox token criterion #1's grep-zero requires: the SwgClient vestigial `.rsp` lib/path tokens, the dangling `swgClientVivox` + vivox lines in `clientUserInterface`/`clientGame` `includePaths.rsp`, all 16 editor `.rsp` refs, the INLINE vivox refs in all 7 editor `.vcxproj`, and the SwgGodClient.vcxproj inline vivox/VChat/Base_vchat/libsndfile tokens — 30 files, deletions only, ZERO build (vestigial + pre-broken editors), with all non-voice deps/dirs (xpcom, xul, qt, libMozilla, lcdui, miles, soePlatform\libs) preserved.**

## Performance

- **Duration:** ~6 min
- **Started:** 2026-05-26T18:22:46Z
- **Completed:** 2026-05-26T18:28:37Z
- **Tasks:** 2 (committed individually)
- **Files modified:** 30 (6 in Task 1, 24 in Task 2)

## What Was Done

### Task 1 — SwgClient vestigial `.rsp` + engine-lib `includePaths.rsp` (commit `0b9c78f0e`)
- `libraries_d.rsp` / `libraries_r.rsp` / `libraries_o.rsp`: removed the `vivoxSharedWrapper_{debug,release}.lib` line (1 each); kept the surviving `dpvs(d).lib`.
- `libraryPaths.rsp`: removed the `...\vivoxSharedWrapper\lib` path; kept libMozilla/directx9/dpvs.
- `clientUserInterface/includePaths.rsp` (D-05): removed `vivox/include`, `vivoxSharedWrapper/include`, dangling `swgClientVivox/include/public` (3 lines).
- `clientGame/includePaths.rsp` (D-05): removed `vivox/include` + dangling `swgClientVivox/include/public` (2 lines).
- Net: 6 files, 9 deletions. **GATE-1 over the 6 .rsp = 0** (rg exit 1).

### Task 2 — 16 editor `.rsp` + 8 editor `.vcxproj` inline (commit `4bc512b45`)
- **16 editor `.rsp`** (AnimationEditor, ClientEffectEditor, LightningEditor, NpcEditor, ParticleEditor, SwooshEditor, Viewer, SwgGodClient): removed `vivoxSharedWrapper_release.lib` from each `libraries.rsp` and the `vivoxSharedWrapper\lib` (Viewer: `/lib` forward-slash) path from each `libraryPaths.rsp` — 16 single-line deletions.
- **7 editor `.vcxproj`**: removed `vivoxSharedWrapper_release.lib;` from `<AdditionalDependencies>` and the `vivoxSharedWrapper\lib;` token from `<AdditionalLibraryDirectories>` across all 3 configs (Debug/Optimized/Release) each. xpcom.lib/xul.lib/qt deps + libMozilla/lcdui/miles dirs preserved (verified xpcom.lib == 3 per file).
- **SwgGodClient.vcxproj** (fuller set): removed `Base_vchat.lib`, `VChatAPI.lib`, `libsndfile-1.lib`, `vivoxplatform.lib`, `vivoxsdk.lib`, `vivoxSharedWrapper_Debug.lib`, `vivoxSharedWrapper_debug.lib` (Debug deps) + `vivoxSharedWrapper_release.lib` (Optimized + Release deps) + the `vivox\lib` and `vivoxSharedWrapper\lib` dir tokens (all 3 configs). KEPT `soePlatform\libs\Win32-Debug` + `Base.lib`/`ChatAPI.lib`/`Network.lib`/`TcpLibrary.lib`/`CommodityAPI.lib`/`989crypt.lib` (Pitfall 4 / T-14-06). Word-diff token audit confirmed removed-only set = {Base_vchat.lib, VChatAPI.lib, libsndfile-1.lib;vivoxplatform.lib;vivoxsdk.lib;vivoxSharedWrapper_Debug.lib;vivoxSharedWrapper_debug.lib, vivoxSharedWrapper_release.lib, both vivox dir paths}; ZERO tokens added.
- Net: 24 files, 48 insertions / 64 deletions (the +/- on .vcxproj is XML single-line element rewrites, 6 lines per file). **GATE-1 over the 16 .rsp + 8 .vcxproj = 0** (rg exit 1).

## Verification

| Gate | Command | Result |
|------|---------|--------|
| Task 1 grep-zero | `rg -i "vivox\|swgClientVivox"` over 6 .rsp | PASS (exit 1, 0 matches) |
| Task 2 grep-zero | `rg -i "vivox\|VChatAPI\|Base_vchat\|libsndfile"` over 16 .rsp + 8 .vcxproj | PASS (exit 1, 0 matches) |
| Full plan-scope | `rg -i "vivox\|swgClientVivox\|VChatAPI\|Base_vchat\|libsndfile"` over all 30 files | PASS (exit 1, 0 matches) |
| Preservation (T-14-05) | `xpcom.lib` count in each of 7 editor .vcxproj | 3 per file (intact) |
| Preservation (T-14-06) | `soePlatform\libs` in SwgGodClient.vcxproj | present (Base.lib/ChatAPI.lib survive) |
| Over-removal audit | `git diff --word-diff-regex` on SwgGodClient.vcxproj | only voice tokens removed; 0 added |

No build performed (D-09 N/A for this plan — `.rsp` are vestigial / not consumed by SwgClient.vcxproj; editors are pre-broken non-build-targets). SwgClient build is unaffected. D-05, D-07 satisfied. The 14-03 repo-wide `rg -i "vivox" src/` gate now has no editor-`.vcxproj` or `.rsp` holdouts in this plan's scope.

## Deviations from Plan

None functionally. One factual refinement of the plan's `<interfaces>` map: the `<AdditionalLibraryDirectories>` path-prefix depth is NOT uniform across the 7 editors — LightningEditor.vcxproj and SwooshEditor.vcxproj use a 4-level `..\..\..\..\external` prefix while AnimationEditor/ClientEffectEditor/NpcEditor/ParticleEditor/Viewer use 6-level `..\..\..\..\..\..\external`. The vivox token was matched per-file with its exact prefix; outcome (token removed in all 3 configs) is identical to the plan's intent. Not a Rule 1–4 deviation — a benign data-correction within the action's literal scope.

## Commit Hashes

| Task | Commit | Files |
| ---- | ------ | ----- |
| 1 | `0b9c78f0e` | 4 SwgClient .rsp + 2 engine includePaths.rsp (6 files, 9 deletions) |
| 2 | `4bc512b45` | 16 editor .rsp + 8 editor .vcxproj (24 files) |

## Commit Hygiene

Only the 30 plan-scope files were staged (verified `git diff --cached --name-only` = 6 then 24, with no Direct3d9/.cursor/AGENTS.md/tools/swg_blender path). The pre-existing unrelated working-tree changes remain untouched and unstaged.

## Self-Check: PASSED

- FOUND: `.planning/phases/14-voice-chat-vivox-source-removal/14-02-SUMMARY.md`
- FOUND: commit `0b9c78f0e` (Task 1)
- FOUND: commit `4bc512b45` (Task 2)
