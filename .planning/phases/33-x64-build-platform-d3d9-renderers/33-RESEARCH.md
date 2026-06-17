# Phase 33: x64 Build Platform + D3D9 Renderers - Research

**Researched:** 2026-06-17
**Domain:** MSBuild platform-add (Win32→x64), third-party x64 sourcing/relink, residual D3DX removal, native x64 link + boot
**Confidence:** HIGH (everything below is filesystem-verified against the live tree + Restoration x64 dir; no inference on lib availability)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions (D-01..D-05 — research the MECHANICS, do NOT re-litigate)
- **D-01 (HYBRID sourcing):** Build x64 from in-tree source where it exists (majority); lift RAD-proprietary binary-only middleware (bink/miles) from `D:\SWG Restoration\x64\` + generate import .libs; prefer official/Restoration prebuilt for icu/discord-rpc. **Universal fallback:** any from-source build that stalls → grab the prebuilt x64 DLL from `D:\SWG Restoration\x64\` + import .lib. `d3d9.lib` x64 from the Windows SDK; `d3dx9` is being removed (not a sourcing target).
- **D-02 (dpvs BUILD-FROM-SOURCE):** Full source + `implementation/msvc8/dpvs.vcxproj` in-tree; `__asm` is `DPVS_X86_ASSEMBLY`-gated with C `#else` fallbacks; add x64 config, leave `DPVS_X86_ASSEMBLY` undefined on x64, plus a self-contained pointer-truncation mini-pass (dpvs was OUT of Phase 31 scope). No binary lift for dpvs.
- **D-03 (platform-add scope):** Boot-path subset ONLY — Phase 31's ~57 in-scope engine libs + gl05/06/07 + SwgClient. Editor apps stay Win32-only. (~127 `.vcxproj`s exist; only the boot subset gets x64.)
- **D-04 (finish D3DX removal BEFORE the x64 link):** (a) non-compile D3DX (~15 mesh/matrix/surface APIs) → DirectXMath/own-impl, NOT redist d3dx9; (b) asm `.vsh` path → D3DAssemble.
- **D-05 (asm `.vsh` → D3DAssemble, census-gated):** `D3DXAssembleShader` (`Direct3d9_VertexShaderData.cpp`) → `D3DAssemble`; gl11's `[P19VSFALLBACK]` does NOT transfer (D3D9 natively executes vs_1_1/vs_2_0 asm, must be genuinely assembled). Redist-d3dx9 fallback explicitly REJECTED — surface as a checkpoint, not a silent shortcut.

### Claude's Discretion
- Exact per-lib sourcing disposition (build-from-source vs lift) — enumerated below.
- dpvs x64-port mechanics: x64 config on `dpvs.vcxproj`, `DPVS_X86_ASSEMBLY`-undefined-on-x64 vs `_BitScanReverse` intrinsic swap, self-contained pointer-truncation pass.
- Which build configurations get x64 — must cover Debug + Release **and all three D3D9 plugin flavors** (Direct3d9 / Direct3d9_ffp / Direct3d9_vsps → gl05/06/07); the Optimized variant is planner's call.
- Whether to grow `x64-scratch/x64-compile.props` into committed x64 props vs author per-project x64 PropertyGroups.
- Build/sequencing order of the D3DX-removal precondition vs the platform-add.

### Deferred Ideas (OUT OF SCOPE)
- Dropping DPVS entirely (post-x64 evaluation). gl11/D3D11 as x64 (Phase 34). Miles 9.3b audio x64 port (Phase 35). redist-d3dx9 fallback for the asm path (contingency checkpoint only). Door-snap (VERIFY-01) / OOM-class (VERIFY-02) confirmation (Phase 36).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| X64-01 | x64 platform added to `swg.sln` + every boot-path `.vcxproj`; `SwgClient_*.exe` produced as x64 (`dumpbin` machine x64) | §Platform-Add Mechanics + §Boot-Path Project List; sln config block at swg.sln:1583, per-project ProjectConfiguration pattern |
| X64-04 | All third-party deps resolve as x64; no missing-DLL / load failure at boot | §Per-Lib x64 Sourcing Table — every boot-path dep enumerated, filesystem-verified |
| X64-02 | gl05/06/07 build as x64 + x64 client boots to char-select under rasterMajor 5/6/7 | §D3DX Removal Residual Surface (the x64-link precondition) + §Validation Architecture |
</phase_requirements>

## Summary

Phase 33 is a **build/link/procurement exercise, not a code-debugging swamp** — Phase 31 made the in-scope tree compile x64-clean and Phase 32's D3DCompile HLSL port already landed. The phase has three distinct work surfaces that must be sequenced: **(1)** finish the residual D3DX removal (the x64-link precondition — D3DX has no x64 static lib), **(2)** add the committed `x64` platform to `swg.sln` + the boot-path `.vcxproj` subset (per-project, since no shared common `.props` exists), and **(3)** resolve every third-party dependency as x64 and get the first x64 link + boot under gl05/06/07.

The single biggest planning correction surfaced by ground-truthing: **the third-party x64 surface is far smaller and lower-risk than the milestone framing implies.** Filesystem evidence shows (a) **Bink is dynamically loaded** via `LoadLibrary("binkw32.dll")` at `BinkVideo.cpp:55` — the x64 swap is a one-line DLL-name string change to `"binkw64.dll"` + dropping the Restoration `binkw64.dll` into `stage/`, **NO import .lib needed**; (b) **icu and discord-rpc are NOT vendored and NOT referenced anywhere in this codebase** — they ship in Restoration's x64 dir but are Restoration-specific, so they are **not Phase 33 sourcing targets**; (c) the genuinely-buildable-from-source libs that have in-tree `.vcxproj`s (zlib, dpvs, ui, udplibrary) just need an x64 config added; (d) **D3DX is entirely confined to the single `Direct3d9` plugin source** (no other boot-path lib touches it), and the D3DAssemble dialect gate **already PASSED byte-identical in Phase 32-01**, so the asm port (D-05) is de-risked to a mechanical `GetProcAddress` swap.

The real risks are: the dpvs x64-port needs an **explicit source edit** to its CPU-detection (it unconditionally defines `DPVS_CPU_X86`→`DPVS_X86_ASSEMBLY` for any WIN32 build, ignoring `_M_X64`); the non-compile D3DX→DirectXMath conversion touches a shared cached-matrix path (`Direct3d9.cpp`) that all three plugins share (ABI-cascade caution); the prebuilt libxml2/pcre/jpeg/tinyxml `.lib`s have **no in-tree source and no in-tree x64 build**, so they hit the D-01 universal fallback (lift Restoration's x64 DLL + generate import lib); and Miles stays on x86 mss32 (Phase 35), so the x64 client links/boots **audio-degraded by design**.

**Primary recommendation:** Sequence as three waves — Wave A finishes D3DX removal (asm→D3DAssemble per 32-03 + non-compile D3DX→DirectXMath/own-impl) on the still-32-bit tree and re-validates the 32-bit boot; Wave B adds the x64 platform per-project (grow `x64-compile.props` into a committed `x64-platform.props` for the compile block + author per-project x64 link blocks) and resolves third-party x64 libs; Wave C performs the first x64 link + gl05/06/07 boot validation under rasterMajor 5/6/7, with the 32-bit non-regression gate held throughout.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| x64 platform definition (ProjectConfiguration/PropertyGroup) | Build system (MSBuild .vcxproj/.sln) | — | Pure build-config surface; no runtime code |
| Third-party x64 lib resolution | Build system (link inputs) + filesystem (lib/DLL procurement) | Runtime (DLL load at boot) | Link-time .lib resolution + runtime DLL presence in `stage/` |
| Residual D3DX removal (matrix/mesh/surface) | Direct3d9 plugin source (own-impl/DirectXMath) | Shared cached-matrix state (3 plugins) | D3DX confined to one plugin's source; DirectXMath is header-only x64-native |
| Asm `.vsh` compile (D3DAssemble) | Direct3d9 plugin runtime (`Direct3d9_VertexShaderData.cpp`) | d3dcompiler_47 (Windows SDK / staged) | Runtime shader assembly; gl05/06/07 natively execute vs_1_1/2_0 |
| dpvs x64 port (asm-gate + truncation) | Third-party dpvs source (build-from-source) | Build config (x64 platform on dpvs.vcxproj) | SOE source in-tree; needs source edit + x64 config |
| x64 boot to character select | SwgClient.exe (x64) + gl0X_*.dll (x64) | Game data (TRE, unchanged) | The integration gate — exe + plugins + libs + cfg all x64-aligned |

## Per-Lib x64 Sourcing Table (X64-04)

> **Ground-truthed against the live tree + `D:\SWG Restoration\x64\` (2026-06-17).** This is the availability map, not inference. "In Restoration x64" = exact DLL filename present in that dir.

### Boot-path third-party dependencies (the ones that actually matter for X64-04)

| Dep | In-tree source? | In-tree x64 build? | 32-bit link input | In Restoration x64? | Disposition (per D-01) | Import .lib need |
|-----|-----------------|--------------------|--------------------|---------------------|------------------------|------------------|
| **zlib** | YES — `zlib-1.2.3/*.c` + `zlib.vcxproj` (Debug/Release configs) | No (Win32 only) | `zlib.lib` from `compile/win32/zlib/` | (statically built into Restoration) | **BUILD-FROM-SOURCE** — add x64 config to `zlib.vcxproj` | none (own .vcxproj output) |
| **dpvs** | YES — `dpvs/implementation/sources/` (70 .cpp) + `msvc8/dpvs.vcxproj` (Debug/IntelCPP/Release) | No | `dpvs.lib` from `compile/win32/dpvs/`; DLL output `dpvs.dll` | `dpvs.dll` (424 KB) | **BUILD-FROM-SOURCE** (D-02) — add x64 config + source edits (see §dpvs) | none. Restoration's `dpvs.dll` is the D-01 fallback escape hatch |
| **ui** (SOE UI lib) | YES — `ui/src/` + `ui/build/win32/ui.vcxproj` | No | `ui.lib` from `compile/win32/ui/` | (statically built) | **BUILD-FROM-SOURCE** — add x64 config | none |
| **udplibrary** | YES — `udplibrary/*.cpp` + `udplibrary.vcxproj` | No | `udplibrary.lib` from `compile/win32/udplibrary/` | (statically built) | **BUILD-FROM-SOURCE** — add x64 config | none |
| **libxml2** | Source present (`libxml/*.c`) but **NO in-tree .vcxproj** — links a prebuilt `.lib` | No | `libxml2-win32-release.lib` / `libxml2-win32-debug.lib` from `libxml2-2.6.7.win32/lib` | `libxml2.dll` (1.3 MB) | **LIFT** (D-01 universal fallback — no build project exists) `libxml2.dll` + generate import .lib; OR author a from-source x64 .vcxproj (heavier) | YES (gen from DLL exports) |
| **pcre** | `pcre/4.1/` prebuilt only — links `libpcre.a` (a MinGW archive!) | No | `libpcre.a` from `pcre/4.1/win32/lib` | `pcre.dll` (147 KB) | **LIFT** `pcre.dll` + gen import .lib (the `.a` is not even a native MSVC lib; x64 needs a real `.lib`) | YES |
| **jpeg** (libjpeg) | `libjpeg/lib/libjpeg.lib` prebuilt only — no in-tree .vcxproj | No | `libjpeg.lib` from `libjpeg/lib` | `jpeg62.dll` (604 KB) | **LIFT** `jpeg62.dll` + gen import .lib (note name differs: `libjpeg.lib`→`jpeg62`) | YES |
| **tinyxml** | YES — `tinyxml/src/` exists but built via legacy `.dsp`, prebuilt `.lib`s shipped | No | `tinyxmld.lib` / `tinyxmld_STL.lib` from `tinyxml/lib` + `tinyxml/build/win32` | (not shipped separately by Restoration) | **BUILD-FROM-SOURCE** (small, pure C++) — author an x64 .vcxproj or compile into the consuming lib; no Restoration fallback DLL | none if built from source |
| **bink** | NO source (RAD proprietary) — `bink/include/*.h` + `binkw32.lib`/`binkw32.dll` | No | **Dynamically loaded** — `LoadLibrary("binkw32.dll")` at `BinkVideo.cpp:55` | `binkw64.dll` (282 KB) | **LIFT DLL ONLY** — change the DLL-name string to `"binkw64.dll"` + stage `binkw64.dll`. **NO import .lib** (dynamic GetProcAddress) | **NONE** — see finding below |
| **Miles** (mss32) | NO source (RAD) — `miles-7.2e/lib/win/Mss32.lib` + headers | No | `Mss32.lib` static link; `AIL_startup()` called directly | `mss64.dll` (483 KB) | **PHASE 35 scope** — x64 client links x86 mss32 → degraded/no audio at boot is acceptable for Phase 33 (X64-02 is boot-to-char-select, audio is AUDIO-01/02) | deferred to P35 |
| **d3d9** | Windows SDK (vendored x86 `directx9/lib/d3d9.lib`) | — | `d3d9.lib` | (system) | **WINDOWS SDK x64** — the platform toolset provides the x64 `d3d9.lib`; do NOT use the vendored x86 stub | none (SDK) |
| **d3dcompiler_47** | `directx9/lib/d3dcompiler_47.dll` + `d3dcompiler.lib` (x86, staged Phase 27) | — | `d3dcompiler.lib` | (system `C:\Windows\System32\D3DCompiler_47.dll` is x64) | **WINDOWS SDK x64** `d3dcompiler.lib` + stage the x64 `d3dcompiler_47.dll` (system copy is x64) | none (SDK) |
| **d3dx9** | `directx9/lib/d3dx9.lib` (x86, legacy DXSDK) | **NO x64 static lib exists** | `d3dx9.lib;d3dx9dt.lib;d3dx.lib` | (Restoration links an x64 d3dx9 redist — we REJECT that) | **REMOVED** (D-04) — must be gone before the x64 link | n/a |

### True gaps (in NEITHER in-tree source NOR a from-source x64 build path)

| Dep | Gap | Mitigation |
|-----|-----|------------|
| **libxml2 / pcre / jpeg** | Prebuilt x86 `.lib` only, no in-tree build project; pcre's `libpcre.a` is a MinGW archive | D-01 universal fallback: lift Restoration's x64 DLL (`libxml2.dll`/`pcre.dll`/`jpeg62.dll`) + generate an import `.lib` via `dumpbin /exports` → `.def` → `lib /machine:x64 /def:`. All three exist in Restoration x64. |
| **tinyxml** | No Restoration x64 DLL fallback (not shipped separately) | MUST build from source (it is small pure C++); author an x64 `.vcxproj` or fold the 3 `.cpp` into a consuming lib. This is the one dep with no binary escape hatch. |

### Findings that correct the milestone framing

1. **`[VERIFIED: grep BinkVideo.cpp:55 + BinkDLL.cpp:139]` Bink needs NO import lib.** `clientGraphics` loads bink via `s_hModule=LoadLibrary(i_name)` + `GetProcAddress` with `s_dllName="binkw32.dll"`. The x64 port is: change the string to `"binkw64.dll"` (or make it `#if _M_X64`-conditional) and drop Restoration's `binkw64.dll` into `stage/`. The D-01 "generate matching import .libs for bink" step is **unnecessary** — there was never a static bink import in the link.
2. **`[VERIFIED: grep -rl discord/icu across src]` icu and discord-rpc are NOT in this codebase.** Zero source references, not vendored under `src/external/3rd/library/`. They appear in Restoration's x64 dir because Restoration added those features. **They are not Phase 33 sourcing targets** — X64-04's "icu, discord-rpc" in the requirement text is inherited from the Restoration availability map, not from this client's actual dependency set. (Confirm with the user; flagged in Assumptions Log A1.)
3. **`[VERIFIED: dumpbin/system]` d3dcompiler_47 x64 is already on the system** (`C:\Windows\System32\D3DCompiler_47.dll` is x64). Stage that (or the SDK redist x64 copy) next to the x64 exe.
4. **`[VERIFIED: grep Mss32/AIL_startup]` Miles is statically linked (`Mss32.lib` + direct `AIL_startup()`).** Unlike bink, swapping Miles to x64 requires the 7.2e→9.3b API port (Phase 35). For Phase 33, the x64 link against the x86 `Mss32.lib` will FAIL (machine mismatch) — so the planner must decide: stub/`#if`-out the clientAudio Miles calls for the x64 build, or accept that clientAudio cannot link x64 until P35. **This is a real Phase-33 sequencing decision** (see Open Questions Q1).

## Platform-Add Mechanics (X64-01)

### swg.sln (the solution-config surface)
- **`[VERIFIED: swg.sln:1583-1596]`** Currently 100% Win32. `GlobalSection(SolutionConfigurationPlatforms)` declares `Debug|Win32`, `Optimized|Win32`, `Release|Win32` (lines 1584-1586). Each of the **127 projects** has a `ProjectConfigurationPlatforms` block mapping `{GUID}.Config|Win32.ActiveCfg`/`.Build.0`.
- **The add:** insert `Debug|x64`, `Release|x64` (and `Optimized|x64` if chosen) into `SolutionConfigurationPlatforms`, then add `{GUID}.Debug|x64.ActiveCfg = Debug|x64` + `.Build.0` lines **for the boot-path subset only** (D-03). Editor-app GUIDs either get no x64 mapping (won't build under the x64 solution config) or map their x64 → their Win32 (so the solution config resolves but they build Win32). Cleanest: omit `.Build.0` for editor projects under x64 so they're excluded from an x64 solution build.

### Per-project `.vcxproj` (no shared common .props — confirmed)
- **`[VERIFIED: grep ProjectReference SwgClient.vcxproj]`** No shared common `.props` is imported across the ~127 projects; each is standalone. So the x64 add is **per-project** for the D-03 subset.
- Each boot-path `.vcxproj` needs, per the existing Win32 pattern:
  1. `<ProjectConfiguration Include="Debug|x64">` + `Release|x64` entries in the `ProjectConfigurations` ItemGroup.
  2. A `<PropertyGroup>` (Configuration|x64) setting `ConfigurationType` (StaticLibrary for engine libs, DynamicLibrary for gl0X plugins, Application for SwgClient), `PlatformToolset=v145`, charset.
  3. An `<ItemDefinitionGroup Condition="...x64">` with the ClCompile block (lift from `x64-compile.props` — the known-good x64 compile flags from Phase 31) and, for linking projects, the Link block with x64 `AdditionalDependencies` + `AdditionalLibraryDirectories` pointing at `compile/win32/<lib>/x64-Debug` (or an x64 output dir).
- **`[VERIFIED: x64-compile.props]`** The Phase-31 `x64-compile.props` is the known-good x64 ClCompile set: `/std:c++20 /EHsc /GR /Zc:wchar_t /MDd`, defines `PLATFORM_WIN32;WIN32;_DEBUG;_MBCS;DEBUG_LEVEL=2;_CRT_SECURE_NO_DEPRECATE=1;_LIB` (note: `_USE_32BIT_TIME_T` is DELIBERATELY DROPPED on x64 — UCRT hard-errors under `_WIN64`; see A2-TIME-T residual). **Recommendation:** promote this into a committed `x64-platform.props` imported by the boot-path projects' x64 ItemDefinitionGroups — it gives a single source of truth for the x64 compile block and re-establishes the N1 `/we4311 /we4312` guardrail (see Residual Worklist). The link block stays per-project (link inputs differ per project).
- **Output-dir collision caution:** the existing projects write to `src/compile/win32/<lib>/<Config>/`. The x64 platform MUST write to a distinct dir (e.g. `src/compile/win32/<lib>/x64/<Config>/` or use `$(Platform)` in the OutDir) so x64 `.obj`/`.lib` never clobber the shipped Win32 artifacts — otherwise the 32-bit non-regression gate is compromised.

### Build configurations to cover (Claude's Discretion, scoped by ROADMAP SC)
- **Debug + Release** for the engine libs and SwgClient.
- **All three D3D9 plugin flavors** — `Direct3d9` (gl05), `Direct3d9_ffp` (gl06), `Direct3d9_vsps` (gl07) are **three separate `.vcxproj`s** sharing the same source dir (`[VERIFIED: find Direct3d9*.vcxproj]`), each producing its DLL. All three need x64 configs.
- **Optimized** variant — planner's call (it's the shipped Release-with-symbols config; X64-02 boot validation can run on Debug or Release). Recommend deferring Optimized|x64 unless cheap.

### Projects built by the solution that also need x64 (in-tree-source third-party)
- `[VERIFIED: swg.sln]` `dpvs`, `zlib`, `ui`, `udplibrary` are **independent solution projects** (their own `.vcxproj`, built into `compile/win32/<name>/`), then SwgClient links the resulting `.lib`. These four each need an x64 config added to their `.vcxproj` so the x64 solution build produces x64 `.lib`s.

## Boot-Path Project List (D-03 — the x64 subset)

`[VERIFIED: x64-scratch/in-scope-tus.txt manifest header = 2216 TUs / 57 libs + swg.sln]`

**Engine/game libs (~53 StaticLibrary):**
archive, clientAnimation, clientAudio, clientBugReporting, clientDirectInput, clientGame, clientGraphics, clientObject, clientParticle, clientSkeletalAnimation, clientTerrain, clientTextureRenderer, clientUserInterface, crypto, fileInterface, libEverQuestTCG, localization, localizationArchive, sharedCollision, sharedCommandParser, sharedCompression, sharedDebug, sharedFile, sharedFoundation, sharedFractal, sharedGame, sharedImage, sharedInputMap, sharedIoWin, sharedLog, sharedMath, sharedMemoryManager, sharedMessageDispatch, sharedNetwork, sharedNetworkMessages, sharedObject, sharedPathfinding, sharedRandom, sharedRegex, sharedRemoteDebugServer, sharedSkillSystem, sharedStatusWindow, sharedSwitcher, sharedSynchronization, sharedTemplate, sharedTemplateDefinition, sharedTerrain, sharedThread, sharedUtility, sharedXml, swgClientUserInterface, swgSharedNetworkMessages, swgSharedUtility, unicode, unicodeArchive, blat

**In-tree third-party (StaticLibrary/DynamicLibrary, own .vcxproj):** zlib, dpvs, ui, udplibrary

**Renderer plugins (DynamicLibrary):** Direct3d9 (gl05), Direct3d9_ffp (gl06), Direct3d9_vsps (gl07) — **NOT Direct3d11/gl11** (Phase 34)

**Executable:** SwgClient

> Note: `sharedTemplateDefinition` appears in the manifest but Phase 31 reclassified it tool-only (links only into pre-broken editors; absent from SwgClient's link closure). Include it in the x64 platform-add only if SwgClient actually links it — verify against SwgClient's `AdditionalDependencies` (it is NOT in the list reviewed). Likely **excludable**.

## D3DX Removal Residual Surface (D-04 — the x64-link precondition)

> **`[VERIFIED: grep D3DX across boot-path tree]` D3DX is ENTIRELY confined to the `Direct3d9` plugin source.** Zero D3DX symbols in any other boot-path engine lib (clientGraphics, sharedMath, sharedFoundation, clientObject all clean). This scopes the entire D3DX-removal to **one plugin's `src/win32/` dir** (shared by gl05/06/07).

### Already done (Phase 32-02, landed)
- The **HLSL compile path** (`D3DXCompileShader` → `D3DCompile`) is ported. The residual `D3DXMACRO`/`ID3DXInclude`/`ID3DXBuffer`/`D3DXINCLUDE_TYPE` tokens in `Direct3d9_VertexShaderData.cpp` are **ABI-compatible typedefs/comments** (the actual compile call is `D3DCompile`); these resolve from `d3dx9shader.h` types that are layout-identical to the `d3dcompiler.h` equivalents. The planner should swap these to the native `D3D_SHADER_MACRO`/`ID3DInclude`/`ID3DBlob` names so `d3dx9*.h` can be dropped entirely (otherwise the include alone re-pulls the header).

### Asm path → D3DAssemble (32-03, PENDING — the gate already PASSED)
- **`[VERIFIED: 32-01-SUMMARY.md]` The D3DAssemble dialect gate PASSED byte-identical** (6/6 shaders incl. `lit` + `if b#`, `CreateVertexShader` S_OK). The asm port at `Direct3d9_VertexShaderData.cpp:760` (`D3DXAssembleShader` → `D3DAssemble`) is now a **mechanical `GetProcAddress`-on-undecorated-export swap** (the same resolve-once pattern the HLSL port uses). 32-04 (the redist fallback) is skipped.
- This is plan **32-03**, which the Phase-32 handoff explicitly hands to the x64 era. **Phase 33 owns landing 32-03** (and 32-05 Fix-A retirement) as Wave A.

### Non-compile D3DX → DirectXMath / own-impl (D-04a — the bulk of the residual)
`[VERIFIED: grep D3DX symbol census, Direct3d9 plugin]`

| API / type | Call sites (file:line) | Replacement | Difficulty |
|------------|------------------------|-------------|------------|
| `D3DXMATRIX` (type, 5 static members) | `Direct3d9.cpp:562-565`, `:4028` | `DirectX::XMFLOAT4X4` or `XMMATRIX` (header-only) | trivial |
| `D3DXMatrixMultiply` (4×) | `Direct3d9.cpp:3291,3357,3359,4034` | `XMMatrixMultiply` (note row/col-major convention — see Pitfall 1) | low |
| `D3DXMatrixTranspose` (1×) | `Direct3d9.cpp:4032` | `XMMatrixTranspose` | trivial |
| `D3DXMatrixMultiplyTranspose` (1×) | `Direct3d9.cpp:4031` | `XMMatrixTranspose(XMMatrixMultiply(...))` | trivial |
| `D3DXLoadSurfaceFromSurface` (4× — CORRECTED from 8×, live grep 2026-06-17) | `Direct3d9_RenderTarget.cpp:317`, `Direct3d9_TextureData.cpp:549,603,681` | **own-impl** — lock both surfaces + copy/convert via `IDirect3DDevice9::StretchRect` or a manual `D3DLOCKED_RECT` blit (`D3DX_FILTER_NONE` = point copy, simplest). **Per-site pre-read REQUIRED: `:681` `copyFrom()` uses NON-NULL src+dst rects (potential scaled blit) — a naive whole-surface point-copy would garble it; the other 3 use NULL/whole-surface.** Record each site's `D3DFORMAT`+pool+dims+rects before choosing StretchRect vs lock-blit. | medium |
| `D3DXCreateMeshFVF` + `ID3DXMesh` (Optimize/Adjacency/Lock) | `Direct3d9.cpp:4619-4679` | **own-impl** — make `optimizeIndexBuffer` a CALLABLE empty-body function (mirror D3D11's `STUB(optimizeIndexBuffer)` at `Direct3d11.cpp:1185`) — it is a real GL-API function-pointer slot (`Gl_dll.def:220`, registered `Direct3d9.cpp:1251`, exposed `Graphics.cpp:3379`, called from `clientSkeletalAnimation/SoftwareBlendSkeletalShaderPrimitive.cpp:1421`), so leaving the slot UNSET → null-pointer boot crash on the skeletal path (SC#4 risk). **The "NvTriStrip already linked" alternative is a PHANTOM** — `GenerateStrips`/`PrimitiveGroup` do not appear in the Direct3d9 plugin source. (the reorder is a perf nicety, not correctness — a callable empty-body leaves correct un-reordered indices) | medium-high |
| `D3DXSaveSurfaceToFile` (1×) | `Direct3d9.cpp:2890` (`D3DXIFF_BMP`) | **own-impl** — screenshot path; write a BMP/TGA from a locked surface (the engine already has TGA writers in sharedImage) | low-medium |
| `D3DXSaveTextureToFile` (1×) | `Direct3d9.cpp:4800` | **own-impl** — same as above, texture dump path | low-medium |

- **`[CITED: docs.microsoft.com DirectXMath]` DirectXMath is header-only, x64-native, ships with the Windows SDK** (`<DirectXMath.h>`) — the matrix conversions are the easy 80%. The surface/mesh/save helpers are the harder 20% (own-impl, ~6 call sites total).
- **ABI-cascade caution:** `Direct3d9.cpp`'s `ms_cached*Matrix` are `D3DXMATRIX` **file-static** members, not in a shared header — so changing them to `XMMATRIX` does NOT trigger the shared-header ABI cascade. But verify no shared `.h` consumed by gl05/06/07 exposes a D3DX type (grep showed none — confirmed clean).

## D3DAssemble Census Plan (D-05)

- **The census is DONE** — `[VERIFIED: 32-CENSUS.md + 32-01-SUMMARY.md]` corpus = **286 unique `.vsh` = 190 //hlsl + 96 //asm, 0 unclassified**. The D3DAssemble dialect probe (`.planning/research/vsh-probe/d3dassemble_probe.cpp`) proved **bit-identical** bytecode to `D3DXAssembleShader` on a representative sample (sizes 588–1636 B, FNV hashes match, `CreateVertexShader` S_OK).
- **Acceptance for 32-03 (lift into the engine):** the runtime asm path produces a valid VS for the representative shaders AND renders clean at the Tatooine bump/dot3 reference scene; **no VS silently nulled (no skipped draws)** — the D3D9 gl05/06/07 path natively executes the assembled vs_1_1/vs_2_0, so a fallback substitute (gl11's `[P19VSFALLBACK]`) is NOT an option and must not be introduced.
- **Contingency (D-05, NOT chosen):** if a non-probed shader fails render in 32-03, roll back, re-route to the (rejected) redist-d3dx9 path **only as a surfaced checkpoint** — never silently. The 32-01 PASS makes this contingency low-probability.

## dpvs x64 Port Mechanics (D-02)

`[VERIFIED: dpvsPrivateDefs.hpp:129-130, dpvsX86.cpp, dpvsBitMath.hpp]`

- **The critical gotcha the planner MUST know:** dpvs defines `DPVS_CPU_X86` **unconditionally for `DPVS_OS_WIN32`** (`dpvsPrivateDefs.hpp:129-130`), which then defines `DPVS_X86_ASSEMBLY` (`:316-317`). It does **NOT** check `_M_X64`/`_WIN64`. So an x64 build will still try to compile the `__asm` blocks (x64-illegal → error). **D-02's "leave `DPVS_X86_ASSEMBLY` undefined on x64" is NOT automatic — it requires an explicit source edit** to the CPU-detection block: guard `DPVS_CPU_X86` (or just `DPVS_X86_ASSEMBLY`) on `#if !defined(_M_X64)`, or add a `_M_X64` branch. This is a small, self-contained edit but it is mandatory.
- **`__asm` sites + C fallbacks confirmed:** the `__asm` blocks live in `dpvsX86.cpp` (10+ sites, all inside one `#if defined(DPVS_X86_ASSEMBLY)`/`#endif` span :32-404), `dpvsMath.cpp`, `dpvsOcclusionBuffer_CacheFiller.cpp`, `dpvsOcclusionBuffer_Edges.cpp`, and the inline math in `dpvsBitMath.hpp`/`dpvsFiller.hpp`. The `dpvsBitMath.hpp` bit ops (`getHighestSetBit`/`getNextPowerOfTwo`) have a `#if !defined(DPVS_X86_ASSEMBLY)` C/LUT fallback (`:155`). **Once `DPVS_X86_ASSEMBLY` is undefined, the C `#else` paths compile automatically** — D-02's claim holds.
- **Optional parity upgrade (Claude's Discretion):** swap the `bsr` LUT fallback to `_BitScanReverse` intrinsics for performance parity. Not required for correctness.
- **Self-contained pointer-truncation mini-pass (D-02):** dpvs was OUT of Phase 31 scope, so it carries unaudited C4311/C4312 (pointer-in-int) defects. The x64 compile of `dpvs.vcxproj` will surface them (apply the same `/we4311 /we4312` as the rest). Budget a small truncation pass over the dpvs sources — it's a self-contained third-party lib, so the blast radius is bounded.
- **Fallback:** if the dpvs x64 build stalls, Restoration's `dpvs.dll` (424 KB) is the D-01 escape hatch (lift + gen import lib). The from-source build is preferred.

## x64 Link/Runtime Residual Worklist (from Phase 31 hand-off)

`[VERIFIED: 31-PHASE33-RESIDUAL-WORKLIST.md]` — these are the items Phase 31 explicitly handed forward that LAND in Phase 33:

| ID | Item | Phase-33 action | Lands in |
|----|------|-----------------|----------|
| **A2-3RDPARTY-LIBS** | All third-party x64 libs unresolved (compile-only harness never linked) | The Per-Lib Sourcing Table above — resolve at link | Wave B |
| **A2-TIME-T** | `_USE_32BIT_TIME_T` dropped on x64 → `time_t` is 8 bytes; re-audit any serialized `time_t` struct | Decide x64 `time_t` width policy + re-audit `time_t`-bearing wire/blob structs against 32-bit format | Wave B (audit) |
| **A1-DBGHELP-WALK** | x64 `StackWalk64` over AMD64 context — compile-clean only, never run | RUNTIME-validate the x64 stack walk produces correct frames | Wave C (runtime) |
| **A1-DBGHELP-RIP** | callStack output buffer is `uint32*` → Rip narrowed to 32 bits on x64 | Widen the callStack output contract to `DWORD64`/`uintptr_t` (or document accepted truncation) | Wave B/C |
| **A1-SSE-ALIGN** | `_mm_loadu_ps` SSE rewrite — alignment-fault class only fires at runtime on x64 movaps | RUNTIME-confirm no alignment fault across live matrix/skin paths (boot smoke + `_DEBUG` oracles = trip-wire) | Wave C (runtime) |
| **N1** | No standing `/we4311 /we4312 /we4244` guardrail survives the scratch-harness deletion | **RE-ESTABLISH** `/we4311 /we4312` as committed errors on the x64 `.vcxproj`s (fold into `x64-platform.props`) | Wave B |
| **N2** | C4267 (`size_t`→smaller) deliberately not promoted in P31; D-07-excluded Archive.h C4244 must NOT be "fixed" | Decide C4267 policy for the committed x64 platform; do NOT touch the D-07 Archive.h exclusion | Wave B (policy) |
| **A2-WATERTEST** | `WaterTestAppearance.cpp:97` `reinterpret_cast<int>` — test code, not boot path | Fix opportunistically (or leave; not on boot path) | optional |
| **A1-FPU-PC** | x87 P_24 precision omitted on x64 (door-snap watch-item) | **Phase 36** (VERIFY-01) — NOT Phase 33 | Phase 36 |

> **WaterTest** (referenced in the additional-context) = A2-WATERTEST above: a test-harness explicit cast, **not** on the boot path, fix opportunistically. **Bink** residual = the DLL-name swap (above), trivial. The N1 guardrail re-establishment and N2 C4267 policy are the two standing-coverage holes Phase 33 must close.

## Runtime State Inventory

> Phase 33 is a build/link/procurement phase. No stored data, no live-service config, no OS-registered state, no secrets are renamed. The one runtime-state concern is **staged DLLs**.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — no database/datastore keys touched | None — verified: no rename/migration in scope |
| Live service config | None | None |
| OS-registered state | None | None |
| Secrets/env vars | None | None |
| Build artifacts / staged DLLs | **`stage/` holds the Win32 gl0X_*.dll + SwgClient_*.exe + binkw32.dll + Mss32 redist.** The x64 build must stage **x64** `gl0X_*.dll`, `SwgClient_*.exe`, `binkw64.dll`, `d3dcompiler_47.dll` (x64), `dpvs.dll`/`libxml2.dll`/`pcre.dll`/`jpeg62.dll` (x64). x86 and x64 staged DLLs CANNOT coexist in the same `stage/` dir (a 32-bit exe loading a 64-bit DLL = load failure). | Use a separate `stage-x64/` (or a platform-conditioned postbuild) so the shipped 32-bit `stage/` is never clobbered — protects the non-regression gate. The existing postbuild auto-copies into `stage/`; the x64 postbuild needs a distinct target dir. |

## Common Pitfalls

### Pitfall 1: DirectXMath row-major vs D3DX/D3D9 convention
**What goes wrong:** `D3DXMatrixMultiply(out, A, B)` computes `out = A*B` (row-major, D3DX convention). `XMMatrixMultiply(A, B)` also returns `A*B` row-major — but the engine's cached matrices and the D3D9 fixed-function pipeline expect specific row/column ordering. A naive 1:1 swap can transpose a matrix and silently break the world transform.
**Why it happens:** D3DX and DirectXMath are both row-major and *mostly* drop-in, but the `D3DXMatrixMultiplyTranspose` at `Direct3d9.cpp:4031` and the cbuffer-upload transpose convention (see memory `d3d11_cbuffer_transpose_quirk` — col-vec engine vs row-vec bytecode) mean transpose points must be preserved exactly.
**How to avoid:** Convert one matrix op at a time; after each, A/B the rendered frame against the 32-bit reference (gl05 Tatooine). Keep the exact transpose at `:4031`. The `_DEBUG` numeric-equivalence oracle pattern from Phase 31 (SseMath) is the model.
**Warning signs:** mirrored/inside-out geometry, world transform off, skybox flipped.

### Pitfall 2: x64/x86 staged-DLL collision
**What goes wrong:** Dropping x64 `gl0X_d.dll` into the same `stage/` as the Win32 `SwgClient_d.exe` → "is not a valid Win32 application" or silent LoadLibrary failure at renderer init.
**How to avoid:** Separate `stage-x64/` (or `$(Platform)`-conditioned postbuild). Confirm exe + all gl0X + all third-party DLLs are uniformly x64 before any boot attempt (`dumpbin /headers | grep machine`).

### Pitfall 3: /FORCE link masks unresolved externals on the FIRST x64 link
**What goes wrong:** SwgClient links under `/FORCE`; the first x64 link will have many unresolved externals (every third-party x64 lib not yet resolved, every D3DX symbol not yet removed) but still emit a binary with exit 0.
**How to avoid:** **`[VERIFIED: AGENTS.md]`** Grep every x64 link log for `unresolved external symbol` (must be 0). Exit 0 is NOT proof. This is THE x64-link acceptance signal.
**Warning signs:** exe produced but crashes instantly at boot / missing-entry-point.

### Pitfall 4: dpvs CPU-detection compiles x64-illegal asm
**What goes wrong:** dpvs's `DPVS_CPU_X86` is WIN32-unconditional (ignores `_M_X64`) → x64 build tries to assemble `__asm` blocks → C4235/compile error.
**How to avoid:** Explicit source edit to guard `DPVS_X86_ASSEMBLY`/`DPVS_CPU_X86` on `!_M_X64` BEFORE adding the x64 config (see §dpvs). This is the #1 dpvs surprise.

### Pitfall 5: Miles x86 static lib blocks the x64 clientAudio link
**What goes wrong:** `clientAudio` calls `AIL_startup()` directly and links `Mss32.lib` (x86 static import). The x64 link will fail with a machine-mismatch on `Mss32.lib`.
**How to avoid:** Phase 33 must `#if !_M_X64`-out (or stub) the Miles call sites for the x64 build, OR accept clientAudio cannot link x64 until Phase 35. Decide early (Open Question Q1) — it gates whether the x64 client links at all.

### Pitfall 6: prebuilt-lib import-lib generation machine mismatch
**What goes wrong:** Generating an x64 import lib from a 32-bit DLL, or from the x86 `.def`, produces an x86 import lib that fails the x64 link.
**How to avoid:** Generate the import lib from the **Restoration x64 DLL's** exports: `dumpbin /exports libxml2.dll` (the x64 one) → `.def` → `lib /machine:x64 /def:libxml2.def /out:libxml2.lib`. Verify `dumpbin /headers` on the result shows x64.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Legacy DXSDK `d3dx9.lib` (static, x86-only, FP-crash on modern toolchain) | `d3dcompiler_47` (`D3DCompile`/`D3DAssemble`) + DirectXMath (header-only) | Phase 32 (compile path) + Phase 33 (non-compile) | D3DX fully removed → x64-clean link possible |
| x87 inline asm / pointer-truncation defects | intrinsics + width-correct types (Phase 31) | Phase 31 | Tree compiles x64-clean; Phase 33 is link/procurement only |
| Bink static import (assumed) | Bink dynamic `LoadLibrary` (always was) | — | x64 = one-line DLL-name swap, no import lib |

**Deprecated/outdated:**
- The vendored `directx9/lib/*.lib` (x86 DXSDK) — for x64, `d3d9.lib`/`d3dcompiler.lib` come from the Windows SDK (platform toolset), and `d3dx9*` is gone entirely.
- pcre's `libpcre.a` (MinGW archive) — must become a real x64 MSVC `.lib`.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | **icu and discord-rpc are NOT dependencies of THIS client** (not vendored, not referenced — they are Restoration-specific) | Per-Lib Sourcing Table, finding #2 | LOW-MED — if some feature does pull icu/discord, X64-04 gains two libs (both available in Restoration x64 as fallback). Confirm with user before declaring X64-04's "icu/discord-rpc resolve" satisfied-by-N/A. |
| A2 | The x64 `time_t` width policy can be resolved by serializing as int32/uint32 at the few `time_t`-bearing wire sites (Phase 31 already audited GroupObject/SwgCuiGroup as display-only) | Residual Worklist A2-TIME-T | LOW — Phase 31 already triaged the known sites as display; a missed serialized `time_t` would be a data-layout bug. The static_assert guards from Phase 31 are the trip-wire. |
| A3 | `D3DXLoadSurfaceFromSurface` can be replaced by `StretchRect`/manual lock-blit with `D3DX_FILTER_NONE` semantics (point copy, same-format) | D3DX Removal Surface | MED — if any call site relies on D3DX format-conversion or filtering, a naive blit changes pixels. The **4** sites (CORRECTED from 8) are render-target/texture copies; verify each is same-format point-copy. **`:681` `copyFrom()` uses NON-NULL src+dst rects (scaled-blit risk) — record `D3DFORMAT`+pool+dims+rects per site first.** |
| A4 | Skipping the `D3DXCreateMeshFVF`/`OptimizeInplace` vertex-cache optimization is acceptable (perf nicety, not correctness) — BUT the slot must remain CALLABLE | D3DX Removal Surface | LOW-MED — it's an index-buffer reorder for GPU vertex-cache efficiency; skipping it costs a little perf but renders identically. **`optimizeIndexBuffer` is a real GL-API function-pointer slot (`Gl_dll.def:220`, `Direct3d9.cpp:1251`, `Graphics.cpp:3379`, called `SoftwareBlendSkeletalShaderPrimitive.cpp:1421`) — register a callable empty-body function (mirror D3D11 `STUB(optimizeIndexBuffer)` `Direct3d11.cpp:1185`); an UNSET slot = null-ptr boot crash on the skeletal path (SC#4). NvTriStrip is a PHANTOM (`GenerateStrips`/`PrimitiveGroup` absent from Direct3d9 source).** Confirm the mesh path isn't load-bearing for collision/LOD use. |
| A5 | The x64 client booting **audio-degraded** (Miles on x86, stubbed for x64) satisfies X64-02 (boot-to-char-select), with full audio deferred to Phase 35 | Pitfall 5 / Open Q1 | LOW — ROADMAP explicitly scopes audio to Phase 35 and says "the x64 client may link/boot with audio degraded until then." |

## Open Questions

1. **Miles/clientAudio x64 linkage for Phase 33.**
   - What we know: clientAudio links x86 `Mss32.lib` + calls `AIL_startup()` directly; the x64 port (mss64/9.3b) is Phase 35.
   - What's unclear: does Phase 33 stub/`#if`-out the Miles calls so clientAudio links x64 (audio dead but client boots), or does clientAudio stay out of the x64 link until P35?
   - Recommendation: **stub the Miles call sites under `#if _M_X64`** (return success/no-op) so clientAudio links x64 and the client boots silent — minimal, reversible, matches the ROADMAP "audio degraded until P35." Surface as a planning decision.

2. **Optimized|x64 configuration — include or defer?**
   - What we know: ROADMAP SC requires Debug + Release + the 3 plugin flavors; Optimized is the shipped Release-with-symbols config.
   - Recommendation: defer Optimized|x64 to a follow-up unless it's near-free; X64-02 boot validation runs on Debug or Release.

3. **tinyxml x64 — author a .vcxproj or fold the source?**
   - What we know: tinyxml has in-tree source but builds via legacy `.dsp`; **no Restoration x64 DLL fallback exists** (the only dep with no escape hatch).
   - Recommendation: author a minimal x64 `.vcxproj` (3 `.cpp`, pure C++) or compile the sources directly into the consuming lib (sharedXml). Low effort either way.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSBuild VS18/v145 (`$env:MSBUILD`) | x64 platform build | ✓ | VS18/2026 BuildTools | — (v145 toolset mandatory) |
| Windows SDK x64 `d3d9.lib` / `d3dcompiler.lib` | x64 plugin link | ✓ | Win10/11 SDK | — |
| x64 `D3DCompiler_47.dll` | runtime shader compile | ✓ | `C:\Windows\System32` (x64) | SDK redist x64 copy |
| Restoration x64 DLLs (bink/dpvs/libxml2/pcre/jpeg) | X64-04 lift/fallback | ✓ | `D:\SWG Restoration\x64\` | — (this IS the fallback) |
| DirectXMath (`<DirectXMath.h>`) | non-compile D3DX replacement | ✓ | Windows SDK header-only | — |

**Missing dependencies with no fallback:**
- **tinyxml x64** — no Restoration DLL; must build from in-tree source.

**Missing dependencies with fallback:**
- libxml2 / pcre / jpeg x64 — no in-tree build project; lift Restoration x64 DLL + gen import lib (D-01 universal fallback).
- dpvs x64 from-source stall → Restoration `dpvs.dll`.

## Validation Architecture

> nyquist_validation = true (config.json). No automated shader/render test framework exists — **builds/boots/renders are the truth** (AGENTS.md). Validation is build-gate + dumpbin + boot smoke.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None (C++ MSBuild; no unit-test harness). Validation = MSBuild + dumpbin + manual dual-bitness boot smoke |
| Config file | `src/build/win32/swg.sln` (+ per-project `.vcxproj`) |
| Quick run command | x64 plugin/lib build: `& $env:MSBUILD swg.sln /t:<proj> /p:Configuration=Debug /p:Platform=x64 /nodeReuse:false` |
| Full suite command | Canonical 5-target x64 build (Direct3d9;Direct3d9_ffp;Direct3d9_vsps;SwgClient) `/p:Platform=x64` + the same `/p:Platform=Win32` non-regression build |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| X64-01 | SwgClient_*.exe is x64 | build + dumpbin | `dumpbin /headers stage-x64/SwgClient_d.exe \| grep -i "machine"` → `8664 (x64)` | ✅ build infra exists |
| X64-01 | x64 platform in sln + boot-path vcxprojs | grep | `grep "Debug|x64" swg.sln` + per-project ProjectConfiguration check | ✅ |
| X64-04 | all third-party x64 libs link; no unresolved | link-grep | x64 link log `grep "unresolved external symbol"` == 0 (the /FORCE trap) | ✅ |
| X64-04 | no missing-DLL at boot | runtime boot | launch x64 exe; no missing-DLL popup; reaches char-select | ❌ manual smoke (Wave 0: confirm `stage-x64/` has all x64 DLLs via dumpbin) |
| X64-02 | gl05/06/07 are x64 | dumpbin | `dumpbin /headers stage-x64/gl0{5,6,7}_d.dll \| grep machine` → x64 | ✅ |
| X64-02 | x64 boots char-select under rasterMajor 5/6/7 | runtime boot | set `rasterMajor=5/6/7` in `client_d.cfg`; launch x64 `SwgClient_d.exe`; reach char-select (3 boots) | ❌ manual smoke |
| SC#4 | 32-bit non-regression | build + boot | full Win32 5-target build links clean (0 unresolved) + dual-renderer boot smoke (rasterMajor 5 + 11) unchanged | ✅ (Phase 31 baseline) |

### Sampling Rate
- **Per task commit:** the affected x64 project/lib builds clean (`/p:Platform=x64`) + link-grep 0 unresolved (for linking projects).
- **Per wave merge:** canonical 5-target x64 build + Win32 non-regression build; both 0 unresolved.
- **Phase gate:** dumpbin x64 on exe+all 3 plugins; x64 boot to char-select under rasterMajor 5/6/7; Win32 boot under rasterMajor 5 + 11 unchanged; all DLLs in `stage-x64/` dumpbin-x64.

### Wave 0 Gaps
- [ ] `stage-x64/` (or platform-conditioned postbuild target) — so x64 DLLs never clobber the shipped Win32 `stage/` (non-regression protection)
- [ ] x64 import-lib generation for libxml2/pcre/jpeg (from Restoration x64 DLL exports) — needed before the x64 link
- [ ] tinyxml x64 build path (author `.vcxproj` or fold source) — only dep with no DLL fallback
- [ ] A boot-time DLL-presence checklist (dumpbin-x64 on every staged DLL) before the first x64 boot attempt
- [ ] Re-establish committed `/we4311 /we4312` on the x64 platform (N1) — fold into `x64-platform.props`

## Security Domain

> security_enforcement = true (config.json). Phase 33 is a build-platform/link/procurement phase — no new attack surface, no input handling, no network/auth/session/crypto code changes. The relevant security concern is **supply-chain integrity of lifted binaries.**

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — (no auth code touched) |
| V3 Session Management | no | — |
| V4 Access Control | no | — |
| V5 Input Validation | no | — (no new input parsing; shader/asset paths unchanged) |
| V6 Cryptography | no | — (`crypto.lib` relinked x64, not modified) |
| V14 Configuration / Supply Chain | **yes** | Lifted x64 DLLs (binkw64.dll, dpvs.dll, libxml2/pcre/jpeg from `D:\SWG Restoration\x64\`) are **third-party binaries from a non-first-party source**. Record provenance (Restoration x64 redist) + sizes/hashes when lifting; prefer build-from-source where possible (zlib/dpvs/ui/udplibrary — D-01's whole rationale). |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Lifted binary tampering / unknown provenance | Tampering | Prefer from-source build (D-01); for genuine lifts (bink/libxml2/pcre/jpeg), document the source DLL + hash; these are the same vetted DLLs the Restoration client ships |
| DLL hijacking via `stage-x64/` search order | Elevation/Tampering | Bink/Miles `LoadLibrary` uses bare DLL names — ensure `stage-x64/` is the controlled load dir (existing pattern); no change from Win32 behavior |

## Sources

### Primary (HIGH confidence — filesystem-verified this session)
- `D:\SWG Restoration\x64\` directory listing — the x64 availability map (binkw64.dll, dpvs.dll, libxml2.dll, pcre.dll, jpeg62.dll, icudt62.dll, libicuuc.dll, discord-rpc.dll, mss64.dll, gl05/06/07_r.dll)
- `src/external/3rd/library/` tree — in-tree source vs prebuilt-lib disposition per dep
- `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` — link inputs + ProjectConfigurations (Debug/Optimized/Release|Win32)
- `src/engine/client/application/Direct3d9/build/win32/Direct3d9{,_ffp,_vsps}.vcxproj` — plugin link inputs + configs
- `src/build/win32/swg.sln:1583-1596` — solution-config block (100% Win32)
- `src/build/win32/x64-scratch/x64-compile.props` + `in-scope-tus.txt` — Phase 31 x64 compile flags + 2216-TU/57-lib manifest
- grep D3DX across boot-path tree — D3DX confined to Direct3d9 plugin; full symbol census
- `BinkVideo.cpp:55` / `BinkDLL.cpp:139` — Bink dynamic LoadLibrary("binkw32.dll")
- `Audio.cpp:1284,1280` — Miles `AIL_startup()` + `AIL_set_redist_directory`
- `dpvs/implementation/include/dpvsPrivateDefs.hpp:129-130,316-317` — DPVS_CPU_X86 WIN32-unconditional
- `.planning/phases/31-64-bit-correctness-foundation/31-PHASE33-RESIDUAL-WORKLIST.md` — the (A)/(B) residual hand-off
- `.planning/phases/32-d3dx-to-d3dcompiler-47/32-01-SUMMARY.md` + `32-CENSUS.md` — D3DAssemble gate PASS + corpus
- `.planning/handoff/phase-32-d3dcompile-port.md` — 32-02 landed, 32-03/05 pending

### Secondary (MEDIUM — referenced)
- `.planning/STATE.md`, `ROADMAP.md`, `REQUIREMENTS.md` — milestone sequencing, X64-01/02/04 wording, core-invariant extension
- `AGENTS.md` §Build — /FORCE link-grep, /nodeReuse:false, rasterMajor 5/6/7 mapping, shared-header ABI cascade
- Memory `reference_restoration_binaries_intel` — Restoration's compile model + x64 D3DX-redist reality (rejected)

### Tertiary (LOW / training)
- DirectXMath API equivalence to D3DX matrix functions (CITED to MS docs convention, row-major; verify transpose points at render time — Pitfall 1)

## Metadata

**Confidence breakdown:**
- Per-lib sourcing table: HIGH — every dep filesystem-verified against both the in-tree source and Restoration x64 dir.
- Platform-add mechanics: HIGH — sln + vcxproj structure read directly; no shared .props confirmed.
- D3DX removal surface: HIGH — full symbol census; D3DX confined to one plugin confirmed.
- D3DAssemble: HIGH — gate already PASSED byte-identical in Phase 32-01.
- dpvs x64 mechanics: HIGH — CPU-detection gotcha read from source; asm-gating confirmed.
- DirectXMath conversion correctness: MEDIUM — header-only & x64-native is certain; transpose-convention parity needs render-time A/B (Pitfall 1).
- Miles x64 linkage decision: MEDIUM — needs a planning decision (Open Q1).

**Research date:** 2026-06-17
**Valid until:** 30 days (stable build-config domain; Restoration x64 dir + in-tree source unlikely to shift). Re-verify if 32-03/32-05 land before Phase 33 planning (they change the D3DX-removal residual surface).

## RESEARCH COMPLETE

Biggest planning risks, ranked:
1. **Miles x64 linkage (Open Q1)** — clientAudio statically links x86 `Mss32.lib` + calls `AIL_startup()` directly; the x64 link FAILS on machine-mismatch unless the Miles calls are `#if _M_X64`-stubbed (audio dead until Phase 35). This gates whether the x64 client links **at all** — decide in Wave A.
2. **dpvs CPU-detection source edit** — dpvs defines `DPVS_CPU_X86`→`DPVS_X86_ASSEMBLY` unconditionally for WIN32 (ignores `_M_X64`); leaving the asm undefined on x64 is NOT automatic, it needs an explicit guard edit, plus dpvs's own pointer-truncation pass (it was out of Phase 31 scope).
3. **Non-compile D3DX → DirectXMath/own-impl** — confined to one plugin (good), but the 4 `D3DXLoadSurfaceFromSurface` (CORRECTED from 8; `:681` is a scaled-blit risk) + mesh-optimize (callable empty-body slot, NOT unset) + 2 save-to-file own-impls (A3/A4) and the matrix transpose-convention parity (Pitfall 1, across gl05/gl06/gl07) need render-time A/B against the 32-bit reference.
4. **tinyxml is the one dep with no Restoration x64 DLL fallback** — must build from in-tree source (small, but no escape hatch).
5. **stage/ collision + N1 guardrail** — x64 DLLs must go to a separate `stage-x64/` to protect the 32-bit non-regression gate, and the `/we4311 /we4312` guardrail (lost when the Phase-31 scratch harness was deleted) must be re-established on the committed x64 platform.
6. **/FORCE false-clean on the first x64 link** — the first link will be full of unresolved externals; only the `unresolved external symbol == 0` grep proves a real link, never exit 0.
