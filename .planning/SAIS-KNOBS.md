# Sais restore-knobs ledger (strict-data-defaults branch)

Every hard-coded behavior of Sais's that the branch removed or gated, and the exact switch
that puts it back. Rule applied throughout the branch: **the data renders as authored by
default; each of his compensations survives as an explicit, loud (WARNING-on-set) opt-in.**

Maintained for the PR #1 body refresh — fold this table in before reopening, so Sais sees
in one place that nothing of his was deleted, only parked behind a key.

## Config knobs (set in his client cfg to restore the old behavior)

| Knob | Default | His original | What it restores | Commit |
| --- | --- | --- | --- | --- |
| `[SharedFoundation] strictData=false` | `true` (strict) | always on | Skeletal ARGB==0→white rewrite: loading/zero-color skeletal vertex ARGB painted opaque white (`clientSkeletalAnimation`). Lenient mode also relaxes every strict-data FATAL to the stock/lenient path — it is the branch-wide leniency switch, not a dedicated lighting knob. | c14 `f4167a197` (gate), c1 `0b23a438e` (the switch itself) |
| `[Direct3d11] ambientBoost=true` | `false` | always on | c_ambient.inc source patch `mov r7, vColor0` → add of scene ambient (c16). ⚠ Not byte-identical to his original: the re-enabled add is COLOR-only with **alpha pinned** to the baked value (his shape inflated COLOR.a to a measured 1.85 — latent hazard for anything reading alpha as opacity). `Direct3d11_ShaderSource::patchIncludeContents`. | c20 `52ba6fc36` |
| `[Direct3d11] diffuseFloorPercent=85` | `0` (off) | hardwired 0.85 | The `max(lightData.ambient.ambientColor + diffuseSpecular.diffuse, 0.85)` floor patched into every stock `//hlsl` program (kept characters ≥85% lit). 85 reproduces his tuning exactly; any other 1–100 value is a new tuning. `Direct3d11_ShaderSource::patchProgramSource`. | c20 `52ba6fc36` |
| `[Direct3d11] synthesizeHemisphericLight=true` | `false` | always on | Synthetic hemisphere for lights authored without hemispheric colors: tangent = 0.65×, back = 0.30× of the key light's diffuse (fills the shade side of characters — his outdoor-characters fix). `Direct3d11_LightManager::setExtendedLightData`. | c21 `bf4aef663` |

Notes:
- All four are cache-safe: the D3D11 compile key hashes post-patch text and includes are
  hashed as served, so flipping a knob is a self-announcing cache miss, never stale bytecode.
- The 0.3 scene-ambient floor is NOT here on purpose — Sais removed it himself
  (`a98867e9d`, 2026-07-27, "stop flooring scene ambient at 0.3"). His call, pre-branch.
  Visual impact of the floor (his own measurement, from the removal commit): authored
  interior ambient is 0.125–0.135, so the floor MORE THAN DOUBLED it (7/12 lit batches
  that frame) — washed-out grey interiors; midday exteriors unaffected. His code comment
  ("matches what SOE shipped") is contradicted by that same measurement.

## ⚠ His D3D9 tree — compensations still hard-coded, DELIBERATELY not knobbed

`Direct3d9_LightManager.cpp` still carries the FULL ungated stack: the 0.3 ambient floor
(:562-568) AND the 0.65/0.30 hemisphere synthesis (:789), and the ShaderSource patch
family (c_ambient add, 0.85 diffuse floor) originates in his DX9 include-serve path.
These went LIVE the moment c2 (Gl_dll.def) made his x64 D3D9 DLLs load. Disposition
**CONFIRMED by Kenny 2026-08-15: leave untouched — his D3D9 line is being replaced
wholesale by our stock-faithful gl05/06/07 (merge agreement #1, "D3D9 is ours"), which
retires the entire stack.** No knobs to be mirrored there.

## Corpus/data side (loose files, not in the squash repo — restore by file copy)

| File | What we changed | Restore path |
| --- | --- | --- |
| `stage-B-override/vertex_program/modules/c_ambient.inc` | His asm2hlsl corpus copy carried `r7 = vColor0 + cLightData_ambient_ambientColor;` — reverted to stock cell semantics `r7 = vColor0;` (CONSULT-74, live-verified). Note the `ambientBoost` engine knob does NOT cover this copy — the engine patch targets the TRE **asm** text, and this loose pseudo-HLSL copy wins search-path resolution over the TRE. | Copy `c_ambient.inc.sais-orig` (preserved beside it) back over `c_ambient.inc`. |

## Behavior changes with NO restore knob (defect fixes, listed for completeness)

- c3 `969086be4` — window reveal activates (dropped his `SWP_NOACTIVATE`); the window never
  received startup focus.
- c15 `1f73947ff` — fade opacity no longer written into the RT alpha/bloom-glow channel;
  DX9's draw-time mask+blend overrides ported (also gives real translucent fade-ins).
- c17 `33ffc4d51` — CreateInputLayout failures retried with phantom stream instead of
  permanently cached null (restored all in-zone nebulas).
- c18 `495913c0f` — data-shader compile/reflection failures demoted FATAL → WARN-and-skip
  (stock-faithful; his client died on any bad data shader).
- c16 `c2983980e` — ten late-NGE Options widgets made optional (his client FATALed on the
  Options screen against the shipped v3.0 data).

If Sais wants any of these five back as-was, they need a revert conversation, not a cfg key —
each replaced a crash or a wrong-output path, not a preference.
