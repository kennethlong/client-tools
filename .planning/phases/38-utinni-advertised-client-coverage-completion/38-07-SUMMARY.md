---
phase: 38-utinni-advertised-client-coverage-completion
plan: 07
subsystem: clientGraphics + Direct3d11
tags: [tatooine, async-load, d3d11, texture-create, null-deref, nvidia-driver, dds-validation, release-safe, consult-47, gap-closure, build-gate]

# Dependency graph
requires:
  - phase: 38-utinni-advertised-client-coverage-completion
    provides: "94-row GetEngineHookPoints at VERSION 3 (38-05); Utinni embed/inject path live (Phase 37/38). The crash this plan guards reproduces under Utinni inject (aggravator) but is overlay-INDEPENDENT (CONSULT-47 de-anchoring: same signature pre-Utinni 2026-06-19 standalone gl11 x64)."
provides:
  - "Texture::load() Release-safe DDS validation: malformed magic/header (A1) + insane dimensions (A2) now fall back to the default texture with a Release-visible WARNING naming the asset -- previously DEBUG_FATAL-only (no-op in Release), so a corrupt DDS fed a garbage D3D11_TEXTURE2D_DESC into CreateTexture2D (the NVIDIA NtGdiDdDDICreateAllocation null-deref). THIS IS THE FIX."
  - "Direct3d11_TextureData ctor desc-reject diagnostic (B): validates engine dims BEFORE CreateTexture2D; on invalid, NAMES the asset (Release-visible WARNING) and gates the create loop on dimsValid so the existing !m_texture FATAL fires with a named diagnostic preceding it -- a named crash beats the opaque driver null. THIS IS THE DISCRIMINATING DIAGNOSTIC."
affects: [clientGraphics, Direct3d11, utinni]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Release-safe asset guard: a DEBUG_FATAL-only validation in a load path is a no-op in the Release client (where the crash actually lives). Promote to a graceful fallback (delete fileInterface; recursion-guarded DEBUG_FATAL on the default name; Release-visible WARNING; load(getDefaultTextureName()); return) -- mirror the function's existing open-failure (:472) and unknown-format (:621) fallback idioms rather than inventing a new one."
    - "Two-layer guard: fix at the engine source (Texture::load fallback) PLUS a named diagnostic at the driver-adjacent call site (Direct3d11_TextureData before CreateTexture2D). The engine fix should prevent ever reaching the diagnostic; the diagnostic discriminates the root cause if a bad desc still slips through (engine bad-desc camp vs. driver/overlay-contention camp)."
    - "Discriminating instrument: if the WARNING fires it NAMES the bad asset (engine bad-desc confirmed + fixed). If the crash persists with NO WARNING logged, the bad-desc hypothesis is refuted and the driver/overlay-contention camp is implicated (Sonnet/Opus H1/H3). The fix and the experiment are the same code change."

key-files:
  created:
    - .planning/handoff/2026-06-23-tatooine-texture-crash-guard.md
    - .planning/phases/38-utinni-advertised-client-coverage-completion/38-07-SUMMARY.md
  modified:
    - src/engine/client/library/clientGraphics/src/shared/Texture.cpp
    - src/engine/client/application/Direct3d11/src/win32/Direct3d11_TextureData.cpp

key-decisions:
  - "CONSULT-47 verdict: the crash path is the async MeshAppearanceTemplate (.msh) callback -> StaticShaderTemplate -> gl11 Direct3d11 CreateTexture2D, NOT the .mgn/SkeletalMeshGenerator path. The prior d1f92ab1f '.mgn hardening' fix is on a DISJOINT path (marauder belt .mgn is temporal co-occurrence, not a caller->callee cause) and could not have prevented this crash."
  - "Root cause (engine bad-desc camp, Codex+Cursor, weighted up by the de-anchoring break since the crash predates Utinni): Texture::load's DDS magic/header checks are DEBUG_FATAL-only -> no-op in the Release client where the crash lives -> a short/corrupt DDS continues and builds a garbage D3D11_TEXTURE2D_DESC. Edit A makes those checks Release-safe; Edit B names the asset at the driver call site."
  - "Edit A is the FIX, Edit B is the DIAGNOSTIC. Edit A should normally prevent ever reaching Edit B; Edit B exists to (1) name the asset if a bad desc still slips through and (2) discriminate the productive 2-2 split (engine bad-desc vs. driver/overlay contention) on the maintainer's next live repro."
  - "Behavior-preserving for VALID textures: both edits only fire on out-of-range input (bad magic/header, or dims outside D3D11 FL11.0 limits w/h<=16384, depth<=2048, mips<=15). The common path -- valid DDS, sane dims -- is byte-for-byte unchanged in behavior."
  - "Release-visible logging: used WARNING(a,b) (sharedFoundation/Fatal.h, fires in Release) in BOTH files. In Texture.cpp WARNING was already used at :475/:624; in Direct3d11_TextureData.cpp WARNING comes from the same Fatal.h as the FATAL already used at :387 (PCH chain) and getName() is already used at :444/:554 -- no new include needed."
  - "Both edits are .cpp only (no shared-header change) -> NO plugin ABI cascade. UTINNI_HOOKPOINTS_VERSION untouched (no edit to the contract source); dumpbin confirms GetEngineHookPoints still exported undecorated on both rebuilt Win32 exes (version still 3)."
  - "Built BOTH platforms: x64 (active milestone; the 2026-06-19 standalone repro was gl11 x64) AND Win32 (the GetEngineHookPoints/Utinni contract is a Win32 feature; the recent crash-20260623-nvwgf2um repro is under Utinni inject = Win32). The shared source guarantees the fix lands on both."

requirements-completed: []

metrics:
  duration: ~40min
  tasks: 1
  files-changed: 2
  completed: 2026-06-23
---

# Phase 38 Plan 07: Tatooine async-load D3D11 texture-create crash guard Summary

CONSULT-47-reviewed guard for the intermittent Tatooine cold-zone-in D3D11 texture-create crash
(NVIDIA driver null-deref in `NtGdiDdDDICreateAllocation`). The crew verdict
(`.planning/research/CONSULT-47-SYNTHESIS.md`): a malformed/short DDS feeds a garbage
`D3D11_TEXTURE2D_DESC` into `CreateTexture2D` because `Texture::load`'s DDS checks are
`DEBUG_FATAL`-only (no-op in the Release client where the crash lives). Two guards landed.

## Edit A -- Texture::load Release-safe (THE FIX)

`src/engine/client/library/clientGraphics/src/shared/Texture.cpp`, in `Texture::load(const char*)`:

- **A1 (malformed magic/header):** after the existing `DEBUG_FATAL` magic + header reads (kept), a
  Release-safe bail if `magicRead != 4 || magic != 'DDS ' || headerRead != isizeof(ddsHeader)` ->
  `delete fileInterface;` + recursion-guarded `DEBUG_FATAL` on the default-texture name + a
  Release-visible `WARNING` naming the file + `load(getDefaultTextureName()); return;`.
- **A2 (insane dimensions):** after `m_width/m_height/m_depth/m_mipmapLevelCount` are set and the
  `mipmapLevelCount==0 -> 1` clamp, before the `#ifdef _DEBUG` block, a dimension-sanity bail
  (D3D11 FL11.0 caps: 2D/3D dim <= 16384, volume depth <= 2048, mips <= 15) with the same graceful
  fallback + named `WARNING`.

Mirrors the function's existing graceful-fallback idiom (open-failure :472, unknown-format :621).

## Edit B -- Direct3d11_TextureData desc-reject (THE DIAGNOSTIC)

`src/engine/client/application/Direct3d11/src/win32/Direct3d11_TextureData.cpp`, in the ctor after
the cube-miscFlags block and before the format loop: validate `getWidth/getHeight/getDepth/
getMipmapLevelCount` into `dimsValid`; on invalid, emit a Release-visible `WARNING` naming the asset
(`getName()`, `<unnamed>` fallback) with all dims + cube/vol flags, and gate the create loop
`for (int i = 0; dimsValid && !m_texture && ...)` so the existing `if (!m_texture) FATAL(...)` fires
with our named diagnostic preceding it. No early-return from the ctor (the SRV path below still runs
the same !m_texture-FATAL handling).

## Build gate (PASS)

Built serially, `$env:MSBUILD` (VS18 v145), PowerShell, `/nodeReuse:false`, staged exes/dlls deleted
to force relink, engine lib rebuilt in both configs before relinking each exe config. **8 builds, all
0 `unresolved external symbol` (links under /FORCE; grepped each log), 0 `LNK1181`:**

| Target      | Platform | Debug | Release |
| ----------- | -------- | ----- | ------- |
| Direct3d11  | x64      | OK    | OK      |
| SwgClient   | x64      | OK    | OK      |
| Direct3d11  | Win32    | OK    | OK      |
| SwgClient   | Win32    | OK    | OK      |

All freshly staged (stage-x64/ for x64, stage/ for Win32). `dumpbin /exports` on both rebuilt Win32
exes: `GetEngineHookPoints = _GetEngineHookPoints` (undecorated, ordinal 83/82) -- contract unchanged,
`UTINNI_HOOKPOINTS_VERSION` still 3 (no edit to the contract source).

## Boot/live verification -- DEFERRED to maintainer

Per execution mode (do NOT run the client), boot-smoke + live-inject are deferred. The handback
(`.planning/handoff/2026-06-23-tatooine-texture-crash-guard.md`) lists the maintainer's controls:
(1) zone into Tatooine cold -- if the WARNING fires it NAMES the bad asset (paste back); no crash +
no warning = bad-desc theory confirmed-and-fixed (or didn't trigger this run); (2) no-Utinni control;
(3) D3D9 (gl05) control; (4) different NVIDIA driver. **If the crash persists with NO Texture::load
WARNING logged, the bad-desc hypothesis is refuted and the Utinni-overlay/driver-contention camp
(Sonnet/Opus) is implicated.**

## Deviations from Plan

Built BOTH platforms (x64 + Win32) rather than one. Rationale: the active milestone is x64 and the
2026-06-19 standalone repro was gl11 x64, but the recent `crash-20260623-nvwgf2um` repro is under
Utinni inject (a Win32 feature) and "the crash is in SwgClient_r.exe". The shared source means the
fix must land on both staging dirs; building both also re-proves the GetEngineHookPoints export
survived on Win32. Tracked as `[Rule 3 - completeness]` -- not a behavior change.

Otherwise: plan executed exactly as written.

## Self-Check: PASSED

- Modified files present: Texture.cpp, Direct3d11_TextureData.cpp (git status confirms only these two
  source files dirty).
- Created files present: this SUMMARY + the handback.
- Build artifacts staged: stage-x64/{SwgClient_d,SwgClient_r}.exe + {gl11_d,gl11_r}.dll and
  stage/{...} all re-dated 2026-06-23.
