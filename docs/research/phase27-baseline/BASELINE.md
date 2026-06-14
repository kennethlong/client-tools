# Phase 27 — Pre-Port A/B Visual Baseline (D3DCompile port)

Reference screenshot set captured **before** the `D3DXCompileShader → D3DCompile` HLSL swap
(Plan 27-02). This is the parity reference that Plan 27-03's dual-renderer verdict diffs against.

- **Captured:** 2026-06-14, Release client (`SwgClient_r.exe`).
- **State at capture:** Plan 27-01 Tasks 1–2 committed (census + `d3dcompiler_47.dll` staged +
  `d3dcompiler.lib` linked alongside retained `d3dx9.lib`). **No compile-call change yet** — the
  runtime still uses `D3DXCompileShader`. gl05+gl07 rebuilt clean Debug+Release (0 unresolved externals).
- **Renderers:** D3D9 = `rasterMajor=5` (gl05_r.dll), D3D11 = `rasterMajor=11` (gl11_r.dll).
- Source frames live in the gitignored `stage/screenshots/`; copied here (tracked) with descriptive names.

## A/B Pairs

| Spot | What it exercises | D3D9 (rasterMajor=5) | D3D11 (rasterMajor=11) |
|------|-------------------|----------------------|------------------------|
| Character Select | UI + character render | `charselect_d3d9_0371.jpg` | `charselect_d3d11_0376.jpg` |
| Black-wall position (distance) | the once-black bump wall, from afar | `blackwall_distance_d3d9_0372.jpg` | `blackwall_distance_d3d11_0378.jpg` |
| Wall close-up (detail/bump) | detail-map surface up close (//asm `a_detail*` consumers + the Tatooine bump/dot3 Fix-A class) | `wall_closeup_detail_d3d9_0373.jpg` | `wall_closeup_detail_d3d11_0379.jpg` |
| Sign (decal) | decal surface (//asm `*_decal` consumers) | `sign_decal_d3d9_0374.jpg` | `sign_decal_d3d11_0380.jpg` |
| Cantina interior | the heavily A/B'd interior parity scene | `cantina_interior_d3d9_0375.jpg` | `cantina_interior_d3d11_0377.jpg` |

Original frame numbers (`stage/screenshots/screenShot0NNN.jpg`):
D3D9 — 0371 char-select, 0372 black-wall distance, 0373 wall close-up, 0374 sign, 0375 cantina.
D3D11 — 0376 char-select, 0377 cantina, 0378 black-wall distance, 0379 wall close-up, 0380 sign.

## Coverage vs Task 3 acceptance

- char-select ✓
- world scene ✓ (black-wall position, distance + close-up)
- Tatooine bump/dot3 (Fix-A class) ✓ (wall close-up — detail/bump surface)
- saber/decal/detail surface ✓ (sign = decal; wall close-up = detail-map). *(No ignited-saber frame
  in this set; the decal + detail-map //asm consumers are covered, which is sufficient for the
  Plan 03 "do the 96 //asm consumers still render" parity check.)*

## Usage in Plan 03

Re-capture the same five spots under both renderers after the Plan 02 HLSL swap and compare:
no flat/black lighting, no garbled transforms, texcoords correct, and the //asm-driven decal/detail
surfaces still render (they stay on `D3DXAssembleShader` all phase). The wall close-up doubles as the
Tatooine Fix-A `0xC0000090` crash-site check on the now-`D3DCompile` HLSL path.
