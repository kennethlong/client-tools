# Handback -- Tatooine async-load D3D11 texture-create crash guard (38-07, CONSULT-47)

**Date:** 2026-06-23
**Plan:** 38-07 (gap-closure)
**Status:** Guards landed + build-gate PASS (Debug+Release, x64+Win32). Boot/live DEFERRED to you.

## CONSULT-47 verdict (in brief)

The intermittent Tatooine cold-zone-in crash is an **NVIDIA driver null-deref in
`NtGdiDdDDICreateAllocation`** reached via the **async `MeshAppearanceTemplate` (.msh) callback ->
`StaticShaderTemplate` -> gl11 `Direct3d11_TextureData::CreateTexture2D`** path. It is **NOT** the
`.mgn`/`SkeletalMeshGenerator` path that the prior `d1f92ab1f` fix hardened -- that path is disjoint
(the marauder-belt `.mgn` is a temporal co-occurrence in a different async-loader slot, not a
caller->callee cause), so `d1f92ab1f` could not have prevented this crash.

**Root cause (engine bad-desc camp, weighted up by the de-anchoring break -- the crash predates
Utinni, see the 2026-06-19 standalone gl11-x64 occurrence with the same asset signature):**
`Texture::load`'s DDS magic/header validation is `DEBUG_FATAL`-only -> a **no-op in the Release
client** where the crash lives -> a short/corrupt DDS continues and builds a garbage
`D3D11_TEXTURE2D_DESC` (bad Width/Height/Mips/Format) that the driver chokes on.

Utinni is at most an **aggravator** (extra concurrent device pressure on the shared device), not the
root.

## Two guards landed

1. **`Texture::load` Release-safe DDS fallback = THE FIX**
   (`clientGraphics/src/shared/Texture.cpp`). Malformed magic/header (A1) and insane dimensions (A2,
   D3D11 FL11.0 caps: dim<=16384, depth<=2048, mips<=15) now fall back to the default texture with a
   Release-visible `WARNING` that NAMES the asset. Mirrors the function's existing open-failure and
   unknown-format fallback idioms. Valid textures: unchanged.

2. **`Direct3d11_TextureData` desc-reject = THE NAMED DIAGNOSTIC**
   (`Direct3d11/src/win32/Direct3d11_TextureData.cpp`). Validates engine dims BEFORE
   `CreateTexture2D`; on invalid, emits a Release-visible `WARNING` naming the asset and gates the
   create loop so the existing `!m_texture FATAL` fires with our named diagnostic preceding it -- a
   named crash beats the opaque driver null. Edit A should normally prevent ever reaching here.

Both edits are `.cpp` only -> no shared-header / plugin-ABI cascade. `UTINNI_HOOKPOINTS_VERSION`
untouched (dumpbin confirms `GetEngineHookPoints` still undecorated on both Win32 exes; version 3).
Staged to both `stage-x64/` and `stage/`.

## Remaining controls -- run on the staged binary

1. **Zone into Tatooine COLD (gl11, Release) several times.**
   - If the `Texture::load: malformed DDS header...` or `...invalid DDS dimensions...` WARNING fires,
     it **NAMES the bad asset** -- paste it back here. That confirms the bad-desc theory AND the
     fallback fixes the crash.
   - If no crash AND no warning across several cold zone-ins: the bad-desc theory is either
     confirmed-and-fixed or simply didn't trigger this run (intermittent).
2. **No-Utinni control** (~10 min, no rebuild): rename `UtinniCore.dll`, 5+ cold Tatooine zone-ins.
   Crash persists ⇒ overlay is not the root (consistent with the de-anchoring break).
3. **D3D9 (gl05) control:** same spot under `rasterMajor=5`. Clean ⇒ D3D11-path-specific.
4. **Driver-version control:** swap the NVIDIA driver build. Version-sensitive ⇒ driver bug -> pin/file.

## Refutation condition

**If the crash persists with NO `Texture::load` WARNING logged, the bad-desc hypothesis is REFUTED**
and the Utinni-overlay / driver-contention camp (Sonnet/Opus -- shared-device resource-create race
during the cold-cache burst, or a driver allocation-tracker bug) is implicated. In that case the
`Direct3d11_TextureData` desc-reject WARNING will also be silent (the desc was clean), which is itself
the discriminating signal: clean desc + still crashes = not our descriptor.
