# Phase 17: PSRC Census + Char-Select Beachhead - Research

**Researched:** 2026-05-27
**Domain:** Legacy game-engine renderer port — D3D9→D3D11 asset pixel-shader pipeline (SWG/whitengold client). C++20, MSVC, VS2022, MSBuild (`src/build/win32/swg.sln`).
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** PSRC census = **engine-instrumented load-path logging** (NOT an offline tool). Add temporary logging at the PSRC-discard site (`ShaderImplementation.cpp:2895-2908`, where `enterChunk(TAG_PSRC); exitChunk(TAG_PSRC,true)` drops the source) to dump, per shader loaded during a **real char-select boot**: name, `//hlsl`-vs-asm classification, target profile, includes, sampler slots, referenced constants.
- **D-02:** Census runs against the asset tree **the client already loads as-is** — `D:/Code/SWGSource Client v3.0/swgsource_3.0.tre` (+ `disable_wayfar_dearic_snow.tre`) and `searchPath D:/swg_dev_bundle` (`stage/client_d.cfg:23-25`). Do NOT pull the SWGSource-vs-retail TRE asset-diff into the gating path.
- **D-03:** Census instrumentation is **kept (not throwaway)** — gated behind a config/log flag so Phase 20 can re-run it on an open-world boot. HLSL:asm ratio recorded as a phase artifact (the lane-mix input).
- **D-04:** Build the **D3DReflect-driven** constant-upload path **generically** (map by reflected name/binding, NOT copied D3D9 register indices c11/c44/c45/c47). But only wire the source-data feeds char-select exercises: **material color + textureFactor** (per census). Defer `textureScroll`/`fog`/`stencil` feeds to Phases 19/21.
- **D-05:** **Hybrid** validation — manual A/B screenshots are the *gate*, but matched `rasterMajor=5`/`=11` pairs are saved into a structured COMPARISON dir + naming convention (mirror `docs/research/phase12-baseline/COMPARISON.md`) so a scripted pixel-diff harness bolts on later without recapturing.
- **D-06:** Hard gates (all must hold): `id=342==0 && id=343==0` in `stage/d3d11-debug.log`; both `rasterMajor=5` and `=11` boot to char-select without crash; **D3D9 reference path unregressed**.
- **D-07:** CHAR-02 (eyes) and any "fixed" claim require a **committed matched A/B pair** before marking done (Iter-44B no-false-pre-claim lesson). "Eyes through back of head" is depth (already fixed Iter-44A, `Direct3d11_StaticShaderData.cpp:658-677`) — verify by a *fresh* A/B screenshot, do NOT re-fold into PS scope.
- **D-08:** Single-stream-vs-multi-stream skinning confirmation is a **manual RenderDoc mesh-viewer A/B capture**, taken *before* attributing residual head/mesh weirdness to the PS. Fix decision deferred to Phase 22.

### Claude's Discretion
- **D-09 (bridge PS — executor's call):** Go **recompile-first** (primary lane: PSRC `//hlsl` → `compilePixelShaderFromHlsl` → bind `m_d3dPS`). Build the minimal multi-sampler diagnostic bridge PS **only if** recompile attribution gets murky. Bridge is a diagnostic tool, not the parity strategy.
- Exact char-select shader set beyond the named anchors (`sul_eye.sht`, `sul_*_head.sht`, + one single-stage body/clothing control) is census-derived — planner/executor pick the single-stage control from what the census shows.
- Census-flag gating mechanism (cfg key vs compile-time define) is executor's choice, as long as it leaves the D3D9 path untouched when off.

### Deferred Ideas (OUT OF SCOPE)
- **SWGSource-vs-retail TRE faithfulness** — censusing against SWGSource 3.0 as-is (D-02); a true retail-TRE reconciliation is NOT in the gating path. Revisit only if the census surfaces clearly SWGSource-modified shaders.
- **Single-stream-skinning fix** — Phase 17 only confirms single vs multi-stream (D-08); fix-vs-flip is Phase 22.
- **Scripted pixel-diff harness** — structurally enabled by D-05's naming convention; actual harness build is Phase 20+.
- **scroll / fog / stencil constant feeds** — Phase 19 (lighting/gamma) and Phase 21 (effects).
- `2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md` (space-scene artifact diffing — informational) and `2026-05-15-cantina-corner-snap-engine-improvement.md` (collision quirk) — both reviewed, not folded.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CHAR-01 | Char skin & clothing render with correct diffuse textures under D3D11 (matches D3D9) — not untextured, flat-white, or magenta. | Primary recompile lane (`compilePixelShaderFromHlsl` at `Direct3d11_PixelShaderProgramData.cpp:86-171`, verified present) binds a real `m_d3dPS` that samples the bound diffuse SRV. SRVs 1..7 already bound (`Direct3d11_StaticShaderData.cpp:711-737`, verified). Single-stage control material proves "pipeline works." |
| CHAR-02 | Char eyes render correctly — correct palette color, seated in face, occluded by head, not gray. | `sul_eye.sht` uses `TAG_PPSH`/`numberOfStages==0` (program path, verified selection logic at `ShaderImplementation.cpp:1592`). Palette color flows via material constants (D-04 reflection-driven upload). Depth occlusion already fixed Iter-44A (`Direct3d11_StaticShaderData.cpp:665-677`, verified) — verify by fresh A/B, do NOT re-fold. |
| CHAR-03 | Head/face multi-stage materials (`sul_*_head.sht`, `sul_eye.sht`) composite texture stages correctly. | Multi-stage compositing lives in the `.psh` PSRC program (NOT `Pass::m_stage` FFP — verified `ShaderImplementation.cpp:1592-1602`). Recompiled PS samples all referenced SRV slots; reflection-driven constants supply per-stage material data. |
</phase_requirements>

## Summary

Phase 17 is the v2.2 milestone beachhead. The entire visual gap between D3D11 (`rasterMajor=11`) and D3D9 (`rasterMajor=5`) traces to one root cause confirmed by direct code read this session: the engine ships `.psh`/`.sht` assets whose pixel-shader programs carry pre-compiled D3D9-era bytecode (`TAG_PEXE`). `ID3D11Device::CreatePixelShader` rejects that bytecode (`Direct3d11_PixelShaderProgramData.cpp:716-756` — explicit "CANNOT pass to CreatePixelShader" comment, `m_d3dPS` left NULL), so D3D11 falls back to a dynamic generated PS (`Direct3d11_PixelShaderProgramData.cpp:339-512`) that samples texture slot 0 / `TEXCOORD0` only, uploads no per-pass material constants, and returns magenta when even slot-0 isn't available. Every named symptom (untextured surfaces, gray eyes, magenta slivers, missing multi-stage composite) is a face of that one gap.

The verified fix path: each `.psh` asset still carries the original shader *source* (`TAG_PSRC`), which the engine loads and immediately discards at `ShaderImplementation.cpp:2900-2901` (`enterChunk(TAG_PSRC); exitChunk(TAG_PSRC, true)` — no read). The PSRC text is a literal string (stored via `IffReadString`/`insertChunkString` in ShaderBuilder, `Node.cpp:5818-5849`) that begins with a `//hlsl <profile>` prefix when it is HLSL, else it is D3D9 assembly (classification is a literal string-prefix check at `PixelShaderProgramView.cpp:308` — `left7 == "//hlsl "`). The fix is to retain that text in shared `clientGraphics` (byte-for-byte D3D9-identical except storing the text), feed it to the already-present-but-unused-for-assets `compilePixelShaderFromHlsl` helper (which already does HlslRewrite + D3DCompile + `.cso` ShaderCache, verified `Direct3d11_PixelShaderProgramData.cpp:86-171`), bind the resulting `m_d3dPS`, and upload per-pass material/textureFactor constants driven by **D3DReflect** on the recompiled PS (the recompiler reallocates registers, so D3D9 register indices are folklore).

The HLSL:asm ratio across the *actual* char-select shader population is the single biggest open unknown and the gating first deliverable. It cannot be answered from this checkout: the char-select anchors (`sul_eye.sht`, `sul_*_head.sht`) live inside packed `.tre`/`.toc` archives, not loose files (verified: `D:/swg_dev_bundle/shader` holds only 2 unrelated `.sht`, no `.psh`; no `sul_*` matches on the filesystem). That packed-archive reality is exactly why D-01 chose engine instrumentation over an offline scan — the engine's TreeFile loader resolves the assets at runtime through the proven `.toc` path.

**Primary recommendation:** Ship the census instrumentation first (gated behind a shared `ConfigFile` flag readable from `clientGraphics`), boot char-select once under D3D11 to record the HLSL:asm ratio + per-shader census, then implement the recompile lane + reflection-driven material/textureFactor upload, validating against `rasterMajor=5` A/B pairs at char-select default pose.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| PSRC census logging | Shared `clientGraphics` (`ShaderImplementation.cpp`) | — | The PSRC text is only visible at the engine load site; the D3D11 plugin never sees it (only `m_exe`). Census must live in shared code, gated so D3D9 path is untouched. |
| PSRC text retention | Shared `clientGraphics` (`ShaderImplementationPassPixelShaderProgram`) | — | The retain edit adds a member (mirror VS `m_text`/`m_textLength`) so the plugin can read source. Shared = boot-gate both renderers. |
| PSRC→`m_d3dPS` recompile | D3D11 plugin (`Direct3d11_PixelShaderProgramData`) | — | Plugin-local; `compilePixelShaderFromHlsl` already exists here. D3D9 keeps consuming `m_exe` unchanged. |
| Reflection-driven constant upload | D3D11 plugin (`Direct3d11_StaticShaderData` + `Direct3d11_ConstantBuffer`) | — | Per-pass `apply()` is the live draw-time hook (`:606-693`); `PerMaterialCB` already flushes to PS slot `b2`. |
| Skinning single/multi-stream confirm | RenderDoc (external) | D3D11 vertex path (read-only this phase) | D-08: confirm only; no code change in Phase 17. |
| A/B screenshot validation | SwgClient + cfg toggle + ScreenShotHelper | `docs/research` COMPARISON dir | Existing Phase 11/12 workflow; renderer chosen via `rasterMajor` in `client_d.cfg`. |

## Standard Stack

This milestone introduces **zero new dependencies** — it wires and extends in-tree infrastructure that Phase 11 already built. "Versions" below are the compile targets / APIs already in use.

### Core
| Component | Version / Target | Purpose | Why Standard |
|-----------|------------------|---------|--------------|
| `D3DCompile` (`d3dcompiler_47`) | `ps_4_0` profile | Compile asset PSRC `//hlsl` → DXBC | Already the engine VS compile path (`compilePixelShaderFromHlsl`); the PS path mirrors it. `[VERIFIED: Direct3d11_PixelShaderProgramData.cpp:86-171]` |
| `D3DReflect` / `ID3D11ShaderReflection` | d3dcompiler_47 | Read recompiled-PS cbuffer/resource bindings → drive constant upload by reflected name | Already used for VS input/output signatures (`Direct3d11_VertexShaderData.cpp:586-673`). `[VERIFIED]` Extend to PS cbuffer layout (NOT yet used for PS — the new work). |
| `Direct3d11_HlslRewrite` (Rules A–E) | in-tree | Clear D3D9-era HLSL idioms for SM4 | Same rewriter the VS path uses; `compilePixelShaderFromHlsl` already calls `applyToMainSource`. `[VERIFIED: Direct3d11_PixelShaderProgramData.cpp:169-171]` |
| `Direct3d11_ShaderCache` (FNV-1a + `.cso`) | in-tree | Cache compiled PS on disk; avoid per-boot recompile | Already keyed on source + defines incl. `D3D11_PROFILE` + `D3D11_REWRITE_VERSION` (currently `"20"`). `[VERIFIED: Direct3d11_PixelShaderProgramData.cpp:151-158]` |
| `Direct3d11_ConstantBuffer::updatePS(slot,...)` | in-tree | Upload per-pass cbuffer (Map WRITE_DISCARD → `PSSetConstantBuffers`) | `PerMaterialCB` already wired to PS slot `b2`. `[VERIFIED: Direct3d11_ConstantBuffer.cpp:174-214; Direct3d11.cpp:682-693]` |
| RenderDoc | external (NOT at default install path this machine) | Mesh-viewer A/B for single/multi-stream skinning (D-08); per-draw SRV/cbuffer/PS inspection | Standard D3D11 capture tool. **Not found** at `C:/Program Files/RenderDoc` — see Environment Availability. `[VERIFIED: not installed]` |
| `ID3D11InfoQueue` → `stage/d3d11-debug.log` | in-tree | Diagnostics channel (compile failures, id=342/343 linkage, census output observable under explorer launch) | Established safe channel; `DEBUG_REPORT_LOG_PRINT` is invisible under explorer launch. `[VERIFIED: Direct3d11_PixelShaderProgramData.cpp:515-519]` |

### Supporting
| Component | Purpose | When to Use |
|-----------|---------|-------------|
| `tools/swg_blender/swg_pipeline/tre_reader.py` | Offline TRE payload read (`read_tre_payload`, `list_tre`) | OPTIONAL offline cross-check of the engine census only if a discrepancy surfaces. NOT the census mechanism (D-01 picked engine instrumentation). `[VERIFIED: tre_reader.py:232-236]` |
| `ConfigDirect3d11` (`[Direct3d11]` cfg section) | Plugin-side config flags via `KEY_BOOL`/`KEY_INT` | Plugin-local toggles (e.g. recompile-on/off). NOT usable from shared `clientGraphics`. `[VERIFIED: ConfigDirect3d11.cpp:34-38]` |
| Shared `ConfigFile::getKeyBool(section,key,default)` | Census gate flag readable from shared code | The census flag MUST use shared `ConfigFile` (e.g. `[ClientGraphics]` or `[SharedDebug]` key), because the census lives in `clientGraphics` which cannot depend on the D3D11 plugin's `ConfigDirect3d11`. `[VERIFIED: pattern in ConfigDirect3d11.cpp]` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Engine-instrumented census (D-01) | Offline `tre_reader.py` scan of `.psh` in TREs | Rejected by D-01: offline scan can't deterministically isolate the *char-select working set* and duplicates the runtime TOC resolution. Keep as a fallback cross-check only. `[CITED: CONTEXT.md D-01]` |
| Recompile PSRC `//hlsl` | Decompile rejected PEXE bytecode / runtime D3D9-asm interpreter | Rejected (REQUIREMENTS.md Out of Scope): PSRC source still exists; bytecode archaeology is unnecessary. `[CITED: REQUIREMENTS.md:55-56]` |
| Port asm PSRC → HLSL → ps_4_0 (secondary lane) | Re-assemble asm via `D3DXAssembleShader` | Rejected (named landmine): reproduces the exact D3D9 bytecode `CreatePixelShader` already rejects. `[CITED: SYNTHESIS.md §A; REQUIREMENTS.md:57]` |
| FFP `TextureOperation` generator as primary VSPS lane | — | Rejected: VSPS passes pick program OR FFP, not both; `sul_eye.sht` (`numberOfStages==0`) uses the `.psh` program path. FFP generator is tertiary/narrow only. `[VERIFIED: ShaderImplementation.cpp:1592-1602]` |
| `ps_4_0` profile | `ps_5_0` / `ps_*_level_9_x` / DXC SM6 | Rejected: `ps_5_0` has reserved-keyword conflicts; `*_level_9_x` re-imposes the SM2 instruction cap that broke a VS; DXC/SM6 is needless scope for SM2-era assets. `[CITED: SUMMARY.md Recommended Stack]` |

**Installation:** None — all in-tree. Build via existing `src/build/win32/swg.sln` (MSBuild, full path; `/nodeReuse:false`; run from PowerShell; delete `SwgClient_d.exe` to force relink; `/FORCE` masks unresolved externals so grep the link output for `unresolved external symbol`).

**Version verification:** Not applicable — no npm/package registry. The compile profile is `kPixelShaderProfile` (resolves to `ps_4_0`, mirroring the VS path); cache version macro `D3D11_REWRITE_VERSION` currently `"20"` (`[VERIFIED: Direct3d11_PixelShaderProgramData.cpp:151]`) — bump it on any PSRC-compile generator/rewrite change so stale `.cso` caches invalidate.

## Architecture Patterns

### System Architecture Diagram

```
.psh / .sht asset (packed in swgsource_3.0.tre, resolved via .toc)
        │
        ▼
ShaderImplementationPassPixelShaderProgram::load_0000(iff)   [clientGraphics, SHARED]
   ├─ enterChunk(TAG_PSRC) ─┐
   │                        ├──► (TODAY) exitChunk(TAG_PSRC, true)  ── source DISCARDED  ✗
   │                        └──► (CENSUS, D-01) read string → classify //hlsl|asm,
   │                                profile, #includes, sampler slots, constants → log
   │                        └──► (RETAIN edit) store text in new m_psrcText/m_psrcLen
   └─ enterChunk(TAG_PEXE) ──► m_exe = D3D9 bytecode  (D3D9 path consumes this unchanged)
        │
        ├──────────────► D3D9 plugin: Direct3d9_PixelShaderProgramData(m_exe) ── works ✓ (UNREGRESSED)
        │
        ▼
D3D11 plugin: Direct3d11_PixelShaderProgramData(program)        [Direct3d11, plugin]
   ├─ (TODAY) looksLikeDxbc(m_exe)==false → m_d3dPS = NULL  → dynamic slot-0/magenta PS  ✗
   └─ (FIX) if program.m_psrcText starts "//hlsl":
              compilePixelShaderFromHlsl(psrcText) ─► HlslRewrite ─► D3DCompile(ps_4_0)
                 ─► ShaderCache(.cso, keyed on src+profile+REWRITE_VERSION) ─► m_d3dPS  ✓
              else (asm PSRC): port asm→HLSL→D3DCompile (secondary lane) ─► m_d3dPS
              else: keep magenta fallback as visible tombstone
        │
        ▼
Per-draw: Direct3d11_StaticShaderData::apply(passNumber)        [Direct3d11, plugin]
   ├─ setCurrentVSData / setCurrentPSData (binds m_d3dPS)            [:613-617, live hook]
   ├─ per-pass depth/alpha-test/blend-enable/colorwrite (Iter-44A/B/E)  [:639-684, present]
   ├─ SRV slots 0..7 + samplers bound (Iter-44E)                    [:711-737, present]
   └─ (FIX, D-04) reflection-driven material/textureFactor upload   [:601-605 deferral site]
              D3DReflect(m_d3dPS) → map cbuffer fields by NAME ─► updatePS(slot, PerMaterialCB)
        │
        ▼
applyPreDrawState → PSSetShaderResources/Samplers/ConstantBuffers (full-array flush)
        │
        ▼
Char-select avatar renders textured + eyes + multi-stage head  → A/B vs rasterMajor=5
```

### Recommended Project Structure
No new directories. Touch points (all existing files):
```
src/engine/client/library/clientGraphics/src/shared/
├── ShaderImplementation.cpp   # :2895-2911 census + PSRC-retain (SHARED — boot-gate both)
└── ShaderImplementation.h     # :650-682 add m_psrcText/m_psrcLen to PixelShaderProgram (mirror VS m_text @ :427)

src/engine/client/application/Direct3d11/src/win32/
├── Direct3d11_PixelShaderProgramData.cpp  # :716-756 ctor: feed PSRC → compilePixelShaderFromHlsl → bind m_d3dPS
├── Direct3d11_StaticShaderData.cpp        # :601-605 deferral: add reflection-driven material/textureFactor upload
├── Direct3d11_ConstantBuffer.h            # :58-66 PerMaterialCB (populate materialDiffuse etc., currently zero)
└── (reflection helper)                    # mirror Direct3d11_VertexShaderData.cpp:586-673 D3DReflect pattern for PS

stage/client_d.cfg            # census gate flag (shared ConfigFile section) + rasterMajor toggle for A/B
docs/research/<char-select-COMPARISON-dir>/   # D-05 matched-pair screenshots + COMPARISON.md
```

### Pattern 1: PSRC source retention (mirror the VS `m_text` precedent)
**What:** The VS already retains its source as `m_text`/`m_textLength` (`ShaderImplementation.h:427`). The PS program struct has only `m_exe` (`ShaderImplementation.h:680`). Add `m_psrcText`/`m_psrcLen` and populate from the `TAG_PSRC` chunk.
**When to use:** The shared-code retain edit (the single shared `clientGraphics` touch).
**Critical constraint:** Keep D3D9 `load_0000` byte-for-byte behaviorally identical except *storing* the text. The PSRC is read as an IFF string (ShaderBuilder writes it via `insertChunkString`).
```cpp
// Source: VERIFIED current code — ShaderImplementation.cpp:2895-2911 (the discard site to edit)
void ShaderImplementationPassPixelShaderProgram::load_0000(Iff &iff)
{
    iff.enterForm(TAG_0000);
        iff.enterChunk(TAG_PSRC);
        iff.exitChunk(TAG_PSRC, true);   // <-- TODAY: source dropped. Retain edit reads it here.
        iff.enterChunk(TAG_PEXE);
            const int exeLength = iff.getChunkLengthLeft(sizeof(DWORD));
            m_exe = new DWORD[exeLength];
            iff.read_uint32(exeLength, m_exe);
        iff.exitChunk(TAG_PEXE);
    iff.exitForm(TAG_0000);
}
// ShaderBuilder writes PSRC as a string (Node.cpp:5818-5820 reads via IffReadString):
//   iff.enterChunk(TAG_PSRC); IffReadString(iff, m_text); iff.exitChunk(TAG_PSRC);
```

### Pattern 2: `//hlsl` vs asm classification (literal prefix check)
**What:** PSRC text classification is a literal first-line prefix check. The profile follows the prefix.
**When to use:** Census (D-01) classification AND the recompile-vs-port lane branch in the plugin ctor.
```cpp
// Source: VERIFIED — ShaderBuilder PixelShaderProgramView.cpp:304-313
//   CString left7 = line.Left(7); left7.MakeLower();
//   if (left7 == "//hlsl ") { target = line.Mid(7); ... }   // e.g. "//hlsl ps_2_0" -> profile "ps_2_0"
//   else  -> D3DXAssembleShader (asm)  [:172]
// Census rule: trim leading whitespace, lowercase first 6-7 chars:
//   starts-with "//hlsl"  -> HLSL,  profile = token after "//hlsl "
//   else                  -> asm
```

### Pattern 3: D3DReflect-driven constant upload (mirror the VS reflection, NOT register folklore)
**What:** `HlslRewrite` + `D3DCompile` reallocate registers; the recompiled PS may not place material at c11/c44/c45/c47 where the D3D9 shader did. Reflect the recompiled PS and upload by reflected cbuffer-variable *name*/binding.
**When to use:** The D-04 deferral site (`Direct3d11_StaticShaderData.cpp:601-605`).
```cpp
// Source: VERIFIED PATTERN — Direct3d11_VertexShaderData.cpp:611-673 (VS uses D3DReflect for I/O sig).
// Extend to PS cbuffers/resources:
ComPtr<ID3D11ShaderReflection> reflector;
D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&reflector));
// For each material/textureFactor source the census shows referenced:
//   reflector->GetConstantBufferByName("PerMaterial") -> GetVariableByName(...)->GetDesc(&vd)
//   reflector->GetResourceBindingDesc(...) for sampler/SRV slot mapping
// Then: build PerMaterialCB by reflected layout, upload via updatePS(slot, &cb, sizeof(cb)).
// PerMaterialCB already flushes to PS slot b2 (Direct3d11.cpp:693).
```

### Pattern 4: VSPS pass selection — program OR FFP (never both)
**What:** A pass with `numberOfStages == 0` and a `TAG_PPSH` child uses a pixel-shader program; otherwise it uses FFP `Pass::m_stage` TextureOperation stages. This is mutually exclusive.
**Why it matters:** Confirms `sul_eye.sht`-style multi-stage compositing is in the `.psh` PROGRAM (recompile lane), not in `Pass::m_stage` (FFP generator). The FFP generator is tertiary/narrow only.
```cpp
// Source: VERIFIED — ShaderImplementation.cpp:1592-1602
if (numberOfStages == 0 && !iff.atEndOfForm() && iff.getCurrentName() == TAG_PPSH)
    m_pixelShader = new PixelShader(iff);       // <-- char-select head/eye path
else { m_stage = new Stages; /* ... push Stage(iff) ... */ }   // <-- FFP path
```

### Anti-Patterns to Avoid
- **D3D9 register folklore:** Do NOT copy D3D9 c11/c44/c45/c47 register indices into D3D11. The recompiler reassigns them; drive from D3DReflect. (Ties to proven id=342/343 pitfalls.)
- **Re-assembling asm PSRC:** `D3DXAssembleShader` reproduces the rejected D3D9 bytecode. Asm lane ports asm→HLSL→`ps_4_0`.
- **`_SRGB` SRV flip:** D3D9 samples sRGB OFF (`Direct3d9_StateCache.cpp:193`). Keep SRVs `_UNORM`.
- **Re-enabling per-pass blend factors early:** Iter-44C amplification regression — wait until the real PS samples all bound SRVs.
- **Removing the magenta fallback:** Keep it as a visible diagnostic tombstone.
- **Partial cbuffer writes:** `Map(WRITE_DISCARD)` leaves the rest undefined — write the full struct.
- **Editing the dead `Direct3d11_ShaderImplementationData::apply()`:** The live per-draw hook is `Direct3d11_StaticShaderData::apply(passNumber)` (`:606+`). Iter-39B already learned this.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| HLSL→DXBC compile | A custom compiler / asm assembler | `compilePixelShaderFromHlsl` (already present, `:86-171`) | Already does rewrite + D3DCompile(ps_4_0) + cache + error logging. Just feed it PSRC text. |
| Constant register mapping | Copy D3D9 register layout | `D3DReflect` by name (mirror `Direct3d11_VertexShaderData.cpp:611-673`) | Recompiler reassigns registers; copied indices cause id=342/343. |
| Per-boot recompilation | Recompile every shader every boot | `Direct3d11_ShaderCache` (FNV-1a + `.cso`) | Already keyed; just bump `D3D11_REWRITE_VERSION` on generator change. |
| cbuffer upload | Direct `PSSetConstantBuffers` from StaticShaderData | `Direct3d11_ConstantBuffer::updatePS(slot,...)` + the lazy StateCache flush | CODEX Q1: never call `PSSet*` directly from StaticShaderData; preserve the lazy-bind model. |
| Offline asset census | Filesystem `.psh` scan | Engine instrumentation at the load site (D-01) | Char-select anchors are packed in TREs (verified: not loose files); engine resolves via `.toc`. |
| Screenshot capture | New capture code | Existing `ScreenShotHelper::screenShot` (bound via DirectInput) | Phase 11/12 already use it; just toggle `rasterMajor` in `client_d.cfg` between captures. |

**Key insight:** Phase 11 already built the entire VS compile/reflect/cache toolchain and the `compilePixelShaderFromHlsl` helper. Phase 17 is wiring (feed PSRC text in, bind `m_d3dPS`, reflect-upload constants), not new infrastructure. The single shared-code edit (PSRC retain) is the only `clientGraphics` touch; everything else is plugin-local.

## Runtime State Inventory

> Phase 17 is a compile-path + render-pipeline change, NOT a rename/refactor/migration. This section is included only to discharge the shader-cache concern, which behaves like cached runtime state.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — no datastores keyed on phase strings. Verified: phase touches only in-memory shader objects + on-disk `.cso` cache. | None |
| Live service config | None — no external services. | None |
| OS-registered state | None. | None |
| Secrets/env vars | None. | None |
| Build artifacts / cached state | **`.cso` ShaderCache entries** keyed on `(source + defines incl. D3D11_PROFILE + D3D11_REWRITE_VERSION)`. Feeding asset PSRC into the PS compile path is NEW input the existing cache will key correctly ONLY if the rewrite/profile version is current. Stale `.cso` from prior boots could mislead debugging. `[VERIFIED: Direct3d11_PixelShaderProgramData.cpp:151-158]` | **Bump `D3D11_REWRITE_VERSION` on any PSRC-compile generator/rewrite change** so stale caches invalidate. Optionally clear the `.cso` cache dir when first landing the recompile lane. |

**The canonical question (cache flavor):** After the recompile lane lands, is any stale `.cso` from a prior generator version still on disk being loaded as a false success? Bump the version macro; the cache key handles the rest.

## Common Pitfalls

### Pitfall 1: PS-VS input linkage mismatch (id=342 / id=343)
**What goes wrong:** `id=342` = D3D11 debug-layer "VS-PS linkage error: PS input requires [a VS output not present]"; `id=343` = SRV0 null when a textured PS is bound. `[VERIFIED: Direct3d11_PixelShaderProgramData.cpp:572-629]`
**Why it happens:** The recompiled PS input signature must be a strict subset of the bound VS output signature, matched by semantic AND register position. A prior iteration produced 65,034 id=343 errors per session from register-position mismatch.
**How to avoid:** Keep reflection-driven input-struct generation (the existing `buildHlslForVSOutputs` walks reflected outputs sorted by Register). Gate every change on `id=342==0 && id=343==0` in `stage/d3d11-debug.log` (D-06 hard gate).
**Warning signs:** Magenta where a textured surface should be; per-frame id=343 flood in the debug log.

### Pitfall 2: Claiming a visual win without a matched A/B screenshot
**What goes wrong:** Iter-44B falsely pre-claimed the minimap was round; the false win propagated through a full plan cycle. The minimap was never round in D3D11. `[CITED: project_phase11_minimap_never_round; SUMMARY.md Pitfall 2]`
**Why it happens:** "No magenta" or "looks plausible" is not parity. Success = matches `rasterMajor=5`.
**How to avoid:** D-07 — CHAR-02 (eyes) and any "fixed" claim require a **committed matched A/B pair** before marking done. Save into the COMPARISON dir with the D-05 naming convention.
**Warning signs:** A done-claim with no committed screenshot pair.

### Pitfall 3: Re-enabling per-pass blend factors before the real PS samples all SRVs
**What goes wrong:** Iter-44C wired blend factors while the PS still ignored slots 1..7; it amplified unsampled-slot content into a whitening regression (reverted twice). `[VERIFIED: Direct3d11_StaticShaderData.cpp:686-689 REVERTED comment; CITED: SUMMARY.md Pitfall 4]`
**How to avoid:** Do NOT re-enable per-pass blend source/dest/op until the recompiled PS samples all bound SRVs and uploads material constants. The existing per-pass `m_alphaBlendEnable` toggle (Iter-39C) stays; the blend *factors* stay deferred.
**Warning signs:** Brightening/whitening on multi-stage materials after a blend-factor change.

### Pitfall 4: Mistaking the cbuffer transpose quirk
**What goes wrong:** D3D11 cbuffer matrices must be `XMMatrixTranspose`d at upload (col-vec engine vs row-vec bytecode). `[CITED: d3d11_cbuffer_transpose_quirk memory]`
**How to avoid:** Any NEW cbuffer matrix constant added for the material path needs `XMMatrixTranspose`. (Material color/textureFactor for char-select are float4 vectors, not matrices — but if a texture-transform matrix is later added, transpose it.)
**Warning signs:** Geometry/UV mirrored or skewed after a new matrix constant.

### Pitfall 5: "Textures appear" ≠ "pipeline complete"
**What goes wrong:** When the single-stage control material textures correctly, eyes can still be gray (multi-stage), head composite still wrong, and you may declare victory. `[CITED: SUMMARY.md Pitfall 6]`
**How to avoid:** Track CHAR-01 (single-stage control), CHAR-02 (eyes), CHAR-03 (multi-stage head) as separate gated items, each with its own A/B pair. The single-stage control proves "pipeline works"; the eye/head anchors prove "multi-sampler works."
**Warning signs:** A single-textured-surface screenshot used to claim all three CHAR reqs.

### Pitfall 6: Regressing the shared D3D9 path
**What goes wrong:** The PSRC-retain edit is in shared `clientGraphics`. A change to `load_0000`'s read sequence or ownership can break D3D9 (which loads the same assets). `[CITED: CONTEXT.md D-06; SYNTHESIS.md Landmines]`
**How to avoid:** Keep D3D9 byte-for-byte behaviorally identical except storing text. Watch ctor/reload/destructor ownership of the new member (free it like `m_exe`). Boot-test BOTH `rasterMajor=5` AND `=11` on every shared edit (set in `client_d.cfg` — debug exe reads `client_d.cfg`, not `client.cfg`).
**Warning signs:** D3D9 boot crash or visual change after the retain edit; leaked PSRC text at shutdown.

### Pitfall 7: Misattributing skeletal-mesh distortion to the PS
**What goes wrong:** Residual head/mesh "shard" weirdness gets blamed on the new PS when it is actually the single-stream skeletal skinning path. `[CITED: SUMMARY.md Pitfall 5; project_d3d11_collide_use_after_free]`
**How to avoid:** D-08 — take a RenderDoc mesh-viewer A/B BEFORE attributing residual weirdness to the PS. The crash fix (`905fb5d64`) used defensive guards, not a multi-stream flip, so draw-side distortion may persist independent of the PS. Fix decision is Phase 22.
**Warning signs:** Spiky/stretched mesh that doesn't change when the PS is swapped to magenta or to a known-good shader.

## Code Examples

### Census log record (D-01) — minimal per-shader data structure
```cpp
// Source: derived from VERIFIED load site (ShaderImplementation.cpp:2895-2911) +
// classification rule (PixelShaderProgramView.cpp:304-313). Gated behind a shared
// ConfigFile flag so the D3D9 path is untouched when off.
// Per loaded PS program, emit one line to the census log:
struct PsrcCensusRecord {
    const char* fileName;     // pixelShaderProgram.getFileName()
    bool        isHlsl;       // first non-ws line starts with "//hlsl"
    const char* targetProfile;// token after "//hlsl " (e.g. "ps_2_0") or "asm"
    // From a light scan of the retained PSRC text:
    int         includeCount; // count of '#include' lines
    // sampler slots referenced: scan for register(s0..sN) / sampler decls
    // constants referenced: scan for register(c0..cN) / named cbuffer vars
};
// Aggregate at end-of-boot: total, hlslCount, asmCount -> HLSL:asm ratio (the lane-mix input).
```

### Recompile-lane branch in the plugin ctor (D-09 recompile-first)
```cpp
// Source: integrates VERIFIED compilePixelShaderFromHlsl (:86-171) into the
// VERIFIED rejection ctor (:716-756). m_psrcText/m_psrcLen come from the retain edit.
Direct3d11_PixelShaderProgramData::Direct3d11_PixelShaderProgramData(
    ShaderImplementationPassPixelShaderProgram const &p)
:   m_program(&p), m_d3dPS()
{
    char const* src = p.m_psrcText;          // NEW (retain edit)
    size_t      len = p.m_psrcLen;           // NEW
    if (src && len && startsWithHlsl(src))   // primary lane
    {
        if (compilePixelShaderFromHlsl(src, len, p.getFileName(), m_d3dPS))
            return;                           // bound a real PS
        // compile failed -> fall through to tombstone (magenta) + log
    }
    // else: asm PSRC -> secondary lane (port asm->HLSL->ps_4_0) when census shows asm.
    // m_exe (D3D9 bytecode) is intentionally NOT passed to CreatePixelShader (E_INVALIDARG).
}
```

## State of the Art

| Old Approach (D3D11 today) | Current Approach (Phase 17) | When Changed | Impact |
|----------------------------|-----------------------------|--------------|--------|
| PSRC discarded; PEXE rejected; dynamic slot-0/magenta PS | Retain PSRC; recompile `//hlsl` → `ps_4_0`; bind real PS | Phase 17 | Asset textures + eyes + multi-stage head render |
| No per-pass material constants on D3D11 (`PerMaterialCB` fields zero) | Reflection-driven material/textureFactor upload | Phase 17 (D-04) | Correct material color; CHAR-02 palette eye color |
| Census unknown (HLSL:asm ratio unmeasured) | Engine-instrumented boot-log census | Phase 17 (D-01) | Lane mix becomes a measured number, gating the rest |

**Already done (do NOT redo):**
- Per-pass depth state (Iter-44A, `:665-677`) — "eyes through head" fix. Verify by A/B, don't re-fold.
- Per-pass alpha-test (Iter-44B, `:641-663`), alpha-blend enable (Iter-39C, `:619-639`).
- SRV slots 0..7 bound (Iter-44E, `:711-737`) — binding proven; sampling+constants are the new variable.
- `PerMaterialCB` flush to PS slot `b2` (Iter-45, `Direct3d11.cpp:682-693`) — userConstants flow; material color fields are the zero-filled gap to populate.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The char-select working set is dominated by HLSL PSRC (cheap recompile lane), not asm. | Summary / lane mix | If asm-heavy, more secondary-lane asm→HLSL porting than estimated. **Mitigated by D-01: the census measures this before fix work — do not pre-assume the ratio.** |
| A2 | `sul_eye.sht` / `sul_*_head.sht` exist in the loaded asset set under those exact names. | Phase Requirements | If renamed/absent in SWGSource 3.0, the census will surface the actual names; planner picks anchors from census output (D-09 allows this). Could NOT verify names — assets are packed in TREs, not loose. |
| A3 | The recompiled `ps_4_0` PS will satisfy the existing VS output signatures without new HlslRewrite rules. | Pattern 3 / Pitfall 1 | If asset PSRC uses idioms HlslRewrite doesn't cover, compile fails → magenta tombstone (safe) + a new rewrite rule needed. Recompile-first (D-09) surfaces this early. |
| A4 | Material color + textureFactor are the only constant feeds char-select exercises. | D-04 scope | If a char-select shader references scroll/fog/stencil, those constants read zero → subtle wrong shading. Census's "constants referenced" field catches this; planner can expand the wired feed set if the census shows it. |
| A5 | RenderDoc can be installed for the D-08 skinning A/B. | Environment Availability | RenderDoc NOT currently installed (verified). If it can't be installed, fall back to the engine's own mesh/vertex diagnostics or PIX. Blocks only the D-08 *confirmation*, not the CHAR fixes. |

## Open Questions

1. **HLSL:asm ratio across the char-select PSRC population**
   - What we know: classification is a literal `//hlsl ` prefix check; the census mechanism (D-01) is specified and the load site is verified.
   - What's unclear: the actual ratio — cannot be read from this checkout (assets packed in TREs).
   - Recommendation: The census IS the answer. It is the gating first task; record the ratio as a phase artifact before any fix work (D-03).

2. **Exact char-select shader set beyond the three named anchors**
   - What we know: `sul_eye.sht` (program path), `sul_*_head.sht`, + one single-stage body/clothing control.
   - What's unclear: the full list and which single-stage material to use as the control.
   - Recommendation: Derive from the census's "shaders loaded during char-select boot" output (D-09 lets the executor pick the control).

3. **Which constant feeds char-select actually references**
   - What we know: D-04 wires material color + textureFactor; defers scroll/fog/stencil.
   - What's unclear: whether any char-select shader references a deferred feed.
   - Recommendation: Add a "constants referenced" column to the census; if a deferred feed appears, the planner expands the wired set (the reflection plumbing is generic, so this is additive).

4. **RenderDoc availability for D-08**
   - What we know: not installed at default path; D-08 needs mesh-viewer A/B.
   - Recommendation: Install RenderDoc (free) before the skinning-confirmation task, OR fall back to engine vertex diagnostics. Confirmation only — does not block CHAR fixes.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Asset tree (`swgsource_3.0.tre`, `.toc`, `disable_wayfar_dearic_snow.tre`) | D-01 census, all CHAR validation | ✓ | SWGSource 3.0 (Oct 2017 TREs at `D:/Code/SWGSource Client v3.0/`) | — |
| `searchPath D:/swg_dev_bundle` overlay | asset resolution | ✓ | present (`shader/` has 2 loose `.sht`, no `.psh`) | — |
| `SwgClient_d.exe` (debug, reads `client_d.cfg`) | A/B boots, census run | ✓ | built (in `stage/`) | — |
| MSBuild / `swg.sln` / VS2022 | build | ✓ | (msbuild NOT on PATH — use full path; PowerShell; `/nodeReuse:false`) | — |
| `d3dcompiler_47` (D3DCompile/D3DReflect) | recompile + reflection | ✓ | linked by Phase 11 D3D11 plugin | — |
| `ScreenShotHelper` + DirectInput screenshot key | D-05 A/B capture | ✓ | in-tree (`ScreenShotHelper.cpp`) | — |
| RenderDoc / `qrenderdoc.exe` | D-08 skinning mesh-viewer A/B | ✗ | not found at default paths | Install (free); or engine vertex diagnostics / PIX. Confirmation-only — not a CHAR blocker. |

**Missing dependencies with no fallback:** None.

**Missing dependencies with fallback:** RenderDoc (D-08 single/multi-stream confirmation) — install before the skinning-confirm task or use engine diagnostics. Does not block CHAR-01/02/03.

## Validation Architecture

> `workflow.nyquist_validation: true` in config — this section is included. This is a C++ game-renderer port; the dominant validation signal is **D3D11-vs-D3D9 screenshot diff + boot gates + debug-log assertions**, NOT a unit-test framework. There is no automated unit-test harness for the renderer; the "test framework" is the build-boot-capture loop plus debug-layer log assertions.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None for the renderer (no GTest/Catch wired for `clientGraphics`/Direct3d11). Validation = build + dual-renderer boot + screenshot A/B + `d3d11-debug.log` assertions. Python `pytest` exists for `tools/swg_blender` only (irrelevant to this phase). |
| Config file | none — see Wave 0 |
| Quick run command (per task) | Build (full-path msbuild, `/nodeReuse:false`, from PowerShell) → delete `stage/SwgClient_d.exe` to force relink → boot `rasterMajor=11` to char-select → grep `stage/d3d11-debug.log` for `id=342`/`id=343` (must be 0) |
| Full suite command (per gate) | Boot BOTH `rasterMajor=5` and `=11` to char-select (no crash) → capture matched A/B pair at default pose → visual diff vs `rasterMajor=5` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | Evidence Exists? |
|--------|----------|-----------|-------------------|-----------------|
| (gate) Census | HLSL:asm ratio recorded | manual+log | Boot `rasterMajor=11` char-select with census flag on → census log written → ratio summarized | ❌ Wave 0 (instrument + flag) |
| CHAR-01 | Skin/clothing diffuse textured | screenshot A/B | Matched `=5`/`=11` pair at default pose; single-stage control material textured, not white/magenta | ❌ Wave 0 (capture dir) |
| CHAR-02 | Eyes correct color, seated, occluded | screenshot A/B (committed pair, D-07) | Matched pair; eye palette color matches; not gray; not through-skull (fresh A/B verifying Iter-44A) | ❌ Wave 0 |
| CHAR-03 | Head/face multi-stage composite | screenshot A/B | Matched pair; `sul_*_head.sht`/`sul_eye.sht` composite matches `=5` | ❌ Wave 0 |
| (gate) Linkage | No VS-PS linkage / null-SRV errors | log assertion | `grep -c "id=342\|id=343" stage/d3d11-debug.log` → 0 (D-06) | ✓ log channel exists |
| (gate) Boot | Both renderers boot to char-select | smoke | Boot `=5` and `=11`; neither crashes (D-06) | ✓ both built |
| (confirm) Skinning | Single vs multi-stream identified | RenderDoc A/B (D-08) | Mesh-viewer capture `=5` vs `=11`; record stream count | ❌ RenderDoc not installed |

### Sampling Rate
- **Per task commit:** Build + boot `rasterMajor=11` to char-select; `id=342==0 && id=343==0`; no boot crash.
- **Per wave merge:** Boot BOTH `=5` and `=11`; capture a matched A/B pair for whatever CHAR req the wave advanced.
- **Phase gate:** All three CHAR reqs have committed matched A/B pairs (D-07); `id=342==0 && id=343==0`; both renderers boot; D3D9 unregressed; census ratio recorded as artifact.

### Wave 0 Gaps
- [ ] Census instrumentation at `ShaderImplementation.cpp:2900-2901` + a shared `ConfigFile` gate flag (so D3D9 path is untouched when off) — produces the gating HLSL:asm ratio artifact.
- [ ] PSRC-retain member (`m_psrcText`/`m_psrcLen`) on `ShaderImplementationPassPixelShaderProgram` (`ShaderImplementation.h:680`), mirroring VS `m_text` (`:427`), with matching ctor/reload/destructor ownership.
- [ ] A char-select COMPARISON dir + naming convention (mirror `docs/research/phase12-baseline/COMPARISON.md`: `{anchor}_{renderer}_{shot}.jpg`) for D-05 matched pairs.
- [ ] RenderDoc install (or chosen fallback) for the D-08 skinning confirmation.
- [ ] (No unit-test framework to stand up — renderer validation is build-boot-capture + log assertions.)

## Security Domain

> `security_enforcement: true`, ASVS level 1. This phase is a local, offline, single-player game-client renderer change. There is no network surface, no auth, no user accounts, no persistence of user data, and no untrusted external input in the phase's changes. The only "input" is game asset files (`.psh`/`.sht`) the client already loads under both renderers today; Phase 17 reads an additional chunk (`TAG_PSRC`) that the shipping ShaderBuilder already wrote into the same assets. ASVS web/app-security categories do not map to this work.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No auth surface in this phase. |
| V3 Session Management | no | No sessions. |
| V4 Access Control | no | Local single-player client. |
| V5 Input Validation | partial | The PSRC text is parsed/compiled. Treat it as already-trusted asset data (same trust level as the PEXE the engine already loads), but apply defensive bounds: validate `m_psrcLen` (use `getChunkLengthLeft`), null-terminate the buffer, and rely on `D3DCompile` to reject malformed HLSL (compile failure → magenta tombstone, never a crash). `isValidHlslIdentifier` guards already exist in the generator (`:344`). |
| V6 Cryptography | no | No crypto in this phase. |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Malformed/oversized PSRC chunk → buffer overrun on read | Tampering / DoS | Read length via `iff.getChunkLengthLeft`, allocate exactly, null-terminate (mirror VS `m_text` at `:2311-2313`); free in destructor. |
| PSRC compile crash on bad source | DoS | `D3DCompile` returns failure (not crash); on failure leave `m_d3dPS` null → magenta tombstone. Never assert-fatal on a compile failure. |
| Stale `.cso` cache poisoning debug sessions | (integrity of diagnostics) | Bump `D3D11_REWRITE_VERSION` on generator change so the cache key invalidates. |
| Shared-code edit regressing D3D9 | (availability of reference path) | D-06 dual-renderer boot gate; D3D9 byte-for-byte behavioral identity. |

## Sources

### Primary (HIGH confidence — read directly this session)
- `ShaderImplementation.cpp:2895-2911` — PSRC discard site (`enterChunk/exitChunk(...,true)`); PEXE retained to `m_exe`. VERIFIED.
- `ShaderImplementation.cpp:1592-1602` — VSPS pass selection: program (`TAG_PPSH`, `numberOfStages==0`) OR FFP stages, never both. VERIFIED.
- `ShaderImplementation.cpp:2305-2319` — VS `m_text` retain precedent (`TreeFile::open`). VERIFIED (mechanism differs: VS opens by filename, PS reads from IFF chunk).
- `ShaderImplementation.h:427` (VS `m_text`/`m_textLength`), `:680` (PS program has only `m_exe`). VERIFIED.
- `Direct3d11_PixelShaderProgramData.cpp:86-171` (`compilePixelShaderFromHlsl`, present, routes via fallback), `:260-274` (`looksLikeDxbc`), `:330-512` (dynamic slot-0/COLOR0/magenta PS generator), `:572-629` (id=342/343 history), `:716-756` (PEXE-rejection ctor, `m_d3dPS` NULL). VERIFIED.
- `Direct3d11_StaticShaderData.cpp:590-693` (per-pass deferral comment + alpha-blend/alpha-test/depth/colorwrite Iter-39C/44A/44B/44C-reverted), `:711-737` (Iter-44E SRV slots 0..7 binding). VERIFIED.
- `Direct3d11_ConstantBuffer.h:50-84` (PerObjectCB/PerMaterialCB/PSAlphaTestCB), `Direct3d11_ConstantBuffer.cpp:174-214` (`updatePS` → Map WRITE_DISCARD → `PSSetConstantBuffers`). VERIFIED.
- `Direct3d11.cpp:660-694` (`setPixelShaderUserConstants_impl` flush of `PerMaterialCB` to PS slot `b2`, material color fields zero). VERIFIED.
- `Direct3d11_VertexShaderData.cpp:586-673` (D3DReflect input/output pattern to mirror for PS). VERIFIED.
- `Direct3d11_HlslRewrite.cpp:790-805` (cbuffer slot-binding rationale). VERIFIED.
- `ShaderBuilder/Node.cpp:5810-5854` (PSRC written/read as IFF string), `PixelShaderProgramView.cpp:172,304-328,465` (`//hlsl ` prefix → `D3DXCompileShader`; else `D3DXAssembleShader`). VERIFIED.
- `ConfigDirect3d11.cpp:34-38` (`KEY_BOOL`/`KEY_INT` plugin config pattern; shared code must use `ConfigFile` directly). VERIFIED.
- `stage/client_d.cfg:14-42` (asset TRE/TOC + `searchPath` + `rasterMajor=11`). VERIFIED.
- Asset/tool availability: `D:/Code/SWGSource Client v3.0/` (TREs present), `D:/swg_dev_bundle/shader` (2 loose `.sht`, no `.psh`, no `sul_*`), RenderDoc not installed. VERIFIED by filesystem probe.

### Secondary (HIGH-MEDIUM — project research artifacts)
- `.planning/research/CONSULT-slot0-SYNTHESIS.md` — CODEX+Cursor-validated reframe; locked lanes, reflection-driven constants, census-on-real-assets, convergent landmines. (SUPERSEDES SUMMARY.md on the asm lane.)
- `.planning/research/SUMMARY.md` — broader v2.2 synthesis; pitfalls catalogue, compile-flag rationale (`ps_4_0`, backwards-compat not strictness).
- `docs/research/phase12-baseline/COMPARISON.md` — 5 gap-bucket baseline + matched-pair naming convention for D-05.
- `.planning/STATE.md`, `.planning/REQUIREMENTS.md`, `17-CONTEXT.md` — locked decisions, requirements, traceability.

### Project memory (HIGH — recorded engine history)
- `d3d11_cbuffer_transpose_quirk`, `d3d9_compare_table_swap`, `d3d11_baked_rt_bgra_format`, `project_phase11_minimap_never_round`, `project_d3d11_collide_use_after_free`, `project_rastermajor_values`, `project_v2.2_slot0_psrc_reframe`, `feedback_debug_exe_reads_client_d_cfg`, `project_decruft_removal_build_graph_gotchas` (build invocation gotchas).

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — every API/helper read directly from the live tree this session; all in-tree, no new deps.
- Architecture: HIGH — root cause chain (discard → PEXE reject → slot-0 fallback) and every integration seam confirmed at exact file:line. `compilePixelShaderFromHlsl` present-but-unused-for-assets de-risks the recompile lane.
- Pitfalls: HIGH — most are recorded engine history (Iter-44B/44C reverts, id=342/343 saga, transpose quirk), not predictions.
- HLSL:asm ratio: LOW (by design) — unmeasurable from this checkout; the census IS the deliverable that resolves it.

**Research date:** 2026-05-27
**Valid until:** ~2026-06-26 (30 days; stable in-tree codebase, but the working tree has uncommitted changes — re-verify line numbers if the tree drifts significantly before planning).
