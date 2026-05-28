# Phase 17 Char-Select — D3D11 vs D3D9 (matched-pair scaffold)

**Created:** 2026-05-28 (Plan 17-01 Task 3 prep)
**Census boot:** 2026-05-28 15:09 (Plan 17-01 Task 3 completed)
**Purpose:** Matched-pair reference + aggregated PSRC census artifact for Phase 17 (CHAR-01 skin/clothing diffuse, CHAR-02 eyes, CHAR-03 multi-stage head). D3D9 (`rasterMajor=5`, `gl05_d.dll`) is the ground truth; D3D11 (`rasterMajor=11`, `gl11_d.dll`) is the plugin under development.

**Method:** Same character at char-select default pose. cfg `rasterMajor` toggled `11↔5` between captures; client restarted in place per pair. Char-select is a deterministic, isolated screen — no LOD streaming, no skinning animation, no light scaling jitter — so matched pairs are EXACT (unlike Phase 12 exterior pairs).

**Naming convention** (mirrors `docs/research/phase12-baseline/COMPARISON.md`):

```
{anchor}_{renderer}_{shot}.jpg|png
```

Phase 17 anchors:

| Anchor | What | Renderer expected pair |
|--------|------|------------------------|
| `sul_eye` | char eye material (`sul_eye.sht`, `TAG_PPSH` program path, `numberOfStages==0`) | `sul_eye_d3d9_NNNN.jpg` + `sul_eye_d3d11_NNNN.jpg` |
| `sul_head` | char head/face multi-stage material (any `sul_*_head.sht` the census shows loaded) | `sul_head_d3d9_NNNN.jpg` + `sul_head_d3d11_NNNN.jpg` |
| `single_stage` | one single-stage body/clothing control material chosen from the census output — proves "pipeline works" separately from "multi-sampler works" | `single_stage_d3d9_NNNN.jpg` + `single_stage_d3d11_NNNN.jpg` |

The default-pose character framing capture at full screen counts as the canonical `char_default_{renderer}_NNNN.jpg|png` overview pair.

---

## Image inventory

| File | Renderer | Anchor | Shot | Match | Notes |
|------|----------|--------|------|-------|-------|
| `char_default_d3d11_0001.png` | D3D11 (`gl11_d.dll`, fresh rebuild against new ShaderImplementation.h layout) | char_default | 0001 | _pending D3D9 pair_ | 1920x1080 windowed. Char visible at default char-select pose. Magenta/white-fallback PS shading the body — character appears washed-out white, dark/closed eyes, plain pants/tunic with no proper material color. Background hangar + X-wing visible. **This is the expected pre-Plan-17-02 baseline** — Plan 17-01 only retains PSRC text; Plan 17-02 wires the recompile lane that fixes the asset-PS path. |
| `char_default_d3d9_NNNN.png` | D3D9 (`gl05_d.dll`, fresh rebuild) | char_default | _pending_ | _DEFERRED_ | Per Plan 17-01 close-out decision 2026-05-28: D3D9 ground-truth pair deferred. `gl05_d.dll` is already rebuilt and staged on host; capture later (during or after Plan 17-02 visual A/B) costs one boot cycle. The HLSL:asm verdict + census TSV is sufficient gating evidence for Plan 17-02 to proceed without the D3D9 pair. |
| `psrc-census.tsv` | — | — | — | — | Snapshot of `stage/stage/psrc-census.tsv` from the census boot (127 raw rows, 32 unique programs). Path is `stage/stage/...` due to SWG's cwd convention: exe runs with cwd=`stage/`, relative paths in shared/clientGraphics resolve from there (same convention as `stage/vs-disasm-all.txt`). |

---

## PSRC census artifact (gating deliverable for Plans 02/03)

**Source:** `stage/psrc-census.tsv` (resolves on host to `D:/Code/swg-client-v2/stage/stage/psrc-census.tsv` due to cwd convention) — written by the file sink in `ShaderImplementationPassPixelShaderProgram::load_0000` when `[ClientGraphics] psrcCensus=true`. Header row + one row per loaded PS program:

```
fileName  isHlsl  profile  includeCount  samplerSlots  constants
```

**Plan-01-only HEAD sha (R3-01d auditability — CONFIRMED):**

Census boot performed on host stage/ binary built from worktree HEAD `5eea334742f9e070ed7364cead452cbfac9df866` (Plan-01-only — Task 2 commit, before any Plan 02 recompile ctor work). Worktree branch `worktree-agent-a52cb01c9f63fa9fb`, deployed to host as `stage/SwgClient_d.exe.plan-17-01` and copied over `stage/SwgClient_d.exe`. Plan 02 had not shipped at census time, so the census output reflects the pre-recompile binary unambiguously.

**Renderer plugins used for the boot:** `gl11_d.dll` and `gl05_d.dll` both freshly rebuilt against the new `ShaderImplementation.h` (the new `m_psrcText` / `m_psrcLen` members shifted `m_graphicsData`'s struct offset by 16 bytes; the existing stage dlls were 4 days stale and crashed deterministically on the first PS-program asset before this rebuild — see `[[project_shared_header_abi_cascade_trap]]` for the diagnostic + fix).

### Aggregated HLSL:asm ratio

**Raw census** (127 rows — duplicates included; each PS program reloads ~4-5x during char-select due to per-StaticShader-template loads):

| Metric | Count | % |
|--------|-------|---|
| Total PS-program LOAD events at char-select | 127 | 100% |
| HLSL PSRC load events (`//hlsl` first non-whitespace → recompile lane, primary) | 80 | 63.0% |
| asm PSRC load events (no `//hlsl` prefix → asm-port lane, secondary) | 47 | 37.0% |

**Unique programs** (32 distinct `.psh` files — dedupe by fileName + classification fields):

| Metric | Count | % |
|--------|-------|---|
| Unique PS programs loaded | 32 | 100% |
| Unique HLSL programs | 22 | 68.75% |
| Unique asm programs | 10 | 31.25% |

**Profile distribution (unique):** `ps_2_0` (18), `ps_1_1` (3), `ps_1_4` (1), `asm` (10). No `ps_3_0` at char-select — modern profile not exercised here.

**Lane mix decision (gates Plans 02/03) — SR-3 STOP NOT TRIGGERED:**

The named anchors do NOT appear directly in the census because shader templates (`.sht`) reference pixel programs (`.psh`) by separate names. The pragmatic interpretation of the SR-3 gate is the character-rendering shader families:

| Family | Unique count | All HLSL? | Verdict |
|--------|--------------|-----------|---------|
| `h_*_ps20.psh` (human body / skin / clothing — `h_specmap_cbmp`, `h_color2_specmap_cbmp`, `h_simple_pp`, `h_color2_bump`, etc.) | 10 | **YES — 100% HLSL ps_2_0** | Character body path is fully HLSL. Plan 17-02 covers it. |
| `a_*_ps20.psh` (asset/armor — `a_simple_pp`, `a_bloom_mask_pass`, `a_specmap_pp`, `a_emismap`) | 5 | **YES — 100% HLSL ps_2_0** | Armor + bloom mask. Plan 17-02 covers. |
| `e_*_ps20.psh` (effects — `e_emisadd_masked_vcolor_bloom`, `e_bloom_mask_pass_vcolor`) | 2 | **YES — 100% HLSL ps_2_0** | Emissive + bloom effects. Plan 17-02 covers. |
| `ui_membrane_color2_ps20.psh` | 1 | YES — HLSL ps_2_0 | UI overlay. Plan 17-02 covers. |
| Asm primitives (`vertex_color`, `3d_vertex_color`, `shadowvolume`, `t`, `a_modulate`, `2d_texture`, `ui.psh`, `bad_vertex_shader`) | 10 | NO — asm ps_1_1/ps_1_4 | Low-level primitives, NOT what's making the character white. Existing PEXE path handles them. Plan 17-02B (asm port) NOT required for char-select beachhead. |

**Verdict: PROCEED with Plan 17-02 (HLSL recompile lane only).** No sub-plan 17-02B spawn required. All character-rendering shader families are HLSL ps_2_0; the asm rows are basic primitives that aren't responsible for the washed-out character body in `char_default_d3d11_0001.png`.

### Per-shader census table (unique programs, sorted by fileName)

| fileName | isHlsl | profile | includes | samplers | constants |
|----------|:------:|---------|:--------:|:--------:|:---------:|
| `pixel_program/2d_heat_composite.psh` | 1 | ps_2_0 | 2 | 2 | 0 |
| `pixel_program/2d_texture.psh` | 0 | asm | 0 | 0 | 0 |
| `pixel_program/3d_vertex_color.psh` | 0 | asm | 0 | 0 | 0 |
| `pixel_program/3d_vertex_color_a.psh` | 0 | asm | 0 | 0 | 0 |
| `pixel_program/a_alpha_emis_full_ps11.psh` | 1 | ps_1_1 | 2 | 1 | 0 |
| `pixel_program/a_bloom_mask_pass_ps20.psh` | 1 | ps_2_0 | 2 | 1 | 0 |
| `pixel_program/a_emismap_ps20.psh` | 1 | ps_2_0 | 2 | 1 | 0 |
| `pixel_program/a_lava_alpha_ps14.psh` | 1 | ps_1_4 | 2 | 3 | 0 |
| `pixel_program/a_modulate.psh` | 0 | asm | 0 | 0 | 0 |
| `pixel_program/a_modulate2x.psh` | 0 | asm | 0 | 0 | 0 |
| `pixel_program/a_simple_pp_ps20.psh` | 1 | ps_2_0 | 2 | 1 | 0 |
| `pixel_program/a_specmap_pp_ps20.psh` | 1 | ps_2_0 | 2 | 2 | 0 |
| `pixel_program/bad_vertex_shader.psh` | 0 | asm | 0 | 0 | 0 |
| `pixel_program/e_bloom_mask_pass_vcolor_ps20.psh` | 1 | ps_2_0 | 2 | 1 | 0 |
| `pixel_program/e_emisadd_masked_vcolor_bloom_ps20.psh` | 1 | ps_2_0 | 2 | 1 | 0 |
| `pixel_program/e_particle_alpha_emis_full_ps11.psh` | 1 | ps_1_1 | 2 | 1 | 0 |
| `pixel_program/h_color2_bump_ps20.psh` | 1 | ps_2_0 | 2 | 2 | 0 |
| `pixel_program/h_color2_pp_ps20.psh` | 1 | ps_2_0 | 2 | 1 | 0 |
| `pixel_program/h_color2_specmap_bump_aniso_ps20.psh` | 1 | ps_2_0 | 2 | 2 | 0 |
| `pixel_program/h_color2_specmap_bump_ps20.psh` | 1 | ps_2_0 | 2 | 2 | 0 |
| `pixel_program/h_color2_specmap_cbmp_ps20.psh` | 1 | ps_2_0 | 2 | 3 | 0 |
| `pixel_program/h_envmask_specmap_ps20.psh` | 1 | ps_2_0 | 2 | 3 | 0 |
| `pixel_program/h_simple_pp_ps20.psh` | 1 | ps_2_0 | 2 | 1 | 0 |
| `pixel_program/h_spec_pp_ps20.psh` | 1 | ps_2_0 | 2 | 1 | 0 |
| `pixel_program/h_specmap_bump_ps20.psh` | 1 | ps_2_0 | 2 | 2 | 0 |
| `pixel_program/h_specmap_cbmp_ps20.psh` | 1 | ps_2_0 | 2 | 2 | 0 |
| `pixel_program/shadowvolume.psh` | 0 | asm | 0 | 0 | 0 |
| `pixel_program/t.psh` | 0 | asm | 0 | 0 | 0 |
| `pixel_program/ui.psh` | 0 | asm | 0 | 0 | 0 |
| `pixel_program/ui_membrane_color2_ps20.psh` | 1 | ps_2_0 | 2 | 1 | 0 |
| `pixel_program/vertex_color.psh` | 0 | asm | 0 | 0 | 0 |
| `pixel_program/zwrite_mask_ps11.psh` | 1 | ps_1_1 | 2 | 0 | 0 |

### Deferred-feed flags (Plan 03 scope gate)

Per RESEARCH Open Question 3: looking for char-select PS references to scroll/fog/stencil constants above and beyond material color + textureFactor.

| Feed | Found in census? | Notes |
|------|:----------------:|-------|
| `textureScroll` | NOT SIGNALED | constants column = 0 across all 32 unique rows |
| `fog` | NOT SIGNALED | constants column = 0 across all 32 unique rows |
| `stencil` | NOT SIGNALED | constants column = 0 across all 32 unique rows |

**Caveat — classifier limitation:** the `constants` column counts `register(c…)` substring occurrences in the retained PSRC text. SWG's PSRC files appear to declare constants via `cbuffer { … }` or untyped-binding syntax (D3D9-fxc HLSL allows this), NOT the `register(c0)` hint style — so the count is uniformly zero and is not a reliable signal here. For Plan 03 scope, do NOT rely on this column alone; revisit by inspecting the retained `m_psrcText` directly during Plan 17-02's recompile work when the actual constant references will surface via `D3DReflect` cbuffer-variable enumeration.

**Action for Plan 03:** keep the planned material-color + textureFactor scope; do NOT expand for scroll/fog/stencil based on this column. Revisit if D3DReflect output during Plan 17-02 surfaces feeds beyond the two committed constants.

### Instrumentation note for Plan 17-02

127 census rows / 32 unique programs → average 4× load per program. Each PS program is reloaded per-StaticShader-template instantiation (or similar — exact source TBD during Plan 17-02 investigation). Plan 17-02's recompile lane MUST dedupe by `m_psrcText` content hash (or by `fileName`) so the same HLSL source isn't re-compiled 4× through `D3DCompile` during char-select boot. Without dedup, the recompile cost on boot is ~5× larger than needed.

---

## Findings (post-boot, bucketed for Phase 17)

### 1. CHAR-01 — single-stage body/clothing diffuse

D3D11 baseline (`char_default_d3d11_0001.png`): character body / clothing / pants are uniformly white-washed with no material color modulation. Skin tone absent; tunic appears as plain off-white cloth instead of the SOE-canonical material color. Plan 02 control candidate from the census: any single-sampler `h_simple_pp_ps20.psh` or `h_color2_pp_ps20.psh` user (these are 1-sampler HLSL ps_2_0 programs — the most isolated "pipeline works" test). Identify the exact `.sht` that consumes these during Plan 17-02 Task 3 CHAR-01 validation.

D3D9 pair: deferred. Capture when convenient (gl05_d.dll is staged).

### 2. CHAR-02 — eyes (palette color, seated, occluded — D-07 requires committed matched pair)

D3D11 baseline: eyes are dark / closed-looking. This matches the project-known v2.2 deferred bug (see `[[project_open_runtime_issues_2026_05_25]]` — "D3D11 eyes (v2.2 asset-PS)"). Plan 17-02 is the v2.2 work that addresses this. Eye material is the `sul_eye.sht` `TAG_PPSH` / `numberOfStages==0` path; verify which `.psh` it references during 17-02 Task 3.

D3D9 pair: deferred.

### 3. CHAR-03 — head/face multi-stage composite

D3D11 baseline: face is washed-out white like the rest of the body, with what appears to be a faint beard/mustache visible but no proper skin tone. Multi-stage head shader (`h_specmap_cbmp_ps20.psh`, `h_color2_specmap_cbmp_ps20.psh`, `h_color2_specmap_bump_*_ps20.psh` are the candidates from the census) is loaded but not bound — fallback PS rendering produces this. Plan 17-02 + 17-03 wiring will restore the composite.

D3D9 pair: deferred.

### 4. D-08 single-stream vs multi-stream skinning A/B (RenderDoc, CONFIRMATION ONLY)

Not exercised at char-select (character is at default pose, no animation). RenderDoc was not installed at the default path per RESEARCH §Environment Availability. This is informational for the Phase 22 GEO-01 decision and does NOT block CHAR-01/02/03.

---

## Census boot operational notes (for reproducibility)

- **Host stage prep (orchestrator-applied, 2026-05-28):**
  - Backed up `stage/SwgClient_d.exe` → `stage/SwgClient_d.exe.pre-17-01`
  - Backed up `stage/gl11_d.dll` → `stage/gl11_d.dll.pre-17-01`
  - Backed up `stage/gl05_d.dll` → `stage/gl05_d.dll.pre-17-01`
  - Deployed worktree's `SwgClient_d.exe` (Plan-01-only build) → `stage/SwgClient_d.exe` + preserved at `stage/SwgClient_d.exe.plan-17-01`
  - Built `gl11_d.dll` from `Direct3d11.vcxproj` in worktree (against new header) → deployed to `stage/gl11_d.dll`
  - Built `gl05_d.dll` from `Direct3d9.vcxproj` in worktree (against new header) → deployed to `stage/gl05_d.dll`
  - Set `[ClientGraphics] psrcCensus=true` in `stage/client_d.cfg`
  - Set `screenWidth=1920` / `screenHeight=1080` / `windowed=true` for 1920x1080 capture
- **Boot:** `SwgClient_d.exe` launched from explorer (cwd = `stage/`). Char-select reached at 1920x1080 windowed. One overview screenshot captured at default pose.
- **Post-boot:** `psrcCensus=false` reset in `stage/client_d.cfg` (D-03 — instrumentation kept in code, off by default).
- **Side artifacts preserved on host:** `stage/SwgClient_d.exe.plan-17-01` (Plan-01-only binary), pre-17-01 dll/exe backups (safe to delete after Phase 17 close-out).
