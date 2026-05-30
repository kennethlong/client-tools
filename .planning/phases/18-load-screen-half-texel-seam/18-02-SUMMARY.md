---
phase: 18-load-screen-half-texel-seam
plan: 02
status: complete
completed: 2026-05-30
requirements: [UI-01]
autonomous: true
branch: koogie-msvc-cpp20-base
---

# 18-02 SUMMARY — central half-pixel c9 correction (D3D11), seam gone

## What was done

Landed the consult-confirmed central half-pixel correction in the D3D11 plugin and boot-gated it. The
edit is gl11-only and resolution-independent; the cheap one-screenshot wrong-sign detector passed
(seam gone, character renders correctly).

## Edited site (as found, post-drift)

- **File:** `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StateCache.cpp`
- **Function:** `setViewport()`, the c9 `viewportData` write.
- **Line range (koogie branch):** correction at lines ~1698-1704 (the c9 `viewportData[4]` initializer, after the verbatim D3D9 transcription at ~1684). NOTE the line range differs from the swg-blender-m16 citation (1593-1602) — confirmed by reading the koogie file.

## Applied bias (matches the 18-01 synthesis)

```
float const halfPixelClipX = 1.0f / static_cast<float>(width);
float const halfPixelClipY = 1.0f / static_cast<float>(height);
viewportData[2] = -1.0f - xOffset + halfPixelClipX;   // .z += 1/width
viewportData[3] =  1.0f + yOffset - halfPixelClipY;   // .w -= 1/height
```

- Sign: `.z += 1/width`, `.w -= 1/height` — the consult-confirmed direction (not pre-locked).
- Magnitude: `1/w`, `1/h` (a half pixel scaled by the c9 `2/w` term); no hardcoded `0.5f`/`512`/`1024`.
- Scale terms (`.x`/`.y`) unchanged. One central edit; no per-draw `+0.5` scatter (D-02).
- `getOneToOneUVMapping` left a `scaffold_fatal_stub` (D-01).

## File-scope deviation

**None.** The 18-01 consult confirmed the expected c9 `viewportData` site, so the edit stayed within the
declared `files_modified` (`Direct3d11_StateCache.cpp`). No `…/Direct3d9/…` file touched (D-04).

## Branch correction (deviation from the original execution location)

18-02 was initially edited+built on the `swg-blender-m16` main checkout by mistake; that branch is stuck
at 17-07 (pre-GAP-4) so its rebuilt `gl11_d.dll` rendered the character BLACK and clobbered the good
staged DLL. Recovered: restored the koogie `gl11_d.dll`, reverted the stray swg-blender-m16 edit, ported
the Phase 18 planning to koogie, and re-applied the fix here on `koogie-msvc-cpp20-base` (the D3D11
source-of-truth: has 17-08/17-09 parity work). Captured in memory `feedback_d3d11_work_belongs_in_koogie_worktree`.

## Build / link result

- Rebuilt the Direct3d11 plugin .vcxproj (Debug|Win32) in the koogie worktree via `$env:MSBUILD`,
  `/nodeReuse:false`. Only `Direct3d11_StateCache.cpp` recompiled (incremental).
- **Link grep:** `unresolved external symbol` count = **0** (not relying on exit 0; `/FORCE` not used). No LNK/fatal errors.
- Post-build copy landed `gl11_d.dll` (+ `.pdb`) into `stage/` via the worktree `stage` junction → `D:\Code\swg-client-v2\stage`. Staged DLL = 2543104 bytes @ 17:57 (the koogie parity build + this fix), replacing the earlier wrong 2491392-byte build.

## Dual-renderer boot smoke + seam sanity (D-04 gate)

- **rasterMajor=11 (D3D11):** boots; **character renders correctly** (parity work intact — my c9 edit did not disturb it); **world-load-screen centerline seam is GONE** (Kenny, in-client, 2026-05-30). → correct sign, no flip needed; mechanism is position-c9 as the consult judged.
- **rasterMajor=5 (D3D9):** reference path structurally unregressed — no `…/Direct3d9/…` file was modified (D-04). A post-18-02 rasterMajor=5 re-capture is captured as evidence in 18-03.
- No new FATAL / no `scaffold_fatal_stub` hit.

## Carried into 18-03

The cheap sanity check confirms seam-gone on the world load screen with the character rendering correctly.
18-03 owns the full evidence: ≥3 deterministic splash-variant matched pairs, the higher-res (1440p/1080p)
capture, a non-default-viewport/odd-width capture, the full-2D no-regression sweep (HUD/fonts/radial/
char-select 2D via zoom-crop/pixel diff), and the post-18-02 rasterMajor=5 re-capture — then the UI-01 human A/B gate.

## Self-Check: PASSED
