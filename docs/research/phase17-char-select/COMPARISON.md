# Phase 17 Char-Select — D3D11 vs D3D9 (matched-pair scaffold)

**Created:** 2026-05-28 (Plan 17-01 Task 3 prep)
**Purpose:** Matched-pair reference + aggregated PSRC census artifact for Phase 17 (CHAR-01 skin/clothing diffuse, CHAR-02 eyes, CHAR-03 multi-stage head). D3D9 (`rasterMajor=5`, `gl05_d.dll`) is the ground truth; D3D11 (`rasterMajor=11`, `gl11_d.dll`) is the plugin under development.

**Method:** Same character at char-select default pose. cfg `rasterMajor` toggled `11↔5` between captures; client restarted in place per pair. Char-select is a deterministic, isolated screen — no LOD streaming, no skinning animation, no light scaling jitter — so matched pairs are EXACT (unlike Phase 12 exterior pairs).

**Naming convention** (mirrors `docs/research/phase12-baseline/COMPARISON.md`):

```
{anchor}_{renderer}_{shot}.jpg
```

Phase 17 anchors:

| Anchor | What | Renderer expected pair |
|--------|------|------------------------|
| `sul_eye` | char eye material (`sul_eye.sht`, `TAG_PPSH` program path, `numberOfStages==0`) | `sul_eye_d3d9_NNNN.jpg` + `sul_eye_d3d11_NNNN.jpg` |
| `sul_head` | char head/face multi-stage material (any `sul_*_head.sht` the census shows loaded) | `sul_head_d3d9_NNNN.jpg` + `sul_head_d3d11_NNNN.jpg` |
| `single_stage` | one single-stage body/clothing control material chosen from the census output — proves "pipeline works" separately from "multi-sampler works" | `single_stage_d3d9_NNNN.jpg` + `single_stage_d3d11_NNNN.jpg` |

The default-pose character framing capture at full screen counts as the canonical `char_default_{renderer}_NNNN.jpg` overview pair.

---

## Image inventory

| File | Renderer | Anchor | Shot | Match | Notes |
|------|----------|--------|------|-------|-------|
| _(populated after Task 3 census boot + screenshots)_ | | | | | |

---

## PSRC census artifact (gating deliverable for Plans 02/03)

**Source:** `stage/psrc-census.tsv` — written by the file sink in `ShaderImplementationPassPixelShaderProgram::load_0000` when `[ClientGraphics] psrcCensus=true`. Header row + one row per loaded PS program:

```
fileName  isHlsl  profile  includeCount  samplerSlots  constants
```

**Plan-01-only HEAD sha (R3-01d auditability):**

The census boot MUST be performed on a tree at HEAD `5eea334742f9e070ed7364cead452cbfac9df866` (Plan-01-only — Task 2 commit, before any Plan 02 recompile ctor work). This proves the census output reflects the pre-Plan-02 binary and is not muddied by Plan 02's recompile path.

Verify before booting:

```bash
git rev-parse HEAD
# expect: 5eea334742f9e070ed7364cead452cbfac9df866
```

If the host stage tree is at a later commit (e.g. Plan 02 has shipped), `git stash --include-untracked` + checkout Plan-01-only HEAD before rebuilding `SwgClient_d.exe` and running the census boot. Document any deviation in this section.

### Aggregated HLSL:asm ratio (populated post-boot)

| Metric | Count | % |
|--------|-------|---|
| Total PS programs loaded at char-select | _TBD_ | 100% |
| HLSL PSRC (`//hlsl` prefix → recompile lane, primary) | _TBD_ | _TBD_ |
| asm PSRC (no `//hlsl` prefix → asm→HLSL port lane, secondary) | _TBD_ | _TBD_ |

**Lane mix decision (gates Plans 02/03):**
- If asm count == 0 across all three anchors → Plan 02 ships the recompile lane only; SR-3 STOP CONDITION not triggered.
- If asm count > 0 in named anchors → Plan 02 STOPS at the first asm anchor and spawns sub-plan 17-02B for the asm-port lane (per SR-3).

### Per-shader census table (populated post-boot)

| fileName | isHlsl | profile | includes | samplers | constants |
|----------|--------|---------|----------|----------|-----------|
| _(aggregated rows from stage/psrc-census.tsv)_ | | | | | |

### Deferred-feed flags (Plan 03 scope gate)

Per RESEARCH Open Question 3: if any char-select PS references scroll/fog/stencil constants (above and beyond material color + textureFactor), flag here for Plan 03 scope expansion.

| Feed | Anchor referencing it | Action |
|------|----------------------|--------|
| _(populated by inspecting the retained PSRC text for the named anchors)_ | | |

---

## Findings (post-boot, bucketed for Phase 17)

*(Populated after Task 3 census boot + matched-pair captures. Mirrors the Phase 12 baseline finding structure.)*

### 1. CHAR-01 — single-stage body/clothing diffuse
_(TBD — single-stage control proves "pipeline works")_

### 2. CHAR-02 — eyes (palette color, seated, occluded — D-07 requires committed matched pair)
_(TBD — multi-stage `sul_eye.sht` `TAG_PPSH`/`numberOfStages==0` path)_

### 3. CHAR-03 — head/face multi-stage composite
_(TBD — `sul_*_head.sht` matched pair)_

### 4. D-08 single-stream vs multi-stream skinning A/B (RenderDoc, CONFIRMATION ONLY)
_(TBD — does NOT block CHAR-01/02/03; informs the Phase 22 GEO-01 fix-vs-flip decision. RenderDoc was not installed at the default path per RESEARCH §Environment Availability.)_
