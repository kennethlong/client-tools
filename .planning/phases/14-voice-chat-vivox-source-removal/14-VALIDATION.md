---
phase: 14
slug: voice-chat-vivox-source-removal
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-05-25
updated: 2026-05-26
---

# Phase 14 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> This is a **source-removal** phase: validation is grep-zero + Debug/Release
> link-grep + dual-renderer boot, NOT a unit-test suite. There is no pytest/jest
> harness — the "tests" are deterministic CLI greps and a manual boot gate.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | none — C++ build + ripgrep + manual boot gate |
| **Config file** | `src/build/win32/swg.sln` (MSBuild; msbuild NOT on PATH — use full path) |
| **Quick run command** | `rg -i "vivox" src/ --glob "*.rsp" --glob "*.vcxproj" -c` (expect 0 hits) |
| **Full suite command** | MSBuild Debug + Release, then grep link output for `unresolved external symbol` (expect 0); then dual-renderer boot |
| **Estimated runtime** | grep < 5s · full Debug+Release build ~minutes · boot gate manual |

---

## Sampling Rate

- **After every task commit:** Run the scoped grep for the symbols/refs that task removed (expect 0 new hits in the touched area) and confirm the affected library still compiles.
- **After every plan wave:** Build the affected libraries (and SwgClient where linked) Debug; grep link output for `unresolved external symbol` == 0.
- **Before `/gsd:verify-work`:** Full criterion sweep — criterion #1 grep-zero (`vivox`/`Vivox` across `.rsp`/source/include), criterion #2 symbol grep-zero, criterion #4 Debug **and** Release link clean (grep, not just exit 0), criterion #5 dual-renderer boot.
- **Max feedback latency:** grep gates < 5s; build gate = single-config build time.

---

## Per-Task Verification Map

> Each removal/de-wire task's automated check is a scoped ripgrep (symbol/ref count == 0
> after removal) plus the affected library compiling. Link-grep (criterion #4) and boot
> (criterion #5) are wave/phase-level gates, not per-task. Paths abbreviated:
> cUI = clientUserInterface · cGame = clientGame · sCUI = swgClientUserInterface ·
> sNM = sharedNetworkMessages.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 14-01-01 | 01 | 1 | DECRUFT-05 (D-02, D-03, D-03a, D-06) | T-14-01 | De-wire LIVE callers + registrations + strip prefs without breaking surviving surfaces (incl. non-voice SwgCuiHudWindowManager:318-319) | grep | `rg -n "CuiVoiceChat\|getVoiceShowFlybar\|ms_voice\|VOICE_INVITE\|voiceInvite\|voiceKick\|WS_VoiceFlyBar\|toggleVoiceFlyBar\|MAKE_SWG_CTOR_WS\(Voice\|REGISTER_OPTION_USER\(voice" <18 caller/registration/prefs files>` (exit 1 = 0 matches) | n/a (edits) | ⬜ pending |
| 14-01-02 | 01 | 1 | DECRUFT-05 (D-01) | T-14-02 | Delete whole voice subsystem (~24 files) + ClCompile/ClInclude entries; no orphaned symbol | grep + file-absent | `Test-Path` on deleted files == false; `rg -n "CuiVoiceChat\|SwgCuiVoice\|VoiceChat" cUI.vcxproj sCUI.vcxproj sNM.vcxproj` (exit 1) | ~24 files ABSENT | ⬜ pending |
| 14-01-03 | 01 | 1 | DECRUFT-05 (D-09) | T-14-02, T-14-03 | Remove inline vivox include-dirs + SwgClient link tokens; KEEP soePlatform\libs; Debug+Release link-grep == 0 | grep + link-grep | `rg -n "vivox\|VChatAPI\|Base_vchat\|libsndfile\|swgClientVivox" SwgClient.vcxproj cUI.vcxproj cGame.vcxproj` (exit 1); + Debug AND Release link log `unresolved external symbol` == 0 | n/a (edits) | ⬜ pending |
| 14-02-01 | 02 | 1 | DECRUFT-05 (D-05, D-09) | T-14-05 | Purge SwgClient vestigial .rsp + engine includePaths.rsp (vivox + dangling swgClientVivox) | grep | `rg -i "vivox\|swgClientVivox" <4 SwgClient .rsp + 2 includePaths.rsp>` (exit 1) | n/a (edits) | ⬜ pending |
| 14-02-02 | 02 | 1 | DECRUFT-05 (D-07) | T-14-06 | Purge 8 editor .rsp vivox refs + SwgGodClient.vcxproj inline tokens; KEEP soePlatform\libs | grep | `rg -i "vivox\|VChatAPI\|Base_vchat\|libsndfile" <16 editor .rsp + SwgGodClient.vcxproj>` (exit 1) | n/a (edits) | ⬜ pending |
| 14-03-01 | 03 | 2 | DECRUFT-05 (D-04, D-10, D-09) | T-14-08, T-14-09, T-14-10, T-14-12 | Delete vivox/, vivoxSharedWrapper/, soePlatform/VChatAPI/; KEEP soePlatform/libs + ChatAPI2; repo-wide grep-zero + Debug+Release link clean | grep + file-absent + link-grep | `Test-Path` on 3 trees == false; `Test-Path soePlatform/libs/Base.lib` == true; `rg -i "vivox" src/` (exit 1); + Debug AND Release link log `unresolved external symbol` == 0 | 3 trees ABSENT; libs/ PRESENT | ⬜ pending |
| 14-03-02 | 03 | 2 | DECRUFT-07 (D-08, D-06a) | T-14-11 | Dual-renderer boot to char-select (rasterMajor=5 AND =11); orphaned TRE assets no-op | manual boot (human) | set `rasterMajor=5` then `=11` in `stage/client_d.cfg`; launch `SwgClient_d.exe` vs SWGSource VM; reach char-select each | n/a (checkpoint) | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

*None — this is a removal phase with no new test infrastructure. Validation reuses the
existing MSBuild + ripgrep + manual boot toolchain established in Phases 12–13.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Criterion #1 — grep-zero `vivox`/`Vivox` across `.rsp`, source, include paths | DECRUFT-05 | CLI grep, no test harness | `rg -i "vivox" src/` returns 0 matches (after the Wave 2 vendored-tree delete) |
| Criterion #2 — voice-symbol grep-zero (`CuiVoiceChat*`, `SwgCuiVoice*`, `VOICE_INVITE`, `voiceInvite/Kick`, `setVoice*`, `ms_voice*`, `WS_VoiceFlyBar`, `toggleVoiceFlyBar`) | DECRUFT-05 | CLI grep | `rg "CuiVoiceChat\|SwgCuiVoice\|VOICE_INVITE\|voiceInvite\|voiceKick\|ms_voice\|WS_VoiceFlyBar\|toggleVoiceFlyBar" src/` returns 0 matches |
| Criterion #4 — Debug **and** Release link clean | DECRUFT-05 | `/FORCE` downgrades unresolved externals to warnings + still emits a binary (Phase 12/13 finding); must grep link log, not trust exit 0 | Build each config; `Select-String "unresolved external symbol"` over the link output returns 0 |
| Criterion #5 — dual-renderer boot to character select | DECRUFT-07 | Requires the SWGSource VM + live client launch; not scriptable | Set `rasterMajor=5` then `=11` in `client_d.cfg` (debug exe reads `client_d.cfg`); boot to char-select under each |

---

## Validation Sign-Off

- [x] Every removal/de-wire task has a scoped grep-zero automated check + affected library compiles
- [x] Sampling continuity: no wave leaves the tree un-buildable (D-02a — 14-01 is one coherent buildable change; 14-02 touches only vestigial .rsp/pre-broken editors; 14-03 deletes trees only after Wave 1's link tokens are gone)
- [x] Wave 0 N/A — existing toolchain covers all checks
- [x] No watch-mode flags
- [x] Link gate greps for `unresolved external symbol` (Debug AND Release), not just MSBuild exit 0 (D-09)
- [x] `nyquist_compliant: true` set in frontmatter — the planner's per-task map is complete

**Approval:** pending
