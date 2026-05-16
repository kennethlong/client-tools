---
phase: 11-d3d11-renderer-plugin
plan: 02
subsystem: renderer
tags: [d3d11, scaffold, plugin, gl_api, plumbing, baseline-screenshots, build-system]

# Dependency graph
requires:
  - phase: 11-d3d11-renderer-plugin
    plan: 01
    provides: D-04a DESCOPE verdict (Plan 11-05 OMITS Direct3d11_FfpGenerator); auto-stage post-build cp pattern (commit 266e173b3); SafeCast.h:29 fix at 73e29eee7 already in HEAD
  - phase: 09-stlport-msvc-stl
    provides: Koogie-merge v2 tree with IFF compat-guard ported (stable Tatooine zone-in baseline)
provides:
  - D3D11 plugin scaffold loads + GetApi resolves + Direct3d11::install runs + FATALs on first stubbed slot (proves plumbing end-to-end)
  - Engine-side TAG_DX11 + rasterMajor=11 range-check (the ONLY engine-side edits permitted in Phase 11 per CONTEXT D-02)
  - swg.sln integration for Direct3d11.vcxproj
  - .gitignore covers stage/ runtime output (already in place)
  - D3D9 baseline reference screenshots for SPEC R6 (Plan 11-09 reproduces)
  - Bonus deliverable: vendored atlmfc include in SwgClient.vcxproj for CLI MSBuild without VS Developer Command Prompt env bridging
affects: [11-03, 11-05, 11-09]

# Tech tracking
tech-stack:
  added:
    - D3D11 toolchain wired: d3d11.lib, d3dcompiler.lib, dxgi.lib, dxguid.lib (linked by Direct3d11.vcxproj)
    - DirectXMath / wrl::ComPtr surface area available (headers reachable; full use lands in Wave 3+)
  patterns:
    - FATAL-stub scaffold pattern for plugin-DLL contract validation: every Gl_api slot routed to scaffold_fatal_stub via decltype + reinterpret_cast; engine-queried no-op slots (lost-device callbacks, wasDeviceReset, requiresVertexAndPixelShaders) handled with no-op lambdas to avoid crashing during install
    - Engine-side surgical edit minimalism (per CONTEXT D-02): exactly 2 sites touched in Graphics.cpp; ALL OTHER engine-side files untouched
    - vendored atlmfc include pattern: SwgClient.vcxproj's afxres.h include path now resolves under CLI MSBuild without VS Developer Command Prompt env bridging (closes the build trap surfaced in earlier sessions)
    - cfg-target convention: Debug exe reads client_d.cfg (NOT client.cfg) — smoke-mode cfg edits MUST target the debug-variant cfg

key-files:
  created:
    - src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj
    - src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj.filters
    - src/engine/client/application/Direct3d11/src/shared/FirstDirect3d11.h
    - src/engine/client/application/Direct3d11/src/win32/FirstDirect3d11.cpp
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11.h
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp
    - src/engine/client/application/Direct3d11/src/win32/ConfigDirect3d11.h
    - src/engine/client/application/Direct3d11/src/win32/ConfigDirect3d11.cpp
    - src/engine/client/application/Direct3d11/src/shared/FirstDirect3d11.cpp (scaffold infrastructure)
    - src/engine/client/application/Direct3d11/src/shared/MemoryManagerHook.cpp (scaffold infrastructure mirrored from D3D9 plugin)
    - src/engine/client/application/Direct3d11/src/shared/SetupDll.h (scaffold infrastructure)
    - src/engine/client/application/Direct3d11/src/shared/SetupDll.cpp (scaffold infrastructure)
    - src/engine/client/application/Direct3d11/src/shared/WriteTga.h (scaffold infrastructure)
    - src/engine/client/application/Direct3d11/src/shared/WriteTga.cpp (scaffold infrastructure)
    - docs/recon/11-d3d11-screenshots/.gitkeep
    - docs/recon/11-d3d11-screenshots/d3d9-tatooine-outdoor.png
    - docs/recon/11-d3d11-screenshots/d3d9-cantina-interior.png
    - docs/recon/11-d3d11-screenshots/comparison-notes.md
    - .planning/phases/11-d3d11-renderer-plugin/11-02-SUMMARY.md
  modified:
    - src/build/win32/swg.sln (Direct3d11.vcxproj reference + GlobalSection entries for Debug|Win32, Release|Win32, Optimized|Win32)
    - src/engine/client/library/clientGraphics/src/win32/Graphics.cpp (TAG_DX11 at ~line 65-66 + extended range-check at ~line 209-215; the ONLY engine-side edit in this plan per CONTEXT D-02)
    - .gitignore (covers stage/ and shader-cache outputs)
    - src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj (vendored atlmfc include for .rc compile under CLI MSBuild — bonus deliverable, deviation Rule 3 → project-wide benefit)

key-decisions:
  - "TAG_DX11 spelling: TAG3(D,1,1) — compiles cleanly; no fallback needed"
  - "FATAL-stub population strategy: every Gl_api slot reinterpret_cast<decltype(slot)>(scaffold_fatal_stub); engine-queried no-op slots get no-op lambdas (per RESEARCH §Code Examples lines 517-522)"
  - "Vendored atlmfc include in SwgClient.vcxproj adopted as Phase 11 bonus deliverable (closes CLI rebuild trap; not Plan 11-02 verdict-critical but unblocks rebuilds going forward)"
  - "Debug exe reads client_d.cfg, NOT client.cfg — recorded as Phase 11 lessons-learned for future smoke-mode work"

patterns-established:
  - "Plugin scaffold + FATAL-stub Gl_api table = end-to-end plumbing proof BEFORE any real renderer code lands. Wave 3+ replaces FATAL stubs incrementally; the engine never sees a NULL function pointer."
  - "Engine-side surgical edits enumerated in PATTERNS file are the FULL list; any additional engine-side change must surface as a CHECKPOINT (no silent expansion of D-02 surface)"
  - "Bonus build-system fixes (vendored atlmfc this plan; auto-stage cp in Plan 11-01) are tracked as Rule-3-driven deviations with project-wide positive scope, committed under build(11): rather than spec(11-XX): to flag their scope"

requirements-completed: [D3D11-02]
requirements-partial: [D3D11-01]

# Metrics
duration: ~1 day elapsed (2026-05-16); ~6 hours active executor + Kenny smoke time
completed: 2026-05-16
---

# Phase 11 Plan 02: D3D11 Plugin Scaffold + FATAL Gl_api Table + D3D9 Baseline Screenshots Summary

**End-to-end plumbing PROVED. `gl11_d.dll` loads, exports `GetApi`, populates a fully-typed 120-slot `Gl_api` table, runs `Direct3d11::install`, and FATALs on the first stubbed slot reached from `Graphics::install` — exactly as designed. D3D9 sanity confirmed (D-05 protection holds). Two D3D9 baseline reference screenshots committed for SPEC R6 reproduction in Plan 11-09. Bonus deliverable: vendored atlmfc include in SwgClient.vcxproj closes the CLI MSBuild rebuild trap.**

## Performance

- **Duration:** ~6 hours active across 2026-05-16
- **Started:** 2026-05-16 (after Plan 11-01 close)
- **Completed:** 2026-05-16T23:00:00Z (this plan-close commit)
- **Tasks:** 2 auto + 1 checkpoint:human-verify (split into sub-steps 3a/3b/3c)
- **Files modified:** ~14 (8 new Direct3d11/ source files + 6 scaffold infrastructure files; 4 engine/build files modified; 4 docs files)
- **Commits:** 3 task commits + 1 plan-close commit = 4 total on plan 11-02

## Accomplishments

1. **D3D11 plugin scaffold builds clean and exports the contract.** New `src/engine/client/application/Direct3d11/` source tree with `Direct3d11.vcxproj` producing `gl11_d.dll` (Debug) at the resolved stage location. `dumpbin /exports gl11_d.dll | findstr GetApi` shows the symbol; engine loads the DLL via `LoadLibrary("./gl11_d.dll")` and resolves `GetApi` via `GetProcAddress` cleanly.

2. **Gl_api table fully populated (no NULL function pointers).** Every slot in `Gl_dll.def` is assigned in `Direct3d11::install`. Implementation slots route to `scaffold_fatal_stub` via `decltype + reinterpret_cast`; engine-queried no-op slots (`addDeviceLostCallback`, `removeDeviceLostCallback`, `addDeviceRestoredCallback`, `removeDeviceRestoredCallback`, `wasDeviceReset`, `requiresVertexAndPixelShaders`) use no-op lambdas with appropriate return values. This means the engine never crashes on a NULL pointer — it FATALs with a known message when it reaches an unimplemented slot.

3. **Engine-side surgical edits minimal and contained.** `Graphics.cpp` has exactly 2 surgical edits per CONTEXT D-02: TAG_DX11 declaration at ~line 65-66 (`TAG3(D,1,1)` — compiles cleanly, no fallback needed) and extended range-check at ~line 209-215 accepting `rasterMajor=11` with the corresponding `TAG_DX11=true` set. NO OTHER engine-side files modified. D-02 invariant intact.

4. **D3D11 end-to-end plumbing smoke PASSED (sub-step 3a).** With `[ClientGraphics] rasterMajor=11` set in `client_d.cfg` (NOT `client.cfg` — see lessons-learned below), the client launches, `Graphics::install` reaches `LoadLibrary("./gl11_d.dll")`, `GetProcAddress("GetApi")` succeeds, the plugin's install path runs, and FATAL fires at `Direct3d11.cpp:62 scaffold_fatal_stub` from inside `Graphics::install` (first stubbed slot at `Graphics.cpp:320`, further into install at `Graphics.cpp:554`). Crash dump persisted at `stage/SwgClient_d.exe-unknown.0-20260516220506.{txt,mdmp}` with the expected FATAL message: `Direct3d11 plugin: scaffold-only -- unimplemented Gl_api slot called (Plan 11-02 expected; Wave 3+ replaces this)`. This is the design contract for this plan.

5. **D3D9 sanity PASSED (sub-step 3b — D-05 protection holds).** With `rasterMajor=11` reverted in `client_d.cfg`, the client reaches char-select and world load cleanly. No SafeCast dialogs — today's `SwgClient_d.exe` relink picked up the ContrailData D-18 guard committed at `73e29eee7` per the [project_safecast_null_dynamic_cast] memory note. The D3D9 fallback rendering path is unaffected by the Phase 11 Wave 2 changes.

6. **D3D9 baseline screenshots captured (sub-step 3c).** Two reference PNGs committed under `docs/recon/11-d3d11-screenshots/`: Mos Eisley plaza outdoor (`d3d9-tatooine-outdoor.png`, 949,442 bytes, world coords `X=3467, Y=5, Z=-4850`) and Mos Eisley cantina interior (`d3d9-cantina-interior.png`, 749,857 bytes, world coords `X=3455, Y=5, Z=-4834`). The two anchors are ~17m apart on the ground plane. Plan 11-09 reproduces these under D3D11 for SPEC R6 side-by-side verdict.

7. **Bonus deliverable — vendored atlmfc include in SwgClient.vcxproj (`dbd7c62dc`).** After the Plan 11-01 post-build cp fix made auto-staging work, a second build-system trap surfaced: `SwgClient.rc` couldn't find `afxres.h` under CLI MSBuild because the VS Developer Command Prompt env bridging wasn't active. Vendoring the atlmfc include path into the vcxproj closes this trap for all future CLI rebuilds. Net add (not reverted) — benefits every future Phase 11 plan and the entire project going forward. Combined with Plan 11-01's `266e173b3` build-system fix, the manual-cp + Dev-Cmd-Prompt-required rebuild workflow is fully retired.

## Task Commits

| # | Commit | Task | Type | Net |
|---|---|---|---|---|
| 1 | `2c518e832` | Task 1 — Plugin scaffold source tree + Direct3d11.vcxproj + Direct3d11.vcxproj.filters + swg.sln integration + .gitignore + docs/recon/11-d3d11-screenshots/.gitkeep | spec | +~1700 lines across 14 new files + 2 modified |
| 2 | `db2116594` | Task 2 — Engine-side surgical edit: Graphics.cpp range check accepts rasterMajor=11; TAG_DX11 declaration added | spec | ~+15 lines / -5 lines in 1 engine file |
| 3 | `dbd7c62dc` | Rule-3 deviation (bonus deliverable) — vendored atlmfc include in SwgClient.vcxproj for CLI MSBuild .rc compile | build | ~+5 lines in 1 vcxproj |

**Plan close commit:** (this commit) — adds 11-02-SUMMARY.md + 2 PNG screenshots + comparison-notes.md + STATE.md + ROADMAP.md + REQUIREMENTS.md updates.

## Sub-step Verifications

### Sub-step 3a — D3D11 plumbing smoke (PASSED)

- `client_d.cfg [ClientGraphics] rasterMajor=11` set; `SwgClient_d.exe` launched.
- FATAL fired with the expected message: `Direct3d11 plugin: scaffold-only -- unimplemented Gl_api slot called (Plan 11-02 expected; Wave 3+ replaces this)`.
- Crash dump path: `D:/Code/swg-client-v2/stage/SwgClient_d.exe-unknown.0-20260516220506.txt` + matching `.mdmp` (gitignored under `stage/`).
- Call stack confirms plumbing reached the plugin's install path:
  - `ClientMain.cpp:312` → `SetupClientGraphics::install`
  - `SetupClientGraphics.cpp:92` → `Graphics::install`
  - `Graphics.cpp:320` → first `Gl_api` call inside install (the executor's prediction was correct — first STUB hit during `Graphics::install`)
  - `Graphics.cpp:554` (further into install)
  - `scaffold_fatal_stub` in `Direct3d11.cpp:62`
- This proves: `gl11_d.dll` loaded, `GetApi` resolved, `Direct3d11::install` ran, STUB hit as designed. End-to-end plumbing is correct.

### Sub-step 3b — D3D9 sanity (PASSED — D-05 protection)

- `client_d.cfg` reverted (rasterMajor key removed → defaults to D3D9).
- `SwgClient_d.exe` reached char-select + world load cleanly. No SafeCast dialogs (current `SwgClient_d.exe` relink picked up ContrailData D-18 guard at commit `73e29eee7`).

### Sub-step 3c — D3D9 baseline screenshots (CAPTURED)

| Anchor | File | Size | World coords (X, Y, Z) |
|--------|------|------|------------------------|
| Mos Eisley plaza outdoor | `docs/recon/11-d3d11-screenshots/d3d9-tatooine-outdoor.png` | 949,442 B | (3467, 5, -4850) |
| Mos Eisley cantina interior | `docs/recon/11-d3d11-screenshots/d3d9-cantina-interior.png` | 749,857 B | (3455, 5, -4834) |

Coords + reproduction protocol persisted in `docs/recon/11-d3d11-screenshots/comparison-notes.md` for Plan 11-09 use.

## Build Verifications

- `MSBuild ... /t:Direct3d11 /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` → EXIT=0; `gl11_d.dll` produced and auto-staged to `stage/` via the post-build cp fix from Plan 11-01 `266e173b3`.
- `MSBuild ... /t:Direct3d9 /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` → EXIT=0 (D-05 protection: `gl05_d.dll` still produces clean).
- `MSBuild ... /t:clientGraphics /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` → EXIT=0 (links the new TAG_DX11 symbol + extended range-check).
- `MSBuild ... /t:SwgClient_d /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v145` → EXIT=0 (full link clean; the atlmfc include vendoring in commit `dbd7c62dc` resolved the .rc compile under CLI MSBuild).
- `dumpbin /exports stage/gl11_d.dll | findstr GetApi` → matched line containing `GetApi` (export contract intact).

## Files Created/Modified

### Created — D3D11 plugin source tree

- `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj`
- `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj.filters`
- `src/engine/client/application/Direct3d11/src/shared/FirstDirect3d11.h`
- `src/engine/client/application/Direct3d11/src/win32/FirstDirect3d11.cpp`
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.h`
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp`
- `src/engine/client/application/Direct3d11/src/win32/ConfigDirect3d11.h`
- `src/engine/client/application/Direct3d11/src/win32/ConfigDirect3d11.cpp`
- Scaffold infrastructure mirrored from D3D9 plugin (not in the strict `files_modified` PLAN frontmatter list but consistent with the plan intent):
  - `src/engine/client/application/Direct3d11/src/shared/FirstDirect3d11.cpp`
  - `src/engine/client/application/Direct3d11/src/shared/MemoryManagerHook.cpp`
  - `src/engine/client/application/Direct3d11/src/shared/SetupDll.{h,cpp}`
  - `src/engine/client/application/Direct3d11/src/shared/WriteTga.{h,cpp}`

### Created — Reference / documentation

- `docs/recon/11-d3d11-screenshots/.gitkeep`
- `docs/recon/11-d3d11-screenshots/d3d9-tatooine-outdoor.png` (949,442 bytes)
- `docs/recon/11-d3d11-screenshots/d3d9-cantina-interior.png` (749,857 bytes)
- `docs/recon/11-d3d11-screenshots/comparison-notes.md`
- `.planning/phases/11-d3d11-renderer-plugin/11-02-SUMMARY.md` (this file)

### Modified

- `src/build/win32/swg.sln` — `Direct3d11.vcxproj` reference + GlobalSection entries (Debug|Win32, Release|Win32, Optimized|Win32).
- `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp` — TAG_DX11 declaration + extended range-check (the ONLY engine-side edit in this plan).
- `.gitignore` — already covers `stage/` runtime output.
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` — vendored atlmfc include for .rc compile under CLI MSBuild (bonus deliverable per Rule 3).

## Decisions Made

1. **TAG_DX11 = TAG3(D,1,1).** Compiled cleanly on first attempt; no fallback to `TAG3(D,X,B)` needed. Decision is cosmetic; `Tag.h` macro accepts digit characters in position 2.
2. **FATAL-stub population via decltype + reinterpret_cast.** Per-slot `ms_glApi.<name> = reinterpret_cast<decltype(ms_glApi.<name>)>(scaffold_fatal_stub);` keeps the compiler honest about pointer type and produces a single shared FATAL message at `Direct3d11.cpp:62` regardless of which slot the engine reaches first. Engine-queried no-op slots use lambdas with the correct return types per RESEARCH §Code Examples lines 517-522.
3. **Vendored atlmfc include adopted as Phase 11 bonus deliverable.** Surfaced during the relink that produced the post-Plan-11-01 `SwgClient_d.exe`. CLI MSBuild couldn't find `afxres.h` (it lives under the atlmfc include set normally bridged in by the VS Developer Command Prompt env). Vendoring the path into `SwgClient.vcxproj` closes the trap permanently for CLI rebuilds. Net add, not reverted — benefits all future Phase 11 plans and the entire project going forward. Committed under `build(11-02):` to flag the project-wide scope.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking → Bonus deliverable] SwgClient.rc could not find afxres.h under CLI MSBuild**
- **Found during:** Task 2 SwgClient_d.exe relink after the Graphics.cpp surgical edit
- **Issue:** `SwgClient.rc` compile failed under CLI MSBuild because the atlmfc include path was normally bridged in by the VS Developer Command Prompt environment, not declared in the vcxproj. This blocked relinking `SwgClient_d.exe` from the CLI workflow that Plan 11-01's `266e173b3` post-build cp fix had just unblocked.
- **Fix:** Vendored the atlmfc include path into `SwgClient.vcxproj`'s `AdditionalIncludeDirectories` so the .rc compile resolves afxres.h without env bridging.
- **Files modified:** `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` (~+5 lines)
- **Verification:** `MSBuild ... /t:SwgClient_d ...` EXIT=0 from CLI without VS Developer Command Prompt active
- **Committed in:** `dbd7c62dc` (marked `build(11-02):` — project-wide benefit beyond Plan 11-02 scope)

### Lessons learned (documented for future executors)

**1. Debug exe reads `client_d.cfg`, NOT `client.cfg`.** The Plan 11-02 PLAN.md and the initial smoke-mode workflow described editing `client.cfg`. The first sub-step 3a launch attempt edited `client.cfg` and the game loaded on D3D9 because Debug-mode reads `client_d.cfg`. Future smoke-mode cfg edits MUST target the debug-variant cfg. Worth saving as a feedback memory.

**2. Modal FATAL dialog occluded by windowed game window.** Kenny initially saw "cursor confinement" with no visible dialog when the D3D11 FATAL fired. The dialog was rendered but hidden behind the game window in windowed mode. The crash dump was still written at the expected `stage/` path. Reading the crash dump path is the reliable verification mechanism, not waiting for a visible FATAL popup.

**3. `rasterMajor` is NOT persisted in `local_machine_options.iff`.** Only `[ClientGraphics] brightness/contrast/gamma/screenShotFormat/...` are persisted there. `rasterMajor` is purely a cfg-file value, so cfg-toggle smoke workflows behave predictably across relaunches.

---

**Total deviations:** 1 auto-fixed (Rule 3 blocking → bonus deliverable) + 3 lessons-learned documented for future executors.
**Impact on plan:** Deviation was necessary to complete the relink; bonus scope (entire project benefits) justifies the build(11-02): commit type.

## Issues Encountered

- **First D3D11 smoke attempt loaded D3D9.** The first sub-step 3a attempt edited `client.cfg` (release-mode cfg) instead of `client_d.cfg` (debug-mode cfg). Game launched on D3D9 because the debug exe ignored the release-mode cfg. Corrected by editing `client_d.cfg`; second attempt produced the expected FATAL. Lessons-learned section above documents the convention.
- **Modal dialog occluded by windowed game window.** First Kenny observation of the D3D11 smoke run was "cursor confinement, no dialog" — interpreted initially as a different failure mode. Actual outcome: dialog present but hidden behind game window in windowed mode; crash dump still written. Reading the dump file confirmed the expected FATAL.

## User Setup Required

None — Plan 11-02 is plugin scaffold + plumbing proof; no external services or new manual steps required. The two D3D9 baseline PNGs are checked-in artifacts; Plan 11-09 reproduces them under D3D11 when Wave 9 fires.

## Next Phase Readiness

- **Plan 11-03 (Wave 3 — D3D11 device + DXGI swap chain + clear-to-color MVP)** is UNBLOCKED. The scaffold is in place, the engine accepts `rasterMajor=11`, and the FATAL stubs are ready to be replaced one slot at a time. First slot to replace: whichever `Gl_api` member is called at `Graphics.cpp:320` during `Graphics::install` (the first slot reached, observed at `Direct3d11.cpp:62 scaffold_fatal_stub`).
- **Plan 11-05 (Wave 5 — Shader layer)** scope confirmed reduced per Plan 11-01 D-04a DESCOPE verdict: no `Direct3d11_FfpGenerator.{h,cpp}` to author.
- **Plan 11-09 (Wave 9 — visual parity)** input artifacts in HEAD: the two D3D9 baseline PNGs + comparison-notes.md at the expected paths under `docs/recon/11-d3d11-screenshots/`.
- **D-05 maintained.** D3D9 plugin (`gl05_d.dll`) still builds and loads cleanly; sub-step 3b confirmed char-select + world load under D3D9.
- **Build-system traps fully retired.** The Plan 11-01 `266e173b3` post-build cp fix + the Plan 11-02 `dbd7c62dc` atlmfc include vendoring together close the CLI MSBuild rebuild + auto-stage workflow. Future Phase 11 plans no longer need manual `cp` or VS Developer Command Prompt env bridging.

### Carry-forward observations (not blockers)

- Pre-existing Koogie `Direct3d9.vcxproj` MSB8012 TargetName/OutputFile mismatch warnings remain (out of scope per Wave 0 / Plan 11-01 precedent).
- Pre-existing C4456 declaration-shadowing warnings in `Direct3d9.cpp:2519` and `Direct3d9_VertexBufferDescriptorMap.cpp:140` remain (out of scope).
- The `Direct3d11.vcxproj` GUID assigned during scaffold authoring is committed in `2c518e832`; future plans reference it via the swg.sln integration block, not by literal value.

## TDD Gate Compliance

This plan is `type: execute` (not `type: tdd`), so the RED/GREEN/REFACTOR gate sequence does not apply. No `test(...)` commits expected. The plumbing smoke (sub-step 3a) and D-05 sanity (sub-step 3b) are the equivalent verification gates.

## Requirements Traceability

- **D3D11-01 (plugin scaffold + Gl_api table):** **PARTIAL** — scaffold + plumbing-to-FATAL proves the plugin loading path end-to-end (LoadLibrary + GetApi + Direct3d11::install + first stubbed slot FATAL all verified by sub-step 3a). Real D3D11 device + swap chain + first non-stub slot is Plan 11-03 (Wave 3) territory. Commit chain: `2c518e832 / db2116594 / dbd7c62dc / <plan-close-hash>`.
- **D3D11-02 (resource management replaces D3DPOOL_MANAGED + lost-device removed):** **PARTIAL** — scaffold structure declares no lost-device primitives (D-13 invariant: zero `D3DPOOL_MANAGED|OnLostDevice|OnResetDevice` references in `src/engine/client/application/Direct3d11/`); engine-queried lost-device callback slots are no-op lambdas in the plugin. Real resource layer is Plan 11-04 (Wave 4). Commit: `2c518e832`.

---

*Phase: 11-d3d11-renderer-plugin*
*Plan: 02 — D3D11 plugin scaffold + plumbing FATAL verified + D3D9 baseline screenshots*
*Completed: 2026-05-16*

## Self-Check: PASSED

- `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj` exists — VERIFIED (committed in `2c518e832`)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp` exists — VERIFIED (committed in `2c518e832`)
- `src/engine/client/library/clientGraphics/src/win32/Graphics.cpp` carries TAG_DX11 + extended range-check — VERIFIED (committed in `db2116594`)
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` carries vendored atlmfc include — VERIFIED (committed in `dbd7c62dc`)
- `docs/recon/11-d3d11-screenshots/d3d9-tatooine-outdoor.png` (949,442 bytes) exists at expected path — VERIFIED
- `docs/recon/11-d3d11-screenshots/d3d9-cantina-interior.png` (749,857 bytes) exists at expected path — VERIFIED
- `docs/recon/11-d3d11-screenshots/comparison-notes.md` carries coords + Plan 11-09 reproduction protocol — VERIFIED
- Task commits present in `git log`: `2c518e832`, `db2116594`, `dbd7c62dc` — VERIFIED
- Sub-step 3a crash dump at `stage/SwgClient_d.exe-unknown.0-20260516220506.{txt,mdmp}` (gitignored under stage/) — RETAINED locally as evidence
- D-05 protection (D3D9 plugin still builds + loads to char-select + world) — VERIFIED via sub-step 3b
