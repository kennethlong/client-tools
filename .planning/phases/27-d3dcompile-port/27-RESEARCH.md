# Phase 27: D3DCompile Port - Research

**Researched:** 2026-06-13
**Domain:** D3D9 runtime shader compilation (D3DX → D3DCompile migration); SWG TRE shader assets
**Confidence:** HIGH (port mechanics, asm census, Restoration format all VERIFIED via tool)

## Summary

Phase 27 replaces the modern-toolchain-faulting `D3DXCompileShader` (and the `D3DXAssembleShader`
sibling) in the live D3D9 plugin (gl05/gl07) with `D3DCompile` from `d3dcompiler_47.dll`. The compile
surface is a **single file**: `Direct3d9_VertexShaderData.cpp`. The D3D9 *pixel* shader path does NOT
runtime-compile — it consumes pre-compiled `PEXE`/`m_exe` bytecode via `CreatePixelShader`
(`Direct3d9_PixelShaderProgramData.cpp:34`), so this port touches vertex shaders only. [VERIFIED: grep
+ source read]

**The locked D-01/D-02 premise is FALSIFIED — read this first.** The plan assumed SWG Restoration ships
its modernized shaders as HLSL *source* inside its TREs, ready to feed to our own `D3DCompile` call.
That is not true: Restoration's `.vsh`/`.psh` files are **binary (encrypted or precompiled), not HLSL
text**. I extracted them directly — even the uncompressed entries have a ~42% printable ratio and a
binary header (`AD F0 C2 26`), and 0 of 275 stored shaders contain `//hlsl` or `//asm` markers. The
531 compressed ones use a non-zlib compressor our tooling can't even decompress. There is nothing to
"lift and shift." [VERIFIED: TRE extraction of `D:\SWG Restoration\SwgRestoration_*.tre`]

**The fallback is simpler and better than the locked plan.** Our own shaders are already clean ASCII
HLSL/asm in the SWGSource TREs (190 `//hlsl` + 96 `//asm` of 286 unique `.vsh`). `D3DCompile` natively
supports the D3D9-era SM2/SM3 targets (`vs_2_0`, `vs_3_0`) and its output `ID3DBlob` bytecode is
directly accepted by D3D9 `CreateVertexShader`. So the HLSL path is a near-1:1
`D3DXCompileShader → D3DCompile` swap on text we already own. The 96 `//asm` shaders are the only
gap; `d3dcompiler_47.dll` exports `D3DAssemble` (the modern `D3DXAssembleShader` equivalent), so the
asm path can also leave D3DX — or stay on `D3DXAssembleShader`+SEH guard as the safe residual.
[VERIFIED: census + d3dcompiler_47 export table + MS docs]

**Primary recommendation:** Drop the "lift Restoration shaders" approach (premise falsified). Do the
in-place compiler swap on `Direct3d9_VertexShaderData.cpp`: HLSL path → `D3DCompile`
(`vs_2_0`/`vs_3_0`, `ENABLE_BACKWARDS_COMPATIBILITY`, reuse the existing `ID3DXInclude` handler logic
adapted to `ID3DInclude`); asm path → try `D3DAssemble`, otherwise keep `D3DXAssembleShader`+SEH guard.
Stage `d3dcompiler_47.dll` next to the exe. Mirror the gl11 cache only if desired (optional). Get an
explicit user decision on dropping D-01 before planning locks.

## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** The port is **Restoration's TRE shaders + our own `D3DCompile` call**, NOT a 1:1
  `D3DXCompileShader → D3DCompile` swap and NOT lifting Restoration's plugin *source* (we have their
  `gl0X_r.dll` as binaries only). We extract Restoration's modernized (assumed HLSL / non-D3DX)
  shader assets from `D:\SWG Restoration\SwgRestoration_*.tre` and feed them to a `D3DCompile` call we
  write, mirroring the **existing** D3D11-plugin pattern (`Direct3d11_CompileIncludeHandler` +
  `Direct3d11_ShaderCache`).
  > ⚠️ **RESEARCH FLAG: the factual premise behind D-01 is FALSE.** Restoration shaders are binary,
  > not HLSL source — see `## D-02 Verification` below. The IN-PLACE swap that D-01 explicitly
  > rejects ("NOT a 1:1 swap") is in fact the correct, simpler approach because our OWN shaders are
  > already clean HLSL. This requires a user decision in discuss-phase before planning.
- **D-02 (asm path dissolves — VERIFY FIRST):** premise is Restoration shaders are HLSL covering 100%
  so the asm `D3DXAssembleShader` path (`:567`) is retired. First job: extract Restoration's shaders,
  confirm format + coverage. FALLBACK: if any shaders can't be replaced by a Restoration HLSL
  equivalent, keep those on `D3DXAssembleShader` + SEH guard.
  > ⚠️ **RESEARCH RESULT: the fallback condition is triggered.** Restoration provides no usable HLSL.
  > 96 of our `.vsh` are `//asm`. Asm path does NOT dissolve. Options documented below.
- **D-03 (SEH guard):** `D3DCompile` is immune to the modern-toolchain post-compile FP fault Fix-A
  guards, so the SEH wrapper (`compileVertexShaderFpGuarded`) can be relaxed/removed on the HLSL path
  once on `D3DCompile`. Retain it only on any residual D3DX path. Decide during validation (D-05).
- **D-04:** Phase 27 is the D3DCompile port ONLY. CORNERSNAP `_DEBUG` probes stay untouched.
  Re-baseline the door snap on the new build as observation only. Probe removal + door-snap fix
  deferred to a future x64 port.
- **D-05:** Parity = dual-renderer boot smoke (rasterMajor=5 AND =11) to char-select/in-game, plus a
  targeted check on the exact Tatooine bump/dot3 VS spot Fix-A was created for (`db83b0f5c`). Evaluate
  whether the SEH guard can be dropped. Boot invariant: Debug exe→`client_d.cfg`, Release exe→`client.cfg`.
- **D-06:** ShaderBuilder Qt editor's D3DX calls — OUT of scope, leave untouched.

### Claude's Discretion

- Exact `D3DCompile` wiring (d3dcompiler version, include-handler reuse, shader cache integration) is
  for research/planning to mirror from the D3D11 plugin.

### Deferred Ideas (OUT OF SCOPE)

- Asm-path migration as a standalone effort (only if Restoration doesn't cover our asm set).
- CORNERSNAP probe removal + door-snap fix (→ future x64 port).
- x64 port (separate future milestone).
- ShaderBuilder editor `D3DCompile` migration (pre-broken Qt editor, non-shipping).

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| HARD-05 | D3D9 shader compilation uses `D3DCompile` instead of `D3DXCompileShader` (Fix B) — preceded by an asm-shader census; Phase-19 SEH guard retained for any path still on D3DX and superseded where ported | Asm census complete (96 `//asm`, 190 `//hlsl`). `D3DCompile` confirmed to accept `vs_2_0`/`vs_3_0` and produce D3D9-compatible bytecode. Include-handler + compile-call patterns proven in gl11. `D3DAssemble` available for asm path. Single-file compile surface (`Direct3d9_VertexShaderData.cpp`). |

## D-02 Verification (the required FIRST task — completed in research)

### (a) Asm census of OUR shader set — VERIFIED

Census across all SWGSource TREs (`D:/Code/SWGSource Client v3.0/*.tre`), the actual shipped shader
source our engine reads at runtime:

| Metric | Count |
|--------|-------|
| Unique `.vsh` | 286 |
| `//hlsl` `.vsh` | **190** |
| `//asm` `.vsh` | **96** |
| Unique `.psh` | 454 (consumed as pre-compiled `PEXE`, NOT runtime-compiled — out of scope) |
| Read errors | 0 |

[VERIFIED: Python TRE reader + payload header inspection, 2026-06-13]

Sample `//asm` `.vsh` (the path that would be dropped/nulled if D3DX is removed without handling):
`a_detail.vsh`, `a_detail_bump.vsh`, `a_detail_specmap.vsh`, `saberbase.vsh`,
`saber_blade_cap.vsh`, `a_replace0_decal.vsh`, `c_2blend_decal_lowmem.vsh`,
`a_2blend_specmap_dirt_p2.vsh`. These are real, rendered content (detail-mapped surfaces, lightsabers,
decals). Nulling them would skip those draws — exactly the failure mode STATE.md's "D3DCompile asm
gap" blocker warns about.

Our HLSL is clean ASCII (`//hlsl vs_2_0` header, e.g. `c_simple.vsh` first bytes
`2f 2f 68 6c 73 6c` = `//hlsl`). [VERIFIED: xxd of stage loose shaders]

### (b) Restoration coverage + format — VERIFIED FALSE

Restoration TREs (`D:\SWG Restoration\SwgRestoration_*.tre`, 46 archives, 215,398 entries) DO contain
shader assets: 313 `.vsh`, 493 `.psh`, 268 `.inc`, 278 `.eft`, 24,267 `.sht`. But the `.vsh`/`.psh`
files are **NOT HLSL source**:

- Stored (uncompressed, compressor=0) `.vsh` entries: binary header `AD F0 C2 26`, ~42% printable
  ratio. `vertex_program/2d.vsh` is 550 bytes of high-entropy data, not text.
- **0 of 275** stored `.vsh`/`.psh` contain `//hlsl` or `//asm` markers.
- 531 of 806 `.vsh`/`.psh` use compressor=2 — a non-zlib scheme our Python tooling cannot decompress
  ("Use TreeFileExtractor.exe for Restoration archives"). Restoration encrypts/obfuscates its asset
  payloads.

**Conclusion:** There is no HLSL source to lift from Restoration. The D-01 "lift and shift" premise is
falsified. Restoration's `gl05_r.dll`/`gl07_r.dll` are D3D9-era plugins (import `d3d9.dll`, expose only
SM1–SM3 profile strings `vs_1_0..vs_3_0`, `ps_1_0..ps_3_0`), and Restoration ships `d3dcompiler_47.dll`
(4.9 MB, exports `D3DCompile`, `D3DAssemble`, `D3DReflect`) in its install root. [VERIFIED: DLL
import-string + export-symbol scan, file listing]

> ⚠️ **CORRECTION 2026-06-14 (dumpbin import-table audit — supersedes the inference that followed).**
> The original draft inferred "strong evidence Restoration did the in-place D3DCompile/D3DAssemble swap."
> That is FALSE. `dumpbin //dependents` shows: the **32-bit** plugins (root, the production client —
> `SwgClient_r.exe` is PE32/i386) import ONLY `d3d9.dll`+`DDRAW.dll` — no `d3dx9`, no `d3dcompiler`, no
> compiler-DLL string → they ship **pre-compiled bytecode** (no runtime compile). The **x64** plugins
> (`x64/gl0*_r.dll`) import **`d3dx9_24.dll`** (the OLD ~2005 DX-SDK redist) and contain
> `D3DXCompileShader`/`D3DXAssembleShader`/`//hlsl`/`//asm` strings → they runtime-compile via the
> prebuilt **old D3DX redist**, NOT D3DCompile. The root `d3dcompiler_47.dll` is imported by NOTHING in
> the game client (Electron-launcher/vestigial). So: (1) there is NO existence-proof that `D3DAssemble`
> handles the SWG asm dialect; (2) a CHEAPER HARD-05 alternative than the D3DCompile port is to link a
> prebuilt `d3dx9_xx.dll` redist instead of compiling D3DX with the modern toolchain (Restoration's x64
> trick) — verify our own build's D3DX linkage first; (3) Restoration's x64 build KEEPS D3DX, so x64 and
> D3DX-removal are independent. Full detail: memory `reference_restoration_binaries_intel`.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Runtime VS compile (HLSL→bytecode) | D3D9 plugin (gl05/gl07) | — | `Direct3d9_VertexShaderData::createVertexShader` owns it |
| Runtime VS assembly (asm→bytecode) | D3D9 plugin (gl05/gl07) | — | same file, `D3DXAssembleShader` branch (:567) |
| Shader text/`.inc` resolution | Engine `TreeFile` (sharedFile) | D3D9 plugin include handler | TRE/override search-path lives in sharedFile; plugin adapts via `ID3DInclude` |
| Pixel shader creation | D3D9 plugin | — | pre-compiled `PEXE` bytecode → `CreatePixelShader`; NO compile (out of scope) |
| `d3dcompiler_47.dll` provisioning | Build/stage (postbuild) | — | redist DLL must sit next to exe at runtime |
| Compiled-bytecode cache (optional) | D3D9 plugin + disk | engine config dir | mirror gl11 `Direct3d11_ShaderCache` only if adopted |

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `d3dcompiler_47.dll` | Win10 SDK (10.0.19041 / 10.0.22621 present on this machine) | `D3DCompile`, `D3DAssemble`, `D3DReflect` | The current, supported HLSL/asm compiler; replaces deprecated D3DX. gl11 already uses it; Restoration ships it. [VERIFIED: SDK header present at `Windows Kits/10/Include/10.0.19041.0/um/d3dcompiler.h`] |
| `d3dcompiler.lib` | Win10 SDK | Import lib for the above | Link-time binding (gl11 links it) [CITED: MS docs req.lib] |
| `d3d9.lib` | (already linked) | `CreateVertexShader` consumes the bytecode blob | No change; D3D9 accepts SM2/SM3 bytecode from D3DCompile [VERIFIED: MS docs — output is generic shader bytecode] |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `d3dx9.lib` | legacy (currently linked) | `D3DXAssembleShader` for residual asm path | Keep ONLY if asm path stays on D3DX (the conservative residual). Remove only if `D3DAssemble` replaces it. [VERIFIED: present in all 3 gl vcxproj `AdditionalDependencies`] |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| In-place `D3DCompile` swap on our HLSL | Lift Restoration HLSL (D-01) | IMPOSSIBLE — Restoration ships binary/encrypted shaders, no HLSL source. Premise falsified. |
| `D3DAssemble` for the 96 `//asm` | Keep `D3DXAssembleShader`+SEH | `D3DAssemble` retires D3DX fully but is unverified against SWG's exact asm dialect; the SEH-guarded D3DX path is proven-working today. Recommend: keep D3DX residual unless `D3DAssemble` is validated. |
| `D3DAssemble` for the 96 `//asm` | Hand-convert asm→HLSL (Phase-19 style) | High effort (96 shaders); Phase 19 only did ~12. Out of scope for a "compiler port." |
| Add a disk cache (mirror gl11) | No cache (compile every launch) | D3D9 has NO shader cache today; compile-every-launch is the current behavior and is acceptable. Cache is optional polish, not required for HARD-05. |

**Installation / linkage:**
```
# Link (vcxproj AdditionalDependencies): add d3dcompiler.lib
# Stage: copy d3dcompiler_47.dll next to SwgClient_d.exe / SwgClient_r.exe (postbuild)
# Source: #include <d3dcompiler.h>  (replaces #include <d3dx9shader.h> for the compile call)
```

**Version verification:** `d3dcompiler_47.dll` is the final, frozen version (Microsoft has shipped
`_47` since 2015; there is no `_48`). The Win10 SDK on this machine provides it. Restoration redistributes
the exact `_47` build — copy from the SDK `redist` or from a known-good location and stage it. Do NOT
rely on it being present on the user's machine. [CITED: MS docs req.dll = d3dcompiler_47.dll; VERIFIED: SDK present]

## Architecture Patterns

### System Architecture Diagram

```
ShaderImplementation::Pass::VertexShader.m_text  (HLSL or asm, ASCII, from TRE/override)
        │
        ▼
Direct3d9_VertexShaderData ctor  ── parses header ──►  m_hlsl flag  +  texcoord-set tags
        │                                              (//hlsl vs //asm)
        ▼
createVertexShader(textureCoordinateSetKey)
        │
        ├── builds D3DXMACRO[] defines (texcoord remap + DECLARE_textureCoordinateSets)
        │      ▼  (rename to D3D_SHADER_MACRO[] — identical struct shape: {Name, Definition})
        │
        ├── m_hlsl == true  ─────────────────────────┐
        │      OLD: compileVertexShaderFpGuarded → D3DXCompileShader(... "vs_2_0"/"vs_1_1" ...)
        │      NEW: D3DCompile(text, len, srcName, defines, ID3DInclude*, "main",
        │                       "vs_2_0"/"vs_3_0", flags, 0, &blob, &errors)
        │                                             │
        ├── m_hlsl == false (//asm) ─────────────────┤
        │      KEEP: D3DXAssembleShader (+SEH)        │   ── OR ──  D3DAssemble (d3dcompiler_47)
        │                                             │
        ▼                                             ▼
   ID3DXBuffer / ID3DBlob compiled bytecode  ──► GetBufferPointer()
        │
        ▼
Direct3d9::getDevice()->CreateVertexShader((DWORD const*)bytecode, &IDirect3DVertexShader9)
        │
        ▼
   cached per textureCoordinateSetKey in m_container / m_nonPatchedVertexShader
```

Include resolution (`#include "vertex_program/include/...inc"`) routes through `TreeFile::open` in BOTH
the old `ID3DXInclude` handler and the new `ID3DInclude` handler — identical engine call, different COM
interface base.

### Recommended Change Surface
```
src/engine/client/application/Direct3d9/src/win32/
└── Direct3d9_VertexShaderData.cpp   # the ONLY compile-surface file
    ├── IncludeHandler : ID3DXInclude   → ID3DInclude  (Open/Close signatures change subtly)
    ├── Defines: std::vector<D3DXMACRO> → std::vector<D3D_SHADER_MACRO>  (same fields)
    ├── compileVertexShaderFpGuarded   → D3DCompile call (SEH wrapper droppable on HLSL path, D-03)
    └── D3DXAssembleShader (:567)       → keep, OR swap to D3DAssemble
build/win32/Direct3d9{,_ffp,_vsps}.vcxproj   # +d3dcompiler.lib ; -d3dx9.lib only if fully retired
(postbuild / stage)                          # stage d3dcompiler_47.dll next to exe
```

### Pattern 1: D3DCompile call (mirror gl11, adapted to D3D9 SM2/SM3)
**What:** Replace the `D3DXCompileShader` call with `D3DCompile`. The parameter mapping is near-identical.
**When to use:** the `m_hlsl == true` branch.
**Example:**
```cpp
// Source: src/engine/client/application/Direct3d11/src/win32/Direct3d11_VertexShaderData.cpp:940
//         (the proven gl11 call shape — adapt profile to D3D9 SM2/SM3)
ComPtr<ID3DBlob> blob, errors;
UINT flags = 0;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
flags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;   // accept SOE's D3D9-era HLSL leniently
// NOTE: do NOT add D3DCOMPILE_PACK_MATRIX_ROW_MAJOR here — that gl11 flag pairs with the
// transposed-cbuffer D3D11 upload convention. D3D9 SetVertexShaderConstantF semantics are the
// SAME as the original D3DX path; keep matrix packing as D3DX produced it (no flag) to preserve
// the existing register layout. [ASSUMED — must be verified by visual parity, see Pitfall 1]
HRESULT hr = D3DCompile(
    m_compileText, m_compileTextLength,
    m_vertexShader->m_fileName.getString(),   // pSourceName for error messages
    &(ms_defines.front()),                     // D3D_SHADER_MACRO[] (NULL-terminated)
    &includeHandler,                           // ID3DInclude*
    "main",                                    // entry point (unchanged)
    target,                                    // "vs_2_0" (>=SM2) or "vs_1_1" — same logic as today
    flags, 0,
    blob.GetAddressOf(), errors.GetAddressOf());
// CreateVertexShader consumes blob->GetBufferPointer() exactly as the old ID3DXBuffer.
```
Profile selection logic is UNCHANGED from the existing code
(`Direct3d9_VertexShaderData.cpp:392-395`): `vs_2_0` if `getShaderCapability() >= (2,0)` else `vs_1_1`.
`D3DCompile` accepts both. [VERIFIED: MS docs "shader model 2, 3, 4, or 5" + d3dcompiler_47 export
profile strings include `vs_1_1`, `vs_2_0`, `vs_3_0`]

### Pattern 2: ID3DInclude handler (adapt the existing ID3DXInclude one)
**What:** The existing `IncludeHandler` (`:55-198`) already routes includes through `TreeFile::open`
with a per-engine-owns-window cache. Only the COM base interface and method signatures change.
**When to use:** passed as the `pInclude` arg to `D3DCompile`.
**Example:**
```cpp
// Source: src/engine/client/application/Direct3d11/src/win32/Direct3d11_CompileIncludeHandler.cpp:136
//   ID3DXInclude::Open(D3DXINCLUDE_TYPE, LPCSTR, LPCVOID, LPCVOID*, UINT*)
//        becomes
//   ID3DInclude::Open(D3D_INCLUDE_TYPE, LPCSTR, LPCVOID, LPCVOID*, UINT*)   // __stdcall
// Body is otherwise the SAME: strip leading "../../", TreeFile::open(PriorityData, canFail=true),
// readEntireFileAndClose, return buffer; Close() delete[]'s it. The D3D9 file already has the
// engine-owns-window include CACHE — keep it (gl11 dropped the cache for simplicity; D3D9 can retain).
```
**Reuse option:** `Direct3d11_CompileIncludeHandler` is in the gl11 plugin (separate DLL); the D3D9
plugin cannot link it. Either (a) adapt the existing D3D9 `IncludeHandler` in place (recommended —
minimal change, keeps the include cache), or (b) factor a shared header into `sharedFile`/clientGraphics
(larger blast radius, not worth it for one call site).

### Pattern 3: D3DAssemble for the asm path (optional, retires D3DX fully)
**What:** `d3dcompiler_47.dll` exports `D3DAssemble` — the modern replacement for `D3DXAssembleShader`.
**When to use:** the `m_hlsl == false` branch, IF the team chooses full D3DX retirement.
**Caveat:** Unverified against SWG's exact `vs_1_1`/`vs_2_0` asm dialect and the `TARGET`/maxTextureCoordinate
macro defines the asm path injects (`:544-558`). The signature differs from `D3DXAssembleShader`
(no `LPD3DXBUFFER`; uses `ID3DBlob`). [VERIFIED export exists; behavior on SWG asm ASSUMED]
**Recommendation:** Default to KEEPING `D3DXAssembleShader`+SEH guard for the 96 asm shaders (proven
working). Treat `D3DAssemble` as a stretch goal validated separately. This satisfies HARD-05's "SEH
guard retained for any path still on D3DX."

### Anti-Patterns to Avoid
- **Feeding our D3D9-era HLSL to a SM4+ profile.** gl11 needed 13+ rewrite iterations (X3000 `point`
  keyword, X3202 struct-member registers, X4016 overlapping registers, cbuffer-wrapping) because it
  compiles to `vs_4_0`. **We compile to `vs_2_0`/`vs_3_0`** — the SM2/SM3 profiles accept the legacy
  syntax directly. DO NOT port gl11's `Direct3d11_HlslRewrite` rules into gl05/gl07; they exist to
  bridge SM2→SM4 and would corrupt a same-profile recompile. [VERIFIED: gl11 source comments
  `Direct3d11_VertexShaderData.cpp:28-64`, `Direct3d11_CompileIncludeHandler.cpp:40-107`]
- **Dropping the asm path silently** → null VS → skipped draws (saber/decal/detail geometry vanishes).
- **Assuming Restoration shaders are usable source.** They are encrypted binary. (D-01 trap.)
- **Forgetting to stage `d3dcompiler_47.dll`.** Missing DLL = boot failure or compile failure on first
  shader. Must be in the postbuild copy + git-tracked-or-documented (PORT-01 lesson).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| HLSL→bytecode compile | A custom compiler / asm assembler | `D3DCompile` (d3dcompiler_47) | The whole point of the port |
| asm→bytecode | Hand-written assembler | `D3DAssemble` OR keep `D3DXAssembleShader` | Both are battle-tested |
| `#include` resolution | New file-search logic | Existing `TreeFile::open` (already wired in the D3D9 IncludeHandler) | Honors TRE + override search-path correctly |
| Bytecode disk cache | Custom format | `Direct3d11_ShaderCache` pattern (FNV-1a hash + `.cso` dump) IF you add caching | gl11 already proved it; but caching is OPTIONAL here |
| Macro list | New struct | `D3D_SHADER_MACRO` (same `{Name, Definition}` layout as `D3DXMACRO`) | Drop-in field-compatible rename |

**Key insight:** `D3DXMACRO` and `D3D_SHADER_MACRO` are structurally identical (`{const char* Name;
const char* Definition;}`). The entire defines-building machinery (`:402-471` HLSL, `:498-564` asm)
ports with a type rename, no logic change.

## Runtime State Inventory

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — shaders are compiled per-launch from TRE/override text; no persisted compiled state today (D3D9 has no shader cache). [VERIFIED: only `Direct3d9_StateCache`/`_Metrics` match "cache", unrelated] | None |
| Live service config | None — no external service holds shader state | None |
| OS-registered state | None | None |
| Secrets/env vars | None | None |
| Build artifacts | (1) `d3dx9.lib` linkage in 3 vcxproj — removable only if asm path fully retired. (2) `d3dcompiler_47.dll` must be ADDED to stage/. (3) Stale gl05/gl07 `_d.dll`/`_r.dll` after the change — both must rebuild (ABI: this is plugin-internal, no shared-header change, so the SwgClient exe is unaffected). | Add d3dcompiler.lib + stage DLL; rebuild gl05+gl07 (both Debug+Release per [[reference_debug_exe_reads_client_d_cfg]] + boot invariant) |

## Common Pitfalls

### Pitfall 1: Register-layout / constant-binding mismatch after recompile
**What goes wrong:** D3DX and d3dcompiler_47 are different compilers; their automatic constant-register
allocation can differ, breaking the C++↔shader contract (`SetVertexShaderConstantF(N, ...)` expecting
a specific `cN`). Visuals look wrong with no FATAL.
**Why it happens:** SWG vertex constants are bound by explicit register (`: register(cN)`) in the
`.inc` headers. Recompiling under a different compiler *should* honor those, but matrix packing
(row/col-major) and `$Globals` promotion can shift things.
**How to avoid:** Compile to the SAME profile family (`vs_2_0`/`vs_3_0`) the assets target. Do NOT add
`PACK_MATRIX_ROW_MAJOR` (a gl11-specific flag for transposed cbuffers). Diff a disassembled output
(`D3DDisassemble`) of one shader old-vs-new if anything looks off.
**Warning signs:** geometry transformed wrong, lighting flat/black, texcoords swapped — the same class
gl11 fought. Lower risk here (same profile), but it is the #1 suspect.

### Pitfall 2: Asm path nulled → invisible geometry
**What goes wrong:** If the asm branch is removed without a working `D3DAssemble`/D3DX path, the 96
`//asm` VS produce NULL → `CreateVertexShader` never called → draws skipped → sabers/decals/detail
surfaces vanish.
**How to avoid:** Keep `D3DXAssembleShader`+SEH guard as the residual asm path (D-02 fallback, now the
expected path). Census proves 96 shaders depend on it.
**Warning signs:** Missing lightsaber blades, decals, detail-mapped surfaces in the parity smoke.

### Pitfall 3: Missing / wrong `d3dcompiler_47.dll`
**What goes wrong:** DLL not staged → `D3DCompile` symbol unresolved (if static-linked, link fails;
if delay/dynamic, runtime failure on first shader).
**How to avoid:** Add `d3dcompiler.lib` to the linker; copy `d3dcompiler_47.dll` to `stage/` via
postbuild; document/track it (PORT-01 precedent — `stage/miles` was missing and broke fresh clones).
**Warning signs:** Boot crash on first world/char-select load; "could not load d3dcompiler_47.dll".

### Pitfall 4: SEH guard removed too early
**What goes wrong:** Dropping `compileVertexShaderFpGuarded` before confirming D3DCompile is FP-clean
on the Tatooine bump/dot3 spot reintroduces the `0xC0000090` crash Fix-A fixed.
**How to avoid:** Per D-03/D-05, keep the SEH guard on the HLSL path through validation; drop it only
after the targeted Tatooine spot renders clean under `D3DCompile`. The asm path KEEPS the guard
(stays on D3DX). [CITED: memory `project_d3d9_d3dxcompileshader_fp_crash`, commit `db83b0f5c`]

### Pitfall 5: /FORCE link false-pass
**What goes wrong:** SwgClient links under `/FORCE`, downgrading unresolved externals (e.g. a
half-removed `d3dx9.lib` symbol) to warnings, emitting a binary anyway.
**How to avoid:** Grep link output for `unresolved external symbol` (must be 0) after changing lib
dependencies. [CITED: STATE.md blocker "/FORCE false-pass"]

## Code Examples

### Current compile entry (what we're replacing)
```cpp
// Source: Direct3d9_VertexShaderData.cpp:477 (HLSL path)
HRESULT result = compileVertexShaderFpGuarded(
    m_compileText, m_compileTextLength, &(ms_defines.front()), &includeHandler,
    "main", target /* "vs_2_0" or "vs_1_1" */, 0, &compiledShader, &error);
// ...
// Source: Direct3d9_VertexShaderData.cpp:573 (consume bytecode — UNCHANGED after port)
Direct3d9::getDevice()->CreateVertexShader(
    reinterpret_cast<DWORD const *>(compiledShader->GetBufferPointer()), &vertexShader);
```

### Current asm entry (keep, or swap to D3DAssemble)
```cpp
// Source: Direct3d9_VertexShaderData.cpp:567
HRESULT result = D3DXAssembleShader(
    m_compileText, m_compileTextLength, &(ms_defines.front()), &includeHandler,
    0, &compiledShader, NULL);
```

### gl11 reference D3DCompile (the proven shape to mirror)
```cpp
// Source: Direct3d11_VertexShaderData.cpp:940 — note flags differ for our SM2/SM3 target (see Pattern 1)
HRESULT hr = D3DCompile(rewrittenSource.data(), rewrittenSource.size(), virtName,
    defines.data(), Direct3d11_CompileIncludeHandler::getInstance(), "main",
    kVertexShaderProfile /* vs_4_0 — we use vs_2_0/vs_3_0 instead */,
    flags, 0, blob.GetAddressOf(), errors.GetAddressOf());
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `D3DXCompileShader` (d3dx9) | `D3DCompile` (d3dcompiler_47) | D3DX deprecated ~2012, removed from Win8+ SDK | The whole port; D3DX is legacy DirectX-SDK-era, unsupported on modern toolchains (the FP-fault root cause) |
| `D3DXAssembleShader` (d3dx9) | `D3DAssemble` (d3dcompiler_47) | same | Optional asm-path modernization; the gl11 comment claiming "D3DAssemble is gone from the modern SDK" is WRONG — it's exported by d3dcompiler_47 [VERIFIED: export table] |
| `ID3DXInclude` | `ID3DInclude` | d3dcompiler era | COM-interface rename; same Open/Close semantics |
| `D3DXMACRO` | `D3D_SHADER_MACRO` | d3dcommon | Field-identical rename |
| `ID3DXBuffer` | `ID3DBlob` | d3dcommon | `GetBufferPointer`/`GetBufferSize` — same usage |

**Deprecated/outdated:**
- `d3dx9.h` / `d3dx9shader.h` includes → replace with `d3dcompiler.h` (for the HLSL compile call;
  keep `d3dx9` only if `D3DXAssembleShader` residual stays).
- The CONTEXT note "gl05/gl07 statically compile D3DX from source" is **inaccurate** — the plugins
  link `d3dx9.lib` (import library), they do not compile D3DX from source. The FP fault is in the
  D3DX *runtime DLL* code paths invoked via that lib, compiled in the legacy SDK. The fix is still to
  stop calling D3DXCompileShader; the "static from source" framing should not drive planning.
  [VERIFIED: vcxproj AdditionalDependencies = `d3dx9.lib`]

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Compiling our HLSL to the SAME `vs_2_0`/`vs_3_0` profile under D3DCompile preserves the register/constant layout the C++ expects | Pitfall 1 / Pattern 1 | If allocation shifts, visuals break with no FATAL. Mitigate via disasm diff + parity smoke. MEDIUM risk. |
| A2 | Not adding `PACK_MATRIX_ROW_MAJOR` keeps matrix packing as D3DX produced it (correct for D3D9 `SetVertexShaderConstantF`) | Pattern 1 | Wrong packing → transforms wrong. Verify on the first matrix-using shader. MEDIUM. |
| A3 | `D3DAssemble` accepts SWG's `vs_1_1`/`vs_2_0` asm dialect + the injected `TARGET`/`maxTextureCoordinate` defines | Pattern 3 | If not, full D3DX retirement is impossible; keep D3DX residual. LOW risk to the phase (residual is the default). |
| A4 | `D3DCompile` is immune to the `0xC0000090` post-compile FP fault (so SEH guard droppable on HLSL path) | D-03 / Pitfall 4 | gl11 runs D3DCompile without the guard and is stable, but that's a different process/profile. Validate on the Tatooine spot before dropping. LOW-MEDIUM. |
| A5 | gl05/gl07 can dynamic-load or static-link d3dcompiler_47 without ABI fallout to SwgClient.exe | Runtime State / Pitfall 3 | This is plugin-internal (no shared header changed), so the exe is unaffected — but both gl plugins must rebuild. LOW. |

## Open Questions

1. **Should D-01 be formally dropped/replaced before planning?**
   - What we know: the "lift Restoration HLSL" premise is factually impossible (binary shaders).
   - What's unclear: whether Kenny wants to (a) accept the in-place swap on our own HLSL, or (b)
     investigate decrypting Restoration shaders via `TreeFileExtractor.exe` (likely still yields
     bytecode, not source — low value).
   - Recommendation: discuss-phase should present the falsified premise + the simpler in-place path
     and get an explicit re-decision. The in-place swap is lower-risk and fully satisfies HARD-05.

2. **Asm path: D3DAssemble vs keep-D3DX-residual?**
   - What we know: 96 `//asm` shaders; `D3DAssemble` exists; `D3DXAssembleShader`+SEH works today.
   - Recommendation: keep D3DX residual (default, satisfies HARD-05's "retained for any path still on
     D3DX"); treat D3DAssemble as a separately-validated stretch goal. Don't block the phase on it.

3. **Add a bytecode disk cache (mirror gl11) or not?**
   - What we know: D3D9 has no cache today; compiles every launch; acceptable.
   - Recommendation: skip caching for HARD-05 (out of the requirement); revisit only if launch-time
     shader compile is a measured problem.

4. **Where does `d3dcompiler_47.dll` get sourced for staging?**
   - Recommendation: copy from the Win10 SDK redist (or the known-good Restoration copy) into a
     tracked location + postbuild copy to `stage/`. Mirror the PORT-01 stage-asset handling.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| `d3dcompiler.h` / `d3dcompiler.lib` | D3DCompile call + link | ✓ | Win10 SDK 10.0.19041 & 10.0.22621 | — |
| `d3dcompiler_47.dll` | runtime compile | ✓ (SDK + Restoration copy) | _47 (frozen) | none — must stage |
| `d3dx9.lib` (residual asm) | `D3DXAssembleShader` keep-path | ✓ (currently linked) | legacy | drop only if D3DAssemble adopted |
| Python TRE tooling (research only) | asm census / format check | ✓ | swg-blender-plugin `tre_reader.py` | — |
| `TreeFileExtractor.exe` | (only if decrypting Restoration TREs) | unknown | — | not needed — Restoration shaders not used |

**Missing dependencies with no fallback:**
- `d3dcompiler_47.dll` must be added to `stage/` (postbuild) — currently NOT staged. [VERIFIED: absent
  from `stage/`]

**Missing dependencies with fallback:**
- None blocking.

## Validation Architecture

> Per D-05 the validation bar is runtime dual-renderer boot smoke + a targeted visual spot, not an
> automated unit-test suite. `.planning/config.json` Nyquist status not separately confirmed; this
> phase is renderer/visual, so validation is runtime A/B per project precedent (every prior D3D
> phase). No code-level test framework applies to shader compilation output.

### Phase Requirements → Validation Map
| Req ID | Behavior | Validation Type | Command / Method |
|--------|----------|-----------------|------------------|
| HARD-05 | HLSL VS compile via D3DCompile (no D3DX on HLSL path) | build + boot smoke | rebuild gl05+gl07 Debug+Release; grep link for 0 unresolved; boot rasterMajor=5 to char-select/in-game |
| HARD-05 | asm path not dropped | visual smoke | confirm saber/decal/detail geometry renders (the 96 `//asm` consumers) |
| HARD-05 | no shader-compile regression | A/B parity | dual-renderer boot (rasterMajor=5 AND =11); compare to pre-port baseline |
| D-05 | Fix-A spot clean | targeted spot | the exact Tatooine bump/dot3 VS spot from `db83b0f5c` renders without `0xC0000090` |

### Boot Invariant (MANDATORY)
- Debug exe → `client_d.cfg`; Release exe → `client.cfg`. Claude owns the rasterMajor toggle.
  [CITED: memory `feedback_debug_exe_reads_client_d_cfg`, `feedback_set_rastermajor_on_renderer_switch`]
- Watch for stage cfg UTF-8 BOM (Release-only crash). [CITED: `reference_client_cfg_bom_release_crash`]

### Wave 0 Gaps
- [ ] Stage `d3dcompiler_47.dll` (postbuild copy + tracked source) — required before any boot test.
- [ ] Capture a pre-port A/B baseline screenshot set (gl05 + gl11) for the parity diff.
- *(No test files — this is runtime visual validation per project convention.)*

## Project Constraints (from CLAUDE.md)

No `./CLAUDE.md` found in the working directory. Constraints derive from MEMORY.md + STATE.md:
- Don't modify Koogie's source-level diagnostic changes without strong reason — fix the caller/data.
- Boot-to-character-select invariant under BOTH rasterMajor=5 and =11 for every client-touching change.
- /FORCE link false-pass: grep link output for `unresolved external symbol` (must be 0).
- Debug exe reads `client_d.cfg`, Release exe reads `client.cfg`.
- Never write `.cfg` via PowerShell `Set-Content`/`Out-File` (UTF-8 BOM → Release boot crash).
- Plugin shared-header struct touches break stale plugin DLL ABI — but this port is plugin-internal
  (no shared header changes), so the SwgClient exe is unaffected; still rebuild BOTH gl05 and gl07.

## Security Domain

Not applicable in the conventional sense — this is an internal renderer compiler swap, no untrusted
input surface added. One relevant note: the (optional) shader-cache path, IF adopted from gl11, must
take its cache directory from engine config NEVER user input (gl11 `Direct3d11_ShaderCache` already
enforces hex-only filenames + config-only dir, T-11-05-01). Since caching is recommended OUT of scope
for HARD-05, this is informational only.

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V5 Input Validation | marginal | shader text comes from trusted TRE/override assets (same trust level as all game assets); D3DCompile errors are FATAL'd, not exploited |
| V6 Cryptography | no | — |
| V12 File handling | marginal (only if cache added) | config-derived cache dir + hex-only names (gl11 precedent) |

## Sources

### Primary (HIGH confidence)
- `Direct3d9_VertexShaderData.cpp` (full read) — the port target; HLSL+asm compile, SEH guard, include handler.
- `Direct3d9_PixelShaderProgramData.cpp` — confirms PS path uses pre-compiled `m_exe`, no runtime compile.
- `Direct3d11_VertexShaderData.cpp`, `Direct3d11_CompileIncludeHandler.{cpp,h}`, `Direct3d11_ShaderCache.cpp` — proven gl11 D3DCompile pattern + the SM2→SM4 rewrite history (NOT to be copied).
- D3DCompile asm census of `D:/Code/SWGSource Client v3.0/*.tre` — 190 //hlsl / 96 //asm of 286 .vsh [tool-verified].
- Restoration TRE extraction `D:\SWG Restoration\SwgRestoration_*.tre` — shaders are binary, not HLSL [tool-verified].
- Restoration install listing — ships `d3dcompiler_47.dll`; gl05/gl07 are D3D9 (SM1-3) plugins [tool-verified].
- d3dcompiler_47.dll export scan — `D3DCompile`, `D3DAssemble`, `D3DReflect` present [tool-verified].
- MS Learn — D3DCompile function reference (signature, SM2-5 targets, d3dcompiler_47.dll) https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompile
- vcxproj `AdditionalDependencies` — `d3dx9.lib` linkage (corrects "static from source" framing).
- Win10 SDK `d3dcompiler.h` present at `Windows Kits/10/Include/10.0.{19041,22621}.0/um/`.

### Secondary (MEDIUM confidence)
- MS Learn — D3DXAssembleShader (legacy reference) https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxassembleshader
- Commit `db83b0f5c` — Fix-A SEH guard; memory `project_d3d9_d3dxcompileshader_fp_crash`.

### Tertiary (LOW confidence)
- D3DAssemble behavior on SWG's exact asm dialect — export confirmed; runtime behavior unverified (A3).

## Metadata

**Confidence breakdown:**
- D-02 verification (asm census + Restoration format): HIGH — direct tool extraction, definitive.
- Port mechanics (D3DCompile swap, include handler, profiles): HIGH — gl11 precedent + MS docs + SDK present.
- Register-layout preservation after recompile: MEDIUM — same-profile lowers risk but must be parity-smoke-verified (A1/A2).
- D3DAssemble for asm: LOW-MEDIUM — export verified, SWG-dialect behavior assumed; default is keep-D3DX-residual.
- SEH-guard removability: MEDIUM — gl11 stable without it, validate on Tatooine spot (D-05).

**Research date:** 2026-06-13
**Valid until:** 2026-07-13 (stable — D3DX/d3dcompiler are frozen legacy APIs; only the codebase moves)
