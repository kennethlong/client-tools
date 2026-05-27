# Phase 17: PSRC Census + Char-Select Beachhead - Context

**Gathered:** 2026-05-27
**Status:** Ready for planning

<domain>
## Phase Boundary

Prove the D3D11 asset pixel-shader pipeline end-to-end on the **deterministic, isolated character-select screen** — skin/clothing diffuse textures, eyes, and multi-stage head/face materials all render correctly under `rasterMajor=11`, matching the known-good `rasterMajor=5` (D3D9) baseline. The phase's first, gating deliverable is a **PSRC language census on the real asset tree** (HLSL:asm ratio), which decides the recompile-vs-port lane mix before any fix work.

This is the milestone beachhead: convert the biggest open risk (HLSL:asm ratio) into a measured number, then validate the recompile + reflection-driven-constant approach on a bounded, known shader surface before any open-world work.

**In scope:** CHAR-01 (skin/clothing diffuse), CHAR-02 (eyes), CHAR-03 (head/face multi-stage compositing) — all at char-select default pose only.

**Out of scope (own phases):** open-world surfaces / extended `TextureOperation` census (Phase 20), gamma LUT + interior lighting (Phase 19), half-texel load-screen seam (Phase 18), particles/ribbon (Phase 21), exterior geometry shards (Phase 22), DPVS remeasure (Phase 23). The single-stream-skinning **fix decision** is deferred to Phase 22 — Phase 17 only *confirms* single vs multi-stream by RenderDoc A/B so residual head weirdness isn't misattributed to the PS.

</domain>

<decisions>
## Implementation Decisions

### Census mechanism + asset source (gating deliverable)
- **D-01:** The PSRC census is produced by **engine-instrumented load-path logging**, not an offline tool. Add temporary logging at the PSRC-discard site (`ShaderImplementation.cpp:2895-2908`, where `enterChunk(TAG_PSRC); exitChunk(TAG_PSRC,true)` currently drops the source) to dump, per shader loaded during a **real char-select boot**: name, `//hlsl`-vs-asm classification, target profile, includes, sampler slots, and referenced constants. Rationale: captures *exactly* the char-select working set deterministically, reuses the proven `.tre` + `.toc` asset resolution, and requires zero guessing about which assets char-select touches.
- **D-02:** Census runs against the asset tree **the client already loads as-is** — `D:/Code/SWGSource Client v3.0/swgsource_3.0.tre` (+ `disable_wayfar_dearic_snow.tre`) and `searchPath D:/swg_dev_bundle` (per `stage/client_d.cfg:23-25`). That IS the deterministic char-select asset set. Do **not** pull the SWGSource-vs-retail TRE asset-diff into the gating path (see Deferred).
- **D-03:** The census instrumentation is **kept (not throwaway)** — gated behind a config/log flag so Phase 20 can re-run it on an open-world boot for the extended `TextureOperation` census. The HLSL:asm ratio is recorded as a phase artifact (the input that decides the lane mix).

### `Pass::apply` constant scope
- **D-04:** Build the **D3DReflect-driven** constant-upload path **generically** — it maps whatever constants the recompiled PS declares (driven by reflection, NOT copied D3D9 register indices c11/c44/c45/c47). But **only wire the source-data feeds char-select actually exercises**: material color + textureFactor (per census). Defer `textureScroll` (animated UV), `fog`, and `stencil` feeds to the phases that need them (Phase 19 lighting/gamma, Phase 21 effects). Rationale: minimal shared `Direct3d11_StaticShaderData` surgery, stays bootable, and the reflection plumbing is reusable so later phases just add feeds rather than re-architect.
- D3D9 reference for the full constant set (do NOT copy register layout, only the data semantics): `Direct3d9_StaticShaderData.cpp:820-955`. D3D11 deferral site: `Direct3d11_StaticShaderData.cpp:601-602`. `PerMaterialCB` already exists at `Direct3d11_ConstantBuffer.h:58-64` (fields currently zero).

### Validation evidence bar
- **D-05:** **Hybrid** — manual A/B screenshots are the *gate* for Phase 17, but matched `rasterMajor=5` / `=11` pairs are saved into a **structured COMPARISON dir + naming convention** (mirroring `docs/research/phase12-baseline/COMPARISON.md`) so a scripted pixel-diff harness can be bolted on in a later phase (20+) **without recapturing**. Cheap now, leaves the door open.
- **D-06:** Hard gates (all must hold): `id=342==0 && id=343==0` in `stage/d3d11-debug.log`; both `rasterMajor=5` and `=11` boot to char-select without crash; **D3D9 reference path unregressed**.
- **D-07:** CHAR-02 (eyes) and any minimap-style "fixed" claim require a **committed matched A/B pair** before marking done — enforces the Iter-44B no-false-pre-claim lesson. "Eyes through back of head" is depth (already fixed Iter-44A, `Direct3d11_StaticShaderData.cpp:658-677`) — verify by a *fresh* A/B screenshot, do NOT re-fold into PS scope.
- **D-08:** The single-stream-vs-multi-stream skinning confirmation is a **manual RenderDoc mesh-viewer A/B capture**, taken *before* attributing any residual head/mesh weirdness to the PS. The fix decision (if any) is deferred to Phase 22.

### Claude's Discretion
- **D-09 (bridge PS — executor's call):** Go **recompile-first** (primary lane: PSRC `//hlsl` → `compilePixelShaderFromHlsl` → bind `m_d3dPS`). Build the minimal multi-sampler diagnostic bridge PS **only if** recompile attribution gets murky (i.e. the first textured frame is ambiguous and you can't separate a constant-upload bug from a bad recompile). The bridge is a diagnostic tool, not the parity strategy. Executor decides at execution time based on how the recompile lands.
- Exact char-select shader set beyond the named anchors (`sul_eye.sht`, `sul_*_head.sht`, + one single-stage body/clothing control) is census-derived — planner/executor pick the single-stage control from what the census shows.
- Census-flag gating mechanism (cfg key vs compile-time define) is executor's choice, as long as it leaves the D3D9 path untouched when off.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Milestone root cause + locked approach (READ FIRST)
- `.planning/research/CONSULT-slot0-SYNTHESIS.md` — the CODEX+Cursor-validated reframe; **supersedes SUMMARY.md** on the asm-pass approach. Locks the lanes, the reflection-driven-constants rule, the census-on-real-assets requirement, and the convergent landmines. The single most important doc for this phase.
- `.planning/research/SUMMARY.md` — broader v2.2 research synthesis (deferred to SYNTHESIS.md where they differ on Claim 2 / asm lane).
- `.planning/research/CONSULT-slot0-codex.out` — raw CODEX reviewer output (review slice).
- `docs/research/phase12-baseline/COMPARISON.md` — the 5 gap-bucket baseline + the matched D3D9/D3D11 screenshot-pair convention to mirror for D-05 validation.

### Requirements + roadmap
- `.planning/ROADMAP.md` § "Phase 17: PSRC Census + Char-Select Beachhead" — goal, 5 success criteria, approach notes (locked lanes + landmines), Research note (a/b/c items needing per-phase research).
- `.planning/REQUIREMENTS.md` — CHAR-01 / CHAR-02 / CHAR-03 acceptance text.

### Key engine source landmarks (cited in the synthesis — verify before editing)
- `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.cpp:2895-2908` — PSRC load-then-discard site (census instrumentation point + the shared PSRC-retain edit). **Shared code: keep D3D9 byte-for-byte identical except storing the text; boot-gate BOTH renderers on any edit.**
- `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.h:626` — PS header has only `m_exe`, no `m_text` (the retain edit adds storage).
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_PixelShaderProgramData.cpp` — `:86-171` `compilePixelShaderFromHlsl` (exists, unused for assets — the primary lane); `:719`/`:748-756` PEXE rejected → `m_d3dPS` null; `:339`/`:405-472`/`:506-509` slot-0/magenta fallback PS.
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticShaderData.cpp` — `:711-728` SRVs 1..7 bound (Iter-44E) but unsampled; `:601-602` per-pass constants deferred; `:658-677` Iter-44A per-pass depth state (the "eyes through head" fix — do not re-fold).
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_ConstantBuffer.h:58-64` — `PerMaterialCB` (material fields currently zero).
- D3D9 reference (do NOT copy register indices, only data semantics): `Direct3d9_StaticShaderData.cpp:820-955` (constant uploads); `Direct3d9_PixelShaderProgramData.cpp:34` (consumes same `m_exe`).
- ShaderBuilder PSRC handling (how `//hlsl` vs asm is compiled): `Node.cpp:5818`/`:5843`, `PixelShaderProgramView.cpp:308-328`/`:461`.

### Config / runtime
- `stage/client_d.cfg:23-25` — asset search trees + `searchPath D:/swg_dev_bundle` (the census asset source). NOTE: debug exe reads `client_d.cfg`, release reads `client.cfg`.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `compilePixelShaderFromHlsl` (`Direct3d11_PixelShaderProgramData.cpp:86-171`) — already exists from Phase 11, just unused for assets. The primary recompile lane plugs into this; mirror the working VS compile stack.
- `PerMaterialCB` (`Direct3d11_ConstantBuffer.h:58-64`) — material cbuffer already declared; the reflection-driven upload (D-04) populates its currently-zero fields.
- SRVs 1..7 already bound by Iter-44E (`Direct3d11_StaticShaderData.cpp:711-728`) — binding is proven; only sampling + constants are the new variable.
- Iter-44A per-pass depth state (`Direct3d11_StaticShaderData.cpp:658-677`) — the "eyes occluded by head" fix already shipped; verify by A/B, don't re-implement.
- Python TRE-reading tooling exists in-tree (`tools/swg_blender/swg_pipeline/tre_reader.py`) — NOT chosen for the census (D-01 picked engine instrumentation), but available if an offline cross-check is ever wanted.

### Established Patterns
- **D3DReflect-driven constants, never D3D9 register folklore** — `HlslRewrite` + `D3DCompile` reallocate registers; the recompiled PS may not use the D3D9 register slots. Drive `Set*ShaderConstants` from the reflected layout. (Ties to proven pitfalls id=342/343.)
- **cbuffer matrices need `XMMatrixTranspose`** (`d3d11_cbuffer_transpose_quirk` memory) — any new cbuffer matrix.
- **D3D9 `Compare[]` swap is intentional** (`C_GreaterOrEqual` ↔ `C_NotEqual`, `d3d9_compare_table_swap` memory) — mirror, don't "fix".
- **Magenta fallback PS kept as a visible diagnostic tombstone** — don't remove it.
- **Full-buffer/discard-safe cbuffer writes** — partial writes leave garbage.
- **Shader-cache invalidation** — generator/rewrite/profile changes must feed the `.cso` cache key/version or stale caches mislead debugging.

### Integration Points
- Shared `clientGraphics/ShaderImplementation.cpp` — the PSRC-retain edit is the single shared-code touch; everything else lives in the `Direct3d11` plugin. The byte-for-byte D3D9-identical constraint + dual-renderer boot gate is the integration guardrail.
- The asm→HLSL secondary lane (if census shows asm shaders) ports asm → HLSL → `D3DCompile ps_4_0`. **NEVER re-assemble asm** (`D3DXAssembleShader` reproduces the exact D3D9 bytecode D3D11 rejects — named landmine).
- Do NOT flip texture SRVs to `_SRGB` (D3D9 samples sRGB OFF, `Direct3d9_StateCache.cpp:193`). Do NOT re-enable per-pass blend factors early (Iter-44C amplification regression) — wait until the real PS samples all bound SRVs.

</code_context>

<specifics>
## Specific Ideas

- Char-select beachhead anchors named in ROADMAP/synthesis: `sul_eye.sht` (uses `TAG_PPSH` with `numberOfStages==0` — compositing lives in the `.psh` program, NOT in `Pass::m_stage`), `sul_*_head.sht` (multi-stage head/face), plus **one single-stage body/clothing control** to separate "pipeline works" from "multi-sampler works."
- Validation pose: char-select **default pose**, matched `rasterMajor=5` vs `=11` pairs.
- Success means "matches `rasterMajor=5`", explicitly NOT merely "no magenta."

</specifics>

<deferred>
## Deferred Ideas

- **SWGSource-vs-retail TRE faithfulness** — censusing against SWGSource 3.0 as-is (D-02) assumes the char-select `.psh`/`.sht` are representative; a true retail-TRE reconciliation is explicitly NOT pulled into the gating path (would risk stalling the beachhead). Revisit only if the census surfaces clearly SWGSource-modified shaders.
- **Single-stream-skinning fix** — Phase 17 only confirms single vs multi-stream (D-08); the fix-vs-flip decision is Phase 22.
- **Scripted pixel-diff harness** — structurally enabled by D-05's naming convention; actual harness build is Phase 20+.
- **scroll / fog / stencil constant feeds** — Phase 19 (lighting/gamma) and Phase 21 (effects).

### Reviewed Todos (not folded)
- `2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md` — reviewed; deferred. Tangentially related to the census asset source (D-02) but its actual content is space-scene graphics-artifact diffing, informational, and pulling TRE reconciliation into the gating path would stall the beachhead. Stays a low-priority informational todo.
- `2026-05-15-cantina-corner-snap-engine-improvement.md` — reviewed; deferred. Collision-response / portal-traversal engine quirk, not PS-pipeline work; matched only on generic keywords. Pre-existing SOE engine quirk, not in Phase 17 scope.

</deferred>

---

*Phase: 17-PSRC Census + Char-Select Beachhead*
*Context gathered: 2026-05-27*
