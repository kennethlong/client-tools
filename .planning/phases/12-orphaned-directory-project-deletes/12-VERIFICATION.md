---
phase: 12-orphaned-directory-project-deletes
verified: 2026-05-25T00:00:00Z
status: passed
score: 15/15 must-haves verified
overrides_applied: 0
resolution: "RESOLVED 2026-05-25 (commit 1d6b80242) — operator chose option (b): removed the 4 dangling <ProjectReference> blocks (to deleted lcdui.vcxproj) from AnimationEditor/ClientEffectEditor/LightningEditor/ParticleEditor.vcxproj. Truth 15 now VERIFIED (0 lcdui ProjectReference/GUID refs in any editor vcxproj). All 15/15 must-haves verified."
human_verification: []
_resolved_human_verification:
  - test: "Editor vcxproj ProjectReference to deleted lcdui.vcxproj — operator accept or fix? [RESOLVED — fixed in 1d6b80242]"
    expected: "Operator confirms whether the 4 dangling ProjectReference entries in AnimationEditor/ClientEffectEditor/LightningEditor/ParticleEditor.vcxproj are acceptable given the editors are already pre-broken on Qt, or whether they need to be removed."
    why_human: "The 4 editor vcxproj files contain <ProjectReference Include='...lcdui_src/build/win32/lcdui.vcxproj'> pointing to the now-deleted file. This is NOT documented in the SUMMARY as an intentional residual. The lcdui\\ lib AdditionalLibraryDirectories PATH segments were explicitly called out as 'inert /LIBPATH' and accepted, but the ProjectReference entries were not mentioned. The editors are pre-broken (Qt MSB8066) and outside /t:SwgClient scope, so this does not block the SwgClient boot or build. It is a latent correctness gap: opening these editor vcxproj files in VS will show an unresolved project reference error. Operator needs to decide: (a) accept as-is (editors are already broken, follow-up cleanup in a later phase), or (b) remove the 4 ProjectReference blocks now."
---

# Phase 12: Orphaned Directory & Project Deletes — Verification Report

**Phase Goal:** Remove four dead modules (trackIR, stationapi, SwgClientSetup, lcdui) from the active Koogie/MSBuild tree, re-establishing the dual-renderer boot baseline for the v2.1 Decruft milestone.
**Verified:** 2026-05-25T00:00:00Z
**Status:** passed (was human_needed; gap resolved 2026-05-25 in commit 1d6b80242)
**Re-verification:** No — initial verification + post-fix update

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|---------|
| 1 | `src/external/3rd/library/stationapi/` absent on disk | VERIFIED | `test -d` returns ABSENT |
| 2 | `src/external/3rd/library/trackIR/` absent on disk | VERIFIED | `test -d` returns ABSENT |
| 3 | Zero stationapi references in build files (`.rsp`, `.vcxproj`, `.sln`) | VERIFIED | `grep -rni stationapi` over SwgClient.vcxproj, SwgGodClient.vcxproj, swg.sln → 0 hits |
| 4 | Zero trackIr/NPClient references in clientGame build files | VERIFIED | `grep -rni "trackIr\|NPClient"` over clientGame.vcxproj + includePaths.rsp → 0 hits; broad scan of all `*.rsp`/`*.vcxproj`/`*.sln` under `src/` → 0 hits |
| 5 | ClientHeadTracking.cpp is NPClient-free — no-op stubs, still in the build | VERIFIED | File includes only `FirstClientGame.h`+`ClientHeadTracking.h`; no NPClient.h; 5 public methods present as no-ops; ClCompile entry retained in clientGame.vcxproj (line 359) |
| 6 | `src/game/client/application/SwgClientSetup/` absent on disk | VERIFIED | `test -d` returns ABSENT |
| 7 | Zero SwgClientSetup/9080903C references in swg.sln | VERIFIED | `grep -ni "SwgClientSetup\|9080903C" swg.sln` → 0 hits |
| 8 | SwgCuiG15Lcd.cpp has no `#include "EZ_LCD.h"` and no `#define USE_LCD` | VERIFIED | `grep "EZ_LCD\|USE_LCD"` → comment only (explaining removal); the `#ifdef USE_LCD` guard blocks remain (latent), the define itself is gone; public no-op methods `initializeLcd`/`updateLcd`/`remove` present |
| 9 | SwgCuiHud.cpp has no `#include "EZ_LCD.h"` | VERIFIED | `grep "EZ_LCD"` over SwgCuiHud.cpp → 0 hits |
| 10 | Zero `20D2AEE7\|lcdui\|lgLcd` in swg.sln | VERIFIED | `grep -rni` → 0 hits; confirmed via `grep -c "lcdui" swg.sln` = 0 |
| 11 | Zero lcdui/lgLcd in SwgClient.vcxproj, SwgClient .rsp, swgClientUserInterface .rsp | VERIFIED | All three files clean: SwgClient.vcxproj grep → 0, libraries.rsp → 0, libraryPaths.rsp → 0, swgClientUserInterface/includePaths.rsp → 0 |
| 12 | Zero lcdui/lgLcd in all 6 out-of-closure editor `.rsp` files | VERIFIED | `grep -rni "lgLcd\|lcdui"` over all 12 editor+SwgGodClient `.rsp` files → 0 hits |
| 13 | `lcdui_src/` and `lcdui/` absent on disk | VERIFIED | Both `test -d` checks return ABSENT |
| 14 | SwgClient target builds clean (MSBuild exit 0, 0 unresolved externals) | VERIFIED (operator-confirmed) | Commits adda94729, 7a7da726d, c10d19f10, f99c645dd all show build gate passed; SUMMARY for each plan documents 0 unresolved external symbols (not just exit 0) |
| 15 | Editor vcxproj files contain no ProjectReference to deleted lcdui.vcxproj | VERIFIED (post-fix) | RESOLVED in commit 1d6b80242: the 4 dangling `<ProjectReference>` blocks (GUID `{20d2aee7-...}`) were removed from AnimationEditor/ClientEffectEditor/LightningEditor/ParticleEditor.vcxproj. `grep -rniE "lcdui_src\|20d2aee7"` over all editor + SwgGodClient vcxproj → 0 ProjectReference/GUID hits; ProjectReference open/close tags balanced (63/63). Inert `lcdui\lib` /LIBPATH search-path segments remain (unused; editors pre-broken on Qt). |

**Score:** 14/15 truths verified (truth 15 is UNCERTAIN pending operator decision)

### Dual-Renderer Boot Gates (Human-Verified PASS — Operator-Confirmed)

All three boot gates (Plans 12-01, 12-02, 12-03) were operator-run and recorded as PASS in the respective SUMMARYs. Per the verification focus note, these are treated as satisfied for this report.

| Gate | Renderer | Result | Evidence |
|------|----------|--------|---------|
| 12-01 (after stationapi+trackIR) | rasterMajor=5 (D3D9) | PASS | 12-01-SUMMARY: "Dual-renderer boot gate PASSED" |
| 12-01 (after stationapi+trackIR) | rasterMajor=11 (D3D11) | PASS | 12-01-SUMMARY: "Dual-renderer boot gate PASSED" |
| 12-02 (after SwgClientSetup) | rasterMajor=5 | PASS | 12-02-SUMMARY: "Dual-renderer boot gate PASSED" |
| 12-02 (after SwgClientSetup) | rasterMajor=11 | PASS | 12-02-SUMMARY: "Dual-renderer boot gate PASSED" |
| 12-03 (after lcdui) | rasterMajor=5 | PASS | 12-03-SUMMARY: "Dual-renderer boot gate PASSED — character select + HUD + input" |
| 12-03 (after lcdui) | rasterMajor=11 | PASS | 12-03-SUMMARY: "Dual-renderer boot gate PASSED — character select + HUD + input" |

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/engine/client/library/clientGame/build/win32/clientGame.vcxproj` | ClientHeadTracking.cpp retained as no-op stub; no trackIr include paths | VERIFIED | ClCompile entry at line 359 (kept); zero grep hits for trackIr/NPClient |
| `src/engine/client/library/clientGame/build/win32/includePaths.rsp` | No trackIr line | VERIFIED | grep → 0 hits |
| `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` | No stationapi, no lcdui, no lgLcd | VERIFIED | grep → 0 hits for all three |
| `src/build/win32/swg.sln` | No SwgClientSetup/9080903C, no lcdui/20D2AEE7 | VERIFIED | grep → 0 hits; `grep -c "lcdui"` = 0 |
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiG15Lcd.cpp` | No EZ_LCD.h, no #define USE_LCD; no-op stubs present | VERIFIED | grep for both → 0 hits on actual directives; methods initializeLcd/updateLcd/remove present |
| `src/game/client/library/swgClientUserInterface/build/win32/includePaths.rsp` | No lcdui lines | VERIFIED | grep → 0 hits |
| `src/engine/client/library/clientGame/src/shared/core/ClientHeadTracking.cpp` | NPClient-free no-op stubs | VERIFIED | Includes only FirstClientGame.h + ClientHeadTracking.h; 5 stub methods present |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| clientGame.vcxproj | ClientHeadTracking.cpp (callers) | ClCompile retained, NPClient removed | VERIFIED | File stays in build; no NPClient.h; callers use NPClient-free .h interface |
| SwgCuiG15Lcd.cpp #ifdef blocks | No-op compiled-out LCD methods | #define USE_LCD removed | VERIFIED | All CEzLcd/s_lcd usage remains inside #ifdef USE_LCD guards which never fire |
| SwgCuiHud.cpp + ClientMain.cpp call sites | SwgCuiG15Lcd stub methods | Callers invoke no-op methods | VERIFIED | Call sites unchanged; methods compile to empty bodies |
| swg.sln ProjectDependencies in 7 projects | Removed lcdui GUID | {20D2AEE7-...} = {20D2AEE7-...} lines deleted | VERIFIED | swg.sln contains zero 20D2AEE7 occurrences |
| 4 editor vcxproj ProjectReference | lcdui.vcxproj (now deleted) | NOT removed | UNCERTAIN | AnimationEditor, ClientEffectEditor, LightningEditor, ParticleEditor still have inline ProjectReference to the now-deleted lcdui.vcxproj. lgLcd.lib/lcdui.lib were removed from AdditionalDependencies (confirmed via git diff f99c645dd). ProjectReference was not touched. |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|---------|
| DECRUFT-01 | 12-01 | trackIR + stationapi deleted; no .rsp/project refs; client boots | SATISFIED | Both dirs absent on disk; zero build file refs; 5 commits; boot gate PASS |
| DECRUFT-02 | 12-02 | SwgClientSetup removed from swg.sln + disk; client boots | SATISFIED | Dir absent; zero swg.sln refs; commit c10d19f10; boot gate PASS |
| DECRUFT-03 | 12-03 | lcdui.vcxproj dropped from swg.sln; include/lib refs purged from .rsp; client boots | SATISFIED (main path) | swg.sln zero lcdui; .rsp files clean; SwgClient.vcxproj clean; dirs deleted; boot PASS. Residual: 4 editor vcxproj have dangling ProjectReference to deleted lcdui.vcxproj (see truth 15) |

DECRUFT-04, DECRUFT-05, DECRUFT-06, DECRUFT-07 are mapped to Phases 13-15 in REQUIREMENTS.md — not in scope for Phase 12.

### Anti-Patterns Found

| File | Pattern | Severity | Impact |
|------|---------|----------|--------|
| `AnimationEditor.vcxproj` (line 872) | `<ProjectReference>` to deleted `lcdui_src/build/win32/lcdui.vcxproj` | Warning | Opening this editor vcxproj in VS will show "project not found" for the lcdui reference. Does not affect /t:SwgClient build. |
| `ClientEffectEditor.vcxproj` (line 308) | Same ProjectReference to deleted lcdui.vcxproj | Warning | Same impact. |
| `LightningEditor.vcxproj` (line 292) | Same ProjectReference to deleted lcdui.vcxproj | Warning | Same impact. |
| `ParticleEditor.vcxproj` (line 1214) | Same ProjectReference to deleted lcdui.vcxproj | Warning | Same impact. |
| All 6 editor vcxproj + SwgGodClient.vcxproj | `lcdui\lib` segment in `AdditionalLibraryDirectories` | Info | Intentional per SUMMARY ("inert /LIBPATH"); directory is deleted so linker ignores it. These search-path entries are inert. |

### Human Verification Required

#### 1. Editor vcxproj ProjectReference to deleted lcdui.vcxproj — accept or clean up?

**Test:** Examine whether the 4 dangling ProjectReference entries in the editor vcxproj files need to be removed or can be accepted as-is.

**Files affected:**
- `src/engine/client/application/AnimationEditor/build/win32/AnimationEditor.vcxproj` (line 872)
- `src/engine/client/application/ClientEffectEditor/build/win32/ClientEffectEditor.vcxproj` (line 308)
- `src/engine/client/application/LightningEditor/build/win32/LightningEditor.vcxproj` (line 292)
- `src/engine/client/application/ParticleEditor/build/win32/ParticleEditor.vcxproj` (line 1214)

**What each entry looks like:**
```xml
<ProjectReference Include="..\..\..\..\..\..\external\3rd\library\lcdui_src\build\win32\lcdui.vcxproj">
  <Project>{20d2aee7-b60a-4ec9-b187-fa76062a6c39}</Project>
  <ReferenceOutputAssembly>false</ReferenceOutputAssembly>
</ProjectReference>
```

**Expected:** One of:
- (a) Accept as-is: these editors are already pre-broken on Qt (MSB8066), they are not in the /t:SwgClient build closure, and DECRUFT-03 per REQUIREMENTS.md only requires lcdui.vcxproj removed from swg.sln + .rsp files clean. The dangling ProjectReference is a latent cosmetic issue that can be tracked for future editor cleanup.
- (b) Remove now: add a follow-up commit deleting the 4 `<ProjectReference>` blocks (3-line blocks) from each affected editor vcxproj.

**Why human:** This is a policy decision, not a build-breaking gap. The /t:SwgClient build is unaffected. The SUMMARY is silent on these entries. Whether to accept this as within-scope "done" or flag for immediate cleanup is an operator call.

### Gaps Summary

No blockers identified. The single uncertain item (truth 15, editor vcxproj ProjectReference to deleted lcdui.vcxproj) is a warning-level cosmetic correctness gap in 4 editor tools that were already pre-broken on Qt. It does not affect the primary validation target (/t:SwgClient), dual-renderer boot, or the DECRUFT-01/-02/-03 requirements as written in REQUIREMENTS.md. Pending operator accept-or-fix decision.

### Documented Deviations (Accepted)

| Deviation | Plan | Disposition | Effect |
|-----------|------|-------------|--------|
| 989crypt.lib was a live (not stale) SwgClient dependency — dropped it | 12-01 | Caught + fixed by build gate | Correct; no regression |
| ClientHeadTracking.cpp kept in build as no-op stubs (plan said "drop from build") | 12-01 | Caught + fixed by link analysis; accepted by operator | Avoids unresolved-external under /FORCE; callers degrade correctly |
| Full-solution validation bar reduced to /t:SwgClient (Qt MSB8066 pre-existing) | 12-03 | Accepted by operator at boot gate | Editors were already broken; SwgClient closure is clean |
| lgLcd.lib/lcdui.lib removed from editor vcxproj AdditionalDependencies; lcdui\lib search PATHS left in AdditionalLibraryDirectories | 12-03 | Documented in SUMMARY as "inert /LIBPATH" | Inert: lcdui/ dir is deleted; linker silently ignores missing search paths |

---

_Verified: 2026-05-25T00:00:00Z_
_Verifier: Claude (gsd-verifier)_
