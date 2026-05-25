---
phase: 13-videocapture-library-unlink
plan: 02
subsystem: build
tags: [dead-code-removal, decruft, videocapture, audiocapture, rsp, vcxproj, includePaths, if0, grep-gate, D-02, D-02a, D-04]

requires:
  - phase: 13-videocapture-library-unlink
    plan: 01
    provides: atomic VideoCapture link-unit removal (live caller + wrapper + SwgClient.vcxproj lib inputs); confirms #if 0 blocks reference no link symbols
provides:
  - All #if 0 / #if PRODUCTION==0 VideoCapture/AudioCapture SOURCE residue deleted from the 5 dead-source files (D-02/D-02a source coherence) — no link change for SwgClient
  - videocapture include path removed from clientGame.vcxproj + clientAudio.vcxproj (3 configs each) and the 3 engine includePaths.rsp
  - Zero capture-family references in ANY .rsp repo-wide (42 files) and in the 10 editor/aux .vcxproj this plan owns — DECRUFT-04 criterion #1 grep gate satisfied for this plan's surface
  - Enumerated + reconciled the capture-ref .vcxproj inventory (10 live = 14-total minus 4 already-clean build-path) for Plan 03's post-Wave-1-merge full-repo gate
affects: [13-03, decruft]

tech-stack:
  added: []
  patterns:
    - "Pattern D (1d6b80242 analog): grep-gate-only purge of vestigial .rsp + pre-broken editor/aux .vcxproj that no build-path project reads — satisfies D-04 without touching the SwgClient build"
    - "Token allow-list discipline (adda94729 analog): remove ONLY the 15 capture-family tokens + videocapture paths; protected deps (binkw32/vivox/zlib/dpvs/crypto) left intact, verified by targeted negative controls in the SPECIFIC files they live in"
    - "D-02a dead-vs-live boundary in Audio.cpp: delete only the #if 0 'AudioCapture Filter' recorder methods; confirm a known-live AIL_* playback call (AIL_start_sample, 7x) survives"

key-files:
  created: []
  modified:
    - src/engine/client/library/clientGame/src/shared/core/Game.cpp
    - src/engine/client/library/clientGame/src/shared/core/Game.h
    - src/engine/client/library/clientAudio/src/win32/Audio.cpp
    - src/engine/client/library/clientAudio/src/win32/Audio.h
    - src/game/client/library/swgClientUserInterface/src/shared/parser/SwgCuiCommandParserScene.cpp
    - src/engine/client/library/clientGame/build/win32/clientGame.vcxproj
    - src/engine/client/library/clientAudio/build/win32/clientAudio.vcxproj
    - src/engine/client/library/clientGame/build/win32/includePaths.rsp
    - src/engine/client/library/clientAudio/build/win32/includePaths.rsp
    - src/engine/client/library/clientGraphics/build/win32/includePaths.rsp
    - "src/game/client/application/SwgClient/build/win32/libraries_{d,r,o}.rsp + libraryPaths.rsp"
    - "10 editor/aux projects (.vcxproj + libraries_{d,r,o}.rsp + libraryPaths.rsp): SwgGodClient, Viewer, TextureBuilder, AnimationEditor, ClientEffectEditor, LightningEditor, ParticleEditor, SwooshEditor, TerrainEditor, NpcEditor"
  deleted: []

key-decisions:
  - "D-02/D-02a honored: deleted all #if 0 recorder source residue across the 5 dead-source files; Audio.cpp live Miles playback (AIL_start_sample x7) untouched; removed methods getAudioCaptureConfig/startAudioCapture/stopAudioCapture == 0."
  - "D-04 honored: zero capture refs remain in any .rsp (42 files purged) and in the 10 editor/aux .vcxproj this plan owns; the vendored tree + the cross-cutting all-files merge check are Plan 03's."
  - "DEVIATION (Rule 3 - tooling): the videocapture .vcxproj path-removal sed errored on a backslash-vs-back-reference ambiguity (`\\3rd` parsed as `\3`); after a targeted git-checkout restore of the 10 untouched files, re-ran with a backslash-free segment regex (`[^;<>]*videocapture[^;<>]*;`). No content impact — final state verified clean."

patterns-established:
  - "Backslash-free path-segment purge for Windows .vcxproj include/lib lists under MSYS sed: match the segment by a stable substring + delimiter (`[^;<>]*TOKEN[^;<>]*;`), never by escaping the literal `\\` path separators (avoids the `\N` back-reference trap)."
---

# Phase 13 Plan 02: VideoCapture/AudioCapture Inert Residue Purge Summary

Deleted every VideoCapture/AudioCapture source-residue and reference-residue that is INERT to the SwgClient link — the `#if 0`/`#if PRODUCTION==0` dead source in 5 files, the engine include paths, all 42 `.rsp`, and the 10 editor/aux `.vcxproj` — satisfying DECRUFT-04 criterion #1's zero-reference gate for this plan's surface, while leaving live Miles audio and all protected deps intact.

## What was built

Ran in Wave 1 alongside Plan 01 (zero file overlap; Plan 01 landed first as `bb2e101b8`). Three atomic task commits:

### Task 1 — dead `#if 0` capture source residue (`e5a76be9f`, 191 deletions)
- **Game.cpp:** removed the `#if 0` `#include "clientGraphics/SwgVideoCapture.h"` block and the three `#if 0` method/callback blocks (`videoCaptureConfig/Start/Stop`, `VideoCaptureCallback`, the `AudioCapture::SwgAudioCaptureManager::GetInstance()` reference).
- **Game.h:** removed the `#if PRODUCTION == 0` block (3 `videoCapture*` decls). `handleCollectionShowServerFirstOptionChanged` (the line immediately above) preserved — verified present.
- **Audio.cpp:** removed the phantom `#if 0 #include "clientAudio/SwgAudioCapture.h"` (no file deleted — it never existed) and the three `#if 0` recorder methods (`getAudioCaptureConfig/startAudioCapture/stopAudioCapture`, the Miles `"AudioCapture Filter"` API). **No live `AIL_*` playback touched (D-02a).**
- **Audio.h:** removed the UNGUARDED `namespace AudioCapture { class ICallback; }` fwd-decl and the `#if PRODUCTION == 0` decl block.
- **SwgCuiCommandParserScene.cpp:** removed the `#if 0` command-name consts, the three `/* */`-commented help rows (precision: kept the surrounding `#if PRODUCTION == 0` debug-table guard that legitimately wraps `warp`/`drawNetworkIds`/etc.), and the three `#if 0` dispatch handlers.

### Task 2 — videocapture include path (`5f13c0172`)
- **clientGame.vcxproj** (3 configs): dropped the single `...\external\3rd\library\videocapture;` `<AdditionalIncludeDirectories>` segment; neighbors `ui\include` → `external\ours\library\archive\include` now adjacent.
- **clientAudio.vcxproj** (3 configs): same; neighbor before is `miles\include` (the plan hint said `bink\include` — trusted the live file per the line-drift guard), neighbor after is `external\ours\library\archive\include`. Both kept.
- **clientGame / clientAudio / clientGraphics includePaths.rsp:** dropped the vestigial `videocapture` line each. (clientGraphics.vcxproj is Plan 01's — only its `.rsp` touched here, no overlap.)

### Task 3 — `.rsp` + editor/aux `.vcxproj` grep gate (`8c2ad61be`, 52 files)
- **42 `.rsp`** purged of capture lib tokens + `videocapture`/`AudioCapture` search paths.
- **10 editor/aux `.vcxproj`** purged of inline `<AdditionalDependencies>` capture tokens + `<AdditionalLibraryDirectories>` videocapture paths (3 configs each).

## Enumeration + reconciliation (DRIFT GUARD)

Live enumeration BEFORE editing (vendored tree excluded):

**`.vcxproj` carrying capture refs = 10** (the editor/aux set this plan owns):
SwgGodClient, Viewer, TextureBuilder, AnimationEditor, ClientEffectEditor, LightningEditor, ParticleEditor, SwooshEditor, TerrainEditor, NpcEditor.

Reconciled to the RESEARCH 14-total inventory: `14 − 4 already-clean build-path = 10`. The 4 already-clean: SwgClient + clientGraphics (Plan 01, `bb2e101b8`), clientGame + clientAudio (this plan, Task 2). **No drift.**

**`.rsp` carrying capture refs = 42** (all outside the vendored tree; 0 inside it):
- SwgClient: `libraries_d.rsp`, `libraries_r.rsp`, `libraries_o.rsp`, `libraryPaths.rsp` (4)
- SwgGodClient, Viewer, TerrainEditor, NpcEditor, SwooshEditor, ParticleEditor, LightningEditor, ClientEffectEditor, AnimationEditor: `libraries_{d,r,o}.rsp` + `libraryPaths.rsp` (4 each)
- TextureBuilder: `libraries_r.rsp` + `libraryPaths.rsp` (2, as expected — no debug/opt lib lists)

Distinct capture lib-token lines removed from `.rsp`: `VideoCapture_{debug,release}.lib`, `ImageCapture_{debug,release}.lib`, `Smart_{debug,release}.lib`, `SoeUtil_{debug,release}.lib`, `ZlibUtil_{debug,release}.lib`, `picn20md.lib`, `picn20m.lib`. (`AudioCapture.lib`/`CaptureCommon_*.lib` appear only in SwgClient.vcxproj — Plan 01's, not in any `.rsp`.) Plus 7 distinct `videocapture/.../lib/win32` path lines (incl. the `AudioCapture/lib/win32` path that SwgClient's `libraryPaths.rsp` carried but the editor ones omit).

## Verification (all PASS)

| Gate | Command shape | Result |
|------|---------------|--------|
| Source areas | `grep -rniE 'videoCapture\|VideoCapture\|AudioCapture\|SwgAudioCapture'` over clientGame/src, clientAudio/src, swgClientUserInterface/src | 0 |
| Include path | `grep -ci videocapture` over clientGame.vcxproj + clientAudio.vcxproj + 3 includePaths.rsp | 0 (sum) |
| LOCAL `.rsp` gate | all `.rsp` outside `external/3rd/library/videocapture/` | 0 files |
| LOCAL `.vcxproj` gate | this plan's 12 owned (10 editor/aux + clientGame + clientAudio) | 0 files |
| Double-`;` | `grep ';;'` over edited vcxproj | 0 |

### Targeted negative controls (protected token survives in the SPECIFIC file it lived in)
- `SwgClient/libraries_d.rsp` → `vivoxSharedWrapper_debug.lib` = 1, `dpvsd.lib` = 1
- `SwgClient/libraryPaths.rsp` → `vivoxSharedWrapper/lib` path = 1, `zlib/lib` path = 1
- `AnimationEditor.vcxproj` → `dpvs.lib` = 6, `vivoxSharedWrapper_release.lib` = 3

### D-02a live-audio control
- Removed recorder methods `getAudioCaptureConfig|startAudioCapture|stopAudioCapture` in Audio.cpp = **0**
- Known-live playback call **`AIL_start_sample` = 7** (at Audio.cpp lines 2877, 2986, 4381, 5338, 5358 outside the deleted blocks). `AIL_quick_play` (the plan's suggested control) does NOT exist in this tree — `AIL_start_sample` is the confirmed-live call named here per the plan's fallback instruction.
- No file named `SwgAudioCapture.h` created or deleted (phantom — only the dead `#include` line removed).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking tooling] MSYS `sed` backslash/back-reference failure on the videocapture .vcxproj path purge**
- **Found during:** Task 3 (editor/aux `.vcxproj` path removal)
- **Issue:** The path-removal regex `s#[^;<>]*\\external\\3rd\\library\\videocapture\\[^;<>]*;##g` errored with `sed: -e expression #1, char 55: Invalid back reference` — the `\\3rd` literal-backslash-then-`3` was parsed as the `\3` back-reference. The capture-lib-token seds had already applied (lib tokens removed) but the path seds had not, leaving a half-applied state.
- **Fix:** Targeted `git checkout --` restore of ONLY the 10 affected `.vcxproj` to HEAD (they were committed-clean), then re-ran with a backslash-free segment regex `s/[^;<>]*videocapture[^;<>]*;//g` that matches the path segment by substring + delimiter without escaping the path separators.
- **Files modified:** the 10 editor/aux `.vcxproj` (no net content difference from the intended result)
- **Commit:** `8c2ad61be` (clean final state; the half-applied intermediate never committed)

**2. [Plan hint drift] clientAudio.vcxproj neighbor segment**
- The plan hinted clientAudio's pre-videocapture neighbor was `bink\include`; the live file showed `miles\include`. Per the LINE-NUMBER/PATTERN DRIFT GUARD, trusted the live file. No functional impact — the videocapture segment was still removed cleanly with both real neighbors (`miles\include`, `external\ours\library\archive\include`) preserved.

## Scope discipline

`git add` was per-file/per-explicit-list only — never `git add -A`/`.`/directory. The pre-existing uncommitted working-tree changes were left untouched and excluded from every commit: `src/engine/client/application/Direct3d9/*` (4 files), `docs/research/README.md`, `.cursor/`, `tools/swg_blender/`, and the untracked `docs/research/*.md`. Confirmed: 0 Direct3d9 paths staged in the Task 3 commit; no tracked-file deletions in any commit.

## Self-Check: PASSED

- [x] Game.cpp/.h, Audio.cpp/.h, SwgCuiCommandParserScene.cpp — 0 capture refs; `handleCollectionShowServerFirstOptionChanged` intact; `AIL_start_sample` x7 intact
- [x] videocapture gone from clientGame/clientAudio .vcxproj (3 configs) + 3 includePaths.rsp
- [x] LOCAL .rsp gate = 0 files; LOCAL .vcxproj gate (12 owned) = 0 files; protected tokens preserved per targeted controls
- [x] Enumeration reconciled (10 .vcxproj = 14 − 4 already-clean); 42 .rsp purged
- [x] 3 per-task commits (e5a76be9f, 5f13c0172, 8c2ad61be) verified in `git log`; no tracked deletions; pre-existing Direct3d9/docs/.cursor/tools changes untouched
