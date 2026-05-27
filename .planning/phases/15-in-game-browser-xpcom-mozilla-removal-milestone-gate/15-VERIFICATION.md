---
phase: 15-in-game-browser-xpcom-mozilla-removal-milestone-gate
verified: 2026-05-26T00:00:00Z
status: passed
score: 9/9 must-haves verified
overrides_applied: 0
human_verification_note: >
  DECRUFT-07 / ROADMAP criterion #4 (dual-renderer boot to character select under
  rasterMajor=5 AND =11) is a non-scriptable checkpoint. It was HUMAN-verified and
  user-APPROVED this session (recorded in 15-04-SUMMARY.md Task 2; boot-gate-PASS
  commit 16fd3ac4c). Per the verification protocol this counts as a VERIFIED truth
  (human checkpoint already satisfied), not an open human_needed item — status is
  therefore `passed`, not `human_needed`. It needs the live SWGSource VM and was NOT
  re-run by this verifier.
---

# Phase 15: In-Game Browser (XPCOM/Mozilla) Removal & Milestone Gate — Verification Report

**Phase Goal:** Fully remove the XPCOM/Mozilla in-game browser from source, project, include path, and stage copy list — the highest-risk live-UI surgery — then run the cross-cutting dual-renderer boot gate that closes the v2.1 Decruft milestone.
**Verified:** 2026-05-26
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

This phase is a 4-plan, 3-wave removal: 15-01 (atomic source/link removal), 15-02 (.rsp + editor/SwgGodClient residue purge), 15-03 (drop libMozilla from swg.sln + delete vendored tree), 15-04 (milestone residue sweep + lcdui A1 cleanup + dual-renderer boot gate). Every must-have was checked against the LIVE tree, not the SUMMARY claims. All 4 ROADMAP success criteria and all plan-frontmatter must-haves are satisfied.

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | (SC#1 / D-07) `libMozilla.vcxproj` dropped from `swg.sln`; zero `xpcom`/`xul`/`Mozilla` in project/.rsp/include config | ✓ VERIFIED | `rg "C6C1E14A-DEDB-48C6-B028-0C9E4EFF5DC4\|libMozilla" swg.sln` → 0 (exit 1). SwgClient project still present (sln well-formed). Mozilla tokens in SwgClient.vcxproj + 5 .rsp + 7 editors + SwgGodClient all → 0 (exit 1). |
| 2 | (SC#2 / D-01,D-02,D-05) `CuiWebBrowser*`/`UIWebBrowserWidget`/XPCOM bridge deleted; symbols grep-zero; no Mozilla DLL in POST_BUILD/stage | ✓ VERIFIED | 9 WebBrowser files (6 src + 3 public re-includes) absent on disk. `rg -i "CuiWebBrowser\|UIWebBrowserWidget" src` → 0 (exit 1). Repo-wide GATE-XPCOM grep → 0 (exit 1). SwgClient.vcxproj PostBuildEvent copies only `_d.exe`/`_d.pdb` — no Mozilla DLL line (D-10 confirmed no-op). |
| 3 | (D-02) `TUIWebBrowser` deleted outright from UITypeID.h with NO placeholder; all 5 IsA sites de-wired | ✓ VERIFIED | UITypeID.h: TUIRunner (:63) → blank → TUIStyle (:65); `rg TUIWebBrowser UITypeID.h` → 0; `rg TUIRunner` → 1. All 5 focus chains (CuiIoWin :513/:647, IMEManager :353, SwgCuiHud, SwgCuiTcgControl.h:93) are syntactically clean — no dangling `\|\|`. |
| 4 | (D-03/D-04) libEverQuestTCG + TCG infra preserved; only the browser tie severed | ✓ VERIFIED | `rg -c libEverQuestTCG SwgCuiTcgManager.cpp` → 27 (intact). SwgCuiTcgControl.h:93 = `return UIWidget::IsA(Type);` (browser clause dropped). No navigate-proc/registration refs remain. |
| 5 | (SCOPE GUARD) sibling list/table *Browser UIs untouched; G15Lcd stubs kept for caller-linking | ✓ VERIFIED | `rg -c "SwgCuiCommandBrowser\|SwgCuiMissionBrowser\|chatRoomBrowser\|persistentMessageBrowser" SwgCuiHudAction.cpp` → 10 (intact). SwgCuiG15Lcd.cpp/.h on disk; defines initializeLcd/updateLcd/remove (lines 321/191/409); ClCompile+ClInclude kept (2). |
| 6 | (D-09) vendored `src/external/3rd/library/libMozilla/` tree deleted entirely | ✓ VERIFIED | `test -d` → DELETED. Repo-wide `nsIWebBrowser\|nspr4\|plc4\|profdirserviceprovider` → 0 (exit 1). |
| 7 | (D-12.1) Milestone-wide residue sweep (P12-P15) == 0 with exact KEEP-list | ✓ VERIFIED | vivox (excl ChatAPI2/libs) → 0; videocapture/AudioCapture → 0; stationapi → 0; lcdui in src → only SwgCuiG15Lcd.cpp:26 removal comment; trackIR → 4 removal comments + checkTrackIr label only; SwgClientSetup → 6 LaunchMeFirst orphan refs only. KEEP assets (ChatAPI2, soePlatform/libs Win32-Debug/Release, 989crypt.lib) all present. |
| 8 | (SC#3 / D-13) SwgClient Debug AND Release link clean (unresolved external symbol == 0); Optimized EXEMPT | ✓ VERIFIED (records + structural) | Build logs are ephemeral/gitignored per phase convention; 15-01-SUMMARY + 15-03-SUMMARY + 15-04-SUMMARY each record Debug+Release `unresolved external symbol == 0` (SwgClient_d.exe 70.1MB, SwgClient_r.exe 28.4MB). Structurally corroborated: G15Lcd stub symbols still defined (no missing-symbol), all 5 phase commits present (8e6dd21fd..16fd3ac4c). Optimized EXEMPT per pre-existing DEF-14-01 SAFESEH (correctly not gated). |
| 9 | (SC#4 / DECRUFT-07 / D-14) Dual-renderer boot to char-select under rasterMajor=5 AND =11 | ✓ VERIFIED (human-approved) | Human checkpoint, user APPROVED this session (15-04-SUMMARY Task 2; boot-gate-PASS commit 16fd3ac4c). stage/client_d.cfg:37 = `rasterMajor=11` (committed end-state). Not scriptable / not re-run — needs live SWGSource VM. |

**Score:** 9/9 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/external/3rd/library/ui/src/shared/core/UITypeID.h` | TUIWebBrowser removed; TUIStyle follows TUIRunner | ✓ VERIFIED | TUIRunner :63 → blank :64 → TUIStyle :65; no placeholder; enum otherwise intact |
| `SwgClient.vcxproj` | All Mozilla-family link tokens/dirs/includes removed (3 configs) | ✓ VERIFIED | `rg -i Mozilla-family → 0` (exit 1); Debug/Optimized/Release AdditionalDependencies inspected directly — no nspr4/plc4/profdirserviceprovider/xpcom/xul/libMozilla; KEEP tokens (legacy_stdio_definitions, soePlatform, libEverQuestTCG) present |
| `SwgCuiTcgManager.cpp` | Browser tie severed, all TCG infra intact | ✓ VERIFIED | libEverQuestTCG → 27 refs; no WebBrowser/navigate refs |
| `swg.sln` | libMozilla project + 6 config-platform + 9 dependency back-refs removed | ✓ VERIFIED | GUID + literal libMozilla → 0; SwgClient project intact |
| `swgClientUserInterface.vcxproj` / `SwgGodClient.vcxproj` | lcdui A1 residue removed; G15Lcd ClCompile/ClInclude kept | ✓ VERIFIED | `rg -i lcdui` → 0; SwgCuiG15Lcd entries → 2 (kept) |
| `stage/client_d.cfg` | rasterMajor=11 end-state | ✓ VERIFIED | :37 = rasterMajor=11 (gitignored, not committed — expected) |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|----|--------|---------|
| callers (SwgCuiManager/CommandParserUI/HudAction/TcgManager) | SwgCuiWebBrowserManager (deleted) | de-wire all call sites | ✓ WIRED (severed cleanly) | 0 residual WebBrowserManager refs in any caller; all units still compile (link grep == 0 per records) |
| 5 IsA(TUIWebBrowser) sites | TUIWebBrowser enum (deleted) | edit all sites with enum delete | ✓ WIRED | All 5 focus chains valid; no `TUIWebBrowser undeclared` (build links clean) |
| SwgCuiWebBrowser*.cpp + Game.cpp include | libMozilla.lib + XPCOM libs (unlinked) | delete consumers, unlink tokens | ✓ WIRED | Game.cpp libMozilla include → 0; SwgClient link tokens → 0; vendored tree gone; link green per records |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Repo-wide XPCOM/WebBrowser grep-zero | `rg -i "<GATE-XPCOM set>" src --glob '!*.filters'` | exit 1 (0 matches) | ✓ PASS |
| swg.sln GUID drop | `rg "C6C1E14A...\|libMozilla" swg.sln` | exit 1 (0 matches) | ✓ PASS |
| Vendored tree deleted | `test -d .../libMozilla` | DELETED | ✓ PASS |
| Milestone vivox/videocapture sweep | `rg -i "vivox\|videocapture\|AudioCapture" src (scoped)` | exit 1 (0 matches) | ✓ PASS |
| G15Lcd stub symbols still defined | `rg "SwgCuiG15Lcd::(initializeLcd\|updateLcd\|remove)"` | 3 defs found | ✓ PASS |
| Dual-renderer boot to char-select | live SWGSource VM launch under rasterMajor=5 + =11 | human-approved this session | ? SKIP (human-confirmed PASS; not re-run) |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| DECRUFT-06 | 15-01,15-02,15-03,15-04 | In-game browser (XPCOM/Mozilla) fully removed from source + build; sln drop; includePaths.rsp purge; CuiWebBrowser*/UIWebBrowserWidget/XPCOM bridge deleted; no Mozilla DLL in stage copy; builds + boots | ✓ SATISFIED | Truths 1,2,3,6,8 + artifacts; repo-wide grep-zero; sln GUID drop; 9 files deleted; PostBuildEvent no-Mozilla; Debug+Release link clean (records) |
| DECRUFT-07 | 15-04 | Compiles clean + boots to char-select under both rasterMajor=5 and =11 after full removal set (cross-cutting milestone gate) | ✓ SATISFIED | Truth 9 (human-approved boot gate); Truth 8 (link clean); client_d.cfg=11 |

Both Phase-15 requirements (DECRUFT-06, DECRUFT-07) are accounted for and satisfied. No orphaned requirements: REQUIREMENTS.md maps only these two IDs to Phase 15, and both appear in the plans' `requirements` fields.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| SwgCuiTcgControl.h / focus call sites | n/a | TCG control lost input-routing identity (WR-01) | ℹ️ Info (out of scope) | Deliberate T-15-02/D-04a TCG-tie severance; TCG launch path commented out; TCG revival is a future v2.x concern, NOT a Phase-15 gap |
| SwgCuiTcgManager.cpp | 65-70 | navigate callbacks now NULL (WR-02) | ℹ️ Info (out of scope) | D-04a threat-model-accepted (callbackTable zero-inits; SWGTCG.dll absent on VM); flagged for future TCG revival |
| SwgCuiHudAction.cpp | 1170-1190 | dead-store finalUrl (WR-03) | ℹ️ Info | Over-deletion residue; CS browser action de-wired; cosmetic, no build/behavior break; carries no grep-gate token |

All 3 code-review warnings are consistent with the plan's deliberate, threat-model-accepted TCG-tie severance (T-15-02 / D-04a) and the CS-action de-wire. They are explicitly out of scope for Phase 15 (the review itself found 0 blockers). Not treated as phase gaps.

### Human Verification Required

None outstanding. The one non-scriptable checkpoint (DECRUFT-07 dual-renderer boot gate) was already HUMAN-verified and user-APPROVED this session (15-04-SUMMARY Task 2; boot-gate-PASS commit 16fd3ac4c). Per protocol it is counted as a verified truth, not a re-opened human item. It was NOT re-run (needs the live SWGSource VM).

### Gaps Summary

No gaps. Every must-have across all 4 plans was independently confirmed against the live tree:
- Source/link removal (15-01): 9 WebBrowser files deleted, TUIWebBrowser gone with no placeholder, all 5 IsA sites + callers de-wired cleanly, TCG tie severed with all TCG infra preserved, SwgClient.vcxproj Mozilla tokens gone (verified by direct inspection of all 3 config link lines).
- Residue purge (15-02): .rsp + 7 editors + SwgGodClient Mozilla tokens all → 0; KEEP assets (989crypt.lib) preserved.
- sln + tree (15-03): GUID dropped at all locations, vendored tree deleted, sln well-formed.
- Milestone gate (15-04): full P12-P15 residue sweep == 0 / KEEP-listed (vivox, videocapture, trackIR, SwgClientSetup, lcdui, stationapi all confirmed), lcdui A1 cleanup done with G15Lcd stub kept, build link-clean (records), dual-renderer boot human-approved, client_d.cfg=rasterMajor=11.

The D3D11 visual-parity caveat noted in 15-04-SUMMARY is the pre-existing v2.2 milestone deferral (tracked since Phase 11), NOT a Decruft regression — DECRUFT-07's bar is "boots to character select under both renderers," which is met.

---

_Verified: 2026-05-26_
_Verifier: Claude (gsd-verifier)_
