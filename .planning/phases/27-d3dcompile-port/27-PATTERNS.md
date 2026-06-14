# Phase 27: D3DCompile Port - Pattern Map

**Mapped:** 2026-06-14
**Files analyzed:** 4 edit points (1 source + 3 vcxproj + 1 stage step)
**Analogs found:** 4 / 4 (all in-repo, proven in gl11)

> **Nature of this phase:** This is an IN-PLACE compiler swap inside a SINGLE source file
> (`Direct3d9_VertexShaderData.cpp`), not new-file creation. "Pattern assignments" below pair each
> edit point with its closest existing analog and the concrete code to copy from. The richest analog
> is the gl11 (D3D11) plugin, which already runs `D3DCompile` + an `ID3DInclude` handler in-repo.

---

## File Classification

| Edit point | Role | Data Flow | Closest Analog | Match Quality |
|------------|------|-----------|----------------|---------------|
| `Direct3d9_VertexShaderData.cpp` — HLSL compile branch (`m_hlsl==true`, helper :87-114 + call :477) | renderer plugin (shader compile) | transform (HLSL text → bytecode) | `Direct3d11_VertexShaderData.cpp:865-967` (`D3DCompile` call) | exact (same call, different profile) |
| `Direct3d9_VertexShaderData.cpp` — `IncludeHandler` (:55-198) | utility (include resolver, COM callback) | file-I/O (TreeFile → buffer) | `Direct3d11_CompileIncludeHandler.{cpp,h}` | exact (gl11 mirrored the D3D9 one) |
| `Direct3d9_VertexShaderData.cpp` — `Defines` typedef + builders (:62, :402-471, :498-564) | utility (macro list) | transform (struct rename) | `Direct3d11_VertexShaderData.cpp:744-758` (`std::vector<D3D_SHADER_MACRO>`) | exact (field-identical struct) |
| `Direct3d9_VertexShaderData.cpp` — asm branch `D3DXAssembleShader` (:567) | renderer plugin (shader assemble) | transform (asm text → bytecode) | `D3DAssemble` (d3dcompiler_47 export) — **no in-repo analog** | none (see No Analog Found) |
| `Direct3d9{,_ffp,_vsps}.vcxproj` — link deps (:115/:175/:232 etc.) | config (build) | n/a | `Direct3d11.vcxproj:118` (`d3dcompiler.lib`) | exact |
| postbuild / stage — `d3dcompiler_47.dll` | config (stage) | file-I/O (copy) | `Direct3d11.vcxproj:135-139` PostBuildEvent (DLL copy pattern only) | partial (gl11 does NOT stage the redist DLL — gap) |

---

## Pattern Assignments

### 1. HLSL compile branch — `D3DXCompileShader` → `D3DCompile`

**Edit site (current):** `Direct3d9_VertexShaderData.cpp:477` (call) + helper `compileVertexShaderFpGuarded` :93-114.

**Current code (what is being replaced):**
```cpp
// Direct3d9_VertexShaderData.cpp:477
HRESULT result = compileVertexShaderFpGuarded(
    m_compileText, m_compileTextLength, &(ms_defines.front()), &includeHandler,
    "main", target /* "vs_2_0" or "vs_1_1" */, 0, &compiledShader, &error);
// ...
// :573 — consume bytecode (UNCHANGED after port; ID3DBlob::GetBufferPointer is field-compatible)
Direct3d9::getDevice()->CreateVertexShader(
    reinterpret_cast<DWORD const *>(compiledShader->GetBufferPointer()), &vertexShader);
```

**Analog (the proven gl11 shape to mirror):** `Direct3d11_VertexShaderData.cpp:940-958`
```cpp
HRESULT hr = D3DCompile(
    rewrittenSource.data(),                       // src text  -> use m_compileText
    rewrittenSource.size(),                       // src len   -> use m_compileTextLength
    virtName,                                     // pSourceName for error msgs (asset filename)
    defines.data(),                               // D3D_SHADER_MACRO[] (NULL-terminated)
    Direct3d11_CompileIncludeHandler::getInstance(), // ID3DInclude*  -> gl05's adapted IncludeHandler
    "main",                                       // entry point (UNCHANGED)
    kVertexShaderProfile,                         // "vs_4_0"  -> gl05 uses "vs_2_0"/"vs_1_1" instead
    flags,
    0,
    blob.GetAddressOf(),                          // ID3DBlob** out
    errors.GetAddressOf());
```

**Profile selection — KEEP gl05's existing logic UNCHANGED** (`Direct3d9_VertexShaderData.cpp:391-395`):
```cpp
char const * target = NULL;
if (Direct3d9::getShaderCapability() >= ShaderCapability(2,0))
    target = "vs_2_0";
else
    target = "vs_1_1";
```
`D3DCompile` accepts both `vs_1_1` and `vs_2_0` (and `vs_3_0`). Do NOT substitute gl11's `vs_4_0`.

**Compile flags — mirror gl11's structure but DROP the SM4-coupled flags** (`Direct3d11_VertexShaderData.cpp:865-920`):
```cpp
UINT flags = 0;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
flags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;   // accept SOE's D3D9-era HLSL leniently
// DO NOT add D3DCOMPILE_PACK_MATRIX_ROW_MAJOR (gl11 :920) — that flag pairs with D3D11's
//   transposed-cbuffer upload convention. D3D9 SetVertexShaderConstantF semantics match the old
//   D3DX path; keep D3DX's default matrix packing to preserve the existing cN register layout.
//   [Pitfall 1 / Assumptions A1+A2 in RESEARCH — verify by parity smoke.]
// DO NOT add D3DCOMPILE_ENABLE_STRICTNESS — mutually exclusive with BACKWARDS_COMPATIBILITY
//   (gl11 X3116 lesson, :848-859).
```

**Includes to change** (`Direct3d9_VertexShaderData.cpp:29-30`): add `#include <d3dcompiler.h>`. Keep
`<d3dx9.h>` only while the asm branch stays on `D3DXAssembleShader` (default residual). gl11 pattern:
`Direct3d11_VertexShaderData.cpp:93` (`#include <d3dcompiler.h>`), `:94` (`#include <wrl/client.h>`
for `ComPtr` — optional; gl05 can keep raw `ID3DBlob*` + manual `Release()` like its current
`ID3DXBuffer*` flow to minimize churn).

**Profile constant pattern (optional polish):** gl11 centralizes the profile string —
`Direct3d11_VertexShaderData.cpp:112` `char const * const kVertexShaderProfile = "vs_4_0";`. gl05
already computes `target` per-call from `getShaderCapability()`, so no constant is needed; keep the
existing dynamic selection.

**SEH guard (D-03/D-05):** `compileVertexShaderFpGuarded` (:93-114) wraps the D3DX call in
`__try/__except` + `_clearfp`/`_fpreset` for the `0xC0000090` post-compile FP fault. gl11 runs
`D3DCompile` with NO such guard (see `Direct3d11_VertexShaderData.cpp:67` comment: "FPU bug doesn't
apply to D3DCompile"). Per D-03, RETAIN the guard wrapper around the new `D3DCompile` call through
validation, then drop it only after the Tatooine bump/dot3 spot (`db83b0f5c`) renders clean (D-05).
The post-call belt-and-suspenders `_clearfp()` at :486 can stay harmlessly.

---

### 2. Include handler — `ID3DXInclude` → `ID3DInclude`

**Edit site (current):** `Direct3d9_VertexShaderData.cpp:55-60` (class decl), `:144-187` (`Open`),
`:191-198` (`Close`).

**Current code (class base + Open signature):**
```cpp
// Direct3d9_VertexShaderData.cpp:55
class IncludeHandler : public ID3DXInclude
{
public:
    virtual HRESULT STDMETHODCALLTYPE Open(D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
    virtual HRESULT STDMETHODCALLTYPE Close(LPCVOID pData);
};
```

**Analog (gl11's, which was deliberately mirrored from this exact D3D9 handler):**
`Direct3d11_CompileIncludeHandler.h:67-87` + `.cpp:136-238`.
```cpp
// Direct3d11_CompileIncludeHandler.h:67
class Direct3d11_CompileIncludeHandler : public ID3DInclude
{
    virtual HRESULT __stdcall Open(
        D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData,
        LPCVOID *ppData, UINT *pBytes) override;
    virtual HRESULT __stdcall Close(LPCVOID pData) override;
};
```

**What changes — base interface + enum type ONLY. The Open/Close BODY logic is identical:**
- `ID3DXInclude` → `ID3DInclude`
- `D3DXINCLUDE_TYPE` → `D3D_INCLUDE_TYPE`
- `STDMETHODCALLTYPE` is fine (== `__stdcall`); gl11 spells it `__stdcall` — either works.

**Body to KEEP (gl05's existing logic is correct and richer than gl11's):** the `../../` strip
(:147-148), the `TreeFile::open(pFileName, AbstractFile::PriorityData, true)` route (:156, :174),
`readEntireFileAndClose()` (:182), and — importantly — gl05's **engine-owns-window include CACHE**
(`ms_includeCache`, :150-171). gl11 DROPPED the cache for simplicity
(`Direct3d11_CompileIncludeHandler.cpp:27-28`); gl05 should RETAIN it. The defensive `../../` strip
appears in both: gl11 at `.cpp:152-154`, gl05 at `:147-148`.

**Reuse note (do NOT cross-link):** `Direct3d11_CompileIncludeHandler` lives in the gl11 DLL — the
gl05/gl07 plugin cannot link it. Adapt gl05's existing in-file `IncludeHandler` in place (RESEARCH
Pattern 2 option (a) — minimal change, keeps the cache). Do not factor a shared header.

---

### 3. Macro list — `D3DXMACRO` → `D3D_SHADER_MACRO`

**Edit site (current):** `Direct3d9_VertexShaderData.cpp:62` (typedef), plus every `D3DXMACRO macro;`
in the HLSL define builder (:414, :437, :469) and the asm define builder (:521, :542, :557, :562).

**Current:**
```cpp
// :62
typedef std::vector<D3DXMACRO>  Defines;
// :469 (NULL terminator)
D3DXMACRO empty = { NULL, NULL };
```

**Analog:** `Direct3d11_VertexShaderData.cpp:744`
```cpp
std::vector<D3D_SHADER_MACRO> defines;
defines.push_back({ "D3D11_PROFILE", kVertexShaderProfile });   // (gl11-specific; gl05 keeps its own defines)
```

**What changes — type name ONLY. The structs are field-identical** (`{const char* Name; const char*
Definition;}` — RESEARCH "Key insight" :317). All the scratch-buffer define-building machinery
(:402-471 HLSL, :498-564 asm) ports with a pure rename, NO logic change. The terminating
`{ NULL, NULL }` sentinel and `&(ms_defines.front())` call convention are unchanged.

---

### 4. Asm branch — `D3DXAssembleShader` (default: KEEP)

**Edit site (current):** `Direct3d9_VertexShaderData.cpp:567`
```cpp
HRESULT result = D3DXAssembleShader(m_compileText, m_compileTextLength, &(ms_defines.front()),
    &includeHandler, 0, &compiledShader, NULL);
```

**Analog:** NONE in-repo. gl11 does NOT assemble — it FATALs or falls back on `//asm` VS
(`Direct3d11_VertexShaderData.cpp:160` "D3DCompile cannot consume D3D9 vertex assembly"). See
"No Analog Found" below. Per D-02-R the **default is to KEEP this call on `D3DXAssembleShader` + SEH
guard** (96 `//asm` shaders depend on it); `D3DAssemble` is a separately-validated stretch goal with
NO existing pattern to copy.

---

### 5. Build linkage — add `d3dcompiler.lib`

**Edit site (current):** `Direct3d9.vcxproj:115/175/232`, `Direct3d9_vsps.vcxproj:115/172/232`,
`Direct3d9_ffp.vcxproj` (verify — FFP has no HLSL compile, may not need it; but it shares the file
list — check whether it compiles `Direct3d9_VertexShaderData.cpp` under `VSPS`).

**Current (gl07_vsps, one of three configs):**
```xml
<AdditionalDependencies>libjpeg.lib;odbc32.lib;odbccp32.lib;winmm.lib;delayimp.lib;dxguid.lib;d3d9.lib;d3dx9.lib;ddraw.lib;dxerr9.lib;legacy_stdio_definitions.lib;version.lib;%(AdditionalDependencies)</AdditionalDependencies>
```

**Analog:** `Direct3d11.vcxproj:118` (and :178, :235 for the other configs)
```xml
<AdditionalDependencies>libjpeg.lib;odbc32.lib;odbccp32.lib;winmm.lib;delayimp.lib;dxguid.lib;d3d11.lib;d3dcompiler.lib;dxgi.lib;legacy_stdio_definitions.lib;%(AdditionalDependencies)</AdditionalDependencies>
```

**Change:** add `d3dcompiler.lib` to the dependency string in ALL THREE configurations (Debug `_d`,
Release `_r`, Optimized `_o`) of EACH of the gl05/gl07 vcxproj that compiles the file.

> **CRITICAL — `d3dx9.lib` CANNOT be dropped.** RESEARCH suggested it *might* be removable; VERIFIED
> it is NOT. D3DX is used in 4 OTHER gl05/gl07 source files beyond `Direct3d9_VertexShaderData.cpp`:
> - `Direct3d9.cpp` — `D3DXMatrixMultiply` (:3291,3357,3359,4034), `D3DXMatrixMultiplyTranspose`
>   (:4031), `D3DXMatrixTranspose` (:4032), `D3DXMATRIX` (:562-565,4028), `D3DXSaveSurfaceToFile`
>   (:2890), `D3DXMesh` (:4619)
> - `Direct3d9_TextureData.cpp` — `D3DXLoadSurfaceFromSurface` (:549,603,681)
> - `Direct3d9_RenderTarget.cpp` — `D3DXLoadSurfaceFromSurface` (:317)
> - `Direct3d9_StaticShaderData.cpp` — D3DX references present
>
> The port retires D3DX from the **runtime compile surface** only (HARD-05's scope). Keep
> `d3dx9.lib` linked. Even if the asm path also moves to `D3DAssemble`, these matrix/surface uses
> keep `d3dx9.lib` mandatory. Do NOT attempt to drop it; the /FORCE false-pass guard (grep link for
> 0 `unresolved external symbol`) would catch a wrongful removal — but better to not try.

---

### 6. Stage `d3dcompiler_47.dll`

**Edit site (current):** PostBuildEvent in each gl05/gl07 vcxproj, e.g.
`Direct3d9_vsps.vcxproj:132-136`:
```xml
<PostBuildEvent>
  <Command>copy /Y "$(OutDir)gl07_d.dll" "..\..\..\..\..\..\..\stage\"
copy /Y "$(OutDir)gl07_d.pdb" "..\..\..\..\..\..\..\stage\"</Command>
</PostBuildEvent>
```

**Analog (DLL-copy mechanism only):** `Direct3d11.vcxproj:135-139` — same `copy /Y "$(OutDir)...dll"
"...\stage\"` shape.

> **GAP — gl11 does NOT stage `d3dcompiler_47.dll` either.** The gl11 postbuild copies only its own
> `gl11_*.dll`/`.pdb`; it relies on the redist DLL already being present (it is currently absent from
> `stage/` — VERIFIED via Glob, 0 hits repo-wide). So there is NO existing "stage a redist DLL"
> pattern to copy. The planner must ADD a new stage step: source `d3dcompiler_47.dll` from the Win10
> SDK redist (or the known-good Restoration copy at `D:\SWG Restoration\`) into a tracked location,
> and copy it to `stage/` via postbuild (or document it like the `stage/miles` precedent, PORT-01).
> Mirror the `copy /Y` shape above. This is a NEW step, classified "partial analog."

---

## Shared Patterns

### Bytecode consumption (no change required)
**Source:** `Direct3d9_VertexShaderData.cpp:573` (D3D9) ↔ `Direct3d11_VertexShaderData.cpp:509` (D3D11)
**Apply to:** the HLSL branch after the swap.
`ID3DBlob::GetBufferPointer()` is binary-compatible with the old `ID3DXBuffer::GetBufferPointer()`;
`CreateVertexShader(reinterpret_cast<DWORD const*>(blob->GetBufferPointer()), &vs)` is UNCHANGED. Same
for `Release()` on the blob.

### TreeFile include resolution (keep as-is)
**Source:** `Direct3d9_VertexShaderData.cpp:156,174` and mirrored at
`Direct3d11_CompileIncludeHandler.cpp:160`.
**Apply to:** the adapted `ID3DInclude::Open`. Both use
`TreeFile::open(name, AbstractFile::PriorityData, /*canFail=*/true)` → `readEntireFileAndClose()`.
Identical engine call, different COM base. Honors TRE + override searchPath transparently.

### Build-config triplication (do not miss a config)
**Source:** every gl plugin vcxproj has THREE configs — `_d` (Debug), `_r` (Release), `_o`
(Optimized) — each with its own `AdditionalDependencies` and PostBuildEvent (see
`Direct3d11.vcxproj:118/178/235` and `Direct3d9_vsps.vcxproj:115/172/232`).
**Apply to:** the `d3dcompiler.lib` add and the DLL stage step — do all three configs of each touched
vcxproj, and rebuild BOTH gl05 and gl07 (and Debug + Release per the boot invariant). The Optimized
config has a known pre-existing SAFESEH/LNK1281 quirk (see memory
`project_optimized_config_safeseh_pre_existing`) — validate removals via Debug+Release link-grep, not
Optimized.

---

## No Analog Found

| Edit point | Role | Data Flow | Reason |
|------------|------|-----------|--------|
| asm branch `D3DAssemble` (IF the stretch goal is taken) | shader assemble | transform | gl11 has NO assembler — it cannot consume D3D9 vertex asm at all (`Direct3d11_VertexShaderData.cpp:160`). No in-repo `D3DAssemble` call exists. The signature differs from `D3DXAssembleShader` (`ID3DBlob` out, no trailing `LPD3DXBUFFER` errors arg). If pursued, derive from MS docs, NOT a codebase analog. DEFAULT per D-02-R: keep `D3DXAssembleShader` + SEH (proven), so no analog is needed for the baseline path. |
| stage `d3dcompiler_47.dll` redist copy | config (stage) | file-I/O | No "stage a redist DLL" pattern exists in any plugin vcxproj (gl11 relies on the DLL being present without staging it). New step; mirror the `copy /Y` shape + `stage/miles` PORT-01 precedent. |

---

## Anti-Patterns (DO NOT copy from gl11)

> The planner MUST flag these so the gl05/gl07 port does not inherit gl11's SM4-bridging machinery.

1. **`Direct3d11_HlslRewrite` rules — DO NOT PORT.** gl11 compiles to `vs_4_0` and needs textual
   rewrites for X3000 (`point`/`line`/`triangle` keyword rename), X3202 (struct-member `: c<N>` /
   `: register(c<N>)` strip), X4016 (overlapping registers), cbuffer wrapping — 13+ iterations of
   SM2→SM4 bridging (`Direct3d11_VertexShaderData.cpp:28-64`, `Direct3d11_CompileIncludeHandler.cpp:40-107`).
   **gl05/gl07 recompile to the SAME `vs_2_0`/`vs_3_0` profile the assets target — the legacy syntax
   is accepted natively.** Applying these rewrites would CORRUPT a same-profile recompile (e.g.
   stripping `: register(cN)` defeats the explicit register contract the C++ `SetVertexShaderConstantF`
   depends on). Do NOT call `applyToMainSource` / `applyToIncludeBuffer`; do NOT inject a
   `D3D11_REWRITE_VERSION` macro. Feed gl05's `m_compileText` to `D3DCompile` UNMODIFIED.

2. **`D3DCOMPILE_PACK_MATRIX_ROW_MAJOR` — DO NOT ADD** (gl11 `:920`). It pairs with D3D11's
   transposed-cbuffer upload (`XMMatrixTranspose`) convention. D3D9 `SetVertexShaderConstantF` matches
   the old D3DX packing; adding the flag would transpose transforms and break geometry/lighting.

3. **`vs_4_0` profile — DO NOT USE** (gl11 `:112`). Keep gl05's dynamic `vs_2_0`/`vs_1_1` selection
   (`:391-395`).

4. **Dropping the asm branch silently** → null VS → skipped draws (sabers/decals/detail vanish). Keep
   a working assemble path (D3DX residual by default).

5. **Dropping `d3dx9.lib`** → unresolved `D3DXLoadSurfaceFromSurface`/`D3DXMatrixMultiply`/etc. in the
   4 other D3D9 files (masked by /FORCE → false-pass binary). Keep it linked.

---

## Metadata

**Analog search scope:**
- `src/engine/client/application/Direct3d9/src/win32/` (port target + D3DX-usage census)
- `src/engine/client/application/Direct3d11/src/win32/` (proven `D3DCompile` + `ID3DInclude` analog)
- `src/engine/client/application/Direct3d9/build/win32/*.vcxproj` (link + postbuild)
- `src/engine/client/application/Direct3d11/build/win32/Direct3d11.vcxproj` (link + postbuild analog)
- repo-wide Glob for `d3dcompiler_47.dll` (0 hits — confirms stage gap)

**Files scanned:** 9 (2 source target/analog, 2 include-handler, 4 vcxproj, 1 D3DX census across 5 files)
**Pattern extraction date:** 2026-06-14
