# Phase 34: x64 D3D11 Renderer - Context

**Gathered:** 2026-06-17
**Status:** Ready for planning

<domain>
## Phase Boundary

Bring the **D3D11 renderer plugin (gl11)** to x64, so the already-linking/booting x64 client
(stood up in Phase 33 under the D3D9 plugins) also boots to character select under
`rasterMajor=11` and **renders the world at the v2.2 visual-parity bar** — without regressing the
shipped 32-bit gl11 build. Requirement **X64-03** (the second, separate renderer boot gate).

**In scope:**
- Add `Debug|x64` + `Release|x64` configurations to `Direct3d11.vcxproj` by **mechanically
  mirroring gl05's existing x64 blocks** (D-01): x64 `ProjectConfiguration`s, the
  `x64-platform.props` import, isolated `compile/win32/Direct3d11/x64/<cfg>` OutDir/IntDir,
  `MachineX64`, `jpeg62-x64.lib` swap, postbuild → `stage-x64/`.
- Register the gl11 GUID's `Debug|x64` (and `Release|x64`) `.Build.0` entries in `swg.sln`.
- Link **Debug|x64 gl11** clean (`/FORCE` link-grep `unresolved external symbol` == 0; `dumpbin`
  machine x64), stage `gl11_d.dll` to `stage-x64/`.
- Boot the x64 client to character select under `rasterMajor=11` and verify the **v2.2 parity bar**
  on the canonical regression triad via a RenderDoc x64-vs-Win32 A/B diff (D-02), using
  **Debug|x64 as the primary verification surface** (D-03).
- Confirm 32-bit gl11 still boots under `rasterMajor=11` (SC#3 non-regression) and that gl05 x64
  still boots (cross-renderer non-regression — both plugins now share the one x64 client).

**Out of scope (this phase):**
- **D3DX removal** — gl11 never used D3DX (links `d3d11.lib;d3dcompiler.lib;dxgi.lib` only, all
  Windows-SDK x64); its shader path is already `D3DCompile`. The Phase-33 dominant cost is **absent
  here**, not deferred.
- **Linking/validating Release|x64** — the Release|x64 block is authored (mirrored) but **only
  Debug|x64 is exercised** this phase (D-04), matching Phase 33. Release|x64 is unproven for ALL
  plugins; first Release|x64 link is a later consolidation.
- **Proactive D3D11-specific x64 re-audit** — gl11 source was already swept x64-clean in Phase 31
  (D-01); a fresh deep audit would re-tread completed work.
- **Miles audio x64** (Phase 35), **door-snap / OOM verification + CORNERSNAP strip** (Phase 36),
  **dropping DPVS** (post-x64 evaluation) — all already roadmapped elsewhere.

</domain>

<decisions>
## Implementation Decisions

### Correctness posture (X64-03)
- **D-01 (MECHANICAL MIRROR + fix-what-breaks):** Mirror gl05's x64 `.vcxproj` blocks for gl11,
  link, and fix **only what the x64 link/boot actually surfaces**. This is justified — not a
  shortcut — because **gl11 was explicitly IN Phase 31's x64-clean sweep**: Phase 31 D-03 scoped
  *all four* renderer plugins (gl05/06/07/**11**), and `compile-all.ps1` compiled the Direct3d11
  TUs x64-clean for the C4311/C4312/C4244 width-defect class (e.g. the
  `Direct3d11_StaticVertexBufferData.cpp:167` / `Direct3d11_DynamicVertexBufferData.cpp:372`
  re-truncation → hash-to-`uintptr_t` fix landed in 31-04). gl11 is therefore **NOT an unswept
  plugin like dpvs was** (dpvs got its own correctness pass in Phase 33 only because it was *out*
  of Phase 31 scope). Residual risk = the **N2 `C4267` (size_t→int) carry-forward warnings** Phase
  31 did not treat as errors, plus any link-time / runtime D3D11 surprise that only appears once the
  plugin actually links + runs x64. Do NOT pre-emptively re-audit the cbuffer/shader-cache/reflection
  paths; let the x64 link + the parity A/B surface anything real.

### Visual-parity verification (X64-03 SC#2)
- **D-02 (Regression triad + RenderDoc x64-vs-Win32 A/B):** Verify the **canonical regression
  triad** — dressed character-select + Tatooine + cantina interior — at the v2.2 parity bar. gl11 is
  THE renderer where the entire v2.2 parity battle was fought (dot3, bump, faces, terrain blend,
  mini-map, gamma, cantina FFP combiner), so a literal boot-only check is insufficient. Method:
  **RenderDoc CAN capture D3D11**, so capture the same scene under **gl11 32-bit Debug (the
  v2.2-validated reference) vs gl11 Debug|x64** and diff — a clean **same-renderer, same-config,
  arch-only A/B** that isolates any x64-codegen regression. (This sidesteps the standing
  "RenderDoc cannot capture D3D9" limitation entirely — both sides of the diff are D3D11.) A full
  v2.2 parity re-sweep was considered overkill if the triad + A/B comes back clean.

### Debug-x64 verification path
- **D-03 (Exploit Debug|x64 as the PRIMARY verify surface):** x64 removes the ~2 GB address-space
  ceiling that hung the 32-bit Debug build under extended play (the chronic OOM that forced prior
  phases to A/B on the Release build). So **Debug|x64 finally allows extended-session play with the
  assert / `DEBUG_FATAL` oracles live** — use it as the main verification surface for this phase.
  Bonus: a clean extended Debug|x64 session pre-stages the Phase-36 OOM-class confidence (it does
  NOT close VERIFY-02 — that stays Phase 36 — but it's the first real signal). The parity-bar A/B
  (D-02) therefore runs in Debug|x64 on both arches (32-bit Debug reference vs Debug|x64).

### Build-config scope
- **D-04 (Debug|x64 ONLY this phase):** Author both x64 blocks (mirror), but **only link + validate
  `Debug|x64`** for gl11 — matching Phase 33, which authored Release|x64 blocks for gl05/06/07 but
  only ever exercised Debug|x64. Release|x64 is currently unproven for **every** plugin + SwgClient;
  the first Release|x64 link should be a single later consolidation across all plugins at once, not
  a one-off here.

### Claude's Discretion
- Exact gl11 x64 `.vcxproj` block layout (PropertyGroup/ImportGroup/ItemDefinitionGroup placement)
  — mirror gl05's committed structure; planner picks the precise diff.
- Whether the DllExport x64 bridge already covers gl11 (it was ported for gl05/06/07 in Phase 33 T2;
  gl11 imports the same `DllExport.dll` host bridge) — confirm at link; re-use, don't re-port.
- RenderDoc capture EIDs / which specific draws to diff in the A/B (researcher/executor picks the
  representative dot3/bump/terrain draws per the v2.2 parity memories).
- Whether any N2 `C4267` site in the Direct3d11 TUs needs a real width fix vs is benign — decide per
  the actual x64 link/runtime behavior (D-01 fix-what-breaks).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope & milestone plan
- `.planning/ROADMAP.md` §"Phase 34: x64 D3D11 Renderer" — goal + 3 success criteria
- `.planning/REQUIREMENTS.md` — **X64-03** (gl11 x64 + boot under rasterMajor=11); X64-01/02/04
  (Phase 33, Complete — the x64 platform/client gl11 slots into)
- `.planning/STATE.md` §"v3.0 x64 Port — the plan" + §Current Position — the extended core invariant
  (boot-to-char-select parity in x64 under BOTH rasterMajor=5 AND =11), the separate-renderer boot
  gates, the `/FORCE` link-grep=0 acceptance, the `stage-x64/` isolation from the 32-bit `stage/`

### Prior-phase context (MUST read — Phase 34 builds directly on these)
- `.planning/phases/33-x64-build-platform-d3d9-renderers/33-CONTEXT.md` — the x64 platform mechanics
  this phase mirrors (x64-platform.props, stage-x64 isolation, jpeg62-x64.lib swap, DllExport x64
  bridge, swg.sln x64 GUID registration); explicitly defers gl11 to Phase 34
- `.planning/phases/31-64-bit-correctness-foundation/31-CONTEXT.md` §D-03 — the in-scope set that
  INCLUDED all 4 plugins (gl05/06/07/**11**); the shared-header ABI cascade trap
- `.planning/phases/31-64-bit-correctness-foundation/31-04-SUMMARY.md` — the Direct3d11 width-defect
  fixes already landed (StaticVertexBufferData/DynamicVertexBufferData re-truncation → hash-to-uintptr_t)
- `.planning/phases/31-64-bit-correctness-foundation/31-PHASE33-RESIDUAL-WORKLIST.md` — the N1/N2
  carry-forward (C4267 width-narrowing not-yet-error, /we4311 guardrail) that may touch gl11 at the
  x64 link
- STATE.md §"RESUME (2026-06-18)" / Plan 33-05/06 — how the FIRST x64 link was achieved (the
  /FORCE+ForceFileOutput+/SAFESEH:NO+/VERBOSE recipe, DllExport x64 port, MachineX64, stage-x64
  postbuild); gl11 reuses this exact x64 link discipline

### Implementation targets
- `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj` — currently **pure
  Win32** (Debug/Optimized/Release|Win32 only, NO x64); the platform-add target. Link inputs already
  `d3d11.lib;d3dcompiler.lib;dxgi.lib` (NO d3dx9) — no lib swap beyond `libjpeg.lib`→`jpeg62-x64.lib`
- `src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj` §"Phase 33 (X64-02)"
  Debug|x64 + Release|x64 link blocks — **the exact pattern to mirror** for gl11 (the x64
  ItemGroup/PropertyGroup/ImportGroup/ItemDefinitionGroup + stage-x64 postbuild)
- `src/build/win32/x64-platform.props` — the committed x64 platform props gl11's x64 configs import
- `src/build/win32/swg.sln` — gl11 GUID needs `.Debug|x64.Build.0` (+ Release|x64) registration,
  mirroring the Phase-33 promotion of the gl05/06/07/SwgClient GUIDs
- `stage-x64/` — the isolated x64 deploy dir (distinct from the shipped 32-bit `stage/`); gl11_d.dll
  + d3dcompiler_47.dll land here

### Visual-parity reference (the v2.2 bar gl11 must still meet on x64)
- Memory pointers (D3D11 parity battle outcomes to re-confirm don't regress on x64):
  `project_d3d11_ffp_combiner_cascade` (cantina interior parity COMPLETE), the dot3/bump/face/terrain
  /mini-map/gamma fixes catalogued in MEMORY.md (2026-06-08..12 sweep "rendering bug list CLEARED")
- `feedback_renderdoc_d3d9_vs_d3d11_is_the_diagnostic` + `project_renderdoc_mcp_server_idea` —
  RenderDoc CLI/MCP at `D:\Code\renderdoc-mcp\v0.3.0\...\bin\`; **here it's a D3D11-vs-D3D11 (Win32
  vs x64) A/B**, which RenderDoc CAN capture on both sides

### Build/convention constraints
- `AGENTS.md` §Build — `$env:MSBUILD` (VS18/v145), `/nodeReuse:false`, run from PowerShell, the
  `/FORCE` link-grep (0 `unresolved external symbol`), shared-header ABI cascade (a shared-header
  touch requires rebuilding ALL plugins), `rasterMajor=11` = gl11, Debug exe → `client_d.cfg`
- `AGENTS.md` §"Config (.cfg) safety" — never write `.cfg` via PS Set-Content/Out-File (UTF-8 BOM
  crashes Release boot); set `rasterMajor=11` in the x64 client's cfg via a BOM-safe editor

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **gl05's committed x64 `.vcxproj` blocks** (Direct3d9.vcxproj, "Phase 33 (X64-02)" section) — the
  literal template to mirror for gl11. Only the lib list differs trivially (gl11 already has the
  right d3d11/d3dcompiler/dxgi inputs; only `libjpeg.lib`→`jpeg62-x64.lib` is the shared swap).
- **`src/build/win32/x64-platform.props`** — the committed, proven x64 compile/platform flag set
  (grown from Phase 31's `x64-scratch/x64-compile.props`).
- **Phase 31's completed Direct3d11 x64 width sweep** — gl11 source is already x64-compile-clean for
  C4311/C4312/C4244; Phase 34 is a **build/link/verify exercise, not a code-correctness swamp** (the
  same posture Phase 33 had for the already-swept engine libs).
- **The Phase-33 x64 link recipe** (ForceFileOutput + /SAFESEH:NO + /VERBOSE + MachineX64 +
  stage-x64 postbuild) — reuse verbatim for the gl11 Debug|x64 link.
- **DllExport x64 bridge** — already ported in Phase 33 (T2); gl11 imports the same host bridge, so
  it should resolve at link without new work (confirm, don't re-port).

### Established Patterns
- **No shared common `.props`** across the ~132 `.vcxproj`s historically, but Phase 33 introduced
  `x64-platform.props` specifically for the x64 platform-add — gl11's x64 configs import it (same as
  gl05/06/07).
- **`/FORCE` link masks unresolved externals** — exit 0 ≠ clean link; grep the gl11 x64 link log for
  `unresolved external symbol` (must be 0). Primary x64-link acceptance signal.
- **Shared-header ABI cascade** — if any shared header is touched while fixing a gl11 x64 surprise,
  ALL plugins (gl05/06/07/11) must rebuild, not just gl11. (Unlikely this phase — fix-what-breaks
  should be plugin-local.)
- **Postbuild auto-deploy** → `stage-x64/`; x64 validation runs from `stage-x64/` with
  `rasterMajor=11` set in the x64 client's `client_d.cfg`.

### Integration Points
- `Direct3d11.vcxproj` `<ProjectConfiguration>` / `<PropertyGroup>` / `<ImportGroup>` /
  `<ItemDefinitionGroup>` — the x64 platform surface to add.
- `swg.sln` solution configurations — gl11 GUID x64 `.Build.0` registration.
- The x64 SwgClient (already linking/booting under gl05) loads `gl11_d.dll` as the renderer plugin
  when `rasterMajor=11` — gl11 slots into the existing x64 client, no SwgClient change expected.

</code_context>

<specifics>
## Specific Ideas

- "gl11 is the renderer where the entire v2.2 parity battle was fought" — the reason a literal
  boot-only check was rejected for D-02; the triad + arch-only A/B is the proportionate verify.
- The RenderDoc A/B is deliberately framed as **D3D11(Win32) vs D3D11(x64)** — the cleanest possible
  diff (same renderer, same scene, same config, only the arch changes), and it dodges the standing
  "RenderDoc cannot capture D3D9" constraint that shaped earlier parity work.
- Debug|x64 being newly viable for extended play (no 2 GB OOM) is treated as a genuine workflow
  unlock for this phase, and a first (non-binding) signal toward the Phase-36 OOM-class verdict.

</specifics>

<deferred>
## Deferred Ideas

- **Release|x64 link for ALL plugins + SwgClient** — first Release|x64 link should be one
  consolidation pass across every plugin, not a gl11-only one-off (D-04). Later in v3.0.
- **Miles 9.3b audio x64 port** — Phase 35 (AUDIO-01/02). x64 audio stays silent-by-design until then.
- **Door-snap (VERIFY-01) / OOM-class (VERIFY-02) confirmation + CORNERSNAP `_DEBUG` probe strip
  (VERIFY-03)** — Phase 36. A clean extended Debug|x64 gl11 session here is a first signal toward
  VERIFY-02 but does NOT close it.
- **Evaluate dropping DPVS entirely (post-x64)** — Phase-23 D3D11 verdict already flips indoor
  occlusion off; revisit after the full x64 client lands. Its own later evaluation.

### Reviewed Todos (not folded)
- `2026-06-13-64bit-x64-port.md` — the whole-milestone driver (Phases 33–36); this phase is one
  prerequisite, not the entire x64 port. Reviewed-not-folded, consistent with Phases 31/32/33.

</deferred>

---

*Phase: 34-x64-d3d11-renderer*
*Context gathered: 2026-06-17*
