# Phase 33: x64 Build Platform + D3D9 Renderers - Pattern Map

**Mapped:** 2026-06-17
**Files analyzed:** ~64 vcxproj edits (boot subset) + swg.sln + 1 new props + ~4 source-edit clusters
**Analogs found:** 7 / 7 high-value analogs (all in-repo, exact)

> RESEARCH.md already has the call-site census (file:line for every D3DX symbol, every
> third-party lib, the dpvs gotcha). This file is the **"here is the exact existing block to
> copy"** layer: for each new/modified file, the closest existing block + the gotcha that
> makes a naive copy wrong.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/build/win32/swg.sln` (edit: add `*\|x64` configs) | config (solution) | build-config | itself, lines 1583-1604 (the `\|Win32` block) | exact (self-mirror) |
| `src/build/win32/x64-platform.props` (NEW — promote scratch) | config (shared props) | build-config | `x64-scratch/x64-compile.props` | exact (source) |
| ~53 engine StaticLibrary `.vcxproj` (add `Debug\|x64`/`Release\|x64`) | config (build) | build-config | `sharedMath.vcxproj` Win32 blocks | exact (role+flow) |
| `Direct3d9{,_ffp,_vsps}.vcxproj` (add x64 cfgs) | config (build, DynamicLibrary) | build-config | `Direct3d9.vcxproj` Win32 blocks | exact (self-mirror) |
| `SwgClient.vcxproj` (add x64 cfgs) | config (build, Application) | build-config | `SwgClient.vcxproj` Win32 blocks | exact (self-mirror) |
| `dpvs.vcxproj` + `dpvsPrivateDefs.hpp` edit | config + source (3rd-party) | build-config + `#if` guard | `dpvsPrivateDefs.hpp:129-131` | exact (the edit site) |
| `Direct3d9.cpp` (D3DXMATRIX → XMMATRIX) | source (renderer) | transform/matrix | `Direct3d11_StateCache.cpp:392-395` | role-match (D3D11 XMMatrix precedent) |
| `BinkVideo.cpp:55` (DLL-name `#if _M_X64`) | source (dynamic DLL load) | file-I/O / dynamic-load | itself (`s_dllName`/`bindBink`) | exact (self) |

---

## Pattern Assignments

### swg.sln — add `Debug|x64` / `Release|x64` (config, build-config)

**Analog:** itself — `src/build/win32/swg.sln:1583-1604`.

**SolutionConfigurationPlatforms** (lines 1583-1587) — add the x64 rows alongside:
```
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		Debug|Win32 = Debug|Win32
		Optimized|Win32 = Optimized|Win32
		Release|Win32 = Release|Win32
		Debug|x64 = Debug|x64          <-- ADD
		Release|x64 = Release|x64      <-- ADD
	EndGlobalSection
```

**Per-project ProjectConfigurationPlatforms** (lines 1589-1600 show the `{GUID}.Cfg|Win32`
pattern). For each **boot-path** project GUID add the four x64 lines mirroring its Win32 mapping:
```
		{GUID}.Debug|x64.ActiveCfg = Debug|x64
		{GUID}.Debug|x64.Build.0 = Debug|x64
		{GUID}.Release|x64.ActiveCfg = Release|x64
		{GUID}.Release|x64.Build.0 = Release|x64
```

**Gotcha (D-03 scope):** Editor-app GUIDs (AnimationEditor/SwgGodClient/…) must get **no
`.Build.0`** under x64 (only `.ActiveCfg` pointing at their Win32 cfg, or omit entirely) so an
x64 solution build does not try to build the pre-broken editors. Note `{3D6683D1…}` at :1603
already maps `Optimized|Win32.ActiveCfg = Debug|Win32` — the sln already uses cross-config
mappings, so this is an established shape. Only the ~64 boot-subset GUIDs get a buildable x64
mapping.

---

### x64-platform.props (NEW — promote the scratch props, config/shared)

**Analog/source:** `src/build/win32/x64-scratch/x64-compile.props` — the known-good Phase-31
x64 ClCompile set. Note its header explicitly says it is "UNCOMMITTED / gitignored … must NOT
be imported by any committed swg.sln/.vcxproj (the committed x64 platform add is **Phase 33
scope**)." Phase 33 is where it becomes committed.

**The committed x64 ClCompile block** to lift (scratch lines 92-108), as a real
`ItemDefinitionGroup` keyed on `'$(Platform)'=='x64'`:
```xml
<ItemDefinitionGroup Condition="'$(Platform)'=='x64'">
  <ClCompile>
    <PreprocessorDefinitions>PLATFORM_WIN32;WIN32;_DEBUG;_MBCS;DEBUG_LEVEL=2;_CRT_SECURE_NO_DEPRECATE=1;_LIB;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    <RuntimeTypeInfo>true</RuntimeTypeInfo>
    <TreatWChar_tAsBuiltInType>true</TreatWChar_tAsBuiltInType>
    <LanguageStandard>stdcpp20</LanguageStandard>
    <WarningLevel>Level4</WarningLevel>
    <TreatSpecificWarningsAsErrors>4311;4312</TreatSpecificWarningsAsErrors>   <!-- N1 guardrail re-establish -->
    <ExceptionHandling>Sync</ExceptionHandling>
  </ClCompile>
</ItemDefinitionGroup>
```

**Gotchas (from the scratch props' own header, lines 24-59):**
1. **`_USE_32BIT_TIME_T` MUST be omitted on x64** — the committed Win32 blocks set
   `_USE_32BIT_TIME_T=1` (see `sharedMath.vcxproj:74`), but the UCRT `corecrt.h` hard-errors
   (`C1189`) on it under `_WIN64`. Dropping it is mandatory; the consequence (`time_t` is 8
   bytes on x64) is the **A2-TIME-T** serialization residual, not a flag fix.
2. **N1 guardrail (`/we4311 /we4312`)** was lost when the scratch harness is deleted — re-fold
   it here as `TreatSpecificWarningsAsErrors`. Research drops `4244` from the committed set
   (the scratch had it; the committed tree never had a `/wd4244`), and **C4267 is deliberately
   NOT promoted** (N2 — floods on `int len = container.size()` count paths). Pick the committed
   set = `4311;4312` only.
3. The scratch props' `Scratch*` flat properties (lines 65-77) are PowerShell-driver-only — do
   NOT copy those; only the `ItemDefinitionGroup` form (lines 92-108) is the committed analog.

---

### Engine StaticLibrary `.vcxproj` (×~53) — add x64 configs (config, build-config)

**Analog:** `src/engine/shared/library/sharedMath/build/win32/sharedMath.vcxproj` — the
canonical StaticLibrary shape, all three Win32 blocks present.

**1. ProjectConfigurations ItemGroup** (sharedMath :3-16) — add two entries:
```xml
<ProjectConfiguration Include="Debug|x64">
  <Configuration>Debug</Configuration>
  <Platform>x64</Platform>
</ProjectConfiguration>
<ProjectConfiguration Include="Release|x64">
  <Configuration>Release</Configuration>
  <Platform>x64</Platform>
</ProjectConfiguration>
```

**2. Configuration PropertyGroup** (sharedMath :35-40 is the Debug|Win32 one) — mirror per x64
config, `ConfigurationType` stays **StaticLibrary**:
```xml
<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration">
  <ConfigurationType>StaticLibrary</ConfigurationType>
  <PlatformToolset>v145</PlatformToolset>
  <UseOfMfc>false</UseOfMfc>
  <CharacterSet>MultiByte</CharacterSet>
</PropertyGroup>
```

**3. OutDir/IntDir PropertyGroup** (sharedMath :57-60 Win32) — **MUST use a distinct x64 dir**:
```xml
<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
  <OutDir>..\..\..\..\..\..\compile\win32\$(ProjectName)\x64\$(Configuration)\</OutDir>
  <IntDir>..\..\..\..\..\..\compile\win32\$(ProjectName)\x64\$(Configuration)\</IntDir>
</PropertyGroup>
```
The Win32 OutDir is `…\compile\win32\$(ProjectName)\$(Configuration)\` — inserting `\x64\`
(or using `$(Platform)`) keeps x64 `.obj`/`.lib` from clobbering the shipped Win32 artifacts
(the 32-bit non-regression gate). **The SwgClient x64 link's `AdditionalLibraryDirectories`
must point at these same x64 dirs** (see SwgClient below) — keep the convention identical.

**4. ItemDefinitionGroup** (sharedMath Debug|Win32 = :69-103) — either import
`x64-platform.props` for the ClCompile block, or inline-mirror it. Keep the per-project
`AdditionalIncludeDirectories` (sharedMath :73) and the PCH names (`FirstSharedMath.h`, :81)
**unchanged** — those are per-project and identical across platforms. Keep the `<Lib>`
`OutputFile` `$(OutDir)$(ProjectName).lib` (:95-98).

**Gotcha:** sharedMath sets `_USE_32BIT_TIME_T=1` in its Win32 `PreprocessorDefinitions`
(:74) — the x64 block must NOT carry it (see props gotcha #1). If you import
`x64-platform.props` for the defines this is automatic; if you inline, delete that token.

---

### Direct3d9 / Direct3d9_ffp / Direct3d9_vsps (×3) — add x64 (config, DynamicLibrary)

**Analog:** `src/engine/client/application/Direct3d9/build/win32/Direct3d9.vcxproj` (gl05).
`_ffp` (gl06) and `_vsps` (gl07) are separate `.vcxproj`s over the same source dir — same shape,
different output name + `FFP`/`VSPS` defines.

**ProjectConfiguration + Configuration PropertyGroup** — identical to the StaticLibrary pattern
above except `ConfigurationType` is **DynamicLibrary** (Direct3d9 :34-39).

**The load-bearing x64 delta is the `<Link>` block** (Direct3d9 Debug|Win32 = :114-127). The
Win32 block:
```xml
<Link>
  <AdditionalDependencies>libjpeg.lib;odbc32.lib;odbccp32.lib;winmm.lib;delayimp.lib;dxguid.lib;d3d9.lib;d3dx9.lib;d3dcompiler.lib;ddraw.lib;dxerr9.lib;legacy_stdio_definitions.lib;version.lib;%(AdditionalDependencies)</AdditionalDependencies>
  <OutputFile>..\..\..\..\..\..\compile\win32\Direct3d9\Debug\gl05_d.dll</OutputFile>
  <AdditionalLibraryDirectories>..\..\..\..\..\..\external\3rd\library\directx9\lib;..\..\..\..\..\..\external\3rd\library\libjpeg\lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
  ...
  <BaseAddress>0x62A00000</BaseAddress>
  <ImportLibrary>...\Direct3d9\Debug/gl05_d.lib</ImportLibrary>
  <TargetMachine>MachineX86</TargetMachine>
  <AdditionalOptions>/SAFESEH:NO %(AdditionalOptions)</AdditionalOptions>
</Link>
```
**x64 mirror — the exact edits:**
- **DROP `d3dx9.lib`** from `AdditionalDependencies` (D-04: no x64 `d3dx9` lib exists; this is
  the x64-link precondition). `d3d9.lib` + `d3dcompiler.lib` come from the **Windows SDK** on
  x64 — remove the `directx9\lib` (legacy x86 DXSDK) entry from `AdditionalLibraryDirectories`
  and let the toolset SDK supply them. Keep `libjpeg` → but see SwgClient note (jpeg is a
  LIFT/import-lib dep on x64).
- **`<TargetMachine>MachineX64</TargetMachine>`** (was `MachineX86`).
- **`<OutputFile>…\Direct3d9\x64\Debug\gl05_d.dll`** — the x64 OutDir.
- **Drop `<BaseAddress>0x62A00000`** — fixed base addresses are an x86 nicety; on x64 let the
  loader ASLR it (harmless to keep, cleaner to drop).
- `/SAFESEH:NO` is **x86-only and ignored on x64** — leave it or drop it; do not block on it.

**PostBuildEvent** (Direct3d9 :132-137) stages to `stage\`:
```
copy /Y "$(OutDir)gl05_d.dll" "..\..\..\..\..\..\..\stage\"
copy /Y "..\..\..\..\..\..\external\3rd\library\directx9\lib\d3dcompiler_47.dll" "..\..\..\..\..\..\..\stage\"
```
**x64 mirror:** retarget to `stage-x64\` (Pitfall 2 — x86/x64 DLLs cannot coexist in one
`stage/`), and source the **x64** `d3dcompiler_47.dll` (the system `C:\Windows\System32` copy
is x64, per RESEARCH finding #3) instead of the legacy DXSDK x86 copy.

---

### SwgClient.vcxproj — add x64 (config, Application)

**Analog:** `src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj` — `Application`
`ConfigurationType` (:35-40); the **massive `<Link>` block** at :102-114 is where every
third-party lib swap lands.

**The Win32 `AdditionalDependencies`** (:103) is the authoritative third-party inventory. The
x64 edits, cross-referenced to RESEARCH's Per-Lib table:
- **DROP entirely (D-04):** `d3dx.lib;d3dx9.lib;d3dx9dt.lib` — no x64 lib, must be gone before
  the x64 link is clean.
- **DROP the x86 static `binkw32.lib`** — Bink is dynamically loaded (no static import needed;
  see BinkVideo below). `binkw32.lib` in this list is vestigial; removing it is safe and
  required (x86 machine-mismatch on x64).
- **Miles `Mss32.lib`/`mss32.lib`** — x86 static, will machine-mismatch the x64 link
  (Pitfall 5 / Open Q1). Either drop + `#if _M_X64`-stub the `AIL_*` call sites, or keep
  clientAudio out of the x64 link. **This gates whether SwgClient links x64 at all** — a
  planning decision, not a mechanical swap.
- **LIFT-and-import-lib deps:** `libxml2-win32-debug.lib;libxml2-win32-release.lib`,
  `libpcre.a`, `libjpeg.lib` → swap to x64 import libs generated from the Restoration x64 DLLs
  (`dumpbin /exports … → .def → lib /machine:x64`). `libpcre.a` (MinGW archive) MUST become a
  real x64 `.lib`.
- **BUILD-FROM-SOURCE deps** (`zlib.lib`, `dpvs.lib`, `ui.lib`, `udplibrary.lib`,
  `gl05_d.lib`/`gl06_d.lib`/`gl07_d.lib`, every `shared*`/`client*` engine lib): no name change,
  but their x64 `.lib`s come from the **x64 OutDir** — so every entry in
  `AdditionalLibraryDirectories` (:105) that points at `…\compile\win32\<lib>\Debug` must point
  at `…\compile\win32\<lib>\x64\Debug` for the x64 config (matching the StaticLibrary OutDir
  pattern above). The DXSDK `directx9\lib` dir comes out (SDK supplies x64 `d3d9`/`d3dcompiler`).
- **Editor/Qt/Maya/Alienbrain/Perforce libs** in the list (qt*, OpenMaya*, NxN_alienbrain*,
  Mfc*, etc.) are editor-only and not on the boot-path link closure — they are tolerated under
  `/FORCE` today; do not spend effort x64-ing them.

**Link gotchas:**
- `<OutputFile>$(OutDir)$(ProjectName)_d.exe` (:104) — point `$(OutDir)` at the x64 dir.
- `/SAFESEH:NO /VERBOSE` + `<ForceFileOutput>Enabled` (:110-113) = the **`/FORCE`** that masks
  unresolved externals → **the first x64 link WILL produce an exe with unresolved symbols and
  exit 0** (Pitfall 3). Grep the x64 link log for `unresolved external symbol` (must be 0) —
  exit 0 is not proof.
- PostBuildEvent (:123-138) stages exe+pdb to `stage\` and the Miles redist → retarget to
  `stage-x64\`.

---

### dpvs — x64 config + the CPU-detection source edit (config + source, 3rd-party)

**Analog (the edit site):** `src/external/3rd/library/dpvs/implementation/include/dpvsPrivateDefs.hpp:129-131`:
```cpp
#if defined (DPVS_OS_WIN32)
#	define DPVS_CPU_X86										// x86 series CPU
#	define DPVS_LITTLE_ENDIAN								// x86 processors are little-endian
```
This is **WIN32-unconditional — it never checks `_M_X64`/`_WIN64`** → an x64 build still
defines `DPVS_CPU_X86` → `DPVS_X86_ASSEMBLY` (:317, :338) → the `__asm` blocks (gated on
`DPVS_X86_ASSEMBLY` at :600, :798, :820) become x64-illegal `C4235`.

**The mandatory edit (Pitfall 4, D-02):** guard so x64 takes the portable-C path:
```cpp
#if defined (DPVS_OS_WIN32)
#	if !defined(_M_X64)
#		define DPVS_CPU_X86								// x86 inline asm only on 32-bit
#	endif
#	define DPVS_LITTLE_ENDIAN							// x64 is also little-endian
```
Leaving `DPVS_CPU_X86` undefined on x64 makes `DPVS_X86_ASSEMBLY` undefined, so the existing
C `#else` fallbacks (e.g. the `getHighestSetBit`/`getNextPowerOfTwo` LUT path) compile
automatically — D-02's claim holds **once this guard is added**. The nearby `#elif defined
(DPVS_OS_MAC)` / `__HPUX_SOURCE` / `DPVS_PS2` branches (:132-159) are the in-file precedent for
platform-conditional defines — mirror that style.

**x64 config on `dpvs.vcxproj`** — same StaticLibrary ProjectConfiguration/PropertyGroup
pattern as sharedMath. **Plus a self-contained C4311/C4312 pointer-truncation pass** (dpvs was
out of Phase 31 scope, so it carries unaudited pointer-in-int defects the x64 compile will
surface; apply the same `/we4311 /we4312`). **Fallback:** Restoration's `dpvs.dll` (424 KB) is
the D-01 escape hatch if the from-source x64 build stalls.

---

### Direct3d9.cpp — D3DXMATRIX → DirectXMath (source, transform/matrix)

**Site being replaced:** `Direct3d9.cpp:562-565` (the four `D3DXMATRIX` file-static cached
matrices) + `:3291` (`D3DXMatrixMultiply`) + `:4031-4034` (multiply/transpose). RESEARCH has the
full census; these are file-static members (NOT in a shared header) so changing them does **not**
trigger the shared-header ABI cascade.

**Analog (the DirectXMath transpose precedent):** `Direct3d11_StateCache.cpp:385-395` — the
in-repo, battle-tested model for the col-vec-engine / row-vec-bytecode transpose convention
(memory `d3d11_cbuffer_transpose_quirk`):
```cpp
// stored matrix must be the transpose of M.
DirectX::XMMATRIX wvp_t = DirectX::XMMatrixTranspose(wvp);
DirectX::XMMATRIX W_t   = DirectX::XMMatrixTranspose(W);
DirectX::XMStoreFloat4x4(&s_slot0Shadow.objectWorldCameraProjectionMatrix, wvp_t);
DirectX::XMStoreFloat4x4(&s_slot0Shadow.objectWorldMatrix,                 W_t);
```
And the `Direct3d11_ConstantBuffer.h` storage convention: cached matrices are
`DirectX::XMFLOAT4X4`, live math is `XMMATRIX`, conversion via `XMStore/XMLoadFloat4x4`.

**Gotcha (Pitfall 1 — the #1 render risk):** `D3DXMatrixMultiply(out,A,B)` and
`XMMatrixMultiply(A,B)` are both row-major and *mostly* drop-in, but the **transpose at
`Direct3d9.cpp:4031` (`D3DXMatrixMultiplyTranspose`) must be preserved exactly** as
`XMMatrixTranspose(XMMatrixMultiply(...))`. Convert **one matrix op at a time** and A/B the
rendered frame against the 32-bit gl05 Tatooine reference after each. The Phase-31 SseMath
`_DEBUG` numeric-equivalence oracle is the validation model. Warning signs:
mirrored/inside-out geometry, flipped skybox, world transform off.

> The mesh (`D3DXCreateMeshFVF`/`OptimizeInplace`), surface (`D3DXLoadSurfaceFromSurface` ×8),
> and save-to-file (`D3DXSaveSurfaceToFile`/`D3DXSaveTextureToFile`) helpers are **own-impl**,
> not DirectXMath — RESEARCH §"Non-compile D3DX" has each call site + replacement strategy
> (StretchRect/lock-blit, skip the vertex-cache optimize, reuse sharedImage TGA writers).

---

### BinkVideo.cpp:55 — DLL-name `#if _M_X64` swap (source, dynamic-load)

**Analog (the site itself):** `src/engine/client/library/clientGraphics/src/Bink/BinkVideo.cpp:55`
(note: this is the **clientGraphics** copy, the live one — RESEARCH cited line 55 generically):
```cpp
static const char     *s_dllName = "binkw32.dll";
...
if (!bindBink(s_dllName))                                      // :100
    WARNING(true, ("Error binding to Bink video DLL (%s)!\n", s_dllName));   // :102
```
Bink is **dynamically loaded** (`bindBink` → `LoadLibrary`/`GetProcAddress`) — there is **no
static import lib in the link**, so the entire x64 Bink port is this one string:
```cpp
#if defined(_M_X64)
static const char     *s_dllName = "binkw64.dll";
#else
static const char     *s_dllName = "binkw32.dll";
#endif
```
+ stage Restoration's `binkw64.dll` into `stage-x64\`. This is the analog for **any** DLL-name
`#if _M_X64` swap in the phase. **No `.def`/import-lib generation for Bink** (RESEARCH finding
#1 — the D-01 "generate import libs for bink" step is unnecessary). `.def`/import-lib generation
is needed only for the LIFT deps (libxml2/pcre/jpeg) — there is no in-tree `.def` precedent;
generate from the Restoration x64 DLL exports per Pitfall 6.

---

## Shared Patterns

### x64 OutDir isolation (protects the 32-bit non-regression gate)
**Source:** the Win32 OutDir convention `…\compile\win32\<proj>\$(Configuration)\`
(sharedMath :57-60, SwgClient :57-59).
**Apply to:** every boot-path `.vcxproj` x64 config AND the SwgClient
`AdditionalLibraryDirectories`.
Insert `\x64\` (or `$(Platform)`) into the x64 OutDir/IntDir so x64 `.obj`/`.lib`/`.dll`/`.exe`
never clobber the shipped Win32 artifacts. Pair with a `stage-x64\` postbuild target (Pitfall 2).

### /FORCE link-grep (the x64-link acceptance signal)
**Source:** `SwgClient.vcxproj:110-113` (`/SAFESEH:NO /VERBOSE` + `<ForceFileOutput>Enabled`) +
`Direct3d9.vcxproj:126` (`/SAFESEH:NO`); AGENTS.md §Build.
**Apply to:** every x64 link (SwgClient + gl05/06/07).
The first x64 link emits an exe at exit 0 *with* unresolved externals. Grep the link log for
`unresolved external symbol` == 0 — exit 0 is never proof (Pitfall 3).

### N1 warnings-as-errors guardrail (`/we4311 /we4312`)
**Source:** `x64-compile.props:103` (`TreatSpecificWarningsAsErrors>4311;4312;4244`), trimmed to
`4311;4312` per the committed-tree policy (research correction).
**Apply to:** the new `x64-platform.props` ClCompile block → every boot-path x64 compile. Do
NOT promote C4267 (N2). Do NOT touch the D-07 `Archive.h` C4244 exclusion.

### `_USE_32BIT_TIME_T` omission on x64
**Source:** `x64-compile.props:24-41` header (UCRT `corecrt.h` hard-errors under `_WIN64`).
**Apply to:** every x64 ClCompile block — strip the `_USE_32BIT_TIME_T=1` token the Win32 blocks
carry (e.g. `sharedMath.vcxproj:74`). Surfaces the A2-TIME-T serialization residual (handle as
int32 at wire sites, not via the flag).

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| x64 import `.lib` for libxml2/pcre/jpeg | build artifact | n/a | No in-tree `.def`/import-lib precedent exists; generate from Restoration x64 DLL exports (`dumpbin /exports → .def → lib /machine:x64`, Pitfall 6). Procurement step, not a code analog. |
| tinyxml x64 build path | config (3rd-party) | build-config | Only dep with **no** Restoration DLL fallback; builds via legacy `.dsp` not a `.vcxproj`. Author a fresh x64 `.vcxproj` (pattern = sharedMath StaticLibrary) over its 3 `.cpp`, or fold into sharedXml. No existing tinyxml `.vcxproj` analog in-tree. |

## Metadata

**Analog search scope:** `src/build/win32/` (sln + scratch props), `src/engine/shared/library/sharedMath/build/win32/`, `src/engine/client/application/Direct3d9/build/win32/`, `src/game/client/application/SwgClient/build/win32/`, `src/external/3rd/library/dpvs/implementation/include/`, `src/engine/client/application/Direct3d11/src/win32/` (DirectXMath precedent), `src/engine/client/library/clientGraphics/src/Bink/`.
**Files scanned:** 9 (3 vcxproj, 1 sln, 1 props, dpvsPrivateDefs.hpp, Direct3d11_StateCache.cpp, BinkVideo.cpp, Direct3d9.cpp census)
**Pattern extraction date:** 2026-06-17

## PATTERN MAPPING COMPLETE
