# Phase 11 — Plan 07 Iteration Log

**Started:** 2026-05-17
**Status:** In progress — iteration 1 landed; awaiting Kenny smoke for next FATAL boundary

## Iteration 1: Author Direct3d11_ShaderImplementationData + Direct3d11_StaticShaderData wrappers (createShaderImplementationGraphicsData FATAL boundary)

- **Date:** 2026-05-17
- **Predicted symptom:** FATAL at `createShaderImplementationGraphicsData` during `ShaderTemplate::install` per Plan 11-06 SUMMARY prediction. Engine calls `Graphics::createShaderImplementationGraphicsData(*this)` from `ShaderImplementation::load_0004 / 0005 / 0006 / 0007 / 0008 / 0009` (7 call sites). The plugin's slot was `STUB()` after Plan 11-06.
- **Hypothesis:** Two new GraphicsData wrapper classes are required:
  1. `Direct3d11_ShaderImplementationData` (inherits `ShaderImplementationGraphicsData`) — held per loaded `ShaderImplementation` asset
  2. `Direct3d11_StaticShaderData` (inherits `StaticShaderGraphicsData`) — held per `StaticShader` instance, must implement virtual `update(StaticShader const &)` + `getTextureSortKey() const`
  Both wrappers can be MINIMAL for Iteration 1 (construct successfully + record bind); per-pass state-apply work (BS/DSS overrides) and per-pass VS/PS lookup land in later iterations driven by visual symptoms. Pitfall classification: novel (not one of the 8 documented Pitfalls — this is "engine-side factory slot was STUB'd").
- **Investigation:**
  - Read Plan 11-06 SUMMARY's Predicted Plan 11-07 first FATAL note: "createShaderImplementationGraphicsData ... first shader-implementation factory called during ShaderTemplate::install."
  - Read `Direct3d9_ShaderImplementationData.{h,cpp}` (D3D9 reference). D3D9 sibling has TWO orthogonal apply paths gated by `#ifdef FFP` and `#ifdef VSPS`. D-04a omits FFP entirely; D3D11 only mirrors the VSPS shape (per-pass alpha-blend / z / color-write state recording + pixel-shader-program pointer).
  - Read `Direct3d9_StaticShaderData.{h,cpp}` (D3D9 reference). Static-shader wrapper resolves the engine's per-instance state at load time: vertex shader lookup, material struct, texture-factor data, alpha-test reference, stencil reference, fog mode, per-stage texture references. Iteration 1 omits these — pure-construct contract is sufficient to surface the NEXT FATAL.
  - Read engine-side `ShaderImplementation.h` base classes:
    - `ShaderImplementationGraphicsData` has 1 virtual: `~ShaderImplementationGraphicsData()`
    - `StaticShaderGraphicsData` has 3 virtuals: dtor + `update(StaticShader const &) = 0` + `getTextureSortKey() const = 0`
  - Verified `setBadVertexShaderStaticShader` + `setStaticShader` were already wired by Plan 11-06 (`Direct3d11_StateCache.cpp:791,800` — no-op recordings).
- **Root cause:** Two `STUB()` lines at `Direct3d11.cpp:583-584` (createShaderImplementationGraphicsData / createStaticShaderGraphicsData). The wrappers themselves did not yet exist in `Direct3d11/src/win32/`.
- **Fix:** Authored 4 new files (h + cpp pairs for ShaderImplementationData + StaticShaderData), wired into `Direct3d11.cpp` install path + `Direct3d11.vcxproj` + `.filters`. Both wrappers minimum-viable Iteration 1: construct + minimal accessors + no-op apply.
  - `Direct3d11_ShaderImplementationData.h` (~90 ln) — class definition + accessor surface.
  - `Direct3d11_ShaderImplementationData.cpp` (~115 ln) — install + remove + ctor + dtor + apply (no-op with documented per-pass mapping intent).
  - `Direct3d11_StaticShaderData.h` (~100 ln) — class definition + ms_active cache slots + virtual override surface.
  - `Direct3d11_StaticShaderData.cpp` (~165 ln) — install + remove + beginFrame + ctor + dtor + update + getTextureSortKey + apply (active-shader cache + no-op record).
  - `Direct3d11.cpp` — added 2 includes, 2 factory `_impl` bodies, install order (after `Direct3d11_Metrics::install()`), reverse-order in `remove_impl`, replaced 2 `STUB()` lines with real factory bindings.
  - `Direct3d11.vcxproj` + `.filters` — added 4 ItemGroup entries (2 ClCompile + 2 ClInclude).
- **Verification:**
  - `MSBuild -t:Direct3d11` EXIT=0. `gl11_d.dll` 1,397,760 bytes (Plan 11-06 was 1,392,128; +5,632 bytes).
  - `MSBuild -t:Direct3d9` EXIT=0 (D-05 protection — pre-existing MSB8012 carry-forward warnings only).
  - `MSBuild -t:SwgClient` EXIT=0.
  - D-13 grep clean: 0 non-comment hits for `D3DPOOL_MANAGED | OnLostDevice | OnResetDevice` in `Direct3d11/`.
  - D-05 diff empty: `git diff 358fe1a7e..HEAD -- src/engine/client/application/Direct3d9/` returns empty.
  - FFP scan clean: 0 `#ifdef FFP | D3DTSS_ | D3DTOP_` functional code in new files (4 comment lines documenting D-04a omission rationale only).
  - STUB count delta: Plan 11-06 ended at 29 STUBs; Iteration 1 removes 2 (createShaderImplementationGraphicsData + createStaticShaderGraphicsData) → **27 STUBs remaining**.
- **Commits:**
  - `972b02427`: feat(11-07): author Direct3d11_ShaderImplementationData + StaticShaderData wrappers (Iter 1)
  - `ff73b7e11`: feat(11-07): wire shader-implementation + static-shader factory slots (Iter 1)
- **Awaiting:** Kenny smoke under `client_d.cfg [ClientGraphics] rasterMajor=11`. Expected outcomes (in order of likelihood):
  - Most likely: another FATAL further down the engine's load chain. Candidates ranked from highest likelihood:
    1. Point-sprite setter (`setPointSize/Max/Min/ScaleEnable/ScaleFactor/SpriteEnable`) STUBs are 6 of the remaining 27 — fire from the particle subsystem on first emit; may not surface until in-game.
    2. Cursor setter (`setMouseCursor / showMouseCursor`) — fires during UI initialization; might land after `SetupClientGraphics::install` completes.
    3. Visual hard-edge: visible window without geometry (clear-to-color steady state) — most aspirational outcome.
  - Less likely: hard crash without FATAL — would indicate a non-FATAL invariant violation (access violation in a non-traversal-protected codepath). If this surfaces, the call stack from the OS exception handler is the next iteration's first investigation lead.

---

## Future iterations (placeholders)

To be filled as Kenny surfaces each new FATAL or visual bug. Each entry follows the 6-field shape (Date / Symptom / Hypothesis / Investigation / Root cause / Fix / Verification + Commit).

## Final state (filled at plan close)

- Tatooine outdoor ≥5 min: PENDING
- Cantina interior ≥5 min: PENDING
- All 4 subsystems visible: terrain ?, skeletal ?, particles ?, HUD ?
- Acceptance: PENDING — ready for Plan 08 (DPVS remeasure) + Plan 09 (visual-parity screenshots) at plan close
