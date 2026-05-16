# Phase 11 — D3D9 vs D3D11 Visual Parity Reference

## Purpose

Plan 11-02 (Wave 2) captures D3D9 baseline reference screenshots at two fixed in-world camera anchors. Plan 11-09 (Wave 9) reproduces these same anchors under the new D3D11 renderer (`rasterMajor=11`, `gl11_d.dll`) for side-by-side visual-parity comparison per SPEC R6.

The PNGs themselves are the framing reference — Plan 11-09 matches by visible landmarks at the recorded `/loc` coordinates.

## Anchors (captured 2026-05-16, D3D9 baseline)

### Anchor 1 — Mos Eisley plaza (outdoor)

- **File:** `d3d9-tatooine-outdoor.png` (949,442 bytes)
- **Scene:** Tatooine, Mos Eisley plaza
- **World coordinates (X, Y, Z):** `(3467, 5, -4850)`
- **Renderer:** D3D9 (`rasterMajor=5`, `gl05_d.dll` built and staged with the post-build cp fix from Plan 11-01 commit `266e173b3`)
- **Capture method:** in-game screenshot at the displayed coordinates after walking to a stable plaza anchor (camera framing visible in the PNG)
- **Framing:** see PNG; Plan 11-09 to match by visible landmarks
- **D3D11 match file (Plan 11-09):** `d3d11-tatooine-outdoor.png` (PENDING)

### Anchor 2 — Mos Eisley cantina (interior)

- **File:** `d3d9-cantina-interior.png` (749,857 bytes)
- **Scene:** Tatooine, Mos Eisley cantina interior
- **World coordinates (X, Y, Z):** `(3455, 5, -4834)`
- **Renderer:** D3D9 (`rasterMajor=5`, `gl05_d.dll`)
- **Capture method:** in-game screenshot at the displayed coordinates inside the cantina at a sparse-NPC angle
- **Framing:** see PNG; Plan 11-09 to match by visible landmarks
- **D3D11 match file (Plan 11-09):** `d3d11-cantina-interior.png` (PENDING)

## SWG coordinate convention reminder

Per `src/engine/client/library/clientUserInterface/src/shared/core/SwgCuiGroundRadar.cpp:541-558`, the three numbers displayed below the minimap are:

- First: **X** (longitude, east/west)
- Second: **Y** (height, altitude)
- Third: **Z** (latitude, north/south)

The two anchors above are ~17m apart on the ground plane: X delta = −12, Z delta = +16, same Y altitude. Consistent with "outdoor plaza anchor and cantina interior anchor are a short walk from each other".

## Plan 11-09 reproduction protocol

When Wave 9 fires:

1. Set `client_d.cfg [ClientGraphics] rasterMajor=11` (note: **Debug exe reads `client_d.cfg`, not `client.cfg`** — see Plan 11-02 SUMMARY lessons-learned).
2. Launch `D:/Code/swg-client-v2/stage/SwgClient_d.exe` with the full Wave 3–7 D3D11 implementation in place.
3. Walk to coords `(3467, 5, -4850)` on Tatooine; match visible-landmark framing to `d3d9-tatooine-outdoor.png`; capture as `d3d11-tatooine-outdoor.png`.
4. Walk to coords `(3455, 5, -4834)` (cantina interior); match framing to `d3d9-cantina-interior.png`; capture as `d3d11-cantina-interior.png`.
5. Write the side-by-side visual-parity verdict in the SPEC R6 acceptance section of `11-09-SUMMARY.md`.

## Cross-references

- **Plan 11-02 sub-step 3a — D3D11 plumbing smoke PASSED.** FATAL fired at `Direct3d11.cpp:62 scaffold_fatal_stub` reached from `Graphics::install` via `Graphics.cpp:320` (first `Gl_api` call inside install) and `Graphics.cpp:554` (further into install), with the stage call chain `ClientMain.cpp:312` → `SetupClientGraphics.cpp:92` → `Graphics.cpp:320` → `Direct3d11.cpp:62`. Crash dump: `stage/SwgClient_d.exe-unknown.0-20260516220506.{txt,mdmp}` (gitignored under `stage/`).
- **Plan 11-02 sub-step 3b — D3D9 sanity PASSED.** With `rasterMajor=11` reverted in `client_d.cfg`, client reaches char-select + world load cleanly (no SafeCast dialogs — today's `SwgClient_d.exe` relink picked up the ContrailData D-18 guard at commit `73e29eee7`). D-05 protection holds.
- **D-04a DESCOPE verdict** (Plan 11-01 SUMMARY): `Direct3d11_FfpGenerator.{h,cpp}` OMITTED from Plan 11-05 (Wave 5) source list.

## SPEC R6 verdict (populated by Plan 11-09)

- **Verdict:** PENDING — awaiting D3D11 captures at the two anchors above.
