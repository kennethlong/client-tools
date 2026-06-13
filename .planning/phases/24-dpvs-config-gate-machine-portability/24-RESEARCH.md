# Phase 24: DPVS Config-Gate + Machine Portability - Research

**Researched:** 2026-06-12
**Domain:** Engine config plumbing (clientGraphics/clientAudio) + repo machine-portability (cfg templating, vendored redist, gitignore/postbuild)
**Confidence:** HIGH (all claims verified against this tree's source; no external library version research needed — this is an in-repo engineering phase)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**DPVS knob design:**
- **D-01:** Add `dpvsOcclusionMode = auto | on | off` per the folded todo, replacing the hardcoded Option α `#else` branch in `RenderWorld.cpp` (~line 914). `auto` = OR `DPVS::Camera::OCCLUSION_CULLING` into culling params only when the camera is NOT inside a POB cell (outdoor on / indoor off). `on`/`off` = unconditional overrides.
- **D-02:** **Default is `off`** — NOT `auto`. Phase 23 deltas were Debug-build; shipped behavior stays Option α (occlusion removed) until a Release measurement justifies flipping.
- **D-03:** Release A/B measurement **deferred to Phase 26**. Do NOT add a Release-safe CSV variant this phase.
- **D-04:** The `_DEBUG`-only F11 force-disable flag (`ms_forceDisableOcclusionCulling`) keeps working on top — force-disable wins over config.

**client_d.cfg cleanup (PORT-02):**
- **D-05:** Full per-key audit with three verdicts: keep / remove / verify-then-remove. Risky removals (e.g. `runtimeDisableAsynchronousLoader=true`) get an explicit boot + play test. Obviously-dead keys (vivox `voiceChatEnabled`, `disableG15Lcd`) just go. Phase ends with the dual-renderer boot gate.
- **D-06:** Canonical committed `rasterMajor=11`. D3D9 stays one config flip away; the boot gate tests both.
- **D-07:** Small engine-side default flips ALLOWED so permanently-required override keys disappear (e.g. make multi-stream skinned VBs the engine default so `disableMultiStreamVertexBuffers=false` goes; wire/no-op the D3D11 Bloom slot so `[ClientGame/Bloom] enable=0` goes). Each flip small, reversible, boot-verified.

**Path portability (PORT-01):**
- **D-08:** A **PowerShell setup script generates the cfgs** from a tracked template (prompts for the machine's TRE asset root, e.g. `D:/Code/SWGSource Client v3.0/`). Auto-substitutes repo-relative paths (e.g. `stage/override`) from the repo root it runs in. Successor to the dead CMake `configure_file` story.
- **D-09:** **Template tracked, generated cfgs untracked.** `stage/client_d.cfg` + `stage/client.cfg` come off the tracked tree; template + script are source of truth. Lets `rasterMajor` be flipped freely per-machine.
- **D-10:** PORT-01 verification is a **real fresh-clone test**: clone to a new dir on this machine, run setup + build, boot to character select.

**Miles redist (PORT-01):**
- **D-11:** **Vendored redist + postbuild copy.** Add the 2 missing 3D providers (`Msseax.m3d`, `msssoft.m3d`) to `src/external/3rd/library/miles-7.2e/redist/`, then postbuild/setup copies redist → `stage/miles`. `AudioCapture.flt` likely excluded (researcher verifies); zero-byte `index.html` excluded.
- **D-12:** Absence detection lives in **both** places: the setup script validates expected files exist, AND the engine emits ONE clear startup warning (not a per-sample flood) if Miles codec providers fail to load.

### Claude's Discretion
- Config key naming/section placement (`[ClientGraphics/Dpvs] occlusionMode` vs `[ClientGraphics] dpvsOcclusionMode`).
- Auto-mode detection mechanics: per-frame evaluation vs cell-notification hook (the existing `ms_lastCullingParameters` change-detection makes per-frame cheap).
- Setup script UX (prompt vs parameter, validation messages), template format, exact gitignore arrangement.
- Per-key verdicts beyond those explicitly decided — anything uncertain stays as a documented cfg key, not a risky removal.

### Deferred Ideas (OUT OF SCOPE)
- **Phase 26:** Release-build DPVS A/B measurement using the new knob (+ the D-15 harness before it's stripped), then decide whether to flip the default from `off` to `auto`.
- Acting on DPVS beyond the config-gate (scene-aware culling redesign).
- Corner-snap fix (Phase 25), instrumentation removal (Phase 26), D3DCompile port (Phase 27), TRE tool (Phases 28–30).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| HARD-01 | DPVS occlusion config-gated per Phase 23 verdict — `auto` enables the bit only outside POB cells, with explicit override | Verified: `RenderWorld.cpp:908-916` is the exact gate site; `CellProperty::isWorldCell()` (CellProperty.cpp:555) is the POB-detection primitive; `ConfigFile::getKeyString` read pattern confirmed at `DpvsProfileInstrumentation.cpp:85,90`; F11 flag at `RenderWorld.cpp:83,909,1200` |
| PORT-01 | Fresh clone + build → bootable, no machine-specific absolute paths; `stage/override` + `stage/miles` handled | Verified: all absolute paths inventoried in both cfgs; existing postbuild Miles copy (`SwgClient.vcxproj:130`) sources a machine-specific path — must be repointed to vendored redist; `stage/override` is tracked, both cfgs already untracked via `stage/*` gitignore |
| PORT-02 | `client_d.cfg` cleaned of Phase-11+ test settings; boots clean under `rasterMajor=5` AND `=11` | Verified: full per-key audit table below; risky keys (`runtimeDisableAsynchronousLoader`, multi-stream, Bloom) identified with engine-side dependencies |
</phase_requirements>

## Summary

This is an **in-repo engineering phase**, not a library-research phase — there is no external SDK to version-check. Every fact below is verified directly against the `koogie-msvc-cpp20-base` working tree. Three loosely-coupled workstreams: (1) a small DPVS config knob in `RenderWorld.cpp`/`ConfigClientGraphics`, (2) a PowerShell cfg-templating + Miles-redist-copy portability mechanism, and (3) a `client_d.cfg` per-key cleanup. The phase-close invariant is the dual-renderer boot gate (character select under `rasterMajor=5` and `=11`).

The DPVS knob is genuinely small: the gate site (`RenderWorld.cpp:908-916`) already has both branches written, `CellProperty::isWorldCell()` is the exact POB predicate, the `ms_lastCullingParameters` change-detection already makes per-frame toggling cheap, and the `ConfigFile::getKeyString("ClientGraphics/Dpvs", ...)` read pattern is already used three lines away in `DpvsProfileInstrumentation.cpp`. The only subtlety is preserving the `_DEBUG` F11 override semantics on top of the new Release-visible config path.

Portability has one surprise that contradicts a CONTEXT.md premise (flagged below as ASSUMED-correction A1): the vendored `redist/` files are **not byte-identical** to the proven-working `stage/miles/` set — they are a different Miles point-release (notably `mssogg.asi` is 41 KB in redist vs 99 KB in stage). The currently-shipping, runtime-verified audio uses the Oct-2017 `stage/miles` bytes. The planner must decide the source-of-truth set, not assume redist is interchangeable.

**Primary recommendation:** Implement the DPVS knob as a `ConfigClientGraphics::getDpvsOcclusionMode()` enum getter consumed in `RenderWorld::drawScene`; repoint the existing `SwgClient.vcxproj` postbuild Miles copy from the machine-specific `D:\Code\SWGSource Client v3.0\miles` to the vendored `src/external/3rd/library/miles-7.2e/redist`; resolve the redist-vs-stage byte discrepancy by copying the proven `stage/miles` bytes into `redist/` (plus the 2 `.m3d` files) so the vendored set is the verified-working one; generate both cfgs from a tracked PowerShell template living OUTSIDE `stage/*`.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| DPVS occlusion bit selection | clientGraphics engine (`RenderWorld`) | clientGraphics config (`ConfigClientGraphics`) | Culling is a per-frame render decision; config read is install-time cache |
| POB-cell detection | sharedObject (`CellProperty::isWorldCell`) | clientGraphics (`ms_cameraCell`) | Cell topology is a sharedObject concept; RenderWorld already tracks the camera's cell |
| Miles codec absence detection | clientAudio engine (`Audio::install`) | PowerShell setup script | Runtime probe belongs at audio init; setup-time file check is a second, earlier gate |
| Path templating / cfg generation | PowerShell setup script (build tooling) | tracked template file | Build-system-independent; replaces dead CMake `configure_file` |
| Redist staging | MSBuild postbuild (`SwgClient.vcxproj`) | setup script | Postbuild is the established stage-population pattern (exe/pdb already copied there) |
| cfg source-of-truth tracking | git (`.gitignore`) | — | Generated artifacts untracked, template tracked |

## Standard Stack

This phase introduces **no new libraries**. It uses only in-tree engine APIs and platform tooling already in use.

### Core (in-tree APIs)
| API | Location | Purpose | Why Standard |
|-----|----------|---------|--------------|
| `ConfigFile::getKeyString/getKeyBool/getKeyInt` | `sharedFoundation/ConfigFile.h` | Read cfg keys | The engine's only config-read path; used everywhere |
| `ConfigClientGraphics` KEY_* macros | `clientGraphics/src/shared/ConfigClientGraphics.cpp:70-76` | Cache `[ClientGraphics]` keys at install | Established getter pattern (`getPsrcCensus` is the closest precedent) |
| `CellProperty::isWorldCell()` | `sharedObject/.../CellProperty.cpp:555` | POB-vs-outdoor predicate | Returns `this == ms_worldCellProperty`; the canonical world-cell test |
| `DPVS::Camera::OCCLUSION_CULLING` / `VIEWFRUSTUM_CULLING` | `dpvsCamera.hpp` | Culling-parameter bits | The bits the knob ORs together |
| `DebugFlags::registerFlag` | `sharedDebug/DebugFlags.h` | `_DEBUG` F11 flag registration | Already wires `ms_forceDisableOcclusionCulling` |

### Supporting (platform tooling)
| Tool | Purpose | When to Use |
|------|---------|-------------|
| PowerShell 5.1+ | Setup script (D-08) — cfg generation, file validation, path substitution | The mandated scripting language (Windows-only repo) |
| MSBuild postbuild `<Command>` (cmd.exe `copy`/`xcopy`/`if exist`) | Stage population | Established pattern in `SwgClient.vcxproj:123-131` |
| `$env:MSBUILD` | Build invocation | msbuild not on PATH (see `.claude/settings.json`) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `ConfigClientGraphics` getter | Direct `ConfigFile::getKeyString` in `RenderWorld.cpp` (like `DpvsProfileInstrumentation.cpp:85`) | Direct read is fewer files but parses the string every frame unless cached; the getter caches once at install and matches the existing `[ClientGraphics]` cache convention. Recommend the getter, parsed once to an enum. |
| PowerShell setup script | Resurrect CMake `configure_file` | The tree abandoned CMake at Phase 9 (Option D, MSBuild). The cfg header's "re-run cmake" claim is stale/false. D-08 locks PowerShell. |
| Postbuild copy of redist | Setup-script-only copy | Postbuild auto-runs on every build (no manual step); setup script catches the very-first pre-build clone. D-11/D-12 want both — postbuild for ongoing, script for first-clone validation. |

**Installation:** None. No `npm`/`pip`/`vcpkg` packages.

## Architecture Patterns

### System Architecture Diagram (data flow)

```
                         ┌─────────────────────────────────────────────┐
  client_d.cfg /         │  cfg TEMPLATE (tracked, outside stage/*)      │
  client.cfg  ◄──────────┤  + PowerShell setup script (D-08)             │
  (generated,            │   • prompts TRE asset root                    │
   untracked)            │   • substitutes repo-relative stage/override  │
                         │   • validates stage/miles files exist (D-12a) │
                         └─────────────────────────────────────────────┘
        │
        │ read at boot
        ▼
  ┌──────────────────────┐     install      ┌──────────────────────────────┐
  │ ConfigClientGraphics │ ◄─────────────── │ [ClientGraphics/Dpvs]        │
  │  ::install           │   getKeyString   │   occlusionMode = off|auto|on│
  │  caches enum         │                  └──────────────────────────────┘
  └──────────┬───────────┘
             │ getDpvsOcclusionMode()
             ▼
  ┌─────────────────────────────────────────────────────────────────┐
  │ RenderWorld::drawScene  (per frame, ~line 908)                    │
  │                                                                   │
  │   mode == off  ──────────────► cullingParams = VIEWFRUSTUM only   │
  │   mode == on   ──────────────► cullingParams |= OCCLUSION         │
  │   mode == auto ─► isWorldCell? ─yes─► |= OCCLUSION (outdoor)       │
  │                   (ms_cameraCell)  └no► VIEWFRUSTUM only (POB)     │
  │                                                                   │
  │   #ifdef _DEBUG: ms_forceDisableOcclusionCulling (F11) clears     │
  │                  OCCLUSION regardless (D-04, force-disable wins)   │
  │                                                                   │
  │   if (cullingParams != ms_lastCullingParameters)                  │
  │        ms_dpvsCamera->setParameters(...)   ◄── change-detect 920  │
  └─────────────────────────────────────────────────────────────────┘

  ┌─────────────────────────────────────────────────────────────────┐
  │ Audio::install (clientAudio, win32)                              │
  │   AIL_set_redist_directory("miles") ──► stage/miles  (line 1276) │
  │   AIL_startup()                                       (line 1280) │
  │   AIL_open_digital_driver(...)                        (line 1290) │
  │      └─ fails → ONE warning + audio off (1292-1307) ◄ already 1×  │
  │   D-12 NEW: probe codec providers (.asi) present →               │
  │      emit ONE clear startup warning if absent (not per-sample)   │
  │   ... per-sample AIL_set_named_sample_file (2823/4436) fails      │
  │       silently per-sample = the 141k flood IF no D-12 guard       │
  └─────────────────────────────────────────────────────────────────┘

  ┌─────────────────────────────────────────────────────────────────┐
  │ SwgClient.vcxproj PostBuild (Debug + Release blocks)             │
  │   copy SwgClient_{d,r}.exe + .pdb → stage/         (123-131)     │
  │   xcopy MILES redist → stage/miles                              │
  │     CURRENT: from D:\Code\SWGSource Client v3.0\miles (MACHINE!) │
  │     PORT-01: repoint to src/external/.../miles-7.2e/redist/     │
  └─────────────────────────────────────────────────────────────────┘
```

### Recommended Project Structure
```
src/engine/client/library/clientGraphics/src/shared/
├── RenderWorld.cpp            # gate site ~908-916; auto-mode logic + F11 interplay
├── ConfigClientGraphics.cpp   # new getDpvsOcclusionMode() getter + enum cache
└── ConfigClientGraphics.h     # new getter decl + DpvsOcclusionMode enum

src/external/3rd/library/miles-7.2e/redist/   # add Msseax.m3d, msssoft.m3d; reconcile bytes (A1)

src/game/client/application/SwgClient/build/win32/
└── SwgClient.vcxproj          # repoint postbuild Miles source (Debug+Release blocks)

<repo-root>/tools/  (or scripts/)             # NEW — outside stage/*
├── client.cfg.template        # tracked source-of-truth (D-09)
├── client_d.cfg.template      # or one template with a rasterMajor parameter
└── setup-client.ps1           # D-08 generator + D-12 file validation

.gitignore                     # add !<template-dir>; cfgs already untracked via stage/*
```

### Pattern 1: Config enum getter (cached at install)
**What:** Read the tri-state mode once at install, cache as an enum, expose via getter.
**When to use:** The DPVS knob (D-01). Matches `ConfigClientGraphics`'s existing one-read-at-install convention.
**Example:**
```cpp
// ConfigClientGraphics.cpp — parse once at install (pattern mirrors getPsrcCensus :128)
// enum DpvsOcclusionMode { DOM_off, DOM_on, DOM_auto };
char const *mode = ConfigFile::getKeyString("ClientGraphics/Dpvs", "occlusionMode", "off"); // D-02 default off
ms_dpvsOcclusionMode = (_stricmp(mode,"auto")==0) ? DOM_auto
                     : (_stricmp(mode,"on")==0)   ? DOM_on
                     : DOM_off;
```
```cpp
// RenderWorld.cpp drawScene (~908) — replaces the hardcoded #else (D-01).
// Source: verified gate site RenderWorld.cpp:908-916 + isWorldCell CellProperty.cpp:555
uint occlusionBit = 0;
switch (ConfigClientGraphics::getDpvsOcclusionMode())
{
    case DOM_on:   occlusionBit = DPVS::Camera::OCCLUSION_CULLING; break;
    case DOM_auto: occlusionBit = ms_cameraCell->isWorldCell() ? DPVS::Camera::OCCLUSION_CULLING : 0; break; // outdoor on / POB off
    case DOM_off:  default: occlusionBit = 0; break;
}
#ifdef _DEBUG
if (ms_forceDisableOcclusionCulling) occlusionBit = 0;   // D-04: force-disable wins
#endif
uint const cullingParameters = occlusionBit | (/* _DEBUG */ ms_disableViewFrustumCulling ? 0 : DPVS::Camera::VIEWFRUSTUM_CULLING);
// ... existing change-detect at 920 picks up the new value cheaply
```
**Note:** `ms_cameraCell` is assigned at `RenderWorld.cpp:873` (`camera.getParentCell()`) before the gate block, so it is valid where the knob reads it. In the `_DEBUG` `ms_lockViewFrustum` path (line 870) `ms_cameraCell` is NOT reassigned that frame but retains its last value — acceptable for a per-frame culling decision.

### Pattern 2: Postbuild stage-population (additive)
**What:** cmd.exe `if not exist ... if exist ... xcopy` guard in the vcxproj postbuild.
**When to use:** Repointing the Miles copy source (D-11). The block already exists at `SwgClient.vcxproj:130` (Debug) and `:233` (Release) — change only the SOURCE path; do NOT restructure Koogie's copy logic.
**Example (the change, both blocks):**
```bat
rem CURRENT (machine-specific — the PORT-01 violation):
if not exist "..\..\..\..\..\..\..\stage\miles\mssmp3.asi" if exist "D:\Code\SWGSource Client v3.0\miles\mssmp3.asi" xcopy /E /I /Y "D:\Code\SWGSource Client v3.0\miles" "..\..\..\..\..\..\..\stage\miles\"
rem PORT-01 (repo-relative vendored redist — $(ProjectDir)-relative or solution-relative):
if not exist "..\..\..\..\..\..\..\stage\miles\mssmp3.asi" xcopy /E /I /Y "..\..\..\..\..\..\..\src\external\3rd\library\miles-7.2e\redist" "..\..\..\..\..\..\..\stage\miles\"
```

### Anti-Patterns to Avoid
- **Modifying Koogie's postbuild copy LOGIC** (only repoint the source path — see MEMORY "don't modify Koogie's source-level changes"). The exe/pdb copy and the `if not exist` guard structure stay.
- **Hardcoding any new absolute path** in a cfg or vcxproj — that's the exact PORT-01 violation being fixed.
- **Coupling the DPVS knob to the D-15 throwaway instrumentation** (the todo and 23-verdict both warn: the Phase 23 harness is removed in Phase 26; the knob must stand alone).
- **Parsing `occlusionMode` every frame** — cache the enum at install.
- **Committing `git rm --cached stage/client_d.cfg`** as a no-op — both cfgs are ALREADY untracked (caught by `stage/*`); the real work is creating the tracked template OUTSIDE `stage/*` and a gitignore negation for it.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| POB-cell detection | A new "am I in a building" predicate or `ms_dpvsCameraCell` comparison | `ms_cameraCell->isWorldCell()` | Canonical, one-liner, already correct (`this == ms_worldCellProperty`) |
| Reading the mode key | A new ini parser | `ConfigFile::getKeyString` | The engine's universal config path |
| Per-frame param re-upload | A dirty flag | Existing `ms_lastCullingParameters` change-detect (920-930) | Already debounces `setParameters` |
| Staging the redist | A custom installer | The existing vcxproj postbuild block | Auto-runs every build; pattern proven |
| Miles codec presence | Parsing Miles internals | A startup file-existence check on the known `.asi`/`.m3d` set + the existing `AIL_open_digital_driver` failure path | Cheap, deterministic, no Miles API archaeology |

**Key insight:** Almost everything this phase needs already exists in the tree — the gate site has both branches written, the POB predicate exists, the change-detect exists, the postbuild Miles copy exists (just mis-sourced). This is wiring and cleanup, not construction.

## Runtime State Inventory

> This phase touches config files and a build-staging path — it is partly a portability/refactor phase. Inventory below.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — no datastore keys renamed. The cfgs reference TRE asset paths, not stored IDs. | None |
| Live service config | None — no n8n/Datadog/external service. LoginServer is in-cfg (`192.168.1.200:44453`), generated by the template. | Template carries it (or prompts) |
| OS-registered state | `[SharedFoundation] ProductRegistryPath=Software/whitengold/Default` — a Windows registry path the client writes. Unchanged by this phase. | None (keep key) |
| Secrets/env vars | None. `$env:MSBUILD` is a build convenience, not a cfg secret. `[Station]` SSO key intentionally absent (keep absent). | None |
| Build artifacts | `stage/miles/` (Oct-2017, runtime-verified set), `stage/client_d.cfg`/`client.cfg` (machine-specific, untracked), `stage/override/` (TRACKED source-of-truth), redist/ (newer Miles vintage — see A1). The 2 `.m3d` files are NOT yet in redist. | Add 2 `.m3d` to redist; reconcile redist vs stage bytes (A1); generate cfgs from template |

**Machine-specific absolute paths found (PORT-01 inventory) — both cfgs:**

| Path | client_d.cfg | client.cfg | Disposition |
|------|--------------|------------|-------------|
| `TOCTreePath="D:/Code/SWGSource Client v3.0/"` | line 22 | line 18 | Template parameter (prompt) |
| `searchTOC_00_0..03_3` (sku0–3 .toc) | 23–26 | 19–22 | Template parameter (same root prefix) |
| `searchTree_00_7/00_8` (snow disable, swgsource_3.0) | 27–28 | 23–24 | Template parameter (same root prefix) |
| `searchPath_00_11/00_10 = D:/Code/swg-client-v2/stage/override` | 31 | 27 | Auto-substitute from repo root (D-08); note index differs (12-vs-11 in client_d.cfg, 10 in client.cfg) |
| `searchPath_00_12=D:/swg_dev_bundle` | 29 | — (absent) | **Verify-then-remove** — see PORT-02 audit |
| Setup script source for Miles | (postbuild) | (postbuild) | Repoint to vendored redist |

## PORT-02 — Full per-key audit (client_d.cfg)

Verdicts: **K**=keep, **R**=remove, **V**=verify-then-remove (boot+play test per D-05).

| Section / Key | Line | Verdict | Rationale (verified) |
|---------------|------|---------|----------------------|
| CMake-generated header ("re-run cmake…") | 1–3 | **R** | Stale/false since Phase 9 MSBuild adoption (D-08 spec). Replace with setup-script provenance header. |
| Phase-6 decision annotations (D-01..D-09) | 4–12 | **K** (port forward) | Still-true ones carry into the template (D-08 specifics). |
| `[SharedFile] runtimeDisableAsynchronousLoader=true` | 15–18 | **V** | Phase 19 DIAGNOSTIC. Comment says "REMOVE after test". Removing re-enables the background loader thread (never re-tested since Phase 19 corruption was root-caused elsewhere — see MEMORY: black-walls/cape/collide fixes were the real causes). **Boot + play test both renderers** before removing. |
| `maxSearchPriority=12` | 19/15 | **K** | Functional — bounds the searchPath/TOC priority range. Must match the highest `_NN` index used. |
| `TOCTreePath` + `searchTOC_*` + `searchTree_*` | 22–28 | **K** (templatize) | Required TRE inventory; become template parameters (PORT-01), not removed. |
| `searchPath_00_12=D:/swg_dev_bundle` | 29 | **V→R** | Referenced ONLY in client_d.cfg (grep: zero code/doc/other-cfg refs). Dir exists but contains Phase-7 dev scratch (PHASE7_SPAWN_NOTES.md, manifests, `swgsource_client_d_snippet.cfg`) — not a boot-required TRE root, and absent from client.cfg (which boots fine). Verify boot without it, then remove. |
| `searchPath_00_11=...stage/override` | 31 | **K** (auto-substitute) | Required for D3D11 //hlsl overrides; preserve priority above TREs. Index normalization vs client.cfg's `00_10` is a discretion call. |
| `[Station] gameFeatures=15 / subscriptionFeatures=1` | 36–38 | **K** | CONFIG-02 feature bits; required. |
| `[ClientGraphics] rasterMajor=11` (+ giant inline comment) | 41 | **K** value, **R** comment | D-06 canonical 11. Strip the multi-paragraph Phase-17/18 history comment (template carries a one-line note). |
| `windowed=true / screenWidth/Height=1920x1080` | 42–46 | **K** | Functional dev settings. |
| `psrcCensus=false` | 47–49 | **K** (default) | Already default-false; harmless. Could remove (no-op when absent) — discretion. Comment trimmable. |
| `disableMultiStreamVertexBuffers=false` | 50–53 | **V (D-07 engine flip)** | Override of engine default `true` (ConfigClientGraphics.cpp:104). D-07 allows flipping the engine default to make multi-stream the default so this key disappears. Requires changing `KEY_BOOL(disableMultiStreamVertexBuffers, true)` → `false` and boot-verifying both renderers (D3D11 skinned DOT3 depends on multi-stream). |
| `[ClientGraphics/PostProcessingEffectsManager] enable=true` | 55–66 | **K** value, **R** comment | Required (UI draws through post-FX per the comment). Strip the Iter-23 essay. |
| `[Direct3d9] allowTearing=true` | 68–72 | **K** | WDDM/Win11 Present-hang workaround; needed for D3D9 boot gate (rasterMajor=5). |
| `[ClientGame] loginServerAddress0/Port0` | 74–77 | **K** (templatize) | VM connection; template parameter or fixed. |
| `skipIntro=true` | 79 | **K** | Faster boot. |
| `skipSplash=false` | 80–87 | **V** | client.cfg has `skipSplash=true`. The `false` was a Phase-11 diagnostic (Iter-28). Recommend flip to `true` to match client.cfg; verify boot. |
| `disableCutScenes=true` | 88 | **K** | Functional. |
| `[ClientGame/Bloom] enable=0` | 90–97 | **V (D-07 engine flip)** | Works around the D3D11 `setBloomEnabled` STUB (verified: `Direct3d11.cpp:1134` is still `STUB(setBloomEnabled)`). D-07 allows wiring it to a logging no-op so this section disappears. Requires changing the STUB → no-op + boot-verify (the STUB currently FATALs if Bloom enabled). |
| `[ClientUserInterface] disableG15Lcd=true` | 99–101 | **R** | Dead subsystem (lcdui removed v2.1 Phase 13). D-05 says "just go." Verify the key's absence doesn't re-enable a call path (lcdui_src unlinked → key is inert; safe remove). |
| `voiceChatEnabled=false` | 102–104 | **R** | Vivox dead (v2.1). D-05 says "just go." |
| `[SharedDebug/InstallTimer] enabled=true` | 106–111 | **V→K or R** | Phase-6 D-09 MVB-1 verification timer. Not a test-toggle in the Phase-11 sense; informational logging. Keep (low cost) or remove if noise — discretion. |
| `[SwgClient] allowMultipleInstances=true` | 113–114 | **K** | Dev convenience. |
| `[SharedFoundation] ProductRegistryPath / lookUpCallStackNames=false` | 116–122 | **K** | Registry-isolation + debug-perf; both functional. |
| `[ClientGraphics/Dpvs] reportInstrumentation=true` | 124–131 | **R (Phase 26)** but **K now** | This drives the D-15 throwaway harness removed in Phase 26 (HARD-03). Out of scope to remove here (sequencing: Phase 24 ships the verdict that supersedes its purpose, Phase 26 strips it). The NEW `occlusionMode` key goes in this same section. |
| `[ClientGraphics/Direct3d11] reportFrameStats=true` | 133–140 | **R** | Phase-11 per-frame metrics spam; default false (Direct3d11_Metrics.cpp:32). A Phase-11 test toggle → remove (PORT-02 target). |

**client.cfg parallel notes:** carries the same TRE paths + `searchPath_00_10` override + a Phase-17 rasterMajor comment essay (line 37–47) and `screenWidth/Height=1600x900` (RenderDoc-windowed compromise). The template must reconcile the two resolution choices (discretion) and strip the essays.

## Common Pitfalls

### Pitfall 1: redist ≠ stage/miles bytes (the D-11 provenance trap)
**What goes wrong:** Assuming `redist/` is interchangeable with the proven-working `stage/miles`. They are a different Miles point-release: `mssogg.asi` is 41,472 B in redist vs 99,840 B in stage; `mssmp3.asi` 95,744 vs 94,208; several `.flt` differ by ~500 B. The runtime-verified audio (the 2026-06-12 fix, MEMORY) used the **stage/miles Oct-2017 set**.
**Why it happens:** CONTEXT.md D-11 says "byte-identical to the public SWGSource Client v3.0 miles/" — that's stage-vs-external, NOT redist-vs-stage. The redist dir was populated separately (May 2025 mtimes).
**How to avoid:** Make redist the proven set — copy `stage/miles`'s `.asi/.flt/.dll` bytes into `redist/`, then add the 2 `.m3d`. Boot-verify audio after the postbuild stages the new redist. Don't trust the existing redist vintage blind.
**Warning signs:** Audio half-dies (UI rollovers work, music/world silent) + log warning-flood after switching the copy source — exactly the signature the phase is meant to eliminate.

### Pitfall 2: cfgs already untracked — wrong git surgery
**What goes wrong:** Planning `git rm --cached stage/client_d.cfg` as a key step. Both cfgs are already untracked (matched by `stage/*`, only `stage/override/` is negated).
**Why it happens:** D-09's "generated cfgs untracked" reads like they're currently tracked.
**How to avoid:** The actual git work is (a) create the template dir OUTSIDE `stage/*` (e.g. `tools/` or `scripts/`), (b) ensure it's tracked, (c) confirm the generated cfgs stay ignored. No `rm --cached` for the cfgs. The 2 new `.m3d` files DO need `git add` to redist/.
**Warning signs:** `git status` shows the template under `stage/` and gitignored — then you'd need a `!stage/<template>` negation, which fights the `stage/*` intent. Put the template outside `stage/`.

### Pitfall 3: F11 force-disable precedence inverted
**What goes wrong:** New config path overrides the `_DEBUG` F11 flag, breaking the Phase-23-style A/B toggle (and Phase 26's deferred Release measurement prep).
**Why it happens:** Natural to compute the config bit last.
**How to avoid:** D-04 is explicit: force-disable wins. Apply `ms_forceDisableOcclusionCulling` AFTER the config/auto logic, inside `#ifdef _DEBUG`, clearing the bit unconditionally (see Pattern 1).
**Warning signs:** F11 in a Debug build no longer flips DPVS:ON/OFF in the DebugMonitor overlay.

### Pitfall 4: Miles absence detection at the wrong hook (per-sample flood)
**What goes wrong:** Hooking absence detection at the per-sample decode site → you reproduce the flood you're trying to replace.
**Why it happens:** The actual failure surfaces at `AIL_set_named_sample_file` (Audio.cpp:2823 / 4436), called once per sample.
**How to avoid:** D-12 wants ONE startup warning. Hook at `Audio::install` after `AIL_set_redist_directory("miles")` + `AIL_startup()` (Audio.cpp:1276-1280): check the known codec files exist on disk (the `.asi`/`.m3d` set) and/or that `AIL_open_digital_driver` succeeded, emit a single `WARNING`. Note: the existing `AIL_open_digital_driver` failure path (1292-1307) already emits one warning AND disables audio — but it does NOT fire on the half-dead case (driver opens via system DirectSound; only codec decode fails). So D-12 needs an ADDITIONAL explicit file-presence probe, not reuse of 1292.
**Warning signs:** Log has thousands of "Error loading and allocating the sample" (Audio.cpp:1525) — the flood signature.

### Pitfall 5: dual-renderer boot gate skipped for D3D9
**What goes wrong:** Verifying only `rasterMajor=11` (the canonical D-06 value) and shipping a cfg that breaks `rasterMajor=5`.
**Why it happens:** D3D9 is "one flip away" and easy to forget; some keys (`allowTearing`, windowed-resolution fallback) matter only on D3D9.
**How to avoid:** Phase-close gate (per CONTEXT + STATE): boot to character select under BOTH. In Debug, set rasterMajor in `client_d.cfg` (not client.cfg). Watch the D3D9 1920×1080-windowed → exclusive-fullscreen fallback noted in client.cfg (kills RenderDoc but boots).
**Warning signs:** rasterMajor=9 is FATAL (MEMORY); only 5/6/7/11 valid.

## Code Examples

### POB detection (verified primitive)
```cpp
// CellProperty.cpp:555 — the canonical world-cell predicate
bool CellProperty::isWorldCell() const { return this == ms_worldCellProperty; }
// In RenderWorld, ms_cameraCell is the camera's parent cell (set at :873).
// Outdoor  → ms_cameraCell->isWorldCell() == true  → occlusion ON in auto
// POB cell → ms_cameraCell->isWorldCell() == false → occlusion OFF in auto
```

### Config read pattern (verified, three lines from the new key's home)
```cpp
// DpvsProfileInstrumentation.cpp:85,90 — the [ClientGraphics/Dpvs] read precedent
char const * const csvDir = ConfigFile::getKeyString("Dpvs/Experiment", "csvDir", "dpvs-profile/");
ms_reportOverlay = ConfigFile::getKeyBool("ClientGraphics/Dpvs", "reportInstrumentation", true);
```

### ConfigClientGraphics getter pattern (verified precedent for the new getter)
```cpp
// ConfigClientGraphics.cpp:128 install + :348 getter — mirror for getDpvsOcclusionMode
KEY_BOOL(psrcCensus, false);                 // install: cache once
bool ConfigClientGraphics::getPsrcCensus() { return ms_psrcCensus; }  // getter
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| CMake `configure_file(@ONLY)` generates cfgs | MSBuild build + (this phase) PowerShell setup-script templating | Phase 9 (Option D, MSBuild adoption) | The cfg header's "re-run cmake" is FALSE; D-08 replaces the story |
| DPVS occlusion hardcoded removed (Option α `#else`) | Config-gated tri-state (`off` default, `auto` per Phase-23 verdict) | This phase (HARD-01) | Operationalizes the verdict without surgery |
| Phase-23 F11-only `_DEBUG` A/B toggle | F11 kept + Release-visible config knob | This phase | Release can now select mode; Phase 26 measures + may flip default |
| Miles copy from machine-specific `D:\Code\SWGSource Client v3.0\miles` | Copy from vendored `src/external/.../miles-7.2e/redist` | This phase (PORT-01) | Fresh-clone audio works without an external install |

**Deprecated/outdated:**
- The "Generated by CMake … re-run cmake to regenerate" header in both cfgs — false since Phase 9.
- `AudioCapture.flt` — the AudioCapture subsystem was removed (v2.1 Phase 13); **verified zero `AudioCapture` references in `src/`** → the engine never enumerates it; safe to exclude from redist (D-11 confirmed).
- `reportFrameStats` / `reportInstrumentation` cfg toggles — Phase-11/Phase-23 diagnostics; the latter is a Phase-26 removal.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | **Correction to D-11's premise:** the vendored `redist/` files are NOT byte-identical to the proven-working `stage/miles` set (different Miles point-release; `mssogg.asi` 41 KB vs 99 KB). Recommendation: copy stage/miles bytes into redist so the vendored set is the runtime-verified one, then add the 2 `.m3d`. | Pitfall 1 / Summary | If the planner trusts the existing redist vintage, a fresh clone could get DIFFERENT (possibly broken) audio than the verified config. Needs a boot-verify of audio after the source switch. **User/planner confirmation recommended on which byte-set is canonical.** |
| A2 | The boot survives entirely-missing `stage/miles` (audio half-dead, UI works) rather than FATALing — so D-12 detection must be an explicit file probe, not reliance on `AIL_open_digital_driver` failure (which opens via system DirectSound and does not fire on the codec-only failure). | Pitfall 4 | If the digital driver actually DOES fail without redist on a clean machine, the existing 1292-1307 path already warns+disables and D-12 is partly redundant. Verify on the fresh-clone test (D-10). |
| A3 | `searchPath_00_12=D:/swg_dev_bundle` is safely removable — it's referenced only in client_d.cfg, absent from client.cfg (which boots), and the dir holds Phase-7 dev scratch, not boot-required TREs. | PORT-02 audit | Low — D-05 mandates a verify-then-remove boot test anyway. If something silently depends on it, the boot test catches it. |
| A4 | The `disableMultiStreamVertexBuffers` and Bloom engine-default flips (D-07) are each a one-line change (`KEY_BOOL` default; STUB→no-op) — small and reversible. | PORT-02 audit | Medium — the Bloom STUB currently FATALs when Bloom enabled (Direct3d11.cpp:1134); the no-op must be verified not to break the post-FX chain. Boot-verify both renderers. |

## Open Questions

1. **Which Miles byte-set is canonical (redist's newer vintage vs stage's Oct-2017 verified set)?**
   - What we know: stage/miles is the runtime-verified working set (2026-06-12 audio fix). redist differs in size for `.asi`/`.flt`.
   - What's unclear: whether the newer redist Miles build also works (untested at runtime in this tree).
   - Recommendation: make redist = stage/miles bytes + the 2 `.m3d`; boot-verify audio. Lowest-risk path to a known-good fresh clone.

2. **One template with a `rasterMajor` parameter, or two templates (client.cfg + client_d.cfg)?**
   - What we know: the two cfgs differ in rasterMajor history, resolution (1920×1080 vs 1600×900), skipSplash, and a few diagnostic keys; both share the TRE-root + override paths.
   - What's unclear: how much per-file divergence to preserve vs unify.
   - Recommendation: one parameterized template emitting both (DRY for the shared TRE/path block), with a `-RasterMajor` and `-Resolution` parameter. Discretion per D-08/CONTEXT.

3. **Where does the template dir live (`tools/` vs `scripts/` vs `src/cmake/config/`)?**
   - What we know: must be outside `stage/*`; the dead CMake template lived at `src/cmake/config/client.cfg.in`.
   - Recommendation: a fresh `tools/setup/` (the CMake dir name is misleading now). Discretion.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| PowerShell | Setup script (D-08) | ✓ | 5.1+ (Win11) | — |
| MSBuild | Build + postbuild | ✓ | via `$env:MSBUILD` (not on PATH) | full path |
| `src/external/.../miles-7.2e/redist/` (9 files) | Miles staging | ✓ | tracked (9/12) | — |
| `Msseax.m3d` + `msssoft.m3d` | D3D 3D audio providers | ✗ (not in redist) | — | present in `stage/miles` + `D:/Code/SWGSource Client v3.0/miles/` — copy in |
| `stage/miles/` (12 files, Oct-2017) | runtime-verified audio set | ✓ | working baseline | — |
| TRE asset root (`D:/Code/SWGSource Client v3.0/`) | boot (TRE/TOC) | ✓ (this machine) | — | template prompts per-machine (the portability fix) |
| `D:/swg_dev_bundle` (`searchPath_00_12`) | nothing verified | ✓ (exists) | dev scratch | removable (A3) |

**Missing dependencies with no fallback:** None.
**Missing dependencies with fallback:** The 2 `.m3d` files — copy from `stage/miles` (preferred, verified bytes) or the external SWGSource install.

## Validation Architecture

> nyquist_validation = true in config.json. This phase's "tests" are boot gates + a fresh-clone gate, not a unit-test framework — there is no pytest/jest harness in this C++ engine repo. The validation is behavioral, captured below.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None (C++ engine; validation = manual boot/play gates per established v2.x convention) |
| Config file | n/a |
| Quick run command | Build via `$env:MSBUILD ...SwgClient.vcxproj`; launch `stage/SwgClient_d.exe` |
| Full suite command | Dual-renderer boot gate (rasterMajor=5 then =11) + fresh-clone test (D-10) |

### Phase Requirements → Validation Map
| Req | Behavior | Validation Type | Method | Exists? |
|-----|----------|-----------------|--------|---------|
| HARD-01 | `auto` enables occlusion outdoors, disables in POB; `on`/`off` force | manual + DebugMonitor | In a `_DEBUG` build with `occlusionMode=auto`, walk from Mos Eisley plaza into the cantina; the DPVS overlay (DpvsProfileInstrumentation) shows the bit set outdoors / clear indoors. F11 still flips it (D-04). Set `occlusionMode=on`/`off` and confirm unconditional. | ✅ existing Phase-23 DebugMonitor overlay + F11 + visible-object count |
| HARD-01 | bit flips at POB boundary | manual | The 23-verdict's visible-object-count signal (359 outdoor / 443 indoor) and the overlay confirm the boundary flip | ✅ |
| PORT-01 | fresh clone boots, no abs paths | fresh-clone test (D-10) | Clone repo to a NEW dir on this machine, run `setup-client.ps1` (prompts TRE root), build, launch → character select. Audio plays (miles staged from redist). | ❌ Wave 0: setup script must exist first |
| PORT-01 | miles absence → ONE warning | manual | Rename `stage/miles`, boot → exactly one startup warning (not a flood); UI audio degrades gracefully | ❌ Wave 0: D-12 hook must be added |
| PORT-02 | clean cfg boots both renderers | dual boot gate | Generated `client_d.cfg`, set rasterMajor=5 → character select; set =11 → character select | ✅ established gate |

### Sampling Rate
- **Per cfg-key removal (risky, D-05):** boot + brief play test, both renderers.
- **Per engine-default flip (D-07):** boot + the affected feature visible (skinned mesh / scene render), both renderers.
- **Phase gate:** dual-renderer boot to character select + the D-10 fresh-clone test.

### Wave 0 Gaps
- [ ] `tools/setup/setup-client.ps1` — generates both cfgs from template, validates miles files (D-08/D-12a). No prior script exists.
- [ ] `tools/setup/client*.cfg.template` — tracked source-of-truth (D-09).
- [ ] D-12b engine hook in `Audio::install` (clientAudio) — one-shot codec-absence warning. No prior hook exists.
- [ ] redist reconciliation + 2 `.m3d` added + `git add` (A1).
- *(Boot/play gates themselves reuse the established v2.x manual convention — no new harness.)*

## Security Domain

> security_enforcement = true, ASVS level 1. This phase has a minimal security surface: it manipulates local config files, a vendored binary redist, and build scripts. No network/auth/crypto code is added.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | LoginServer cfg is a static dev VM address; SSO key intentionally absent (CONFIG-01) |
| V3 Session Management | no | — |
| V4 Access Control | no | localhost/single-user dev client |
| V5 Input Validation | yes (light) | The PowerShell setup script takes a user-supplied TRE root path — validate it exists and is a directory before substitution; quote paths-with-spaces (the cfgs already quote `"D:/Code/SWGSource Client v3.0/"` to survive ConfigFile space-splitting). |
| V6 Cryptography | no | No crypto added. (989crypt is unrelated, untouched.) |
| V12 File/Resource | yes (light) | Vendored redist binaries (`.asi/.m3d/.flt/.dll`) enter git — provenance matters (A1: confirm they match the known-good SWGSource set, ~240 KB). |

### Known Threat Patterns for this phase
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Untrusted path injection via setup-script prompt | Tampering | Validate `Test-Path -PathType Container`; quote on substitution; reject paths with cfg-breaking chars |
| Tampered/unknown-provenance redist binary | Tampering | Source the `.m3d` from the verified stage/miles or known SWGSource install; record sizes (A1 table); the bytes are unchanged-in-kind per D-11 provenance note |
| Accidental secret/abs-path commit (the recurring "DO NOT COMMIT this cfg") | Information Disclosure | D-09 permanently fixes this — generated cfgs stay gitignored; only the template (with placeholders, no machine paths) is tracked |

## Sources

### Primary (HIGH confidence — verified in this tree)
- `src/engine/client/library/clientGraphics/src/shared/RenderWorld.cpp` — gate site (908-916), `ms_cameraCell` assignment (873), F11 flag (83/909/1200-1203), change-detect (920-930)
- `src/engine/shared/library/sharedObject/src/shared/portal/CellProperty.cpp:555` + `.h:105` — `isWorldCell()`
- `src/engine/client/library/clientGraphics/src/shared/ConfigClientGraphics.cpp` (70-128, 348) + `.h` — getter pattern
- `src/engine/client/library/clientGraphics/src/shared/DpvsProfileInstrumentation.cpp:85-91` — `[ClientGraphics/Dpvs]` read precedent
- `src/engine/client/library/clientAudio/src/win32/Audio.cpp` — Miles init (1276-1318), digital-driver failure path (1292-1307), per-sample decode (2823/4436), per-sample warning flood site (1525)
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11.cpp:1134` — `STUB(setBloomEnabled)` (D-07 Bloom flip dependency)
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj:123-131,226-234` — postbuild exe/pdb + Miles copy (current machine-specific source)
- `stage/client_d.cfg`, `stage/client.cfg` — full key inventory
- `.gitignore:82-93` — `stage/*` + `!stage/override/`
- Git tracking probes — cfgs untracked, override + redist tracked
- Byte-size compare — redist vs stage/miles divergence (A1)
- Grep — zero `AudioCapture` refs in `src/`; `swg_dev_bundle` referenced only in client_d.cfg

### Secondary (MEDIUM)
- `docs/recon/23-dpvs-d3d11-profiling.md` — the HARD-01 verdict (outdoor keep / indoor remove, Debug-build caveat)
- `.planning/todos/pending/2026-06-12-config-gate-dpvs-occlusion-...md` — adopted knob design
- MEMORY entries — 2026-06-12 audio fix (stage/miles missing signature), Koogie postbuild copy, rasterMajor values, don't-modify-Koogie rule

## Metadata

**Confidence breakdown:**
- DPVS knob (HARD-01): HIGH — gate site, POB predicate, config pattern, and F11 interplay all verified in source; the change is small and the building blocks exist.
- Portability (PORT-01): HIGH on the path inventory and postbuild repoint; MEDIUM on the Miles byte-set decision (A1 — needs a boot-verify after the source switch).
- cfg cleanup (PORT-02): HIGH — every key verdict cross-checked against source defaults and git/grep; the two risky keys (multi-stream, Bloom) have their engine dependencies pinned to exact lines.

**Research date:** 2026-06-12
**Valid until:** ~30 days (in-repo facts; only invalidated by further commits to the named files)
