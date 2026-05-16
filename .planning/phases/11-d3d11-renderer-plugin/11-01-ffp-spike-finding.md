# Phase 11 — D-04 FFP Spike Finding

**Captured:** 2026-05-15
**Decision:** D-04a verdict — PHASE B REQUIRED (Phase A non-empty; runtime instrumentation needed for final BUILD vs DESCOPE)
**Methodology:** Phase A (static analysis) — complete; Phase B (runtime instrumentation) — gated by Task 2 checkpoint
**Author:** Claude (executor for plan 11-01 Task 1)

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

## Phase B Findings (if executed)

*Placeholder — populated by Task 3 if Task 2 selects `phase-b-execute`.*

---

## D-04a Final Verdict

*Placeholder — populated by Task 4.*
