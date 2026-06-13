---
phase: 24-dpvs-config-gate-machine-portability
verified: 2026-06-13T00:00:00Z
status: human_needed
score: 11/11 must-haves verified (code); 4 items require human runtime verification
overrides_applied: 0
human_verification:
  - test: "DPVS occlusion-mode in-game: set occlusionMode=auto in client_d.cfg, walk Mos Eisley -> cantina. With the Phase-23 DPVS DebugMonitor overlay open, confirm the occlusion bit is SET outdoors and CLEAR inside the cantina (visible-object signal changes). Press F11 — confirm it force-disables occlusion regardless of mode (D-04). Then test occlusionMode=on (bit unconditional) and occlusionMode=off (bit never set). Also confirm that a Debug build with NO occlusionMode key in the cfg defaults to OFF (no longer on-by-default)."
    expected: "auto -> occlusion active outdoors / inactive in cantina; F11 wins over any mode; on -> always set; off/absent -> never set"
    why_human: "Requires interactive in-game traversal from an outdoor cell into a POB cell with the DPVS DebugMonitor overlay open — cannot be verified by code inspection or offline command"
  - test: "Dual-renderer boot gate: boot stage/SwgClient_d.exe with rasterMajor=11 (D3D11) to character select, then with rasterMajor=5 (D3D9). Confirm both renderers reach character select cleanly after the multi-stream-VB default flip and the setBloomEnabled_impl no-op binding."
    expected: "Both rasterMajor=11 and rasterMajor=5 boot to character select with no FATAL / crash"
    why_human: "Requires launching the game executable and reaching character select — cannot be verified offline"
  - test: "Miles audio from vendored redist: after a clean build (postbuild runs), confirm stage/miles is populated from the vendored redist (check Msseax.m3d = 143,872 B present). Launch SwgClient_d.exe and confirm music + in-world audio plays (not just UI rollovers). Then rename stage/miles to stage/miles_off, relaunch, and confirm the log shows EXACTLY ONE Miles codec-absence WARNING and no per-sample 'Error loading and allocating the sample' flood. Restore stage/miles afterward."
    expected: "Audio plays normally when miles is present; exactly one startup WARNING and zero per-sample flood when miles is absent"
    why_human: "Requires game launch with audio playback judgment and log inspection — cannot be verified programmatically"
  - test: "Fresh-clone setup test (D-10): run tools/setup/setup-client.ps1, supply a TRE root, confirm it generates stage/client_d.cfg + stage/client.cfg with the clone's own repo-relative stage/override path (no hardcoded D:/Code/swg-client-v2/stage/override), no dead keys (swg_dev_bundle, disableMultiStreamVertexBuffers, [ClientGame/Bloom] enable=0, disableG15Lcd, voiceChatEnabled, reportFrameStats), and that both generated cfgs boot to character select on both rasterMajor=5 and =11."
    expected: "Generated cfgs contain no machine-specific absolute paths beyond the supplied TRE root; boot succeeds on both renderers; no dead keys present"
    why_human: "Requires game boot and visual inspection of the generated cfg content to confirm absence of machine-specific paths and dead keys; the fresh-clone-on-another-network scenario cannot be tested programmatically on a single machine"
---

# Phase 24: DPVS Config-Gate + Machine Portability — Verification Report

**Phase Goal:** DPVS Config-Gate + Machine Portability — Occlusion auto-gated on POB-cell membership (HARD-01); de-hardcoded stage paths + machine-portable Miles audio redist + cleaned client_d.cfg (PORT-01); cfg template + generator + Phase-11+ test-key cleanup (PORT-02); dual-renderer (D3D9/D3D11) boot verified.
**Verified:** 2026-06-13
**Status:** human_needed
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | D-01: occlusionMode=auto sets OCCLUSION_CULLING bit only when ms_cameraCell->isWorldCell() is true | ✓ VERIFIED | RenderWorld.cpp:922-923 — `case ConfigClientGraphics::DOM_auto: occlusionBit = (ms_cameraCell && ms_cameraCell->isWorldCell()) ? DPVS::Camera::OCCLUSION_CULLING : 0;` |
| 2 | D-01: occlusionMode=on sets bit unconditionally; occlusionMode=off (default) never sets it | ✓ VERIFIED | RenderWorld.cpp:919-928 — switch cases DOM_on/DOM_off/default confirmed in source |
| 3 | D-02: default mode is off — shipped behavior stays Option alpha when key is absent | ✓ VERIFIED | ConfigClientGraphics.cpp:144,150 — getKeyString default is "off"; _stricmp else branch sets DOM_off; DOM_off=0 is zero-init safe |
| 4 | D-04: ms_forceDisableOcclusionCulling (F11) clears occlusion bit regardless of config | ✓ VERIFIED | RenderWorld.cpp:931-935 — inside `#ifdef _DEBUG`, applied AFTER the config/auto switch, clears occlusionBit unconditionally |
| 5 | D-07: disableMultiStreamVertexBuffers default flipped to false | ✓ VERIFIED | ConfigClientGraphics.cpp:111 — `KEY_BOOL(disableMultiStreamVertexBuffers, false)` |
| 6 | D-07: D3D11 setBloomEnabled bound to non-fatal setBloomEnabled_impl (not scaffold_fatal_stub) | ✓ VERIFIED | Direct3d11.cpp:322 defines `void setBloomEnabled_impl(bool)`, :1154 binds `ms_glApi.setBloomEnabled = setBloomEnabled_impl`; STUB(setBloomEnabled) is absent |
| 7 | D-11: vendored redist is the proven stage/miles byte-set + 2 .m3d providers | ✓ VERIFIED | Msseax.m3d=143,872 B, msssoft.m3d=79,360 B, mssogg.asi=99,840 B, mssmp3.asi=94,208 B, msseax.flt=59,392 B, mssdsp.flt=56,832 B, mssds3d.flt=12,800 B — all match the A1 target column |
| 8 | D-11: SwgClient postbuild copies miles from repo-relative vendored redist (not machine-specific path) | ✓ VERIFIED | SwgClient.vcxproj:134-135 (Debug) and :242-243 (Release) — both reference `miles-7.2e\redist`; zero occurrences of `SWGSource Client v3.0\miles` |
| 9 | D-12: Audio::install emits exactly one startup WARNING + s_milesCodecRedistAvailable flood guard | ✓ VERIFIED | Audio.cpp:101 declares flag (default true); :1295-1314 probe sets false + emits one WARNING; :1564 uses `DEBUG_WARNING(s_milesCodecRedistAvailable, ...)` |
| 10 | D-08/D-09: tracked template + generator produce both cfgs; dead keys absent; @LOGIN_SERVER@ placeholder present | ✓ VERIFIED | tools/setup/client.cfg.template exists with all required @...@ tokens; swg_dev_bundle/disableMultiStreamVertexBuffers/disableG15Lcd/voiceChatEnabled/reportFrameStats/[ClientGame/Bloom] absent; setup-client.ps1 generates both cfgs; gitignore correctly excludes generated cfgs and tracks template |
| 11 | Docs + gitignore updated: src/cmake/config reference removed, build.md references setup-client.ps1 workflow | ✓ VERIFIED | .gitignore:84 comment now reads "tools/setup/" (grep for "src/cmake/config" returns 0 matches); docs/build.md:169-200 describes setup-client.ps1 workflow with no reference to build/bin/Debug/client.cfg |

**Score:** 11/11 code truths verified

---

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `ConfigClientGraphics.h` | DpvsOcclusionMode enum + getDpvsOcclusionMode() declaration | ✓ VERIFIED | Lines 19-31: enum with DOM_off=0/DOM_on/DOM_auto; line 68: static getter declared |
| `ConfigClientGraphics.cpp` | occlusionMode parse-at-install + getter; multistream default flip | ✓ VERIFIED | Lines 137-151: getKeyString parse; line 380-383: getter; line 111: KEY_BOOL false |
| `RenderWorld.cpp` | config-gated occlusion bit replacing Option alpha #else branch | ✓ VERIFIED | Lines 903-945: full switch + F11 override; old Option alpha `#else DPVS::Camera::VIEWFRUSTUM_CULLING` is gone |
| `Direct3d11.cpp` | setBloomEnabled_impl no-op free function + slot binding | ✓ VERIFIED | Line 322: function defined; line 1154: slot bound; STUB(setBloomEnabled) absent |
| `src/external/3rd/library/miles-7.2e/redist/Msseax.m3d` | EAX 3D provider (was absent), 143,872 B | ✓ VERIFIED | File exists at 143,872 bytes |
| `src/external/3rd/library/miles-7.2e/redist/msssoft.m3d` | Software 3D provider (was absent), 79,360 B | ✓ VERIFIED | File exists at 79,360 bytes |
| `SwgClient.vcxproj` | postbuild Miles copy repointed to vendored redist, .m3d guard | ✓ VERIFIED | Both Debug (:134-135) and Release (:242-243) blocks reference `miles-7.2e\redist`; guard on Msseax.m3d and msssoft.m3d; zero machine-specific paths |
| `Audio.cpp` | s_milesCodecRedistAvailable + one-shot warning + flood guard | ✓ VERIFIED | Lines 101, 1295-1314, 1564 |
| `tools/setup/client.cfg.template` | PORT-02-cleaned key set with PORT-01 placeholders + occlusionMode | ✓ VERIFIED | All @...@ tokens present; dead keys absent; occlusionMode=@OCCLUSION_MODE@ in [ClientGraphics/Dpvs] |
| `tools/setup/setup-client.ps1` | cfg generator with validation, auto-substitution, miles check, banner | ✓ VERIFIED | [CmdletBinding()]/param form; Test-Path + cfg-breaking-char reject; @OVERRIDE_PATH@ auto-substituted; @LOGIN_SERVER@ param; miles validation; next-steps banner |
| `.gitignore` | tools/setup/ tracked; generated cfgs ignored; stale comment corrected | ✓ VERIFIED | stage/client_d.cfg is gitignored; tools/setup/client.cfg.template is not ignored; "src/cmake/config" absent from file |
| `docs/build.md` | Runtime asset setup section references setup-client.ps1 workflow | ✓ VERIFIED | Lines 169-200 describe the PowerShell generator workflow |

---

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| RenderWorld.cpp drawScene | ConfigClientGraphics::getDpvsOcclusionMode | enum getter consumed in switch block | ✓ WIRED | RenderWorld.cpp:917 calls getter; result drives occlusionBit |
| RenderWorld.cpp auto branch | CellProperty::isWorldCell | ms_cameraCell->isWorldCell() with null guard | ✓ WIRED | RenderWorld.cpp:923 — `(ms_cameraCell && ms_cameraCell->isWorldCell())` |
| Direct3d11.cpp install | setBloomEnabled_impl | ms_glApi.setBloomEnabled = setBloomEnabled_impl | ✓ WIRED | Direct3d11.cpp:1154 binds the slot |
| SwgClient.vcxproj postbuild | src/external/3rd/library/miles-7.2e/redist | xcopy /E /I /Y into stage/miles | ✓ WIRED | Both Debug and Release postbuild blocks confirmed |
| Audio::install probe | Audio.cpp:1564 per-sample DEBUG_WARNING | s_milesCodecRedistAvailable flag set at install, read at flood site | ✓ WIRED | Flag declared at :101, set at :1312, consumed at :1564 |
| tools/setup/setup-client.ps1 | tools/setup/client.cfg.template | reads template, substitutes tokens, writes stage/client*.cfg | ✓ WIRED | script:86 reads template; New-Cfg function at :88-102 performs substitutions; lines 113-116 write both cfgs |
| tools/setup/setup-client.ps1 | stage/miles codec set | Test-Path validation of .asi/.m3d files | ✓ WIRED | script:119-130 validates mssmp3.asi/mssogg.asi/Msseax.m3d/msssoft.m3d |

---

### Data-Flow Trace (Level 4)

Not applicable — this phase produces config/build infrastructure, not data-rendering components.

---

### Behavioral Spot-Checks

Not applicable — no runnable entry points can be exercised without launching the game executable. The build is established (see context). Runtime behaviors are routed to human_verification.

---

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| HARD-01 | 24-01 | DPVS occlusion config-gated per Phase 23 verdict | ✓ SATISFIED | DpvsOcclusionMode enum + getDpvsOcclusionMode() + RenderWorld config-gate with null-guarded auto branch + F11 override all verified in source |
| PORT-01 | 24-02, 24-03 | Fresh clone + build produces bootable client with no machine-specific absolute paths | ✓ CODE SATISFIED / runtime human_needed | Vendored redist reconciled; postbuild repointed; Audio flood guard present; template + generator in place; docs updated; runtime boot verification is the human checkpoint |
| PORT-02 | 24-01, 24-03 | client_d.cfg cleaned of Phase-11+ test settings; boots clean on both renderers | ✓ CODE SATISFIED / runtime human_needed | Engine-default flips (multistream VB, Bloom no-op) make override keys redundant; template confirmed clean of all audit-flagged dead keys; dual-renderer boot is the human checkpoint |

---

### Anti-Patterns Found

| File | Issue | Severity | Impact |
|------|-------|----------|--------|
| `SwgClient.vcxproj` (Optimized config) | WR-02 (from 24-REVIEW.md): Optimized|Win32 config has no PostBuildEvent — no exe staging + no stage/miles repair for _o builds | Warning | Does not affect Debug/Release portability goal; Optimized config is a pre-existing known-broken link target per project history (SwgClient_o.exe uses FORCE linker masking). Advisory only, not a HARD-01/PORT-01/PORT-02 blocker. |
| `SwgClient.vcxproj` (Debug + Release postbuild) | WR-01 (from 24-REVIEW.md): guard is keyed on .m3d files; a stale stage that has both .m3d providers but a stale mssmp3.asi passes both guards without repair | Warning | The Audio probe at runtime will catch the stale case and emit a startup warning, so the failure surface is "silent music + one warning" rather than "silent + flood". The current vendored redist bytes are the proven set, so a fresh clone postbuild will always get correct bytes. Advisory. |

No stub patterns (TODO/FIXME/placeholder/return null/empty implementations) found in any of the 8 source files modified by this phase.

---

### Human Verification Required

#### 1. In-game DPVS occlusion-mode verification

**Test:** Set `[ClientGraphics/Dpvs] occlusionMode=auto` in stage/client_d.cfg. Launch SwgClient_d.exe (rasterMajor=11). With the Phase-23 DPVS DebugMonitor overlay open, walk from Mos Eisley plaza into the cantina. Check that the occlusion bit is active outdoors (visible-object count reduced vs no-occlusion baseline) and inactive inside the cantina. Then test F11 — confirm it force-disables occlusion regardless of mode. Then test occlusionMode=on (bit always active) and occlusionMode=off (bit never active). Finally, remove the occlusionMode key entirely and confirm the Debug build defaults to OFF (the bit is not active).

**Expected:** auto -> outdoor on / cantina off; F11 always wins; on -> unconditional; off/absent -> none

**Why human:** Requires interactive in-game traversal through a portal into a POB cell with the DPVS DebugMonitor overlay (F10/overlay hotkey) visually readable — no offline inspection can substitute.

#### 2. Dual-renderer boot gate

**Test:** With stage/client_d.cfg at rasterMajor=11, launch stage/SwgClient_d.exe and confirm it boots to character select (skinned characters render — the multi-stream-VB flip is exercised; no Bloom FATAL). Then set rasterMajor=5, relaunch, and confirm D3D9 boot to character select.

**Expected:** Both rasterMajor=11 and rasterMajor=5 reach character select cleanly with no crash or FATAL.

**Why human:** Requires launching the executable and reaching character select under each renderer — cannot be verified offline.

#### 3. Miles audio from vendored redist + flood suppression

**Test:** After a build (postbuild stages miles), launch SwgClient_d.exe. Confirm music + in-world audio play (not just UI rollovers). Check the log has no "Error loading and allocating the sample" flood. Then rename stage/miles to stage/miles_off, relaunch — confirm exactly ONE startup WARNING appears in the log (the `Audio: Miles codec/provider redist missing` message) and NO per-sample flood fires. Restore stage/miles.

**Expected:** Audio plays when miles is present; exactly one warning + zero flood when miles is absent.

**Why human:** Requires audio playback judgment (music audible vs not) and log inspection for flood count — cannot be verified programmatically.

#### 4. Fresh-clone setup + generated cfg quality

**Test:** Run `tools/setup/setup-client.ps1` with a supplied TRE root. Inspect the generated stage/client_d.cfg: confirm it contains no hardcoded machine-specific path other than the supplied TRE root (stage/override should be the repo-relative path for this clone), no dead keys (swg_dev_bundle, disableMultiStreamVertexBuffers, [ClientGame/Bloom] enable=0, disableG15Lcd, voiceChatEnabled, reportFrameStats), loginServerAddress0 = 192.168.1.200 (or the -LoginServer value), and occlusionMode=auto in the Debug cfg. Then boot SwgClient_d.exe from the generated cfg on both rasterMajor=5 and =11.

**Expected:** Generated cfg is clean (no dead keys, no stale machine paths); both renderers boot to character select.

**Why human:** The V-key removal verdicts (runtimeDisableAsynchronousLoader removed, skipSplash=true) require a boot+play test to confirm no regression — the template was authored with these removals on the assumption of a clean boot, and that assumption needs runtime confirmation.

---

## Gaps Summary

No code-level gaps found. All 11 code must-haves are VERIFIED in the actual source. The four human_verification items are by-design runtime checkpoints (plan autonomous:false; Task-4 human-verify gates in all three plans). They cannot be classified as gaps because:

- The code is correct and complete.
- The behaviors being verified (in-game occlusion overlay, renderer boot, audio playback, generated cfg correctness) are definitionally unverifiable by code inspection.
- The project context explicitly states these are genuine runtime-only verifications that only the user can perform.

**WR-01 and WR-02** from the code review are advisory warnings carried forward; neither blocks the phase goal for Debug/Release configs (the primary development targets). WR-02 (Optimized config) reflects a pre-existing known-broken state documented in project history.

---

_Verified: 2026-06-13_
_Verifier: Claude (gsd-verifier)_
