# Phase 33: x64 Build Platform + D3D9 Renderers - Context

**Gathered:** 2026-06-17
**Status:** Ready for planning

<domain>
## Phase Boundary

Stand up the **first fully-linking native x64 client**: add the `x64` platform to
`src/build/win32/swg.sln` and every **boot-path** `.vcxproj`, resolve all third-party
dependencies as x64, finish removing the residual D3DX dependency that still blocks a clean
x64 link, and get the **D3D9 renderer plugins (gl05 / gl06 / gl07)** to carry the x64 client
to character select under `rasterMajor=5`, `=6`, and `=7` — without regressing the shipped
32-bit build. Requirements **X64-01, X64-04, X64-02**.

**In scope:**
- Add the committed `x64` platform to `swg.sln` + the **boot-path subset** of `.vcxproj`s
  (Phase 31's ~57 in-scope engine libs + gl05/gl06/gl07 + SwgClient). See D-03.
- Resolve every third-party dep as x64 via the **hybrid sourcing strategy** (D-01): build from
  in-tree source where it exists; lift Restoration's prebuilt x64 binaries for binary-only
  middleware.
- **Finish D3DX removal as the x64-link precondition** (D-04): the non-compile D3DX
  (mesh/matrix/surface ~15 APIs) → DirectXMath/own-impl; the asm `.vsh` path
  (`D3DXAssembleShader`) → `D3DAssemble` (census-gated, D-05).
- First x64 link of SwgClient + gl05/06/07; x64 boot-to-character-select under rasterMajor 5/6/7.

**Out of scope (this phase):**
- **gl11 / D3D11 as x64** — Phase 34 (X64-03). Restoration's x64 reference is D3D9-only; gl11 is
  the second, separate boot gate.
- **Miles audio x64** (`mss64.dll`, 7.2e→9.3b port) — Phase 35 (AUDIO-01/02). The x64 client may
  link/boot with audio degraded until then.
- **Editor apps** (ShaderBuilder / AnimationEditor / ParticleEditor / SwgGodClient / Headless /
  DllExport / TemplateCompiler / …) — MSB8066-pre-broken, Win32-only, NOT the x64 ship target.
- **Dropping DPVS entirely** — captured as a post-x64 evaluation (Deferred), not this phase.
- Door-snap (VERIFY-01) and OOM-class (VERIFY-02) confirmation — Phase 36, after the x64 client
  runs in-world.

</domain>

<decisions>
## Implementation Decisions

### Third-party x64 sourcing (X64-04)
- **D-01 (HYBRID sourcing):** Build x64 from in-tree source where source exists — this is the
  **majority** of deps: zlib / pcre / libxml2 / jpeg / tinyxml **and dpvs** (full source + a
  `dpvs.vcxproj` are in-tree — see D-02). For the genuinely binary-only proprietary middleware with
  no source anywhere — **bink** (`binkw32.lib`, RAD; no source) and **miles** (RAD; Phase 35 scope) —
  lift the vetted x64 binary (`D:\SWG Restoration\x64\binkw64.dll` / `mss64.dll`) + generate matching
  import `.lib`s. For **icu / discord-rpc** (open-source but not vendored as source here), prefer an
  official/Restoration prebuilt x64 over building. Rationale: the "Restoration took shortcuts I don't
  want to take" value applies **wherever a clean build is actually possible**; lifting is reserved
  for libs that genuinely cannot be built (RAD-proprietary). `d3d9.lib` x64 comes from the Windows
  SDK (not the legacy DXSDK); `d3dx9` is being removed (D-04), so it is NOT a sourcing target.
  - **Universal fallback (applies to EVERY dep, including the build-from-source ones):** if building
    a lib x64 from source proves troublesome or time-consuming, **grab its prebuilt x64 DLL from
    `D:\SWG Restoration\x64\`** + generate an import `.lib`. The from-source build is the preferred
    path (clean, no blob); the Restoration binary is the always-available escape hatch so no single
    lib can block the x64 link. (dpvs included — build it from source per D-02, but `dpvs.dll` is
    right there if the port stalls.)

### dpvs on x64 (CORRECTED — buildable from in-tree source, NOT a binary lift)
- **D-02:** **Build dPVS x64 from the in-tree source.** We have the **full dPVS source**
  (`src/external/3rd/library/dpvs/implementation/sources/` — 70 `.cpp`, 83 headers, `interface/`)
  plus an existing `implementation/msvc8/dpvs.vcxproj`. The `lib/win32-x86` dir holds only a stray
  `.pdb` — the 32-bit build compiles dPVS from source, it is NOT a prebuilt blob. Restoration's x64
  `dpvs.dll` was almost certainly built from this same SOE-licensed source. The x64 port is
  **low-risk**: every `__asm` block (in `dpvsBitMath.hpp`, `dpvsFiller.hpp`, `dpvsPrivateDefs.hpp`)
  is gated behind `#if defined(DPVS_X86_ASSEMBLY)` and **already has a portable C `#else` fallback**
  (e.g. `getHighestSetBit`/`getNextPowerOfTwo` `bsr` → LUT fallback). So the port ≈ add an x64 config
  to `dpvs.vcxproj` + leave `DPVS_X86_ASSEMBLY` undefined on x64 (C fallbacks compile automatically;
  optionally swap `bsr` → `_BitScanReverse` intrinsics for parity) + a small pointer-truncation pass
  (the C4311/C4312 class Phase 31 handled, but dPVS was OUT of Phase 31 scope so it gets its own
  mini-pass). **No binary lift, no import-lib generation for dpvs.**
  - **Flagged follow-up (NOT this phase):** evaluate **dropping DPVS entirely** after the x64
    migration — only marginal culling benefit was observed even on the D3D9 path, and Phase 23's
    D3D11 verdict already flips indoor occlusion off. See Deferred + ties to the Phase-23 DPVS
    config-gate. "Build it x64 from source for now; decide whether to keep it after x64 lands."

### Project scope of the platform-add (X64-01)
- **D-03:** Add `x64` to the **boot-path subset ONLY** — Phase 31's ~57 in-scope engine libs
  (sharedFoundation/sharedMath/sharedCollision/sharedMemoryManager/sharedDebug/clientGame/
  sharedNetwork/sharedFile/IFF/…) + gl05/gl06/gl07 + SwgClient. Editor apps stay Win32-only
  (pre-broken, not the ship target). Matches Phase 31's D-03 scope exactly — keeps the platform-add
  mechanical and bounded, no editor sprawl. (~132 `.vcxproj`s exist; only the boot subset gets x64.)

### Residual D3DX removal — the x64-link precondition (absorbed into Phase 33)
- **D-04:** **Absorb the remaining D3DX removal into Phase 33, sequenced BEFORE the platform-add
  link.** Static `d3dx9` has no x64 lib, so every residual D3DX symbol must be gone before the x64
  link is clean. Two parts:
  - **(a) Non-compile D3DX** (mesh / matrix / surface, ~15 APIs: `D3DXMATRIX`, `D3DXMatrixMultiply`,
    `D3DXMatrixTranspose`, `D3DXMesh`, `D3DXLoadSurfaceFromSurface`, `D3DXCreateMeshFVF`,
    `D3DXSaveTextureToFile`, …) → **DirectXMath / own-impl, NOT redist d3dx9** (per Phase 32 D-05).
    Matrix helpers are near-trivial header-only DirectXMath; mesh/surface helpers convert to own code.
  - **(b) The asm `.vsh` path** → `D3DAssemble` (D-05).
- **D-05 (asm `.vsh` → D3DAssemble, census-gated):** Port `D3DXAssembleShader`
  (`Direct3d9_VertexShaderData.cpp:760`) → `D3DAssemble` (`d3dcompiler_47`). **Run the Phase-32 asm
  census FIRST** on a representative asm sample; commit only if it compiles **and renders** clean.
  No VS may be silently nulled (no skipped draws). Keep the **Fix-A SEH guard for the asm path**
  until D3DAssemble is proven, then retire it.
  - **KEY FINDING — the D3D11/gl11 work does NOT transfer.** gl11 *sidestepped* asm entirely:
    `Direct3d11_VertexShaderData.cpp:375` ("reassembly via D3DAssemble is gone") + the Phase-19
    `[P19VSFALLBACK]` generic HLSL world-transform fallback VS (`:158-275`, the `skippedNullVS
    38927→0` work) **substitute** a modern VS because D3D11 cannot execute `vs_1_1`/`vs_2_0`
    bytecode at all. D3D9 gl05/06/07 **natively execute** these asm programs, so they must be
    *genuinely assembled* — a fallback substitute is not an option. `D3DAssemble` is the direct
    d3dcompiler_47 successor to `D3DXAssembleShader` (same legacy-D3D9 asm grammar); Phase 32's
    caveat was "unproven," not "known-broken." The census is the de-risk.
  - **Fallback (contingency, NOT chosen):** if the census shows D3DAssemble cannot parse the SWG
    asm dialect, linking the x64 redist `d3dx9` DLL for the asm path was explicitly **rejected** as
    a shortcut — it would re-introduce the D3DX dependency we are removing. Surface as a checkpoint
    instead of silently taking it.

### Claude's Discretion
- Exact per-lib sourcing disposition (which deps build-from-source vs lift) — research enumerates
  against `D:\SWG Restoration\x64\` and the in-tree source availability.
- dpvs x64-port mechanics: x64 config on `dpvs.vcxproj`, `DPVS_X86_ASSEMBLY`-undefined-on-x64 vs
  `_BitScanReverse` intrinsic swap, and its self-contained pointer-truncation pass.
- Which build configurations get x64 — must cover Debug + Release **and all three D3D9 plugin
  flavors** (the `Direct3d9` / `Direct3d9_ffp` / `Direct3d9_vsps` configs that produce gl05/06/07);
  the Optimized variant is planner's call.
- Whether to grow the Phase-31 `x64-scratch/x64-compile.props` into the committed x64 platform
  props, or author fresh per-project x64 PropertyGroups (no shared common `.props` exists today).
- Build/sequencing order of the D3DX-removal precondition vs the platform-add within the phase.

### Folded Todos
- None folded — the matching todos are the milestone parent or belong to other phases (see
  Reviewed Todos).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope & milestone plan
- `.planning/ROADMAP.md` §"Phase 33: x64 Build Platform + D3D9 Renderers" — goal + 4 success criteria
- `.planning/REQUIREMENTS.md` — **X64-01** (x64 platform + binaries), **X64-02** (gl05/06/07 boot
  under rasterMajor 5/6/7), **X64-04** (all third-party deps resolve as x64)
- `.planning/STATE.md` §"v3.0 x64 Port — the plan" + §Decisions — locked milestone sequencing,
  the extended core invariant, separate-renderer boot gates, `/FORCE` link-grep on every x64 link

### Prior-phase context (MUST read — Phase 33 consumes both directly)
- `.planning/phases/31-64-bit-correctness-foundation/31-CONTEXT.md` — D-03 in-scope lib set (the
  boot-path subset this phase platform-adds), the scratch x64 props pattern, the shared-header ABI
  cascade trap, the residual worklist handed forward
- `.planning/phases/31-64-bit-correctness-foundation/31-06-SUMMARY.md` +
  `31-PHASE33-RESIDUAL-WORKLIST.md` — the named class-(A)/3rd-party residuals that surface at the
  x64 *link/runtime* (DebugHelp x64 unwind walk, SSE alignment runtime re-confirm, Bink/WaterTest,
  the N1 `/we4311` guardrail re-establishment, the N2 C4267 carry-forward)
- `.planning/phases/32-d3dx-to-d3dcompiler-47/32-CONTEXT.md` — D-05 (non-compile D3DX →
  DirectXMath/own-impl, NOT redist), D-03/D-06 (asm census + D3DAssemble dialect risk), Fix-A
  retirement boundary; the explicit "must land BEFORE the Phase-33 x64 link" flag
- `.planning/handoff/phase-32-d3dcompile-port.md` — 32-02 landed state; what's still on D3DX

### x64 availability reference (binaries only — NOT a source diff)
- `D:\SWG Restoration\x64\` — the third-party x64 **availability map**: ships gl05/06/07_r.dll,
  `dpvs.dll`, `binkw64.dll`, `mss64.dll`, `icudt62.dll`/`libicuuc.dll`, `libxml2.dll`, `pcre.dll`,
  `jpeg62.dll`, `discord-rpc.dll` — **D3D9-only (no gl11)**. Lift the RAD-proprietary binaries
  (bink/miles) from here; the rest (incl. dpvs) we build from in-tree source. Restoration's
  `dpvs.dll` is itself a from-source build of our same SOE dPVS source.
- Memory `reference_restoration_binaries_intel` — Restoration's compile model + the x64 D3DX-redist
  reality (an x64 d3dx9 redist DLL exists; we are NOT using it — D-05 fallback rejected)

### Implementation targets
- `src/build/win32/swg.sln` — currently **0 x64** (pure Win32); the platform-add target
- `src/build/win32/x64-scratch/x64-compile.props` — Phase-31 compile-only harness props (starting
  point for the committed x64 platform props)
- `src/engine/client/application/Direct3d9/src/win32/Direct3d9_VertexShaderData.cpp` — asm path
  `:760` (`D3DXAssembleShader` → `D3DAssemble`, D-05); `:155` (D3DX-typed code note); Fix-A SEH guard
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexShaderData.cpp:158-275,:375`
  — gl11's `[P19VSFALLBACK]` generic fallback VS = WHY the D3D11 asm handling does NOT transfer to
  D3D9 (D-05 finding)
- `src/external/3rd/library/dpvs/implementation/` — **full in-tree dPVS source** + `msvc8/dpvs.vcxproj`
  (build x64 from here, D-02); `__asm` is `DPVS_X86_ASSEMBLY`-gated with C `#else` fallbacks
- `src/external/3rd/library/` — in-tree third-party source/libs (dpvs/lib = `win32-x86` stray .pdb only,
  no prebuilt binary;
  directx9/lib = x86 only; bink = `binkw32.lib` only) — the sourcing gap this phase closes

### Build/convention constraints
- `AGENTS.md` §Build — `$env:MSBUILD` (VS18/v145), canonical 5-target build, `/nodeReuse:false`,
  run from PowerShell, **`/FORCE` link-grep (0 `unresolved external symbol`)**, shared-header ABI
  cascade (rebuild ALL plugins on a shared-header touch), editor-apps-pre-broken validation bar,
  `.cfg` BOM-safety, `rasterMajor` 5/6/7 = gl05/06/07

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **Phase-31 made the in-scope tree compile x64-clean** (BITS-01/02/03 done) — so Phase 33 is a
  **build/link/procurement exercise, not a code-debugging swamp**. The residual worklist enumerates
  the few link/runtime items that only surface once the platform actually links.
- **`x64-scratch/x64-compile.props`** — the Phase-31 compile-only harness; a known-good x64 compile
  flag set to grow into the committed platform props.
- **`D:\SWG Restoration\x64\`** — vetted x64 DLLs. Primary source for the RAD-proprietary binaries
  (bink/miles); **universal fallback for EVERY other dep** (incl. dpvs) if its from-source x64 build
  stalls (D-01). Preferred path is still build-from-source where we have it.
- **gl11's full D3D11 x64 readiness is NOT here yet** — gl11 stays Win32 until Phase 34; do not pull
  it into the x64 boot subset.

### Established Patterns
- **No shared common `.props`** across the ~132 `.vcxproj`s — each project is standalone, so the
  x64 platform-add is per-project (bounded to the D-03 boot subset).
- **`/FORCE` link masks unresolved externals** — exit 0 ≠ clean link; grep every x64 link log for
  `unresolved external symbol` (must be 0). This is the primary x64-link acceptance signal.
- **Shared-header ABI cascade** — any shared-header touch (e.g. during D3DX→DirectXMath conversion)
  requires rebuilding ALL plugin `.vcxproj`s (gl05/06/07 + gl11), not just SwgClient.
- **Postbuild auto-deploy** stages `gl0X_*.dll` + `SwgClient_*.exe` into `stage/` — x64 validation
  runs from `stage/` with `rasterMajor` set in `client_d.cfg` (Debug) / `client.cfg` (Release).

### Integration Points
- `swg.sln` solution configurations + each boot-path `.vcxproj` `<ProjectConfiguration>` /
  `<PropertyGroup>` / `<ItemDefinitionGroup>` — the x64 platform surface.
- Third-party link inputs — swap x86 → x64 `.lib`s in `AdditionalDependencies` / library dirs for
  the x64 configs (dpvs, bink, icu, libxml2, pcre, jpeg, zlib, discord-rpc; `d3d9.lib` from the
  Windows SDK; **NO `d3dx9`** after D-04).
- `Direct3d9_VertexShaderData.cpp` + the non-compile D3DX call sites — the D3DX-removal precondition
  surface that must clear before the x64 link.

</code_context>

<specifics>
## Specific Ideas

- "Restoration took shortcuts I don't want to take" — the guiding value, **scoped precisely**:
  prefer the clean build/port where one is genuinely possible (own-impl D3DX, D3DAssemble), accept
  lifting vetted x64 binaries only for RAD-proprietary middleware (bink/miles) that *cannot* be built.
- "I thought we had the source to dpvs" — confirmed: we do (full source + `dpvs.vcxproj` in-tree).
  This CORRECTED the initial framing — dpvs is build-from-source, not a binary lift. Restoration's
  x64 `dpvs.dll` is a from-source build of this same SOE source.
- "Decide if we want to keep DPVS at all after the x64 migration — we were finding only marginal
  advantages even in the D3D9 use case." — the directive behind the Deferred DPVS-removal evaluation
  (build it x64 from source now; revisit keeping it after x64 lands).
- The D3D11 asm-fallback question was explicitly raised and resolved: gl11's `[P19VSFALLBACK]`
  substitution is NOT reusable for D3D9 — gl05/06/07 must truly assemble the asm (D-05 finding).

</specifics>

<deferred>
## Deferred Ideas

- **Evaluate dropping DPVS entirely (post-x64).** Occlusion shows only marginal benefit even on
  D3D9; Phase 23's D3D11 verdict flips indoor occlusion off; a config gate already exists. After the
  x64 client lands, decide whether to keep dPVS at all or rip it out (deleting a whole third-party
  middleware + its x64 build). → its own evaluation/decision, likely a later v3.x item. NOT this phase.
- **gl11 / D3D11 as x64** — Phase 34 (X64-03). Second, separate boot gate.
- **Miles 9.3b audio x64 port** (`mss64.dll`, 7.2e→9.x, the `AIL_room_type(dig, 0)` edit) — Phase 35
  (AUDIO-01/02). Already roadmapped.
- **redist-d3dx9 fallback for the asm path** — explicitly NOT taken; contingency only, surfaced as a
  checkpoint if the D3DAssemble census fails (re-introduces the D3DX dep we're removing).
- **Door-snap (VERIFY-01) / OOM-class (VERIFY-02) confirmation + CORNERSNAP probe strip (VERIFY-03)**
  — Phase 36, after the x64 client runs in-world. The x64 build is the working theory for the
  door-snap fix and the OOM-class elimination, but verification waits for an in-world x64 client.

### Reviewed Todos (not folded)
- `2026-06-13-64bit-x64-port.md` (score 0.6) — the **whole-milestone driver** (spans Phases 33-36);
  this phase is one prerequisite, not the entire x64 port. Reviewed-not-folded, consistent with
  Phase 31/32.
- `2026-05-31-port-d3d9-shader-compile-to-d3dcompile.md` (score 0.6) — Phase 32's core work (already
  folded there). The *residual* D3DX/asm tail lives in Phase 33 (D-04/D-05) but the todo itself
  belongs to 32.
- `2026-05-15-swgsource-vs-whitengold-tre-asset-diff.md` (0.6) /
  `2026-06-13-test-jtl-space-rendering-post-v2.2.md` (0.4) — informational space/asset-content items,
  unrelated to the x64 platform-add. Not folded.

</deferred>

---

*Phase: 33-x64-build-platform-d3d9-renderers*
*Context gathered: 2026-06-17*
