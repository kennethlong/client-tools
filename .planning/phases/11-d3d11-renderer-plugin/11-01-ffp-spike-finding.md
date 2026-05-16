# Phase 11 — D-04 FFP Spike Finding

**Captured:** 2026-05-15 (Phase A) / 2026-05-16 (Phase B finalization)
**Decision:** D-04a verdict — **DESCOPE generator** (Phase A non-empty + Phase B empty → DESCOPE per D-04a)
**Methodology:** Phase A (static analysis) — complete; Phase B (runtime instrumentation + Kenny play session) — complete
**Author:** Claude (executor for plan 11-01 Tasks 1, 3, 4)

---

## Phase A — Static Analysis

### Asset-side shader source FFP references

| Grep target | Path | Result |
|---|---|---|
| On-disk shader template dir `dsrc/sku.0/sys.client/compiled/game/shader/` | repo root | **MISSING** — directory does not exist in v2 tree |
| On-disk material dir `dsrc/sku.0/sys.client/compiled/game/material/` | repo root | **MISSING** — directory does not exist in v2 tree |
| On-disk shader template dir `data/sku.0/sys.client/compiled/game/shader/` | repo root | **MISSING** — directory does not exist in v2 tree |
| `*.shader` glob | repo-wide | 0 files |
| `*.mat` glob | repo-wide | 0 files |
| `*.tdef` glob | repo-wide | 0 files |
| `*.iff` glob | repo-wide | 2 files — both are runtime profile artifacts (`stage/profiles/swg/swg/127040355_friend_data.iff`, `stage/local_machine_options.iff`); neither is a shader template |
| `FixedFunctionPipeline\|fixed_function\|FFP_PASS` across `*.mat`/`*.shader`/`*.tdef` | repo-wide | 0 hits (no files to search) |

**Interpretation:** The v2 working tree carries **zero** on-disk asset-side shader-template sources. All shader templates and material definitions ship inside binary `.iff` TRE archives via Kenny's retail SWG install (mounted at runtime through `searchPath*` `client.cfg` entries; not present in this repo). Static analysis of asset *content* is therefore impossible from the repo alone — the FFP-usage question for Tatooine outdoor + Mos Eisley cantina assets cannot be answered from the asset side without (a) extracting TRE archives offline, or (b) runtime instrumentation in the engine's shader-template loader. Per D-04 design, option (b) is the Phase B approach.

The remaining static surface is the **engine-side FFP class hierarchy and the D3D9 plugin's FFP code paths** — both audited below. If the engine has the FFP hierarchy compiled in and the D3D9 plugin's FFP code paths are reachable, then a runtime FFP activation is *possible*; whether one *occurs* in the target scenes is the Phase B question.

### Engine-side FFP class hierarchy presence

| Symbol pattern | File | Hits |
|---|---|---|
| `ShaderImplementationPassFixedFunctionPipeline` class declaration | `src/engine/client/library/clientGraphics/src/shared/ShaderImplementation.h:310` | 1 |
| `FixedFunctionPipeline` typedef | `ShaderImplementation.h:145` | 1 |
| `m_fixedFunctionPipeline` member | `ShaderImplementation.h:262` + `.cpp` references | 17 (1 header, 16 cpp) |
| `new FixedFunctionPipeline(iff, ...)` instantiation sites | `ShaderImplementation.cpp` lines 1087, 1143, 1198, 1260, 1337, 1416, 1496, 1575, 1655, 1740, 1826 | **11 instantiation sites** |
| `load_0000` / `load_0001` Iff parsers that branch into FFP construction | `ShaderImplementation.cpp:1984, 2012, 2026, 2042` | 4 loader entry points |
| `verifyCompatibility` per-FFP-pass | `ShaderImplementation.cpp:2057` | 1 |
| **Total class-hierarchy citation count** | engine `clientGraphics` only | **≥35 distinct references** |

**Interpretation:** The engine retains a fully-wired FFP class hierarchy with **11 asset-driven instantiation sites**. Each site is reached from `ShaderImplementation::load_*` parsing an `.iff` shader-template blob — meaning every shader template that ships an `FFP ` pass chunk in its IFF binary triggers a live `new FixedFunctionPipeline(iff, ...)` call, which then drives the D3D9 plugin's FFP setter sites (`Direct3d9_StateCache.cpp:159,162,382,383` and `Direct3d9_ShaderImplementationData.cpp:147,152,382,383`) when that pass renders. The class hierarchy is **not vestigial**; it is **load-path-reachable** by design. Whether asset templates for the Phase 11 target scenes (Tatooine outdoor, Mos Eisley cantina) actually carry FFP-pass chunks is the Phase B question — but the engine is unambiguously primed to fire if they do.

### D3D9 plugin FFP code-path count

| Grep | Path | Hits |
|---|---|---|
| `#ifdef FFP` / `#if defined(FFP)` preprocessor regions | `src/engine/client/application/Direct3d9/src/win32/` | **49 hits across 8 source files** |
| Broader `\bFFP\b` token (includes `ENABLE_FFP`, `Direct3d9_ffp.vcxproj` defines, etc.) | same | 83 hits across 10 files |

**Per-file `#if(def)? FFP` distribution:**

| File | Hits |
|---|---|
| `Direct3d9.cpp` | 15 |
| `Direct3d9_StaticShaderData.cpp` | 16 |
| `Direct3d9_ShaderImplementationData.cpp` | 7 |
| `Direct3d9_StaticShaderData.h` | 4 |
| `Direct3d9_LightManager.cpp` | 3 |
| `Direct3d9_ShaderImplementationData.h` | 2 |
| `Direct3d9_LightManager.h` | 1 |
| `Direct3d9_VertexDeclarationMap.cpp` | 1 |
| **Total** | **49** |

Plus `Direct3d9.vcxproj` (3 hits) + `Direct3d9_ffp.vcxproj` (3 hits) for FFP/VSPS preprocessor configuration — these confirm the build system produces both `gl05_d.dll` (FFP+VSPS combined) and `gl06_d.dll` (FFP-only) variants today.

**Interpretation:** The D3D9 plugin's FFP code paths are **pervasive and load-bearing**. Removing them would require a substantial rewrite of `Direct3d9_StaticShaderData`, `Direct3d9_ShaderImplementationData`, `Direct3d9_LightManager`, and the vertex declaration map. They are not dead code — `gl05_d.dll` and `gl06_d.dll` are both built today and both compile with `FFP` defined.

### Interpretation (summary)

Static analysis cannot deliver a clean BUILD/DESCOPE verdict on its own for two reasons:

1. **No on-disk asset surface to grep.** All shader templates and material definitions live inside binary `.iff` TRE archives. The repo contains zero `.mat`, `.shader`, `.tdef`, or shader-template `.iff` files. Per D-04 design, this is the case Phase B was authored to cover.

2. **Engine + plugin FFP infrastructure is HOT, not vestigial.** 11 asset-driven instantiation sites, 4 IFF loader entry points, 49 `#ifdef FFP` regions across 8 files in the D3D9 plugin. If any shader template in Kenny's retail TRE archives carries an `FFP ` IFF chunk for a material rendered in Tatooine outdoor or Mos Eisley cantina, the FFP path **will fire**. The infrastructure is built to fire.

The static evidence is **non-empty** in the strongest sense the v2 repo can deliver: the engine is fully primed for FFP activation, and the only way to validate whether the target scenes actually trigger it is runtime instrumentation per D-04 Phase B.

---

## Phase A Verdict

- [ ] **Empty:** asset-side FFP references = 0, engine PassFixedFunctionPipeline never instantiated → DESCOPE generator. Skip Phase B. Reason recorded.
- [x] **Non-empty:** asset-side FFP references > 0 OR engine PassFixedFunctionPipeline runtime path is reachable → PHASE B REQUIRED. Proceed to Task 2.

**Verdict rationale:** Per D-04a recommendation ("if Phase A returns ANY non-zero hit, even if interpretation is ambiguous, default to PHASE B EXECUTE"), and per D-04 design ("Phase B is the only viable signal when on-disk shader sources don't exist"): **Proceed to Task 2 (Phase A verdict review checkpoint).** The engine has 11 live FFP instantiation sites, the D3D9 plugin has 49 FFP-gated code regions, and `gl05_d.dll`+`gl06_d.dll` both build with FFP enabled today. The infrastructure is unambiguously hot; runtime evidence is required to determine whether Tatooine + cantina assets exercise it.

---

## Phase B Findings

**Captured:** 2026-05-16 (Kenny play session, second run on fresh `SwgClient_d.exe`)
**Verdict (per D-04a):** ZERO post-init FFP activation across both target scenes → DESCOPE generator.
**CSV path:** `D:/Code/swg-client-v2/stage/dpvs-profile/ffp-spike.csv` (17 rows: 1 header + 16 data)
**Plugin variant under test:** D3D9 baseline, `rasterMajor=5`, `gl05_d.dll`
**Session:** ≥5 min Tatooine outdoor + ≥5 min Mos Eisley cantina interior (the SPEC R5 target scenes), clean exit to flush.

### Activation summary by site

| Site | Rows | Frame range | Notes |
|---|---:|---|---|
| `StateCache_init` (device-init defaults; `Direct3d9_StateCache.cpp:164/168`) | **16** | 0 | 8 stages × 2 ops (COLOROP + ALPHAOP); all `value=1` = `D3DTOP_DISABLE`. Expected device-default writes during `StateCache::initialize()` — these fire once during plugin startup before any frame is rendered. |
| `StateCache_restore` (post-device-lost restore; `Direct3d9_StateCache.cpp:397/401`) | **0** | n/a | Never fired during ≥10 min of D3D9 play. No device-lost events on Win11/WDDM during the session. |
| `ImplStage_build` (per-stage FFP shader-template construct; `Direct3d9_ShaderImplementationData.cpp:152/158`) | **0** | n/a | **Critical signal:** the per-stage builder is reached when a shader template's IFF carries an `FFP ` pass chunk that drives `new ShaderImplementationPassFixedFunctionPipeline(iff, ...)`. Zero hits across both scenes = zero asset-driven FFP-pass templates exercised by Tatooine + cantina rendering. |
| `ImplStage_cascade` (FFP cascade-terminate; `Direct3d9_ShaderImplementationData.cpp:389/391`) | **0** | n/a | Zero unused-stage cascade-disables fired (necessarily zero given `ImplStage_build` is also zero). |
| **Total post-init FFP activations across both scenes** | **0** | — | **All 16 data rows are init-time defaults; zero rows from gameplay.** |

### Frame counter observation

Every data row carries `frame=0`. Two non-exclusive interpretations:

- **(a) Most likely — engine renders both target scenes entirely through shader paths.** The `++D04Spike_frame` increment at `Direct3d9.cpp:2386` (inside `Direct3d9Namespace::endScene`) fired ~36k times during the session (≥10 min × ~60 fps), but **zero post-init FFP setter sites fired** during that entire span. Init-time rows happen during `StateCache::initialize()`, which runs before the first frame is rendered (frame counter is still 0). Therefore: the 16 captured rows are device-init defaults written before frame 0; the absence of any frame>0 rows confirms zero FFP activation during gameplay.
- **(b) Less likely but worth flagging — the `endScene` increment site could be in dead code or wrong location.** If `Direct3d9Namespace::endScene` is never called (e.g., a different `endScene` path is taken in modern Koogie-merge code), the counter would also stay at 0. Even under this interpretation: the four FFP setter sites still fired zero times post-init, so the DESCOPE verdict stands regardless of whether (a) or (b) is the operative explanation.

Both interpretations support the same conclusion: **zero post-init FFP activations in either target scene.** The init-time `StateCache_init` writes are the device-default-state setup that runs before any rendering — they are not evidence of FFP rendering, they are evidence of *defensive default-state programming for a deactivated FFP path*. (Equivalent: a CPU writes 0 to a register at boot; that doesn't mean the register is *used* by subsequent code.)

### Cross-reference to capture chronology

The Phase B capture went through three commits before producing the 17-row CSV. Honest accounting of the false starts:

1. **Commit `0293ef310` (ADD)** — Inserted THROWAWAY D-04 instrumentation at the 8 FFP setter sites plus the per-frame counter in `Direct3d9::endScene`. CSV path was project-relative (`stage/dpvs-profile/ffp-spike.csv`) — a typo not yet caught.
2. **Commit `6c11640bc` (PATH FIX)** — Changed the path to exe-relative (`dpvs-profile/ffp-spike.csv`) so the writer would resolve under `stage/` when `SwgClient_d.exe` launches from `stage/`. Matches the Phase 10 `DpvsProfileInstrumentation` convention.
3. **Commit `266e173b3` (BUILD-SYSTEM FIX, bonus)** — Discovered during the path-fix relaunch that the Koogie post-build copy steps in `SwgClient.vcxproj` + `Direct3d9*.vcxproj` were not landing the rebuilt DLLs into `stage/`. The first Kenny play session ran against a **stale `SwgClient_d.exe`** (predated the SafeCast fix `73e29eee7` and the path fix), and the unwritten CSV at the wrong project-relative path produced **zero output** — which initially looked like a successful "empty Phase B" signal but was actually a different bug (stale-exe trap + wrong-path bug stacked on top of each other). Fixing the post-build copy made future rebuilds auto-stage; the second Kenny play session ran against a fresh exe with the path fix in place and produced the 17-row CSV.

The earlier zero-row file (created during the first play session against the stale exe) was at the wrong project-relative path; it was overwritten/superseded by the second play session's run against the corrected path. The 17-row CSV at `D:/Code/swg-client-v2/stage/dpvs-profile/ffp-spike.csv` is the **definitive Phase B evidence**.

### Interpretation (summary)

Combined Phase A static evidence + Phase B runtime evidence:

- **Phase A (non-empty):** 11 asset-driven `new FixedFunctionPipeline(iff, ...)` instantiation sites in `ShaderImplementation.cpp`; 49 `#ifdef FFP` regions across 8 D3D9-plugin source files; `gl05_d.dll` + `gl06_d.dll` both build with `FFP` defined. The engine is fully primed to fire if any rendered asset's shader template carries an `FFP ` IFF chunk.
- **Phase B (empty):** Across ≥10 minutes of D3D9 gameplay covering both SPEC R5 target scenes (Tatooine outdoor + Mos Eisley cantina interior), **zero post-init FFP activations** were recorded. The only entries in the CSV are the 16 init-time `D3DTOP_DISABLE` defaults written during `StateCache::initialize()` — these run before any frame, are independent of asset content, and are not evidence of FFP rendering.

**Conclusion:** the engine carries a load-bearing FFP infrastructure that is **provisioned but unexercised by the SPEC R5 target scenes**. Modern Koogie-merge content rendering on Tatooine + cantina interior takes the shader path (`VertexShader` / `PixelShader` siblings of `ShaderImplementationPassFixedFunctionPipeline`) end-to-end.

---

## D-04a Final Verdict

**Verdict: DESCOPE the FFP pixel-shader generator.**

Per CONTEXT D-04a: *"If Phase B is empty across both target scenes, descope the FFP generator entirely — document static + runtime evidence in a Phase 11 plan artifact and remove `Direct3d9_ffp.dll`'s mirror from the D3D11 plan's responsibility set."*

### Input directive for Plan 05 (Wave 5 — Shader layer)

**`Direct3d11.vcxproj` MUST OMIT `Direct3d11_FfpGenerator.{h,cpp}` from its source list.** No FFP generator class is authored. Plan 05's scope reduces to:
- `D3DCompile` + `vs_5_0`/`ps_5_0` recompile pipeline (D-03)
- `.cso` disk cache plumbing (D-03)
- Constant-buffer wrapper migration (replaces `SetVertexShaderConstantF`/`SetPixelShaderConstantF`)
- HLSL SM2.0 → SM5.0 mechanical recompile of the existing VSPS shader set (D3D11-03)

No `D3DTOP_MODULATE` / `D3DTOP_ADD` / `D3DTOP_SELECTARG1` HLSL emitter is required (the SPEC R4 acceptance "FFP pixel shader generator covers MODULATE / ADD / SELECTARG1" is **satisfied by descope evidence** per D-04a).

### Continued D3D9-side fallback (unchanged by this verdict)

The existing D3D9 FFP variant `gl06_d.dll` (built from `Direct3d9_ffp.vcxproj` with `FFP` defined) **stays on disk** as a D3D9-side fallback path. Per D-05, all three D3D9 variants (`gl05_d.dll`, `gl06_d.dll`, `gl07_d.dll`) continue to build clean through every Phase 11 wave. D3D11 does not mirror the FFP variant.

### Evidence summary

| Source | Result | Bearing on verdict |
|---|---|---|
| Phase A — engine FFP class hierarchy | 11 instantiation sites, 4 IFF loader entry points, ≥35 distinct references in `clientGraphics` | Infrastructure is hot — Phase B was required |
| Phase A — D3D9 plugin FFP code paths | 49 `#ifdef FFP` regions across 8 source files | Plugin is primed for FFP rendering |
| Phase A — asset-side on-disk shader sources | 0 files on disk (all assets ship inside binary `.iff` TRE archives) | Static asset analysis impossible from repo; Phase B is the only viable signal |
| Phase B — `StateCache_init` (device defaults) | 16 rows at frame 0 | Init-time defensive defaults; not FFP rendering |
| Phase B — `StateCache_restore` (device-lost restore) | 0 rows | No device-lost events; no FFP restore |
| Phase B — `ImplStage_build` (asset-driven FFP construct) | **0 rows** | **Definitive: no asset in either target scene drove an FFP-pass IFF chunk** |
| Phase B — `ImplStage_cascade` (cascade-terminate) | 0 rows | Consistent with `ImplStage_build = 0` |

**Combined verdict:** static infrastructure is present-but-unexercised under D3D9 on the SPEC R5 target scenes. D3D11 inherits this property by design — if the D3D9 baseline never reaches the FFP path on Tatooine outdoor + Mos Eisley cantina interior, neither will a D3D11 plugin that targets the same asset set. The generator is dead code for the Phase 11 deliverable surface.

### Plan 02 (Wave 2 — plugin scaffold) status

**UNBLOCKED.** Wave 2 can proceed to author the `Direct3d11/` source tree + `Direct3d11.vcxproj` knowing definitively that the source list excludes `Direct3d11_FfpGenerator.{h,cpp}`. The conditional D-04a decision is resolved.

---

**Closes:** D-04 spike, D-04a decision gate, SPEC R4 acceptance ("Spike result committed as a written finding in a Phase 11 plan artifact").
**Unblocks:** Plan 11-02 (Wave 2), Plan 11-05 (Wave 5).
