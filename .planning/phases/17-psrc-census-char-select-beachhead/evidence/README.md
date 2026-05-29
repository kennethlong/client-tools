# Phase 17 char-select evidence — capture-reproducibility contract

This sidecar pins the **deterministic capture contract** for the Phase 17 gap-closure
A/B screenshot cycle (GAP-1). Without an identical framing across the D3D9 baseline, the
D3D11 PRE-gap, and the D3D11 POST-gap captures, a pixel-diff is meaningless. Plan 17-05
Tasks 2 + 4 (Kenny's host boots) **reject any capture taken without honoring this contract.**

Naming convention is mirrored from `docs/research/phase12-baseline/COMPARISON.md`
(`{anchor}_{renderer}_{shot}` form), using **PNG instead of JPG** so a future Phase 20
pixel-diff harness has lossless input. See SECTION 5.

---

## SECTION 1 — Capture-reproducibility contract (pins)

All three captures (D3D9 baseline, D3D11 PRE-gap, D3D11 POST-gap) MUST frame identically.
If ANY pin below drifts between the PRE and POST captures, that capture is **INVALID** and
Kenny must re-run the boot.

- **Character slot:** the FIRST character slot at the default selection cursor on char-select
  entry. Do NOT navigate — capture the slot the screen lands on by default after login.
- **Camera / pose:** the char-select default pose. No UI interaction, no mouse drag-rotate,
  no zoom. Capture within ~3 seconds of the char-select screen settling so the idle animation
  cycle sits at approximately the same frame across PRE/POST/D3D9 boots. Frame-perfect idle
  alignment may not be achievable; this is the documented convention, not a hard guarantee.
- **UI state:** name-entry dialog dismissed (default); no chat-window open; no popup; no tooltip.
- **Resolution:** pinned to whatever `stage/client_d.cfg` currently specifies —
  `screenWidth=1920` × `screenHeight=1080` at time of writing. If resolution changes mid-cycle,
  recapture BOTH the PRE and POST D3D11 shots.
- **Gamma:** the `stage/client_d.cfg` gamma setting MUST be identical across all three captures
  (D3D9, D3D11 PRE, D3D11 POST). No explicit `gamma` key is present in the cfg at time of
  writing (engine default applies). If gamma is touched mid-cycle, recapture.
- **Color-space:** SDR. No HDR display path is active under the current Phase 11 `client_d.cfg`
  state. SRVs remain `_UNORM` (no sRGB-view double-correction — per v2.2 gamma decision).
- **Time-of-day:** the char-select scene has fixed lighting — no time-of-day skew. (Recorded
  here for the future open-world phases; not a variable for char-select.)

---

## SECTION 2 — The four capture filenames + provenance

| Filename | Renderer | When | Provenance |
|----------|----------|------|------------|
| `char_default_d3d9_0003.png` | D3D9 (rasterMajor=5) | Task 2 | Baseline reference, invariant across the gap-closure cycle (D3D9 path is the fixed truth). |
| `char_default_d3d11_0003_preGap.png` | D3D11 (rasterMajor=11) | Task 2 | PRE-gap state (current INFRASTRUCTURE-COMPLETE tree: `wroteDiffuse=0` / `wroteEmissive=0` / 0-of-9 asset-PS binds — fallback PS owns every bind). Isolates the gap surface. |
| `char_default_d3d11_0003_postGap.png` | D3D11 (rasterMajor=11) | Task 4 | POST-gap state (after 17-06a/06b + 17-07 land: diffuse + emissive writes into discovered channels; asset-PS bind rate flipped). Measures the closure. |
| `char_default_d3d11_0003.png` | D3D11 (rasterMajor=11) | Task 5 | Canonical alias — a copy of `_postGap.png` under the ROADMAP success-criterion #5 literal filename, so the future Phase 20 pixel-diff harness consumes the canonical name without re-capture. |

---

## SECTION 3 — Capture procedure (Kenny, Task 2 + Task 4 — verbatim)

For each renderer, in order:

1. **rasterMajor flip discipline.** Edit `stage/client_d.cfg` (the **debug** exe reads
   `client_d.cfg`, NOT `client.cfg` — silent ignore if you flip the wrong file). Set:
   - D3D9 baseline → `rasterMajor=5` (loads `gl05_d.dll`)
   - D3D11 PRE/POST → `rasterMajor=11` (loads `gl11_d.dll`)
   - NEVER set `rasterMajor=9` (FATAL).
   > NOTE (D3D9 baseline only): `client_d.cfg:37` flags the D3D9 reference path as parked at
   > Phase 17.X (gl05 rebuild texture-binding regression — pre-17-01 binaries work only with a
   > pre-17-01 `SwgClient.exe` due to the ABI cascade). If the current `SwgClient_d.exe` cannot
   > produce a clean D3D9 char-select baseline, record that in 17-VERIFICATION.md and use the
   > last known-good D3D9 capture as the reference rather than blocking the cycle.
2. **Clean-slate the log.** Delete `stage/d3d11-debug.log` before the D3D11 boots so the grep
   counts reflect only this boot:
   `Remove-Item stage/d3d11-debug.log -ErrorAction SilentlyContinue`
3. **Boot** `SwgClient_d.exe` to char-select and let the screen settle (~3 s).
4. **Screenshot.** Use the in-client ScreenShotHelper key; the client writes the PNG into the
   `stage/screenshots/` path. Rename/move it to the canonical filename from SECTION 2 and place
   it under this `evidence/` directory.
5. **Run the boot-gate grep set** (SECTION 4) against `stage/d3d11-debug.log` and report the
   counts back in the checkpoint resume signal.
6. **Confirm the pins** (SECTION 1) held across this boot.

---

## SECTION 4 — SYMMETRIC PRE + POST boot-gate grep set

Both the PRE-gap (Task 2) and POST-gap (Task 4) D3D11 cycles run **all** of these against
`stage/d3d11-debug.log`. (PRE-gap = current tree; POST-gap = after 17-06a/06b + 17-07 land.)

| Pattern (`-SimpleMatch` / `-Pattern`) | PRE expected | POST expected |
|---|---|---|
| `id=342` | 0 | 0 |
| `id=343` | ~0 | ~0 |
| `PSRC recompile FAILED` | 0 | 0 |
| `wroteDiffuse=1` | 0 | ≥ 1 (after 17-06b) |
| `wroteEmissive=1` | 0 | ≥ 1 (after 17-06b) |
| `wroteSpecular=1` | ≥ 9 (since 04ef976) | ≥ 9 (unchanged) |
| `Plan 17-07 .* COMPATIBLE vs=` | 0 (rewritten-lane log absent until 17-07 deploys) | ≥ 8 (after 17-07) |
| `Plan 17-07 .* INCOMPATIBLE vs=` | 0 (no Plan 17-07 log PRE-gap) | ≤ 1 (after 17-07) |
| `asset-PS bound=` | 0 | ≥ 8 (after 17-07 bind-time wiring) |
| `fallback-PS bound=` | ~9 | ≤ 1 |

PowerShell form for each (example):
`Select-String -Path stage/d3d11-debug.log -Pattern 'wroteDiffuse=1' -SimpleMatch | Measure-Object | % Count`

The regex patterns (`Plan 17-07 .* COMPATIBLE vs=` / `... INCOMPATIBLE vs=`) drop `-SimpleMatch`.

> **Round-5 item 1 — grep contract:** the COMPATIBLE/INCOMPATIBLE counters here use the
> **rewritten-lane token** `Plan 17-07 .* COMPATIBLE vs=` that Plan 17-07 emits from
> `isCompatibleWithVS_withExplicitPSInputs`. Do NOT grep a bare `COMPATIBLE vs=` substring —
> it does not exist in the live native log. The ~9 PRE-gap native INCOMPATIBLE lines belong to
> the separate `Plan 17-04 Task 1 .* INCOMPATIBLE vs=` token; capture those under that pattern
> if a PRE-gap native count is wanted.

> **Round-4 'success metric inflation':** `asset-PS bound=` is a NEW bind-time attribution log
> distinct from the COMPATIBLE verdict log — a COMPATIBLE verdict proves the validator flipped,
> but only `asset-PS bound=` proves the asset PS was actually bound at `PSSetShader`. If 17-07
> ships the per-VS rewrite path without emitting this bind-attribution log, the 17-07 executor
> MUST add it before this acceptance criterion can be satisfied.

---

## SECTION 5 — Naming-convention parent

Parent convention: `docs/research/phase12-baseline/COMPARISON.md`
(`{spot/anchor}_{renderer}_{shot}.jpg`). Phase 17 mirrors it with PNG instead of JPG for
lossless future-diff input. Success = "matches `rasterMajor=5`", NOT "no magenta".

---

## SECTION 6 — POST-gap build provenance (stale-DLL hard gate)

POST-gap build provenance (UPDATED after the 17-07 rewriter redesign):
- **HEAD commit:** `d8cc1ca99` (`feat(17-07): redesign rewriter to RECONSTRUCT PS input sig vs VS skeleton`) — final 17-07 code; sits on top of 17-06b `97d7bbf93`, so this build contains BOTH 17-06b (StaticShaderData) and 17-07 (PSData + StateCache + VertexShaderData). [Superseded earlier provenance for `f9e5ac569` (axis-b reorder, which the boot proved insufficient — see 17-07-SUMMARY).]
- **`stage/gl11_d.dll` LastWriteTime:** `2026-05-29 17:23 -05:00` (rebuilt this session; Plan 17-05 Task 4 must verify the deployed DLL matches this mtime before booting).
- **POST-gap visual:** asset-PS lane active (9/9 bind, `path=rewritten`) but renders BLACK — GAP-4 (b0↔b2 constant-buffer reconcile, cross-AI consult in flight). 17-05 Task 4/5 verdicts CHAR-01/02/03 = PARTIAL.
- **`stage/SwgClient_d.exe`:** UNCHANGED (`2026-05-28 20:51`) and CORRECT to be so — 17-07 touched only Direct3d11-plugin-internal headers (`Direct3d11_PixelShaderProgramData.h`, `Direct3d11_VertexShaderData.h`), which no other binary includes. Empirically confirmed by the incremental build: only `gl11_d.dll` relinked; `SwgClient_d.exe` was up-to-date and was NOT relinked. The plugin's EXPORTED interface is unchanged, so the 5/28 exe + 5/29 `gl11_d.dll` is a valid pairing. (This is NOT the shared-header ABI cascade trap of `project_shared_header_abi_cascade_trap`, which was specific to the clientGraphics `ShaderImplementation.h` shared header — untouched here.)
- **Build scope note:** built via incremental `swg.sln` Debug Win32. `Direct3d11.vcxproj` and `SwgClient.vcxproj` both reported **0 errors / 0 unresolved external symbols**. The whole-solution build exits non-zero only due to pre-existing, client-irrelevant tool/server project failures (MFC-dependent editors + missing `serverGame` headers: `SwgSpaceZoneEditor`, `CoreWeaponExporterTool`, `DataTableTool`, `Md5sum`, `UpdateLocalizedStrings`, etc.). Build log: `build-17-07-allplugins.msbuild.log`.

Plan 17-07 Task 1 STAGE 2 APPENDS to the line above after its all-plugin rebuild + commit,
recording the HEAD commit SHA (containing BOTH 17-06b and 17-07) and the rebuilt
`stage/gl11_d.dll` LastWriteTime. Plan 17-05 Task 4's deploy hard-gate verifies the deployed
DLL mtime matches this line BEFORE booting — so the POST-gap capture is provably taken against
the combined build, not a stale partial DLL (memory `project_shared_header_abi_cascade_trap`:
both 17-06b and 17-07 rebuild `gl11_d.dll`; 17-07 must be the final write).
