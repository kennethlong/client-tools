# Handoff — Phase 24 COMPLETE (DPVS Config-Gate + Machine Portability)

**Date:** 2026-06-12
**Branch:** `koogie-msvc-cpp20-base` (HEAD `87e675ef3`)
**Status:** Phase 24 fully verified + closed. Clean boundary — nothing mid-flight.

## What shipped (3/3 plans, 2 waves, all merged)

- **24-01 (HARD-01):** `[ClientGraphics/Dpvs] occlusionMode = auto|on|off` knob — `DpvsOcclusionMode` enum (`DOM_off=0` zero-init default) + `getDpvsOcclusionMode()` in `ConfigClientGraphics`; config-gated occlusion bit in `RenderWorld::drawScene` (DOM_auto = null-guarded `ms_cameraCell && isWorldCell()`; F11 force-disable preserved, wins over any mode). Plus D-07 default flips: multistream-VB default + D3D11 Bloom no-op (`Direct3d11.cpp`). **Note:** Debug builds with NO `occlusionMode` key now default to **OFF** (was on-by-default); Phase-23 posture returns via `occlusionMode=auto` (shipped in the Debug template) or F11.
- **24-02 (PORT-01):** Machine-portable Miles redist — vendored proven byte-set + 2 `.m3d` providers at `src/external/3rd/library/miles-7.2e/redist`; postbuild repointed from machine-specific path to repo-relative redist with codec-repair guard; one-shot codec-absence WARNING + flood-suppression flag (`s_milesCodecRedistAvailable`) in `Audio.cpp`.
- **24-03 (PORT-02):** Tracked `tools/setup/client.cfg.template` + `tools/setup/setup-client.ps1` generator; Phase-11+ test-key cleanup (swg_dev_bundle, disableMultiStreamVertexBuffers, [ClientGame/Bloom] enable=0, disableG15Lcd, voiceChatEnabled, reportFrameStats all gone); `.gitignore` + `docs/build.md` updated.

## Verification

- **Build:** clientGraphics + Direct3d11 + SwgClient (Debug) — **0 unresolved externals** (grep-confirmed past /FORCE), `SwgClient_d.exe` + `gl11_d.dll` postbuild-staged. Rebuilt clean after code-review fixes.
- **Verifier:** 11/11 code must-haves passed (HARD-01, PORT-01, PORT-02 all accounted for).
- **User (Kenny) approved** all 4 runtime checks 2026-06-12: in-game DPVS occlusion (auto/on/off/F11), dual-renderer boot (rasterMajor=11 & 5), Miles audio + rename-warning, fresh-clone setup-client.ps1. `24-VERIFICATION.md` + `24-HUMAN-UAT.md` both status: passed.
- **Code review (`24-REVIEW.md`):** 0 critical / 4 warn / 5 info. **WR-01** (Miles postbuild guard now also checks `mssmp3.asi` mp3 decoder) + **WR-04** (dead clamp removed in `screenShot_impl`) FIXED. **WR-02** (Optimized config no postbuild — known-broken link target) + **WR-03** (setup script defers staging to postbuild — deliberate D-11) recorded WONTFIX.

## State after completion

- `phase.complete 24` ran: ROADMAP/STATE/REQUIREMENTS updated, STATE advanced to **Phase 25**.
- PROJECT.md evolved; pending todo `2026-06-12-config-gate-dpvs-occlusion-per-phase-23-verdict.md` moved to `todos/completed/`.
- **35 commits unpushed** on `koogie-msvc-cpp20-base` vs `origin/koogie-msvc-cpp20-base` (Phase 24 = `4cf9926b5`..`87e675ef3`, the rest are prior planning commits). Kenny pushes on his own call — NOT pushed.

## Next

**Phase 25: Cantina Corner-Snap Fix** — mechanism already found (same-frame portal ping-pong, re-entrant `positionChanged`; CORNERSNAP `_DEBUG` instrumentation committed `a9b419daf`). No CONTEXT.md yet.
- Start: `/gsd-discuss-phase 25` (recommended) → `/gsd-plan-phase 25` → `/gsd-execute-phase 25`.
- Fix direction noted in memory: re-entrancy guard on `positionChanged`, deferred. See `project_cantina_corner_snap_engine_quirk` memory.

## Unrelated note (resolved this session)

The amber **"Debug"** badge in the Claude Code footer = the **Ctrl+O verbose/transcript toggle** (`app:toggleTranscript`), NOT the `--debug` flag, NOT skip-permissions, NOT the custom statusline. Press **Ctrl+O** to clear it; unbind in `~/.claude/keybindings.json` (`"ctrl+o": null`) to prevent accidental re-toggle.
