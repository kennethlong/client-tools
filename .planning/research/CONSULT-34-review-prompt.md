# Cross-AI Plan Review Request — Phase 34 (x64 D3D11 Renderer)

You are reviewing implementation plans for a software-engineering phase. Provide **structured,
critical** feedback on plan quality, completeness, and risk. This is adversarial review — your job is
to catch blind spots the other reviewers and the planner missed. Do not rubber-stamp.

## Project Context

**Project:** whitengold — a Star Wars Galaxies client modernisation port (C++20, MSVC, MSBuild). The
active milestone is **v3.0 — x64 Port**: port the modernized 32-bit client to a native x64 build,
keeping BOTH renderers (D3D9 gl05/06/07 + D3D11 gl11) bootable to character select, to eliminate
32-bit-bound defects (a cantina door-snap float-codegen transient, chronic address-space OOM) and
remove the x64-hostile D3DX dependency. SWG Restoration ships a stable x64 D3D9 client as a reference.

**Prior phases (all complete):**
- Phase 31: whole tree compiles x64-clean (x87→SSE intrinsics, pointer/int truncation fixes,
  serialization-width audit). Its x64-clean sweep scoped ALL FOUR renderer plugins gl05/06/07/**11**.
- Phase 32: D3DX → d3dcompiler_47 (D3DCompile) port + D3DX removal (for the D3D9 plugins).
- Phase 33: x64 build platform stood up; the D3D9 plugins (gl05/06/07) build x64 and the x64 client
  boots to char-select under rasterMajor=5/6/7. Introduced `x64-platform.props`, the isolated
  `stage-x64/` deploy dir, the `jpeg62-x64.lib` swap, the DllExport x64 host-bridge, and swg.sln x64
  GUID registration. **Explicitly deferred gl11 (D3D11) to Phase 34.**

**Key build/run facts (LOCKED — treat as given, do not re-derive):**
- `rasterMajor` in the active .cfg selects the renderer plugin: 5=gl05 (D3D9), 11=gl11 (D3D11).
  `rasterMajor=9` is FATAL.
- `SwgClient_d.exe` (Debug) reads `client_d.cfg`. The x64 client lives in `stage-x64/` (isolated from
  the shipped 32-bit `stage/`).
- The build links SwgClient.exe under `/FORCE`, which downgrades `unresolved external symbol` to
  warnings and still emits a binary — so **exit 0 ≠ clean link**; the acceptance signal is grepping
  the link log for 0 `unresolved external symbol`. (The plugin DLL link itself does NOT use /FORCE.)
- Writing a `.cfg` via PowerShell Set-Content/Out-File adds a UTF-8 BOM that crashes the Release
  client at boot — cfg edits must be BOM-safe.
- Touching a SHARED header (consumed by the plugins) silently breaks ABI with stale plugin DLLs →
  deterministic boot crash; a shared-header touch requires rebuilding ALL plugins (gl05/06/07/11).
- RenderDoc CANNOT capture D3D9, but CAN capture D3D11 — so a gl11-Win32-vs-gl11-x64 A/B is a clean
  same-renderer, same-config, arch-only diff (both sides D3D11).
- gl11 never used D3DX — it links only Windows-SDK x64 libs (d3d11.lib;d3dcompiler.lib;dxgi.lib) and
  its shader path is already D3DCompile. So Phase 32's dominant cost (D3DX removal) is ABSENT for gl11.

## Phase 34 — Goal & Success Criteria (from ROADMAP)

**Goal:** The D3D11 renderer plugin (gl11) builds as x64 and carries the x64 client to character
select under `rasterMajor=11`, bringing the second renderer to x64 parity.
**Requirement:** X64-03.
**Success Criteria (must be TRUE):**
1. gl11 builds as x64 (`gl11_d.dll` / `gl11_r.dll`, dumpbin machine x64).
2. The x64 client boots to character select under rasterMajor=11 and renders the world at the v2.2
   visual-parity bar.
3. The 32-bit gl11 build still boots to character select under rasterMajor=11 (no Win32 regression).

## Implementation Decisions (from CONTEXT)

- **D-01 (MECHANICAL MIRROR + fix-what-breaks):** Mirror gl05's committed x64 .vcxproj blocks for
  gl11, link, and fix only what the x64 link/boot actually surfaces. gl11 was in Phase 31's x64-clean
  sweep, so it's NOT an unswept plugin. Do NOT pre-emptively re-audit cbuffer/shader-cache/reflection
  paths; let the link + parity A/B surface anything real. Residual risk = benign C4267 (size_t→int)
  carry-forward warnings + any link/runtime D3D11 x64 surprise.
- **D-02 (Regression triad + RenderDoc x64-vs-Win32 A/B):** Verify the canonical triad — dressed
  character-select + Tatooine + cantina interior — at the v2.2 parity bar via a gl11-32bit-Debug
  (reference) vs gl11-Debug|x64 arch-only A/B. gl11 is THE renderer where the entire v2.2 parity
  battle was fought (dot3, bump, faces, terrain blend, mini-map, gamma, cantina FFP combiner), so a
  boot-only check is insufficient.
- **D-03 (Debug|x64 as the PRIMARY verify surface):** x64 removes the ~2 GB ceiling that hung 32-bit
  Debug under extended play, so Debug|x64 finally allows extended-session play with assert/DEBUG_FATAL
  oracles live. A clean extended Debug|x64 session is a first (non-binding) signal toward a later
  OOM-class verdict.
- **D-04 (Debug|x64 ONLY this phase):** Author both Debug|x64 + Release|x64 .vcxproj blocks (mirror),
  but only link + validate Debug|x64. Release|x64 is unproven for every plugin; first Release|x64 link
  is a later single consolidation across all plugins.

## Plan 01 (Wave 1) — Build + Link gl11 x64

Files: `Direct3d11.vcxproj`, `swg.sln`. Three tasks:
1. Mirror gl05's six x64 regions into `Direct3d11.vcxproj` (ProjectConfigurations, Configuration
   PropertyGroups, PropertySheet ImportGroups importing `x64-platform.props`, isolated x64 OutDir/IntDir
   PropertyGroups → `compile/win32/Direct3d11/x64/<cfg>/`, two x64 ItemDefinitionGroups, two PCH-Create
   conditions on FirstDirect3d11.cpp). Author BOTH Debug|x64 + Release|x64; NO Optimized|x64 (gl05 has
   none). gl11-specialized: keep `DIRECT3D11_EXPORTS` (no FFP/VSPS — those are D3D9-flavor switches);
   swap `libjpeg.lib`→`jpeg62-x64.lib`; lib list otherwise unchanged
   (odbc32;odbccp32;winmm;delayimp;dxguid;d3d11;d3dcompiler;dxgi;legacy_stdio_definitions); MachineX64;
   DelayLoadDLLs DllExport.dll; NO /SAFESEH:NO (x86-only), NO /FORCE/BaseAddress (those live on the exe,
   not the plugin); PostBuild copies gl11_d.dll+pdb+`%SystemRoot%\Sysnative\d3dcompiler_47.dll` →
   `stage-x64/`. The x64-platform.props relative path is IDENTICAL to gl05's (same dir depth).
2. Register the gl11 GUID `{DC3F2C16-...}` x64 lines in `swg.sln` (4 lines: Debug|x64 + Release|x64,
   ActiveCfg + Build.0), tab-indented, after gl11's Win32 lines, BOM-safe (Edit tool).
3. Link Debug|x64 from PowerShell (`$env:MSBUILD ... /t:Direct3d11 /p:Configuration=Debug
   /p:Platform=x64 /nodeReuse:false`), after deleting the output DLL + killing stale MSBuild/cl/link
   nodes. Gate: grep log for 0 `unresolved external symbol`, 0 `LNK1181`; `dumpbin /headers` reads 8664;
   gl11_d.dll+pdb+d3dcompiler_47.dll staged to stage-x64/; 32-bit `stage/gl11_d.dll` byte-unchanged.
   Fix-what-breaks PLUGIN-LOCAL only (a shared-header fix triggers the ABI cascade → STOP and flag).

## Plan 02 (Wave 2) — Boot + Parity Verify

File: `stage-x64/client_d.cfg`. Three tasks:
1. Set rasterMajor=11 (BOM-safe) in stage-x64/client_d.cfg; confirm exe+cfg+gl11_d.dll all machine
   8664 and lined up in stage-x64/; boot stage-x64/SwgClient_d.exe to DRESSED char-select under gl11
   (Debug|x64, assert oracles live); capture the triad (dressed char-select + Tatooine + cantina
   interior) under gl11 Debug|x64 in RenderDoc; enumerate representative dot3/bump/terrain/FFP EIDs.
2. Capture the SAME triad under 32-bit gl11 Debug (the v2.2 reference); run the arch-only A/B diff
   (32-bit Debug LEFT vs Debug|x64 RIGHT) on the representative draws via renderdoc-cli
   `pipeline`/`pixel`/`debug pixel --trace`/`export-rt`; record per-draw PASS/divergence. Then run the
   two non-regression boots: 32-bit gl11 rasterMajor=11 → char-select (stage/gl11_d.dll still machine
   14C/x86); gl05 x64 rasterMajor=5 → char-select (gl05 still loads in the shared x64 client); restore
   cfg to 11.
3. Human checkpoint (blocking): sign off the v2.2 parity bar across the triad + four green boots.

## Review Instructions

Analyze the plans and provide, in markdown:

1. **Summary** — one-paragraph assessment.
2. **Strengths** — what's well-designed (bullets).
3. **Concerns** — issues/gaps/risks, each tagged severity **HIGH / MEDIUM / LOW**. Be specific and
   actionable. Probe especially:
   - Is the "mechanical mirror" assumption safe? What could differ between gl05 (D3D9) and gl11 (D3D11)
     beyond the lib list — and would the plan catch it?
   - Is the verification (link-grep + dumpbin + RenderDoc arch-only A/B) actually sufficient to prove
     SC#1/SC#2/SC#3? Any oracle gaps? Any way a real x64 regression passes the gate?
   - Dependency/ordering issues, missing edge cases, missing failure-handling, missing rollback.
   - Scope creep or over-engineering vs under-specification.
   - Anything about D3D11-specific x64 risk (cbuffer alignment, shader reflection indices, the
     compile-once shader-cache path) that D-01's "don't pre-audit" posture might let slip.
4. **Suggestions** — concrete improvements.
5. **Risk Assessment** — overall LOW / MEDIUM / HIGH with justification.

Be concise but rigorous. Lead with your highest-severity concern.
