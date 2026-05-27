---
phase: 16-v2-1-tech-debt-cleanup
verified: 2026-05-27T00:00:00Z
status: passed
score: 8/8 must-haves verified
overrides_applied: 0
re_verification: false
gaps: []
deferred: []
human_verification: []
---

# Phase 16: v2.1 Tech-Debt Cleanup — Verification Report

**Phase Goal:** Close the non-blocking tech debt from the v2.1 milestone audit so v2.1 completes clean — (1) unlink the dead SwgGodClient 989crypt.lib token, (2) verify the 4 editor vcxproj are lcdui-clean (already swept by 15-04, verify-only), (3) remove cosmetic dead source (finalUrl block in SwgCuiHudAction.cpp + voice-volume API in CuiPreferences.cpp/.h). Invariant: SwgClient still links clean (Debug+Release link-grep unresolved-external == 0) and boots to character-select after the cleanup.
**Verified:** 2026-05-27
**Status:** PASSED
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | D-01: 989crypt.lib token gone from SwgGodClient.vcxproj; deleted stationapi/ lib NOT re-added | VERIFIED | `rg -i "989crypt" SwgGodClient.vcxproj` → exit 1 (0 matches); `rg -i "stationapi" SwgGodClient.vcxproj` → exit 1 (0 matches) |
| 2 | D-02: Full dep-list sweep across 3 configs — only 989crypt.lib was dead and is removed; all 9 soePlatform KEEP tokens + separate live crypto.lib survive | VERIFIED | D-02 sweep (`lcdui\|VideoCapture\|vivox\|libMozilla\|xpcom\|xul\|VChatAPI`) → exit 1 (0 matches). KEEP-list: Base.lib=1, ChatAPI.lib=1, ChatMono.lib=1, CommodityAPI.lib=1, dbgutil.lib=1, monapi.lib=1, Network.lib=2 (incl. sharedNetwork — both legitimate), rdp.lib=1, TcpLibrary.lib=1; `crypto.lib` → still present (exit 0). |
| 3 | D-03: SwgGodClient verified grep-only; never built (pre-broken Qt MSB8066, out of /t:SwgClient closure) | VERIFIED | No build invoked per plan and SUMMARY. `rg -i "989crypt"` used as verification; no msbuild target referenced SwgGodClient.vcxproj. |
| 4 | D-04: All 4 editor vcxproj (AnimationEditor/ClientEffectEditor/LightningEditor/ParticleEditor) contain 0 lcdui references — verify-only, no edit made | VERIFIED | `rg -ni "lcdui"` across all 4 editor vcxproj → exit 1 (0 matches); git log shows no post-phase-16 commits on those files. |
| 5 | D-05: Doc-staleness items (12-VERIFICATION.md stale score line; 13-SUMMARY.md empty requirements_completed) intentionally out of scope — no doc edits made | VERIFIED | `git log --after="2026-05-26"` for both files → empty output (no commits). |
| 6 | D-06: Dead finalUrl block (~:1170-1189) removed from SwgCuiHudAction.cpp; now-dead ConfigClientGame.h and shellapi.h includes removed; narrow scope honored — httpParams accumulation (~:1079-1166), s_confirmCsBrowserSpawn confirm-box, and enclosing closing brace at :1167 intact | VERIFIED | `rg "finalUrl" SwgCuiHudAction.cpp` → exit 1; `rg "ConfigClientGame" SwgCuiHudAction.cpp` → exit 1; `rg "shellapi\|ShellExecute\|SW_SHOW" SwgCuiHudAction.cpp` → exit 1. Live check confirms `httpParams` block at lines 1079-1166, `s_confirmCsBrowserSpawn` at 1073-1074, and closing `}` at line 1167 all present. Commit `9ffd140b7` exists. |
| 7 | D-06/D-07: Full voice-volume API removed from CuiPreferences — 2 statics, 2 REGISTER_OPTION persistence lines, 4 accessor defs, 4 header decls; zero external callers confirmed; no ordinal-preserving placeholders (CR-01 inapplicable to plain float statics) | VERIFIED | `rg "speakerVolume\|micVolume\|SpeakerVolume\|MicVolume" src` → exit 1 (0 matches repo-wide); `rg "REGISTER_OPTION\(speakerVolume\)\|REGISTER_OPTION\(micVolume\)" src` → exit 1; `rg "ms_speakerVolume\|ms_micVolume\|[gs]etSpeakerVolume\|[gs]etMicVolume" src` → exit 1; `rg "RESERVED_\|placeholder" CuiPreferences.cpp/.h` → exit 1 (no placeholders). Commits `842b44989` (CuiPreferences) exist. |
| 8 | D-08: SwgClient Debug AND Release link logs contain 0 'unresolved external symbol' (grepped from LOG, not msbuild exit code; Optimized EXEMPT per DEF-14-01); client boots ONCE to character-select under rasterMajor=11 (D3D11) with no crash/init regression | VERIFIED | 16-03-SUMMARY: both Debug and Release link logs grep 0 unresolved externals; exes deleted-then-recreated (real relink confirmed); Optimized not built (EXEMPT). Boot gate: operator-confirmed rasterMajor=11 (D3D11) boot reached character-select and ran ~670s (~11 min) in-world. In-world Options-window crash is confirmed pre-existing defect (SwgCuiOptUi.cpp:219 / CuiMediator.cpp:1487 `checkShowToolbarCommandCooldownTimer` .ui-data mismatch from feature commit d1b3c0eaf); phase-16 diff touches none of SwgCuiOptUi/SwgCuiOpt/CuiMediator/.ui — not a phase-16 regression. |

**Score:** 8/8 truths verified

---

## Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/game/client/application/SwgGodClient/build/win32/SwgGodClient.vcxproj` | Debug AdditionalDependencies with 989crypt.lib removed, soePlatform KEEP-list + crypto.lib intact | VERIFIED | Single-token deletion at line ~99; all 9 KEEP tokens + crypto.lib confirmed present by per-token grep; commit `577f68def` |
| `src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudAction.cpp` | CuiActions::service branch with dead finalUrl block + now-dead includes removed, enclosing brace + httpParams logic intact | VERIFIED | finalUrl/ConfigClientGame/shellapi all 0; httpParams at :1079-1166 and closing `}` at :1167 confirmed present; commit `9ffd140b7` |
| `src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp` | Voice-volume statics + 2 REGISTER_OPTION lines + 4 accessor bodies removed | VERIFIED | All voice-volume symbols 0 repo-wide; no placeholders; commit `842b44989` |
| `src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.h` | 4 voice-volume accessor declarations removed | VERIFIED | `rg "SpeakerVolume\|MicVolume\|speakerVolume\|micVolume" src` → 0; same commit `842b44989` |
| `stage/SwgClient_d.exe` | Freshly relinked Debug client binary | VERIFIED | Deleted-then-rebuilt via swg.sln /t:SwgClient; 0 unresolved externals in Debug link log (per 16-03-SUMMARY) |

---

## Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| SwgGodClient.vcxproj Debug `<AdditionalDependencies>` | 989crypt.lib token removed | Single-token inline deletion | VERIFIED | `rg -i "989crypt" SwgGodClient.vcxproj` → exit 1 |
| clientUserInterface.lib + swgClientUserInterface.lib (Plan 02 edits) | SwgClient Debug+Release link | swg.sln /t:SwgClient ProjectDependencies rebuild | VERIFIED | Both link logs: 0 `unresolved external symbol`; built through solution so edited libs were rebuilt before relinking |

---

## Data-Flow Trace (Level 4)

Not applicable — this is a removal-only phase. No components render dynamic data; all artifacts are build configuration or dead-code deletion. No data-flow tracing required.

---

## Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| SwgGodClient.vcxproj: 989crypt.lib absent | `rg -i "989crypt" SwgGodClient.vcxproj` | exit 1 (0 matches) | PASS |
| SwgGodClient.vcxproj: all 9 soePlatform KEEP tokens present | `rg "Base\.lib\|ChatAPI\.lib\|ChatMono\.lib\|CommodityAPI\.lib\|dbgutil\.lib\|monapi\.lib\|Network\.lib\|rdp\.lib\|TcpLibrary\.lib" SwgGodClient.vcxproj` | All 9 found | PASS |
| SwgGodClient.vcxproj: live crypto.lib intact | `rg "crypto\.lib" SwgGodClient.vcxproj` | present (exit 0) | PASS |
| All 4 editor vcxproj: 0 lcdui references | `rg -ni "lcdui"` across 4 files | exit 1 (0 matches) | PASS |
| SwgCuiHudAction.cpp: finalUrl/ConfigClientGame/shellapi gone | `rg "finalUrl\|ConfigClientGame\|shellapi\|ShellExecute\|SW_SHOW"` | exit 1 (0 matches) | PASS |
| SwgCuiHudAction.cpp: httpParams accumulation intact | `rg -n "httpParams"` | lines 1079-1166 present | PASS |
| SwgCuiHudAction.cpp: enclosing service-branch `}` intact | Read lines 1155-1174 | `}` at line 1167 confirmed | PASS |
| CuiPreferences: all voice-volume symbols gone repo-wide | `rg "speakerVolume\|micVolume\|SpeakerVolume\|MicVolume" src` | exit 1 (0 matches) | PASS |
| CuiPreferences: REGISTER_OPTION persistence lines gone | `rg "REGISTER_OPTION\(speakerVolume\)\|REGISTER_OPTION\(micVolume\)" src` | exit 1 (0 matches) | PASS |
| CuiPreferences: no ordinal placeholders introduced | `rg "RESERVED_\|placeholder" CuiPreferences.cpp/.h` | exit 1 (0 matches) | PASS |
| Task commits exist in git history | `git log --oneline 577f68def 9ffd140b7 842b44989` | All 3 found | PASS |
| D-05: 12-VERIF + 13-SUMM not touched by phase 16 | `git log --after="2026-05-26" -- <file>` | Empty output for both | PASS |

---

## Requirements Coverage

No product REQ-IDs were assigned to Phase 16 (post-hoc tech-debt closure; traces to `.planning/v2.1-MILESTONE-AUDIT.md`). No requirements traceability checks required.

---

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `SwgCuiHudAction.cpp` | 1079-1166 | Orphaned write-only `httpParams` builder (IN-01 from 16-REVIEW.md) | Info | `s_allowServiceWindow` is hardcoded `false` at install lines 301/304, making the entire `CuiActions::service` branch unreachable. The `httpParams` block predates this phase's diff and is harmless (no behavior, no output). Confirmed by the code reviewer (0 blockers, 0 warnings, 1 info). Flagged for a follow-up tech-debt pass. |

No blockers. No warnings.

---

## Human Verification Required

None. D-08 boot gate was operator-confirmed prior to this verification (rasterMajor=11 D3D11 boot; character-select reached; ~11 min in-world; clean init). This is documented in 16-03-SUMMARY.md as a locked decision. The in-world Options-window crash is a confirmed pre-existing defect unrelated to this phase's diff, captured in `.planning/todos/pending/2026-05-27-options-toolbar-cooldown-ui-data-mismatch-crash.md`.

---

## Gaps Summary

No gaps. All 8 must-haves (D-01 through D-08) are verified against the live codebase.

The one INFO item (IN-01: orphaned `httpParams` builder) is not a blocker or regression — the branch is unreachable (`s_allowServiceWindow = false` hardcoded), and it predates this phase's diff. The code reviewer independently rated it 0 blockers / 0 warnings.

---

_Verified: 2026-05-27_
_Verifier: Claude (gsd-verifier)_
