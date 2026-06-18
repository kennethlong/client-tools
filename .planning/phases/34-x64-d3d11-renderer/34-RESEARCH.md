# Phase 34: x64 D3D11 Renderer (gl11) - Research

**Researched:** 2026-06-17
**Domain:** MSBuild x64 platform-add (vcxproj mirror + swg.sln registration) → x64 link → boot/parity verify
**Confidence:** HIGH (every claim verified against the live tree; gl05 template is committed and exact)

## Summary

Phase 34 is a **mechanical build/link/verify exercise**, not a code-correctness pass. gl11's source
is already x64-compile-clean (Phase 31 D-03 swept all four plugins including Direct3d11; the
hash-to-`uintptr_t` re-truncation fixes are confirmed present in the live tree), gl11 never used
D3DX (it links only Windows-SDK x64 libs `d3d11.lib;d3dcompiler.lib;dxgi.lib`), and the gl05
Direct3d9.vcxproj carries a **committed, exact x64 block template** ("Phase 33 (X64-02)" section)
that gl11 mirrors near-verbatim. The only substantive lib-list change is the shared
`libjpeg.lib → jpeg62-x64.lib` swap (jpeg62-x64.lib is present in the tree). All x64 platform
scaffolding (`x64-platform.props`, `stage-x64/`, the DllExport x64 bridge, jpeg62-x64.lib) already
exists from Phase 33 and is **reused, not re-built**.

The work decomposes into: (1) add two x64 `ProjectConfiguration`s + the `x64-platform.props` import +
isolated `compile/win32/Direct3d11/x64/<cfg>` OutDir + two x64 `ItemDefinitionGroup`s (mirroring
gl05) to `Direct3d11.vcxproj`; (2) add the four `.Debug|x64`/`.Release|x64` GUID lines for the gl11
GUID `{DC3F2C16-...}` to `swg.sln`; (3) link Debug|x64 clean (`/FORCE` link-grep == 0, dumpbin machine
x64), stage `gl11_d.dll` to `stage-x64/`; (4) boot the existing x64 SwgClient under `rasterMajor=11`
and run a RenderDoc D3D11(Win32)-vs-D3D11(x64) arch-only A/B on the regression triad; (5) confirm
32-bit gl11 + gl05 x64 non-regression.

**Primary recommendation:** Mirror gl05's committed x64 `.vcxproj` blocks into `Direct3d11.vcxproj`
verbatim (swap the gl05 lib list for gl11's existing `d3d11/d3dcompiler/dxgi` set, `libjpeg.lib →
jpeg62-x64.lib`, output `gl11_d.dll`), register the gl11 GUID's four x64 lines in `swg.sln`, link
Debug|x64, then verify by boot + RenderDoc arch-only A/B. Do NOT re-audit gl11's D3D11 source.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| x64 platform configs on gl11 | Build system (`Direct3d11.vcxproj`) | `x64-platform.props` (shared compile flags) | Per-project platform-add; no shared common props except the Phase-33 x64 platform props |
| Solution-level x64 build wiring | `swg.sln` solution configs | — | `.Build.0` entries decide what actually builds for the x64 solution config |
| gl11 plugin x64 link (d3d11/dxgi/d3dcompiler) | Direct3d11 renderer plugin | Windows SDK x64 (toolset-supplied libs) | All link inputs are Windows-SDK x64; no DXSDK, no D3DX |
| Plugin↔host symbol bridge | DllExport.dll (x64, ported P33) | gl11 `DelayLoadDLLs` + `ProjectReference` | gl11 imports the same host bridge gl05/06/07 use; resolves at link with no new port |
| Renderer selection at runtime | SwgClient (`rasterMajor=11`) | gl11_d.dll loaded from stage-x64/ | The existing x64 client loads gl11 as a plugin; no SwgClient change |
| Visual-parity verification | RenderDoc CLI/MCP (D3D11-vs-D3D11 A/B) | Debug|x64 assert/DEBUG_FATAL oracles | RenderDoc CAN capture D3D11 on both arches → clean arch-only diff |

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| X64-03 | The D3D11 renderer plugin (gl11) builds as x64 and the x64 client boots to character select under `rasterMajor=11` | gl05 committed x64 template (exact mirror), gl11 source already x64-clean (P31 D-03), jpeg62-x64.lib + x64-platform.props + DllExport-x64 + stage-x64 all present from P33; verification via dumpbin machine-x64 + `/FORCE` link-grep==0 + boot + RenderDoc arch-only A/B on the triad |
</phase_requirements>

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01 (MECHANICAL MIRROR + fix-what-breaks):** Mirror gl05's x64 `.vcxproj` blocks for gl11,
  link, and fix **only what the x64 link/boot actually surfaces**. gl11 was IN Phase 31's x64-clean
  sweep (D-03 scoped all four plugins gl05/06/07/**11**); the StaticVertexBufferData/
  DynamicVertexBufferData re-truncation → hash-to-`uintptr_t` fix landed in 31-04. gl11 is NOT an
  unswept plugin like dpvs was. Residual risk = the **N2 C4267 (size_t→int) carry-forward warnings**
  + any link/runtime D3D11 surprise that only appears once gl11 links + runs x64. **Do NOT
  pre-emptively re-audit** the cbuffer/shader-cache/reflection paths; let the link + parity A/B
  surface anything real.
- **D-02 (Regression triad + RenderDoc x64-vs-Win32 A/B):** Verify the canonical regression triad —
  **dressed character-select + Tatooine + cantina interior** — at the v2.2 parity bar. Method:
  capture the same scene under **gl11 32-bit Debug (the v2.2-validated reference) vs gl11 Debug|x64**
  and diff — a clean same-renderer, same-config, **arch-only A/B** (both sides D3D11, sidestepping
  the "RenderDoc cannot capture D3D9" limitation). A full v2.2 parity re-sweep is overkill if the
  triad + A/B comes back clean.
- **D-03 (Debug|x64 as the PRIMARY verify surface):** x64 removes the ~2 GB ceiling that hung 32-bit
  Debug under extended play, so **Debug|x64 finally allows extended-session play with the assert /
  `DEBUG_FATAL` oracles live** — use it as the main verification surface. A clean extended Debug|x64
  session is a first (non-binding) signal toward the Phase-36 OOM-class verdict (does NOT close
  VERIFY-02). The parity A/B runs in Debug|x64 (32-bit Debug reference vs Debug|x64).
- **D-04 (Debug|x64 ONLY this phase):** Author both x64 blocks (mirror), but **only link + validate
  `Debug|x64`** for gl11 — matching Phase 33. Release|x64 is unproven for every plugin + SwgClient;
  the first Release|x64 link is a single later consolidation across all plugins, not a one-off here.

### Claude's Discretion
- Exact gl11 x64 `.vcxproj` block layout (PropertyGroup/ImportGroup/ItemDefinitionGroup placement) —
  mirror gl05's committed structure; planner picks the precise diff.
- Whether the DllExport x64 bridge already covers gl11 (ported for gl05/06/07 in P33 T2; gl11 imports
  the same `DllExport.dll` host bridge) — **confirm at link; re-use, don't re-port.**
- RenderDoc capture EIDs / which specific draws to diff in the A/B — researcher/executor picks the
  representative dot3/bump/terrain draws per the v2.2 parity memories.
- Whether any N2 C4267 site in the Direct3d11 TUs needs a real width fix vs is benign — decide per the
  actual x64 link/runtime behavior (D-01 fix-what-breaks).

### Deferred Ideas (OUT OF SCOPE)
- **Release|x64 link for ALL plugins + SwgClient** — first Release|x64 link is one consolidation pass
  across every plugin, not a gl11-only one-off (D-04). Later in v3.0.
- **Miles 9.3b audio x64 port** — Phase 35 (AUDIO-01/02). x64 audio stays silent-by-design until then.
- **Door-snap (VERIFY-01) / OOM-class (VERIFY-02) confirmation + CORNERSNAP `_DEBUG` probe strip
  (VERIFY-03)** — Phase 36. A clean extended Debug|x64 gl11 session is a first signal toward VERIFY-02
  but does NOT close it.
- **Evaluate dropping DPVS entirely (post-x64)** — its own later evaluation.
- **D3DX removal** — gl11 never used D3DX; absent here, not deferred.
- **Proactive D3D11-specific x64 re-audit** — gl11 was already swept x64-clean in Phase 31.
</user_constraints>

---

## Standard Stack

This is a build-system/link phase. The "stack" is the committed Phase-33 x64 infrastructure, all
already present in the tree and reused as-is.

### Core
| Asset | Location | Purpose | Status |
|-------|----------|---------|--------|
| gl05 x64 template | `Direct3d9.vcxproj` lines 16-23, 48-59, 75-82, 106-118, 299-377 | The exact `.vcxproj` x64 block pattern to mirror | `[VERIFIED: read live]` committed |
| `x64-platform.props` | `src/build/win32/x64-platform.props` | Shared x64 ClCompile flags (warnings/std/RTTI/EH, N1 /we4311;4312, DEBUG_LEVEL split) | `[VERIFIED: read live]` committed |
| `jpeg62-x64.lib` | `src/external/3rd/library/libjpeg/lib/jpeg62-x64.lib` | The x64 libjpeg import lib (replaces 32-bit `libjpeg.lib`) | `[VERIFIED: ls]` present (24808 bytes, Jun 17) |
| DllExport x64 | `DllExport.vcxproj` (18 x64 refs) + System32 `DllExport.dll` | Plugin↔host engine-dllimport bridge, x64-ported in P33 T2 | `[VERIFIED: grep]` x64 configs exist |
| `stage-x64/` | repo-root `stage-x64/` | Isolated x64 deploy dir (distinct from shipped 32-bit `stage/`) | `[VERIFIED: ls]` exists; has gl05/06/07 + SwgClient, **NO gl11 yet** |
| x64 `d3dcompiler_47.dll` | `%SystemRoot%\Sysnative\d3dcompiler_47.dll` → `stage-x64/` | gl11's shader-compile runtime dep (already staged by gl05 postbuild) | `[VERIFIED: ls]` `D3DCompiler_47.dll` in stage-x64/ |

### gl11's existing (correct) link inputs — NO change needed
`[VERIFIED: read Direct3d11.vcxproj:118]` Debug|Win32 Link `AdditionalDependencies`:
```
libjpeg.lib;odbc32.lib;odbccp32.lib;winmm.lib;delayimp.lib;dxguid.lib;d3d11.lib;d3dcompiler.lib;dxgi.lib;legacy_stdio_definitions.lib
```
`d3d11.lib`, `d3dcompiler.lib`, `dxgi.lib` are all Windows-SDK libs that resolve x64 from the v145
toolset. **No `d3dx9`, no DXSDK directx9\lib path** in gl11 (confirmed — gl11 has no
`AdditionalLibraryDirectories` pointing at `directx9\lib`; only `libjpeg\lib`). The Phase-33 dominant
cost (D3DX removal) is genuinely **absent** for gl11.

### The ONLY lib-list delta gl11's x64 block needs vs its Win32 block
- `libjpeg.lib` → `jpeg62-x64.lib` (the shared x64 swap)
- Drop the 32-bit-only postbuild `stage\` copy → x64 `stage-x64\` copy with Sysnative d3dcompiler_47

Everything else (`odbc32;odbccp32;winmm;delayimp;dxguid;d3d11;d3dcompiler;dxgi;legacy_stdio_definitions`)
carries over **unchanged** — confirm at link.

**Version verification:** N/A — no new package installs. All assets are in-tree, version-pinned by the
committed Phase-33 build.

## Architecture Patterns

### System Architecture Diagram
```
  Direct3d11.vcxproj  ──(add 2 x64 ProjectConfigurations)──┐
        │                                                   │
        ├── ImportGroup Debug|x64 ─── x64-platform.props ───┤  (shared compile flags + N1 guardrail)
        │                                                   │
        ├── PropertyGroup Debug|x64 ── OutDir: compile/win32/Direct3d11/x64/Debug/  (isolated)
        │                                                   │
        └── ItemDefinitionGroup Debug|x64                   │
              ├── ClCompile (Optimization Disabled, DIRECT3D11_EXPORTS, RTTI/PCH)
              └── Link: MachineX64                          │
                    ├── AdditionalDependencies: jpeg62-x64.lib + d3d11/d3dcompiler/dxgi + ...
                    ├── DelayLoadDLLs: DllExport.dll  ──────┤  (resolves via DllExport x64 + ProjectReference)
                    ├── OutputFile: $(OutDir)gl11_d.dll     │
                    └── PostBuild → stage-x64/ + Sysnative d3dcompiler_47.dll
                                                            │
  swg.sln ── {DC3F2C16-...}.Debug|x64.ActiveCfg/.Build.0 ──┘  (4 lines: Debug|x64 + Release|x64)
        │
        ▼
  $env:MSBUILD /t:Direct3d11 /p:Configuration=Debug /p:Platform=x64 /nodeReuse:false
        │
        ▼  link-grep "unresolved external symbol" == 0  +  dumpbin /headers → "machine (x64)"
        │
        ▼  gl11_d.dll → stage-x64/
        │
        ▼  boot SwgClient_d.exe (stage-x64/) + client_d.cfg rasterMajor=11
        │
        ▼  RenderDoc capture gl11 Debug|x64  ──diff──  gl11 32-bit Debug (v2.2 reference)
                    (triad: dressed char-select + Tatooine + cantina interior; arch-only A/B)
```

### Pattern 1: Mirror gl05's committed x64 block structure
**What:** gl11's `Direct3d11.vcxproj` is currently **pure Win32** (Debug/Optimized/Release|Win32, NO
x64 — `[VERIFIED: read lines 3-16]`). Add the x64 surface by mirroring gl05's five committed x64
insertion points.
**When to use:** This is the entire structural diff. Mirror these five regions:

| Region | gl05 reference (Direct3d9.vcxproj) | gl11 equivalent to add |
|--------|-----------------------------------|------------------------|
| (1) `ProjectConfiguration`s | lines 16-23 (`Debug|x64`, `Release|x64`) | Add both x64 entries to the `ProjectConfigurations` ItemGroup |
| (2) Configuration PropertyGroups | lines 48-59 (`DynamicLibrary`, `v145`, `MultiByte`) | Add Debug|x64 + Release|x64 config PropertyGroups |
| (3) PropertySheet ImportGroups | lines 75-82 (import `..\..\..\..\..\..\build\win32\x64-platform.props`) | Add both x64 ImportGroups importing x64-platform.props |
| (4) OutDir/IntDir PropertyGroups | lines 106-118 (`compile\win32\Direct3d9\x64\<cfg>\`) | `compile\win32\Direct3d11\x64\Debug\` + `\x64\Release\` |
| (5) ItemDefinitionGroups | lines 304-377 (ClCompile + Link MachineX64 + stage-x64 postbuild) | Two x64 IDGs with gl11's lib list + `gl11_d.dll` output |
| (6) PCH Create on FirstDirect3d11.cpp | gl05 lines 405-406 (`Create` for Debug|x64 + Release|x64) | Add the two x64 PCH-Create conditions to `FirstDirect3d11.cpp` |

**Note the relative path depth:** gl05 and gl11 are at the same directory depth
(`src/engine/client/application/<plugin>/build/win32/`), so the `..\..\..\..\..\..\build\win32\x64-platform.props`
import path is **identical** — copy it verbatim.

### Pattern 2: The plugin x64 Link block is CLEAN — no /FORCE, no /SAFESEH:NO
**What:** `[VERIFIED: read Direct3d9.vcxproj:320-331]` The gl05 **Debug|x64 Link block** contains
ONLY: `AdditionalDependencies` (jpeg62-x64 + DX libs), `OutputFile`, `AdditionalLibraryDirectories`
(libjpeg\lib), `IgnoreSpecificDefaultLibraries`, `DelayLoadDLLs DllExport.dll`,
`GenerateDebugInformation`, `ProgramDatabaseFile`, `ImportLibrary`, and **`<TargetMachine>MachineX64</TargetMachine>`**.
It does **NOT** carry `/SAFESEH:NO`, `/FORCE`, `ForceFileOutput`, `/VERBOSE`, or `BaseAddress`.
**Why this matters (corrects the research-question framing):** `/SAFESEH:NO` is an **x86-only**
concept (x64 has no SAFESEH); the gl05 Win32 blocks carry `/SAFESEH:NO` but the x64 blocks correctly
omit it. The `/FORCE` + `ForceFileOutput` + `/VERBOSE` recipe lives on the **SwgClient.exe** link
(`[VERIFIED: SwgClient.vcxproj:329,332]` — `/SAFESEH:NO /VERBOSE` + `<ForceFileOutput>Enabled`), NOT
on the plugin DLLs. The SwgClient exe already links (Phase 33). gl11's plugin link is a clean
`MachineX64` link.
**gl11 x64 Link block = gl05's x64 block with:** lib list swapped to gl11's set, `OutputFile`
`gl11_d.dll`, `ProgramDatabaseFile`/`ImportLibrary` `gl11_d.pdb`/`gl11_d.lib`.

### Pattern 3: swg.sln GUID x64 registration
**What:** Register the gl11 GUID `{DC3F2C16-A478-482B-BAD5-83F4957554D4}` for x64.
`[VERIFIED: grep swg.sln]` Today gl11 has only Win32 lines (1657-1662); gl05
`{E89A79A9-...}` has the full x64 pattern at lines 1653-1656. The solution-level
`Debug|x64 = Debug|x64` / `Release|x64 = Release|x64` global configs already exist (lines 1589-1590).
**Add these four lines** (mirroring gl05 1653-1656, inserting after gl11's Win32 lines ~1662):
```
		{DC3F2C16-A478-482B-BAD5-83F4957554D4}.Debug|x64.ActiveCfg = Debug|x64
		{DC3F2C16-A478-482B-BAD5-83F4957554D4}.Debug|x64.Build.0 = Debug|x64
		{DC3F2C16-A478-482B-BAD5-83F4957554D4}.Release|x64.ActiveCfg = Release|x64
		{DC3F2C16-A478-482B-BAD5-83F4957554D4}.Release|x64.Build.0 = Release|x64
```
The `.Build.0` line is what makes it actually build for the x64 solution config (ActiveCfg alone only
sets the active config without building). Per D-04, both Debug|x64 and Release|x64 are authored
(mirror), but only Debug|x64 is exercised this phase — a `/t:Direct3d11 /p:Platform=x64` targeted
build does not require the Release|x64 to link.

### Anti-Patterns to Avoid
- **Adding an Optimized|x64 config:** gl05 has **only** Debug|x64 + Release|x64 (NO Optimized|x64 —
  `[VERIFIED: read lines 16-23]`). gl11's Win32 set has three configs (Debug/Optimized/Release) but
  the x64 mirror is **two** configs, matching gl05. Do not mirror the Optimized config to x64.
- **Re-auditing gl11's D3D11 source:** Explicitly out of scope (D-01). gl11 was swept in P31.
- **Touching a shared header to "fix" a gl11 surprise without rebuilding all plugins:** Shared-header
  ABI cascade — if a shared header is touched, ALL plugins (gl05/06/07/11) must rebuild. Fix-what-breaks
  should be plugin-local this phase.
- **Writing client_d.cfg with PowerShell Set-Content/Out-File:** PS 5.1 adds a UTF-8 BOM that crashes
  the Release client at boot (Debug masks it). Use a BOM-safe editor to set `rasterMajor=11`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| x64 compile flag set | Per-config ClCompile flags in gl11 | Import `x64-platform.props` | Committed single-source x64 parity block; carries the N1 /we4311;4312 guardrail + DEBUG_LEVEL split (avoids the Release-debug-define miscompile trap documented in the props header) |
| x64 libjpeg | Build libjpeg x64 | `jpeg62-x64.lib` (already in tree) | Built in Phase 33; present at `libjpeg\lib\jpeg62-x64.lib` |
| Plugin↔host bridge x64 | Re-port DllExport `__asm int 3` | DllExport.dll x64 (P33 T2) | Already ported (`__asm int 3` → `__debugbreak`); gl11 imports the same bridge via DelayLoadDLLs + ProjectReference |
| x64 deploy isolation | New deploy dir scheme | `stage-x64/` (Sysnative d3dcompiler_47 already staged) | Phase-33 convention; keeps the shipped 32-bit `stage/` (machine 14C) untouched |

**Key insight:** Every piece of x64 infrastructure gl11 needs already exists and is proven against
gl05/06/07. This phase is wiring gl11 into existing scaffolding, not building scaffolding.

## Common Pitfalls

### Pitfall 1: `/FORCE` link masks unresolved externals (exit 0 ≠ clean link)
**What goes wrong:** SwgClient links under `/FORCE`, which downgrades `unresolved external symbol`
(LNK2019/2001) to warnings and still emits a binary with exit 0. A gl11 x64 symbol gap could pass a
naive exit-0 check.
**Why it happens:** `/FORCE` + `ForceFileOutput` on the SwgClient.exe link (the exe links the plugins'
import libs). The gl11 plugin DLL link itself does NOT use /FORCE, so a gl11-local unresolved would be
a hard error — but the **full client link** (which the boot depends on) is the /FORCE surface.
**How to avoid:** Grep the x64 build/link log for `unresolved external symbol` (must be 0). This is
the primary x64-link acceptance signal (AGENTS.md §Build). Also grep for `LNK1181` (cannot open input
file — a hard error, e.g. if jpeg62-x64.lib path is wrong).
**Warning signs:** Build "succeeds" but the x64 client crashes at the gl11 entry point, or gl11
renders nothing.

### Pitfall 2: Wrong cfg / wrong exe / wrong rasterMajor mismatch
**What goes wrong:** Editing the wrong cfg silently has no effect. `SwgClient_d.exe` reads
`client_d.cfg`; `SwgClient_r.exe` reads `client.cfg`. The x64 client lives in `stage-x64/`, distinct
from the 32-bit `stage/`.
**How to avoid:** For the Debug|x64 verify, boot `stage-x64/SwgClient_d.exe` with
`stage-x64/client_d.cfg` set to `rasterMajor=11`. Confirm exe + cfg + rasterMajor + gl11_d.dll all
line up in stage-x64/ before declaring a verify. (`rasterMajor=9` is FATAL — DX9 is value 5.)
**Warning signs:** Client boots gl05 (D3D9) when you expected gl11; or cfg edits are ignored.

### Pitfall 3: WoW64 redirector hides the x64 d3dcompiler_47.dll from the x86 MSBuild host
**What goes wrong:** The MSBuild host is x86; a naive `%SystemRoot%\System32\d3dcompiler_47.dll` copy
in a postbuild gets WoW64-redirected to the 32-bit SysWOW64 DLL.
**Why it happens:** WoW64 file-system redirection on an x86 process.
**How to avoid:** `[VERIFIED: gl05 postbuild :337]` Use `%SystemRoot%\Sysnative\d3dcompiler_47.dll`
(Sysnative dodges the redirector) — copy gl11's postbuild verbatim from gl05. (It's already in
stage-x64/ from the gl05 postbuild, but gl11's postbuild should keep the same line for correctness.)

### Pitfall 4: Stale-plugin ABI cascade if a shared header is touched
**What goes wrong:** If fix-what-breaks touches a shared header (e.g. `ShaderImplementation.h`), a
stale gl05/06/07 plugin DLL silently breaks ABI → deterministic boot crash.
**How to avoid:** Fix-what-breaks should be **plugin-local** (D-01 expectation). If a shared header is
touched, rebuild ALL plugins (gl05/06/07/11), not just gl11. Detect via dll-vs-exe mtime + shared-header
touch.

## Code Examples

### gl11 Debug|x64 Link block (mirror of gl05:304-339, gl11-specialized)
```xml
<!-- Phase 34 (X64-03): Debug|x64 link block for gl11. gl11 never used D3DX — links only
     Windows-SDK x64 d3d11/d3dcompiler/dxgi. libjpeg → jpeg62-x64.lib (the shared swap).
     MachineX64, isolated x64 OutDir, no /SAFESEH:NO (x86-only), no BaseAddress. PostBuild
     → stage-x64\ + Sysnative d3dcompiler_47.dll. -->
<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
  <ClCompile>
    <Optimization>Disabled</Optimization>
    <AdditionalIncludeDirectories><!-- gl11 Debug|Win32 include set verbatim --></AdditionalIncludeDirectories>
    <PreprocessorDefinitions>_WINDOWS;_USRDLL;DIRECT3D11_EXPORTS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    <RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>
    <PrecompiledHeader>Use</PrecompiledHeader>
    <PrecompiledHeaderFile>FirstDirect3d11.h</PrecompiledHeaderFile>
    <PrecompiledHeaderOutputFile>$(OutDir)Direct3d11.pch</PrecompiledHeaderOutputFile>
    <ObjectFileName>$(OutDir)</ObjectFileName>
    <ProgramDataBaseFileName>$(OutDir)</ProgramDataBaseFileName>
    <UseFullPaths>true</UseFullPaths>
  </ClCompile>
  <Link>
    <AdditionalDependencies>jpeg62-x64.lib;odbc32.lib;odbccp32.lib;winmm.lib;delayimp.lib;dxguid.lib;d3d11.lib;d3dcompiler.lib;dxgi.lib;legacy_stdio_definitions.lib;%(AdditionalDependencies)</AdditionalDependencies>
    <OutputFile>$(OutDir)gl11_d.dll</OutputFile>
    <AdditionalLibraryDirectories>..\..\..\..\..\..\external\3rd\library\libjpeg\lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
    <IgnoreSpecificDefaultLibraries>libc; libcp;%(IgnoreSpecificDefaultLibraries)</IgnoreSpecificDefaultLibraries>
    <DelayLoadDLLs>DllExport.dll;%(DelayLoadDLLs)</DelayLoadDLLs>
    <GenerateDebugInformation>true</GenerateDebugInformation>
    <ProgramDatabaseFile>$(OutDir)gl11_d.pdb</ProgramDatabaseFile>
    <ImportLibrary>$(OutDir)gl11_d.lib</ImportLibrary>
    <TargetMachine>MachineX64</TargetMachine>
  </Link>
  <PostBuildEvent>
    <Message>Stage Debug x64 gl11_d.dll + pdb + d3dcompiler_47.dll</Message>
    <Command>if not exist "..\..\..\..\..\..\..\stage-x64\" mkdir "..\..\..\..\..\..\..\stage-x64\"
copy /Y "$(OutDir)gl11_d.dll" "..\..\..\..\..\..\..\stage-x64\"
copy /Y "$(OutDir)gl11_d.pdb" "..\..\..\..\..\..\..\stage-x64\"
copy /Y "%SystemRoot%\Sysnative\d3dcompiler_47.dll" "..\..\..\..\..\..\..\stage-x64\"</Command>
  </PostBuildEvent>
</ItemDefinitionGroup>
```
**Note:** gl11 keeps `DIRECT3D11_EXPORTS` (its export define) where gl05 has
`DIRECT3D9_EXPORTS;FFP;VSPS`. gl11 has NO `FFP`/`VSPS` defines (those are D3D9-plugin-flavor switches).
gl11's include set drops the `directx9\include` dir gl05 carries.

### Build invocation (Debug|x64, targeted)
```powershell
# Force a relink so link gates actually run
Remove-Item src\compile\win32\Direct3d11\x64\Debug\gl11_d.dll -ErrorAction SilentlyContinue
Get-Process MSBuild,cl,link -ErrorAction SilentlyContinue | Stop-Process -Force
& $env:MSBUILD src\build\win32\swg.sln /t:Direct3d11 `
    /p:Configuration=Debug /p:Platform=x64 /nodeReuse:false `
    2>&1 | Tee-Object build-34-gl11-x64.log
# Acceptance: 0 unresolved external symbol, 0 LNK1181, machine x64
Select-String -Path build-34-gl11-x64.log -Pattern "unresolved external symbol" # must be empty
& $env:DUMPBIN /headers stage-x64\gl11_d.dll | Select-String "machine"          # must read 8664 (x64)
```
*(`$env:MSBUILD` = the VS18/v145 BuildTools MSBuild from `.claude/settings.json`. Build from
PowerShell, not Git Bash — MSYS mangles `/t:` and `/p:`. dumpbin: use the VS x64 toolchain dumpbin.)*

## Runtime State Inventory

> Phase 34 is a build/link/verify phase, not a rename/refactor/migration. The only "runtime state"
> concern is staged binaries and the x64 client cfg. Included for completeness.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — no datastore keys/IDs touched | None |
| Live service config | None — no external service config | None |
| OS-registered state | None — no task scheduler / service registration | None |
| Secrets/env vars | None — no secret/env-var names change (`$env:MSBUILD` unchanged) | None |
| Build artifacts | `stage-x64/` currently has gl05/06/07 + SwgClient but **NO gl11_d.dll** `[VERIFIED: ls]`; `compile/win32/Direct3d11/x64/` does not yet exist | Phase produces `gl11_d.dll` x64 → stage-x64/; the gl11 32-bit `stage/gl11_d.dll` is untouched (non-regression) |

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| gl11 Win32-only | gl11 + x64 platform | This phase (P34) | Second renderer reaches x64 parity |
| Plugin /SAFESEH:NO on x86 | x64 omits /SAFESEH (x86-only concept) | P33 (gl05 template) | gl11 x64 block correctly omits it |
| 32-bit Debug OOM-hangs under extended play | Debug|x64 plays extended with asserts live | x64 (no 2GB ceiling) | Debug|x64 is the primary verify surface (D-03) |
| RenderDoc can't capture D3D9 (parity diffs were indirect) | D3D11(Win32)-vs-D3D11(x64) arch-only A/B | This phase | Cleanest possible parity diff — both sides capturable |

**Deprecated/outdated:** D3DX (`d3dx9*`) — never present in gl11; fully removed from gl05 in P33. Not
a gl11 concern.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | gl11's x64 link resolves with NO source/symbol surprise beyond benign C4267 (P31 swept gl11 clean; D3D11 cbuffer/reflection/shader-cache paths link x64 without runtime defect) | D-01 / Standard Stack | LOW-MEDIUM — if a D3D11-specific x64 runtime defect appears (e.g. a `size_t`-narrowed reflection index that's functionally lossy, or an alignment fault in a cbuffer upload), fix-what-breaks adds a plugin-local task. Mitigated by: the only narrowing sites found are explicit `static_cast<int>(...size())` display/count paths (benign), and gl11 already runs clean in 32-bit. |
| A2 | The DllExport x64 bridge resolves gl11's `DelayLoadDLLs DllExport.dll` at link with zero new port work | Don't Hand-Roll | LOW — gl11 uses the identical DelayLoadDLLs + ProjectReference to DllExport that gl05/06/07 use; DllExport.vcxproj has x64 configs. Confirm at link (CONTEXT explicitly says "confirm, don't re-port"). |
| A3 | The v2.2 parity fixes (dot3/bump/face/terrain/mini-map/gamma/cantina-FFP) are all deterministic-codegen-stable across x86→x64 (no float-transient parity divergence) | Validation | LOW-MEDIUM — x64's deterministic SSE float is generally MORE stable than x86 x87 (it's the door-snap fix theory), so a parity divergence is unlikely; the arch-only A/B is precisely the probe that would catch it if it occurs. |

## Open Questions (RESOLVED)

1. **Will any benign C4267 in a Direct3d11 TU need a real width fix?**
   - What we know: `[VERIFIED: grep]` The only `size_t`→`int` narrowing sites in Direct3d11 TUs are
     **already explicit casts**: `Direct3d11_LightManager.cpp:380` (`static_cast<int>(lightList.size())`,
     a frame-log arg), `Direct3d11_StaticShaderData.cpp:459` (`static_cast<int>(tags.size())` tag count),
     `Direct3d11_VertexBufferVectorData.cpp:43` (`static_cast<int>(list.size())` count). Explicit casts
     do not warn under the N2 policy (only implicit narrowing warns), and these are display/count paths
     in the D-07-excluded class.
   - What's unclear: Whether any of these counts feed a functional bound that overflows int on x64
     (extremely unlikely — light/tag/vertex-attr counts are tiny).
   - Recommendation: Treat as a **watch list, NOT must-fix-preemptively** (D-01). Let the x64 link
     (silent — explicit casts don't warn) and the parity A/B surface anything real. No preemptive task.

2. **Which specific draws to diff in the RenderDoc A/B?**
   - What we know: gl11 is where the v2.2 parity battle was fought; representative draws = a dot3/bump
     character surface (char-select dressed NPC), a terrain-blend draw (Tatooine), and a cantina
     FFP-combiner wall-light draw (cantina interior).
   - What's unclear: Exact EIDs (capture-dependent).
   - Recommendation: Executor picks EIDs at capture time (Claude's Discretion). Use
     `renderdoc-cli.exe <cap.rdc> draws [--filter]` to enumerate, then `pixel`/`debug pixel --trace`
     on the representative dot3/bump/terrain draws.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| `$env:MSBUILD` (VS18/v145) | x64 build | ✓ | VS18 BuildTools (v145 toolset) | — (v2022 fails MSB8020) |
| `jpeg62-x64.lib` | gl11 x64 link | ✓ | in-tree (Jun 17) | — |
| `x64-platform.props` | gl11 x64 compile flags | ✓ | committed | — |
| DllExport x64 (`DllExport.dll`) | gl11 plugin↔host bridge | ✓ | P33-ported | — |
| `%SystemRoot%\Sysnative\d3dcompiler_47.dll` | gl11 shader runtime | ✓ | already in stage-x64/ | — |
| x64 SwgClient (`stage-x64/SwgClient_d.exe`) | boot gl11 under rasterMajor=11 | ✓ | P33 (machine 8664) | — |
| RenderDoc CLI | parity A/B verify | ✓ | v0.3.0 (`D:\Code\renderdoc-mcp\v0.3.0\...\bin\renderdoc-cli.exe`) | RenderDoc MCP server (post-restart) |
| dumpbin (x64 toolchain) | machine-x64 confirm | ✓ | VS18 | `link /dump /headers` |
| Game data (TRE/TOC) | in-world Tatooine/cantina load | ✓ | `D:\Code\SWGSource Client v3.0\` | — |

**Missing dependencies with no fallback:** None — all x64 infrastructure exists from Phase 33.
**Missing dependencies with fallback:** None blocking.

## Validation Architecture

**Nyquist validation is ON** (`[VERIFIED: config.json]` `"nyquist_validation": true`). This phase is
predominantly build/boot/visual-verify; the automated test surface is the link-grep + dumpbin checks,
and the human/RenderDoc surface is the boot + parity A/B. There is no unit-test framework for renderer
plugins (the project has no renderer test suite — validation is build-gate + boot + RenderDoc, per the
v2.2/v3.0 history).

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None (no renderer unit-test framework); validation = MSBuild link-gate + dumpbin + boot + RenderDoc CLI diff |
| Config file | `src/build/win32/swg.sln` (build); `stage-x64/client_d.cfg` (runtime renderer select) |
| Quick run command | `Select-String -Path build-34-gl11-x64.log -Pattern "unresolved external symbol"` (must be empty) |
| Full suite command | Build `/t:Direct3d11 /p:Configuration=Debug /p:Platform=x64` + dumpbin machine-x64 + boot stage-x64 rasterMajor=11 + RenderDoc triad A/B |

### Phase Requirements → Test Map (X64-03 success criteria → observable validation)
| SC | Behavior | Test Type | Automated/Observed Command | Exists? |
|----|----------|-----------|----------------------------|---------|
| SC#1 | gl11 builds as x64 (`gl11_d.dll` machine x64) | build-gate | `dumpbin /headers stage-x64\gl11_d.dll` → `machine (x64)` / `8664`; `Select-String build-34-gl11-x64.log "unresolved external symbol"` == empty; `LNK1181` == 0 | ✅ (build + dumpbin) |
| SC#2 | x64 client boots rasterMajor=11 + v2.2 parity | boot + RenderDoc A/B | Boot `stage-x64\SwgClient_d.exe` (client_d.cfg rasterMajor=11) → char-select reached; `renderdoc-cli.exe` capture gl11 Debug|x64 vs gl11 32-bit Debug on triad (dressed char-select + Tatooine + cantina interior); diff representative dot3/bump/terrain draws → no arch-only divergence | ✅ (RenderDoc CLI present) |
| SC#3 | 32-bit gl11 non-regression | boot | Boot 32-bit `stage\SwgClient_d.exe` (or `_r.exe`) rasterMajor=11 → char-select still reached; `stage\gl11_d.dll` untouched (machine 14C) | ✅ |
| (cross) | gl05 x64 non-regression (shared x64 client) | boot | Boot `stage-x64\SwgClient_d.exe` rasterMajor=5 → char-select still reached (gl05 still loads in the shared x64 client) | ✅ |

### Sampling Rate
- **Per task commit (vcxproj/sln edit):** targeted `/t:Direct3d11 /p:Platform=x64` build →
  link-grep `unresolved external symbol` == 0 + dumpbin machine-x64.
- **Per wave merge:** boot stage-x64 under rasterMajor=11 to char-select (D-03 Debug|x64 surface,
  assert oracles live).
- **Phase gate:** RenderDoc arch-only A/B clean on the full triad + both non-regression boots
  (32-bit gl11 + x64 gl05) before `/gsd-verify-work`.

### Wave 0 Gaps
- None — existing build/boot/RenderDoc infrastructure covers all X64-03 criteria. No new test files,
  no framework install. (Renderer validation has always been build-gate + boot + RenderDoc, not a unit
  suite — consistent with Phases 17–33.)

## Security Domain

Not applicable. `security_enforcement` is not engaged for this phase: it is a build-platform-add for a
renderer plugin DLL. No authentication, session, access-control, input-validation, or cryptography
surface is touched. No new network/serialization/secret surface. (V5 input validation / V6 crypto: no
applicable controls — the phase adds compiler/linker configuration, not data-handling code.) ASVS
categories all non-applicable for an x64 vcxproj mirror.

## Sources

### Primary (HIGH confidence)
- `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj` — gl11 target, currently
  pure Win32; confirmed link inputs (d3d11/d3dcompiler/dxgi, no d3dx9), DllExport ProjectReference + DelayLoadDLLs
- `src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj` — gl05 committed x64 template
  ("Phase 33 (X64-02)" lines 299-377); exact mirror source
- `src/build/win32/x64-platform.props` — committed x64 compile flags (N1 /we4311;4312 guardrail, DEBUG_LEVEL split)
- `src/build/win32/swg.sln` — gl11 GUID `{DC3F2C16-...}` Win32-only (lines 1657-1662); gl05 x64 pattern (1653-1656); global x64 configs (1589-1590)
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` — the /FORCE+/SAFESEH:NO+/VERBOSE+ForceFileOutput recipe lives HERE (exe link), not on plugins
- `src/engine/client/application/Direct3d11/src/win32/Direct3d11_StaticVertexBufferData.cpp:173` + `Direct3d11_DynamicVertexBufferData.cpp:377` — hash-to-uintptr fixes confirmed present (P31 31-04)
- `stage-x64/` (ls) — gl05/06/07 + SwgClient staged x64, gl11 absent; jpeg62-x64.lib, d3dcompiler_47, renderdoc-cli all present
- `.planning/STATE.md` §Current Position / RESUME(2026-06-18) — the first x64 link recipe + invariants
- 34-CONTEXT.md (D-01..D-04), 33-CONTEXT.md, 31-PHASE33-RESIDUAL-WORKLIST.md (N1/N2 carry-forward)

### Secondary (MEDIUM confidence)
- None — all claims verified directly against the live tree.

### Tertiary (LOW confidence)
- None.

## Metadata

**Confidence breakdown:**
- vcxproj mirror diff: HIGH — gl05 template read in full; gl11 target read in full; the diff is mechanical and the path depths match
- swg.sln registration: HIGH — exact gl05 lines + gl11's missing lines confirmed by grep
- DllExport x64 coverage: HIGH — gl11 ProjectReference + DelayLoadDLLs identical to gl05; DllExport.vcxproj has x64 configs
- C4267 watch list: HIGH — the only narrowing sites are already-explicit display/count casts (benign)
- Link recipe: HIGH — confirmed /FORCE etc. are on the exe, plugin x64 link is clean MachineX64
- Parity verification method: HIGH (method) / MEDIUM (outcome — the A/B has not been run; that's the phase's work)

**Research date:** 2026-06-17
**Valid until:** Stable until the tree changes (the vcxproj/sln structure is committed); re-verify
stage-x64/ contents and any new upstream merge before execution.

## RESEARCH COMPLETE
